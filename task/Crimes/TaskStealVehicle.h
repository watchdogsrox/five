//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_STEAL_VEHICLE_H
#define INC_TASK_STEAL_VEHICLE_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "modelinfo/VehicleModelInfo.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/System/Tuning.h"
#include "Task/Crimes/RandomEventManager.h"

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CVehicle;
class CPed;

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls a ped stealing a vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskStealVehicle : public CTask
{
public:
    CTaskStealVehicle( CVehicle* pVehicle );
    ~CTaskStealVehicle();

    // PURPOSE:	Task tuning parameter struct.
    struct Tunables : public CTuning
    {
        Tunables();

        float   m_MaxDistanceToFindVehicle;
		float   m_MaxDistanceToPursueVehicle;
		float	m_DistanceToRunToVehicle;
        bool    m_CanStealPlayersVehicle;
		bool	m_CanStealCarsAtLights;
		bool	m_CanStealParkedCars;
		bool    m_CanStealStationaryCars;

        PAR_PARSABLE;
    };

    // PURPOSE: Instance of tunable task params
    static Tunables	ms_Tunables;	

    // PURPOSE: FSM states
    enum eStealVehicleState
    {
        State_Stealing_Vehicle,
		State_Stolen_Vehicle,
        State_Finish
    };

    // RETURNS: The task Id. Each task class has an Id for quick identification
    virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_STEAL_VEHICLE; }

    // PURPOSE: Used to produce a copy of this task
    // RETURNS: A copy of this task
    virtual aiTask* Copy() const { return rage_new CTaskStealVehicle(m_pVehicle); }

#if !__FINAL

    // PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

#endif // !__FINAL

protected:

    // PURPOSE: FSM implementation
    virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

    // RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
    virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

private:

    ////////////////////////////////////////////////////////////////////////////////

    // PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
    FSM_Return Stealing_Vehicle_OnEnter();

    FSM_Return Stealing_Vehicle_OnUpdate();

	FSM_Return Stole_Vehicle_OnEnter();

	FSM_Return Stole_Vehicle_Update();

    FSM_Return Finish_OnUpdate();

    RegdVeh	m_pVehicle;

};

////////////////////////////////////////////////////////////////////////////////
// CStealVehicleCrime
////////////////////////////////////////////////////////////////////////////////

class CStealVehicleCrime : public CDefaultCrimeInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CStealVehicleCrime, CDefaultCrimeInfo);
public:

	CVehicle* IsApplicable( CPed* pPed );
	virtual CTask* GetPrimaryTask( CVehicle* pVehicle );

private:

	PAR_PARSABLE;
};


#endif // INC_TASK_STEAL_VEHICLE_H
