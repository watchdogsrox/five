#ifndef VEHICLE_INTELLIGENCE_H
#define VEHICLE_INTELLIGENCE_H

#include "ai/ExpensiveProcess.h"
#include "Debug/Debug.h"
#include "Peds/PedEventScanner.h"
#include "Event/EventGroup.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/TaskManager.h"
#include "vehicleAi/pathfind.h"
#include "vehicleAi/VehMission.h"
#include "vehicleAi/VehicleNodeList.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "Templates/CircularArray.h"

struct tGameplayCameraSettings;

struct CCollisionRecord;
class CJunction;
class CVehicle;
class CVehMission;
class CTaskVehicleMissionBase;
class CHeli;

#define SWIRVEDURATION (2500)


enum
{
	JUNCTION_COMMAND_GO = 0,
	JUNCTION_COMMAND_APPROACHING,
	JUNCTION_COMMAND_WAIT_FOR_LIGHTS,
	JUNCTION_COMMAND_WAIT_FOR_TRAFFIC,
	JUNCTION_COMMAND_NOT_ON_JUNCTION,
	JUNCTION_COMMAND_GIVE_WAY,
	JUNCTION_COMMAND_LAST
};
enum
{
	JUNCTION_FILTER_NONE = 0,
	JUNCTION_FILTER_LEFT,
	JUNCTION_FILTER_MIDDLE,
	JUNCTION_FILTER_RIGHT,
	JUNCTION_FILTER_LAST
};
#if 0
// This structure is used to store a history of the coordinates of the car.
// If the car gets stuck these points can be used to reverse back out to the main road.
#define	PPSPACING		(2.0f)						// The points are this far apart
#define	PPSPACINGSQR	(PPSPACING * PPSPACING)
class CVehPointPath
{
public:
	static const int	MAXNUMPOINTS = 16;		// This many points can be remembered.

public:
	void		Clear() { m_numPoints = 0; }
#if __BANK
	void		RenderDebug() const;
#endif // __DEV
	bool		FindClosestIndex(const Vector3 &Crs, int &index, float &distOnLine);
	void		Shift(int number);

	s32		m_numPoints;
	Vector2		m_points[MAXNUMPOINTS];
};

#endif //0

class CVehPIDController
{
public:
	CVehPIDController(float fKp = 0.0f, float fKi = 0.0f, float fKd = 0.0f)
		: m_fKp(fKp), m_fKi(fKi), m_fKd(fKd)
		, m_fIntegral(0.0f)
		, m_fPreviousInput(0.0f)
		, m_fPreviousDerivative(0.0f)
		, m_fPreviousOutput(0.0f)
	{
	}

	void Setup(float fKp, float fKi, float fKd)
	{
		m_fKp = fKp;
		m_fKi = fKi;
		m_fKd = fKd;
	}

	float Update(float fInput);

	float GetIntegral() const { return m_fIntegral; }
	float GetPreviousInput() const { return m_fPreviousInput; }
	float GetPreviousDerivative() const { return m_fPreviousDerivative; }
	float GetPreviousOutput() const { return m_fPreviousOutput; }

private:
	float m_fKp;
	float m_fKi;
	float m_fKd;
	float m_fIntegral;
	float m_fPreviousInput;
	float m_fPreviousDerivative;
	float m_fPreviousOutput;
};

class CVehicleIntelligence
{
public:
#if __BANK
	static	const char  *MissionStrings[MISSION_LAST];
	static	const char  *TempActStrings[TEMPACT_LAST];
	static	const char	*GetMissionName(s32 Mission);
	static	const char	*GetTempActName(s32 TempAct);
	static	const char  *JunctionCommandStrings[JUNCTION_COMMAND_LAST];
	static	const char  *JunctionFilterStrings[JUNCTION_FILTER_LAST];
	static	const char	*JunctionCommandStringsShort[JUNCTION_COMMAND_LAST];
	static	const char	*JunctionFilterStringsShort[JUNCTION_FILTER_LAST];
	static	const char *GetJunctionCommandName(s32 JunctionCommand);
	static	const char *GetJunctionFilterName(s32 JunctionFilter);
	static	const char *GetJunctionCommandShortName(s32 JunctionCommand);
	static	const char *GetJunctionFilterShortName(s32 JunctionFilter);
	static	const char	*GetDrivingFlagName(s32 flag);
#endif // __BANK

	CVehicleIntelligence(CVehicle *pVehicle);
	virtual ~CVehicleIntelligence();

	FW_REGISTER_CLASS_POOL(CVehicleIntelligence);

public:

	u32 GetTimeWeStartedCollidingWithPlayerVehicle() const { return m_uTimeWeStartedCollidingWithPlayerVehicle; }

public:

	virtual void			Process(bool fullUpdate, float fFullUpdateTimeStep);
	void					Process_OnDeferredTaskCompletion();

	void					PreUpdateVehicle();
	void					ResetVariables(float fTimeStep);
	void					NetworkResetMillisecondsNotMovingForRemoval(); //used on networked vehicles to reset this value when migrating after stuck on screen

	// -----------------------------------------------------------------------------
	// TASK MANAGER INTERFACE

	CTaskManager*			GetTaskManager()								{ return(&m_TaskManager); }
	const CTaskManager*		GetTaskManager() const							{ return(&m_TaskManager); }
	CTaskVehicleMissionBase	*GetActiveTask() const;
	CTaskVehicleMissionBase	*GetActiveTaskSimplest() const;

	void AddTask(const s32 iTreeIndex, aiTask* pTask, const s32 iPriority, const bool bForceNewTask = false
		ASSERT_ONLY(, bool /*bSkipTrailerAssert*/ = false))
	{
// 		Assertf(!m_pVehicle 
// 			|| bSkipTrailerAssert
// 			|| iTreeIndex != VEHICLE_TASK_TREE_PRIMARY
// 			|| !pTask
// 			|| pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_NO_DRIVER
// 			|| (!m_pVehicle->InheritsFromTrailer() && !m_pVehicle->m_nVehicleFlags.bHasParentVehicle)
// 			, "Trying to give a primary task to a trailer! Vehicle: %s, Task: %s"
// 			, m_pVehicle->pHandling->m_handlingName.TryGetCStr(), pTask->GetTaskName());
		GetTaskManager()->SetTask(iTreeIndex, pTask, iPriority, bForceNewTask);
	}

	// used to query the active task on local or clone vehicles
	void					GetActiveTaskInfo(s32& taskType, sVehicleMissionParams& params) const;

	void					SetRecordingNumber(s8 recordingNumber);
	int						GetRecordingNumber()							{return m_RecordingNumber;}

	// -----------------------------------------------------------------------------
	// CAMERA SYSTEM INTERFACE

	void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// -----------------------------------------------------------------------------
	// NODE STUFF

	//TODO: get rid of this and only allow access through the route search helper
	CVehicleNodeList* GetNodeList() const;

	const CVehicleFollowRouteHelper* GetFollowRouteHelper() const;
	const CPathNodeRouteSearchHelper* GetRouteSearchHelper() const;
	CVehicleFollowRouteHelper* GetFollowRouteHelperMutable()
	{
		return const_cast<CVehicleFollowRouteHelper*>(GetFollowRouteHelper());
	}

	static float			FindSpeedMultiplierWithSpeedFromNodes		(s32 SpeedFromNodes)
	{
		static const int s_iNumSpeeds = 4;
		static dev_float s_fMultiplierAtSpeed[s_iNumSpeeds] = {0.5f, 1.0f, 1.6f, 2.2f};

#if __BANK
		TUNE_GROUP_BOOL(CAR_AI, OverrideNodeSpeed, false);
		TUNE_GROUP_INT(CAR_AI, OverriddenSpeed, 1, 0, 3, 1);

		if (OverrideNodeSpeed)
		{
			return s_fMultiplierAtSpeed[OverriddenSpeed];
		}
#endif //__BANK

		if (SpeedFromNodes >= 0 && SpeedFromNodes < s_iNumSpeeds)
		{
			return s_fMultiplierAtSpeed[SpeedFromNodes];	
		}
		else if (SpeedFromNodes < 0)
		{
			return s_fMultiplierAtSpeed[0];
		}
		else
		{
			return s_fMultiplierAtSpeed[1];
		}
	}
	static void				FindNodeOffsetForCarFromRandomSeed		(u16 randomSeed, Vector3 &Result);
	static float			CalculateSpeedMultiplierBend				(const CVehicle* const pVeh, float carOrientation, float StraightLineSpeed);
	static float			FindSpeedMultiplier							(const CVehicle* pVehicle, float Angle, float MinSpeedAngle, float MaxSpeedAngle, float MinSpeed, float MaxSpeed);
	static void				FindTargetCoorsWithNode						(const CVehicle* const pVeh, CNodeAddress PreviousNode, CNodeAddress Node, CNodeAddress NextNode, s32 regionLinkIndexPreviousNodeToNode, s32 regionLinkIndexNodeToNextNode, s32 LaneIndexOld, s32 LaneIndexNew, Vector3 &Result);
	static void				FindNodeOffsetForCar						(const CVehicle* const pVeh, Vector3 &Result);
	static float			MultiplyCruiseSpeedWithMultiplierAndClip	(float CruiseSpeed, float SpeedMultiplierFromNodes, const bool bShouldCapSpeedToSpeedZones, const bool bIsMission, const Vector3& vPosition);
	static CTaskVehicleMissionBase*		CreateCruiseTask(CVehicle& in_Vehicle, sVehicleMissionParams& params, const bool bSpeedComesFromVehPopulation = false);
	static CTaskVehicleMissionBase*		CreateAutomobileGotoTask(sVehicleMissionParams& params, const float fStraightLineDist);
	static int				GetAutomobileGotoTaskType();

	void					UpdateJustBeenGivenNewCommand();
	void					UpdateCarHasReasonToBeStopped();

	bool					OkToDropOffPassengers();
	bool					IsOnHighway() const;
	bool					IsOnSingleTrackRoad() const;
	bool					IsOnSmallRoad() const;
	
	// PURPOSE: Decides if the current vehicle route is going to be intercepted by the predicted path (projected velocity) for an entity.
	bool					CheckRouteInterception(Vec3V_In vEntityPos, Vec3V_In vEntityVelocity, float fMaxSideDistance, float fMaxForwardDistance, float fMinTime, float fMaxTime) const;
	bool					CheckEntityInterceptionXY(Vec3V_In vVehiclePos, Vec2V_In vVehicleForwardXY, Vec2V_In vVehicleVelocityXY, Vec3V_In vEntityPos, Vec2V_In vEntityVelocityXY, ScalarV_In scMaxSideDistance, ScalarV_In scMaxForwardDistance, ScalarV_In scMaxTime,
													  ScalarV_Ptr pscInterceptionT, Vec3V_Ptr pvInterceptionVehPos, Vec3V_Ptr pvInterceptionEntityPos, ScalarV_Ptr pscInterceptionDistanceLocalX, ScalarV_Ptr pscInterceptionDistanceLocalY) const;

	bool					DoesRoutePassByPosition(Vec3V_ConstRef vPos, const float fThreshold, const float fCareThreshold = 0.0f) const;
	
	bool					AreFutureRoutePointsCloseToPoint(Vec3V_In vPosition, ScalarV_In scRadius) const;

	float					FindMaxSteerAngle			() const;
	float					GetTurnRadiusAtCurrentSpeed() const;

	static aiTask			*GetGotoTaskForVehicle(CVehicle *pVehicle, CPhysical* pTargetEntity, Vector3 *vTarget, s32 iDrivingFlags, float fTargetReached, float fStraightLineDistance, float fCruiseSpeed);
	static aiTask			*GetTaskFromMissionIdentifier(CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, s32 iDrivingFlags, float fTargetReached, float fStraightLineDistance, float fCruiseSpeed, bool bAllowedToGoAgainstTraffic, bool bDoEverythingInReverse = false);
	static aiTask			*CreateVehicleTaskForNetwork(u32 taskType);
	static aiTask			*GetSubTaskFromMissionIdentifier( CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, float fTargetReached, float fCruiseSpeed, float fSubOrientation, s32 iMinHeightAboveTerrain, float fSlowDownDistance, int iSubFlags = 0);
	static aiTask			*GetHeliTaskFromMissionIdentifier(CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, float fTargetReached, float fCruiseSpeed, float fHeliOrientation, s32 iFlightHeight, s32 iMinHeightAboveTerrain, float fSlowDownDistance, int iHeliFlags = 0);
	static aiTask			*GetPlaneTaskFromMissionIdentifier(CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, float fTargetReached, float fCruiseSpeed, float fOrientation, s32 iFlightHeight, s32 iMinHeightAboveTerrain, bool bPrecise);
	static aiTask			*GetBoatTaskFromMissionIdentifier(CVehicle *pVehicle, int iMission, CPhysical* pTargetEntity, Vector3 *vTarget, s32 iDrivingFlags, float fTargetReached, float fCruiseSpeed, int iBoatFlags = 0);
	static VehMissionType	GetMissionIdentifierFromTaskType(int iTaskType, sVehicleMissionParams& params);
	static aiTask			*GetTempActionTaskFromTempActionType(int iTempActionType, u32 nDuration, fwFlags8 uConfigFlags = 0);

	// Update the cars parked and stationary status
	void					UpdateIsThisADriveableCar();
	void					UpdateIsThisAParkedCar();
	void					UpdateIsThisAStationaryCar();
	void					UpdateTimeNotMoving(float fTimeStep);

	bool					GetSlowingDownForPed();
	bool					GetSlowingDownForCar();
	bool					GetShouldObeyTrafficLights();

	// Sets whether the AI controls should be humanised and clamped, this is reset each frame
	inline bool GetHumaniseControls() const { return m_bHumaniseControls; }
	inline void SetHumaniseControls( bool bHumanise ) { m_bHumaniseControls = bHumanise; }

	// Scanners
	const CPedScanner&		 GetPedScanner() const { return m_pedScanner;    }
	const CVehicleScanner&	 GetVehicleScanner() const { return m_vehicleScanner;  }
	const CObjectScanner&	 GetObjectScanner() const { return m_objectScanner; }
	const CExpensiveProcess& GetSteeringDistributer() const { return m_SteeringDistributer; }
	const CExpensiveProcess& GetBrakingDistributer() const { return m_BrakingDistributer; }
	const CExpensiveProcess& GetLaneChangeDistributer() const { return m_LaneChangeDistributer;	}
	const CExpensiveProcess& GetDummyAvoidanceDistributer() const { return m_DummyAvoidanceDistributer; }
	const CExpensiveProcess& GetTailgateDistributer() const { return m_TailgateDistributer; }
	const CExpensiveProcess& GetNavmeshLOSDistributer() const { return m_NavmeshLOSDistributer; }
	const CExpensiveProcess& GetUpdateJunctionsDistributer() const { return m_UpdateJunctionsDistributer; }
	const CExpensiveProcess& GetSirenReactionDistributer() const { return m_SirenReactionDistributer;}
	CPedScanner&			 GetPedScanner() { return m_pedScanner;    }
	CVehicleScanner&		 GetVehicleScanner() { return m_vehicleScanner;  }
	CObjectScanner&			 GetObjectScanner() { return m_objectScanner; }
	CExpensiveProcess&		 GetSteeringDistributer() { return m_SteeringDistributer; }
	CExpensiveProcess&		 GetBrakingDistributer() { return m_BrakingDistributer; }
	CExpensiveProcess&		 GetLaneChangeDistributer() { return m_LaneChangeDistributer;	}
	CExpensiveProcess&		 GetDummyAvoidanceDistributer() { return m_DummyAvoidanceDistributer; }
	CExpensiveProcess&		 GetTailgateDistributer() { return m_TailgateDistributer; }
	CExpensiveProcess&		 GetNavmeshLOSDistributer() { return m_NavmeshLOSDistributer; }
	CExpensiveProcess&		 GetUpdateJunctionsDistributer() { return m_UpdateJunctionsDistributer; }
	CExpensiveProcess&		 GetSirenReactionDistributer() { return m_SirenReactionDistributer;}
	
	void ClearScanners();
	
	void SetCarWeAreBehind(CVehicle* pCarWeAreBehind);
	CVehicle* GetCarWeAreBehind() const { return m_pCarWeAreBehind; }
	float GetDistanceBehindCarSq() const { return m_fDistanceBehindCarSq; }

	CVehicle* GetCarWeCouldBeSlipstreamingBehind() const { return m_pCarWeCouldBeSlipStreaming; }
	float GetDistanceBehindCarWeCouldBeSlipstreamingBehindSq() const { return m_fDistanceBehindCarWeCouldBeSlipStreamingSq; }
	
	float GetSmoothedSpeedSq() const { return m_fSmoothedSpeedSq; }

	const CPhysical* GetObstructingEntity() const { return m_pObstructingEntity;}
	const CPhysical* GetFleeEntity() const {return m_pFleeEntity;}
	const CPhysical* GetEntitySteeringAround() const {return m_pSteeringAroundEntity;}

	void CacheNodeList(CTaskVehicleMissionBase* pOptionalForceTask = NULL);
	void InvalidateCachedNodeList();
#if __ASSERT
	void VerifyCachedNodeList();
#endif // __ASSERT
	void CacheFollowRoute(CTaskVehicleMissionBase* pOptionalForceTask = NULL);
	void InvalidateCachedFollowRoute();
	
	struct AvoidanceCache
	{
		AvoidanceCache()
		: m_pTarget(NULL)
		, m_fDesiredOffset(0.0f)
		, m_fAdditionalBuffer(0.0f)
		{}
		
		RegdConstPhy	m_pTarget;
		float			m_fDesiredOffset;
		float			m_fAdditionalBuffer;
	};
	
			AvoidanceCache& GetAvoidanceCache()			{ return m_AvoidanceCache; }
	const	AvoidanceCache& GetAvoidanceCache() const	{ return m_AvoidanceCache; }
	void	ClearAvoidanceCache()
	{
		m_AvoidanceCache.m_pTarget = NULL;
		m_AvoidanceCache.m_fDesiredOffset = 0.0f;
		m_AvoidanceCache.m_fAdditionalBuffer = 0.0f;
	}
	
	struct InvMassScaleOverride
	{
		InvMassScaleOverride()
		: m_pVehicle(NULL)
		, m_fValue(1.0f)
		{}
		
		RegdConstVeh	m_pVehicle;
		float			m_fValue;
	};
	
			InvMassScaleOverride& GetInvMassScaleOverride()			{ return m_InvMassScaleOverride; }
	const	InvMassScaleOverride& GetInvMassScaleOverride() const	{ return m_InvMassScaleOverride; }
	
	void ClearInvMassScaleOverride()
	{
		m_InvMassScaleOverride.m_pVehicle	= NULL;
		m_InvMassScaleOverride.m_fValue		= 1.0f;
	}
	
	void SetInvMassScaleOverride(const CVehicle* pVehicle, float fValue)
	{
		m_InvMassScaleOverride.m_pVehicle	= pVehicle;
		m_InvMassScaleOverride.m_fValue		= fValue;
	}

	void ProcessCollision(const CCollisionRecord* pColRecord);
		
#if __DEV
	void ResetCachedNodeListHits() {m_iCacheNodeListHitsThisFrame = 0;}
	void ResetCachedFollowRouteHits() {m_iCacheFollowRouteHitsThisFrame = 0;}
#endif //__DEV

private:
	CVehicleScanner			m_vehicleScanner;
	CPedScanner				m_pedScanner;
	CObjectScanner			m_objectScanner;
	CExpensiveProcess		m_SteeringDistributer;
	CExpensiveProcess		m_BrakingDistributer;
	CExpensiveProcess		m_LaneChangeDistributer;
	CExpensiveProcess		m_DummyAvoidanceDistributer;
	CExpensiveProcess		m_TailgateDistributer;
	CExpensiveProcess		m_NavmeshLOSDistributer;
	CExpensiveProcess		m_UpdateJunctionsDistributer;
	CExpensiveProcess		m_SirenReactionDistributer;

	CEventGroupVehicle		m_eventGroup;
	sVehicleMissionParams	m_PretendOccupantEventParams;

	RegdVeh					m_pCarWeAreBehind;

public:
	CVehicleFollowRouteHelper	m_BackupFollowRoute;

	static const int	ms_iNumScenarioRoutePointsToSave = 4;

	const CRoutePoint*		GetScenarioRoutePointHistory() const {return m_pScenarioRoutePointHistory;}
	void				DeleteScenarioPointHistory()
	{
		delete [] m_pScenarioRoutePointHistory;
		m_pScenarioRoutePointHistory = NULL;
		m_iNumScenarioRouteHistoryPoints = 0;
	}
	void EnsureHasScenarioPointHistory()
	{
		if (!m_pScenarioRoutePointHistory)
		{
			m_pScenarioRoutePointHistory = rage_new CRoutePoint[ms_iNumScenarioRoutePointsToSave];
			m_iNumScenarioRouteHistoryPoints = 0;
		}
	}

	CBoatAvoidanceHelper* GetBoatAvoidanceHelper() { return m_BoatAvoidanceHelper; }
	CBoatAvoidanceHelper*		m_BoatAvoidanceHelper;

	CRoutePoint*		m_pScenarioRoutePointHistory;
	RegdConstPhy		m_pObstructingEntity;					// The entity that stops us from moving.
	RegdConstPhy		m_pFleeEntity;
	RegdConstPhy		m_pSteeringAroundEntity;
	RegdVeh				m_pCarThatIsBlockingUs;			// Storage for the car that we are waiting on.
	RegdVeh				m_pCarWeCouldBeSlipStreaming;

	float				m_fDistanceBehindCarSq;
	float				m_fDistanceBehindCarWeCouldBeSlipStreamingSq;
	float				m_fSmoothedSpeedSq;
	float				m_fDesiredSpeedThisFrame;

	s32					m_NumCarsBehindUs;
	s32					m_NumCarsBehindContributedToCarWeAreBehind;
	s32					m_iNumScenarioRouteHistoryPoints;
	u32					LastTimeNotStuck;					// When was the last time we were not stuck
	u32					LastTimeHonkedAt;
	u32					m_nLastTimeToldOthersAboutRevEngine;
	
	void AddScenarioHistoryPoint(const CRoutePoint& pt);

	inline CJunction* GetJunction() const;
	inline CJunction* GetPreviousJunction() const { return m_PreviousJunction; }

	void SetJunctionEntrance(CJunctionEntrance* junctionEntrance) { m_JunctionEntrance = junctionEntrance; }
	CJunctionEntrance* GetJunctionEntrance() const { return m_JunctionEntrance; }
	CJunctionEntrance* GetPreviousJunctionEntrance() const { return m_PreviousJunctionEntrance; }

	void SetJunctionExit(CJunctionEntrance* junctionExit) { m_JunctionExit = junctionExit; }
	CJunctionEntrance* GetJunctionExit() const { return m_JunctionExit; }
	CJunctionEntrance* GetPreviousJunctionExit() const { return m_PreviousJunctionExit; }

	void SetJunctionNode(const CNodeAddress& junctionNode, CJunction* junctionPtr);
	const CNodeAddress& GetJunctionNode() const { return m_JunctionNode; }
	const CNodeAddress& GetPreviousJunctionNode() const { return m_PreviousJunctionNode; }

	void SetJunctionEntranceNode(const CNodeAddress& junctionEntranceNode) { m_JunctionEntranceNode = junctionEntranceNode; }
	const CNodeAddress& GetJunctionEntranceNode() const { return m_JunctionEntranceNode; }
	const CNodeAddress& GetPreviousJunctionEntranceNode() const { return m_PreviousJunctionEntranceNode; }

	void SetJunctionExitNode(const CNodeAddress& junctionExitNode) { m_JunctionExitNode = junctionExitNode; }
	const CNodeAddress& GetJunctionExitNode() const { return m_JunctionExitNode; }
	const CNodeAddress& GetPreviousJunctionExitNode() const { return m_PreviousJunctionExitNode; }

	void SetJunctionEntrancePosition(const Vector3& vJunctionEntrance) { m_vJunctionEntrance = vJunctionEntrance; }
	const Vector3& GetJunctionEntrancePosition() const { return m_vJunctionEntrance; }
	const Vector3& GetPreviousJunctionEntrancePosition() const { return m_vPreviousJunctionEntrance; }

	void SetJunctionExitPosition(const Vector3& vJunctionExit) { m_vJunctionExit = vJunctionExit; }
	const Vector3& GetJunctionExitPosition() const { return m_vJunctionExit; }
	const Vector3& GetPreviousJunctionExitPosition() const { return m_vPreviousJunctionExit; }

	void SetJunctionEntranceLane(const s32 junctionEntranceLane) { m_JunctionEntranceLane = junctionEntranceLane; }
	s32 GetJunctionEntranceLane() const { return m_JunctionEntranceLane; }
	s32 GetPreviousJunctionEntranceLane() const { return m_PreviousJunctionEntranceLane; }

	void SetJunctionExitLane(const s32 junctionExitLane) { m_JunctionExitLane = junctionExitLane; }
	s32 GetJunctionExitLane() const { return m_JunctionExitLane; }
	s32 GetPreviousJunctionExitLane() const { return m_PreviousJunctionExitLane; }

	u32 GetJunctionTurnDirection()
	{
		if (m_iJunctionTurnDirection == BIT_NOT_SET)
		{
			CacheJunctionTurnDirection();
		}

		return m_iJunctionTurnDirection;
	}

	void CacheJunctionTurnDirection()
	{
		bool bSharpTurn;
		float fLeftness;
		float fDotProduct;
		s32 iNumExitLanes;

		m_iJunctionTurnDirection = CPathNodeRouteSearchHelper::FindUpcomingJunctionTurnDirection(GetJunctionNode(), GetNodeList(), &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForUsingJunctionNode, &fDotProduct, iNumExitLanes);
	}

	void SetJunctionArrivalTime(const u32 junctionArrivalTime) { m_iJunctionArrivalTime = junctionArrivalTime; }
	void RecordJunctionArrivalTime() { m_iJunctionArrivalTime = CNetwork::GetSyncedTimeInMilliseconds(); }
	u32 GetJunctionArrivalTime() const { return m_iJunctionArrivalTime; }
	void SetJunctionCommand(s8 junctionCommand) { m_JunctionCommand = junctionCommand; }
	s8 GetJunctionCommand() const { return m_JunctionCommand; }
	s8 GetJunctionFilter() const {return m_iJunctionFilter;}
	void SetJunctionFilter(s8 junctionFilter) {m_iJunctionFilter = junctionFilter;}
	bool GetHasFixedUpPathForCurrentJunction() const {return m_bHasFixedUpPathForCurrentJunction != 0;}
	void SetHasFixedUpPathForCurrentJunction(bool b) {m_bHasFixedUpPathForCurrentJunction = b;}

	void CacheJunctionInfo();
	void ResetCachedJunctionInfo();

	void SetTrafficLightCommand(ETrafficLightCommand trafficLightCommand) { m_TrafficLightCommand = trafficLightCommand; }
	ETrafficLightCommand GetTrafficLightCommand() const { return m_TrafficLightCommand; }

	float GetPretendOccupantAggressiveness() const {return m_fPretendOccupantAggressiveness;}
	float GetPretendOccupantAbility() const {return m_fPretendOccupantAbility;}
	void SetPretendOccupantAggressivness(float fAggressivness) {m_fPretendOccupantAggressiveness = fAggressivness;}
	void SetPretendOccupantAbility(float fAbility) {m_fPretendOccupantAbility = fAbility;}

	float GetCustomPathNodeStreamingRadius() const {return m_fCustomPathNodeStreamingRadius;}
	void SetCustomPathNodeStreamingRadius(float fRadius) {m_fCustomPathNodeStreamingRadius = fRadius;} 
	
	// Event creation.
	
	// Create shocking events from properties of the vehicle for other entities in the game to react against.
	void GenerateEvents();
	bool IsPedAffectedByHonkOrRev(const CPed& rPed, float& fDistSq, Vec3V_In vForwardTestDir, Vec3V_In vRightTestDir) const;
	bool IsVehicleAffectedByHonkOrRev(const CVehicle& rVehicle, float& fDistSq, Vec3V_In vForwardTestDir, Vec3V_In vRightTestDir) const;
	bool IsEntityInAffectedZoneForHonkOrRev(const CEntity& rEntity, float& fDistSq, Vec3V_In vForwardTestDirIn, Vec3V_In vRightTestDirIn) const;
	void TellOthersAboutHonk() const;
	void TellOthersAboutRevEngine() const;

	bool ShouldHandleEvents() const
	{
		return m_pVehicle->m_nVehicleFlags.bUsingPretendOccupants && m_pVehicle->m_nVehicleFlags.bIsThisADriveableCar;
	}
	CEvent* AddEvent(const CEvent& rEvent, const bool bForcePersistence=false, const bool bRemoveOtherEventsToMakeSpace = true);
	const CEventGroupVehicle& GetEventGroup() const { return m_eventGroup; }

	void ResetPretendOccupantEventData()
	{
		m_PretendOccupantEventPriority = VehicleEventPriority::VEHICLE_EVENT_PRIORITY_NONE;
		m_PretendOccupantEventParams.Clear();
		if (m_CurrentEvent)
		{
			delete m_CurrentEvent;
			m_CurrentEvent = NULL;
		}	
	}

	CEvent* GetCurrentEvent() const {return m_CurrentEvent;}
	void	SetCurrentEvent(CEvent* pEvent) 
	{
		Assert(m_CurrentEvent == NULL);
		m_CurrentEvent = pEvent;
	}
	void	ClearCurrentEvent() 
	{
		delete m_CurrentEvent;
		m_CurrentEvent = NULL;
	}

	VehicleEventPriority::VehicleEventPriority GetPretendOccupantEventPriority() const
	{
		return m_PretendOccupantEventPriority;
	}

	const sVehicleMissionParams& GetPretendOccupantEventParams() const
	{
		return m_PretendOccupantEventParams;
	}

	u32 GetNumPedsThatNeedTaskFromPretendOccupant() const
	{
		return m_nNumPedsThatNeedTaskFromPretendOccupant;
	}

	//called when we transition from real->pretend occupants,
	//to preserve any event that we may be currently reacting to
	void SetPretendOccupantEventDataBasedOnCurrentTask();

	void SetPretendOccupantEventData(VehicleEventPriority::VehicleEventPriority eventPriority, const sVehicleMissionParams& params)
	{
		m_PretendOccupantEventPriority = eventPriority;
		m_PretendOccupantEventParams = params;
	}

	void SetNumPedsThatNeedTaskFromPretendOccupant(u32 nNumPeds)
	{
		m_nNumPedsThatNeedTaskFromPretendOccupant = nNumPeds;
	}

	void DecrementNumPedsThatNeedTaskFromPretendOccupant()
	{
		Assert(m_nNumPedsThatNeedTaskFromPretendOccupant > 0);
		--m_nNumPedsThatNeedTaskFromPretendOccupant;
	}

	void FlushPretendOccupantEventGroup()
	{
		m_eventGroup.FlushAll();
	}

	void BlockWeaponSelection( bool in_BlockWeaponSelection ) { m_bBlockWeaponSelection = in_BlockWeaponSelection; }
	bool IsWeaponSelectionBlocked() const { return m_bBlockWeaponSelection; }

	u32 GetLastTimePedEntering() const { return m_uLastTimePedEntering; }
	void PedIsEntering()
	{
		m_uLastTimePedEntering = fwTimer::GetTimeInMilliseconds();
	}

	u32 GetLastTimePedInCover() const { return m_uLastTimePedInCover; }
	void PedIsInCover()
	{
		m_uLastTimePedInCover = fwTimer::GetTimeInMilliseconds();
	}

	u32 GetLastTimeRamming() const { return m_uLastTimeRamming; }
	void Ramming()
	{
		m_uLastTimeRamming = fwTimer::GetTimeInMilliseconds();
	}

	u32 GetLastTimePlayerAimedAtOrAround() const { return m_uLastTimePlayerAimedAtOrAround; }
	void PlayerAimedAtOrAround()
	{
		m_uLastTimePlayerAimedAtOrAround = fwTimer::GetTimeInMilliseconds();
	}

	s32 GetImpatienceTimerOverride() const {return m_iImpatienceTimerOverride;}
	void SetImpatienceTimerOverride(s32 iImpatienceTimer)
	{
		m_iImpatienceTimerOverride = iImpatienceTimer;
	}

	bool HasBeenSlowlyPushingPlayer() const;

	bool HasBeenInWater() const { return (m_uLastTimeInWater != 0); }
	u32 GetTimeSinceLastInWater() const;

	void SetUsingScriptAutopilot(bool bValue) { m_bUsingScriptAutopilot = bValue; }
	bool GetUsingScriptAutopilot() const { return m_bUsingScriptAutopilot; }

	RegdEvent	m_CurrentEvent;
	VehicleEventPriority::VehicleEventPriority	m_PretendOccupantEventPriority;

	s32					m_iImpatienceTimerOverride;
	u32					m_nHandlingOverrideHash;
	u32					m_nNumPedsThatNeedTaskFromPretendOccupant;
	u32					MillisecondsNotMoving;				// Used to trigger 3 point turns.
	u32					MillisecondsNotMovingForRemoval;	// Used to remove vehicle if stuck for a long time.
	u32					m_nTimeLastTriedToShakePedFromRoof;
	u32					m_lastTimeTriedToPushPlayerPed;
	u32					m_uTimeLastTouchedRealMissionVehicleWhileDummy;
	u32					m_uLastTimeInWater;
	float				SpeedMultiplier;	// The resulting speed multiplier
	//float				m_LaneShift;				// Vehicles can have a consistent offset from the centre line of the road. (Used to have 2 convoys of bikes next to each other in e1)
	s8					m_iNumVehsWeAreBlockingThisFrame;

	s8					SpeedFromNodes;		// What is the speed here as set by the level designers (0=normal, 1=fast, 2=motorway)

	// The following fields are solely for ai cars trying to follow a recorded path.
	s8					m_RecordingNumber;

public:
	bool					bCarHasToReverseFirst : 1;	// If this is true the car has been parked perpendicular and should be reversed first when stolen.
	bool					m_bHumaniseControls	  : 1;	// Set to TRUE if the car is cruising and the AI controls should be humanised.
	bool					m_bHasCachedNodeList  :	1;	// This will be true if this vehicle has a valid cached node list ptr
	bool					m_bHasCachedFollowRoute : 1;
	bool					m_bRemoveAggressivelyForCarjackingMission : 1;		//hacky tags for mission specific stuff that can no longer go in CVehicleFlags
	bool					m_bSwerveAroundPlayerVehicleForRiotVanMission : 1;
	bool					m_bStationaryInTank : 1;	//hacky tag to get peds in tanks to not drive, just fire at target
	bool					m_bSetBoatIgnoreLandProbes : 1;	//hacky tag to get boats to not check land probes when in combat boat task
	bool					m_bSteerForBuildings : 1;				//hacky tag to prevent vehicles steering for buildings
	bool					m_bDontSwerveForUs : 1;			//hack tag to stop ambient vehicles swerving for this vehicle for head on collisions
	bool					m_bIgnorePlanesSmallPitchChange : 1;	//hack tag to stop planes from pitching up and down in the PID controller for small Z changes
	static bool				ms_bReallyPreventSwitchedOffFleeing;	//hacky tag to prevent fleeing vehicles using switched off (and dead end) nodes globally
	
#if __BANK
	void					RenderDebug();
	void					PrintTasks();

	u32					lastTimeJoinedWithRoadSystem;			// A debug thing so we can print out how long ago this car was joined with road system
	u32					lastTimeActuallyJoinedWithRoadSystem;	// This one registers the nodes actually having been replaced (this will not happen if the nodes are up-to-date)
#if __DEV
	s32					m_iCacheNodeListHitsThisFrame;
	s32					m_iCacheFollowRouteHitsThisFrame;
#endif

	static float			ms_fDisplayVehicleAIRange;
	static bool				m_bDisplayCarAiDebugInfo;
	static int				m_eFilterVehicleDisplayByType;
	static bool				ms_debugDisplayFocusVehOnly;
	static bool				m_bDisplayCarAddresses;
	static bool				m_bDisplayDirtyCarPilons;
	static bool				m_bDisplayCarAiTaskHistory;
	static bool				m_bDisplayVehicleFlagsInfo;
	static bool				m_bDisplayCarAiTaskInfo;
	static bool				m_bFindCoorsAndSpeed;
	static bool				m_bDisplayCarFinalDestinations;
	static bool				m_bDisplayCarAiTaskDetails;
	static bool				m_bDisplayVehicleRecordingInfo;
	static bool				m_bDisplayDebugInfoOkToDropOffPassengers;
	static bool				m_bDisplayDebugJoinWithRoadSystem;
	static bool				m_bDisplayAvoidanceCache;
	static bool				m_bDisplayRouteInterceptionTests;
	static bool				m_bHideFailedRouteInterceptionTests;
	static bool				m_bDisplayDebugFutureNodes;
	static bool				m_bDisplayDebugFutureFollowRoute;
	static bool				m_bDisplayDrivableExtremes;
	static bool				m_bDisplayControlDebugInfo;
	static bool				m_bDisplayAILowLevelControlInfo;
	static bool				m_bDisplayControlTransmissionInfo;
	static bool				m_bDisplayStuckDebugInfo;
	static bool				m_bDisplaySlowDownForBendDebugInfo;
	static bool				m_bDisplayDebugSlowForTraffic;
	static bool				m_bDisplayDebugCarBehind;
	static bool				m_bDisplayDebugCarBehindSearch;
	static bool				m_bDisplayDebugTailgate;
	static bool				m_bAICarHandlingDetails;
	static bool				m_bAICarHandlingCurve;
	static bool				m_bOnlyForPlayerVehicle;
	static bool				m_bAllCarsStop;
	static bool				m_bDisplayInterestingDrivers;
	static bool				m_bFindCarBehind;
	static bool				m_bDisplayStatusInfo;

	static bool				AllowCarAiDebugDisplayForThisVehicle		(const CVehicle* const pVeh);

	#if	__DEV
		static void				AddBankWidgets(bkBank& bank);
	#endif
#endif	// __BANK
	static float			ms_fMoveSpeedBelowWhichToCheckIfStuck;
	static bool				m_bTailgateOtherCars;
	static bool				m_bUseRouteToFindCarBehind;
	static bool				m_bUseScannerToFindCarBehind;
	static bool				m_bUseProbesToFindCarBehind;
	static bool				m_bUseFollowRouteToFindCarBehind;
	static bool				m_bUseNodeListToFindCarBehind;
	static bool				m_bEnableNewJoinToRoadInAnyDir;
	static bool				m_bEnableThreePointTurnCenterProbe;

#if __BANK
	//
	struct SDEBUG_RouteInterceptionTest
	{
		u32		uTS;
		bool	bRoutePassByEntityPos;
		bool	bRouteAvailable;

		struct SCheck
		{
			float fTime;
			Vec3V vEntityPos;
			Vec2V vEntityVelXY;
			Vec3V vVehiclePos;
			Vec2V vVehicleVelXY;
			bool bInterception;
			ScalarV scDistanceXY;
			ScalarV scDistanceLocalX;
			ScalarV scDistanceLocalY;
		};

		void AddCheck(float fTime, bool bInterception, Vec3V_In vEntityPos, Vec2V_In vEntityVelXY, Vec3V_In vVehiclePos, Vec2V_In vVehicleVelXY, ScalarV scDistanceXY, ScalarV scDistanceLocalX, ScalarV scDistanceLocalY)
		{
			SCheck& rCheck = aChecks.Grow();
			rCheck.fTime = fTime;
			rCheck.bInterception = bInterception;
			rCheck.vEntityPos = vEntityPos;
			rCheck.vEntityVelXY = vEntityVelXY;
			rCheck.vVehiclePos = vVehiclePos;
			rCheck.vVehicleVelXY = vVehicleVelXY;
			rCheck.scDistanceXY = scDistanceXY;
			rCheck.scDistanceLocalX = scDistanceLocalX;
			rCheck.scDistanceLocalY = scDistanceLocalY;
		}

		atArray<SCheck> aChecks;
	};

	static const s32 uNUM_DEBUG_SAVED_TESTS = 1;
	mutable TCircularArray<SDEBUG_RouteInterceptionTest, uNUM_DEBUG_SAVED_TESTS> m_aDEBUG_RouteInterceptionTests;

#endif // __BANK

	void					Process_NearbyEntityLists();
private:
	void					Process_DummyFlags();
	void					Process_CarBehind();
	void					Process_TouchingRealMissionVehicle();
	void					AddCarsBehindUs(const s32 numCarsBehindUs);
	void					Process_NumCarsBehindUs();
	bool					AreWeBehindCar(const CVehicle* pOtherVeh, Vec3V_In vFwd, Vec3V_In vFrontBumperPos, ScalarV_In scMaxDistSq, ScalarV_InOut RESTRICT scDistSq, ScalarV_In scDotLimit) const;
	CVehicle*				FindCarWeAreBehind(Vec3V_In vFwd, Vec3V_In vFrontBumperPos, ScalarV_In scMaxDistSq, ScalarV_InOut RESTRICT scDistSqOut) const;
	CVehicle*				FindCarWeAreSlipstreamingBehind(Vec3V_ConstRef UNUSED_PARAM(vPos), Vec3V_ConstRef vFwd, Vec3V_ConstRef vFrontBumperPos, ScalarV_ConstRef scMaxDistSq) const;

	void					HandleEvents();
	bool					ShouldStopForSiren();

protected:
	CVehicleTaskManager		m_TaskManager;
	AvoidanceCache			m_AvoidanceCache;
	InvMassScaleOverride	m_InvMassScaleOverride;
	CVehicle				*m_pVehicle;
	CVehicleNodeList		*m_pCachedNodeList;		// Cached node list - valid from after task update, until beginning of task update the next frame
	RegdTask				m_pCachedNodeListTask;		// Reg'd pointer to task owning the cached node list; if this becomes NULL then cached node list must be invalidated
	const CVehicleFollowRouteHelper*	m_pCachedFollowRouteHelper;
	RegdTask				m_pCachedFollowRouteHelperTask;

	CJunction*			m_Junction;		// Pointer to the CJunction associated with m_JunctionNode.
	CJunction*			m_PreviousJunction;		// Pointer to the CJunction associated with m_JunctionNode.

	CJunctionEntrance*	m_JunctionEntrance;
	CJunctionEntrance*	m_JunctionExit;
	CJunctionEntrance*	m_PreviousJunctionEntrance;
	CJunctionEntrance*	m_PreviousJunctionExit;

	CNodeAddress		m_JunctionNode;		// What junction (identified by its node) does this car belong to?
	CNodeAddress		m_PreviousJunctionNode;
	CNodeAddress		m_JunctionEntranceNode;
	CNodeAddress		m_JunctionExitNode;
	CNodeAddress		m_PreviousJunctionEntranceNode;
	CNodeAddress		m_PreviousJunctionExitNode;

	Vector3				m_vJunctionEntrance;
	Vector3				m_vJunctionExit;
	Vector3				m_vPreviousJunctionEntrance;
	Vector3				m_vPreviousJunctionExit;

	float				m_fPretendOccupantAggressiveness;
	float				m_fPretendOccupantAbility;
	float				m_fCustomPathNodeStreamingRadius;

	s32					m_JunctionEntranceLane;
	s32					m_JunctionExitLane;
	s32					m_PreviousJunctionEntranceLane;
	s32					m_PreviousJunctionExitLane;

	u32					m_uTimeWeStartedCollidingWithPlayerVehicle;
	u32					m_uLastTimePedEntering;
	u32					m_uLastTimePedInCover;
	u32					m_uLastTimeRamming;
	u32					m_uLastTimePlayerAimedAtOrAround;
	u32					m_iJunctionTurnDirection;
	u32					m_iJunctionArrivalTime;

	ETrafficLightCommand	m_TrafficLightCommand;

	s8					m_JunctionCommand;	// What is the junction code telling this car to do?
	s8					m_iJunctionFilter;	// Left, Right, Middle lanes?
	u8					m_bHasFixedUpPathForCurrentJunction;
	bool				m_bBlockWeaponSelection; // 
	bool				m_bUsingScriptAutopilot;
};


CJunction* CVehicleIntelligence::GetJunction() const
{
	// This stuff should hold up, at least as long as we don't run out of junctions:
	//	Assert(!m_Junction || m_Junction->ContainsJunctionNode(m_JunctionNode));
	//	Assert(CJunctions::FindJunctionFromNode(m_JunctionNode) == m_Junction);
	return m_Junction;
}

inline CEvent* CVehicleIntelligence::AddEvent(const CEvent& rEvent, const bool bForcePersistence, const bool bRemoveOtherEventsToMakeSpace)
{
	AI_EVENT_GROUP_LOCK(&m_eventGroup);

	CEvent* pNewEvent = NULL;

	if (!(m_eventGroup.IsFull() && !bRemoveOtherEventsToMakeSpace))
	{
		pNewEvent = static_cast<CEvent*>(m_eventGroup.Add(rEvent));

		if (pNewEvent)
		{
			pNewEvent->ForcePersistence(bForcePersistence);
		}
	}

	return pNewEvent;
}

class CHeliIntelligence : public CVehicleIntelligence
{
public:
	CHeliIntelligence(CVehicle *pVehicle);

	~CHeliIntelligence()
	{
		m_pExtraEntityToDoHeightChecksFor = NULL;
	}


	virtual void	Process(bool fullUpdate, float fFullUpdateTimeStep);

	float	GetOldOrientation() const							{ return m_OldOrientation; }
	void	SetOldOrientation(float fOldOrientation)			{ m_OldOrientation = fOldOrientation; }

	float	GetOldTilt() const									{ return m_OldTilt; }
	void	SetOldTilt(float fOldTilt)							{ m_OldTilt = fOldTilt; }

	float	GetCrashTurnSpeed() const							{ return m_crashTurnSpeed; }

	float	GetTimeSpentLanded() const							{ return m_fTimeSpentLanded; }

	const CPhysical* GetExtraEntityToDoHeightChecksFor() const		{ return m_pExtraEntityToDoHeightChecksFor;}
	void	SetExtraEntityToDoHeightChecksFor(CPhysical* pEntity) {m_pExtraEntityToDoHeightChecksFor = pEntity;}

	void	UpdateOldPositionVaraibles();					

	float	GetHeliAvoidanceHeight() const { return m_fHeliAvoidanceHeight; }
	
	bool	GetProcessAvoidance() const { return m_bProcessAvoidance; }
	void	SetProcessAvoidance(bool bValue) { m_bProcessAvoidance = bValue; }

	CVehPIDController& GetPitchController() { return m_PitchController; }
	CVehPIDController& GetRollController() { return m_RollController; }
	CVehPIDController& GetYawController() { return m_YawController; }
	CVehPIDController& GetThrottleController() { return m_ThrottleController; }

	bool CheckTimeToRetractLandingGear() const { return fwTimer::GetTimeInMilliseconds() > m_RetractLandingGearTimeInMS; }
	void SetRetractLandingGearTime(u32 in_RetractLandingGearTimeInMS) { m_RetractLandingGearTimeInMS = in_RetractLandingGearTimeInMS; }
	void ExtendLandingGearTime(u32 timeExtenstion) { m_RetractLandingGearTimeInMS += timeExtenstion; }

public:

	bool bHasCombatOffset;
	Vector3 vCombatOffset;

	void MarkLandingAreaBlocked();
	bool IsLandingAreaBlocked() const;
	bool IsLanded() const;

private:

	void ProcessAvoidance();
	void ProcessLanded();

private:

	u32 m_RetractLandingGearTimeInMS;
	CExpensiveProcess	m_AvoidanceDistributer;	

	CVehPIDController m_PitchController;
	CVehPIDController m_RollController;
	CVehPIDController m_YawController;
	CVehPIDController m_ThrottleController;

	//	if something gets attached by rope to this heli, do height
	//	checks for it as well
	RegdPhy				m_pExtraEntityToDoHeightChecksFor;

	float	m_crashTurnSpeed;
	float	m_OldOrientation;
	float	m_OldTilt;
	float	m_fHeliAvoidanceHeight;
	float	m_fTimeSpentLanded;
	float	m_fTimeLandingAreaBlocked;
	bool	m_bProcessAvoidance;
};

class CPlaneIntelligence : public CVehicleIntelligence
{
public:

	CPlaneIntelligence(CVehicle *pVehicle)
		: CVehicleIntelligence(pVehicle)
		, m_RetractLandingGearTimeInMS(0xffffffff)
	{}

	CVehPIDController& GetPitchController() { return m_PitchController; }
	CVehPIDController& GetRollController() { return m_RollController; }
	CVehPIDController& GetYawController() { return m_YawController; }
	CVehPIDController& GetThrottleController() { return m_ThrottleController; }

	bool CheckTimeToRetractLandingGear() const { return fwTimer::GetTimeInMilliseconds() > m_RetractLandingGearTimeInMS; }
	void SetRetractLandingGearTime(u32 in_RetractLandingGearTimeInMS) { m_RetractLandingGearTimeInMS = in_RetractLandingGearTimeInMS; }

private:

	u32 m_RetractLandingGearTimeInMS;
	CVehPIDController m_PitchController;
	CVehPIDController m_RollController;
	CVehPIDController m_YawController;
	CVehPIDController m_ThrottleController;
};

#endif
