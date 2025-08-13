//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_PURSUE_CRIMINAL_H
#define INC_TASK_PURSUE_CRIMINAL_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "modelinfo/VehicleModelInfo.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/System/Tuning.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Motion/TaskMotionBase.h"

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CVehicle;
class CPed;


class CTaskPursueCriminal : public CTask
{
public:
	CTaskPursueCriminal( CEntity* pPed, bool instantlychase = false, bool leader = false );
	virtual ~CTaskPursueCriminal();

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float	m_MinDistanceToFindVehicle;
		float	m_MaxDistanceToFindVehicle;
		float	m_MaxHeightDifference;
		float	m_DotProductFacing;
		float	m_DotProductBehind;
		int		m_DistanceToFollowVehicleBeforeFlee;
		float	m_DistanceToSignalVehiclePursuitToCriminal;
		int		m_TimeToSignalVehiclePursuitToCriminalMin;
		int		m_TimeToSignalVehiclePursuitToCriminalMax;
		bool	m_DrawDebug;
		bool	m_AllowPursuePlayer;
		float	m_CriminalVehicleMinStartSpeed;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: FSM states
	enum ePursueCriminalState
	{
		State_Start,
		State_ApproachSuspectInVehicle,
		State_RespondToFleeInVehicle,
		State_ReturnToVehicle,
		State_Finish
	};

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

    // RETURNS: The task Id. Each task class has an Id for quick identification
    virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_PURSUE_CRIMINAL; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskPursueCriminal(m_pCriminal.Get(),m_bInstantlyChase,m_bLeader); }

#if !__FINAL

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;
#endif // !__FINAL

	//PURPOSE: Returns pointer to cop vehicle
	CVehicle* GetPoliceVehicle(CPed* pPed);

	CEntity* GetCriminal() const { return m_pCriminal; }

	eRandomEvent	GetPursueType() { return m_Type; }

	static void GiveTaskToPed(CPed& rPed, CEntity* pCriminal, bool bInstantlyChase, bool bLeader);

protected:

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	virtual bool ProcessMoveSignals();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function

	FSM_Return PursueCriminal_Start_OnUpdate();

	void ApproachSuspectInVehicle_OnEnter();
	FSM_Return ApproachSuspectInVehicle_OnUpdate();

	void RespondToFleeInVehicle_OnEnter();
	FSM_Return RespondToFleeInVehicle_OnUpdate();
	void RespondToFleeInVehicle_OnExit();

	void RespondToOnFootCriminal_OnEnter();
	FSM_Return RespondToOnFootCriminal_OnUpdate();

	void ReturnToVehicle_OnEnter();
	FSM_Return ReturnToVehicle_OnUpdate();

	//PURPOSE: Gets the criminal ped based on the passed in criminal and type
	CPed* GetCriminalPed();

	//PURPOSE: Returns the distance between the ped/cop running this task and the criminal.
	float GetDistanceToCriminalVehicle();

	void CheckCriminalTask(CPed& rCriminal);
	void CheckCriminalTasks();
	void CheckPlayerAudio(CPed* pPed);

	void CullExtraFarAway(CPed& rPed);
	void CullExtraFarAway(CVehicle& rVehicle);
	void ProcessFlags(CVehicle& rVehicle);

	RegdEnt 	m_pCriminal;

	RegdIncident m_pArrestIncident;

	//Criminals vehicle
	RegdVeh		m_pCriminalVehicle;

	//Timer used to allow police vehicle to follow criminal vehicle before pursuit.
	CTaskGameTimer	m_PoliceFollowTimer;

	//Timer used to delay cop state change so the ped doesn't instantly react and instead appear to react to visual actions.
	CTaskGameTimer	m_DelayStateChangeTimer;

	//Type of pursuit
	eRandomEvent		m_Type;

	bool m_bInstantlyChase;

	bool m_bLeader;
};


#endif // INC_TASK_PURSUE_CRIMINAL_H
