//rage headers
#include "math/vecmath.h"

//game headers
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAi/task/TaskVehiclePark.h"
#include "vehicles/VehicleDefines.h"
#include "peds/ped.h"
#include "peds/pedintelligence.h"
#include "control/trafficlights.h"
#include "control/record.h"
#include "debug/DebugScene.h"
#include "event/EventNetwork.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "frontend/minimap.h"
#include "scene/world/gameWorld.h"
#include "script/Handlers/GameScriptEntity.h"
#include "ai/Task/Task.h"
#include "ai/debug/types/VehicleControlDebugInfo.h"
#include "ai/debug/types/VehicleDebugInfo.h"
#include "ai/debug/types/VehicleFlagDebugInfo.h"
#include "ai/debug/types/VehicleNodeListDebugInfo.h"
#include "ai/debug/types/VehicleStuckDebugInfo.h"
#include "ai/debug/types/VehicleTaskDebugInfo.h"
#include "task/General/TaskBasic.h"
#include "Task/System/TaskManager.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskTree.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Response/TaskFlee.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "vehicleAi/roadspeedzone.h"
#include "vehicleAi/task/DeferredTasks.h"
#include "vehicleAi/Task/TaskVehicleApproach.h"
#include "vehicleAi/Task/TaskVehicleGoTo.h"
#include "vehicleAi/Task/TaskVehicleGotoAutomobile.h"
#include "vehicleAi/Task/TaskVehicleGotoBoat.h"
#include "vehicleAi/Task/TaskVehicleGoToHelicopter.h"
#include "vehicleAi/Task/TaskVehicleGoToPlane.h"
#include "vehicleAi/Task/TaskVehicleGoToSubmarine.h"
#include "vehicleAi/Task/TaskVehicleAttack.h"
#include "vehicleAi/Task/TaskVehicleBlock.h"
#include "vehicleAi/Task/TaskVehicleCircle.h"
#include "vehicleAi/Task/TaskVehicleCruise.h"
#include "vehicleAi/Task/TaskVehicleCruiseBoat.h"
#include "vehicleAi/Task/TaskVehicleDeadDriver.h"
#include "vehicleAi/Task/TaskVehicleEscort.h"
#include "vehicleAi/Task/TaskVehicleFlying.h"
#include "vehicleAi/Task/TaskVehicleFleeBoat.h"
#include "vehicleAi/Task/TaskVehicleHeliProtect.h"
#include "vehicleAi/Task/TaskVehicleFollowRecording.h"
#include "vehicleAi/Task/TaskVehicleGoTo.h"
#include "vehicleAi/Task/TaskVehicleGoToLongRange.h"
#include "VehicleAi/task/TaskVehicleLandPlane.h"
#include "vehicleAi/Task/TaskVehiclePark.h"
#include "vehicleAi/Task/TaskVehiclePlayer.h"
#include "vehicleAi/Task/TaskVehiclePoliceBehaviour.h"
#include "vehicleAi/Task/TaskVehiclePullAlongside.h"
#include "vehicleAi/Task/TaskVehiclePursue.h"
#include "vehicleAi/Task/TaskVehicleRam.h"
#include "vehicleAi/Task/TaskVehicleSpinOut.h"
#include "vehicleAi/Task/TaskVehicleTempAction.h"
#include "vehicleAi/Task/TaskVehicleThreePointTurn.h"
#include "vehicleAi/Task/TaskVehiclePlaneChase.h"
#include "vehicles/metadata/AIHandlingInfo.h"
#include "vehicles/Trailer.h"
#include "physics/physics.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Submarine.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/vehiclefactory.h"
#include "ai/debug/system/AIDebugLogManager.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CVehicleIntelligence, VEHICLE_POOL_SIZE, 0.22f, atHashString("Vehicle Intelligence",0xb7750119), sizeof(CHeliIntelligence));

#if __BANK
const char *CVehicleIntelligence::MissionStrings[MISSION_LAST] = {
	"MISSION_NONE",
	"MISSION_CRUISE",
	"MISSION_RAM",
	"MISSION_BLOCK",
	"MISSION_GOTO",
	"MISSION_STOP",
	"MISSION_ATTACK",
	"MISSION_FOLLOW",
	"MISSION_FLEE",
	"MISSION_CIRCLE",
	"MISSION_ESCORT_LEFT",
	"MISSION_ESCORT_RIGHT",
	"MISSION_ESCORT_REAR",
	"MISSION_ESCORT_FRONT",
	"MISSION_GOTO_RACING",
	"MISSION_FOLLOW_RECORDING",
	"MISSION_POLICE_BEHAVIOUR",
	"MISSION_PARK_PERPENDICULAR",
	"MISSION_PARK_PARALLEL",
	"MISSION_LAND",
	"MISSION_LAND_AND_WAIT",
	"MISSION_CRASH",
	"MISSION_PULL_OVER",
	"MISSION_PROTECT",
	"MISSION_PARK_PERPENDICULAR_1",
	"MISSION_PARK_PERPENDICULAR_2",
	"MISSION_PARK_PARALLEL_1",
	"MISSION_PARK_PARALLEL_2",
	"MISSION_GOTO_STRAIGHTLINE",
	"MISSION_LEAVE_ALONE",
	"MISSION_BLOCK_BACKANDFORTH",
	"MISSION_TURN_CLOCKWISE_GOING_FORWARD",
	"MISSION_TURN_CLOCKWISE_GOING_BACKWARD",
	"MISSION_TURN_COUNTERCLOCKWISE_GOING_FORWARD",
	"MISSION_TURN_COUNTERCLOCKWISE_GOING_BACKWARD",
	"MISSION_BLOCK_FRONT",
	"MISSION_BLOCK_BEHIND"
};
const char *CVehicleIntelligence::TempActStrings[TEMPACT_LAST] = {
	"TEMPACT_NONE",
	"TEMPACT_WAIT",
	"TEMPACT_EMPTYTOBEREUSED",
	"TEMPACT_REVERSE",
	"TEMPACT_HANDBRAKETURNLEFT",
	"TEMPACT_HANDBRAKETURNRIGHT",
	"TEMPACT_HANDBRAKESTRAIGHT",
	"TEMPACT_TURNLEFT",
	"TEMPACT_TURNRIGHT",
	"TEMPACT_GOFORWARD",
	"TEMPACT_SWIRVELEFT",
	"TEMPACT_SWIRVERIGHT",
	"TEMPACT_EMPTYTOBEREUSED2",
	"TEMPACT_REVERSE_LEFT",
	"TEMPACT_REVERSE_RIGHT",
	"TEMPACT_PLANE_FLY_UP",
	"TEMPACT_PLANE_FLY_STRAIGHT",
	"TEMPACT_PLANE_SHARP_LEFT",
	"TEMPACT_PLANE_SHARP_RIGHT",
	"TEMPACT_HEADON_COLLISION",
	"TEMPACT_SWIRVELEFT_STOP",
	"TEMPACT_SWIRVERIGHT_STOP",
	"TEMPACT_REVERSE_STRAIGHT",
	"TEMPACT_BOOST_USE_STEERING_ANGLE",
	"TEMPACT_BRAKE",
	"TEMPACT_HANDBRAKETURNLEFT_INTELLIGENT",
	"TEMPACT_HANDBRAKETURNRIGHT_INTELLIGENT",
	"TEMPACT_HANDBRAKESTRAIGHT_INTELLIGENT",
	"TEMPACT_REVERSE_STRAIGHT_HARD",

};
const char *CVehicleIntelligence::JunctionCommandStrings[JUNCTION_COMMAND_LAST] = {
	"JUNCTION_COMMAND_GO",
	"JUNCTION_COMMAND_APPROACHING",
	"JUNCTION_COMMAND_WAIT_FOR_LIGHTS",
	"JUNCTION_COMMAND_WAIT_FOR_TRAFFIC",
	"JUNCTION_COMMAND_NOT_ON_JUNCTION",
	"JUNCTION_COMMAND_GIVE_WAY"
};
const char *CVehicleIntelligence::JunctionFilterStrings[JUNCTION_FILTER_LAST] = {
	"JUNCTION_FILTER_NONE",
	"JUNCTION_FILTER_LEFT",
	"JUNCTION_FILTER_MIDDLE",
	"JUNCTION_FILTER_RIGHT"
};
const char *CVehicleIntelligence::JunctionCommandStringsShort[JUNCTION_COMMAND_LAST] = {
	"GO",
	"AP",
	"WL",
	"WT",
	"NJ",
	"GW"
};
const char *CVehicleIntelligence::JunctionFilterStringsShort[JUNCTION_FILTER_LAST] = {
	"N",
	"L",
	"M",
	"R"
};
#endif

float CVehPIDController::Update(float fInput)
{
	Assert(fInput == fInput);
	float fIntegral = fInput * fwTimer::GetTimeStep();

	Assert(fIntegral == fIntegral);
	m_fIntegral += fIntegral;
	Assert(m_fIntegral == m_fIntegral);
	m_fPreviousDerivative = (fInput - m_fPreviousInput) / fwTimer::GetTimeStep();
	m_fPreviousInput = fInput;

	m_fPreviousOutput = (m_fKp * fInput) + (m_fKi * m_fIntegral) + (m_fKd * m_fPreviousDerivative);
	return m_fPreviousOutput;
}

//---------------------------------------------------------------------------

// Helper function used for some LOD checks since POPTYPE_RANDOM_PARKED doesn't
// seem to be reliable enough to detect a parked vehicle (it doesn't get set again
// if the player enters and then leaves the vehicle, for example).
static bool sConsiderParkedVehicleForLod(CVehicle* pVehicle)
{
	ePopType popType = pVehicle->PopTypeGet();
	if(popType == POPTYPE_RANDOM_PARKED)
	{
		return true;
	}
	else if(popType == POPTYPE_RANDOM_AMBIENT)
	{
		CVehicleIntelligence* pIntel = pVehicle->GetIntelligence();
		if(!pIntel || !pIntel->GetActiveTask())
		{
			return true;
		}
	}
	else if(popType == POPTYPE_MISSION)
	{
		// Mission vehicles can get low LOD processing if they are not
		// running any primary nor secondary tasks.
		CVehicleIntelligence* pVehicleIntelligence = pVehicle->GetIntelligence();
		if(pVehicleIntelligence)
		{
			const CTaskManager* pTaskManager = pVehicleIntelligence->GetTaskManager();
			if( pTaskManager && !pTaskManager->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY) && !pTaskManager->GetActiveTask(VEHICLE_TASK_TREE_SECONDARY) )
			{
				// no primary and no secondary task, treat as parked
				return true;
			}
		}
		else // pVehicleIntelligence is NULL
		{
			// no tasks running if no intelligence exists, treat as parked
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------
// Internals
//---------------------------------------------------------------------------
namespace // anonymous namespace
{
#if __BANK
	const u32 uDEBUG_MAX_TIME_TO_SHOW_ROUTE_INTERCEPTION_TEST_MS = 10000u;
#endif // __BANK
}

//---------------------------------------------------------------------------
// CVehicleIntelligence
//---------------------------------------------------------------------------
bool CVehicleIntelligence::ms_bReallyPreventSwitchedOffFleeing = false;
CVehicleIntelligence::CVehicleIntelligence(CVehicle *pVehicle) :
	m_pVehicle(pVehicle),
	m_TaskManager(pVehicle),
	m_pedScanner(CExpensiveProcess::VPT_PedScanner),
	m_vehicleScanner(CExpensiveProcess::VPT_VehicleScanner),
	m_objectScanner(CExpensiveProcess::VPT_ObjectScanner),
	m_SteeringDistributer(CExpensiveProcess::VPT_SteeringDistributer),
	m_BrakingDistributer(CExpensiveProcess::VPT_BrakingDistributer),
	m_LaneChangeDistributer(CExpensiveProcess::VPT_LaneChangeDistributer),
	m_DummyAvoidanceDistributer(CExpensiveProcess::VPT_DummyAvoidanceDistributer),
	m_TailgateDistributer(CExpensiveProcess::VPT_TailgateDistributer),
	m_NavmeshLOSDistributer(CExpensiveProcess::VPT_NavmeshLOSDistributer),
	m_UpdateJunctionsDistributer(CExpensiveProcess::VPT_UpdateJunctionsDistributer),
	m_SirenReactionDistributer(CExpensiveProcess::VPT_SirenReactionDistributer),
	m_eventGroup(pVehicle),
	m_BoatAvoidanceHelper(pVehicle && ( pVehicle->InheritsFromBoat() || pVehicle->InheritsFromAmphibiousAutomobile() ) ? rage_new CBoatAvoidanceHelper(*pVehicle) : NULL)
{
	m_SteeringDistributer.RegisterSlot();
	m_BrakingDistributer.RegisterSlot();
	m_LaneChangeDistributer.RegisterSlot();
	m_DummyAvoidanceDistributer.RegisterSlot();
	m_TailgateDistributer.RegisterSlot();
	m_NavmeshLOSDistributer.RegisterSlot();
	m_UpdateJunctionsDistributer.RegisterSlot();
	m_SirenReactionDistributer.RegisterSlot();

	LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();
	LastTimeHonkedAt = 0;
	m_nLastTimeToldOthersAboutRevEngine = 0;
	
	m_fSmoothedSpeedSq = 0.0f;
	m_iNumScenarioRouteHistoryPoints = 0;
	m_pScenarioRoutePointHistory = NULL;

	//		bDontGoAgainstTraffic = false;
	SpeedFromNodes = 0;
	SpeedMultiplier = 1.0f;
	m_RecordingNumber = -1;
	//		AISwitchToStraightLineDistance = 20;
	bCarHasToReverseFirst = false;
	//		PreviousMission = MISSION_NONE;
	MillisecondsNotMoving = 0;
	MillisecondsNotMovingForRemoval = 0;
	m_lastTimeTriedToPushPlayerPed = 0;
	m_nTimeLastTriedToShakePedFromRoof = 0;
	m_iNumVehsWeAreBlockingThisFrame = 0;
	m_iJunctionTurnDirection = BIT_NOT_SET;
	m_iJunctionArrivalTime = UINT_MAX;
	m_JunctionCommand = JUNCTION_COMMAND_NOT_ON_JUNCTION;
	m_iJunctionFilter = JUNCTION_FILTER_NONE;
	m_bHasFixedUpPathForCurrentJunction = false;
	m_JunctionNode.SetEmpty();
	m_PreviousJunctionNode.SetEmpty();
	m_Junction = NULL;
	m_PreviousJunction = NULL;
	m_JunctionEntrance = NULL;
	m_PreviousJunctionEntrance = NULL;
	m_JunctionExit = NULL;
	m_PreviousJunctionExit = NULL;
	m_JunctionEntranceNode.SetEmpty();
	m_JunctionExitNode.SetEmpty();
	m_vJunctionEntrance.Zero();
	m_vJunctionExit.Zero();
	m_PreviousJunctionEntranceNode.SetEmpty();
	m_PreviousJunctionExitNode.SetEmpty();
	m_vPreviousJunctionEntrance.Zero();
	m_vPreviousJunctionExit.Zero();
	m_JunctionEntranceLane = -1;
	m_JunctionExitLane = -1;
	m_PreviousJunctionEntranceLane = -1;
	m_PreviousJunctionExitLane = -1;
	m_TrafficLightCommand = TRAFFICLIGHT_COMMAND_INVALID;
	m_pCarThatIsBlockingUs = NULL;
	//m_LaneShift = 0.0f;
	m_bHumaniseControls = false;
	m_bHasCachedNodeList = false;
	m_bHasCachedFollowRoute = false;
	m_bRemoveAggressivelyForCarjackingMission = false;
	m_bSwerveAroundPlayerVehicleForRiotVanMission = false;
	m_bStationaryInTank = false;
	m_bSetBoatIgnoreLandProbes = false;
	m_bSteerForBuildings = true;
	m_bDontSwerveForUs = false;
	m_bIgnorePlanesSmallPitchChange = false;

	m_pCachedNodeList = NULL;
	m_pCachedNodeListTask = NULL;
	m_pCachedFollowRouteHelper = NULL;
	m_pCachedFollowRouteHelperTask = NULL;
	m_pCarWeAreBehind = NULL;
	m_pCarWeCouldBeSlipStreaming = NULL;
	m_pObstructingEntity = NULL;
	m_pSteeringAroundEntity = NULL;
	m_pFleeEntity = NULL;
	m_fDesiredSpeedThisFrame = -1.0f;
	m_NumCarsBehindUs = 0;
	m_NumCarsBehindContributedToCarWeAreBehind = 0;

	m_iImpatienceTimerOverride = -1;
	m_nHandlingOverrideHash = 0;
	m_nNumPedsThatNeedTaskFromPretendOccupant = 0;
	m_fPretendOccupantAbility = -1.0f;
	m_fPretendOccupantAggressiveness = -1.0f;

	m_fCustomPathNodeStreamingRadius = -1.0f;

	m_uTimeWeStartedCollidingWithPlayerVehicle = 0;
	m_uTimeLastTouchedRealMissionVehicleWhileDummy = 0;
	m_uLastTimeInWater = 0;

	m_uLastTimePedEntering = 0;
	m_uLastTimePedInCover = 0;
	m_uLastTimeRamming = 0;
	m_uLastTimePlayerAimedAtOrAround = 0;

	m_bBlockWeaponSelection = false;
	m_bUsingScriptAutopilot = false;
#if __BANK
	lastTimeJoinedWithRoadSystem = 0;
	lastTimeActuallyJoinedWithRoadSystem = 0;
#endif // __BANK

#if __DEV
	m_iCacheNodeListHitsThisFrame = 0;
	m_iCacheFollowRouteHitsThisFrame = 0;
#endif
}


CVehicleIntelligence::~CVehicleIntelligence()
{
	SetCarWeAreBehind(NULL);

	m_pCarThatIsBlockingUs = NULL;
	m_pObstructingEntity = NULL;
	m_pFleeEntity = NULL;
	m_pSteeringAroundEntity = NULL;

	if (m_CurrentEvent)
	{
		delete m_CurrentEvent;
		m_CurrentEvent = NULL;
	}

	// safe to delete null pointers
	delete m_BoatAvoidanceHelper;

	DeleteScenarioPointHistory();
}

bool CVehicleIntelligence::HasBeenSlowlyPushingPlayer() const
{
	TUNE_GROUP_INT(FOLLOW_PATH_AI_THROTTLE, timeAfterWaitingForPedToContinueSteering, 3000, 0, 10000, 1);
	return fwTimer::GetTimeInMilliseconds() < m_lastTimeTriedToPushPlayerPed + timeAfterWaitingForPedToContinueSteering;
}

u32 CVehicleIntelligence::GetTimeSinceLastInWater() const
{
	Assert(HasBeenInWater());

	return CTimeHelpers::GetTimeSince(m_uLastTimeInWater);
}

void CVehicleIntelligence::AddScenarioHistoryPoint(const CRoutePoint& pt)
{
	EnsureHasScenarioPointHistory();

	Assert(m_iNumScenarioRouteHistoryPoints == 0 || 
		IsGreaterThanAll(DistSquared(pt.GetPosition(), m_pScenarioRoutePointHistory[m_iNumScenarioRouteHistoryPoints-1].GetPosition())
		, ScalarV(V_FLT_SMALL_2) * ScalarV(V_FLT_SMALL_2)));

	if (m_iNumScenarioRouteHistoryPoints >= ms_iNumScenarioRoutePointsToSave)
	{
		//if we're full, shift all the points over
		for (int i = 0; i < m_iNumScenarioRouteHistoryPoints-1; i++)
		{
			m_pScenarioRoutePointHistory[i] = m_pScenarioRoutePointHistory[i+1];
		}
	}
	

	m_iNumScenarioRouteHistoryPoints = rage::Min(m_iNumScenarioRouteHistoryPoints+1, ms_iNumScenarioRoutePointsToSave);

	m_pScenarioRoutePointHistory[m_iNumScenarioRouteHistoryPoints-1] = pt;
}

void CVehicleIntelligence::SetJunctionNode(const CNodeAddress& junctionNode, CJunction* junctionPtr)
{
// This stuff should hold up, but we may not want the overhead compiled in even in Beta builds:
#if __ASSERT && 0
	if(junctionNode.IsEmpty())
	{
		Assert(!junctionPtr);
	}
	else if(junctionPtr)
	{
		Assert(junctionPtr->ContainsJunctionNode(junctionNode));
	}
	Assert(CJunctions::FindJunctionFromNode(junctionNode) == junctionPtr);
#endif	// __ASSERT

	m_JunctionNode = junctionNode;
	m_Junction = junctionPtr;
}

void CVehicleIntelligence::CacheJunctionInfo()
{
	if (m_Junction)
	{
		CNodeAddress entranceAddress;
		CNodeAddress exitAddress;
		s32 iEntranceLane = -1;
		s32 iExitLane = -1;

		m_Junction->FindEntryAndExitNodes(m_pVehicle, entranceAddress, exitAddress, iEntranceLane, iExitLane);

		const CVehicleFollowRouteHelper* pFollowRouteHelper = GetFollowRouteHelper();
		const CRoutePoint* pRoutePoints = pFollowRouteHelper->GetRoutePoints();

		if (!entranceAddress.IsEmpty())
		{
			s32 iJunctionEntrance = m_Junction->FindEntranceIndexWithNode(entranceAddress);
			CJunctionEntrance* pJunctionEntrance = &m_Junction->GetEntrance(iJunctionEntrance);

			SetJunctionEntrance(pJunctionEntrance);
			SetJunctionEntranceNode(entranceAddress);
			SetJunctionEntranceLane(iEntranceLane);

			const CPathNode* pEntranceNode = ThePaths.FindNodePointerSafe(entranceAddress);

			if (pEntranceNode)
			{
				const s16 iEntrancePoint = pFollowRouteHelper->FindClosestPointToPosition(VECTOR3_TO_VEC3V(pEntranceNode->GetPos()));
				const CRoutePoint& entrancePoint = pRoutePoints[iEntrancePoint];
				const Vector3 vEntrance = VEC3V_TO_VECTOR3(entrancePoint.GetPosition() + entrancePoint.GetLaneCenterOffset());

				SetJunctionEntrancePosition(vEntrance);
			}
		}

		if (!exitAddress.IsEmpty())
		{
			s32 iJunctionExit = m_Junction->FindEntranceIndexWithNode(exitAddress);
			CJunctionEntrance* pJunctionExit = &m_Junction->GetEntrance(iJunctionExit);

			SetJunctionExit(pJunctionExit);
			SetJunctionExitNode(exitAddress);
			SetJunctionExitLane(iExitLane);

			const CPathNode* pExitNode = ThePaths.FindNodePointerSafe(exitAddress);

			if (pExitNode)
			{
				const s16 iExitPoint = pFollowRouteHelper->FindClosestPointToPosition(VECTOR3_TO_VEC3V(pExitNode->GetPos()));
				const CRoutePoint& exitPoint = pRoutePoints[iExitPoint];
				const Vector3 vExit = VEC3V_TO_VECTOR3(exitPoint.GetPosition() + exitPoint.GetLaneCenterOffset());

				SetJunctionExitPosition(vExit);
			}
		}

		CacheJunctionTurnDirection();
	}
}

void CVehicleIntelligence::ResetCachedJunctionInfo()
{
	m_PreviousJunctionNode = m_JunctionNode;
	m_PreviousJunction = m_Junction;
	m_PreviousJunctionEntrance = m_JunctionEntrance;
	m_PreviousJunctionExit = m_JunctionExit;
	m_PreviousJunctionEntranceNode = m_JunctionEntranceNode;
	m_PreviousJunctionExitNode = m_JunctionExitNode;
	m_vPreviousJunctionEntrance = m_vJunctionEntrance;
	m_vPreviousJunctionExit = m_vJunctionExit;
	m_PreviousJunctionEntranceLane = m_JunctionEntranceLane;
	m_PreviousJunctionExitLane = m_JunctionExitLane;

	m_JunctionNode.SetEmpty();
	m_Junction = NULL;
	m_JunctionEntrance = NULL;
	m_JunctionExit = NULL;
	m_JunctionEntranceNode.SetEmpty();
	m_JunctionExitNode.SetEmpty();
	m_vJunctionEntrance.Zero();
	m_vJunctionExit.Zero();
	m_JunctionEntranceLane = -1;
	m_JunctionExitLane = -1;
	m_iJunctionTurnDirection = BIT_NOT_SET;
	m_iJunctionArrivalTime = UINT_MAX;
	m_JunctionCommand = JUNCTION_COMMAND_GO;
	m_iJunctionFilter = JUNCTION_FILTER_NONE;
	m_bHasFixedUpPathForCurrentJunction = false;
}

// Create shocking events from properties of the vehicle for other entities in the game to react against.
void CVehicleIntelligence::GenerateEvents()
{
	// We basically just want it to happen continuously but at the same time not spam events every frame.  
	// Could use a timer or something prettier here but I don't think we want to waste a member variable just for this.
	static dev_u32 s_nFramesBetweenSirenShockingEvents = 127;
	if ((fwTimer::GetFrameCount() & s_nFramesBetweenSirenShockingEvents) == 0)
	{
		if(m_pVehicle->m_nVehicleFlags.GetIsSirenOn())
		{
			CEventShockingSiren sirenEvent(*m_pVehicle);
			CShockingEventsManager::Add(sirenEvent);
		}

		if(m_pVehicle->IsAlarmActivated())
		{
			CEventShockingCarAlarm alarmEvent(*m_pVehicle);
			CShockingEventsManager::Add(alarmEvent);
		}
	}

	// Below this line all events created are only when the player is in control of the vehicle.
	if (!m_pVehicle->GetDriver() || !m_pVehicle->GetDriver()->IsLocalPlayer() || !m_pVehicle->GetDriver()->GetControlFromPlayer())
	{
		return;
	}

	static dev_float s_fRevRatioThreshold = 0.89f;
	if (m_pVehicle->m_Transmission.GetRevRatio() > s_fRevRatioThreshold
		&& m_pVehicle->GetThrottle() > 0.9f
		&& (m_pVehicle->GetHandBrake() || m_pVehicle->GetBrake() > 0.9f)
		&& m_pVehicle->GetVehicleUpDirection().GetZf() > 0.0f
		&& m_pVehicle->m_nVehicleFlags.bEngineOn
		&& !m_pVehicle->GetBurnoutMode()
		&& m_pVehicle->GetVelocity().Mag2() < 2.0f)
	{
		if (m_pVehicle->InheritsFromAutomobile())
		{
			CAutomobile* pAutomobile = static_cast<CAutomobile*>(m_pVehicle);
			pAutomobile->m_nAutomobileFlags.bPlayerIsRevvingEngineThisFrame = true;
		}

		//only do this occasionally
		static dev_u32 s_nTimeBetweenTellOthersAboutRevMs = 600;
		if (fwTimer::GetTimeInMilliseconds() > m_nLastTimeToldOthersAboutRevEngine + s_nTimeBetweenTellOthersAboutRevMs)
		{
			m_nLastTimeToldOthersAboutRevEngine = fwTimer::GetTimeInMilliseconds();
			TellOthersAboutRevEngine();
		}
	}
}

bool CVehicleIntelligence::IsPedAffectedByHonkOrRev(const CPed& rPed, float& fDistSq, Vec3V_In vForwardTestDir, Vec3V_In vRightTestDir) const
{
	//Ensure the ped is not in a vehicle.
	if(rPed.GetIsInVehicle())
	{
		return false;
	}
	
	//Ensure the ped is in the honk zone.
	if(!IsEntityInAffectedZoneForHonkOrRev(rPed, fDistSq, vForwardTestDir, vRightTestDir))
	{
		return false;
	}
	
	return true;
}

bool CVehicleIntelligence::IsVehicleAffectedByHonkOrRev(const CVehicle& rVehicle, float& fDistSq, Vec3V_In vForwardTestDir, Vec3V_In vRightTestDir) const
{
	//Ensure we are not honking at ourself.
	if(&rVehicle == m_pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle is in the honk zone.
	if(!IsEntityInAffectedZoneForHonkOrRev(rVehicle, fDistSq, vForwardTestDir, vRightTestDir))
	{
		return false;
	}
	
	return true;
}

bool CVehicleIntelligence::IsEntityInAffectedZoneForHonkOrRev(const CEntity& rEntity, float& fDistSq, Vec3V_In vForwardTestDirIn, Vec3V_In vRightTestDirIn) const
{
	const ScalarV svZero(V_ZERO);
	Vec3V vPosDelta = rEntity.GetTransform().GetPosition() - m_pVehicle->GetVehiclePosition();

	//these are passed by reference since passing them by value doesn't align on x86-32 builds,
	//but we don't want to actually modify the params so just copy them onto the stack here
	//before we flatten them
	Vec3V vForwardTestDir = vForwardTestDirIn;
	Vec3V vRightTestDir = vRightTestDirIn;

	vPosDelta.SetZ(svZero);
	vForwardTestDir.SetZ(svZero);
	vRightTestDir.SetZ(svZero);

	//grcDebugDraw::Arrow(m_pVehicle->GetVehiclePosition(), m_pVehicle->GetVehiclePosition() + (vForwardTestDir * ScalarV(V_FIVE)), 0.5f, Color_yellow);
	//grcDebugDraw::Arrow(m_pVehicle->GetVehiclePosition(), m_pVehicle->GetVehiclePosition() + (vRightTestDir * ScalarV(V_FIVE)), 0.5f, Color_green);

	//reject anyone behind us
	if ((Dot(vPosDelta, vForwardTestDir) < svZero).Getb())
	{
		return false;
	}

	//reject anyone too far away
	static dev_float s_fMaxDistForHonkingSqr = 40.0f * 40.0f;
	const ScalarV svMaxDistForHonkingSqr(s_fMaxDistForHonkingSqr);
	const ScalarV svDistSq = MagSquared(vPosDelta);
	if ((svDistSq > svMaxDistForHonkingSqr).Getb())
	{
		return false;
	}

	//reject anyone too far off to the side
	static dev_float s_fMaxLateralDistToReceiveHonk = 6.0f;
	const ScalarV svMaxLateralDistToRecieveHonk(s_fMaxLateralDistToReceiveHonk);
	if ( (Abs(Dot(vPosDelta, vRightTestDir)) > svMaxLateralDistToRecieveHonk).Getb() )
	{
		return false;
	}
	
	fDistSq = svDistSq.Getf();
	
	return true;
}

void CVehicleIntelligence::TellOthersAboutHonk() const
{
	Assert(m_pVehicle);

	float fDistSqrToClosestCar = FLT_MAX;
	CVehicle* pClosestCarInFront = NULL;

	const Vec3V vVehicleForwardDirection	= VECTOR3_TO_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetFront());
	const Vec3V vVehicleRightDirection		= VECTOR3_TO_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetRight());

	CEntityScannerIterator vehicleList = m_pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
	{
		CVehicle* pOtherVehicle = (CVehicle*) pEntity;
		
		//Ensure the vehicle cares about the honk.
		float fDistSq;
		if(!IsVehicleAffectedByHonkOrRev(*pOtherVehicle, fDistSq, vVehicleForwardDirection, vVehicleRightDirection))
		{
			continue;
		}
		
		//Update the closest car.
		if(fDistSq < fDistSqrToClosestCar)
		{
			fDistSqrToClosestCar = fDistSq;
			pClosestCarInFront = pOtherVehicle;
		}
	}
	
	float fDistSqrToClosestPed = FLT_MAX;
	CPed* pClosestPedInFront = NULL;

	CEntityScannerIterator pedList = m_pVehicle->GetIntelligence()->GetPedScanner().GetIterator();
	for( CEntity* pEntity = pedList.GetFirst(); pEntity; pEntity = pedList.GetNext() )
	{
		CPed* pOtherPed = (CPed*) pEntity;

		//Ensure the ped cares about the honk.
		float fDistSq;
		if(!IsPedAffectedByHonkOrRev(*pOtherPed, fDistSq, vVehicleForwardDirection, vVehicleRightDirection))
		{
			continue;
		}

		//Update the closest ped.
		if(fDistSq < fDistSqrToClosestPed)
		{
			fDistSqrToClosestPed = fDistSq;
			pClosestPedInFront = pOtherPed;
		}
	}
	
	if (pClosestCarInFront)
	{
		pClosestCarInFront->GetIntelligence()->LastTimeHonkedAt = fwTimer::GetTimeInMilliseconds();
		
		//Check if the driver is valid.
		CPed* pDriver = pClosestCarInFront->GetDriver();
		if(pDriver)
		{
			//Notify the driver that they were honked at.
			pDriver->GetPedIntelligence()->HonkedAt(*m_pVehicle);
		}
	}
	
	if (pClosestPedInFront)
	{
		//Notify the ped that they were honked at.
		pClosestPedInFront->GetPedIntelligence()->HonkedAt(*m_pVehicle);
	}
}

void CVehicleIntelligence::TellOthersAboutRevEngine() const
{
	Assert(m_pVehicle);

	float fDistSqrToClosestCar = FLT_MAX;
	CVehicle* pClosestCarInFront = NULL;

	const Vec3V vForwardDir = VECTOR3_TO_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetFront());
	const Vec3V vRightDir = VECTOR3_TO_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetRight());

	CEntityScannerIterator vehicleList = m_pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
	{
		CVehicle* pOtherVehicle = (CVehicle*) pEntity;

		//Ensure the vehicle cares about the honk.
		float fDistSq;
		if(!IsVehicleAffectedByHonkOrRev(*pOtherVehicle, fDistSq, vForwardDir, vRightDir))
		{
			continue;
		}

		//Update the closest car.
		if(fDistSq < fDistSqrToClosestCar)
		{
			fDistSqrToClosestCar = fDistSq;
			pClosestCarInFront = pOtherVehicle;
		}
	}

	float fDistSqrToClosestPed = FLT_MAX;
	CPed* pClosestPedInFront = NULL;

	CEntityScannerIterator pedList = m_pVehicle->GetIntelligence()->GetPedScanner().GetIterator();
	for( CEntity* pEntity = pedList.GetFirst(); pEntity; pEntity = pedList.GetNext() )
	{
		CPed* pOtherPed = (CPed*) pEntity;

		//Only gang members are affecting by revving in this way.
		if(!pOtherPed->IsGangPed())
		{
			continue;
		}

		//Ensure the ped cares about the rev.
		float fDistSq;
		if(!IsPedAffectedByHonkOrRev(*pOtherPed, fDistSq, vForwardDir, vRightDir))
		{
			continue;
		}

		//Update the closest ped.
		if(fDistSq < fDistSqrToClosestPed)
		{
			fDistSqrToClosestPed = fDistSq;
			pClosestPedInFront = pOtherPed;
		}
	}

	if (pClosestCarInFront)
	{
		//Check if the driver is valid.
		CPed* pDriver = pClosestCarInFront->GetDriver();
		if(pDriver)
		{
			//Notify the driver that they were revved at.
			pDriver->GetPedIntelligence()->RevvedAt(*m_pVehicle);
		}
	}

	if (pClosestPedInFront)
	{
		//Notify the ped that they were revved at.
		pClosestPedInFront->GetPedIntelligence()->RevvedAt(*m_pVehicle);
	}

	// Throw a shocking event for nearby peds to react to.
	CEventShockingEngineRevved shockEvent(*m_pVehicle);
	CShockingEventsManager::Add(shockEvent);
}
//
//
//
void CVehicleIntelligence::PreUpdateVehicle()
{
	//update vehicle specific stuff
	if(m_pVehicle->InheritsFromAutomobile())
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(m_pVehicle);

		// Police cars and such will report crimes immediately
		// (No need for peds to use the phone)
		if (pAutomobile->GetStatus() != STATUS_ABANDONED && pAutomobile->GetStatus() != STATUS_WRECKED && pAutomobile->GetStatus() != STATUS_PLAYER
			/*&& GetStatus() != STATUS_PLAYER_REMOTE*/ && pAutomobile->GetStatus() != STATUS_PLAYER_DISABLED)	
		{
			if (pAutomobile->IsLawEnforcementVehicle())
			{
				pAutomobile->ScanForCrimes();
			}
		}
	}

	if(m_pVehicle->InheritsFromBike())
	{
		CBike *pBike = static_cast<CBike*>(m_pVehicle);
		if(pBike->GetDriver())
		{
			pBike->SetBikeOnSideStand(false);
			pBike->m_nBikeFlags.bGettingPickedUp = false;
		}
		else if (pBike->GetSeatManager()->CountPedsInSeats() > 0)
		{
			pBike->SetBikeOnSideStand(true);
		}
	}
}

void CVehicleIntelligence::NetworkResetMillisecondsNotMovingForRemoval()
{
	if( aiVerifyf( m_pVehicle,"NULL m_pVehicle" ) &&
		aiVerifyf( NetworkInterface::IsGameInProgress(),"%s Only expect to use this command in network games",m_pVehicle->GetDebugName() ) )
	{
		MillisecondsNotMovingForRemoval = 0;
	}
}
//
//
//
void CVehicleIntelligence::ResetVariables(float fTimeStep)
{
	float flatSpeed = m_pVehicle->GetAiVelocity().XYMag2();

	//for player vehicle, check has throttle down as otherwise causes AI pushing past player vehicle to stop when they nudge the car
	bool bNoDriverOrHasInput = !m_pVehicle->GetDriver() || !m_pVehicle->GetDriver()->IsPlayer() || m_pVehicle->GetThrottle() != 0.0f;
	if (flatSpeed > (0.1f * 0.1f) && bNoDriverOrHasInput)
	{
		LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();
	}

	if(flatSpeed >=(1.5f*1.5f) || m_pVehicle->m_nVehicleFlags.bIsThisAParkedCar)
	{
		MillisecondsNotMovingForRemoval = 0;
	}
	else
	{
		u32 timeStepInMs = (u32)(fTimeStep*1000.0f);
		MillisecondsNotMovingForRemoval += timeStepInMs;
	}
		
	//Calculate the smoothed speed.
	TUNE_GROUP_FLOAT(CAR_AI, fWeightForSmoothedSpeedPerSecond, 0.75f, 0.0f, 3.0f, 0.01f);
	float fWeightCurr = fTimeStep * fWeightForSmoothedSpeedPerSecond;
	fWeightCurr = Clamp(fWeightCurr, 0.0f, 1.0f);
	float fWeightPrev = 1.0f - fWeightCurr;
	m_fSmoothedSpeedSq = (fWeightPrev * m_fSmoothedSpeedSq) + (fWeightCurr * flatSpeed);

	//update flags
	//m_pVehicle->m_nVehicleFlags.bCopperBlockedCouldLeaveCar = false;

	SetHumaniseControls(false);
	m_pObstructingEntity = NULL;
	m_pFleeEntity = NULL;
	m_pSteeringAroundEntity = NULL;

	m_fDesiredSpeedThisFrame = -1.0f;
}

void CVehicleIntelligence::SetPretendOccupantEventDataBasedOnCurrentTask()
{
	const CTaskVehicleMissionBase* pActiveTask = GetActiveTask();
	if (!pActiveTask)
	{
		return;
	}

	if (pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_FLEE)
	{
// 		Assertf(!m_pVehicle->GetPedInSeat(0) 
// 			|| m_pVehicle->GetPedInSeat(0)->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE)
// 			, "Encountered ped driving vehicle running TASK_VEHICLE_FLEE, but not running TASK_SMART_FLEE");

		m_PretendOccupantEventPriority = VehicleEventPriority::VEHICLE_EVENT_PRIORITY_RESPOND_TO_THREAT;
		m_PretendOccupantEventParams = pActiveTask->GetParams();
	}
	else if (pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_REACT_TO_COP_SIREN)
	{
		m_PretendOccupantEventPriority = VehicleEventPriority::VEHICLE_EVENT_PRIORITY_RESPOND_TO_SIREN;
		m_PretendOccupantEventParams = pActiveTask->GetParams();
	}
	else
	{
		ResetPretendOccupantEventData();
		FlushPretendOccupantEventGroup();	//necessary?
	}
}

void CVehicleIntelligence::Process_TouchingRealMissionVehicle()
{
	Assert(m_pVehicle);

	const CCollisionHistory* pCollisionHistory = m_pVehicle->GetFrameCollisionHistory();
	if(pCollisionHistory->GetNumCollidedEntities() > 0)
	{
		const CCollisionRecord* pCurrent = pCollisionHistory->GetFirstVehicleCollisionRecord();
		while (pCurrent)
		{
			const CVehicle* pOtherVeh = (const CVehicle*)pCurrent->m_pRegdCollisionEntity.Get();

			if (pOtherVeh && !pOtherVeh->IsDummy() 
				&& (pOtherVeh->PopTypeIsMission() || (pOtherVeh->GetDriver() && pOtherVeh->GetDriver()->PopTypeIsMission())))
			{
				m_uTimeLastTouchedRealMissionVehicleWhileDummy = fwTimer::GetTimeInMilliseconds();
				break;
			}

			pCurrent = pCurrent->GetNext();
		}
	}
}

//
//
//

#if __STATS
EXT_PF_TIMER(VehicleIntelligenceProcessTask_Run);
#endif // __STATS

void CVehicleIntelligence::Process(bool fullUpdate, float fFullUpdateTimeStep)
{
	// Immediately propagate the number of cars behind us to avoid painfully slow, multi-pass, propagation
	Process_NumCarsBehindUs();

	if(!fullUpdate)
	{
		return;
	}

	// reset stuff for frame
	m_pVehicle->m_nVehicleFlags.bWarnedPeds = false;
	m_pVehicle->m_nVehicleFlags.bRestingOnPhysical = false;
	m_pVehicle->m_nVehicleFlags.bTasksAllowDummying = false;
	m_pVehicle->m_nVehicleFlags.bHasParentVehicle = m_pVehicle->GetAttachParentVehicleDummyOrPhysical() != NULL;
	m_pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing = false;		// Tasks should set this again during the imminent update, if they allow timeslicing.
	m_pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = false;
	m_pVehicle->m_nVehicleFlags.bLodShouldSkipUpdatesInTimeslicedLod = false;
	m_pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = m_pVehicle->GetIsBeingTowed();
	m_pVehicle->m_nVehicleFlags.bUnbreakableLandingGear = false;
	m_pVehicle->m_nVehicleFlags.bIsDoingHandBrakeTurn = false;
	m_pVehicle->m_nVehicleFlags.bWasStoppedForDoor = false;
	m_pVehicle->m_nVehicleFlags.bIsHesitating = false;
	m_pVehicle->m_nVehicleFlags.bDisableAvoidanceProjection = false;

	//stop for the tank, or air vehicles the player has owned or our siren is valid
	const bool bIsThreatening = m_pVehicle->IsTank() || ((m_pVehicle->InheritsFromHeli() || m_pVehicle->InheritsFromPlane()) && m_pVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer);
	if ((bIsThreatening && !m_pVehicle->GetDriver() && (m_pVehicle->m_CarGenThatCreatedUs == -1 || m_pVehicle->m_LastTimeWeHadADriver > 0)) || ShouldStopForSiren() )
	{
		m_pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	}

	if((m_pVehicle->m_Buoyancy.GetStatus() != NOT_IN_WATER) || (m_pVehicle->GetIsInWater()))
	{
		m_uLastTimeInWater = fwTimer::GetTimeInMilliseconds();
	}

	if ( m_BoatAvoidanceHelper )
	{
		m_BoatAvoidanceHelper->Process(*m_pVehicle, fFullUpdateTimeStep);
	}

	//update junctions
	//this is expensive now - we dont want to do it each frame
	const bool bUpdateJunctions = m_UpdateJunctionsDistributer.IsRegistered() && m_UpdateJunctionsDistributer.ShouldBeProcessedThisFrame();

	// Don't process controls for vehicles that are controlled by a remote machine
	if (m_pVehicle->IsNetworkClone())
    {
        Process_NearbyEntityLists();

		if (bUpdateJunctions)
		{
			m_pVehicle->UpdateJunctions();
		}

		return;
    }

	// Prevent vehicles hit from explosions from going to dummy until they are inactive
	if(m_pVehicle->m_nVehicleFlags.bInMotionFromExplosion && m_pVehicle->GetCurrentPhysicsInst() && m_pVehicle->GetCurrentPhysicsInst()->IsInLevel())
	{
		m_pVehicle->m_nVehicleFlags.bInMotionFromExplosion = CPhysics::GetLevel()->IsActive(m_pVehicle->GetCurrentPhysicsInst()->GetLevelIndex());
		m_pVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame = 
			m_pVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame || m_pVehicle->m_nVehicleFlags.bInMotionFromExplosion;
	}

	//if we were using pretend occupants and our task ended for some reason,
	//create a new default task.
	//this can happen if we transition from real->pretend occupants while pulling
	//over for a cop siren
	if (m_pVehicle->m_nVehicleFlags.bUsingPretendOccupants && !GetActiveTask()
		&& m_pVehicle->CanSetUpWithPretendOccupants())
	{
		m_pVehicle->SetUpWithPretendOccupants();
	}

	//Process the dummy flags.
	Process_DummyFlags();

	if (GetRecordingNumber() >= 0 && !CVehicleRecordingMgr::GetUseCarAI(GetRecordingNumber()))
    {
		// Invalidate the cached node list
		InvalidateCachedNodeList();
		InvalidateCachedFollowRoute();

		//vehicles on recordings that are not using ai should be assumed dirty
		m_pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

		//assume vehicle recordings always know what they're doing
		UpdateCarHasReasonToBeStopped();

		HandleEvents();

        //process the tasks even when running a car recording so convertible roofs still work.
		m_TaskManager.Process(fFullUpdateTimeStep);
    }
    else
    {
	    PreUpdateVehicle();

		if(bUpdateJunctions)
		{
			m_pVehicle->UpdateJunctions();
		}

	    //Process the car we are behind.
	    Process_CarBehind();

	    ResetVariables(fFullUpdateTimeStep);

	    Process_NearbyEntityLists();

		// Invalidate the cached node list
		InvalidateCachedNodeList();
		InvalidateCachedFollowRoute();

		UpdateTimeNotMoving(fFullUpdateTimeStep);

		HandleEvents();

	    m_TaskManager.Process(fFullUpdateTimeStep);
    }

#if !__PS3
	TUNE_GROUP_BOOL(AI_DEFERRED_TASKS, USING_ANY_DEFERRED_TASKS, true);

	if(USING_ANY_DEFERRED_TASKS)
	{
		aiDeferredTasks::TaskData taskData(NULL, m_pVehicle, fFullUpdateTimeStep, fullUpdate);
		aiDeferredTasks::g_VehicleIntelligenceProcess.Queue(taskData);
	}
	else
	{
#endif
		PF_START(VehicleIntelligenceProcessTask_Run);
		Process_OnDeferredTaskCompletion();
		PF_STOP(VehicleIntelligenceProcessTask_Run);
#if !__PS3
	}
#endif
}

void CVehicleIntelligence::Process_OnDeferredTaskCompletion()
{
	//cache off the current CVehicleNodeList, to avoid querying the task tree each time
#if __DEV
	ResetCachedNodeListHits();
	ResetCachedFollowRouteHits();
#endif //__DEV
	CacheNodeList();
	CacheFollowRoute();

	if (GetRecordingNumber() >= 0 && !CVehicleRecordingMgr::GetUseCarAI(GetRecordingNumber()))
	{
		//Clear the car we are behind.
		SetCarWeAreBehind(NULL);
	}
	else
	{
		if(sConsiderParkedVehicleForLod(m_pVehicle))
		{
			m_pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = true;
		}
    }

	Process_TouchingRealMissionVehicle();

	if (m_pVehicle->IsDummy())
	{
		//CVehicleNodeList * pNodeList = GetNodeList();
		const CVehicleFollowRouteHelper* pFollowRoute = GetFollowRouteHelper();

		Assert(m_pVehicle->GetCurrentPhysicsInst());
		Assert(!m_pVehicle->GetCurrentPhysicsInst() || m_pVehicle->GetCurrentPhysicsInst()->IsInLevel());

		const bool bTaskPreventingDummy = m_pVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame;
		const bool bTaskPreventingSuperDummy = m_pVehicle->IsSuperDummy() && m_pVehicle->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame;

		const bool bFollowRouteEmpty = (pFollowRoute &&
										m_pVehicle->GetCurrentPhysicsInst() && 
										m_pVehicle->GetCurrentPhysicsInst()->IsInLevel() &&
										CPhysics::GetLevel()->IsActive(m_pVehicle->GetCurrentPhysicsInst()->GetLevelIndex()) && 
										pFollowRoute->GetNumPoints() < 2);
		const bool bDummyConstraintsChanged = m_pVehicle->ProcessHaveDummyConstraintsChanged(ScalarV(V_TEN), true);

		if (bTaskPreventingDummy || bFollowRouteEmpty || bDummyConstraintsChanged)
		{
			//only skip intersection test if follow route is empty - for task and constraints it's not the end of the world
			//if we can't convert due to intersection but if we don't have a follow route we'll fall through the world
			const bool bSkipIntersectionTest = bFollowRouteEmpty;
			if(!m_pVehicle->InheritsFromTrailer() && !m_pVehicle->m_nVehicleFlags.bHasParentVehicle && !m_pVehicle->TryToMakeFromDummy(bSkipIntersectionTest))
			{
				if(bFollowRouteEmpty)
				{
					//couldn't switch back for some reason, if we don't have follow route then deactivate physics
					m_pVehicle->DeActivatePhysics();
				}
#if __BANK
				if(bFollowRouteEmpty)
				{
					CAILogManager::GetLog().Log("DUMMY CONVERSION: %s (%s) (0x%p) Unable to convert to real (%s). Follow route empty disabling physics.\n", 
						AILogging::GetDynamicEntityNameSafe(m_pVehicle), AILogging::GetDynamicEntityIsCloneStringSafe(m_pVehicle), m_pVehicle, m_pVehicle->GetNonConversionReason());
				}
				else
				{
					char debugText[32];
					if(bTaskPreventingDummy) sprintf(debugText,"Task preventing dummy");
					if(bDummyConstraintsChanged) sprintf(debugText,"Dummy constraint changed");
					CAILogManager::GetLog().Log("DUMMY CONVERSION: %s (%s) (0x%p) Unable to convert to real (%s). (%s)\n",AILogging::GetDynamicEntityNameSafe(m_pVehicle), 
									AILogging::GetDynamicEntityIsCloneStringSafe(m_pVehicle), m_pVehicle, m_pVehicle->GetNonConversionReason(), debugText);
				}
#endif
			}
		}
		else if (m_pVehicle->ContainsLocalPlayer())
		{
			//also try to convert from dummy if we have a local player.
			//someone in QA hit an assert from the vehicle ai lod manager update
			//where they detected a player-controlled vehicle in dummy lod after switching
			//characters. I wasn't able to repro this myself but my hunch is now that these 
			//are batched, each vehicle only attempts conversion every second or so.
			//now, if we ever detect a player-controlled vehicle that's in dummy mode,
			//just convert it right away
			const bool bSkipIntersectionTest = true;
			m_pVehicle->TryToMakeFromDummy(bSkipIntersectionTest);
		}
		else if (bTaskPreventingSuperDummy && !m_pVehicle->InheritsFromTrailer())
		{
			m_pVehicle->TryToMakeIntoDummy(VDM_DUMMY);		
		}
	}
	else if (fwTimer::GetTimeInMilliseconds() != m_pVehicle->m_TimeOfCreation && 
			 !m_pVehicle->m_nVehicleFlags.bIsThisAParkedCar && 
			 !m_pVehicle->InheritsFromTrailer() &&
			 !m_pVehicle->m_nVehicleFlags.bHasParentVehicle &&
			 !(GetRecordingNumber() >= 0 && !CVehicleRecordingMgr::GetUseCarAI(GetRecordingNumber())))
	{
		Assert(!m_pVehicle->IsDummy());
		//if we're real, and have got a valid route, but no collision loaded around us,
		//convert now or we'll either fall or freeze, neither of which looks good
		const eVehicleDummyMode currentMode = m_pVehicle->GetVehicleAiLod().GetDummyMode();
		eVehicleDummyMode overriddenMode = m_pVehicle->ProcessOverriddenDummyMode(VDM_REAL);
		if (overriddenMode != currentMode)
		{
			m_pVehicle->TryToMakeIntoDummy(overriddenMode);
		}
	}

	// Create shocking events from properties of the vehicle for other entities in the game to react against.
	GenerateEvents();

	m_eventGroup.FlushExpiredEvents();
	
	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE)
	{
		CVehicleNodeList * pNodeList = m_pCachedNodeList;
		if(pNodeList)
		{
			static dev_s32 iRandomNodeOffsetMaxExclusive = 3;
			s32 node = rage::Min(rage::Max(pNodeList->GetTargetNodeIndex()-1, 0)
				, rage::Max(pNodeList->GetTargetNodeIndex()-iRandomNodeOffsetMaxExclusive, 0) + (m_pVehicle->GetRandomSeed() & iRandomNodeOffsetMaxExclusive));
			if (!pNodeList->GetPathNodeAddr(node).IsEmpty() && ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)) &&
				ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node))->m_2.m_inTunnel )
			{
				m_pVehicle->m_nVehicleFlags.bIsInTunnel = true;
			}
			else
			{
				m_pVehicle->m_nVehicleFlags.bIsInTunnel = false;
			}
		}
		//if we don't have a nodelist, don't reset the tunnel flag
	}

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(VEH_INTELLIGENCE, ShowDummyParents, false);
	TUNE_GROUP_BOOL(VEH_INTELLIGENCE, ShowDummyChildren, false);
	TUNE_GROUP_BOOL(VEH_INTELLIGENCE, ShowCargoCars, false);
	TUNE_GROUP_BOOL(VEH_INTELLIGENCE, ShowPhysicalAttachParent, false);
	if (ShowDummyParents)
	{
		if (m_pVehicle->GetDummyAttachmentParent())
		{
			CVehicle::ms_debugDraw.AddArrow(m_pVehicle->GetVehiclePosition(), m_pVehicle->GetDummyAttachmentParent()->GetVehiclePosition(), 0.5f, Color_red);
		}
	}
	if (ShowDummyChildren)
	{
		if (m_pVehicle->HasDummyAttachmentChildren())
		{
			for (int i = 0; i < m_pVehicle->GetMaxNumDummyAttachmentChildren(); i++)
			{
				if (m_pVehicle->GetDummyAttachmentChild(i))
				{
					CVehicle::ms_debugDraw.AddArrow(m_pVehicle->GetVehiclePosition(), m_pVehicle->GetDummyAttachmentChild(i)->GetVehiclePosition(), 0.5f, Color_orange);
				}
			}
		}
	}
	if (ShowCargoCars)
	{
		if (m_pVehicle->InheritsFromTrailer())
		{
			CTrailer* pMeAsTrailer = (CTrailer*)m_pVehicle;
			if (pMeAsTrailer->HasCargoVehicles())
			{
				for (int i = 0; i < MAX_CARGO_VEHICLES; i++)
				{
					if (pMeAsTrailer->GetCargoVehicle(i))
					{
						CVehicle::ms_debugDraw.AddArrow(pMeAsTrailer->GetVehiclePosition(), pMeAsTrailer->GetCargoVehicle(i)->GetVehiclePosition(), 0.5f, Color_green);			
					}
				}
			}
		}
	}
	if (ShowPhysicalAttachParent)
	{
		if (m_pVehicle->GetAttachParent())
		{
			CVehicle::ms_debugDraw.AddArrow(m_pVehicle->GetVehiclePosition(), m_pVehicle->GetAttachParent()->GetTransform().GetPosition(), 0.5f, Color_blue);				
		}
	}
#endif //DEBUG_DRAW
}

// NAME : GetNodeList
// PURPOSE : Obtain the node list to use, by traversing task tree
// NOTE : If this function is changed, please also change CacheNodeList accordingly
CVehicleNodeList* CVehicleIntelligence::GetNodeList() const
{
	if(m_bHasCachedNodeList)
	{
		// If task has been removed, then we must try to cache a new nodelist
		if(m_pCachedNodeListTask)
		{
#if __DEV
			CVehicleIntelligence * pVehInt = const_cast<CVehicleIntelligence*>(this);
			pVehInt->m_iCacheNodeListHitsThisFrame++;
#endif
			return m_pCachedNodeList;
		}
	}

	CVehicleNodeList * pNodeList = NULL;

	if (m_pVehicle && m_pVehicle->IsNetworkClone())
	{
        pNodeList = &static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetVehicleNodeList();
	}
	else
	{
		CTaskVehicleMissionBase * pActiveTask = GetActiveTask();
		while(pActiveTask)
		{
			pNodeList = pActiveTask->GetNodeList();
			if(pNodeList)
				break;
			pActiveTask = (CTaskVehicleMissionBase*)pActiveTask->GetSubTask();
		}
	}
	return pNodeList;
}

// NAME : CacheNodeList
// PURPOSE : Caches a nodelist pointer for use outside of the task update
void CVehicleIntelligence::CacheNodeList(CTaskVehicleMissionBase* pOptionalForceTask/* = NULL*/)
{
	Assert(!m_bHasCachedNodeList);

	if (pOptionalForceTask)
	{
		m_pCachedNodeListTask = pOptionalForceTask;
		m_pCachedNodeList = pOptionalForceTask->GetNodeList();
		m_bHasCachedNodeList = true;
		return;
	}

	m_pCachedNodeList = NULL;
	m_pCachedNodeListTask = NULL;

	if (m_pVehicle && m_pVehicle->IsNetworkClone())
	{
		const CTaskVehicleMissionBase* pCloneTask = static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetCloneAITask();

		if (pCloneTask)
		{
			m_pCachedNodeList = const_cast<CTaskVehicleMissionBase*>(pCloneTask)->GetNodeList();
			m_pCachedNodeListTask = const_cast<CTaskVehicleMissionBase*>(pCloneTask);
			m_bHasCachedNodeList = true;
		}
	}
	else
	{
		CTaskVehicleMissionBase * pActiveTask = GetActiveTask();
		while(pActiveTask)
		{
			m_pCachedNodeList = pActiveTask->GetNodeList();
			if(m_pCachedNodeList)
			{
				m_pCachedNodeListTask = pActiveTask;
				m_bHasCachedNodeList = true;
				break;
			}
			pActiveTask = (CTaskVehicleMissionBase*)pActiveTask->GetSubTask();
		}
	}
}

void CVehicleIntelligence::InvalidateCachedNodeList()
{
	m_bHasCachedNodeList = false;
	m_pCachedNodeList = NULL;
	m_pCachedNodeListTask = NULL;
}

#if __ASSERT
void CVehicleIntelligence::VerifyCachedNodeList()
{
	if (m_bHasCachedNodeList)
	{
		CTaskVehicleMissionBase *pActiveTask = m_pVehicle->IsNetworkClone() ? const_cast<CTaskVehicleMissionBase*>(static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetCloneAITask()) : GetActiveTask();

		Assert(pActiveTask);
		Assert(pActiveTask == m_pCachedNodeListTask);

		if (pActiveTask)
		{
			const CVehicleNodeList *pNodeList = pActiveTask->GetNodeList();

			Assert(pNodeList == m_pCachedNodeList);
		}
	}
}
#endif // __ASSERT

void CVehicleIntelligence::InvalidateCachedFollowRoute()
{
	m_bHasCachedFollowRoute = false;
	m_pCachedFollowRouteHelper = NULL;
	m_pCachedFollowRouteHelperTask = NULL;
}

void CVehicleIntelligence::ProcessCollision(const CCollisionRecord* pColRecord)
{
	//Get the vehicle we collided with.
	CVehicle* pVehicleCollidedWith = (pColRecord && pColRecord->m_pRegdCollisionEntity && pColRecord->m_pRegdCollisionEntity->GetIsTypeVehicle()) ?
		static_cast<CVehicle *>(pColRecord->m_pRegdCollisionEntity.Get()) : NULL;
	
	if (pVehicleCollidedWith && m_pVehicle)
	{	
		if (m_pVehicle->GetModelIndex() == MI_CAR_DUNE4 || m_pVehicle->GetModelIndex() == MI_CAR_DUNE5)
		{
			static const u16 MAX_TIME_IN_AIR_TO_KEEP_REAL = 3000;

			if(pVehicleCollidedWith->GetVehicleAiLod().GetDummyMode() == VDM_SUPERDUMMY)
			{
				NetworkInterface::SetSuperDummyLaunchedIntoAir(pVehicleCollidedWith, MAX_TIME_IN_AIR_TO_KEEP_REAL);
			}			
			NetworkInterface::ForceVeryHighUpdateLevelOnVehicle(pVehicleCollidedWith, MAX_TIME_IN_AIR_TO_KEEP_REAL);
		}

		if (m_pVehicle->GetModelIndex() == MI_CAR_PHANTOM2)
		{
			static const u16 MAX_TIME_IN_MAX_UPDATE_RATE = 3000;
			NetworkInterface::ForceVeryHighUpdateLevelOnVehicle(pVehicleCollidedWith, MAX_TIME_IN_MAX_UPDATE_RATE);
		}
	}

	//Check if we collided with the player's vehicle.
	bool bCollidedWithPlayerVehicle = (pVehicleCollidedWith && pVehicleCollidedWith->ContainsLocalPlayer());
	if(bCollidedWithPlayerVehicle)
	{
		//Check if the player's vehicle is moving, or trying to move.
		static dev_float s_fMinSpeed = 0.5f;
		bool bIsPlayerVehicleMoving = (pVehicleCollidedWith->GetVelocity().Mag2() > square(s_fMinSpeed));
		static dev_float s_fMinThrottle = 0.1f;
		bool bIsPlayerVehicleTryingToMove = (Abs(pVehicleCollidedWith->GetThrottle()) > s_fMinThrottle);
		if(bIsPlayerVehicleMoving || bIsPlayerVehicleTryingToMove)
		{
			//Check if we are not colliding with the player vehicle.
			if(m_uTimeWeStartedCollidingWithPlayerVehicle == 0)
			{
				//Set the time.
				m_uTimeWeStartedCollidingWithPlayerVehicle = fwTimer::GetTimeInMilliseconds();
			}
			else
			{
				//Check if we have been colliding with the player for a while.
				static float s_fMinTimeToConsiderContinuouslyColliding = 0.25f;
				if(m_uTimeWeStartedCollidingWithPlayerVehicle + (u32)(s_fMinTimeToConsiderContinuouslyColliding * 1000.0f) < fwTimer::GetTimeInMilliseconds())
				{
					//Send the event.
					for(int i = 0; i < m_pVehicle->GetSeatManager()->GetMaxSeats(); ++i)
					{
						CPed* pPed = m_pVehicle->GetSeatManager()->GetPedInSeat(i);
						if(pPed)
						{
							CEventVehicleDamageWeapon event(m_pVehicle, pColRecord->m_pRegdCollisionEntity,
								WEAPONTYPE_RAMMEDBYVEHICLE, 0.0f, pColRecord->m_MyCollisionPosLocal, pColRecord->m_MyCollisionNormal);
							event.SetIsContinuouslyColliding(true, pVehicleCollidedWith->GetAiXYSpeed());
							pPed->GetPedIntelligence()->AddEvent(event);
						}
					}
				}
			}
		}
		else
		{
			//Clear the time.
			m_uTimeWeStartedCollidingWithPlayerVehicle = 0;
		}
	}
	else
	{
		//Clear the time.
		m_uTimeWeStartedCollidingWithPlayerVehicle = 0;
	}
}

//NOTE : If this function is changed, please also change CacheFollowRouteHelper accordingly!!!!!
const CVehicleFollowRouteHelper* CVehicleIntelligence::GetFollowRouteHelper() const
{
	const bool bHasGoodBackupRoute = m_BackupFollowRoute.GetNumPoints() >= 2;
	if(m_bHasCachedFollowRoute)
	{
		//sometimes, we can cache a followroute with zero points. we don't want to prevent caching
		//this route, as the early return here helps prevent us searching for a new one every time,
		//but if it's bad and we have a good backup followroute (most common when following
		//vehicle scenario chains in direct mode), we should prefer it here
		const bool bPreferBackupRouteToCachedRoute = bHasGoodBackupRoute 
			&& (!m_pCachedFollowRouteHelper || !m_pCachedFollowRouteHelperTask || (m_pCachedFollowRouteHelper && m_pCachedFollowRouteHelper->GetNumPoints() < 2));
		if (bPreferBackupRouteToCachedRoute)
		{
			return &m_BackupFollowRoute;
		}
		// If task has been removed, then we must try to cache a new followroute
		else if(m_pCachedFollowRouteHelperTask)
		{
#if __DEV
			CVehicleIntelligence * pVehInt = const_cast<CVehicleIntelligence*>(this);
			pVehInt->m_iCacheFollowRouteHitsThisFrame++;
#endif
			return m_pCachedFollowRouteHelper;
		}
	}

	const CVehicleFollowRouteHelper* pFollowRouteHelper = NULL;

	if (m_pVehicle && m_pVehicle->IsNetworkClone())
	{
        pFollowRouteHelper = &static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetFollowRouteHelper();
	}
	else
	{
		CTaskVehicleMissionBase * pActiveTask = GetActiveTask();
		while(pActiveTask)
		{
			if (pActiveTask->GetFollowRouteHelper() && (!pFollowRouteHelper || pActiveTask->GetFollowRouteHelper()->GetNumPoints() > 0))
			{
				pFollowRouteHelper = pActiveTask->GetFollowRouteHelper();
			}
			pActiveTask = (CTaskVehicleMissionBase*)pActiveTask->GetSubTask();
		}
	}

	//if we didn't find anything, and it looks like we might have a good backup route, return it
	if (!pFollowRouteHelper || (pFollowRouteHelper && pFollowRouteHelper->GetNumPoints() < 2 && bHasGoodBackupRoute))
	{
		pFollowRouteHelper = &m_BackupFollowRoute;
	}

	return pFollowRouteHelper;
}

// NAME : CacheFollowRoute
// PURPOSE : Caches a followroute pointer for use outside of the task update
void CVehicleIntelligence::CacheFollowRoute(CTaskVehicleMissionBase* pOptionalForceTask/* = NULL*/)
{
	Assert(!m_bHasCachedFollowRoute);

	if (pOptionalForceTask)
	{
		m_pCachedFollowRouteHelperTask = pOptionalForceTask;
		m_pCachedFollowRouteHelper = pOptionalForceTask->GetFollowRouteHelper();
		m_bHasCachedFollowRoute = true;
		return;
	}

	m_pCachedFollowRouteHelper = NULL;
	m_pCachedFollowRouteHelperTask = NULL;
	m_bHasCachedFollowRoute = false;

	if (m_pVehicle && m_pVehicle->IsNetworkClone())
	{
		const CTaskVehicleMissionBase* pCloneTask = static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetCloneAITask();

		if (pCloneTask)
		{
			m_pCachedFollowRouteHelper = const_cast<CTaskVehicleMissionBase*>(pCloneTask)->GetFollowRouteHelper();
			m_pCachedFollowRouteHelperTask = const_cast<CTaskVehicleMissionBase*>(pCloneTask);
			m_bHasCachedFollowRoute = true;
		}
	}
	else
	{
		CTaskVehicleMissionBase * pActiveTask = GetActiveTask();
		while(pActiveTask)
		{		
			if (pActiveTask->GetFollowRouteHelper() && (!m_pCachedFollowRouteHelper || pActiveTask->GetFollowRouteHelper()->GetNumPoints() > 0))
			{
				m_pCachedFollowRouteHelper = pActiveTask->GetFollowRouteHelper();
				m_pCachedFollowRouteHelperTask = pActiveTask;
				m_bHasCachedFollowRoute = true;
			}
			pActiveTask = (CTaskVehicleMissionBase*)pActiveTask->GetSubTask();
		}
	}
}

const CPathNodeRouteSearchHelper* CVehicleIntelligence::GetRouteSearchHelper() const
{
	const CPathNodeRouteSearchHelper* pRouteSearchHelper = NULL;

	if (m_pVehicle && m_pVehicle->IsNetworkClone())
	{
		const CTaskVehicleMissionBase* pCloneTask = static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetCloneAITask();

		if (pCloneTask)
		{
			pRouteSearchHelper = const_cast<CTaskVehicleMissionBase*>(pCloneTask)->GetRouteSearchHelper();
		}
	}
	else
	{
		CTaskVehicleMissionBase * pActiveTask = GetActiveTask();
		while(pActiveTask)
		{
			pRouteSearchHelper = pActiveTask->GetRouteSearchHelper();
			if(pRouteSearchHelper)
				break;
			pActiveTask = (CTaskVehicleMissionBase*)pActiveTask->GetSubTask();
		}
	}
	return pRouteSearchHelper;
}

#if __BANK

void CVehicleIntelligence::RenderDebug()
{
	char debugText[256];

	if(!m_pVehicle || !AllowCarAiDebugDisplayForThisVehicle(m_pVehicle))
	{
		return;
	}

	//Visualise a load of stuff (will slowly want to move most/all of below into here)
	CVehicleDebugVisualiser* pDebugVisualiser = CVehicleDebugVisualiser::GetInstance();
	pDebugVisualiser->Visualise(m_pVehicle);

	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());

	if(CVehicleIntelligence::m_bDisplayCarAddresses)
	{
		sprintf(debugText, "0x%p", m_pVehicle);
		grcDebugDraw::Text(vVehiclePos, Color_white, 0, 0, debugText);

#if 0 && __DEV	
		sprintf(debugText, "nodelist hits: %i", m_iCacheNodeListHitsThisFrame);
		//sprintf(debugText, "followroute hits: %i", m_iCacheFollowRouteHitsThisFrame);
		grcDebugDraw::Text(vVehiclePos, Color_green, 0, 16, debugText);		
#endif
	}

	if (CVehicleIntelligence::m_bDisplayDirtyCarPilons && m_pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag)
	{
		// Draw our position.
		Color32 col = Color_red;
		grcDebugDraw::Line(vVehiclePos, Vector3(vVehiclePos.x, vVehiclePos.y,vVehiclePos.z + 4.0f), col);
	}

	int iNoOfTexts=2;

#if __BANK//def CAM_DEBUG
	{
		if( CVehicleIntelligence::m_bAICarHandlingDetails|| CVehicleIntelligence::m_bAICarHandlingCurve )
		{
			Vector3 WorldCoors = vVehiclePos + Vector3(0,0,1.0f);
			const CAIHandlingInfo* pAIHandlingInfo = m_pVehicle->GetAIHandlingInfo();
			char debugText[255];
			sprintf(debugText, "Name: %s", pAIHandlingInfo->GetName().GetCStr());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;
			sprintf(debugText, "MinBrakeDistance: %f", pAIHandlingInfo->GetMinBrakeDistance());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;			
			sprintf(debugText, "MaxBrakeDistance: %f", pAIHandlingInfo->GetMaxBrakeDistance());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;		
			sprintf(debugText, "MaxSpeedAtBrakeDistance: %f", pAIHandlingInfo->GetMaxSpeedAtBrakeDistance());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;	
			for( s32 i = 0; i < pAIHandlingInfo->GetCurvePointCount(); i++ )
			{
				sprintf(debugText, "Curve %d: Angle %.2f Speed %.2f", i, pAIHandlingInfo->GetCurvePoint(i)->GetAngle(), pAIHandlingInfo->GetCurvePoint(i)->GetSpeed());
				grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
				iNoOfTexts++;			  
			}
			sprintf(debugText, "AbsoluteMinSpeed: %f", pAIHandlingInfo->GetAbsoluteMinSpeed());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;		
		}

		if( CVehicleIntelligence::m_bAICarHandlingCurve && CDebugScene::FocusEntities_IsInGroup(m_pVehicle) )
		{
			const CAIHandlingInfo* pAIHandlingInfo = m_pVehicle->GetAIHandlingInfo();
			// Draw some axes
			static Vector2 AXIS_ORIGIN(0.8f, 0.3f);
			static Vector2 AXIS_END(0.95f, 0.0733f);
			grcDebugDraw::Line(AXIS_ORIGIN, Vector2(AXIS_END.x, AXIS_ORIGIN.y), Color_red);
			grcDebugDraw::Text(Vector2(AXIS_END.x-0.05f, AXIS_ORIGIN.y+0.05f), Color_red, "Speed (50.0f)");
			grcDebugDraw::Line(AXIS_ORIGIN, Vector2(AXIS_ORIGIN.x, AXIS_END.y), Color_green);
			grcDebugDraw::Text(Vector2(AXIS_ORIGIN.x-0.1f, AXIS_END.y), Color_green, "Angle (180.0f)");
			Vector2 vLastPoint = AXIS_ORIGIN;
			for( s32 i = 0; i < pAIHandlingInfo->GetCurvePointCount(); i++ )
			{
				Vector2 vNewPoint;
				vNewPoint.x = AXIS_ORIGIN.x + (AXIS_END.x - AXIS_ORIGIN.x) * (pAIHandlingInfo->GetCurvePoint(i)->GetAngle()/180.0f);
				vNewPoint.y = AXIS_ORIGIN.y + (AXIS_END.y - AXIS_ORIGIN.y) * (pAIHandlingInfo->GetCurvePoint(i)->GetSpeed()/50.0f);

				if(i>0)
				{
					grcDebugDraw::Line(vLastPoint, vNewPoint, Color_blue);
				}
				grcDebugDraw::Circle(vNewPoint, 0.005f, Color_purple4);
				vLastPoint = vNewPoint; 
			}
		}
	}
#endif // #if __BANK

	if( CVehicleIntelligence::m_bDisplayVehicleRecordingInfo )
	{
		const Vector3 vVehiclePosition = vVehiclePos + Vector3(0.f,0.f,3.0f);
		int iNoOfTexts = 0;
		int recording = CVehicleRecordingMgr::GetPlaybackSlot(m_pVehicle);
		if( recording != -1 )
		{
			CVehicleStateEachFrame *pVehState =(CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(recording))[CVehicleRecordingMgr::GetPlaybackIndex(recording)]);
			sprintf(debugText, "Running vehicle recording R(%s), T(%i)", CVehicleRecordingMgr::GetRecordingName(recording), pVehState->TimeInRecording);
			grcDebugDraw::Text(vVehiclePosition, Color_green, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			sprintf(debugText, "Playback speed: %f", CVehicleRecordingMgr::GetPlaybackSpeed(recording));
			grcDebugDraw::Text(vVehiclePosition, Color_green, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			sprintf(debugText, "Using AI: %s", CVehicleRecordingMgr::GetUseCarAI(recording) ? "yes" : "no");
			grcDebugDraw::Text(vVehiclePosition, Color_green, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			Vector3 vSpeed( pVehState->GetSpeedX(), pVehState->GetSpeedY(), pVehState->GetSpeedZ() );
			sprintf(debugText, "Desired speed (inc playback speed): %.2f (%.2f)", vSpeed.Mag(), vSpeed.Mag()*CVehicleRecordingMgr::GetPlaybackSpeed(recording) );
			grcDebugDraw::Text(vVehiclePosition, Color_green, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			sprintf(debugText, "Actual speed: %.2f", m_pVehicle->GetVelocity().Mag());
			grcDebugDraw::Text(vVehiclePosition, Color_green, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			bool bOn = (CVehicleRecordingMgr::GetVehicleRecordingFlags(recording)&CVehicleRecordingMgr::VRF_ConvertToAIOnImpact_PlayerVehcile)?true:false;
			sprintf(debugText, "Flags: VRF_ConvertToAIOnImpact_PlayerVehcile(%s)", bOn?"On":"Off");
			grcDebugDraw::Text(vVehiclePosition, bOn?Color_green:Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			bOn = (CVehicleRecordingMgr::GetVehicleRecordingFlags(recording)&CVehicleRecordingMgr::VRF_ConvertToAIOnImpact_AnyVehicle)?true:false;
			sprintf(debugText, "Flags: VRF_ConvertToAIOnImpact_AnyVehicle(%s)", bOn?"On":"Off");
			grcDebugDraw::Text(vVehiclePosition, bOn?Color_green:Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			bOn = (CVehicleRecordingMgr::GetVehicleRecordingFlags(recording)&CVehicleRecordingMgr::VRF_StartEngineInstantly)?true:false;
			sprintf(debugText, "Flags: VRF_StartEngineInstantly(%s)", bOn?"On":"Off");
			grcDebugDraw::Text(vVehiclePosition, bOn?Color_green:Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			bOn = (CVehicleRecordingMgr::GetVehicleRecordingFlags(recording)&CVehicleRecordingMgr::VRF_StartEngineWithStartup)?true:false;
			sprintf(debugText, "Flags: VRF_StartEngineWithStartup(%s)", bOn?"On":"Off");
			grcDebugDraw::Text(vVehiclePosition, bOn?Color_green:Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			bOn = (CVehicleRecordingMgr::GetVehicleRecordingFlags(recording)&CVehicleRecordingMgr::VRF_ContinueRecordingIfCarDestroyed)?true:false;
			sprintf(debugText, "Flags: VRF_ContinueRecordingIfCarDestroyed(%s)", bOn?"On":"Off");
			grcDebugDraw::Text(vVehiclePosition, bOn?Color_green:Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
		else
		{
			sprintf(debugText, "Not running vehicle recording");
			grcDebugDraw::Text(vVehiclePosition, Color_red, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}

	if( CVehiclePopulation::ms_bDisplayAbandonedRemovalDebug )
	{
		// Gather the relevant information to display (see CVehiclePopulation::PossiblyRemoveVehicle )
		bool bIsOnScreen = m_pVehicle->GetIsVisibleInSomeViewportThisFrame();
		bool bHasPedsInIt = m_pVehicle->HasPedsInIt();
		bool bIsAParkedCar = m_pVehicle->m_nVehicleFlags.bIsThisAParkedCar;
		// Get a handle to the vehicle seat manager
		const CSeatManager* pSeatManager = m_pVehicle->GetSeatManager();
		bool bHasEverHadPedInAnySeat = pSeatManager && pSeatManager->HasEverHadPedInAnySeat();
		// Check for the former peds to see if any ped is still alive
		bool bPedFound = false;
		CPed* pLastPedInSeat = NULL;
		if( bHasEverHadPedInAnySeat && !bHasPedsInIt )
		{
			for(int iSeat = 0; iSeat < pSeatManager->GetMaxSeats() && !bPedFound; iSeat++)
			{
				pLastPedInSeat = pSeatManager->GetLastPedInSeat(iSeat);
				if( pLastPedInSeat != NULL && !pLastPedInSeat->IsDead() )
				{
					bPedFound = true;
				}
			}
		}
		bool bShouldUseCullRangeOffScreenPedsRemoved = !bIsOnScreen && !bIsAParkedCar && !bHasPedsInIt && bHasEverHadPedInAnySeat && !bPedFound;

		const Vector3 vVehiclePosition = vVehiclePos + Vector3(0.f,0.f,3.0f);
		int iNoOfTexts = 0;
		sprintf(debugText, "OnScreen: %s", bIsOnScreen ? "true" : "false");		
		grcDebugDraw::Text(vVehiclePosition, bIsOnScreen ? Color_green : Color_red, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "Parked: %s", bIsAParkedCar ? "true" : "false");
		grcDebugDraw::Text(vVehiclePosition, bIsAParkedCar ? Color_green : Color_red, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "HasPedsInIt: %s", bHasPedsInIt ? "true" : "false");
		grcDebugDraw::Text(vVehiclePosition, bHasPedsInIt ? Color_green : Color_red, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "HasEverHadPedsInIt: %s", bHasEverHadPedInAnySeat ? "true" : "false");
		grcDebugDraw::Text(vVehiclePosition, bHasEverHadPedInAnySeat ? Color_red : Color_green, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		if( bPedFound )
		{
			sprintf(debugText, "PedFound: true [0x%p]", pLastPedInSeat);
			grcDebugDraw::Line(vVehiclePosition, VEC3V_TO_VECTOR3(pLastPedInSeat->GetTransform().GetPosition()), Color_green, Color_green);
		}
		else
		{
			sprintf(debugText, "PedFound: false");
		}
		grcDebugDraw::Text(vVehiclePosition, bPedFound ? Color_green : Color_red, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "Special cull range applies[%.1f]: %s", CVehiclePopulation::GetCullRangeOffScreenPedsRemoved(), bShouldUseCullRangeOffScreenPedsRemoved ? "true" : "false");
		grcDebugDraw::Text(vVehiclePosition, bShouldUseCullRangeOffScreenPedsRemoved ? Color_green : Color_red, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		
	}

	if (CVehicleIntelligence::m_bDisplayInterestingDrivers)
	{
		const Vector3 vVehiclePosition = vVehiclePos + Vector3(0.f,0.f,3.0f);
		char sInterestingDriverType[64];
		const CPed* pDriver = m_pVehicle->GetDriver();
		bool bDrawAnything = false;
		if (m_pVehicle->m_nVehicleFlags.bMadDriver)
		{
			sprintf(sInterestingDriverType, "Aggressive Driver");
			bDrawAnything = true;
		}
		else if (pDriver && pDriver->GetPedIntelligence()->GetDriverAbilityOverride() < SMALL_FLOAT 
			&& pDriver->GetPedIntelligence()->GetDriverAggressivenessOverride() < SMALL_FLOAT
			&& pDriver->GetPedIntelligence()->GetDriverAbilityOverride() >= 0.0f
			&& pDriver->GetPedIntelligence()->GetDriverAggressivenessOverride() >= 0.0f)
		{
			sprintf(sInterestingDriverType, "Sunday Driver");
			bDrawAnything = true;
		}
		else if (pDriver && pDriver->GetPedIntelligence()->GetDriverAbilityOverride() > 0.99f 
			&& pDriver->GetPedIntelligence()->GetDriverAggressivenessOverride() > 0.99f )
		{
			sprintf(sInterestingDriverType, "Pro Driver");
			bDrawAnything = true;
		}

		if (bDrawAnything)
		{
			sprintf(debugText, "%s", sInterestingDriverType);
			grcDebugDraw::Text( vVehiclePosition, Color_purple, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}

	if(CVehicleIntelligence::m_bDisplayDebugInfoOkToDropOffPassengers)
	{
		Vector3 Crs = vVehiclePos;
		Crs.z += 3.0f;

		if(m_pVehicle->GetIntelligence()->OkToDropOffPassengers())
		{
			grcDebugDraw::Text(Crs, CRGBA(255, 255, 255, 255), "OkToDropOffPassengers");
		}
		else
		{
			grcDebugDraw::Text(Crs, CRGBA(255, 0, 0, 255), "NOTOkToDropOffPassengers");
		}
	}

	if(CVehicleIntelligence::m_bDisplayDebugJoinWithRoadSystem)
	{
		Vector3 Crs = vVehiclePos;
		Crs.z += 2.5f;

		u32	timeAgo = fwTimer::GetTimeInMilliseconds() - m_pVehicle->GetIntelligence()->lastTimeJoinedWithRoadSystem;
		u32	timeAgo2 = fwTimer::GetTimeInMilliseconds() - m_pVehicle->GetIntelligence()->lastTimeActuallyJoinedWithRoadSystem;
		if(timeAgo < 10000)
		{
			if(timeAgo == timeAgo2)
			{
				sprintf(debugText, "Actually calculated new nodes %d", timeAgo);
				grcDebugDraw::Text(Crs, CRGBA(255, 0, 255, 255), debugText);
			}
			else
			{
				sprintf(debugText, "Requested new nodes %d", timeAgo);
				grcDebugDraw::Text(Crs, CRGBA(128, 128, 128, 255), debugText);
			}
		}
	}
	
	if(CVehicleIntelligence::m_bDisplayAvoidanceCache)
	{
		//Calculate the text position.
		Vector3 vTextPosition = vVehiclePos;
		vTextPosition.z += 3.0f;
		
		//Count the lines.
		int iNoOfTexts = 0;
		
		//Display the target.
		formatf(debugText, "Target: 0x%p", m_AvoidanceCache.m_pTarget.Get());
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		
		//Display the desired offset.
		formatf(debugText, "Desired Offset: %.2f", m_AvoidanceCache.m_fDesiredOffset);
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}

#if __BANK
	if (CVehicleIntelligence::m_bDisplayRouteInterceptionTests)
	{
		const u32 uNowTS = fwTimer::GetTimeInMilliseconds();
		const u32 uNumInterceptionTests = m_aDEBUG_RouteInterceptionTests.GetCount();

		if (uNumInterceptionTests > 0)
		{
			const SDEBUG_RouteInterceptionTest& rLastTest = m_aDEBUG_RouteInterceptionTests[uNumInterceptionTests - 1];
			
			formatf(debugText, "LRIT TS: %.2f", (uNowTS - rLastTest.uTS) / 1000.0f);
			Vector3 vTextPosition = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
			vTextPosition.z += 3.0f;
			grcDebugDraw::Text(vTextPosition, Color_blue, 0, grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		for (u32 uInterceptionTestIdx = 0; uInterceptionTestIdx < uNumInterceptionTests; ++uInterceptionTestIdx)
		{
			const SDEBUG_RouteInterceptionTest& rTest = m_aDEBUG_RouteInterceptionTests[uInterceptionTestIdx];
			
			const u32 uNumChecks = rTest.aChecks.GetCount();
			const bool bShowTest = !m_bHideFailedRouteInterceptionTests || ((uNumChecks > 0) && (rTest.aChecks[uNumChecks - 1].bInterception));

			if (bShowTest && (rTest.uTS != 0) && (rTest.uTS + uDEBUG_MAX_TIME_TO_SHOW_ROUTE_INTERCEPTION_TEST_MS > uNowTS))
			{
				Color32 colVehicleInfoColor = Color_blue;
				Color32 colTargetInfoColor = Color_yellow;

				const int iAlpha = (rTest.uTS + uDEBUG_MAX_TIME_TO_SHOW_ROUTE_INTERCEPTION_TEST_MS - uNowTS) * 255 / uDEBUG_MAX_TIME_TO_SHOW_ROUTE_INTERCEPTION_TEST_MS;
				colVehicleInfoColor.SetAlpha(iAlpha);
				colTargetInfoColor.SetAlpha(iAlpha);

				Vec3V vPrevVehicleDrawPos(V_ZERO);
				Vec3V vPrevEntityDrawPos(V_ZERO);

				for (u32 uCheckIdx = 0; uCheckIdx < uNumChecks; ++uCheckIdx)
				{
					const SDEBUG_RouteInterceptionTest::SCheck& rCheck = rTest.aChecks[uCheckIdx];

					ScalarV scHighestPosZ = Max(rCheck.vVehiclePos.GetZ(), rCheck.vEntityPos.GetZ());

					Vec3V vVehicleDrawPos = rCheck.vVehiclePos;
					vVehicleDrawPos.SetZ(scHighestPosZ);

					static const ScalarV scVELOCITY_DRAW_SCALE(0.1f);
					static const float fVELOCITY_ARROW_HEAD_SIZE = 0.5f;

					grcDebugDraw::Sphere(vVehicleDrawPos, 1.0f, colVehicleInfoColor, false);
					grcDebugDraw::Arrow(vVehicleDrawPos, vVehicleDrawPos + Vec3V(rCheck.vVehicleVelXY, ScalarV(V_ZERO)) * scVELOCITY_DRAW_SCALE, fVELOCITY_ARROW_HEAD_SIZE, colVehicleInfoColor);
					Vector3 vTextPosition = VEC3V_TO_VECTOR3(vVehicleDrawPos);
					vTextPosition.z += 1.0f;
					formatf(debugText, "ST: %.2f", rCheck.fTime);
					grcDebugDraw::Text(vTextPosition, colVehicleInfoColor, 0, grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

					Vec3V vEntityDrawPos = rCheck.vEntityPos;
					vEntityDrawPos.SetZ(scHighestPosZ);

					grcDebugDraw::Sphere(vEntityDrawPos, 1.0f, colTargetInfoColor, false);
					grcDebugDraw::Arrow(vEntityDrawPos, vEntityDrawPos + Vec3V(rCheck.vEntityVelXY, ScalarV(V_ZERO)) * scVELOCITY_DRAW_SCALE, fVELOCITY_ARROW_HEAD_SIZE, colTargetInfoColor);
					vTextPosition = VEC3V_TO_VECTOR3(vEntityDrawPos);
					vTextPosition.z += 1.0f;
					formatf(debugText, "ST: %.2f", rCheck.fTime);
					grcDebugDraw::Text(vTextPosition, colTargetInfoColor, 0, grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

					Color32 colCheckColor = rCheck.bInterception ? Color_red : Color_green;
					colCheckColor.SetAlpha(iAlpha);

					grcDebugDraw::Line(vVehicleDrawPos, vEntityDrawPos, colCheckColor, colCheckColor);

					vTextPosition = VEC3V_TO_VECTOR3(vVehicleDrawPos + vEntityDrawPos) * 0.5f;
					vTextPosition.z += 1.0f;
					if (uCheckIdx == uNumChecks - 1)
					{
						formatf(debugText, "WD: %.2f, LDX: %.2f, LDY: %.2f, RouteAv: %d, RoutePass: %d", rCheck.scDistanceXY.Getf(), rCheck.scDistanceLocalX.Getf(), rCheck.scDistanceLocalY.Getf(), rTest.bRouteAvailable, rTest.bRoutePassByEntityPos);
					}
					else
					{
						formatf(debugText, "Dt: %.2f, LDX: %.2f, LDY: %.2f", rCheck.scDistanceXY.Getf(), rCheck.scDistanceLocalX.Getf(), rCheck.scDistanceLocalY.Getf());
					}
					grcDebugDraw::Text(vTextPosition, colCheckColor, 0, grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

					if (uCheckIdx > 0)
					{
						grcDebugDraw::Line(vPrevVehicleDrawPos, vVehicleDrawPos, colVehicleInfoColor, colVehicleInfoColor);
						grcDebugDraw::Line(vPrevEntityDrawPos, vEntityDrawPos, colTargetInfoColor, colTargetInfoColor);
					}

					vPrevVehicleDrawPos = vVehicleDrawPos;
					vPrevEntityDrawPos = vEntityDrawPos;
				}
			}
		}
	}
#endif // __BANK
}

#endif	// __BANK


#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetMissionName
// PURPOSE :  
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
const char *CVehicleIntelligence::GetMissionName(s32 Mission)
{

	Assert(Mission >= 0 && Mission < MISSION_LAST);

	return (MissionStrings[Mission]);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetDrivingFlagName
// PURPOSE :  
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
const char	* CVehicleIntelligence::GetDrivingFlagName( s32 flag )
{
	if( flag == DF_StopForCars ) return "DF_StopForCars";
	if( flag == DF_StopForPeds ) return "DF_StopForPeds";
	if( flag == DF_SwerveAroundAllCars ) return "DF_SwerveAroundAllCars";
	if( flag == DF_SteerAroundStationaryCars ) return "DF_SteerAroundStationaryCars";
	if( flag == DF_SteerAroundPeds ) return "DF_SteerAroundPeds";
	if( flag == DF_SteerAroundObjects ) return "DF_SteerAroundObjects";
	if( flag == DF_DontSteerAroundPlayerPed ) return "DF_DontSteerAroundPlayerPed";
	if( flag == DF_StopAtLights ) return "DF_StopAtLights";
	if( flag == DF_GoOffRoadWhenAvoiding) return "DF_GoOffRoadWhenAvoiding";
	if( flag == DF_DriveIntoOncomingTraffic ) return "DF_DriveIntoOncomingTraffic";
	if( flag == DF_DriveInReverse ) return "DF_DriveInReverse";
	if( flag == DF_UseWanderFallbackInsteadOfStraightLine) return "DF_UseWanderFallbackInsteadOfStraightLine";
	if( flag == DF_AvoidRestrictedAreas ) return "DF_AvoidRestrictedAreas";
	if( flag == DF_PreventBackgroundPathfinding ) return "DF_PreventBackgroundPathfinding";
	if( flag == DF_AdjustCruiseSpeedBasedOnRoadSpeed ) return "DF_AdjustCruiseSpeedBasedOnRoadSpeed";
	if( flag == DF_PreventJoinInRoadDirectionWhenMoving ) return "DF_PreventJoinInRoadDirectionWhenMoving";
	if( flag == DF_DontAvoidTarget ) return "DF_DontAvoidTarget";
	if( flag == DF_TargetPositionOverridesEntity ) return "DF_TargetPositionOverridesEntity";
	if( flag == DF_UseShortCutLinks ) return "DF_UseShortCutLinks";
	if( flag == DF_ChangeLanesAroundObstructions ) return "DF_ChangeLanesAroundObstructions";
	if( flag == DF_AvoidTargetCoors ) return "DF_AvoidTargetCoors";
	if( flag == DF_UseSwitchedOffNodes ) return "DF_UseSwitchedOffNodes";
	if( flag == DF_PreferNavmeshRoute ) return "DF_PreferNavmeshRoute";
	if( flag == DF_PlaneTaxiMode) return "DF_PlaneTaxiMode";
	if( flag == DF_ForceStraightLine) return "DF_ForceStraightLine";
	if( flag == DF_UseStringPullingAtJunctions) return "DF_UseStringPullingAtJunctions";
	if( flag == DF_AvoidAdverseConditions) return "DF_AvoidAdverseConditions";
	if( flag == DF_AvoidTurns) return "DF_AvoidTurns";
	if( flag == DF_ExtendRouteWithWanderResults) return "DF_ExtendRouteWithWanderResults";
	if( flag == DF_AvoidHighways) return "DF_AvoidHighways";
	if( flag == DF_ForceJoinInRoadDirection) return "DF_ForceJoinInRoadDirection";
	if( flag == DF_DontTerminateTaskWhenAchieved ) return "DF_DontTerminateTaskWhenAchieved";

	return "Unknown driving flag!";
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetTempActName
// PURPOSE :  
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
const char *CVehicleIntelligence::GetTempActName(s32 TempAct)
{

	Assert(TempAct >= 0 && TempAct < TEMPACT_LAST);

	return (TempActStrings[TempAct]);
}

const char *CVehicleIntelligence::GetJunctionCommandName(s32 JunctionCommand)
{
	Assert(JunctionCommand >= 0 && JunctionCommand < JUNCTION_COMMAND_LAST);
	return (JunctionCommandStrings[JunctionCommand]);
}
const char *CVehicleIntelligence::GetJunctionFilterName(s32 JunctionFilter)
{
	Assert(JunctionFilter >= 0 && JunctionFilter < JUNCTION_FILTER_LAST);
	return (JunctionFilterStrings[JunctionFilter]);
}
const char *CVehicleIntelligence::GetJunctionCommandShortName(s32 JunctionCommand)
{
	Assert(JunctionCommand >= 0 && JunctionCommand < JUNCTION_COMMAND_LAST);
	return (JunctionCommandStringsShort[JunctionCommand]);
}
const char *CVehicleIntelligence::GetJunctionFilterShortName(s32 JunctionFilter)
{
	Assert(JunctionFilter >= 0 && JunctionFilter < JUNCTION_FILTER_LAST);
	return (JunctionFilterStringsShort[JunctionFilter]);
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : OkToDropOffPassengers
// PURPOSE :  Returns true if the car is in a good place to drop off passengers.
//			  No junctions, no one way, no higher speed nodes.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleIntelligence::OkToDropOffPassengers()
{
	CVehicleNodeList * pNodeList = GetNodeList();
	Assertf(pNodeList, "CVehicleIntelligence::OkToDropOffPassengers - vehicle has no node list");
	if(!pNodeList)
		return true;
	s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;
	s32 iFutureNode = Clamp(iOldNode + 5, 0, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED);

	for (s32 node = iOldNode; node < iFutureNode; node++)
	{
		if (pNodeList->GetPathNodeAddr(node).IsEmpty() || !ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)))
		{
			return false;
		}

		const CPathNode *pNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node));

		// Only on straight bit of road (no junctions)
		if (pNode->NumLinks() != 2)
		{
			return false;
		}

		// No special nodes
		if (pNode->HasSpecialFunction())
		{
			return false;
		}

		// No fast nodes (probably motorway or somesuch)
		if (pNode->m_2.m_speed >= 2)
		{
			return false;
		}

		CPathNodeLink * pLink = ThePaths.FindLinkPointerSafe(pNodeList->GetPathNodeAddr(node).GetRegion(),pNodeList->GetPathLinkIndex(node));

		// Only for roads with two way traffic (off-ramps and motorways are single direction)
		if (!pLink || pLink->m_1.m_LanesFromOtherNode == 0)
		{
			return false;
		}
	}
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsOnHighway
// PURPOSE :  Returns true if the car is on a highway or a road where cars drive fast
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleIntelligence::IsOnHighway() const
{
	CVehicleNodeList * pNodeList = GetNodeList();
	if(!pNodeList)
		return false;

	s32 iFutureNode = pNodeList->GetTargetNodeIndex() + 1;
	s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;

	s32 iIterStart = rage::Max(iOldNode, 0);
	s32 iIterEnd = rage::Min(iFutureNode, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
	for (s32 node = iIterStart; node <= iIterEnd; node++)
	{
		if (pNodeList->GetPathNodeAddr(node).IsEmpty() || !ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)))
		{
			continue;
		}

		const CPathNode *pNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node));

		if (pNode->IsHighway())
		{
			return true;
		}

		if (pNode->m_2.m_speed >= 2)
		{
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsOnSingleTrackRoad
// PURPOSE :  Returns true if the car is on a single track road--ie both directions of traffic travel down the same lane
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleIntelligence::IsOnSingleTrackRoad() const
{
	CVehicleNodeList * pNodeList = GetNodeList();
	if(!pNodeList)
		return false;

	s32 iFutureNode = pNodeList->GetTargetNodeIndex() + 1;
	s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;
	s32 iIterStart = rage::Max(iOldNode, 0);
	s32 iIterEnd = rage::Min(iFutureNode, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);

	for (s32 node = iIterStart; node < iIterEnd; node++)
	{
		s32 nextNode = node+1;
		if (pNodeList->GetPathNodeAddr(node).IsEmpty() || !ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)))
		{
			continue;
		}

		s16 iLink = -1;
		const bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(pNodeList->GetPathNodeAddr(node), pNodeList->GetPathNodeAddr(nextNode), iLink);
		if (bLinkFound)
		{
			const CPathNode *pNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node));
			const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pNode, iLink);
			if (pLink->IsSingleTrack())
			{
				return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsOnSmallRoad
// PURPOSE :  Returns true if the car is on a single track road--ie both directions of traffic travel down the same lane
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleIntelligence::IsOnSmallRoad() const
{
	CVehicleNodeList * pNodeList = GetNodeList();
	if(!pNodeList)
		return false;

	s32 iFutureNode = pNodeList->GetTargetNodeIndex() + 1;
	s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;
	s32 iIterStart = rage::Max(iOldNode, 0);
	s32 iIterEnd = rage::Min(iFutureNode, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);

	for (s32 node = iIterStart; node < iIterEnd; node++)
	{
		s32 nextNode = node+1;
		if (pNodeList->GetPathNodeAddr(node).IsEmpty() || !ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)))
		{
			continue;
		}

		s16 iLink = -1;
		const bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(pNodeList->GetPathNodeAddr(node), pNodeList->GetPathNodeAddr(nextNode), iLink);
		if (bLinkFound)
		{
			const CPathNode *pNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node));
			const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pNode, iLink);
			if (pLink->m_1.m_NarrowRoad || pLink->IsSingleTrack())
			{
				return true;
			}

			if (pLink->m_1.m_LanesToOtherNode <= 1 && pLink->m_1.m_LanesFromOtherNode <= 1)
			{
				return true;
			}
		}
	}

	return false;
}

bool CVehicleIntelligence::CheckRouteInterception(Vec3V_In vEntityPos, Vec3V_In vEntityVelocity, float fMaxSideDistance, float fMaxForwardDistance, float fMinTime, float fMaxTime) const
{
#if __BANK
	SDEBUG_RouteInterceptionTest test;
#endif // __BANK

	// Interception check output
	bool bInterception = false;
	ScalarV scInterceptionT(V_ZERO);
	Vec3V vInterceptionVehicleWorldPos(V_ZERO);
	Vec3V vInterceptionEntityWorldPos(V_ZERO);
	ScalarV scInterceptionDistanceLocalX(V_ZERO);
	ScalarV scInterceptionDistanceLocalY(V_ZERO);

	// Grab vehicle / entity info.
	const Vec2V vVehicleForwardXY = m_pVehicle->GetTransform().GetForward().GetXY();
	const Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(m_pVehicle->GetVelocity());
	const Vec2V vVehicleVelocityXY = vVehicleVelocity.GetXY();
	const ScalarV scVehicleSpeedXY = Mag(vVehicleVelocityXY);
	const Vec2V vEntityVelocityXY = vEntityVelocity.GetXY();

	// Generate the squared distance threshold / time threshold.
	const ScalarV scMaxSideDistance = ScalarVFromF32(fMaxSideDistance);
	const ScalarV scMaxForwardDistance = ScalarVFromF32(fMaxForwardDistance);
	const ScalarV scMinTime(fMinTime);
	const ScalarV scMaxTime(fMaxTime);

#if __BANK
	test.AddCheck(0.0f, false, vEntityPos, vEntityVelocityXY, m_pVehicle->GetTransform().GetPosition(), vVehicleVelocityXY, MagXY(vEntityPos - m_pVehicle->GetTransform().GetPosition()), ScalarV(V_NEGONE), ScalarV(V_NEGONE));
#endif // __BANK		

	// Check route
	CVehicleNodeList* pNodeList = GetNodeList();
	if (pNodeList)
	{
#if __BANK
		test.bRouteAvailable = true;
#endif // __BANK		

		//Grab the current node.
		s32 iCurNode = pNodeList->GetTargetNodeIndex();
		Assertf(iCurNode >= 0 && iCurNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED, "Current road node is invalid.");

		//Iterate over the road nodes checking intersection to the route in each segment.
		ScalarV scSimulatedTime = ScalarV(V_ZERO);
		Vec3V vVehiclePredictedPos = m_pVehicle->GetTransform().GetPosition();
		Vec3V vEntityPredictedPos = vEntityPos;

		static CNodeAddress iNodes[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];
		static const CPathNode * pStoredNodes[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];

		for(s32 n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
		{
			iNodes[n] = pNodeList->GetPathNodeAddr(n);
			pStoredNodes[n] = ThePaths.FindNodePointerSafe(iNodes[n]);
		}

		for (s32 iNode = iCurNode; !bInterception && IsLessThanOrEqualAll(scSimulatedTime, scMaxTime) && (iNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED); ++iNode)
		{
			// Grab the coordinates of the next node
			bool bNodeOk = false;
			Vector3 vNodePos;
			if (iNode < 1)
			{
				if (pStoredNodes[iNode])
				{
					pStoredNodes[iNode]->GetCoors(vNodePos);
					bNodeOk = true;
				}
			}
			else if (pStoredNodes[iNode] && pStoredNodes[iNode-1])
			{
				if(iNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED - 1)
				{
					FindTargetCoorsWithNode(m_pVehicle, iNodes[iNode - 1], iNodes[iNode], iNodes[iNode + 1],
						pNodeList->GetPathLinkIndex(iNode - 1), pNodeList->GetPathLinkIndex(iNode),
						pNodeList->GetPathLaneIndex(iNode), pNodeList->GetPathLaneIndex(iNode + 1), vNodePos);
				}
				else
				{
					FindTargetCoorsWithNode(m_pVehicle, iNodes[iNode - 1], iNodes[iNode], EmptyNodeAddress,
						pNodeList->GetPathLinkIndex(iNode - 1), 0,
						pNodeList->GetPathLaneIndex(iNode), 0, vNodePos);
				}
				
				bNodeOk = true;
			}

			if (!bNodeOk)
			{
				break;
			}

			// Calculate interception point T (closest point between vehicle and entity paths)
			ScalarV scSegmentTime(V_ZERO);
			Vec2V vSegmentVehicleVelocityXY(V_ZERO);

			// If the segment has not zero length...
			const Vec2V vSegmentDeltaXY = (VECTOR3_TO_VEC3V(vNodePos) - vVehiclePredictedPos).GetXY();
			const ScalarV scSegmentLengthXYSquared = MagSquared(vSegmentDeltaXY);
			if (IsGreaterThanAll(scSegmentLengthXYSquared, ScalarV(V_ZERO)))
			{
				// ...we calculate the max segment time / vehicle velocity dir
				const ScalarV scSegmentLengthXY = Sqrt(scSegmentLengthXYSquared);

				const ScalarV scMaxSegmentTime = Max(ScalarV(V_ZERO), scMaxTime - scSimulatedTime);
				scSegmentTime = Min(scMaxSegmentTime, IsGreaterThanAll(scVehicleSpeedXY, ScalarV(V_ZERO)) ? scSegmentLengthXY / scVehicleSpeedXY : ScalarV(V_FLT_LARGE_8));

				const Vec2V vSegmentDirXY = vSegmentDeltaXY / scSegmentLengthXY;
				vSegmentVehicleVelocityXY = vSegmentDirXY * scVehicleSpeedXY;
			}

			bInterception = CheckEntityInterceptionXY(vVehiclePredictedPos, vVehicleForwardXY, vSegmentVehicleVelocityXY, vEntityPredictedPos, vEntityVelocityXY, scMaxSideDistance, scMaxForwardDistance, scSegmentTime,
				&scInterceptionT, &vInterceptionVehicleWorldPos, &vInterceptionEntityWorldPos, &scInterceptionDistanceLocalX, &scInterceptionDistanceLocalY);

			const ScalarV scTotalInterceptionT = scSimulatedTime + scInterceptionT;
			bInterception = bInterception && IsGreaterThanOrEqualAll(scTotalInterceptionT, scMinTime);

			if (!bInterception)
			{
				scSimulatedTime  += scSegmentTime;
				vEntityPredictedPos  += scSegmentTime * Vec3V(vEntityVelocityXY, ScalarV(V_ZERO));
				vVehiclePredictedPos += scSegmentTime * Vec3V(vSegmentVehicleVelocityXY, ScalarV(V_ZERO));
#if __BANK
				test.AddCheck(scSimulatedTime.Getf(), false, vEntityPredictedPos, vEntityVelocityXY, vVehiclePredictedPos, vSegmentVehicleVelocityXY, MagXY(vEntityPredictedPos - vVehiclePredictedPos), ScalarV(V_NEGONE), ScalarV(V_NEGONE));
#endif // __BANK	
			}
#if __BANK
			else
			{
				test.AddCheck((scSimulatedTime + scInterceptionT).Getf(), true, vInterceptionEntityWorldPos, vEntityVelocityXY, vInterceptionVehicleWorldPos, vSegmentVehicleVelocityXY, MagXY(vInterceptionEntityWorldPos - vInterceptionVehicleWorldPos), scInterceptionDistanceLocalX, scInterceptionDistanceLocalY);
			}
#endif // __BANK	

		}
	}
	else
	{
		// No node list -> we calculate with the current positions and velocities	
		bInterception = CheckEntityInterceptionXY(m_pVehicle->GetTransform().GetPosition(), vVehicleForwardXY, vVehicleVelocityXY, vEntityPos, vEntityVelocityXY, scMaxSideDistance, scMaxForwardDistance, scMaxTime,
							&scInterceptionT, &vInterceptionVehicleWorldPos, &vInterceptionEntityWorldPos, &scInterceptionDistanceLocalX, &scInterceptionDistanceLocalY);
		
		bInterception = bInterception && IsGreaterThanOrEqualAll(scInterceptionT, scMinTime);
#if __BANK
		test.bRouteAvailable = false;

		if (bInterception)
		{
			test.AddCheck(scInterceptionT.Getf(), true, vInterceptionEntityWorldPos, vEntityVelocityXY, vInterceptionVehicleWorldPos, vVehicleVelocityXY, MagXY(vInterceptionEntityWorldPos - vInterceptionVehicleWorldPos), scInterceptionDistanceLocalX, scInterceptionDistanceLocalY);
		}
		else
		{
			Vec3V vEntityPredictedPos  = vEntityPos								  + scMaxTime * Vec3V(vEntityVelocityXY, ScalarV(V_ZERO));
			Vec3V vVehiclePredictedPos = m_pVehicle->GetTransform().GetPosition() + scMaxTime * Vec3V(vVehicleVelocityXY, ScalarV(V_ZERO));

			test.AddCheck(scMaxTime.Getf(), false, vEntityPredictedPos, vEntityVelocityXY, vVehiclePredictedPos, vVehicleVelocityXY, MagXY(vEntityPredictedPos - vVehiclePredictedPos), ScalarV(V_NEGONE), ScalarV(V_NEGONE));
		}
#endif // __BANK	
	}

#if __BANK
	test.uTS = fwTimer::GetTimeInMilliseconds();
	// We save the previous test to compare
	test.bRoutePassByEntityPos = DoesRoutePassByPosition(vEntityPos, CTaskSmartFlee::sm_Tunables.m_ExitVehicleRouteMinDistance, CTaskSmartFlee::sm_Tunables.m_ExitVehicleMaxDistance);

	m_aDEBUG_RouteInterceptionTests.Append(test);
#endif // __BANK

	return bInterception;
}

bool CVehicleIntelligence::CheckEntityInterceptionXY(Vec3V_In vVehiclePos, Vec2V_In vVehicleForwardXY, Vec2V_In vVehicleVelocityXY, Vec3V_In vEntityPos, Vec2V_In vEntityVelocityXY, ScalarV_In scMaxSideDistance, ScalarV_In scMaxForwardDistance, ScalarV_In scMaxTime,
	ScalarV_Ptr pscInterceptionT, Vec3V_Ptr pvInterceptionVehPos, Vec3V_Ptr pvInterceptionEntityPos, ScalarV_Ptr pscInterceptionDistanceLocalX, ScalarV_Ptr pscInterceptionDistanceLocalY) const
{
	aiAssert(pscInterceptionT && pvInterceptionVehPos && pvInterceptionEntityPos && pscInterceptionDistanceLocalX && pscInterceptionDistanceLocalY);

	// No node list -> we calculate with the current positions and velocities
	ScalarV scInterceptionT(V_ZERO);

	const ScalarV scVehicleSpeedXYSquared(MagSquared(vVehicleVelocityXY));
	const Vec2V vVehicleMoveDirXY = IsGreaterThanAll(scVehicleSpeedXYSquared, ScalarV(V_ZERO)) ? vVehicleVelocityXY / Sqrt(scVehicleSpeedXYSquared) : vVehicleForwardXY;

	const ScalarV scVehicleHeading(fwAngle::GetATanOfXY( vVehicleMoveDirXY.GetYf(), -vVehicleMoveDirXY.GetXf() ));
	const Vec2V vEntityLocalVelXY = Rotate(vEntityVelocityXY - vVehicleVelocityXY, -scVehicleHeading);
	const Vec2V vEntityLocalPosXY = Rotate(vEntityPos.GetXY() - vVehiclePos.GetXY(), -scVehicleHeading);

	if (IsGreaterThanAll(MagSquared(vEntityLocalVelXY), ScalarV(V_ZERO)))
	{
		scInterceptionT = Clamp(geomTValues::FindTValueOpenSegToOriginV(Vec3V(vEntityLocalPosXY, ScalarV(V_ZERO)), Vec3V(vEntityLocalVelXY, ScalarV(V_ZERO))), ScalarV(V_ZERO), scMaxTime);
	}

	const Vec2V vInterceptionEntityLocalPosXY = vEntityLocalPosXY + vEntityLocalVelXY * scInterceptionT;
	const ScalarV scInterceptionEntitLocalyDistanceX = vInterceptionEntityLocalPosXY.GetX();
	const ScalarV scInterceptionEntityLocalDistanceY = vInterceptionEntityLocalPosXY.GetY();
	bool bInterception = IsLessThanAll(Abs(scInterceptionEntitLocalyDistanceX), scMaxSideDistance) && IsLessThanAll(Abs(scInterceptionEntityLocalDistanceY), scMaxForwardDistance);

	if (bInterception)
	{
		const Vec3V   vInterceptionVehPos			  = vVehiclePos + scInterceptionT * Vec3V(vVehicleVelocityXY, ScalarV(V_ZERO));
		const Vec3V   vInterceptionEntityPos		  = vEntityPos  + scInterceptionT * Vec3V(vEntityVelocityXY, ScalarV(V_ZERO));

		// Copy to output params
		*pscInterceptionT = scInterceptionT;
		*pvInterceptionVehPos = vInterceptionVehPos;
		*pvInterceptionEntityPos = vInterceptionEntityPos;
		*pscInterceptionDistanceLocalX = scInterceptionEntitLocalyDistanceX;
		*pscInterceptionDistanceLocalY = scInterceptionEntityLocalDistanceY;
	}

	return bInterception;
}

bool CVehicleIntelligence::DoesRoutePassByPosition(Vec3V_ConstRef vPos, const float fThreshold, const float fCareThreshold) const
{
	//Grab the vehicle position.
	Vec3V vVehiclePosition = m_pVehicle->GetTransform().GetPosition();
	
	//Generate the squared threshold.
	float fThresholdSq = square(fThreshold);
	ScalarV scThresholdSq = ScalarVFromF32(fThresholdSq);
	
	//Check if the vehicle's immediate position is within the threshold.
	ScalarV scDistSq = DistSquared(vPos, vVehiclePosition);
	if(IsLessThanAll(scDistSq, scThresholdSq))
	{
		return true;
	}
	
	//Generate the squared care threshold.
	ScalarV scCareThresholdSq = ScalarVFromF32(square(fCareThreshold));
	
	//Ensure the vehicle is within the care threshold.
	if(IsGreaterThanAll(scDistSq, scCareThresholdSq))
	{
		return false;
	}
	
	//The rest of algorithm works as follows:
	//	1) Start with the vehicle's current road node.
	//	2) Create a line segment out of the current and next road nodes.
	//	3) Test the closest distance from the position to the line segment.
	//	4) If the distance is less than the threshold, the route is considered to pass by the position.
	//	5) Check if the distance from the position to the next road node lies within a "care" threshold.
	//	6) If the distance is outside the "care" threshold, the rest of the route is considered "don't care" and the route is NOT considered to pass by the position (yet).
	//	7) If there are no road nodes left, the route is NOT considered to pass by the position.
	//	8) Assign the current road node to the next road node.
	//	9) Repeat from Step 2.
	
	//Ensure the node list is valid.
	CVehicleNodeList* pNodeList = GetNodeList();
	if(!pNodeList)
	{
		return false;
	}
	
	//Grab the current node.
	s32 iCurNode = pNodeList->GetTargetNodeIndex();
	Assertf(iCurNode >= 0 && iCurNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED, "Current road node is invalid.");
	
	//Iterate over the road nodes.
	for(s32 iNode = iCurNode; iNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED - 1; ++iNode)
	{
		//Ensure the road node is not empty.
		const CNodeAddress& rAddress = pNodeList->GetPathNodeAddr(iNode);
		if(rAddress.IsEmpty())
		{
			continue;
		}
		
		//Ensure the road node is loaded.
		if(!ThePaths.IsRegionLoaded(rAddress))
		{
			continue;
		}
		
		//Ensure the next road node is not empty.
		const CNodeAddress& rNextAddress = pNodeList->GetPathNodeAddr(iNode + 1);
		if(rNextAddress.IsEmpty())
		{
			continue;
		}
		
		//Ensure the next road node is loaded.
		if(!ThePaths.IsRegionLoaded(rNextAddress))
		{
			continue;
		}
		
		//Grab the road nodes.
		const CPathNode* pNode = ThePaths.FindNodePointer(rAddress);
		const CPathNode* pNextNode = ThePaths.FindNodePointer(rNextAddress);
		
		//Grab the coordinates of the road nodes.
		Vector3 vNodePos;
		pNode->GetCoors(vNodePos);
		Vector3 vNextNodePos;
		pNextNode->GetCoors(vNextNodePos);
		
		//Check if the line segment intersects the sphere generated by the position/threshold combination.
		if( geomSpheres::TestSphereToSeg(vPos, ScalarVFromF32(fThreshold), VECTOR3_TO_VEC3V(vNodePos), VECTOR3_TO_VEC3V(vNextNodePos)) )
		{
			return true;
		}
		
		//Check if the care threshold is valid.
		if(!IsZeroAll(scCareThresholdSq))
		{
			//Check if the distance from the next node segment to the position exceeds the care threshold.
			ScalarV scDistSq = DistSquared(VECTOR3_TO_VEC3V(vNextNodePos), vPos);
			if(IsGreaterThanAll(scDistSq, scCareThresholdSq))
			{
				return false;
			}
		}
	}
	
	return false;
}

bool CVehicleIntelligence::AreFutureRoutePointsCloseToPoint(Vec3V_In vPosition, ScalarV_In scRadius) const
{
	//Square the radius.
	ScalarV scRadiusSq = Scale(scRadius, scRadius);
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = m_pVehicle->GetTransform().GetPosition();
	
	//Calculate the distance squared from the vehicle to the position.
	ScalarV scDistSq = DistSquared(vVehiclePosition, vPosition);
	if(IsLessThanAll(scDistSq, scRadiusSq))
	{
		return true;
	}
	
	//Ensure the node list is valid.
	CVehicleNodeList* pNodeList = GetNodeList();
	if(!pNodeList)
	{
		return false;
	}
	
	//Grab the current node.
	s32 iCurNode = pNodeList->GetTargetNodeIndex();
	Assertf(iCurNode >= 0 && iCurNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED, "Current road node is invalid.");

	//Iterate over the road nodes.
	for(s32 iNode = iCurNode; iNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++iNode)
	{
		//Ensure the road node is not empty.
		const CNodeAddress& rAddress = pNodeList->GetPathNodeAddr(iNode);
		if(rAddress.IsEmpty())
		{
			continue;
		}

		//Ensure the road node is loaded.
		if(!ThePaths.IsRegionLoaded(rAddress))
		{
			continue;
		}
		
		//Grab the road node.
		const CPathNode* pNode = ThePaths.FindNodePointer(rAddress);

		//Grab the coordinates of the road node.
		Vec3V vNodePosition;
		pNode->GetCoors(RC_VECTOR3(vNodePosition));
		
		//Calculate the distance squared from the node to the position.
		scDistSq = DistSquared(vNodePosition, vPosition);
		if(IsLessThanAll(scDistSq, scRadiusSq))
		{
			return true;
		}
	}
	
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindMaxSteerAngle
// PURPOSE :  Finds the maximum angle the steering wheel will go. Bigger vehicles
//			  might need bigger values to reduce turning radius.
/////////////////////////////////////////////////////////////////////////////////
float CVehicleIntelligence::FindMaxSteerAngle() const
{
	const float	fSpeed = m_pVehicle->GetAiXYSpeed();
	const float fBaseVal = 0.7f;
	const float	fMaxForSpeed = fSpeed > 42.0f 
		? 0.2f 
		: 0.9f - (fSpeed / 60.0f);

	//	if(pVeh->GetModelIndex() == MODELID_CAR_ENFORCER || pVeh->GetModelIndex() == MODELID_CAR_PACKER)
	//	{
	//		return 0.7f;
	//	}

	return rage::Min(fBaseVal, fMaxForSpeed);
}

float CVehicleIntelligence::GetTurnRadiusAtCurrentSpeed() const
{
	const float fMaxSteeringAngle = FindMaxSteerAngle();
	CWheel* pFrontWheel = m_pVehicle->GetWheelFromId(VEH_WHEEL_LF);
	CWheel* pRearWheel = m_pVehicle->GetWheelFromId(VEH_WHEEL_LR);
	//Assertf(pFrontWheel && pRearWheel, "CVehicleIntelligence::GetTurnRadiusAtCurrentSpeed expected VEH_WHEEL_LF and VEH_WHEEL_LR to exist!  Returning 0.0");
	if (!pFrontWheel || !pRearWheel)
	{
		return 0.0f;
	}

	Vector3 vFrontWheelPos, vRearWheelPos;
	pFrontWheel->GetWheelPosAndRadius(vFrontWheelPos);
	pRearWheel->GetWheelPosAndRadius(vRearWheelPos);
	const float fWheelBase = vFrontWheelPos.Dist(vRearWheelPos);
	//const float fWheelBase = m_pVehicle->GetVehicleModelInfo()->GetFragType()->GetPhysics(0)->GetArchetype()->GetBound()->GetBoundingBoxMax().GetYf() 
	//	- m_pVehicle->GetVehicleModelInfo()->GetFragType()->GetPhysics(0)->GetArchetype()->GetBound()->GetBoundingBoxMin().GetYf();
	const float fSinSteeringAngle = Sinf(fMaxSteeringAngle);

	return fMaxSteeringAngle > 0.0f ? fWheelBase / fSinSteeringAngle : 0.0f;
}
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateJustBeenGivenNewCommand
// PURPOSE :  Called by other code when this vehicle is given a new command.
//			  This should make sure the car starts with a clean sheet and doesn't start
//			  by reversing or a 3-point turn.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
void CVehicleIntelligence::UpdateJustBeenGivenNewCommand()
{
	LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();
	MillisecondsNotMoving = 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetActiveTask
// PURPOSE :  Gets the current active vehicle task from the taskmanager
// RETURNS : current active taskmission
/////////////////////////////////////////////////////////////////////////////////
CTaskVehicleMissionBase	*CVehicleIntelligence::GetActiveTask() const
{
	aiTask *pTask = m_TaskManager.GetActiveTask(VEHICLE_TASK_TREE_PRIMARY);

	if(pTask)
	{
#if __ASSERT
		//Assertf(), "pTask was valid but not a CTaskVehicleMissionBase in CVehicleIntelligence::GetActiveTask().");
		const CTaskVehicleMissionBase* pTaskMissionBaseDebug = dynamic_cast<CTaskVehicleMissionBase*>(pTask);
		Assertf(pTaskMissionBaseDebug, "pTask was valid but not a CTaskVehicleMissionBase in CVehicleIntelligence::GetActiveTask().");
		Assertf(pTaskMissionBaseDebug, "Trying to get task name: %s", pTask->GetTaskName());
#endif //__ASSERT

		return static_cast<CTaskVehicleMissionBase*>(pTask);
	}
	else
	{
		return NULL;
	}
}

CTaskVehicleMissionBase	*CVehicleIntelligence::GetActiveTaskSimplest() const
{
	CTask * pTask = GetActiveTask();
	while(pTask && pTask->GetSubTask())
	{
		pTask = pTask->GetSubTask();
	}

	if(pTask)
	{
		Assertf((dynamic_cast<CTaskVehicleMissionBase*>(pTask)), "pTask was null in CVehicleIntelligence::GetActiveTaskSimplest().");
		return static_cast<CTaskVehicleMissionBase*>(pTask);
	}
	else
	{
		return NULL;
	}
}

void CVehicleIntelligence::GetActiveTaskInfo(s32& taskType, sVehicleMissionParams& params) const
{
	const CTaskVehicleMissionBase	*pActiveTask = NULL;

	if (m_pVehicle->IsNetworkClone())
	{
		pActiveTask = static_cast<CNetObjVehicle*>(m_pVehicle->GetNetworkObject())->GetCloneAITask();
	}
	else
	{
		pActiveTask = CVehicleIntelligence::GetActiveTask();
	}

	if (pActiveTask)
	{
		taskType = pActiveTask->GetTaskType();
		params = pActiveTask->GetParams();
	}
	else
	{
		taskType = CTaskTypes::TASK_INVALID_ID;
	}
}

void CVehicleIntelligence::SetRecordingNumber(s8 recordingNumber)
{
	Assert(m_pVehicle);
	m_pVehicle->m_nVehicleFlags.bIsInRecording = (recordingNumber != -1);
	m_RecordingNumber = recordingNumber;
}

#if __BANK

void CVehicleIntelligence::PrintTasks()
{	
	// Print the task hierarchy for this vehicle
	Printf("Task hierarchy\n");
	CTask* pActiveTask = GetActiveTask();
	if(pActiveTask)
	{
		CTask* pTaskToPrint = pActiveTask;
		while(pTaskToPrint)
		{
			Printf("name: %s\n", (const char*) pTaskToPrint->GetName());
			pTaskToPrint=pTaskToPrint->GetSubTask();
		}
	}
}

#endif
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateCarHasReasonToBeStopped
// PURPOSE :  Should be called if this car is stopping but it has a reason for it.
//			  Calling this will make sure the driver doesn't get impatient.
/////////////////////////////////////////////////////////////////////////////////
void CVehicleIntelligence::UpdateCarHasReasonToBeStopped()
{
	MillisecondsNotMoving = 0;
}


/////////////////////////////////////////////////////////////////////////////////

aiTask	*CVehicleIntelligence::GetGotoTaskForVehicle(CVehicle *pVehicle, CPhysical* pTargetEntity, Vector3 *vTarget, s32 iDrivingFlags, float fTargetReached, float fStraightLineDistance, float fCruiseSpeed)
{	
	CTaskVehicleMissionBase* pCarTask = NULL;

	sVehicleMissionParams params;
	params.SetTargetEntity(pTargetEntity);
	params.m_iDrivingFlags = iDrivingFlags;
	params.m_fTargetArriveDist = fTargetReached;
	params.m_fCruiseSpeed = fCruiseSpeed;
	params.SetTargetPosition(vTarget);

	VehicleType vehicleType = pVehicle->GetVehicleType();
	if(vehicleType == VEHICLE_TYPE_SUBMARINECAR)
	{
		vehicleType = pVehicle->IsInSubmarineMode() ? VEHICLE_TYPE_SUBMARINE : VEHICLE_TYPE_CAR;
	}
	if( vehicleType == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
		vehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
	{
		vehicleType = VEHICLE_TYPE_CAR;
	}

	switch(vehicleType)
	{
	case VEHICLE_TYPE_CAR:
	case VEHICLE_TYPE_BIKE:
	case VEHICLE_TYPE_BICYCLE:
    case VEHICLE_TYPE_QUADBIKE:
		pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, fStraightLineDistance);
		break;
    case VEHICLE_TYPE_BOAT:
		pCarTask = rage_new CTaskVehicleGoToBoat(params);
		break;
	case VEHICLE_TYPE_SUBMARINE:
		pCarTask = rage_new CTaskVehicleGoToSubmarine(params);
		break;
	case VEHICLE_TYPE_PLANE:
		if(iDrivingFlags & DF_PlaneTaxiMode)
		{
			pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, fStraightLineDistance);
		}
		else
		{
			pCarTask = rage_new CTaskVehicleGoToPlane(params);
		}
		break;
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		pCarTask = rage_new CTaskVehicleGoToHelicopter(params);
		break;
	default:
		BANK_ONLY(AI_LOG_WITH_ARGS("Vehicle type %s is not supported for this task type.", pVehicle->GetVehicleTypeString(vehicleType)));
		break;
	}

	return pCarTask;
}

/////////////////////////////////////////////////////////////////////////////////

bool	CVehicleIntelligence::GetSlowingDownForPed()
{
	aiTask *pTask = m_TaskManager.FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE);
	if(pTask)
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile *pAvoidanceTask = 	static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(pTask);
		return pAvoidanceTask->GetSlowingDownForPed();
	}

	return false;//just return false if no valid task found
}

/////////////////////////////////////////////////////////////////////////////////

bool	CVehicleIntelligence::GetSlowingDownForCar()
{
	aiTask *pTask = m_TaskManager.FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE);
	if(pTask)
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile *pAvoidanceTask = 	static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(pTask);
		return pAvoidanceTask->GetSlowingDownForCar();
	}

	return false;//just return false if no valid task found
}


bool CVehicleIntelligence::GetShouldObeyTrafficLights()
{
	CTaskVehicleMissionBase * pTask = GetActiveTask();
	if(pTask)
	{
		return pTask->GetShouldObeyTrafficLights();
	}
	return false;
}

void CVehicleIntelligence::ClearScanners()
{
	m_vehicleScanner.Clear(); 
	m_pedScanner.Clear(); 
	m_objectScanner.Clear();
}

aiTask *CVehicleIntelligence::GetSubTaskFromMissionIdentifier( CVehicle *UNUSED_PARAM(pVehicle),
															   int iMission, 
															   CPhysical* pTargetEntity, 
															   Vector3 *vTarget, 
															   float fTargetReached, 
															   float fCruiseSpeed, 
															   float fSubOrientation,
															   s32 iMinHeightAboveTerrain,
															   float fSlowDownDistance,
															   int iSubFlags)
{
	s32 flags = iSubFlags;
	if( fSubOrientation >= 0.0f )
	{
		flags |= CTaskVehicleGoToSubmarine::SF_AttainRequestedOrientation;
	}

	//Assertf(!(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR && !pVehicle->IsInSubmarineMode()), "TASK_SUB_MISSION called when not in sub mode");

	sVehicleMissionParams params;
	params.SetTargetEntity(pTargetEntity);
	params.SetTargetPosition(vTarget);
	params.m_fTargetArriveDist = fTargetReached;
	params.m_fCruiseSpeed = fCruiseSpeed;

	ASSERT_ONLY(params.IsTargetValid());

	CTaskVehicleMissionBase* pSubTask = NULL;
	switch(iMission)
	{
	case MISSION_RAM:
		pSubTask = rage_new CTaskVehicleRam(params);
		break;
	case MISSION_BLOCK:
		pSubTask = rage_new CTaskVehicleBlock(params);
		break;
	case MISSION_GOTO:
		pSubTask = rage_new CTaskVehicleGoToSubmarine(params,	flags, iMinHeightAboveTerrain, fSubOrientation);
		if( fSlowDownDistance > 0.0f )
		{
			static_cast<CTaskVehicleGoToSubmarine*>(pSubTask)->SetSlowDownDistance(fSlowDownDistance);
		}
		if (iMission == MISSION_GOTO && CTaskSequences::ms_iActiveSequence < 0)		// In sequences we do finish the goto task. Otherwise we hover.
		{
			pSubTask->SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, true);
		}
		break;
	case MISSION_STOP:
		pSubTask = rage_new CTaskVehicleStop();
		break;
	case MISSION_ATTACK:
		pSubTask = rage_new CTaskVehicleAttack(params);
		static_cast<CTaskVehicleAttack*>(pSubTask)->SetSubmarineSpecificParams(iMinHeightAboveTerrain, flags);
		break;
	case MISSION_FOLLOW:
		pSubTask = rage_new CTaskVehicleFollow(params);
		break;
	case MISSION_FLEE:
		params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);
		pSubTask = rage_new CTaskVehicleFlee(params);
		break;
	case MISSION_CIRCLE:
		pSubTask = rage_new CTaskVehicleCircle(params);
		break;
	case MISSION_ESCORT_LEFT:
		pSubTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_LEFT);
		break;
	case MISSION_ESCORT_RIGHT:
		pSubTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_RIGHT);
		break;
	case MISSION_ESCORT_REAR:
		pSubTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_REAR);
		break;
	case MISSION_ESCORT_FRONT:
		pSubTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_FRONT);
		break;
	case MISSION_FOLLOW_RECORDING:
		pSubTask = rage_new CTaskVehicleFollowRecording(params);
		break;
	case MISSION_CRASH:
		pSubTask = rage_new CTaskVehicleCrash(); //I suppose this is valid - will probably want to subclass it
		break;
	case MISSION_CRUISE:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't cruise");
		break;
	case MISSION_POLICE_BEHAVIOUR:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't be police!");
		break;
	case MISSION_GOTO_RACING:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't use goto racing.");
		break;
	case MISSION_PARK_PERPENDICULAR:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't park perpendicular.");
		break;
	case MISSION_PARK_PARALLEL:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't park parallel.");
		break;
	case MISSION_LAND:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't land.");
		break;
	case MISSION_LAND_AND_WAIT:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't land.");
		break;
	case MISSION_PULL_OVER:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't pull over.");
		break;
	case MISSION_PROTECT:
		Assertf(0, "TASK_SUB_MISSION incorrect mission type, Sub's can't protect.");
		break;
	default:
		Assertf(0,"TASK_SUB_MISSION Invalid mission type %d", iMission);
	}
	return pSubTask;
}

aiTask *CVehicleIntelligence::GetHeliTaskFromMissionIdentifier(CVehicle *UNUSED_PARAM(pVehicle),
															int iMission, 
															CPhysical* pTargetEntity, 
															Vector3 *vTarget, 
															float fTargetReached, 
															float fCruiseSpeed, 
															float fHeliOrientation,
															s32 iFlightHeight,
															s32 iMinHeightAboveTerrain,
															float fSlowDownDistance,
															int iHeliFlags)
{
	s32 flags = iHeliFlags;
	if( fHeliOrientation >= 0.0f )
	{
		flags |= CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation;
	}

	sVehicleMissionParams params;
	params.SetTargetEntity(pTargetEntity);
	params.SetTargetPosition(vTarget);
	params.m_fTargetArriveDist = fTargetReached;
	params.m_fCruiseSpeed = fCruiseSpeed;

	ASSERT_ONLY(params.IsTargetValid());

	CTaskVehicleMissionBase* pHeliTask = NULL;
	switch(iMission)
	{
	case MISSION_CRUISE:
		Assertf(0, "TASK_HELI_MISSION incorrect mission type, Heli's can't cruise");
		break;
	case MISSION_RAM:
		pHeliTask = rage_new CTaskVehicleRam(params);
		break;
	case MISSION_BLOCK:
		pHeliTask = rage_new CTaskVehicleBlock(params);
		break;
	case MISSION_GOTO:
		pHeliTask = rage_new CTaskVehicleGoToHelicopter(params, 
			flags, 
			fHeliOrientation, 
			iMinHeightAboveTerrain);
		if( fSlowDownDistance > 0.0f )
		{
			static_cast<CTaskVehicleGoToHelicopter*>(pHeliTask)->SetSlowDownDistance(fSlowDownDistance);
		}

		if (iMission == MISSION_GOTO && CTaskSequences::ms_iActiveSequence < 0)		// In sequences we do finish the goto task. Otherwise we hover.
		{
			pHeliTask->SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, true);
		}
		break;
	case MISSION_STOP:
		pHeliTask = rage_new CTaskVehicleStop();
		break;
	case MISSION_ATTACK:
		pHeliTask = rage_new CTaskVehicleAttack(params);
		static_cast<CTaskVehicleAttack*>(pHeliTask)->SetHelicopterSpecificParams(fHeliOrientation, iFlightHeight, iMinHeightAboveTerrain, flags);
		break;
	case MISSION_FOLLOW:
		pHeliTask = rage_new CTaskVehicleFollow(params);
		static_cast<CTaskVehicleFollow*>(pHeliTask)->SetHelicopterSpecificParams(fHeliOrientation, iFlightHeight, iMinHeightAboveTerrain, flags);
		break;
	case MISSION_FLEE:
		params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);
		//pHeliTask = rage_new CTaskVehicleFlee(params);
		pHeliTask = rage_new CTaskVehicleFleeAirborne(params, iFlightHeight, iMinHeightAboveTerrain, (flags & CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation) != 0, fHeliOrientation);
		break;
	case MISSION_CIRCLE:
		pHeliTask = rage_new CTaskVehicleCircle(params);
		static_cast<CTaskVehicleCircle*>(pHeliTask)->SetHelicopterSpecificParams(fHeliOrientation, iFlightHeight, iMinHeightAboveTerrain, flags);
		break;
	case MISSION_ESCORT_LEFT:
		pHeliTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_LEFT, -1.0f, iMinHeightAboveTerrain);
		break;
	case MISSION_ESCORT_RIGHT:
		pHeliTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_RIGHT, -1.0f, iMinHeightAboveTerrain);
		break;
	case MISSION_ESCORT_REAR:
		pHeliTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_REAR, -1.0f, iMinHeightAboveTerrain);
		break;
	case MISSION_ESCORT_FRONT:
		pHeliTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_FRONT, -1.0f, iMinHeightAboveTerrain);
		break;
	case MISSION_GOTO_RACING:
		Assertf(0, "TASK_HELI_MISSION incorrect mission type, Heli's can't use goto racing.");
		break;
	case MISSION_FOLLOW_RECORDING:
		pHeliTask = rage_new CTaskVehicleFollowRecording(params);
		break;
	case MISSION_POLICE_BEHAVIOUR:
		pHeliTask = rage_new CTaskVehiclePoliceBehaviour(params);
		break;
	case MISSION_PARK_PERPENDICULAR:
		Assertf(0, "TASK_HELI_MISSION incorrect mission type, Heli's can't park perpendicular.");
		break;
	case MISSION_PARK_PARALLEL:
		Assertf(0, "TASK_HELI_MISSION incorrect mission type, Heli's can't park parallel.");
		break;
	case MISSION_LAND:
		pHeliTask = rage_new CTaskVehicleLand(params, flags, fHeliOrientation);
		break;
	case MISSION_LAND_AND_WAIT:
		params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);
		pHeliTask = rage_new CTaskVehicleLand(params, flags, fHeliOrientation);
		break;
	case MISSION_CRASH:
		pHeliTask = rage_new CTaskVehicleCrash();
		break;
	case MISSION_PULL_OVER:
		Assertf(0, "TASK_HELI_MISSION incorrect mission type, Heli's can't pull over.");
		break;
	case MISSION_PROTECT:
		pHeliTask = rage_new CTaskVehicleHeliProtect(params, fTargetReached, iMinHeightAboveTerrain, flags);
		break;
	default:
		Assertf(0,"TASK_HELI_MISSION Invalid mission type %d", iMission);
	}
	return pHeliTask;
}

/////////////////////////////////////////////////////////////////////////////////
aiTask* CVehicleIntelligence::GetPlaneTaskFromMissionIdentifier(CVehicle* UNUSED_PARAM( pVehicle ), int iMission, CPhysical* pTargetEntity, 
																Vector3 *vTarget, float fTargetReached, float fCruiseSpeed, 
																float fOrientation, s32 iFlightHeight, s32 iMinHeightAboveTerrain, 
																bool bPrecise)
{
	bool bUseOrientation = false;
	if( fOrientation >= 0.0f )
	{
		bUseOrientation = true;
	}

	sVehicleMissionParams params;
	params.SetTargetPosition(vTarget);
	params.SetTargetEntity(pTargetEntity);
	params.m_fTargetArriveDist = fTargetReached;
	params.m_fCruiseSpeed = fCruiseSpeed;

	ASSERT_ONLY(params.IsTargetValid());

	CTaskVehicleMissionBase* pPlaneTask = NULL;
	switch(iMission)
	{
	case MISSION_CRUISE:
		Assertf(0, "TASK_PLANE_MISSION incorrect mission type, Plane's can't cruise");
		break;
	case MISSION_RAM:
		pPlaneTask = rage_new CTaskVehicleRam(params);
		break;
	case MISSION_BLOCK:
		pPlaneTask = rage_new CTaskVehicleBlock(params);
		break;
	case MISSION_GOTO:
		pPlaneTask = rage_new CTaskVehicleGoToPlane( params,
													 iFlightHeight, 
													 iMinHeightAboveTerrain,
													 bPrecise,
													 bUseOrientation,
													 fOrientation);

		if( pPlaneTask && iMission == MISSION_GOTO && CTaskSequences::ms_iActiveSequence < 0)		// In sequences we do finish the goto task. Otherwise we hover.
		{
			pPlaneTask->SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, true);
		}

		break;
	case MISSION_STOP:
		pPlaneTask = rage_new CTaskVehicleStop();
		break;
	case MISSION_ATTACK:
		pPlaneTask = rage_new CTaskVehicleAttack(params);
		static_cast<CTaskVehicleAttack*>(pPlaneTask)->SetHelicopterSpecificParams(fOrientation, iFlightHeight, iMinHeightAboveTerrain, 0);
		break;
	case MISSION_FOLLOW:
		pPlaneTask = rage_new CTaskVehicleFollow(params);
		break;
	case MISSION_FLEE:
		params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);
		//pPlaneTask = rage_new CTaskVehicleFlee(params);
		pPlaneTask = rage_new CTaskVehicleFleeAirborne(params, iFlightHeight, iMinHeightAboveTerrain, bUseOrientation, fOrientation);
		break;
	case MISSION_CIRCLE:
		pPlaneTask = rage_new CTaskVehicleCircle(params);
		static_cast<CTaskVehicleCircle*>(pPlaneTask)->SetHelicopterSpecificParams(fOrientation, iFlightHeight, iMinHeightAboveTerrain, 0);
		break;
	case MISSION_ESCORT_LEFT:
		pPlaneTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_LEFT);
		break;
	case MISSION_ESCORT_RIGHT:
		pPlaneTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_RIGHT);
		break;
	case MISSION_ESCORT_REAR:
		pPlaneTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_REAR);
		break;
	case MISSION_ESCORT_FRONT:
		pPlaneTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_FRONT);
		break;
	case MISSION_GOTO_RACING:
		Assertf(0, "TASK_PLANE_MISSION incorrect mission type, Plane's can't use goto racing.");
		break;
	case MISSION_FOLLOW_RECORDING:
		pPlaneTask = rage_new CTaskVehicleFollowRecording(params);
		break;
	case MISSION_POLICE_BEHAVIOUR:
		pPlaneTask = rage_new CTaskVehiclePoliceBehaviour(params);
		break;
	case MISSION_PARK_PERPENDICULAR:
		Assertf(0, "TASK_PLANE_MISSION incorrect mission type, Plane's can't park perpendicular.");
		break;
	case MISSION_PARK_PARALLEL:
		Assertf(0, "TASK_PLANE_MISSION incorrect mission type, Plane's can't park parallel.");
		break;
	case MISSION_LAND:
		Assertf(0, "TASK_PLANE_LAND NOT SUPPORTTED ANYMORE.");
		//pPlaneTask = rage_new CTaskVehicleLandPlane(params);
		break;
	case MISSION_LAND_AND_WAIT:
		Assertf(0, "TASK_PLANE_LAND NOT SUPPORTTED ANYMORE.");
		//params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);
		//pPlaneTask = rage_new CTaskVehicleLandPlane(params);
		break;
	case MISSION_CRASH:
		pPlaneTask = rage_new CTaskVehicleCrash();
		break;
	case MISSION_PULL_OVER:
		Assertf(0, "TASK_PLANE_MISSION incorrect mission type, Plane's can't pull over.");
		break;
	case MISSION_PROTECT:
		Assertf(0, "TASK_PLANE_MISSION incorrect mission type, Plane's can't MISSION_PROTECT.");
		break;
	default:
		Assertf(0,"TASK_PLANE_MISSION Invalid mission type %d", iMission);
	}
	return pPlaneTask;
}

aiTask*	CVehicleIntelligence::GetBoatTaskFromMissionIdentifier(CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, s32 iDrivingFlags, float fTargetReached, float fCruiseSpeed, int iBoatFlags )
{
	float fStraightLineDistance = 10.0f;

	switch(iMission)
	{
	case MISSION_GOTO:
	case MISSION_GOTO_RACING:
		{
			sVehicleMissionParams params;
			params.SetTargetEntity(pTargetEntity);
			params.m_iDrivingFlags = iDrivingFlags;
			params.m_fTargetArriveDist = fTargetReached;
			params.m_fCruiseSpeed = fCruiseSpeed;
			params.SetTargetPosition(vTarget);

			return rage_new CTaskVehicleGoToBoat(params, static_cast<rage::u16>(iBoatFlags));
		}
		
	default:
		return GetTaskFromMissionIdentifier(pVehicle, iMission, pTargetEntity, vTarget, iDrivingFlags, fTargetReached, fStraightLineDistance, fCruiseSpeed, (iDrivingFlags & DF_DriveIntoOncomingTraffic)>0, (iDrivingFlags & DF_DriveInReverse)>0);
	}
}


/////////////////////////////////////////////////////////////////////////////////

aiTask *CVehicleIntelligence::GetTaskFromMissionIdentifier(CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, s32 iDrivingFlags, float fTargetReached, float fStraightLineDistance, float fCruiseSpeed, bool bAllowedToGoAgainstTraffic, bool bDoEverythingInReverse)
{
	if( bDoEverythingInReverse )
 	{
 		iDrivingFlags |= DF_DriveInReverse;
 	}
	if( bAllowedToGoAgainstTraffic )
	{
		iDrivingFlags |= DF_DriveIntoOncomingTraffic;
	}

	sVehicleMissionParams params;
	// Set target entity prior to setting target position to avoid false positive in SetTargetPosition(),
	// the target position may be used as an offset from the target entity, and otherwise invalid target positions (zero) become valid
	params.SetTargetEntity(pTargetEntity);
	params.SetTargetPosition(vTarget);
	params.m_iDrivingFlags = iDrivingFlags;
	params.m_fTargetArriveDist = fTargetReached;
	params.m_fCruiseSpeed = fCruiseSpeed;

	//for pull-over
	Vector3 dummyDir(VEC3_ZERO);

	aiTask* pCarTask = NULL;
	switch(iMission)
	{
	case MISSION_CRUISE:
		pCarTask = CreateCruiseTask(*pVehicle, params);
		break;
	case MISSION_RAM:
		pCarTask = rage_new CTaskVehicleRam(params);
		break;
	case MISSION_BLOCK:
		pCarTask = rage_new CTaskVehicleBlock(params);
		break;
	case MISSION_GOTO:
	case MISSION_GOTO_RACING:
		pCarTask = GetGotoTaskForVehicle(pVehicle, pTargetEntity, vTarget, iDrivingFlags, fTargetReached, fStraightLineDistance, fCruiseSpeed);
		break;
	case MISSION_STOP:
		pCarTask = rage_new CTaskVehicleStop(0);
		break;
	case MISSION_ATTACK:
		pCarTask = rage_new CTaskVehicleAttack(params);
		break;
	case MISSION_FOLLOW:
		pCarTask = rage_new CTaskVehicleFollow(params);
		break;
	case MISSION_FLEE:
		{
			params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);
			if ( pVehicle->InheritsFromBoat() )
			{
				pCarTask = rage_new CTaskVehicleFleeBoat(params);
			}
			else
			{

				pCarTask = rage_new CTaskVehicleFlee(params);
			}
		}

		break;
	case MISSION_CIRCLE:
		pCarTask = rage_new CTaskVehicleCircle(params);
		break;
	case MISSION_ESCORT_LEFT:
		pCarTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_LEFT);
		break;
	case MISSION_ESCORT_RIGHT:
		pCarTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_RIGHT);
		break;
	case MISSION_ESCORT_REAR:
		pCarTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_REAR);
		break;
	case MISSION_ESCORT_FRONT:
		pCarTask = rage_new CTaskVehicleEscort(params, CTaskVehicleEscort::VEHICLE_ESCORT_FRONT);
		break;
	case MISSION_FOLLOW_RECORDING:
		pCarTask = rage_new CTaskVehicleFollowRecording(params);
		break;
	case MISSION_POLICE_BEHAVIOUR:
		pCarTask = rage_new CTaskVehiclePoliceBehaviour(params);
		break;
	case MISSION_PARK_PERPENDICULAR:
		pCarTask = rage_new CTaskVehicleParkNew(params, VEC3_ZERO, CTaskVehicleParkNew::Park_Perpendicular_NoseIn, 30.0f * DtoR);
		break;
	case MISSION_PARK_PARALLEL:
		pCarTask = rage_new CTaskVehicleParkNew(params, VEC3_ZERO, CTaskVehicleParkNew::Park_Parallel, 30.0f * DtoR);
		break;
	case MISSION_LAND:
		pCarTask = rage_new CTaskVehicleLand(params);
		break;
	case MISSION_CRASH:
		pCarTask = rage_new CTaskVehicleCrash();
		break;
	case MISSION_PULL_OVER:
		//pCarTask = rage_new CTaskVehiclePullOver();
		pCarTask = rage_new CTaskVehicleParkNew(params, dummyDir, CTaskVehicleParkNew::Park_PullOver, 0.175f);
		break;
	case MISSION_TURN_CLOCKWISE_GOING_BACKWARD:
	case MISSION_TURN_CLOCKWISE_GOING_FORWARD:
	case MISSION_TURN_COUNTERCLOCKWISE_GOING_BACKWARD:
	case MISSION_TURN_COUNTERCLOCKWISE_GOING_FORWARD:
		pCarTask = rage_new CTaskVehicleThreePointTurn(params, false);
		break;
	default:
		Assertf(0,"TASK_VEHICLE_MISSION Invalid mission type %d", iMission);
	}

	return pCarTask;
}

VehMissionType	CVehicleIntelligence::GetMissionIdentifierFromTaskType(int iTaskType, sVehicleMissionParams& params)
{
	switch (iTaskType)
	{
	case CTaskTypes::TASK_VEHICLE_HELI_PROTECT:
		return MISSION_PROTECT;
	case CTaskTypes::TASK_VEHICLE_CRUISE_NEW:
		return MISSION_CRUISE;
	case CTaskTypes::TASK_VEHICLE_RAM:
		return MISSION_RAM;
	case CTaskTypes::TASK_VEHICLE_BLOCK:
		return MISSION_BLOCK;
	case CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW:
	case CTaskTypes::TASK_VEHICLE_GOTO_PLANE:
	case CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER:
	case CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE:
	case CTaskTypes::TASK_VEHICLE_GOTO_BOAT:
	case CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE:
	case CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE:
		return MISSION_GOTO;
	case CTaskTypes::TASK_VEHICLE_STOP:
		return MISSION_STOP;
	case CTaskTypes::TASK_VEHICLE_ATTACK:
		return MISSION_ATTACK;
	case CTaskTypes::TASK_VEHICLE_FOLLOW:
		return MISSION_FOLLOW;
	case CTaskTypes::TASK_VEHICLE_FLEE:
		return MISSION_FLEE;
	case CTaskTypes::TASK_VEHICLE_CIRCLE:
		return MISSION_CIRCLE;
	case CTaskTypes::TASK_VEHICLE_ESCORT:
		aiWarningf("Trying to get a mission type from TASK_VEHICLE_ESCORT. Multiple escort types not supported, returning MISSION_ESCORT_REAR");
		return MISSION_ESCORT_REAR;
		break;
	//case TASK_TASKNAME:
	//	return MISSION_GOTO_RACING;
	case CTaskTypes::TASK_VEHICLE_FOLLOW_RECORDING:
		return MISSION_FOLLOW_RECORDING;
	case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR:
	case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_HELICOPTER:
	case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_BOAT:
		return MISSION_POLICE_BEHAVIOUR;
	case CTaskTypes::TASK_VEHICLE_LAND:
		if (params.m_iDrivingFlags & DF_DontTerminateTaskWhenAchieved)
		{
			return MISSION_LAND_AND_WAIT;
		}
		else
		{
			return MISSION_LAND;
		}
		break;
	case CTaskTypes::TASK_VEHICLE_CRASH:
		return MISSION_CRASH;
	case CTaskTypes::TASK_VEHICLE_PULL_OVER:
		return MISSION_PULL_OVER;
	default:
		return MISSION_NONE;
	}
}

aiTask* CVehicleIntelligence::GetTempActionTaskFromTempActionType(int iTempActionType, u32 nDuration, fwFlags8 uConfigFlags)
{
	CTaskVehicleMissionBase *pTempTask = NULL;

	//TODO: think of a better way to initialize the "swerve to road edge" stuff
	Vector2 vRoadEdgeNormal(0.0f, 0.0f);
	Vector2 vRoadEdgePos(0.0f, 0.0f);

	switch(iTempActionType)
	{
	case TEMPACT_WAIT:
		pTempTask = rage_new CTaskVehicleWait(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_REVERSE:
		pTempTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleReverse::Reverse_Opposite_Direction);
		break;
	case TEMPACT_HANDBRAKETURNLEFT:
		pTempTask = rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleHandBrake::HandBrake_Left);
		break;
	case TEMPACT_HANDBRAKETURNRIGHT:
		pTempTask = rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleHandBrake::HandBrake_Right);
		break;
	case TEMPACT_HANDBRAKESTRAIGHT:
		pTempTask = rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleHandBrake::HandBrake_Straight);
		break;
	case TEMPACT_TURNLEFT:
		pTempTask = rage_new CTaskVehicleTurn(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleTurn::Turn_Left);
		break;
	case TEMPACT_TURNRIGHT:
		pTempTask = rage_new CTaskVehicleTurn(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleTurn::Turn_Right);
		break;
	case TEMPACT_GOFORWARD:
		pTempTask = rage_new CTaskVehicleGoForward(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_SWERVELEFT:
		pTempTask = rage_new CTaskVehicleSwerve(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleSwerve::Swerve_Left, false, false, vRoadEdgeNormal, vRoadEdgePos);
		break;
	case TEMPACT_SWERVERIGHT:
		pTempTask = rage_new CTaskVehicleSwerve(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleSwerve::Swerve_Right, false, false, vRoadEdgeNormal, vRoadEdgePos);
		break;
	case TEMPACT_REVERSE_LEFT:
		pTempTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleReverse::Reverse_Left, uConfigFlags);
		break;
	case TEMPACT_REVERSE_RIGHT:
		pTempTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleReverse::Reverse_Right, uConfigFlags);
		break;
	case TEMPACT_PLANE_FLY_UP:
		pTempTask = rage_new CTaskVehicleFlyDirection(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleFlyDirection::Fly_Up);
		break;
	case TEMPACT_PLANE_FLY_STRAIGHT:
		pTempTask = rage_new CTaskVehicleFlyDirection(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleFlyDirection::Fly_Straight);
		break;
	case TEMPACT_PLANE_SHARP_LEFT:
		pTempTask = rage_new CTaskVehicleFlyDirection(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleFlyDirection::Fly_Left);
		break;
	case TEMPACT_PLANE_SHARP_RIGHT:
		pTempTask = rage_new CTaskVehicleFlyDirection(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleFlyDirection::Fly_Right);
		break;
	case TEMPACT_HEADON_COLLISION:
		pTempTask = rage_new CTaskVehicleHeadonCollision(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_SWERVELEFT_STOP:
		pTempTask = rage_new CTaskVehicleSwerve(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleSwerve::Swerve_Left, true, false, vRoadEdgeNormal, vRoadEdgePos);
		break;
	case TEMPACT_SWERVERIGHT_STOP:
		pTempTask = rage_new CTaskVehicleSwerve(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleSwerve::Swerve_Right, true, false, vRoadEdgeNormal, vRoadEdgePos);
		break;
	case TEMPACT_REVERSE_STRAIGHT:
		pTempTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleReverse::Reverse_Straight, uConfigFlags);
		break;
	case TEMPACT_BOOST_USE_STEERING_ANGLE:
		pTempTask = rage_new CTaskVehicleBoostUseSteeringAngle(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_BRAKE:
		pTempTask = rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_HANDBRAKETURNLEFT_INTELLIGENT:
		pTempTask = rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleHandBrake::HandBrake_Left_Intelligence);
		break;
	case TEMPACT_HANDBRAKETURNRIGHT_INTELLIGENT:
		pTempTask = rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleHandBrake::HandBrake_Right_Intelligence);
		break;
	case TEMPACT_HANDBRAKESTRAIGHT_INTELLIGENT:
		pTempTask = rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleHandBrake::HandBrake_Straight_Intelligence);
		break;
	case TEMPACT_REVERSE_STRAIGHT_HARD:
		pTempTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, CTaskVehicleReverse::Reverse_Straight_Hard, uConfigFlags);
		break;
	case TEMPACT_BURNOUT:
		pTempTask = rage_new CTaskVehicleBurnout(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_REV_ENGINE:
		pTempTask = rage_new CTaskVehicleRevEngine(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_GOFORWARD_HARD:
		pTempTask = rage_new CTaskVehicleGoForward(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration, true);
		break;
	case TEMPACT_SURFACE_SUBMARINE:
		pTempTask = rage_new CTaskVehicleSurfaceInSubmarine(NetworkInterface::GetSyncedTimeInMilliseconds() + nDuration);
		break;
	case TEMPACT_NONE:
		break;
	default:
		Assertf(0, "CarSetTempAction_OnUpdate invalid temp action");
	}

	return pTempTask;
}

/////////////////////////////////////////////////////////////////////////////////

aiTask* CVehicleIntelligence::CreateVehicleTaskForNetwork(u32 taskType)
{
	CAITarget target;
	sVehicleMissionParams params;
	switch (taskType)
	{
	case CTaskTypes::TASK_VEHICLE_ANIMATION:
		//return rage_new CTaskVehicleAnimation();
		break;
	case CTaskTypes::TASK_VEHICLE_APPROACH:
		return rage_new CTaskVehicleApproach(params);
	case CTaskTypes::TASK_VEHICLE_ATTACK:
		return rage_new CTaskVehicleAttack(params);
	case CTaskTypes::TASK_VEHICLE_BLOCK:
		return rage_new CTaskVehicleBlock(params);
	case CTaskTypes::TASK_VEHICLE_BLOCK_CRUISE_IN_FRONT:
		return rage_new CTaskVehicleBlockCruiseInFront(params);
	case CTaskTypes::TASK_VEHICLE_BLOCK_BRAKE_IN_FRONT:
		return rage_new CTaskVehicleBlockBrakeInFront(params);
	case CTaskTypes::TASK_VEHICLE_BLOCK_BACK_AND_FORTH:
		return rage_new CTaskVehicleBlockBackAndForth(params);
	case CTaskTypes::TASK_VEHICLE_BRAKE:
		return rage_new CTaskVehicleBrake();
	case CTaskTypes::TASK_VEHICLE_CIRCLE:
		return rage_new CTaskVehicleCircle(params);
	case CTaskTypes::TASK_VEHICLE_CONVERTIBLE_ROOF:
		//return rage_new CTaskVehicleConvertibleRoof();
		break;
	case CTaskTypes::TASK_VEHICLE_TRANSFORM_TO_SUBMARINE:
		//return rage_new CTaskVehicleTransformToSubmarine();
		break;
	case CTaskTypes::TASK_VEHICLE_CRASH:
		return rage_new CTaskVehicleCrash();
	case CTaskTypes::TASK_VEHICLE_CRUISE_NEW:
		return rage_new CTaskVehicleCruiseNew(params);
	case CTaskTypes::TASK_VEHICLE_CRUISE_BOAT:
		return rage_new CTaskVehicleCruiseBoat(params);
	case CTaskTypes::TASK_VEHICLE_DEAD_DRIVER:
		return rage_new CTaskVehicleDeadDriver();
	case CTaskTypes::TASK_VEHICLE_ESCORT:
		return rage_new CTaskVehicleEscort(params);
	case CTaskTypes::TASK_VEHICLE_FLEE:
		return rage_new CTaskVehicleFlee(params);
	case CTaskTypes::TASK_VEHICLE_FLEE_AIRBORNE:
		return rage_new CTaskVehicleFleeAirborne(params);
	case CTaskTypes::TASK_VEHICLE_FLEE_BOAT:
		return rage_new CTaskVehicleFleeBoat(params);
	case CTaskTypes::TASK_VEHICLE_FOLLOW:
		return rage_new CTaskVehicleFollow(params);
	case CTaskTypes::TASK_VEHICLE_FOLLOW_RECORDING:
		return rage_new CTaskVehicleFollowRecording(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW:
		return CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	case CTaskTypes::TASK_VEHICLE_GOTO_LONGRANGE:
		return rage_new CTaskVehicleGotoLongRange(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH:
		return rage_new CTaskVehicleGoToNavmesh(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_PLANE:
		return rage_new CTaskVehicleGoToPlane(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER:
		return rage_new CTaskVehicleGoToHelicopter(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE:
		return rage_new CTaskVehicleGoToSubmarine(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_BOAT:
		return rage_new CTaskVehicleGoToBoat(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE:
		return rage_new CTaskVehicleGoToPointAutomobile(params);
	case CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE:
		return rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params);
	case CTaskTypes::TASK_VEHICLE_HELI_PROTECT:
		return rage_new CTaskVehicleHeliProtect(params);
	case CTaskTypes::TASK_VEHICLE_HOVER:
		return rage_new CTaskVehicleHover();
	case CTaskTypes::TASK_VEHICLE_LAND:
		return rage_new CTaskVehicleLand(params);
	case CTaskTypes::TASK_VEHICLE_LAND_PLANE:
		return rage_new CTaskVehicleLandPlane();
	case CTaskTypes::TASK_VEHICLE_NO_DRIVER:
		return rage_new CTaskVehicleNoDriver();
	case CTaskTypes::TASK_VEHICLE_PARK_NEW:
		return rage_new CTaskVehicleParkNew(params);
	case CTaskTypes::TASK_VEHICLE_PLANE_CHASE:
		return rage_new CTaskVehiclePlaneChase(params, target);
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AUTOMOBILE:
		return rage_new CTaskVehiclePlayerDriveAutomobile();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AMPHIBIOUS_AUTOMOBILE:
		return rage_new CTaskVehiclePlayerDriveAmphibiousAutomobile();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BIKE:
		return rage_new CTaskVehiclePlayerDriveBike();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BOAT:
		return rage_new CTaskVehiclePlayerDriveBoat();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_SUBMARINE:
		return rage_new CTaskVehiclePlayerDriveSubmarine();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_SUBMARINECAR:
		return rage_new CTaskVehiclePlayerDriveSubmarineCar();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_PLANE:
		return rage_new CTaskVehiclePlayerDrivePlane();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_HELI:
		return rage_new CTaskVehiclePlayerDriveHeli();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AUTOGYRO:
		return rage_new CTaskVehiclePlayerDriveAutogyro();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_DIGGER_ARM:
		return rage_new CTaskVehiclePlayerDriveDiggerArm();
	case CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_TRAIN:
		return rage_new CTaskVehiclePlayerDriveTrain();
	case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR:
		return rage_new CTaskVehiclePoliceBehaviour(params);
	case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_BOAT:
		return rage_new CTaskVehiclePoliceBehaviourBoat(params);
	case CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_HELICOPTER:
		return rage_new CTaskVehiclePoliceBehaviourHelicopter(params);
	case CTaskTypes::TASK_VEHICLE_PULL_ALONGSIDE:
		return rage_new CTaskVehiclePullAlongside(params);
	case CTaskTypes::TASK_VEHICLE_PULL_OVER:
		return rage_new CTaskVehiclePullOver();
	case CTaskTypes::TASK_VEHICLE_PURSUE:
		return rage_new CTaskVehiclePursue(params);
	case CTaskTypes::TASK_VEHICLE_RAM:
		return rage_new CTaskVehicleRam(params);
	case CTaskTypes::TASK_VEHICLE_REACT_TO_COP_SIREN:
		return rage_new CTaskVehicleReactToCopSiren();
	case CTaskTypes::TASK_VEHICLE_STOP:
		return rage_new CTaskVehicleStop();
	case CTaskTypes::TASK_VEHICLE_SPIN_OUT:
		return rage_new CTaskVehicleSpinOut(params);
	case CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN:
		return rage_new CTaskVehicleThreePointTurn(params, false);
	default:
		Assertf(0, "CreateVehicleTaskForNetwork: Task %s is not supported", TASKCLASSINFOMGR.GetTaskName(taskType));

#if ENABLE_NETWORK_LOGGING
		// we have received a vehicle task we don't sync over the network yet, so log the task type
		NetworkInterface::GetObjectManagerLog().WriteDataValue("NON_SYNCED_VEHICLE_TASK", TASKCLASSINFOMGR.GetTaskName(taskType));
#endif
		break;
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////

void CVehicleIntelligence::Process_DummyFlags()
{
	//Ensure this is a law enforcement vehicle.
	if(!m_pVehicle->m_nVehicleFlags.bIsLawEnforcementVehicle)
	{
		return;
	}
	
	//Ensure the driver is valid.
	const CPed* pDriver = m_pVehicle->GetDriver();
	if(!pDriver)
	{
		return;
	}
	
	//Ensure the driver has an order.
	if(!pDriver->GetPedIntelligence()->GetOrder())
	{
		return;
	}
	
	//At this point, we have:
	//	1) A law enforcement vehicle,
	//	2) with a driver,
	//	3) that has an order.
	//Vehicles that meet these conditions should always be real, unless there is no collision.
	//If there is no collision, and the vehicle is forced to be real, it will fall through the map.
	
	//Force the vehicle to be real, unless there is no collision.
	CVehicleDummyHelper::Process(*m_pVehicle, CVehicleDummyHelper::ForceRealUnlessNoCollision);
}

void CVehicleIntelligence::Process_NearbyEntityLists()
{
	// Would be good to do this, I think, but we would have to be careful to check
	// if there are any users of the scanners for vehicles that are parked. Possibly
	// car alarms or reflections.
	//	if(sConsiderParkedVehicleForLod(m_pVehicle))
	//	{
	//		// Parked vehicles shouldn't need to scan, hopefully.
	//		return;
	//	}

	//Need to force updates during fast-moving chases, to ensure data for avoidance is up-to-date.
	bool bForceVehicleAndPedScans = m_pVehicle->m_TimeOfCreation == fwTimer::GetTimeInMilliseconds();
	bool bForceObjectScans = false;
	if(m_pVehicle->m_nVehicleFlags.bForceEntityScansAtHighSpeeds)
	{
		static dev_float s_fMinSpeed = 20.0f;
		bForceObjectScans = (m_pVehicle->GetAiXYSpeed() >= s_fMinSpeed);
		bForceVehicleAndPedScans |= bForceObjectScans;
	}

	//Update all peds and vehicles and objects in range.
	m_vehicleScanner.ScanForEntitiesInRange(*m_pVehicle, bForceVehicleAndPedScans);
	m_pedScanner.ScanForEntitiesInRange(*m_pVehicle, bForceVehicleAndPedScans);
	m_objectScanner.ScanForEntitiesInRange(*m_pVehicle, bForceObjectScans);
}

void CVehicleIntelligence::Process_CarBehind()
{
	//Ensure we are finding the car we are behind.
#if __BANK
	if(!CVehicleIntelligence::m_bFindCarBehind)
	{
		return;
	}
#endif	// __BANK
	
	CVehicle* pCarWeAreBehind = m_pCarWeAreBehind;
	aiAssert(m_TailgateDistributer.IsRegistered());

	//Check if the tailgate can be processed this frame.
	// Note: the "simple" version of ShouldBeProcessThisFrame() doesn't support the m_iMaxFramesBetweenUpdatesForIrregularUse feature.
	const bool processThisFrame = m_TailgateDistributer.ShouldBeProcessedThisFrameSimple();

	if(!pCarWeAreBehind && !processThisFrame)
	{
		return;
	}

	//Grab the vehicle.
	const CVehicle* pVeh = m_pVehicle;
	aiAssert(pVeh);
	
	//Grab the position.
	const Vec3V vPos = pVeh->GetVehiclePosition();
	
	//Grab the direction.
	const Vec3V vFwd = pVeh->GetVehicleForwardDirection();

	const Vec3V bndBoxMaxV = pVeh->GetBaseModelInfo()->GetBoundingBox().GetMax();
	
	//Calculate the front bumper position.
	const Vec3V vFrontBumperPos = AddScaled(vPos, vFwd, bndBoxMaxV.GetY());
	
	//Generate the max distance squared.
	ScalarV scMaxDistSq(Vec::V4VConstantSplat<FLOAT_TO_INT(625.0f)>());
	ScalarV scMaxDistSqForPlane(1225);

	if( m_pVehicle->InheritsFromPlane() )
	{
		scMaxDistSq = scMaxDistSqForPlane;
	}

	ScalarV scDistSq;
	if(processThisFrame)
	{
		//Update the vehicle we are behind.
		pCarWeAreBehind = FindCarWeAreBehind(vFwd, vFrontBumperPos, scMaxDistSq, scDistSq);
		SetCarWeAreBehind(pCarWeAreBehind);

		if(!pCarWeAreBehind)
		{
			m_pCarWeCouldBeSlipStreaming = NULL;
			return;
		}

#if __ASSERT
		// Should hold up, AreWeBehindCar() is called as a part of FindCarWeAreBehind().
		//	ScalarV scDistSq2;
		//	aiAssert(AreWeBehindCar(pCarWeAreBehind, vFwd, vFrontBumperPos, scMaxDistSq, scDistSq2, ScalarV(V_ZERO)));
		//	aiAssert(IsEqualAll(scDistSq2, scDistSq));
#endif
	}
	else
	{
		//Ensure we are still behind the car.
		if(!AreWeBehindCar(pCarWeAreBehind, vFwd, vFrontBumperPos, scMaxDistSq, scDistSq, ScalarV(V_ZERO)))
		{
			//Clear the car.
			SetCarWeAreBehind(NULL);
			pCarWeAreBehind = NULL;
		}
	}
	
	//Assign the distance behind the car, squared.
	StoreScalar32FromScalarV(m_fDistanceBehindCarSq, scDistSq);

	m_pCarWeCouldBeSlipStreaming = NULL;

	//Check to see if the players car should be slip streaming another car.
	if(CVehicle::sm_bSlipstreamingEnabled && pCarWeAreBehind)
	{
		CTaskVehicleMissionBase* pActiveTask = pVeh->GetIntelligence()->GetActiveTask();
		const bool bIsDriverPlayer = pActiveTask && CTaskTypes::IsPlayerDriveTask(pActiveTask->GetTaskType());

		if(bIsDriverPlayer)
		{
			scMaxDistSq = ScalarV(CVehicle::ms_fSlipstreamMaxDistance*CVehicle::ms_fSlipstreamMaxDistance);
			m_pCarWeCouldBeSlipStreaming = FindCarWeAreSlipstreamingBehind(vPos, vFwd, vFrontBumperPos, scMaxDistSq);

			m_fDistanceBehindCarWeCouldBeSlipStreamingSq = scDistSq.Getf();
		}

	}

	
#if __BANK
	if (CVehicleIntelligence::m_bDisplayDebugCarBehind && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vFrontBumperPos), 0.25f, Color_red);
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vFrontBumperPos), 0.25f, Color_blue);
		if(pCarWeAreBehind)
		{
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(vFrontBumperPos), VEC3V_TO_VECTOR3(pCarWeAreBehind->GetTransform().GetPosition()), Color_red, Color_blue);
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////

void CVehicleIntelligence::SetCarWeAreBehind(CVehicle* pCarWeAreBehind)
{
	if (m_pCarWeAreBehind)
	{
		m_pCarWeAreBehind->GetIntelligence()->AddCarsBehindUs(-m_NumCarsBehindContributedToCarWeAreBehind);
	}

	if (pCarWeAreBehind)
	{
		pCarWeAreBehind->GetIntelligence()->AddCarsBehindUs(m_NumCarsBehindUs + 1);
	}

	m_NumCarsBehindContributedToCarWeAreBehind = m_NumCarsBehindUs + 1;

	m_pCarWeAreBehind = pCarWeAreBehind;
}

void CVehicleIntelligence::AddCarsBehindUs(const s32 numCarsBehindUs)
{
	m_NumCarsBehindUs += numCarsBehindUs;

	Assertf(m_NumCarsBehindUs >= 0, "m_NumCarsBehindUs: %i, numCarsBehindUs: %i", m_NumCarsBehindUs, numCarsBehindUs);
}

void CVehicleIntelligence::Process_NumCarsBehindUs()
{
	SetCarWeAreBehind(m_pCarWeAreBehind);
}

bool CVehicleIntelligence::AreWeBehindCar(const CVehicle* pOtherVeh, Vec3V_In vFwd, Vec3V_In vFrontBumperPos, ScalarV_In scMaxDistSq, ScalarV_InOut RESTRICT scDistSq, ScalarV_In scDotLimit) const
{
	// For the math to work here, we can only handle a non-negative dot product limit,
	// i.e. no more than a 180 degree range.
	aiAssert(IsGreaterThanOrEqualAll(scDotLimit, ScalarV(V_ZERO)));

	// Grab the other position.
	const Vec3V vOtherPos = pOtherVeh->GetVehiclePosition();

	// Grab the other forward direction.
	const Vec3V vOtherFwd = pOtherVeh->GetVehicleForwardDirection();

	// Calculate the other back bumper position.
	const Vec3V bndBoxMinV = pOtherVeh->GetBaseModelInfo()->GetBoundingBox().GetMin();
	const Vec3V vOtherBackBumperPosNew = AddScaled(vOtherPos, vOtherFwd, bndBoxMinV.GetY());

	// Compute the delta vector between the bumper positions.
	const Vec3V vDiff = Subtract(vOtherBackBumperPosNew, vFrontBumperPos);

	// Ignore if Z diff is too great (Stunt races allow for cars to be driving on top of each other on overlapping tracks)
	TUNE_GROUP_FLOAT(VEH_INTELLIGENCE, HeightLimitForBehindCarDetection, 10.0f, 0.0f, 100.0f, 0.1f);
	if (Abs(vDiff.GetZf()) > HeightLimitForBehindCarDetection)
	{
		return false;
	}

	// Compute a 4D vector that contains the X and Y coordinates from the delta vector and the forward direction.
	const Vec4V diffAndFwdXXYYV = GetFromTwo<Vec::X1, Vec::X2, Vec::Y1, Vec::Y2>(Vec4V(vDiff), Vec4V(vFwd));

	// Compute the squared magnitude of both of these vectors by multiplying each element
	// by itself, then rotating the elements and adding them.
	const Vec4V diffAndFwdXXYYSqV = Scale(diffAndFwdXXYYV, diffAndFwdXXYYV);
	const Vec4V diffAndFwdXXYYSqRotV = diffAndFwdXXYYSqV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();
	const Vec4V diffAndFwdXXYYSqAddV = Add(diffAndFwdXXYYSqV, diffAndFwdXXYYSqRotV);
	const ScalarV diffFlatMagSqV = diffAndFwdXXYYSqAddV.GetX();
	const ScalarV fwdFlatMagSqV = diffAndFwdXXYYSqAddV.GetY();

	// Check if the squared magnitude of the flattened delta vector is less than the squared max distance.
	const BoolV withinRangeV = IsLessThanOrEqual(diffFlatMagSqV, scMaxDistSq);

	// Compute a 2D dot product of the delta vector and the forward vector
	// (neither of which is normalized at this point).
	// Note that a 2D dot product here is likely to be faster than a 3D dot product with Z cleared out,
	// since we have to multiply-add them together, or use the 360-specific dot product instruction that has high latency.
	const ScalarV dotV = Dot(Vec2V(vDiff.GetIntrin128()), Vec2V(vFwd.GetIntrin128()));

	// We basically want to check if the cosine of the angle between the	
	// vectors is larger than the threshold, i.e. something like this:
	//		(a*b)/(|a|*|b|) >= threshold
	// To avoid square roots and division, we multiply by |a|*|b| on both sides,
	// and then square:
	//		(a*b) >= threshold*|a|*|b|
	//		(a*b)^2 >= threshold^2*|a|^2*|b|^2
	// We know that the threshold is not negative and the lengths are not negative,
	// so the only thing we have to be careful about is the dot product
	// being negative, so we check for that here.
	const BoolV dotPositiveV = IsGreaterThanOrEqual(dotV, ScalarV(V_ZERO));

	// Compute the square of the dot product, and the square of the threshold (cosine of the angle).
	const ScalarV dotSqV = Scale(dotV, dotV);
	const ScalarV dotNormThresholdSqV = Scale(scDotLimit, scDotLimit);	// Could have just passed this in pre-squared.

	// Multiply together the right hand side of the inequality.
	const ScalarV dotThresholdSqV = Scale(dotNormThresholdSqV, Scale(diffFlatMagSqV, fwdFlatMagSqV));

	// Check if the dot product is above the threshold (after squaring both sides).
	const BoolV dotAboveThresholdV = IsGreaterThanOrEqual(dotSqV, dotThresholdSqV);

	// Combine the result: we need to be within range, have a positive dot product,
	// and the square of the dot product has to be large enough.
	const BoolV resultV = And(withinRangeV, And(dotPositiveV, dotAboveThresholdV));

	scDistSq = diffFlatMagSqV;

	return resultV.Getb();

	// Less optimized older code:
	//
	//	//Grab the other position.
	//	Vec3V vOtherPos = pOtherVeh->GetVehiclePosition();
	//
	//	//Grab the other forward direction.
	//	Vec3V vOtherFwd = pOtherVeh->GetVehicleForwardDirection();
	//
	//	//Calculate the other back bumper position.
	//	Vec3V vOtherBackBumperPos = Add(vOtherPos, Scale(vOtherFwd, pOtherVeh->GetBaseModelInfo()->GetBoundingBox().GetMin().GetY()));
	//
	//	//Generate the difference.
	//	Vec3V vDiff = Subtract(vOtherBackBumperPos, vFrontBumperPos);
	//
	//	//We only care about the XY plane when checking distance.
	//	Vec3V vDiffNoZ = vDiff;
	//	vDiffNoZ.SetZ(ScalarV(V_ZERO));
	//
	//	//Ensure the other vehicle is within range.
	//	ScalarV scDistSqReg = MagSquared(vDiffNoZ);
	//	scDistSq = scDistSqReg;
	//	if(IsGreaterThanAll(scDistSqReg, scMaxDistSq))
	//	{
	//		return false;
	//	}
	//
	//	//We only care about the XY plane when checking direction.
	//	Vec3V vFwdNoZ = vFwd;
	//	vFwdNoZ.SetZ(ScalarV(V_ZERO));
	//	Vec3V vFwdN = NormalizeFastSafe(vFwdNoZ, Vec3V(V_ZERO));
	//	vDiffNoZ = NormalizeFastSafe(vDiffNoZ, Vec3V(V_ZERO));
	//
	//	//Ensure the other vehicle is generally in front.
	//	ScalarV scDot = Dot(vFwdN, vDiffNoZ);
	//	if(IsLessThanAll(scDot, scDotLimit))
	//	{
	//		return false;
	//	}
	//
	//	return true;
}

CVehicle* CVehicleIntelligence::FindCarWeAreSlipstreamingBehind(Vec3V_ConstRef UNUSED_PARAM(vPos), Vec3V_ConstRef vFwd, Vec3V_ConstRef vFrontBumperPos, ScalarV_ConstRef scMaxDistSq) const
{
	//Grab the vehicle.
	CVehicle* pVeh = m_pVehicle;
	if(!pVeh)
	{
		return NULL;
	}

	//Keep track of the results.
	struct Results
	{
		Results()
			: m_pVehicle(NULL)
			, m_scDistSq()
		{
		}

		CVehicle*	m_pVehicle;
		ScalarV		m_scDistSq;
	};
	Results lResults;



	//Scan the nearby vehicles.
	CEntityScannerIterator entityList = GetVehicleScanner().GetIterator();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Ensure the entity does not match the vehicle.
		if(pEntity == pVeh)
		{
			continue;
		}

		//Grab the other vehicle.
		CVehicle* pOtherVeh = static_cast<CVehicle *>(pEntity);

		//Ensure we are behind the other vehicle.
		ScalarV scDistSq;
		static dev_float sfDotLimit = 0.98f;
		if(!AreWeBehindCar(pOtherVeh, vFwd, vFrontBumperPos, scMaxDistSq, scDistSq, ScalarV(sfDotLimit)))
		{
			continue;
		}

		//Ensure the other vehicle is a better potential result.
		if(lResults.m_pVehicle && IsGreaterThanAll(scDistSq, lResults.m_scDistSq))
		{
			continue;
		}

		//Assign the result.
		lResults.m_pVehicle = pOtherVeh;
		lResults.m_scDistSq = scDistSq;
	}

	return lResults.m_pVehicle;
}

// Adapted from geomBoxes::TestSegmentToBoxMinMax(), but tests four separate
// segments against one AABB.
// Note: p01X/Y/Z is the segment delta, rather than the second point on the segment,
// which is a difference compared to TestSegmentToBoxMinMax().
static VecBoolV_Out sTestSegmentToBoxMinMax(Vec4V_In p0X, Vec4V_In p0Y, Vec4V_In p0Z,
		Vec4V_In p01X, Vec4V_In p01Y, Vec4V_In p01Z,
		Vec3V_In boxMinV, Vec3V_In boxMaxV)
{
	const ScalarV halfV(V_HALF);

	const Vec3V boxHalfMaxV = Scale(boxMaxV, halfV);
	const Vec3V boxCenterV = AddScaled(boxHalfMaxV, boxMinV, halfV);
	const Vec3V boxHalfExtentsV = Subtract(boxMaxV, boxCenterV);
	// const Vec3V boxHalfExtentsV = SubtractScaled(boxHalfMaxV, boxMinV, halfV);

	const Vec4V boxHalfExtentsX = Vec4V(boxHalfExtentsV.GetX());
	const Vec4V boxHalfExtentsY = Vec4V(boxHalfExtentsV.GetY());
	const Vec4V boxHalfExtentsZ = Vec4V(boxHalfExtentsV.GetZ());
	const Vec4V boxCenterX = Vec4V(boxCenterV.GetX());
	const Vec4V boxCenterY = Vec4V(boxCenterV.GetY());
	const Vec4V boxCenterZ = Vec4V(boxCenterV.GetZ());

	// Note: only depend on the segments, could be reused for multiple boxes
	const Vec4V signedSegHalfExtentsX = Scale(p01X, halfV);
	const Vec4V signedSegHalfExtentsY = Scale(p01Y, halfV);
	const Vec4V signedSegHalfExtentsZ = Scale(p01Z, halfV);
	const Vec4V segCenterX = Add(p0X, signedSegHalfExtentsX);
	const Vec4V segCenterY = Add(p0Y, signedSegHalfExtentsY);
	const Vec4V segCenterZ = Add(p0Z, signedSegHalfExtentsZ);
	const Vec4V absSegHalfExtentsX = Abs(signedSegHalfExtentsX);
	const Vec4V absSegHalfExtentsY = Abs(signedSegHalfExtentsY);
	const Vec4V absSegHalfExtentsZ = Abs(signedSegHalfExtentsZ);

	// Translate segment relative to box center.
	const Vec4V centerDeltaX = Subtract(segCenterX, boxCenterX);
	const Vec4V centerDeltaY = Subtract(segCenterY, boxCenterY);
	const Vec4V centerDeltaZ = Subtract(segCenterZ, boxCenterZ);
	const Vec4V absCenterDeltaX = Abs(centerDeltaX);
	const Vec4V absCenterDeltaY = Abs(centerDeltaY);
	const Vec4V absCenterDeltaZ = Abs(centerDeltaZ);

	// Test for separating axis using aabb axes.
	const Vec4V sumHalfExtentsX = Add(absSegHalfExtentsX, boxHalfExtentsX);
	const Vec4V sumHalfExtentsY = Add(absSegHalfExtentsY, boxHalfExtentsY);
	const Vec4V sumHalfExtentsZ = Add(absSegHalfExtentsZ, boxHalfExtentsZ);
	const VecBoolV disjoint0X = IsGreaterThan(absCenterDeltaX, sumHalfExtentsX);
	const VecBoolV disjoint0Y = IsGreaterThan(absCenterDeltaY, sumHalfExtentsY);
	const VecBoolV disjoint0Z = IsGreaterThan(absCenterDeltaZ, sumHalfExtentsZ);

	// Test for separating axis using segment axes.
	// Almost a cross product, but not quite.
	const Vec4V tX1 = Scale(boxHalfExtentsY, absSegHalfExtentsZ);
	const Vec4V tY1 = Scale(boxHalfExtentsZ, absSegHalfExtentsX);
	const Vec4V tZ1 = Scale(boxHalfExtentsX, absSegHalfExtentsY);
	const Vec4V tX = AddScaled(tX1, boxHalfExtentsZ, absSegHalfExtentsY);
	const Vec4V tY = AddScaled(tY1, boxHalfExtentsX, absSegHalfExtentsZ);
	const Vec4V tZ = AddScaled(tZ1, boxHalfExtentsY, absSegHalfExtentsX);

	const Vec4V crossSegCenterSignedHalfExtentX1 = Scale(centerDeltaY, signedSegHalfExtentsZ);
	const Vec4V crossSegCenterSignedHalfExtentY1 = Scale(centerDeltaZ, signedSegHalfExtentsX);
	const Vec4V crossSegCenterSignedHalfExtentZ1 = Scale(centerDeltaX, signedSegHalfExtentsY);
	const Vec4V crossSegCenterSignedHalfExtentX = SubtractScaled(crossSegCenterSignedHalfExtentX1, centerDeltaZ, signedSegHalfExtentsY);
	const Vec4V crossSegCenterSignedHalfExtentY = SubtractScaled(crossSegCenterSignedHalfExtentY1, centerDeltaX, signedSegHalfExtentsZ);
	const Vec4V crossSegCenterSignedHalfExtentZ = SubtractScaled(crossSegCenterSignedHalfExtentZ1, centerDeltaY, signedSegHalfExtentsX);

	const VecBoolV disjoint1X = IsGreaterThan(Abs(crossSegCenterSignedHalfExtentX), tX);
	const VecBoolV disjoint1Y = IsGreaterThan(Abs(crossSegCenterSignedHalfExtentY), tY);
	const VecBoolV disjoint1Z = IsGreaterThan(Abs(crossSegCenterSignedHalfExtentZ), tZ);

	const VecBoolV disjointX = Or(disjoint0X, disjoint1X);
	const VecBoolV disjointY = Or(disjoint0Y, disjoint1Y);
	const VecBoolV disjointZ = Or(disjoint0Z, disjoint1Z);

	const VecBoolV disjoint = Or(Or(disjointX, disjointY), disjointZ);

	return InvertBits(disjoint);
}


CVehicle* CVehicleIntelligence::FindCarWeAreBehind(Vec3V_In vFwd, Vec3V_In vFrontBumperPos, ScalarV_In scMaxDistSq, ScalarV_InOut RESTRICT scDistSqOut) const
{
	//Grab the vehicle.
	aiAssert(m_pVehicle);
	CVehicle* pVeh = m_pVehicle;
	
	CVehicle* resultVehicle = NULL;
	ScalarV resultDistSqV = scMaxDistSq;

	aiAssert(pVeh->GetIntelligence() == this);
	CTaskVehicleMissionBase* pActiveTask = GetActiveTask();
	const bool bIsDriverPlayer = pActiveTask && CTaskTypes::IsPlayerDriveTask(pActiveTask->GetTaskType());
	
	//Check if we are using the route to find the car we are behind.
	if(CVehicleIntelligence::m_bUseRouteToFindCarBehind && !bIsDriverPlayer)
	{
		static const int numEntitiesToPrefetch = 4;

		CVehicle* pOtherVehicles[CEntityScanner::MAX_NUM_ENTITIES];
		int numOtherVehicles = 0;

		const CVehicleScanner& scanner = GetVehicleScanner();
		int numEntities = scanner.GetNumEntities();
		int prefetchCtr = numEntitiesToPrefetch;
		aiAssert(numEntities <= CEntityScanner::MAX_NUM_ENTITIES);
		for(int entIndex = 0; entIndex < numEntities; entIndex++)
		{
			CEntity* pEntity = scanner.GetEntityByIndex(entIndex);

			//Ensure the entity exists and does not match the vehicle.
			if(pEntity && pEntity != pVeh)
			{
				CVehicle* pOtherVeh = static_cast<CVehicle*>(pEntity);
				if(prefetchCtr > 0)
				{
					pOtherVeh->PrefetchPtrToMatrix();
					pOtherVeh->PrefetchPtrToModelInfo();
					prefetchCtr--;
				}
				pOtherVehicles[numOtherVehicles++] = pOtherVeh;
			}
		}

		//Keep track of the path segments.
		static const int sMaxPathSegments = 4;

		// We do this now using vectors, all start X coordinates in one, all start Y in another, etc.
		// If we needed to support more than four, we would need small arrays of vectors, which shouldn't
		// be too hard to set up but would be good to not have to worry about.
		CompileTimeAssert(sMaxPathSegments <= 4);
		Vec4V pathSegmentsStartX(V_ZERO);
		Vec4V pathSegmentsStartY(V_ZERO);
		Vec4V pathSegmentsStartZ(V_ZERO);
		Vec4V pathSegmentsEndX(V_ZERO);
		Vec4V pathSegmentsEndY(V_ZERO);
		Vec4V pathSegmentsEndZ(V_ZERO);
		VecBoolV pathSegmentsValid(V_ZERO);		// For keeping track of which elements are in use.

		int iNumSegments = 0;
		if(CVehicleIntelligence::m_bUseFollowRouteToFindCarBehind)
		{
			//Grab the follow route.
			const CVehicleFollowRouteHelper* pFollowRouteHelper = GetFollowRouteHelper();
			if(!pFollowRouteHelper)
			{
				return NULL;
			}
			
			//Grab the route points.
			const CRoutePoint* pRoutePoints = pFollowRouteHelper->GetRoutePoints();
			if(!pRoutePoints)
			{
				return NULL;
			}
			
			//Grab the number of points.
			s16 iNumPoints = pFollowRouteHelper->GetNumPoints();
			if(iNumPoints == 0)
			{
				return NULL;
			}
			
			//Grab the target point.
			const int iterStart = Max((int)pFollowRouteHelper->GetCurrentTargetPoint(), 1);

			aiAssert(iNumSegments < sMaxPathSegments);	// We only check after we add, now.
			
			//Generate the path segments.
			for(s32 i = iterStart; i < iNumPoints; ++i)
			{
				//Grab the route points.
				const CRoutePoint& rLastPoint = pRoutePoints[i - 1];
				const CRoutePoint& rNextPoint = pRoutePoints[i];
				
				//Make sure the lane has not changed a significant amount.
				if(IsGreaterThanAll(Abs(Subtract(rLastPoint.GetLane(), rNextPoint.GetLane())), ScalarV(V_HALF)))
				{
					break;
				}
				
				//Calculate the start and end positions.
				Vec3V vStart = rLastPoint.GetPosition();
				Vec3V vEnd = rNextPoint.GetPosition();
				
				//Account for the lane width.
				vStart = Add(vStart, rLastPoint.GetLaneCenterOffset());
				vEnd = Add(vEnd, rNextPoint.GetLaneCenterOffset());
				
				// Note: possibly we could use CVehicleFollowRouteHelper::m_vPathWaypointsAndSpeed here, to
				// access more compact memory and not have to add the lane center. Not sure to what extent
				// we can be sure that it's always up to date, etc, so holding off on that for now.

				//Append the start and end positions, y shifting the coordinates into the vectors.
				pathSegmentsStartX = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsStartX, Vec4V(vStart.GetX()));
				pathSegmentsStartY = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsStartY, Vec4V(vStart.GetY()));
				pathSegmentsStartZ = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsStartZ, Vec4V(vStart.GetZ()));
				pathSegmentsEndX = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsEndX, Vec4V(vEnd.GetX()));
				pathSegmentsEndY = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsEndY, Vec4V(vEnd.GetY()));
				pathSegmentsEndZ = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsEndZ, Vec4V(vEnd.GetZ()));
				pathSegmentsValid.SetIntrin128(GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(Vec4V(pathSegmentsValid), Vec4V(V_T_T_T_T)).GetIntrin128());

				if(++iNumSegments >= sMaxPathSegments)
				{
					break;
				}

				//Check if the end position has exceeded the maximum distance.
				ScalarV scDistSq = DistSquared(vFrontBumperPos, vEnd);
				if(IsGreaterThanAll(scDistSq, scMaxDistSq))
				{
					break;
				}
			}
		}
		else if(CVehicleIntelligence::m_bUseNodeListToFindCarBehind)
		{
			//Grab the node list.
			CVehicleNodeList* pNodeList = GetNodeList();
			if(!pNodeList)
			{
				return NULL;
			}
			
			//Initialize the current node.
			s32 iCurNode = pNodeList->GetTargetNodeIndex();
			Assertf(iCurNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED, "Invalid current node.");

			//Initialize the lane.
			s32 iLane = pNodeList->GetPathLaneIndex(iCurNode);

			aiAssert(iNumSegments < sMaxPathSegments);	// We only check after we add, now.

			//Generate the path segments.
			for(s32 i = iCurNode; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++i)
			{
				//Grab the last node.
				const CPathNode* pLastNode = pNodeList->GetPathNode(i - 1);
				if(!pLastNode)
				{
					break;
				}

				//Grab the next node.
				const CPathNode* pNode = pNodeList->GetPathNode(i);
				if(!pNode)
				{
					break;
				}

				//Find the link index.
				s16 iLinkIndex = -1;
				ThePaths.FindNodesLinkIndexBetween2Nodes(pLastNode, pNode, iLinkIndex);
				Assertf(iLinkIndex >= 0, "Invalid link index.");

				//Grab the link.
				const CPathNodeLink* pLink = &ThePaths.GetNodesLink(pLastNode, iLinkIndex);
				Assertf(pLink, "Invalid link.");

				//Calculate the lane center offset.
				float fLaneCenterOffset = ThePaths.GetLinksLaneCentreOffset(*pLink, iLane);

				//Calculate the lane center offset vector.
				Vector2 vSide = Vector2(pNode->GetPos().y - pLastNode->GetPos().y, pLastNode->GetPos().x - pNode->GetPos().x);
				vSide.Normalize();
				Vector3 vLaneCenterOffset((vSide.x * fLaneCenterOffset),(vSide.y * fLaneCenterOffset), 0.0f);

				//Calculate the start and end positions.
				Vec3V vStart = VECTOR3_TO_VEC3V(pLastNode->GetPos() + vLaneCenterOffset);
				Vec3V vEnd = VECTOR3_TO_VEC3V(pNode->GetPos() + vLaneCenterOffset);

				//Append the start and end positions.
				pathSegmentsStartX = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsStartX, Vec4V(vStart.GetX()));
				pathSegmentsStartY = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsStartY, Vec4V(vStart.GetY()));
				pathSegmentsStartZ = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsStartZ, Vec4V(vStart.GetZ()));
				pathSegmentsEndX = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsEndX, Vec4V(vEnd.GetX()));
				pathSegmentsEndY = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsEndY, Vec4V(vEnd.GetY()));
				pathSegmentsEndZ = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(pathSegmentsEndZ, Vec4V(vEnd.GetZ()));
				pathSegmentsValid.SetIntrin128(GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(Vec4V(pathSegmentsValid), Vec4V(V_T_T_T_T)).GetIntrin128());

				if(++iNumSegments >= sMaxPathSegments)
				{
					break;
				}

				//Check if the end position has exceeded the maximum distance.
				ScalarV scDistSq = DistSquared(vFrontBumperPos, vEnd);
				if(IsGreaterThanAll(scDistSq, scMaxDistSq))
				{
					break;
				}
			}
		}

		//Count the segments.
		if(iNumSegments == 0)
		{
			return NULL;
		}

		// Raise the segments up a bit, so we don't go underneath any bounding boxes.
		const Vec4V raiseV = Vec4V(V_HALF);	// Bumped up to 0.5 from 0.25, since the bounding boxes we get now tend to not extend as far down.
		pathSegmentsStartZ = Add(pathSegmentsStartZ, raiseV);
		pathSegmentsEndZ = Add(pathSegmentsEndZ, raiseV);

#if __BANK
		if(CVehicleIntelligence::m_bDisplayDebugCarBehindSearch && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			for(s32 i = 0; i < iNumSegments; ++i)
			{
				const int index = 3 - i;
				const Vec3V startV(pathSegmentsStartX[index], pathSegmentsStartY[index], pathSegmentsStartZ[index]);
				const Vec3V endV(pathSegmentsEndX[index], pathSegmentsEndY[index], pathSegmentsEndZ[index]);
				grcDebugDraw::Line(startV, endV, Color_purple, Color_purple, -1);
			}
		}
#endif
		
		// Scan the nearby vehicles.
		for(int vehIndex = 0; vehIndex < numOtherVehicles; vehIndex++)
		{
			const int prefetchIndex1 = vehIndex + numEntitiesToPrefetch;
			if(prefetchIndex1 < numOtherVehicles)
			{
				CVehicle* prefetchVeh = pOtherVehicles[prefetchIndex1];
				prefetchVeh->PrefetchPtrToMatrix();
				prefetchVeh->PrefetchPtrToModelInfo();
			}

			const int prefetchIndex2 = vehIndex + 2;
			if(prefetchIndex2 < numOtherVehicles)
			{
				CVehicle* prefetchVeh = pOtherVehicles[prefetchIndex2];
				PrefetchDC((void*)&prefetchVeh->GetMatrixRef());
				PrefetchDC((void*)((u8*)prefetchVeh->GetBaseModelInfo() + 32 /* OffsetOf(m_BoundingBox) */));
			}

			//Grab the other vehicle.
			CVehicle* pOtherVeh = pOtherVehicles[vehIndex];

			// Ensure we are behind the other vehicle.
			// Note: we pass in resultDistSqV here, may as well let AreWeBehindCar()
			// reject vehicles further away than the closest we've found so far.
			ScalarV scDistSq;
			if(!AreWeBehindCar(pOtherVeh, vFwd, vFrontBumperPos, resultDistSqV, scDistSq, ScalarV(V_ZERO)))
			{
				continue;
			}
			
			// Ensure the other vehicle is a better potential result.
			// - no longer needed, already rejected by AreWeBehindCar.
			//	if(resultVehicle && IsGreaterThanAll(scDistSq, resultDistSqV))
			//	{
			//		continue;
			//	}
			aiAssert(!resultVehicle || !IsGreaterThanAll(scDistSq, resultDistSqV));

			//Grab the other forward direction.
			const Vec3V vOtherFwd = pOtherVeh->GetVehicleForwardDirection();

			// Grab the min/max bounding box values.
			// We used to get these like this:
			//	otherBoundBoxMinV = RCC_VEC3V(pOtherVeh->GetBoundingBoxMin());
			//	otherBoundBoxMaxV = RCC_VEC3V(pOtherVeh->GetBoundingBoxMax());
			// but there is quiet a bit more overhead that way than using the bounding
			// box from the model info, which should generally be accurate enough.

			const CBaseModelInfo* pOtherVehModelInfo = pOtherVeh->GetBaseModelInfo();
			Vec3V otherBoundBoxMinV = RCC_VEC3V(pOtherVehModelInfo->GetBoundingBoxMin());
			Vec3V otherBoundBoxMaxV = RCC_VEC3V(pOtherVehModelInfo->GetBoundingBoxMax());

			//Fatten up the bounding box a bit.
			const Vec3V fattenV(V_QUARTER);
			otherBoundBoxMinV = Subtract(otherBoundBoxMinV, fattenV);
			otherBoundBoxMaxV = Add(otherBoundBoxMaxV, fattenV);

#if __BANK
			if(CVehicleIntelligence::m_bDisplayDebugCarBehindSearch && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				grcDebugDraw::BoxOriented(otherBoundBoxMinV, otherBoundBoxMaxV, pOtherVeh->GetMatrix(), Color_purple, false, -1);
			}
#endif
			
			// Test each segment against the box.
			// Since we currently only have up to four segments and they all fit in one
			// set of vectors, there is no actual loop, but we would have to do one here if
			// we needed more than four.
			do
			{
				// What we do here is to test all (up to) four segments in parallel, which
				// should be making efficient use of the vector pipeline.

				//Calculate the segments.

				const Vec4V segmentXV = Subtract(pathSegmentsEndX, pathSegmentsStartX);
				const Vec4V segmentYV = Subtract(pathSegmentsEndY, pathSegmentsStartY);
				const Vec4V segmentZV = Subtract(pathSegmentsEndZ, pathSegmentsStartZ);

				const Vec4V segmentXSqV = Scale(segmentXV, segmentXV);
				const Vec4V segmentXYSqV = AddScaled(segmentXSqV, segmentYV, segmentYV);
				const Vec4V segmentMagSqV = AddScaled(segmentXYSqV, segmentZV, segmentZV);

				const Vec4V otherFwdXV = Vec4V(vOtherFwd.GetX());
				const Vec4V otherFwdYV = Vec4V(vOtherFwd.GetY());
				const Vec4V otherFwdZV = Vec4V(vOtherFwd.GetZ());

				//The vehicle must be pointed in the same general direction as the segment.

				const Vec4V dotXV = Scale(segmentXV, otherFwdXV);
				const Vec4V dotXYV = AddScaled(dotXV, segmentYV, otherFwdYV);
				const Vec4V dotV = AddScaled(dotXYV, segmentZV, otherFwdZV);
				const Vec4V dotSqV = Scale(dotV, dotV);
				const Vec4V dotThresholdSqV = Scale(segmentMagSqV, ScalarV(V_QUARTER));	// cosine of the angle has to be more than 0.5
				const ScalarV zeroV(V_ZERO);
				const VecBoolV pointingInSameDirectionV = And(IsGreaterThanOrEqual(dotV, Vec4V(zeroV)), IsGreaterThanOrEqual(dotSqV, dotThresholdSqV));

				//Transform the segment into car space.

				const Mat34V& mtrx = pOtherVeh->GetMatrixRef();
				const Vec3V mtrxA = mtrx.GetCol0();
				const Vec3V mtrxB = mtrx.GetCol1();
				const Vec3V mtrxC = mtrx.GetCol2();
				const Vec3V mtrxD = mtrx.GetCol3();

				const ScalarV mtrxDXV = mtrxD.GetX();
				const ScalarV mtrxDYV = mtrxD.GetY();
				const ScalarV mtrxDZV = mtrxD.GetZ();

				const Vec4V tmpXV = Subtract(pathSegmentsStartX, Vec4V(mtrxDXV));
				const Vec4V tmpYV = Subtract(pathSegmentsStartY, Vec4V(mtrxDYV));
				const Vec4V tmpZV = Subtract(pathSegmentsStartZ, Vec4V(mtrxDZV));

				const ScalarV mtrxAXV = mtrxA.GetX();
				const ScalarV mtrxAYV = mtrxA.GetY();
				const ScalarV mtrxAZV = mtrxA.GetZ();

				const Vec4V start1LXxV = Scale(tmpXV, mtrxAXV);
				const Vec4V start1LXxyV = AddScaled(start1LXxV, tmpYV, mtrxAYV);
				const Vec4V start1LXV = AddScaled(start1LXxyV, tmpZV, mtrxAZV);

				const Vec4V segment1LXxV = Scale(segmentXV, mtrxAXV);
				const Vec4V segment1LXxyV = AddScaled(segment1LXxV, segmentYV, mtrxAYV);
				const Vec4V segment1LXV = AddScaled(segment1LXxyV, segmentZV, mtrxAZV);

				const ScalarV mtrxBXV = mtrxB.GetX();
				const ScalarV mtrxBYV = mtrxB.GetY();
				const ScalarV mtrxBZV = mtrxB.GetZ();

				const Vec4V start1LYxV = Scale(tmpXV, mtrxBXV);
				const Vec4V start1LYxyV = AddScaled(start1LYxV, tmpYV, mtrxBYV);
				const Vec4V start1LYV = AddScaled(start1LYxyV, tmpZV, mtrxBZV);

				const Vec4V segment1LYxV = Scale(segmentXV, mtrxBXV);
				const Vec4V segment1LYxyV = AddScaled(segment1LYxV, segmentYV, mtrxBYV);
				const Vec4V segment1LYV = AddScaled(segment1LYxyV, segmentZV, mtrxBZV);

				const ScalarV mtrxCXV = mtrxC.GetX();
				const ScalarV mtrxCYV = mtrxC.GetY();
				const ScalarV mtrxCZV = mtrxC.GetZ();

				const Vec4V start1LZxV = Scale(tmpXV, mtrxCXV);
				const Vec4V start1LZxyV = AddScaled(start1LZxV, tmpYV, mtrxCYV);
				const Vec4V start1LZV = AddScaled(start1LZxyV, tmpZV, mtrxCZV);

				const Vec4V segment1LZxV = Scale(segmentXV, mtrxCXV);
				const Vec4V segment1LZxyV = AddScaled(segment1LZxV, segmentYV, mtrxCYV);
				const Vec4V segment1LZV = AddScaled(segment1LZxyV, segmentZV, mtrxCZV);

				// Test the segments.

				VecBoolV segsIntersectBoxV = sTestSegmentToBoxMinMax(start1LXV, start1LYV, start1LZV,
						segment1LXV, segment1LYV, segment1LZV,
						otherBoundBoxMinV, otherBoundBoxMaxV);

				VecBoolV matchesV = And(pathSegmentsValid, And(pointingInSameDirectionV, segsIntersectBoxV));

				if(!IsEqualIntAll(matchesV, VecBoolV(zeroV.GetIntrin128())))
				{
					//Assign the result.
					resultVehicle = pOtherVeh;
					resultDistSqV = scDistSq;
					
					// No need to continue testing segments.
					// - but no real loop to break out of now.
					break;
				}
			} while(0);
		}
	}
	else if(CVehicleIntelligence::m_bUseScannerToFindCarBehind || bIsDriverPlayer)
	{
		//Scan the nearby vehicles.
		CEntityScannerIterator entityList = GetVehicleScanner().GetIterator();
		for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
		{
			//Ensure the entity does not match the vehicle.
			if(pEntity == pVeh)
			{
				continue;
			}

			//Grab the other vehicle.
			CVehicle* pOtherVeh = static_cast<CVehicle *>(pEntity);
			
			//Ensure we are behind the other vehicle.
			ScalarV scDistSq;
			if(!AreWeBehindCar(pOtherVeh, vFwd, vFrontBumperPos, scMaxDistSq, scDistSq, ScalarV(V_ZERO)))
			{
				continue;
			}

			//Ensure the other vehicle is a better potential result.
			if(resultVehicle && IsGreaterThanAll(scDistSq, resultDistSqV))
			{
				continue;
			}

			//Assign the result.
			resultVehicle = pOtherVeh;
			resultDistSqV = scDistSq;
		}
	}
	else if(CVehicleIntelligence::m_bUseProbesToFindCarBehind)
	{
		//Calculate the probe end position.
		Vec3V vEnd = vFrontBumperPos + Scale(vFwd, ScalarVFromF32(25.0f));
		
#if __BANK
		if (CVehicleIntelligence::m_bDisplayDebugCarBehindSearch && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(vFrontBumperPos), VEC3V_TO_VECTOR3(vEnd), Color_purple, Color_purple, -1);
		}
#endif

		//Shoot a probe from the front bumper position in the forward direction.
		WorldProbe::CShapeTestFixedResults<> probeResult;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(vFrontBumperPos), VEC3V_TO_VECTOR3(vEnd));
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);
		probeDesc.SetExcludeEntity(pVeh);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			//Grab the entity.
			CEntity* pEntity = CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());
			Assertf(pEntity, "Entity is not valid.");
			Assertf(pEntity->GetIsTypeVehicle(), "Entity is not a vehicle.");
			
			//Grab the other vehicle.
			CVehicle* pOtherVeh = static_cast<CVehicle *>(pEntity);
			
			//Ensure we are behind the other vehicle.
			ScalarV scDistSq;
			if(AreWeBehindCar(pOtherVeh, vFwd, vFrontBumperPos, scMaxDistSq, scDistSq, ScalarV(V_ZERO)))
			{
				//Assign the result.
				resultVehicle = pOtherVeh;
				resultDistSqV = scDistSq;
			}
		}
	}

	scDistSqOut = resultDistSqV;
	return resultVehicle;
}

/////////////////////////////////////////////////////////////////////////////////

void CVehicleIntelligence::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	//Find the lowest/simplest task in the hierarchy that overrides the camera and use its settings.

	aiTask *task		= m_TaskManager.GetActiveLeafTask(VEHICLE_TASK_TREE_PRIMARY);
	CTask* activeTask	= static_cast<CTask*>(task);
	while(activeTask)
	{
		tGameplayCameraSettings tempSettings(settings);

		activeTask->GetOverriddenGameplayCameraSettings(tempSettings);

		//NOTE: A task must override the camera in order to override the other settings.
		if(tempSettings.m_CameraHash)
		{
			settings = tempSettings;
			break;
		}

		activeTask = activeTask->GetParent();
	}
}

#if 0
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindClosestIndex
// PURPOSE :  Given a coordinate this function will return the index of a 
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehPointPath::FindClosestIndex(const Vector3 &Crs, int &index, float &distOnLine)
{
	if (m_numPoints <= 1)
	{
		return false;
	}

	float	closestDistSqr = (9999999.9f * 9999999.9f);
	index = 0;
	distOnLine = 0.0f;

	for (s32 i = 0; i < m_numPoints-1; i++)
	{
		float	lineDirX = m_points[i+1].x - m_points[i].x;
		float	lineDirY = m_points[i+1].y - m_points[i].y;
		float	deltaX = Crs.x - m_points[i].x;
		float	deltaY = Crs.y - m_points[i].y;

		float	fDistanceAlongLine = deltaX * lineDirX + deltaY * lineDirY;
		float	lineDirLengthSqr = lineDirX*lineDirX + lineDirY*lineDirY;
		fDistanceAlongLine /= lineDirLengthSqr;

		float	distSqr;
		if (fDistanceAlongLine <= 0.0f)
		{
			float	dX = Crs.x - m_points[i].x;
			float	dY = Crs.y - m_points[i].y;
			distSqr = dX * dX + dY * dY;
			fDistanceAlongLine = 0.0f;
		}
		else if (fDistanceAlongLine >= 1.0f)
		{
			float	dX = Crs.x - m_points[i+1].x;
			float	dY = Crs.y - m_points[i+1].y;
			distSqr = dX * dX + dY * dY;
			fDistanceAlongLine = 1.0f;
		}
		else
		{
			float	perpX = lineDirY;
			float	perpY = -lineDirX;
			float	dX = Crs.x - m_points[i].x;
			float	dY = Crs.y - m_points[i].y;

			distSqr = (perpX * dX + perpY * dY);
			distSqr = (distSqr * distSqr) / lineDirLengthSqr;
		}

		if (distSqr < closestDistSqr)
		{
			closestDistSqr = distSqr;

			index = i;
			distOnLine = fDistanceAlongLine;
		}
	}
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shift
// PURPOSE :  Given a coordinate this function will return the index of a 
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
void CVehPointPath::Shift(int number)
{
	if (number == 0)
	{
		return;
	}

	Assert(number <= m_numPoints);

	number = MIN(number, m_numPoints);

	m_numPoints -= number;

	for (int i = 0; i < m_numPoints; i++)
	{
		m_points[i].x = m_points[i+number].x;
		m_points[i].y = m_points[i+number].y;
	}
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CVehPointPath::Display
// PURPOSE :  A debug function to display point paths. (Collection of points that a vehicle
//			  has passed and that can be used to drive back out of trouble once stuck.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
void CVehPointPath::RenderDebug() const
{
	rage::Color32 col = rage::Color32(255, 200, 255, 255);
	//rage::Color32 col2 = rage::Color32(128, 128, 128, 128);

	for (s32 i = 0; i < m_numPoints; i++)
	{
		grcDebugDraw::Line(Vector3(m_points[i].x, m_points[i].y, 0.0f), 
			Vector3(m_points[i].x, m_points[i].y, 30.0f), 
			col);
	}

	for (s32 i = 0; i < m_numPoints-1; i++)
	{
		grcDebugDraw::Line(Vector3(m_points[i].x, m_points[i].y, CGameWorld::FindLocalPlayerCoors().z+3.0f), 
			Vector3(m_points[i+1].x, m_points[i+1].y, CGameWorld::FindLocalPlayerCoors().z+3.0f), 
			col);
	}
}


#endif // __BANK

#endif //0

#if __BANK

float	CVehicleIntelligence::ms_fDisplayVehicleAIRange = 30.0f;
bool	CVehicleIntelligence::m_bDisplayCarAiDebugInfo = false;
int		CVehicleIntelligence::m_eFilterVehicleDisplayByType = 0;
bool	CVehicleIntelligence::ms_debugDisplayFocusVehOnly = false;
bool	CVehicleIntelligence::m_bDisplayCarAddresses = false;
bool	CVehicleIntelligence::m_bDisplayDirtyCarPilons = false;
bool	CVehicleIntelligence::m_bDisplayCarAiTaskHistory = false;
bool	CVehicleIntelligence::m_bDisplayVehicleFlagsInfo = false;
bool	CVehicleIntelligence::m_bDisplayCarAiTaskInfo = false;
bool	CVehicleIntelligence::m_bFindCoorsAndSpeed = false;
bool	CVehicleIntelligence::m_bDisplayCarFinalDestinations = false;
bool	CVehicleIntelligence::m_bDisplayVehicleRecordingInfo = false;
bool	CVehicleIntelligence::m_bDisplayCarAiTaskDetails = false;
bool	CVehicleIntelligence::m_bDisplayDebugInfoOkToDropOffPassengers = false;
bool	CVehicleIntelligence::m_bDisplayDebugJoinWithRoadSystem = false;
bool	CVehicleIntelligence::m_bDisplayAvoidanceCache = false;
bool	CVehicleIntelligence::m_bDisplayRouteInterceptionTests = false;
bool    CVehicleIntelligence::m_bHideFailedRouteInterceptionTests = false;
bool	CVehicleIntelligence::m_bDisplayDebugFutureNodes = false;
bool	CVehicleIntelligence::m_bDisplayDebugFutureFollowRoute = false;
bool	CVehicleIntelligence::m_bDisplayDrivableExtremes = false;
bool	CVehicleIntelligence::m_bDisplayControlDebugInfo = false;
bool	CVehicleIntelligence::m_bDisplayAILowLevelControlInfo = false;
bool	CVehicleIntelligence::m_bDisplayControlTransmissionInfo = false;
bool	CVehicleIntelligence::m_bDisplayStuckDebugInfo = false;
bool	CVehicleIntelligence::m_bDisplaySlowDownForBendDebugInfo = false;
bool	CVehicleIntelligence::m_bDisplayDebugSlowForTraffic = false;
bool	CVehicleIntelligence::m_bDisplayDebugCarBehind = false;
bool	CVehicleIntelligence::m_bDisplayDebugCarBehindSearch = false;
bool	CVehicleIntelligence::m_bDisplayDebugTailgate = false;
bool	CVehicleIntelligence::m_bAICarHandlingDetails = false;
bool	CVehicleIntelligence::m_bAICarHandlingCurve = false;
bool	CVehicleIntelligence::m_bOnlyForPlayerVehicle = false;
bool	CVehicleIntelligence::m_bAllCarsStop = false;
bool	CVehicleIntelligence::m_bDisplayInterestingDrivers = false;
bool	CVehicleIntelligence::m_bFindCarBehind = true;
bool	CVehicleIntelligence::m_bDisplayStatusInfo = false;

#endif
float	CVehicleIntelligence::ms_fMoveSpeedBelowWhichToCheckIfStuck = 0.4f;
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
bool	CVehicleIntelligence::m_bTailgateOtherCars = false;
#else
bool	CVehicleIntelligence::m_bTailgateOtherCars = true;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
bool	CVehicleIntelligence::m_bUseRouteToFindCarBehind = true;
bool	CVehicleIntelligence::m_bUseScannerToFindCarBehind = false;
bool	CVehicleIntelligence::m_bUseProbesToFindCarBehind = false;
bool	CVehicleIntelligence::m_bUseFollowRouteToFindCarBehind = true;
bool	CVehicleIntelligence::m_bUseNodeListToFindCarBehind = false;
bool	CVehicleIntelligence::m_bEnableNewJoinToRoadInAnyDir = true;
bool	CVehicleIntelligence::m_bEnableThreePointTurnCenterProbe = true;

#if __DEV

s32	Debug_Mission = MISSION_CRUISE;
float	Debug_CruiseSpeed = 10.0f;
Vector3	Debug_TargetPos = Vector3(0.0f, 0.0f, 0.0f);
bool	Debug_DoEverythingInReverse = false;
bool	Debug_UseLastCreatedVehicleAsTarget = false;
u32		Debug_DrivingMode = DMode_StopForCars;
bool	Debug_bHasHeliRequestedOrientation = false;
float	Debug_HeliRequestedOrientation = 0.0f;
s32	Debug_FlightHeight = 30;
s32	Debug_MinHeightAboveTerrain = 20;
float	Debug_TargetReachedDist = 4.0f;
float	Debug_StraightLineDist = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST;



///////////////////////////////////////////////////////////////////////////

void CopySettingsIntoVeh(CVehicle* pVeh)
{
	Assert(pVeh);

	// First make sure the vehicle has a driver(or it won't go anywhere)
	CVehiclePopulation::AddDriverAndPassengersForVeh(pVeh, 0, CVehicleAILodManager::ComputeRealDriversAndPassengersRemaining());
	pVeh->PopTypeSet(POPTYPE_TOOL);
	if(pVeh->GetDriver())
	{
		pVeh->GetDriver()->m_PedConfigFlags.SetCantBeKnockedOffVehicle( KNOCKOFFVEHICLE_NEVER );
		pVeh->GetDriver()->PopTypeSet(POPTYPE_TOOL);
		pVeh->GetDriver()->SetDefaultDecisionMaker();
		pVeh->GetDriver()->SetCharParamsBasedOnManagerType();
		pVeh->GetDriver()->GetPedIntelligence()->SetDefaultRelationshipGroup();

		CTaskControlVehicle *pTask = NULL;

		CTask* pCarTask = rage_new CTaskVehicleStop();
		pTask = rage_new CTaskControlVehicle(pVeh, pCarTask);

		pVeh->GetDriver()->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_DEFAULT);
	}


		aiTask* pNewTask = CVehicleIntelligence::GetTaskFromMissionIdentifier(pVeh, 
													   (VehMissionType)Debug_Mission,
													   NULL,
													   &Debug_TargetPos,
													   Debug_DrivingMode,
													   Debug_TargetReachedDist,
													   (u16)Debug_StraightLineDist,
													   Debug_CruiseSpeed,
													   false,
													   Debug_DoEverythingInReverse);

	CTaskVehicleMissionBase* pVehMissionTask= static_cast<CTaskVehicleMissionBase*>(pNewTask);

	// For certain missions we assume the player is the target.
	switch(Debug_Mission)
	{
	case MISSION_BLOCK:
	case MISSION_FOLLOW:
	case MISSION_ATTACK:
	case MISSION_RAM:
	case MISSION_FLEE:
	case MISSION_CIRCLE:
	case MISSION_ESCORT_LEFT:
	case MISSION_ESCORT_RIGHT:
	case MISSION_ESCORT_REAR:
	case MISSION_ESCORT_FRONT:
		if(Debug_UseLastCreatedVehicleAsTarget && CVehicleFactory::GetPreviouslyCreatedVehicle())
		{
			if(CVehicleFactory::GetPreviouslyCreatedVehicle()->GetDriver())
			{
				pVehMissionTask->SetTargetEntity(CVehicleFactory::GetPreviouslyCreatedVehicle()->GetDriver());
			}
			else
			{
				pVehMissionTask->SetTargetEntity(CVehicleFactory::GetPreviouslyCreatedVehicle());
			}
		}
		else
		{
			pVehMissionTask->SetTargetEntity(FindPlayerPed());
		}
		break;
	}

	pVeh->GetIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, pNewTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);

	CVehicleNodeList * pNodeList = pVeh->GetIntelligence()->GetNodeList();
	if(pNodeList)
		pNodeList->ClearPathNodes();

	// Also set a radar blip for this car to make it easier to follow.
	CEntityPoolIndexForBlip vehicleIndex(pVeh, BLIP_TYPE_CAR);

	CMiniMap::CreateBlip(true, BLIP_TYPE_CAR, vehicleIndex, BLIP_DISPLAY_BOTH, "car ai debug");
}

///////////////////////////////////////////////////////////////////////////

void CopySettingsIntoFocusVehs()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pEnt);
			Assert(pVeh);

			CopySettingsIntoVeh(pVeh);
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void CopySettingsIntoLastCreatedVeh()
{
	CVehicle* pVeh = CVehicleFactory::GetCreatedVehicle();
	if(!pVeh)
	{
		return;
	}
	CopySettingsIntoVeh(pVeh);
}


///////////////////////////////////////////////////////////////////////////

void CopySettingsFromVeh(CVehicle* ASSERT_ONLY(pVeh))
{
	Assert(pVeh);

/* TODO
	CVehMission* pMission = pVeh->GetIntelligence()->FindUberMissionForCar();
	//Assert(pMission);

	if(pMission)
	{
		Debug_Mission = pMission->m_missionType;
		Debug_CruiseSpeed = pMission->m_cruiseSpeed;
		Debug_TargetPos = pMission->m_targetPos;
		Debug_DoEverythingInReverse = pMission->m_bDoEverythingInReverse;
		Debug_DrivingMode = pMission->m_drivingMode;
		Debug_bHasHeliRequestedOrientation = pMission->m_bHasHeliRequestedOrientation;
		Debug_HeliRequestedOrientation = pMission->m_heliRequestedOrientation;
		Debug_FlightHeight = pMission->m_flightHeight;
		Debug_MinHeightAboveTerrain = pMission->m_minHeightAboveTerrain;
		Debug_TargetReachedDist = pMission->m_targetArriveDist;
		Debug_StraightLineDist = pMission->m_straightLineDist;
	}
*/
}

void CopySettingsFromFocusVeh()
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh)
	{
		return;
	}

	Assert(pVeh);

	CopySettingsFromVeh(pVeh);
}

void CopySettingsFromLastCreatedVeh()
{
	CVehicle* pVeh = CVehicleFactory::GetCreatedVehicle();
	if(!pVeh)
	{
		return;
	}

	CopySettingsFromVeh(pVeh);
}

void AllCarsPullOver()
{
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVeh;
	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);
		if(pVeh && !pVeh->IsNetworkClone())
		{
			Vector3 dummyDir;
			sVehicleMissionParams params;
			params.SetTargetPosition(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()));
			aiTask *pCarTask = rage_new CTaskVehicleParkNew(params,dummyDir,CTaskVehicleParkNew::Park_PullOver, 0.08f);
			pVeh->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
		}
	}
}

#endif // __DEV

#if __DEV

void CVehicleIntelligence::AddBankWidgets(bkBank& bank)
{
	bank.PushGroup("Set Autopilot", false);
		bank.AddCombo("Select Mission", &Debug_Mission, MISSION_LAST, CVehicleIntelligence::MissionStrings, NullCB); 
		bank.AddSlider("Select Cruise speed", &Debug_CruiseSpeed, 0, 300, 1);
		bank.AddSlider("TargetPos X", &Debug_TargetPos.x, -4000.0f, 4000.0f, 10.0f);
		bank.AddSlider("TargetPos Y", &Debug_TargetPos.y, -4000.0f, 4000.0f, 10.0f);
		bank.AddSlider("TargetPos Z", &Debug_TargetPos.z, -4000.0f, 4000.0f, 10.0f);
		bank.AddToggle("Do everything in reverse", &Debug_DoEverythingInReverse);
		bank.AddToggle("Use last created vehicle as target", &Debug_UseLastCreatedVehicleAsTarget);
		bank.AddSlider("Select driving mode", &Debug_DrivingMode, 0, 0xFFFFFFFF, 1);
		bank.AddSlider("TargetReachedDist", &Debug_TargetReachedDist, -1, 10000, 1);
		bank.AddSlider("StraightLineDist", &Debug_StraightLineDist, -1, 10000, 1);

		bank.AddToggle("HasHeliRequestedOrientation", &Debug_bHasHeliRequestedOrientation);
		bank.AddSlider("HeliRequestedOrientation", &Debug_HeliRequestedOrientation, -1.0f, 6.28f, 1.0f);
		bank.AddSlider("FlightHeight", &Debug_FlightHeight, 0, 10000, 5);
		bank.AddSlider("MinHeightAboveTerrain", &Debug_MinHeightAboveTerrain, 0, 10000, 5);

		bank.AddButton("Copy settings into focus veh(s)", datCallback(CFA(CopySettingsIntoFocusVehs)));
		bank.AddButton("Copy settings from focus veh", datCallback(CFA(CopySettingsFromFocusVeh)));//not hooked up yet
		bank.AddButton("Copy settings into last created veh", datCallback(CFA(CopySettingsIntoLastCreatedVeh)));
		bank.AddButton("Copy settings from last created veh", datCallback(CFA(CopySettingsFromLastCreatedVeh)));
		bank.AddButton("Toggle Scripted Autopilot On Focus Vehicle", CVehicle::ToggleScriptedAutopilot);
	bank.PopGroup();

	bank.AddButton("Make all cars pull over", datCallback(CFA(AllCarsPullOver)));
	bank.AddToggle("Make all cars stop", &CVehicleIntelligence::m_bAllCarsStop);
}
#endif // __BANK


#if __BANK
bool CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(const CVehicle* const pVeh)
{
	bool displayDebugInfo =(!CVehicleIntelligence::ms_debugDisplayFocusVehOnly ||(CDebugScene::FocusEntities_IsInGroup(pVeh)));
	if(!displayDebugInfo)
	{
		return false;
	}

	if ( CVehicleIntelligence::m_eFilterVehicleDisplayByType-1 != VEHICLE_TYPE_NONE 
		&& CVehicleIntelligence::m_eFilterVehicleDisplayByType-1 != pVeh->GetVehicleType() )
	{
		return false;
	}

	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
	Vector3 vDiff = vVehiclePosition - camInterface::GetPos();
	float fDist = vDiff.Mag();
	if(fDist >= CVehicleIntelligence::ms_fDisplayVehicleAIRange && !CVehicleIntelligence::ms_debugDisplayFocusVehOnly && !CVehicleIntelligence::m_bOnlyForPlayerVehicle) 
		return false;

	if(!CVehicleIntelligence::m_bOnlyForPlayerVehicle)
	{
		return true;
	}

	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	Assert(pPlayer);
	if(pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && (pPlayer->GetMyVehicle() == pVeh))
	{
		return true;
	}
	else
	{
		return false;
	}
}
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeOffsetForCar
// PURPOSE :  To give traffic a bit more of a chaotic feel the cars randomise the
//			  coordinates that they go to by a small amount.
//			  This function finds the offset that this particular car uses.
//			  This offset gets applied to each autopilotNodeIndex.
/////////////////////////////////////////////////////////////////////////////////
void CVehicleIntelligence::FindNodeOffsetForCarFromRandomSeed(u16 randomSeed, Vector3 &Result)
{
	float	Angle =(randomSeed & 255) *(6.28f / 256.0f);
	Result = Vector3(0.7f * rage::Sinf(Angle), 0.7f * rage::Cosf(Angle), 0.0f);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalculateSpeedMultiplierBend
// PURPOSE :  Works out how fast we want to go considering the orientation of the path
//			  ahead of us.
/////////////////////////////////////////////////////////////////////////////////
float CVehicleIntelligence::CalculateSpeedMultiplierBend(const CVehicle* const pVeh, float vehDriveOrientation, float StraightLineSpeed)
{
	CVehicleNodeList * pNodeList = pVeh->GetIntelligence()->GetNodeList();
	Assertf(pNodeList, "CVehicleIntelligence::CalculateSpeedMultiplierBend - vehicle has no node list");
	if(!pNodeList)
		return 1.0f;

	static	float	maxSpeedMult;
	maxSpeedMult = 1.0f;
	static	float	maxBrakeDist = 60.0f;

	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());

	s32 iOldNode = Clamp(pNodeList->GetTargetNodeIndex() - 1, 0, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED);

	// This is the new method inherited from the racing ai
	for(s32 node = iOldNode; node < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-2; node++)		// This used to start at NODE_NEW. This would cause cars to accelerate on junctions even though they hadn't cleared the junction yet.
	{
		s32	node2 = node+1;
		CNodeAddress	nodeAddress = pNodeList->GetPathNodeAddr(node);
		CNodeAddress	node2Address = pNodeList->GetPathNodeAddr(node2);

		if(nodeAddress.IsEmpty() || node2Address.IsEmpty())
		{
			continue;
		}

		if(!ThePaths.IsRegionLoaded(nodeAddress) || !ThePaths.IsRegionLoaded(node2Address))
		{
			continue;
		}

		const CPathNode *pNode = ThePaths.FindNodePointer(nodeAddress);
		const CPathNode *pNode2 = ThePaths.FindNodePointer(node2Address);

		Vector3	coors, coors2;
		pNode->GetCoors(coors);
		float	dist =(coors - vVehiclePosition).XYMag();

		dist = rage::Max(dist - 20.0f, 0.0f);	// A certain area of max braking. We never quite get there anyway.

		if(dist > maxBrakeDist)
		{
			node = 999;
		}
		else
		{
			pNode2->GetCoors(coors2);
			Vector3 diff = coors2 - coors;
			diff.z = 0.0f;
			diff.Normalize();
			float	linkOrientation = fwAngle::GetATanOfXY(diff.x, diff.y);

			float mult = FindSpeedMultiplier(pVeh, linkOrientation-vehDriveOrientation, PI * 0.05f, PI * 0.35f, 0.25f, 1.0f);
			mult = 1.0f -((1.0f - mult) *(1.0f - dist/maxBrakeDist));
			maxSpeedMult = rage::Min(maxSpeedMult, mult);

			// Now also calculate an absolute maximum speed.
			// This will fix cases where the cruiseSpeed for a car is set very high.
			// A maxSpeedMult of 0.5f will still not slow it down enough.
			static	float	slowSpeed = 11.0f;
			static	float	fastSpeed = 62.0f;
			float maxSpeed = FindSpeedMultiplier(pVeh, linkOrientation-vehDriveOrientation, PI * 0.05f, PI * 0.35f, slowSpeed, fastSpeed);
			mult = maxSpeed / StraightLineSpeed;
			mult = 1.0f -((1.0f - mult) *(1.0f - dist/maxBrakeDist));
			maxSpeedMult = rage::Min(maxSpeedMult, mult);
		}
	}

	// We never want to slow cars down below a certain speed because of a bend.(6 m/s is pretty slow)
	float smallestSpeedMult = 6.0f / StraightLineSpeed;
	maxSpeedMult = Clamp(maxSpeedMult, smallestSpeedMult, 1.0f);
#if __DEV
	if(m_bDisplaySlowDownForBendDebugInfo && AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		char debugText[15];
		sprintf(debugText, "Bend:%.2f", maxSpeedMult);
		grcDebugDraw::Text(vVehiclePosition + Vector3(0.f,0.f,1.5f), CRGBA(255, 255, 255, 255), debugText);
	}
#endif

	return maxSpeedMult;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSpeedMultiplier
// PURPOSE :  From the angle to the target this function calculates what the
//			  speed multiplier should be.
//			  MinSpeedAngle(angle for which minimum speed is returned)
//			  MaxSpeedAngle(angle for which maximum speed is returned)
//			  MinSpeed(the minimum speed, say 0.5)
/////////////////////////////////////////////////////////////////////////////////
float CVehicleIntelligence::FindSpeedMultiplier(const CVehicle* pVehicle, float Angle, float MinSpeedAngle, float MaxSpeedAngle, float MinSpeed, float MaxSpeed)
{
	float	Result;

#if __BANK
	if (CVehiclePopulation::ms_bEverybodyCruiseInReverse)
	{
		Angle = fwAngle::LimitRadianAngleSafe(Angle);
	}
	else
#endif //BANK
	{
		Angle = fwAngle::LimitRadianAngle(Angle);
	}

	// DirAngleDiff is between 0.0f and PI.
	// 0.0f .. 0.5f -> full speed
	// 0.5f .. 1.2f -> slow down. 1.2f and larger results in 0.3f
	Angle = ABS(Angle);
	Angle = rage::Max(0.0f, Angle - MinSpeedAngle);

	// Modify grip loss based on handling and material we're on
	float fTractionLoss = 1.0f;
	if (pVehicle)
	{
		TUNE_GROUP_FLOAT(VEH_INTELLIGENCE, MaterialFrictionCoefficient, 1.0f, 0.0f, 100.0f, 0.1f);

		float fWheelGrip = 0.0f;
		for(int i=0; i<pVehicle->GetNumWheels(); i++)
		{
			fWheelGrip += pVehicle->GetWheel(i)->GetMaterialGrip();
		}
		fWheelGrip = pVehicle->GetNumWheels() > 0 ? fWheelGrip / pVehicle->GetNumWheels() : 1.0f;

		fTractionLoss += ((fWheelGrip - 1.0f)) * MaterialFrictionCoefficient * pVehicle->pHandling->m_fTractionLossMult;
	}

	//const float fOrigResult = MaxSpeed -(Angle /(MaxSpeedAngle - MinSpeedAngle)) *(MaxSpeed - MinSpeed);
	//const float fDriverAbility = CDriverPersonality::FindDriverAbility(pVehicle->GetDriver());

	//the turning circles are tuned for roads, so allow us to slow down
	//more for non-road surfaces
	MinSpeed *= fTractionLoss;
	
	Result = MaxSpeed - (Angle /((MaxSpeedAngle - MinSpeedAngle) /* * fTractionLoss*/)) *(MaxSpeed - MinSpeed);

// 	if (fOrigResult > Result)
// 	{
// 		aiDisplayf("fOrigResult: %.2f, NewResult: %.2f", fOrigResult, Result);
// 	}

	if(Angle >(MaxSpeedAngle - MinSpeedAngle))
	{
		Result = MinSpeed;
	}

	Result = Max(Result, MinSpeed);

	return Result;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindTargetCoorsWithNode
// PURPOSE :  Given a autopilotNodeIndex and the lane that the car is in this function returns the
//			  actual world coordinates the car will aim for.
/////////////////////////////////////////////////////////////////////////////////
void CVehicleIntelligence::FindTargetCoorsWithNode(const CVehicle* const pVeh, CNodeAddress PreviousNode, CNodeAddress Node, CNodeAddress NextNode, s32 regionLinkIndexPreviousNodeToNode, s32 regionLinkIndexNodeToNextNode, s32 LaneIndexOld, s32 LaneIndexNew, Vector3 &Result)
{
	Assert(!Node.IsEmpty());
	Assert(ThePaths.IsRegionLoaded(Node));
	Assert(LaneIndexOld >= 0 && LaneIndexOld <= 6);
	Assert((LaneIndexNew >= 0 && LaneIndexNew <= 6) || NextNode.IsEmpty());
	Assert(PreviousNode != Node);
	Assert(NextNode != Node);
	Assert(pVeh);

	Vector2	Dir(0.0f, 0.0f);
	Vector3 laneCentreOffsetVec(0.0f, 0.0f, 0.0f);
	Vector2 CombinedDir(0.0f, 0.0f);

	const CPathNode *pNode = ThePaths.FindNodePointer(Node);
	Vector3 thisNodeCoors;
	pNode->GetCoors(thisNodeCoors);

	bool bUseReducedOffset = false;

	Assert(pVeh->GetVehicleModelInfo());
	if( pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) /*|| (pVeh->GetIntelligence()->m_LaneShift!=0.0f)*/ )
	{
		bUseReducedOffset = true;
	}

	bool	bIsSingleTrackRoad = false;

	// Calculate the offset from the actual autopilotNodeIndex coordinates based on the previous and next nodes.

	TUNE_GROUP_BOOL(CAR_AI, treatAllRoadsAsNarrow, false);

	if(!NextNode.IsEmpty() && !Node.IsEmpty() && ThePaths.IsRegionLoaded(NextNode))
	{
		Assert(regionLinkIndexNodeToNextNode >= 0 && regionLinkIndexNodeToNextNode < 30000);
		CPathNodeLink *pLink = ThePaths.FindLinkPointerSafe(Node.GetRegion(),regionLinkIndexNodeToNextNode);
		Vector2 nextNodeCoors;
		ThePaths.FindNodePointer(NextNode)->GetCoors2(nextNodeCoors);

		Dir = nextNodeCoors - Vector2(thisNodeCoors, Vector2::kXY);
		Dir.Normalize();
		CombinedDir += Dir;

		float laneCentreOffset = ThePaths.GetLinksLaneCentreOffset(*pLink, LaneIndexNew);

		laneCentreOffsetVec.x += Dir.y * laneCentreOffset;
		laneCentreOffsetVec.y -= Dir.x * laneCentreOffset;
		laneCentreOffsetVec.z = 0.0f;

		if (pLink->IsSingleTrack())
		{
			bIsSingleTrackRoad = true;
		}

		if(treatAllRoadsAsNarrow || pLink->m_1.m_NarrowRoad)
		{
			bUseReducedOffset = true;
		}
	}

	if(!PreviousNode.IsEmpty() && ThePaths.IsRegionLoaded(PreviousNode))
	{
		Assert(regionLinkIndexPreviousNodeToNode >= 0 && regionLinkIndexPreviousNodeToNode < 30000);
		CPathNodeLink *pLink = ThePaths.FindLinkPointerSafe(PreviousNode.GetRegion(),regionLinkIndexPreviousNodeToNode);

		Vector2 previousNodeCoors;
		ThePaths.FindNodePointer(PreviousNode)->GetCoors2(previousNodeCoors);

		Dir = Vector2(thisNodeCoors, Vector2::kXY) - previousNodeCoors;
		Dir.Normalize();
		CombinedDir += Dir;

		// If there already is a laneCentreOffsetVec(from the next autopilotNodeIndex) we reduce it's contribution to that in
		// the direction of this segment.
		float	dot = laneCentreOffsetVec.x * Dir.x + laneCentreOffsetVec.y * Dir.y;
		laneCentreOffsetVec.x = Dir.x * dot;
		laneCentreOffsetVec.y = Dir.y * dot;
		laneCentreOffsetVec.z = 0.0f;

		float laneCentreOffset = ThePaths.GetLinksLaneCentreOffset(*pLink, LaneIndexOld);

		if (pLink->IsSingleTrack())
		{
			bIsSingleTrackRoad = true;
		}

		laneCentreOffsetVec.x += Dir.y * laneCentreOffset;
		laneCentreOffsetVec.y -= Dir.x * laneCentreOffset;
		laneCentreOffsetVec.z = 0.0f;

		if(treatAllRoadsAsNarrow || pLink->m_1.m_NarrowRoad)
		{
			bUseReducedOffset = true;
		}
	}	

	// Vehicles can have a consistent offset from the centre line of the road.(Used to have 2 convoys of bikes next to each other in e1)
	CombinedDir.NormalizeSafe();
	//laneCentreOffsetVec.x += CombinedDir.y * pVeh->GetIntelligence()->m_LaneShift;	//laneshift was always zero
	//laneCentreOffsetVec.y -= CombinedDir.x * pVeh->GetIntelligence()->m_LaneShift;	//laneshift was always 0
	laneCentreOffsetVec.z = 0.0f;

	Result = thisNodeCoors + laneCentreOffsetVec;

	Vector3	RandomOffset(VEC3_ZERO);
	TUNE_GROUP_BOOL(CAR_AI, useRandomOffsets, true);
	if(useRandomOffsets)
	{
		FindNodeOffsetForCar(pVeh, RandomOffset);
	}

	TUNE_GROUP_FLOAT(CAR_AI, sfDefaultOffsetMult, 1.0f, 0.0f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAR_AI, sfWaterOffsetMult, 10.0f, 0.0f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAR_AI, sfReducedOffsetMult, 0.25f, 0.0f, 1.0f, 0.01f);
	float	offsetMultiplier =(pNode->m_2.m_waterNode)? sfWaterOffsetMult : sfDefaultOffsetMult;
	if( bUseReducedOffset )
	{
		offsetMultiplier *= sfReducedOffsetMult;
	}

	//if we're on a single track road, only apply a random offset to the right
	if (bIsSingleTrackRoad)
	{
		Vector3 CombinedRight(CombinedDir.y, -CombinedDir.x, 0.0f);
		if (RandomOffset.Dot(CombinedRight) < 0.0f)
		{
			RandomOffset.Negate();
		}
	}

	Result.x += offsetMultiplier*RandomOffset.x;
	Result.y += offsetMultiplier*RandomOffset.y;
	// And leave z untouched.
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNodeOffsetForCar
// PURPOSE :  To give traffic a bit more of a chaotic feel the cars randomise the
//			  coordinates that they go to by a small amount.
//			  This function finds the offset that this particular car uses.
//			  This offset gets applied to each autopilotNodeIndex.
/////////////////////////////////////////////////////////////////////////////////
void CVehicleIntelligence::FindNodeOffsetForCar(const CVehicle* const pVeh, Vector3 &Result)
{
	FindNodeOffsetForCarFromRandomSeed(pVeh->GetRandomSeed(), Result);

	if (pVeh && pVeh->m_nVehicleFlags.bIsBig)
	{
		Result *= 0.15f;
	}
	else if (pVeh && pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS))
	{
		//kind of hacky: sports cars tend to go faster so try to keep them toward the middle
		//of the lane
		Result *= 0.5f;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSpeedMultiplierWithSpeedFromNodes
// PURPOSE :  Given the speed value that comes from the nodes we return the speed for a car.
/////////////////////////////////////////////////////////////////////////////////
float CVehicleIntelligence::MultiplyCruiseSpeedWithMultiplierAndClip(float CruiseSpeed, float SpeedMultiplierFromNodes, const bool bShouldCapSpeedToSpeedZones, const bool bIsMission, const Vector3& vPosition)
{
	TUNE_GROUP_FLOAT(CAR_AI, MaxSpeed, 35.0f, 0.0f, 100.0f, 1.0f);
	// When the cruise speed gets multiplier with the speed from the node we still want it to be a reasonable number so it has to be clipped.
	float retVal = CruiseSpeed * SpeedMultiplierFromNodes;
	if(SpeedMultiplierFromNodes > 1.0f)
	{
		retVal = Clamp(retVal, CruiseSpeed, MaxSpeed);	// 25 is fast enough(90 km/h)
	}

	if (bShouldCapSpeedToSpeedZones)
	{
		retVal = rage::Min(retVal, CRoadSpeedZoneManager::GetInstance().FindMaxSpeedForThisPosition(vPosition, bIsMission));
	}

	return retVal;
}

void CVehicleIntelligence::UpdateIsThisADriveableCar()
{
	CVehicle* pVehicle = m_pVehicle;
	bool bCheckForPlayer = !pVehicle->GetDriver() || (FindPlayerPed() == pVehicle->GetDriver());

	const bool wasDriveableCar = pVehicle->m_nVehicleFlags.bIsThisADriveableCar;
	pVehicle->m_nVehicleFlags.bIsThisADriveableCar = pVehicle->GetVehicleDamage()->GetIsDriveable(bCheckForPlayer);

	//Set an event for the vehicle being undrivable.
	if (NetworkInterface::IsGameInProgress() && wasDriveableCar && !pVehicle->m_nVehicleFlags.bIsThisADriveableCar)
	{
		if (!pVehicle->IsNetworkClone() && pVehicle->GetNetworkObject())
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkVehicleUndrivable(pVehicle, pVehicle->GetWeaponDamageEntity(), (int)pVehicle->GetWeaponDamageHash()));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateIsThisAParkedCar
// PURPOSE :  Returns true if this car should be treated as an obstacle and not as
//			  traffic. This will be the case for parked cars.
/////////////////////////////////////////////////////////////////////////////////

void CVehicleIntelligence::UpdateIsThisAParkedCar()
{
	CVehicle* pVehicle = m_pVehicle;
	pVehicle->m_nVehicleFlags.bIsThisAParkedCar = false;

	if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
	{
		pVehicle->m_nVehicleFlags.bIsThisAParkedCar = true;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateIsThisAStationaryCar
// PURPOSE :  Returns true if this car should be treated as an obstacle and not as
//			  traffic. This will be the case for parked cars.
/////////////////////////////////////////////////////////////////////////////////

void CVehicleIntelligence::UpdateIsThisAStationaryCar()
{
	CVehicle* pVehicle = m_pVehicle;

	pVehicle->m_nVehicleFlags.bIsOvertakingStationaryCar = false;
	pVehicle->m_nVehicleFlags.bForceOtherVehiclesToOvertakeThisVehicle = false;

	// If this car is set to be marked as stationary cars will steer round it.
	// If the override flag is set to make sure other cars stop, make sure its not flagged.
	// bForceOtherVehiclesToStopForThisVehicle is a reset flag so ensure its reset here so it can be set anywhere
	// else in the frame.
	if( pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle )
	{
		pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = false;
		pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = false;
		return;
	}

	// The following may still be the case even when IsThisAParkedCar returned false. Perhaps setting the avoidance data didn't happen.
	if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
	{
		pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = true;
		return;
	}
	
	CVehicle* pParentVehicle = pVehicle->m_nVehicleFlags.bHasParentVehicle 
		? CTaskVehicleGoToPointWithAvoidanceAutomobile::GetTrailerParentFromVehicle(pVehicle)
		: NULL;

	// Trailers inherit their stationary status from the cab
	if( pVehicle->InheritsFromTrailer() )
	{
		if(pParentVehicle)
		{
			pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = pParentVehicle->m_nVehicleFlags.bIsThisAStationaryCar;
			return;
		}
		
		pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = true;
		return;
	}

	// If we're a car attached to a trailer, inherit our stationary status from the trailer
	if (pParentVehicle && pParentVehicle->InheritsFromTrailer())
	{
		pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = pParentVehicle->m_nVehicleFlags.bIsThisAStationaryCar;
		return;
	}

	// If the car hasn't had a driver for a little while or has a dead driver it is also considered a parked car.
	u32 uTimeSinceLastDriverForStationary = pVehicle->m_nVehicleFlags.bBecomeStationaryQuicker ? 500 : 5000;
	if ( /*pVehicle->m_LastTimeWeHadADriver && not sure why we needed this. Caused problems with mission vehicles blocking road before being used.*/
		(NetworkInterface::GetSyncedTimeInMilliseconds() > pVehicle->m_LastTimeWeHadADriver + uTimeSinceLastDriverForStationary) || 
		(pVehicle->GetDriver() && pVehicle->GetDriver()->IsDead()))
	{
		pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = true;
		return;
	}			

	// If the car was told to stop (like the taxis in a taxi rank) it is also considered a parked car.
 	if (pVehicle->GetDriver() && !pVehicle->GetDriver()->IsPlayer())
 	{
		CTaskVehicleMissionBase* pActiveTask = dynamic_cast<CTaskVehicleMissionBase*>(GetActiveTask());
		if(pActiveTask == NULL || pActiveTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_STOP || pActiveTask->GetCruiseSpeed() <= 0.0f )
		{			
			pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = true;
			return;
		}
 	}

	// If the car is upside down or blown up it will not move.
	if (pVehicle->m_nPhysicalFlags.bRenderScorched || !pVehicle->m_nVehicleFlags.bIsThisADriveableCar || pVehicle->GetVehicleUpDirection().GetZf() < 0.2f)
	{
		pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = true;
		return;
	}

	// If the player is controlling this car but he is not moving and it's not a mission vehicle
	// except if it's a personal vehicle (always classed as mission vehicle)
	if (pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer() && (!pVehicle->PopTypeIsMission() || pVehicle->IsPersonalVehicle()))
	{
		if ((fwTimer::GetTimeInMilliseconds() - LastTimeNotStuck) > 4000)
		{
			pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = true;
			return;
		}
	}
	
	pVehicle->m_nVehicleFlags.bIsThisAStationaryCar = false;
}

void CVehicleIntelligence::UpdateTimeNotMoving(float fTimeStep)
{
	//TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_moveSpeedBelowWhichToCheckIfStuck, 0.4f, 0.0f, 100.0f, 0.1f);

	const CPed* driver = m_pVehicle->GetDriver();
	bool bIsDriverPlayer = driver ? driver->IsPlayer() : false;

	if (bIsDriverPlayer) // Make sure the player is actually in control of the car
	{
		CTaskVehicleMissionBase* pActiveTask = GetActiveTask();
		bIsDriverPlayer = pActiveTask && CTaskTypes::IsPlayerDriveTask(pActiveTask->GetTaskType());
	}

	if (!bIsDriverPlayer && m_pVehicle->GetAiXYSpeed() < ms_fMoveSpeedBelowWhichToCheckIfStuck)
	{
		const u32 timeStepInMs = (u32)(1000.0f*fTimeStep);
		MillisecondsNotMoving += timeStepInMs;
	}
	else
	{
		MillisecondsNotMoving = 0;
	}
}

CTaskVehicleMissionBase* CVehicleIntelligence::CreateCruiseTask( CVehicle& in_Vehicle, sVehicleMissionParams& params, const bool bSpeedComesFromVehPopulation )
{
	if ( in_Vehicle.InheritsFromBoat() )
	{
		return rage_new CTaskVehicleCruiseBoat(params, bSpeedComesFromVehPopulation);
	}
	else
	{
		return rage_new CTaskVehicleCruiseNew(params, bSpeedComesFromVehPopulation);
	}

}

CTaskVehicleMissionBase* CVehicleIntelligence::CreateAutomobileGotoTask( sVehicleMissionParams& params, const float fStraightLineDist)
{
	return rage_new CTaskVehicleGoToAutomobileNew(params, fStraightLineDist);
}

int CVehicleIntelligence::GetAutomobileGotoTaskType()
{
	return CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW;
}

void CVehicleIntelligence::HandleEvents()
{
	//don't bother handling any events if we don't have pretend occupants
	if (!ShouldHandleEvents())
	{
		return;
	}

	VehicleEventPriority::VehicleEventPriority currentEventPriority = m_PretendOccupantEventPriority;

	AI_EVENT_GROUP_LOCK(&m_eventGroup);

	CEvent* pHighestPriorityEvent = m_eventGroup.GetHighestPriorityEvent();
	if (pHighestPriorityEvent)
	{
		VehicleEventPriority::VehicleEventPriority highestEventPriority = pHighestPriorityEvent->GetVehicleResponsePriority();
		if (highestEventPriority > currentEventPriority)
		{
			sVehicleMissionParams params;
			CTask* pTask = pHighestPriorityEvent->CreateVehicleReponseTaskWithParams(params);
			Assert(pTask);

			SetPretendOccupantEventData(highestEventPriority, params);

			//Assert(!m_CurrentEvent);
			if (m_CurrentEvent)
			{
				delete m_CurrentEvent;
			}
			m_CurrentEvent = static_cast<CEvent*>(pHighestPriorityEvent->Clone());

			AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
		}
	}

	m_eventGroup.TickEvents();

}

bool CVehicleIntelligence::ShouldStopForSiren()
{
	bool bIsSirenValid =	(m_pVehicle->m_nVehicleFlags.GetIsSirenOn() && 
		(m_pVehicle->m_nVehicleFlags.bIsLawEnforcementVehicle || 
		m_pVehicle->m_nVehicleFlags.bIsAmbulanceOnDuty || 
		m_pVehicle->m_nVehicleFlags.bIsFireTruckOnDuty));

	//check siren is on
	if( !bIsSirenValid )
	{
		return false;
	}

	//just obey all mission vehicles or if we've never had a driver (road block)
	if( m_pVehicle->PopTypeIsMission() || m_pVehicle->m_LastTimeWeHadADriver == 0 )
	{
		return true;
	}

	//not valid if our driver is dead
	const CPed* pDriver = m_pVehicle->GetDriver();
	if( pDriver && pDriver->IsDead() )
	{
		return false;
	}

	//not valid if we don't have a driver AND we have a last driver AND the last driver is dead
	const CPed* pLastDriver	= m_pVehicle->GetLastDriver();
	if( !pDriver && pLastDriver && pLastDriver->IsDead() )
	{
		return false;
	}

	const bool bNoDriverInfo			= !pDriver && !pLastDriver;
	const bool bDriverIsAlive			= pDriver && !pDriver->IsDead();
	const bool bNoDriverButLastAlive	= !pDriver && pLastDriver && !pLastDriver->IsDead();

	//Valid if current driver is alive OR no current driver but last driver is alive 
	//OR we've no driver information but 30 seconds since we've been driven (happens drivers are removed)
	return bDriverIsAlive || bNoDriverButLastAlive || ( bNoDriverInfo && NetworkInterface::GetSyncedTimeInMilliseconds() < m_pVehicle->m_LastTimeWeHadADriver + 30000 );
}

/////////////////////////////////////////////////////////////////////////////////

CHeliIntelligence::CHeliIntelligence(CVehicle *pVehicle)
: CVehicleIntelligence(pVehicle)
, m_AvoidanceDistributer(CExpensiveProcess::VPT_HeliAvoidanceDistributer)
, m_fTimeSpentLanded(0.0f)
, m_fTimeLandingAreaBlocked(0.0f)
, m_bProcessAvoidance(false)
{
	m_crashTurnSpeed = fwRandom::GetRandomNumberInRange(2.0f, 8.0f);
	m_OldOrientation = 0.0f;
	m_OldTilt = 0.0f;
	m_fHeliAvoidanceHeight = 0.0f;
	m_pExtraEntityToDoHeightChecksFor = NULL;
	bHasCombatOffset = false;
}

/////////////////////////////////////////////////////////////////////////////////

void	CHeliIntelligence::Process(bool fullUpdate, float fFullUpdateTimeStep)
{
	CVehicleIntelligence::Process(fullUpdate, fFullUpdateTimeStep);

	UpdateOldPositionVaraibles();
	
	//Process the avoidance.
	ProcessAvoidance();

	//Process the landed state.
	ProcessLanded();
}

/////////////////////////////////////////////////////////////////////////////////

void	CHeliIntelligence::UpdateOldPositionVaraibles()
{
	// The following 2 are used by the ai steering code.
	const Vector3 vecForward(VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetB()));
	m_OldOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
	m_OldTilt = rage::Atan2f(vecForward.z, vecForward.XYMag());
}

void CHeliIntelligence::MarkLandingAreaBlocked()
{
	m_fTimeLandingAreaBlocked = 1.0f;
}

bool CHeliIntelligence::IsLandingAreaBlocked() const
{
	return m_fTimeLandingAreaBlocked > 0.0f;
}


/////////////////////////////////////////////////////////////////////////////////

bool CHeliIntelligence::IsLanded() const
{
	return (m_pVehicle->GetNumContactWheels() > 1);
}

void CHeliIntelligence::ProcessAvoidance()
{
	//Ensure we should process avoidance.
	if(!m_bProcessAvoidance)
	{
		return;
	}
	
	//Reset the flag.
	m_bProcessAvoidance = false;
	
	//Ensure the avoidance distributer is registered.
	if(!m_AvoidanceDistributer.IsRegistered())
	{
		m_AvoidanceDistributer.RegisterSlot();
	}
	
	//Ensure the avoidance should be processed this frame.
	if(!m_AvoidanceDistributer.ShouldBeProcessedThisFrame())
	{
		return;
	}
	
	//Define the closest heli structure.
	struct ClosestHeli
	{
		ClosestHeli()
		: m_pHeli(NULL)
		, m_scHeight(V_ZERO)
		{
		
		}
		
		const CHeli*	m_pHeli;
		ScalarV			m_scHeight;
	};
	
	//Find the heli that is:
	//1) Within range
	//2) Closest to our height
	//3) Below us
	//The goal of this is to adjust ourselves upward based on the heli that is closest below us.
	ClosestHeli closestHeli;
	
	//Initialize the tuning values.
	TUNE_GROUP_FLOAT(HELI_AI, fAvoidanceMaxDist,	40.0f, 0.0f, 50.0f, 1.0f);
	TUNE_GROUP_FLOAT(HELI_AI, fAvoidanceOffset,		15.0f, 0.0f, 50.0f, 1.0f);

	//Grab the position.
	Vec3V vPosition = m_pVehicle->GetTransform().GetPosition();
	
	//Grab the height.
	ScalarV scHeight = vPosition.GetZ();

	//Iterate over the nearby vehicles.
	CEntityScannerIterator entityList = GetVehicleScanner().GetIterator();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		//Grab the vehicle.
		Assert(pEntity->GetIsTypeVehicle());
		const CVehicle* pVehicle = static_cast<const CVehicle *>(pEntity);

		//Ensure the vehicle is a heli.
		if(!pVehicle->InheritsFromHeli())
		{
			continue;
		}

		//Grab the heli.
		const CHeli* pHeli = static_cast<const CHeli *>(pVehicle);
		
		//Grab the other position.
		Vec3V vOtherPosition = pVehicle->GetTransform().GetPosition();

		//Ensure the helis are within a certain range of each other.
		ScalarV scDistSq = DistSquared(vPosition, vOtherPosition);
		ScalarV scMaxDistSq = ScalarVFromF32(square(fAvoidanceMaxDist));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}
		
		//Grab the other height.
		ScalarV scOtherHeight = vOtherPosition.GetZ();
		
		//Ensure the heli is above the other heli.
		ScalarV scHeightDifference = Subtract(scHeight, scOtherHeight);
		if(IsLessThanAll(scHeightDifference, ScalarV(V_ZERO)))
		{
			continue;
		}
		
		//Check if the closest heli is valid.
		if(closestHeli.m_pHeli)
		{
			//Ensure the other heli is above the closest heli.
			if(IsLessThanAll(scOtherHeight, closestHeli.m_scHeight))
			{
				continue;
			}
		}
		
		//Assign the closest heli.
		closestHeli.m_pHeli = pHeli;
		closestHeli.m_scHeight = scOtherHeight;
	}
	
	//Check if the closest heli is valid.
	if(closestHeli.m_pHeli)
	{
		//Set the avoidance height to be above the closest heli.
		m_fHeliAvoidanceHeight = closestHeli.m_scHeight.Getf() + fAvoidanceOffset;
	}
	else
	{
		//Clear the avoidance height.
		m_fHeliAvoidanceHeight = 0.0f;
	}
}

void CHeliIntelligence::ProcessLanded()
{
	if ( m_fTimeLandingAreaBlocked >= 0.0f )
	{
		m_fTimeLandingAreaBlocked -= fwTimer::GetTimeStep();
	}

	if(IsLanded())
	{
		m_fTimeSpentLanded += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimeSpentLanded = 0.0f;
	}
}
