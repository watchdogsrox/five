// FILE :    TaskAgitated.h
// CREATED : 10-5-2011

#ifndef TASK_AGITATED_H
#define TASK_AGITATED_H

// Rage headers
#include "atl/array.h"
#include "atl/queue.h"

// Game headers
#include "task/Response/info/AgitatedEnums.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CAgitatedAction;
class CAgitatedContext;
class CAgitatedPersonality;
class CAgitatedReaction;
class CAgitatedResponse;
class CAgitatedSay;
class CAgitatedState;
class CAgitatedTalkResponse;
class CEvent;
class CTaskAgitatedAction;

// Defines
#define CAPTURE_AGITATED_HISTORY __DEV

////////////////////////////////////////////////////////////////////////////////
// CTaskAgitated
////////////////////////////////////////////////////////////////////////////////

class CTaskAgitated : public CTask
{

public:

	struct Tunables : CTuning
	{
		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_Info;
			bool m_Hashes;
			bool m_History;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Rendering m_Rendering;
		
		float m_TimeBetweenLookAts;

		// Velocity magnitude squared threshold for testing if the agitator is moving.
		float m_MovingAwayVelocityMSThreshold;

		PAR_PARSABLE;
	};
	
public:

	enum ConfigFlags
	{
		CF_IgnoreForcedAudioFailures	= BIT0,
	};

	enum State
	{
		State_Start = 0,
		State_Resume,
		State_Listen,
		State_React,
		State_Finish
	};

#if CAPTURE_AGITATED_HISTORY
public:

	struct HistoryEntry
	{
		HistoryEntry()
		: m_hReaction()
		, m_uFlags(0)
		{}

		atHashWithStringNotFinal	m_hReaction;
		fwFlags32					m_uFlags;
	};
#endif

private:
	
	enum RunningFlags
	{
		RF_OverrodeOnFootClipSet	= BIT0,
		RF_IsOnSecondaryTaskTree	= BIT1,
		RF_DontResetOnFootClipSet	= BIT2,
		RF_HasBeenActivelyProvoked	= BIT3,
		RF_IsAgitatorArmed			= BIT4,
		RF_HasAgitatorBeenKilled	= BIT5,
		RF_WasFriendlyWithAgitator	= BIT6,
		RF_HasBeenHit				= BIT7,
	};
	
public:

	explicit CTaskAgitated(CPed* pAgitator);
	virtual ~CTaskAgitated();

public:

			fwFlags8&	GetConfigFlags()							{ return m_uConfigFlags; }
			u32			GetLastAgitationTime()				const	{ return m_uLastAgitationTime; }
	const	CPed*		GetLeader()							const	{ return m_pLeader; }
			float		GetTimeInAgitatedState()			const	{ return m_fTimeInAgitatedState; }
			float		GetTimeSinceAudioEndedForState()	const	{ return m_fTimeSinceAudioEndedForState; }
			float		GetTimeSpentWaitingInState()		const	{ return m_fTimeSpentWaitingInState; }
	
public:

	void SetLeader(const CPed* pPed)										{ m_pLeader = pPed; }
	void SetResponseOverride(atHashWithStringNotFinal hResponseOverride)	{ m_hResponseOverride = hResponseOverride; }

public:

	bool HasBeenActivelyProvoked() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_HasBeenActivelyProvoked));
	}

	bool WasFriendlyWithAgitator() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_WasFriendlyWithAgitator));
	}
	
public:

	void AddAnger(float fAnger);
	void AddFear(float fFear);
	void ApplyAgitation(AgitatedType nType);
	bool CanCallPolice() const;
	bool CanFight() const;
	bool CanHurryAway() const;
	bool CanWalkAway() const;
	void ChangeResponse(atHashWithStringNotFinal hResponse, atHashWithStringNotFinal hState);
	void ChangeState(atHashWithStringNotFinal hState);
	void ChangeTask(CTask* pTask);
	void ClearAgitation(AgitatedType nType);
	void ClearHash(atHashWithStringNotFinal hValue);
	bool HasAudioEnded() const;
	bool HasBeenHostileFor(float fMinTime, float fMaxTime) const;
	bool HasBeenHostileFor(float fMinTime) const;
	bool HasFriendsNearby(u8 uMin) const;
	bool HasLeader() const;
	bool HasLeaderBeenFightingFor(float fMinTime, float fMaxTime) const;
	bool HasLeaderBeenFightingFor(float fMinTime) const;
	bool HasPavement() const;
	bool IsAgitated(AgitatedType nType) const;
	bool IsAgitatorArmed() const;
	bool IsAgitatorEnteringVehicle() const;
	bool IsAgitatorInOurTerritory() const;
	bool IsAgitatorMovingAway(float fMinDot) const;
	bool IsAGunPulled() const;
	bool IsAngry(const float fThreshold) const;
	bool IsArgumentative() const;
	bool IsAvoiding() const;
	bool IsBecomingArmed() const;
	bool IsBumped() const;
	bool IsBumpedInVehicle() const;
	bool IsBumpedByVehicle() const;
	bool IsConfrontational() const;
	bool IsDodged() const;
	bool IsDodgedVehicle() const;
	bool IsFacing() const;
	bool IsFacing(float fMinDot) const;
	bool IsFearful(const float fThreshold) const;
	bool IsFlippingOff() const;
	bool IsFollowing() const;
	bool IsFriendlyTalking() const;
	bool IsGriefing() const;
	bool IsGunAimedAt() const;
	bool IsHarassed() const;
	bool IsHash(atHashWithStringNotFinal hValue) const;
	bool IsHonkedAt() const;
	bool IsHostile() const;
	bool IsHurryingAway() const;
	bool IsInState(atHashWithStringNotFinal hState) const;
	bool IsInjured() const;
	bool IsInsulted() const;
	bool IsIntervene() const;
	bool IsIntimidate() const;
	bool IsLastAgitationApplied(AgitatedType nType) const;
	bool IsLeaderUsingResponse(atHashWithStringNotFinal hResponse) const;
	bool IsLoitering() const;
	bool IsMeleeLockOn() const;
	bool IsTerritoryIntruded() const;
	bool IsOutsideClosestDistance(const float fDistance) const;
	bool IsOutsideDistance(const float fDistance) const;
	bool IsProvoked(bool bIgnoreHostility = false) const;
	bool IsRanting() const;
	bool IsRevvedAt() const;
	bool IsSirenOn() const;
	bool IsTalking() const;
	void ReportCrime();
	void ResetAction();
	void ResetClosestDistance();
	void Say(const CAgitatedSay& rSay);
	void SetAnger(float fAnger);
	void SetFear(float fFear);
	void SetHash(atHashWithStringNotFinal hValue);
	void SetMinAnger(float fMinAnger);
	void SetMinFear(float fMinFear);
	void SetMaxAnger(float fMaxAnger);
	void SetMaxFear(float fMaxFear);
	void TurnOnSiren(float fTime);
	bool WasLeaderHit() const;
	bool WasRecentlyBumpedWhenStill(float fMaxTime) const;
	bool WasUsingTerritorialScenario() const;
	void SetStartedFromScript(bool bFromScript) { m_bStartedFromScript = bFromScript; }

public:

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_AGITATED; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }

	virtual	void		CleanUp();
	virtual FSM_Return	ProcessPreFSM();
	virtual bool		ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
	CPed* GetAgitator() const { return m_pAgitator; }

	atHashWithStringNotFinal GetContext() const { return m_hContext; }
	void SetContext(atHashWithStringNotFinal hContext) { m_hContext = hContext; }
	
public:

	static CTaskAgitated*		CreateDueToEvent(const CPed* pPed, const CEvent& rEvent);
	static CTaskAgitatedAction*	FindAgitatedActionTaskForPed(const CPed& rPed);
	static CTaskAgitated*		FindAgitatedTaskForPed(const CPed& rPed);
	static CPed*				GetAgitatorForEvent(const CEvent& rEvent);
	static AgitatedType			GetTypeForEvent(const CPed& rPed, const CEvent& rEvent);
	static bool					HasContext(const CPed& rPed, atHashWithStringNotFinal hContext);
	static bool					IsAggressive(const CPed& rPed);
	static bool					ShouldIgnoreEvent(const CPed& rPed, const CEvent& rEvent);
	
public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

public:

#if __DEV
	static void InitWidgets();
#endif

private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	FSM_Return	Resume_OnUpdate();
	void		Listen_OnEnter();
	FSM_Return	Listen_OnUpdate();
	void		React_OnEnter();
	FSM_Return	React_OnUpdate();
	
private:

	bool	CanSay() const;
	void	ClearFriendlyException();
	void	ClearHostility();
	void	ClearTalkResponse();
	void	ExecuteAction(const CAgitatedAction& rAction);
	void	ExecuteActionForReaction(const CAgitatedReaction& rReaction);
	void	ExecuteActionForState();
	void	ExecuteActionForState(const CAgitatedState& rState);
	bool	HasBeenActivelyProvokedThisFrame() const;
	bool	HasNoTalkResponse() const;
	void	InitializeResponse();
	bool	IsAgitatorBeingShoved() const;
	bool	IsAgitatorTalking() const;
	bool	IsConfronting() const;
	bool	IsEnteringOrExitingScenario() const;
	bool	IsGettingUp() const;
	bool	IsInCombat() const;
	bool	IsIntimidating() const;
	bool	IsListening() const;
	bool	IsPavementRequiredForHurryAway() const;
	bool	IsPedTalking(const CPed& rPed) const;
	bool	IsShoving() const;
	bool	IsWaiting() const;
	void	LookAtAgitator(int iTime);
	void	ProcessActivelyProvoked();
	void	ProcessAgitatorFlags();
	void	ProcessAgitatorTalking();
	void	ProcessArmed();
	void	ProcessDesiredHeading();
	void	ProcessDistance();
	void	ProcessFacialIdleClip();
	void	ProcessGestures();
	void	ProcessKinematicPhysics();
	void	ProcessLeader();
	void	ProcessLookAt();
	void	ProcessOnFootClipSet();
	void	ResetAgitatedFlags();
	void	ResetFacialIdleClip();
	void	ResetOnFootClipSet();
	void	ResetReactions();
	void	Say();
	void	SetFriendlyException();
	void	SetOnFootClipSet();
	void	SetReactions();
	void	SetTalkResponse();
	bool	ShouldFinish() const;
	bool	ShouldForceAiUpdateWhenChangingTask() const;
	bool	ShouldListen() const;
	bool	ShouldProcessDesiredHeading();
	void	UpdateReactions();
	void	UpdateSay();
	void	UpdateTimersForState();

private:

	atHashWithStringNotFinal FindNameForReaction(const CAgitatedReaction& rReaction) const;
	atHashWithStringNotFinal FindNameForResponse() const;
	atHashWithStringNotFinal FindNameForState() const;
	atHashWithStringNotFinal GetResponse() const;

private:

	const CAgitatedAction*			FindFirstValidActionForReaction(const CAgitatedReaction& rReaction) const;
	const CAgitatedAction*			FindFirstValidActionForState(const CAgitatedState& rState) const;
	const CAgitatedReaction*		FindFirstValidReaction() const;
	const CAgitatedSay*				FindFirstValidSayForReaction(const CAgitatedReaction& rReaction) const;
	const CAgitatedTalkResponse*	FindFirstValidTalkResponse() const;

private:
	
	static bool	IsAngry(const CPed* pPed, const float fThreshold);
	static bool	IsArgumentative(const CPed* pPed);
	static bool	IsConfrontational(const CPed* pPed);
	static bool	IsFearful(const CPed* pPed, const float fThreshold);
	static bool	IsGunPulled(const CPed* pPed);
	static bool	IsInjured(const CPed* pPed);

private:

	static const CAgitatedContext*		GetContext(const CPed& rPed, atHashWithStringNotFinal hContext);
	static const CAgitatedPersonality*	GetPersonality(const CPed& rPed);
	
private:

	static const int sMaxReactions = 16;
	static const int sMaxHashes = 4;

private:

	atFixedArray<const CAgitatedReaction *, sMaxReactions>	m_aReactions;
	atFixedArray<atHashWithStringNotFinal, sMaxHashes>		m_aHashes;
	fwClipSetRequestHelper									m_OnFootClipSetRequestHelper;
	RegdPed													m_pAgitator;
	RegdConstPed											m_pLeader;
	const CAgitatedResponse*								m_pResponse;
	const CAgitatedState*									m_pState;
	const CAgitatedSay*										m_pSay;
	float													m_fTimeInAgitatedState;
	float													m_fTimeSinceAudioEndedForState;
	float													m_fTimeSpentWaitingInState;
	float													m_fDistance;
	float													m_fClosestDistance;
	float													m_fTimeAgitatorHasBeenTalking;
	float													m_fTimeSinceLastLookAt;
	float													m_fTimeSpentWaitingInListen;
	atHashWithStringNotFinal								m_hResponseOverride;
	atHashWithStringNotFinal								m_hContext;
	AgitatedType											m_nLastAgitationApplied;
	u32														m_uLastAgitationTime;
	u32														m_uTimeBecameHostile;
	fwFlags32												m_uAgitatedFlags;
	fwFlags16												m_uRunningFlags;
	fwFlags8												m_uConfigFlags;
	bool													m_bStartedFromScript;

#if CAPTURE_AGITATED_HISTORY
private:

	atQueue<HistoryEntry, 12> m_History;
#endif
	
private:

	static Tunables	sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CTaskAgitatedAction
////////////////////////////////////////////////////////////////////////////////

class CTaskAgitatedAction : public CTask
{

public:

	enum AgitatedActionState
	{
		State_Start = 0,
		State_Resume,
		State_Task,
		State_Finish
	};
	
	enum AgitatedActionRunningFlags
	{
		AARF_Quit	= BIT0,
	};

	explicit CTaskAgitatedAction(aiTask* pTask);
	~CTaskAgitatedAction();
	
public:

			fwFlags8& GetRunningFlags()			{ return m_uRunningFlags; }
	const	fwFlags8& GetRunningFlags() const	{ return m_uRunningFlags; }
	
public:

	virtual aiTask*	Copy() const { return rage_new CTaskAgitatedAction(m_pTask ? m_pTask->Copy() : NULL); }
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_AGITATED_ACTION; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

public:

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

public:

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Resume_OnUpdate();
	void		Task_OnEnter();
	FSM_Return	Task_OnUpdate();

private:

	int	ChooseStateToResumeTo(bool& bKeepSubTask) const;

private:

	RegdaiTask	m_pTask;
	fwFlags8	m_uRunningFlags;

};

#endif //TASK_AGITATED_H
