#ifndef TASK_BIRD_LOCOMOTION_H
#define TASK_BIRD_LOCOMOTION_H

#include "math/angmath.h"

#include "Task/Motion/TaskMotionBase.h"
#include "Task/System/Tuning.h"

//*********************************************************************
//	CTaskBirdLocomotion
//	The top-level motion task for the on-foot moveblender for a bird. 
//*********************************************************************

class CTaskBirdLocomotion
	: public CTaskMotionBase
{
public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// Takeoff
		u32		m_MinWaitTimeBetweenTakeOffsMS;
		u32		m_MaxWaitTimeBetweenTakeOffsMS;
		float	m_MinTakeOffRate;
		float	m_MaxTakeOffRate;
		float	m_MinTakeOffHeadingChangeRate;
		float	m_MaxTakeOffHeadingChangeRate;
		float	m_DefaultTakeoffBlendoutPhase;

		// Flapping
		float	m_TimeToFlapMin;
		float	m_TimeToFlapMax;

		// Timeslicing
		float	m_NoAnimTimeslicingShadowRange;				// Distance to the shadow segment at which we check the shadow visibility to disable timeslicing.
		float	m_ForceNoTimeslicingHeadingDiff;

		// Stuck checks
		float	m_MinDistanceFromPlayerToDeleteStuckBird;
		float	m_TimeUntilDeletionWhenStuckOffscreen;
		float	m_TimeWhenStuckToIgnoreBird;

		// Walking
		float	m_HighLodWalkHeadingLerpRate;
		float	m_LowLodWalkHeadingLerpRate;
		float	m_PlayerWalkCapsuleRadius;

		// Turning
		float	m_AIBirdTurnApproachRate;
		float	m_PlayerGlideIdealTurnRate;
		float	m_PlayerGlideToZeroTurnRate;
		float	m_PlayerGlideTurnAcceleration;
		float	m_PlayerHeadingDeadZoneThresholdDegrees;
		float	m_PlayerFlappingTurnAcceleration;
		float	m_PlayerFlappingIdealTurnRate;
		float	m_PlayerFlappingToZeroTurnRate;

		// Pitching
		float m_AIPitchTurnApproachRate;
		float m_PlayerPitchIdealRate;
		float m_PlayerPitchAcceleration;

		// Player speed boost
		float m_PlayerTiltedDownToleranceDegrees;
		float m_PlayerTiltedDownSpeedBoost;
		float m_PlayerTiltedSpeedCap;

		// CAMERAS
		atHashString m_PlayerControlCamera;

		PAR_PARSABLE;
	};


	CTaskBirdLocomotion( u16 initialState, u16 initialSubState, u32 blendType,  const fwMvClipSetId &clipSetId);
	virtual ~CTaskBirdLocomotion();

	enum
	{
		State_Initial,
		State_Idle, 
		State_Walk,
		State_WaitForTakeOff,
		State_TakeOff,
		State_Descend,
		State_Land,
		State_Fly,
		State_Glide,
		State_FlyUpwards,
		State_Dead
	};

	virtual FSM_Return	ProcessPreFSM();

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual FSM_Return	ProcessPostFSM();

	virtual bool		ProcessMoveSignals();

	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	// Birds are able to adjust their pitch in flight, so override this function to allow pitch changes.
	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut) { fMinOut = -ms_fMaxPitch; fMaxOut = ms_fMaxPitch; }

	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*	Copy() const { return rage_new CTaskBirdLocomotion(m_initialState, m_initialSubState, m_blendType, m_clipSetId); }
	virtual s32		GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_BIRD; }

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
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Dead:
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
		case CPedMotionStates::MotionState_Walk:
			return GetState()==State_Walk;
		case CPedMotionStates::MotionState_Dead:
			return GetState() == State_Dead;
		default:
			return false;
		}
	}

	virtual CTask* CreatePlayerControlTask();

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	bool CanLandNow() const;

	void RequestPlayerLand();

	void RequestFlap();

	bool IsGliding() const { return GetState() == State_Glide; }

	bool IsFlapping() const { return GetState() == State_Fly; }

	// Align the bird to slopes - static/public so I can reuse this for the hens...
	static void SetNewGroundPitchForBird(CPed& rPed, float fDT);

	static const Tunables& GetTunables() { return sm_Tunables; }

protected:

	bool ForceMotionState(CPedMotionStates::eMotionState UNUSED_PARAM(state), bool UNUSED_PARAM(shouldRestart))
	{
		return false;
	}

	// Helper functions for adjusting heading.
	void SetNewHeading();
	float CalcDesiredHeading() const;

	// Helper functions for adjusting pitch.
	void SetNewPitch();
	float CalcDesiredPitch() const;

	bool CanBirdTakeOffYet();

private:

	bool IsInWaterWhileLowLod() const;

	void ProcessActiveFlightMode();

	void ProcessInactiveFlightMode();

	void ProcessShadows();

	// Return the ratio of Z to XY translation in the clip.
	float ComputeClipRatio(fwMvClipId clipId);

	// Return true if the bird is sufficiently submerged.
	bool IsBirdUnderwater() const;

	// Look and see if this bird hit anything this frame.
	void CheckForCollisions();

	float GetFlapSpeedScale() const;
	float GetGlideSpeedScale() const;

	void ForceStateChange(const fwMvStateId &targetStateId);

public:

	virtual bool IsOnFoot();

	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds & speeds);

	virtual	bool CanFlyThroughAir();

	virtual bool ShouldStickToFloor() { return true; }

	virtual	bool IsInMotion(const CPed * UNUSED_PARAM(pPed)) const { return GetState()==State_Walk || GetState()==State_TakeOff || GetState()==State_Land || GetState()==State_Fly || GetState()==State_Glide || GetState()==State_FlyUpwards || GetState()==State_Descend; }

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_clipSetId;
		outClip = CLIP_IDLE;
	}

	// Ratio of Z translation to XY translation in the descend clip.
	float GetDescendClipRatio() const { return m_fDescendClipRatio; }

	// Ratio of Z translation to XY translation in the ascend clip.
	float GetAscendClipRatio() const { return m_fAscendClipRatio; }

	//*************************************************************************************************
	// This function can be implemented to increase the radius on movement tasks' completion criteria.
	// It may be useful for movement modes which cause peds to move at extra-fast speeds.
	virtual	float GetExtraRadiusForGotoPoint() const;

	void			StateInitial_OnEnter(); 
	FSM_Return		StateInitial_OnUpdate(); 

	void			StateIdle_OnEnter(); 
	FSM_Return		StateIdle_OnUpdate(); 
	void			StateIdle_OnProcessMoveSignals();
	
	void			StateWalk_OnEnter(); 
	FSM_Return		StateWalk_OnUpdate();
	void			StateWalk_OnProcessMoveSignals();

	FSM_Return		StateWaitForTakeOff_OnUpdate();
	
	void			StateTakeOff_OnEnter(); 
	FSM_Return		StateTakeOff_OnUpdate(); 
	void			StateTakeOff_OnProcessMoveSignals();

	void			StateLand_OnEnter(); 
	FSM_Return		StateLand_OnUpdate();
	void			StateLand_OnProcessMoveSignals();

	void			StateFly_OnEnter();
	FSM_Return		StateFly_OnUpdate();
	void			StateFly_OnProcessMoveSignals();

	void			StateGlide_OnEnter();
	FSM_Return		StateGlide_OnUpdate();
	void			StateGlide_OnProcessMoveSignals();

	void			StateFlyUpwards_OnEnter();
	FSM_Return		StateFlyUpwards_OnUpdate();
	void			StateFlyUpwards_OnProcessMoveSignals();

	void			StateDescend_OnEnter();
	FSM_Return		StateDescend_OnUpdate();
	void			StateDescend_OnProcessMoveSignals();

	FSM_Return		StateDead_OnUpdate();

	void AllowTimeslicing();
	bool ShouldForceNoTimeslicing();

	// Return true if the bird has been stuck long enough for other systems to try and ignore it.
	bool ShouldIgnoreDueToContinuousCollisions();

	// If the bird is offscreen for long enough and stuck, delete it.
	void PossiblyDeleteStuckBird();

	CMoveNetworkHelper m_networkHelper;	// Loads the parent move network 
	CTaskGameTimer m_FlapTimer;

	u16 m_initialState;
	u16 m_initialSubState;
	u32 m_blendType;		//corresponds to the motion task network blend type. (see CMove::MotionTaskBlendType)
	fwMvClipSetId m_clipSetId;
	float m_fTurnDiscrepancyTurnRate; 
	float m_fDesiredTurnForHeadingLerp; 
	float m_fLandClipXYTranslation;
	float m_fLandClipZTranslation;
	float m_fDescendClipRatio;
	float m_fAscendClipRatio;

	// Interpolated turning parameter sent to move.
	float			m_fTurnPhase;

	// Interpolated rate of blending in turns.
	float			m_fTurnRate;

	// Used to affect the turn blend rate.
	float			m_fLastTurnDirection;

	// Rate of changing pitch.
	float			m_fPitchRate;

	// Used to affect the pitch rate.
	float			m_fLastPitchDirection;

	// How to adjust extra heading changes during takeoff.
	float			m_fTurnRateDuringTakeOff;

	// How long this bird has been colliding with something during certain fly states.
	float			m_fTimeWithContinuousCollisions;

	// How long has this bird been offscreen when stuck.
	float			m_fTimeOffScreenWhenStuck;

	bool			m_bWakeUpInProcessPhysics;
	bool			m_bLandWhenAble;
	
	// PURPOSE:  Cache MoVE signals for timeslicing.
	bool			m_bInTargetMotionState;

	// PURPOSE:  Check if the takeoff animation is in a good spot for blending.
	bool			m_bTakeoffAnimationCanBlendOut;

	// PURPOSE:  Handle requests for flapping by external systems.
	bool			m_bFlapRequested;


	static const fwMvRequestId ms_startIdleId;
	static const fwMvBooleanId ms_OnEnterIdleId;
	static const fwMvStateId   ms_IdleId;

	static const fwMvRequestId ms_startWalkId;
	static const fwMvBooleanId ms_OnEnterWalkId;
	static const fwMvStateId   ms_WalkId;

	static const fwMvRequestId ms_startTakeOffId;
	static const fwMvBooleanId ms_OnEnterTakeOffId;
	static const fwMvFloatId ms_TakeOffPhaseId;

	static const fwMvRequestId ms_startDescendId;
	static const fwMvBooleanId ms_OnEnterDescendId;

	static const fwMvRequestId ms_startLandId;
	static const fwMvBooleanId ms_OnEnterLandId;
	static const fwMvBooleanId ms_OnExitLandId;

	static const fwMvRequestId ms_startFlyId;
	static const fwMvBooleanId ms_OnEnterFlyId;
	static const fwMvStateId   ms_FlyId;
	static const fwMvRequestId ms_startGlideId;
	static const fwMvBooleanId ms_OnEnterGlideId;
	static const fwMvStateId   ms_GlideId;

	static const fwMvRequestId ms_startFlyUpwardsId;
	static const fwMvBooleanId ms_OnEnterFlyUpwardsId;

	static const fwMvFloatId ms_DirectionId;
	static const fwMvFloatId ms_TakeOffRateId;
	static const fwMvBooleanId ms_CanEarlyOutForMovement;

	static dev_float ms_fMaxPitch;
	static dev_float ms_fMaxPitchPlayer;
	static u32 ms_uPlayerFlapTimeMS;

	// PURPOSE:  When the next bird is allowed to take off (to space out their anims and give a better audio effect).
	static u32 sm_TimeForNextTakeoff;

private:

	// Instance of tunable task params
	static Tunables sm_Tunables;
};

#endif // !TASK_BIRD_LOCOMOTION_H
