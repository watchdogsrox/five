#ifndef TASK_VEHICLE_TEMP_ACTION_H
#define TASK_VEHICLE_TEMP_ACTION_H

//Rage headers
#include "fwutil/Flags.h"
#include "Vector/Vector3.h"

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"

class CVehicle;
class CPlane;
class CVehControls;
class CSubmarine;


//
//
//
class CTaskVehicleTempAction : public CTaskVehicleMissionBase
{
public:
	// Constructor/destructor

	CTaskVehicleTempAction(u32 tempActionFinishTimeMs)
	  : CTaskVehicleMissionBase()
		, m_tempActionFinishTimeMs(tempActionFinishTimeMs)
		, m_bAllowSuperDummyForThisTask(false)
		{
		}
		
	virtual ~CTaskVehicleTempAction(){}

	FSM_Return	ProcessPreFSM						();
	u32	GetTempActionFinishTime(){return m_tempActionFinishTimeMs;}

	virtual bool IsSyncedAcrossNetwork() const { return false; }
	virtual bool IsIgnoredByNetwork() const { return true; }

	template <class Serialiser> 
	void Serialise(Serialiser &serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, m_tempActionFinishTimeMs, sizeof(m_tempActionFinishTimeMs)<<3, "Finish time");
	}

protected:

	u32	m_tempActionFinishTimeMs;
	bool m_bAllowSuperDummyForThisTask;
};

//
//
//
class CTaskVehicleWait : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Wait = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleWait(u32 tempActionFinishTimeMs):
		CTaskVehicleTempAction(tempActionFinishTimeMs)
	{
		m_Params.m_fCruiseSpeed = 0.0f;
		m_bAllowSuperDummyForThisTask = true;

		SetInternalTaskType(CTaskTypes::TASK_VEHICLE_WAIT);
	}
	~CTaskVehicleWait(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_WAIT; }
	CTask*			Copy() const {return rage_new CTaskVehicleWait(m_tempActionFinishTimeMs);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Wait;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	u32			GetTempActionFinishTime(){return m_tempActionFinishTimeMs;}
private:

	// FSM function implementations
	FSM_Return		Wait_OnUpdate(CVehicle* pVehicle);
};

//
//
//
class CTaskVehicleReverse : public CTaskVehicleTempAction
{
public:

	enum ConfigFlags
	{
		CF_QuitIfVehicleCollisionDetected	= BIT0,
		CF_RandomizeThrottle				= BIT1,
	};

public:
	typedef enum
	{
		State_Invalid = -1,
		State_Reverse = 0,
		State_Exit
	} VehicleControlState;

	typedef enum
	{
		Reverse_Opposite_Direction,
		Reverse_Straight,
		Reverse_Left,
		Reverse_Right,
		Reverse_Straight_Hard,
	} ReverseType;

	// Constructor/destructor
	CTaskVehicleReverse(u32 tempActionFinishTimeMs, ReverseType reverseType, fwFlags8 uConfigFlags = 0):
		CTaskVehicleTempAction(tempActionFinishTimeMs),
		m_ReverseType(reverseType),
		m_uConfigFlags(uConfigFlags)
	{
		SetInternalTaskType(CTaskTypes::TASK_VEHICLE_REVERSE);
	}
	~CTaskVehicleReverse(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_REVERSE; }
	CTask*			Copy() const {return rage_new CTaskVehicleReverse(m_tempActionFinishTimeMs, m_ReverseType, m_uConfigFlags);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Reverse;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	void			Reverse_OnEnter(CVehicle* pVehicle);
	FSM_Return		Reverse_OnUpdate(CVehicle* pVehicle);

	ReverseType m_ReverseType;
	fwFlags8	m_uConfigFlags;
};

//
//
//
class CTaskVehicleBrake : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Brake = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleBrake(u32 tempActionFinishTimeMs = 0): // 0 is used by the network code to create an empty task that is serialised
		CTaskVehicleTempAction(tempActionFinishTimeMs)
	{
		m_bDontResetSteerAngle = false;
		m_bUseHandBrake = false;
		m_bUseReverse = false;
		m_Params.m_fCruiseSpeed = 0.0f;
		m_bAllowSuperDummyForThisTask = true;

		SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BRAKE);
	}
	~CTaskVehicleBrake(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BRAKE; }
	CTask*			Copy() const 
	{
		CTaskVehicleBrake* pTask = rage_new CTaskVehicleBrake(m_tempActionFinishTimeMs);
		pTask->SetDontResetSteerAngle(m_bDontResetSteerAngle);
		pTask->SetUseHandBrake(m_bUseHandBrake);
		pTask->SetUseReverse(m_bUseReverse);
		return pTask;
	}

	void		SetDontResetSteerAngle(bool bDontReset) {m_bDontResetSteerAngle = bDontReset;}
	bool		GetDontResetSteerAngle() const {return m_bDontResetSteerAngle;}

	void		SetUseHandBrake(bool bValue) { m_bUseHandBrake = bValue; }

	void		SetUseReverse(bool bValue) { m_bUseReverse = bValue; }

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Brake;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	// network 
	virtual bool IsSyncedAcrossNetworkScript() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleBrake>; }

private:
	// FSM function implementations
	FSM_Return		Brake_OnUpdate(CVehicle* pVehicle);

	bool			m_bDontResetSteerAngle;
	bool			m_bUseHandBrake;
	bool			m_bUseReverse;
};


//
//
//
class CTaskVehicleHandBrake : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_HandBrake = 0,
		State_Exit
	} VehicleControlState;

	typedef enum
	{
		HandBrake_Straight = -1,
		HandBrake_Left = 0,
		HandBrake_Right,
		HandBrake_Straight_Intelligence,
		HandBrake_Left_Intelligence,
		HandBrake_Right_Intelligence,
	} HandBrakeAction;


	// Constructor/destructor
	CTaskVehicleHandBrake(u32 tempActionFinishTimeMs, HandBrakeAction handBrakeAction):
		CTaskVehicleTempAction(tempActionFinishTimeMs),
		m_HandBrakeAction(handBrakeAction)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_HANDBRAKE);
		}
	~CTaskVehicleHandBrake(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_HANDBRAKE; }
	CTask*			Copy() const {return rage_new CTaskVehicleHandBrake(m_tempActionFinishTimeMs, m_HandBrakeAction);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_HandBrake;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	FSM_Return		HandBrake_OnUpdate(CVehicle* pVehicle);

	HandBrakeAction	m_HandBrakeAction;
public:
	static float s_fMaxSpeedForHandbrake;
};


///////////////////////////////////////////////
//Make a vehicle halt in a very short distance
///////////////////////////////////////////////

class CTaskBringVehicleToHalt : public CTaskVehicleMissionBase
{
public:

	// If nTimeToStopFor is negative, then the vehicle will halt forever.
	CTaskBringVehicleToHalt(const float fStoppingDistance, const int nTimeToStopFor, const bool bControlVerticalVelocity=false);
	~CTaskBringVehicleToHalt();

	enum state
	{
		State_Start = 0,
		State_HaltingCar,
		State_HaltedAndWaiting,
		State_Finish
	};

	virtual CTask*	Copy() const {return rage_new CTaskBringVehicleToHalt(m_fStoppingDistance, m_nTimeToStopFor);}
	virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_BRING_VEHICLE_TO_HALT;}

	float GetStoppingDistance() const { return m_fStoppingDistance; }
	bool GetControlVerticalVelocity() const { return m_bControlVerticalVelocity; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }
    virtual bool IsIgnoredByNetwork() const { return true; }

	// FSM implementations
	FSM_Return		UpdateFSM( const s32 iState, const FSM_Event iEvent);
	virtual	void	CleanUp();

private:
	FSM_Return		Start_OnUpdate(CVehicle* pVehicle);

	FSM_Return		HaltingCar_OnEnter(CVehicle* pVehicle);
	FSM_Return		HaltingCar_OnUpdate(CVehicle* pVehicle);
	FSM_Return		HaltedAndWaiting_OnEnter(CVehicle* pVehicle);
	FSM_Return		HaltedAndWaiting_OnUpdate(CVehicle* pVehicle);
	Vector3			m_vInitialPos;
	Vector3			m_vInitialVel;
	float			m_fInitialSpeed;
	float			m_fBrakeAmount;
	float			m_fStoppingDistance;
	int				m_nTimeToStopFor;
	int				m_nWaitingTimeStart;
	bool			m_bStopped;
	bool			m_bControlVerticalVelocity;
};


//
//
//
class CTaskVehicleTurn : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Turn = 0,
		State_Exit
	} VehicleControlState;

	typedef enum
	{
		Turn_Left = -1,
		Turn_Right = 0,
	} TurnAction;

	// Constructor/destructor
	CTaskVehicleTurn(u32 tempActionFinishTimeMs, TurnAction turnAction):
		CTaskVehicleTempAction(tempActionFinishTimeMs),
		m_TurnAction(turnAction)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_TURN);
		}
	~CTaskVehicleTurn(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_TURN; }
	CTask*			Copy() const {return rage_new CTaskVehicleTurn(m_tempActionFinishTimeMs, m_TurnAction);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Turn;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	FSM_Return		Turn_OnUpdate(CVehicle* pVehicle);

	TurnAction		m_TurnAction;
};

//
//
//
class CTaskVehicleGoForward : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_GoForward = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleGoForward(u32 tempActionFinishTimeMs, bool bHard = false):
		CTaskVehicleTempAction(tempActionFinishTimeMs),
		m_bHard(bHard)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GO_FORWARD);
		}
	~CTaskVehicleGoForward(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_GO_FORWARD; }
	CTask*			Copy() const {return rage_new CTaskVehicleGoForward(m_tempActionFinishTimeMs, m_bHard);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_GoForward;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	void			GoForward_OnEnter(CVehicle* pVehicle);
	FSM_Return		GoForward_OnUpdate(CVehicle* pVehicle);

private:

	bool m_bHard;

};

//
//
//
class CTaskVehicleSwerve : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Swerve = 0,
		State_Brake,
		State_Wait,
		State_Exit
	} VehicleControlState;

	typedef enum
	{
		Swerve_Left = -1,
		Swerve_Right = 0,
		Swerve_Jitter
	} SwerveDirection;

	// Constructor/destructor
	CTaskVehicleSwerve(u32 tempActionFinishTimeMs, SwerveDirection swirveDirection, bool stopAfterSwirve
		, bool stopAfterCrossingEdge, Vector2 vEdgeNormal, Vector2 vEdgePoint):
		CTaskVehicleTempAction(tempActionFinishTimeMs),
		m_SwirveDirection(swirveDirection),
		m_StopAfterSwirve(stopAfterSwirve),
		m_StopAfterCrossingEdge(stopAfterCrossingEdge),
		m_vEdgeNormal(vEdgeNormal),
		m_vPointOnEdge(vEdgePoint)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_SWERVE);
		}
	~CTaskVehicleSwerve(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_SWERVE; }
	CTask*			Copy() const {return rage_new CTaskVehicleSwerve(m_tempActionFinishTimeMs
		, m_SwirveDirection, m_StopAfterSwirve, m_StopAfterCrossingEdge, m_vEdgeNormal, m_vPointOnEdge);}

	SwerveDirection GetSwerveDirection() const {return m_SwirveDirection;}
	void SetStopWhenCrossingRoadEdge(Vector2 vEdgeNormal, Vector2 vEdgePoint)
	{
		m_StopAfterCrossingEdge = true;
		m_vEdgeNormal = vEdgeNormal;
		m_vPointOnEdge = vEdgePoint;
	}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Swerve;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	FSM_Return		Swerve_OnUpdate(CVehicle* pVehicle);
	void			Wait_OnEnter(CVehicle* pVehicle);
	FSM_Return		Wait_OnUpdate(CVehicle* pVehicle);
	void			Brake_OnEnter(CVehicle* pVehicle);

	Vector2			m_vEdgeNormal;
	Vector2			m_vPointOnEdge;

	SwerveDirection	m_SwirveDirection;
	bool			m_StopAfterSwirve;
	bool			m_StopAfterCrossingEdge;
};

//
//
//
class CTaskVehicleFlyDirection : public CTaskVehicleTempAction
{
public:
	static const int AIUPDATEFREQ = 1000;

	typedef enum
	{
		State_Invalid = -1,
		State_FlyDirection = 0,
		State_Exit
	} VehicleControlState;

	typedef enum
	{
		Fly_Up,
		Fly_Straight,
		Fly_Left,
		Fly_Right,
	} FlyDirection;


	// Constructor/destructor
	CTaskVehicleFlyDirection(u32 tempActionFinishTimeMs, FlyDirection flyDirection):
		CTaskVehicleTempAction(tempActionFinishTimeMs),
		m_FlyDirection(flyDirection)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FLY_DIRECTION);
		}
	~CTaskVehicleFlyDirection(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FLY_DIRECTION; }
	CTask*			Copy() const {return rage_new CTaskVehicleFlyDirection(m_tempActionFinishTimeMs, m_FlyDirection);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_FlyDirection;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	FSM_Return		FlyDirection_OnUpdate(CVehicle* pVehicle);

	void			FlyAIPlaneInCertainDirection(CPlane *pPlane);
	float			FindFlightHeight(CPlane *pPlane, float Orientation);
	bool			FindHeightForVerticalAngle(CPlane *pPlane, float Angle, float Orientation, float *pResult);

	FlyDirection m_FlyDirection;
};

//
//
//
class CTaskVehicleHeadonCollision : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_HeadonCollision = 0,
		State_Exit
	} VehicleControlState;

	typedef enum
	{
		Swerve_NoPreference,
		Swerve_Left,
		Swerve_Right,
		Swerve_Straight,
	} SwerveDirection;

	// Constructor/destructor
	CTaskVehicleHeadonCollision(u32 tempActionFinishTimeMs, SwerveDirection forceSwerveInDirection=Swerve_NoPreference):
		CTaskVehicleTempAction(tempActionFinishTimeMs)
		{
			m_SwerveDirection = forceSwerveInDirection;
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_HEADON_COLLISION);
		}
	~CTaskVehicleHeadonCollision(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_HEADON_COLLISION; }
	CTask*			Copy() const {return rage_new CTaskVehicleHeadonCollision(m_tempActionFinishTimeMs, m_SwerveDirection);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_HeadonCollision;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	FSM_Return		HeadonCollision_OnUpdate(CVehicle* pVehicle);

	SwerveDirection m_SwerveDirection;
};

//
//
//
class CTaskVehicleBoostUseSteeringAngle : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_BoostUseSteeringAngle = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleBoostUseSteeringAngle(u32 tempActionFinishTimeMs):
		CTaskVehicleTempAction(tempActionFinishTimeMs)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BOOST_USE_STEERING_ANGLE);
		}
	~CTaskVehicleBoostUseSteeringAngle(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BOOST_USE_STEERING_ANGLE; }
	CTask*			Copy() const {return rage_new CTaskVehicleBoostUseSteeringAngle(m_tempActionFinishTimeMs);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_BoostUseSteeringAngle;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	// FSM function implementations
	FSM_Return		BoostUseSteeringAngle_OnUpdate(CVehicle* pVehicle);
};

#if 0

//
//
//
class CTaskVehicleFollowPointPath : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_FollowPointPath = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleFollowPointPath(u32 tempActionFinishTimeMs):
		CTaskVehicleTempAction(tempActionFinishTimeMs)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FOLLOW_POINT_PATH);
		}
	~CTaskVehicleFollowPointPath(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FOLLOW_POINT_PATH; }
	CTask*			Copy() const {return rage_new CTaskVehicleFollowPointPath(m_tempActionFinishTimeMs);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_FollowPointPath;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

protected:

	// FSM function implementations
	FSM_Return		FollowPointPath_OnUpdate(CVehicle* pVehicle);

	bool			FollowPointPath(CVehicle* pVeh, CVehControls* pVehControls, bool inReverse);
};


//
//
//

class CTaskVehicleReverseAlongPointPath : public CTaskVehicleFollowPointPath
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_ReverseAlongPointPath = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleReverseAlongPointPath(u32 tempActionFinishTimeMs):
		CTaskVehicleFollowPointPath(tempActionFinishTimeMs)
		{
			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_REVERSE_ALONG_POINT_PATH);
		}
	~CTaskVehicleReverseAlongPointPath(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_REVERSE_ALONG_POINT_PATH; }
	CTask*			Copy() const {return rage_new CTaskVehicleReverseAlongPointPath(m_tempActionFinishTimeMs);}

		// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_ReverseAlongPointPath;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:
	// FSM function implementations
	FSM_Return		ReverseAlongPointPath_OnUpdate(CVehicle* pVehicle);
};

#endif //0

//
//
//
class CTaskVehicleBurnout : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Burnout = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleBurnout(u32 tempActionFinishTimeMs):
		CTaskVehicleTempAction(tempActionFinishTimeMs)
		{
			m_Params.m_fCruiseSpeed = 0.0f;

			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BURNOUT);
		}
		~CTaskVehicleBurnout(){}

		int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_BURNOUT; }
		CTask*			Copy() const 
		{
			CTask* pTask = rage_new CTaskVehicleBurnout(m_tempActionFinishTimeMs);
			return pTask;
		}

		// FSM implementations
		FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
		s32			GetDefaultStateAfterAbort()	const {return State_Burnout;}
#if !__FINAL
		friend class CTaskClassInfoManager;
		static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:
	// FSM function implementations
	FSM_Return		Burnout_OnUpdate(CVehicle* pVehicle);
	void			Burnout_OnExit(CVehicle* pVehicle);
};

//CTaskVehicleRevEngine
//Do some random revving the engine
//
class CTaskVehicleRevEngine : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Rev = 0,
		State_Exit
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleRevEngine(u32 tempActionFinishTimeMs):
		CTaskVehicleTempAction(tempActionFinishTimeMs)
		{
			m_Params.m_fCruiseSpeed = 0.0f;
			m_bAllowSuperDummyForThisTask = true;

			SetInternalTaskType(CTaskTypes::TASK_VEHICLE_REV_ENGINE);
		}
		~CTaskVehicleRevEngine(){}

		int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_REV_ENGINE; }
		CTask*			Copy() const 
		{
			CTask* pTask = rage_new CTaskVehicleRevEngine(m_tempActionFinishTimeMs);
			return pTask;
		}

		// FSM implementations
		FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
		s32			GetDefaultStateAfterAbort()	const {return State_Rev;}
#if !__FINAL
		friend class CTaskClassInfoManager;
		static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:
	// FSM function implementations
	FSM_Return		Rev_OnUpdate(CVehicle* pVehicle);
	void			Rev_OnExit(CVehicle* pVehicle);
};

class CTaskVehicleSurfaceInSubmarine : public CTaskVehicleTempAction
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Surface = 0,
		State_Exit
	} VehicleControlState;

	CTaskVehicleSurfaceInSubmarine(u32 tempActionFinishTimeMs)
		: CTaskVehicleTempAction(tempActionFinishTimeMs)
	{
		SetInternalTaskType(CTaskTypes::TASK_VEHICLE_SURFACE_IN_SUBMARINE);
	}
	~CTaskVehicleSurfaceInSubmarine(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_SURFACE_IN_SUBMARINE; }
	CTask*			Copy() const 
	{
		CTask* pTask = rage_new CTaskVehicleSurfaceInSubmarine(m_tempActionFinishTimeMs);
		return pTask;
	}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Surface;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:
	// FSM function implementations
	FSM_Return		Surface_OnUpdate(CVehicle* pVehicle);
};

#endif
