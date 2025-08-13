#ifndef TASK_VEHICLE_PARK_H
#define TASK_VEHICLE_PARK_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/Scenario/ScenarioChainingTests.h"	// CParkingSpaceFreeFromObstructionsTestHelper
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "ai/AITarget.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"


//
//
//
class CTaskVehicleStop : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Stop
	} VehicleControlState;

	enum
	{
		SF_SupressBrakeLight = BIT(0),
		SF_DontTerminateWhenStopped = BIT(1),
		SF_DontResetSteerAngle = BIT(2),
		SF_UseFullBrake = BIT(3),
		SF_EnableTimeslicingWhenStill = BIT(4),
		SF_NumStopFlags = 5
	};

	// Constructor/destructor
	CTaskVehicleStop(u32 Stopflags = 0);
	~CTaskVehicleStop(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_STOP; }
	aiTask*			Copy() const {return rage_new CTaskVehicleStop(m_StopFlags);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Stop;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	FSM_Return		ProcessPreFSM();

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleStop>; }
    virtual void CloneUpdate(CVehicle *pVehicle);
	virtual void CleanUp();

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, m_StopFlags, SF_NumStopFlags, "Stop flags");
	}

	float ComputeThrottleForSpeed(CVehicle& in_Vehicle, float in_Speed);

private:

	// FSM function implementations
	//State_Stop
	FSM_Return		Stop_OnUpdate					(CVehicle* pVehicle);

	u32			m_StopFlags;
};

//
//
//
class CTaskVehiclePullOver : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Cruise,
		State_HeadForTarget,
		State_Slowdown,
		State_Stop,
	} VehicleControlState;

	typedef enum
	{
		Force_NoPreference,
		Force_Left,
		Force_Right,
	} ForcePulloverDirection;

	// Constructor/destructor
	CTaskVehiclePullOver(bool bForcePullOverRightAway = false, ForcePulloverDirection forceDirection = Force_NoPreference
		, bool bJustPullOverToSideOfCurrentLane = false);
	~CTaskVehiclePullOver()
	{
		delete m_pFollowRoute;
	}

	virtual void		CleanUp();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PULL_OVER; }
	aiTask*			Copy() const {return rage_new CTaskVehiclePullOver(m_bForcePullOverRightAway, m_ForcePulloverInDirection, m_bJustPullOverToSideOfCurrentLane);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Cruise;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	float			GetPointToAimFor(CVehicle *pVehicle, Vector3 *pPointToAimFor, float& fDotProductOut);

	FSM_Return		ProcessPreFSM();

	virtual CVehicleNodeList * GetNodeList()				{ return GetState() == State_Cruise ? NULL : &m_NodeList;}
	virtual const CVehicleFollowRouteHelper* GetFollowRouteHelper() const { return GetState() == State_Cruise ? NULL : m_pFollowRoute;}
	virtual void CopyNodeList(const CVehicleNodeList * pNodeList);

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePullOver>; }

private:

	// FSM function implementations
	//Cruise
	void		Cruise_OnEnter(CVehicle* pVehicle);
	FSM_Return	Cruise_OnUpdate(CVehicle* pVehicle);

	// HeadForTarget
	void		HeadForTarget_OnEnter(CVehicle* pVehicle);
	FSM_Return	HeadForTarget_OnUpdate(CVehicle* pVehicle);

	//State_Slowdown
	void		Slowdown_OnEnter(CVehicle* pVehicle);
	FSM_Return	Slowdown_OnUpdate(CVehicle* pVehicle);

	//State_Stop
	void		Stop_OnEnter(CVehicle* pVehicle);
	FSM_Return	Stop_OnUpdate(CVehicle* pVehicle);

	void		UpdateIndicatorsForCar(CVehicle* pVeh);
	bool		HeadingIsWithinToleranceForVehicle(CVehicle* pVehicle, float fDot) const;

	CVehicleNodeList m_NodeList;
	CVehicleFollowRouteHelper* m_pFollowRoute;

	ForcePulloverDirection	m_ForcePulloverInDirection;
	bool m_bRightSideOfRoad;
	bool m_bFirstUpdateInCruise;
	bool m_bForcePullOverRightAway;
	bool m_bJustPullOverToSideOfCurrentLane;
};

class CTaskVehiclePassengerExit : public CTaskVehiclePullOver
{
public:	
	typedef enum
	{
		State_GoToTarget,
		State_Stop,
		State_WaitForPassengersToExit,
	} PassengerExitState;

	CTaskVehiclePassengerExit(const Vector3& target);
	~CTaskVehiclePassengerExit() {}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PASSENGER_EXIT; }
	aiTask* Copy() const { return rage_new CTaskVehiclePassengerExit(m_vTarget); }

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const { return State_Stop; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL
	
	FSM_Return		ProcessPreFSM();

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehiclePassengerExit>; }

private:
	void				GoToTarget_OnEnter(CVehicle* pVehicle);
	FSM_Return	GoToTarget_OnUpdate(CVehicle* pVehicle);

	void				Stop_OnEnter(CVehicle* pVehicle);
	FSM_Return	Stop_OnUpdate(CVehicle* pVehicle);

	void				WaitForPassengersToExit_OnEnter(CVehicle* pVehicle);
	FSM_Return  WaitForPassengersToExit_OnUpdate(CVehicle* pVehicle);

	CScenarioPoint* GetDriverScenarioPoint(const CVehicle* pVehicle) const;

	void  GivePedExitTask(CPed* pPed, CVehicle* pVehicle) const;

	atArray<RegdPed> m_exitingPeds;
	Vector3 m_vTarget;
	u32 m_uNumPassengersToExit;
};

class CTaskVehicleParkNew : public CTaskVehicleMissionBase
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	If tests are enabled to see if the parking space is blocked,
		//			this is the number of seconds we wait until we do the next test.
		float m_ParkingSpaceBlockedWaitTimePerAttempt;

		// PURPOSE:	If tests are enabled to see if the parking space is blocked,
		//			this is the number of times we wait for the parking space
		//			to become unblocked before failing the task.
		// NOTES:	For example, if this is set to 2 and m_ParkingSpaceBlockedWaitTimePerAttempt
		//			is 5.0 s, we can spend 10.0 seconds waiting before giving up. There
		//			can however be 3 tests done, since we would do one final test before
		//			driving off after the last wait.
		u8 m_ParkingSpaceBlockedMaxAttempts;

		PAR_PARSABLE;
	};

	typedef enum
	{
		State_Invalid = -1,
		State_Start,
		State_Navigate,
		State_ForwardIntoSpace,
		State_BackIntoSpace,
		State_BackAwayFromSpace,
		State_PullForward,		//before backing in, pull up in front of the space
		State_PullOver,
		State_PassengerExit,
		State_Stop,
		State_Failed			// Parking spot blocked, or similar failure causing us to give up
	} VehicleControlState;

	typedef enum
	{
		Park_Parallel,
		Park_Perpendicular_NoseIn,
		Park_Perpendicular_BackIn,
		Park_PullOver,
		Park_LeaveParallelSpace,
		Park_BackOutPerpendicularSpace,
		Park_PassengerExit,
		Park_PullOverImmediate,
	} ParkType;

	CTaskVehicleParkNew(const sVehicleMissionParams& params, const Vector3& direction = VEC3_ZERO, const ParkType parkType = Park_Parallel, const float toleranceRadians = 0.0f, const bool bKeepLightsOn=false);
	~CTaskVehicleParkNew();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PARK_NEW; }
	aiTask*			Copy() const;

	bool			GetDoTestsForBlockedSpace() const { return m_bDoTestsForBlockedSpace; }
	void			SetDoTestsForBlockedSpace(bool b) { m_bDoTestsForBlockedSpace = b; }

	int				GetMaxPathSearchDistance() const { return m_iMaxPathSearchDistance; }
	void			SetMaxPathSearchDistance(int dist) { m_iMaxPathSearchDistance = dist; }

	// FSM implementations
	FSM_Return	ProcessPreFSM							();
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

	virtual void	Debug() const;
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleParkNew>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_DIRECTION = 10;
		static const unsigned SIZEOF_PARK_TYPE = 3;
		static const unsigned SIZEOF_HEADING = 8;

		// only the target is used by the task. Serialising everything will exceed the max task size
		CTaskVehicleMissionBase::SerialiseTarget(serialiser);

		bool hasDir = m_SpaceDir.x != 0.0f || m_SpaceDir.y != 0.0f || m_SpaceDir.z != 0.0f;

		SERIALISE_BOOL(serialiser, hasDir);

		if (hasDir || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_SpaceDir.x, 1.01f, SIZEOF_DIRECTION, "Direction X");
			SERIALISE_PACKED_FLOAT(serialiser, m_SpaceDir.y, 1.01f, SIZEOF_DIRECTION, "Direction Y");
			SERIALISE_PACKED_FLOAT(serialiser, m_SpaceDir.z, 1.01f, SIZEOF_DIRECTION, "Direction Z");

			m_SpaceDir.Normalize();
		}
		else
		{
			m_SpaceDir = VEC3_ZERO;
		}

		unsigned parkType = (unsigned)m_ParkType;

		SERIALISE_UNSIGNED(serialiser, parkType, SIZEOF_PARK_TYPE, "Park Type");
		SERIALISE_PACKED_FLOAT(serialiser, m_HeadingToleranceRadians, TWO_PI, SIZEOF_HEADING, "Heading tolerance");

		m_ParkType = (ParkType) parkType;
	}

private:

	//Start
	void		Start_OnEnter(CVehicle* pVehicle);
	FSM_Return	Start_OnUpdate(CVehicle* pVehicle);

	// Navigate
	void		Navigate_OnEnter(CVehicle* pVehicle);
	FSM_Return	Navigate_OnUpdate(CVehicle* pVehicle);

	//ForwardIntoSpace
	void		ForwardIntoSpace_OnEnter(CVehicle* pVehicle);
	FSM_Return	ForwardIntoSpace_OnUpdate(CVehicle* pVehicle);

	//BackIntoSpace
	void		BackIntoSpace_OnEnter(CVehicle* pVehicle);
	FSM_Return	BackIntoSpace_OnUpdate(CVehicle* pVehicle);

	//BackAwayFromSpace
	void		BackAwayFromSpace_OnEnter(CVehicle* pVehicle);
	FSM_Return	BackAwayFromSpace_OnUpdate(CVehicle* pVehicle);

	//PullForwardParallel
	void		PullForward_OnEnter(CVehicle* pVehicle);
	FSM_Return	PullForward_OnUpdate(CVehicle* pVehicle);

	//PullOver
	void		PullOver_OnEnter(CVehicle* pVehicle);
	FSM_Return	PullOver_OnUpdate(CVehicle* pVehicle);

	//PassengerExit
	void				PassengerExit_OnEnter(CVehicle* pVehicle);
	FSM_Return	PassengerExit_OnUpdate(CVehicle* pVehicle);

	//Stop
	void		Stop_OnEnter(CVehicle* pVehicle);
	FSM_Return	Stop_OnUpdate(CVehicle* pVehicle);

	//Failed
	void		Failed_OnEnter();
	FSM_Return	Failed_OnUpdate();

	bool IsReadyToQuit(CVehicle* pVehicle) const;
	void FixUpParkingSpaceDirection(CVehicle* pVehicle);
	void HelperGetParkingSpaceFrontPosition(Vector3& vPositionOut) const;
	void HelperGetParkingSpaceRearPosition(Vector3& vPositionOut) const;
	void HelperGetPositionToSteerTo(CVehicle* pVehicle, Vector3& vPositionOut) const;
	void HelperGetBackAwayFromSpacePosition(CVehicle* pVehicle, Vector3& vPositionOut) const;
	bool HelperIsAnythingBlockingEntryToSpace(CVehicle* pVehicle) const;
	bool HelperIsVehicleStopped(CVehicle* pVehicle) const;
	bool HelperUseReverseSpace() const;
	bool HelperUseRearBonnetPos() const;
	bool IsCurrentHeadingWithinTolerance(CVehicle* pVehicle) const;
	bool IsCurrentPositionWithinTolerance(CVehicle* pVehicle) const;
	bool IsCurrentPositionWithinXTolerance(CVehicle* pVehicle) const;
	bool IsCurrentPositionWithinYTolerance(CVehicle* pVehicle) const;
	float GetDistanceInFrontOfSpace(CVehicle* pVehicle) const;
	float GetCruiseSpeedForDistanceToTarget(CVehicle* pVehicle) const;

	static dev_float ms_fCruiseSpeedToPark;
	static dev_float ms_fCruiseSpeedToParkBoat;
	static dev_float ms_fParkingSpaceHalfLength;
	static dev_float ms_fParkingSpaceHalfWidth;
	static dev_float ms_fPositionXTolerance;
	static dev_float ms_fPositionYTolerance;
	static dev_float ms_fPositionXToleranceBike;
	static dev_float ms_fPositionYToleranceBike;
	static dev_float ms_fPositionXTolerancePlane;
	static dev_float ms_fPositionYTolerancePlane;
	static dev_u16 ms_iParkingStuckTime;

	// PURPOSE:	Helper object used to do checks to see if the parking space
	//			is blocked by something.
	CParkingSpaceFreeFromObstructionsTestHelper	m_SpaceFreeFromObstructionsHelper;

	//we don't necessarily care about +/- direction, we may flip it
	//in the constructor based on the type of parking that the user 
	//has specified and the space's direction relative to the car's
	Vector3		m_SpaceDir;

	float		m_HeadingToleranceRadians;
	ParkType	m_ParkType;

	// PURPOSE:	Number of times the intended parking space has been sequentially
	//			found to be blocked, when testing.
	u8			m_NumTimesParkingSpaceBlocked;	

	// PURPOSE:	Max distance for how far we can search for a path on roads leading to
	//			the parking space, or -1 for no limit at this level.
	int			m_iMaxPathSearchDistance;

	// PURPOSE:	If true, let the task do tests to see if the parking space is blocked,
	//			and potentially let it give up if the space has been blocked for enough time.
	bool		m_bDoTestsForBlockedSpace;

	bool		m_bKeepLightsOnAfterParking;

	// PURPOSE:	Static tuning data for this task.
	static Tunables	sm_Tunables;
};

//may pull over, may just wait
class CTaskVehicleReactToCopSiren : public CTaskVehicleMissionBase
{
public:

	typedef enum
	{
		React_PullOverLeft,
		React_PullOverRight,
		React_JustWait,
		React_NumReactions
	} ReactToCopSirenType;

	typedef enum
	{
		State_Invalid = -1,
		State_Start,
		State_Pullover,
		State_Wait,
	} VehicleControlState;

	CTaskVehicleReactToCopSiren(ReactToCopSirenType reactionType = React_JustWait, u32 nWaitDuration = 0);
	~CTaskVehicleReactToCopSiren() {};

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_REACT_TO_COP_SIREN; }
	aiTask*			Copy() const {return rage_new CTaskVehicleReactToCopSiren(m_ReactionType, m_nWaitDuration);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Start;}
	virtual	void	CleanUp();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleReactToCopSiren>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static unsigned const SIZE_OF_REACTION_TYPE = datBitsNeeded<React_NumReactions-1>::COUNT;
		static unsigned const SIZE_OF_DURATION = 16;

		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_ReactionType), SIZE_OF_REACTION_TYPE, "Reaction type");
		SERIALISE_UNSIGNED(serialiser, m_nWaitDuration, SIZE_OF_DURATION, "Wait duration");
	}

private:

	//State_Start
	FSM_Return	Start_OnUpdate(CVehicle* pVehicle);

	//State_Pullover
	void		Pullover_OnEnter(CVehicle* pVehicle);
	FSM_Return	Pullover_OnUpdate(CVehicle* pVehicle);

	//State_Wait
	void		Wait_OnEnter(CVehicle* pVehicle);
	FSM_Return	Wait_OnUpdate(CVehicle* pVehicle);

	//this task prevents the vehicle from going pretend-occupant,
	//when it's done, restores the previous value
	ReactToCopSirenType m_ReactionType;
	u32					m_nWaitDuration;
	bool	m_bPreviousDisablePretendOccupantsCached;
};

#endif
