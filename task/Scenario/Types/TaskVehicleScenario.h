//
// task/Scenario/Types/TaskVehicleScenario.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_TYPES_TASK_VEHICLE_SCENARIO_H
#define TASK_SCENARIO_TYPES_TASK_VEHICLE_SCENARIO_H

#include "task/Scenario/TaskScenario.h"
#include "Task/System/Tuning.h"

class CScenarioVehicleInfo;

//-----------------------------------------------------------------------------

class CTaskUseVehicleScenario : public CTaskScenario
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	When using CTaskBringVehicleToHalt, this is the distance we pass in.
		float	m_BringVehicleToHaltDistance;

		// PURPOSE:	Amount of randomness applied to the idle time, as a fraction.
		float	m_IdleTimeRandomFactor;

		// PURPOSE:	Distance from the destination where we may start to slow down, if wanting to stop.
		float	m_SlowDownDist;

		// PURPOSE:	The speed to slow down to once getting close to the destination, in m/s.
		float	m_SlowDownSpeed;

		// PURPOSE:	Distance at which we let the vehicle goto task switch to straight line navigation.
		float	m_SwitchToStraightLineDist;

		// PURPOSE:	The distance from the destination at which we consider ourselves there.
		float	m_TargetArriveDist;

		// PURPOSE: Special arrival distance used for planes.
		float	m_PlaneTargetArriveDist;

		// PURPOSE: Special arrival distance used for helis.
		float	m_HeliTargetArriveDist;

		// PURPOSE: Special arrival distance used for boats.
		float	m_BoatTargetArriveDist;
		
		// PURPOSE: Special arrival distance used for a taxi plane on the ground
		float   m_PlaneTargetArriveDistTaxiOnGround;

		// PURPOSE: For the driving subtask, the arrival distance when using taxi scenario points
		float	m_PlaneDrivingSubtaskArrivalDist;

		// PURPOSE: For boat avoidance settings, in degrees
		float	m_BoatMaxAvoidanceAngle;

		// PURPOSE:	Max search distance for pathfinding on roads, for CTaskVehicleGoToAutomobileNew/CPathNodeRouteSearchHelper.
		//			A lower value helps to limit the risk of performance problem in case of pathfinding failure.
		u16		m_MaxSearchDistance;

		PAR_PARSABLE;
	};


	// State
	enum State
	{
		State_Start = 0,
		State_DrivingToScenario,
		State_UsingScenario,
		State_Finish,
		State_Failed,

		kNumStates
	};

	// Flags
	enum
	{
		VSF_NavUseNavMesh		= BIT0,		// Allow use of navmesh when driving.
		VSF_NavUseRoads			= BIT1,		// Allow use of roads (plus navmesh, if needed) when driving.
		VSF_WillContinueDriving	= BIT2,		// If true, the user wants us to drive somewhere else after we're done with the current point.
		VSF_HasFinishedChain    = BIT3,		// We're done with following the chain and now just driving off into distance

		VSF_MaxFlagBit = VSF_HasFinishedChain
	};

	explicit CTaskUseVehicleScenario(s32 iScenarioIndex, CScenarioPoint* pScenarioPoint, u32 flags, float driveSpeed, CScenarioPoint* pPrevScenarioPoint);
	explicit CTaskUseVehicleScenario(const CTaskUseVehicleScenario& other);

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskUseVehicleScenario(*this); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_USE_VEHICLE_SCENARIO; }

	void SetWillContinueDriving(bool b) { m_Flags.ChangeFlag(VSF_WillContinueDriving, b); }
	void SetScenarioPointNxt(CScenarioPoint* point, float driveSpeed);//used for figureing out turn speeds and such.
	void SetScenarioPointPrev(CScenarioPoint* point) {m_pScenarioPointPrev = point; m_iCachedPrevWorldScenarioPointIndex =-1;}

	const CScenarioPoint* GetScenarioPointPrev() const {return m_pScenarioPointPrev;}

	const CConditionalAnimsGroup* GetScenarioConditionalAnimsGroup() const;

	// PURPOSE:	Get the time to use the scenario, in seconds, before adding random variation.
	float GetBaseTimeToUse() const;

	// PURPOSE: Set the timeout on the scenario point.
	virtual void SetTimeToLeave(float fTime) { m_IdleTime = fTime; }

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	void ClearCachedPrevAndNextWorldScenarioPointIndex() { m_iCachedPrevWorldScenarioPointIndex = -1; m_iCachedNextWorldScenarioPointIndex = -1; }

protected:
	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);	
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                IsInScope(const CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);

	mutable s32		m_iCachedPrevWorldScenarioPointIndex;	// Used for create clone queriable update
	mutable s32		m_iCachedNextWorldScenarioPointIndex;	// Used for create clone queriable update

	bool	AreClonesPrevNextScenariosValid();
	// PURPOSE: Called once per frame before FSM update
	// RETURNS: If FSM_Quit is returned the state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: The default state that the task will start in after resuming from abort
	virtual s32 GetDefaultStateAfterAbort() const;

	FSM_Return Start_OnUpdate();

	void DrivingToScenario_OnEnter();
	FSM_Return DrivingToScenario_OnUpdate();

	void UsingScenario_OnEnter();
	FSM_Return UsingScenario_OnUpdate();

	void FinishScenario_OnEnter();
	FSM_Return FinishScenario_OnUpdate();

	void Failed_OnEnter();
	FSM_Return Failed_OnUpdate();

	// PURPOSE:	Helper function to get info about the vehicle scenario to use.
	const CScenarioVehicleInfo &GetScenarioInfo() const;

	// PURPOSE:	Helper function to compute the remaining squared distance to the target.
	ScalarV_Out ComputeDistSqToTarget() const;

	// PURPOSE:	Helper function to see if we crossed the plain of the scenario point
	bool HasCrossedArrivalPlane() const;
	
	// PURPOSE: If true, in taxi arrival range of the point 
	bool HasPlaneTaxiedToArrivalPosition(ScalarV_In distSq) const;

	// PURPOSE:	If true, we should be slowing down at this time, because of being close enough to the destination.
	bool ShouldSlowDown() const;

	// PURPOSE:	If true, we should stop at the destination and enter the UsingScenario state.
	bool ShouldStopAtDestination() const;

	// PURPOSE:	Create the subtask to drive to the scenario point.
	bool CreateDriveSubTask(float desiredDriveSpeed);

	// PURPOSE:	Update running subtasks for driving, with a new speed.
	void UpdateDriveSubTask(float desiredDriveSpeed);

	void ConsiderTellingOtherCarsToPassUs();

	bool ShouldVehiclePauseDueToUnloadedNodes() const;

	// We don't want to use the bonnet position for helicopters.
	bool ShouldUseBonnetPositionForDistanceChecks() const;

	// If controlling a helicopter, this determines if it should hover to the point to avoid trying to rotate for small heading adjustments.
	bool ShouldHoverToPoint(const Vector3& vTarget, float fTolerance) const;

	// PURPOSE:	Speed to drive at, in m/s.
	float			m_DriveSpeed;

	// PURPOSE:	Speed to drive at, in m/s.
	float			m_DriveSpeedNxt;

	RegdScenarioPnt m_pScenarioPointNxt;

	RegdScenarioPnt m_pScenarioPointPrev;

	// PURPOSE:	Time to stay in play ambients / idle.
	float			m_IdleTime;

	// PURPOSE:	Various flags controlling the behavior of the task.
	fwFlags32		m_Flags;

	// PURPOSE:	True if we are currently trying to park.
	bool			m_Parking;

	// PURPOSE:	True if we're currently slowing down.
	bool			m_SlowingDown;

	// PURPOSE: If we require road nodes, but don't have them,
	// set this flag, which will be used to try again for road nodes
	bool			m_bDriveSubTaskCreationFailed;

	// PURPOSE:	Tunables for the class.
	static Tunables sm_Tunables;
};

//-----------------------------------------------------------------------------

// Info for syncing CTaskUseVehicleScenario
class CClonedTaskUseVehicleScenarioInfo : public CClonedScenarioFSMInfo
{
public:

	CClonedTaskUseVehicleScenarioInfo(s32 state,
								s16 scenarioType,
								CScenarioPoint* pScenarioPoint,
								s32 prevScenarioPointIndex,
								s32 nextScenarioPointIndex,
								int worldScenarioPointIndex,
								u8 flags,
								float driveSpeed,
								float driveSpeedNxt);
	CClonedTaskUseVehicleScenarioInfo();
	~CClonedTaskUseVehicleScenarioInfo(){}

	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskUseVehicleScenario::kNumStates - 1>::COUNT;}
	virtual s32 GetTaskInfoType( ) const { return INFO_TYPE_USE_VEHICLE_SCENARIO; }
	virtual bool HasState() const { return true; }
	virtual const char* GetStatusName(u32) const { return "Task Use Vehicle Scenario State";}

	float GetDriveSpeed() const { return m_DriveSpeed; }
	float GetDriveSpeedNxt() const { return m_DriveSpeedNxt; }
	s32 GetFlags() const { return static_cast<u32>(m_Flags); }
	s32 GetPrevScenarioPointIndex() const { return m_PrevScenarioPointIndex; }
	s32 GetNextScenarioPointIndex() const { return m_NextScenarioPointIndex; }

	CScenarioPoint* GetPrevScenarioPoint();
	CScenarioPoint* GetNextScenarioPoint();

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask *CreateLocalTask(fwEntity* pEntity);

	void Serialise(CSyncDataBase& serialiser)
	{
		CClonedScenarioFSMInfo::Serialise(serialiser);

		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_DriveSpeed, 100.0f, SIZEOF_DRIVE_SPEED, "Drive Speed");

		Assertf(SIZEOF_FLAGS <=(sizeof(m_Flags)<<4),"SIZEOF_FLAGS %d too big should be %" SIZETFMT "d",SIZEOF_FLAGS ,(sizeof(m_Flags)<<4) );

		SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS, "Task Use Vehicle Scenario Flags");

		bool bHasPrevScenario = m_PrevScenarioPointIndex!=-1;
		bool bHasNextScenario = m_NextScenarioPointIndex!=-1;

		SERIALISE_BOOL(serialiser, bHasPrevScenario, "has previous scenario pt");
		SERIALISE_BOOL(serialiser, bHasNextScenario, "has next scenario pt");

		if(bHasPrevScenario || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_PrevScenarioPointIndex, SIZEOF_SCENARIO_ID, "Previous Scenario id");
		}
		else
		{
			m_PrevScenarioPointIndex = -1;
		}
		if(bHasNextScenario || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_NextScenarioPointIndex, SIZEOF_SCENARIO_ID, "Next Scenario id");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_DriveSpeedNxt, 100.0f, SIZEOF_DRIVE_SPEED, "Drive Speed Next");
		}
		else
		{
			m_NextScenarioPointIndex = -1;
		}
	}

private:
	static const unsigned int SIZEOF_DRIVE_SPEED = 8;
	static const unsigned int SIZEOF_FLAGS = datBitsNeeded<CTaskUseVehicleScenario::VSF_MaxFlagBit>::COUNT;

	float	m_DriveSpeed;
	float	m_DriveSpeedNxt;
	s32		m_PrevScenarioPointIndex;			// the index into the world Scenario store ,expected to be a always world scenario point
	s32		m_NextScenarioPointIndex;			// the index into the world Scenario store ,expected to be a always world scenario point
	u8		m_Flags;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_TYPES_TASK_VEHICLE_SCENARIO_H

// End of file 'task/Scenario/Types/TaskVehicleScenario.h'
