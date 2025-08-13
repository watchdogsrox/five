#ifndef TASK_LOCOMOTION_H
#define TASK_LOCOMOTION_H

#include "Peds/ped.h"
#include "Task/Motion/TaskMotionBase.h"

#define USE_LINEAR_INTERPOLATION 1

//*********************************************************************
//	CTaskMotionBasicLocomotion
//	The basic movement for a ped, which runs a MoVE network consisting
//	of walk/run/sprint clips.
//*********************************************************************

class CTaskMotionBasicLocomotion :
	public CTaskMotionBase
{
public:	
	
	CTaskMotionBasicLocomotion( u16 initialState, const fwMvClipSetId &clipSetId, bool isCrouching = false );
	virtual ~CTaskMotionBasicLocomotion();

	enum
	{
		State_Initial,
		State_Idle,
		State_IdleTurn,
		State_MoveStartRight,
		State_MoveStartLeft,
		State_ChangeDirection,
		State_Locomotion,
		State_MoveTurn,
		State_MoveStop,
		State_Finish
	};

	enum
	{
		Flag_Crouching = BIT1
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool	ProcessPostMovement();

	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask*	Copy() const { return rage_new CTaskMotionBasicLocomotion((u16)GetState(), m_clipSetId, m_flags.IsFlagSet(Flag_Crouching)); }
	virtual s32		GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION; }

	// FSM optional functions
	virtual	void	CleanUp();

	// helper method to start the move network off
	bool StartMoveNetwork();

	void SendWeaponSignals();

	CMoveNetworkHelper* GetMoveNetworkHelper(){ return  &m_moveNetworkHelper; }
	const CMoveNetworkHelper*	GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }

	inline bool SupportsMotionState(CPedMotionStates::eMotionState state);

	bool IsInMotionState(CPedMotionStates::eMotionState state) const;

protected:

	void SendAlternateWalkSignals();

public:

	inline bool IsStartingToMove() const { return GetState()==State_MoveStartRight || GetState()==State_MoveStartLeft; }

	inline bool GetIsStopping() const { return GetState()==State_MoveStop; }

	// TODO - implement this properly!
	inline bool IsPerformingIdleTurn() const { return IsStartingToMove() && GetPed()->GetVelocity().Mag2()<0.01f; }

	virtual bool IsOnFoot() { return true; }

	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	virtual	bool CanFlyThroughAir()	{ return false; }

	virtual bool ShouldStickToFloor() { return true; }

	virtual	bool IsInMotion(const CPed * pPed) const;

	static float CalcVelChangeLimit(const CPed * pPed, float fTimestep);

	static void ClampVelocityChange(const CPed * pPed, Vector3& vChangeInAndOut, float fTimestep, float fChangeLimit);

	CTask* CreatePlayerControlTask();

	u8 GetWhichFootIsOnGround()
	{
		if ( m_moveNetworkHelper.GetFootstep(ms_AefFootHeelLId) )
		{
			return 1;
		}
		else if ( m_moveNetworkHelper.GetFootstep(ms_AefFootHeelRId) )
		{
			return 0;
		}
		else
		{
			return m_lastFootRight ? 0:1;
		}
	}

	void SetMovementClipSet(const fwMvClipSetId &clipSetId) { m_clipSetId = clipSetId; if (m_moveNetworkHelper.IsNetworkActive()) m_moveNetworkHelper.SetClipSet(clipSetId); }
	void SetWeaponClipSet(const fwMvClipSetId &clipSetId, atHashWithStringNotFinal filter, const CPed* pPed);

	virtual float GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive = NULL);
	float CalculateRemainingMovementDistance() const;

	fwMvClipSetId GetClipSet() { return m_clipSetId; }

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
	{
		outClip = CLIP_IDLE;
		outClipSet = m_clipSetId;
	}

	void StartAlternateClip(AlternateClipType type, sAlternateClip& data, bool bLooping);
	void EndAlternateClip(AlternateClipType type, float fBlendDuration);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif //!__FINAL

#if __BANK
	void DebugRenderHeading(float angle, Color32 color, bool pedRelative) const;
#endif //__BANK
	
	static void ProcessMovement(CPed* pPed, float fTimeStep);

private:

	FSM_Return		StateInitial_OnEnter();
	FSM_Return		StateInitial_OnUpdate();

	FSM_Return		StateIdle_OnEnter();
	FSM_Return		StateIdle_OnUpdate();
	FSM_Return		StateIdle_OnExit();

	FSM_Return		StateChangeDirection_OnUpdate();

	FSM_Return		StateMoveStartRight_OnEnter();
	FSM_Return		StateMoveStartRight_OnUpdate();
	FSM_Return		StateMoveStartRight_OnExit();

	FSM_Return		StateMoveStartLeft_OnEnter();
	FSM_Return		StateMoveStartLeft_OnUpdate();
	FSM_Return		StateMoveStartLeft_OnExit();

	FSM_Return		StateLocomotion_OnEnter();
	FSM_Return		StateLocomotion_OnUpdate();
	FSM_Return		StateLocomotion_OnExit();

	FSM_Return		StateMoveTurn_OnEnter();
	FSM_Return		StateMoveTurn_OnUpdate();
	FSM_Return		StateMoveTurn_OnExit();

	FSM_Return		StateMoveStop_OnEnter();
	FSM_Return		StateMoveStop_OnUpdate();
	FSM_Return		StateMoveStop_OnExit();

	FSM_Return		StateFinish_OnEnter();
	FSM_Return		StateFinish_OnUpdate();

	void CalcRatios(const float fMBR, float & fWalkRatio, float & fRunRatio, float & fSprintRatio);

public:
	// PURPOSE: Convert an angle in radians to a MoVE signal mapping the range
	//			-PI->PI to 0.0f->1.0f (0.0f becomes 0.5f)
	inline static float ConvertRadianToSignal(float angle){ angle = Clamp( angle/PI, -1.0f, 1.0f);	return ((-angle * 0.5f) + 0.5f); }

	// PURPOSE: Interpolates angle to target angle in the shortest direction, based on the rate provided. Will not wrap from -PI->PI
	//			Or visa versa, but will instead stop at -PI or PI. This is a useful method when dealing with 360 degree turn systems
	//			in MoVE when they are not animated to wrap around these values correctly)
	inline static void LerpAngleNoWrap(float& angle, float target, float lerpRate)
	{
		InterpValue(angle, target, lerpRate, true);

		angle = Clamp(angle, -PI, PI);
	}

	static float InterpValue(float& current, float target, float interpRate, bool angular = false);

private:

	void SendParams();
	bool SyncToMoveState();

	// sends the necessary signals to enter the move start state (based on desired directions / etc
	void StartMoving();

	//sends the necessary signals to start the 180 degree move turn
	void StartMoveTurn();

	// Calculates the turn direction needed to be passed to the 180 turn motion tree to make the desired
	// heading, given the amount of turn remaining in the clips. Returns false if the turn cannot be made
	// in the remainder of the clip.
	bool AdjustTurnForDesiredDirection( float& requiredTurn );

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	float m_moveDirection;							// Used to store the current direction in 360 turn motion trees. We need this to lerp the direction value over time
	float m_referenceDirection;						// Used when interpolating direction values. Stores the heading of the ped at the start of a directional turn
	float m_referenceDesiredDirection;				// Used to see if the desired direction changes during the turn clip.  Stores the desired heading of the ped at the start of a directional turn

	float m_smoothedDirection;						// Used to smooth the direction parameter over time.
	float m_smoothedSlopeDirection;

	float m_transitionTimer;						// Used to stop rapid oscillations between states - should be removed once this functionality is available in MoVE

	float m_previousDesiredSpeed;					// Stores the previous frames desired forward move blend ratio

	u16 m_initialState;								// Used to determine the state we'll force the move network into on startup.

	u16 m_moveStartWaitCounter;						//TEMPORARY -	Stops the move start from being called until a certain number of frames have passed.

	fwMvClipSetId m_clipSetId;						// main clip clipSetId for ped locomotion
	fwMvClipSetId m_weaponClipSet;					// Clip clipSetId to use for synced partial weapon clips
													// If 0, no weapon clip clipSetId will be applied

	crFrameFilterMultiWeight* m_pWeaponFilter;		// The filter to be used for upper body weapon clips 

	fwFlags16 m_flags;

	bool m_bShouldUpdateMoveTurn : 1;
	bool m_lastFootRight :1;

	static const fwMvBooleanId ms_AefFootHeelLId;
	static const fwMvBooleanId ms_AefFootHeelRId;
	static const fwMvBooleanId ms_AlternateWalkFinishedId;
	static const fwMvBooleanId ms_FinishedIdleTurnId;
	static const fwMvBooleanId ms_LoopedId;
	static const fwMvBooleanId ms_OnEnterIdleId;
	static const fwMvBooleanId ms_OnEnterMoveTurnId;
	static const fwMvBooleanId ms_OnEnterLocomotionId;
	static const fwMvBooleanId ms_OnExitLocomotionId;
	static const fwMvBooleanId ms_OnExitMoveStartLId;
	static const fwMvBooleanId ms_OnExitMoveStartRId;
	static const fwMvBooleanId ms_OnExitMoveStopId;
	static const fwMvBooleanId ms_OnExitMoveTurnId;
	static const fwMvBooleanId ms_ResetTransitionTimerId;
	static const fwMvClipId ms_ClipId;
	static const fwMvClipId ms_IdleId;
	static const fwMvClipId ms_IdleTurnClip0Id;
	static const fwMvClipId ms_IdleTurnClip0LId;
	static const fwMvClipId ms_RunId;
	static const fwMvClipId ms_WalkId;
	static const fwMvFilterId ms_WeaponFilterId;
	static const fwMvFlagId ms_AlternateWalkFlagId;
	static const fwMvRequestId ms_AlternateWalkChangedId;
	static const fwMvFlagId ms_BlockMoveStopId;
	static const fwMvFlagId ms_HasDesiredVelocityId;
	static const fwMvFlagId ms_LastFootRightId;
	static const fwMvFlagId ms_LocomotionVisibleId;
	static const fwMvFlagId ms_MoveStartLeftVisibleId;
	static const fwMvFlagId ms_MoveStartRightVisibleId;
	static const fwMvFlagId ms_MoveStopVisibleId;
	static const fwMvFlagId ms_MoveTurnVisibleId;
	static const fwMvFlagId ms_IsPlayerControlledId;
	static const fwMvFloatId ms_DesiredDirectionId;
	static const fwMvFloatId ms_DesiredSpeedId;
	static const fwMvFloatId ms_DirectionId;
	static const fwMvFloatId ms_DurationId;
	static const fwMvFloatId ms_EndDirectionId;
	static const fwMvFloatId ms_WalkStopRateId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_StartDirectionId;
	static const fwMvFloatId ms_StartDirectionLeftId;
	static const fwMvFloatId ms_StartDirectionToDesiredDirectionId;
	static const fwMvFloatId ms_StartDirectionToDesiredDirectionLeftId;
	static const fwMvFloatId ms_TransitionTimerId;
	static const fwMvFloatId ms_WeaponWeightId;
	static const fwMvClipId ms_MoveStopClip;
	static const fwMvFloatId ms_MoveStopPhase;
	static const fwMvFloatId ms_MoveRateOverrideId;
	static const fwMvFloatId ms_SlopeDirectionId;
	static const fwMvFlagId ms_StairWalkFlagId;
	static const fwMvFlagId ms_SlopeWalkFlagId;


public:
	static dev_float ms_fPedMoveAccel;
	static dev_float ms_fPedMoveDecel;
	static dev_float ms_fPlayerMoveAccel;
	static dev_float ms_fPlayerMoveDecel;
};

//*********************************************************************
//	CTaskMotionBasicLocomotionLowLod
//	The basic movement for a ped, which runs a MoVE network consisting
//	of walk/run/sprint clips.
//	Lodded version.
//*********************************************************************

class CTaskMotionBasicLocomotionLowLod :
	public CTaskMotionBase
{
public:	

	enum State
	{
		State_Initial,
		State_Idle,
		State_Locomotion,
	};

	CTaskMotionBasicLocomotionLowLod(State initialState, const fwMvClipSetId& clipSetId);
	virtual ~CTaskMotionBasicLocomotionLowLod();

	//
	// aiTask API
	//

	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION_LOW_LOD; }
	virtual aiTask*	Copy() const { return rage_new CTaskMotionBasicLocomotionLowLod((State)GetState(), m_clipSetId); }

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	//
	// CTask API
	//

	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_moveNetworkHelper; }

	//
	// Motion Base API
	//
	virtual bool IsOnFoot() { return true; }
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state);
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const;
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds);
	virtual	bool IsInMotion(const CPed* pPed) const;
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual CTask* CreatePlayerControlTask();

	//
	// Locomotion API
	//

	void SetMovementClipSet(const fwMvClipSetId& clipSetId);
	void SetWeaponClipSet(const fwMvClipSetId &clipSetId, atHashWithStringNotFinal filter, const CPed* pPed);
	const fwMvClipSetId& GetMovementClipSetId() const { return m_clipSetId; }

#if __ASSERT
	static void VerifyLowLodMovementClipSet(const fwMvClipSetId &clipSetId, const CPed *pPed);
#endif	//__ASSERT

protected:

	//
	// aiTask API
	//

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();
	virtual	void CleanUp();
	virtual s32	GetDefaultStateAfterAbort() const { return State_Initial; }

private:

	//
	// States
	//

	FSM_Return StateInitial_OnUpdate();

	FSM_Return StateIdle_OnEnter();
	FSM_Return StateIdle_OnUpdate();
	FSM_Return StateIdle_OnExit();

	FSM_Return StateLocomotion_OnEnter();
	FSM_Return StateLocomotion_OnUpdate();
	FSM_Return StateLocomotion_OnExit();

	//
	// Movement
	//

	void UpdateHeading();

	//
	// MoVE
	//

	void SendParams();
	void SendMovementClipSet();

	void SendWeaponSignals();

	//
	// Members
	//

	// Move helper
	CMoveNetworkHelper m_moveNetworkHelper;

	// Main clip clipSetId for ped locomotion
	fwMvClipSetId m_clipSetId;

	// Clip clipSetId to use for synced partial weapon clips,  If 0, no weapon clip clipSetId will be applied
	fwMvClipSetId m_weaponClipSet;					
	
	// The filter to be used for upper body weapon clips 
	crFrameFilterMultiWeight* m_pWeaponFilter;		

	// Clip set ID that the speeds in m_cachedSpeeds belong to
	fwMvClipSetId m_clipSetIdForCachedSpeeds;

	// Cached speeds, from last call to GetMoveSpeeds()
	CMoveBlendRatioSpeeds m_cachedSpeeds;

	// Used to determine the state we'll force the move network into on startup.
	State m_initialState;

	// The current extra heading rate
	float m_fExtraHeadingRate;

	// Does the clipset need updating?
	bool m_bMovementClipSetChanged : 1;

	// Have we set the initial phase?
	bool m_bMovementInitialPhaseSet : 1;

	//
	// Statics
	//

public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float MovingExtraHeadingChangeRate;
		float MovingExtraHeadingChangeRateAcceleration;

		// PURPOSE:	Heading delta at which we should start to force full updates (disable timeslicing), in radians.
		float	m_ForceUpdatesWhenTurningStartThresholdRadians;

		// PURPOSE:	Heading delta at which we should stop to force full updates (disable timeslicing), in radians.
		float	m_ForceUpdatesWhenTurningStopThresholdRadians;

		PAR_PARSABLE;
	};

	// Instance of tunable task params
	static Tunables ms_Tunables;	

	// PURPOSE : Heading threshold below which low physics & motion LOD peds should not move
	static dev_float ms_fHeadingMoveThreshold;

private:

	//
	// Move Signal Id's
	//

	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_MovementRateId;
	static const fwMvFloatId ms_MovementInitialPhaseId;
	static const fwMvClipId ms_WeaponIdleId;
	static const fwMvClipId ms_WeaponRunId;
	static const fwMvClipId ms_WeaponWalkId;
	static const fwMvFilterId ms_WeaponAnimFilterId;
	static const fwMvFloatId ms_WeaponAnimWeightId;
};

#endif // TASK_LOCOMOTION_H
