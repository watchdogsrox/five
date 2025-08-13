#ifndef TASK_QUAD_LOCOMOTION_H
#define TASK_QUAD_LOCOMOTION_H

#include "Task/Motion/TaskMotionBase.h"
#include "Task/System/Tuning.h"

//*********************************************************************
//	CTaskQuadLocomotion
//	The top-level motion task for the on-foot moveblender for a quadruped. 
//*********************************************************************

class CTaskQuadLocomotion
	: public CTaskMotionBase
{
public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// TURNING
		// Turn in place - transitioning from idle to turns.
		float m_StartAnimatedTurnsD;
		float m_StopAnimatedTurnsD;
		float m_TurnTransitionDelay;
		float m_TurnToIdleTransitionDelay;
		float m_SteepSlopeStartAnimatedTurnsD;
		float m_SteepSlopeStopAnimatedTurnsD;
		float m_SteepSlopeThresholdD;

		// ProcessPhysics constants.
		float m_InMotionAlignmentVelocityTolerance;
		float m_InMotionTighterTurnsVelocityTolerance;
		float m_InPlaceAlignmentVelocityTolerance;

		// Locomotion turning
		float m_TurnSpeedMBRThreshold;
		float m_SlowMinTurnApproachRate;
		float m_FastMinTurnApproachRate;
		float m_SlowTurnApproachRate;
		float m_FastTurnApproachRate;
		float m_SlowTurnAcceleration;
		float m_FastTurnAcceleration;
		float m_TurnResetThresholdD;
		float m_ProcessPhysicsApproachRate;

		// Timeslicing
		float m_DisableTimeslicingHeadingThresholdD;
		float m_LowLodExtraHeadingAdjustmentRate;

		// Start locomotion
		float m_StartLocomotionBlendoutThreshold;
		float m_StartLocomotionHeadingDeltaBlendoutThreshold;
		float m_StartLocomotionDefaultBlendDuration;
		float m_StartLocomotionDefaultBlendOutDuration;
		float m_StartLocomotionEarlyOutBlendOutDuration;
		float m_StartLocomotionWalkRunBoundary;
		float m_StartToIdleDirectlyPhaseThreshold;
		float m_MovementAcceleration;

		// GAITS
		float m_MinMBRToStop;

		// PURSUIT MODE
		float	m_PursuitModeGallopRateFactor;
		float	m_PursuitModeExtraHeadingRate;

		// STOPPING
		float	m_StoppingDistanceWalkMBR;
		float	m_StoppingDistanceRunMBR;
		float	m_StoppingDistanceGallopMBR;
		float	m_StopPhaseThreshold;
		float	m_StoppingGotoPointRemainingDist;
		float	m_MinStopPhaseToResumeMovement;
		float	m_MaxStopPhaseToResumeMovement;

		// CAMERAS
		atHashString m_PlayerControlCamera;

		// REVERSING
		float	m_ReversingHeadingChangeDegreesForBreakout;
		float	m_ReversingTimeBeforeAllowingBreakout;

		PAR_PARSABLE;
	};

	CTaskQuadLocomotion(u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId);
	virtual ~CTaskQuadLocomotion();

	enum
	{
		State_Initial,
		State_Idle, 
		State_StartLocomotion,
		State_StopLocomotion,
		State_Locomotion,
		State_TurnInPlace,
		State_Dead,
		State_DoNothing,
	};

	virtual FSM_Return	ProcessPreFSM();

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool		ProcessMoveSignals();

	// PURPOSE: Manipulate the animated velocity to achieve tighter turns.
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual CTask*		CreatePlayerControlTask();

	virtual s32			GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*		Copy() const { return rage_new CTaskQuadLocomotion(m_initialState, m_initialSubState, m_blendType, m_ClipSetId); }
	virtual s32			GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_QUAD; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void		CleanUp();

	inline bool			SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
		case CPedMotionStates::MotionState_Dead:
		case CPedMotionStates::MotionState_DoNothing:
			return true;
		default:
			return false;
		}
	}

	bool			IsInMotionState(CPedMotionStates::eMotionState state) const;

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

public:

	virtual bool	IsOnFoot() { return true; }

	virtual void	GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds);

	virtual float GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* bOutIsClipActive);

	virtual	bool	CanFlyThroughAir()	{ return false; }

	virtual bool	ShouldStickToFloor() { return true; }

	virtual void	GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);

	virtual	bool	IsInMotion(const CPed* pPed) const;

	virtual bool	ShouldDisableSlowingDownForCornering() const;

	virtual bool	AllowLadderRunMount() const { return true; }

	bool			IsReversing() const { return m_bReverseThisFrame; }

	void			GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_ClipSetId;
		outClip = CLIP_IDLE;
	}

	// FSM States

	void			StateInitial_OnEnter(); 
	FSM_Return		StateInitial_OnUpdate(); 

	void			StateIdle_OnEnter(); 
	FSM_Return		StateIdle_OnUpdate();
	void			StateIdle_OnProcessMoveSignals();

	void			StateStartLocomotion_OnEnter();
	FSM_Return		StateStartLocomotion_OnUpdate();
	void			StateStartLocomotion_OnExit();
	void			StateStartLocomotion_OnProcessMoveSignals();

	void			StateStopLocomotion_OnEnter();
	FSM_Return		StateStopLocomotion_OnUpdate();
	void			StateStopLocomotion_OnProcessMoveSignals();
	
	void			StateLocomotion_OnEnter(); 
	FSM_Return		StateLocomotion_OnUpdate(); 
	void			StateLocomotion_OnProcessMoveSignals();

	void			StateTurnInPlace_OnEnter(); 
	FSM_Return		StateTurnInPlace_OnUpdate();
	void			StateTurnInPlace_OnProcessMoveSignals();

	void			StateDead_OnEnter();
	FSM_Return		StateDead_OnUpdate();
	void			StateDead_OnProcessMoveSignals();

	void			StateDoNothing_OnEnter();
	FSM_Return		StateDoNothing_OnUpdate();
	void			StateDoNothing_OnProcessMoveSignals();

	virtual float	CalcDesiredDirection() const;

	// Quad moves faster and has tighter turning than normal while chasing down a high priority target.
	void			SetUsePursuitMode(bool bPursuit)			{ m_bPursuitMode = bPursuit; }
	bool			GetIsUsingPursuitMode() const				{ return m_bPursuitMode; }

	static const Tunables& GetTunables()						{ return sm_Tunables; }

private:

	bool			CanBeTimesliced() const;
	bool			ShouldForceNoTimeslicing() const;

	bool			IsInWaterWhileLowLod() const;

	bool			IsBlockedInFront() const;
	
	// Calculates what the floats that determine animation blending should be for this ped.
	void			ProcessGaits();
	void			ProcessTurnPhase();

	// Send the floats that determine animation blending to MoVE.
	void			SendLocomotionMoveSignals();
	void			SendStopLocomotionMoveSignals();
	void			SendTurnSignals(float fTurnPhase);


	float			GetMinTurnRate(float fMBR) const;
	float			GetIdealTurnRate(float fMBR) const;
	float			GetTurnAcceleration(float fMBR) const;
	
	bool			ShouldBeMoving();
	bool			ShouldStopMoving();
	bool			ShouldScaleTurnVelocity(float fDesiredHeadingChange, float fAnimationRotateSpeed) const;
	float			GetTurnVelocityScaleFactor() const;


	// Apply extra steering to quadrupeds using low lod motion.
	void			ApplyExtraHeadingAdjustments();

	// Look at the ped specific tuning and return how long it takes for this qped to switch out gaits.
	float			GetTimeBetweenGaits(float fCurrentGait, float fNextGait) const;

	// Look at the metadata to see what animations are supported.
	bool			HasWalkStarts() const;
	bool			HasRunStarts() const;
	bool			HasQuickStops() const;
	bool			CanTrot() const;

private:

	// Move Network Signal IDs

	//States
	static const fwMvStateId ms_LocomotionStateId;
	static const fwMvStateId ms_IdleStateId;
	static const fwMvStateId ms_TurnInPlaceStateId;
	static const fwMvStateId ms_NoClipStateId;

	static const fwMvRequestId ms_StartTurnInPlaceId;
	static const fwMvBooleanId ms_OnEnterTurnInPlaceId;

	static const fwMvRequestId ms_startIdleId;
	static const fwMvBooleanId ms_OnEnterIdleId;
	static const fwMvFloatId ms_IdleRateId;

	static const fwMvRequestId ms_StartLocomotionId;
	static const fwMvBooleanId ms_OnEnterStartLocomotionId;
	static const fwMvBooleanId ms_StartLocomotionDoneId;
	static const fwMvFloatId ms_StartLocomotionBlendDurationId;
	static const fwMvFloatId ms_StartLocomotionBlendOutDurationId;

	static const fwMvRequestId ms_StopLocomotionId;
	static const fwMvBooleanId ms_OnEnterStopLocomotionId;
	static const fwMvBooleanId ms_StopLocomotionDoneId;
	static const fwMvFloatId ms_StopPhaseId;

	static const fwMvRequestId ms_LocomotionId;
	static const fwMvBooleanId ms_OnEnterLocomotionId;

	static const fwMvRequestId ms_StartNoClipId;
	static const fwMvBooleanId ms_OnEnterNoClipId;

	static const fwMvFloatId ms_DesiredSpeedId;
	static const fwMvFloatId ms_BlendedSpeedId;
	static const fwMvFloatId ms_MoveDirectionId;
	static const fwMvFloatId ms_IntroPhaseId;

	static const fwMvFloatId ms_MoveWalkRateId;
	static const fwMvFloatId ms_MoveTrotRateId;
	static const fwMvFloatId ms_MoveCanterRateId;	
	static const fwMvFloatId ms_MoveGallopRateId;

	static const fwMvFlagId ms_HasWalkStartsId;
	static const fwMvFlagId ms_HasRunStartsId;
	static const fwMvFlagId ms_CanTrotId;

	static const fwMvFlagId ms_LastFootLeftId;
	static const fwMvBooleanId ms_CanEarlyOutForMovement;
	static const fwMvBooleanId ms_BlendToLocomotion;

	// Member variables.

	CMoveNetworkHelper		m_MoveNetworkHelper;				// Loads the parent move network 

	CGetupSetRequestHelper	m_QuadGetupReqHelper;			// For streaming in blend from NM anims

	u16						m_initialState;
	u16						m_initialSubState;
	u32						m_blendType;					//corresponds to the motion task network blend type. (see fwMove::MotionTaskBlendType)
	fwMvClipSetId			m_ClipSetId;
	fwClipSet*				m_ClipSet;

	float					m_fCurrentGait;							// What the Quad is currently moving at.
	float					m_fNextGait;							// What the Quad wants to move towards.

	float					m_fTurnPhase;							// What turn animations should blend in while in the motions tate.
	float					m_fPreviousTurnPhaseGoal;				// Where the ped was turning last frame.
	float					m_fTurnApproachRate;					// Rate to approaching turning.
	float					m_fTransitionCounter;					// Countdown to gait swaps.
	float					m_fTurnInPlaceInitialDirection;			// Which way the quad was turning when starting turn in place.
	float					m_fIdleDesiredHeading;					// The desired heading of the qped when it got into idle.
	float					m_fLocomotionGaitBlendValue;			// What the ped needs to convert to a MoVE signal for blending in the gaits.
	float					m_fGaitWhenDecidedToStop;				// What the qped's gait originally was when it decided to stop moving.

	float					m_fLocomotionStartInitialDirection;		// Where the qped was facing at the start of locomotion, used to know if the direction has changed while the anim is playing.
	float					m_fOldMoveHeadingValue;					// Store off the direction blend sent to move at the beginning of start locomotion so that signal can be sent continuously.

	float					m_fLastIdleStateDuration;				// How long the ped was in the idle state the last time they were in it.
	float					m_fTimeBetweenThrowingEvents;			// How long before events were generated by this quadruped.

	// PURPOSE: Cached motion state variable.
	float					m_fAnimPhase;
	float					m_fDesiredHeadingWhenReverseStarted;
	float					m_fTimeReversing;

	bool					m_bTurnInPlaceIsBlendingOut;			// Is in the idle state, but there is still some animation blending out.
	bool					m_bPursuitMode;							// Quad moves faster and has tighter turning than normal while chasing down a high priority target.
	bool					m_bUsingWalkStart;
	bool					m_bUsingRunStart;
	bool					m_bMovingForForcedMotionState;
	bool					m_bLeftFootLast;						// Whether the last (rear) foot to touch the ground was right or left.
	bool					m_bInTargetMotionState;
	bool					m_bStartLocomotionIsInterruptible;
	bool					m_bStartLocomotionBlendOut;
	bool					m_bReverseThisFrame;					// Should the rate be sent in as negative this frame.

	// Instance of tunable task params
	static Tunables sm_Tunables;

};


#endif // TASK_QUAD_LOCOMOTION_H
