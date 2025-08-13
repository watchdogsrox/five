//
// Task/Motion/Locomotion/TaskMotionAiming.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_MOTION_AIMING_H
#define INC_TASK_MOTION_AIMING_H

////////////////////////////////////////////////////////////////////////////////

// Game headers
#include "Peds/ped.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAimingTransition.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/System/Tuning.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Motion/Locomotion/TaskInWater.h"

// Forward declarations
class CPreviousDirections;
class CTaskMoveGoToPointAndStandStill;

////////////////////////////////////////////////////////////////////////////////

class CTaskMotionAiming : public CTaskMotionBase
{
public:

	// PURPOSE: Helper functions
	static void ClampVelocityChange(const CPed * pPed, Vector3& vChangeInAndOut, float fTimestep, float fChangeLimit);
	static u32 CountFiringEventsInClip(const crClip& rClip);
	static bool WillStartInMotion(const CPed * pPed);
	static void ProcessOnFootAimingLeftHandGripIk(CPed& rPed, const CWeaponInfo& rWeaponInfo, const bool bAllow2HandedAttachToStockGrip = false, float fOverrideBlendInDuration = -1.0f, float fOverrideBlendOutDuration = -1.0f);

	// PURPOSE: FSM states
	enum
	{
		State_Initial,

		// Varied aim pose
		State_StreamTransition,
		State_PlayTransition,
		State_StreamStationaryPose,
		State_StationaryPose,
		State_Turn,

		// Stepping in cover
		State_CoverStep,

		// Strafing/Aiming
		State_StartMotionNetwork,
		State_StandingIdleIntro,
		State_StandingIdle,
		State_SwimIdleIntro,
		State_SwimIdle,
		State_SwimIdleOutro,
		State_SwimStrafe,
		State_StrafingIntro,
		State_StartStrafing,
		State_Strafing,
		State_StopStrafing,
		State_Roll,
		State_Turn180,
		State_Finish,
	};

	CTaskMotionAiming(const bool bIsCrouched, const bool bLastFootLeft, const bool bUseLeftFootStrafeTransition, const bool bBlockTransition, CTaskMotionAimingTransition::Direction direction, const float fIdleDir, const bool bBlockIndependentMover, const float fPreviousVelocityChange = CTaskMotionSwimming::s_fPreviousVelocityChange, const float fHeightBelowWater = CTaskMotionSwimming::s_fHeightBelowWater, const float fHeightBelowWaterGoal = CTaskMotionSwimming::s_fHeightBelowWaterGoal, const float fNormalisedVel = FLT_MAX, const bool bBlendInSetHeading = true);
	virtual ~CTaskMotionAiming();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_AIMING; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskMotionAiming(m_bIsCrouched, m_bLastFootLeft, m_bUseLeftFootStrafeTransition, m_bBlockTransition, m_TransitionDirection, m_fInitialIdleDir, m_bBlockIndependentMover, m_PreviousVelocityChange, m_fHeightBelowWater, m_fHeightBelowWaterGoal FPS_MODE_SUPPORTED_ONLY(, m_fNormalisedVel)); }
	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper*   GetMoveNetworkHelper() const { return &m_MoveGunStrafingMotionNetworkHelper; }	
	virtual CMoveNetworkHelper*	GetMoveNetworkHelper() { return &m_MoveGunStrafingMotionNetworkHelper; }

	bool AreNetworksAttached() const { return m_MoveGunStrafingMotionNetworkHelper.IsNetworkAttached() && m_MoveGunOnFootNetworkHelper.IsNetworkAttached(); }
	CMoveNetworkHelper& GetMotionMoveNetworkHelper() { return m_MoveGunStrafingMotionNetworkHelper; }
	CMoveNetworkHelper& GetAimingMoveNetworkHelper() { return m_MoveGunOnFootNetworkHelper; }

	bool GetIsCrouched() const { return m_bIsCrouched; }
	
	bool GetNeedsRestart() const { return m_bNeedsRestart; }
	
	void SetSkipIdleIntro(bool bSkip) { m_bSkipIdleIntro = bSkip; }
	
	bool GetIsStationary() const { return m_StationaryAimPose.IsLoopClipValid(); }
	
	float GetDesiredPitch() const { return m_fDesiredPitch; }
	float GetCurrentPitch() const { return m_fCurrentPitch; }
	
	const CPedMotionData::StationaryAimPose& GetStationaryAimPose() const { return m_StationaryAimPose; }
	const CPedMotionData::AimPoseTransition& GetAimPoseTransition() const { return m_AimPoseTransition; }
	
	void SetStationaryAimPose(const CPedMotionData::StationaryAimPose& rValue) { m_StationaryAimPose = rValue; }
	void SetAimPoseTransition(const CPedMotionData::AimPoseTransition& rValue) { m_AimPoseTransition = rValue; }

	// PURPOSE: Motion task implementation
	virtual bool IsOnFoot() { return true; }
	virtual	bool CanFlyThroughAir() { return false; }
	virtual bool ShouldStickToFloor() { return true; }
	virtual	bool IsInMotion(const CPed * UNUSED_PARAM(pPed)) const { return IsInStrafingState(); }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds);
	virtual CTask* CreatePlayerControlTask();
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state) { return (state == CPedMotionStates::MotionState_Aiming) ? true : false;}
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const { return (state == CPedMotionStates::MotionState_Aiming) ? true : false;}
	virtual bool ForceLeafTask() const { return true; }
	virtual void GetPitchConstraintLimits(float& fMinOut, float& fMaxOut);
	virtual bool IsUnderWater() const { return (GetPed() && GetPed()->GetIsFPSSwimmingUnderwater()); }

	// Predict the stopping distance
	virtual float GetStoppingDistance(const float fStopDirection, bool* bOutIsClipActive = NULL);

	void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip) { outClipSet = m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(); outClip = CLIP_IDLE; }

	// PURPOSE: Interface functions
	bool CanPlayTransition() const;
	bool IsInStrafingState() const;
	bool IsPlayingTransition() const;
	bool IsTurning() const;
	void RestartUpperBodyAimNetwork(const fwMvClipSetId clipSetId);
	void RestartLowerBodyAimNetwork();

	bool EnteredAimState() const;
	bool EnteredFiringState() const;
	void SetMoveFlag(const bool bVal, const fwMvFlagId flagId);
	bool SetMoveClip(const crClip* pClip, const fwMvClipId clipId);
	void SetMoveFloat(const float fVal, const fwMvFloatId floatId);
	void SetMoveBoolean(const bool bVal, const fwMvBooleanId boolId);
	bool GetMoveBoolean(const fwMvBooleanId boolId) const;
	void RequestAim();
	
	void SetAimClipSet(const fwMvClipSetId& aimClipSet);
	const fwMvClipSetId& GetAimClipSet() const { return m_AimClipSetId; }
	const fwMvClipSetId GetMoveClipSet() const { return m_MoveGunStrafingMotionNetworkHelper.GetClipSetId(); }

	void SetFPSAdditivesClipSet(const fwMvClipSetId& fallbackClipSetId);

	bool GetUsesFireEvents() const { return m_bUsesFireEvents; }

	float GetStrafeDirection() const { return m_fStrafeDirection; }

	float GetOverwriteMaxPitchChange() { return m_OverwriteMaxPitchChange; }
	void SetOverwriteMaxPitchChange(float val) { m_OverwriteMaxPitchChange = val; }

	bool IsLastFootLeft() const;
	bool UseLeftFootStrafeTransition() const;
	bool GetAimIntroFinished() const;

	bool IsFullyInStop() const { return m_bFullyInStop; }
	bool IsFootDown() const;

	void PlayCustomAimingClip(fwMvClipSetId clipSetId);

	bool IsTransitionBlocked() const;

	bool WillStartInStrafingIntro(const CPed* pPed) const;

	void StopAimIntro();

	bool HasAimIntroTimedOut() const { return m_bAimIntroTimedOut; }

	float GetStartDirection() const { return m_fStartDirection; }

	static bool GetCanMove(const CPed* pPed);

	const float GetPreviousVelocityChange() const { return m_PreviousVelocityChange; } 
	const float GetHeightBelowWater() const { return m_fHeightBelowWater; } 
	const float GetHeightBelowWaterGoal() const { return m_fHeightBelowWaterGoal; } 

	bool UsingLeftTriggerAimMode() const;

	bool GetWasDiving() const { return m_bWasDiving; }
	const float GetLastWaveDelta() const {return m_fLastWaveDelta; }

	void PlaySwimIdleOutro(bool bPlayOutro) { m_bPlayOutro = bPlayOutro; }
	bool GetSwimIdleOutroFinished() const { return m_bOutroFinished; }

	void SetFPSIntroTransitionClip(fwMvClipSetId clipsetId) { m_FPSAimingIntroClipSetId = clipsetId; }
	void SetFPSIntroBlendOutTime(float f) { m_fFPSAimingIntroBlendOutTime = f; }

	bool GetFPSFidgetClipFinished() const { return m_bMoveFPSFidgetClipEnded; }
	bool GetFPSFidgetClipNonInterruptible() const { return m_bMoveFPSFidgetClipNonInterruptible; }

	void SetUsingMotionAimingWhileSwimming(bool bVal) { m_bUsingMotionAimingWhileSwimmingFPS =  bVal; }
	bool GetUsingMotionAimingWhileSwimming() const { return m_bUsingMotionAimingWhileSwimmingFPS; }

#if FPS_MODE_SUPPORTED
	// Used in GetPitchConstraintLimits() so we don't pop when switching from dive/swim to strafe.
	void SetPreviousPitch(float fPitch) { m_fPrevPitch = fPitch; } 
	float GetNormalisedVel() const { return m_fNormalisedVel; }
#endif	//FPS_MODE_SUPPORTED

	virtual float GetStandingSpringStrength();

	static bool IsValidForUnarmedIK(const CPed& ped);

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Update movement variables
	virtual void ProcessMovement();

	// PURPOSE: A task can query a fully updated and resolved camera frame within this function, as the camera system has already been updated.
	virtual bool ProcessPostCamera();

	// PURPOSE: Called after the skeleton update but before the Ik (I think this is now before the skeleton update)
	virtual void ProcessPreRender2();

	// PURPOSE: Called after the PreRender
	virtual bool ProcessPostPreRender(); // Called after the PreRender

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool ProcessPostMovement();

	virtual bool ProcessMoveSignals();

	// Are we about to stop?
	bool IsGoingToStopThisFrame(const CTaskMoveGoToPointAndStandStill* pGotoTask) const;

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on FSM events within main UpdateFSM function
	FSM_Return	Initial_OnEnter();
	FSM_Return	Initial_OnUpdate();
	FSM_Return	StreamTransition_OnUpdate();
	FSM_Return	PlayTransition_OnEnter();
	FSM_Return	PlayTransition_OnUpdate();
	FSM_Return	PlayTransition_OnExit();
	void		PlayTransition_OnProcessMoveSignals();
	FSM_Return	StreamStationaryPose_OnUpdate();
	FSM_Return	StationaryPose_OnEnter();
	FSM_Return	StationaryPose_OnUpdate();
	FSM_Return	StationaryPose_OnExit();
	FSM_Return	CoverStep_OnEnter();
	FSM_Return	CoverStep_OnUpdate();
	FSM_Return	CoverStep_OnExit();
	void		CoverStep_OnProcessMoveSignals();
	FSM_Return	StartMotionNetwork_OnUpdate();
	FSM_Return	StandingIdleIntro_OnEnter();
	FSM_Return	StandingIdleIntro_OnUpdate();
	FSM_Return	StandingIdleIntro_OnExit();
	void		StandingIdleIntro_OnProcessMoveSignals();
	FSM_Return	StandingIdle_OnEnter();
	FSM_Return	StandingIdle_OnUpdate();
	FSM_Return	StandingIdle_OnExit();
	void		StandingIdle_OnProcessMoveSignals();
	FSM_Return  SwimIdleIntro_OnEnter();
	FSM_Return  SwimIdleIntro_OnUpdate();
	FSM_Return  SwimIdleIntro_OnExit();
	FSM_Return  SwimIdle_OnEnter();
	FSM_Return  SwimIdle_OnUpdate();
	FSM_Return  SwimIdle_OnExit();
	FSM_Return  SwimIdleOutro_OnEnter();
	FSM_Return  SwimIdleOutro_OnUpdate();
	FSM_Return  SwimIdleOutro_OnExit();
	FSM_Return	SwimStrafe_OnEnter();
	FSM_Return	SwimStrafe_OnUpdate();
	FSM_Return	SwimStrafe_OnExit();
	FSM_Return	StrafingIntro_OnEnter();
	FSM_Return	StrafingIntro_OnUpdate();
	FSM_Return	StartStrafing_OnEnter();
	FSM_Return	StartStrafing_OnUpdate();
	void		StartStrafing_OnProcessMoveSignals();
	FSM_Return	Strafing_OnEnter();
	FSM_Return	Strafing_OnUpdate();
	FSM_Return	Strafing_OnExit();
	void		Strafing_OnProcessMoveSignals();
	FSM_Return	StopStrafing_OnEnter();
	FSM_Return	StopStrafing_OnUpdate();
	FSM_Return	StopStrafing_OnExit();
	void		StopStrafing_OnProcessMoveSignals();
	FSM_Return	Turn_OnEnter();
	FSM_Return	Turn_OnUpdate();
	FSM_Return	Turn_OnExit();
	void		Turn_OnProcessMoveSignals();
	FSM_Return	Roll_OnEnter();
	FSM_Return	Roll_OnUpdate();
	FSM_Return	Roll_OnExit();
	FSM_Return	Turn180_OnEnter();
	FSM_Return	Turn180_OnUpdate();
	void		Turn180_OnProcessMoveSignals();

	////////////////////////////////////////////////////////////////////////////////

	void SetState(s32 iState);

	bool StartMotionNetwork();
	bool StartAimingNetwork();
	bool SetUpAimingNetwork();

	void ComputeAndSendStrafeAimSignals(float fUseTimeStep = -1.0f);
	void ComputePitchSignal(float fUseTimeStep);
	void ComputeAimIntroSignals(float fUseTimeStep);
	void UpdateStrafeAimSignals(float fUseTimeStep);
	void SendStrafeAimSignals();
#if FPS_MODE_SUPPORTED
	void SendFPSSwimmingMoveSignals();
#endif	//FPS_MODE_SUPPORTED

	bool GetMaxAndMinPitchBetween0And1(CPed* pPed, float& fMinPitch, float& fMaxPitch);

	Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep);
	Vector3 ProcessSwimmingResistance(float fTimestep);
	Vector3 ProcessDivingResistance(float fTimestep);
	void CalculateSwimStrafingSpeed(Mat34V_ConstRef updatedPedMatrixIn, Vector3 &vVelocity);

	void SendParentSignals();

	fwMvClipSetId GetClipSet() const;

	const CAimingInfo* GetAimingInfo() const;

	bool IsStateValidForPostCamUpdate() const;
	bool IsStateValidForIK() const;

	float CalculateTurnBlendFactor() const;
	void ChooseDefaultAimingClip(fwMvClipSetId& clipSetId, fwMvClipId& clipId) const;
	bool ChooseNewAimingClip();
	fwMvClipSetId ChooseTurnClipSet() const;
	bool ChooseVariedAimingClip(fwMvClipSetId& clipSetId, fwMvClipId& clipId) const;
	fwMvClipSetId GetAlternateAimingClipSetId() const;
	void SetAimingClip();
	bool ShouldChooseNewAimingClip() const;
	bool ShouldProcessPhysics() const;
	bool ShouldTurn() const;
	bool ShouldTurnOffUpperBodyAnimations(float& fBlendDuration) const;

	static bool GetWantsToMove(const CPed* pPed);
	bool GetWantsToCoverStep() const;
	bool GetUseIdleIntro() const;
	bool GetUseStartsAndStops() const;
	bool GetUseTurn180() const;
	bool GetDesiredStrafeDirection(float& fStrafeDirection) const;
	bool GetCurrentStrafeDirection(float& fStrafeDirection) const;

	bool DoPostCameraAnimUpdate() const;
	void ReadCommonMoveSignals();
	void ResetCommonMoveSignals();

	float ComputeRandomAnimRate() const;

	void UpdateAdditives();

	float CalculateUnWarpedTimeStep(float fTimeStep) const;

	//
	void ComputeStandingIdleSignals();

	// 
	void SendStandingIdleSignals();

	//
	float CalculateNormaliseVelocity(const bool bFirstPersonModeEnabled) const;

	// 
	bool DoPostCameraSetHeading() const;

#if __ASSERT
	void VerifyMovementClipSet();
#endif // __ASSERT

	//
	// Members
	//

#if FPS_MODE_SUPPORTED
	Vector3 m_vLastSlopePosition;
#endif // FPS_MODE_SUPPORTED
	
	//
	Vector2 m_VelDirection;

	// PURPOSE: Main move network helper (TaskAimGunOnFoot.mxtf)
	CMoveNetworkHelper m_MoveGunOnFootNetworkHelper;

	// PURPOSE: Strafing motion network helper, gets inserted into the on foot gun network (OnFootAiming.mxtf)
	CMoveNetworkHelper m_MoveGunStrafingMotionNetworkHelper;
	
	// PURPOSE: Streams alternate aiming animations
	fwClipSetRequestHelper m_ClipSetRequestHelper;
	
	// PURPOSE: Streams the aim pose transition
	fwClipSetRequestHelper m_AimPoseTransitionClipSetRequestHelper;
	
	// PURPOSE: Streams the stationary aim pose
	fwClipSetRequestHelper m_StationaryAimPoseClipSetRequestHelper;
	
	// PURPOSE: Streams the turn clip set
	CPrioritizedClipSetRequestHelper m_TurnClipSetRequestHelper;

	// PURPOSE: Streams the custom aiming clip set
	CPrioritizedClipSetRequestHelper m_ClipSetRequestHelperForCustomAimingClip;
	
	// PURPOSE: Stores the aim pose transition
	CPedMotionData::AimPoseTransition m_AimPoseTransition;
	
	// PURPOSE: Stores the stationary aim pose
	CPedMotionData::StationaryAimPose m_StationaryAimPose;
	
	// PURPOSE: Stores the aiming clip set id
	fwMvClipSetId m_AimingClipSetId;

	// PURPOSE: Stores the FPS aiming intro clip set id
	fwMvClipSetId m_FPSAimingIntroClipSetId;

	// PURPOSE: The FPS intro blend out time
	float m_fFPSAimingIntroBlendOutTime;
	
	// PURPOSE: Stores the aiming clip id
	fwMvClipId m_AimingClipId;
	
	// PURPOSE: Stores the parent's clip set id
	fwMvClipSetId m_ParentClipSetId;

	// PURPOSE: Desired pitch parameter
	float m_fDesiredPitch;

	// PURPOSE: Current pitch parameter
	float m_fCurrentPitch;

	// PURPOSE: Clip sets
	fwMvClipSetId m_AimClipSetId;

	// PURPOSE: Strafe speed calculated from move blend ratio
	float m_fStrafeSpeed;

	// PURPOSE: Desired strafe direction
	float m_fDesiredStrafeDirection;

#if FPS_MODE_SUPPORTED
	// PURPOSE: Desired swim-strafe direction (softened version of m_fDesiredStrafeDirection)
	float m_fDesiredSwimStrafeDirection;
	float m_fSwimStrafeGroundClearanceVelocity;
#endif	//FPS_MODE_SUPPORTED

	// PURPOSE: Actual strafe direction
	float m_fStrafeDirection;

	// PURPOSE: Starting direction of move start, relative to our current direction
	float m_fStartDirection;

	// PURPOSE: Starting desired heading
	float m_fStartDesiredHeading;

	// 
	float m_fStopDirection;

	// PURPOSE: Extra turn rate
	float m_fStandingIdleExtraTurnRate;

	// PURPOSE: Overwrite 
	float m_OverwriteMaxPitchChange;

	// PURPOSE: Aim intro initial aim heading
	float m_fAimIntroInitialAimHeading;

	// PURPOSE: Direction to base aim intro calculations off
	float m_fAimIntroInitialDirection;

	// PURPOSE: Interpolated aim intro direction, to drive the aim intro phase
	float m_fAimIntroInterpolatedDirection;

	// PURPOSE: Aim intro direction to drive left/right/forward blends
	float m_fAimIntroDirection;

	// PURPOSE: Interpolated aim intro phase
	float m_fAimIntroPhase;

	// PURPOSE: Aim intro playback rate
	float m_fAimIntroRate;

	// PURPOSE: Override movement rate
	float m_fMovementRate;

	// PURPOSE: Slow walk rate at MBR below 1.
	float m_fWalkMovementRate;

	// PURPOSE: Last frames aiming blend factor value
	float m_fPreviousAimingBlendFactor;

	// PURPOSE: When first adjusting pitch, accelerate, so this hides small pops
	float m_fPitchLerp;

	//
	float m_fStrafeSpeedAdditive;
	float m_fStrafeDirectionAdditive;

	// Previous frames desired direction for triggering 180 turns
	CPreviousDirections* m_pPreviousDesiredDirections;

	// Prevent stopping when checking for 180 turns for a number of frames, so we get a chance to trigger them
	s32 m_iMovingStopCount;

	// Store the direction at the start of the transition
	float m_fStrafeDirectionAtStartOfTransition;

	//
	CTaskMotionAimingTransition::Direction m_TransitionDirection;

	// 
	float m_fExtraHeadingChange;

	//
	float m_fInitialIdleDir;

	//
	float m_fVelLerp;

#if FPS_MODE_SUPPORTED
	//
	float m_fNormalisedVel;
	float m_fNormalisedStopVel;

	float m_fPrevPitch;
#endif // FPS_MODE_SUPPORTED

	//
	float m_fWalkVel;
	float m_fRunVel;
#if FPS_MODE_SUPPORTED
	float m_fOnFootWalkVel;
	float m_fOnFootRunVel;
	float m_fFPSVariableRateIntroBlendInDuration;
	float m_fFPSVariableRateIntroBlendInTimer;

	// Slopes/stairs
	u32 m_uLastSlopeScanTime;
	u32 m_uStairsDetectedTime;

	// Snow
	float m_fSmoothedSnowDepth;
	float m_fSnowDepthSignal;
#endif // FPS_MODE_SUPPORTED

	// 
	float m_fCameraTurnSpeed;
	u32 m_uCameraHeadingLerpTime;

	// SwimIdle variables
	bool m_bWasDiving;
	float m_fHeightBelowWaterGoal;
	float m_fHeightBelowWater;
	float m_PreviousVelocityChange;
	bool m_bSwimIntro;
	bool m_bPlayOutro;
	bool m_bOutroFinished;
	float m_fSwimStrafePitch;
	float m_fSwimStrafeRoll;
	Vector3 m_vSwimStrafeSpeed;
	float m_fLastWaveDelta;

	u32 m_uLastFpsPitchUpdateTime;

	u32 m_uLastDisableNormaliseDueToCollisionTime;

	// Flags
	bool m_bEnteredAimState : 1;
	bool m_bEnteredFiringState : 1;
	bool m_bEnteredAimIntroState : 1;
	bool m_bSkipTransition : 1;					// PURPOSE: Skip the transition state.
	bool m_bUsesFireEvents : 1;					// PURPOSE: Are we using fire events?
	bool m_bIsCrouched : 1;
	bool m_bNeedsRestart : 1;
	bool m_bSkipIdleIntro : 1;
	bool m_bUpdateStandingIdleIntroFromPostCamera : 1;
	bool m_bUseLeftFootStrafeTransition : 1;
	bool m_bBlockTransition : 1;
	bool m_bAimIntroFinished : 1;
	bool m_bSuppressHasIntroFlag : 1;
	bool m_bAimIntroTimedOut : 1;
	bool m_bWasRunning : 1;
	bool m_bIsRunning : 1;
	bool m_bLastFootLeft : 1;
	bool m_bFullyInStop : 1;
	bool m_bMoveTransitionAnimFinished : 1;
	bool m_bMoveAimOnEnterState : 1;
	bool m_bMoveFiringOnEnterState : 1;
	bool m_bMoveAimIntroOnEnterState : 1;
	bool m_bMoveAimingClipEnded : 1;
	bool m_bMoveWalking : 1;
	bool m_bMoveRunning : 1;
	bool m_bMoveCoverStepClipFinished : 1;
	bool m_bMoveBlendOutIdleIntro : 1;
	bool m_bMoveCanEarlyOutForMovement : 1;
	bool m_bMoveBlendOutStart : 1;
	bool m_bMoveBlendOutStop : 1;
	bool m_bMoveBlendOutTurn180 : 1;
	bool m_bMoveStartStrafingOnExit : 1;
	bool m_bMoveMovingOnExit : 1;
	bool m_bIsPlayingCustomAimingClip : 1;
	bool m_bNeedToSetAimClip : 1;				// PURPOSE: If true, we still need to call SetAimingClip() on every update. Will be set to false once we have reached certain states.
	bool m_bSnapMovementWalkRate : 1;
	bool m_bInStrafingTransition : 1;
	bool m_bDoPostMovementIndependentMover : 1;
	bool m_bSwitchActiveAtStart : 1;
	bool m_bDoVelocityNormalising : 1;
	bool m_bBlockIndependentMover : 1;
	bool m_bMoveFPSFidgetClipEnded : 1;
	bool m_bMoveFPSFidgetClipNonInterruptible : 1;
	bool m_bUsingMotionAimingWhileSwimmingFPS : 1;
	bool m_bResetExtractedZ : 1;
#if FPS_MODE_SUPPORTED
	mutable bool m_bWasUnarmed : 1;
	mutable bool m_bFPSPlayToUnarmedTransition : 1;
#endif // FPS_MODE_SUPPORTED
	bool m_bBlendInSetHeading : 1;

#if __ASSERT
	bool m_bCanPedMoveForVerifyClips : 1;
#endif

	//
	// Statics
	//

	static dev_float MIN_ANGLE;

public:

	static dev_float BWD_FWD_BORDER_MIN;
	static dev_float BWD_FWD_BORDER_MAX;

	// OnFootStrafingTagSync signals
	static const fwMvRequestId ms_IdleIntroId;
	static const fwMvRequestId ms_IdleId;
	static const fwMvRequestId ms_MovingIntroId;
	static const fwMvRequestId ms_StartStrafingId;
	static const fwMvRequestId ms_MovingId;
	static const fwMvRequestId ms_StopStrafingId;
	static const fwMvRequestId ms_CoverStepId;
	static const fwMvRequestId ms_RollId;
	static const fwMvRequestId ms_Turn180Id;
	static const fwMvRequestId ms_MovingFwdId;
	static const fwMvRequestId ms_MovingBwdId;
	static const fwMvRequestId ms_IndependentMoverRequestId;
	static const fwMvBooleanId ms_IdleIntroOnEnterId;
	static const fwMvBooleanId ms_IdleOnEnterId;
	static const fwMvBooleanId ms_MovingIntroOnEnterId;
	static const fwMvBooleanId ms_StartStrafingOnEnterId;
	static const fwMvBooleanId ms_StartStrafingOnExitId;
	static const fwMvBooleanId ms_MovingOnEnterId;
	static const fwMvBooleanId ms_MovingOnExitId;
	static const fwMvBooleanId ms_MovingTransitionOnEnterId;
	static const fwMvBooleanId ms_MovingTransitionOnExitId;
	static const fwMvBooleanId ms_StopStrafingOnEnterId;
	static const fwMvBooleanId ms_RollOnEnterId;
	static const fwMvBooleanId ms_Turn180OnEnterId;
	static const fwMvBooleanId ms_BlendOutIdleIntroId;
	static const fwMvBooleanId ms_BlendOutStartId;
	static const fwMvBooleanId ms_BlendOutStopId;
	static const fwMvBooleanId ms_BlendOutTurn180Id;
	static const fwMvBooleanId ms_CanEarlyOutForMovementId;
	static const fwMvBooleanId ms_CoverStepOnEnterId;
	static const fwMvBooleanId ms_CoverStepClipFinishedId;
	static const fwMvBooleanId ms_LeftFootStrafeTransitionId;
	static const fwMvBooleanId ms_WalkingId;
	static const fwMvBooleanId ms_RunningId;
	static const fwMvFloatId ms_DesiredStrafeDirectionId;
	static const fwMvFloatId ms_StrafeDirectionId;
	static const fwMvFloatId ms_DesiredStrafeSpeedId;
	static const fwMvFloatId ms_StrafeSpeedId;
	static const fwMvFloatId ms_DesiredHeadingId;
	static const fwMvFloatId ms_IdleTurnRateId;
	static const fwMvFloatId ms_AnimRateId;
	static const fwMvFloatId ms_IdleIntroAnimRateId;
	static const fwMvFloatId ms_IdleTurnAnimRateId;
	static const fwMvFloatId ms_AimingTransitionRateId;
	static const fwMvFloatId ms_CoverStepClipPhaseId;
	static const fwMvFloatId ms_MovementRateId;
	static const fwMvFloatId ms_Turn180PhaseId;
	static const fwMvFlagId ms_SkipIdleIntroId;
	static const fwMvFlagId ms_SkipStartsAndStopsId;
	static const fwMvFlagId ms_SkipTurn180Id;
	static const fwMvFlagId ms_SkipMovingTransitionsId;
	static const fwMvFlagId ms_LastFootLeftId;
	static const fwMvFlagId ms_BlockMovingTransitionsId;
	static const fwMvFlagId ms_IsPlayerId;
	static const fwMvFlagId ms_IsRemotePlayerUsingFPSModeId;
#if FPS_MODE_SUPPORTED
	static const fwMvFlagId ms_FirstPersonMode;
	static const fwMvFlagId ms_FirstPersonModeIK;
	static const fwMvFlagId ms_FirstPersonAnimatedRecoil;
	static const fwMvFlagId ms_FirstPersonSecondaryMotion;
	static const fwMvFlagId ms_BlockAdditives;
	static const fwMvFlagId ms_SwimmingId;
	static const fwMvFlagId ms_FPSUseGripAttachment;
	static const fwMvBooleanId ms_FPSAimIntroTransitionId;
#endif // FPS_MODE_SUPPORTED
	static const fwMvNetworkId ms_MovingIntroNetworkId;
	static const fwMvNetworkId ms_RollNetworkId;
	static const fwMvClipId ms_Walk0ClipId;
	static const fwMvClipId ms_Run0ClipId;
	static const fwMvClipId ms_CoverStepClipId;
	static const fwMvClipId ms_Turn180ClipId;
	// MotionAiming stop signals
	static const fwMvClipId ms_StopClipBwd_N_45Id;
	static const fwMvClipId ms_StopClipBwd_N_90Id;
	static const fwMvClipId ms_StopClipBwd_N_135Id;
	static const fwMvClipId ms_StopClipBwd_180Id;
	static const fwMvClipId ms_StopClipBwd_135Id;
	static const fwMvClipId ms_StopClipFwd_N_45Id;
	static const fwMvClipId ms_StopClipFwd_0Id;
	static const fwMvClipId ms_StopClipFwd_90Id;
	static const fwMvClipId ms_StopClipFwd_135Id;
	static const fwMvFloatId ms_StopPhaseBwd_N_45Id;
	static const fwMvFloatId ms_StopPhaseBwd_N_90Id;
	static const fwMvFloatId ms_StopPhaseBwd_N_135Id;
	static const fwMvFloatId ms_StopPhaseBwd_180Id;
	static const fwMvFloatId ms_StopPhaseBwd_135Id;
	static const fwMvFloatId ms_StopPhaseFwd_N_45Id;
	static const fwMvFloatId ms_StopPhaseFwd_0Id;
	static const fwMvFloatId ms_StopPhaseFwd_90Id;
	static const fwMvFloatId ms_StopPhaseFwd_135Id;
	static const fwMvFloatId ms_StopBlendBwd_N_45Id;
	static const fwMvFloatId ms_StopBlendBwd_N_90Id;
	static const fwMvFloatId ms_StopBlendBwd_N_135Id;
	static const fwMvFloatId ms_StopBlendBwd_180Id;
	static const fwMvFloatId ms_StopBlendBwd_135Id;
	static const fwMvFloatId ms_StopBlendFwd_N_45Id;
	static const fwMvFloatId ms_StopBlendFwd_0Id;
	static const fwMvFloatId ms_StopBlendFwd_90Id;
	static const fwMvFloatId ms_StopBlendFwd_135Id;
	// TaskAimGunOnFoot signals
	static const fwMvRequestId ms_AimId;
	static const fwMvRequestId ms_UseAimingIntroId;
	static const fwMvRequestId ms_UseAimingIntroFPSId;
	static const fwMvRequestId ms_UseAimingFidgetFPSId;
	static const fwMvRequestId ms_AimingIntroFinishedId;
	static const fwMvBooleanId ms_FireLoop1;
	static const fwMvBooleanId ms_FireLoop2;
	static const fwMvBooleanId ms_CustomFireLoopFinished;
	static const fwMvBooleanId ms_CustomFireIntroFinished;
	static const fwMvBooleanId ms_CustomFireOutroFinishedId;
	static const fwMvBooleanId ms_CustomFireOutroBlendOut;
	static const fwMvBooleanId ms_DontAbortFireIntroId;
	static const fwMvFloatId ms_PitchId;
	static const fwMvFloatId ms_NormalizedPitchId;
	static const fwMvFloatId ms_AimIntroDirectionId;
	static const fwMvFloatId ms_AimIntroRateId;
	static const fwMvFloatId ms_StrafeSpeed_AdditiveId;
	static const fwMvFloatId ms_StrafeDirection_AdditiveId;
	static const fwMvFloatId ms_AimIntroPhaseId;
	static const fwMvFlagId ms_HasIntroId;
	// PedMotion signals
	static const fwMvRequestId ms_AimingTransitionId;
	static const fwMvRequestId ms_StationaryAimingPoseId;
	static const fwMvRequestId ms_AimTurnId;
	static const fwMvBooleanId ms_AimTurnAnimFinishedId;
	static const fwMvBooleanId ms_TransitionAnimFinishedId;
	static const fwMvBooleanId ms_OnEnterAimingTransitionId;
	static const fwMvFloatId ms_AimTurnBlendFactorId;
	static const fwMvFloatId ms_AimingBlendFactorId;
	static const fwMvFloatId ms_AimingBlendDurationId;
	static const fwMvClipId ms_AimingTransitionClipId;
	static const fwMvClipId ms_StationaryAimingPoseClipId;
	// Underwater signals
	static const fwMvRequestId ms_SwimIdleId;
	static const fwMvBooleanId ms_SwimIdleOnEnterId;
	static const fwMvRequestId ms_SwimStrafeId;
	static const fwMvFloatId ms_SwimStrafePitchId;
	static const fwMvFloatId ms_SwimStrafeRollId;
	static const fwMvRequestId ms_UseSwimIdleIntroId;
	static const fwMvRequestId ms_UseSwimIdleOutroId;
	static const fwMvClipId ms_SwimIdleIntroTransitionClipId;
	static const fwMvFloatId ms_SwimIdleTransitionPhaseId;
	static const fwMvFloatId ms_SwimIdleTransitionOutroPhaseId;
	static const fwMvBooleanId ms_SwimIdleTransitionFinishedId;
	// Variable clipset
	static const fwMvClipSetVarId ms_FPSAimingIntroClipSetId;
	static const fwMvClipSetVarId ms_FPSAdditivesClipSetId;
public:

	// Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		struct Turn
		{
			Turn() {}
			
			float m_MaxVariationForCurrentPitch;
			float m_MaxVariationForDesiredPitch;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();

		float m_PlayerMoveAccel;
		float m_PlayerMoveDecel;
		float m_PedMoveAccel;
		float m_PedMoveDecel;
		float m_FromOnFootAccelerationMod;
		float m_WalkAngAccel;
		float m_RunAngAccel;
		float m_WalkAngAccelDirectBlend;
		float m_RunAngAccelDirectBlend;
		float m_WalkAngAccelLookBehindTransition;
		float m_RunAngAccelLookBehindTransition;
		float Turn180ActivationAngle;
		float Turn180ConsistentAngleTolerance;
		Turn m_Turn;
		float m_PitchChangeRate;
		float m_PitchChangeRateAcceleration;
		float m_OverwriteMaxPitch;
		float m_AimIntroMaxAngleChangeRate;
		float m_AimIntroMinPhaseChangeRate;
		float m_AimIntroMaxPhaseChangeRate;
		float m_AimIntroMaxTimedOutPhaseChangeRate;
		float m_PlayerIdleIntroAnimRate;
		float m_MovingWalkAnimRateMin;
		float m_MovingWalkAnimRateMax;
		float m_MovingWalkAnimRateAcceleration;
		bool m_DoPostCameraClipUpdateForPlayer;
		bool m_EnableIkForAI;
		bool m_FPSForceUseStarts;
		bool m_FPSForceUseStops;
		float m_FPSForceUseStopsMinTimeInStartState;
		bool m_FPSForceUse180s;
		PAR_PARSABLE;
	};

	static const Tunables& GetTunables() { return ms_Tunables; }

private:
	static Tunables ms_Tunables;

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 );
#endif // !__FINAL
};

////////////////////////////////////////////////////////////////////////////////

inline bool CTaskMotionAiming::IsFootDown() const
{
	return m_MoveGunStrafingMotionNetworkHelper.IsNetworkActive() && (CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelLId) || CTaskHumanLocomotion::CheckBooleanEvent(this, CTaskHumanLocomotion::ms_AefFootHeelRId));
}

////////////////////////////////////////////////////////////////////////////////

class CPreviousDirections
{
public:

	CPreviousDirections();

	void RecordDirection(float fDirection);
	void ResetDirections(float fDirection = 0.f, u32 uTime = 0);

	bool GetDirectionForTime(const u32 uElapsedTime, float& fDirection) const;

private:
	struct sPreviousDirection
	{
		sPreviousDirection() : fPreviousDirection(0.f), uTime(0) {}
		float fPreviousDirection;
		u32 uTime;
	};

	static const s32 MAX_PREVIOUS_DIRECTION_FRAMES = 50;
	sPreviousDirection m_PreviousDirections[MAX_PREVIOUS_DIRECTION_FRAMES];
};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_TASK_MOTION_AIMING_H
