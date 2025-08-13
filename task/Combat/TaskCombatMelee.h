/////////////////////////////////////////////////////////////////////////////////
// Title	:	TaskCombatMelee.h
//
// These classes enables individual peds to engage in melee combat
// with other peds.
//
// Note: The major parts of our "Melee Combat AI" are marked with the quoted
// term in this sentence.
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_TASK_COMBAT_MELEE_H
#define INC_TASK_COMBAT_MELEE_H

#include "Pathserver/PathServer.h"
#include "Peds/Action/ActionManager.h"
#include "Task/Combat/CombatManager.h"
#include "Task/System/Task.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h"

// Rage forward declarations
namespace rage
{
	class phBoundComposite;
}

class CTaskMeleeActionResult;
class CDefensiveArea;

static const unsigned int INVALID_ACTIONRESULT_ID = 0;
static const unsigned int INVALID_ACTIONDEFINITION_ID = 0;

struct sNearbyPedInfo
{
	CPed*		m_pPed;
	CTask*		m_pMeleeTask;
	float		m_fDistanceToTarget;
};

struct sCollisionInfo
{
	WorldProbe::CShapeTestHitPoint* m_pResult;
	Vector3 m_vImpactDirection;
};

struct StrikeCapsule
{
	Vector3				m_start;
	Vector3				m_end;
	float				m_radius;
	phMaterialMgr::Id	m_materialId;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Helper class that will embody a traditional timer that one can pause
// and resume. CTaskGameTimer is a bit clunky in terms of pausing and unpausing 
// a timer.
/////////////////////////////////////////////////////////////////////////////////
class CMeleeTimer
{
public:

	CMeleeTimer()
		: m_iStartTime(0),
		  m_iDuration(0),
		  m_bEnabled(false),
		  m_bPaused(false)
	{
	}

	CMeleeTimer( const u32 iStartTime, const u32 iDuration, const bool bEnabled = true )
		: m_iStartTime(iStartTime),
		  m_iDuration(iDuration),
		  m_bEnabled(bEnabled),
		  m_bPaused(false)
	{
	}

	~CMeleeTimer()
	{
		m_iStartTime = 0;
		m_iDuration = 0;
		m_bEnabled = false;
		m_bPaused = false;
	}

	void Set( const u32 iStartTime, const u32 iDuration, const bool bEnabled = true )
	{
		m_iStartTime = iStartTime;
		m_iDuration = iDuration;
		m_bEnabled = bEnabled;
		m_bPaused = false;
	}

	bool IsEnabled( void ) const { return m_bEnabled; };
	void Enable( void ) { m_bEnabled = true; }
	void Disable( void ) { m_bEnabled = false; }

	bool IsOutOfTime() const
	{
		if( IsEnabled() )
			return IsPaused() ? GetDuration() == 0 : ( GetStartTime() + GetDuration() ) < fwTimer::GetTimeInMilliseconds();
		
		return true;
	}

	s32 GetTimeRemaining( void ) const
	{
		if( IsEnabled() )
			return ( GetStartTime() + GetDuration() ) - fwTimer::GetTimeInMilliseconds();
		
		Assertf( 0, "CMeleeTimer::GetTimeRemaining - Asking for the time remaining on an uninitialised timer." );
		return 0;
	}

	s32 GetTimeElapsed ( void ) const
	{
		if( IsEnabled() )
			return fwTimer::GetTimeInMilliseconds() - GetStartTime();
	
		Assertf( 0, "CMeleeTimer::GetTimeElapsed - Asking for the time elapsed on an uninitialised timer." );
		return 0;
	}

	u32 GetStartTime( void ) const { return m_iStartTime; }
	u32 GetDuration( void ) const { return m_iDuration; }
	bool IsPaused( void ) const { return m_bPaused; }
	void UnPause( void )
	{
		m_bPaused = false;
		m_iStartTime = fwTimer::GetTimeInMilliseconds();
	}

	void Pause( void )
	{
		m_bPaused = true;
		m_iDuration = GetDuration() - GetTimeElapsed();
	}

private:

	u32 m_iStartTime;
	u32 m_iDuration;
	bool m_bEnabled;
	bool m_bPaused;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Enables individual peds to perform series of melee actions in a
// smooth and convincing manner.
/////////////////////////////////////////////////////////////////////////////////
class CTaskMelee : public CTaskFSMClone
{
	friend class CClonedTaskMeleeInfo;

public:	
	// Task states
	enum MeleeState
	{
		State_Invalid = -1,
		State_Start,
		State_InitialPursuit,
		State_LookAtTarget,
		State_ChaseAfterTarget,
		State_RetreatToSafeDistance,
		State_WaitForTarget,
		State_WaitForMeleeActionDecide,
		State_WaitForMeleeAction,
		State_InactiveObserverIdle,
		State_MeleeAction,
		State_SwapWeapon,
		State_Finish
	};

	// Melee flags
	enum MeleeFlags
	{
		MF_IsDoingReaction							= BIT0,
		MF_ShouldBeInMeleeMode						= BIT1,
		MF_HasLockOnTarget							= BIT2,
		MF_QuitTaskAfterNextAction					= BIT3,
		MF_EndsInIdlePose							= BIT4,
		MF_ShouldStartGunTaskAfterNextAction		= BIT5,
		MF_ForceStrafe								= BIT6,
		MF_PerformIntroAnim							= BIT7,
		MF_CannotFindNavRoute						= BIT8,
		MF_TargetIsUnreachable						= BIT9,
		MF_ForcedReaction							= BIT10,
		MF_AttemptIdleControllerInterrupt			= BIT11,
		MF_AttemptRunControllerInterrupt			= BIT12,
		MF_AttemptMeleeControllerInterrupt			= BIT13,
		MF_AttemptVehicleEnterInterrupt				= BIT14,
		MF_ResetTauntTimerAfterNextAction			= BIT15,
		MF_ReactionResetTimeInTaskAfterNextAction	= BIT16,
		MF_AttackResetTimeInTaskAfterNextAction		= BIT17,
		MF_AllowNoTargetInterrupt					= BIT18,
		MF_SuppressMeleeActionHoming				= BIT19,
		MF_BehindTargetPed							= BIT20,
		MF_AllowStrafeMode							= BIT21,
		MF_AllowStealthMode							= BIT22,
		MF_ForceInactiveTauntMode					= BIT23,
		MF_GiveThreatResponseAfterNextAction		= BIT24,
		MF_PlayTauntBeforeAttacking					= BIT25,
		MF_HasPerformedInitialPursuit				= BIT26,
		MF_AnimSynced								= BIT27,
		MF_AttemptCoverEnterInterrupt				= BIT28,
		MF_AttemptDiveInterrupt						= BIT29,
		MF_ForceStrafeIndefinitely					= BIT30,
		MF_TagSyncBlendOut							= BIT31,
	};

	enum QueueType
	{
		QT_Invalid = -1,
		QT_InactiveObserver,
		QT_SupportCombatant,
		QT_ActiveCombatant
	};

	// Instance members.
	CTaskMelee(const CActionResult* pInitialActionResult, CEntity* pEntity, fwFlags32 iFlags = 0, CSimpleImpulseTest::Impulse iInitialImpulse = CSimpleImpulseTest::ImpulseNone, s32 nTimeInTask = 0, u16 uUniqueNetworkID = 0);

	virtual aiTask*			Copy							(void) const;
	virtual bool			ShouldAbort						(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual int				GetTaskTypeInternal				(void) const {return CTaskTypes::TASK_MELEE;}
	virtual void			GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	void					StopAll							(void);

	bool					IsCurrentlyDoingAMove			(bool bCountTauntAsAMove = true, bool bCountIntrosAsMove = false, bool bAllowInterrupt = false) const;
	bool					CheckForBranch					(CPed* pPed, CTaskMeleeActionResult* pCurrentActionResultTask, u32& nBranchActionId);
	void					SetTargetEntity					(CEntity* pTargetEntity, bool bHasLockOnTargetEntity = false, bool bReplaceLockOn = false);
	CEntity*				GetTargetEntity					(void) const { return m_pTargetEntity; }
	bool					HasLockOnTargetEntity			(void) const { return m_nFlags.IsFlagSet( MF_HasLockOnTarget ); }
	bool					ShouldPerformIntroAnim			(CPed* pPed) const;
	bool					CanPerformMeleeAttack			(CPed* pPed, CEntity* pTargetEntity) const;

	void					CalculateDesiredTargetDistance	(void);
	float					GetDesiredTargetDistance		(void) const { return m_fDesiredTargetDist; }
	Vec3V_Out				CalculateDesiredChasePosition	(CPed* pPed);
	bool					IsDoingQueueForTarget			(void) const { return m_queueType != QT_ActiveCombatant; }
	QueueType				GetQueueType					(void) const { return m_queueType; }
	void					SetQueueType					(QueueType queueType);
	bool					IsLosBlocked					(void) const { return m_bLosBlocked; }
	bool					ShouldBeInInactiveTauntMode		(void) const { return GetIsTaskFlagSet( MF_ForceInactiveTauntMode ) || GetIsTaskFlagSet( MF_TargetIsUnreachable ) || GetIsTaskFlagSet( MF_CannotFindNavRoute ); }

	float					GetAngleSpread					(void) const { return m_fAngleSpread; }
	void					SetAngleSpread					(float fSpread) { m_fAngleSpread = fSpread; }

	u32						GetActionResultId				(void) const;
	void					SetLocalReaction				(const CActionResult* pActionResult, bool bHitReaction, u16 uMeleeID);

	void					ResetImpulseCombo				(void);

	inline bool				GetIsTaskFlagSet				(s32 flag) const	{ return m_nFlags.IsFlagSet( flag ); }
	inline void				SetTaskFlag						(s32 flag)			{ m_nFlags.SetFlag( flag ); }
	inline void				ClearTaskFlag					(s32 flag)			{ m_nFlags.ClearFlag( flag ); }
	fwFlags32&				GetTaskFlags					(void)				{ return m_nFlags; }

	bool					IsPedDoingGunDisArm				(void) { return false; }

	float					GetBlockDelayTimer				(void) const { return m_fBlockDelayTimer; }
	float					GetBlockBuff					(void) const { return m_fBlockBuff; }

	void					IncreaseMeleeBuff				(CPed* pPed);
	void					DecreaseMeleeBuff				(void);

	// Stealth camera data
	u32						GetTakeDownCameraAnimId			(void) const { return m_nCameraAnimId; } 
	Vec3V_Out				GetStealthKillWorldOffset		(void) const { return m_vStealthKillWorldOffset; }
	float					GetStealthKillHeading			(void) const { return m_fStealthKillWorldHeading; }
	
	bool					IsBlocking						(void) const { return m_bIsBlocking; }
	bool					ShouldDaze						(void) const { return m_bCanPerformDazedReaction; }
	bool					IsUsingStealthClips				(void) const { return m_bUsingStealthClipClose; }
	bool					IsBehindTargetPed				(void) const { return GetIsTaskFlagSet( MF_BehindTargetPed ); }
	bool					WasLastActionARecoil			(void) const { return m_bLastActionWasARecoil; }

	void					ResetTauntTimer					(void);

	// Check properties of the task data to determine if the heading should be updated.
	static bool		ShouldUpdateDesiredHeading			(const CPed& ped, Vec3V_In vTargetPosition);

	virtual bool UseCustomGetup() { return true; }
	virtual bool AddGetUpSets ( CNmBlendOutSetList& sets, CPed* pPed );
	
	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed*) { return false; }
	bool						HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* pTaskInfo);
	bool						CanRetainLocalTaskOnFailedSwitch() { return true; }
	virtual bool				AllowsLocalHitReactions() const;
	virtual bool				ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) { return !GetIsTaskFlagSet( CTaskMelee::MF_ForcedReaction ); }

	// Qsort functions
	static int				CompareCollisionInfos				(const sCollisionInfo* pA1, const sCollisionInfo* pA2);

	u16						GetUniqueNetworkMeleeActionID() const;

	static crFrameFilter* GetMeleeGripFilter(fwMvClipSetId clipsetId);

protected:

	// FSM implementations
	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void	CleanUp						(void);
	FSM_Return		ProcessPreFSM				(void);
	virtual void	ProcessPreRender2			(void);
	FSM_Return		UpdateFSM					(const s32 iState, const FSM_Event iEvent);
	virtual s32		GetDefaultStateAfterAbort	(void)	const { return State_Start; }
	
	// FSM state implementations
	// Start
	void			Start_OnEnter						(CPed* pPed);
	FSM_Return		Start_OnUpdate						(CPed* pPed);
	void			Start_OnExit						(void);
	
	// InitialPursuit
	void			InitialPursuit_OnEnter				(CPed* pPed);
	FSM_Return		InitialPursuit_OnUpdate				(CPed* pPed);
	void			InitialPursuit_OnExit				(void);

	// LookAtTarget
	FSM_Return		LookAtTarget_OnUpdate				(CPed* pPed);
	void			LookAtTarget_OnExit					(void);

	// ChaseAfterTarget
	void			ChaseAfterTarget_OnEnter			(CPed* pPed);
	FSM_Return		ChaseAfterTarget_OnUpdate			(CPed* pPed);
	void			ChaseAfterTarget_OnExit				(void);

	// RetreatToSafeDistance
	void			RetreatToSafeDistance_OnEnter		(CPed* pPed);
	FSM_Return		RetreatToSafeDistance_OnUpdate		(CPed* pPed);
	void			RetreatToSafeDistance_OnExit		(CPed* pPed);

	// WaitForTarget
	void			WaitForTarget_OnEnter				(CPed* pPed);
	FSM_Return		WaitForTarget_OnUpdate				(CPed* pPed);
	void			WaitForTarget_OnExit				(void);

	// WaitForMeleeActionDecide
	void			WaitForMeleeActionDecide_OnEnter	(CPed* pPed);
	FSM_Return		WaitForMeleeActionDecide_OnUpdate	(void);

	// WaitForMeleeAction
	void			WaitForMeleeAction_OnEnter			(void);
	FSM_Return		WaitForMeleeAction_OnUpdate			(CPed* pPed);
	void			WaitForMeleeAction_OnExit			(void);

	// InactiveObserverIdle
	FSM_Return		InactiveObserverIdle_OnUpdate		(CPed* pPed);
	
	// Melee Action
	void			MeleeAction_OnEnter					(CPed* pPed);
	FSM_Return		MeleeAction_OnUpdate				(CPed* pPed);
	void			MeleeAction_OnExit					(CPed* pPed);

	// Swap weapon
	void			SwapWeapon_OnEnter					(CPed* pPed);
	FSM_Return		SwapWeapon_OnUpdate					(void);

	bool			CheckForActionBranch				(CPed* pPed, CTaskMeleeActionResult* pActionResultTask, MeleeState& newState);
	FSM_Return		ManageActionSubtaskAndBranches		(CPed* pPed);

	void			UpdateBlocking						(CPed* pPed, bool bManageUpperbodyAnims = true);
	void			UpdateStealth						(CPed* pPed);

	void			UpdateBlockingTargetEvaluation		(CPed* pPed);

	void			RegisterMeleeOpponent				(CPed* pPed, CEntity* pTargetEntity);
	void			UnRegisterMeleeOpponent				();

	// Set properties in the motion task to achieve better chasing.
	bool			ShouldAssistMotionTask				();
	void			ProcessMotionTaskAssistance			(bool bAssistance);

	bool			CanChangeTarget						() const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *		GetStaticStateName			(s32 iState);
	virtual void			Debug() const;
#endif //!__FINAL

	void MakeStreamingClipRequests		(CPed* pPed);
	void ReleaseStreamingClipRequests	(void);

	void UpdateTarget					(CPed* pPed);
	void UpdateHeadAndSpineLooksAndLegIK(CPed* pPed);
	void MeleeMoveTaskControl			(CPed* pPed, bool bAllowMovementInput, bool bAllowStrafing, bool bAllowHeadingUpdate, bool bAllowStealth, CEntity* pAvoidEntity);

	// Generate the target flags for this ped
	void UpdateTargetFlags				(CPed* pPed);

	// Cancel any pending line-of-sight request in the path-server
	void CancelPendingLosRequest		(void);
	void ProcessLineOfSightTest			(CPed* pPed);
	void ProcessUnreachableNavMeshTest	(CPed* pPed, CEntity* pTargetEntity );

	MeleeState				GetDesiredStateAndUpdateBreakoutConditions(CPed* pPed);

	bool					CheckCloneForLocalBranch(CPed* pPed);

	CNavmeshRouteSearchHelper m_RouteSearchHelper;
	CTaskGameTimer			m_nextRequiredMoveTimer;
	CTaskGameTimer			m_tauntTimer;
	CMeleeTimer				m_forceStrafeTimer;
	CTaskGameTimer			m_seekModeScanTimer;
	CTaskGameTimer			m_seekModeLosTestNextRequestTimer;
	CImpulseCombo			m_impulseCombo;
	WorldProbe::CShapeTestSingleResult m_seekModeLosTestHandle1;
	WorldProbe::CShapeTestSingleResult m_seekModeLosTestHandle2;
	fwClipSetRequestHelper	m_MovementClipSetRequestHelper;
	fwClipSetRequestHelper	m_TauntClipSetRequestHelper;
	fwClipSetRequestHelper	m_VariationClipSetRequestHelper;
	CCombatMeleeOpponent	m_Opponent;
	const CActionResult*	m_pNextActionResult;
	RegdEnt					m_pTargetEntity;
	RegdEnt					m_pAvoidEntity;
	fwFlags32				m_nFlags;
	Vec3V					m_vStealthKillWorldOffset;
	float					m_fStealthKillWorldHeading;
	float					m_fBlockDelayTimer;
	float					m_fBlockBuff;
	float					m_fDesiredTargetDist;
	float					m_fAngleSpread;
	u32						m_nCameraAnimId;
	u32						m_nActionResultNetworkID;
	u16						m_nUniqueNetworkMeleeActionID;
	QueueType				m_queueType;
	QueueType				m_cloneQueueType;
	bool					m_bActionIsBranch : 1;
	bool					m_bStopAll : 1;
	bool					m_bHasLockOnTargetEntity : 1;
	bool					m_bLocalReaction : 1;
	bool					m_bSeekModeLosTestHasBeenRequested : 1;
	bool					m_bLosBlocked : 1;
	bool					m_bIsBlocking : 1;
	bool					m_bUsingStealthClipClose : 1;
	bool					m_bUsingBlockClips : 1;
	bool					m_bCanPerformCombo : 1;
	bool					m_bCanPerformDazedReaction : 1;
	bool					m_bCanBlock : 1;
	bool					m_bCanCounter : 1;
	bool					m_bLastActionWasARecoil : 1;
	bool					m_bEndsInAimingPose : 1;
#if FPS_MODE_SUPPORTED
	bool					m_bInterruptForFirstPersonCamera : 1;
#endif

public:// Static members.

	static	bool			IsClipSetAMeleeGroupForPed		(const CPed* pPed, fwMvClipSetId clipSetId);
	static	bool			ShouldCheckForMeleeAmbientMove	(CPed* pPed, bool& bCheckTaunt, bool bAllowHeavyAttack );
	static	aiTask*			CheckForAndGetMeleeAmbientMove	(const CPed* pPed, CEntity* pTargetEntity, bool hasLockOnTarget, bool goIntoMeleeEvenWhenNoSpecificMoveFound, bool bPerformMeleeIntro = true, bool bAllowStrafeMode = false, bool bCheckLastFound = false );

	static CActionFlags::ActionTypeBitSetData GetActionTypeToLookFor(const CPed* pPed);
	static const CActionDefinition* FindSuitableActionForTarget(const CPed* pPed, const CEntity* pTargetEntity, bool bHasLockOnTarget, bool bCacheActionDefinition = true);
	static const CActionDefinition* FindSuitableActionForTargetType(const CPed* pPed, const CEntity* pTargetEntity, bool bHasLockOnTarget, const CActionFlags::ActionTypeBitSetData &typeToLookFor, bool bCacheActionDefinition = true);

	static void  ResetLastFoundActionDefinition() { ms_LastFoundActionDefinition = NULL; }
	static const CActionDefinition *GetLastFoundActionDefinition() { return ms_LastFoundActionDefinition; }

	static void SetLastFoundActionDefinitionForNetworkDamage(const CActionDefinition *pDefinition) { ms_LastFoundActionDefinitionForNetworkDamage = pDefinition; }
	static void  ResetLastFoundActionDefinitionForNetworkDamage() { ms_LastFoundActionDefinitionForNetworkDamage = NULL; }
	static const CActionDefinition *GetLastFoundActionDefinitionForNetworkDamage() { return ms_LastFoundActionDefinitionForNetworkDamage; }

	static bool SetupMeleeGetup(CNmBlendOutSetList& sets, CPed* pPed, const CEntity* pTarget);

	static bool VelocityHighEnoughForTagSyncBlendOut(const CPed* pPed);

	static	s32				sm_nMaxNumActiveCombatants;
	static	s32				sm_nMaxNumActiveSupportCombatants;
	static 	s32				sm_nTimeInTaskAfterHitReaction;
	static 	s32				sm_nTimeInTaskAfterStardardAttack;
	static	s32				ms_nCameraInterpolationDuration;

	static const CActionDefinition *ms_LastFoundActionDefinition;
	static const CActionDefinition *ms_LastFoundActionDefinitionForNetworkDamage;

	// Tuning parameters.
	static float			sm_fMeleeStrafingMinDistance;
	static float			sm_fMovingTargetStrafeTriggerRadius;
	static float			sm_fMovingTargetStrafeBreakoutRadius;
	static float			sm_fForcedStrafeRadius;
	static float			sm_fBlockTimerDelayEnter;
	static float			sm_fBlockTimerDelayExit;
	static float			sm_fFrontTargetDotThreshold;
	static bool				sm_bDoSpineIk;
	static bool				sm_bDoHeadIk;
	static bool				sm_bDoEyeIK;
	static bool				sm_bDoLegIK;
	static float			sm_fHeadLookMinYawDegs;
	static float			sm_fHeadLookMaxYawDegs;
	static float			sm_fSpineLookMinYawDegs;
	static float			sm_fSpineLookMaxYawDegs;
	static float			sm_fSpineLookMinPitchDegs;
	static float			sm_fSpineLookMaxPitchDegs;
	static float			sm_fSpineLookTorsoOffsetYawDegs;
	static float			sm_fSpineLookMaxHeightDiff;
	static float			sm_fSpineLookAmountPowerCurve;
	static float			sm_fPlayerAttemptEscapeExitMultiplyer;
	static float			sm_fPlayerMoveAwayExitMultiplyer;
	static float			sm_fPlayerClipPlaybackSpeedMultiplier;
	static s32				sm_nPlayerTauntFrequencyMinInMs;
	static s32				sm_nPlayerTauntFrequencyMaxInMs;
	static u32				sm_nPlayerImpulseInterruptDelayInMs;
	static s32				sm_nAiTauntHitDelayTimerInMs;
	static float			sm_fAiBlockConsecutiveHitBuffIncrease;
	static float			sm_fAiBlockActionBuffDecrease;
	static s32				sm_fAiAttackConsecutiveHitBuffIncreaseMin;
	static s32				sm_fAiAttackConsecutiveHitBuffIncreaseMax;
	static s32				sm_nAiForceMeleeMovementAtStartTimeMs;
	static float			sm_fAiTargetSeparationDesiredSelfToTargetRange;
	static float			sm_fAiTargetSeparationDesiredSelfToGetupTargetRange;
	static float			sm_fAiTargetSeparationPrefdDiffMinForImpetus;
	static float			sm_fAiTargetSeparationPrefdDiffMaxForImpetus;
	static float			sm_fAiTargetSeparationForwardImpetusPowerFactor;
	static float			sm_fAiTargetSeparationBackwardImpetusPowerFactor;
	static float			sm_fAiTargetSeparationMinTargetApproachDesireSize;
	static float			sm_fAiAvoidSeparationDesiredSelfToTargetRange;
	static s32				sm_nAiAvoidEntityDelayMinInMs;
	static s32				sm_nAiAvoidEntityDelayMaxInMs;
	static float			sm_fAiAvoidSeparationPrefdDiffMinForImpetus;
	static float			sm_fAiAvoidSeparationPrefdDiffMaxForImpetus;
	static float			sm_fAiAvoidSeparationForwardImpetusPowerFactor;
	static float			sm_fAiAvoidSeparationBackwardImpetusPowerFactor;
	static float			sm_fAiAvoidSeparationMinTargetApproachDesireSize;
	static float			sm_fAiAvoidSeparationCirlclingStrength;
	static s32				sm_nAiSeekModeScanTimeMin;
	static s32				sm_nAiSeekModeScanTimeMax;
	static float			sm_fAiSeekModeTargetHeight;
	static float			sm_fAiSeekModeInitialPursuitBreakoutDist;
	static float			sm_fAiSeekModeChaseTriggerOffset;
	static float			sm_fAiSeekModeFleeTriggerOffset;
	static float			sm_fAiSeekModeActiveCombatantRadius;
	static float			sm_fAiSeekModeSupportCombatantRadiusMin;
	static float			sm_fAiSeekModeSupportCombatantRadiusMax;
	static float			sm_fAiSeekModeInactiveCombatantRadiusMin;
	static float			sm_fAiSeekModeInactiveCombatantRadiusMax;
	static float			sm_fAiSeekModeRetreatDistanceMin;
	static float			sm_fAiSeekModeRetreatDistanceMax;
	static float			sm_fAiQueuePrefdPosDiffMinForImpetus;
	static float			sm_fAiQueuePrefdPosDiffMaxForImpetus;
	static s32				sm_nAiLosTestFreqInMs;
	static float			sm_fLookAtTargetHeadingBlendoutDegrees;
	static float			sm_fLookAtTargetTimeout;
	static float			sm_fLookAtTargetLooseToleranceDegrees;
	static float			sm_fReTargetTimeIfNotStrafing;
};

class CTaskMeleeUpperbodyAnims : public CTask
{
public:	
	// Task states
	enum UpperbodyClipState
	{
		State_Invalid = -1,
		State_Start,
		State_PlayAnim,
		State_Finish
	};

	// Melee flags
	enum AnimFlags
	{
		AF_IsLooped							= BIT0,
	};

	CTaskMeleeUpperbodyAnims(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, const fwMvFilterId& filterId, float fBlendDuration = NORMAL_BLEND_DURATION, fwFlags32 iFlags = 0);

	virtual aiTask* Copy() const { return rage_new CTaskMeleeUpperbodyAnims( m_ClipSetId, m_ClipId, m_FilterId, m_fBlendDuration, m_nFlags ); }

#if !__FINAL
	static const char*	GetStaticStateName						(s32 iState);
#endif


protected:

	// FSM implementations
	virtual int			GetTaskTypeInternal								(void) const { return CTaskTypes::TASK_MELEE_UPPERBODY_ANIM; }
	virtual	void		CleanUp									(void);
	virtual FSM_Return	UpdateFSM								(const s32 iState, const FSM_Event iEvent);
	virtual s32			GetDefaultStateAfterAbort				(void)	const	{ return State_Finish; }
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper		(void)	const	{ return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper			(void)			{ return &m_MoveNetworkHelper; }

	// FSM state implementations
	// Start
	FSM_Return			Start_OnUpdate							(CPed* pPed);

	// PlayAnim
	void				PlayAnim_OnEnter						(CPed* pPed);
	FSM_Return			PlayAnim_OnUpdate						(void);

	// MoVE 
	bool				PrepareMoveNetwork						(CPed* pPed);

	// MoVE
	CMoveNetworkHelper				m_MoveNetworkHelper;
	static const fwMvClipId			ms_UpperbodyClipId;
	static const fwMvFilterId		ms_UpperbodyFilterId;
	static const fwMvBooleanId		ms_IsClipLoopedId;
	static const fwMvBooleanId		ms_AnimClipFinishedId;
	// Melee Grips
	static const fwMvFlagId			ms_HasGrip;
	static const fwMvClipSetVarId	ms_GripClipSetId;

private:
	fwMvClipSetId					m_ClipSetId;
	fwMvClipId						m_ClipId;
	fwMvFilterId					m_FilterId;
	crFrameFilterMultiWeight*		m_pFilter;
	float							m_fBlendDuration;
	fwFlags32						m_nFlags;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Enables individual peds to move around while in combat.  This
// is usually running underneath any action result tasks.  The action moves
// always have higher priority and override the clip being provided by
// this.
/////////////////////////////////////////////////////////////////////////////////

class CTaskMoveMeleeMovement : public CTaskMove
{
public:// Static members.

	// States
	enum
	{
		Running
	};

public:// Instance members.

	CTaskMoveMeleeMovement									(CEntity* pTargetEntity, bool bHasLockOnTargetEntity, bool bAllowMovement, bool bAllowStrafing, bool bAllowHeadingUpdate, bool bAllowStealth );

	virtual aiTask*		Copy								(void) const;
	virtual int			GetTaskTypeInternal							(void) const {return CTaskTypes::TASK_MOVE_MELEE_MOVEMENT;}


#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "Running";  };
#endif

	virtual bool		HasTarget							(void) const { return false; }
	virtual Vector3		GetTarget							(void) const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float		GetTargetRadius						(void) const;

	virtual s32			GetDefaultStateAfterAbort			(void) const { return Running; }

	virtual FSM_Return	UpdateFSM							(const s32 iState, const FSM_Event iEvent);

	void				StateRunning_OnEnter				(CPed* pPed);
	FSM_Return			StateRunning_OnUpdate				(CPed* pPed);

	void				SetTargetEntity						(CEntity* pTargetEntity, bool bHasLockOnTargetEntity);
	CEntity*			GetTargetEntity						(void) const { return m_pTargetEntity; }
	bool				HasLockOnTargetEntity				(void) const { return m_bHasLockOnTargetEntity; }
	void				SetTargetCurrentDesiredRange		(float desiredRange) { m_fTargetSeparationDesiredRange = desiredRange; }
	float				GetTargetCurrentDesiredRange		(void) const { return m_fTargetSeparationDesiredRange; }
	void				SetTargetCurrentMinRangeForImpetus	(float minRangeForImpetus) { m_fTargetSeparationMinRangeForImpetus = minRangeForImpetus; }
	float				GetTargetCurrentMinRangeForImpetus	(void) const { return m_fTargetSeparationMinRangeForImpetus; }
	void				SetTargetCurrentMaxRangeForImpetus	(float maxRangeForImpetus) { m_fTargetSeparationMaxRangeForImpetus = maxRangeForImpetus; }
	float				GetTargetCurrentMaxRangeForImpetus	(void) const { return m_fTargetSeparationMaxRangeForImpetus; }

	void				SetAvoidEntity						(CEntity* pAvoidEntity) { m_pAvoidEntity = pAvoidEntity; }
	CEntity*			GetAvoidEntity						(void) const { return m_pAvoidEntity; }
	void				SetAvoidEntityDesiredRange			(float desiredRange) { m_fAvoidEntityDesiredRange = desiredRange; }
	float				GetAvoidEntityDesiredRange			(void) const { return m_fAvoidEntityDesiredRange; }
	void				SetAvoidEntityMinRangeForImpetus	(float minRangeForImpetus) { m_fAvoidEntityMinRangeForImpetus = minRangeForImpetus; }
	float				GetAvoidEntityMinRangeForImpetus	(void) const { return m_fAvoidEntityMinRangeForImpetus; }
	void				SetAvoidEntityMaxRangeForImpetus	(float maxRangeForImpetus) { m_fAvoidEntityMaxRangeForImpetus = maxRangeForImpetus; }
	float				GetAvoidEntityMaxRangeForImpetus	(void) const { return m_fAvoidEntityMaxRangeForImpetus; }

	bool				IsDoingSomething					(void) const { return m_bIsDoingSomething; }

	bool				ShouldAllowMovement					(void) const { return m_bAllowMovement; }
	void				SetAllowMovement					(bool val)	{ m_bAllowMovement = val; }
	bool				ShouldAllowMovementInput			(void) const { return m_bAllowMovementInput; }
	void				SetAllowMovementInput				(bool val)	{ m_bAllowMovementInput = val; }
	bool				ShouldAllowStrafing					(void) const { return m_bAllowStrafing; }
	void				SetAllowStrafing					(bool val)	{ m_bAllowStrafing = val; }
	bool				ShouldAllowHeadingUpdate			(void) const { return m_bAllowHeadingUpdate; }
	void				SetAllowHeadingUpdate				(bool val)	{ m_bAllowHeadingUpdate = val; }
	bool				ShouldAllowStealth					(void) const { return m_bAllowStealth; }
	void				SetAllowStealth						(bool val)	{ m_bAllowStealth = val; }
	void				SetForceRun							(bool val)	{ m_bForceRun = val; }
			

	void				SetAppliedPedMoveMomentum			(Vector3& dir, float pedMoveMomentumPortion);
	void				UpdateAppliedPedMoveMomentum		(void);
	void				AppliedPedMoveMomentumMoveBlendTweak(CPed* pPed, float* fMoveBlendRatio, float* fLateralMoveBlendRatio);

	bool				CanMoveWhenTargetEnteringVehicle	(void) const { return m_bCanMoveWhenTargetEnteringVehicle; }
	void				SetMoveWhenTargetEnteringVehicle	(bool val)	{ m_bCanMoveWhenTargetEnteringVehicle = val; }

protected:
	void				ProcessStdMove						(CPed* pPlayerPed);
	void				ProcessStrafeMove					(CPed* pPlayerPed);
	void				ProcessStealthMode					(CPed* pPlayerPed);
	void				GetPrefdDistanceMoveComponents(		float& forwardTargetBasedMoveComponentOut,
															float& rightwardTargetBasedMoveComponentOut,
															Vec3V_In vSelf,
															Vec3V_In vForwardHeading,
															Vec3V_In vRightwardHeading,
															Vec3V_In targetCentre,
															const float currentDesiredRange,
															const bool	avoidOnly,
															const float currentMinRangeForImpetus,
															const float currentMaxRangeForImpetus,
															const float distancingForwardImpetusPowerFactor,
															const float distancingBackwardImpetusPowerFactor,
															const float distancingMinTargetApproachDesireSize, 
															const CDefensiveArea* pDefensiveArea);

	RegdEnt				m_pTargetEntity;
	float				m_fTargetSeparationDesiredRange;
	float				m_fTargetSeparationMinRangeForImpetus;
	float				m_fTargetSeparationMaxRangeForImpetus;
	bool				m_bHasLockOnTargetEntity : 1;

	RegdEnt				m_pAvoidEntity;
	float				m_fAvoidEntityDesiredRange;
	float				m_fAvoidEntityMinRangeForImpetus;
	float				m_fAvoidEntityMaxRangeForImpetus;

	Vector3				m_vPedMoveMomentumDir;
	float				m_fPedMoveMomentumPortion;
	float				m_fPedMoveMomentumTime;

	float				m_fStickAngle;
	bool				m_bStickAngleTweaked;

	u32					m_uStealthLastPressed;

	bool				m_bIsStrafing : 1;
	bool				m_bIsDoingSomething : 1;
	bool				m_bAllowMovement : 1;
	bool				m_bAllowMovementInput : 1;
	bool				m_bAllowStrafing: 1;
	bool				m_bAllowHeadingUpdate : 1;
	bool				m_bAllowStealth : 1;
	bool				m_bForceRun : 1;
	bool				m_bCanMoveWhenTargetEnteringVehicle : 1;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Enables individual peds to do a single chain-able action result.
/////////////////////////////////////////////////////////////////////////////////
class CTaskMeleeActionResult : public CTaskFSMClone
{
	friend class CClonedTaskMeleeResultInfo;

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_ActionModeTime;
		float m_ForceRunDelayTime;
		u32 m_MeleeEnduranceDamage;

		PAR_PARSABLE;
	};
	static Tunables	sm_Tunables;

	enum MeleeState
	{
		State_Invalid = -1,
		State_Start = 0,
		State_PlayClip,
		State_Finish
	};

	CTaskMeleeActionResult(const CActionResult* pActionResult, 
		CEntity* pTargetEntity, 
		bool bHasLockOnTargetEntity, 
		bool bIsDoingReaction, 
		bool bShouldSuppressHoming, 
		u32 nSelfDamageWeaponHash = WEAPONTYPE_UNARMED, 
		eAnimBoneTag eSelfDamageAnimBoneTag = BONETAG_INVALID, 
		float fMoveBlendRatioSq = 1.0f );

	static void		Shutdown						();

	virtual aiTask*	Copy							() const;

	virtual int		GetTaskTypeInternal				() const {return CTaskTypes::TASK_MELEE_ACTION_RESULT;}

	bool			PrepareMoveNetwork				(CPed* pPed);

	// FSM implementations
	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void			CleanUp				(void);
	virtual FSM_Return		ProcessPreFSM		(void);
	virtual	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	virtual s32		GetDefaultStateAfterAbort()	const {return State_Finish;}

	void			CacheActionResult(CPed* pPed);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName			(s32 iState);
	virtual const char * GetStateName				(s32 iState ) const;
	const float			GetCurrentPhase				() const;
#endif
	virtual bool	ProcessPostPreRender			();

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent );

	// ActionResult specific 
	const CActionResult*	GetActionResult			(void) const { return m_pActionResult; }
	const crClip*	GetClip							(void) const { return fwClipSetManager::GetClip( m_clipSetId, m_clipId ); }
	u32				GetResultId						(void) const { return m_nResultId; }
	u32				GetResultPriority				(void) const { return m_nResultPriority; }
	u32				GetSelfDamageWeaponHash			(void) const { return m_nSelfDamageWeaponHash; }
	eAnimBoneTag	GetSelfDamageAnimBoneTag		(void) const { return m_eSelfDamageAnimBoneTag; }
	float			GetCurrentClipTime				(void) const;
	bool			AllowMovement					(void) const;
	bool			AllowStrafing					(void) const;
	bool			AllowHeadingUpdate				(void) const;
	bool			AllowStealthMode				(void) const;
	bool			IsOffensiveMove					(void) const { return m_bOffensiveMove; };

	void			SetTargetEntity					(CEntity* pTargetEntity, bool bHasLockOnTargetEntity);
	CEntity*		GetTargetEntity					(void) const { return m_pTargetEntity; }
	bool			HasLockOnTargetEntity			(void) const { return m_bHasLockOnTargetEntity; }

	Vec3V_Out		GetCachedHeadIkOffset			(void) const { return m_vCachedHeadIkOffset; }
	void			StopDistanceHoming				(void) { m_bAllowDistanceHoming = false; }
	bool			ShouldForceHomingArrival		(void) const { return m_bForceHomingArrival; }
	void			SetForceHomingArrival			(bool bForceHomingArrival) { m_bForceHomingArrival = bForceHomingArrival; }
	float			GetInitialAnimPhase				(void) const { return m_fInitialPhase; }
	float			GetCriticalFrame				(void) const { return m_fCriticalFrame; }
	float			GetCachedMoveBlendRatioSq		(void) const { return m_fCachedMoveBlendRatioSq; }

	bool 			ShouldBranchOnCurrentFrame		(const CPed* pPed, 
													 const CImpulseCombo* pImpulseCombo, 
													 u32* pActionIdOut, 
													 bool bImpulseInitiatedMovesOnly, 
													 const CActionFlags::ActionTypeBitSetData &typeToLookFor,
													 u32 nForcingResultId);

	bool			IsDoingSomething				(void) const { return ( m_pActionResult != NULL ); }
	bool			IsDoingIntro					(void) const { return m_bIsDoingIntro; }
	bool			IsDoingTaunt					(void) const { return m_bIsDoingTaunt; }
	bool			IsDazed							(void) const { return m_bIsDazed; }
	bool			IsUpperbodyOnly					(void) const { return m_bIsUpperbodyOnly; }
	bool			IsDoingBlock					(void) const { return m_bIsDoingBlock; }
	bool			IsDoingReaction					(void) const { return m_bIsDoingReaction; }
	bool			IsAStandardAttack				(void) const { return m_bIsStandardAttack; }
	bool			IsNonMeleeHitReaction			(void) const { return m_bIsNonMeleeHitReaction; }
	bool			IsPastCriticalFrame				(void) const { return m_bPastCriticalFrame; }
	bool			HasJustStruckSomething			(void) const { return m_bHasJustStruckSomething; }
	bool			ShouldAllowHoming				(void) const { return m_bAllowHoming; }
	bool			ShouldAllowDistanceHoming		(void) const { return m_bAllowDistanceHoming && ShouldAllowHoming(); }
	bool			DidAllowDistanceHoming			(void) const { return m_bDidAllowDistanceHoming; }
	bool			ShouldAllowInterrupt			(void) const { return m_bAllowInterrupt; }
	bool			ShouldAllowDodge				(void) const { return m_bAllowDodge; }
	bool			ShouldAllowAimInterrupt			(void) const { return m_bAllowAimInterrupt; }
	bool			ShouldProcessMeleeCollisions	(void) const { return m_bProcessCollisions; }
	bool			HasStartedProcessingCollisions	(void) const { return m_bHasStartedProcessingCollisions; }
	bool			IsInvulnerableToMelee			(void) const { return m_bMeleeInvulnerability; }
	bool			ShouldFireWeapon				(void) const { return m_bFireWeapon; }
	bool			ShouldSwitchToRagdoll			(void) const { return m_bSwitchToRagdoll; }	
	bool			ShouldForceDamage				(void) const { return m_bForceDamage; }
	bool			ShouldIgnoreArmor				(void) const { return m_bIgnoreArmor; }
	bool			ShouldIgnoreStatModifiers		(void) const { return m_bIgnoreStatModifiers; }
	bool			ShouldInterruptWhenOutOfWater	(void) const { return m_bInterruptWhenOutOfWater; }
	bool			ShouldAllowScriptTaskAbort		(void) const { return m_bAllowScriptTaskAbort; }
	bool			ShouldAllowPlayerToEarlyOut		(void) const { return m_pActionResult && m_pActionResult->GetShouldAllowPlayerToEarlyOut(); }
	bool			ShouldDamagePeds				(void) const { return m_bShouldDamagePeds; }

	// Network clones aren't allowed to damage themselves
	void			OverrideSelfDamageAmount		(float fSelfDamageAmount) {m_fSelfDamageAmount = fSelfDamageAmount;}

#if __BANK
	// Action table reload support.
	void			MakeCurrentResultGoStale		(void) { m_pActionResult = NULL; }

	struct sDebugTestCapsule
	{
		sDebugTestCapsule() {}
		sDebugTestCapsule(float fRadius, const Vector3 &vStart, const Vector3 &vEnd)
			: fCapsuleRadius(fRadius)
			, vCapsuleStart(vStart)
			, vCapsuleEnd(vEnd)
		{

		}
		~sDebugTestCapsule() {};

		float			fCapsuleRadius;
		Vector3			vCapsuleStart;
		Vector3			vCapsuleEnd;
	};

#endif // __BANK

	static void		HitImpulseCalculation			(WorldProbe::CShapeTestHitPoint* pResult, float fImpulseMult, Vector3& vecImpulseDir, float& fImpulseMag, CPed *hitPed);
	static bool		DoPhaseWindowsIntersect			(float fCurrentPhase, float fNextPhase, float fStartPhase, float fEndPhase);

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed*);
	virtual bool				OverridesNetworkHeadingBlender(CPed*);
	bool						HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* pTaskInfo);
	virtual bool				AllowsLocalHitReactions() const;
	bool						IsOverridingNetBlenderCommon(CPed *pPed);
	
	void						SetForcedReactionActionDefinition(const CActionDefinition *pDefinition) { m_pForcedReactionActionDefinition = pDefinition; } 
	void						ProcessPostHitResults(CPed* pPed, CPed* pTargetPed, bool bForce, Vec3V_In vRagdollImpulseDir, float fRagdollImpulseMag);

	void						FixupKillMoveOnMigration();

	u16							GetUniqueNetworkActionID() const { return m_nUniqueNetworkActionID; }
	void						SetUniqueNetworkActionID(u16 uID) { m_nUniqueNetworkActionID = uID; }

	u32							GetNetworkTimeTriggered() const { return m_nNetworkTimeTriggered; }

	bool						HandlesRagdoll(const CPed* pPed) const;

	void						AddToHitEntityCount(CEntity* pEntity, s32 component);
	bool						EntityAlreadyHitByResult		(const CEntity* pHitEntity) const;

	// Cloud tunables
	static void InitTunables();

	static u32 GetMeleeEnduranceDamage();

protected:
	void			ProcessCachedNetworkDamage		(CPed* pPed);
	void			ProcessSelfDamage				(CPed* pPed, bool bForceFatalDamage = false);
	void			ProcessMeleeClipEvents			(CPed* pPed);
	void			ProcessVfxClipTag				(CPed* pPed, const crClip* pClip, const crTag* pTag);
	void			ProcessFacialClipTag			(CPed* pPed, const crClip* pClip, const crTag* pTag);
	bool			ProcessWeaponDisarmsAndDrops	(CPed* pPed);
	void			ProcessPreHitResults			(CPed* pPed, CEntity* pTargetEntity);
	void			ProcessHomingAndSeparation		(CPed* pPed, float fTimeStep, int nTimeSlice);
	void			ProcessMeleeCollisions			(CPed* pPed);
	bool			HasActiveMeleeCollision			(CPed* pPed) const;
	phBound*		GetCustomHitTestBound			(CPed* pPed);
	void			TriggerHitReaction				(CPed* pPed, CEntity* pHitEnt, sCollisionInfo& refResults, bool bEntityAlreadyHit, bool bBreakableObject);
	void			DropWeaponAndCreatePickup		(CPed* pPed);

	bool ShouldApplyEnduranceDamageOnly(const CPed* pPed, const CPed* pTargetPed) const;

	// FSM state implementations
	// Start
	void			Start_OnEnter				(CPed* pPed);
	FSM_Return		Start_OnUpdate				(CPed* pPed);

	// PlayClip
	void			PlayClip_OnEnter			(CPed* pPed);
	FSM_Return		PlayClip_OnUpdate			(CPed* pPed);
	void			PlayClip_OnExit				(CPed* pPed);

	void			Finish_OnEnter				(CPed* pPed);

	void			InitHoming(CPed *pPed, CEntity* pTargetEntity);

	virtual bool	ProcessPhysics(float fTimeStep, int nTimeSlice);

	// MoVE
	void			CheckForForcedReaction		(CPed* pPed);
	bool			CanCloneQuitForcedReaction(CPed *pTargetPed) const;
	float			ComputeInitialAnimPhase		(const crClip* pClip);
	CHoming *		GetHoming(const CPed *pPed);

	CMoveNetworkHelper				m_MoveNetworkHelper;

	static const fwMvClipId			ms_ClipId;
	static const fwMvClipId			ms_AdditiveClipId;
	static const fwMvFilterId		ms_ClipFilterId;
	static const fwMvRequestId		ms_PlayClipRequestId;
	static const fwMvRequestId		ms_RestartPlayClipRequestId;
	static const fwMvFloatId		ms_ClipPhaseId;
	static const fwMvFloatId		ms_ClipRateId;
	static const fwMvFloatId		ms_IkShadowWeightId;
	static const fwMvBooleanId		ms_AnimClipFinishedId;
	static const fwMvFlagId			ms_UseAdditiveClipId;
	static const fwMvBooleanId		ms_DropWeaponOnDeath;
	// Melee Grips
	static const fwMvFlagId			ms_HasGrip;
	static const fwMvClipSetVarId	ms_GripClipSetId;

	crFrameFilter*					m_pClipFilter; 

	// Main pointer to action result information.
	const CActionResult*			m_pActionResult;
	u32								m_nActionResultNetworkID;

	//! Network Vars.
	u32								m_nNetworkTimeTriggered;
	u16								m_nUniqueNetworkActionID;
	u32								m_nCloneKillAnimFinishedTime;

	//! Replicate forced reaction, so that they are consistent across machines.
	const CActionDefinition*		m_pForcedReactionActionDefinition;

	// Our current target.
	RegdEnt							m_pTargetEntity;

	// Homing
	Vec3V							m_vCachedHomingPosition;
	Vec3V							m_vCachedHomingDirection;
	Vec3V							m_vCachedHeadIkOffset;
	Vec3V							m_vMoverDisplacement;
	Vec3V							m_vCachedHomingPedPosition;
	Vec3V							m_vCloneCachedHomingPosition;
	// Weapon matrix cache
	Matrix34						m_matLastKnownWeapon;

	// Local versions of information for convenience (and so we can
	// dynamically reload the action table).
	fwMvClipSetId					m_clipSetId;
	fwMvClipId						m_clipId;
	float							m_fInitialPhase;
	float							m_fCriticalFrame;
	float							m_fSelfDamageAmount;
	float							m_fCachedMoveBlendRatioSq;
	u32								m_nResultId;
	u32								m_nResultPriority;
	u32								m_nSoundNameHash;
	u32								m_nSelfDamageWeaponHash;
	eAnimBoneTag					m_eSelfDamageAnimBoneTag;
	int								m_nBoneIdxCache;			// Bone cache index (allows us to store off a bone index rather than evaluate every frame)
	

	BANK_ONLY (const char*			m_pszSoundName);
	bool							m_bForceImmediateReaction : 1;
	bool							m_bIsDoingIntro: 1;
	bool							m_bIsDoingTaunt: 1;
	bool							m_bIsDazed : 1;
	bool							m_bIsUpperbodyOnly : 1;
	bool							m_bIsDoingBlock : 1;
	bool							m_bDropWeapon : 1;
	bool							m_bDisarmWeapon : 1;
	bool							m_bIsStandardAttack: 1;
	bool							m_bForceDamage : 1;
	bool							m_bIgnoreArmor : 1;
	bool							m_bIgnoreStatModifiers : 1;
	bool							m_bForceHomingArrival : 1;
	bool							m_bInterruptWhenOutOfWater : 1;
	bool							m_bAllowScriptTaskAbort : 1;
	bool							m_bShouldDamagePeds : 1;

	// Additional State Data.
	bool							m_bIsFirstProcessingFrame : 1;
	bool							m_bIsDoingReaction : 1;
	bool							m_bIsNonMeleeHitReaction : 1;
	bool							m_bSelfDamageApplied : 1;
	bool							m_bPreHitResultsApplied : 1;
	bool							m_bPostHitResultsApplied : 1;
	bool							m_bAllowHoming : 1;
	bool							m_bAllowDistanceHoming : 1;
	bool							m_bDidAllowDistanceHoming : 1;
	bool							m_bOwnerStoppedDistanceHoming : 1;
	bool							m_bAllowDodge : 1;
	bool							m_bHasJustStruckSomething : 1;
	bool							m_bHasTriggerMeleeSwipeAudio : 1;
	bool							m_bAllowAimInterrupt : 1;
	bool							m_bHeadingInitialized : 1;
	bool							m_bOffensiveMove : 1;

	// Clip events
	bool							m_bPastCriticalFrame : 1;

	bool							m_bAllowInterrupt : 1;
	bool							m_bProcessCollisions : 1;
	bool							m_bHasStartedProcessingCollisions : 1;
	bool							m_bMeleeInvulnerability : 1;
	bool							m_bFireWeapon : 1;
	bool							m_bSwitchToRagdoll : 1;

	bool							m_bFixupActionResultFromNetwork : 1;
	bool							m_bCloneKillAnimFinished : 1;
	bool							m_bLastKnowWeaponMatCached : 1;
	bool							m_bHasLockOnTargetEntity : 1;

	bool							m_bPlaysClip : 1;

	bool							m_bPickupDropped : 1;

	bool							m_bHandledLocalToRemoteSwitch : 1;
	bool							m_bInNetworkBlenderTolerance : 1;

	class CEntityComponent
	{
	public:
		CEntityComponent() : m_pEntity(NULL), m_component(-1){;}
		CEntityComponent(CEntity* pEntity, s32 element) : m_pEntity(pEntity), m_component(element){;}
		CEntity*	m_pEntity;
		s32		m_component;
	};
	u32								m_nHitEntityComponentsCount;
	static const unsigned int		sm_nMaxHitEntityComponents = 8;
	CEntityComponent				m_hitEntityComponents[sm_nMaxHitEntityComponents];

	static phBoundComposite*		sm_pTestBound;

public:

	struct MeleeNetworkDamage
	{
		MeleeNetworkDamage() 
			: m_pHitPed(NULL)
			, m_pInflictorPed(NULL)
			, m_vWorldHitPos(Vector3::ZeroType)
			, m_iComponent(-1)
			, m_fDamage(0.0f)
			, m_uFlags(0)
			, m_uNetworkTime(0)
			, m_uParentMeleeActionResultID(0)
			, m_uForcedReactionDefinitionID(0)
			, m_uNetworkActionID(0)
		{}

		void Reset() { m_pHitPed = NULL; } // quick reset - set to no hit ped.

		RegdPed	m_pHitPed;
		RegdPed	m_pInflictorPed;
		Vector3	m_vWorldHitPos;
		float m_fDamage;
		s32 m_iComponent;
		u32 m_uFlags;
		u32 m_uNetworkTime;
		u32 m_uParentMeleeActionResultID;
		u32 m_uForcedReactionDefinitionID;
		u16 m_uNetworkActionID;
	};

	static void CacheMeleeNetworkDamage(CPed *pHitPed,
		CPed *pFiringEntity,
		const Vector3 &vWorldHitPos,
		s32 iComponent,
		float fAppliedDamage, 
		u32 flags, 
		u32 uParentMeleeActionResultID,
		u32 uForcedReactionDefinitionID,
		u16 uNetworkActionID);

	MeleeNetworkDamage *FindMeleeNetworkDamageForHitPed(CPed *pHitPed);

private:

	static u8 sm_uMeleeDamageCounter;
	static const int s_MaxCachedMeleeDamagePackets = 10;
	static MeleeNetworkDamage sm_CachedMeleeDamage[s_MaxCachedMeleeDamagePackets];

	// Cloud tunables
	static float			sm_fMeleeRightFistMultiplier;
	static s32				sm_nMeleeEnduranceDamage;
};

///////////////
// Uninterruptable melee task that will re-orient ped
///////////////

class CTaskMeleeUninterruptable : public CTask
{
public:
	CTaskMeleeUninterruptable() { SetInternalTaskType(CTaskTypes::TASK_MELEE_UNINTERRUPTABLE);}
	~CTaskMeleeUninterruptable() {}

	virtual aiTask* Copy() const {return rage_new CTaskMeleeUninterruptable();}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MELEE_UNINTERRUPTABLE;}
	virtual s32 GetDefaultStateAfterAbort() const { return GetState(); }
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* ) 
	{
		if(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority)
			return true;
		return false;
	}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "No state";  };
#endif
	virtual FSM_Return UpdateFSM(const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent))
	{
		return FSM_Continue;
	}
};

static const unsigned int SIZEOF_ACTIONRESULT_ID = 32;

class CClonedTaskMeleeInfo : public CSerialisedFSMTaskInfo
{
	friend class CTaskMelee;

public:

	CClonedTaskMeleeInfo();
	CClonedTaskMeleeInfo(u32 iActionResultID, 
		CEntity* pTarget, 
		u32 iFlags, 
		s32 iState, 
		s32 nQueueType, 
		bool bBlocking,
		bool bInFP);
	~CClonedTaskMeleeInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_MELEE; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskMelee::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Melee State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_UNSIGNED(serialiser, m_nActionResultID, SIZEOF_ACTIONRESULT_ID, "Action Result ID");
		SERIALISE_UNSIGNED(serialiser, m_nFlags, SIZEOF_FLAGS, "Task Melee Flags");
		SERIALISE_SYNCEDENTITY(m_pTarget, serialiser, "Target Entity");

		const unsigned int SIZEOF_MELEE_QUEUE_TYPE = 5;
		SERIALISE_INTEGER(serialiser, m_nQueueType, SIZEOF_MELEE_QUEUE_TYPE, "Queuing for target");
		SERIALISE_BOOL(serialiser, m_bBlocking, "Blocking");
		SERIALISE_BOOL(serialiser, m_bInFP, "In first person");
	}

private:

	static const unsigned int SIZEOF_FLAGS = 32;
	u32				m_nActionResultID;
	CSyncedEntity	m_pTarget;
	u32				m_nFlags;
	int				m_nQueueType;
	bool			m_bBlocking;
	bool			m_bInFP;
};

class CClonedTaskMeleeResultInfo : public CSerialisedFSMTaskInfo
{
	friend class CTaskMeleeActionResult;
	friend class CTaskMelee;

public:

	CClonedTaskMeleeResultInfo();
	CClonedTaskMeleeResultInfo( u32 uActionResultID, 
							    u32 uForcedReactionActionDefinitionID,
							    CEntity* pTarget, 
							    bool bLockOn, 
							    s32 iState, 
							    bool bIsDoingReaction, 
							    bool bAllowDistanceHoming,
								bool bOwnerStoppedDistanceHoming,
								u32 uSelfDamageWeaponHash,
								s32 nSelfDamageAnimBoneTag,
								u32 uNetworkTimeTriggered,
								u16 uUniqueID,
								const Vector3 &vHomingPosition);
	~CClonedTaskMeleeResultInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_MELEE_ACTION_RESULT; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskMeleeActionResult::State_Finish>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Melee Result State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_UNSIGNED(serialiser, m_uActionResultID, SIZEOF_ACTIONRESULT_ID, "Action Result ID");
		SERIALISE_UNSIGNED(serialiser, m_uForcedReactionActionDefinitionID, SIZEOF_ACTIONRESULT_ID, "Forced Reaction Action Def ID");
		SERIALISE_UNSIGNED(serialiser, m_uSelfDamageWeaponHash, SIZEOF_ACTIONRESULT_ID, "Self damage weapon hash");
		SERIALISE_INTEGER(serialiser, m_nSelfDamageAnimBoneTag, SIZEOF_ACTIONRESULT_ID, "Self damage anim bone tag");
		SERIALISE_UNSIGNED(serialiser, m_uActionID, 16, "Action ID");
		SERIALISE_UNSIGNED(serialiser, m_uNetworkTimeTriggered, 32, "Trigger Time");
		SERIALISE_BOOL(serialiser, m_bIsDoingReaction, "Doing Reaction");
		SERIALISE_BOOL(serialiser, m_bAllowDistanceHoming, "Allow Dist Homing");
		SERIALISE_BOOL(serialiser, m_bOwnerStoppedDistanceHoming, "Owner Stopped Homing");
		SERIALISE_BOOL(serialiser, m_bHasLockOnTargetEntity, "Has Lock On");
		SERIALISE_SYNCEDENTITY(m_pTarget, serialiser, "Target Entity");
		SERIALISE_POSITION(serialiser, m_vCachedHomingPosition, "Homing Position");
	}

private:
	u32				m_uActionResultID;
	u32				m_uForcedReactionActionDefinitionID;
	u32				m_uSelfDamageWeaponHash;
	s32				m_nSelfDamageAnimBoneTag;
	CSyncedEntity	m_pTarget;
	bool			m_bHasLockOnTargetEntity;
	bool			m_bIsDoingReaction;
	bool			m_bAllowDistanceHoming;
	bool			m_bOwnerStoppedDistanceHoming;
	u16				m_uActionID;
	u32				m_uNetworkTimeTriggered;
	Vector3			m_vCachedHomingPosition;

public:
	static u8		m_nActionIDCounter;
};

#endif // INC_TASK_COMBAT_MELEE_H
