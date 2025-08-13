#ifndef TASK_VEHICLE_SHOT_TIRE_H
#define TASK_VEHICLE_SHOT_TIRE_H

// Game headers
#include "renderer/HierarchyIds.h"
#include "Task/System/Tuning.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"

//=========================================================================
// CTaskVehicleShotTire
//=========================================================================

class CTaskVehicleShotTire : public CTaskVehicleMissionBase
{

public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float	m_MaxTimeInSwerve;
		float	m_MinSpeedInSwerve;
		float	m_MinSpeedToApplyTorque;
		float	m_MaxDotToApplyTorque;
		float	m_TorqueMultiplier;
		
		PAR_PARSABLE;
	};
	
	enum VehicleShotTireRunningFlags
	{
		VSTRF_WheelIsOnRight	= BIT0,
		VSTRF_ApplyTorque		= BIT1,
	};

	enum VehicleShotTireState
	{
		State_Start = 0,
		State_Swerve,
		State_Finish
	};
	
	CTaskVehicleShotTire(eHierarchyId nTire);
	virtual ~CTaskVehicleShotTire();

	aiTask*	Copy() const;
	int		GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_SHOT_TIRE; }
	s32		GetDefaultStateAfterAbort()	const { return State_Finish; }
	
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Swerve_OnEnter();
	FSM_Return	Swerve_OnUpdate();
	
private:

	bool	IsSwerveComplete() const;
	bool	ShouldApplyTorque() const;
		
private:

	CVehControls	m_SwerveControls;
	Vec3V			m_vInitialVehicleForward;
	float			m_fTorque;
	eHierarchyId	m_nTire;
	fwFlags8		m_uRunningFlags;
	
private:

	static Tunables	ms_Tunables;

};


#endif
