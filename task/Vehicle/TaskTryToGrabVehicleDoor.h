//
// Task/Physics/TaskTryToGrabVehicleDoor.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_TRY_TO_GRAB_VEHICLE_DOOR_H
#define INC_TASK_TRY_TO_GRAB_VEHICLE_DOOR_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////

class CTaskTryToGrabVehicleDoor : public CTask
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		u32		m_MinGrabTime;
		u32		m_MaxGrabTime;

		float	m_MaxHandToHandleDistance;
	
		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define TRY_TO_GRAB_VEHICLE_DOOR_STATES(X)	X(State_Start),					\
												X(State_GrabVehicleDoor),		\
												X(State_AttachedToDoorHandle),	\
												X(State_Finish)

	// PURPOSE: FSM states
	enum eTaskState
	{
		TRY_TO_GRAB_VEHICLE_DOOR_STATES(DEFINE_TASK_ENUM)
	};

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(CVehicle* pVehicle, CPed* pPed, s32 iEntryPointIndex);

	static bool IsWithinGrabbingDistance(const CPed* pPed, const CVehicle* pVehicle, s32 iEntryPointIndex);

	////////////////////////////////////////////////////////////////////////////////

	CTaskTryToGrabVehicleDoor(CVehicle* pVehicle, s32 iEntryPointIndex);
	virtual ~CTaskTryToGrabVehicleDoor();

	// PURPOSE: FSM implementation
	virtual aiTask*		Copy() const { return rage_new CTaskTryToGrabVehicleDoor(m_pVehicle, m_iEntryPointIndex); }
	virtual s32			GetTaskTypeInternal() const	{ return CTaskTypes::TASK_TRY_TO_GRAB_VEHICLE_DOOR; }

	virtual bool UseCustomGetup() { return true; }
	virtual bool AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed );

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: FSM implementation
	virtual	void		CleanUp();
	virtual s32			GetDefaultStateAfterAbort() const { return State_Finish; }
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return 	Start_OnUpdate();
	FSM_Return  GrabVehicleDoor_OnEnter();
	FSM_Return  GrabVehicleDoor_OnUpdate();

    //PURPOSE: Sets up an NM balance tasks grab helper - static so functionality can be reused externally as a function pointer
    static void SetupBalanceGrabHelper(aiTask *pTaskTryToGrabVehicleDoor, aiTask *pTaskNM);

	////////////////////////////////////////////////////////////////////////////////

private:

	////////////////////////////////////////////////////////////////////////////////

	RegdVeh		m_pVehicle;
	s32			m_iEntryPointIndex;

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
public:
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, TRY_TO_GRAB_VEHICLE_DOOR_STATES)
#endif // !__FINAL

};

#endif // INC_TASK_TRY_TO_GRAB_VEHICLE_DOOR_H
