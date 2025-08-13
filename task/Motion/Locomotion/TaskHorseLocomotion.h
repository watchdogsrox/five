#ifndef TASK_HORSE_LOCOMOTION_H
#define TASK_HORSE_LOCOMOTION_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Peds/ped.h"
#include "Task/Motion/TaskMotionBase.h"

struct tGameplayCameraSettings;


//*********************************************************************
//	CTaskHorseLocomotion
//	The top-level motion task for the on-foot moveblender for a horse. 
//*********************************************************************

class CTaskHorseLocomotion
	: public CTaskMotionBase
{
public:
	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();
		
		// ProcessPhysics constants.
		float m_InMotionAlignmentVelocityTolerance;
		float m_InMotionTighterTurnsVelocityTolerance;

		// Locomotion turning
		float m_ProcessPhysicsApproachRate;

		// Time slicing
		float m_DisableTimeslicingHeadingThresholdD;
		float m_LowLodExtraHeadingAdjustmentRate;
		
		// PURSUIT MODE
		float	m_PursuitModeGallopRateFactor;
		float	m_PursuitModeExtraHeadingRate;

		// STOPPING
		float	m_StoppingDistanceWalkMBR;
		float	m_StoppingDistanceRunMBR;
		float	m_StoppingDistanceGallopMBR;
		float	m_StoppingGotoPointRemainingDist;

		PAR_PARSABLE;
	};

	CTaskHorseLocomotion( u16 initialState, u16 initialSubState, u32 blendType, const fwMvClipSetId &clipSetId);
	virtual ~CTaskHorseLocomotion();

	enum
	{
		State_Initial,
		State_Idle, 
		State_TurnInPlace,
		State_Locomotion,
		State_Stop,
		State_QuickStop,
		State_RearUp,
		State_Slope_Scramble,
		State_Dead,
		State_DoNothing
	};

	enum QuickStopDirection
	{
		QUICKSTOP_FORWARD,
		QUICKSTOP_RIGHT,
		QUICKSTOP_LEFT		
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool		ProcessMoveSignals();

	// PURPOSE: Manipulate the animated velocity to achieve tighter turns.
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual CTask * CreatePlayerControlTask();

	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*	Copy() const { return rage_new CTaskHorseLocomotion(m_initialState, m_initialSubState, m_blendType, m_clipSetId); }
	virtual s32		GetTaskTypeInternal() const { return CTaskTypes::TASK_ON_FOOT_HORSE; }

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
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint: 
		case CPedMotionStates::MotionState_Dead:
		case CPedMotionStates::MotionState_DoNothing:
			return true;
		default:
			return false;
		}
	}

	bool IsInMotionState(CPedMotionStates::eMotionState state) const;

	//Synch rider
	bool GetHorseGaitPhaseAndSpeed(Vector4& phase, float& speed, float& turnPhase, float &slopePhase);

	virtual bool IsOnFoot() { return true; }

	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);
	virtual Vec3V_Out CalcDesiredAngularVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep);

	virtual	bool CanFlyThroughAir()	{ return false; }

	virtual bool ShouldStickToFloor() { return true; }

	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);

	virtual	bool IsInMotion(const CPed * UNUSED_PARAM(pPed)) const { return GetState()==State_Locomotion; }

	float GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive = NULL);


	void RequestQuickStop(QuickStopDirection eDir = QUICKSTOP_FORWARD) {m_bQuickStopRequested=true; m_eQuickStopDirection = eDir;}
	void RequestCancelQuickStop() {m_bCancelQuickStopRequested=true;}	
	void RequestRearUp(bool buckRider=false) {m_bRearUpRequested=true; m_bRearUpBuckRiderRequested=buckRider;}
	bool IsRearingUp() const { return GetState() == State_RearUp || m_bRearUpRequested; }

	void RequestOverSpur() { m_bOverSpurRequested = true; }

	virtual const CMoveNetworkHelper*	GetMoveNetworkHelper() const	{ return &m_networkHelper; }
	virtual CMoveNetworkHelper*			GetMoveNetworkHelper()			{ return &m_networkHelper; }

	float GetTurnPhase() { return m_TurnPhase; }

	static const Tunables& GetTunables() { return sm_Tunables; }

private:
	FSM_Return StateInitial_OnEnter(); 
	FSM_Return StateInitial_OnUpdate(); 

	FSM_Return StateIdle_OnEnter(); 
	FSM_Return StateIdle_OnUpdate(); 

	FSM_Return StateTurnInPlace_OnEnter();
	FSM_Return StateTurnInPlace_OnUpdate();
	
	FSM_Return StateLocomotion_OnEnter(); 
	FSM_Return StateLocomotion_OnUpdate(); 

	FSM_Return StateStop_OnEnter();
	FSM_Return StateStop_OnUpdate();

	FSM_Return StateQuickStop_OnEnter(); 
	FSM_Return StateQuickStop_OnUpdate(); 

	FSM_Return StateRearUp_OnEnter(); 
	FSM_Return StateRearUp_OnUpdate(); 

	void	   StateSlopeScramble_OnEnter();
	FSM_Return StateSlopeScramble_OnUpdate();

	void	   StateDead_OnEnter();
	FSM_Return StateDead_OnUpdate();
	void	   StateDead_OnProcessMoveSignals();
	
	void ProcessGaits();
	void SendLocomotionMoveSignals(CPed* pPed);

	bool			CanBeTimesliced() const;
	bool			ShouldForceNoTimeslicing() const;

	// Look at the ped specific tuning and return how long it takes for this qped to switch out gaits.
	float GetTimeBetweenGaits(float fCurrentGait, float fNextGait) const;

	void SetIntroClipandBlend();
	bool ShouldStopMoving();
	bool HasStops();
	bool HasQuickStops();

	// Quad moves faster and has tighter turning than normal while chasing down a high priority target.
	void			SetUsePursuitMode(bool bPursuit)			{ m_bPursuitMode = bPursuit; }
	bool			GetIsUsingPursuitMode() const				{ return m_bPursuitMode; }

	bool			ShouldScaleTurnVelocity(float fDesiredHeadingChange, float fAnimationRotateSpeed) const;
	float			GetTurnVelocityScaleFactor() const;

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClipSet = m_clipSetId;
		outClip = CLIP_IDLE;
	}

	virtual float CalcDesiredDirection() const;
	bool ShouldUseTighterTurns() const;
	void UpdateSlopePhase();

	bool GetIsCarryingPlayer() const;

	void AddIdleNetwork(CPed* pPed);

	CMoveNetworkHelper m_networkHelper;	// Loads the parent move network 
	CMoveNetworkHelper m_IdleNetworkHelper;

	CGetupSetRequestHelper m_HorseGetupReqHelper; // For streaming in blend from NM anims

	float m_fQuickStopMaxSpeed;

	u16 m_initialState;
	u16 m_initialSubState;
	u32 m_blendType;		//corresponds to the motion task network blend type. (see fwMove::MotionTaskBlendType)
	fwMvClipSetId m_clipSetId;

	float m_CurrentSpeed;		
	float m_TurnPhase;
	float m_TurnPhaseGoal;
	float m_fTurnHeadingLerpRate;
	float m_LeftToRightGaitBlendPhase;
	float m_SlopePhase;
	float m_fNextGait;
	float m_fCurrentGait;
	float m_fTransitionPhase;
	float m_fDesiredSpeed;
	float m_fSustainedRunTime;
	float m_fInitialDirection;
	float m_fRumbleDelay;

	bool  m_bOverSpurRequested : 1;
	bool  m_bQuickStopRequested : 1;
	bool  m_bCancelQuickStopRequested : 1;
	bool  m_bRearUpRequested : 1;
	bool  m_bRearUpBuckRiderRequested : 1;
	bool  m_bIdlesRunning : 1;
	bool  m_bInTargetMotionState : 1;
	bool  m_bPursuitMode : 1; // Quad moves faster and has tighter turning than normal while chasing down a high priority target.
	bool  m_bMovingForForcedMotionState : 1;

	QuickStopDirection m_eQuickStopDirection;

	static const fwMvStateId ms_LocomotionStateId;
	static const fwMvStateId ms_IdleStateId;
	static const fwMvStateId ms_TurnInPlaceStateId;
	static const fwMvStateId ms_NoClipStateId;

	static const fwMvRequestId ms_startIdleId;
	static const fwMvRequestId ms_startLocomotionId;
	static const fwMvRequestId ms_startStopId;
	static const fwMvRequestId ms_TurninPlaceId;
	static const fwMvRequestId ms_QuickStopId;
	static const fwMvRequestId ms_RearUpId;	
	static const fwMvRequestId ms_OverSpurId;
	static const fwMvRequestId ms_CancelQuickStopId;	
	static const fwMvRequestId ms_InterruptAnimId;
	static const fwMvRequestId ms_StartNoClipId;

	static const fwMvBooleanId ms_OnEnterIdleId;
	static const fwMvBooleanId ms_OnEnterStopId;
	static const fwMvBooleanId ms_OnEnterLocomotionId;
	static const fwMvBooleanId ms_OnEnterQuickStopId;
	static const fwMvBooleanId ms_QuickStopClipEndedId;	
	static const fwMvBooleanId ms_OnEnterRearUpId;
	static const fwMvBooleanId ms_RearUpClipEndedId;	
	static const fwMvBooleanId ms_OnEnterTurnInPlaceId;
	static const fwMvBooleanId ms_OnEnterNoClipId;

	static const fwMvFloatId ms_AnimPhaseId;
	static const fwMvFloatId ms_MoveDesiredSpeedId;
	static const fwMvFloatId ms_MoveTurnPhaseId;
	static const fwMvFloatId ms_MoveSlopePhaseId;
	static const fwMvFloatId ms_MoveWalkPhaseId;	
	static const fwMvFloatId ms_MoveTrotPhaseId;	
	static const fwMvFloatId ms_MoveCanterPhaseId;	
	static const fwMvFloatId ms_MoveGallopPhaseId;	
	static const fwMvFloatId ms_MoveWalkRateId;	
	static const fwMvFloatId ms_MoveTrotRateId;	
	static const fwMvFloatId ms_MoveCanterRateId;	
	static const fwMvFloatId ms_MoveGallopRateId;	
	static const fwMvFloatId ms_MoveLeftToRightControlId;
	static const fwMvFloatId ms_MoveQuickStopDirectionId;

	static const fwMvFloatId ms_IntroBlendId;
	static const fwMvClipId  ms_IntroClip0Id;
	static const fwMvClipId  ms_IntroClip1Id;

	static const fwMvFlagId ms_NoIntroFlagId;	

	// Instance of tunable task params
	static Tunables sm_Tunables;
};

//-------------------------------------------------------------------------
// Task to control the mount's clips whilst mounting
//-------------------------------------------------------------------------
class CTaskRiderRearUp : public CTaskFSMClone
{	
public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_ThrownRiderNMTaskTimeMin;
		float m_ThrownRiderNMTaskTimeMax;

		PAR_PARSABLE;
	};

	// FSM states
	enum eRiderRearUpState
	{
		State_Start,
		State_RearUp,
		State_DetachedFromHorse,
		State_Finish
	};

public:

	CTaskRiderRearUp(bool bBuckOff); 
	virtual ~CTaskRiderRearUp() {}

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskRiderRearUp(m_bBuckOff); }
	virtual int			GetTaskTypeInternal() const					{ return CTaskTypes::TASK_RIDER_REARUP; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Clone task implementation
	// When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	// State Machine Update Functions	
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool	ProcessPostMovement();


	// Local state function calls
	FSM_Return			Start_OnUpdate();
	FSM_Return			RearUp_OnUpdate();
	void				DetachedFromHorse_OnEnter();
	FSM_Return			DetachedFromHorse_OnUpdate();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	bool m_bBuckOff;
	CPlayAttachedClipHelper	m_PlayAttachedClipHelper;

	// Instance of tunable task params
	static Tunables sm_Tunables;
};

#endif // ENABLE_HORSE

#endif // TASK_HORSE_LOCOMOTION_H
