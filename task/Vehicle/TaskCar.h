#ifndef TASK_CAR_H
#define TASK_CAR_H

// Rage headers
#include "fwutil/Flags.h"

// Gta headers.
#include "network/General/NetworkUtil.h"
#include "network/Objects/Synchronisation/SyncNodes/VehicleSyncNodes.h"
#include "peds/QueriableInterface.h"
#include "Renderer/HierarchyIds.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAi/VehMission.h"
#include "vehicleAi/VehicleIntelligence.h"

class camFollowVehicleCamera;
class CPed;
class CPhysical;
class CVehicle;
class CClipPlayer;
class CPointRoute;
class CVehMission;

//Rage headers.
#include "Vector/Vector3.h"

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Makes a ped leave any vehicle they are in. This is really a wrapper
//			task for the exit vehicle task, useful since scripts can use it
//			as part of a sequence without having to specify the actual vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskLeaveAnyCar : public CTask
{
public:

	// PURPOSE: FSM states
	enum eLeaveAnyVehicleState
	{
		State_Start,
		State_ExitVehicle,
		State_Finish
	};

	CTaskLeaveAnyCar(s32 iDelayTime = 0, VehicleEnterExitFlags iRunningFlags = VehicleEnterExitFlags());
	
	virtual ~CTaskLeaveAnyCar() {}

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_LEAVE_ANY_CAR; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskLeaveAnyCar(m_iDelayTime, m_iRunningFlags); }

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

    // Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return ExitVehicle_OnEnter();
	FSM_Return ExitVehicle_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Time to delay exiting the vehicle
	s32 m_iDelayTime;

	// PURPOSE: Exit vehicle flags
	VehicleEnterExitFlags m_iRunningFlags;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

//-------------------------------------------------------------------------
// Task info for leaving any car
//-------------------------------------------------------------------------
class CClonedLeaveAnyCarInfo : public CSerialisedFSMTaskInfo
{
public:

    CClonedLeaveAnyCarInfo(s32 delayTime, VehicleEnterExitFlags runningFlags);
    CClonedLeaveAnyCarInfo();

    virtual CTask  *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_LEAVE_ANY_CAR; }
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_LEAVE_ANY_VEHICLE; }

    void Serialise(CSyncDataBase& serialiser)
	{
        const unsigned SIZEOF_DELAY_TIME = 10;
        SERIALISE_UNSIGNED(serialiser, m_iDelayTime, SIZEOF_DELAY_TIME, "Delay Time");
        SERIALISE_VEHICLE_ENTER_EXIT_FLAGS(serialiser, m_iRunningFlags,	"Running Flags");
    }

private:

    s32                   m_iDelayTime;    // Time to wait before leaving the vehicle
    VehicleEnterExitFlags m_iRunningFlags; // Flags that change the behaviour of the task
};

//////////////////
//Drive
//////////////////

class CTaskInVehicleBasic : public CTask
{
	friend class CTaskCarDrive;

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_fSecondsInAirBeforePassengerComment;

		PAR_PARSABLE;
	};
	static Tunables sm_Tunables;

	enum InVehicleFlags
	{
		IVF_DisableAmbients	= BIT0,
		IVF_PlayedJumpAudio = BIT1,
	};
	
private:

	struct DriveBy
	{
		DriveBy()
		: m_bRequested(false)
		, m_fTime(0.0f)
		, m_iMode(-1)
		, m_Target()
		{}
		
		bool		m_bRequested;
		float		m_fTime;
		int			m_iMode;
		CAITarget	m_Target;
	};

	struct Siren
	{
		Siren()
		: m_bRequested(false)
		, m_fTime(0.0f)
		{}

		bool	m_bRequested;
		float	m_fTime;
	};
	
public:

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define IN_VEHICLE_BASIC_STATES(X)		X(State_Start),					\
											X(State_StreamAssets),			\
											X(State_AttachedToVehicle),		\
											X(State_PlayingAmbients),		\
											X(State_Idle),					\
											X(State_UseWeapon),				\
											X(State_DriveBy),				\
											X(State_ExitVehicle),			\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eInVehicleBasicState
	{
		IN_VEHICLE_BASIC_STATES(DEFINE_TASK_ENUM)
	};

	// PURPOSE: Stream any required assets, return true when they are streamed in
	static bool StreamTaskAssets(const CVehicle* pVehicle, s32 iSeatIndex);

	// Constructor
	CTaskInVehicleBasic(CVehicle* pTargetVehicle, const bool bDriveAnyCar=false, const CConditionalAnimsGroup* pAnims = NULL, fwFlags8 uConfigFlags = 0);

	// Destructor
	virtual ~CTaskInVehicleBasic();

public:

	fwFlags8	GetInVehicleFlags() const	{ return m_uInVehicleBasicFlags; }
	fwFlags8&	GetInVehicleFlags()				{ return m_uInVehicleBasicFlags; }

public:

	// Task required implementations
    virtual aiTask* Copy() const { return rage_new CTaskInVehicleBasic(m_pTargetVehicle, m_bDriveAnyCar, m_pAnims, m_uInVehicleBasicFlags); }
    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_IN_VEHICLE_BASIC;}
	s32 GetDefaultStateAfterAbort() const { return State_Finish; }
	virtual bool MayDeleteOnAbort() const { return true; }	// State_Finish would have just returned FSM_Quit, so we don't need to keep this task around if aborted.

	// Interface functions
	void CheckForUndrivableVehicle( CPed* pPed );

	CVehicle* GetVehicle() const { return m_pTargetVehicle; }

	void TriggerIK(CPed* pPed);
	void AbortIK(CPed* pPed);

	const CConditionalAnimsGroup* GetConditionalAnimGroup() const { return m_pAnims; }
	
	void RequestDriveBy(float fTime, int iMode, const CAITarget& rTarget);
	void RequestSiren(float fTime);
   
protected:

	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Local state function calls
	FSM_Return					Start_OnUpdate();
	FSM_Return					StreamAssets_OnUpdate();
	void								AttachedToVehicle_OnEnter();
	FSM_Return					AttachedToVehicle_OnUpdate();
	FSM_Return					AttachedToVehicle_OnExit();
	void								PlayingAmbients_OnEnter();
	FSM_Return					PlayingAmbients_OnUpdate();
	FSM_Return					Idle_OnUpdate();
	void								UseWeapon_OnEnter();
	FSM_Return					UseWeapon_OnUpdate();
	void								DriveBy_OnEnter();
	FSM_Return					DriveBy_OnUpdate();
	void								DriveBy_OnExit();
	void								ExitVehicle_OnEnter();
	FSM_Return					ExitVehicle_OnUpdate();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	virtual	void CleanUp();

private:

	bool CheckForPlayingAmbients();
	bool CheckForTrainExit();
	void ProcessSiren();
	bool ShouldDriveBy() const;
	void UpdateCommentOnVehicle(CPed* pPed);
	void SetConditionalAnimsGroup();
	void AllowTimeslicing(CPed* pPed);

	DriveBy													m_DriveBy;
	Siren														m_Siren;
	const CConditionalAnimsGroup*		m_pAnims;  // If using a scenario, this is the animation group specified in the scenario metadata.
	RegdVeh													m_pTargetVehicle;	
	RegdaiTask											m_pMotionInAutomobileTask;	// To avoid task tree look up each frame
	CTaskGameTimer									m_broadcastStolenCarTimer;
	CTaskGameTimer									m_broadcaseNiceCarTimer;
	u32															m_nUpsideDownCounter;
	float 													m_fTimeOffTheGround;
	VehicleEnterExitFlags						m_iVehicleEnterExitFlags;
	fwFlags8												m_uInVehicleBasicFlags;
	u8															m_bDriveAnyCar			:1;
	u8															m_bCanAttachToVehicle	:1;
	u8															m_bSetPedOutOfVehicle :1;
	u8															m_bWasVehicleUpsideDown : 1;
	u8															m_bWasVehicleOnFire : 1;
	RegdPed								m_pPassengerPlayerIsLookingAt;
	u32									m_uPassengerLookingEndTime;	

	static u32							sm_LastVehicleFlippedSpeechTime;
	static u32							sm_LastVehicleFireSpeechTime;
	static u32							sm_LastVehicleJumpSpeechTime;
public:
#if !__FINAL
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, IN_VEHICLE_BASIC_STATES)
#endif 
};


///////////////////////////////
//BASE CAR DRIVE 
///////////////////////////////

class CTaskCarDrive : public CTask
{
	enum eState
	{
		State_Start = 0,
		State_CarDrive,
		State_EnterVehicle,
		State_LeaveAnyCar,
		State_DriveBy,
		State_Finish
	};
public:

	friend class CTaskCarDriveWander;
	
    CTaskCarDrive(CVehicle* pVehicle);
	CTaskCarDrive(CVehicle* pVehicle, const float CruiseSpeed, const s32 iDrivingFlags, const bool bUseExistingNodes);
	CTaskCarDrive(CVehicle* pVehicle, aiTask *pVehicleTask, const bool bUseExistingNodes = false);  


    virtual ~CTaskCarDrive();

    virtual aiTask*		Copy() const 
    {
		CTaskCarDrive* pTask = NULL;
		pTask = rage_new CTaskCarDrive(m_pVehicle, m_pVehicleTask ? m_pVehicleTask->Copy() : m_pVehicleTask, m_bUseExistingNodes);
    	pTask->m_bAsDriver=m_bAsDriver;
    	return pTask;
    }
    
    virtual int			GetTaskTypeInternal() const {return CTaskTypes::TASK_CAR_DRIVE;}
    virtual s32			GetDefaultStateAfterAbort()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 
    
    void				SetAsDriver(const bool b) {m_bAsDriver=b;}
	void				GetCarState();

	FSM_Return			UpdateFSM( const s32 iState, const FSM_Event iEvent);
	FSM_Return			ProcessPostFSM();

	virtual	void		CleanUp();

	//Accessors for vehicle task
	const aiTask*	GetVehicleTask() const { return m_pActualVehicleTask ? m_pActualVehicleTask : m_pVehicleTask; }

	void	RequestDriveBy(const CAITarget& target, const float fFrequency, const float fAbortDistance, const u32 uFiringPattern, const u32 uDurationMs, const u32 uWeaponHash = -1);

protected:
	virtual CTask::FSM_Return Start_OnUpdate(CPed* pPed);

	FSM_Return CarDrive_OnEnter(CPed* pPed);
	FSM_Return CarDrive_OnUpdate(CPed* pPed);

	FSM_Return EnterVehicle_OnEnter(CPed* pPed);
	FSM_Return EnterVehicle_OnUpdate(CPed* pPed);

	FSM_Return LeaveAnyCar_OnEnter(CPed* pPed);
	FSM_Return LeaveAnyCar_OnUpdate(CPed* pPed);

	FSM_Return DriveBy_OnEnter(CPed* pPed);
	FSM_Return DriveBy_OnUpdate(CPed* pPed);

	bool ShouldDoDriveBy();

	bool IsNetworkVehicleValid(CVehicle* pVehicle) { if(NetworkInterface::IsGameInProgress() && pVehicle == NULL) { return false; } return true; }

	struct DriveByInfo
	{
		CAITarget	DriveByTarget;
		float		fFrequency;
		float		fAbortDistance;
		u32			uFiringPattern;
		u32			uDuration;
		u32			uWeaponHash;
	};
	DriveByInfo m_DriveByInfo;

	RegdaiTask m_pVehicleTask;	//this is task passed in on creation of CTaskCarDrive
	RegdaiTask m_pActualVehicleTask; //copy of task provided to vehicle sub systems. This is the one that is active and updated

    RegdVeh			m_pVehicle;
	u32				m_nTimeLastDriveByRequested;
    bool			m_bAsDriver;
	bool			m_bUseExistingNodes;
	bool			m_bDriveByRequested;
	bool            m_bSetPedOuOfVehicle;

    virtual void		SetUpCar();
};

///////////////////////////////////////
//DRIVE A CAR LIKE A WANDER
///////////////////////////////////////

class CTaskCarDriveWander : public CTaskCarDrive
{
public:

    CTaskCarDriveWander(CVehicle* pVehicle, const s32 iDrivingFlags=DMode_StopForCars, const float CruiseSpeed=10.0f, const bool bUseExistingNodes = false);
	CTaskCarDriveWander(CVehicle* pVehicle, aiTask *pVehicleTask, const bool bUseExistingNodes = false);
  
	~CTaskCarDriveWander();

    virtual aiTask*			Copy() const 
	{
		CTaskCarDriveWander* pTask = NULL;

		pTask = rage_new CTaskCarDriveWander(m_pVehicle, m_pVehicleTask ? m_pVehicleTask->Copy() : m_pVehicleTask, m_bUseExistingNodes);

		pTask->m_bAsDriver=m_bAsDriver;
		return pTask;
	}

    virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_CAR_DRIVE_WANDER;}

	FSM_Return		ProcessPreFSM();

    // Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

private:
	void			ThinkAboutBecomingInterestingDriver();
    virtual void	SetUpCar();
};

//-------------------------------------------------------------------------
// Task info for drive car wander
//-------------------------------------------------------------------------
class CClonedDriveCarWanderInfo : public CSerialisedFSMTaskInfo
{
public:

    CClonedDriveCarWanderInfo(CVehicle *pVehicle, const s32 iDrivingFlags, const float CruiseSpeed, const bool bUseExistingNodes);
    CClonedDriveCarWanderInfo();

    virtual CTask  *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_DRIVE_CAR_WANDER; }
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_VEHICLE_DRIVE_WANDER; }

    void Serialise(CSyncDataBase& serialiser)
	{
        ObjectId vehicleID = m_Vehicle.GetVehicleID();

        SERIALISE_OBJECTID(serialiser, vehicleID, "Vehicle");
        SERIALISE_INTEGER(serialiser, m_DrivingFlags, sVehicleMissionParams::SIZE_OF_DRIVING_FLAGS, "Driving Flags");
        SERIALISE_PACKED_FLOAT(serialiser, m_CruiseSpeed, CTaskVehicleMissionBase::MAX_CRUISE_SPEED, sVehicleMissionParams::SIZE_OF_CRUISE_SPEED, "Cruise Speed");
        SERIALISE_BOOL(serialiser, m_UseExistingNodes, "Use Existing Nodes");

        m_Vehicle.SetVehicleID(vehicleID);
    }

private:

    CSyncedVehicle	      m_Vehicle;          // Target vehicle
    s32                   m_DrivingFlags;     // Driving Flags
    float                 m_CruiseSpeed;      // Cruise speed
    bool                  m_UseExistingNodes; // Use existing nodes?
};

/////////////////////////////
//PLAYER DRIVEBY TASK
/////////////////////////////

class CTaskPlayerDrive : public CTask
{
public:
	// PURPOSE:	Global task tuning data.
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Time between stealth noise blips, in ms.
		u32		m_StealthNoisePeriodMS;

		// PURPOSE:	Threshold for what's considered low speed for stealth noise purposes, in m/s.
		float	m_StealthSpeedThresholdLow;

		// PURPOSE:	Threshold for what's considered high speed for stealth noise purposes, in m/s.
		float	m_StealthSpeedThresholdHigh;

		// PURPOSE:	Noise range scaling factor when riding a bicycle.
		float	m_StealthVehicleTypeFactorBicycles;

		// PURPOSE: Minimum speed player can jump off a bike
		float	m_MinPlayerJumpOutSpeedBike;

		// PURPOSE: Minimum speed player can jump out of a car
		float	m_MinPlayerJumpOutSpeedCar;

		// PURPOSE: Time between adding dangerous vehicle shocking events.
		float	m_TimeBetweenAddingDangerousVehicleEvents;

		PAR_PARSABLE;
	};

	// PURPOSE:	Task tuning data.
	static Tunables sm_Tunables;

	enum eState
	{
		State_Init,
		State_DriveBasic,	// Standard vehicle task
		State_Gun,			// Think driveby
		State_MountedWeapon,// Using a mounted weapon
		State_Projectile,	// Throw grenades etc. out of vehicle
		State_ExitVehicle,
		State_ShuffleSeats,
		State_Finish
	};

	CTaskPlayerDrive();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Init; }
	virtual bool ShouldAlwaysDeleteSubTaskOnAbort() const { return true; }	// This task supports resuming, but won't need the aborted subtasks to be preserved.

	virtual aiTask* Copy() const { return rage_new CTaskPlayerDrive(); }

	virtual	void CleanUp();

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_PLAYER_DRIVE; }

	virtual bool ProcessPostPreRenderAfterAttachments();

	void	SetForceExit(bool bForce) { m_bForcedExit = bForce; }

	void	SetSmashedWindow(bool bSmashedWindow) { m_bSmashedWindow = bSmashedWindow; }
	bool	GetSmashedWindow() const { return m_bSmashedWindow; }

	bool	IsExitingGunState() const { return m_bIsExitingGunState; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif


protected:

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM( const s32 iState, const FSM_Event iEvent);

	FSM_Return Init_OnUpdate(CPed* pPed);

	void DriveBasic_OnEnter(CPed* pPed);
	FSM_Return DriveBasic_OnUpdate(CPed* pPed);

	void Gun_OnEnter(CPed* pPed);
	FSM_Return Gun_OnUpdate(CPed* pPed);

	void MountedWeapon_OnEnter(CPed* pPed);
	FSM_Return MountedWeapon_OnUpdate(CPed* pPed);

	void Projectile_OnEnter(CPed* pPed);
	FSM_Return Projectile_OnUpdate(CPed* pPed);

	void ExitVehicle_OnEnter(CPed* pPed);
	FSM_Return ExitVehicle_OnUpdate(CPed* pPed);

	void ShuffleSeats_OnEnter(CPed* pPed);
	FSM_Return ShuffleSeats_OnUpdate(CPed* pPed);
	void ShuffleSeats_OnExit();

	FSM_Return Finish_OnUpdate(CPed* pPed);

	void ProcessBuddySpeedSpeech( CPed* pPed );
	void ProcessStealthNoise();
	void PreStreamDrivebyClipSets( CPed* pPed );
	void StreamBustedClipGroup( CPed* pPed );

	// PURPOSE:	Send events to peds about to be run over, so they can evade or brace for impact.
	void ScanForPedDanger();

	s32 GetDesiredState(CPed* pPed);
	void SetState(s32 iState);

	// PURPOSE: Used to update the searchlight turret and still allow drive bys.
	bool AllowDrivebyWithSearchLight(CPed* pPed, CVehicle* pVehicle);
	void ControlMountedSearchLight(CPed* pPed, CVehicle* pVehicle);
	bool GetTargetInfo(CPed* pPed, Vector3& vTargetPosOut,const CEntity*& pTargetOut);
	bool ShouldProcessPostPreRenderAfterAttachments(CPed* pPed, CVehicle* pVehicle);

	fwClipSetRequestHelper		m_ClipSetRequestHelper;
	fwMvClipSetId				m_chosenAmbientSetId;
	bool						m_bClipGroupLoaded;
	bool						m_bExitButtonUp;
	bool						m_bSmashedWindow;
	bool						m_bDriveByClipSetsLoaded;
	float						m_fTimePlayerDrivingSlowly;
	float						m_fTimePlayerDrivingQuickly;
	float						m_fDangerousVehicleCounter;
	u32							m_uTimeSinceLastButtonPress;
	u32							m_uTimeSinceStealthNoise;	// Time in ms since last stealth noise emission.
	bool						m_bExitCarWhenGunTaskFinishes;
	bool						m_bForcedExit;
	bool						m_bIsExitingGunState;

	static const s32 MAX_DRIVE_BY_CLIPSETS = 16;
	CMultipleClipSetRequestHelper<MAX_DRIVE_BY_CLIPSETS> m_MultipleClipSetRequestHelper;

	s32							m_iTargetShuffleSeatIndex;
 
    bool                        m_bVehicleDriveTaskAddedOrNotNeeded;
	bool						m_bRestarting;
	bool						m_bInFirstPersonVehicleCamera;

	// State transition variables
	// Reset in PreProcessFSM
	VehicleEnterExitFlags		m_iExitVehicleRunningFlags;

	bool						m_bTryingToInterruptVehMelee;
	float						m_fTimeToDisableFP;

};

enum
{
	MODE_NORMAL=0,
	MODE_RACING,
	MODE_REVERSING
};
///////////////////////////
//Drive point route
///////////////////////////

class CTaskDrivePointRoute : public CTask
{
	enum eState
	{
		State_DrivePointRoute = 0,
		State_Finish
	};

public:

	CTaskDrivePointRoute
		(CVehicle* pVehicle, const CPointRoute& route, 
    	 const float CruiseSpeed=10.0f, 
    	 const int iMode=MODE_NORMAL, 
    	 const int iDesiredCarModel = -1, 
    	 const float fTargetRadius=4, 
    	 const s32 iDrivingFlags=DMode_StopForCars);
	~CTaskDrivePointRoute();	

    virtual aiTask*		Copy() const;
    virtual int			GetTaskTypeInternal() const {return CTaskTypes::TASK_CAR_DRIVE_POINT_ROUTE;}
    
	virtual s32		GetDefaultStateAfterAbort()	const {return State_DrivePointRoute;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 
  
	inline int			GetProgress() { return m_iProgress; }
	inline CPointRoute* GetRoute() { return m_pRoute; }

	//Accessors to the desired subtask
	const aiTask*		GetDesiredSubtask() const { return m_pDesiredSubtask; }
	void				SetDesiredSubtask(aiTask* pTask);

	FSM_Return			UpdateFSM( const s32 iState, const FSM_Event iEvent);
	s32					GetDrivingFlags() const { return m_iDrivingFlags; }
private:
	FSM_Return DrivePointRoute_OnEnter(CPed* pPed);
	FSM_Return DrivePointRoute_OnUpdate(CPed* pPed);

    RegdVeh			m_pVehicle;
    CPointRoute*	m_pRoute;
    int				m_iMode;
    int				m_iDesiredCarModel;
    float			m_fTargetRadius;
	float			m_fCruiseSpeed;
    int				m_iProgress;
	RegdaiTask m_pDesiredSubtask;
	s32			m_iDrivingFlags;
};

#if 0
class CTaskComplexCarDriveUsingNavMesh : public CTask
{
public:
	enum eRouteState
	{
		ROUTESTATE_NOT_FOUND,
		ROUTESTATE_FOUND,
		ROUTESTATE_STILL_WAITING
	};
private:
	enum eTaskState
	{
		STATE_REQUEST_PATH,
		STATE_FOLLOWING_PATH
	};
public:

	CTaskComplexCarDriveUsingNavMesh(CVehicle * pVehicle, const Vector3 & vTarget);
	~CTaskComplexCarDriveUsingNavMesh();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskComplexCarDriveUsingNavMesh(m_pVehicle, m_vTarget);
	}

	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_COMPLEX_CAR_DRIVE_USING_NAVMESH; }
	virtual FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort() const { return STATE_REQUEST_PATH; }

#if !__FINAL
	virtual void		Debug() const;
	virtual atString	GetName() const { return atString("DriveUsingNavMesh"); }
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	void RequestPath_OnEnter(CPed * pPed);
	FSM_Return RequestPath_OnUpdate(CPed * pPed);
	FSM_Return FollowingPath_OnUpdate(CPed * pPed);

	inline int	GetRouteState() const { return m_iRouteState; }

	void		GiveCarPointPathToFollow(CVehicle* pVeh, Vector3 *pPoints, int numPoints);

private:

	Vector3						m_vTarget;
	RegdVeh						m_pVehicle;

	CTaskGameTimer				m_CheckPathResultTimer;
	int							m_iRouteState;
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;

	static const float			ms_fTargetRadius;
};

#endif //0

/////////////////////
//Set temp action
/////////////////////

class CTaskCarSetTempAction : public CTask
{
public:

	CTaskCarSetTempAction(CVehicle* pVehicle, const int iAction, const int iTime);
	~CTaskCarSetTempAction();

public:

	fwFlags8	GetConfigFlagsForAction()			const	{ return m_uConfigFlagsForAction; }
	fwFlags8&	GetConfigFlagsForAction()					{ return m_uConfigFlagsForAction; }
	fwFlags8	GetConfigFlagsForInVehicleBasic()	const	{ return m_uConfigFlagsForInVehicleBasic; }
	fwFlags8&	GetConfigFlagsForInVehicleBasic()			{ return m_uConfigFlagsForInVehicleBasic; }

public:

	enum state
	{
		State_Start = 0,
		State_CarTempAction,
		State_Finish
	};
	
  	virtual aiTask*	Copy() const;
    virtual int		GetTaskTypeInternal() const {return CTaskTypes::TASK_CAR_SET_TEMP_ACTION;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// FSM implementations
	FSM_Return		UpdateFSM( const s32 iState, const FSM_Event iEvent);

	const bool		IsTargetVehicleNetworkClone() { return m_pTargetVehicle && m_pTargetVehicle->IsNetworkClone(); };

private:
	FSM_Return		Start_OnUpdate(CPed* pPed);

	FSM_Return		CarSetTempAction_OnEnter(CPed* pPed);
	FSM_Return		CarSetTempAction_OnUpdate(CPed* pPed);
	RegdVeh			m_pTargetVehicle;
	int				m_iAction;
	int				m_iTime;
	fwFlags8		m_uConfigFlagsForInVehicleBasic;
	fwFlags8		m_uConfigFlagsForAction;
};


//////////////////////////////////////////////////////////////////////////
// CTaskComplexReactToRanPedOver
//////////////////////////////////////////////////////////////////////////

class CTaskReactToRanPedOver : public CTask
{
public:
	enum eActionToTake
	{
		ACTION_INVALID = 0,
		ACTION_GET_OUT_ANGRY,
		ACTION_GET_OUT_CONCERNED,
		ACTION_SAY_ANGRY,
		ACTION_SAY_CONCERNED,
		ACTION_FIGHT,
	};
	CTaskReactToRanPedOver(CVehicle* pMyVehicle, CPed* pPedRanOver, float fDamageCaused, CTaskReactToRanPedOver::eActionToTake action = ACTION_INVALID);
	~CTaskReactToRanPedOver();

	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_REACT_TO_RUNNING_PED_OVER;}

	virtual s32 GetDefaultStateAfterAbort() const { return State_finish; }

#if !__FINAL
	//virtual void Debug() const{}
	//virtual atString GetName() const {return atString("ReactToRunningPedOver");}
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	//virtual aiTask* CreateNextSubTask(CPed* pPed);
	//virtual aiTask* CreateFirstSubTask(CPed* pPed);
	//virtual aiTask* ControlSubTask(CPed* pPed);

	// FSM implementations
	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM( const s32 iState, const FSM_Event iEvent);

private:

	typedef enum {
		State_start,
		State_getOutOfCar,
		State_watchPed,
		State_getBackInCar,
		State_sayAudio,
		State_wait,
		State_fight,
		State_finish
	} ReactToRanPedOverState;

	FSM_Return Start_OnUpdate(CPed* pPed);

	FSM_Return GetOutOfCar_OnEnter(CPed* pPed);
	FSM_Return GetOutOfCar_OnUpdate(CPed* pPed);

	FSM_Return WatchPed_OnEnter(CPed* pPed);
	FSM_Return WatchPed_OnUpdate();

	FSM_Return SayAudio_OnEnter();
	FSM_Return SayAudio_OnUpdate(CPed* pPed);

	FSM_Return Wait_OnEnter();
	FSM_Return Wait_OnUpdate();

	FSM_Return GetBackInCar_OnEnter();
	FSM_Return GetBackInCar_OnUpdate(CPed* pPed);

	FSM_Return Fight_OnEnter(CPed* pPed);
	FSM_Return Fight_OnUpdate();

	RegdVeh		m_pMyVehicle;
	RegdPed		m_pPedRanOver;
	Vector3		m_vTarget;

	eActionToTake m_nAction;

	float m_fDamageCaused;

	CTask*	CreateSubTask(const int iTaskType, CPed* pPed);
	void	CalcTargetPosWithOffset(CPed * pPed, CPed * pInjuredPed);
	char*	GetSayPhrase() const;
};


//////////////////////////////////////////////////////////////////////////
// CTaskComplexGetOffBoat
//////////////////////////////////////////////////////////////////////////

class CTaskComplexGetOffBoat : public CTaskComplex
{
public:

	static const int ms_iNumJumpAttempts;

	CTaskComplexGetOffBoat(const int iTimer=-1);
	~CTaskComplexGetOffBoat();

	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_COMPLEX_GET_OFF_BOAT; }

#if !__FINAL
	virtual atString GetName() const {return atString("GetOffBoat");}
#endif

	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	virtual aiTask* CreateNextSubTask(CPed* pPed);
	virtual aiTask* CreateFirstSubTask(CPed* pPed);
	virtual aiTask* ControlSubTask(CPed* pPed);

private:

	CBoat * ProbeForBoat(CPed * pPed);
	CTask*	CreateSubTask(const int iTaskType, CPed* pPed);
	void WarpPedOffBoat(CPed * pPed);

	bool CheckForOnBoat(CPed * pPed);

	RegdVeh m_pBoat;
	Vector3 m_vGetOffLocalPos;
	float m_fGetOffLocalHeading;
	int m_iNumJumpAttempts;
	bool m_bTriggerAutoVault;
	bool m_bFallBackToNavMeshTask;
	u32 m_uTimeLastOnBoat;
	CTaskGameTimer m_Timer;
};


///////////////////////////////
// Main vehicle control task
///////////////////////////////
class CTaskControlVehicle : public CTaskFSMClone
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_EnterVehicle,
		State_StartVehicle,
		State_JoinWithRoadSystem,
		State_Drive,
		State_Exit
	} VehicleControlState;

	enum ConfigFlags
	{
		CF_IgnoreNewCommand = BIT0,
	};

	// Constructor/destructor
	CTaskControlVehicle(CVehicle* pVehicle, aiTask* pVehicleTask, aiTask* pSubtask = NULL, fwFlags8 uConfigFlags = 0);

	~CTaskControlVehicle();

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_CONTROL_VEHICLE; }


	virtual aiTask* Copy() const {
		return rage_new CTaskControlVehicle(m_pVehicle, m_pVehicleTask ? m_pVehicleTask->Copy() : m_pVehicleTask, m_pDesiredSubTask ? m_pDesiredSubTask->Copy() : m_pDesiredSubTask, m_uConfigFlags);
	}

	// FSM implementations
	FSM_Return		ProcessPreFSM();
	FSM_Return		UpdateFSM( const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void	CleanUp();

	virtual s32		GetDefaultStateAfterAbort()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	//Accessors to the desired subtask
	const aiTask*	GetDesiredSubtask() const { return m_pDesiredSubTask; }
	void			SetDesiredSubtask(aiTask* pTask);

	//Accessors for vehicle task
	const aiTask*	GetVehicleTask() const { return m_pActualVehicleTask ? m_pActualVehicleTask : m_pVehicleTask; }
	aiTask*			GetVehicleTask() { return m_pActualVehicleTask ? m_pActualVehicleTask : m_pVehicleTask; }

	//
	bool			IsVehicleTaskUnableToGetToRoad () const;

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void                OnCloneTaskNoLongerRunningOnOwner();
	virtual bool				IsInScope(const CPed*) { return true; } // always returns true so that the vehicle task gets applied to the car properly
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

private:

	// FSM function implementations
	// State_Start
	FSM_Return	Start_OnUpdate	(CPed* pPed);
	FSM_Return	Start_OnUpdateClone	(CPed* pPed);
	// State_EnterVehicle
	void		EnterVehicle_OnEnter	(CPed* pPed);
	FSM_Return	EnterVehicle_OnUpdate	(CPed* pPed);
	// State_StartVehicle
	void		StartVehicle_OnEnter	(CPed* pPed);
	FSM_Return	StartVehicle_OnUpdate	(CPed* pPed);
	// State_JoinWithRoadSystem
	FSM_Return	JoinWithRoadSystem_OnUpdate	(CPed* pPed);
	FSM_Return	JoinWithRoadSystem_OnUpdateClone(CPed* pPed);
	// State_Drive
	void		Drive_OnEnter	(CPed* pPed);
	FSM_Return	Drive_OnUpdate	(CPed* pPed);
	FSM_Return	Drive_OnUpdateClone	(CPed* pPed);
	// Helpers
	bool		IsPedInTheCorrectVehicle(CPed* pPed);

	RegdaiTask m_pVehicleTask;	//this is task passed in on creation of CTaskControlVehicle
	RegdaiTask m_pActualVehicleTask; //copy of task provided to vehicle sub systems. This is the one that is active and updated

	// Vehicle to control
	RegdVeh		m_pVehicle;
	// Any subtask that should be running as the ped drives the vehicle
	RegdaiTask	m_pDesiredSubTask;
	bool		m_bNewDesiredSubtask;
	fwFlags8	m_uConfigFlags;
};


//-------------------------------------------------------------------------
// Task info for being in cover
//-------------------------------------------------------------------------
class CClonedControlVehicleInfo : public CSerialisedFSMTaskInfo
{
public:

	typedef void (*fnTaskDataLogger)(netLoggingInterface *log, u32 taskType, const u8 *taskData, const int numBits);

	static void SetTaskDataLogFunction(fnTaskDataLogger taskDataLogFunction) { m_taskDataLogFunction = taskDataLogFunction; }

	static const unsigned MAX_VEHICLE_TASK_DATA_SIZE = CVehicleTaskDataNode::MAX_VEHICLE_TASK_DATA_SIZE;

public:
	CClonedControlVehicleInfo();
	CClonedControlVehicleInfo(u32 state, CVehicle* pVehicle, CTaskVehicleMissionBase* pVehicleTask);
	~CClonedControlVehicleInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_CONTROL_VEHICLE;}
    virtual int GetScriptedTaskType() const;
	CVehicle* GetVehicle() { return m_targetVehicle.GetVehicle(); }

	virtual CTaskFSMClone *CreateCloneFSMTask();
    virtual CTask         *CreateLocalTask(fwEntity *pEntity);

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskControlVehicle::State_Exit>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "State";}

	virtual bool IsNetworked() const { return m_network; }

    u32 GetVehicleTaskType() const { return m_vehicleTaskType; }

	void ApplyDataToVehicleTask(CTaskVehicleMissionBase& vehicleTask);

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		m_targetVehicle.Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_hasVehicleTask);

		if (m_hasVehicleTask)
		{
			SERIALISE_TASKTYPE(serialiser, m_vehicleTaskType, "Task Type");
			SERIALISE_UNSIGNED(serialiser, m_vehicleTaskDataSize, SIZEOF_TASK_DATA);

			if (serialiser.GetIsMaximumSizeSerialiser())
			{
				m_vehicleTaskDataSize = MAX_VEHICLE_TASK_DATA_SIZE;
			}

			if(m_vehicleTaskDataSize > 0)
			{
				SERIALISE_DATABLOCK(serialiser, m_vehicleTaskData, m_vehicleTaskDataSize);

				if (m_taskDataLogFunction)
				{
					(*m_taskDataLogFunction)(serialiser.GetLog(), m_vehicleTaskType, m_vehicleTaskData, m_vehicleTaskDataSize);
				}
			}	
		}
	}

private:
	static const unsigned SIZEOF_TASK_DATA = datBitsNeeded<MAX_VEHICLE_TASK_DATA_SIZE>::COUNT;

	// static task data logging function
	static fnTaskDataLogger m_taskDataLogFunction;

	CSyncedVehicle	m_targetVehicle;
	bool			m_hasVehicleTask;
	u32				m_vehicleTaskType;
	u32				m_vehicleTaskDataSize;
	u8				m_vehicleTaskData[MAX_VEHICLE_TASK_DATA_SIZE];
	bool			m_network;
};

#endif
