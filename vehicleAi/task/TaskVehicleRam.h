#ifndef TASK_VEHICLE_RAM_H
#define TASK_VEHICLE_RAM_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

//=========================================================================
// CTaskVehicleRam
//=========================================================================

class CTaskVehicleRam : public CTaskVehicleMissionBase
{

public:

	enum ConfigFlags
	{
		CF_Continuous = BIT0,
	};

private:

	enum RunningFlags
	{
		RF_IsMakingContact = BIT0,
	};

public:

	struct Tunables : public CTuning
	{
		Tunables();
		
		float m_BackOffTimer;
		float m_MinBackOffDistance;
		float m_MaxBackOffDistance;
		float m_CruiseSpeedMultiplierForMinBackOffDistance;
		float m_CruiseSpeedMultiplierForMaxBackOffDistance;

		PAR_PARSABLE;
	};

	enum VehicleRamState
	{
		State_Start = 0,
		State_Ram,
		State_Finish,
	};

	CTaskVehicleRam(const sVehicleMissionParams& rParams, float fStraightLineDistance = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST, fwFlags8 uConfigFlags = 0);
	~CTaskVehicleRam();
	
public:

	fwFlags8&	GetConfigFlags()		{ return m_uConfigFlags; }
	u32			GetNumContacts() const	{ return m_uNumContacts; }

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const { return rage_new CTaskVehicleRam(m_Params, m_fStraightLineDistance, m_uConfigFlags); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_RAM; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleRam>; }

private:

	FSM_Return	Start_OnUpdate();
	void		Ram_OnEnter();
	FSM_Return	Ram_OnUpdate();

private:

	const CVehicle*	GetTargetVehicle() const;
	bool			IsMakingContact() const;
	void			UpdateContact();
	void			UpdateCruiseSpeed();
	
private:

	float		m_fStraightLineDistance;
	float		m_fBackOffTimer;
	u32			m_uNumContacts;
	u32			m_uLastTimeMadeContact;
	fwFlags8	m_uConfigFlags;
	fwFlags8	m_uRunningFlags;

private:

	static Tunables	sm_Tunables;

};

#endif
