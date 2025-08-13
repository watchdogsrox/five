#ifndef TASK_VEHICLE_CRUISE_H
#define TASK_VEHICLE_CRUISE_H

// Gta headers.
#include "data/bitfield.h"
#include "ai/ExpensiveProcess.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "task/System/Tuning.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGotoAutomobile.h"

class CEvent;
class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"
#include "fwMaths/random.h"

//
//
//Attempts to flee a target
class CTaskVehicleFlee : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_Flee,
		State_Swerve,
		State_Hesitate,
		State_Cruise,
		State_SkidToStop,
		State_Pause,
		State_Stop,
		State_StopAndWait
	} VehicleControlState;
	
	enum ConfigFlags
	{
		CF_SkidToStop					= BIT0,
		CF_Swerve						= BIT1,
		CF_Hesitate						= BIT2,
		CF_HesitateAfterSkidToStop		= BIT3,
		CF_HesitateUntilTargetGetsUp	= BIT4,
	};
	
	enum VehicleFleeRunningFlags
	{
		VFRF_HasSkiddedToStop	= BIT0,
		VFRF_SkidLeft			= BIT1,
		VFRF_SkidStraight		= BIT2,
		VFRF_SkidRight			= BIT3,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_ChancesForSwerve;
		float m_MinSpeedForSwerve;
		float m_MinTimeToSwerve;
		float m_MaxTimeToSwerve;
		float m_ChancesForHesitate;
		float m_MaxSpeedForHesitate;
		float m_MinTimeToHesitate;
		float m_MaxTimeToHesitate;
		float m_ChancesForJitter;
		float m_MinTimeToJitter;
		float m_MaxTimeToJitter;

		PAR_PARSABLE;
	};

public:

	// Constructor/destructor
	CTaskVehicleFlee(const sVehicleMissionParams& params, const fwFlags8 uConfigFlags = 0, float fFleeIntensity = 1.);
	~CTaskVehicleFlee(){}

public:

	static bool ShouldHesitateForEvent(const CPed* pPed, const CEvent& rEvent);
	static bool ShouldSwerveForEvent(const CPed* pPed, const CEvent& rEvent);

public:

	void				SetFleeIntensity(float fIntensity) { m_fFleeIntensity = fIntensity; }
	int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FLEE; }
	aiTask*				Copy() const {return rage_new CTaskVehicleFlee(m_Params, m_uConfigFlags, m_fFleeIntensity);}

	// FSM implementations
	FSM_Return			ProcessPreFSM						();
	FSM_Return			UpdateFSM							(const s32 iState, const FSM_Event iEvent);

	s32				GetDefaultStateAfterAbort			()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName					( s32 iState );
#endif //!__FINAL

	// network 
	void CloneUpdate(CVehicle* pVehicle);
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleFlee>; }

	static void ResetStatics();
	static void SwapFirstTwoNodesIfPossible(CVehicleNodeList* pNodeList, const Vector3& vSearchPosition, const Vector3& vInitialVelocity, const Vector3& vTargetPos, const bool bMissionVeh);
	static bool WillBarrelThroughInsteadOfUTurn(const Vector3& vSearchPosition, const Vector3& vTargetPos, const Vector3& vInitialVelocity, const bool bMissionVeh);
	void		PreventStopAndWait();

	static const float ms_fSpeedBelowWhichToBailFromVehicle;

protected:
	//State Start
	FSM_Return			Start_OnUpdate	(CVehicle* pVehicle);
	//State Flee
	FSM_Return			Flee_OnUpdate	(CVehicle* pVehicle);
	void				Flee_OnExit();
	//State Swerve
	void				Swerve_OnEnter	();
	FSM_Return			Swerve_OnUpdate	();
	//State Hesitate
	void				Hesitate_OnEnter	();
	FSM_Return			Hesitate_OnUpdate	();
	//State Cruise
	void				Cruise_OnEnter	(CVehicle* pVehicle);
	FSM_Return			Cruise_OnUpdate	(CVehicle* pVehicle);
	//State SkidToStop
	void				SkidToStop_OnEnter	();
	FSM_Return			SkidToStop_OnUpdate	();
	//State Stop
	void				Stop_OnEnter	(CVehicle* pVehicle);
	FSM_Return			Stop_OnUpdate	(CVehicle* pVehicle);
	//State StopAndWait
	void				StopAndWait_OnEnter	(CVehicle* pVehicle);
	FSM_Return			StopAndWait_OnUpdate(CVehicle* pVehicle);
	void				StopAndWait_OnExit	(CVehicle* pVehicle);
	//State Pause
	void				Pause_OnEnter	();
	FSM_Return			Pause_OnUpdate	();
	
	bool CheckShouldSkidToStop();
	void HonkHorn();
	bool IsTargetGettingUp() const;
	bool ShouldHonkHorn() const;
	bool ShouldSwerveRight() const;
	void UpdateCruiseSpeed();
	bool ShouldJitter() const;
	bool ShouldWaitForAccident(CVehicle* pVehicle, Vec3V_InOut wreckCenter, int& numWrecks);
	bool IsEntityWithinLink(CVehicle& wreck, CPathNode* pFirstNode, CPathNode* pSecondNode);

private:

	u32			m_uLastTimeTargetGettingUp;
	fwFlags8	m_uConfigFlags;
	fwFlags8	m_uRunningFlags;
	float		m_fFleeIntensity;
	float		m_fMaxCruiseSpeed;
	float		m_fStopRecheckTimer;
	bool		m_bPreventStopAndWait;

private:

	static Tunables sm_Tunables;
	static u32 sm_uLastHonkTime;
	static u32 sm_uTimeBetweenHonks;
	
};

//Overridden goto automobile task, this will pick nodes to allow a car to wander around with no target
class CTaskVehicleCruiseNew : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_FindRoute = 0,
		State_CalculatingRoute,
		State_Cruise,
		State_StopForJunction,
		State_WaitBeforeMovingAgain,
		State_GoToStraightLine,
		State_GoToNavmesh,
		State_GetToRoadUsingNavmesh,
		State_Burnout,
		State_RevEngine,
		State_PauseForUnloadedNodes,
		State_Stop
	} CruiseState;
	
private:

	enum RunningFlags
	{
		RF_IsUnableToFindRoute	= BIT0,
		RF_ForceRouteUpdate		= BIT1,
		RF_IsUnableToGetToRoad  = BIT2,
	};

public:

	static const unsigned int LINK_NUMBITS = 5;
	static const unsigned int LANE_NUMBITS = 3;
	typedef union linkandlane
	{
		u8 m_linkAndLane;

		struct  
		{
			u8 DECLARE_BITFIELD_2(
				m_link,LINK_NUMBITS,
				m_lane,LANE_NUMBITS
				);
		} m_1;
	} LinkAndLane;

	CTaskVehicleCruiseNew(const sVehicleMissionParams& params, const bool bSpeedComesFromVehPopulation = false);
	virtual ~CTaskVehicleCruiseNew();

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_CRUISE_NEW; }
	aiTask*		Copy() const;


	// FSM implementations
	FSM_Return	ProcessPreFSM						();
	FSM_Return	ProcessPreFSM_Shared				();
	FSM_Return	UpdateFSM							(const s32 iState, const FSM_Event iEvent);

	virtual void		CleanUp();

	s32			GetDefaultStateAfterAbort			()	const {return State_FindRoute;}

	virtual CVehicleNodeList * GetNodeList()				{ return m_pRouteSearchHelper ? &m_pRouteSearchHelper->GetNodeList() : NULL; }
	virtual void		CopyNodeList(const CVehicleNodeList * pNodeList);

	// PURPOSE:	Make a copy of the supplied node list for use as a preference for target node selection.
	// PARAMS:	listToCopyFrom	- Node list to copy, or NULL to clear.
	void SetPreferredTargetNodeList(const CVehicleNodeList* listToCopyFrom);
	
	virtual const CVehicleFollowRouteHelper* GetFollowRouteHelper() const { return (const CVehicleFollowRouteHelper*)&m_followRoute; }
	virtual const CPathNodeRouteSearchHelper* GetRouteSearchHelper() const { return m_pRouteSearchHelper; }

	void FixUpPathForTemplatedJunction(const u32 iForceDirectionAtJunction);
	void ConstructDefaultFollowRouteFromNodeList();

	virtual void DoSoftReset();

	void UpdateEmptyLaneAwareness();
	bool AttemptLaneChange(const float fMinimumOvertakeSpeed, const float fForwardSearchDistance, const float fReverseSearchDistance);
	//if bFailifLaneAlreadyReserved is true, this function will return false
	//if the vehicle already has a required lane set
	bool RequestImmediateLaneChange(int iLane, bool bReverseFirst, bool bFailIfLaneAlreadyReserved);

	// network 
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleCruiseNew>; }

	static const unsigned int NUM_NODESSYNCED = 2; //sending first node, then info to link to two more
	static const unsigned int SIZEOF_LANEANDLINK = 8;
	static const unsigned int SIZEOF_NODEADDRESS = 32;
	void SerialiseMigrationData(CSyncDataBase& serialiser)
	{
		CTaskVehicleMissionBase::SerialiseMigrationData(serialiser);

		//sync future nodes
		//Should total 48 bits. Have to sync first node fully (32 bits)
		//then array of link indexes (5 bits) and lane index (3 bits) packed into s8
		s32 iNodeRegionAndAddress;
		m_syncedNode.ToInt(iNodeRegionAndAddress);
		SERIALISE_INTEGER(serialiser, iNodeRegionAndAddress, SIZEOF_NODEADDRESS, "Future Node Address");
		m_syncedNode.FromInt(iNodeRegionAndAddress);
		for(int i = 0; i < NUM_NODESSYNCED; ++i )
		{
			SERIALISE_UNSIGNED(serialiser, m_syncedLinkAndLane[i].m_linkAndLane, SIZEOF_LANEANDLINK, "Future Lane index");
		}	
	}

	bool IsDoingStraightLineNavigation() const
	{
		return GetState() == State_GoToStraightLine;
	}
	
	bool IsUnableToFindRoute() const
	{
		return m_uRunningFlags.IsFlagSet(RF_IsUnableToFindRoute);
	}

	bool IsUnableToGetToRoad() const
	{
		return m_uRunningFlags.IsFlagSet(RF_IsUnableToGetToRoad);
	}

	int GetMaxPathSearchDistance() const { return m_iMaxPathSearchDistance; }
	void SetMaxPathSearchDistance(int dist) { m_iMaxPathSearchDistance = dist; }
	void SetStraightLineDist(float fDist) { m_fStraightLineDist = fDist; }

	void ForceRouteUpdate() { m_uRunningFlags.SetFlag(RF_ForceRouteUpdate); }

	bool DoesPathContainUTurn() const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );
	virtual void		Debug() const;
#endif //!__FINAL

    // network methods
	void		CloneUpdate(CVehicle *pVehicle);
    void        SetupAfterMigration(CVehicle *pVehicle);

	void EnsureHasRouteSearchHelper()
	{
		if (!m_pRouteSearchHelper)
		{
			m_pRouteSearchHelper = rage_new CPathNodeRouteSearchHelper();
		}
	}

	static bank_bool ms_bAllowCruiseTaskUseNavMesh;

protected:
	// State implementation
	FSM_Return FindRoute_OnEnter();
	FSM_Return FindRoute_OnUpdate();
	// State implementation
	FSM_Return Cruise_OnEnter();
	FSM_Return Cruise_OnUpdate();
	FSM_Return Cruise_OnExit();
	//State_WaitBeforeMovingAgain
	FSM_Return WaitBeforeMovingAgain_OnEnter();
	FSM_Return WaitBeforeMovingAgain_OnUpdate();
	//State_StopForJunction
	FSM_Return StopForJunction_OnEnter();
	FSM_Return StopForJunction_OnUpdate();
	//State_GoToStraightLine
	FSM_Return GotoStraightLine_OnEnter();
	FSM_Return GotoStraightLine_OnUpdate();
	FSM_Return GotoStraightLine_OnExit();
	//State_Stop
	FSM_Return Stop_OnEnter();
	FSM_Return Stop_OnUpdate();
	//State_GoToNavmesh
	FSM_Return GotoNavmesh_OnEnter();
	FSM_Return GotoNavmesh_OnUpdate();
	FSM_Return GotoNavmesh_OnExit();
	//State_GetToRoadUsingNavmesh
	FSM_Return GetToRoadUsingNavmesh_OnEnter();
	FSM_Return GetToRoadUsingNavmesh_OnUpdate();
	FSM_Return GetToRoadUsingNavmesh_OnExit();

	FSM_Return Navmesh_Shared_OnEnter();
	FSM_Return Navmesh_Shared_OnUpdate(Vector3& vTargetPos, CruiseState desiredNextState, const float fDistToAllowSwitchToStraightLineRoute);
	FSM_Return Navmesh_Shared_OnExit();

	//State_Burnout
	FSM_Return Burnout_OnEnter();
	FSM_Return Burnout_OnUpdate();
	//State_RevEngine
	FSM_Return RevEngine_OnEnter();
	FSM_Return RevEngine_OnUpdate();
	FSM_Return RevEngine_OnExit();
	//State_PauseForUnloadedNodes
	FSM_Return PauseForUnloadedNodes_OnEnter();
	FSM_Return PauseForUnloadedNodes_OnUpdate();

	bool UpdateNavmeshLOSTestForCruising(const Vector3& vStartPos, const bool bForceLOSTestThisFrame);
	bool ShouldFindNewRoute(const float fDistToTheEndOfRoute) const;
	bool UpdateShouldFindNewRoute(const float fDistToTheEndOfRoute, const bool bAllowStateChanges, const bool bIsOnStraightLineUpdate);
	void AddAdditionalSearchFlags(const CVehicle* pVehicle, u32& iSearchFlags) const;
	bool GetTargetPositionForGetToRoadUsingNavmesh(Vector3::Ref vTargetPos) const;

	bool UpdateShouldRevEngine();
	bool IsThereSomethingAroundToRevEngineAt(const bool bWillRevAtHotPeds, const bool bWillRevAtOtherCars) const;

	bool DoesSubtaskThinkWeShouldBeStopped(const CVehicle* pVehicle, Vector3& vTargetPosOut, const bool bCalledFromWithinAiUpdate) const;

	//work out whether we should have the indicators on depending on the up and comming node direction
	void		UpdateIndicatorsForCar				(CVehicle* pVeh);

	virtual u32 GetDefaultPathSearchFlags() const
	{
		u32 retVal = CPathNodeRouteSearchHelper::Flag_JoinRoadInDirection|CPathNodeRouteSearchHelper::Flag_WanderRoute;
		return retVal;
	}

	// Calculates the position the vehicle should be driving towards and the speed.
	// Returns the distance to the end of the route
	float FindTargetPositionAndCapSpeed( CVehicle* pVehicle, const Vector3& vStartCoords, Vector3& vTargetPosition
		, float& fOutSpeed, const bool bMustReturnActualDistToEndOfPath);

	virtual bool		CheckForStateChangeDueToDistanceToTarget(CVehicle* /*pVehicle*/) { return false; };//this can be overridden if we want to ignore the targetpos.
	virtual bool		ShouldFallBackOnWanderInsteadOfStraightLine() const { return false; }
	virtual const CVehicleNodeList* GetPreferredTargetNodeList() const { return m_PreferredTargetNodeList; }
	void				UpdateUsingWanderFallback();

	bool CanUseNavmeshToReachPoint(const CVehicle* pVehicle, const Vector3* pvAltStartCoords = NULL, const Vector3* pvAltEndCoords=NULL) const;
	bool IsEscorting() const;

	void ProcessSuperDummyHelper(CVehicle* pVehicle, const float fBackupSpeed) const; 
	void ProcessRoofConvertibleHelper(CVehicle* pVehicle, const bool bStoppedAtRedLight) const;
	void ProcessTimeslicingAllowedWhenStopped(); 
	void UpdateSpeedMultiplier(CVehicle* pVehicle) const;
	void SetNextTimeAllowedToReplan(CVehicle* pVehicle, const s32 iOverrideTime = 0);
	int	GetMinTimeToFindNewPathAfterSearch(const CVehicle* pVehicle) const;
	void CacheMigrationData();

	// PURPOSE:	Optional allocated copy of a node list that will be used as a preference
	//			for the purposes of finding a target node near the destination.
	CVehicleNodeList*	m_PreferredTargetNodeList;


	CPathNodeRouteSearchHelper*			m_pRouteSearchHelper;
	RegdVeh								m_pCarWeWereBehindOnLastLaneChangeAttempt;
	CVehicleFollowRouteHelper			m_followRoute;
	CVehicleJunctionHelper				m_JunctionHelper;
	CVehicleShockingEventHelper			m_ShockingEventHelper;
	CNavmeshLineOfSightHelper			m_NavmeshLOSHelper;
	CVehicleLaneChangeHelper			m_LaneChangeHelper;
	CExpensiveProcess					m_AdverseConditionsDistributer;
	CVehicleConvertibleRoofHelper		m_RoofHelper;
	float								m_fPathLength;
	float								m_fPickedCruiseSpeedWithVehModel;
	float								m_fStraightLineDist;
	s32									m_iMaxPathSearchDistance;	// Rough limit to how long path we can search for, or -1 for no limit.
	u32									m_uTimeLastNotDoingStraightLineNav;
	u32									m_uTimeStartedUsingWanderFallback;
	u32									m_uNextTimeAllowedToRevEngine;
	u32									m_uLastTimeUpdatedIndicators;
	u32									m_uNextTimeAllowedToReplan;
	u32									m_uNextTimeAllowedToChangeLanes;
	fwFlags8							m_uRunningFlags;
	u8									m_nTimesFailedResettingTarget;
	LinkAndLane							m_syncedLinkAndLane[NUM_NODESSYNCED];
	CNodeAddress						m_syncedNode;
	bool								m_bFindingNewRoute;
	bool								m_bNeedToPickCruiseSpeedWithVehModel;
	bool								m_bLeftIndicator;
	bool								m_bRightIndicator;
	bool								m_bHasComeToStop;		// Used in certain states to keep track of we have come to a halt.
	bool								m_bSpeedComesFromVehPopulation;
#if __ASSERT
	bool								m_bDontAssertIfTargetInvalid;	// Actually only used by CTaskVehicleGoToAutomobileNew, but located here to save memory.
#endif
};

class CTaskVehicleGoToAutomobileNew : public CTaskVehicleCruiseNew
{
public:
	CTaskVehicleGoToAutomobileNew(const sVehicleMissionParams& params
		,	const float fStraightLineDist = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST
		,	const bool bSpeedComesFromVehPopulation = false);

	virtual ~CTaskVehicleGoToAutomobileNew();

#if __ASSERT
	void SetDontAssertIfTargetInvalid(bool bValue) { m_bDontAssertIfTargetInvalid = bValue; }
#endif

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW; }

	aiTask* Copy() const
	{
		CTaskVehicleGoToAutomobileNew* pTask = rage_new CTaskVehicleGoToAutomobileNew(m_Params, m_fStraightLineDist);
#if __ASSERT
		pTask->SetDontAssertIfTargetInvalid(m_bDontAssertIfTargetInvalid);
#endif
		if(pTask)
		{
			pTask->SetMaxPathSearchDistance(GetMaxPathSearchDistance());
			pTask->SetPreferredTargetNodeList(m_PreferredTargetNodeList);
		}
		return pTask;
	}

	FSM_Return	ProcessPreFSM						();

#if !__FINAL
	virtual void		Debug() const;
#endif //!__FINAL

private:
	virtual u32			GetDefaultPathSearchFlags() const;
	virtual bool		CheckForStateChangeDueToDistanceToTarget(CVehicle *pVehicle);//this can be overridden if we want to ignore the targetpos.
	virtual bool		ShouldFallBackOnWanderInsteadOfStraightLine() const;
};

#endif
