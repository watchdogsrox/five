// FILE :    TaskCombat.h
// PURPOSE : Combat tasks to replace killped* tasks, greater dependancy on decision makers
//			 So that designers can tweak combat behaviour and proficiency based on character type
// AUTHOR :  Phil H
// CREATED : 18-08-2005


#ifndef TASK_NEW_COMBAT_H
#define TASK_NEW_COMBAT_H

// Framework headers
#include "ai/AITarget.h"
#include "ai/ExpensiveProcess.h"
#include "ai/task/taskstatetransition.h"

// Game headers
#include "Peds/CombatBehaviour.h"
#include "Peds/DefensiveArea.h"
#include "Peds/QueriableInterface.h"
#include "Task/Combat/CombatOrders.h"
#include "task/Combat/TacticalAnalysis.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/Tuning.h"

// Forward Declarations
class CCoverPoint;
class CEntity;
class CEvent;
class CEventCallForCover;
class CEventExplosion;
class CEventGunAimedAt;
class CEventGunShot;
class CEventGunShotBulletImpact;
class CEventGunShotWhizzedBy;
class CEventPotentialBlast;
class CEventProvidingCover;
class CEventShoutBlockingLos;
class CVehMission;
class CTaskSet;
class CWeightedTaskSet;

typedef enum
{
	CF_StateEnded					= BIT0,
	CF_LosBlocked					= BIT1,
	CF_LosNotBlockedByFriendly		= BIT2,
	CF_LosClear						= BIT3,
	CF_WillAdvance					= BIT4,
	CF_WillUseCover					= BIT5,
	CF_TargetLocationLost			= BIT6,
	CF_TargetLocationKnown			= BIT7,
	CF_InCover						= BIT8, // At a coverpoint that provides cover from the target
	CF_CanReactToBuddyShot			= BIT9,
	CF_NotInCover					= BIT10, // No coverpoint associated
	CF_OutsideAttackWindow			= BIT11,
	CF_InsideAttackWindow			= BIT12,
	CF_OutsideDefensiveArea			= BIT13,
	CF_InVehicle					= BIT14,
	CF_OnFoot						= BIT15,
	CF_CanArrestTarget				= BIT16,
	CF_IsSafeToLeaveCover			= BIT17, // It is safe to leave our current cover spot (generally used to check if we're pinned)
	CF_CanJackTarget				= BIT18,	// Target is in a vehicle and we're close enough to jack
	CF_CanChaseInVehicle			= BIT19,
	CF_CanChaseOnFoot				= BIT20,
	CF_DisallowChaseOnFoot			= BIT21,
	CF_CanDragInjuredPedToSafety	= BIT22,
	CF_Mounted						= BIT23,	// Mounted on a horse or similar animal.
	CF_CanFlank						= BIT24,
	CF_HasDesiredCover				= BIT25,
	CF_TargetWithinDefensiveArea	= BIT26,
	CF_WillUseFrustratedAdvance		= BIT27,
	CF_CanReactToImminentExplosion	= BIT28,
	CF_CanReactToExplosion			= BIT29,
	CF_CanPerformMeleeAttack		= BIT30,
} ConditionFlags;

// Additional transition flags that can't fit into the enum
#define CF_CanMoveWithinDefensiveArea	(s64)0x080000000LL
#define CF_WillUseDefensiveAttackWindow (s64)0x100000000LL
#define CF_CanMoveToTacticalPoint		(s64)0x200000000LL

// Armed combat transitions
class COnFootArmedTransitions : public aiTaskStateTransitionTable
{
public:
	COnFootArmedTransitions();

	virtual void Init();
};

// Armed combat transitions where the BF_JustSeekCover has been set
class COnFootArmedCoverOnlyTransitions : public aiTaskStateTransitionTable
{
public:
	COnFootArmedCoverOnlyTransitions();

	virtual void Init();
};

// Unarmed combat transitions
class COnFootUnarmedTransitions : public aiTaskStateTransitionTable
{
public:
	COnFootUnarmedTransitions();

	virtual void Init();
};

// Underwater armed combat transitions
class CUnderwaterArmedTransitions : public aiTaskStateTransitionTable
{
public:
	CUnderwaterArmedTransitions();

	virtual void Init();
};

//-------------------------------------------------------------------------
// Main combat task, uses the decision maker to decide which
// task to undertake
//-------------------------------------------------------------------------
class CTaskCombat : public CTaskFSMClone
{
public:

	static void InitClass();
	static void InitTransitionTables();
	static void UpdateClass();

	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_Resume,
		State_Fire,
		State_InCover,
		State_MoveToCover,
		State_Retreat,
		State_Advance,
		State_AdvanceFrustrated,
		State_ChaseOnFoot,
		State_ChargeTarget,
		State_Flank,
		State_MeleeAttack,
		State_MoveWithinAttackWindow,
		State_MoveWithinDefensiveArea,
		State_PersueInVehicle,
		State_WaitingForEntryPointToBeClear,
		State_GoToEntryPoint,
		State_PullTargetFromVehicle,
		State_EnterVehicle,
		State_Search,
		State_DecideTargetLossResponse,
		State_DragInjuredToSafety,
		State_Mounted,
		State_MeleeToArmed,
		State_ReactToImminentExplosion,
		State_ReactToExplosion,
		State_ReactToBuddyShot,
		State_GetOutOfWater,
		State_WaitingForCoverExit,
		State_AimIntro,
		State_TargetUnreachable,
		State_ArrestTarget,
		State_MoveToTacticalPoint,
		State_ThrowSmokeGrenade,
		State_Victory,
		State_ReturnToInitialPosition,
		State_Finish
	} CombatState;

	enum CombatFlags
	{
		ComF_ForceFire								= BIT0,
		ComF_WeaponAlreadyEquipped					= BIT1,
		ComF_DrawWeaponWhenLosing					= BIT2,
		ComF_TargetLost								= BIT3,
		ComF_IsInDefensiveArea						= BIT4,
		ComF_TargetChangedThisFrame					= BIT5,
		ComF_WasCrouched							= BIT6,
		ComF_UsingSecondaryTarget					= BIT7,
		ComF_ExplosionValid							= BIT8,
		ComF_RequiresOrder							= BIT9,
		ComF_FiringAtTarget							= BIT10,
		ComF_WasPlayingAimPoseTransitionLastFrame	= BIT11,
		ComF_IsPlayingAimPoseTransition				= BIT12,
		ComF_HasStarted								= BIT13,
		ComF_UseFlinchAimIntro						= BIT14,
		ComF_UseSurprisedAimIntro					= BIT15,
		ComF_ArrestTarget							= BIT16,
		ComF_DoingEarlyVehicleEntry					= BIT17,
		ComF_ExitCombatRequested					= BIT18,
		ComF_IsUsingFranticRuns						= BIT19,
		ComF_UseFrustratedAdvance					= BIT20,
		ComF_MeleeAnimPhaseSync						= BIT21,
		ComF_MeleeImmediateThreat					= BIT22,
		ComF_IsWaitingAtRoadBlock					= BIT23,
		ComF_TaskAbortedForStaticMovement			= BIT24,
		ComF_MoveToCoverStopped						= BIT25,
		ComF_PreventChangingTarget					= BIT26,
		ComF_DisableAimIntro						= BIT27,
		ComF_TemporarilyForceFiringState			= BIT28,
		ComF_UseSniperAimIntro						= BIT29,
		ComF_QuitIfDriverJacked						= BIT30,
	};
	
	// Used to configure things when a ped in cover changes from clone to owner or vice versa to stop popping
	enum MigrationFlags
	{
		MigrateF_PreventPeriodicStateChangeScan		= BIT1, // After migrating, the new owner needs a frame run through to set things up corretly...
	};

	enum GestureType
	{
		GT_None,
		GT_Beckon,
		GT_OverThere,
		GT_Halt,
	};

private:

	enum ReasonToHoldFire
	{
		RTHF_None,
		RTHF_TargetInjured,
		RTHF_TargetArrested,
		RTHF_MP,
		RTHF_LimitCombatants,
		RTHF_LackOfHostility,
		RTHF_UnarmedTarget,
		RTHF_ApproachingTargetVehicle,
		RTHF_ForcedHoldFireTimer,
		RTHF_InitialTaskDelay
	};

	enum RunningFlags
	{
		RF_DontForceArrest			= BIT0,
		RF_UsingStationaryCombat	= BIT1,
		RF_HasPlayedRoadBlockAudio	= BIT2,
	};

private:

	struct PotentialBlast
	{
		PotentialBlast()
		{ Reset(); }

		void GetTarget(const CPed& rPed, CAITarget& rTarget) const
		{
			if(m_pEntity)
			{
				rTarget.SetEntity(m_pEntity);
			}
			else
			{
				rTarget.SetPosition(Vector3(
					m_fPositionX,
					m_fPositionY,
					rPed.GetTransform().GetPosition().GetZf()));
			}
		}

		bool IsValid() const
		{
			return ((m_pEntity != NULL) || (m_fPositionX != FLT_MAX) || (m_fPositionY != FLT_MAX));
		}

		void Reset()
		{
			m_pEntity			= NULL;
			m_fPositionX		= FLT_MAX;
			m_fPositionY		= FLT_MAX;
			m_fRadius			= -1.0f;
			m_uTimeOfExplosion	= 0;
		}

		void Set(const CAITarget& rTarget, float fRadius, u32 uTimeOfExplosion)
		{
			m_pEntity = rTarget.GetEntity();

			Vector3 vTargetPosition;
			rTarget.GetPosition(vTargetPosition);
			m_fPositionX = vTargetPosition.x;
			m_fPositionY = vTargetPosition.y;

			m_fRadius = fRadius;

			m_uTimeOfExplosion = uTimeOfExplosion;
		}

		RegdConstEnt	m_pEntity;
		float			m_fPositionX;
		float			m_fPositionY;
		float			m_fRadius;
		u32				m_uTimeOfExplosion;
	};

public:

	// Constructor/destructor
	CTaskCombat( const CPed* pPrimaryTarget, float fTimeToQuit = 0.0f, const fwFlags32 uFlags = 0);
	~CTaskCombat();

public:

			fwFlags8&	GetConfigFlagsForVehiclePursuit()		{ return m_uConfigFlagsForVehiclePursuit; }
	const	fwFlags8&	GetConfigFlagsForVehiclePursuit() const	{ return m_uConfigFlagsForVehiclePursuit; }

public:

	void SetConfigFlagsForVehiclePursuit(fwFlags8 uConfigFlags) { m_uConfigFlagsForVehiclePursuit = uConfigFlags; }

public:

	bool IsHoldingFire() const
	{
		return (m_nReasonToHoldFire != RTHF_None);
	}

	bool IsHoldingFireDueToLackOfHostility() const
	{
		return (m_nReasonToHoldFire == RTHF_LackOfHostility);
	}

public:

	void SetTaskCombatState(const s32 iState);

	virtual aiTask* Copy() const;
	virtual s32		GetTaskTypeInternal() const {return CTaskTypes::TASK_COMBAT;}
	virtual s32		GetDefaultStateAfterAbort()	const { return State_Resume; }
	virtual void	DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// FSM implementations
	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void	CleanUp				();
	FSM_Return		ProcessPreFSM		();
	FSM_Return		ProcessPreFSM_Clone	();

	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	FSM_Return		ProcessPostFSM		();

	// Sends communication events to friendly peds, generates audio
	void UpdateCommunication( CPed* pPed );
	// Restarts the current state or updates the gun tasks target when the target has changed (based on the flag)
	void UpdateGunTarget(bool bRestartCurrentState);
	// Updates the position we want to move away from for varied aim poses
	void UpdateVariedAimPoseMoveAwayFromPosition(CPed* pPed);
	
	// Checks if cover finder is satisfied and desired cover point conditions are met
	bool HasValidatedDesiredCover() const;
	bool IsPlayingAimPoseTransition() const;
	bool JustStartedPlayingAimPoseTransition() const;

	// For the in cover subtask to report request processed
	void ReportSmokeGrenadeThrown();

	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	const CPed* GetPrimaryTarget() const { return m_pPrimaryTarget; }

	// Returns the smallest radius of the defensive area
	static float GetClosestDefensiveAreaPointToNavigateTo( CPed* pPed, Vector3& vPoint, CDefensiveArea* pDefensiveAreaToUse = NULL );

	static void ChooseMovementAttributes(CPed* pPed, bool& bShouldStrafe, float& fMoveBlendRatio);
	static s32 GenerateCombatRunningFlags(bool bShouldStrafe, float fDesiredMbr);

	static bool ShouldReactBeforeAimIntroForEvent(const CPed* pPed, const CEvent& rEvent, bool& bUseFlinch, bool& bUseSurprised, bool& bUseSniper);
	static bool ShouldArrestTargetForEvent(const CPed* pPed, const CPed* pTargetPed, const CEvent& rEvent);
	static bool IsAnImmediateMeleeThreat(const CPed* pPed, const CPed* pTargetPed, const CEvent& rEvent);
	static bool ShouldMaintainMinDistanceToTarget(const CPed& rPed);
	static bool IsVehicleMovingSlowerThanSpeed(const CVehicle& rVehicle, float fSpeed);

	static void ResetTimeLastDragWasActive() { ms_uTimeLastDragWasActive = 0; }
	static void ResetTimeLastAsyncProbeSubmitted() { ms_iTimeLastAsyncProbeSubmitted = 0; }
	
	void OnCallForCover(const CEventCallForCover& rEvent);
	void OnExplosion(const CEventExplosion& rEvent);
	void OnGunAimedAt(const CEventGunAimedAt& rEvent);
	void OnGunShot(const CEventGunShot& rEvent);
	void OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent);
	void OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent);
	void OnPotentialBlast(const CEventPotentialBlast& rEvent);
	void OnProvidingCover(const CEventProvidingCover& rEvent);
	void OnShoutBlockingLos(const CEventShoutBlockingLos& rEvent);

	void SetPreventChangingTarget(bool bTrue)
	{
		if (bTrue)
		{
			m_uFlags.SetFlag(ComF_PreventChangingTarget);
		}
		else
		{
			m_uFlags.ClearFlag(ComF_PreventChangingTarget);
		}

#if __BANK
		if (GetEntity())
		{
			CPed* pPed = GetPed();
			if (pPed && !pPed->IsPlayer())
			{
				AI_LOG_WITH_ARGS("CTaskCombat::SetPreventChangingTarget() - ComF_PreventChangingTarget = %s, ped = %s, task = %p \n", m_uFlags.IsFlagSet(ComF_PreventChangingTarget) ? "TRUE" : "FALSE", pPed->GetLogName(), this);
			}
		}
#endif
	}

	void SetRequiresOrder(bool bRequiresOrder)
	{
		if(bRequiresOrder)
		{
			m_uFlags.SetFlag(ComF_RequiresOrder);
		}
		else
		{
			m_uFlags.ClearFlag(ComF_RequiresOrder);
		}
	}

	bool GetIsPlayingAmbientClip() const;

	bool GetIsPreventChangingTargetSet() const {return m_uFlags.IsFlagSet(ComF_PreventChangingTarget);}

	// Look at the event and decide who a recipient should fight.
	static const CPed* GetCombatTargetFromEvent(const CEvent& rEvent);

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

#if __BANK
	void LogTargetInformation();
#endif
	//-------------------------------------------------------------------------
	// Checks the cover point to see if it is considered valid
	//-------------------------------------------------------------------------
	
	// Returns a pointer to the transition table data, overriden by tasks that use transitional sets
	virtual aiTaskTransitionTableData* GetTransitionTableData() { return &m_transitionTableData; }

	//-------------------------------------------------------------------------
	// Cloned Task
	//-------------------------------------------------------------------------

	virtual CTaskInfo*	CreateQueriableState() const;
	virtual void        ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return  UpdateClonedFSM (const s32 /*iState*/, const FSM_Event /*iEvent*/);
	virtual void		UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual void        OnCloneTaskNoLongerRunningOnOwner();
    virtual void        LeaveScope(const CPed* pPed);

	// Fn for building a list of influence spheres
	static bool SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed );
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed));
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	
	CCombatOrders& GetCombatOrders() { return m_CombatOrders; }
	const CCombatOrders& GetCombatOrders() const { return m_CombatOrders; }
	
	fwFlags32 GetFlags() const { return m_uFlags; }

	struct Tunables : public CTuning
	{
		struct BuddyShot
		{
			BuddyShot()
			{}

			bool	m_Enabled;
			float	m_MinTimeBeforeReact;
			float	m_MaxTimeBeforeReact;
			float	m_MaxTimeSinceShot;
			float	m_MaxDistance;

			PAR_SIMPLE_PARSABLE;
		};

		struct LackOfHostility
		{
			struct WantedLevel
			{
				WantedLevel()
				{}

				bool	m_Enabled;
				float	m_MinTimeSinceLastHostileAction;

				PAR_SIMPLE_PARSABLE;
			};

			LackOfHostility()
			{}

			WantedLevel m_WantedLevel1;
			WantedLevel m_WantedLevel2;
			WantedLevel m_WantedLevel3;
			WantedLevel m_WantedLevel4;
			WantedLevel m_WantedLevel5;

			float m_MaxSpeedForVehicle;

			PAR_SIMPLE_PARSABLE;
		};

		struct EnemyAccuracyScaling
		{
			EnemyAccuracyScaling()
			{}

			int		m_iMinNumEnemiesForScaling;
			float	m_fAccuracyReductionPerEnemy;
			float	m_fAccuracyReductionFloor;

			PAR_SIMPLE_PARSABLE;
		};

		struct ChargeTuning
		{
			ChargeTuning() 
			{}

			bool m_bChargeTargetEnabled;
			u8 m_uMaxNumActiveChargers;
			u32 m_uConsiderRecentChargeAsActiveTimeoutMS;
			u32 m_uMinTimeBetweenChargesAtSameTargetMS;
			u32 m_uMinTimeForSamePedToChargeAgainMS;
			u32 m_uCheckForChargeTargetPeriodMS;
			float m_fMinTimeInCombatSeconds;
			float m_fMinDistanceToTarget;
			float m_fMaxDistanceToTarget;
			float m_fMinDistToNonTargetEnemy;
			float m_fMinDistBetweenTargetAndOtherEnemies;
			float m_fDistToHidingTarget_Outer;
			float m_fDistToHidingTarget_Inner;
			
			float m_fChargeGoalCompletionRadius;

			float m_fCancelTargetOutOfCoverMovedDist;
			float m_fCancelTargetInCoverMovedDist;

			PAR_SIMPLE_PARSABLE;
		};

		struct ThrowSmokeGrenadeTuning
		{
			ThrowSmokeGrenadeTuning()
			{}

			bool m_bThrowSmokeGrenadeEnabled;
			u8 m_uMaxNumActiveThrowers;
			u32 m_uConsiderRecentThrowAsActiveTimeoutMS;
			u32 m_uMinTimeBetweenThrowsAtSameTargetMS;
			u32 m_uMinTimeForSamePedToThrowAgainMS;
			u32 m_uCheckForSmokeThrowPeriodMS;
			float m_fMinDistanceToTarget;
			float m_fMaxDistanceToTarget;
			float m_fDotMinThrowerToTarget;

			float m_fMinLoiteringTimeSeconds;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		BuddyShot		m_BuddyShot;
		LackOfHostility m_LackOfHostility;
		EnemyAccuracyScaling m_EnemyAccuracyScaling;
		ChargeTuning	m_ChargeTuning;
		ThrowSmokeGrenadeTuning m_ThrowSmokeGrenadeTuning;

		float m_MaxDistToCoverZ;
		float m_MaxDistToCoverXY;
		float m_fAmbientAnimsMinDistToTargetSq;		// How far we need to be from our target in order to play ambient combat animations
		float m_fAmbientAnimsMaxDistToTargetSq;		// How close we need to be from our target in order to play ambient combat animations
		float m_fGoToDefAreaTimeOut;
		float m_fFireContinuouslyDistMin;
		float m_fFireContinuouslyDistMax;
		float m_fLostTargetTime;
		float m_fMinTimeAfterAimPoseForStateChange;
		float m_fMaxAttemptMoveToCoverDelay;		// how much time is required between cover movements for each ped (when moving cover -> cover)
		float m_fMaxAttemptMoveToCoverDelayGlobal;	// how much time is required between cover movements from all peds (when moving cover -> cover)
		float m_fMinAttemptMoveToCoverDelay;		// how much time is required between cover movements for each ped (when moving cover -> cover)
		float m_fMinAttemptMoveToCoverDelayGlobal;	// how much time is required between cover movements from all peds (when moving cover -> cover)
		float m_fMinDistanceForAltCover;			// The minimum distance to search for alternate cover when in cover or at cover
		float m_fMinTimeStandingAtCover;			// the amount of time required to be standing at cover before allowing the ped to move to other cover
		float m_fMinTimeBetweenFrustratedPeds;		// The min time it takes for another ped to be allowed to get frustrated
		float m_fMaxTimeBetweenFrustratedPeds;		// The max time is takes for another ped to be allowed to get frustrated
		float m_fMaxWaitForCoverExitTime;
		float m_fRetreatTime;
		float m_fTargetTooCloseDistance;
		float m_fTimeBetweenCoverSearchesMax;
		float m_fTimeBetweenCoverSearchesMin;
		float m_fTimeBetweenAltCoverSearches;
		float m_fTimeBetweenJackingAttempts;
		float m_fShoutTargetPositionInterval;
		float m_fShoutBlockingLosInterval;
		float m_fTimeBetweenDragsMin;
		float m_fTimeBetweenSecondaryTargetUsesMin;
		float m_fTimeBetweenSecondaryTargetUsesMax;
		float m_fTimeToUseSecondaryTargetMin;
		float m_fTimeToUseSecondaryTargetMax;
		float m_fTimeBetweenCombatDirectorUpdates;
		float m_fTimeBetweenPassiveAnimsMin;		// The timer using this is per ped
		float m_fTimeBetweenPassiveAnimsMax;		// The timer using this is per ped
		float m_fTimeBetweenQuickGlancesMin;		// The timer using this is per ped
		float m_fTimeBetweenQuickGlancesMax;		// The timer using this is per ped
		float m_fTimeBetweenGestureAnimsMin;
		float m_fTimeBetweenGestureAnimsMax;
		float m_fTimeBetweenFailedGestureMin;
		float m_fTimeBetweenFailedGestureMax;
		float m_fTimeBetweenGesturesMinGlobal;
		float m_fTimeBetweenGesturesMaxGlobal;
		float m_fTimeSinceLastAimedAtForGesture;
		float m_fMinTimeBeforeReactToExplosion;
		float m_fMaxTimeBeforeReactToExplosion;
		float m_TargetInfluenceSphereRadius;
		float m_TargetMinDistanceToRoute;
		float m_TargetMinDistanceToAwayFacingNavLink;	// When choosing a cover point that requires using a nav link, this is the min distance from the target to the nav link
		float m_fMaxDstanceToMoveAwayFromAlly;		// This is the max distance that we'll care about an ally when deciding if we should move away
		float m_fTimeBetweenAllyProximityChecks;	// This is the base time between checks of 
		float m_fMinDistanceFromPrimaryTarget;		// The min distance we need to be from our primary target before allowing a switch to the secondary target
		float m_fMaxAngleBetweenTargets;			// The max angle allowed between self->primary and self->secondary targets for switching to secondary
		float m_MaxDistanceFromPedToHelpPed;
		float m_MaxDotToTargetToHelpPed;
		float m_MaxHeadingDifferenceForQuickGlanceInSameDirection;
		float m_MinTimeBetweenQuickGlancesInSameDirection;
		float m_MaxSpeedToStartJackingVehicle;
		float m_MaxSpeedToContinueJackingVehicle;
		float m_TargetJackRadius;
		float m_SafetyProportionInDefensiveAreaMin;	// A value 0.0 to 1.0 representing how far (%) from the center we choose a position for defensive areas
		float m_SafetyProportionInDefensiveAreaMax;	// A value 0.0 to 1.0 representing how far (%) from the center we choose a position for defensive areas
		float m_MaxMoveToDefensiveAreaAngleVariation; // This is the max we could rotate our DA to Ped vector (ccw or cw) that helps find the position to move to
		float m_MinDistanceToEnterVehicleIfTargetEntersVehicle;
		float m_MaxDistanceToMyVehicleToChase;
		float m_MaxDistanceToVehicleForCommandeer;
		u8 m_NumEarlyVehicleEntryDriversAllowed;
		u32 m_SafeTimeBeforeLeavingCover;
		u32 m_WaitTimeForJackingSlowedVehicle;
		float m_MaxInjuredTargetTimerVariation;
		u8 m_MaxNumPedsChasingOnFoot;
		float m_FireTimeAfterStaticMovementAbort;
		float m_MinMovingToCoverTimeToStop;
		float m_MinDistanceToCoverToStop;
		float m_FireTimeAfterStoppingMoveToCover;
		float m_ApproachingTargetVehicleHoldFireDistance;
		float m_MinDefensiveAreaRadiusForWillAdvance;
		float m_MaxDistanceToHoldFireForArrest;
		float m_TimeToDelayChaseOnFoot;
		float m_FireTimeAfterChaseOnFoot;
		u32	m_MinTimeToChangeChaseOnFootSpeed;
		bool  m_EnableForcedFireForTargetProximity;
		float m_MinForceFiringStateTime;
		float m_MaxForceFiringStateTime;
		float m_TimeBeforeInitialForcedFire;
		float m_TimeBetweenForcedFireStates;
		float m_MinTimeInStateForForcedFire;
		float m_MinForceFiringDistance;
		float m_MaxForceFiringDistance;
		float m_MinDistanceForAimIntro;
		float m_MinDistanceToBlockAimIntro;
		float m_MinBlockedLOSTimeToBlockAimIntro;
		float m_AmbientAnimLengthBuffer;
		u32 m_TimeBetweenPlayerArrestAttempts;
		u32 m_TimeBetweenArmedMeleeAttemptsInMs;
		bool m_AllowMovingArmedMeleeAttack;
		float m_TimeToHoldFireAfterJack;
		u32 m_MinTimeBetweenMeleeJackAttempts;
		u32 m_MinTimeBetweenMeleeJackAttemptsOnNetworkClone;
		float m_MaxTimeToHoldFireAtTaskInitialization;
		u32 m_MaxTimeToRejectRespawnedTarget;
		float m_MinDistanceForLawToFleeFromCombat;
		float m_MaxDistanceForLawToReturnToCombatFromFlee;
		float m_fTimeLosBlockedForReturnToInitialPosition;

		PAR_PARSABLE;
	};

	// our tunables
	static Tunables				ms_Tunables;

	// Should we be allowed to break up strafe by running?
	static bool ms_bEnableStrafeToRunBreakup;

#if __BANK
#define SHOT_ACCURACY_HISTORY_SIZE 128
	//
	// Debugging class to help tune enemy accuracy scaling
	//
	class CEnemyAccuracyScalingLog
	{
	public:
		// Represents a single shot measure
		class CEnemyShotAccuracyMeasurement
		{
		public:
			// Constructors
			CEnemyShotAccuracyMeasurement()
				: m_uTimeOfShotMS(0), m_iCurrentPlayerWL(-1), m_iNumEnemyShooters(0), m_fAccuracyReduction(0.0f), m_fAccuracyFinal(0.0f)
			{}

			CEnemyShotAccuracyMeasurement(u32 uTimeOfShotMS, int iCurrentPlayerWL, int iNumEnemyShooters, float fAccuracyReduction, float fAccuracyFinal) 
				: m_uTimeOfShotMS(uTimeOfShotMS), m_iCurrentPlayerWL(iCurrentPlayerWL), m_iNumEnemyShooters(iNumEnemyShooters), m_fAccuracyReduction(fAccuracyReduction), m_fAccuracyFinal(fAccuracyFinal)
			{}

			// When was the shot fired?
			u32 m_uTimeOfShotMS;
			// What was the local player's wanted level at the time of this shot?
			int m_iCurrentPlayerWL;
			// How many shooters were counted at the time of this shot?
			int m_iNumEnemyShooters;
			// How much was this shot's accuracy reduced for number of shooting enemies?
			float m_fAccuracyReduction;
			// What was the final shot accuracy?
			float m_fAccuracyFinal;
		};

		// Default constructor
		CEnemyAccuracyScalingLog()
			: m_bEnabled(false)
			, m_bRenderDebug(false)
			, m_bHasRecordsToLog(false)
			, m_uNextOutputTimeMS(0)
			, m_uMeasureOutputIntervalMS(5000)
			, m_uRenderHistoryTimeMS(5000)
			, m_iHistoryWriteIndex(0)
			, m_uLoggingTestStartTimeMS(0)
			, m_iLoggingTestHighestWL(-1)
			, m_iLoggingTestHighestNumShooters(0)
		{

		}

		bool IsEnabled() const { return m_bEnabled; }
		void SetEnabled(bool bEnable) { m_bEnabled = bEnable; }

		bool& GetEnabled() { return m_bEnabled; }
		bool& GetRenderDebug() { return m_bRenderDebug; }
		u32& GetMeasureOutputIntervalMS() { return m_uMeasureOutputIntervalMS; }
		u32& GetRenderHistoryTimeMS() { return m_uRenderHistoryTimeMS; }

		// Begin a new logging test
		void BeginLoggingTest();

		// Main update method
		void Update();

		// Record shot fired occurrence
		void RecordEnemyShotFired(int currentPlayerWL, int iNumEnemyShooters, float fNumEnemiesAccuracyReduction, float fShotAccuracyFinal);

		// Record local player wanted level cleared
		void RecordWantedLevelCleared();

		// Record local player killed
		void RecordLocalPlayerDeath();

	private:
		bool m_bEnabled; // toggle recording and output logging
		bool m_bRenderDebug; // optionally render debug display
		bool m_bHasRecordsToLog;
		u32 m_uNextOutputTimeMS;
		u32 m_uMeasureOutputIntervalMS;
		u32 m_uRenderHistoryTimeMS; // only show values for history items this young
		u32 m_uLoggingTestStartTimeMS;
		int m_iLoggingTestHighestWL; // highest player wanted level reached during test
		int m_iLoggingTestHighestNumShooters; // highest number of shooters during test

		int m_iHistoryWriteIndex; // loop around when adding to history
		atFixedArray<CEnemyShotAccuracyMeasurement, SHOT_ACCURACY_HISTORY_SIZE> m_ShotAccuracyHistory;
	};
	static CEnemyAccuracyScalingLog ms_EnemyAccuracyLog;

	// Callback for logging enemy accuracy:
	// Gives local player full health, removes invincibility, and starts logging
	static void EnemyAccuracyLogButtonCB();
	
#endif // __BANK

private:
	// Transition table implementations
	// Return the valid transitional set
	virtual aiTaskStateTransitionTable*	GetTransitionSet();
	// Generate the conditional flags to choose a transition
	virtual s64	GenerateTransitionConditionFlags( bool bStateEnded );
	// Periodically check for a new state
	virtual bool PeriodicallyCheckForStateTransitions(float fAverageTimeBetweenChecks);

	// Returns the reaction scale of the AI based on the proficiency used to scale reaction times
	// 1 defines the best reaction times, 10 the slowe;st
	float ScaleReactionTime( CPed* pPed, float fOriginalTime );
	// Update the peds current cover status
	void UpdateCoverStatus( CPed* pPed, bool bIncrementTimers );
	// Updates the cover finder if needed
	void UpdateCoverFinder( CPed* pPed );
	// Checks if the target is fleeing
	bool ShouldChaseTarget( CPed* pPed, const CPed* pTarget );
	// Checks if the ped should draw their weapon
	bool ShouldDrawWeapon() const;
	// Checks for navigation failure from a sub task
	bool CheckForNavFailure( CPed* pPed );
	// Checks if this ped should respond to losing their target
	bool CheckForTargetLossResponse();
	// Checks if the ped is outside their attack window
	bool CheckShouldMoveBackInsideAttackWindow( CPed * pPed );
	// Determine if we should exit our cover because another ped is there
	bool CheckIfAnotherPedIsAtMyCoverPoint();
	// Checks if we can be frustrated based on a number of conditions
	bool CanBeFrustrated( CPed* pPed );
	// Updates our async probe which is likely checking for LOS to the targets lock on position
	void UpdateAsyncProbe( CPed* pPed );
	// Updates our primary target (which has a possibility of choosing our secondary target for brief periods of time)
	bool UpdatePrimaryTarget( CPed* pPed );
	// Updates our ambient animations by checking the logic for various anims as well as how recently "ambient" anims were played
	void UpdateAmbientAnimPlayback( CPed* pPed );
	bool UpdateQuickGlanceAnim( CPed* pPed );
	bool UpdateGestureAnim( CPed* pPed );
	bool RequestAndStartAmbientAnim( CPed* pPed );
	fwMvClipId GetAmbientClipFromHeadingDelta(float fHeadingDelta);
	// Checks if a cover point is providing us cover
	bool DoesCoverPointProvideCover(CCoverPoint* pCoverPoint);
	// Checks if our desired cover point conditions are met
	bool CanUseDesiredCover() const;
	// Checks if the ped can be considered in cover for transitions to the cover state
	bool CanPedBeConsideredInCover( const CPed* pPed ) const;
	
	static bool AllowDraggingTask();
	static bool CanVaryAimPose(CPed* pPed);
	
	void ActivateTimeslicing();
	bool CanArrestTarget() const;
	bool ShouldArrestTarget() const;
	bool CanHelpAnyPed() const;
	bool CanHelpSpecificPed(CPed* pPed, const CPed* pTarget) const;
	bool CanJackTarget() const;
	bool CanChaseInVehicle();
	bool CanClearPrimaryDefensiveArea() const;
	bool CanMoveToTacticalPoint() const;
	bool CanMoveWithinDefensiveArea() const;
	bool CanOpenCombat() const;
	bool CanPlayAmbientAnim() const;
	bool CanReactToBuddyShot() const;
	bool CanReactToBuddyShot(CPed** ppBuddyShot) const;
	bool CanReactToExplosion() const;
	bool CanReactToImminentExplosion() const;
	bool CanUpdatePrimaryTarget() const;
	void ChangeTarget(const CPed* pTarget);
	bool CheckForGiveUpOnGoToEntryPoint() const;
	bool CheckForGiveUpOnWaitingForEntryPointToBeClear() const;
	int CountPedsWithClearLineOfSightToTarget() const;
	bool GetDesiredCoverStateToResume(s32 &iDesiredState);
	int ChooseStateToResumeTo(bool& bKeepSubTask);
	void ClearFiringAtTarget();
	void ClearHoldingFire();
	void ClearEarlyVehicleEntry();
	void ClearPrimaryDefensiveArea();
	bool DoesPedHaveGun(const CPed& rPed) const;
	bool DoesPedNeedHelp(CPed* pPed) const;
	bool DoesTargetHaveGun() const;
	void EquipWeaponForThreat();
	void ForceFiringAtTarget();
	void GetAimTarget(CAITarget& rTarget) const;
	GestureType GetGestureTypeForClipSet(fwMvClipSetId clipSetId, CPed* pPed) const;
	bool HasClearLineOfSightToTarget() const;
	void HoldFireFor(float fTime);
	bool IsVehicleSafeForJackOrArrest(const CVehicle* pTargetVehicle) const;
	bool IsArresting() const;
	bool IsCoverPointInDefensiveArea(const CCoverPoint& rCoverPoint) const;
	bool IsEventTypeDirectlyThreatening(int iEventType) const;
	bool IsInDefensiveArea() const;
	bool IsNearbyFriendlyNotInCover() const;
	bool IsOnFoot() const;
	bool IsOnScreen() const;
	bool IsPhysicallyInDefensiveArea() const;
	bool IsPotentialStateChangeUrgent() const;
	bool IsTargetOnFoot() const;
	bool IsTargetWanted() const;
	bool IsWithinDistanceOfTarget(float fMaxDistance) const;
	bool HasTargetFired(float queryTimeSeconds) const;
	bool GetTimeSinceTargetVehicleMoved(u32& uTimeSinceTargetVehicleMoved) const;
	void OnGunShot();
	void OnHoldingFire(ReasonToHoldFire nReasonToHoldFire);
	void OnNoLongerHoldingFire(ReasonToHoldFire nReasonWeWereHoldingFire);
	void OpenCombat();
	void PlayRoadBlockAudio();
	void ProcessActionsForDefensiveArea();
	void ProcessAimPose();
	void ProcessAudio();
	void ProcessCombatDirector();
	void ProcessCombatOrders();
	void ProcessExplosion();
	void ProcessFacialIdleClip();
	void ProcessFiringAtTarget();
	void ProcessHoldFire();
	void ProcessMovementClipSet();
	void ProcessPotentialBlast();
	void ProcessWeaponAnims();
	void ProcessMovementType();
	void ProcessDefensiveArea();
	void ProcessRoadBlockAudio();
	void RefreshCombatDirector();
	void ReleaseCombatDirector();
	void ReleaseTacticalAnalysis();
	void RequestCombatDirector();
	void RequestTacticalAnalysis();
	void ResetCombatMovement();
	void SayInitialAudio();
	void SetFiringAtTarget();
	void SetHoldingFire(ReasonToHoldFire nReasonToHoldFire);
	void SetUsingActionMode(bool bUsingActionMode);
	bool ShouldConsiderVictoriousInMelee() const;
	bool ShouldFinishTaskToSearchForTarget() const;
	ReasonToHoldFire ShouldHoldFire() const;
	bool ShouldHoldFireDueToLackOfHostility() const;
	bool ShouldHoldFireDueToLimitCombatants() const;
	bool ShouldHoldFireDueToMP() const;
	bool ShouldHoldFireDueToTargetArrested() const;
	bool ShouldHoldFireDueToTargetInjured() const;
	bool ShouldHoldFireDueToUnarmedTarget() const;
	bool ShouldHoldFireDueToApproachingTargetVehicle() const;
	bool ShouldHoldFireAtTaskInitialization() const;
	bool ShouldNeverChangeStateUnderAnyCircumstances() const;
	bool ShouldPlayAimIntro(CombatState eDesiredNextCombatState, bool bWantToArrestTarget) const;
	bool IsAimToTargetBlocked(const CPed* pTargetPed) const;

	bool ShouldPlayRoadBlockAudio() const;
	bool ShouldUseFranticRuns() const;
	bool ShouldWarpIntoVehicleIfPossible() const;
	bool ShouldReturnToInitialPosition() const;
	bool StateCanChange() const;
	bool StateHandlesBlockedLineOfSight() const;
	void UpdateFlagsForNearbyPeds(s64& iConditionFlags);
	void UpdateOrder();
	bool WillNeedToReactToImminentExplosionSoon() const;
	bool IsStateStillValid(s32 iState);
	
	fwMvClipSetId ChooseClipSetForFranticRuns(bool& bAreDefaultRunsFrantic) const;
	fwMvClipSetId ChooseClipSetForGesture(const CPed& rPed, GestureType nGestureType) const;
	fwMvClipSetId ChooseClipSetForQuickGlance(const CPed& rPed) const;

	// State implementations
	// Start
	FSM_Return		Start_OnUpdate		(CPed* pPed);
	void			Start_OnEnter_Clone (CPed* pPed);

	// Resume
	FSM_Return		Resume_OnUpdate();
	void			Resume_OnEnter_Clone(CPed* pPed);

	// Fire when standing
	void			Fire_OnEnter		(CPed* pPed);
	FSM_Return		Fire_OnUpdate		(CPed* pPed);
	void			Fire_OnExit			();

	// In cover
	void			InCover_OnEnter		(CPed* pPed);
	FSM_Return		InCover_OnUpdate	(CPed* pPed);
	FSM_Return		InCover_OnExit_Clone(CPed *pPed);
	// Move to cover
	void			MoveToCover_OnEnter	(CPed* pPed);
	FSM_Return		MoveToCover_OnUpdate(CPed* pPed);
	// Retreat
	void			Retreat_OnEnter		(CPed* pPed);
	FSM_Return		Retreat_OnUpdate	(CPed* pPed);
	// Melee attack
	void			MeleeAttack_OnEnter	(CPed* pPed);
	FSM_Return		MeleeAttack_OnUpdate(CPed* pPed);
	void			MeleeAttack_OnExit	(CPed* pPed);
	// Advance directly toward target
	void			Advance_OnEnter		(CPed* pPed);
	FSM_Return		Advance_OnUpdate	(CPed* pPed);
	// Advance directly toward target when the target is in cover and can't see their torso
	void			AdvanceFrustrated_OnEnter	(CPed* pPed);
	FSM_Return		AdvanceFrustrated_OnUpdate	(CPed* pPed);
	void			AdvanceFrustrated_OnExit	();
	// Charge the target's position
	void			ChargeTarget_OnEnter	(CPed* pPed);
	FSM_Return		ChargeTarget_OnUpdate	(CPed* pPed);
	void			ChargeTarget_OnExit		(CPed* pPed);
	// Run after a fleeing target
	void			ChaseOnFoot_OnEnter (CPed* pPed);
	FSM_Return		ChaseOnFoot_OnUpdate(CPed* pPed);
	void			ChaseOnFoot_OnExit();
	// Flank around target
	void			Flank_OnEnter		(CPed* pPed);
	FSM_Return		Flank_OnUpdate		(CPed* pPed);
	// Move back within the attack window
	void			MoveWithinAttackWindow_OnEnter(CPed* pPed);
	FSM_Return		MoveWithinAttackWindow_OnUpdate(CPed* pPed);
	void			MoveWithinAttackWindow_OnExit(CPed* pPed);
	// Move back within the DefensiveArea
	void			MoveWithinDefensiveArea_OnEnter(CPed* pPed);
	FSM_Return		MoveWithinDefensiveArea_OnUpdate(CPed* pPed);
	void			MoveWithinDefensiveArea_OnExit(CPed* pPed);
	// Persue the car from inside a vehicle
	void			PersueInVehicle_OnEnter	(CPed* pPed);
	FSM_Return		PersueInVehicle_OnUpdate(CPed* pPed);
	void			PersueInVehicle_OnExit(CPed* pPed);
	// Search for a lost target
	void			Search_OnEnter			(CPed* pPed);
	FSM_Return		Search_OnUpdate			(CPed* pPed);
	// PullTargetFromVehicle
	void			WaitingForEntryPointToBeClear_OnEnter();
	FSM_Return		WaitingForEntryPointToBeClear_OnUpdate();
	void			GoToEntryPoint_OnEnter();
	FSM_Return		GoToEntryPoint_OnUpdate();
	void			PullTargetFromVehicle_OnEnter		(CPed* pPed);
	FSM_Return		PullTargetFromVehicle_OnUpdate		(CPed* pPed);
	// EnterVehicle
	void			EnterVehicle_OnEnter		(CPed* pPed);
	FSM_Return		EnterVehicle_OnUpdate		(CPed* pPed);
	void			EnterVehicle_OnExit			();

	FSM_Return		DecideTargetLossResponse_OnUpdate		(CPed* pPed);
	
	void			DragInjuredToSafety_OnEnter			();
	FSM_Return		DragInjuredToSafety_OnUpdate		();

	// Mounted
	void			Mounted_OnEnter();
	FSM_Return		Mounted_OnUpdate();
	
	// MeleeToArmed
	FSM_Return		MeleeToArmed_OnUpdate();
	
	// ReactToImminentExplosion
	void			ReactToImminentExplosion_OnEnter();
	FSM_Return		ReactToImminentExplosion_OnUpdate();
	
	// ReactToExplosion
	void			ReactToExplosion_OnEnter(CPed const* pPed);
	FSM_Return		ReactToExplosion_OnUpdate();

	// ReactToBuddyShot
	void			ReactToBuddyShot_OnEnter();
	FSM_Return		ReactToBuddyShot_OnUpdate();
	
	void			ReactToBuddyShot_OnEnter_Clone();
	FSM_Return		ReactToBuddyShot_OnUpdate_Clone();
	void			ReactToBuddyShot_OnExit_Clone();

	// GetOutOfWater
	void			GetOutOfWater_OnEnter();
	FSM_Return		GetOutOfWater_OnUpdate();

	// Wait for cover exit
	FSM_Return		WaitingForCoverExit_OnUpdate();
	
	// Do an aiming intro
	void			AimIntro_OnEnter();
	FSM_Return		AimIntro_OnUpdate();
	void			AimIntro_OnExit();
	
	// Target unreachable
	void			TargetUnreachable_OnEnter();
	FSM_Return		TargetUnreachable_OnUpdate();

	// Arrest target
	void			ArrestTarget_OnEnter();
	FSM_Return		ArrestTarget_OnUpdate();
	void			ArrestTarget_OnExit();

	// MoveToTacticalPoint
	void			MoveToTacticalPoint_OnEnter();
	FSM_Return		MoveToTacticalPoint_OnUpdate();

	// Throw Smoke Grenade
	void			ThrowSmokeGrenade_OnEnter();
	FSM_Return		ThrowSmokeGrenade_OnUpdate();

	void			Victory_OnEnter();
	FSM_Return		Victory_OnUpdate();

	void			ReturnToInitialPosition_OnEnter();
	FSM_Return		ReturnToInitialPosition_OnUpdate();

	bool			CheckForMovingWithinDefensiveArea(bool bSetState);
	bool			CheckForStopMovingToCover();
	bool			CheckForForceFireState();
	
	// Return whether ped should go to the charge target state, optionally setting the state
	bool CheckForChargeTarget(bool bChangeState = true);

	// Return whether the ped should stop charging
	bool CheckForStopChargingTarget();

	// Does the ped want to charge the target's position?
	// This is the general test which will then start the charge election process
	bool WantsToChargeTarget(const CPed* pPed, const CPed* pTargetPed);

	// Check if the ped should charge the target's position
	// Either the ped was elected to charge, or scripts are triggering directly
	bool ShouldChargeTarget(const CPed* pPed, const CPed* pTarget);

	// Return whether ped should go to the throw smoke grenade state, optionally setting the state
	bool CheckForThrowSmokeGrenade(bool bChangeState = true);

	// Does the ped want to throw a smoke grenade at the target?
	// This is the general test which will start the thrower election process
	bool WantsToThrowSmokeGrenade(const CPed* pPed, const CPed* pTargetPed);

	// Check if the ped should throw a smoke grenade at the target
	// Either the ped was elected to throw, or scripts are triggering directly
	bool ShouldThrowSmokeGrenade(const CPed* pPed, const CPed* pTargetPed);

	const CWeaponInfo*	HelperGetEquippedWeaponInfo() const;
	bool			HelperCanPerformMeleeAttack() const;
	bool			CheckForTargetChangedThisFrame(bool bRestartStateOnTargetChange);
	bool			CheckForPossibleStateChanges(bool bResetStateOnTargetChange);

	s32				GetTargetsEntryPoint(CVehicle& vehicle) const;

	void			ResetCoverSearchTimer();
	void			ResetRetreatTimer();
	void			ResetLostTargetTimer();
	void			ResetGoToAreaTimer();
	void			ResetJackingTimer();
	void			ResetMoveToCoverTimer();

	//
	// Members
	//

	// ** Variables that require to be aligned at the top (Vectors etc.) ** //
	
	//Combat orders
	CCombatOrders m_CombatOrders;

	PotentialBlast m_PotentialBlast;
	
	// Transition table data - stores variables used by the base transition table implementation
	aiTaskTransitionTableData	m_transitionTableData;;
	
	// Generally using this probe helper to test LOS to a targets torso when the target is in cover
	CAsyncProbeHelper			m_asyncProbeHelper;

	CPrioritizedClipSetRequestHelper m_AmbientClipSetRequestHelper;
	CPrioritizedClipSetRequestHelper m_FranticRunsClipSetRequestHelper;

	CExpensiveProcess m_TransitionFlagDistributer;

	// Our local cover finder
	RegdCoverFinder m_pCoverFinder;

	// Current target and the original target
	RegdConstPed m_pPrimaryTarget;

	// The chosen secondary target
	RegdPed m_pSecondaryTarget;
	
	RegdPed m_pInjuredPedToHelp;
	RegdPed m_pBuddyShot;

	RegdVeh m_pVehicleToUseForChase;

	RegdTacticalAnalysis m_pTacticalAnalysis;

	// Internal timers

	//	- stores the next time the ped should attempt to communicate with buddies
	float	m_fLastCommunicationTimer;
	//	- stores the next time the ped should attempt to shout at a ped blocking their los
	float	m_fLastCommunicationBlockingLosTimer;
	// stores how long until we switch to firing at our secondary target
	// alternately also stores how long we will shoot at the secondary target when that behavior is active
	float	m_fSecondaryTargetTimer;
	
	float	m_fLastCombatDirectorUpdateTimer;

	CTaskSimpleTimer	m_retreatTimer;
	CTaskSimpleTimer	m_lostTargetTimer;
	CTaskSimpleTimer	m_goToAreaTimer;
	CTaskSimpleTimer	m_jackingTimer;

	CTaskSimpleTimer	m_coverSearchTimer;
	CTaskSimpleTimer	m_moveToCoverTimer;			// This controls the time between cover movement attempts for this specific task/ped
	float				m_fTimeUntilNextAltCoverSearch;
	float				m_fCoverMovementTimer;		// stores how long ago the coverpoint last moved (seconds since last movement)

	float				m_fTimeWaitingForClearEntry;

	float				m_fTimeUntilAmbientAnimAllowed;
	float				m_fGestureTimer;
	float				m_fTimeSinceLastQuickGlance;
	float				m_fLastQuickGlanceHeadingDelta;
	float				m_fQuickGlanceTimer;		// When this timer runs up we'll have a chance at playing a head look towards an ally

	float				m_fTimeUntilAllyProximityCheck;
	float				m_fInjuredTargetTimer;
	float				m_fInjuredTargetTimerModifier;

	float				m_fTimeSinceWeLastHadCover;
	float				m_fTimeChaseOnFootDelayed;
	float				m_fTimeUntilForcedFireAllowed;
	float				m_fTimeJacking;

	u32					m_uTimeOfLastChargeTargetCheck; // Stores the last time we checked for charging target
	u32					m_uTimeWeLastCharged; // Stores the last time we charged any target

	u32					m_uTimeOfLastSmokeThrowCheck; // Stores the last time we checked for smoke grenade throw
	u32					m_uTimeWeLastThrewSmokeGrenade; // The last time this ped threw a smoke grenade

	u32					m_uTimeTargetVehicleLastMoved;	// Stores the last time the target's vehicle moved (checked against a min speed)
	u32					m_uCachedWeaponHash;
	u32					m_uTimeToReactToExplosion;
	u32					m_uTimeChaseOnFootSpeedSet;
	u32					m_uTimeOfLastMeleeAttack;
	u32					m_uTimeToForceStopAmbientAnim;

	u32					m_uLastFrameCheckedStateTransitions;
	u32					m_uTimeToStopHoldingFire;
	u32					m_uTimeOfLastJackAttempt;

	fwMvClipSetId m_DesiredAmbientClipSetId;
	fwMvClipId m_DesiredAmbientClipId;

	// The state we want to go into but can't go immediately to (used primarily for waiting on the exit from cover to finish before changing states)
	s32 m_iDesiredState;
	
	//Using floats because apparently we don't have enough memory for a Vec3V.
	float m_fExplosionX;
	float m_fExplosionY;
	float m_fExplosionRadius;

	fwFlags32 m_uFlags;

	fwFlags8 m_uRunningFlags;
	fwFlags8 m_uConfigFlagsForVehiclePursuit;

	fwFlags8  m_uMigrationFlags;
	fwFlags32 m_uMigrationCoverSetup;

	ReasonToHoldFire m_nReasonToHoldFire : 4;

	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelperForBuddyShot;
	bool								m_buddyShotReactSubTaskLaunched;

	bool m_bPersueVehicleTargetOutsideDefensiveArea;

	// Initial heading and position
	Vec3V	m_vInitialPosition;
	float	m_fInitialHeading;

	// Statics
	// Static transition sets
	static aiTaskStateTransitionTable* ms_combatTransitionTables[CCombatBehaviour::CT_NumTypes];
	// Reaction scale set, it varies depending on the combat ability.
	static float ms_ReactionScale[CCombatData::CA_NumTypes];

	// This controls how frequently any ped can play the halt/come gesture anims (in addition to the per ped timer)
	static float ms_fTimeUntilNextGestureGlobal;

	// This timer is what controls how long it takes to allow a ped to move to another cover point when already in cover
	// and takes into account the last time any ped moved to cover
	static CTaskTimer ms_moveToCoverTimerGlobal;

	// Peds are not allowed to be frustrated unless this timer has expired
	static CTaskTimer ms_allowFrustrationTimerGlobal;

	// The time we last ran our async probe (don't want to run it too frequently)
	static u32 ms_iTimeLastAsyncProbeSubmitted;
	
	// The last time a drag was active
	static u32 ms_uTimeLastDragWasActive;

	// The number of drivers currently doing an early vehicle entry
	static u8 ms_uNumDriversEnteringVehiclesEarly;

	// The number of peds we currently have running the chase on foot state
	static u8 ms_uNumPedsChasingOnFoot;

	// Keeps track if we did a gesture probe this frame for the class
	static bool ms_bGestureProbedThisFrame;

	// This is the current ped that is getting frustrated or running the frustrated behavior.
	static RegdPed ms_pFrustratedPed;

	// Clip sets for ambient animations
	static const fwMvClipSetId	ms_BeckonPistolClipSetId;
	static const fwMvClipSetId	ms_BeckonPistolGangClipSetId;
	static const fwMvClipSetId	ms_BeckonRifleClipSetId;
	static const fwMvClipSetId	ms_BeckonSmgGangClipSetId;
	static const fwMvClipSetId	ms_GlancesPistolClipSetId;
	static const fwMvClipSetId	ms_GlancesPistolGangClipSetId;
	static const fwMvClipSetId	ms_GlancesRifleClipSetId;
	static const fwMvClipSetId	ms_GlancesSmgGangClipSetId;
	static const fwMvClipSetId	ms_HaltPistolClipSetId;
	static const fwMvClipSetId	ms_HaltPistolGangClipSetId;
	static const fwMvClipSetId	ms_HaltRifleClipSetId;
	static const fwMvClipSetId	ms_HaltSmgGangClipSetId;
	static const fwMvClipSetId	ms_OverTherePistolClipSetId;
	static const fwMvClipSetId	ms_OverTherePistolGangClipSetId;
	static const fwMvClipSetId	ms_OverThereRifleClipSetId;
	static const fwMvClipSetId	ms_OverThereSmgGangClipSetId;
	static const fwMvClipSetId	ms_SwatGetsureClipSetId;

#if !__FINAL

	void PrintAllDataValues() const;

#endif /* !__FINAL */
};

//-------------------------------------------------------------------------
// Cloned combat task info
//-------------------------------------------------------------------------

class CClonedCombatTaskInfo : public CSerialisedFSMTaskInfo
{
public:
	
	CClonedCombatTaskInfo()
	:
		m_target(NULL),
		m_buddyShot(NULL),
		m_fExplosionX(0.0f),
		m_fExplosionY(0.0f),
		m_fExplosionRadius(0.0f),
		m_bIsWaitingAtRoadBlock(false),
		m_bPreventChangingTarget(false)
	{}

	CClonedCombatTaskInfo(s32 iState, const CPed* pTarget, const CPed* pBuddyShot, float const explosionX, float const explosionY, float const explosionRadius, bool bIsWaitingAtRoadBlock, bool bPreventChangingTarget)
	:
		m_target(const_cast<CPed*>(pTarget)),
		m_buddyShot(const_cast<CPed*>(pBuddyShot)),
		m_fExplosionX(explosionX),
		m_fExplosionY(explosionY),
		m_fExplosionRadius(explosionRadius),
		m_bIsWaitingAtRoadBlock(bIsWaitingAtRoadBlock),
		m_bPreventChangingTarget(bPreventChangingTarget)
	{
		SetStatusFromMainTaskState(iState);
	}

	s32						GetTaskInfoType( ) const	{ return INFO_TYPE_COMBAT; }
	virtual CTaskFSMClone*	CreateCloneFSMTask();
	virtual CTask*			CreateLocalTask(fwEntity *pEntity);

	virtual bool			HasState() const			{ return true; }
	virtual u32				GetSizeOfState() const		{ return datBitsNeeded<CTaskCombat::State_Finish + 1>::COUNT;}
	static const char*		GetStateName(s32)			{ return "FSM State";}

	virtual const CEntity*	GetTarget()	const			{ return (const CEntity*) m_target.GetPed(); }
	virtual CEntity*		GetTarget()					{ return (CEntity*) m_target.GetPed(); }

	virtual const CEntity*	GetBuddyShot()	const		{ return (const CEntity*) m_buddyShot.GetPed(); }
	virtual CEntity*		GetBuddyShot()				{ return (CEntity*) m_buddyShot.GetPed(); }

	virtual float			GetExplosionX() const		{ return m_fExplosionX;	}
	virtual float			GetExplosionY() const		{ return m_fExplosionY;	}
	virtual float			GetExplosionRadius() const	{ return m_fExplosionRadius; }

	bool					IsWaitingAtRoadBlock() const { return m_bIsWaitingAtRoadBlock; }
	bool					GetPreventChangingTarget() const { return m_bPreventChangingTarget; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		m_target.Serialise(serialiser);
		m_buddyShot.Serialise(serialiser);

		bool explosion = m_fExplosionX != 0.0f || m_fExplosionY != 0.0f || m_fExplosionRadius != 0.0f;
		SERIALISE_BOOL(serialiser, explosion, "Is Explosion");
		if(explosion)
		{
			Vector3 pos(m_fExplosionX, m_fExplosionY, m_fExplosionRadius);
			SERIALISE_POSITION(serialiser, pos, "Explosion Info");
			m_fExplosionX		= pos.x;
			m_fExplosionY		= pos.y;
			m_fExplosionRadius	= pos.z;
		}

		SERIALISE_BOOL(serialiser, m_bIsWaitingAtRoadBlock, "Is Waiting At Road Block");
		SERIALISE_BOOL(serialiser, m_bPreventChangingTarget, "Prevent Changing Target");
	}

private:

	CClonedCombatTaskInfo(const CClonedCombatTaskInfo &);

	CClonedCombatTaskInfo &operator=(const CClonedCombatTaskInfo &);

	CSyncedPed	m_target;
	CSyncedPed	m_buddyShot;
	float		m_fExplosionX;
	float		m_fExplosionY;
	float		m_fExplosionRadius;
	bool		m_bIsWaitingAtRoadBlock;
	bool		m_bPreventChangingTarget;
};

#if __BANK
class CCombatDebug
{
public:
	// Initialise RAG widgets
	static void InitWidgets();

	static void TellFocusPedToAttackPlayerCB();
	static void PedHealthChangeCB();
	static void PedIdRangeChangeCB();
	static void PedSeeingRangeChangeCB();
	static void PedSeeingRangePeripheralChangeCB();
	static void PedVisualFieldMinElevationAngleChangeCB();
	static void PedVisualFieldMaxElevationAngleChangeCB();
	static void PedVisualFieldMinAzimuthAngleChangeCB();
	static void PedVisualFieldMaxAzimuthAngleChangeCB();
	static void PedCenterOfGazeMaxAngleChangeCB();
	static void PedHearingRangeChangeCB();
	static void GetDefensivePos1FromCameraCoordsCB();
	static void GetDefensivePos2FromCameraCoordsCB();
	static void CreateSphereDefensiveAreaForFocusPedCB();
	static void CreateAngledDefensiveAreaForFocusPedCB();
	static void UpdateRadiusForFocusPedCB();

	static void PrintDefensiveAreaCB();

	// Set/Unset a peds combat flags in game
	static void SetBehaviourFlag(s32 iFlag);
	static void UnSetBehaviourFlag(s32 iFlag);
	static void SetCanUseCoverFlagCB()									{ SetBehaviourFlag(CCombatData::BF_CanUseCover); }
	static void UnSetCanUseCoverFlagCB()								{ UnSetBehaviourFlag(CCombatData::BF_CanUseCover); }
	static void SetCanUseVehiclesFlagCB()								{ SetBehaviourFlag(CCombatData::BF_CanUseVehicles); }
	static void UnSetCanUseVehiclesFlagCB()								{ UnSetBehaviourFlag(CCombatData::BF_CanUseVehicles); }
	static void SetCanDoDriveBysFlagCB()								{ SetBehaviourFlag(CCombatData::BF_CanDoDrivebys); }
	static void UnSetCanDoDriveBysFlagCB()								{ UnSetBehaviourFlag(CCombatData::BF_CanDoDrivebys); }
	static void SetCanLeaveVehicleCB()									{ SetBehaviourFlag(CCombatData::BF_CanLeaveVehicle); }
	static void UnSetCanLeaveVehicleCB()								{ UnSetBehaviourFlag(CCombatData::BF_CanLeaveVehicle); }
	static void SetCanInvestigateFlagCB()								{ SetBehaviourFlag(CCombatData::BF_CanInvestigate); }
	static void UnSetCanInvestigateFlagCB()								{ UnSetBehaviourFlag(CCombatData::BF_CanInvestigate); }
	static void SetFleeWhilstInVehicleFlagCB()							{ SetBehaviourFlag(CCombatData::BF_FleeWhilstInVehicle); }
	static void UnSetFleeWhilstInVehicleFlagCB()						{ UnSetBehaviourFlag(CCombatData::BF_FleeWhilstInVehicle); }
	static void SetJustFollowInVehicleFlagCB()							{ SetBehaviourFlag(CCombatData::BF_JustFollowInVehicle); }
	static void UnSetJustFollowInVehicleFlagCB()						{ UnSetBehaviourFlag(CCombatData::BF_JustFollowInVehicle); }
	static void SetWillScanForDeadPedsFlagCB()							{ SetBehaviourFlag(CCombatData::BF_WillScanForDeadPeds); }
	static void UnSetWillScanForDeadPedsFlagCB()						{ UnSetBehaviourFlag(CCombatData::BF_WillScanForDeadPeds); }
	static void SetJustSeekCoverFlagCB()								{ SetBehaviourFlag(CCombatData::BF_JustSeekCover); }
	static void UnSetJustSeekCoverFlagCB()								{ UnSetBehaviourFlag(CCombatData::BF_CanUseCover); }
	static void SetBlindFireWhenInCoverFlagCB()							{ SetBehaviourFlag(CCombatData::BF_BlindFireWhenInCover); }
	static void UnSetBlindFireWhenInCoverFlagCB()						{ UnSetBehaviourFlag(CCombatData::BF_BlindFireWhenInCover); }
	static void SetAggressiveFlagCB()									{ SetBehaviourFlag(CCombatData::BF_Aggressive); }
	static void UnSetAggressiveFlagCB()									{ UnSetBehaviourFlag(CCombatData::BF_Aggressive); }
	static void SetHasRadioFlagCB()										{ SetBehaviourFlag(CCombatData::BF_HasRadio); }
	static void UnSetHasRadioFlagCB()									{ UnSetBehaviourFlag(CCombatData::BF_HasRadio); }
	static void SetCanUseVehicleTauntsCB()								{ SetBehaviourFlag(CCombatData::BF_CanTauntInVehicle); }
	static void UnSetCanUseVehicleTauntsCB()							{ UnSetBehaviourFlag(CCombatData::BF_CanTauntInVehicle); }
	static void SetWillDragInjuredPedsToSafetyCB()						{ SetBehaviourFlag(CCombatData::BF_WillDragInjuredPedsToSafety); }
	static void UnSetWillDragInjuredPedsToSafetyCB()					{ UnSetBehaviourFlag(CCombatData::BF_WillDragInjuredPedsToSafety); }
	static void SetUseProximityFiringRateCB()							{ SetBehaviourFlag(CCombatData::BF_UseProximityFiringRate); }
	static void UnSetUseProximityFiringRateCB()							{ UnSetBehaviourFlag(CCombatData::BF_UseProximityFiringRate); }
	static void SetMaintainMinDistanceToTargetCB()						{ SetBehaviourFlag(CCombatData::BF_MaintainMinDistanceToTarget); }
	static void UnSetMaintainMinDistanceToTargetCB()					{ UnSetBehaviourFlag(CCombatData::BF_MaintainMinDistanceToTarget); }
	static void SetCanUsePeekingVariationsCB()							{ SetBehaviourFlag(CCombatData::BF_CanUsePeekingVariations); }
	static void UnSetCanUsePeekingVariationsCB()						{ UnSetBehaviourFlag(CCombatData::BF_CanUsePeekingVariations); }
	static void SetEnableTacticalPointsWhenDefensiveCB()				{ SetBehaviourFlag(CCombatData::BF_EnableTacticalPointsWhenDefensive); }
	static void UnSetEnableTacticalPointsWhenDefensiveCB()				{ UnSetBehaviourFlag(CCombatData::BF_EnableTacticalPointsWhenDefensive); }
	static void SetDisableAimAtAITargetsInHelisCB()						{ SetBehaviourFlag(CCombatData::BF_DisableAimAtAITargetsInHelis); }
	static void UnSetDisableAimAtAITargetsInHelisCB()					{ UnSetBehaviourFlag(CCombatData::BF_DisableAimAtAITargetsInHelis); }
	static void SetCanIgnoreBlockedLosWeightingCB()						{ SetBehaviourFlag(CCombatData::BF_CanIgnoreBlockedLosWeighting); }	
	static void UnSetCanIgnoreBlockedLosWeightingCB()					{ UnSetBehaviourFlag(CCombatData::BF_CanIgnoreBlockedLosWeighting); }
	static void SetWillGenerateDeadPedSeenScriptEventsFlagCB()			{ SetBehaviourFlag(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents); }
	static void UnSetWillGenerateDeadPedSeenScriptEventsFlagCB()		{ UnSetBehaviourFlag(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents); }

	// Sets the combat movement style
	static void SetMovementFlag(CCombatData::Movement iFlag);
	static void SetStationaryCB()										{ SetMovementFlag(CCombatData::CM_Stationary); }
	static void SetDefensiveCB()										{ SetMovementFlag(CCombatData::CM_Defensive); }
	static void SetWillAdvanceCB()										{ SetMovementFlag(CCombatData::CM_WillAdvance); }
	static void SetWillRetreatCB()										{ SetMovementFlag(CCombatData::CM_WillRetreat); }

	static void SetAttackRange(CCombatData::Range eRange);
	static void SetAttackRangeNearCB()									{ SetAttackRange(CCombatData::CR_Near); }
	static void SetAttackRangeMediumCB()								{ SetAttackRange(CCombatData::CR_Medium); }
	static void SetAttackRangeFarCB()									{ SetAttackRange(CCombatData::CR_Far); }

	static void SetNeverLoseTargetCB();

	static void RenderPreferredCoverDebugForPed(const CPed& ped);

	static bool		ms_bRenderCombatTargetInfo;
	static bool		ms_bPlayGlanceAnims;
	static float	ms_fSetHealth;
	static float	ms_fIdentificationRange;
	static float	ms_fSeeingRange;
	static float	ms_fSeeingRangePeripheral;
	static float	ms_fVisualFieldMinElevationAngle;
	static float	ms_fVisualFieldMaxElevationAngle;
	static float	ms_fVisualFieldMinAzimuthAngle;
	static float	ms_fVisualFieldMaxAzimuthAngle;
	static float	ms_fCenterOfGazeMaxAngle;
	static float	ms_fHearingRange;

	// Debug creating defensive areas
	static char ms_vDefensiveAreaPos1[256];
	static char ms_vDefensiveAreaPos2[256];
	static float ms_fDefensiveAreaWidth;
	static bool ms_bDefensiveAreaAttachToPlayer;
	static bool ms_bDefensiveAreaDragging;
	static bool ms_bGiveSphereDefensiveArea;
	static bool ms_bRenderPreferredCoverPoints;
	static bool ms_bUseSecondaryDefensiveArea;
};
#endif // __BANK

#endif // TASK_NEW_COMBAT_H
