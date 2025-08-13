#ifndef INC_TASK_FLEE_H
#define INC_TASK_FLEE_H

// Framework headers
#include "ai/task/taskstatetransition.h"
#include "fwutil/Flags.h"

// Game headers
#include "Event/System/EventData.h"
#include "Peds/FleeBehaviour.h"
#include "AI/AITarget.h"
#include "Scene/RegdRefTypes.h"
#include "Task/Scenario/ScenarioPoint.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"
#include "Peds/QueriableInterface.h"
#include "Network/General/NetworkUtil.h"
#include "ik/IkRequest.h"

// Game forward Declarations
class CEntity;
class CEvent;
class CEventCarUndriveable;
class CTaskMoveFollowNavMesh;

////////////////////////////
// FLEE TRANSITIONS		  //
////////////////////////////

// PURPOSE : Flee transition table to determine which state to move to when using PickNewStateTransition

// Global enums available to files including this header
typedef enum
{
	FF_WillUseCover				= BIT0,
	FF_ReturnToOriginalPosition = BIT1,

} FleeConditionFlags;

// Normal flee transitions
class CFleeOnFootTransitions : public aiTaskStateTransitionTable
{
public:
	CFleeOnFootTransitions();

	virtual void Init();
};

// Skiing flee transitions
class CFleeSkiingTransitions : public aiTaskStateTransitionTable
{
public:
	CFleeSkiingTransitions();

	virtual void Init();
};

// Vehicle flee transitions
class CFleeInVehicleTransitions : public aiTaskStateTransitionTable
{
public:
	CFleeInVehicleTransitions();

	virtual void Init();
};

////////////////////////////
//  TASK SMART FLEE		  //
////////////////////////////

// PURPOSE : Combines functionality of CTaskSmartFleePoint and CTaskSmartFleeEntity
//			 Fleeing from a point does not use updating of the flee point currently. This
//			 functionality is used when fleeing from an entity. When fleeing from an entity 
//			 the task updates the flee point depending on whether the entity moves away 
//			 by a predetermined distance. A flag bWillReturnToOriginalPos is used to determine if the
//			 ped returns to their starting position after fleeing. Once the ped has reached the safe distance,
//			 they will do a running wander in the direction they were heading when they reached that point,
//			 and will resume fleeing if the target entity comes within range.

// TODO: - Play various fleeing clips

class CTaskSmartFlee : public CTaskFSMClone
{

public:

	enum ConfigFlags
	{
		CF_DontScream											= BIT0,
		CF_ReverseInVehicle										= BIT1,
		CF_CanPutHandsUp										= BIT2,
		CF_NeverLeaveWater										= BIT3,
		CF_ForceLeaveVehicle									= BIT4,
		CF_FleeAtCurrentHeight									= BIT5,
		CF_CanCallPolice										= BIT6,
		CF_AccelerateInVehicle									= BIT7,
		CF_Cower												= BIT8,
		CF_DisablePanicInVehicle								= BIT9,
		CF_DisableExitVehicle									= BIT10,
		CF_OnlyCallPoliceIfChased								= BIT11,
		CF_EnterLastUsedVehicleImmediately						= BIT12,
		CF_EnterLastUsedVehicleImmediatelyIfCloserThanTarget	= BIT13,
		CF_NeverEnterWater										= BIT14,
		CF_NeverLeaveDeepWater									= BIT15,
	};

	enum ExecConditions
	{
		EC_DontUseCars = BIT0,
		EC_DisableCower = BIT1,
		// Update with the flag list
		EC_LAST_SYNCED_FLAG = EC_DisableCower
	};

private:

	enum RunningFlags
	{
		RF_Cower						= BIT0,
		RF_HasCheckedCowerDueToRoute	= BIT1,
		RF_HasCalledPolice				= BIT2,
		RF_HasAcceleratedInVehicle		= BIT3,
		RF_CarIsUndriveable				= BIT4,
		RF_CowerAtEndOfRoute			= BIT5,
		RF_HasBeenTrapped				= BIT6,
		RF_HasReversedInVehicle			= BIT7,
		RF_HasScreamed					= BIT8,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		bool	m_ExitVehicleDueToRoute;
		bool    m_UseRouteInterceptionCheckToExitVehicle;
		float   m_RouteInterceptionCheckMinTime;
		float   m_RouteInterceptionCheckMaxTime;
		float   m_RouteInterceptionCheckDefaultMaxSideDistance;
		float   m_RouteInterceptionCheckDefaultMaxForwardDistance;
		float   m_RouteInterceptionCheckVehicleMaxSideDistance;
		float   m_RouteInterceptionCheckVehicleMaxForwardDistance;
		float   m_EmergencyStopMinSpeedToUseDefault;
		float	m_EmergencyStopMaxSpeedToUseDefault;
		float   m_EmergencyStopMaxSpeedToUseOnHighWays;
		float   m_EmergencyStopMinTimeBetweenStops;
		float   m_EmergencyStopTimeBetweenChecks;
		float   m_EmergencyStopInterceptionMinTime;
		float   m_EmergencyStopInterceptionMaxTime;
		float   m_EmergencyStopInterceptionMaxSideDistance;
		float   m_EmergencyStopInterceptionMaxForwardDistance;
		float   m_EmergencyStopTimeToUseIfIsDrivingParallelToTarget;
		float   m_DrivingParallelToThreatMaxDistance;
		float	m_DrivingParallelToThreatMinVelocityAlignment;
		float   m_DrivingParallelToThreatMinFacing;
		float	m_ExitVehicleMaxDistance;
		float	m_ExitVehicleRouteMinDistance;
		float	m_TimeBetweenHandsUpChecks;
		float	m_TimeBetweenExitVehicleDueToRouteChecks;
		float	m_TimeToCower;
		float	m_MinTimeForHandsUp;
		float	m_MaxTimeForHandsUp;
		float	m_MinDelayTimeForExitVehicle;
		float	m_MaxDelayTimeForExitVehicle;
		float	m_ChanceToDeleteOnExitVehicle;
		float	m_MinDistFromPlayerToDeleteOnExitVehicle;
		float	m_MaxRouteLengthForCower;
		float	m_MinDistFromTargetWhenCoweringToCheckForExit;
		float	m_FleeTargetTooCloseDistance;
		float	m_MinFleeOnBikeDistance;
		float	m_TimeOnBikeWithoutFleeingBeforeExitVehicle;
		int		m_MaxRouteSizeForCower;
		bool	m_ForceCower;
		float   m_ChancesToAccelerateTimid;
		float   m_ChancesToAccelerateAggressive;
		float   m_TargetObstructionSizeModifier;
		float	m_TimeToReverseCarsBehind;
		float	m_TimeToReverseInoffensiveTarget;
		float	m_TimeToReverseDefaultMin;
		float	m_TimeToReverseDefaultMax;
		float	m_TimeToExitCarDueToVehicleStuck;
		float   m_RoutePassNearTargetThreshold;
		float   m_RoutePassNearTargetCareThreshold;
		bool	m_CanAccelerateAgainstInoffensiveThreat;
		float	m_TargetInvalidatedMaxStopDistanceLeftMinVal;
		float	m_TargetInvalidatedMaxStopDistanceLeftMaxVal;
		float	m_MinSpeedToJumpOutOfVehicleOnFire;

		PAR_PARSABLE;
	};

	static void InitClass();
	static void ShutdownClass();
	static void UpdateClass();

	static void InitTransitionTables();

	// FSM States
	enum 
	{ 
		State_Start = 0, 
		State_ExitVehicle,
		State_WaitBeforeExitVehicle,
		State_FleeOnFoot,
		State_FleeOnSkis,
		State_SeekCover,
		State_CrouchAtCover,
		State_ReturnToStartPosition,
		State_EnterVehicle,
		State_FleeInVehicle,
		State_EmergencyStopInVehicle,
		State_ReverseInVehicle,
		State_AccelerateInVehicle,
		State_StandStill,
		State_HandsUp,
		State_TakeOffSkis,
		State_Cower,
		State_CowerInVehicle,
		State_Finish,
	};

	static const float ms_fMaxStopDistance; // NOTE[egarcia]: ms_fMaxStopDistance MUST be changed together with CClonedControlTaskSmartFleeInfo::iMAX_STOP_DISTANCE_SIZE 
	static const float ms_fMaxIgnorePreferPavementsTime; // NOTE[egarcia]: ms_fMaxIgnorePreferPavementsTime MUST be changed together with CClonedControlTaskSmartFleeInfo::iMIN_FLEEING_TIME_TO_PREFER_PAVEMENTS_SIZE 
	static bank_float	ms_fStopDistance;	// The distance at which a ped will stop and stand still indefinitely
	static bank_s32	ms_iFleeTime;
	static bank_u32	ms_uiRecoveryTime;
	static bank_u32	ms_uiEntityPosCheckPeriod;
	static bank_u32	ms_uiVehicleCheckPeriod;
	//static bank_float	ms_fEntityPosChangeThreshold;
	static bank_float	ms_fMinDistAwayFromThreatBeforeSeekingCover;
	static bank_s32 ms_iMinPanicAudioRepeatTime;
	static bank_s32 ms_iMaxPanicAudioRepeatTime;

	// Flee from an entity or a position (make sure only one is specified)
	CTaskSmartFlee (const CAITarget& target, 
		const float	fStopDistance				= ms_fStopDistance, 
		const int	iFleeTime					= ms_iFleeTime, 
		const int	iEntityPosCheckPeriod		= ms_uiEntityPosCheckPeriod, 
		const float	fEntityPosChangeThreshold	= 0.0f,
		const bool  bKeepMovingWhilstWaitingForFleePath  = false,
		const bool	bResumeFleeingAtEnd			= true
		);

	~CTaskSmartFlee();

public:

	atHashWithStringNotFinal GetAmbientClips() const { return m_hAmbientClips; }

public:

	void SetConfigFlags(fwFlags16 uConfigFlags)				{ m_uConfigFlags = uConfigFlags; }
	void SetConfigFlagsForVehicle(fwFlags8 uConfigFlags)	{ m_uConfigFlagsForVehicle = uConfigFlags; }

	void SetExecConditions(fwFlags8 uExecConditions)		{ m_uExecConditions = uExecConditions; }

	void SetLastTargetPosition(Vec3V_In vPosition)			{ m_vLastTargetPosition = vPosition; }
	void ForceRepath(CPed* pPed);

public:

	bool HasCalledPolice() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_HasCalledPolice));
	}

	bool IsCarUndriveable() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_CarIsUndriveable));
	}

	virtual const CEntity* GetTargetPositionEntity() const
	{ 
		if(GetSubTask() && !m_FleeTarget.GetEntity())
		{
			return GetSubTask()->GetTargetPositionEntity();
		}

		return m_FleeTarget.GetEntity();
	}

public:

	float	GetTimeFleeingInVehicle() const;
	float	GetTimeFleeingOnFoot() const;
	bool	IsFleeingInVehicle() const;
	bool	IsFleeingOnFoot() const;
	void	OnCarUndriveable(const CEventCarUndriveable& rEvent);
	bool	SetSubTaskForOnFoot(CTask* pTask);

	// PURPOSE: Check if the target entity is valid (Eg: not a dead player)
	bool	IsValidTargetEntity(const CEntity* pTarget) const;

	// PURPOSE: If the target entity is no longer valid, we have to stop updating info from it
	void	OnTargetEntityNoLongerValid();

	static bool AllowedToFleeOnRoads(const CVehicle& veh);

public:

	static bool						AccelerateInVehicleForEvent(const CPed* pPed, const CEvent& rEvent);
	static bool						CanCallPoliceDueToEvent(const CPed* pTarget, const CEvent& rEvent);
	static Vec3V_Out				AdjustTargetPosByNearByPeds(const CPed& rPed, Vec3V_In vFleePosition, const CEntity* pFleeEntity, bool bForceScanNearByPeds, ScalarV_Ptr pscCrowdNormalizedSize = NULL NOTFINAL_ONLY(, Vec3V_Ptr pvExitCrowdDir = NULL, Vec3V_Ptr pvWallCollisionPosition = NULL, Vec3V_Ptr pvWallCollisionNormal = NULL, Vec3V_Ptr pvBeforeWallCheckFleeAdjustedDir = NULL, ScalarV_Ptr pscNearbyPedsTotalWeight = NULL, Vec3V_Ptr pvNearbyPedsCenterOfMass = NULL, atArray<Vec3V>* paFleePositions = NULL, atArray<ScalarV>* paFleeWeights = NULL));
	static CTaskMoveFollowNavMesh*	CreateMoveTaskForFleeOnFoot(Vec3V_In vTarget);
	static bool						ReverseInVehicleForEvent(const CPed* pPed, const CEvent& rEvent);
	static bool						ShouldCowerDueToEvent(const CPed* pPed, const CEvent& rEvent);
	static bool						ShouldEnterLastUsedVehicleImmediatelyIfCloserThanTarget(const CPed * pPed, const CEvent& rEvent);
	static bool						ShouldExitVehicleDueToEvent(const CPed* pPed, const CPed* pTarget, const CEvent& rEvent);
	static bool						ShouldNeverEnterWaterDueToEvent(const CEvent& rEvent);
	static bool						IsDistanceUndefined(float fDistance) { return fDistance <= -0.5f; }

public:

	// See eEventResponseFleeFlags
	void SetFleeFlags( int fleeFlags );


	// Task required functions
	virtual aiTask* Copy() const
	{
		CTaskSmartFlee* pTask = NULL;
		pTask = rage_new CTaskSmartFlee(m_FleeTarget,
										m_fStopDistance,
										m_iFleeTime,
										m_iEntityPosCheckPeriod,
										0.0f, //m_fEntityPosChangeThreshold,
										m_bKeepMovingWhilstWaitingForFleePath,
										m_bResumeFleeingAtEnd
										);
		pTask->m_fMoveBlendRatio = m_fMoveBlendRatio;
		pTask->SetQuitTaskIfOutOfRange(m_bQuitTaskIfOutOfRange);
		pTask->SetFleeFlags(m_FleeFlags);
		pTask->m_vLastTargetPosition = m_vLastTargetPosition;
		pTask->m_OldTargetPosition = m_OldTargetPosition;
		pTask->UseExistingMoveTask(m_pExistingMoveTask);
		pTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
		pTask->SetPullBackFleeTargetFromPed(m_bPullBackFleeTargetFromPed);
		pTask->SetAmbientClips(m_hAmbientClips);
		pTask->SetForcePreferPavements(m_bForcePreferPavements);
		pTask->SetIgnorePreferPavementsTimeLeft(m_fIgnorePreferPavementsTimeLeft);
		pTask->SetNeverLeavePavements(m_bNeverLeavePavements);
		pTask->SetDontClaimProp(m_bDontClaimProp);
		pTask->SetConfigFlags(m_uConfigFlags);
		pTask->SetConfigFlagsForVehicle(m_uConfigFlagsForVehicle);
		pTask->SetExecConditions(m_uExecConditions);
		pTask->SetConsiderRunStartForPathLookAhead(m_bConsiderRunStartForPathLookAhead);
		pTask->SetKeepMovingOnlyIfMoving(m_bKeepMovingOnlyIfMoving);

		return pTask;
	}

	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_SMART_FLEE; }

	// FSM required functions
	virtual FSM_Return	UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort()	const { return State_Start; }

	// FSM optional functions
	FSM_Return			ProcessPreFSM		();
	virtual	void		CleanUp			();

	// Helper methods
	void				SetAmbientClips(atHashWithStringNotFinal hAmbientClips);
	void				SetFleeingAnimationGroup(CPed* pPed);
	bool				SetFleePosition(const Vector3& vNewFleePos, const float fStopDistance);
	void				SetStopDistance(const float fStopDistance);
	float				GetIgnorePreferPavementsTimeLeft() const { return m_fIgnorePreferPavementsTimeLeft; }
	void				SetIgnorePreferPavementsTimeLeft(const float fIgnorePreferPavementsTimeLeft);
	bool				HasIgnorePreferPavementsTimeExpired() const { return m_fIgnorePreferPavementsTimeLeft <= 0.0f; }
	bool				IsIgnorePreferPavementsTimeDisabled() const { return m_fIgnorePreferPavementsTimeLeft < -0.5f; }
	void				DisableIgnorePreferPavementsTime() { m_fIgnorePreferPavementsTimeLeft = -1.0f; }
	const CAITarget&	GetFleeTarget() const { return m_FleeTarget; }
	void				SetMoveBlendRatio(float fMoveBlendRatio);
	float				GetMoveBlendRatio() { return m_fMoveBlendRatio; };
	inline void			SetKeepMovingWhilstWaitingForFleePath(const bool b)   { m_bKeepMovingWhilstWaitingForFleePath = b; }
	inline void			SetUseLargerSearchExtents(const bool b)				  { m_bUseLargerSearchExtents = b; }
	inline bool			GetUseLargerSearchExtents()	const					  { return m_bUseLargerSearchExtents; }
	inline void			SetPullBackFleeTargetFromPed(const bool b)				  { m_bPullBackFleeTargetFromPed = b; }
	inline bool			GetPullBackFleeTargetFromPed()	const					  { return m_bPullBackFleeTargetFromPed; }
	inline void			SetResumeFleeingAtEnd(const bool bResumeFlee)		  { m_bResumeFleeingAtEnd = bResumeFlee; }
	void				SetQuitTaskIfOutOfRange(const bool bQuitIfOutOfRange) { m_bQuitTaskIfOutOfRange = bQuitIfOutOfRange; }
	bool				ShouldPreferPavements(const CPed* pPed) const;
	void				SetForcePreferPavements(const bool bForcePreferPavements) { m_bForcePreferPavements = bForcePreferPavements; }
	void				SetNeverLeavePavements(const bool bNeverLeavePavements) { m_bNeverLeavePavements = bNeverLeavePavements; }
	void				SetDontClaimProp(const bool b)						  { m_bDontClaimProp = b; }
	void				SetQuitIfOutOfRange(const bool b)						  { m_bQuitIfOutOfRange = b; }
	void				SetConsiderRunStartForPathLookAhead(const bool b)		{ m_bConsiderRunStartForPathLookAhead = b; }
	void				SetKeepMovingOnlyIfMoving(const bool b)		{ m_bKeepMovingOnlyIfMoving = b; }

	void UseExistingMoveTask(CTask* pTask){ m_pExistingMoveTask = pTask;}
	CTaskMoveFollowNavMesh* FindFollowNavMeshTask();

	void SetPriorityEvent(const eEventType priority_event) 
	{ 
		if ((m_priorityEvent != priority_event && priority_event != EVENT_GUN_AIMED_AT) || m_priorityEvent == EVENT_NONE)
		{
			m_bNewPriorityEvent = true;
			m_priorityEvent = priority_event;
		}
	}

	virtual bool IsValidForMotionTask(CTaskMotionBase& pTask) const;

// Debug functions, removed from final build
#if !__FINAL
	virtual void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

#if __BANK
	static void DEBUG_RenderFleeingOutOfPavementManager();
#endif // __BANK


	// Clone task implementation for CTaskSmartFlee
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	void						FleeOnFoot_OnUpdateClone();

	bool							GetOutgoingMigratingCower()					const {	return m_bOutgoingMigratedWhileCowering ;	}
	bool							GetIncomingMigratingCower()					const {	return m_bIncomingMigratedWhileCowering ;	}

	static bool SpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed);
	static bool SpheresOfInfluenceBuilderForTarget(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed, const CAITarget& rTarget);
	static void AddInfluenceSphere(const CPed& rPed, TInfluenceSphere* pSpheres, int& iNumSpheres, int iMaxSpheres);

	virtual bool		UseCustomGetup() { return true; }
	virtual bool		AddGetUpSets ( CNmBlendOutSetList& sets, CPed* pPed );
	virtual void		GetUpComplete ( eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed );
	virtual void		RequestGetupAssets ( CPed* pPed );
	static void			SetupFleeingGetup(CNmBlendOutSetList& sets, const Vector3& vFleePos, CPed* pPed, const CTaskSmartFlee * pFleeTask=NULL);

	static float		GetBaseTimeToCowerS();

	bool SuppressingHandsUp() { return m_SuppressHandsUp;}
	
	fwFlags16&	GetConfigFlags()					{ return m_uConfigFlags; }
	fwFlags16	GetConfigFlags()			const	{ return m_uConfigFlags; }
	fwFlags8&	GetConfigFlagsForVehicle()			{ return m_uConfigFlagsForVehicle; }
	fwFlags8	GetConfigFlagsForVehicle()	const	{ return m_uConfigFlagsForVehicle; }

protected:

	bool IsFlagSet( eEventResponseFleeFlags flag ) const
	{
		return (m_FleeFlags & (1<<flag)) != 0;
	}

	// State Functions
	void						Start_OnEnter					(CPed* pPed);
	FSM_Return					Start_OnUpdate					(CPed* pPed);

	void						ExitVehicle_OnEnter				(CPed* pPed);
	FSM_Return					ExitVehicle_OnUpdate			(CPed* pPed);
    void						ExitVehicle_OnEnterClone		(CPed* pPed);
	FSM_Return					ExitVehicle_OnUpdateClone		(CPed* pPed);
	void						ExitVehicle_OnExit				();
	
	void						WaitBeforeExitVehicle_OnEnter	();
	FSM_Return					WaitBeforeExitVehicle_OnUpdate	();

	void						FleeOnFoot_OnEnter				(CPed* pPed);
	FSM_Return					FleeOnFoot_OnUpdate				(CPed* pPed);
	void						FleeOnFoot_OnExit				();

	void						FleeOnSkis_OnEnter				(CPed* pPed);
	FSM_Return					FleeOnSkis_OnUpdate				(CPed* pPed);

	void						ReturnToStartPosition_OnEnter	(CPed* pPed);
	FSM_Return					ReturnToStartPosition_OnUpdate	(CPed* pPed);

	void						EnterVehicle_OnEnter			(CPed* pPed);
	FSM_Return					EnterVehicle_OnUpdate			(CPed* pPed);
	void						EnterVehicle_OnEnterClone		(CPed* pPed);
	FSM_Return					EnterVehicle_OnUpdateClone		(CPed* pPed);

	void						FleeInVehicle_OnEnter			(CPed* pPed);
	FSM_Return					FleeInVehicle_OnUpdate			(CPed* pPed);
	void						FleeInVehicle_OnExit			();

	void						ReverseInVehicle_OnEnter();
	FSM_Return					ReverseInVehicle_OnUpdate();

	void						EmergencyStopInVehicle_OnEnter();
	FSM_Return					EmergencyStopInVehicle_OnUpdate();

	void						AccelerateInVehicle_OnEnter();
	FSM_Return					AccelerateInVehicle_OnUpdate();

	void						SeekCover_OnEnter				(CPed* pPed);
	FSM_Return					SeekCover_OnUpdate				(CPed* pPed);

	void						CrouchAtCover_OnEnter			(CPed* pPed);
	FSM_Return					CrouchAtCover_OnUpdate			(CPed* pPed);

	void						StandStill_OnEnter				(CPed* pPed);
	FSM_Return					StandStill_OnUpdate				(CPed* pPed);

	void						HandsUp_OnEnter					(CPed* pPed);
	FSM_Return					HandsUp_OnUpdate				(CPed* pPed);

	void						TakeOffSkis_OnEnter				(CPed* pPed);
	FSM_Return					TakeOffSkis_OnUpdate			(CPed* pPed);

	void						Cower_OnEnter					();
	FSM_Return					Cower_OnUpdate					();

	void						CowerInVehicle_OnEnter();
	FSM_Return					CowerInVehicle_OnUpdate();

	// Is the specified state handled by the clone FSM
	bool StateChangeHandledByCloneFSM(s32 iState);

private:

	/// FSM state transition functions
	CFleeBehaviour::FleeType			GetFleeType( CPed* pPed );

	// Return the valid transitional set
	virtual aiTaskStateTransitionTable*	GetTransitionSet();

	// Generate the conditional flags to choose a transition
	virtual s64						GenerateTransitionConditionFlags( bool bStateEnded );

	// Returns a pointer to the transition table data, overridden by tasks that use transitional sets
	virtual aiTaskTransitionTableData*	GetTransitionTableData() { return &m_transitionTableData; }

	bool	CheckForNewPriorityEvent();
	
	bool	CheckForNewAimedAtEvent();

	// Helper functions
	bool	AreWeOutOfRange(CPed* pPed, float fDistanceToCheck);
	u8	ComputeFleeDir(CPed* pPed);
	void	SetDefaultTaskWanderDir(CPed* pPed);
	bool	LookForCarsToUse(CPed* pPed);
	void	TryToLookAtTarget();

	// Update the position of the thing we're fleeing from
	void UpdateFleePosition(CPed* pPed);

	float CalculatePositionUpdateDelta(CPed * pPed) const;
	
	bool	CanAccelerateInVehicle() const;
	bool	CanCallPolice() const;
	bool	CanCower() const;
	bool	CanExitVehicle() const;
	bool	CanPutHandsUp() const;
	bool	CanReverseInVehicle() const;
	void	CheckForCowerDuringFleeOnFoot();
	bool	CheckForExitVehicleDueToRoute();
	void	ResetExitVehicleDueToVehicleStuckTimer();
	bool	UpdateExitVehicleDueToVehicleStuck();
	bool	ShouldExitVehicleDueToVehicleStuck(CPed* pPed, bool bIsDriver, bool bIsHangingOn);
	s32		ChooseStateForExitVehicle() const;
	CTask*	CreateSubTaskForAmbientClips() const;
	CTask*	CreateSubTaskForCallPolice() const;
	bool	DoesVehicleHaveNoDriver() const;
	atHashWithStringNotFinal GetAmbientClipsToUse() const;
	const CPed*	GetPedWeAreFleeingFrom() const;
	bool	HasOnFootClipSetOverride() const;
	bool	IsLifeInDanger() const;
	bool	IsFarEnoughAwayFromTarget() const;
	bool	IsOnTrain() const;
	bool	IsStayingInVehiclePreferred() const;
	bool	IsTargetArmed() const;
	bool	IsTargetArmedWithGun() const;
	bool	IsInoffensiveTarget(const CEntity* pTargetEntity) const;
	bool	CanExitVehicleToFleeFromTarget(const CEntity* pEntity) const;
	bool	IsVehicleStuck() const;
	bool	IsWaitingForPathInFleeOnFoot() const;
	void	OnAmbientClipsChanged();
	void	OnExitingVehicle();
	void	OnMoveBlendRatioChanged();
	void	OnNoLongerExitingVehicle();
	void	RestartSubTaskForOnFoot();
	void	ProcessMoveTask();
	void	ResetRunStartPhase();
	void	SetRunStartPhase(float fPhase);
	void	SetStateForExitVehicle();
	bool	ShouldAccelerateInVehicle() const;
	bool	ShouldCower() const;
	bool    ShouldEnterLastUsedVehicleImmediately() const;
	bool	ShouldExitVehicleDueToRoute() const;
	bool	ShouldLaunchEmergencyStop() const;
	bool	ShouldCowerInsteadOfFleeDueToTarget() const;
	void    ResetDrivingParallelToTargetTime();
	void    UpdateDrivingParallelToTargetTime();
	bool	IsDrivingParallelToAnyTarget(const CVehicle& rVehicle) const;
	bool	IsDrivingParallelToTarget(const CVehicle& rVehicle, const CEntity* pTargetEntity) const;
	bool	IsPedHangingOnAVehicle() const;

	// PURPOSE: Target info used to check if it is potentially intercepting my vehicle route
	struct SVehicleRouteInterceptionParams
	{
		SVehicleRouteInterceptionParams() 
			: vPosition(V_ZERO)
			, vForward(V_ZERO)
			, vVelocity(V_ZERO)
			, fMaxSideDistance(1.5f)
			, fMaxForwardDistance(3.0f)
			, fMinTime(1.0f)
			, fMaxTime(3.0f)
		{ /* ... */ }

		Vec3V vPosition;
		Vec3V vForward;
		Vec3V vVelocity;
		float fMaxSideDistance;
		float fMaxForwardDistance;
		float fMinTime;
		float fMaxTime;
	};

	bool	ShouldLaunchEmergencyStopDueToRoute(const CVehicle& rVehicle) const;
	bool	ShouldLaunchEmergencyStopDueToRouteAndTarget(const CVehicle& rVehicle, const SVehicleRouteInterceptionParams& rTargetVehicleInterceptionParams) const;
	bool	GetFleeTargetEmergencyStopRouteInterceptionParams(SVehicleRouteInterceptionParams* pParams) const;
	bool	GetTargetEmergencyStopRouteInterceptionParams(const CEntity& rTargetEntity, SVehicleRouteInterceptionParams* pParams) const;
	s32		GetPauseAfterLaunchEmergencyStopDuration() const;

	bool	ShouldExitVehicleDueToRouteAndTarget(const CVehicle& rVehicle, const SVehicleRouteInterceptionParams& rTargetVehicleInterceptionParams) const;
	bool	GetFleeTargetExitVehicleRouteInterceptionParams(SVehicleRouteInterceptionParams* pParams) const;
	void	GetTargetExitVehicleRouteInterceptionParams(const CEntity& rTargetEntity, SVehicleRouteInterceptionParams* pParams) const;

	bool	ShouldExitVehicleDueToAttributes() const;
	bool	ShouldFleeOurOwnVehicle() const;
	bool	ShouldProcessMoveTask() const;
	bool	ShouldRestartSubTaskForOnFoot() const;
	bool	ShouldReverseInVehicle() const;
	bool	ShouldWaitBeforeExitVehicle() const;
	bool	ShouldJumpOutOfVehicle() const;
	bool	ShouldWaitBeforeExitVehicleDueToProximity() const;
	void	UpdateCowerDueToVehicle();
	void	UpdateSecondaryTaskForFleeInVehicle();
	void	UpdateSubTaskForFleeOnFoot();
	void	TryToScream();
	bool	CanScream() const;
	void	Scream();
	void	GenerateAIScreamEvents();
	s32		GetScreamTimerDuration() const;
	Vec3V_Out GetFleeTargetVelocity() const;
	void	UpdateFleeInVehicleIntensity();
	void	InitPreferPavements();
	void	UpdatePreferPavements(CPed * pPed);

	void	TryToPlayTrappedSpeech();

#if __ASSERT
	void CheckForOriginFlee();
#endif // __ASSERT

	void AddExitVehicleDueToRoutePassByThreatPositionRequest();
	bool ProcessExitVehicleDueToRoutePassByThreatRequest();
	
private:

	static const CVehicle* RetrieveTargetEntityVehicle(const CEntity* pTargetEntity);
	static int	CountPedsExitingTheirVehicle();
	static bool	IsPedThreatening(const CPed& rPed, const CPed& rOtherPed, bool bIncludeNonViolentWeapons);
	static bool IsPositionWithinConstraints(const CEntity& rEntity, Vec3V_In vPosition, float fMinDot, float fMaxDistance);
	static bool	ShouldConsiderPedThreatening(const CPed& rPed, const CPed& rOtherPed, bool bIncludeNonViolentWeapons);

private:

	// Static transition sets
	static aiTaskStateTransitionTable* ms_fleeTransitionTables[CFleeBehaviour::FT_NumTypes];

	// Task member variables
	Vec3V			m_vLastTargetPosition;
	RegdVeh			m_pCloneMyVehicle;
	CAITarget		m_FleeTarget;
	CAITarget		m_SecondaryTarget;
	int				m_iEntityPosCheckPeriod;
	//float			m_fEntityPosChangeThreshold;
	Vector3			m_vStartPos;
	int				m_iFleeTime;
	float			m_fStopDistance;
	float			m_fMoveBlendRatio;
	float			m_fFleeInVehicleIntensity;
	float			m_fFleeInVehicleTime;
	CTaskGameTimer	m_fleeTimer;
	CTaskGameTimer	m_calcFleePosTimer;
	CTaskGameTimer  m_stateTimer;
	CTaskGameTimer	m_standStillTimer;
	CTaskGameTimer	m_screamTimer;
	CTaskGameTimer	m_OffscreenDeletePedTimer;
	u32				m_uTimeToLookAtTargetNext;
	u32				m_uTimeWeLastCheckedIfWeShouldStopWaitingBeforeExitingTheVehicle;
	u32				m_uTimeStartedFleeing;
	Vector3			m_vOldPedPos;
	atHashWithStringNotFinal m_hAmbientClips;
	u8				m_iFleeDir;
	u8				m_FleeFlags;		// See eEventResponseFleeFlags
	fwFlags16		m_uConfigFlags;
	fwFlags8		m_uConfigFlagsForVehicle;
	fwFlags8		m_uExecConditions;
	fwFlags16		m_uRunningFlags;
	bool			m_bKeepMovingWhilstWaitingForFleePath;
	bool			m_bHadAVehicle;
	bool			m_bNewPriorityEvent;
	bool			m_bResumeFleeingAtEnd;
	bool			m_bUpdateTargetToNearestHatedPed;

	bool			m_bHaveStopped;
	bool			m_bQuitTaskIfOutOfRange;
	bool			m_bForcePreferPavements;
	float			m_fIgnorePreferPavementsTimeLeft;
	bool			m_bNeverLeavePavements;
	bool			m_bPedStartedInWater;
	bool			m_bUseLargerSearchExtents;
	bool			m_bPullBackFleeTargetFromPed;
	bool			m_SuppressHandsUp;
	bool			m_bDontClaimProp;
	bool			m_bQuitIfOutOfRange;
	bool			m_bConsiderRunStartForPathLookAhead;
	bool			m_bKeepMovingOnlyIfMoving;
	bool			m_bCanCheckForVehicleExitInCower;

	bool			m_bOutgoingMigratedWhileCowering : 1;
	bool			m_bIncomingMigratedWhileCowering : 1;


	eEventType		m_priorityEvent;
	RegdTask		m_pExistingMoveTask;

	fwClipSetRequestHelper	m_moveGroupClipSetRequestHelper;
	CGetupSetRequestHelper	m_GetupReqHelper[2];

	CTaskSimpleTimer	m_HandsUpTimer;
	CTaskSimpleTimer	m_ExitVehicleDueToRouteTimer;

	CTaskSimpleTimer	m_ExitVehicleDueToVehicleStuckTimer;

	CTaskGameTimer			m_AfterEmergencyStopTimer;
	CTaskGameTimer			m_BetweenEmergencyStopsTimer;
	float					m_fDrivingParallelToTargetTime;
	mutable CTaskGameTimer	m_EmergencyStopRouteInterceptionCheckCooldownTimer;

	// Transition table data - stores variables used by the base transition table implementation
	aiTaskTransitionTableData m_transitionTableData;

	//the last time we checked for the thing we are running's from position, this is what we saw
	//this can only be updated a maximum of once per second
	Vector3			m_OldTargetPosition;
	
	// Look at request
	CIkRequestBodyLook	m_LookRequest;

	//
	struct SExitCarIfRoutePassByThreatPositionRequest
	{
		SExitCarIfRoutePassByThreatPositionRequest() :
			bPending(false)
		{ /* ... */ }
		bool bPending;
	};

	SExitCarIfRoutePassByThreatPositionRequest m_ExitCarIfRoutePassByThreatPositionRequest;

private:

	static atFixedArray<RegdPed, 8> sm_aPedsExitingTheirVehicle;

	static bool PedFleeingOutOfPavementIsValidCB(const CPed& rPed);
	static void PedFleeingOutOfPavementOnOutOfSightCB(CPed& rPed);
	static CPedsWaitingToBeOutOfSightManager sm_PedsFleeingOutOfPavementManager;

private:

#if __BANK
	friend class CVehicleIntelligence;
#endif // __BANK

	static Tunables	sm_Tunables;
	
};

////////////////////////////
// ESCAPE BLAST			  //
////////////////////////////

// PURPOSE: Task used to escape a blast radius, the ped will flee the position. If
// a negative time is passed the entity is used to terminate the task when it is safe,
// otherwise the time is use to determine when it is safe.

class CTaskEscapeBlast : public CTaskFSMClone
{
public:

	// FSM states
	typedef enum 
	{ 
		State_Start = 0, 
		State_Crouch,
		State_MoveToSafePoint,
		State_DuckAndCover,
		State_Finish
	} EscapeBlastState;

	static const float ms_iPostExplosionWaitTime;

	CTaskEscapeBlast
		(CEntity* pBlastEntity, 
		const Vector3& vBlastPos, 
		const float fSafeDistance,
		const bool bStopTaskAtSafeDistance,
		const float m_fFleeTime = -1.0f);
	~CTaskEscapeBlast() {};

	virtual aiTask* Copy() const 
	{
		return rage_new CTaskEscapeBlast(m_pBlastEntity,m_vBlastPos,m_fSafeDistance,m_bStopTaskAtSafeDistance,m_fFleeTime);
	}

	// Task required functions
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_ESCAPE_BLAST;}

	// FSM required functions

	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

protected:
	// State functions
	FSM_Return					Start_OnUpdate (CPed* pPed);

	void						Crouch_OnEnter  (CPed* pPed);
	FSM_Return					Crouch_OnUpdate (CPed* pPed);

	void						MoveToSafePoint_OnEnter (CPed* pPed);
	FSM_Return					MoveToSafePoint_OnUpdate (CPed* pPed);
	
	void						DuckAndCover_OnEnter (CPed* pPed);
	FSM_Return					DuckAndCover_OnUpdate (CPed* pPed);

	/// Helper functions
	// Used to handle timer code
	FSM_Return					HandleFleeTimer();
	
	bool						OutsideSafeDistance() const;

	const CEntity* GetBlastEntity() const { return m_pBlastEntity.Get(); }

		// Is the specified state handled by the clone FSM
	bool StateChangeHandledByCloneFSM(s32 iState);

protected:

	RegdEnt		m_pBlastEntity;
	Vector3		m_vBlastPos;
	float		m_fSafeDistance;
	float		m_fFleeTime;
	bool		m_bStopTaskAtSafeDistance;
};

class CTaskScenarioFlee : public CTaskFSMClone
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		// PURPOSE:  How far ahead of the searcher to place the "ideal" flee point
		float	m_fFleeProjectRange;

		// PURPOSE:  How large of an area to search initially
		float	m_fInitialSearchRadius;

		// PURPOSE:  If we don't find a scenario, how much should we multiply the search radius by for the next search
		float	m_fSearchScaler;

		// PURPOSE:  The largest value that the search range can grow to before capping out
		float	m_fSearchRangeMax;

		// PURPOSE:  How far away we need to be from the chaser to stop fleeing
		float	m_fFleeRange;

		// PURPOSE:  How far away we need to be from the chaser to stop fleeing when its a really dangerous event.
		float	m_fFleeRangeExtended;

		// PURPOSE:  When we move to the scenario point, this is the tolerance used in determining if we have reached it
		float	m_fTargetScenarioRadius;

		// PURPOSE:  When probing, this is how forward to cast the probe from the ped's transform.
		float	m_fProbeLength;

		// PURPOSE: How long between avoidance probe checks (ms).
		u32		m_uAvoidanceProbeInterval;

		PAR_PARSABLE;
	};

	enum
	{
		State_FindScenario,
		State_MoveToScenario,
		State_UseScenario,
		State_Flee,
		State_Finish
	};

	// See eEventResponseFleeFlags for fleeFlags
	CTaskScenarioFlee(CEntity* pSourceEntity, int fleeFlags);
	~CTaskScenarioFlee();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskScenarioFlee(m_pEventSourceEntity, m_FleeFlags);
	}

	// Task required functions
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_SCENARIO_FLEE;}

	// FSM functions
	virtual FSM_Return		ProcessPreFSM();
	virtual void			CleanUp();
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_FindScenario; }

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

private:
	// Is the specified state handled by the clone FSM
	bool StateChangeHandledByCloneFSM(s32 iState);

	// Return true if the ped needs to project a capsule forward to find potential environmental collisions.
	bool ShouldProbeForCollisions() const;

	// Cast the capsule forward asynchronously.
	void ProbeForCollisions();

protected:

	// State functions

	FSM_Return		FindScenario_OnUpdate();
	
	FSM_Return		MoveToScenario_OnEnter();
	FSM_Return		MoveToScenario_OnUpdate();

	FSM_Return		UseScenario_OnEnter();
	FSM_Return		UseScenario_OnUpdate();

	FSM_Return		Flee_OnEnter();
	FSM_Return		Flee_OnUpdate();

	// Member functions

	// Function that puts together the code which attempts to find a new flee scenario
	void			FindNewFleeScenario();

	// Add a count to the scenario point that we have chosen (or remove the count)
	void			UnreserveScenarioPoint();

	// Member variables

	// Source entity
	RegdEnt				m_pEventSourceEntity;	
	
	// The current scenarioPoint we are using
	RegdScenarioPnt		m_pScenarioPoint;

	// Time to keep track of how often we want to search for a new scenario
	CTaskGameTimer	m_scenarioSearchTimer;

	// Timer to keep track of how frequently we check the distance to our flee source
	CTaskGameTimer	m_sourceDistanceTimer;

	// This time controls how long we stay at our flee scenario before we are allowed to leave
	// Generally to prevent going in and immediately coming out because the threat is far enough away
	CTaskGameTimer	m_scenarioUseTimer;

	// Timer used to control when probe tests are issued
	CTaskGameTimer							m_ProbeTimer;

	// LoS probe for object collision avoidance
	WorldProbe::CShapeTestSingleResult		m_CapsuleResult;

	// If a collision was found, this is where it was located.
	Vec3V			m_vCollisionPoint;

	//the search radius in which to look for viable scenarios
	float			m_fFleeSearchRange;

	// PURPOSE:	The (real) scenario type of m_pScenarioPoint.
	int				m_ScenarioTypeReal;

	// These are flags, driven from custom event responses, that control what we are allowed to do
	// See eEventResponseFleeFlags
	u8				m_FleeFlags;

	// PURPOSE:  Does the ped have a clear obstruction in front of them?
	bool			m_bAboutToCollide;

	// PURPOSE:  Is the ped already steering to avoid a secondary collision target?
	bool			m_bAlteredPathForCollision;

	// PURPOSE:	Tunables for the class.
	static Tunables sm_Tunables;

#if __BANK
	// PURPOSE:  Debug drawing.
	static bool sm_bRenderProbeChecks;
#endif

};

class CTaskFlyAway : public CTaskFSMClone
{
public:

	enum
	{
		State_Check,
		State_Flee,
		State_Finish
	};

	CTaskFlyAway(CEntity* pSourceEntity);
	~CTaskFlyAway();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskFlyAway(m_pEventSourceEntity);
	}

	// Task required functions
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_FLY_AWAY;}

	// FSM required functions
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Flee; }

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

private:
	// Is the specified state handled by the clone FSM
	bool StateChangeHandledByCloneFSM(s32 iState);

protected:

	// State functions

	FSM_Return		Check_OnUpdate();

	void			Flee_OnEnter();
	FSM_Return		Flee_OnUpdate();

	// FSM functions

	// PURPOSE: Handle aborting the task
	virtual void DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* UNUSED_PARAM(pEvent));

	// Member function
	bool IsDirectionBlocked(Vec3V_In vPos, Vec3V_In vDir);

	// Member variables

	// Source entity
	RegdEnt		m_pEventSourceEntity;
	Vec3V		m_vFleeDirection;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskExhausedFlee
// - A version of smart flee that prevents the ped from maintaining a sprint for very long.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


class CTaskExhaustedFlee : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Starting energy.
		float		m_StartingEnergy;

		// PURPOSE: Energy lost per second while sprinting.
		float		m_EnergyLostPerSecond;

		// PURPOSE: Required distance threshold to the threat to consider sprinting.
		float		m_OuterDistanceThreshold;

		// PURPOSE: Required distance threshold to always sprint regardless of energy.
		// (Between the two bands sprinting is determined by energy).
		float		m_InnerDistanceThreshold;

		PAR_PARSABLE;
	};

	CTaskExhaustedFlee(CEntity* pSourceEntity);
	~CTaskExhaustedFlee();
	
	enum
	{
		State_Flee,
		State_Finish
	};

	virtual aiTask* Copy() const
	{
		return rage_new CTaskExhaustedFlee(m_pEventSourceEntity);
	}

	// Task required functions
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_EXHAUSTED_FLEE;}

	// FSM required functions
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Flee; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 


protected:

	// State functions
	void			Flee_OnEnter();
	FSM_Return		Flee_OnUpdate();


private:

	// Helper functions

	bool			ShouldLimitMBRBecauseofNM();

	// Member variables

	// Source entity
	RegdEnt					m_pEventSourceEntity;

	// Current energy of the ped
	float					m_fEnergy;

	// Whether or not the MBR was limited to walk because of NM.
	bool					m_bLimitingMBRBecauseOfNM;

	// Tunable struct
	static Tunables			sm_Tunables;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskWalkAway
// - A version of fleeing but with walking and very short distances.  Used by animals who can't hurry away, but still need
// - a minor response to some events.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


class CTaskWalkAway : public CTask
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	How far away is far enough when fleeing.
		float		m_SafeDistance;

		// PURPOSE: How long between readjusting route.
		float		m_TimeBetweenRouteAdjustments;

		PAR_PARSABLE;
	};


	CTaskWalkAway(CEntity* pSourceEntity);
	~CTaskWalkAway();

	enum
	{
		State_Flee,
		State_Finish
	};

	virtual aiTask* Copy() const
	{
		return rage_new CTaskWalkAway(m_pEventSourceEntity);
	}

	// Task required functions
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_WALK_AWAY;}

	// FSM required functions
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Flee; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 


protected:

	// State functions
	void			Flee_OnEnter();
	FSM_Return		Flee_OnUpdate();


private:

	// Member variables

	// Source entity
	RegdEnt					m_pEventSourceEntity;

	// Keep track of how long its been since a route adjustment.
	float					m_fRouteTimer;

	// Tunable struct
	static Tunables			sm_Tunables;
};

#if __BANK
class CFleeDebug
{
public:

	static void InitWidgets();
	static void Render();

	static bool	ms_bLookForCarsToSteal;
	static bool	ms_bLookForCover;
	static bool	ms_bForceMissionCarStealing;
	static bool ms_bDisplayFleeingOutOfPavementManagerDebug;
};
#endif // __BANK

//-------------------------------------------------------------------------
// Task info for smart flee
//-------------------------------------------------------------------------
class CClonedControlTaskSmartFleeInfo : public CSerialisedFSMTaskInfo
{
public:

	static const int iMAX_STOP_DISTANCE_SIZE = 20;
	static const int iIGNORE_PREFER_PAVEMENTS_TIME_SIZE = 16;

	CClonedControlTaskSmartFleeInfo(s32 state, const CEntity* pTarget, const CVehicle* pVehicle, u32 conditionalAnimGroupHash, const Vector3& vecTargetPos,  const Vector3& vecStartPos, float fStopDistance, float fIgnorePreferPavementsTimeLeft, fwFlags8 uExecConditions );
	CClonedControlTaskSmartFleeInfo();
	~CClonedControlTaskSmartFleeInfo();

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_SMART_FLEE;}
    virtual int GetScriptedTaskType() const;

	CVehicle* GetVehicle() { return m_VehicleEntity.GetVehicle(); }
	const CVehicle* GetVehicle() const { return m_VehicleEntity.GetVehicle(); }
	CEntity* GetTargetCEntity() { return m_TargetEntity.GetEntity(); }
    const CEntity* GetTargetCEntity() const { return m_TargetEntity.GetEntity(); }
	Vector3 GetStartPos() { return m_vStartPos; }
	Vector3 GetTargetPos() { return m_vTargetPos; }
	u32		GetConditionalAnimGroupHash()			{ return m_conditionalAnimGroupHash;  }
	float GetStopDistance() const { return m_fStopDistance; }
	float GetIgnorePreferPavementsTimeLeft() const { return m_fIgnorePreferPavementsTimeLeft; }
	u8 GetExecConditions() const { return m_uExecConditions; }

	virtual CTaskFSMClone *CreateCloneFSMTask();
    virtual CTask         *CreateLocalTask(fwEntity *pEntity);

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskSmartFlee::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Smart Flee State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bHasTargetEntity = m_TargetEntity.GetEntityID() != NETWORK_INVALID_OBJECT_ID;

		SERIALISE_BOOL(serialiser, bHasTargetEntity,"Has target entity");

		if(bHasTargetEntity || serialiser.GetIsMaximumSizeSerialiser() )
		{
			ObjectId targetID = m_TargetEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "Target entity");
			m_TargetEntity.SetEntityID(targetID);
		}

		bool bHasVehicle = m_VehicleEntity.GetVehicleID() != NETWORK_INVALID_OBJECT_ID;

		SERIALISE_BOOL(serialiser, bHasVehicle,"Has vehicle");

		if(bHasVehicle || serialiser.GetIsMaximumSizeSerialiser() )
		{
			ObjectId vehicleID = m_VehicleEntity.GetVehicleID();
			SERIALISE_OBJECTID(serialiser, vehicleID, "Vehicle");
			m_VehicleEntity.SetVehicleID(vehicleID);
		}
		
		SERIALISE_POSITION(serialiser, m_vTargetPos, "Target Position");
		SERIALISE_POSITION(serialiser, m_vStartPos, "Start Position");

		bool bHasAmbientAnimGrouphash = m_conditionalAnimGroupHash != 0;

		SERIALISE_BOOL(serialiser, bHasAmbientAnimGrouphash,"Has Smart Flee Ambient Anim group Hash");

		if(bHasAmbientAnimGrouphash || serialiser.GetIsMaximumSizeSerialiser() )
		{
			SERIALISE_UNSIGNED(serialiser, m_conditionalAnimGroupHash,	SIZEOF_CONDITIONAL_ANIM_GROUP_HASH, "Smart Flee Conditional Anim Group hash");
		}
		else
		{
			m_conditionalAnimGroupHash =0;
		}

		SERIALISE_UNSIGNED(serialiser, m_uExecConditions, datBitsNeeded<CTaskSmartFlee::EC_LAST_SYNCED_FLAG>::COUNT, "Exec conditions");
		SERIALISE_PACKED_FLOAT(serialiser, m_fStopDistance, CTaskSmartFlee::ms_fMaxStopDistance, iMAX_STOP_DISTANCE_SIZE, "Stop distance");
		SERIALISE_PACKED_FLOAT(serialiser, m_fIgnorePreferPavementsTimeLeft, CTaskSmartFlee::ms_fMaxIgnorePreferPavementsTime, iIGNORE_PREFER_PAVEMENTS_TIME_SIZE, "Min Fleeing Time To Prefer Pavements");
	}

private:
	static const unsigned int SIZEOF_CONDITIONAL_ANIM_GROUP_HASH		= 32;

	CClonedControlTaskSmartFleeInfo(const CClonedControlTaskSmartFleeInfo &);

	CClonedControlTaskSmartFleeInfo &operator=(const CClonedControlTaskSmartFleeInfo &);

	CSyncedVehicle	m_VehicleEntity;
	CSyncedEntity	m_TargetEntity;
	Vector3			m_vStartPos;	// start position of fleeing ped - can be used to return to after fleeing
	Vector3			m_vTargetPos;	// target position - used if the target is null

	u32				m_conditionalAnimGroupHash;  //for ambient anims
	float			m_fStopDistance;
	float			m_fIgnorePreferPavementsTimeLeft;
	u8				m_uExecConditions;

};

//-------------------------------------------------------------------------
// Task info for Escape Blast
//-------------------------------------------------------------------------
class CClonedControlTaskEscapeBlastInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedControlTaskEscapeBlastInfo();
	CClonedControlTaskEscapeBlastInfo(s32 escapeBlastState);
	~CClonedControlTaskEscapeBlastInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_ESCAPE_BLAST;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskEscapeBlast::State_Finish>::COUNT;}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

};


//-------------------------------------------------------------------------
// Task info for Fly Away
//-------------------------------------------------------------------------
class CClonedControlTaskFlyAwayInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedControlTaskFlyAwayInfo();
	CClonedControlTaskFlyAwayInfo(s32 flyAwayState, CEntity* pSourceEntity);
	~CClonedControlTaskFlyAwayInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_FLY_AWAY;}

	CEntity* GetEventsSourceEntity() { return m_EventSourceEntity.GetEntity(); }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskFlyAway::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Fly away State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		ObjectId sourceEntityID = m_EventSourceEntity.GetEntityID();
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_OBJECTID(serialiser, sourceEntityID, "Fly away source  entity");
		m_EventSourceEntity.SetEntityID(sourceEntityID);
	}

private:

	CSyncedEntity m_EventSourceEntity;
};

//-------------------------------------------------------------------------
// Task info for Scenario Flee
//-------------------------------------------------------------------------
class CClonedControlTaskScenarioFleeInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedControlTaskScenarioFleeInfo();
	CClonedControlTaskScenarioFleeInfo(s32 scenarioFleeState, CEntity* pSourceEntity  );
	~CClonedControlTaskScenarioFleeInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_SCENARIO_FLEE;}

	CEntity*	GetEventsSourceEntity()		{ return m_EventSourceEntity.GetEntity(); }
	int			GetFleeFlags()				{ return m_iFleeFlags; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskScenarioFlee::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Scenario Flee State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		ObjectId sourceEntityID = m_EventSourceEntity.GetEntityID();

		SERIALISE_OBJECTID(serialiser, sourceEntityID, "Scenario source entity");
		SERIALISE_INTEGER(serialiser, m_iFleeFlags, 32, "Task Scenario Flee Flags");
		
		m_EventSourceEntity.SetEntityID(sourceEntityID);
	}

private:
	CSyncedEntity	m_EventSourceEntity;
	int				m_iFleeFlags;

};

#endif // INC_TASK_FLEE_H
