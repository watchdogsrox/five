#pragma once

#if !__FINAL
#define ENABLE_MOTION_IN_TURRET_TASK 1
#else
#define ENABLE_MOTION_IN_TURRET_TASK 1
#endif

#if ENABLE_MOTION_IN_TURRET_TASK
# define MOTION_IN_TURRET_TASK_ONLY(x) x
#else 
# define MOTION_IN_TURRET_TASK_ONLY(x) 
#endif

#if ENABLE_MOTION_IN_TURRET_TASK

// Game headers
#include "Task/Motion/Vehicle/TaskMotionInTurretMoveNetworkMetadata.h"
#include "Task/Motion/TaskMotionBase.h"

////////////////////////////////////////////////////////////////////////////////
//
// PURPOSE: Controls motion clips whilst in a turreted vehicle seat
//			must be run as a subtask of CTaskMotionInVehicle
//
////////////////////////////////////////////////////////////////////////////////

class CTaskMotionInTurret : public CTaskMotionBase
{
public:

	// Define the list of task states
#undef  TASK_STATES
#define TASK_STATES(X)		X(State_Start),\
							X(State_InitialAdjust),\
							X(State_StreamAssets),\
							X(State_Idle),\
							X(State_Turning),\
							X(State_Shunt),\
							X(State_Finish)

	// Define the task state enum list
	enum eTaskState
	{
		TASK_STATES(DEFINE_TASK_ENUM)
	};

	// A struct containing per-task tunable parameters
	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinAnimPitch;
		float m_MaxAnimPitch;
		float m_DeltaHeadingDirectionChange;
		float m_DeltaHeadingTurnThreshold;
		float m_MinHeadingDeltaTurnSpeed;
		float m_MaxHeadingDeltaTurnSpeed;
		float m_PitchApproachRate;
		float m_TurnSpeedApproachRate;
		float m_MinTimeInTurnState;
		float m_MinTimeInIdleState;
		float m_MinTimeInTurnStateAiOrRemote;
		float m_MinTimeInIdleStateAiOrRemote;
		float m_MinHeadingDeltaToAdjust;
		float m_PlayerTurnControlThreshold;
		float m_RemoteOrAiTurnControlThreshold;
		float m_RemoteOrAiPedTurnDelta;
		float m_RemoteOrAiPedStuckThreshold;
		float m_TurnVelocityThreshold;
		float m_TurnDecelerationThreshold;
		float m_NoTurnControlThreshold;
		float m_MinTimeTurningForWobble;
		float m_SweepHeadingChangeRate;
		float m_MinMvPitchToPlayAimUpShunt;
		float m_MaxMvPitchToPlayAimDownShunt;
		float m_MaxAngleToUseDesiredAngle;
		float m_EnterPitchSpeedModifier;
		float m_EnterHeadingSpeedModifier;
		float m_ExitPitchSpeedModifier;
		float m_DelayMovementDuration;
		float m_InitialAdjustFinishedHeadingTolerance;
		float m_InitialAdjustFinishedPitchTolerance;
		float m_LookBehindSpeedModifier;
		float m_RestartIdleBlendDuration;

		fwMvClipId m_AdjustStep0LClipId;
		fwMvClipId m_AdjustStep90LClipId;
		fwMvClipId m_AdjustStep180LClipId;
		fwMvClipId m_AdjustStep0RClipId;
		fwMvClipId m_AdjustStep90RClipId;
		fwMvClipId m_AdjustStep180RClipId;
		fwMvClipId m_TurnLeftSlowDownClipId;
		fwMvClipId m_TurnLeftFastDownClipId;
		fwMvClipId m_TurnLeftSlowClipId;
		fwMvClipId m_TurnLeftFastClipId;
		fwMvClipId m_TurnLeftSlowUpClipId;
		fwMvClipId m_TurnLeftFastUpClipId;
		fwMvClipId m_TurnRightSlowDownClipId;
		fwMvClipId m_TurnRightFastDownClipId;
		fwMvClipId m_TurnRightSlowClipId;
		fwMvClipId m_TurnRightFastClipId;
		fwMvClipId m_TurnRightSlowUpClipId;
		fwMvClipId m_TurnRightFastUpClipId;
		fwMvClipId m_ShuntLeftClipId;
		fwMvClipId m_ShuntRightClipId;
		fwMvClipId m_ShuntFrontClipId;
		fwMvClipId m_ShuntBackClipId;
		fwMvClipId m_ShuntUpClipId;
		fwMvClipId m_ShuntDownClipId;
		fwMvClipId m_TurretFireClipId;

		PAR_PARSABLE;
	};

	// Public task interface
	static const CTaskMotionInTurret::Tunables&			GetTuningValues() { return sm_Tunables; }
	static void											IkHandsToTurret(CPed &rPed, const CVehicle& rVeh, const CTurret& rTurret);
	static bool											HasValidWeapon(const CPed& rPed, const CVehicle& rVeh);
	static bool											IsSeatWithHatchProtection(s32 iSeatIndex, const CVehicle& rVeh);
	static void											KeepTurretHatchOpen(const s32 iSeatIndex, CVehicle& rVeh);
	static bool											ShouldUseAlternateJackDueToTurretOrientation(CVehicle& rVeh, bool bLeftEntry);

	bool												GetCanFireDuringAdjustStep() const;
	bool												GetCanFire() const;
	float												GetAdjustStepMoveRatio() const;
	bool												GetIsSeated() const { return m_bSeated; }
	float												GetInitialTurretHeading() const { return m_fInitialTurretHeading; }
	bool												ShouldHaveControlOfTurret() const;
	bool												IsDucking() const { return m_bIsDucking; }

	virtual bool										ProcessPostCamera();

	CTaskMotionInTurret(CVehicle* pVehicle, CMoveNetworkHelper& rMoveNetworkHelper);
	virtual ~CTaskMotionInTurret();

	// TODO: Remove when integrating to RDR
	virtual s32											GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_IN_TURRET; }

protected:

	// Main FSM task interface
	virtual aiTask*										Copy() const { return rage_new CTaskMotionInTurret(m_pVehicle, m_MoveNetworkHelper); }
	virtual s32											GetDefaultStateAfterAbort() const   { return State_Finish; }
	virtual	void										CleanUp();
	virtual bool										ProcessMoveSignals();
	virtual FSM_Return									ProcessPreFSM();
	virtual FSM_Return									UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return									ProcessPostFSM();

	// Motion task interface
	virtual bool										SupportsMotionState(CPedMotionStates::eMotionState state) { return (state == CPedMotionStates::MotionState_InVehicle) ? true : false;}
	virtual bool										IsInMotionState(CPedMotionStates::eMotionState state) const { return state == CPedMotionStates::MotionState_InVehicle; }
	virtual void										GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds) { speeds.Zero(); }
	virtual	bool										IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }
	virtual void										GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual bool										IsInVehicle() { return true; }
	virtual Vec3V_Out									CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);
	virtual CTask*										CreatePlayerControlTask();

private:

	// Internal FSM interface
	void												Start_OnUpdate();	
	void												InitialAdjust_OnEnter();
	void												InitialAdjust_OnUpdate();
	void												StreamAssets_OnEnter();
	void												StreamAssets_OnUpdate();
	void												Idle_OnEnter();
	void												Idle_OnUpdate();
	void												Turning_OnEnter();
	void												Turning_OnUpdate();
	void												Shunt_OnEnter();
	void												Shunt_OnUpdate();

	fwMvClipId											GetShuntClip();
	bool												GetAdjustStepClips(atArray<const crClip*>& rClipArray) const;
	bool												GetTurningClips(atArray<const crClip*>& rClipArray) const;
	bool												StartMoveNetwork(CMoveNetworkHelper& rMoveNetworkHelper, const CPed& rPed);
	void												ProcessFiring();
	CTurret*											GetTurret();
	fwMvFloatId											GetPhaseIdForAdjustStep() const;
	u32													GetDominantClipForAdjustStep() const;
	float												ComputeHeadingDelta(const CTurret* pTurret, float fTurretHeading, CPed &rPed) const;

	void												UpdateMvPitch(const CTurret* pTurret, const float fTimeStep);
	void												UpdateMvTurnSpeed(const float fTimeStep);
	void												UpdateMvHeading(const CTurret* pTurret, CPed& rPed);
	void												UpdateAngVelocityAndAcceleration(const CTurret* pTurret, const float fTimeStep);
	void												SendMvParams();
	bool												SetSeatClipSet(const CVehicleSeatAnimInfo* pSeatClipInfo);
	bool												AssetsHaveBeenStreamed(const CPed* pPed, const CVehicle* pVehicle);
	bool												IsPlayerTryingToTurn(const CPed& rPed);
	void												SetInitialState();
	bool												ShouldSetAircraftMode(const CPed &rPed);
	bool												WantsToDuck() const;
	bool												CanBeDucked() const;
	fwMvClipSetId										GetDefaultClipSetId() const;
	fwMvClipSetId										GetDuckedClipSetId() const;
	fwMvClipSetId										GetAppropriateClipSetId() const;
	bool												IsInFirstPersonCamera() const;

	// State change helpers
	bool												CheckForTurning(bool& bRestart);
	bool												HasTurnAnims() const;
	bool												ShouldRestartStateBecauseDuckingStateChange();

private:

	RegdVeh												m_pVehicle;
	CMoveNetworkHelper&									m_MoveNetworkHelper;
	float												m_fMvPitch;
	float												m_fMvHeading;
	float												m_fMvTurnSpeed;
	float												m_fInitialHeadingDelta;
	float												m_fInitialTurretHeading;
	float												m_LastFramesTurretHeading;
	float												m_fAngVelocity;
	float												m_fAngAcceleration;
	float												m_fTimeDucked;
	bool												m_bTurningLeft;
	bool												m_bWasTurretMoving;
	bool												m_bSeated;
	bool												m_bSweeps;
	bool												m_bFiring;
	bool												m_bInterruptDelayedMovement;
	bool												m_bCanBlendOutOfCurrentAnimState;
	bool												m_bIsLookingBehind;
	bool												m_bIsDucking;
	Vector3												m_vLastFramesVehicleVelocity;
	Vector3												m_vCurrentVehicleAcceleration;
	static Tunables										sm_Tunables;

public:

#if !__FINAL
	void												Debug() const;
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, TASK_STATES)
#endif // !__FINAL

};

#endif // ENABLE_MOTION_IN_TURRET_TASK
