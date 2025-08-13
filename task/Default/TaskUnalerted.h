//
// task/Default/TaskUnalerted.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_DEFAULT_TASK_UNALERTED_H
#define TASK_DEFAULT_TASK_UNALERTED_H

#include "Task/Helpers/PropManagementHelper.h"
#include "Task/Helpers/PavementFloodFillHelper.h"
#include "task/Scenario/ScenarioChaining.h"
#include "Task/Scenario/ScenarioFinder.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Tuning.h"

#define POST_HANGOUT_SCENARIOS_SIZE 2 // How many scenarios peds can move to after the hangout scenario [8/9/2012 mdawe]

// Forward declarations

class CVehicle;

//-----------------------------------------------------------------------------

class CTaskUnalerted : public CTaskFSMClone
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Minimum time after an unsuccessful scenario search before we try again.
		float		m_ScenarioDelayAfterFailureMin;

		// PURPOSE:	Maximum time after an unsuccessful scenario search before we try again.
		float		m_ScenarioDelayAfterFailureMax;

		// PURPOSE:	Time after an unsuccessful scenario search before trying again, if we're not
		//			moving (when in roam mode).
		float		m_ScenarioDelayAfterFailureWhenStationary;

		// PURPOSE:	Time after not being allowed to search for scenarios, until we try again.
		float		m_ScenarioDelayAfterNotAbleToSearch;

		// PURPOSE:	Minimum time after successfully using a scenario, before we try to search for scenarios again.
		float		m_ScenarioDelayAfterSuccessMin;

		// PURPOSE:	Maximum time after successfully using a scenario, before we try to search for scenarios again.
		float		m_ScenarioDelayAfterSuccessMax;

		// PURPOSE:	Minimum time after the task starts, before we try to search for a scenario to use.
		float		m_ScenarioDelayInitialMin;

		// PURPOSE:	Maximum time after the task starts, before we try to search for a scenario to use.
		float		m_ScenarioDelayInitialMax;

		// PURPOSE: Minimum time, in seconds, before a passenger should check the driver's conditional anim group.
		float		m_TimeBeforeDriverAnimCheck;
		
		// PURPOSE: Minimum time, in seconds, between searches for the next scenario point in a chain.
		float		m_TimeBetweenSearchesForNextScenarioInChain;

		// PURPOSE:	Minimum time, in seconds, that must have passed before considering reusing the last point.
		float		m_TimeMinBeforeLastPoint;

		// PURPOSE:	Minimum time, in seconds, that must have passed before considering reusing the last point type.
		float		m_TimeMinBeforeLastPointType;

		// PURPOSE: Defines a radius to perform a flood fill search for nearby pavement to wander on.
		float		m_PavementFloodFillSearchRadius;

		// PURPOSE: Defines a wait time between attempts to exit a vehicle (the delay will prevent infinite loops of state switches in the same game frame)
		float		m_WaitTimeAfterFailedVehExit;

		// PURPOSE: Maximum distance to return to our last used vehicle.
		float		m_MaxDistanceToReturnToLastUsedVehicle;

		PAR_PARSABLE;
	};

	// State
	enum State
	{
		State_Start = 0,
		State_Wandering,
		State_SearchingForScenario,
		State_SearchingForNextScenarioInChain,
		State_UsingScenario,
		State_InVehicleAsDriver,
		State_InVehicleAsPassenger,
		State_ResumeAfterAbort,
		State_HolsteringWeapon,
		State_ExitingVehicle,
		State_ReturningToVehicle,
		State_WaitForVehicleLand,
		State_WanderingWait,
		State_MoveOffToOblivion,
		State_Finish,
		kNumStates
	};
	
	enum RunningFlags
	{
		RF_RelaxNextScenarioInChainConstraints	= BIT0,
		RF_DisableHolsterWeapon									= BIT1,
		RF_HasNearbyPavementToWanderOn					= BIT2,
		RF_WanderAwayFromSpecifiedTarget				= BIT3,
		RF_HasStartedScenarioFinder							= BIT4,
		RF_SearchForAnyNonChainedScenarios			= BIT5,
	};

	explicit CTaskUnalerted(CTask* pScenarioTask = NULL, CScenarioPoint* pScenarioPoint = NULL, int scenarioTypeReal = -1);
	virtual ~CTaskUnalerted();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_UNALERTED; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const;

	virtual	CScenarioPoint* GetScenarioPoint() const;
	virtual int GetScenarioPointType() const;
	
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	virtual CPropManagementHelper* GetPropHelper() { return m_pPropTransitionHelper; }
	virtual const CPropManagementHelper* GetPropHelper() const { return m_pPropTransitionHelper; }

	// PURPOSE: Try and create a prop on starting
	void CreateProp() { m_bCreateProp = true; }

	void SetRoamMode(bool b) { m_bRoamMode = b; }

	void SetKeepMovingWhilstWaitingForFirstWanderPath(bool b)	{ m_bKeepMovingWhenFindingWanderPath = b; }

	void SetShouldWanderAwayFromPlayer(bool bAway)				{ m_bWanderAwayFromThePlayer = bAway; }
	void SetWanderPathFrameDelay(u8 nFrames)					{ m_nFrameWanderDelay = nFrames; }

	CScenarioPoint* GetLastUsedScenarioPoint() const { return m_LastUsedPoint; }

	// PURPOSE:  Helper function for creating an in vehicle task for a passenger of a vehicle.
	static CTask* CreateTaskForPassenger(CVehicle& rVehicle);

	static CTask * SetupDrivingTaskForEmergencyVehicle(const CPed& rPedDriver, CVehicle& rVehicle, const bool bIsFiretruck, const bool bIsAmbulance);

	// PURPOSE:  Helper function for grabbing the driver's conditional anims group (if any).
	static const CConditionalAnimsGroup* GetDriverConditionalAnimsGroup(const CVehicle& rVehicle);

	// PURPOSE: Helper function for checking if a scenario point can be reused.
	// First the ped will try to reuse the real type passed in, if that fails it will try the remaining virtual types.
	// If a new real type is selected then scenarioTypeReal will be updated accordingly.
	static bool CanScenarioBeReused(CScenarioPoint& rPoint, CScenarioCondition::sScenarioConditionData& conditionData, s32& scenarioTypeReal, CScenarioPointChainUseInfo* pExistingUserInfo);

	// PURPOSE: Helper function for checking if a scenario point and scenario real type can be reused
	static bool CanScenarioBeReusedWithCurrentRealType(CScenarioPoint& rPoint, CScenarioCondition::sScenarioConditionData& conditionData,
			s32 scenarioTypeReal, CScenarioPointChainUseInfo* pExistingUserInfo);

	// PURPOSE: Check the point's other virtual types and see if any are valid for the ped to use.
	// If there is, then set scenarioTypeReal to the new value.
	static bool CanScenarioBeReusedWithNewRealType(CScenarioPoint& rPoint, CScenarioCondition::sScenarioConditionData& conditionData,
		s32& scenarioTypeReal, CScenarioPointChainUseInfo* pExistingUserInfo);

	CTask* GetScenarioTask() const
	{	return static_cast<CTask*>(m_pScenarioTask.Get());	}

	CScenarioPointChainUseInfo* GetScenarioChainUserData() const;
	void SetScenarioChainUserInfo(CScenarioPointChainUseInfo* _user);

	void SetScenarioAction(int iAction)		{ m_ScenarioFinder.SetAction(iAction); }
	void SetScenarioNavMode(u8 iNavMode)	{ m_ScenarioFinder.SetNavMode(iNavMode); }
	void SetScenarioNavSpeed(u8 iNavSpeed)	{ m_ScenarioFinder.SetNavSpeed(iNavSpeed); }

	void SetTargetToWanderAwayFrom(Vector3& vTarget);

	void ClearScenarioPointToReturnTo();
#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
	// Clone task implementation for CTaskUnalerted
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
	static bool IsHashPostHangoutScenario(u32 uHash);
#endif
	void SetScenarioMigratedWhileCowering(bool bValue) { m_bScenarioCowering = bValue; }

protected:

	// PURPOSE: Called once per frame before FSM update
	// RETURNS: If FSM_QUIT is returned the state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Handle aborting the task
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// Determine whether or not the task should abort
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// RETURNS: The default state that the task will start in after resuming from abort
	virtual s32 GetDefaultStateAfterAbort() const;

	// PURPOSE:	Should return true if we know that the aborted subtasks won't be needed
	//			if we resume after getting aborted.
	virtual bool ShouldAlwaysDeleteSubTaskOnAbort() const;

	FSM_Return Start_OnUpdate();

	void Wandering_OnEnter();
	FSM_Return Wandering_OnUpdate();

	void SearchingForScenario_OnEnter();
	FSM_Return SearchingForScenario_OnUpdate();
	void SearchingForScenario_OnExit();

	void SearchingForNextScenarioInChain_OnEnter();
	FSM_Return SearchingForNextScenarioInChain_OnUpdate();
	void SearchingForNextScenarioInChain_OnExit();

	void UsingScenario_OnEnter();
	FSM_Return UsingScenario_OnUpdate();
	void UsingScenario_OnExit();

	void InVehicleAsDriver_OnEnter();
	FSM_Return InVehicleAsDriver_OnUpdate();

	void InVehicleAsPassenger_OnEnter();
	FSM_Return InVehicleAsPassenger_OnUpdate();

	void HolsteringWeapon_OnEnter();
	FSM_Return HolsteringWeapon_OnUpdate();

	void ExitingVehicle_OnEnter();
	FSM_Return ExitingVehicle_OnUpdate();

	void ReturningToVehicle_OnEnter();
	FSM_Return ReturningToVehicle_OnUpdate();
	void ReturningToVehicle_OnExit();

	FSM_Return WaitForVehicleLand_OnUpdate();

	FSM_Return WanderingWait_OnUpdate();

	void		ResumeAfterAbort_OnEnter();
	FSM_Return	ResumeAfterAbort_OnUpdate();
	void		ResumeAfterAbort_OnExit();

	void		MoveOffToOblivion_OnEnter();
	FSM_Return	MoveOffToOblivion_OnUpdate();

	void CleanUpScenarioPointUsage();

	void StartPavementFloodFillRequest();
	bool ProcessPavementFloodFillRequest();
	void CleanupPavementFloodFillRequest();

	void StartScenarioFinderSearch();

	bool IsPedInVehicle() const;
	static bool HasVehicleLanded(const CVehicle& vehicle);

	// PURPOSE:	If applicable, prevent a running scenario subtask from ending due to
	//			the scenario use time expiring.
	void MakeScenarioTaskNotLeave();

	bool MaySearchForScenarios() const;

	void SetTimeUntilCheckScenario(float delay);
	void SetTimeUntilCheckScenario(float minDelay, float maxDelay);

	void PickUseScenarioState();
	void PickVehicleState();
	void PickWanderState();

	// PURPOSE:	Check some conditions on a vehicle, so they can be shared between
	//			CanReturnToVehicle() and ShouldReturnToLastUsedVehicle().
	// RETURNS:	False if the vehicle should not be entered.
	bool CheckCommonReturnToVehicleConditions(const CVehicle& veh) const;

	bool CanReturnToVehicle() const;
	bool CloseEnoughForHangoutTransition() const;
	bool ShouldHolsterWeapon() const;
	bool ShouldReturnToLastUsedScenarioPointDueToAgitated() const;
	bool ShouldReturnToLastUsedVehicle() const;
	bool WillDefaultTasksBeGoodForVehicle(const CVehicle& veh) const;
	void UpdatePointToReturnToAfterResume();
	
	void ProcessKeepMovingWhenFindingWanderPath();
	
	bool ShouldWaitWhileSearchingForNextScenarioInChain() const;
	void StartSearchForNextScenarioInChain();

	// PURPOSE: Helper function to determine if the active scenario should quit out because of the player being nearby.
	bool ShouldEndScenarioBecauseOfNearbyPlayer() const;
	
	// PURPOSE: Tell the actively running scenario task to timeout.
	void EndActiveScenario();

	// PURPOSE:	Check if the given scenario point is within a blocking area, and if so,
	//			make the vehicle get removed aggressively.
	// RETURNS:	True if we should also abort following the chain.
	bool HandleEndVehicleScenarioBecauseOfBlockingArea(const CScenarioPoint& pt);

	void RegisterDistress();

	CVehicle* FindVehicleUsingMyChain() const;

	s32 GetRandomScenarioTypeSafeForResume();

	void TryToObtainChainUserInfo(CScenarioPoint& pt, bool reallyNeeded);

	// PURPOSE:	Helper object used to search for scenarios.
	CScenarioFinder	m_ScenarioFinder;

	// PURPOSE:	This is the scenario task that we can be supplied if we want to use it right away.
	RegdaiTask		m_pScenarioTask;

	// PURPOSE:	The last scenario point we used, if any.
	RegdScenarioPnt	m_LastUsedPoint;

	// PURPOSE:	The point we should return at if possible, if we get aborted and resume.
	RegdScenarioPnt	m_PointToReturnTo;
	
	// PURPOSE:	The point we are searching from.
	RegdScenarioPnt	m_PointToSearchFrom;

	// PURPOSE:	A prop management helper to transition between old and new TaskUseScenarios.
	CPropManagementHelper::SPtr m_pPropTransitionHelper;

	// PURPOSE: Delays the time between scenario searches
	float			m_fTimeSinceLastSearchForNextScenarioInChain;

	// PURPOSE:	The real scenario type associated with m_PointToReturnTo.
	int				m_PointToReturnToRealTypeIndex;
	
	// PURPOSE:	The real scenario type associated with m_LastUsedPoint.
	int				m_LastUsedPointRealTypeIndex;
	
	// PURPOSE:	The real scenario type associated with m_PointToSearchFrom.
	int				m_PointToSearchFromRealTypeIndex;

	// PURPOSE:	Scenario type index of a wander scenario to implicitly use next.
	int				m_WanderScenarioToUseNext;

	// PURPOSE:	Time stamp from fwTimer::GetTimeInMilliseconds() for when we were last
	//			using m_LastUsedPoint.
	u32				m_LastUsedPointTimeMs;

	// PURPOSE:	When in State_Wandering, this counts down time until we try to
	//			search for scenarios.
	u32				m_TimeUntilCheckScenarioMs;

	CScenarioPointChainUseInfoPtr m_SPC_UseInfo;

	CPavementFloodFillHelper m_PavementHelper;

	// PURPOSE:  When in InVehicleAsPassenger_OnUpdate, count down to check if the driver's
	//			conditional anim's group has changed (in seconds).
	float			m_fTimeUntilDriverAnimCheckS;

	// PURPOSE: Set externally if the ped should wander away from a certain location when resuming wander
	Vector3			m_TargetToInitiallyWanderAwayFrom;
	u32				m_timeTargetSet;

	// PURPOSE:	Part of an awkward mechanism to prevent state switches right as the subtask gets aborted.
	u8				m_FrameCountdownAfterAbortAttempt;

	// PURPOSE:  If set, when starting the CTaskWander subtask, don't create a wander path this time
	u8				m_nFrameWanderDelay;

	// PURPOSE: Running flags
	fwFlags8		m_uRunningFlags;

	// PURPOSE:	If set, try to create a prop for the ped when the task starts.
	bool			m_bCreateProp : 1;

	// PURPOSE:	Set to true if something happened to our current/last vehicle so that we
	//			shouldn't return to it.
	bool			m_bDontReturnToVehicle : 1;

	// PURPOSE:	Used for networking, set if m_SPC_UseInfo either is set (local control)
	//			or should be set (when cloned).
	bool			m_bHasScenarioChainUser : 1;

	// PURPOSE:	True if the next point (the one after the one we are moving towards)
	//			has been tested against blocking areas.
	bool			m_NextPointTestedForBlockingAreas;

	// PURPOSE:	If set, behave like the old CTaskRoam: go straight between points
	//			without wandering.
	bool			m_bRoamMode : 1;

	// PURPOSE:  If set, when creating the CTaskWander subtask, tell it to keep moving while waiting for the first path
	bool			m_bKeepMovingWhenFindingWanderPath : 1;

	// PURPOSE:  If set, when starting the CTaskWander subtask, tell it to initially wander away from the player.
	bool			m_bWanderAwayFromThePlayer : 1;

	// PURPOSE:  If set, when resuming the scenario point have the ped do a cowardly enter.
	bool			m_bDoCowardScenarioResume : 1;

	// PURPOSE:  Remember if the scenario task is being started as a resume or not.
	bool			m_bScenarioReEntry : 1;

	// PURPOSE:  Remember if we are never supposed to return to our scenario point.
	bool			m_bNeverReturnToLastPoint : 1;

	// PURPOSE:  Set when migrating to local to allow state start to prevent returning to vehicle if wasnt doing so at migrate.
	bool			m_bDontReturnToVehicleAtMigrate : 1;

	// PURPOSE:  When task migrated check this records the status of the task use scenario at that time
	mutable bool m_bScenarioCowering : 1;

	static Tunables sm_Tunables;
	static const u32 sm_aSafeForResumeScenarioHashes[POST_HANGOUT_SCENARIOS_SIZE];
};

//-----------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Task info for TaskUnalerted
//-------------------------------------------------------------------------
class CClonedTaskUnalertedInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskUnalertedInfo();
	CClonedTaskUnalertedInfo(s32 state, bool hasScenarioChainUser, bool bIsCowering);
	~CClonedTaskUnalertedInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_UNALERTED;}

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask *CreateLocalTask(fwEntity* pEntity);

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskUnalerted::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Task Unalerted State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool	bHasScenarioChainUser	= m_bHasScenarioChainUser;
		SERIALISE_BOOL(serialiser, bHasScenarioChainUser, "Has Scenario Chain User");
		m_bHasScenarioChainUser = bHasScenarioChainUser;

		bool	bIsCowering				= m_bIsCowering;
		SERIALISE_BOOL(serialiser, bIsCowering, "Is cowering");
		m_bIsCowering = bIsCowering;
	}

	bool GetHasScenarioChainUser()	const { return m_bHasScenarioChainUser; }
	bool GetIsCowering()			const { return m_bIsCowering; }
	
private:
	bool	m_bHasScenarioChainUser : 1;
	bool	m_bIsCowering: 1;
};
#endif	// TASK_DEFAULT_TASK_UNALERTED_H

// End of file 'task/Default/TaskUnalerted.h'
