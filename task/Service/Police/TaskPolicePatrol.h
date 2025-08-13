// FILE :    TaskPolicePatrol.h
// PURPOSE : Basic patrolling task of a police officer, patrol in vehicle or on foot until given specific orders by dispatch
// CREATED : 25-05-2009

#ifndef INC_TASK_POLICE_DEFAULT_H
#define INC_TASK_POLICE_DEFAULT_H

//Game headers
#include "game/Dispatch/DispatchHelpers.h"
#include "Network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"
#include "Scene/RegdRefTypes.h"
#include "Task/Default/TaskUnalerted.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioChaining.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

//Forward declarations
class CEvent;
class CPoliceOrder;

/////////////////////////////////
//		CRIMINAL SCANNER       //
/////////////////////////////////

// Purpose: Scanner to find criminal peds for the ambient police to arrest

class CCriminalScanner
{
public:
	CCriminalScanner();
	~CCriminalScanner();

	void Scan(CPed* pPed, bool bIgnoreTimers = false);
	void ClearScan() {m_CriminalEnt = NULL;}

	CEntity* GetEntity() const {return m_CriminalEnt;}

private:
	RegdEnt		m_CriminalEnt;
};

/////////////////////////////////
//		TASK POLICE			   //
/////////////////////////////////

// Purpose: Main Controlling Task For Police

class CTaskPolice : public CTask
{
public:

	// FSM states
	enum
	{
		State_Start = 0,
		State_Resume,
		State_Unalerted,
		State_EnterVehicle,
		State_WaitForBuddiesToEnterVehicle,
		State_PatrolOnFoot,		
		State_PatrolInVehicleAsDriver,
		State_PatrolInVehicleFastAsDriver,
		State_PatrolInVehicleAsPassenger,
		State_RespondToOrder,
		State_PursueCriminal,
		State_Finish
	};

	// Constructor
	CTaskPolice(CTaskUnalerted* pUnalertedTask = NULL);

	// Destructor
	virtual ~CTaskPolice();

	// Task required implementations
	virtual aiTask* Copy() const;
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_POLICE; }
	virtual s32	GetDefaultStateAfterAbort() const { return State_Resume; }

	virtual	CScenarioPoint* GetScenarioPoint() const;
	virtual int GetScenarioPointType() const;

	// Interface Functions
	CTaskUnalerted* GetTaskUnalerted() { return m_pUnalertedTask && m_pUnalertedTask->GetTaskType() == CTaskTypes::TASK_UNALERTED ? static_cast<CTaskUnalerted*>(m_pUnalertedTask.Get()) : NULL; }

private:

	// PURPOSE: Handle aborting the task
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// State Machine Update Functions
	FSM_Return		 UpdateFSM(const s32 iState, const FSM_Event iEvent);
	FSM_Return		 ProcessPostFSM();

	// State Functions
	FSM_Return		 Start_OnUpdate();
	void			 Start_OnExit();
	FSM_Return		 Resume_OnUpdate();
	void			 Unalerted_OnEnter();
	FSM_Return		 Unalerted_OnUpdate();
	void			 EnterVehicle_OnEnter();
	FSM_Return		 EnterVehicle_OnUpdate();
	void			 WaitForBuddiesToEnterVehicle_OnEnter();
	FSM_Return		 WaitForBuddiesToEnterVehicle_OnUpdate();
	void			 PatrolOnFoot_OnEnter();
	FSM_Return		 PatrolOnFoot_OnUpdate();
	void			 PatrolOnFoot_OnExit();
	void			 PatrolInVehicleAsDriver_OnEnter();
	FSM_Return		 PatrolInVehicleAsDriver_OnUpdate();
	void			 PatrolInVehicleFastAsDriver_OnEnter();
	FSM_Return		 PatrolInVehicleFastAsDriver_OnUpdate();
	FSM_Return		 PatrolInVehicleFastAsDriver_OnExit();
	void			 PatrolInVehicleAsPassenger_OnEnter();
	FSM_Return		 PatrolInVehicleAsPassenger_OnUpdate();
	void			 RespondToOrder_OnEnter();
	FSM_Return		 RespondToOrder_OnUpdate();
	void			 RespondToOrder_OnExit();
	void			 PursueCriminal_OnEnter();
	FSM_Return		 PursueCriminal_OnUpdate();

public:

	// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	void	CacheScenarioPointToReturnTo();
	void	CheckPlayerAudio(CPed* pPed);
	int		ChooseStateToResumeTo(bool& bKeepSubTask) const;
	void	SetDriveState(CPed* pPed);

private:

	CCriminalScanner m_CriminalScanner;
	CVehicleClipRequestHelper m_VehicleClipRequestHelper;
	fwClipSetRequestHelper m_ExitToAimClipSetRequestHelper;
	fwMvClipSetId m_ExitToAimClipSetId;

	RegdEnt			m_pPedToPursue;
	RegdTask		m_pUnalertedTask;
	RegdScenarioPnt	m_pPointToReturnTo;
	CScenarioPointChainUseInfoPtr m_SPC_UseInfo; //NOTE: this class does not own this pointer do not delete it.
	int				 m_iPointToReturnToRealTypeIndex;
	bool			 m_bForceStartOnAbort;
};

/////////////////////////////////////////////////
//		  TASK POLICE ORDER RESPONSE		   //
/////////////////////////////////////////////////

// Purpose: Task run when ped has a valid order

class CTaskPoliceOrderResponse : public CTask
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float	m_MaxTimeToWait;
		float	m_MaxSpeedForVehicleMovingSlowly;
		float	m_MinSpeedForVehicleMovingQuickly;
		float	m_TimeBeforeOvertakeToMatchSpeedWhenPulledOver;
		float	m_TimeBeforeOvertakeToMatchSpeedWhenCruising;
		float	m_CheatPowerIncreaseForMatchSpeed;
		float	m_MinTimeToWander;
		float	m_MaxTimeToWander;
		float	m_TimeBetweenExitVehicleRetries;

		PAR_PARSABLE;
	};
	
	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_Resume,
		State_RespondToOrder,
		State_SwapWeapon,
		State_EnterVehicle,
		State_ExitVehicle,
		State_GoToIncidentLocationInVehicle,
		State_InvestigateIncidentLocationInVehicle,
		State_InvestigateIncidentLocationAsPassenger,
		State_FindIncidentLocationOnFoot,
		State_GoToIncidentLocationOnFoot,
		State_GoToIncidentLocationAsPassenger,
		State_UnableToReachIncidentLocation,
		State_SearchInVehicle,
		State_FindPositionToSearchOnFoot,
		State_SearchOnFoot,
		State_WanderOnFoot,
		State_WanderInVehicle,
		State_WaitForArrestedPed,
		State_Combat,
		State_WaitPulledOverInVehicle,
		State_WaitCruisingInVehicle,
		State_WaitAtRoadBlock,
		State_MatchSpeedInVehicle,
		State_Finish
	};
	
	enum OrderState
	{
		OS_Continue,
		OS_Failed,
		OS_Succeeded,
	};

private:

	enum RunningFlags
	{
		RF_CanSeeTarget	= BIT0,
	};

public:

	CTaskPoliceOrderResponse ();
	virtual ~CTaskPoliceOrderResponse();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskPoliceOrderResponse(); }
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_POLICE_ORDER_RESPONSE; }
	
	// FSM required implementations
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort() const { return State_Resume; }
	virtual bool				ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);


// debug:
#if !__FINAL
	void						Debug() const;
	void						DrawDebug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	Vector3						m_vCurrentSearchLocation;
#endif // !__FINAL
private:
	// Local state function calls
	// Start
	FSM_Return Start_OnUpdate				();
	// Resume
	FSM_Return Resume_OnUpdate				();
	// Respond to order
	FSM_Return RespondToOrder_OnUpdate		();
	// SwapWeapon
	void SwapWeapon_OnEnter					();
	FSM_Return SwapWeapon_OnUpdate			();
	// EnterVehicle
	void EnterVehicle_OnEnter				();
	FSM_Return EnterVehicle_OnUpdate		();
	// ExitVehicle
	void ExitVehicle_OnEnter				();
	FSM_Return ExitVehicle_OnUpdate			();
	// PatrolAsPassenger
	void PatrolAsPassenger_OnEnter			();
	FSM_Return PatrolAsPassenger_OnUpdate	();
	// PatrolOnFoot
	void PatrolOnFoot_OnEnter				();
	FSM_Return PatrolOnFoot_OnUpdate		();
	// GoToIncidentLocationInVehicle
	void GoToIncidentLocationInVehicle_OnEnter	();
	FSM_Return GoToIncidentLocationInVehicle_OnUpdate();
	// GoToIncidentLocationAsPassenger
	void GoToIncidentLocationAsPassenger_OnEnter	();
	FSM_Return GoToIncidentLocationAsPassenger_OnUpdate();
	// InvestigateIncidentLocationInVehicle
	void InvestigateIncidentLocationInVehicle_OnEnter	();
	FSM_Return InvestigateIncidentLocationInVehicle_OnUpdate();
	// InvestigateIncidentLocationAsPassenger
	void InvestigateIncidentLocationAsPassenger_OnEnter	();
	FSM_Return InvestigateIncidentLocationAsPassenger_OnUpdate();
	// 
	void FindIncidentLocationOnFoot_OnEnter		();
	FSM_Return FindIncidentLocationOnFoot_OnUpdate();
	// GoToIncidentLocationOnFoot
	void GoToIncidentLocationOnFoot_OnEnter		();
	FSM_Return GoToIncidentLocationOnFoot_OnUpdate();
	void UnableToReachIncidentLocation_OnEnter();
	FSM_Return UnableToReachIncidentLocation_OnUpdate();
	void SearchInVehicle_OnEnter();
	FSM_Return SearchInVehicle_OnUpdate();
	void FindPositionToSearchOnFoot_OnEnter();
	FSM_Return FindPositionToSearchOnFoot_OnUpdate(); 
	void SearchOnFoot_OnEnter();
	FSM_Return SearchOnFoot_OnUpdate();
	void WanderOnFoot_OnEnter();
	FSM_Return WanderOnFoot_OnUpdate();
	void WanderInVehicle_OnEnter();
	FSM_Return WanderInVehicle_OnUpdate();
	// WaitForArrestedPed
	void WaitForArrestedPed_OnEnter();
	FSM_Return WaitForArrestedPed_Update();
	
	void Combat_OnEnter();
	FSM_Return Combat_OnUpdate();

	void WaitPulledOverInVehicle_OnEnter();
	FSM_Return WaitPulledOverInVehicle_OnUpdate();
	
	void WaitCruisingInVehicle_OnEnter();
	FSM_Return WaitCruisingInVehicle_OnUpdate();
	
	void WaitAtRoadBlock_OnEnter();
	FSM_Return WaitAtRoadBlock_OnUpdate();
	void WaitAtRoadBlock_OnExit();
	
	void MatchSpeedInVehicle_OnEnter();
	FSM_Return MatchSpeedInVehicle_OnUpdate();
	void MatchSpeedInVehicle_OnExit();
	
	Vec3V_Out	CalculateDirectionForPed(const CPed& rPed) const;
	Vec3V_Out	CalculateDirectionForPhysical(const CPhysical& rPhysical) const;
	bool		CanOfficerSeeTarget() const;
	int			ChooseStateToResumeTo(bool& bKeepSubTask) const;
	int			ConvertPoliceOrderFromInVehicleToOnFoot(int iPoliceOrderType) const;
	bool		DoWeCareIfWeCanSeeTheTarget() const;
	bool		DoesPoliceOrderRequireDrivingAVehicle(int iPoliceOrderType) const;
	OrderState	GenerateOrderState() const;
	OrderState	GenerateOrderStateForCombat() const;
	OrderState	GenerateOrderStateForMatchSpeed() const;
	OrderState	GenerateOrderStateForSearchInVehicle() const;
	OrderState	GenerateOrderStateForSearchOnFoot() const;
	OrderState	GenerateOrderStateForWaitCruising() const;
	OrderState	GenerateOrderStateForWaitPulledOver() const;
	int			GeneratePoliceOrderType() const;
	int			GeneratePoliceOrderTypeForEnterVehicle() const;
	int			GeneratePoliceOrderTypeForGoToIncidentLocationAsPassenger() const;
	int			GeneratePoliceOrderTypeForWaitAtRoadBlock() const;
	bool		GenerateRandomSearchPosition(Vec3V_InOut vSearchPosition);
	float		GenerateTimeToWander() const;
	int			GetCurrentPoliceOrderType() const;
	CPed*		GetTarget() const;
	bool		HasOfficerWaitedTooLong() const;
	bool		HasSearchLocation() const;
	bool		HasSearchLocation(Vec3V_InOut vSearchLocation) const;
	bool		HasTargetPassedOfficer() const;
	bool		IsDriverExitingVehicle() const;
	bool		IsDriverInjured() const;
	bool		IsDriverValid() const;
	bool		IsOfficerInVehicle() const;
	bool		IsOfficerVehicleMovingAsFastAsTargetVehicle() const;
	bool		IsSubTaskActive() const;
	bool		IsTargetInVehicle() const;
	bool		IsTargetMovingAwayFromOfficer() const;
	bool		IsTargetVehicleMovingSlowly() const;
	bool		IsTargetWithinDistance(float fDistance) const;
	void		ProcessCanSeeTarget();
	void		ProcessDriver();
	bool		ProcessOrder();
	bool		ProcessOrderState();
	void		RequestNewOrder(const bool bSuccess);
	void		SetCheatPowerIncrease(float fCheatPowerIncrease);
	void		SetSearchLocation(Vec3V_In vSearchLocation);
	void		SetStateForPoliceOrder(const CPoliceOrder& rOrder);
	void		SetStateForDefaultOrder();
	bool		ShouldIgnoreCombatEvents() const;
	bool		ShouldSwapWeapon() const;
	bool		WillTargetVehicleOvertakeOfficerVehicleWithinTime(float fTime) const;
	bool		CreateInvestigateIncidentLocationInVehicleTask();
	CPed*		FindInvestigateIncidentSuspiciousPed();
	void		ProcessInvestigateIncidentInVehicleLookAt();

private:

	Vec3V										m_vIncidentLocationOnFoot;
	Vec3V										m_vPositionToSearchOnFoot;
	CDispatchHelperForIncidentLocationOnFoot	m_HelperForIncidentLocationOnFoot;
	CDispatchHelperSearchOnFoot					m_HelperForSearchOnFoot;
	float										m_fTimeToWander;
	float										m_fTimeSinceDriverHasBeenValid;
	fwFlags8									m_uRunningFlags;
	CTaskGameTimer								m_WaitWhileInvestigatingIncidentTimer;
	CTaskGameTimer								m_SlowDrivingWhileInvestigatingIncidentTimer;
	CAITarget									m_InvestigatingIncidentLookAt;

	bool										m_bWantedConesDisabled;

private:

	static Tunables	sm_Tunables;
	
};

/////////////////////////////////
//		TASK ARREST PED 2	   //
/////////////////////////////////

class CTaskArrestPed2 : public CTaskFSMClone
{
public:

	// Constructor
	CTaskArrestPed2(CPed *pOtherPed, bool bArrestOtherPed);

	// Destructor
	virtual ~CTaskArrestPed2();

	// Task required functions
	virtual aiTask*		Copy() const { return rage_new CTaskArrestPed2(m_pOtherPed, m_bArrestOtherPed); }
	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_ARREST_PED2; }
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32			GetDefaultStateAfterAbort() const { return State_Finish; }

	// Task optional functions
	virtual FSM_Return	ProcessPreFSM();
	virtual void		CleanUp();

	// Clone task implementation
	virtual CTaskInfo*	CreateQueriableState() const;
	virtual void		ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return	UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual void		OnCloneTaskNoLongerRunningOnOwner();

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState);
	void Debug() const;
#endif // !__FINAL

	bool IsCop() const { return m_bArrestOtherPed; }
	bool IsCrook() const { return !m_bArrestOtherPed; }

	bool IsOtherPedRunningTask() const;
	s32 GetOtherPedState() const;

	bool IsNetPedRunningTask() const;
	s32 GetNetPedState() const;

	bool IsPlayerLockedOn() const;
	void SetPlayerLockedOn();
	void UpdatePlayerLockedOn();

	virtual bool UseCustomGetup() { return true; }
	bool AddGetUpSets(CNmBlendOutSetList &sets, CPed* pPed);

	// FSM states
	enum 
	{
		State_Start = 0,

		State_CopFaceCrook,
		State_CopOrderGetUp,
		State_CopWaitForGetUp,
		State_CopThrowHandcuffs,
		State_CopWaitForHandcuffsOn,

		State_CrookWaitWhileProne,
		State_CrookGetUp,
		State_CrookFaceCopPutHandsUp,
		State_CrookFaceCopWithHandsUp,
		State_CrookWaitForHandcuffs,
		State_CrookPutHandcuffsOn,
		State_CrookWaitWhileStanding,

		State_Finish
	};

private:

	// State Functions
	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();

	void		CopFaceCrook_OnEnter();
	FSM_Return	CopFaceCrook_OnUpdate();

	void		CopOrderGetUp_OnEnter();
	FSM_Return	CopOrderGetUp_OnUpdate();

	void		CopWaitForGetUp_OnEnter();
	FSM_Return	CopWaitForGetUp_OnUpdate();

	void		CopThrowHandcuffs_OnEnter();
	FSM_Return	CopThrowHandcuffs_OnUpdate();

	void		CopWaitForHandcuffsOn_OnEnter();
	FSM_Return	CopWaitForHandcuffsOn_OnUpdate();

	void		CopHolsterWeapon_OnEnter();
	FSM_Return	CopHolsterWeapon_OnUpdate();

	void		CrookWaitWhileProne_OnEnter();
	FSM_Return	CrookWaitWhileProne_OnUpdate();

	void		CrookGetUp_OnEnter();
	FSM_Return	CrookGetUp_OnUpdate();

	void		CrookFaceCopPutHandsUp_OnEnter();
	FSM_Return	CrookFaceCopPutHandsUp_OnUpdate();

	void		CrookFaceCopWithHandsUp_OnEnter();
	FSM_Return	CrookFaceCopWithHandsUp_OnUpdate();

	void		CrookWaitForHandcuffs_OnEnter();
	FSM_Return	CrookWaitForHandcuffs_OnUpdate();

	void		CrookPutHandcuffsOn_OnEnter();
	FSM_Return	CrookPutHandcuffsOn_OnUpdate();

	void		CrookWaitWhileStanding_OnEnter();
	FSM_Return	CrookWaitWhileStanding_OnUpdate();

	void		Finish_OnEnter();
	FSM_Return	Finish_OnUpdate();

	RegdPed			m_pOtherPed;
	bool			m_bArrestOtherPed;
	fwMvClipSetId	m_ClipSetId;

	fwClipSetRequestHelper m_ClipSetHelper;
	CMoveNetworkHelper m_MoveNetworkHelper;
};

class CClonedTaskArrestPed2Info : public CSerialisedFSMTaskInfo
{
public:

	CClonedTaskArrestPed2Info(s32 iState, CPed *pOtherPed, bool bArrestOtherPed);
	CClonedTaskArrestPed2Info() {}
	~CClonedTaskArrestPed2Info() {}

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_ARREST_PED2; }

	CPed *GetOtherPed() { return NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_OtherPedId)); }
	bool GetArrestOtherPed() { return m_bArrestOtherPed; }
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool HasState() const { return true; }
	virtual u32 GetSizeOfState() const { return datBitsNeeded< CTaskArrestPed2::State_Finish >::COUNT; }
	static const char *GetStateName(s32) { return "Arrest State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_OBJECTID(serialiser, m_OtherPedId, "Other Ped");
		SERIALISE_BOOL(serialiser, m_bArrestOtherPed, "Arrest Other Ped");
	}

private:

	CClonedTaskArrestPed2Info(const CClonedTaskArrestPed2Info &);

	CClonedTaskArrestPed2Info &operator=(const CClonedTaskArrestPed2Info &);

	ObjectId m_OtherPedId;
	bool m_bArrestOtherPed;
};

/////////////////////////////////
//		TASK ARREST PED		   //
/////////////////////////////////

// Purpose: Task to arrest a ped on wanted level 1

class CTaskArrestPed : public CTask
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_AimDistance;
		float m_ArrestDistance;
		float m_ArrestInVehicleDistance;
		float m_MoveToDistanceInVehicle;
		float m_TargetDistanceFromVehicleEntry;
		float m_ArrestingCopMaxDistanceFromTarget;
		float m_BackupCopMaxDistanceFromTarget;
		float m_MinTimeOnFootTargetStillForArrest;
		u32	  m_TimeBetweenPlayerInVehicleBustAttempts;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

	// Constructor
	CTaskArrestPed(const CPed* pPedToArrest, bool bForceArrest = false, bool bQuitOnCombat = false);

	// Destructor
	virtual ~CTaskArrestPed();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskArrestPed(m_pPedToArrest, m_bWillArrest); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_ARREST_PED; }
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// Task optional functions
	FSM_Return					ProcessPreFSM	();
	FSM_Return					ProcessPostFSM	();
	virtual void				CleanUp			();
	virtual bool				ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	bool						OnShouldAbort(const aiEvent* pEvent);

	bool GetTryToTalkTargetDown() const { return m_bTryToTalkTargetDown; }
	void SetTryToTalkTargetDown(bool val) { m_bTryToTalkTargetDown = val; }

	bool GetWillArrest() const { return m_bWillArrest; }

	const CPed* GetPedToArrest() const { return m_pPedToArrest; }

	bool IsTargetBeingBusted() const;

	void SetTargetResistingArrest() { m_bPedIsResistingArrest = true; }
	
	static bool CanArrestPedInVehicle(const CPed* pPed, const CPed* pTarget, bool bSetLastFailedBustTime, bool bCheckForArrestInVehicleAnims, bool bDoCapsuleTest);

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void						Debug() const;
#endif // !__FINAL

private:

	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_ExitVehicle,
		State_MoveToTarget,
		State_ArrestInVehicleTarget,
		State_MoveToTargetInVehicle,
		State_AimAtTarget,
		State_Combat,
		State_WaitForRestart,
		State_SwapWeapon,
		State_Finish
	};

	// State Functions
	void						Start_OnEnter			();
	FSM_Return					Start_OnUpdate			();

	void						ExitVehicle_OnEnter		();
	FSM_Return					ExitVehicle_OnUpdate	();

	void						MoveToTarget_OnEnter	();
	FSM_Return					MoveToTarget_OnUpdate	();

	void						ArrestInVehicleTarget_OnEnter	();
	FSM_Return					ArrestInVehicleTarget_OnUpdate	();

	void						MoveToTargetInVehicle_OnEnter	();
	FSM_Return					MoveToTargetInVehicle_OnUpdate	();

	void						AimAtTarget_OnEnter();
	FSM_Return					AimAtTarget_OnUpdate();

	void						Combat_OnEnter	();
	FSM_Return					Combat_OnUpdate	();

	void						WaitForRestart_OnEnter();
	FSM_Return					WaitForRestart_OnUpdate();

	void						SwapWeapon_OnEnter();
	FSM_Return					SwapWeapon_OnUpdate();

	s32		CheckForDesiredArrestState();
	void	IncreaseWantedAndReportCrime();
	bool	IsHoldingFireDueToLackOfHostility() const;
	bool	IsTargetConsideredDangerous() const;
	bool	IsTargetStill();
	bool	IsAnotherCopCloserToTarget();
	void	UpdateTalkTargetDown();
	bool	StreamClipSet();

	void	SetPedIsArrestingTarget(bool bIsArrestingTarget);
	bool	CanPlayArrestAudio();
	bool	IsTargetFacingAway() const;

private:

	fwClipSetRequestHelper		m_clipSetRequestHelper;
	RegdConstPed				m_pPedToArrest;
	fwMvClipSetId				m_ExitToAimClipSetId;
	bool						m_bWillArrest : 1;
	bool						m_bPedIsResistingArrest : 1;
	bool						m_bTryToTalkTargetDown : 1;
	bool						m_bForceArrest : 1;
	bool						m_bQuitOnCombat : 1;
	bool						m_bIsApproachingTarget : 1;
	bool						m_bHasCheckCanArrestInVehicle : 1;
	bool						m_bCanArrestTargetInVehicle : 1;
	bool						m_bWasTargetEnteringVehicleLastFrame : 1;
	u32							m_uTimeMoveOnFootSpeedSet;
	float						m_fTalkingTargetDownTimer;
	float						m_fGetOutTimer;
	float						m_fTimeTargetStill;
	float						m_fTimeUntilNextDialogue;

	Vector3						m_vTargetPositionWhenBeingTalkedDown;

	static CVehicleClipRequestHelper ms_VehicleClipRequestHelper;
};

/////////////////////////////////
//		TASK BUSTED			   //
/////////////////////////////////

// Purpose: Player task to play hands up clip

class CTaskBusted : public CTask
{
public:

	// Constructor
	CTaskBusted(const CPed* pArrestingPed);

	// Destructor
	virtual ~CTaskBusted();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskBusted(m_pArrestingPed); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_BUSTED; }
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// Task optional functions
	virtual FSM_Return			ProcessPreFSM	();
	virtual void				CleanUp			();
	virtual bool				ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void				DoAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	const CPed* GetArrestingPed() { return m_pArrestingPed; }
	bool IsPlayingBustedClip() const { return GetState() == State_PlayClip; }

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void						Debug() const;
#endif // !__FINAL

private:

	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_PlayClip,
		State_Finish
	};

	// State Functions
	void						Start_OnEnter		();
	FSM_Return					Start_OnUpdate		();

	void						PlayClip_OnEnter	();
	FSM_Return					PlayClip_OnUpdate	();

	FSM_Return					Finish_OnUpdate		();

	RegdConstPed	m_pArrestingPed;

	fwMvClipSetId	m_ClipSetId;
	fwMvClipSetId	m_VehClipSetId;

	float			m_fAllowBreakoutTime;
	float			m_fPreventBreakoutTime;

	bool			m_bDroppedWeapon;
	bool			m_bToldEveryoneToBackOff;

	static const fwMvBooleanId	ms_DropWeaponId;
	static const fwMvBooleanId	ms_AllowBreakoutId;
	static const fwMvBooleanId	ms_PreventBreakoutId;
};

/////////////////////////////////////////////////
//		TASK POLICE WANTED RESPONSE			   //
/////////////////////////////////////////////////

// Purpose: Task run when a cop gets a wanted aquaintance event

class CTaskPoliceWantedResponse : public CTask
{
public:

	// Constructor
	CTaskPoliceWantedResponse(CPed* pWantedPed);

	// Destructor
	virtual ~CTaskPoliceWantedResponse();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskPoliceWantedResponse(m_pWantedPed); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_POLICE_WANTED_RESPONSE; }
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Resume; }

	// Task optional functions
	virtual FSM_Return			ProcessPreFSM	();
	virtual void				CleanUp			();

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void						Debug() const;
#endif // !__FINAL

private:

	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_Resume,
		State_Combat,
		State_Finish
	};

	FSM_Return	Start_OnUpdate();
	FSM_Return	Resume_OnUpdate();
	void		Combat_OnEnter();
	FSM_Return	Combat_OnUpdate();

private:

	int	ChooseStateToResumeTo(bool& bKeepSubTask) const;

private:

	RegdPed	m_pWantedPed;
};

#endif // INC_TASK_POLICE_DEFAULT_H
