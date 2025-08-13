// 
// task/motion/Locomotion/taskmotionped.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef TASK_MOTION_PED_H
#define TASK_MOTION_PED_H

#include "task/motion/TaskMotionBase.h"
#include "task/Motion/Locomotion/TaskLocomotion.h"

class CTaskHumanLocomotion;
class CTaskMotionBasicLocomotion;
class CTaskMotionStrafing;

#define __LOG_MOTIONPED_RESTART_REASONS __DEV

class CTaskMotionPed : public CTaskMotionBase
{
public:

	static const float AIMING_TRANSITION_DURATION;

	enum PedMotionFlags
	{
		PMF_PerformStrafeToOnFootTransition	= BIT0,
		PMF_SkipStrafeIntroAnim = BIT1,
		PMF_UseMotionBaseCalcDesiredVelocity = BIT2
	};

	enum State
	{
		State_Start,
		State_OnFoot,
		State_Strafing,
		State_StrafeToOnFoot,
		State_Swimming,
		State_SwimToRun,
		State_DiveToSwim,
		State_Diving,
		State_DoNothing,
		State_AnimatedVelocity,
		State_Crouch,
		State_Stealth,
		State_InVehicle,
		State_Aiming,
		State_StandToCrouch,
		State_CrouchToStand,
		State_ActionMode,
		State_Parachuting,
		State_Jetpack,
#if ENABLE_DRUNK
		State_Drunk,
#endif // ENABLE_DRUNK
		State_Dead,
		State_Exit,
	};

	CTaskMotionPed();
	~CTaskMotionPed();

	virtual aiTask* Copy() const { return rage_new CTaskMotionPed(); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_PED; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Exit; }
	virtual bool MayDeleteOnAbort() const { return true; }	// This task doesn't resume if it gets aborted, should be safe to delete.

#if !__NO_OUTPUT
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);	
	FSM_Return		ProcessPreFSM();	
	FSM_Return		ProcessPostFSM();	
	virtual bool	ProcessPostMovement();

	CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

#if __DEV 
	void SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId);
#endif

	virtual void CleanUp();

	//	PURPOSE: Should return true if the motion task is in an on foot state
	virtual bool IsOnFoot() { int nState = GetState(); return (nState==State_OnFoot || nState==State_Crouch || /*nState==State_Stealth || */nState==State_Strafing || nState==State_StrafeToOnFoot); }
	
	//	PURPOSE: Should return true if the motion task is in a strafing state
	virtual bool IsStrafing() const { return (GetState()==State_Strafing); }
	
	//	PURPOSE: Should return true if the motion task is in a swimming state
	virtual bool IsSwimming() const { return (GetState()==State_Swimming || GetState()==State_DiveToSwim); }

	//	PURPOSE: Should return true if the motion task is in a diving state
	virtual bool IsDiving() const { return (GetState()==State_Diving); }

	// PURPOSE: Should return true if the motion task is in an in water state
	virtual bool IsInWater() { return (GetState()==State_Swimming || GetState() == State_Diving || GetState()==State_DiveToSwim); }

	// PURPOSE: Should return true in derived classes when in a vehicle motion state
	virtual bool IsInVehicle() { return (GetState()==State_InVehicle); }

	virtual	bool IsUnderWater() const { return (GetState()==State_Diving || (GetState() == State_Aiming && GetPed() && GetPed()->GetIsFPSSwimmingUnderwater()));  }

	// PURPOSE: Should return true in derived classes when the moveblender is preserving animated velocity
	virtual bool IsUsingAnimatedVelocity() { return (GetState()==State_AnimatedVelocity); }
	
	// PURPOSE: Should return true in derived classes when in an aiming motion state
	virtual bool IsAiming() const { return (GetState() == State_Aiming); }

    // PURPOSE: Should return true in derived classes when in stealth motion state
	virtual bool IsUsingStealth() const { return (GetState() == State_Stealth); }

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	virtual const crClip* GetBaseIdleClip();

	//*************************************************************************************************************
	// This function takes the ped's animated velocity and modifies it based upon the orientation of the surface underfoot
	// It sets the desired velocity on the ped in world space.
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

	//***********************************************************************************************
	// This function returns whether the ped is allowed to fly through the air in its current state
	virtual	bool CanFlyThroughAir();

	//********************************************************************************************
	// This function returns the default main-task-tree player control task for this motion task
	virtual CTask * CreatePlayerControlTask();

	//*************************************************************************************
	// Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor();

	//*************************************************************************************
	// Check if we can perform a transition from our current state to the network requested target state
	virtual bool IsClonePedMoveNetworkStateTransitionAllowed(u32 const targetState) const;
	virtual bool IsClonePedNetworkStateChangeAllowed(u32 const state);
	virtual s32  ConvertMoveNetworkStateAndClipSetLod(u32 const state, fwMvClipSetId& pendingClipSetId) const;

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const;

	void GetNMBlendOutClip(fwMvClipSetId& outClipSetId, fwMvClipId& outClipId)
	{
		if (GetSubTask())
		{
			smart_cast<CTaskMotionBase*>(GetSubTask())->GetNMBlendOutClip(outClipSetId, outClipId);
		}
		else
		{
			outClipSetId = m_defaultOnFootClipSet;
			outClipId = CLIP_IDLE;
		}
	}

	void SetInstantStrafePhaseSync( float fInstantStrafePhaseSync ) { m_fInstantStrafePhaseSync = fInstantStrafePhaseSync; }
	void SetStrafeDurationOverride( float fStrafeDurationOverride ) { m_fStrafeDurationOverride = fStrafeDurationOverride; }

	void SetBeginMoveStartPhase(float fPhase) { m_fBeginMoveStartPhase = fPhase; }

	inline bool SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
		case CPedMotionStates::MotionState_Crouch_Idle:
		case CPedMotionStates::MotionState_Crouch_Walk:
		case CPedMotionStates::MotionState_Crouch_Run:
		case CPedMotionStates::MotionState_DoNothing:
		case CPedMotionStates::MotionState_AnimatedVelocity:
		case CPedMotionStates::MotionState_InVehicle:
		case CPedMotionStates::MotionState_Aiming:
		case CPedMotionStates::MotionState_Diving_Idle:
		case CPedMotionStates::MotionState_Diving_Swim: 
		case CPedMotionStates::MotionState_Swimming_TreadWater:
		case CPedMotionStates::MotionState_Dead:
		case CPedMotionStates::MotionState_Stealth_Idle:
		case CPedMotionStates::MotionState_Stealth_Walk:
		case CPedMotionStates::MotionState_Stealth_Run:
		case CPedMotionStates::MotionState_Parachuting:
		case CPedMotionStates::MotionState_ActionMode_Idle:
		case CPedMotionStates::MotionState_ActionMode_Walk:
		case CPedMotionStates::MotionState_ActionMode_Run:
		case CPedMotionStates::MotionState_Jetpack:
			return true;
		default:
			return false;
		}
	}

	bool IsInMotionState(CPedMotionStates::eMotionState state) const;


	void		SetTaskFlag		(u32 flag)			{ m_iFlags.SetFlag( flag ); }
	bool		ClearTaskFlag	(u32 flag)			{ return m_iFlags.ClearFlag( flag ); }
	bool		IsTaskFlagSet	(u32 flag) const 	{ return m_iFlags.IsFlagSet( flag ); }
	fwFlags32&	GetTaskFlags	(void)				{ return m_iFlags; }

	void SetDoAimingIndependentMover(const bool bDoAimingIndependentMover) { m_bDoAimingIndependentMover = bDoAimingIndependentMover; }

public:

#if !__LOG_MOTIONPED_RESTART_REASONS
	void SetRestartCurrentStateThisFrame(bool bRestart) { m_restartCurrentStateNextFrame = bRestart; }
#else
	void SetRestartCurrentStateThisFrame(bool bRestart);
#endif // !__LOG_MOTIONPED_RESTART_REASONS
	void SetRestartAimingUpperBodyStateThisFrame(bool bRestart) { m_restartAimingUpperBodyState = bRestart; }

	inline bool IsIdle() const { return (m_isIdle & 1)!=0; }
	
public: //base class overrides
	
	virtual void StealthClipSetChanged(const fwMvClipSetId &clipSetId);
	virtual void InVehicleContextChanged(u32 inVehicleContextHash);

	virtual void OverrideWeaponClipSetCleared();
	
	virtual void AlternateClipChanged(AlternateClipType type);

	virtual bool ProcessMoveSignals();

	void SetNetworkInsertDuration(const float networkInsertDuration) { m_networkInsertDuration = networkInsertDuration; }

#if ENABLE_DRUNK
	void InsertDrunkNetwork(CMoveNetworkHelper& helper);
	void ClearDrunkNetwork(float duration);
#endif // ENABLE_DRUNK

	void SetDiveImpactSpeed(float f) { m_DiveImpactSpeed = f;}
	void SetDivingFromFall() { m_DiveFromFall = true;}

	// PURPOSE: Are we aiming, or are we not blocked from aiming
	bool IsAimingOrNotBlocked() const;
	bool IsAimingTransitionNotBlocked() const;

	// PURPOSE: Disallow transition from on foot states to aiming for the specified time
	void SetBlockAimingTransitionTime(const float fBlockTime) { m_fBlockAimingTransitionTimer = Max(fBlockTime, m_fBlockAimingTransitionTimer); m_blockAimingActive = 2; }

	bool IsAimingIndependentMoverNotBlocked() const;

	void SetBlockAimingTransitionIndependentMover(const float fBlockTime) { m_fBlockAimingIndependentMoverTimer = Max(fBlockTime, m_fBlockAimingIndependentMoverTimer); m_blockAimingIndependentMoverActive = 2; }

	// 
	bool IsOnFootTransitionNotBlocked() const;

	void SetBlockOnFootTransitionTime(const float fBlockTime) { m_fBlockOnFootTransitionTimer = Max(fBlockTime, m_fBlockOnFootTransitionTimer); m_blockOnFootActive = 2; }

	static const CWeaponInfo* GetWeaponInfo(const CPed* pPed);

#if FPS_MODE_SUPPORTED
	// FPS Swim: Used for lerping down the velocity when we stop receiving input in CTaskMotionBase::CalcDesiredVelocity
	void SetCachedSwimVelocity(Vector3 vVelocity) { m_vCachedSwimVelocity = vVelocity; }
	Vector3 GetCachedSwimVelocity() const { return m_vCachedSwimVelocity; }
	void SetCachedSwimStrafeVelocity(Vector3 vVelocity) { m_vCachedSwimStrafeVelocity = vVelocity; }
	Vector3 GetCachedSwimStrafeVelocity() const { return m_vCachedSwimStrafeVelocity; }

	void ProcessFPSIdleFidgets();
	bool CanSafelyPlayFidgetsInFPS(const CPed* pPed);

	int GetFPSFidgetVariation() const { return m_iFPSFidgetVariation; }
	void SetFPSFidgetVariation(int iVal) { m_iFPSFidgetVariation = iVal; }
	bool ProcessCloneFPSIdle(CPed* pPed);

	bool IsPlayingIdleFidgets() const { return m_bPlayingIdleFidgets; }
	bool GetFPSPreviouslySprinting() const { return m_bFPSPreviouslySprinting; }

	// Sets blend time based on current state
	static float SelectFPSBlendTime(CPed *pPed, bool bStateToTransition = true, CPedMotionData::eFPSState prevFPSStateOverride = CPedMotionData::FPS_MAX, CPedMotionData::eFPSState currFPSStateOverride = CPedMotionData::FPS_MAX);
#endif	//FPS_MODE_SUPPORTED

	bool						CheckForDiving();

private:

	void						ApplyWeaponClipSet(CTaskMotionBasicLocomotion* subTask);
	void						ApplyWeaponClipSet(CTaskHumanLocomotion* subTask);
	void						ApplyWeaponClipSet(CTaskMotionStrafing* subTask);
	void						ApplyWeaponClipSet(CTask* subTask);

	// State Functions
	// Start
	FSM_Return					Start_OnUpdate();
	
	// On foot behaviour
	void						OnFoot_OnEnter();
	FSM_Return					OnFoot_OnUpdate();
	void						OnFoot_OnExit();

	void						Crouch_OnEnter();
	FSM_Return					Crouch_OnUpdate();
	void						Crouch_OnExit();

	void						Stealth_OnEnter();
	FSM_Return					Stealth_OnUpdate();

	void						ActionMode_OnEnter();
	FSM_Return					ActionMode_OnUpdate();
	void						ActionMode_OnExit();

	void						Strafing_OnEnter();
	FSM_Return					Strafing_OnUpdate();
	void						Strafing_OnExit();

	void						StrafingToOnFoot_OnEnter();
	FSM_Return					StrafingToOnFoot_OnUpdate();
	void						StrafingToOnFoot_OnExit();

	// Swimming
	void						Swimming_OnEnter();
	FSM_Return					Swimming_OnUpdate();
	void						Swimming_OnExit();

	void						SwimToRun_OnEnter();
	FSM_Return					SwimToRun_OnUpdate();

	void						DiveToSwim_OnEnter();
	FSM_Return					DiveToSwim_OnUpdate();	

	// Diving
	void						Diving_OnEnter();
	FSM_Return					Diving_OnUpdate();
	void						Diving_OnExit();

	// Do nothing behaviour
	void						DoNothing_OnEnter();
	FSM_Return					DoNothing_OnUpdate();
	void						DoNothing_OnExit();

	void						AnimatedVelocity_OnEnter();
	FSM_Return					AnimatedVelocity_OnUpdate();
	void						AnimatedVelocity_OnExit();

	void						InVehicle_OnEnter();
	FSM_Return					InVehicle_OnUpdate();
	void						InVehicle_OnExit();
	bool						InVehicle_OnProcessMoveSignals();

	FSM_Return					DiveUnder_OnUpdate();

	void						Aiming_OnEnter();
	FSM_Return					Aiming_OnUpdate();
	void						Aiming_OnExit();

	void						StandToCrouch_OnEnter();
	FSM_Return					StandToCrouch_OnUpdate();
	void						CrouchToStand_OnEnter();
	FSM_Return					CrouchToStand_OnUpdate();

	void						Parachuting_OnEnter();
	FSM_Return					Parachuting_OnUpdate();
	void						Parachuting_OnExit();

	void						Jetpack_OnEnter();
	FSM_Return					Jetpack_OnUpdate();
	void						Jetpack_OnExit();

#if ENABLE_DRUNK
	void						Drunk_OnEnter();
	FSM_Return					Drunk_OnUpdate();
#endif // ENABLE_DRUNK

	void						Dead_OnEnter();
	FSM_Return					Dead_OnUpdate();
	void						Dead_OnExit();

	void						RefDiveUnderClip();
	void						ReleaseDiveUnderClip();

	bool						CheckForTransitionBreakout();

	CTaskHumanLocomotion*		CreateOnFootTask(const fwMvClipSetId& clipSetId);
	bool						DoAimingTransitionIndependentMover(const CTaskHumanLocomotion* pTask);

	bool						RequestAimingClipSet();

	bool						IsParachuting() const;

	bool						IsUsingJetpack() const;

	bool						ShouldStrafe() const;
	bool						ShouldUseAimingStrafe() const;
	void						ForceSubStateChange(const fwMvStateId &targetStateId, const fwMvStateId &stateMachineId)
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s ped %s forcing MoVE state change to target state %s, state machine %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), targetStateId.GetCStr(), stateMachineId.GetCStr());
		m_MoveNetworkHelper.ForceStateChange(targetStateId, stateMachineId);
	}
	void						ForceStateChange(const fwMvStateId &targetStateId)
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(GetPed(), "[%s] - %s ped %s forcing MoVE state change to main target state %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), targetStateId.GetCStr());
		//taskAssertf(!m_ForcedMotionStateThisFrame, "[%s] - %s Ped %s already forced a motion state this frame", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()));
		m_MoveNetworkHelper.ForceStateChange(targetStateId);
#if __DEV
		m_ForcedMotionStateThisFrame = true;
#endif // __DEV
	}

	//This function sets m_WaitingForTargetState if not yet in target start 
	//and assumes that it is called via the UpdateFSM which initially 
	//clears m_WaitingForTargetState
	bool IsMoveNetworkHelperInTargetState()
	{
		if (!m_MoveNetworkHelper.IsNetworkAttached())
		{
			return false;
		}
		else if(!m_MoveNetworkHelper.IsInTargetState())
		{
			m_WaitingForTargetState = true;
			return false;
		}
		return true;
	}

	void TaskSetState(u32 const iState);

	// Flags
	fwFlags32 m_iFlags;

	u16 m_forcedInitialSubState;

	u32 m_isIdle : 1;
	u32 m_isUsingDiveUnderClip : 1;	
	u32 m_restartCurrentStateNextFrame : 1;
	u32 m_restartAimingUpperBodyState : 1;
	u32 m_DiveFromFall : 1;
	u32 m_bDoAimingIndependentMover : 1;
	u32 m_blockAimingActive : 2;
	u32 m_blockAimingIndependentMoverActive : 2;
	u32 m_blockOnFootActive : 2;
	u32 m_OnFootIndependentMover : 1;
#if __DEV
	u32 m_ForcedMotionStateThisFrame : 1;
#endif // __DEV

	// Keep track of the fwTimer::GetTimeStep accumulation for accurate time elapsed
	// in the presence of timeslicing, where GetTimeInState only accounts for UpdateFSM time
	float m_fActualElapsedTimeInState;

	float m_fRestartDuration;
	
	float m_fBlockAimingTransitionTimer;
	float m_fBlockAimingIndependentMoverTimer;
	float m_fBlockOnFootTransitionTimer;

	float m_DiveImpactSpeed;

	float m_networkInsertDuration;
	float m_subtaskInsertDuration;

	// Strafe instant blend override
	float m_fInstantStrafePhaseSync;
	float m_fStrafeDurationOverride;

	// Time in sprint.
	float m_fActionModeTimeInSprint;

	float m_fBeginMoveStartPhase;

	CMoveNetworkHelper m_MoveNetworkHelper;
	fwClipSetRequestHelper m_ActionModeClipSetRequestHelper;
	fwClipSetRequestHelper m_SwimClipSetRequestHelper;
	fwClipSetRequestHelper m_WeaponClipSetRequestHelper;
	fwClipSetRequestHelper m_IdleClipSetRequestHelper;

	float m_fPostMovementIndependentMoverTargetHeading;
	u32 m_uLastOnFootRestartTime;

#if FPS_MODE_SUPPORTED
	Vector3 m_vCachedSwimVelocity;
	Vector3 m_vCachedSwimStrafeVelocity;
	float	m_fFPSFidgetIdleCountdownTime;
	bool	m_bPlayingIdleFidgets;
	int		m_iFPSFidgetVariation;
	int		m_iFPSFidgetVariationLoading;
	float	m_fRandomizedTimeToStartFidget;
	fwMvClipSetId m_UseFPSIntroAfterAimingRestart;
	float	m_fFPSIntroAfterAimingRestartBlendOutTime;
	bool	m_bFPSPreviouslySprinting;
	bool	m_bFPSPreviouslyCombatRolling;
	int 	m_iDivingInitialState;
	bool	m_bCloneUsingFPAnims;
	int		m_bFPSInitialStateForCombatRollUnholsterTransition;
	bool	m_bSafeToPlayFidgetsInFps;
	WorldProbe::CShapeTestResults m_FidgetsFPSProbe;
#endif	//FPS_MODE_SUPPORTED

	static const fwMvStateId ms_AimingStateId;
	static const fwMvStateId ms_ParachutingStateId;
	static const fwMvStateId ms_OnFootParentStateId;
	static const fwMvStateId ms_OnFootParentParentStateId;
	static const fwMvRequestId ms_DivingId;
	static const fwMvRequestId ms_DivingFromFallId;	
	static const fwMvRequestId ms_SwimToRunId;
	static const fwMvRequestId ms_DiveToSwimId;
	static const fwMvRequestId ms_SwimmingId;
	static const fwMvRequestId ms_CrouchId;
	static const fwMvRequestId ms_StealthId;
	static const fwMvRequestId ms_ActionModeId;
	static const fwMvRequestId ms_OnFootId;
	static const fwMvRequestId ms_AimingId;
	static const fwMvRequestId ms_StrafingId;
	static const fwMvRequestId ms_InVehicleId;
	static const fwMvRequestId ms_DoNothingId;
	static const fwMvRequestId ms_AnimatedVelocityId;
	static const fwMvRequestId ms_StandToCrouchId;
	static const fwMvRequestId ms_CrouchToStandId;
	static const fwMvRequestId ms_StrafeToOnFootId;
#if ENABLE_DRUNK
	static const fwMvRequestId ms_DrunkId;
#endif // ENABLE_DRUNK
	static const fwMvRequestId ms_ParachutingId;
	static const fwMvRequestId ms_DeadId;
	static const fwMvBooleanId ms_OnEnterCrouchId;
	static const fwMvBooleanId ms_OnExitCrouchId;
	static const fwMvBooleanId ms_OnEnterStealthId;
	static const fwMvBooleanId ms_OnExitStealthId;
	static const fwMvBooleanId ms_OnEnterActionModeId;
	static const fwMvBooleanId ms_OnExitActionModeId;
	static const fwMvBooleanId ms_OnEnterDivingId;
	static const fwMvBooleanId ms_OnEnterInVehicleId;
	static const fwMvBooleanId ms_OnEnterOnFootId;
	static const fwMvBooleanId ms_OnExitOnFootId;
	static const fwMvBooleanId ms_OnEnterDoNothing;
	static const fwMvBooleanId ms_OnEnterStrafingId;
	static const fwMvBooleanId ms_OnEnterSwimmingId;
	static const fwMvBooleanId ms_OnEnterCrouchedStrafingId;
	static const fwMvBooleanId ms_OnEnterStandStrafingId;
	static const fwMvBooleanId ms_OnEnterStandToCrouchId;
	static const fwMvBooleanId ms_OnEnterCrouchToStandId;
	static const fwMvBooleanId ms_OnEnterStrafeToOnFootId;
	static const fwMvBooleanId ms_OnEnterSwimToRunId;
	static const fwMvBooleanId ms_OnEnterDiveToSwimId;
	static const fwMvBooleanId ms_OnEnterParachutingId;
	static const fwMvBooleanId ms_TransitionBreakPointId;
	static const fwMvBooleanId ms_LongerBlendId;
	static const fwMvBooleanId ms_NoTagSyncId;
	static const fwMvBooleanId ms_IgnoreMoverRotationId;
	static const fwMvBooleanId ms_TransitionAnimFinishedId;
	static const fwMvFlagId ms_IsIdleId;
	static const fwMvFlagId ms_IsCrouchedId;
	static const fwMvFlagId ms_IsStationaryId;
	static const fwMvFlagId ms_SkipTransitionId;
	static const fwMvNetworkId ms_CrouchedStrafingNetworkId;
	static const fwMvNetworkId ms_StandStrafingNetworkId;
	static const fwMvNetworkId ms_CrouchNetworkId;
	static const fwMvNetworkId ms_StealthNetworkId;
	static const fwMvNetworkId ms_ActionModeNetworkId;
	static const fwMvNetworkId ms_DivingNetworkId;
	static const fwMvNetworkId ms_InVehicleNetworkId;
	static const fwMvNetworkId ms_OnFootNetworkId;
	static const fwMvNetworkId ms_StrafingNetworkId;
	static const fwMvNetworkId ms_SwimmingNetworkId;
#if ENABLE_DRUNK
	static const fwMvNetworkId ms_DrunkNetworkId;
	static const fwMvRequestId ms_DrunkClearRequestId;
	static const fwMvFloatId ms_DrunkClearDurationId;
#endif // ENABLE_DRUNK
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_VelocityId;
	static const fwMvFloatId ms_StrafeDurationOverrideId;
	static const fwMvFloatId ms_SwimToRunPhaseId;
	static const fwMvFloatId ms_TransitionPhaseId;
	static const fwMvFloatId ms_InVehicleRestartBlendDurationId;
	static const fwMvFloatId ms_OnFootRestartBlendDurationId;
	static const fwMvFloatId ms_AimToSwimDiveBlendDurationId;
	static const fwMvNetworkId ms_ParachutingNetworkId;
	static const fwMvRequestId ms_RestartOnFootId;
	static const fwMvRequestId ms_RestartOnFootNoTagSyncId;
	static const fwMvRequestId ms_RestartInVehicleId;
	static const fwMvRequestId ms_RestartAimingId;
	static const fwMvRequestId ms_RestartUpperBodyAimingId;
	static const fwMvRequestId ms_RestartStrafingId;
	static const fwMvRequestId ms_RestartActionModeId;
	static const fwMvRequestId ms_RestartStealthId;
	static const fwMvRequestId ms_IndependentMoverExpressionOnFootId;
	static const fwMvRequestId ms_IndependentMoverExpressionAimingId;
	static const fwMvFlagId ms_WantsToAimId;
	static const fwMvFlagId ms_HasDesiredVelocityId;
	static const fwMvFloatId ms_AimingRestartTransitionDuration;
	static const fwMvFlagId ms_UseFirstPersonSwapTransition;

public:

	static const fwMvFloatId ms_DiveUnderPhaseId;
	static const fwMvBooleanId ms_TransitionClipFinishedId;
	static const fwMvBooleanId ms_ActionModeBreakoutId;
	static const fwMvBooleanId ms_OnEnterAimingId;
	static const fwMvNetworkId ms_CrouchedAimingNetworkId;
	static const fwMvNetworkId ms_AimingNetworkId;

#if __LOG_MOTIONPED_RESTART_REASONS
	static bool ms_LogRestartReasons;
#endif // __LOG_MOTIONPED_RESTART_REASONS
};

////////////////////////////////////////////////////////////////////////////////

class CTaskMotionPedLowLod : public CTaskMotionBase
{
public:

	enum State
	{
		State_Start,
		State_OnFoot,
		State_Swimming,
		State_InVehicle,
		State_Exit,
	};

public:

	CTaskMotionPedLowLod();
	~CTaskMotionPedLowLod();

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOTION_PED_LOW_LOD; }
	virtual aiTask* Copy() const { return rage_new CTaskMotionPedLowLod(); }

#if __DEV 
	void SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId);
#endif

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state)
	{
		switch (state)
		{
		case CPedMotionStates::MotionState_Idle: //intentional fall through
		case CPedMotionStates::MotionState_Walk:
		case CPedMotionStates::MotionState_Run:
		case CPedMotionStates::MotionState_Sprint:
		case CPedMotionStates::MotionState_Swimming_TreadWater:
		case CPedMotionStates::MotionState_InVehicle:
			return true;
		default:
			return false;
		}
	}

	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const;

	//***********************************************************************************
	// This function returns the movement speeds at walk, run & sprint move-blend ratios
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);

	//*********************************************************************************
	// Returns whether the ped is currently moving based upon internal movement forces
	virtual	bool IsInMotion(const CPed * pPed) const;

	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);

	//********************************************************************************************
	// This function returns the default main-task-tree player control task for this motion task
	virtual CTask * CreatePlayerControlTask();

	//*************************************************************************************
	// Whether the ped should be forced to stick to the floor based upon the movement mode
	virtual bool ShouldStickToFloor();

	//	PURPOSE: Should return true if the motion task is in an on foot state
	virtual bool IsOnFoot() { return GetState()==State_OnFoot; }

	//*************************************************************************************
	// Check if we can perform a transition from our current state to the network requested target state
	virtual bool IsClonePedMoveNetworkStateTransitionAllowed(u32 const targetState) const;
	virtual bool IsClonePedNetworkStateChangeAllowed(u32 const state);
	virtual bool GetStateMoveNetworkRequestId(u32 const state, fwMvRequestId& requestId) const;
	virtual s32  ConvertMoveNetworkStateAndClipSetLod(u32 const state, fwMvClipSetId& pendingClipSetId) const;

	void SetNetworkInsertDuration(const float fNetworkInsertDuration) { m_fNetworkInsertDuration = fNetworkInsertDuration; }

protected:

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual void CleanUp();
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Exit; }
	virtual bool MayDeleteOnAbort() const { return true; }	// This task doesn't resume if it gets aborted, should be safe to delete.

private:

	void TaskSetState(u32 const iState);

	void ApplyWeaponClipSet(CTaskMotionBasicLocomotionLowLod* pSubTask);

	// State Functions
	FSM_Return Start_OnUpdate();
	FSM_Return OnFoot_OnEnter();
	FSM_Return OnFoot_OnUpdate();
	FSM_Return Swimming_OnEnter();
	FSM_Return Swimming_OnUpdate();
	FSM_Return InVehicle_OnEnter();
	FSM_Return InVehicle_OnUpdate();

	//This function sets m_WaitingForTargetState if not yet in target start 
	//and assumes that it is called via the UpdateFSM which initially 
	//clears m_WaitingForTargetState
	bool IsMoveNetworkHelperInTargetState()
	{
		if(!m_MoveNetworkHelper.IsInTargetState())
		{
			m_WaitingForTargetState = true;
			return false;
		}
		return true;
	}
	//
	// Members
	//

	// MoVE network helper
	CMoveNetworkHelper m_MoveNetworkHelper;

	// Time for network insertion
	float m_fNetworkInsertDuration;

	//
	u32 m_uLastOnFootRestartTime;

	// Statics
	static const fwMvNetworkId ms_OnFootNetworkId;
	static const fwMvNetworkId ms_SwimmingNetworkId;
	static const fwMvNetworkId ms_InVehicleNetworkId;
	static const fwMvRequestId ms_OnFootId;
	static const fwMvRequestId ms_SwimmingId;
	static const fwMvRequestId ms_InVehicleId;
	static const fwMvBooleanId ms_OnEnterOnFootId;
	static const fwMvBooleanId ms_OnExitOnFootId;
	static const fwMvBooleanId ms_OnEnterSwimmingId;
	static const fwMvBooleanId ms_OnEnterInVehicleId;
	static const fwMvRequestId ms_RestartOnFootId;
	static const fwMvRequestId ms_RestartInVehicleId;
};

#endif //TASK_MOTION_PED_H
