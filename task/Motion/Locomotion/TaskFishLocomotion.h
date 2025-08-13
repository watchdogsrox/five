#ifndef TASK_FISH_LOCOMOTION_H
#define TASK_FISH_LOCOMOTION_H

#include "Task/Motion/TaskMotionBase.h"

//*********************************************************************
//	CTaskFishLocomotion
//	The top-level motion task for the moveblender for a fish 
//*********************************************************************

class CTaskFishLocomotion
	: public CTaskMotionBase
{
public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// TURNING
		float m_StartTurnThresholdDegrees;
		float m_StopTurnThresholdDegrees;
		float m_MinTurnApproachRate;
		float m_IdealTurnApproachRate;
		float m_IdealTurnApproachRateSlow;
		float m_TurnAcceleration;
		float m_TurnAccelerationSlow;
		float m_AssistanceAngle;
		float m_ExtraHeadingRate;

		// OUT OF WATER CHECKS
		float m_FishOutOfWaterDelay;
		float m_PlayerOutOfWaterThreshold;

		// PITCH
		float m_PitchAcceleration;
		float m_PlayerPitchAcceleration;
		float m_HighLodPhysicsPitchIdealApproachRate;
		float m_LowLodPhysicsPitchIdealApproachRate;
		float m_FastPitchingApproachRate;

		// SURFACE STICKING
		float m_SurfaceProbeHead;
		float m_SurfaceProbeTail;
		float m_SurfacePitchLerpRate;
		float m_SurfaceHeightFallingLerpRate;
		float m_SurfaceHeightRisingLerpRate;
		float m_SurfaceHeightFollowingTriggerRange;

		// STATE BLENDS
		float m_LongStateTransitionBlendTime;
		float m_ShortStateTransitionBlendTime;

		// CAMERAS
		atHashString m_PlayerControlCamera;

		// RATES
		float m_GaitlessRateBoost;

		PAR_PARSABLE;
	};


	CTaskFishLocomotion( u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId );
	virtual ~CTaskFishLocomotion();

	enum
	{
		State_Initial,
		State_Idle,
		State_TurnInPlace,
		State_Swim,
		State_Flop,
		State_Dead,
		State_DoNothing
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool	ProcessMoveSignals();

	virtual bool	ProcessPostMovement();

	virtual CTask * CreatePlayerControlTask();

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*	Copy() const { return rage_new CTaskFishLocomotion(m_initialState, m_initialSubState, m_blendType, m_clipSetId); }
	virtual s32		GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_FISH; }

	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void	CleanUp();

	inline bool SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
			return true;
		case CPedMotionStates::MotionState_Dead:
			return true;
		case CPedMotionStates::MotionState_DoNothing:
			return true;
		default:
			return false;
		}
	}

	bool IsInMotionState(CPedMotionStates::eMotionState state) const
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle:
			return GetState()==State_Idle;
		case CPedMotionStates::MotionState_Dead:
			return GetState() == State_Dead;
		case CPedMotionStates::MotionState_DoNothing:
			return GetState() == State_DoNothing;
		default:
			return false;
		}
	}

public:

	virtual bool IsOnFoot() { return true; } //Correct?

	virtual bool IsSwimming() const { return GetState() != State_Flop && GetState() != State_Dead;}

	virtual bool IsInWater() { return GetState() != State_Flop && GetState() != State_Dead; }

	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	virtual	bool CanFlyThroughAir()	{ return false; }

	virtual bool ShouldStickToFloor() { return GetState() == State_Flop || GetState() == State_Dead; }

	virtual bool IsUnderWater() const { return GetState() != State_Flop && GetState() != State_Dead; }


	// Fish are in motion even when playing an idle
	virtual	bool IsInMotion(const CPed * UNUSED_PARAM(pPed)) const { return GetState()==State_Swim; }

	virtual float GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* UNUSED_PARAM(bOutIsClipActive)) { return 0.5f; } //todo

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_clipSetId;
		outClip = CLIP_IDLE;
	}

	void SetSwimNearSurface(bool bNear, bool bAllowDeeper) { m_bSwimNearSurface = bNear; m_bSwimNearSurfaceAllowDeeperThanWaves = bAllowDeeper; }
	bool GetSwimNearSurface() const		{ return m_bSwimNearSurface; }

	void SetSurviveBeingOutOfWater(bool bSurvive)	{ m_bSurviveBeingOutOfWater = bSurvive; }
	bool GetSurviveBeingOutOfWater() const			{ return m_bSurviveBeingOutOfWater; }

	void SetForceFastPitching(bool bFast)			{ m_bForceFastPitching = bFast; }
	bool GetForceFastPitching() const				{ return m_bForceFastPitching; }

	void SetUseGaitlessRateBoostThisFrame(bool bBoost)		{ m_bUseGaitlessRateBoost = bBoost; }
	bool GetUseGaitlessRateBoostThisFrame() const			{ return m_bUseGaitlessRateBoost; }

	static void TogglePedSwimNearSurface(CPed* pPed, bool bSwimNearSurface, bool bAllowDeeperThanWaves=false);
	static void ToggleFishSurvivesBeingOutOfWater(CPed* pPed, bool bSurvive);

	static void CheckAndRespectSwimNearSurface(CPed* pPed, bool bAllowDeeperThanWaves=false);

	static const Tunables& GetTunables() {	return sm_Tunables;	}

protected:
	
	//FSM State functions.
	void		StateInitial_OnEnter(); 
	FSM_Return	StateInitial_OnUpdate();

	void		StateIdle_OnEnter(); 
	FSM_Return	StateIdle_OnUpdate();
	void		StateIdle_OnProcessMoveSignals();

	void		StateTurnInPlace_OnEnter();
	FSM_Return	StateTurnInPlace_OnUpdate();
	void		StateTurnInPlace_OnProcessMoveSignals();
	
	void		StateSwim_OnEnter(); 
	FSM_Return	StateSwim_OnUpdate();
	void		StateSwim_OnProcessMoveSignals();

	void		StateFlop_OnEnter();
	FSM_Return	StateFlop_OnUpdate();

	void		StateDead_OnEnter();
	FSM_Return	StateDead_OnUpdate();
	void		StateDead_OnProcessMoveSignals();

	void		StateDoNothing_OnEnter();
	FSM_Return	StateDoNothing_OnUpdate();
	void		StateDoNothing_OnProcessMoveSignals();
		

private:

	void AllowTimeslicing();

	// Locomotion Helper functions.
	float CalcDesiredPitch();

	void InterpolateSpeed(float fDT);
	void SetNewHeading();
	void SetNewPitch();
	float GetPitchAcceleration() const;

	float GetStateTransitionDuration() const;
	float GetIdealTurnApproachRate() const;
	float GetTurnAcceleration() const;

	// Returns true if we are above the waterline.
	bool IsOutOfWater();


private:

	// Static constants.
	static const fwMvStateId ms_IdleStateId;
	static const fwMvStateId ms_NoClipStateId;

	static const fwMvRequestId ms_startIdleId;
	static const fwMvBooleanId ms_OnEnterIdleId;

	static const fwMvRequestId ms_startTurnInPlaceId;
	static const fwMvBooleanId ms_OnEnterTurnInPlaceId;

	static const fwMvRequestId ms_startSwimId;
	static const fwMvBooleanId ms_OnEnterSwimId;

	static const fwMvRequestId ms_startAccelerateId;
	static const fwMvBooleanId ms_OnEnterAccelerateId;

	static const fwMvRequestId ms_StartNoClipId;
	static const fwMvBooleanId ms_OnEnterNoClipId;

	static const fwMvFloatId ms_DirectionId;
	static const fwMvFloatId ms_MoveDesiredSpeedId;
	static const fwMvFloatId ms_MoveRateId;

	static const fwMvFloatId ms_StateTransitionDurationId;

	static dev_float ms_fFishAccelRate;
	static dev_float ms_fBigFishAccelRate;
	static dev_float ms_fBigFishDecelRate;

#if __BANK
	static bool sm_bRenderSurfaceChecks;
#endif

	// Instance of tunable task params
	static Tunables sm_Tunables;

public:

	// Publicly accessible tuning parameters.

	static dev_float ms_fBigFishMaxPitch;
	static dev_float ms_fMaxPitch;

private:

	// Member variables

	CMoveNetworkHelper	m_networkHelper;	// Loads the parent move network 

	float				m_fTurnPhase;
	float				m_fPreviousTurnPhaseGoal;
	float				m_fTurnApproachRate;

	float				m_fPitchRate;
	float				m_fStoredDesiredPitch;

	float				m_fTimeSpentOutOfWater;

	u16					m_initialState;
	u16					m_initialSubState;
	u32					m_blendType;		//corresponds to the motion task network blend type. (see CMove::MotionTaskBlendType)
	fwMvClipSetId		m_clipSetId;
	bool				m_bOutOfWater;
	bool				m_bSwimNearSurface;
	bool				m_bSwimNearSurfaceAllowDeeperThanWaves;
	bool				m_bSurviveBeingOutOfWater;
	bool				m_bForceFastPitching;
	
	// PURPOSE:  Cachce off MoVE signals for timeslicing.
	bool				m_bMoveInTargetState;
	bool				m_bUseGaitlessRateBoost;
};

#endif // TASK_FISH_LOCOMOTION_H
