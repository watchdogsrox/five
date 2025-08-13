#ifndef TASK_VEHICLE_DEAD_DRIVER_H
#define TASK_VEHICLE_DEAD_DRIVER_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "Task/System/Tuning.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"

//
//
//
class CTaskVehicleDeadDriver : public CTaskVehicleMissionBase
{
public:

	enum SteerAngleControl
	{
		SAC_Retain,
		SAC_Minimum,
		SAC_Maximum,
		SAC_Randomize
	};
	
	enum ThrottleControl
	{
		TC_Retain,
		TC_Minimum,
		TC_Maximum,
		TC_Randomize
	};
	
	enum BrakeControl
	{
		BC_Retain,
		BC_Minimum,
		BC_Maximum,
		BC_Randomize
	};
	
	enum HandBrakeControl
	{
		HBC_Retain,
		HBC_Yes,
		HBC_No,
		HBC_Randomize
	};

	struct Tunables : CTuning
	{
		Tunables();

		float				m_SwerveTime;
		SteerAngleControl	m_SteerAngleControl;
		float				m_MinSteerAngle;
		float				m_MaxSteerAngle;
		ThrottleControl		m_ThrottleControl;
		float				m_MinThrottle;
		float				m_MaxThrottle;
		BrakeControl		m_BrakeControl;
		float				m_MinBrake;
		float				m_MaxBrake;
		HandBrakeControl	m_HandBrakeControl;

		PAR_PARSABLE;
	};
	
	typedef enum
	{
		State_Invalid = -1,
		State_Dead = 0,
		State_Crash,
		State_Exit
	} VehicleControlState;
	
	enum VehicleDeadDriverFlags
	{
		VDDF_DeathControlsAreValid	= BIT0
	};

	// Constructor/destructor
	CTaskVehicleDeadDriver(CEntity *pCulprit = NULL);
	~CTaskVehicleDeadDriver();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_DEAD_DRIVER; }
	aiTask*			Copy() const {return rage_new CTaskVehicleDeadDriver(m_pCulprit);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Dead;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }

private:

	// FSM function implementations
	// State_Dead
	FSM_Return	Dead_OnUpdate			(CVehicle* pVehicle);
	// State_Crash
	FSM_Return	Crash_OnUpdate			(CVehicle* pVehicle);

	void		ControlAIVeh_DeadDriver	(CVehicle* pVeh, CVehControls* pVehControls);
	
	void		GenerateDeathControls(CVehControls* pVehControls);
	
	CVehControls	m_DeathControls;
	RegdEnt			m_pCulprit;
	
	fwFlags8		m_uFlags;
	float			m_fTimeAfterDeath;
	u32				m_uSwerveTime;
	
	static Tunables sm_Tunables;
};


#endif
