#ifndef TASK_VEHICLE_GO_TO_AUTOMOBILE_H
#define TASK_VEHICLE_GO_TO_AUTOMOBILE_H

#include "atl/map.h"

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "Task/System/Tuning.h"
#include "VehicleAi\racingline.h"
#include "vehicleAi/task/DeferredTasks.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoTo.h"

class CVehicle;
class CVehControls;

#define USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS 1

struct AvoidanceTaskObstructionData; // Some params that get passed between the avoidance task functions

enum CarAvoidanceLevel
{
	CAR_AVOIDANCE_NONE = 0,
	CAR_AVOIDANCE_LITTLE,
	CAR_AVOIDANCE_FULL
};

/*
//
//
//Overridden Goto which makes the vehicle race
class CTaskVehicleGoToRacing : public CTaskVehicleGoToAutomobile
{
public:
	// Constructor/destructor
	CTaskVehicleGoToRacing(CPhysical* pTargetEntity, 
		const DrivingModeType iDrivingStyle=DMode_StopForCars, 
		const int iCruiseSpeed=10, 
		const Vector3* pvTargetCoords=NULL, 
		const bool bEveryThingInReverse = false, 
		const int TargetReachedDist = CTaskVehicleMissionBase::DEFAULT_TARGET_REACHED_DIST, 
		const int StraightLineDist = CTaskVehicleMissionBase::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST, 
		const bool bAllowedToGoAgainstTraffic = true) :
	CTaskVehicleGoToAutomobile(pTargetEntity, iDrivingStyle, iCruiseSpeed, pvTargetCoords, bEveryThingInReverse, TargetReachedDist, StraightLineDist, bAllowedToGoAgainstTraffic)
	{

	}

	~CTaskVehicleGoToRacing(){;}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_RACING; }
	aiTask*			Copy() const {return rage_new CTaskVehicleGoToRacing(m_pTargetEntity,
		(DrivingModeType)m_iDrivingStyle, 
		GetCruiseSpeed(),
		&m_vTargetPos,
		m_bDoEverythingInReverse, 
		m_iTargetArriveDist,
		m_bAllowedToGoAgainstTraffic);}

	//overloaded goto function
	void		ControlAIVeh_FollowPath				(CVehicle* pVeh, CVehControls* pVehControls);
protected:

	RacingLineCubic		m_TheRacingLineCubic;
	RacingLineCardinal	m_TheRacingLineCardinal;
	RacingLineOptimizing	m_TheRacingLineOptimizing;

};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskVehicleGoToPointWithAvoidanceAutomobile
// Takes in a target and does it's best to drive there whilst still avoiding things
// will do a three point turn if massively off target
///////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskVehicleGoToPointWithAvoidanceAutomobile : public CTaskVehicleGoTo
{
public:

	struct Tunables : CTuning
	{
		Tunables();
		
		void OnPostLoad()
		{
			//Compute constant(ish) values here so they don't have to be re-computed every frame.
			m_TailgateDistanceMaxSq = rage::square(m_TailgateDistanceMax);
			m_TailgateIdealDistanceMinSq = rage::square(m_TailgateIdealDistanceMin);
			m_TailgateIdealDistanceMaxSq = rage::square(m_TailgateIdealDistanceMax);
		}

		//Read in from metadata.
		float	m_TailgateDistanceMax;
		float	m_TailgateIdealDistanceMin;
		float	m_TailgateIdealDistanceMax;
		float	m_TailgateSpeedMultiplierMin;
		float	m_TailgateSpeedMultiplierMax;
		float	m_TailgateVelocityMin;
		float	m_ChanceOfPedSeeingCarFromBehind;
		float	m_MinSpeedForAvoid;
		float	m_MinDistanceForAvoid;
		float	m_MaxSpeedForAvoid;
		float	m_MaxDistanceForAvoid;
		float	m_MinDistanceForAvoidDirected;
		float	m_MinSpeedForAvoidDirected;
		float	m_MaxDistanceForAvoidDirected;
		float	m_MaxSpeedForAvoidDirected;
		float	m_MaxAbsDotForAvoidDirected;
		float	m_MaxSpeedForBrace;
		float	m_MinSpeedForDive;
		float	m_MinTimeToConsiderDangerousDriving;
		float	m_MultiplierForDangerousDriving;
		float	m_MinDistanceToSideOnPavement;
		float	m_MaxDistanceToSideOnPavement;
		
		//Computed after load for performance reasons.
		float	m_TailgateDistanceMaxSq;
		float	m_TailgateIdealDistanceMinSq;
		float	m_TailgateIdealDistanceMaxSq;

		PAR_PARSABLE;
	};

	struct ThreePointTurnInfo
	{
		ThreePointTurnInfo() : m_iNumRecentThreePointTurns(0)
		{

		}

		Vector3 m_vLastTurnPosition;
		u32 m_fLastThreePointTurnTime;
		int m_iNumRecentThreePointTurns;
		static int ms_iMaxAttempts;
	};
	
	struct ObstructionInformation
	{
		enum Flags
		{
			IsCompletelyObstructed				= BIT0,
			IsObstructedByLawEnforcementPed		= BIT1,
			IsObstructedByLawEnforcementVehicle	= BIT2,
			IsCompletelyObstructedClose			= BIT3,
			IsCompletelyObstructedBySingleObstruction = BIT4,
		};
		
		ObstructionInformation()
		{ Clear(); }
		
		void Clear()
		{
			//Clear the distances.
			m_fDistanceToLawEnforcementObstruction = FLT_MAX;

			//Clear the flags.
			m_uFlags.ClearAllFlags();
		}
		
		float		m_fDistanceToLawEnforcementObstruction;
		fwFlags8	m_uFlags;
	};

	typedef enum
	{
		State_Invalid = -1,
		State_GoToPoint,
		State_ThreePointTurn,
		State_WaitForTraffic,
		State_WaitForPed,
		State_TemporarilyBrake,
		State_Swerve,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleGoToPointWithAvoidanceAutomobile( const sVehicleMissionParams& params,
													const bool bAllowThreePointTurns = true,
													const bool bAllowAchieveMission = true);
	~CTaskVehicleGoToPointWithAvoidanceAutomobile();


	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE; }
	aiTask*			Copy() const {return rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(	m_Params, m_bAllowThreePointTurns);}

	// FSM implementations
	virtual FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);
	s32				GetDefaultStateAfterAbort	()	const {return State_GoToPoint;}

	bool		GetSlowingDownForPed() const {return m_bSlowingDownForPed;}
	bool		GetSlowingDownForCar() const {return m_bSlowingDownForCar;}

	// PURPOSE:	Used by CTaskPlayerDrive to share code for triggering ped evasion.
	static void ScanForPedDanger(CVehicle* pVeh, float minX, float minY, float maxX, float maxY)
	{
		SlowCarDownForPedsSectorListHelper(NULL, pVeh, minX, minY, maxX, maxY, NULL, 0.0f, false);
	}

	static void FindPlayerNearMissTargets(const CVehicle* pPlayerVehicle, CPlayerInfo* playerInfo, bool trackPreciseNearMiss = false);
	static void MakeWayForCarWithSiren(CVehicle *pVehicleWithSiren);
	void MakeWayForCarTellingOthersToFlee(CVehicle* pVehicleToFlee);

	static bool ShouldNotAvoidVehicle(const CVehicle& rVeh, const CVehicle& rOtherVeh);
	bool ShouldIgnoreNonDirtyVehicle(const CVehicle& rVeh, const CVehicle& rOtherVeh, const bool bDriveInReverse, const bool bOnSingleTrackRoad) const;

	void SetWaitingForPlayerPed(bool bWaitingForPlayerPed);
	void SetAllowAchieveMission(bool in_bAllowAchieveMission) { m_bAllowAchieveMission = in_bAllowAchieveMission; }
	
	void SetAvoidanceMarginForOtherLawEnforcementVehicles(float fValue) { m_fAvoidanceMarginForOtherLawEnforcementVehicles = fValue; }
	
	const ObstructionInformation& GetObstructionInformation() const { return m_ObstructionInformation; }

	bool IsCurrentlyWaiting() const
	{
		return (GetState() == State_WaitForPed) 
			|| (GetState() == State_WaitForTraffic) 
			|| (GetState() == State_TemporarilyBrake);
	}

	void RequestOvertakeCurrentCar(bool bReverseFirst)
	{
		m_bOvertakeCurrentCarRequested = true;
		m_bReverseBeforeOvertakeCurrentCarRequested = bReverseFirst;
	}

	class AvoidanceInfo
	{
	public:
		AvoidanceInfo()
		{
			Clear();
		}
		Vector3 vVelocity;
		Vector3 vPosition;
		float fDirToObstruction;
		float fDirection;
		float fDistToObstruction;
		float fObstructionScore;
		bool bGiveWarning;
		bool bIsStationaryCar;
		bool bIsObstructedByLaw;
		bool bIsTurningLeftAtJunction;
		bool bIsHesitating;
		void Clear()
		{	
			fDirection = 0.0f;
			fDistToObstruction = -1.0f;
			ObstructionType = Invalid_Obstruction;
			vVelocity.Zero();
			vPosition.Zero();
			fDirToObstruction = 0.0f;
			bGiveWarning = false;
			fObstructionScore = 0.0f;
			bIsStationaryCar = false;
			bIsObstructedByLaw = false;
			bIsTurningLeftAtJunction = false;
			bIsHesitating = false;
		}
		bool HasObstruction()
		{
			return fDistToObstruction >= 0.0f;
		}
		enum eObstructionType
		{
			Invalid_Obstruction = -1,
			Obstruction_Vehicle,
			Obstruction_Ped,
			Obstruction_Object,
			Obstruction_RoadEdge,
			Num_Obstructions
		};
		eObstructionType ObstructionType;
	};

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
	virtual void		Debug() const;
#endif //!__FINAL

	static CVehicle* GetTrailerParentFromVehicle(const CVehicle* pVehicle);

	void		GoToPointWithAvoidance_OnDeferredTask(const aiDeferredTasks::TaskData& data);
	void		GoToPointWithAvoidance_OnDeferredTask_ProcessSuperDummy(const aiDeferredTasks::TaskData& data);
	void		GoToPointWithAvoidance_OnDeferredTaskCompletion(const aiDeferredTasks::TaskData& data);

protected:
	// FSM function implementations
	// State_GoToPoint
	void		GoToPointWithAvoidance_OnEnter				(CVehicle* pVehicle);
	FSM_Return	GoToPointWithAvoidance_OnUpdate				(CVehicle* pVehicle);

	// State_ThreePointTurn
	void		ThreePointTurn_OnEnter						(CVehicle* pVehicle);
	FSM_Return	ThreePointTurn_OnUpdate						(CVehicle* pVehicle);
	FSM_Return  ThreePointTurn_OnExit						(CVehicle* pVehicle);

	//wait update function used for all wait tasks
	FSM_Return	Wait_OnUpdate								(CVehicle* pVehicle);
	// State_WaitForTraffic
	void		WaitForTraffic_OnEnter						(CVehicle* pVehicle);
	// State_WaitForPed
	void		WaitForPed_OnEnter							(CVehicle* pVehicle);
	//State_TemporarilyStop
	void		TemporarilyBrake_OnEnter					(CVehicle* pVehicle);
	FSM_Return	TemporarilyBrake_OnUpdate					(CVehicle* pVehicle);

	void		SwerveForHeadOnCollision_OnEnter			(CVehicle* pVehicle);
	FSM_Return	SwerveForHeadOnCollision_OnUpdate			(CVehicle* pVehicle);

	//Helper functions
	void		DealWithShoreAvoidance						(const CVehicle *pVeh, float &desiredSpeed, float &toTargetOrientation, float vehDriveOrientation);
	void		DealWithTrafficAndAvoidance					(CVehicle *pVeh, float &desiredSpeed, float &toTargetOrientation, float vehDriveOrientation, bool& bGiveWarning, const bool bWasSlowlyPushingPlayer, const float fTimeStep);
	bool		ShouldDoThreePointTurn						(CVehicle *pVehicle, float* pDirToTargetOrientation, float* pVehDriveOrientation);

	float		FindMaximumSpeedForThisCarInTraffic			(CVehicle* pVeh, const float SteerDirection, const bool bUseAltSteerDirection, const bool bGiveWarning, float RequestedSpeed, float avoidanceRadius, const spdAABB& boundBox, CarAvoidanceLevel carAvoidanceLevel, bool bWasSlowlyPushingPlayer);
	float		FindAngleToAvoidBuildings					(CVehicle* const pVeh, bool bTurningClockwise, float vehDriveOrientation, float dirToTargetOrientation, const spdAABB& boundBox);
	float		FindAngleToWeaveThroughTraffic				(CVehicle* pVeh, const CEntity *pException, float Direction, float vehDriveOrientation, float CheckDistMult, CarAvoidanceLevel carAvoidanceLevel, bool bAvoidPeds, bool bAvoidObjs, float avoidanceRadius, bool& bGiveWarning, const bool bWillStopForCars, const spdAABB& boundBox, const float fDesiredSpeed);
	float		FindMaximumSpeedForRacingCar				(CVehicle* pVeh, const float fDesiredSpeed, const spdAABB& boundBox);

	void		FindAvoidancePoints							(phBound *bound, Vec4V& Result);
	void		FindAvoidancePointsBox						(Vec3V_In BoxSize, Vec3V_In CentroidOffset, Vector3& MinVec, Vector3& MaxVec, bool& bResult);
	void		FindAvoidancePointsSphere					(const phBoundSphere *boundSphere, Vector3& MinVec, Vector3& MaxVec, bool& bResult);
	void		FindAvoidancePointsPoly						(const Vector3 &vtx0, const Vector3 &vtx1, const Vector3 &vtx2, Vector3& MinVec, Vector3& MaxVec, bool& bResult);
	void		FindAvoidancePointsBound					(phBound *bound, Vector3& MinVec, Vector3& MaxVec, bool& bResult, Matrix34 &mat);
	
	bool		IsObstructedByLawEnforcementPed				(const CPed* pPed) const;
	bool		IsObstructedByLawEnforcementVehicle			(const CVehicle* pVehicle) const;

	//about vehicle position
	bool		IsThisAStationaryCar						(const CVehicle* const pVeh) const;
	bool		IsThisAParkedCar							(const CVehicle* const pVeh) const { return pVeh->m_nVehicleFlags.bIsThisAParkedCar; } 

	bool		IsFirstCarBestOneToGo						(const CVehicle* const pVeh, const CVehicle* const pOtherVeh, const bool bFirstCarIsAlreadyGoing);

	//Slow down
	void		SlowCarDownForPedsSectorList				(CVehicle* pVeh, const float MinX, const float MinY, const float MaxX, const float MaxY, float *pMaxSpeed, float OriginalMaxSpeed, bool bWasSlowlyPushingPlayer);
	static void	SlowCarDownForPedsSectorListHelper			(CTaskVehicleGoToPointWithAvoidanceAutomobile* pTask, CVehicle* pVeh, const float MinX, const float MinY, const float MaxX, const float MaxY, float *pMaxSpeed, float OriginalMaxSpeed, bool bWasSlowlyPushingPlayer);
	inline static void GenerateMinMaxForPedAvoidance		(const Vector3& vecVehiclePosition, const float avoidanceRadius, float& MinX, float& MinY, float& MaxX, float& MaxY);

	void		SlowCarDownForDoorsSectorList				(CVehicle* pVeh, const float MinX, const float MinY, const float MaxX, const float MaxY, float *pMaxSpeed, float OriginalMaxSpeed, const spdAABB& boundBox, const bool bStopForBreakableDoors);

	void		SlowCarDownForCarsSectorList				(CVehicle* pVeh, float MinX, float MinY, float MaxX, float MaxY, float &maxSpeed, float OriginalMaxSpeed, const float SteerDirection, const bool bUseAltSteerDirection, const bool bGiveWarning, const spdAABB& boundBox, CarAvoidanceLevel carAvoidanceLevel);
	void		SlowCarDownForOtherCar						(CVehicle* pVeh, CVehicle* pOtherVeh, float &maxSpeed, float OriginalMaxSpeed, const float SteerDirection, const bool bUseAltSteerDirection, const bool bGiveWarning, const spdAABB& boundBox, const float fBoundRadius, CarAvoidanceLevel carAvoidanceLevel, const bool bPreventFullStop, const bool bSkipSphereCheck);

	bool		ShouldAllowTimeslicingWhenAvoiding			() const;
	bool		ShouldGiveWarning							(const CVehicle* const pVeh, const CVehicle* const pOtherVeh) const;
	bool		ShouldWaitForTrafficBeforeTurningLeft		(CVehicle* pVeh, const CEntity *pException, const spdAABB& boundBox) const;

	bool		IsThereRoomToOvertakeVehicle				(const CVehicle* const pVeh, const CVehicle* const pOtherVeh, const bool bOnlyWaitForCarsMovingInOppositeDirection) const;

	//bool		CanSwitchLanesToAvoidObstruction			(const CVehicle* const pVeh, const CPhysical* const pAvoidEnt, const float fDesiredSpeed);
	//bool		UpdateLanesToPassObstruction				(const CVehicle* const pVeh, const float fDesiredSpeed);
	//s8			HelperGetNumLanesToThisNode					(const int node, const CVehicleNodeList* pNodeList) const;
	//bool		IsSegmentObstructed							(const int node, const CVehicle* const pVeh, const int laneToCheck, const float fDesiredSpeed, float& fDistToObstructionOut, CPhysical*& pObstructingEntityOut, float& fObstructionSpeedOut) const;
	//float		HelperGetSegmentLength						(const int node, const CVehicleNodeList* pNodeList) const;
	//bool		IsAreaClearToChangeLanes					(const CVehicle* const pVeh, const CVehicleNodeList* pNodeList, const bool bSearchLeft, const float fDesiredSpeed) const;
	CTrailer*	HelperGetVehicleTrailer						(const CVehicle* const pVeh) const;

	float ScoreAvoidanceDirections( const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo* AvoidanceInfoArray, const float distToOriginalObstruction);

	//do all the scoring for one obstruction that doesn't rely on the angle to the obstruction
	float ScoreOneObstructionAngleIndependent(const AvoidanceInfo::eObstructionType obstructionType, const float fDistToObstruction, const Vector3& vOurVelocity, const Vector3& vTheirVelocity, const float fOurCurrentSpeed) const;

	float		TestDirectionsForObstructionsNew			(CVehicle* pVeh, const CEntity *pException, float MinX, float MinY, float MaxX, float MaxY, const float fOriginalDirection, const float vehDriveOrientation, CarAvoidanceLevel carAvoidanceLevel, bool bAvoidVehs, bool bAvoidPeds, bool bAvoidObjs, bool& bGiveWarning, const bool bWillStopForCars, const spdAABB& boundBox, const float fDesiredSpeed);

	//void		FindRoadEdgesUsingNodeList					(AvoidanceTaskObstructionData& obstructionData);
	void		FindRoadEdgesUsingFollowRoute				(AvoidanceTaskObstructionData& obstructionData);
	void		FindVehicleObstructions						(AvoidanceTaskObstructionData& obstructionData, const bool bGiveWarning);
	void		FindPedObstructions							(AvoidanceTaskObstructionData& obstructionData, const bool bOnlyAvoidTrulyStationaryPeds);
	void		FindObjectObstructions						(AvoidanceTaskObstructionData& obstructionData);
	void		AddObstructionForSingleVehicle				(AvoidanceTaskObstructionData& obstructionData, const CVehicle* const pOtherCar, bool bStationaryCar, bool bParkedCar, bool bGiveWarning);
	void		AddObstructionForSinglePed					(AvoidanceTaskObstructionData& obstructionData, CEntity* pEntity);
	void		AddObstructionForSingleObject				(AvoidanceTaskObstructionData& obstructionData, CObject* pEntity );
	//bool		DoesLineSegmentCrossRoadBoundariesUsingNodeList			(const AvoidanceTaskObstructionData& obstructionData, const float fHeading, const float fSegmentLength, float& fDistToIntersectionOut) const;
	bool		DoesLineSegmentCrossRoadBoundariesUsingFollowRoute		(const AvoidanceTaskObstructionData& obstructionData, const float fHeading, const float fSegmentLength, float& fDistToIntersectionOut) const;

	// PURPOSE:	Test a number of line segments against the boundaries of the road.
	// PARAMS:	obstructionData			- Various information about the task at hand.
	//			avoidanceInfoArray		- Array of avoidance info, incl. test direction. Used only for input.
	//			numDirections			- The number of elements in avoidanceInfoArray.
	//			fSegmentLength			- The length of the segments to test.
	//			distToRoadEdgeVectorArrayOut - Pointer to a vector-aligned array with numDirections float elements, in which the results will be stored.
	// RETURNS	True if distToRoadEdgeVectorArrayOut was populated with valid information, or false if we found that
	//			no tests are necessary and all tests should be considered misses.
	bool		DoLineSegmentsCrossRoadBoundariesUsingFollowRoute(const AvoidanceTaskObstructionData& obstructionData, const AvoidanceInfo* avoidanceInfoArray, int numDirections, const float& fSegmentLength, Vec4V* distToRoadEdgeVectorArrayOut) const;

	bool		TestOneDirectionAgainstObstructions			(const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo& avoidanceInfo, bool testRoadEdges, bool& bShouldGiveWarning, const CPhysical** pHitEntity = NULL);

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	// PURPOSE:	Test a number of line segments against an array of polygons representing vehicles to potentially avoid.
	// PARAMS:	obstructionData			- Various information about the task at hand.
	//			avoidanceInfoArray		- Array of avoidance info, incl. test direction and scores from previous tests. Will be used both for input and output.
	//			numDirections			- The number of elements in avoidanceInfoArray.
	//			bHitSomethingArrayInOut	- Array of numDirections bools. Any element will get set to true if it hit an obstruction and beat the previous score.
	//			pHitEntity				- If set, information about which entity was hit is written here. Really only makes sense if you have just one direction.
	void		TestAndScoreLineSegmentsAgainstVehicles(const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo* avoidanceInfoArray, int numDirections, bool* bHitSomethingArrayInOut, bool& bShouldGiveWarning, const CPhysical** pHitEntity = NULL);

	// PURPOSE:	Test a number of line segments against an array of cylindrical obstructions representing peds and vehicles,
	//			and update the corresponding avoidance scores.
	// PARAMS:	obstructionData			- Various information about the task at hand.
	//			avoidanceInfoArray		- Array of avoidance info, incl. test direction and scores from previous tests. Will be used both for input and output.
	//			numDirections			- The number of elements in avoidanceInfoArray.
	//			bHitSomethingArrayInOut	- Array of numDirections bools. Any element will get set to true if it hit an obstruction and beat the previous score.
	//			pHitEntity				- If set, information about which entity was hit is written here. Really only makes sense if you have just one direction.
	void		TestAndScoreLineSegmentsAgainstCylindricalObstructions(const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo* avoidanceInfoArray, int numDirections, bool* bHitSomethingArrayInOut, const CPhysical** pHitEntity = NULL) const;
#endif	// USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS

	// PURPOSE:	Test a number of line segments against the boundaries of the road, and update the corresponding avoidance scores.
	// PARAMS:	obstructionData			- Various information about the task at hand.
	//			avoidanceInfoArray		- Array of avoidance info, incl. test direction and scores from previous tests. Will be used both for input and output.
	//			numDirections			- The number of elements in avoidanceInfoArray.
	//			bHitSomethingArrayInOut	- Array of numDirections bools. Any element will get set to true if it hit the road edge and beat the previous score.
	void		TestAndScoreLineSegmentsCrossRoadBoundariesUsingFollowRoute(const AvoidanceTaskObstructionData& obstructionData, AvoidanceInfo* avoidanceInfoArray, int numDirections, bool* bHitSomethingArrayInOut) const;

#if USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	bool		TestDirectionsAgainstSingleVehicle			(const AvoidanceTaskObstructionData& obstructionData, CEntity* pEntity, bool bStationaryCar
		, bool bParkedCar, bool& bGiveWarning, AvoidanceInfo* avoidanceInfoArray, int numDirections, const bool bPreventAddMarginForOtherCar);
#else
	bool		TestDirectionAgainstSingleVehicle			(const AvoidanceTaskObstructionData& obstructionData, CEntity* pEntity, bool bStationaryCar, bool bParkedCar, bool& bGiveWarning, AvoidanceInfo& avoidanceInfo, const bool bPreventAddMarginForOtherCar);
#endif

	float		Test2MovingRectCollision_OnlyFrontBumper	(const CPhysical* const pVeh, const CPhysical* const pOtherVeh, 
															float OtherSpeedX, float OtherSpeedY,
															const Vector3& ourNewDriveDir,
															const Vector3& otherDriveDir,
															const spdAABB& boundBox,
															const spdAABB& otherBox,
															const bool bOtherEntIsDoor);
	float		Test2MovingRectCollision					(const CPhysical* const pVeh, const CPhysical* const pOtherVeh, 
															float OtherSpeedX, float OtherSpeedY,
															const Vector3& ourNewDriveDir,
															const Vector3& otherDriveDir,
                                                            const spdAABB& boundBox,
															const spdAABB& otherBox,
															const bool bOtherEntIsDoor);
	float		Test2MovingSphereCollision					(const CVehicle* const pVeh, const CVehicle* const pOtherVeh, const Vector3& vOurVel, const Vector3& vTheirVel, const float fBoundRadius);

#if !USE_VECTORIZED_VEH_PED_OBJ_OBSTRUCTION_TESTS
	bool		TestForThisAngle							(float Angle, Vector3 *pLine1Start, Vector3 *pLine1Delta, Vector3 *pLine2Start, Vector3 *pLine2Delta, Vector3 *pLine1Start_Other, Vector3 *pLine1Delta_Other, Vector3 *pLine2Start_Other, Vector3 *pLine2Delta_Other, float OtherCarMoveSpeedX, float OtherCarMoveSpeedY, float OurMoveSpeed, bool bSwapRound, float fLookaheadTime) const;
#else
	bool		TestForThisAngleNew							(Vec2V_In dirXYV, Vec2V_In line1StartOtherV, Vec2V_In line1DeltaOtherV, Vec2V_In line2StartOtherV, Vec2V_In line2DeltaOtherV, Vec2V_In line1StartV, Vec2V_In line1DeltaV, Vec2V_In line2StartV, Vec2V_In line2DeltaV, float OtherCarMoveSpeedX, float OtherCarMoveSpeedY, const float& OurMoveSpeed, const float& fLookaheadTime) const;
#endif

	//is this car in a convoy with other cars
	bool		InConvoyTogether							(const CVehicle* const UNUSED_PARAM(pVeh), const CVehicle* const UNUSED_PARAM(pOtherVeh));
	
	bool	IsAnyVehicleTooCloseToTailgate(const CVehicle& rVehicle, const CVehicle& rTailgateVehicle);
	bool	IsVehicleTooCloseToTailgate(const spdAABB& rVehicleBoundingBox, const CVehicle& rOtherVehicle) const;
	bool	TailgateCarInFront(const CVehicle* pVeh, float* pMaxSpeed);

	// PURPOSE:	Check if the vehicle is about to make a sharp turn to a target position to the
	//			side or behind, where we may have to leave superdummy mode.
	bool	CheckIsAboutToMakeSharpTurn(const CVehicle& veh, Vec3V_In tgtPos) const;

	void		ProcessSuperDummy(CVehicle* pVehicle, float fTimeStep) const;
	
	float	CalculateOtherVehicleMarginForAvoidance(const CVehicle& rVehicle, const CVehicle& rOtherVehicle, bool bGiveWarning) const;

	void UpdatePlayerOnFootIsBlockingUs(CVehicle* pVeh, CPed* pDriver, CPed* pBlockingPed, float *pMaxSpeed, const float fDistFromSideOfCar, const float fDistFromFrontOfCarBumper);

	bool StartOvertakeCurrentCar();
	
	ObstructionInformation m_ObstructionInformation;
	ThreePointTurnInfo m_threePointTurnInfo;
	
	float		m_fSmallestCollisionTSoFar;			// This is used to make sure we find the nearest collision.
	Vector3		m_SteeringTargetPos;				//the target pos we get after avoidance
	Vector3		m_vCachedGiveWarningPosition;
	bool		m_bBumperInsideBoundsThisFrame;
	u32		m_startTimeAvoidCarsUntilClear;		// Keep track of when the iDrivingFlags got set to DF_AvoidingCarsUntilClear
	//u32		m_NextTimeAllowedToChangeLanesForTraffic;
	u32		m_startTimeWaitingForPlayerPed;
	u32		m_lastTimeHonkedAtAnyPed;

	float	m_fMostImminentCollisionThisFrame;
	
	float	m_fAvoidanceMarginForOtherLawEnforcementVehicles;

	RegdVeh			m_pOptimization_pLastCarBlockingUs;	// If a car makes us stop 100% we check for that one first next frame to avoid unnecessary work.
	RegdVeh			m_pOptimization_pLastCarWeSlowedFor;
	RegdConstVeh	m_pOptimization_pVehicleTooCloseToTailgate;
	RegdVeh			m_pVehicleToIgnoreWhenStopping;

	u8		m_bStoppingForTraffic : 1;			// Are we actually stopping for traffic
	u8		m_bSlowingDownForCar : 1;			// Are we going slow because of car
	u8		m_bSlowingDownForPed : 1;			// Are we going slow because of ped
	u8		m_bAllowThreePointTurns : 1;
	u8		m_bAvoidedLeftLastFrame : 1;
	u8		m_bAvoidedRightLastFrame : 1;
	u8		m_bWaitingForPlayerPed : 1;
	u8		m_bSlowlyPushingPlayerThisFrame : 1;	//if the player was a dick, we might decide to not stop for him anymore
	u8		m_bReEnableSlowForCarsWhenDoneAvoiding : 1;
	u8		m_bAllowAchieveMission : 1;
	u8		m_bAvoidingCarsUntilClear : 1;
	u8		m_bPathContainsUTurn : 1;
	u8		m_bOvertakeCurrentCarRequested : 1;
	u8		m_bReverseBeforeOvertakeCurrentCarRequested : 1;
	
	float		m_fAvoidedEntityCounter; // Timer that counts down from the last time this vehicle avoided something

	u32 m_iTargetSideFlags;
	enum
	{
		FLAG_NEG_X	= 1,
		FLAG_POS_X	= 2,
		FLAG_NEG_Y	= 4,
		FLAG_POS_Y	= 8,
		FLAG_ALL_SIDES = FLAG_NEG_X + FLAG_POS_X + FLAG_NEG_Y + FLAG_POS_Y
	};

	static Vector2		ms_vAvoidXExtents;
	static Vector2		ms_vNmXExtents;
	static Vector2		ms_vNmVelocityScale;

	//static dev_float	ms_fChangeLanesVelocityRatio;
	//static u32		ms_TimeBeforeOvertake;
	//static u32		ms_TimeBeforeUndertake;

	static dev_float	ms_fAVOIDANCE_LOOKAHEAD_TIME;
	static dev_float	ms_fAVOIDANCE_LOOKAHEAD_TIME_FOR_FAST_VEHICLES;
	static dev_float	ms_fExtraWidthForBikes;
	static dev_float	ms_fExtraWidthForCars;
	static dev_float	ms_fMinLengthForDoors;	//for doors, "length" is their thickness

	static float GetAvoidanceLookaheadTime(const float fSpeed, const bool bCanUseLongerProbe)
	{
		static dev_float s_fFastSpeedThreshold = 25.0f;
		return (bCanUseLongerProbe && (fSpeed >= s_fFastSpeedThreshold))
			? ms_fAVOIDANCE_LOOKAHEAD_TIME_FOR_FAST_VEHICLES
			: ms_fAVOIDANCE_LOOKAHEAD_TIME;
	}

	static Tunables sm_Tunables;

	public:
#if __BANK
		static bool	ms_bEnableNewAvoidanceDebugDraw;
		static bool	ms_bDisplayObstructionProbeScores;
		static bool ms_bDisplayAvoidanceRoadSegments;
#endif //__BANK
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Just drives a car to a point, doesn't do any avoidance
///////////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskVehicleGoToPointAutomobile : public CTaskVehicleGoTo
{
public:

	typedef enum
	{
		State_Invalid = -1,
		State_GoToPoint
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleGoToPointAutomobile( const sVehicleMissionParams& params);
	~CTaskVehicleGoToPointAutomobile(){;}
	
	void SetCanUseHandBrakeForTurns(bool b) { m_bCanUseHandBrakeForTurns = b; }


	// TASK implementations
	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE; }
	aiTask*			Copy() const {return rage_new CTaskVehicleGoToPointAutomobile(m_Params);}

	// FSM implementations
	virtual FSM_Return	UpdateFSM							(const s32 iState, const FSM_Event iEvent);
	s32				GetDefaultStateAfterAbort			()	const {return State_GoToPoint;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );

#endif //!__FINAL
	float GetMaxThrottleFromTraction() const { return m_fMaxThrottleFromTraction; }

	static float CalculateMaximumThrottleBasedOnTraction(const CVehicle* pVehicle);
	static void		HumaniseCarControlInput(CVehicle* pVeh, CVehControls* pVehControls, bool bConservativeDriving, bool bGoingSlowly, const float fTimeStep, const float fDesiredSpeed);

	void GoToPoint_OnDeferredTask(const aiDeferredTasks::TaskData& data);

protected:

	// FSM function implementations

	// State_GoToPoint
	void		GoToPoint_OnEnter						(CVehicle* pVehicle);
	FSM_Return	GoToPoint_OnUpdate						(CVehicle* pVehicle);

	void		AdjustControls(CVehicle* pVeh, CVehControls* pVehControls, const float desiredSteerAngle, const float desiredSpeed);

	float	m_fMaxThrottleFromTraction;
	bool	m_bCanUseHandBrakeForTurns;
};

class CTaskVehicleGoToNavmesh : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Start,
		State_WaitingForResults,
		State_Goto,
		State_Stop
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleGoToNavmesh( const sVehicleMissionParams& params);
	~CTaskVehicleGoToNavmesh();

	// TASK implementations
	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH; }
	aiTask*			Copy() const {return rage_new CTaskVehicleGoToNavmesh(m_Params);}

	// FSM implementations
	FSM_Return	ProcessPreFSM							();
	virtual FSM_Return	UpdateFSM						(const s32 iState, const FSM_Event iEvent);
	s32				GetDefaultStateAfterAbort			()	const {return State_Start;}

	virtual const CVehicleFollowRouteHelper* GetFollowRouteHelper() const { return &m_followRoute; }

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleGoToNavmesh>; }
    virtual void CloneUpdate(CVehicle *pVehicle);

	const Vector3& GetClosestPointFoundToTarget() const;

	virtual void CleanUp();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );

	virtual void		Debug() const;
#endif //!__FINAL

	static dev_float			ms_fBoundRadiusMultiplier;
	static dev_float			ms_fBoundRadiusMultiplierBike;
	static dev_float			ms_fMaxNavigableAngleRadians;

private:

	// Calculates the position the vehicle should be driving towards and the speed.
	// Returns the distance to the end of the route
	float FindTargetPositionAndCapSpeed( CVehicle* pVehicle, const Vector3& vStartCoords, Vector3& vTargetPosition, float& fOutSpeed );

	void ResetLastTargetPos();
	bool UpdateTargetMoved();

	FSM_Return Start_OnEnter();
	FSM_Return Start_OnUpdate();

	FSM_Return WaitingForResults_OnEnter();
	FSM_Return WaitingForResults_OnUpdate();

	FSM_Return Goto_OnEnter();
	FSM_Return Goto_OnUpdate();
	void	   Goto_OnExit();

	FSM_Return Stop_OnEnter();
	FSM_Return Stop_OnUpdate();

	CVehicleFollowRouteHelper	m_followRoute;
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;
	Vector3						m_vLastTargetPosition;
	float						m_fPathLength;
	u32							m_iNumSearchFailures;
	u32							m_iOnRoadCheckTimer;
	bool						m_bWasOnRoad;
};

#endif
