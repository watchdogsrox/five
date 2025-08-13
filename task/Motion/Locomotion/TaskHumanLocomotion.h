#ifndef TASK_HUMAN_LOCOMOTION_H
#define TASK_HUMAN_LOCOMOTION_H

// Game headers
#include "Peds/Ped.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CTaskMoveGoToPointAndStandStill;

//*********************************************************************
//	CTaskHumanLocomotion
//	The basic movement for a human, which runs a MoVE network consisting
//	of walk/run/sprint clips.
//*********************************************************************

class CTaskHumanLocomotion : public CTaskMotionBase
{
public:	

	static dev_float BLEND_TO_IDLE_TIME;

	static const CTaskMoveGoToPointAndStandStill* GetGotoPointAndStandStillTask(const CPed* pPed);

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float Player_MBRAcceleration;
		float Player_MBRDeceleration;
		float AI_MBRAcceleration;
		float AI_MBRDeceleration;
		float FromStrafeAccelerationMod;
		float FastWalkRateMin;
		float FastWalkRateMax;
		float SlowRunRateMin;
		float SlowRunRateMax;
		float FastRunRateMin;
		float FastRunRateMax;
		float Turn180ActivationAngle;
		float Turn180ConsistentAngleTolerance;
		float IdleHeadingLerpRate;
		float Player_IdleTurnRate;
		float AI_IdleTurnRate;
		float FromStrafe_WeightRate;
		float FromStrafe_MovingBlendOutTime;
		float IdleTransitionBlendTimeFromActionMode;
		float IdleTransitionBlendTimeFromStealth;

		enum MovingVarsSetTypes
		{
			MVST_Normal,
			MVST_Action,
			MVST_Stealth,
			MVST_Max,
		};

		struct sMovingVars
		{
			float MovingDirectionSmoothingAngleMin;
			float MovingDirectionSmoothingAngleMax;
			float MovingDirectionSmoothingRateMin;
			float MovingDirectionSmoothingRateMaxWalk;
			float MovingDirectionSmoothingRateMaxRun;
			float MovingDirectionSmoothingRateAccelerationMin;
			float MovingDirectionSmoothingRateAccelerationMax;
			float MovingDirectionSmoothingForwardAngleWalk;
			float MovingDirectionSmoothingForwardAngleRun;
			float MovingDirectionSmoothingForwardRateMod;
			float MovingDirectionSmoothingForwardRateAccelerationMod;
			float MovingExtraHeadingChangeAngleMin;
			float MovingExtraHeadingChangeAngleMax;
			float MovingExtraHeadingChangeRateWalk;
			float MovingExtraHeadingChangeRateRun;
			float MovingExtraHeadingChangeRateAccelerationMin;
			float MovingExtraHeadingChangeRateAccelerationMax;
			bool UseExtraHeading;
			bool UseMovingDirectionDiff;
			PAR_SIMPLE_PARSABLE;
		};

		struct sMovingVarsSet
		{
			atHashWithStringNotFinal Name;
			sMovingVars Standard;
			sMovingVars StandardAI;
			sMovingVars TighterTurn;
			PAR_SIMPLE_PARSABLE;
		};

		sMovingVarsSet MovingVarsSet[MVST_Max];

		PAR_PARSABLE;
	};

	enum State
	{
		State_Initial,
		State_Idle,
		State_IdleTurn,
		State_Start,
		State_Moving,
		State_Turn180,
		State_Stop,
		State_RepositionMove,
		State_FromStrafe,
		State_CustomIdle
	};

	struct LadderScanInfoCache
	{
		LadderScanInfoCache()
		{
			Clear();
		}

		void Clear(void)
		{
			m_playerPos.Zero();

			m_ladder = NULL;
			m_effectIndex = -1;
			m_vTop.Zero();
			m_vBottom.Zero();
			m_vNormal.Zero();
			m_vGetOnPosition.Zero();
			m_bCanGetOffAtTop		= false;
			m_bGettingOnAtBottom	= false;
		}

		Vector3		m_playerPos;		
		CEntity*	m_ladder;			
		s32			m_effectIndex;		
		Vector3		m_vTop;
		Vector3		m_vBottom;
		Vector3		m_vNormal;
		Vector3		m_vGetOnPosition;
		bool		m_bCanGetOffAtTop;
		bool		m_bGettingOnAtBottom:1;
		bool		m_bForceSlowDown:1;		// once we've decided to slow down, don't speed back up for any reason so it doesn't look silly...
	};

	CTaskHumanLocomotion(State initialState, const fwMvClipSetId& clipSetId, const bool bIsCrouching, const bool bFromStrafe, const bool bLastFootLeft, const bool bUseLeftFootStrafeTransition, const bool bUseActionModeRateOverride, const fwMvClipSetId& baseModelClipSetId, const float fInitialMovingDirection);
	virtual ~CTaskHumanLocomotion();

	//
	// aiTask API
	//

	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_HUMAN_LOCOMOTION; }
	virtual aiTask*	Copy() const { return rage_new CTaskHumanLocomotion((State)GetState(), m_ClipSetId, m_Flags.IsFlagSet(Flag_Crouching), m_Flags.IsFlagSet(Flag_FromStrafe), m_Flags.IsFlagSet(Flag_StartsOnLeftFoot), m_Flags.IsFlagSet(Flag_UseLeftFootStrafeTransition), m_pActionModeRateData ? true : false, m_pActionModeRateData ? m_pActionModeRateData->BaseModelClipSetId : CLIP_SET_ID_INVALID, m_fInitialMovingDirection); }

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState);
#endif // !__FINAL

	//
	// CTask API
	//

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);
	virtual bool ProcessPostCamera();
	virtual bool ProcessMoveSignals();
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	//
	// Motion Base API
	//

	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state);
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const;
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds);
	virtual	bool IsInMotion(const CPed* pPed) const;
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual CTask* CreatePlayerControlTask();
	virtual bool ShouldStickToFloor() { return true; }
	virtual bool IsOnFoot() { return true; }
	virtual bool ForceLeafTask() const { return true; } 
	virtual bool ShouldDisableSlowingDownForCornering() const;
	virtual bool AllowLadderRunMount() const;

	//
	// Locomotion API
	//

	void SetMovementClipSet(const fwMvClipSetId& clipSetId);
	void SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId, bool bUpperBodyShadowExpression = false);

	void StartAlternateClip(AlternateClipType type, sAlternateClip& data, bool bLooping);
	void EndAlternateClip(AlternateClipType type, float fBlendDuration);

	bool UseLeftFootTransition() const;
	bool UseLeftFootStrafeTransition() const;
	bool IsLastFootLeft() const;
	bool IsFullyInIdle() const { return m_Flags.IsFlagSet(Flag_FullyInIdle); }
	bool IsFullyInStop() const { return m_Flags.IsFlagSet(Flag_FullyInStop); }
	bool IsFootDown() const;
	bool IsFootDown(bool leftFoot) const;
	bool IsTryingToStop() const { return m_iMovingStopCount > 0; }

	bool IsRunning() const { return m_Flags.IsFlagSet(Flag_Running); }
	bool WasRunning() const { return m_Flags.IsFlagSet(Flag_WasRunning); }

	virtual float GetStandingSpringStrength();

	// Predict the stopping distance
	virtual float GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive = NULL);

	// Are we about to stop?
	bool IsGoingToStopThisFrame(const CTaskMoveGoToPointAndStandStill* pGotoTask) const;

	bool IsOkToSetDesiredStopHeading(const CTaskMoveGoToPointAndStandStill* pGotoTask) const;

	// Don't change speed input, so we don't trigger a run to walk/walk to run transition
	void SetBlockSpeedChangeWhilstMoving(bool bBlock) { m_Flags.ChangeFlag(Flag_BlockSpeedChangeWhilstMoving, bBlock); }

	// Set during awkward gait transitions or other situations that may inadvertently trigger gait reduction
	void SetGaitReductionBlock(float fDuration = 0.5f) { m_fGaitReductionBlockTimer = fDuration; }

	// Set the idle intro transition clip to use
	void SetTransitionToIdleIntro(const CPed *pPed, const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, bool bSupressWeaponHolding = false, float fBlendOutTime = 0.25f);

	// Clear the idle intro transition data
	void ClearTransitionToIdleIntro();

	//
	bool IsTransitionToIdleIntroActive() const;

	// Set that we should skip the idle intro when doing a transition
	void SetSkipIdleIntroFromTransition(bool bSkip) { m_Flags.ChangeFlag(Flag_SkipIdleIntroFromTransition, bSkip); }

	const fwMvClipSetId& GetMovementClipSetId() const { return m_ClipSetId; }

	// Allow the initial phase of the walk/run starts to be overridden
	void SetBeginMoveStartPhase(const float fPhase) { m_fBeginMoveStartPhase = fPhase; }

	//
	void SetPreferSuddenStops(bool bBlock) { m_Flags.ChangeFlag(Flag_PreferSuddenStops, bBlock); }

	//
	void SetUseLeftFootTransition(bool bUseLeftFoot);

	// Force the next change in weapon clip set to be an instant blend
	void SetInstantBlendNextWeaponClipSet(bool bInstantBlend) { m_Flags.ChangeFlag(Flag_InstantBlendWeaponClipSet, bInstantBlend); }

	//
	void SetCheckForAmbientIdle() { m_CheckForAmbientIdle = true; }

	//
	bool IsTransitionBlocked() const;

	//
	bool WillDoStrafeTransition(const CPed* pPed) const;

	//
	bool CanEarlyOutForMovement() const { return m_MoveCanEarlyOutForMovement; }

	//
	u32 GetIdleTransitionWeaponHash() const { return m_uIdleTransitionWeaponHash; }

	//
	// Helpers
	//

	// Check for a particular move event flag
	static bool CheckClipSetForMoveFlag(const fwMvClipSetId& clipSetId, const fwMvFlagId& flagId);

	//
	static bool CheckBooleanEvent(const CTask* pTask, const fwMvBooleanId& id);

#if __ASSERT
	enum VerifyClipFlags
	{
		VCF_HasFootTags = BIT0,
		VCF_HasHeelProperty = BIT1,
		VCF_ZeroDuration = BIT2,
		VCF_HasAlternatingFootTags = BIT3,
		VCF_HasEvenNumberOfFootTags = BIT4,
		VCF_FootTagOnPhase0 = BIT5,
		VCF_FirstFootTagRight = BIT6,
		VCF_FirstFootTagLeft = BIT7,
		VCF_MovementCycle = VCF_HasFootTags|VCF_HasHeelProperty|VCF_ZeroDuration|VCF_HasAlternatingFootTags|VCF_HasEvenNumberOfFootTags,
		VCF_MovementOneShot = VCF_HasFootTags|VCF_HasHeelProperty|VCF_ZeroDuration|VCF_HasAlternatingFootTags/*|VCF_FootTagOnPhase0*/,
		VCF_MovementOneShotLeft = VCF_MovementOneShot|VCF_FirstFootTagRight,
		VCF_MovementOneShotRight = VCF_MovementOneShot|VCF_FirstFootTagLeft,
		VCF_OneShot = VCF_HasFootTags|VCF_HasHeelProperty|VCF_ZeroDuration|VCF_HasAlternatingFootTags,
	};
	static void VerifyClip(const crClip* pClip, const s32 iFlags, const char* clipName, const char* dictionaryName, const CPed *pPed);

	struct sVerifyClip
	{
		sVerifyClip() {}
		sVerifyClip(const fwMvClipId& clipId, s32 iFlags) : clipId(clipId), iFlags(iFlags) {}
		fwMvClipId clipId;
		s32 iFlags;
	};
	static void VerifyClips(const fwMvClipSetId& clipSetId, const atArray<sVerifyClip>& clips, const CPed *pPed);

	static void VerifyMovementClipSet(const fwMvClipSetId clipSetId, fwFlags32 flags, fwMvClipSetId genericMovementClipSetId, const CPed *pPed);
#endif // __ASSERT
	
	// Calculate slope/stair values
	static void ProcessSlopesAndStairs(CPed* pPed, u32& uLastSlopeScanTime, u32& uStairsDetectedTime, Vector3& vLastSlopePosition, const bool bIgnoreStrafeCheck = false);

	// Clear slope/stairs variables
	static void ResetSlopesAndStairs(CPed* pPed, Vector3& vLastSlopePosition);

	static bool IsOnSlopesAndStairs(const CPed* pPed, const bool bCrouching, float& fSlopePitch);

	// Calculate the snow values
	static void ProcessSnowDepth(const CPed* pPed, const float fTimeStep, float& fSmoothedSnowDepth, float& fSnowDepthSignal);

protected:

	//
	// aiTask API
	//

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();
	virtual	void CleanUp();
	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }

	//
	// CTask API
	//

	virtual bool ProcessPostMovement();

private:

	//
	// States
	//

	FSM_Return StateInitial_OnUpdate();
	FSM_Return StateIdle_OnEnter();
	FSM_Return StateIdle_OnUpdate();
	FSM_Return StateIdle_OnExit();
	FSM_Return StateIdleTurn_OnEnter();
	FSM_Return StateIdleTurn_OnUpdate();
	FSM_Return StateIdleTurn_OnExit();
	FSM_Return StateStart_OnEnter();
	FSM_Return StateStart_OnUpdate();
	FSM_Return StateStart_OnExit();
	FSM_Return StateMoving_OnEnter();
	FSM_Return StateMoving_OnUpdate();
	FSM_Return StateMoving_OnExit();
	FSM_Return StateTurn180_OnEnter();
	FSM_Return StateTurn180_OnUpdate();
	FSM_Return StateTurn180_OnExit();
	FSM_Return StateStop_OnEnter();
	FSM_Return StateStop_OnUpdate();
	FSM_Return StateStop_OnExit();
	FSM_Return StateRepositionMove_OnEnter();
	FSM_Return StateRepositionMove_OnUpdate();
	FSM_Return StateFromStrafe_OnEnter();
	FSM_Return StateFromStrafe_OnUpdate();
	FSM_Return StateFromStrafe_OnExit();
	FSM_Return StateCustomIdle_OnEnter();
	FSM_Return StateCustomIdle_OnUpdate();	
	FSM_Return StateCustomIdle_OnExit();

	// Per state process move signal calls
	void StateIdle_InitMoveSignals();
	void StateIdle_OnProcessMoveSignals();
	void StateIdleTurn_InitMoveSignals();
	void StateIdleTurn_OnProcessMoveSignals();
	void StateStart_InitMoveSignals();
	void StateStart_OnProcessMoveSignals();
	void StateTurn180_InitMoveSignals();
	void StateTurn180_OnProcessMoveSignals();
	void StateStop_InitMoveSignals();
	void StateStop_OnProcessMoveSignals();
	void StateCustomIdle_InitMoveSignals();
	void StateCustomIdle_OnProcessMoveSignals();

	//
	// Locomotion
	//

	void SendMovementClipSet();
	void SendWeaponClipSet();
	void StoreWeaponFilter();
	void SendAlternateParams(AlternateClipType type);

	// Update move blend ratios, turn velocities etc.
	void ProcessMovement();

	// Calculate if player should slow down in MP when running towards a ladder
	void ProcessMPLadders(Vector2 const& vCurrentMBR, Vector2& vDesiredMBR);

	// Update events
	void ProcessAI();

	// Decide whether or not to flip the foot flags when coming to a stop, so we can start part way through the anim for a shorter stop
	void FlipFootFlagsForStopping(const CTaskMoveGoToPointAndStandStill* pGotoTask);

	// Should the ped be using the AI style motion?
	bool GetUsingAIMotion() const;

	//
	bool UsingHeavyWeapon();

	// 
	void InitFromStrafeUpperBodyTransition();

	//
	bool UpdateFromStrafeUpperBodyTransition();

	//
	void CleanUpFromStrafeUpperBodyTransition();

	//
	void UpdateActionModeMovementRate();

	//
	bool ShouldUseActionModeMovementRate() const;

	//
	bool IsSuppressingWeaponHolding() const; 

	//
	// State functions
	//

	void StateMoving_MovingDirection(const float fTimeStep, const float fDesiredDirection);
	void StateMoving_ExtraHeading(const float fTimeStep, const float fDesiredDirection, const Tunables::sMovingVars& vars);
	void StateMoving_SlopesAndStairs(const float fTimeStep);
	void StateMoving_Snow();
	void StateMoving_MovementRate();

	float CNC_GetNormalizedSlopeDirectionForAdjustedMovementRate() const;

#if __ASSERT
	void VerifyWeaponClipSet();
#endif // __ASSERT

#if __DEV
	void LogMovingVariables();
#endif // __DEV

	//
	void RecordStateTransition();

	//
	bool CanChangeState() const;

	//
	void ComputeIdleTurnSignals();

	// 
	void SendIdleTurnSignals();

	//
	bool TransitionToIdleIntroSet() const { return m_TransitionToIdleIntroClipSetId != CLIP_SET_ID_INVALID && m_TransitionToIdleIntroClipId != CLIP_ID_INVALID; }

#if FPS_MODE_SUPPORTED
	//
	void ComputeAndSendPitchedWeaponHolding();
#endif // FPS_MODE_SUPPORTED

	//
	// Members
	//

	// Last stored slope position
	Vector3 m_vLastSlopePosition;

	// Move helper
	CMoveNetworkHelper m_MoveNetworkHelper;

	// Main clip clipSetId for ped locomotion
	fwClipSet* m_ClipSet;
	fwMvClipSetId m_ClipSetId;

	// Generic movement clipSetId - taken from the fall back of m_ClipSetId if used
	fwMvClipSetId m_GenericMovementClipSetId;

	// Clip clipSetId to use for synced partial weapon clips
	fwClipSet* m_WeaponClipSet;
	fwMvClipSetId m_WeaponClipSetId;

	// The filter to be used for weapon clips 
	fwMvFilterId m_WeaponFilterId;
	crFrameFilterMultiWeight* m_pWeaponFilter;

	// Transition to idle intro anim
	fwMvClipSetId m_TransitionToIdleIntroClipSetId;
	fwMvClipId m_TransitionToIdleIntroClipId;
	fwClipSetRequestHelper m_TransitionToIdleClipSetRequestHelper;

	//Custom idle asset management
	fwClipSetRequestHelper m_CustomIdleClipSetRequestHelper;

	// Used to determine the state we'll force the move network into on startup.
	State m_InitialState;

	// Used to store the results of a ladder scan as it's expensive...
	LadderScanInfoCache m_MPOnlyLadderInfoCache;

	// Starting direction of move start, relative to our current direction
	float m_fStartDirection;

	// If the target changes, we interpolate m_fStartDirection to m_fStartDirectionTarget
	float m_fStartDirectionTarget;

	// Starting desired direction of move start
	float m_fStartDesiredDirection;

	// Stopping direction of move stop, relative to our current direction
	float m_fStopDirection;

	// Current moving direction, interpolated to desired direction
	float m_fMovingDirection;

	// The current rate that we interpolate towards the desired from m_fMovingDirection
	float m_fMovingDirectionRate;

	// Time we have spent moving forward
	float m_fMovingForwardTimer;

	// Used to determine if we are still heading in the same direction as we were last frame
	float m_fPreviousMovingDirectionDiff;

	// Previous frames desired direction for triggering 180 turns
	static const s32 MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES = 4;
	float m_fPreviousDesiredDirection[MAX_PREVIOUS_DESIRED_DIRECTION_FRAMES];

	// Blend between slope angles
	float m_fSmoothedSlopeDirection;

	// Blend between snow depths
	float m_fSmoothedSnowDepth;

	// Snow depth signal
	float m_fSnowDepthSignal;

	// Movement rate modifier
	float m_fMovementRate;

	// Movement turn rate modifier
	float m_fMovementTurnRate;

	// Foliage movement rate
	float m_fFoliageMovementRate;

	// Stealth movement rate
	float m_fStealthMovementRate;

	// Prevent stopping when checking for 180 turns for a number of frames, so we get a chance to trigger them
	s32 m_iMovingStopCount;

	// Block gait reduction for brief amounts of time during transitions
	float m_fGaitReductionBlockTimer;

	// Frames to delay before triggering idle turn, in case we perform a move start
	s32 m_iIdleTurnCounter;

	// Last time we performed a slope scan
	u32 m_uLastSlopeScanTime;

	// The current extra heading rate
	float m_fExtraHeadingRate;

	// Override the starting phase for the start anims, is cleared after being used
	float m_fBeginMoveStartPhase;

	// Time between adding running events
	float m_fTimeBetweenAddingRunningShockingEvents;

	// Generic move forward blend weight
	float m_fGenericMoveForwardBlendWeight;

	// 
	float m_fFromStrafeInitialFacingDirection;

	//
	float m_fFromStrafeTransitionUpperBodyWeight;

	//
	float m_fFromStrafeTransitionUpperBodyDirection;

	//
	float m_fInitialMovingDirection;

	//
	u32 m_uIdleTransitionWeaponHash;

	//
	float m_fTransitionToIdleBlendOutTime;

	//
	struct sActionModeRateData
	{
		static const s32 ACTION_MODE_SPEEDS = 2;

		sActionModeRateData() 
			: ActionModeMovementRate(1.f)
			, BaseModelClipSetId(CLIP_SET_ID_INVALID)
		{
			for(s32 i = 0; i < ACTION_MODE_SPEEDS; i++)
				ActionModeSpeedRates[i] = 1.f;
		}

		// Action mode movement rate
		float ActionModeMovementRate;

		// Cached speeds
		float ActionModeSpeedRates[ACTION_MODE_SPEEDS];

		// Base model movement clip set
		fwMvClipSetId BaseModelClipSetId;
	};

	//
	sActionModeRateData* m_pActionModeRateData;

	//
	static const u32 MAX_ACTIVE_STATE_TRANSITIONS = 3;
	u32 m_uStateTransitionTimes[MAX_ACTIVE_STATE_TRANSITIONS];

	//
	u32 m_uStairsDetectedTime;

	// Flags
	enum Flags
	{
		Flag_Crouching								= BIT0,
		Flag_Running								= BIT1,
		Flag_WasRunning								= BIT2,
		Flag_MovementClipSetChanged					= BIT3,
		Flag_WeaponClipSetChanged					= BIT4,
		Flag_WeaponFilterChanged					= BIT5,
		Flag_WasOnSlope								= BIT6,
		Flag_FullyInIdle							= BIT7,
		Flag_FullyInStop							= BIT8,
		Flag_UpperBodyShadowExpression				= BIT9,
		Flag_SetInitialMovingPhase					= BIT10,
		Flag_FromStrafe								= BIT11,
		Flag_UseLeftFootStrafeTransition			= BIT12,
		Flag_LastFootLeft							= BIT13,
		Flag_FlipLastFootLeft						= BIT14,
		Flag_LastFootLeftThisFrame					= BIT15,
		Flag_LastFootRightThisFrame					= BIT16,
		Flag_BlockSpeedChangeWhilstMoving			= BIT17,
		Flag_OrientatedStop							= BIT18,
		Flag_PreferSuddenStops						= BIT19,
		Flag_CustomIdleLoaded						= BIT20,
		Flag_SkipIdleIntroFromTransition			= BIT21,
		Flag_BlockGenericFallback					= BIT22,
		Flag_InstantBlendWeaponClipSet				= BIT23,
		Flag_HasEmptyClip							= BIT24,
		Flag_SuppressWeaponHoldingForIdleTransition	= BIT25,
		Flag_StartsOnLeftFoot						= BIT26,
		Flag_UpperBodyShadowExpressionChanged		= BIT27,
		Flag_IsFat									= BIT28,
		Flag_HasPitchedWeaponHolding				= BIT29,
		Flag_WasOnStairs							= BIT30,
		Flag_WasOnSnow								= BIT31,
	};

	// Flags
	fwFlags32 m_Flags;

	// Handles the upper body aiming anims
	CUpperBodyAimPitchHelper* m_pUpperBodyAimPitchHelper;

	//
	u32 m_uSlowDownInWaterTime;

#if FPS_MODE_SUPPORTED
	float m_fCurrentPitch;
#endif // FPS_MODE_SUPPORTED

	//
	u32 m_uCameraHeadingLerpTime;
	
	// Cached move signals
	bool m_MoveIdleOnEnter : 1;
	bool m_MoveIdleTurnOnEnter : 1;
	bool m_MoveIdleStandardEnter : 1;
	bool m_MoveStartOnEnter : 1;
	bool m_MoveMovingOnEnter : 1;
	bool m_MoveTurn180OnEnter : 1;
	bool m_MoveStopOnEnter : 1;
	bool m_MoveRepositionMoveOnEnter : 1;
	bool m_MoveFromStrafeOnEnter : 1;
	bool m_MoveCustomIdleOnEnter : 1;
	bool m_MoveStartOnExit : 1;
	bool m_MoveMovingOnExit : 1;
	bool m_MoveUseLeftFootStrafeTransition : 1;
	bool m_MoveBlendOutIdleTurn : 1;
	bool m_MoveBlendOutStart : 1;
	bool m_MoveBlendOut180 : 1;
	bool m_MoveBlendOutStop : 1;
	bool m_MoveCanEarlyOutForMovement : 1;
	bool m_MoveCanResumeMoving : 1;
	bool m_MoveActionModeBlendOutStart : 1;
	bool m_MoveActionModeBlendOutStop : 1;
	bool m_MoveIdleTransitionFinished : 1;
	bool m_MoveIdleTransitionBlendInWeaponHolding : 1;
	bool m_MoveCustomIdleFinished : 1;
	bool m_CheckForAmbientIdle : 1;
	bool m_DoIdleTurnPostMovementUpdate : 1;
	bool m_SkipNextMoveIdleStandardEnter : 1;
#if FPS_MODE_SUPPORTED
	bool m_FirstTimeRun : 1;
#endif // FPS_MODE_SUPPORTED
	bool m_MoveAlternateFinished[ACT_MAX];

	//
	// Statics
	//

public:

	// Instance of tunable task params
	static Tunables ms_Tunables;	

	//
	// Move Signal Id's
	//

public:

	// Public Id's
	static const fwMvBooleanId ms_AefFootHeelLId;
	static const fwMvBooleanId ms_AefFootHeelRId;

	// Variable clipsets
	static const fwMvClipSetVarId ms_WeaponHoldingClipSetVarId;
	// Filters
	static const fwMvFilterId ms_WeaponHoldingFilterId;
	static const fwMvFilterId ms_MeleeGripFilterId;
	// Requests
	static const fwMvRequestId ms_RestartWeaponHoldingId;
	// Flags
	static const fwMvFlagId ms_UseWeaponHoldingId;
private:

	// Networks
	static const fwMvNetworkId ms_RepositionMoveNetworkId;
	static const fwMvNetworkId ms_FromStrafeNetworkId;
	// States
	static const fwMvStateId ms_LocomotionStateId;
	static const fwMvStateId ms_IdleStateId;
	static const fwMvStateId ms_MovingStateId;
	static const fwMvStateId ms_FromStrafeStateId;
	// Variable clipsets
	static const fwMvClipSetVarId ms_GenericMovementClipSetVarId;
	static const fwMvClipSetVarId ms_SnowMovementClipSetVarId;
	static const fwMvClipSetVarId ms_FromStrafeTransitionUpperBodyClipSetVarId;
	// Clips
	static const fwMvClipId ms_WalkClipId;
	static const fwMvClipId ms_RunClipId;
	static const fwMvClipId ms_MoveStopClip_N_180Id;
	static const fwMvClipId ms_MoveStopClip_N_90Id;
	static const fwMvClipId ms_MoveStopClipId;
	static const fwMvClipId ms_MoveStopClip_90Id;
	static const fwMvClipId ms_MoveStopClip_180Id;
	static const fwMvClipId ms_TransitionToIdleIntroId;
	static const fwMvClipId ms_Turn180ClipId;
	static const fwMvClipId ms_CustomIdleClipId;
	static const fwMvClipId ms_ClipEmpty;
#if FPS_MODE_SUPPORTED
	static const fwMvClipId ms_RunHighClipId;
#endif // FPS_MODE_SUPPORTED
	// Floats
	static const fwMvFloatId ms_MovementRateId;
	static const fwMvFloatId ms_MovementTurnRateId;
	static const fwMvFloatId ms_IdleTurnDirectionId;
	static const fwMvFloatId ms_IdleTurnRateId;
	static const fwMvFloatId ms_StartDirectionId;
	static const fwMvFloatId ms_DesiredDirectionId;
	static const fwMvFloatId ms_DirectionId;
	static const fwMvFloatId ms_StopDirectionId;
	static const fwMvFloatId ms_SlopeDirectionId;
	static const fwMvFloatId ms_SnowDepthId;
	static const fwMvFloatId ms_DesiredSpeedId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_MoveStopPhase_N_180Id;
	static const fwMvFloatId ms_MoveStopPhase_N_90Id;
	static const fwMvFloatId ms_MoveStopPhaseId;
	static const fwMvFloatId ms_MoveStopPhase_90Id;
	static const fwMvFloatId ms_MoveStopPhase_180Id;
	static const fwMvFloatId ms_MoveStopBlend_N_180Id;
	static const fwMvFloatId ms_MoveStopBlend_N_90Id;
	static const fwMvFloatId ms_MoveStopBlendId;
	static const fwMvFloatId ms_MoveStopBlend_90Id;
	static const fwMvFloatId ms_MoveStopBlend_180Id;
	static const fwMvFloatId ms_IdleIntroStartPhaseId;
	static const fwMvFloatId ms_BeginMoveStartPhaseId;
	static const fwMvFloatId ms_IdleStartPhaseId;
	static const fwMvFloatId ms_IdleRateId;
	static const fwMvFloatId ms_Turn180PhaseId;
	static const fwMvFloatId ms_MovingInitialPhaseId;
	static const fwMvFloatId ms_GenericMoveForwardWeightId;
	static const fwMvFloatId ms_WeaponHoldingBlendDurationId;
	static const fwMvFloatId ms_FromStrafeTransitionUpperBodyWeightId;
	static const fwMvFloatId ms_FromStrafeTransitionUpperBodyDirectionId;
	static const fwMvFloatId ms_IdleTransitionBlendOutDurationId;
#if FPS_MODE_SUPPORTED
	static const fwMvFloatId ms_PitchId;
#endif // FPS_MODE_SUPPORTED
	// Booleans/Events
	// State enters
	static const fwMvBooleanId ms_IdleOnEnterId;
	static const fwMvBooleanId ms_IdleTurnOnEnterId;
	static const fwMvBooleanId ms_IdleStandardEnterId;
	static const fwMvBooleanId ms_StartOnEnterId;
	static const fwMvBooleanId ms_MovingOnEnterId;
	static const fwMvBooleanId ms_Turn180OnEnterId;
	static const fwMvBooleanId ms_StopOnEnterId;
	static const fwMvBooleanId ms_RepositionMoveOnEnterId;
	static const fwMvBooleanId ms_FromStrafeOnEnterId;
	static const fwMvBooleanId ms_CustomIdleOnEnterId;
	// State exits
	static const fwMvBooleanId ms_StartOnExitId;
	static const fwMvBooleanId ms_MovingOnExitId;
	// Running flags
	static const fwMvBooleanId ms_WalkingId;
	static const fwMvBooleanId ms_RunningId;
	static const fwMvBooleanId ms_UseLeftFootStrafeTransitionId;
#if FPS_MODE_SUPPORTED
	static const fwMvBooleanId ms_UsePitchedWeaponHoldingId;
#endif // FPS_MODE_SUPPORTED
	// Early outs
	static const fwMvBooleanId ms_BlendOutIdleTurnId;
	static const fwMvBooleanId ms_BlendOutStartId;
	static const fwMvBooleanId ms_BlendOut180Id;
	static const fwMvBooleanId ms_BlendOutStopId;
	static const fwMvBooleanId ms_CanEarlyOutForMovementId;
	static const fwMvBooleanId ms_CanResumeMovingId;
	static const fwMvBooleanId ms_ActionModeBlendOutStartId;
	static const fwMvBooleanId ms_ActionModeBlendOutStopId;
	//
	static const fwMvBooleanId ms_IdleTransitionFinishedId;
	static const fwMvBooleanId ms_IdleTransitionBlendInWeaponHoldingId;
	static const fwMvBooleanId ms_CustomIdleFinishedId;
	//
	static const fwMvBooleanId ms_StartLeftId;
	static const fwMvBooleanId ms_StartRightId;
	//
	static const fwMvBooleanId ms_AlternateFinishedId[ACT_MAX];
	// Requests
	static const fwMvRequestId ms_StoppedId;
	static const fwMvRequestId ms_IdleTurnId;
	static const fwMvRequestId ms_StartMovingId;
	static const fwMvRequestId ms_MovingId;
	static const fwMvRequestId ms_Turn180Id;
	static const fwMvRequestId ms_StopMovingId;
	static const fwMvRequestId ms_RepositionMoveId;
	static const fwMvRequestId ms_CustomIdleId;
	// Flags
	static const fwMvFlagId ms_OnStairsId;
	static const fwMvFlagId ms_OnSteepStairsId;
	static const fwMvFlagId ms_OnSlopeId;
	static const fwMvFlagId ms_OnSnowId;
	static const fwMvFlagId ms_ForceSuddenStopId;
	static const fwMvFlagId ms_SkipWalkRunTransitionId;
	static const fwMvFlagId ms_UseTransitionToIdleIntroId;
	static const fwMvFlagId ms_EnableIdleTransitionsId;
	static const fwMvFlagId ms_EnableUpperBodyShadowExpressionId;
	static const fwMvFlagId ms_UseSuddenStopsId;
	static const fwMvFlagId ms_NoSprintId;
	static const fwMvFlagId ms_IsRunningId;
	static const fwMvFlagId ms_BlockGenericFallbackId;
	static const fwMvFlagId ms_SkipIdleIntroFromTransitionId;
	static const fwMvFlagId ms_LastFootLeftId;
	static const fwMvFlagId ms_InStealthId;
	static const fwMvFlagId ms_UseFromStrafeTransitionUpperBodyId;
	static const fwMvFlagId ms_EnableUpperBodyFeatheredLeanId;
	static const fwMvFlagId ms_WeaponEmptyId;
	static const fwMvFlagId ms_UseRunSprintId;
	static const fwMvFlagId ms_SkipIdleIntroId;
	static const fwMvFlagId ms_IsFatId;
};

////////////////////////////////////////////////////////////////////////////////

inline bool CTaskHumanLocomotion::IsFootDown() const
{
	return m_MoveNetworkHelper.IsNetworkActive() && (CheckBooleanEvent(this, ms_AefFootHeelLId) || CheckBooleanEvent(this, ms_AefFootHeelRId));
}

inline bool CTaskHumanLocomotion::IsFootDown(bool bLeftFoot) const
{
	if (bLeftFoot)
	{
		return m_MoveNetworkHelper.IsNetworkActive() && CheckBooleanEvent(this, ms_AefFootHeelLId);
	}
	else
	{
		return m_MoveNetworkHelper.IsNetworkActive() && CheckBooleanEvent(this, ms_AefFootHeelRId);
	}
}


#endif // TASK_HUMAN_LOCOMOTION_H
