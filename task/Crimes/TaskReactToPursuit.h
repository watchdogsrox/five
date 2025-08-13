//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_REACT_TO_PURSUIT_H
#define INC_TASK_REACT_TO_PURSUIT_H

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

#define MAX_COPS 3

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls a ped stealing a vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskReactToPursuit : public CTask
{
public:
    CTaskReactToPursuit( CPed* pCop );
    ~CTaskReactToPursuit();

    // PURPOSE:	Task tuning parameter struct.
    struct Tunables : public CTuning
    {
        Tunables();

		int		m_MinTimeToFleeInVehicle;
		int		m_MaxTimeToFleeInVehicle;
		float	m_FleeSpeedInVehicle;
	
        PAR_PARSABLE;
    };

    // PURPOSE: Instance of tunable task params
    static Tunables	ms_Tunables;	

    // PURPOSE: FSM states
    enum eReactToPursuitState
    {
		State_Start,
		State_FleeInVehicle,
		State_LeaveVehicle,
		State_CombatOnFoot,
		State_ReturnToVehicle,
		State_FleeArea,
        State_Finish
    };

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

    // RETURNS: The task Id. Each task class has an Id for quick identification
    virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_REACT_TO_PURSUIT; }

    // PURPOSE: Used to produce a copy of this task
    // RETURNS: A copy of this task
	virtual aiTask* Copy() const;

	virtual	void CleanUp();

#if !__FINAL

    // PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

#endif // !__FINAL

	void AddCop(CPed* pCop);

	static void GiveTaskToPed(CPed& rPed, CPed* pCop);

protected:

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	virtual bool ProcessMoveSignals();

    // PURPOSE: FSM implementation
    virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

    // RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
    virtual s32	GetDefaultStateAfterAbort() const { return State_Start; }

private:

    ////////////////////////////////////////////////////////////////////////////////

    // PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
    FSM_Return ReactToPursuit_OnUpdate();

	void FleeInVehicle_OnEnter();
	FSM_Return FleeInVehicle_OnUpdate();
	void FleeInVehicle_OnExit();

	void LeaveVehicle_OnEnter();
	FSM_Return LeaveVehicle_OnUpdate();

	void CombatOnFoot_OnEnter();
	FSM_Return CombatOnFoot_OnUpdate();

	void ReturnToVehicle_OnEnter();
	FSM_Return ReturnToVehicle_OnUpdate();

	void FleeArea_OnEnter();
	FSM_Return FleeArea_OnUpdate();

	CPed* GetBestCop(bool bClosest = false);
	
    atFixedArray<RegdPed, MAX_COPS> m_pCop;

	CTaskGameTimer	m_FleeTimer;

	//No distance check on fleeing on foot
	bool m_bForceFlee; 

	bool m_bFleeOnFootFromVehicle;

	bool m_bPreviousDisablePretendOccupantsCached;
    
};

#endif // INC_TASK_STEAL_VEHICLE_H
