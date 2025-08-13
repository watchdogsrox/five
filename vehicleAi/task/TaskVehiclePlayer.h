#ifndef TASK_VEHICLE_PLAYER_H
#define TASK_VEHICLE_PLAYER_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\racingline.h"
#include "VehicleAi\VehMission.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "modelinfo\PedModelInfo.h"

class CVehicle;
class CVehControls;
class CAutomobile;
class CBike;
class CBoat;
class CSubmarine;
class CPlane;
class CAutogyro;
class CHeli;
class CRotaryWingAircraft;

//Rage headers.
#include "Vector/Vector3.h"
#include "vector/vector2.h"

//
//
//
class CTaskVehiclePlayerDrive : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Drive,
		State_NoDriver,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePlayerDrive();
	virtual ~CTaskVehiclePlayerDrive();

	virtual int			GetTaskTypeInternal() const =0;
	virtual aiTask*		Copy() const =0;

	// FSM implementations
	s32				GetDefaultStateAfterAbort()	const {return State_Drive;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	//Active controls
	virtual void		ProcessDriverInputsForPlayerOnEnter						(CVehicle *pVehicle)=0;
	virtual void		ProcessDriverInputsForPlayerOnUpdate					(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle)=0;
	virtual void		ProcessDriverInputsForPlayerOnExit						(CVehicle* UNUSED_PARAM(pVehicle)){}
	
	//shared functionality
	void				ProcessPlayerControlInputsForBrakeAndGasPedal	(CControl* pControl, bool bForceBrake, CVehicle *pVehicle);
	
	//Inactive controls
	virtual void		ProcessDriverInputsForPlayerInactiveOnUpdate			(CControl *pControl, CVehicle *pVehicle)=0;

	static bool			IsThePlayerControlInactive						(CControl *pControl);

	virtual bool IsIgnoredByNetwork() const { return true; }

	static bank_float sfSpecialAbilitySteeringTimeStepMult;

protected:

	//State_Drive,
	virtual void	Drive_OnEnter		(CVehicle* pVehicle);
	FSM_Return	    Drive_OnUpdate		(CVehicle* pVehicle);
	void			Drive_OnExit		(CVehicle* pVehicle);
	//State_NoDriver
	void		    NoDriver_OnEnter	(CVehicle* pVehicle);
	FSM_Return	    NoDriver_OnUpdate	(CVehicle* pVehicle);

	float m_fTimeSincePlayingRecording;
	bool m_bExitButtonUp;
	bool m_bDoorControlButtonUp;
	u32 m_uTimeHydraulicsControlPressed;
#if RSG_PC
	float m_fLowriderUpDownInput;
#endif
};

//
//
//
class CTaskVehiclePlayerDriveAutomobile : public CTaskVehiclePlayerDrive
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Drive,
		State_NoDriver,
		State_DiggerMode,
	} VehicleControlState;
	

	static float ms_fAUTOMOBILE_MAXSPEEDNOW;
	static float ms_fAUTOMOBILE_MAXSPEEDCHANGE;
	static dev_float ms_fCAR_STEER_SMOOTH_RATE_STOPPED;
	static bank_float ms_fCAR_STEER_SMOOTH_RATE_STOPPED_SPECIAL;
	static dev_float ms_fCAR_STEER_SMOOTH_RATE_STOPPED_QUAD;
    static dev_float ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_QUAD;
	static bank_float ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_SPECIAL;
	static dev_float ms_fCAR_STEER_SMOOTH_RATE_ATSPEED;
	static dev_float ms_fCAR_STEER_SMOOTH_RATE_MAXSPEED;
	static dev_float ms_fCAR_STEERING_CURVE_POW;
	static dev_float ms_fCAR_STEER_STATIONARY_AUTOCENTRE_MAX;

	// Constructor/destructor
	CTaskVehiclePlayerDriveAutomobile();
	~CTaskVehiclePlayerDriveAutomobile(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AUTOMOBILE; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveAutomobile();}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );

	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);

protected:
	//State_DiggerMode
	void		DiggerMode_OnEnter	(CVehicle* pVehicle);
	FSM_Return	DiggerMode_OnUpdate	(CVehicle* pVehicle);

	void		ProcessHydraulics( CControl* pControl, CVehicle* pVehicle, float fStickX, float fStickY );

	void	ProcessHydraulicsStateChange( CControl* pControl, CVehicle* pVehicle );
	bool	ProcessHydraulicsTilt( CControl* pControl, CVehicle* pVehicle, float fStickX, float fStickY );
    void    ProcessSuspensionDrop( CVehicle* pVehicle );

	bool m_JustEnteredDiggerMode;
    bool m_JustLeftDiggerMode;
#if RSG_PC
	bool m_MouseSteeringInput;
#endif
};


class CVehicleGadgetArticulatedDiggerArm;
//
//
//
class CTaskVehiclePlayerDriveDiggerArm : public CTaskVehiclePlayerDrive
{
public:
	// Constructor/destructor
	CTaskVehiclePlayerDriveDiggerArm(CVehicleGadgetArticulatedDiggerArm *diggerGadget = NULL);
	~CTaskVehiclePlayerDriveDiggerArm(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_DIGGER_ARM; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveDiggerArm(m_DiggerGadget);}

	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *UNUSED_PARAM(pControl), CVehicle *UNUSED_PARAM(pVehicle)){}
private:
	CVehicleGadgetArticulatedDiggerArm *m_DiggerGadget;
};

//
//
//
class CTaskVehiclePlayerDriveBike : public CTaskVehiclePlayerDrive
{
public:
	static float ms_fBIKE_MAXSPEEDNOW;
	static float ms_fBIKE_MAXSPEEDCHANGE;

	static dev_float ms_fBIKE_STEER_SMOOTH_RATE_MAXSPEED;
	static dev_float ms_fBIKE_STD_STEER_SMOOTH_RATE;
	static dev_float ms_fBIKE_STOPPED_STEER_SMOOTH_RATE;
	static dev_float ms_fBIKE_STD_LEAN_SMOOTH_RATE;
	
	static dev_float ms_fBICYCLE_STEER_SMOOTH_RATE_MAXSPEED;
	static dev_float ms_fBICYCLE_STD_STEER_SMOOTH_RATE;
	static dev_float ms_fBICYCLE_STOPPED_STEER_SMOOTH_RATE;
	static dev_float ms_fBICYCLE_STD_LEAN_SMOOTH_RATE;

	static dev_float ms_fBICYCLE_MIN_FORWARDNESS_SLOW;
	static dev_float ms_fBICYCLE_MAX_FORWARDNESS_SLOW;

	static dev_float ms_fMOTION_STEER_SMOOTH_RATE_MULT;
	static dev_float ms_fMOTION_STEER_DEAD_ZONE;
	static dev_float ms_fPAD_STEER_REUCTION_PAD_END;
	static dev_float ms_fBICYCLE_STEER_REDUCTION_UNDER_LIMIT_MULT;
	static dev_float ms_fBIKE_STEER_REDUCTION_UNDER_LIMIT_MULT;

	// Constructor/destructor
	CTaskVehiclePlayerDriveBike();
	~CTaskVehiclePlayerDriveBike(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BIKE; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveBike();}
	u32			GetLastSprintTime() const { return m_uLastSprintTime; }

	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);
	void		ProcessPlayerControlInputsForBrakeAndGasPedalBicycle	(CControl* pControl, bool bForceBrake, CBike *pBike, CPed * pPlayerPed);
private:
	
	float m_fStartingThrottle;
	float m_fMaxBicycleForwardnessSlowCycle;
	u32 m_uLastSprintTime;
	u32 m_uStartTime;
	bool m_bClearedForwardness;
	bool m_bStartFinished;
};


//
//
//
class CTaskVehiclePlayerDriveBoat : public CTaskVehiclePlayerDrive
{
public:
	static dev_float ms_fBOAT_STEER_SMOOTH_RATE;
	static dev_float ms_fBOAT_PITCH_FWD_SMOOTH_RATE;

	// Constructor/destructor
	CTaskVehiclePlayerDriveBoat();
	~CTaskVehiclePlayerDriveBoat(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BOAT; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveBoat();}


	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerOnExit				( CVehicle* pVehicle );
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);
};

class CTaskVehiclePlayerDriveTrain : public CTaskVehiclePlayerDrive
{
public:

	// Constructor/destructor
	CTaskVehiclePlayerDriveTrain();
	~CTaskVehiclePlayerDriveTrain(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_TRAIN; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveTrain();}


	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);
};


//
//
//
class CTaskVehiclePlayerDriveSubmarine : public CTaskVehiclePlayerDrive
{
public:
	// Constructor/destructor
	CTaskVehiclePlayerDriveSubmarine();
	~CTaskVehiclePlayerDriveSubmarine(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_SUBMARINE; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveSubmarine();}


	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* pVehicle);
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerOnExit				(CVehicle* pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);
};

//
//
//
class CTaskVehiclePlayerDriveSubmarineCar : public CTaskVehiclePlayerDriveAutomobile
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Drive,
		State_NoDriver,
		State_SubmarineMode,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePlayerDriveSubmarineCar();
	~CTaskVehiclePlayerDriveSubmarineCar();

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_SUBMARINECAR; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveSubmarineCar();}


	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* pVehicle);
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);

	FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );

private:
	void		SubmarineMode_OnEnter	();
	FSM_Return	SubmarineMode_OnUpdate	(CVehicle* pVehicle);
};


class CTaskVehiclePlayerDriveAmphibiousAutomobile : public CTaskVehiclePlayerDriveAutomobile
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Drive,
		State_NoDriver,
		State_BoatMode,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePlayerDriveAmphibiousAutomobile();
	~CTaskVehiclePlayerDriveAmphibiousAutomobile(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AMPHIBIOUS_AUTOMOBILE; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveSubmarineCar();}


	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* pVehicle);
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);

	FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );

private:
	void		BoatMode_OnEnter	();
	FSM_Return	BoatMode_OnUpdate	(CVehicle* pVehicle);
};


//
//
//
class CTaskVehiclePlayerDrivePlane : public CTaskVehiclePlayerDrive
{
public:
	static float ms_fPLANE_MAXSPEEDNOW;

	static dev_float ms_fPLANE_STEERING_CURVE_POW_MOUSE;

	// Constructor/destructor
	CTaskVehiclePlayerDrivePlane();
	~CTaskVehiclePlayerDrivePlane(){}

	int			    GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_PLANE; }
	aiTask*		    Copy() const {return rage_new CTaskVehiclePlayerDrivePlane();}

    virtual void    Drive_OnEnter(CVehicle* pVehicle);

	void			ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		    ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		    ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);
    void            ProcessPlayerControlInputsForBrakeAndGasPedal   (CControl* pControl, bool bForceBrake, CVehicle *pVehicle);

	u32 m_TimeOfLastLandingGearToggle;

private:
	bool m_ThrottleControlHasBeenActive;

#if RSG_PC
	bool m_MouseSteeringInput;
#endif
};



//
//
//this class is the base class to heli and autogyro
class CTaskVehiclePlayerDriveRotaryWingAircraft : public CTaskVehiclePlayerDrive
{
public:
	static dev_float ms_fHELI_THROTTLE_CONTROL_DAMPING;
	static dev_float ms_fHELI_AUTO_THROTTLE_FALLOFF;

	static dev_float ms_fHELI_STEERING_CURVE_POW_MOUSE;

	static dev_float ms_fRollYawSpeedThreshold;
	static dev_float ms_fRollYawSpeedBlendRate;
	static dev_float ms_fRollYawMult;

	// Constructor/destructor
	CTaskVehiclePlayerDriveRotaryWingAircraft( );
	virtual ~CTaskVehiclePlayerDriveRotaryWingAircraft(){}

	virtual void	ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	virtual void	ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);

	u32 m_TimeOfLastLandingGearToggle;

#if RSG_PC
	bool m_MouseSteeringInput;
#endif
};

//
//
//
class CTaskVehiclePlayerDriveHeli : public CTaskVehiclePlayerDriveRotaryWingAircraft
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Drive,
		State_NoDriver,
		State_Hover,
        State_InactiveHover,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehiclePlayerDriveHeli();
	~CTaskVehiclePlayerDriveHeli(){}

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_HELI; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveHeli();}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );

	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);

    void		ProcessHoverDriverInputsForPlayer		(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);

	//State_Hover
	void		Hover_OnEnter	(CVehicle* pVehicle);
	FSM_Return	Hover_OnUpdate	(CVehicle* pVehicle);
    void		Hover_OnExit	(CVehicle* pVehicle);

    //State_InactiveHover
    void		InactiveHover_OnEnter	(CVehicle* pVehicle);
    FSM_Return	InactiveHover_OnUpdate	(CVehicle* pVehicle);

private:
    bool m_bJustEnteredHoverMode;
    bool m_bJustLeftHoverMode;
};


//
//
//
class CTaskVehiclePlayerDriveAutogyro : public CTaskVehiclePlayerDriveRotaryWingAircraft
{
public:
	// Constructor/destructor
	CTaskVehiclePlayerDriveAutogyro();
	~CTaskVehiclePlayerDriveAutogyro(){}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_AUTOGYRO; }
	aiTask*		Copy() const {return rage_new CTaskVehiclePlayerDriveAutogyro();}


	void		ProcessDriverInputsForPlayerOnEnter				(CVehicle* UNUSED_PARAM(pVehicle)) {}
	void		ProcessDriverInputsForPlayerOnUpdate			(CPed *pPlayerPed, CControl *pControl, CVehicle *pVehicle);
	void		ProcessDriverInputsForPlayerInactiveOnUpdate	(CControl *pControl, CVehicle *pVehicle);
};


//
//
//
class CTaskVehicleNoDriver : public CTaskVehicleMissionBase
{
public:

	typedef enum
	{
		NO_DRIVER_TYPE_ABANDONED,
		NO_DRIVER_TYPE_WRECKED,
		NO_DRIVER_TYPE_PLAYER_DISABLED,
        NO_DRIVER_TYPE_TOWED,
		//this type was added in order to allow
		//scripters to create a plane mid-air and task it
		//on a later frame. it should behave identically to
		//NO_DRIVER_TYPE_ABANDONED, except for that the 
		//priority of the crash task is PRIMARY instead of CRASH,
		//so script commands can override it with a GOTO or
		//something else later
		NO_DRIVER_TYPE_INITIALLY_CREATED,
		NUM_NO_DRIVER_TYPES
	}NoDriverType;

	typedef enum
	{
		State_Invalid = -1,
		State_NoDriver,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleNoDriver(NoDriverType noDriverType = NO_DRIVER_TYPE_ABANDONED);
	~CTaskVehicleNoDriver(){}

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_NO_DRIVER; }
	aiTask*			Copy() const {return rage_new CTaskVehicleNoDriver(m_eNoDriverType);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_NoDriver;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleNoDriver>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_NODRIVER_TYPE = datBitsNeeded<NUM_NO_DRIVER_TYPES>::COUNT;

		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_eNoDriverType), SIZEOF_NODRIVER_TYPE, "No driver type");
	}

private:

	// FSM function implementations
	//State_NoDriver
	FSM_Return	NoDriver_OnUpdate			(CVehicle* pVehicle);

	void		ProcessAutomobile			(CVehicle* pVehicle);
	void		ProcessPlane				(CPlane* pPlane);
	void		ProcessBoat					(CVehicle* pBoat);
    void		ProcessSub					(CSubmarine* pSub);
	void		ProcessRotaryWingAirfcraft	(CRotaryWingAircraft* pVehicle);
	void		ProcessBike					(CBike* pBike);

	NoDriverType m_eNoDriverType;

};
#endif
