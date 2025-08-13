#ifndef TASK_THREAT_RESPONSE_H
#define TASK_THREAT_RESPONSE_H

// Rage headers
#include "Vector/Vector3.h"

// Game headers
#include "modelinfo/PedModelInfo.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Physics/BlendFromNmData.h"
#include "Peds/QueriableInterface.h"

// Game forward declarations
class CEvent;
class CEventFriendlyFireNearMiss;
class CEventGunShot;
class CEventGunShotBulletImpact;
class CEventGunShotWhizzedBy;
class CEntity;
class CFire;
class CPed;
class CPedTargetting;
class CSingleTargetInfo;

////////////////////
//Main combat task response - decides whether the ped should fight or run
////////////////////
class CTaskThreatResponse : public CTask
{
public:

	enum ConfigFlags
	{
		CF_NeverFight						= BIT0,
		CF_NeverFlee						= BIT1,
		CF_PreferFight						= BIT2,
		CF_PreferFlee						= BIT3,
		CF_CanFightArmedPedsWhenNotArmed	= BIT4,
		CF_ThreatIsIndirect					= BIT5,
		CF_DisableReactBeforeFlee			= BIT6,
		CF_SetFriendlyException				= BIT7,
		CF_FleeInVehicleAfterJack			= BIT8,
		CF_ForceFleeIfNotArmed				= BIT9,
	};

	enum ResponseState
	{
		State_Invalid = -1,
		State_Analysis = 0,
		State_Resume,
		State_Combat,
		State_Flee,
		State_ReactAndFlee,
		State_Finish
	};
	
	enum ThreatResponse
	{
		TR_None = -1,
		TR_Fight,
		TR_Flee,
		TR_Max,
	};
	
private:

	enum PedArmament
	{
		PA_Unknown = -1,
		PA_Unarmed = 0,
		PA_Melee,
		PA_Armed,
		PA_Max,
	};

	enum PedArmamentSelector
	{
		PAS_Equipped,
		PAS_Best,
	};
	
	enum RunningFlags
	{
		RF_HasMadeInitialDecision		= BIT0,
		RF_FleeFromCombat				= BIT1,
		RF_HasBeenDirectlyThreatened	= BIT2,
		RF_MakeNewDecision				= BIT3,
		RF_HasWantedLevelBeenSet		= BIT4,
	};
	
private:

	struct DecisionCache
	{
		struct Ped
		{
			Ped()
			: m_bIsCarryingGun(false)
			, m_bAlwaysFlee(false)
			{}

			bool m_bIsCarryingGun;
			bool m_bAlwaysFlee;
		};

		struct Target
		{
			Target()
			: m_nArmament(PA_Unknown)
			, m_bAllRandomsFlee(false)
			, m_bAllNeutralRandomsFlee(false)
			, m_bAllRandomsOnlyAttackWithGuns(false)
			{}

			PedArmament	m_nArmament;
			bool		m_bAllRandomsFlee;
			bool		m_bAllNeutralRandomsFlee;
			bool		m_bAllRandomsOnlyAttackWithGuns;
		};

		DecisionCache()
		{}

		Ped		m_Ped;
		Target	m_Target;
	};
	
public:

	// Constructor/destructor
	CTaskThreatResponse(const CPed* pTarget, float fCombatTime = -1.0f);
	~CTaskThreatResponse();
	
public:

			fwFlags16&	GetConfigFlags()							{ return m_uConfigFlags; }
	const	fwFlags16&	GetConfigFlags()					const	{ return m_uConfigFlags; }
			fwFlags32&	GetConfigFlagsForCombat()					{ return m_uConfigFlagsForCombat; }
	const	fwFlags32&	GetConfigFlagsForCombat()			const	{ return m_uConfigFlagsForCombat; }
			fwFlags16&	GetConfigFlagsForFlee()						{ return m_uConfigFlagsForFlee; }
	const	fwFlags16&	GetConfigFlagsForFlee()				const	{ return m_uConfigFlagsForFlee; }
			fwFlags8&	GetConfigFlagsForVehicleFlee()				{ return m_uConfigFlagsForVehicleFlee; }
	const	fwFlags8&	GetConfigFlagsForVehicleFlee()		const	{ return m_uConfigFlagsForVehicleFlee; }
			fwFlags8&	GetConfigFlagsForVehiclePursuit()			{ return m_uConfigFlagsForVehiclePursuit; }
	const	fwFlags8&	GetConfigFlagsForVehiclePursuit()	const	{ return m_uConfigFlagsForVehiclePursuit; }

public:
	
	void SetConfigFlags(fwFlags16 uFlags)						{ m_uConfigFlags = uFlags; }
	void SetConfigFlagsForCombat(fwFlags32 uFlags)				{ m_uConfigFlagsForCombat = uFlags; }
	void SetConfigFlagsForFlee(fwFlags16 uFlags)				{ m_uConfigFlagsForFlee = uFlags; }
	void SetConfigFlagsForVehicleFlee(fwFlags8 uFlags)			{ m_uConfigFlagsForVehicleFlee = uFlags; }
	void SetConfigFlagsForVehiclePursuit(fwFlags8 uFlags)		{ m_uConfigFlagsForVehiclePursuit = uFlags; }
	void SetFleeFlags(const int nFlag)							{ Assign(m_FleeFlags, nFlag); }
	void SetThreatResponseOverride(ThreatResponse nOverride)	{ m_nThreatResponseOverride = nOverride; }
	void SetFleeFromCombat(bool bFleeFromCombat);

public:

	bool IsFleeFromCombatFlagSet() const { return m_uRunningFlags.IsFlagSet(RF_FleeFromCombat); }
	bool IsFleeing() const;
	void FleeInVehicleAfterJack();
	void OnFriendlyFireNearMiss(const CEventFriendlyFireNearMiss& rEvent);
	void OnGunShot(const CEventGunShot& rEvent);
	void OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent);
	void OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);
	void OnGunAimedAt(const CEventGunAimedAt& rEvent);
	void SetMoveBlendRatioForFlee(float fMoveBlendRatio);

	void SetIsResponseToEventType(eEventType eIsResponseToEventType) { m_eIsResponseToEventType = eIsResponseToEventType; }
	
public:

	ThreatResponse DetermineThreatResponseState(const CPed& rPed) const;
	ThreatResponse GetThreatResponse() const;

public:
	
	static bool				CanFight(const CPed& rPed, const CPed& rTarget);
	static ThreatResponse	PreDetermineThreatResponseStateFromEvent(CPed& rPed, const CEvent& rEvent);
	static ThreatResponse	DetermineThreatResponseState(const CPed& rPed, const CPed* pTarget, ThreatResponse override = TR_None, fwFlags16 uConfigFlags = 0, fwFlags8 uRunningFlags = 0);

	static bool				ShouldPedBroadcastRespondedToThreatEvent(const CPed& rPed);
	static void				BroadcastPedRespondedToThreatEvent(CPed& rPed, const CPed* pTarget);
	
	static const CPedModelInfo::PersonalityThreatResponse* GetThreatResponse(const CPed& rPed, const CPed* pTarget);

private:

	int ChooseStateToResumeTo(bool& bKeepSubTask) const;

private:

	static bool CanFightArmedPedsWhenNotArmed(const CPed& rPed, fwFlags16 uConfigFlags);

	// Static helper functions for determining if a ped should fight or flee.
	static ThreatResponse		ChooseThreatResponseFromAttributes(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags);
	static ThreatResponse		ChooseThreatResponseFromCombatBehavior(const CPed& rPed);
	static ThreatResponse		ChooseThreatResponseFromFallbacks(const CPed& rPed, fwFlags16 uConfigFlags);
	static ThreatResponse		ChooseThreatResponseFromPersonality(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags);
	static ThreatResponse		ChooseThreatResponseFromRules(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags);
	
	static bool					ShouldAlwaysFightDueToAttributes(const CPed& rPed);
	static bool					ShouldAlwaysFightDueToRules(const CPed& rPed);
	static bool					ShouldAlwaysFleeDueToAttributes(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags);
	static bool					ShouldAlwaysFleeDueToRules(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags);
	static bool					ShouldAlwaysThreatenDueToAttributes();
	static bool					ShouldAlwaysThreatenDueToRules();
	static bool					ShouldFleeFromIndirectThreats(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags);
	static bool					ShouldFleeFromWantedTarget(const CPed& rPed, fwFlags8 uRunningFlags);
	static bool					ShouldFleeDueToVehicle(const CPed& rPed);
	static bool					ShouldFleeFromPedUnlessArmed(const CPed& rPed);
	
	static bool					ShouldNeverFight(fwFlags16 uConfigFlags);
	static bool					ShouldNeverFlee(const CPed& rPed, fwFlags16 uConfigFlags);
	
	static bool					IsArmed(const CPed& rPed);
	static bool					IsArmedWithGun(const CPed& rPed);
	static bool					IsCarryingGun(const CPed& rPed);
	static float				GetFightModifier(const CPed& rPed);
	static float				GetTargetFleeModifier(const CPed* pTarget);
	static PedArmament			GetArmamentForPed(const CPed& rPed, PedArmamentSelector nPedArmamentType = PAS_Equipped);
	static PedArmament			GetTargetArmament(const CPed* pTarget);
	
public:

	static CTaskThreatResponse* CreateForTargetDueToEvent(const CPed* pPed, const CPed* pTarget, const CEvent& rEvent);
	static float				GetMoveBlendRatioForFleeForEvent(const CEvent& rEvent);

	virtual const CEntity* GetTargetPositionEntity() const
	{ 
		if(GetSubTask() && !m_pTarget)
		{
			return GetSubTask()->GetTargetPositionEntity();
		}

		return m_pTarget;
	}

public:

	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_THREAT_RESPONSE;}
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }

	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// PURPOSE: If the task was created to respond to an event, returns the event type. If not, it returns EVENT_INVALID
	virtual eEventType GetIsResponseForEventType() const { return m_eIsResponseToEventType; }

	// FSM implementations
	virtual FSM_Return ProcessPreFSM();
	FSM_Return		UpdateFSM (const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();

	// State function implementations
	//Analysis of how to respond, previously start state
	void		Analysis_OnEnter();
	FSM_Return	Analysis_OnUpdate(CPed* pPed);
	// Resume
	FSM_Return	Resume_OnUpdate();
	// Combat state
	void		Combat_OnEnter	(CPed* pPed);
	FSM_Return	Combat_OnUpdate	(CPed* pPed);
	void		Combat_OnExit();
	// Flee state
	void		Flee_OnEnter	(CPed* pPed);
	FSM_Return	Flee_OnUpdate	(CPed* pPed);
	void		Flee_OnExit();
	// React and Flee state
	void		ReactAndFlee_OnEnter	(CPed* pPed);
	FSM_Return	ReactAndFlee_OnUpdate	(CPed* pPed);
	void		ReactAndFlee_OnExit		();
	// Finish state
	FSM_Return	Finish_OnUpdate(CPed* pPed);

#if !__FINAL || __FINAL_LOGGING
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	virtual bool UseCustomGetup() { return true; }
	virtual bool AddGetUpSets ( CNmBlendOutSetList& sets, CPed* pPed );
	virtual void GetUpComplete ( eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed );
	virtual void RequestGetupAssets (CPed* pPed);
	virtual bool IsConsideredInCombat();
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	virtual void CleanUp();

	virtual CTaskInfo* CreateQueriableState() const;

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

private:

	// Internal helper functions
	s32				GetThreatResponseState	( CPed* pPed );
	bool			CheckForResponseStateChange( CPed* pPed );
	
	void			BuildDecisionCache(DecisionCache& rCache, CPed* pPed) const;
	bool			CanFightArmedPedsWhenNotArmed() const;
	ResponseState	ChooseFleeState(CPed* pPed) const;
	bool			ChooseReactAndFleeClipSets(fwMvClipSetId& reactClipSetId, fwMvClipSetId& reactToFleeClipSetId) const;
	void			ClearFriendlyException();
	void			GenerateFleeTarget(CAITarget& rTarget) const;
	bool			IsThreatResponseValid(CPed* pPed) const;
	bool			MustTargetBeValid() const;
	void			OnDirectlyThreatened();
	void			OnPerceivedGun();
	void			SetFriendlyException();
	bool			ShouldFleeFromCombat() const;
	bool			ShouldReturnToCombatFromFlee() const;
	bool			ShouldMakeNewDecisionDueToCache(CPed* pPed) const;
	bool			CanSetTargetWantedLevel(const CWanted* pTargetWanted, const CPed* pPed, const CPed* pTargetPed, bool bCheckLosStatus) const;
	void			SetTargetWantedLevelDueToEvent(const CEvent& rEvent);
	void			SetTargetWantedLevel(CPed* pPed);
	void			SetWantedLevelForPed(CPed* pPed, const CEvent& rEvent);
	void			PossiblyIncreaseTargetWantedLevel(const CEvent &rEvent);
	void			UpdateTargetWantedLevel(CPed* pPed);
	bool			CheckIfEstablishedDecoy();

private:

	static bool IsEventAnIndirectThreat(const CPed* pPed, const CEvent& rEvent);
	static bool ShouldDisableReactBeforeFlee(const CPed* pPed, const CPed* pTarget);
	static bool ShouldFleeInVehicleAfterJack(const CPed* pPed, const CEvent& rEvent);
	static bool ShouldPreferFight(const CPed* pPed, const CEvent& rEvent);
	static bool ShouldSetFriendlyException(const CPed* pPed, const CPed* pTarget, const CEvent& rEvent);
	static bool ShouldSetForceFleeIfUnarmed(const CPed* pPed, const CEvent& rEvent);

private:

#if !__NO_OUTPUT
	static const char* ToString(ThreatResponse nThreatResponse);
#endif

private:

	DecisionCache	m_DecisionCache;
	RegdConstPed	m_pTarget;
	ResponseState	m_threatResponse;
	CSingleTargetInfo* m_pTargetInfo;
	CPedTargetting* m_pPedTargetting;

	bool			m_bGoDirectlyToFlee : 1;
	bool			m_bSuppressSlowingForCorners : 1;
	RegdTask		m_pInitialMoveTask;

	CGetupSetRequestHelper m_FleeGetupReqHelper;

	static const u32 ms_iMaxCombatGetupHelpers = 9;

	CGetupSetRequestHelper m_CombatGetupReqHelpers[ms_iMaxCombatGetupHelpers];
	
	float			m_fCombatTime;
	float			m_fMoveBlendRatioForFlee;

	fwMvClipSetId	m_reactClipSetId;
	fwMvClipSetId	m_reactToFleeClipSetId;
	
	ThreatResponse	m_nThreatResponseOverride;
	fwFlags16		m_uConfigFlags;
	fwFlags32		m_uConfigFlagsForCombat;
	fwFlags8		m_uConfigFlagsForVehiclePursuit;
	fwFlags16		m_uConfigFlagsForFlee;
	fwFlags8		m_uConfigFlagsForVehicleFlee;
	fwFlags8		m_uRunningFlags;
	// These are flags, driven from custom event responses, that control what we are allowed to do when flee task is created
	// See eEventResponseFleeFlags
	u8		m_FleeFlags;

	// PURPOSE: Store the event this task was use to respond to
	eEventType      m_eIsResponseToEventType;

	u32 m_uTimerToFlee;
};

class CClonedThreatResponseInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedThreatResponseInfo()
		: m_target(NULL)
		, m_uIsResponseToEventType(EVENT_NONE)
	{
	}

	CClonedThreatResponseInfo(const CPed* pTarget, eEventType eIsResponseToEventType)
		: m_target(const_cast<CPed*>(pTarget))
		, m_uIsResponseToEventType(static_cast<u8>(eIsResponseToEventType))
	{
		taskAssert(static_cast<s32>(eIsResponseToEventType) >= 0 && static_cast<s32>(eIsResponseToEventType) <= 255);
	}

	virtual CTask* CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
	{
		CTaskThreatResponse* pTaskThreatResponse = rage_new CTaskThreatResponse(m_target.GetPed());
		pTaskThreatResponse->SetIsResponseToEventType(GetIsResponseToEventType());

		return pTaskThreatResponse;
	}

	virtual s32 GetTaskInfoType() const		{ return INFO_TYPE_THREAT_RESPONSE; }
	virtual int GetScriptedTaskType() const	{ return SCRIPT_TASK_COMBAT; }
	
	virtual const CEntity*	GetTarget()	const			{ return (const CEntity*)m_target.GetPed(); }
	virtual CEntity*		GetTarget()					{ return (CEntity*)m_target.GetPed(); }

	eEventType GetIsResponseToEventType() const { return static_cast<eEventType>(m_uIsResponseToEventType); }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		// sync the target id
		ObjectId targetID = m_target.GetPedID();
		SERIALISE_OBJECTID(serialiser, targetID, "Target");
		m_target.SetPedID(targetID);

		SERIALISE_UNSIGNED(serialiser, m_uIsResponseToEventType, 8, "IsResponseToEventType")
	}

private:

	CClonedThreatResponseInfo(const CClonedThreatResponseInfo&);
	CClonedThreatResponseInfo &operator=(const CClonedThreatResponseInfo&);

	CSyncedPed m_target;
	u8		   m_uIsResponseToEventType;
};

CompileTimeAssert(eEVENTTYPE_MAX <= 255); // NOTE[egarcia]: We are using a byte in CClonedThreatResponseInfo to sync m_uIsResponseToEventType

////////////////////
//Hands up
////////////////////
class CTaskHandsUp : public CTaskFSMClone
{
public:

	enum ConfigFlags
	{
		Flag_StraightToLoop = BIT(0)
	};

public:
	CTaskHandsUp(const s32 iDuration,
				 CPed* pPedToFace = NULL,
				 const s32 iFacePedDuration = -1,
				 const u32 flags = 0);
	virtual ~CTaskHandsUp() {}

	virtual aiTask* Copy() const {return rage_new CTaskHandsUp(m_iDuration, m_pPedToFace, m_iFacePedDuration, m_flags);}
	virtual bool	IsInterruptable(const CPed*) const {return false;}
	virtual int		GetTaskTypeInternal() const {return (int)CTaskTypes::TASK_HANDS_UP;}
	virtual s32		GetDefaultStateAfterAbort()	const { return State_Finish; }

	// network methods
	virtual CTaskInfo*	CreateQueriableState() const;
	virtual void        ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return  UpdateClonedFSM (const s32 /*iState*/, const FSM_Event /*iEvent*/);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual	void CleanUp();

	void SetDuration(s32 duration) { m_iDuration = duration; }
private:

	enum
	{
		State_Start = 0,
		State_RaiseHands,
		State_HandsUpLoop,
		State_LowerHands,
		State_Finish,
	};

	virtual FSM_Return	UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	
	FSM_Return			Start_OnUpdate();
	FSM_Return			RaiseHands_OnEnter();
	FSM_Return			RaiseHands_OnUpdate();
	FSM_Return			HandsUpLoop_OnEnter();
	FSM_Return			HandsUpLoop_OnUpdate();
	FSM_Return			LowerHands_OnEnter();
	FSM_Return			LowerHands_OnUpdate();

private:

	s32 m_iDuration;
	s32 m_iDictIndex;

	RegdPed	m_pPedToFace;
	s32 m_iFacePedDuration;

	ConfigFlags m_flags;

	bool	m_bNetworkMigratingInHandsUpLoop;
	float   m_fNetworkMigrateHandsUpLoopPhase;

public:

	// debug:
#if !__FINAL || __FINAL_LOGGING
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	

};


class CClonedHandsUpInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedHandsUpInfo()
		: m_duration(0)
		, m_pPedToFace(NULL)
		, m_iFacePedDuration(0)
		, m_straightToLoop(false)
	{}

	CClonedHandsUpInfo(s32 duration, CPed* pPedToFace = NULL, s16 iFacePedDuration = -1, bool straightToLoop = 0)
		: m_duration(duration)
		, m_pPedToFace(pPedToFace)
		, m_iFacePedDuration(iFacePedDuration)
		, m_straightToLoop(straightToLoop)
	{

	}

	virtual CTask* CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
	{
		return rage_new CTaskHandsUp(m_duration, GetWatchedPed(), GetTimeToWatchPed(), GetFlags());
	}
	virtual CTaskFSMClone *CreateCloneFSMTask() ;

	virtual s32 GetTaskInfoType() const		{ return INFO_TYPE_HANDS_UP; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_HANDS_UP; }

	virtual s32	GetDuration()	const		{ return m_duration; }
	virtual CPed*	GetWatchedPed()	const	{ return m_pPedToFace.GetPed(); }
	virtual s16	GetTimeToWatchPed()	const	{ return m_iFacePedDuration; }
	virtual u32 GetFlags() const			{ return (u32)m_straightToLoop;	}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_INTEGER(serialiser, m_duration, SIZEOF_DURATION, "Duration");

		ObjectId targetID = m_pPedToFace.GetPedID();
		SERIALISE_OBJECTID(serialiser, targetID, "PedToFaceID");
		m_pPedToFace.SetPedID(targetID);

		SERIALISE_INTEGER(serialiser, m_iFacePedDuration, SIZEOF_FACEPEDDURATION, "FacePedDuration");
		bool straightToLoop = m_straightToLoop;
		SERIALISE_BOOL(serialiser, straightToLoop, "Flag_StraightToLoop");
		m_straightToLoop = straightToLoop;
	}

private:

	CClonedHandsUpInfo(const CClonedHandsUpInfo&);
	CClonedHandsUpInfo &operator=(const CClonedHandsUpInfo&);

	static const unsigned SIZEOF_DURATION = 20;
	static const unsigned SIZEOF_FACEPEDDURATION = 16;

	s32 m_duration;
	CSyncedPed m_pPedToFace;
	s16 m_iFacePedDuration;
	bool m_straightToLoop:1;
};

/////////////////////////
//React to having a gun aimed at.
/////////////////////////

class CTaskReactToGunAimedAt : public CTask
{
public:

	// Constructor
	CTaskReactToGunAimedAt(CPed* pAggressorPed);
	
	// Destructor
	~CTaskReactToGunAimedAt();

	// Task required functions
	virtual aiTask* Copy() const { return rage_new CTaskReactToGunAimedAt(m_pAggressorPed); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT; }

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM();

	// debug:
#if !__FINAL || __FINAL_LOGGING
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL	

private:

	enum 
	{
		State_Start = 0,
		State_Pause,
		State_ExitVehicle,
		State_HandsUp,
		State_Finish
	};

	FSM_Return		Start_OnUpdate();

	void			Pause_OnEnter();
	FSM_Return		Pause_OnUpdate();

	void			ExitVehicle_OnEnter();
	FSM_Return		ExitVehicle_OnUpdate();

	void			HandsUp_OnEnter();
	FSM_Return		HandsUp_OnUpdate();

private:

	RegdPed m_pAggressorPed;
	s32 m_iHandsUpTime;
};

#endif
