//
// Task/Vehicle/TaskInVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_IN_VEHICLE_H
#define INC_TASK_IN_VEHICLE_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "Camera/Helpers/DampedSpring.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/motion/TaskMotionBase.h"
#include "Task/motion/Locomotion/TaskMotionPed.h"
#include "Task/Vehicle/TaskVehicleBase.h"

// Game forward declarations
class CCarDoor;
class CSeatOverrideAnimInfo;
class CInVehicleOverrideInfo;
class CTaskMotionInAutomobile;

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Helper to compute bike lean angle parameter
////////////////////////////////////////////////////////////////////////////////

class CBikeLeanAngleHelper : public CTaskHelperFSM
{
public: 

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		bool m_UseReturnOvershoot;
		bool m_UseInitialLeanForcing;
		float m_DesiredLeanAngleTolToBringLegIn;
		float m_DesiredSpeedToBringLegIn;
		float m_DesiredLeanAngleRate;
		float m_DesiredLeanAngleRateQuad;
		float m_LeanAngleReturnRate;
		float m_LeanAngleDefaultRate;
		float m_LeanAngleDefaultRatePassenger;
		float m_DesiredOvershootLeanAngle;
		float m_LeanAngleReturnedTol;
		float m_HasStickInputThreshold;
		float m_LeaningExtremeThreshold;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// FSM states
	enum
	{
		State_Start = 0,
		State_NoStickInput,
		State_HasStickInput,
		State_LeaningExtremeWithStickInput,
		State_Finish
	};

	enum
	{
		State_ReturnInitial = 0,
		State_Returning,
		State_ReturnFinish
	};

	CBikeLeanAngleHelper(CVehicle* pVehicle, CPed* pPed);
	~CBikeLeanAngleHelper();

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return Start_OnUpdate();
	FSM_Return NoStickInput_OnEnter();
	FSM_Return NoStickInput_OnUpdate();
	FSM_Return HasStickInput_OnUpdate();
	FSM_Return LeaningExtremeWithStickInput_OnUpdate();

	void ProcessLeanAngleDefault();
	void ProcessReturnToIdleLean();
	void ProcessInitialLean();
	bool HasStickInput() const;
	bool IsLeaningExtreme() const;

	float GetLeanAngle() const { return m_fLeanAngle; }
	void SetLeanAngle(float LeanAngle) { m_fLeanAngle = LeanAngle; }
	float GetLeanAngleRate() const { return m_fLeanAngleRate; }
	void SetLeanAngleRate(float LeanAngleRate) { m_fLeanAngleRate = LeanAngleRate; }
	float GetPreviousFramesLeanAngle() const { return m_fPreviousFramesLeanAngle; }
	void SetPreviousFramesLeanAngle(float PreviousFramesLeanAngle) { m_fPreviousFramesLeanAngle = PreviousFramesLeanAngle; }
	float GetDesiredLeanAngle() const { return m_fDesiredLeanAngle; }
	void SetDesiredLeanAngle(float DesiredLeanAngle) { m_fDesiredLeanAngle = DesiredLeanAngle; }
	float GetLeanDirection() const { return Sign(m_fLeanAngle - m_fPreviousFramesLeanAngle); }
	s32 GetReturnToIdleState() const { return m_iReturnToIdleState; }
	void SetUseInitialLeanForcing(bool bUseInitialLeanForcing) { m_bUseInitialLeanForcing = bUseInitialLeanForcing; }
	float GetTimeSinceLeaningExtreme() const { return m_fTimeSinceLeaningExtreme; }

private:

	RegdPed m_pPed;
	RegdVeh m_pVehicle;
	float	m_fLeanAngle;
	float	m_fLeanAngleRate;
	float	m_fPreviousFramesLeanAngle;
	float	m_fDesiredLeanAngle;
	float	m_fTimeSinceLeaningExtreme;
	s32		m_iPreviousState;
	s32		m_iReturnToIdleState;
	bool	m_bUseInitialLeanForcing;

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls the characters clips in a vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskMotionInVehicle : public CTaskMotionBase
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //			// NOTES:

		bool m_DisableCloseDoor;

		// ---	END_DEV_TO_REMOVE	--- //

		float m_MaxVehVelocityToKeepStairsDown;
		float m_StillPitchAngleTol;
		float m_MinSpeedForVehicleToBeConsideredStillSqr;
		float m_VelocityDeltaThrownOut;
		float m_VelocityDeltaThrownOutPlayerSP;
		float m_VelocityDeltaThrownOutPlayerMP;
		float m_MinRateForInVehicleAnims;
		float m_MaxRateForInVehicleAnims;
		float m_HeavyBrakeYAcceleration;
		float m_MinRatioForClosingDoor;
		float m_InAirZAccelTrigger;
		float m_InAirProbeDistance;
		float m_InAirProbeForwardOffset;
		float m_MinPitchDefault;
		float m_MaxPitchDefault;
		float m_MinPitchInAir;
		float m_MaxPitchInAir;
		float m_DefaultPitchSmoothingRate;
		float m_BikePitchSmoothingRate;
		float m_BikePitchSmoothingPassengerRate;
		float m_BikeLeanSmoothingPassengerRate;
		float m_WheelieAccelerateControlThreshold;
		float m_WheelieUpDownControlThreshold;
		float m_WheelieMaxSpeedThreshold;
		float m_WheelieDesiredLeanAngleTol;
		float m_StillAccTol;
		float m_AccelerationSmoothing;
		float m_AccelerationSmoothingBike;
		float m_AccelerationScaleBike;
		float m_MinTimeInCurrentStateForStill;
		float m_AccelerationToStartLeaning;
		float m_ZAccelerationToStartLeaning;
		float m_MaxAccelerationForLean;
		float m_MaxXYAccelerationForLeanBike;
		float m_MaxZAccelerationForLeanBike;
		float m_StillDelayTime;
		float m_ShuntAccelerateMag;
		float m_ShuntAccelerateMagBike;
		float m_MinTimeInShuntStateBeforeRestart;
		float m_MaxAbsThrottleForCloseDoor;
		float m_MaxVehSpeedToConsiderClosingDoor;
		float m_MaxDoorSpeedToConsiderClosingDoor;
		float m_MinVehVelocityToGoThroughWindscreen;
		float m_MinVehVelocityToGoThroughWindscreenMP;
		float m_MaxZComponentForCollisionNormal;
		float m_MaxTimeStreamInVehicleClipSetBeforeStartingEngine;
		u32	  m_MinTimeSinceDriverValidToShuffle;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Check for a vehicle event
	static void ProcessSteeringWheelArmIk(const CVehicle& rVeh, CPed& ped);
	static bool CheckForStill(float fTimeStep, const CVehicle& vehicle, const CPed& ped, float fPitchAngle, float& fStillDelayTimer, bool bIgnorePitchCheck = false, bool bCheckTimeInState = false, float fTimeInState = -1.0f);
	static bool CheckForMovingForwards(const CVehicle& vehicle, CPed& ped);
	static bool CheckForShunt(CVehicle& vehicle, CPed& ped, float fZAcceleration);
	static bool CheckForHeavyBrake(const CPed& rPed, const CVehicle& rVeh, float fYAcceleration, u32 uLastTimeLargeZAcceleration);
	static bool CheckForThroughWindow(CPed& ped, CVehicle& vehicle, const Vector3& vPreviousVelocity, Vector3& vThrownForce);
	static bool CheckForReverse(const CVehicle& vehicle, const CPed& ped, bool bFirstPerson = false, bool bIgnoreThrottleCheck = false, bool bIsPanicking = false);
	static bool CheckForClosingDoor(const CVehicle& vehicle, const CPed& ped, bool bIgnoreTaskCheck = false, s32 iTargetSeatIndex = -1, s32 iEntryPointEnteringFrom = -1, bool bCloseDoorInterruptTagCheck = false);
	static bool CheckForStartingEngine(const CVehicle& vehicle, const CPed& ped);
	static bool CheckForHotwiring(const CVehicle& vehicle, const CPed& ped);
	static bool CheckForChangingStation(const CVehicle& vehicle, const CPed& ped);
	static bool CheckForAttachedPedClimbingStairs(const CVehicle& rVeh);
	static bool CheckIfStairsConditionPasses(const CVehicle& rVeh, const CPed& rPed);

	static Vector3 GetCurrentAccelerationSignalNonSmoothed(float fTimeStep, const CVehicle& vehicle, Vector3& vLastFramesVelocity);

	// PURPOSE: Compute a move parameter
	static void ComputeSteeringSignal(const CVehicle& vehicle, float& fCurrentValue, float fSmoothing);
	static void ComputeSteeringSignalWithDeadZone(const CVehicle& vehicle, float& fCurrentValue, u32& nCentreTime, u32 nLastNonCentredTime, u32 uTimeTillCentred, u32 uDeadZoneTime, float fDeadZone, float fSmoothingIn, float fSmoothingOut, bool bInvert);
													 
	static void ComputeAccelerationSignal(float fTimeStep, const CVehicle& vehicle, Vector3& vCurrentValue, const Vector3& vLastFramesVelocity, bool bLastFramesVelocityValid, float fAccelerationSmoothing = ms_Tunables.m_AccelerationSmoothing, bool bIsBike = false);
	static void ComputeAngAccelerationSignal(const CVehicle& vehicle, Vector3& vCurrentValue, Vector3& vLastFramesAngVelocity, bool& bLastFramesVelocityValid, float fTimeStep);
	static void ComputeSpeedSignal(const CPed& rPed, const CVehicle& vehicle, float& fCurrentValue, float fSpeedSmoothing, float fPitch);
	static void ComputePitchSignal(const CPed& ped, float& fCurrentValue, float fSmoothingRate, float fMinPitch = ms_Tunables.m_MinPitchDefault, float fMaxPitch = ms_Tunables.m_MaxPitchDefault);
	static void ComputeLeanSignal(float& fCurrentValue, const Vec3V& vAcceleration);
	static void ComputeBikeLeanAngleSignal(const CPed& ped, CVehicle& vehicle, float& fCurrentValue, float fSmoothingRate, float fDefaultSmoothingRate);
	static void ComputeBikeLeanAngleSignalNew(const CVehicle& vehicle, float& fCurrentValue, float& fDesiredValue, float fSmoothingRate);
	static void ComputeBikeDesiredLeanAngleSignal(const CVehicle& vehicle, float& fCurrentValue, float& fDesiredValue, float fSmoothingRate);

	// PURPOSE: Does a line probe down and returns true if it doesn't hit anything
	static bool IsVerticalProbeClear(const Vector3& vTestPos, float fProbeLength, const CVehicle& vehicle, const CPed& ped);

	static const CCarDoor* GetOpenDoorForDoubleDoorSeat(const CVehicle& vehicle, s32 iSeatIndex, s32& iEntryIndex);
	static const CCarDoor* GetDirectAccessEntryDoorFromPed(const CVehicle& vehicle, const CPed& ped);
	static const CCarDoor* GetDirectAccessEntryDoorFromSeat(const CVehicle& vehicle, s32 iSeatIndex);
	static const CCarDoor* GetDirectAccessEntryDoorFromEntryPoint(const CVehicle& vehicle, s32 iEntryPointIndex);
	static const CSeatOverrideAnimInfo* GetSeatOverrideAnimInfoFromPed(const CPed& ped);
	static bool VehicleHasHandleBars(const CVehicle& rVeh);

	static bool ShouldPerformInVehicleHairScale(const CPed& ped, float& fHairScale);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId ms_StillOnEnterId;
	static const fwMvBooleanId ms_LowLodOnEnterId;
	static const fwMvBooleanId ms_IdleOnEnterId;
	static const fwMvBooleanId ms_ReverseOnEnterId;
	static const fwMvBooleanId ms_CloseDoorOnEnterId;
	static const fwMvBooleanId ms_ShuntOnEnterId;
	static const fwMvBooleanId ms_StartEngineOnEnterId;
	static const fwMvBooleanId ms_HeavyBrakeOnEnterId;
	static const fwMvBooleanId ms_HornOnEnterId;
	static const fwMvBooleanId ms_FirstPersonHornOnEnterId;
	static const fwMvBooleanId ms_PutOnHelmetInterrupt;
	static const fwMvBooleanId ms_FirstPersonSitIdleOnEnterId;
	static const fwMvBooleanId ms_FirstPersonReverseOnEnterId;
	static const fwMvBooleanId ms_IsDoingDriveby;
	static const fwMvBooleanId ms_LowriderArmTransitionFinished;
	static const fwMvRequestId ms_LowLodRequestId;
	static const fwMvRequestId ms_StillRequestId;
	static const fwMvRequestId ms_IdleRequestId;
	static const fwMvRequestId ms_ReverseRequestId;
	static const fwMvRequestId ms_CloseDoorRequestId;
	static const fwMvRequestId ms_ShuntRequestId;
	static const fwMvRequestId ms_HeavyBrakeRequestId;
	static const fwMvRequestId ms_StartEngineRequestId;
	static const fwMvRequestId ms_HornRequestId;
	static const fwMvRequestId ms_FirstPersonHornRequestId;
	static const fwMvRequestId ms_FirstPersonSitIdleRequestId;
	static const fwMvRequestId ms_FirstPersonReverseRequestId;
	static const fwMvFloatId ms_BodyMovementXId;
	static const fwMvFloatId ms_BodyMovementYId;
	static const fwMvFloatId ms_BodyMovementZId;
	static const fwMvFloatId ms_SpeedId;
	static const fwMvFloatId ms_SteeringId;
	static const fwMvFloatId ms_StartEnginePhaseId;
	static const fwMvFloatId ms_StartEngineRateId;
	static const fwMvFloatId ms_SuspensionMovementUDId;
	static const fwMvFloatId ms_SuspensionMovementLRId;
	static const fwMvFloatId ms_WindowHeight;
	static const fwMvFlagId ms_IsDriverId;
	static const fwMvFlagId sm_PedShouldSwayInVehicleId;
	static const fwMvFlagId ms_FrontFlagId;
	static const fwMvFlagId ms_BackFlagId;
	static const fwMvFlagId ms_LeftFlagId;
	static const fwMvFlagId ms_RightFlagId;
	static const fwMvFlagId ms_UseNewBikeAnimsId;
	static const fwMvFlagId ms_HornIsBeingUsedId;
	static const fwMvFlagId ms_BikeHornIsBeingUsedId;
	static const fwMvFlagId ms_IsLowriderId;
	static const fwMvFlagId ms_LowriderTransitionToArm;
	static const fwMvFlagId ms_LowriderTransitionToSteer;
	static const fwMvFlagId ms_UsingLowriderArmClipset;
	static const fwMvClipSetVarId ms_EntryVarClipSetId;
	static const fwMvClipId ms_LowLodClipId;
	static const fwMvStateId ms_InvalidStateId;
	

	////////////////////////////////////////////////////////////////////////////////

	CTaskMotionInVehicle();
	virtual ~CTaskMotionInVehicle();

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define MOTION_IN_VEHICLE_STATES(X)	X(State_Start),						\
										X(State_StreamAssets),				\
										X(State_InAutomobile),				\
										X(State_InRemoteControlVehicle),	\
										X(State_OnBicycle),					\
										X(State_InTurret),					\
										X(State_Finish)

	// PURPOSE: FSM states
	enum eInVehicleState
	{
		MOTION_IN_VEHICLE_STATES(DEFINE_TASK_ENUM)
	};


	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_IN_VEHICLE; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskMotionInVehicle(); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }

	virtual CTask * CreatePlayerControlTask();

	const CVehicleClipRequestHelper& GetVehicleRequestHelper() const 
	{
		taskAssert(GetPed() && !GetPed()->IsLocalPlayer());
		return m_VehicleClipRequestHelper; 
	}

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Start; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Called after position is updated
	virtual bool ProcessPostMovement();

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

	// PURPOSE: Task motion base methods
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state) { return (state == CPedMotionStates::MotionState_InVehicle) ? true : false;}
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const { return state == CPedMotionStates::MotionState_InVehicle; }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds) { speeds.Zero(); }
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual bool IsInVehicle() { return true; }
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);

    //PURPOSE: Get the entry point for the ped's current seat if valid
    const CEntryExitPoint *GetEntryExitPointForPedsSeat(CPed *pPed) const;

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnEnter();
	FSM_Return Start_OnUpdate();
	FSM_Return StreamAssets_OnEnter();
	FSM_Return StreamAssets_OnUpdate();
	FSM_Return InAutomobile_OnEnter();
	FSM_Return InAutomobile_OnUpdate();
	FSM_Return InRemoteControlVehicle_OnEnter();
	FSM_Return InRemoteControlVehicle_OnUpdate();
	FSM_Return OnBicycle_OnEnter();
	FSM_Return OnBicycle_OnUpdate();
	FSM_Return InTurret_OnEnter();
	FSM_Return InTurret_OnUpdate();

private:

	////////////////////////////////////////////////////////////////////////////////

	void ProcessDamageDeath();
	void ProcessShuffleForGroupMember();
	bool CheckCanPedMoveToDriversSeat(const CPed& ped, const CVehicle& vehicle);
	bool GetShouldExitVehicle(CPed* pPed);
	bool ShouldBlockArmIkDueToLowRider(const CTaskMotionInAutomobile* pInAutomobile) const;

private:

	

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: The vehicle the ped is in
	RegdVeh m_pVehicle;
	RegdPed m_pDriver;
	u32		m_uTimeDriverValid;
	u32		m_uTimeInVehicle;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Request vehicle clips
	CVehicleClipRequestHelper m_VehicleClipRequestHelper;

	bool m_bWasUsingPhone;
	bool m_bDriverWasPlayerLastUpdate;

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, MOTION_IN_VEHICLE_STATES)

#endif // !__FINAL

#if __DEV
	static void VerifyNetworkStartUp(const CVehicle& vehicle, const CPed& ped, const fwMvClipSetId& seatClipSetId);
#endif

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls the characters clips in an automobile
////////////////////////////////////////////////////////////////////////////////

class CTaskMotionInAutomobile : public CTaskMotionBase
{

private:

	enum DefaultClipSet
	{
		DCS_Base,
		DCS_Idle,
		DCS_Panic,
		DCS_Agitated,
		DCS_Ducked,
		DCS_LowriderLean,
		DCS_LowriderLeanAlt,
	};

public:

#if __BANK
	#define SWITCH_TO_STRING_AUTOMOBILE(X) case X : return #X;
	static const char* GetDefaultClipSetString(DefaultClipSet dc)
	{
		switch (dc)
		{
			SWITCH_TO_STRING_AUTOMOBILE(DCS_Base)
			SWITCH_TO_STRING_AUTOMOBILE(DCS_Idle)
			SWITCH_TO_STRING_AUTOMOBILE(DCS_Panic)
			SWITCH_TO_STRING_AUTOMOBILE(DCS_Agitated)
			SWITCH_TO_STRING_AUTOMOBILE(DCS_Ducked)
			SWITCH_TO_STRING_AUTOMOBILE(DCS_LowriderLean)
			SWITCH_TO_STRING_AUTOMOBILE(DCS_LowriderLeanAlt)
			default: break;
		}
		return "Unknown, please add to CTaskMotionInAutomobile::GetDefaultClipSetString";
	}
	#undef SWITCH_TO_STRING_AUTOMOBILE
#endif // __BANK

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //			// NOTES:

		bool m_TestLowLodIdle;						// Force into the low lod idle

		// ---	END_DEV_TO_REMOVE	--- //

		float m_DefaultShuntToIdleBlendDuration;
		float m_PanicShuntToIdleBlendDuration;

		float m_MaxVelocityForSitIdles;
		float m_MaxSteeringAngleForSitIdles;
		u32	  m_MinCentredSteeringAngleTimeForSitIdles;
		float m_MinTimeInHornState;
		
		float m_LeanSidewaysAngleSmoothingRateMin;
		float m_LeanSidewaysAngleSmoothingRateMax;
		float m_LeanSidewaysAngleSmoothingAcc;
		float m_LeanSidewaysAngleMinAccAngle;
		float m_LeanSidewaysAngleMaxAccAngle;

		float m_LeftRightStickInputSmoothingRate;
		float m_LeftRightStickInputMin;
		float m_LeanForwardsAngleSmoothingRate;
		float m_UpDownStickInputSmoothingRate;
		float m_UpDownStickInputMin;

		float m_ZAccForLowImpact;
		float m_ZAccForMedImpact;
		float m_ZAccForHighImpact;
		float m_StartEngineForce;

		bool m_UseLegIkOnBikes;
		float m_LargeVerticalAccelerationDelta;
		s32 m_NumFramesToPersistLargeVerticalAcceleration;

		fwMvClipSetId m_LowLodIdleClipSetId;
		float m_SeatDisplacementSmoothingRateDriver;
		float m_SeatDisplacementSmoothingRatePassenger;
		float m_SeatDisplacementSmoothingStandUpOnJetski;
		float m_TimeInWheelieToEnforceMinPitch;

		float m_MinForwardsPitchSlope;
		float m_MaxForwardsPitchSlope;

		float m_MinForwardsPitchSlopeBalance;
		float m_MaxForwardsPitchSlopeBalance;

		float m_MinForwardsPitchWheelieBalance;
		float m_MinForwardsPitchWheelieBegin;
		float m_MaxForwardsPitchWheelieBalance;

		float m_SlowFastSpeedThreshold;
		float m_MinForwardsPitchSlowSpeed;
		float m_MaxForwardsPitchSlowSpeed;
		float m_MinForwardsPitchFastSpeed;
		float m_MaxForwardsPitchFastSpeed;
		float m_SlowApproachRate;
		float m_FastApproachRate;
		float m_WheelieApproachRate;

		float m_NewLeanSteerApproachRate;
		float m_MinTimeBetweenCloseDoorAttempts;
		float m_ShuntDamageMultiplierAI;
		float m_ShuntDamageMultiplierPlayer;
		float m_MinDamageTakenToApplyDamageAI;
		float m_MinDamageTakenToApplyDamagePlayer;
		float m_MinTimeInTaskToCheckForDamage;
		float m_MinDamageToCheckForRandomDeath;
		float m_MaxDamageToCheckForRandomDeath;
		float m_MinHeavyCrashDeathChance;
		float m_MaxHeavyCrashDeathChance;
		float m_MinDisplacementScale;
		float m_DisplacementScaleApproachRateIn;
		float m_DisplacementScaleApproachRateOut;

		u32 m_SteeringDeadZoneCentreTimeMS;
		u32 m_SteeringDeadZoneTimeMS;
		float m_SteeringDeadZone;

		// PURPOSE:	When timeslicing, how much does the steering have to change
		//			before we start to update the MoVE signal every frame.
		float m_SteeringChangeToStartProcessMoveSignals;

		// PURPOSE:	When timeslicing, how much does the steering have to change
		//			before we stop updating the MoVE signal every frame.
		float m_SteeringChangeToStopProcessMoveSignals;

		float m_SeatBlendLinSpeed;
		float m_SeatBlendAngSpeed;
		float m_HoldLegOutVelocity;
		float m_ForcedLegUpVelocity;
		float m_MinVelStillStart;
		float m_MinVelStillStop;
		float m_BurnOutBlendInTol;
		float m_BurnOutBlendInSpeed;
		float m_BurnOutBlendOutSpeed;
		float m_BikeInAirDriveToStandUpTimeMin;
		float m_BikeInAirDriveToStandUpTimeMax;
		float m_MinSpeedToBlendInDriveFastFacial;

		float m_SteeringWheelBlendInApproachSpeed;
		float m_SteeringWheelBlendOutApproachSpeed;
		float m_FirstPersonSteeringWheelAngleApproachSpeed;
		float m_FirstPersonSteeringTangentT0;
		float m_FirstPersonSteeringTangentT1;
		float m_FirstPersonSteeringSplineStart;
		float m_FirstPersonSteeringSplineEnd;
		float m_FirstPersonSteeringSafeZone;
		float m_FirstPersonSmoothRateToFastSteering;
		float m_FirstPersonSmoothRateToSlowSteering;
		float m_FirstPersonMaxSteeringRate;
		float m_FirstPersonMinSteeringRate;
		float m_FirstPersonMinVelocityToModifyRate;
		float m_FirstPersonMaxVelocityToModifyRate;
		float m_SitToStillForceTriggerVelocity;
		float m_MinVelocityToAllowAltClipSet;
		float m_MaxVelocityToAllowAltClipSet;
		float m_MaxVelocityToAllowAltClipSetWheelie;
		float m_LeanThresholdToAllowAltClipSet;
		float m_AltLowRiderBlendDuration;
		float m_MaxThrottleForAltLowRiderPose;
		float m_TimeSinceLastDamageToAllowAltLowRiderPoseLowRiderPose;
		float m_MaxPitchForBikeAltAnims;
		float m_MinTimeInLowRiderClipset;

		u32 m_FirstPersonTimeToAllowFastSteering;
		u32 m_FirstPersonTimeToAllowSlowSteering;

		u32	m_MinTimeSinceDriveByForAgitate;
		u32	m_MinTimeSinceDriveByForCloseDoor;

		fwMvClipId m_StartEngineClipId;
		fwMvClipId m_FirstPersonStartEngineClipId;
		fwMvClipId m_HotwireClipId;
		fwMvClipId m_FirstPersonHotwireClipId;
		fwMvClipId m_PutOnHelmetClipId;
		fwMvClipId m_PutOnHelmetFPSClipId;
		fwMvClipId m_PutHelmetVisorUpClipId;
		fwMvClipId m_PutHelmetVisorDownClipId;
		fwMvClipId m_PutHelmetVisorUpPOVClipId;
		fwMvClipId m_PutHelmetVisorDownPOVClipId;
		fwMvClipId m_ChangeStationClipId;
		fwMvClipId m_StillToSitClipId;
		fwMvClipId m_SitToStillClipId;
		fwMvClipId m_BurnOutClipId;
		fwMvClipId m_BikeHornIntroClipId;
		fwMvClipId m_BikeHornClipId;
		fwMvClipId m_BikeHornOutroClipId;
		float m_RestartIdleBlendDuration;
		float m_RestartIdleBlendDurationpassenger;
		float m_DriveByToDuckBlendDuration;
		float m_MinTimeDucked;
		float m_DuckControlThreshold;
		float m_DuckBikeSteerThreshold;

		float m_MinTimeToNextFirstPersonAdditiveIdle;
		float m_MaxTimeToNextFirstPersonAdditiveIdle;
		float m_FirstPersonAdditiveIdleSteeringCentreThreshold;
		float m_MaxDuckBreakLockOnTime;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define MOTION_IN_AUTOMOBILE_STATES(X)	X(State_Start),						\
											X(State_StreamAssets),				\
											X(State_Still),						\
											X(State_StillToSit),				\
											X(State_PutOnHelmet),				\
											X(State_SitIdle),					\
											X(State_SitToStill),				\
											X(State_Reverse),					\
											X(State_ReserveDoorForClose),		\
											X(State_CloseDoor),					\
											X(State_StartEngine),				\
											X(State_Shunt),						\
											X(State_ChangeStation),				\
											X(State_Hotwiring),					\
											X(State_HeavyBrake),				\
											X(State_Horn),						\
											X(State_FirstPersonSitIdle),		\
											X(State_FirstPersonReverse),		\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eInAutomobileState
	{
		MOTION_IN_AUTOMOBILE_STATES(DEFINE_TASK_ENUM)
	};


	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(const CVehicle* pVehicle, s32 iSeatIndex);

	static bool ShouldUseFemaleClipSet(const CPed& rPed, const CVehicleSeatAnimInfo& rClipInfo);

	// PURPOSE: Stream any required assets, return true when they are streamed in
	static fwMvClipSetId GetInVehicleClipsetId(const CPed& rPed, const CVehicle* pVehicle, s32 iSeatIndex);

	// PURPOSE: Do we use the bike in vehicle anims?
	static bool ShouldUseBikeInVehicleAnims(const CVehicle& vehicle);

	// PURPOSE: Common parts of checking if we need to put on a helmet
	static bool WillPutOnHelmet(const CVehicle& vehicle, const CPed& ped);

	//PURPOSE: Are we allowed to interrupt putting on helmet
	static bool WillAbortPuttingOnHelmet(const CVehicle& vehicle, const CPed& ped);

	// PURPOSE: Are feet animated to off the ground?
	static bool AreFeetOffGround(float fTimeStep, const CVehicle& vehicle, const CPed& ped);

	static bool IsDoingAWheelie(const CVehicle& rVeh);
	static bool CheckIfVehicleIsDoingBurnout(const CVehicle& rVeh, const CPed& rPed);

	static bool IsVehicleDrivable(const CVehicle& rVeh);
	static bool WantsToDuck(const CPed& rPed, bool bIgnoreCustomVehicleChecks = false);
	static bool ShouldUseCustomPOVSettings(const CPed& rPed, const CVehicle& rVeh);
	static float ComputeLeanAngleSignal(const CVehicle& rVeh);
	static bool IsFirstPersonDrivingJetski(const CPed& rPed, const CVehicle& rVeh);
	static bool IsFirstPersonDrivingQuadBike(const CPed& rPed, const CVehicle& rVeh); 
	static bool IsFirstPersonDrivingBicycle(const CPed& rPed, const CVehicle& rVeh); 
	static bool IsFirstPersonDrivingBikeQuadBikeOrBicycle(const CPed& rPed, const CVehicle& rVeh);

	static bool CheckForHelmetVisorSwitch(CPed& Ped, bool bWaitingForVisorPropToStreamIn = false, bool bPlayingVisorSwitchAnim = false);

	////////////////////////////////////////////////////////////////////////////////

	CTaskMotionInAutomobile(CVehicle* pVehicle, CMoveNetworkHelper& moveNetworkHelper);
	virtual ~CTaskMotionInAutomobile();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_IN_AUTOMOBILE; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskMotionInAutomobile(m_pVehicle, m_moveNetworkHelper); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }

	const Vec3V& GetAcceleration() const { return m_vCurrentVehicleAcceleration; }

	// PURPOSE: Get the force we should be thrown out at
	const Vector3& GetThrownForce() const { return m_vThrownForce; }

	// PURPOSE: Interface Functions
	bool IsHotwiringVehicle() const { return GetState() == State_Hotwiring ? true : false; }

	float GetBlendDurationForClipSetChange() const;
	float GetTimeDucked() const { return m_fTimeDucked; }
	float GetPitchAngle() const { return m_fPitchAngle; }
	float GetBodyMovementY();
	const CSeatOverrideAnimInfo* GetSeatOverrideAnimInfo() const { return m_pSeatOverrideAnimInfo; }

	void  SetRemoteDesiredPitchAngle(float fPitchAngle) { m_fRemoteDesiredPitchAngle = fPitchAngle; }

	void SetSitIdleMoveNetworkState();
	s32 GetSitIdleState() const;

	bool IsUsingFirstPersonSteeringAnims() const;
	bool IsUsingFirstPersonAnims() const;
	bool CanDoInstantSwitchToFirstPersonAnims() const;
	bool CanTransitionToFirstPersonAnims(bool bShouldTestCamera = true) const;
	bool CanDelayFirstPersonCamera() const;
	float GetSteeringWheelWeight() const;
	bool IsDucking() const { return m_bIsDucking; }
	float GetSteeringWheelAngle() const;
	void UpdateFirstPersonAdditiveIdles();
	void FinishFirstPersonIdleAdditive();
	bool CanPedBeVisibleInPoVCamera(bool bShouldTestCamera) const;
	bool CanUsePoVAnims() const;
	bool CanBlockCameraViewModeTransitions() const; 
	void UpdateFirstPersonAnimationState();

	void TriggerRoadRage();
	void PickRoadRageAnim();

	bool CanBeDucked(bool bIgnoreDrivebyCheck = false) const;
	bool ShouldBeDucked() const;

	bool CheckCanPedMoveToDriversSeat() const { return GetState() != State_PutOnHelmet; }

	// Lowrider alt clipset triggering functions.
	bool ShouldUseLowriderLeanAltClipset(bool bCheckPanicOrAgitated = true) const;
	bool PassesVelocityConditionForBikeAltAnims() const;
	bool PassesLeanConditionForBikeAltAnims() const;
	bool PassesActionConditionsForBikeAltAnims() const;
	bool PassesWheelConditionsForBikeAltAnims() const;
	bool PassesPassengerConditionsForBikeAltAnims() const;
	bool CanUseLowriderLeanAltClipset() const;
	bool IsDoingLowriderArmTransition() const { return m_bPlayingLowriderClipsetTransition; }
	bool IsUsingAlternateLowriderClipset() const { return (m_nDefaultClipSet == DCS_LowriderLeanAlt); }

	bool IsPlayingVisorSwitchAnim() const { return m_bPlayingVisorSwitchAnim; }
	bool IsPlayingVisorUp() const { return m_bPlayingVisorUpAnim; }
	bool IsWaitingForVisorToStreamIn() const { return m_bWaitingForVisorPropToStreamIn; }
	bool IsUsingFPVisorArmIK() const { return m_bUsingVisorArmIK; }

protected:


	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Start; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after position is updated
	virtual bool ProcessPostMovement();

	// PURPOSE: Called after camera is updated
	virtual bool ProcessPostCamera();

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

	// PURPOSE: Task motion base methods
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state) { return (state == CPedMotionStates::MotionState_InVehicle) ? true : false;}
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const { return state == CPedMotionStates::MotionState_InVehicle; }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds) { speeds.Zero(); }
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual bool IsInVehicle() { return true; }
	virtual CTask * CreatePlayerControlTask();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return StreamAssets_OnEnter();
	FSM_Return StreamAssets_OnUpdate();
	FSM_Return Still_OnEnter();
	FSM_Return Still_OnUpdate();
	FSM_Return Still_OnExit();
	FSM_Return StillToSit_OnEnter();
	FSM_Return StillToSit_OnUpdate();
	FSM_Return PutOnHelmet_OnEnter();
	FSM_Return PutOnHelmet_OnUpdate();
	FSM_Return PutOnHelmet_OnExit();
	FSM_Return SitIdle_OnEnter();
	FSM_Return SitIdle_OnUpdate();
	FSM_Return SitToStill_OnEnter();
	FSM_Return SitToStill_OnUpdate();
	FSM_Return SitToStill_OnExit();
	FSM_Return Reverse_OnEnter();
	FSM_Return Reverse_OnUpdate();
	FSM_Return Shunt_OnEnter();
	FSM_Return Shunt_OnUpdate();
    FSM_Return ReserveDoorForClose_OnUpdate();
	FSM_Return CloseDoor_OnEnter();
	FSM_Return CloseDoor_OnUpdate();
	FSM_Return CloseDoor_OnExit();
	FSM_Return StartEngine_OnEnter();
	FSM_Return StartEngine_OnUpdate();
	FSM_Return ChangeStation_OnEnter();
	FSM_Return ChangeStation_OnUpdate();
	FSM_Return Hotwiring_OnEnter();
	FSM_Return Hotwiring_OnUpdate();
	FSM_Return HeavyBrake_OnEnter();
	FSM_Return HeavyBrake_OnUpdate();
	FSM_Return Horn_OnEnter();
	FSM_Return Horn_OnUpdate();
	FSM_Return FirstPersonSitIdle_OnEnter();
	FSM_Return FirstPersonSitIdle_OnUpdate();
	FSM_Return FirstPersonReverse_OnEnter();
	FSM_Return FirstPersonReverse_OnUpdate();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	////////////////////////////////////////////////////////////////////////////////

	bool CheckIfPedShouldSwayInVehicle(const CVehicle* pVehicle);
	void DriveCloseDoorFromClip(CVehicle* pVehicle, s32 iEntryPointIndex, const crClip* pCloseDoorClip, const float fOpenDoorPhase);
	bool CheckForHelmet(const CPed& Ped) const;
	bool CheckForHorn(bool bAllowBikes = false) const;
	bool StartMoveNetwork(const CVehicleSeatAnimInfo* pSeatClipInfo);
	bool SetSeatClipSet(const CVehicleSeatAnimInfo* pSeatClipInfo);
	bool SetEntryClipSet();
	bool IsCloseDoorStateValid();
	bool IsWithinRange(float fTestValue, float fMin, float fMax);
	void ComputePitchParameter(float fTimeStep);
	void ProcessInVehicleOverride();
	void ProcessHelmetVisorSwitching();
	void ProcessBlockHandIkForVisor();
	void ProcessDefaultClipSet();
	bool ShouldSkipDuckIntroOutros() const;
	bool CanPanic() const;
	bool ShouldPanic() const;
	bool ShouldPanicDueToDriverBeingJacked() const;
	bool CanBeAgitated() const;
	bool ShouldBeAgitated() const;
	bool PlayerShouldBeAgitated() const;
	bool ShouldUseBaseIdle() const;
	void DontUsePassengerLeanAnims(bool bValue);
	void UseLeanSteerAnims(bool bValue);
	void UseIdleSteerAnims(bool bValue);
	bool ProcessImpactReactions(CPed& rPed, CVehicle& rVeh);
	void ProcessDamageFromImpacts(CPed& rPed, const CVehicle& rVeh, bool bGoingThroughWindscreen);
	void ProcessInAirHeight(CPed& rPed, CVehicle& rVeh, float fTimeStep);
	void ProcessRagdollDueToVehicleOrientation();
	void ProcessPositionReset(CPed& rPed);
	void ProcessParameterSmoothingNoScale(float& fCurrentValue, float fDesiredValue, float fApproachSpeedMin, float fApproachSpeedMax, float fSmallDelta, float fTimeStep);
	void ProcessParameterSmoothing(float& fCurrentValue, float fDesiredValue, float& fApproachSpeed, float fApproachSpeedMin, float fApproachSpeedMax, float fSmallDelta, float fTimeStep, float fTimeStepScale);
	void ComputeAccelerationParameter(const CPed& rPed, const CVehicle& rVeh, float fTimeStep);
	void ComputeSteeringParameter(const CVehicleSeatAnimInfo& rSeatClipInfo, const CVehicle& rVeh, float fTimeStep);
	float Compute2DBodyBlendSteeringParam(const CVehicle& rVeh, float fCurrentSteeringValue, float fTimeStep);
	void ComputeBikeParameters(const CPed& rPed);

	// B*2305888: Lowrider lean functions:
	bool  ShouldUseLowriderLean() const;
	bool  HasModifiedLowriderSuspension() const;	
	void  ComputeLowriderParameters();
	bool  ShouldUseLowriderAltClipsetActionTransition() const;

	// B*3057565: Reusing lowrider alt arm system for bike alternate anims
	
	bool  ShouldUseBikeAltClipset() const;

	void ProcessDrivingAggressively();
	
	// Lateral lean functions:
	// PURPOSE: Randomize spring parameters so we have slight variations in movement between the peds in the vehicle.
	void  SetupLowriderSpringParameters();

	// PURPOSE: Sets the L/R roll parameter in the MoVE network.
	void  ComputeLowriderLateralRoll();
	// PURPOSE: To make the driver/passenger sway left/right based on the forces being applied to the lowrider vehicle.
	// RETURNS: A float that represents the target lateral lean signal.
	float ComputeLowriderLeanLateral();
	// PURPOSE: To calculate the acceleration due to changes in the lateral roll of the lowrider.
	// RETURNS: The acceleration that the lowrider is experiencing due to lateral roll.
	float CalculateLowriderAccelerationLateral();
	// PURPOSE: To calculate the force of the spring that drives the swaying of the ped in the lowrider.
	// RETURNS: The response of the spring.
	float CalculateSpringForceLateral() const;

	// Longitudinal lean functions:
	// PURPOSE: Sets the U/D roll parameter in the MoVE network.
	void  ComputeLowriderLongitudinalRoll();
	// PURPOSE: To make the driver/passenger sway forward/back based on the forces being applied to the lowrider vehicle.
	// RETURNS: A float that represents the target longitudinal lean signal.
	float ComputeLowriderLeanLongitudinal();
	// PURPOSE: To calculate the acceleration due to changes in the longitudinal roll of the lowrider.
	// RETURNS: The acceleration that the lowrider is experiencing due to longitudinal roll.
	float CalculateLowriderAccelerationLongitudinal();
	// PURPOSE: To calculate the acceleration due to changes in the Z velocity of the lowrider.
	// RETURNS: The acceleration that the lowrider is experiencing due to forcing all wheels up/down.
	float CalculateLowriderAccelerationZ();
	// PURPOSE: To calculate the force of the spring that drives the swaying of the ped in the lowrider.
	// RETURNS: The response of the spring.
	float CalculateSpringForceLongitudinal() const;

	void SetClipPlaybackRate();
	void SetHotwireClip();
	bool SetClipsForState();
	bool CheckForChangingStation(const CVehicle& rVeh, const CPed& rPed) const;
	bool CheckForStillToSitTransition(const CVehicle& rVeh, const CPed& rPed) const;
	bool ShouldUseSitToStillTransition(const CVehicle& rVeh, const CPed& rPed) const;
	bool CheckForSitToStillTransition(const CVehicle& rVeh, const CPed& rPed) const;
	float CheckIfPedWantsToMove(const CPed& rPed, const CVehicle& rVeh) const;
	bool CheckForShuntFromHighFall(const CVehicle& rVeh, const CPed& rPed, bool bForFPSCameraShake = false);
	bool CheckForStartingEngine(const CVehicle& vehicle, const CPed& ped);
	void ProcessFacial();
	void ProcessAutoCloseDoor();
	void SetUpRagdollOnCollision(CPed& rPed, CVehicle& rVeh);
	void ResetRagdollOnCollision(CPed& rPed);

	void SmoothSteeringWheelSpeedBasedOnInput(const CVehicle& vehicle, 
											  float& fCurrentSteeringRate,
											  float fSteeringSafeZone,
											  u32 iTimeToAllowFastSteering,
											  u32 iTimeToAllowSlowSteering,
											  float fSmoothRateToFastSteering,
											  float fSmoothRateToSlowSteering,
											  float fMaxSteeringRate,
											  float fMinSteeringRate,
											  float fMinVelocityToModifyRate,
											  float fMaxVelocityToModifyRate);

	void StartFirstPersonCameraShake(const float fJumpHeight);

	// PURPOSE: Given a moment in time, a start value, an end value and the tangents at the
	// beginning and ending point, this function creates a spline.
	// PARAMS:
	// - fTime: Moment in time (between 0 and 1) where we should be looking for a value in the spline.
	// - fStartValue: Start value of the spline.
	// - fEndValue: End value of the spline.
	// - fTangentT0: Value of the tangent for the moment t = 0
	// - fTangengT1: Value of the tangent for the moment t = 1
	// RETURNS: Given a value between 0 and 1, it returns the mapped value in the spline based
	// on the curves formed by the tangents.
	float CubicHermiteSpline(float fTime, float fStartValue, float fEndValue, float fTangentT0, float fTangentT1)
	{
		return (((2.0f * fTime * fTime * fTime) - (3.0f * fTime * fTime) + 1.0f) * fStartValue)	+
				(((fTime * fTime * fTime) - (2.0f * fTime * fTime) + fTime) * fTangentT0)		+
				(((-2.0f * fTime * fTime * fTime) + (3.0f * fTime * fTime)) * fEndValue)		+
				(((fTime * fTime * fTime) - (fTime * fTime)) * fTangentT1);
	}

	const crClip* GetClipForStateNoAssert(s32 iState, fwMvClipId* pMvClipId = NULL) const
	{
		return GetClipForState(iState, pMvClipId, false);
	}
	const crClip* GetClipForStateWithAssert(s32 iState, fwMvClipId* pMvClipId = NULL) const
	{
		return GetClipForState(iState, pMvClipId, true);
	}
	const crClip* GetClipForState(s32 iState, fwMvClipId* pMvClipId = NULL, bool bAssertIfClipDoesntExist = true) const;

	fwMvClipSetId GetAgitatedClipSet() const;
	fwMvClipSetId GetDuckedClipSet() const;
	fwMvClipSetId GetPanicClipSet() const;
	fwMvClipSetId GetSeatClipSetId() const;
	fwMvClipSetId GetLowriderLeanClipSet() const;
	fwMvClipSetId GetLowriderLeanAlternateClipSet() const;

	DefaultClipSet	GetDefaultClipSetToUse() const;
	fwMvClipSetId	GetDefaultClipSetId(DefaultClipSet nDefaultClipSet) const;

	bool IsPanicking() const
	{
		return (m_nDefaultClipSet == DCS_Panic);
	}

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Holds Bike Specific Variables
	struct sBikeAnimParams
	{
		sBikeAnimParams(CVehicle* pVeh, CPed* pPed) : fSeatDisplacement(0.0f)
						  , fPreviousSeatDisplacementSignal(0.5f)
						  , fPreviousTargetSeatDisplacement(0.0f)
						  , iNumFramesToHoldVerticalAcceleration(0)
						  , bikeLeanAngleHelper(pVeh, pPed)
						  , bLargeVerticalAccelerationDetected(false)
						  , bIsDoingWheelie(false)
						  , fTimeDoingWheelie(0.0f)
		{}

		~sBikeAnimParams() {}

		float					fSeatDisplacement;
		float					fPreviousSeatDisplacementSignal; 
		float					fPreviousTargetSeatDisplacement;
		s32						iNumFramesToHoldVerticalAcceleration;
		CBikeLeanAngleHelper	bikeLeanAngleHelper;
		camDampedSpring			seatDisplacementSpring;
		bool					bLargeVerticalAccelerationDetected;
		bool					bIsDoingWheelie;
		float					fTimeDoingWheelie;
	};

	// Velocity tracking
	Vec3V m_vLastFramesVehicleVelocity;
	Vec3V m_vCurrentVehicleAcceleration;
	Vec3V m_vCurrentVehicleAccelerationNorm;

#if __DEV
	Vec3V m_vDebugLastVelocity;
	Vec3V m_vDebugCurrentVelocity;
	Vec3V m_vDebugCurrentVehicleAcceleration;
#endif // __DEV

#if __ASSERT
	CTaskSimpleTimer m_TaskStateTimer;
#endif // __ASSERT

	fwClipSetRequestHelper m_AdditiveIdleClipSetRequestHelper;
	fwClipSetRequestHelper m_RoadRageClipSetRequestHelper;
	float m_fTimeTillNextAdditiveIdle;
	bool m_bPlayingFirstPersonAdditiveIdle;
	bool m_bPlayingFirstPersonShunt;
	bool m_bPlayingFirstPersonRoadRage;

	fwClipSetRequestHelper m_HelmetVisorFirsPersonClipSetRequestHelper;

	// PURPOSE: Last time that the steering wheel was in the
	// "slow zone" (the zone where the lerp goes slow) for FPS steering
	u32		m_iLastTimeWheelWasInSlowZone;
	// PURPOSE: Last time that the steering wheel was in the
	// "fast zone" (the zone where the lerp goes fast) for FPS steering
	u32		m_iLastTimeWheelWasInFastZone;
	// PURPOSE: Used to help scale the fast lerp rate when we get a significant change in steering angle.
	float	m_fSteeringAngleDelta;

	//Vector3 m_vLastFramesVelocity;
	//Vector3 m_vLastFramesAngularVelocity;
	Vector3 m_vPreviousFrameAcceleration;
	//Vector3 m_vAcceleration;
	//Vector3 m_vAngularAcceleration;
	Vector3 m_vThrownForce;
	CMoveNetworkHelper& m_moveNetworkHelper;
	RegdVeh m_pVehicle;
	float	m_fApproachRateVariance;
	float	m_fBodyLeanX;
	float	m_fBodyLeanApproachRateX;
	float	m_fBodyLeanY;
	float	m_fBodyLeanApproachRateY;
	float   m_fSpeed;
	float   m_fSteering;
	u32		m_uTaskStartTimeMS;
	u32		m_uLastSteeringCentredTime;
	u32		m_uLastSteeringNonCentredTime;
	u32		m_uLastTimeLargeZAcceleration;
	u32		m_uStateSitIdle_OnEnterTimeMS;
	u32		m_uLastDamageTakenTime;
	u32		m_uLastDriveByTime;
	float	m_FirstPersonSteeringWheelCurrentAngleApproachSpeed;
	float   m_fPitchAngle;
	float	m_fRemoteDesiredPitchAngle;
	float	m_fEngineStartPhase;
	float	m_fIdleTimer;
	float	m_fDuckTimer;
	float	m_fTimeDucked;
	float	m_fStillTransitionRate;
	float	m_fHoldLegTime;
	float	m_fBurnOutBlend;
	float	m_fTimeSinceBurnOut;
	float	m_fBurnOutPhase;
	float	m_fMinHeightInAir;
	float	m_fMaxHeightInAir;
	float	m_fTimeInAir;
	float	m_fDisplacementDueToPitch;
	float	m_fDisplacementScale;
	float	m_fFrontWheelOffGroundScale;
	float	m_fSteeringWheelWeight;
	float	m_fSteeringWheelAngle;
	float	m_fSteeringWheelAnglePrev;
	u32		m_TimeControlLastDisabled;
	const CInVehicleOverrideInfo* m_pInVehicleOverrideInfo;
	const CSeatOverrideAnimInfo* m_pSeatOverrideAnimInfo;
	sBikeAnimParams* m_pBikeAnimParams;
	Quaternion	m_qInitialPedRotation;
	DefaultClipSet m_nDefaultClipSet;
	bool	m_bLastFramesVelocityValid : 1;
	bool	m_bCloseDoorSucceeded : 1;
	bool	m_bCurrentAnimFinished : 1;		// Used by ProcessMoveSignals() during certain states.
	bool	m_bTurnedOnDoorHandleIk : 1;
	bool	m_bTurnedOffDoorHandleIk : 1;
	bool	m_bDefaultClipSetChangedThisFrame : 1;
	bool	m_bUseLeanSteerAnims : 1;
	bool	m_bDontUsePassengerLeanAnims : 1;
	bool	m_bUseIdleSteerAnims : 1;
	bool 	m_bHasFinishedBlendToSeatPosition : 1;
	bool 	m_bHasFinishedBlendToSeatOrientation : 1;
	bool	m_bWantsToMove : 1;
	bool	m_bStoppedWantingToMove : 1;
	bool	m_bWasJustInAir : 1;
	bool	m_bHasTriedToStartUndriveableVehicle : 1;
	bool	m_bDontUseArmIk : 1;
	bool	m_bSetUpRagdollOnCollision : 1;
	bool	m_bUseFirstPersonAnims : 1;
	bool	m_bWasInFirstPersonVehicleCamera : 1;
	bool	m_bIsDucking : 1;
	bool	m_bWasDucking : 1;
	bool	m_bUseHelmetPutOnUpperBodyOnlyFPS : 1;
	bool	m_bWaitingForVisorPropToStreamIn : 1;
	bool	m_bPlayingVisorSwitchAnim : 1;
	bool	m_bWasPlayingVisorSwitchAnimLastUpdate : 1;
	bool	m_bPlayingVisorUpAnim : 1;
	bool	m_bUsingVisorArmIK : 1;
	bool	m_bIsAltRidingToStillTransition : 1;
	bool	m_bForceFinishTransition : 1;

	// B*2305888: Lowrider lean params.
	float	m_fLowriderLeanX;							// Parameter passed into MoVE for lateral lean.
	float	m_fLowriderLeanY;							// Parameter passed into MoVE for longitudinal lean.
	float	m_fLowriderLeanXVelocity;
	float	m_fLowriderLeanYVelocity;
	float	m_fPreviousLowriderVelocityLateral;		
	float	m_fPreviousLowriderVelocityLongitudinal;	
	Vector3	m_vPreviousLowriderVelocity;				// Used to calculate Z acceleration in CalculateLowriderAccelerationZ.
	Mat34V	m_mPreviousLowriderMat;						// Used to calculate lateral/longitudinal lean accelerations based on delta from current matrix.
	float   m_fTimeToTriggerAltLowriderClipset;
	float	m_fAltLowriderClipsetTimer;
	CTaskSimpleTimer m_MinTimeInAltLowriderClipsetTimer;
	u32		m_iTimeLastAcceleratedAggressively;
	u32		m_iTimeStartedAcceleratingAggressively;
	u32		m_iTimeStoppedDrivingAtAggressiveSpeed;
	int     m_iTimeLastModifiedHydraulics;
	bool	m_bPlayingLowriderClipsetTransition;
	bool	m_bDrivingAggressively;

	// Jetski stand.
	float m_fJetskiStandPoseBlendTime;

	// Lowrider spring parameters.
	bool  m_bInitializedLowriderSprings;
	float m_fLateralLeanSpringK;
	float m_fLateralLeanMinDampingK;
	float m_fLateralLeanMaxExtraDampingK;
	float m_fLongitudinalLeanSpringK;
	float m_fLongitudinalLeanMinDampingK;
	float m_fLongitudinalLeanMaxExtraDampingK;

#if FPS_MODE_SUPPORTED
	static ioRumbleEffect m_RumbleEffect;
	CFirstPersonIkHelper* m_pFirstPersonIkHelper;
#endif	//FPS_MODE_SUPPORTED

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId ms_CloseDoorClipFinishedId;
	static const fwMvBooleanId ms_EngineStartClipFinishedId;
	static const fwMvClipId ms_CloseDoorClipId;
	static const fwMvClipId ms_StartEngineClipId;
	static const fwMvClipId ms_HotwireClipId;
	static const fwMvClipId ms_InputHotwireClipId;
	static const fwMvFloatId ms_CloseDoorPhaseId;
	static const fwMvFloatId ms_StartEnginePhaseId;
	static const fwMvFloatId ms_ChangeStationPhaseId;
	static const fwMvFloatId ms_HotwiringPhaseId;
	static const fwMvFloatId ms_PutOnHelmetPhaseId;
	static const fwMvFlagId ms_UseStandardInVehicleAnimsFlagId;
	static const fwMvFlagId ms_UseBikeInVehicleAnimsFlagId;
	static const fwMvFlagId ms_UseBasicAnimsFlagId;
	static const fwMvFlagId ms_UseJetSkiInVehicleAnimsId;
	static const fwMvFlagId ms_UseBoatInVehicleAnimsId;
	static const fwMvFlagId ms_LowInAirFlagId;
	static const fwMvFlagId ms_LowImpactFlagId;
	static const fwMvFlagId ms_MedImpactFlagId;
	static const fwMvFlagId ms_HighImpactFlagId;
	static const fwMvFlagId ms_UseLeanSteerAnimsFlagId;
	static const fwMvFlagId ms_DontUsePassengerLeanAnimsFlagId;
	static const fwMvFlagId ms_UseIdleSteerAnimsFlagId;
	static const fwMvFlagId ms_FirstPersonCameraFlagId;
	static const fwMvFlagId ms_FirstPersonModeFlagId;
	static const fwMvFlagId ms_UsePOVAnimsFlagId;
	static const fwMvFlagId ms_SkipDuckIntroOutrosFlagId;
	static const fwMvRequestId ms_ChangeStationRequestId;
	static const fwMvBooleanId ms_ChangeStationOnEnterId;
	static const fwMvBooleanId ms_ChangeStationClipFinishedId;
	static const fwMvBooleanId ms_ChangeStationShouldLoopId;
	static const fwMvRequestId ms_HotwiringRequestId;
	static const fwMvBooleanId ms_HotwiringOnEnterId;
	static const fwMvBooleanId ms_HotwiringClipFinishedId;
	static const fwMvBooleanId ms_HotwiringShouldLoopId;
	static const fwMvBooleanId ms_ShuntAnimFinishedId;
	static const fwMvBooleanId ms_HeavyBrakeClipFinishedId;
	static const fwMvBooleanId ms_StartEngineId;
	static const fwMvRequestId ms_StillToSitRequestId;
	static const fwMvRequestId ms_SitToStillRequestId;
	static const fwMvBooleanId ms_StillToSitOnEnterId;
	static const fwMvBooleanId ms_SitToStillOnEnterId;
	static const fwMvBooleanId ms_PutOnHelmetOnEnterId;
	static const fwMvFloatId   ms_HotwireRate;
	static const fwMvFloatId   ms_ShuntPhaseId;
	static const fwMvFloatId   ms_StillToSitRate;
	static const fwMvFloatId   ms_SitToStillRate;
	static const fwMvFloatId   ms_StillToSitPhaseId;
	static const fwMvFloatId   ms_SitToStillPhaseId;
	static const fwMvFloatId   ms_HeavyBrakePhaseId;
	static const fwMvFloatId   ms_SitFirstPersonRateId;
	static const fwMvFloatId ms_InVehicleAnimsRateId;
	static const fwMvFloatId ms_InVehicleAnimsPhaseId;
	static const fwMvFloatId ms_InvalidPhaseId;
	static const fwMvFloatId ms_StartEngineBlendDurationId;
	static const fwMvFloatId ms_ShuntToIdleBlendDurationId;
	static const fwMvFloatId ms_IdleBlendDurationId;
	static const fwMvFloatId ms_BurnOutBlendId;
	static const fwMvFloatId ms_FirstPersonBlendDurationId;
	static const fwMvFloatId ms_RestartIdleBlendDurationId;
	static const fwMvBooleanId ms_PutOnHelmetClipFinishedId;
	static const fwMvBooleanId ms_PovHornExitClipFinishedId;
	static const fwMvRequestId ms_PutOnHelmetRequestId;
	static const fwMvBooleanId ms_PutOnHelmetCreateId;
	static const fwMvBooleanId ms_AttachHelmetRequestId;
	static const fwMvBooleanId ms_PutOnHelmetInterruptId;
	static const fwMvBooleanId ms_CanKickStartDSId;
	static const fwMvBooleanId ms_CanKickStartPSId;
	static const fwMvBooleanId ms_BurnOutInterruptId;
	static const fwMvClipId ms_PutOnHelmetClipId;
	static const fwMvClipId ms_ChangeStationClipId;
	static const fwMvClipId ms_StillToSitClipId;
	static const fwMvClipId ms_SitToStillClipId;
	static const fwMvClipId ms_BurnOutClipId;
	static const fwMvFlagId ms_Use2DBodyBlendFlagId;
	static const fwMvFlagId ms_IsDuckingFlagId;
	static const fwMvFlagId ms_PlayDuckOutroFlagId;
	static const fwMvClipSetVarId ms_DuckClipSetId;

	static const fwMvFlagId ms_SwitchVisor;
	static const fwMvClipId ms_SwitchVisorClipId;
	static const fwMvFloatId ms_SwitchVisorPhaseId;
	static const fwMvBooleanId ms_SwitchVisorAnimFinishedId;
	static const fwMvBooleanId ms_SwitchVisorAnimTag;
	static const fwMvClipSetId ms_SwitchVisorPOVClipSet;

	static const fwMvClipSetVarId ms_FirstPersonAdditiveClipSetId;
	static const fwMvClipSetVarId ms_FirstPersonSteeringClipSetId;
	static const fwMvClipSetVarId ms_FirstPersonRoadRageClipSetId;
	static const fwMvFlagId ms_PlayFirstPersonAdditive;
	static const fwMvBooleanId ms_FirstPersonAdditiveIdleFinishedId;
	static const fwMvFlagId ms_PlayFirstPersonShunt;
	static const fwMvFlagId ms_PlayFirstPersonRoadRage;
	static const fwMvBooleanId ms_FirstPersonShuntFinishedId;
	static const fwMvBooleanId ms_FirstPersonRoadRageFinishedId;
	static const fwMvFlagId ms_FPSHelmetPutOnUseUpperBodyOnly;

	////////////////////////////////////////////////////////////////////////////////

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, MOTION_IN_AUTOMOBILE_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls the characters clips on a bicycle
////////////////////////////////////////////////////////////////////////////////

class CTaskMotionOnBicycle : public CTaskMotionBase
{
public:

	static bool ms_bUseStartStopTransitions;
	static float ms_fMinTimeInStandState;
	static const fwMvBooleanId ms_CanTransitionToJumpPrepId;
	static const fwMvBooleanId ms_CanTransitionToJumpPrepRId;

	// PURPOSE: Check for a vehicle event
	static bool CheckForWantsToMove(const CVehicle& vehicle, const CPed& ped, bool bIgnoreVelocityCheck = false, bool bIsStill = false);
	static bool CheckForSitting(const CPed* pPed, bool bIsPedaling, const CMoveNetworkHelper& pedMoveNetworkHelper);
	static bool CheckForStill(const CVehicle& veh, const CPed& ped);
	static bool CheckForJumpPrepTransition(const CMoveNetworkHelper& rPedMoveNetworkHelper, const CVehicle& veh, const CPed& ped, bool bCachedJumpInput);
	static bool ShouldStandDueToPitch(const CPed& rPed);
	static float ComputeDesiredClipRateFromVehicleSpeed(const CVehicle& rVeh, const CBicycleInfo& rBicycleInfo, bool bCruising = true);
	static bool WantsToDoSpecialMove(const CPed& rPed, bool bIsFixie);
	static bool WantsToTrackStand(const CPed& rPed, const CVehicle& rVeh);
	static bool WantsToJump(const CPed& rPed, const CVehicle& rVeh, bool bIgnoreInAirCheck);
	static bool WantsToWheelie(const CPed& rPed, bool bHaveShiftedWeight);
	static bool CanTrackStand(const CMoveNetworkHelper& rPedMoveNetworkHelper);
	static bool CanJump(const CMoveNetworkHelper& rPedMoveNetworkHelper);
	static bool WasSprintingRecently(const CVehicle& rVeh);
	static float GetSprintResultFromPed(const CPed& rPed, const CVehicle& rVeh, bool bIgnoreVelocityTest = false);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(const CVehicle* pVehicle, s32 iSeatIndex);

	// PURPOSE: Stream any required assets, return true when they are streamed in
	static bool StreamTaskAssets(const CVehicle* pVehicle, s32 iSeatIndex);

	////////////////////////////////////////////////////////////////////////////////


	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_LeanAngleSmoothingRate;
		float m_StillToSitPedalGearApproachRate;
		float m_PedalGearApproachRate;
		float m_MinXYVelForWantsToMove;
		float m_MaxSpeedForStill;
		float m_MaxSpeedForStillReverse;
		float m_MaxThrottleForStill;
		float m_DefaultPedalToFreewheelBlendDuration;
		float m_SlowPedalToFreewheelBlendDuration;
		float m_MaxRateForSlowBlendDuration;
		float m_StillToSitLeanRate;
		float m_StillToSitApproachRate;
		float m_UpHillMinPitchToStandUp;
		float m_DownHillMinPitchToStandUp;
		float m_MinTimeInStandState;
		float m_MinTimeInStandStateFPS;
		float m_MinSprintResultToStand;
		float m_MaxTimeSinceShiftedWeightForwardToAllowWheelie;
		float m_WheelieShiftThreshold;
		float m_MinPitchDefault;
		float m_MaxPitchDefault;
		float m_MinForwardsPitchSlope;
		float m_MaxForwardsPitchSlope;
		float m_OnSlopeThreshold;
		float m_MaxJumpHeightForSmallImpact;
		float m_LongitudinalBodyLeanApproachRateSlope;
		float m_LongitudinalBodyLeanApproachRate;
		float m_LongitudinalBodyLeanApproachRateSlow;
		float m_SideZoneThreshold;
		float m_ReturnZoneThreshold;
		float m_MaxYIntentionToUseSlowApproach;
		float m_MinTimeToStayUprightAfterImpact;
		float m_BicycleNetworkBlendOutDuration;
		float m_DefaultSitToStandBlendDuration;
		float m_WheelieSitToStandBlendDuration;
		float m_WheelieStickPullBackMinIntention;
		float m_TimeSinceNotWantingToTrackStandToAllowStillTransition;
		float m_MinTimeInSitToStillStateToReverse;
		bool m_PreventDirectTransitionToReverseFromSit;
		fwMvClipId m_DefaultSmallImpactCharClipId;
		fwMvClipId m_DefaultImpactCharClipId;
		fwMvClipId m_DefaultSmallImpactBikeClipId;
		fwMvClipId m_DefaultImpactBikeClipId;
		fwMvClipId m_DownHillSmallImpactCharClipId;
		fwMvClipId m_DownHillImpactCharClipId;
		fwMvClipId m_DownHillSmallImpactBikeClipId;
		fwMvClipId m_DownHillImpactBikeClipId;
		fwMvClipId m_SitToStillCharClipId;
		fwMvClipId m_SitToStillBikeClipId;
		fwMvClipId m_TrackStandToStillLeftCharClipId;
		fwMvClipId m_TrackStandToStillLeftBikeClipId;
		fwMvClipId m_TrackStandToStillRightCharClipId;
		fwMvClipId m_TrackStandToStillRightBikeClipId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define MOTION_ON_BICYCLE_STATES(X)		X(State_Start),						\
											X(State_StreamAssets),				\
											X(State_Still),						\
											X(State_PutOnHelmet),				\
											X(State_StillToSit),				\
											X(State_SitToStill),				\
											X(State_Sitting),					\
											X(State_Standing),					\
											X(State_Reversing),					\
											X(State_Impact),					\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eOnBicycleState
	{
		MOTION_ON_BICYCLE_STATES(DEFINE_TASK_ENUM)
	};

	CTaskMotionOnBicycle(CVehicle* pVehicle, CMoveNetworkHelper& moveNetworkHelper, const fwMvNetworkDefId& networkDefId);
	virtual ~CTaskMotionOnBicycle();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_ON_BICYCLE; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskMotionOnBicycle(m_pVehicle, m_pedMoveNetworkHelper, m_PedsNetworkDefId); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_pedMoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_pedMoveNetworkHelper; }

	inline f32	GetPedalRate() const { return m_fPedalRate; }
	void		SetPedalRate(float fPedalRate) { m_fPedalRate = fPedalRate; }
	inline f32	GetJumpMinHeight() const { return m_fMinJumpHeight; }
	void		SetJumpMinHeight(float fMinJumpHeight) { m_fMinJumpHeight = fMinJumpHeight; }
	inline f32	GetJumpMaxHeight() const { return m_fMaxJumpHeight; }
	void		SetJumpMaxHeight(float fMaxJumpHeight) { m_fMaxJumpHeight = fMaxJumpHeight; }
	inline bool GetWantedToDoSpecialMoveInStillTransition() const { return m_bWantedToDoSpecialMoveInStillTransition; }
	void		SetWantedToDoSpecialMoveInStillTransition(bool bWantedToDoSpecialMoveInStillTransition) { m_bWantedToDoSpecialMoveInStillTransition = bWantedToDoSpecialMoveInStillTransition; }
	inline bool GetCachedJumpInput() const { return m_bCachedJumpInput; }
	void		SetCachedJumpInput(bool bJump) { m_bCachedJumpInput = bJump; }
	inline f32	GetDownHillBlend() const { return m_fDownHillBlend; }
	void		SetDownHillBlend(float fDownHillBlend) { m_fDownHillBlend = fDownHillBlend; }
	inline f32	GetBodyLeanY() const { return m_fBodyLeanY; }

	bool IsPedPedalling() const;
	bool IsPedStandingAndPedalling() const;
	bool IsPedFreewheeling() const;
	float GetTimePedalling() const;
	float GetTimeFreewheeling() const;
	void SetWasInAir(bool bWasInAir) { m_bWasInAir = bWasInAir; } 
	bool UseUpperBodyDriveby() const;
	bool ShouldLimitCameraDueToJump() const;
	bool ShouldApplyForcedCamPitchOffset(float& fForcedPitchOffset) const;

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const  { return State_Start; };

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Task update method that is called once RequestProcessMoveSignals is called
	// RETURNS: Whether this method should continue to be called, false will stop it.
	virtual bool ProcessMoveSignals();

	// PURPOSE: Called after camera is updated
	virtual bool ProcessPostCamera();

	// PURPOSE: Task motion base methods
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state) { return (state == CPedMotionStates::MotionState_InVehicle) ? true : false;}
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const { return state == CPedMotionStates::MotionState_InVehicle; }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds) { speeds.Zero(); }
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual bool IsInVehicle() { return true; }
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep);
	virtual CTask * CreatePlayerControlTask();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate(CPed& rPed);
	FSM_Return StreamAssets_OnEnter();
	FSM_Return StreamAssets_OnUpdate(CPed& rPed);
	void       StreamAssets_OnProcessMoveSignals();
	FSM_Return Still_OnEnter(CPed& rPed);
	FSM_Return Still_OnUpdate(CPed& rPed, CVehicle& rVeh);
	FSM_Return Still_OnExit();
	void       Still_OnProcessMoveSignals();
	FSM_Return PutOnHelmet_OnEnter();
	FSM_Return PutOnHelmet_OnUpdate();
	FSM_Return PutOnHelmet_OnExit();
	void       PutOnHelmet_OnProcessMoveSignals();
	FSM_Return StillToSit_OnEnter();
	FSM_Return StillToSit_OnUpdate(CPed& rPed);
	void       StillToSit_OnProcessMoveSignals();
	FSM_Return SitToStill_OnEnter();
	FSM_Return SitToStill_OnUpdate(CPed& rPed);
	void       SitToStill_OnProcessMoveSignals();
	FSM_Return Sitting_OnEnter();
	FSM_Return Sitting_OnUpdate(CPed& rPed);
	void       Sitting_OnProcessMoveSignals();
	FSM_Return Standing_OnEnter();
	FSM_Return Standing_OnUpdate(CPed& rPed);
	void       Standing_OnProcessMoveSignals();
	FSM_Return Reversing_OnEnter();
	FSM_Return Reversing_OnUpdate(CPed& rPed);
	void       Reversing_OnProcessMoveSignals();
	FSM_Return Impact_OnEnter();
	FSM_Return Impact_OnUpdate();
	void	   Impact_OnExit();
	void       Impact_OnProcessMoveSignals();

	// PURPOSE: Internal helper functions
	void SetTaskState(eOnBicycleState eState);
	void SetMoveNetworkStates(const fwMvRequestId& rRequestId, const fwMvBooleanId& rOnEnterId, bool bIgnoreVehicle = false);
	bool AreMoveNetworkStatesValid(fwMvRequestId requestId);
	void SetBlendDurationForState(bool bSetOnBicycleNetwork);
	bool CanBlockCameraViewModeTransitions() const; 

	// PURPOSE: Transition checks
	bool IsTransitioningToState(eOnBicycleState eState) { if (CheckForStateTransition(eState)) { SetTaskState(eState); return true; } else return false; }
	bool CheckForStateTransition(eOnBicycleState eState);
	bool CheckForStillTransition() const;
	bool CheckForStillToSitTransition() const;
	bool CheckForSittingTransition() const;
	bool CheckForStandingTransition() const;
	bool CheckForSitToStillTransition() const;
	bool CheckForImpactTransition() const;
	bool CheckForReversingTransition() const;
	bool CheckForHelmet(const CPed& Ped) const;
	bool CheckForRestartingStateDueToCameraSwitch();

	// PURPOSE: Anim parameter computation
	void ProcessDriveBySpineAdditive(const CPed& rPed);
	void ProcessLateralBodyLean(const CPed& rPed, float fTimeStep);
	void ProcessLongitudinalBodyLean(const CPed& rPed, float fTimeStep);
	void ProcessPedalRate(const CPed& rPed, float fTimeStep);
	void ProcessAllowWheelie(CPed& rPed, float fTimeStep);
	void ProcessDownHillSlopeBlend(CPed& rPed, float fTimeStep);
	void ProcessTuckBlend(CPed& rPed, float fTimeStep);

	bool CanTransitionToStill() const;
	bool IsSubTaskInState(s32 iState) const;
	bool PedWantsToStand(const CPed& rPed, const CVehicle& rVeh, float& fSprintResult, bool bHaveShiftedWeight) const;
	bool PedWantsToStand(const CPed& rPed, const CVehicle& rVeh, bool bHaveShiftedWeight) const { float fUnused; return PedWantsToStand(rPed, rVeh, fUnused, bHaveShiftedWeight); }
	bool StartPedMoveNetwork(const CVehicleSeatAnimInfo* pSeatClipInfo);
	bool StartVehicleMoveNetwork();
	bool SetSeatClipSet(const CVehicleSeatAnimInfo* pSeatClipInfo);
	inline void SyncronizeFloatParameter(fwMvFloatId floatParamId) { m_bicycleMoveNetworkHelper.SetFloat(floatParamId, m_pedMoveNetworkHelper.GetFloat(floatParamId)); }
	bool SetClipsForState(bool bForBicycle);
	bool ShouldUseFirstPersonAnims() const;
	bool ShouldChangeAnimsDueToCameraSwitch();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: The vehicle the ped is in
	RegdVeh m_pVehicle;

	// PURPOSE: Reference to the peds move network helper passed in from the parent in vehicle task
	CMoveNetworkHelper& m_pedMoveNetworkHelper;

	// PURPOSE: The bicycles move network helper so we can play pedal clips
	CMoveNetworkHelper m_bicycleMoveNetworkHelper;

	// PURPOSE: The peds network def id
	const fwMvNetworkDefId m_PedsNetworkDefId;
	const CVehicleSeatAnimInfo* m_pVehicleSeatAnimInfo;
	const CBicycleInfo*	m_pBicycleInfo;

	float	m_fPreviousBodyLeanX;
	float	m_fBodyLeanX;
	float	m_fBodyLeanY;
	float	m_fPitchAngle;
	float	m_fPedalRate;
	float	m_fDownHillBlend;
	float	m_fTimeSinceShiftedWeightForward;
	float	m_fTimeSinceWheelieStarted;
	float	m_fTimeSinceBikeInAir;
	float	m_fTimeInDeadZone;
	float	m_fMaxJumpHeight;
	float	m_fMinJumpHeight;
	float	m_fLeanRate;
	float	m_fTimeStillAfterImpact;
	float	m_fIdleTimer;
	float	m_fTimeSinceWantingToTrackStand;
	float	m_fTuckBlend;
	bool	m_bHaveShiftedWeightFwd : 1;
	bool	m_bWheelieTimerUp : 1;
	bool	m_bStartedBicycleNetwork : 1;
	bool	m_bPastFirstSprintPress : 1;
	bool	m_bWantedToDoSpecialMoveInStillTransition : 1;
	bool	m_bWantedToStandInStillTransition : 1;
	bool	m_bShouldStandBecauseOnSlope : 1;
	bool	m_bWasInAir : 1;
	bool	m_bUseStillTransitions : 1;
	bool	m_bBicyclePitched : 1;
	bool	m_bWasFreewheeling : 1;
	bool	m_bHaveBeenInSideZone : 1;
	bool	m_bWasInSlowZone : 1;
	bool	m_bShouldUseSlowLongitudinalRate : 1;
	bool	m_bSentAdditiveRequest : 1;
	bool	m_bUseSitTransitionJumpPrepBlendDuration : 1;
	bool	m_bCachedJumpInput : 1;
	bool	m_bIsTucking : 1;
	bool	m_bUseTrackStandTransitionToStill : 1;
	bool	m_bUseLeftFootTransition : 1;
	bool	m_bCanBlendOutImpactClip : 1;
	bool	m_bWasUsingFirstPersonClipSet : 1;
	bool	m_bNeedToRestart : 1;
	bool	m_bRestartedThisFrame : 1;
	bool	m_bShouldSetCanWheelieFlag : 1;
	bool	m_bMoVE_PedNetworkStarted : 1;
	bool	m_bMoVE_VehicleNetworkStarted : 1;
	bool	m_bMoVE_SeatClipHasBeenSet : 1;
	bool	m_bMoVE_AreMoveNetworkStatesValid : 1;
	bool	m_bMoVE_PutOnHelmetInterrupt : 1;
	bool	m_bMoVE_PutOnHelmetClipFinished : 1;
	bool	m_bMoVE_StillToSitTransitionFinished : 1;
	bool	m_bMoVE_SitToStillTransitionFinished : 1;
	bool	m_bShouldSkipPutOnHelmetAnim : 1;
	bool	m_bWasLaunchJump : 1;
	bool	m_bSwitchClipsThisFrame : 1;
	u32		m_uTimeBicycleLastSentRequest;
	u32		m_uTimeLastReversing;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvRequestId ms_StillRequestId;
	static const fwMvRequestId ms_StillToSitRequestId;
	static const fwMvRequestId ms_SittingRequestId;
	static const fwMvRequestId ms_StandingRequestId;
	static const fwMvRequestId ms_SitToStillRequestId;
	static const fwMvRequestId ms_ImpactRequestId;
	static const fwMvRequestId ms_IdleRequestId;

	static const fwMvBooleanId ms_StillOnEnterId;
	static const fwMvBooleanId ms_StillToSitOnEnterId;
	static const fwMvBooleanId ms_SittingOnEnterId;
	static const fwMvBooleanId ms_StandingOnEnterId;
	static const fwMvBooleanId ms_SitToStillOnEnterId;
	static const fwMvBooleanId ms_ImpactOnEnterId;
	static const fwMvBooleanId ms_StillToSitLeanBlendInId;
	
	static const fwMvFloatId ms_StillPhaseId;
	static const fwMvFloatId ms_StillToSitPhase;
	static const fwMvFloatId ms_SitToStillPhase;
	static const fwMvFloatId ms_ReversePhaseId;
	static const fwMvFloatId ms_ImpactPhaseId;
	static const fwMvFloatId ms_InAirPhaseId;

	static const fwMvFloatId ms_StillToSitRateId;
	static const fwMvFloatId ms_CruisePedalRateId;
	static const fwMvFloatId ms_FastPedalRateId;

	// Blending parameters
	static const fwMvFloatId ms_BodyLeanXId;
	static const fwMvFloatId ms_BodyLeanYId;
	static const fwMvFloatId ms_TuckBlendId;
	static const fwMvFlagId  ms_UseSmallImpactFlagId;
	static const fwMvFlagId ms_UseTuckFlagId;
	static const fwMvFlagId ms_DoingWheelieFlagId;
	static const fwMvFloatId ms_DownHillBlendId;

	static const fwMvFloatId ms_SitTransitionBlendDuration;
	static const fwMvFloatId ms_SitToStandBlendDurationId;

	static const fwMvFloatId ms_PedalToFreewheelBlendDuration;
	static const fwMvBooleanId ms_UseDefaultPedalRateId;
	static const fwMvBooleanId ms_CanBlendOutImpactClipId;
	static const fwMvBooleanId ms_FreeWheelBreakoutId;
	static const fwMvBooleanId ms_StillToSitFreeWheelBreakoutId;

	static const fwMvFloatId ms_TimeSinceExitStandStateId;
	static const fwMvBooleanId ms_StillToSitStillInterruptId;
	static const fwMvBooleanId ms_StillToSitTransitionFinished;
	static const fwMvBooleanId ms_SitToStillTransitionFinished;
	
	static const fwMvClipId ms_ImpactClipId;
	static const fwMvClipId ms_ImpactClip1Id;
	static const fwMvClipId ms_StillToSitClipId;
	static const fwMvClipId ms_ToStillTransitionClipId;
	static const fwMvFlagId	ms_LeanFlagId;

	static const fwMvBooleanId ms_BikePutOnHelmetInterruptId;
	static const fwMvBooleanId ms_BikePutOnHelmetClipFinishedId;
	static const fwMvRequestId ms_BikePutOnHelmetRequestId;
	static const fwMvBooleanId ms_BikePutOnHelmetOnEnterId;
	static const fwMvBooleanId ms_BikePutOnHelmetCreateId;
	static const fwMvBooleanId ms_BikePutOnHelmetAttachId;

	////////////////////////////////////////////////////////////////////////////////

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

#if __BANK

	static dev_float SCREEN_START_X_POS;
	static dev_float SCREEN_START_Y_POS;
	static dev_float TIME_LINE_WIDTH;
	static dev_float ELEMENT_HEIGHT;

	struct sClipDebugInfo
	{
		fwMvClipId clipId;
		fwMvFloatId phaseId;
		fwMvFloatId rateId;
	};

	void VisualizeClip(const crClip& rClip, float fPhase, float fRate, s32& iNumElements) const;
	void VisualizeFreewheelBreakOutTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos) const;
	void VisualizeFreewheelReversePedalTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos) const;
	void VisualizePrepLaunchTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos, bool bLeftTags) const;
	void VisualizeTrackStandTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos, bool bLeftTags) const;
#endif // __BANK

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, MOTION_ON_BICYCLE_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

class CTaskMotionOnBicycleController : public CTaskMotionBase
{
public:

	static const fwMvRequestId ms_FreeWheelRequestId;
	static const fwMvRequestId ms_PedalingRequestId;
	static const fwMvRequestId ms_PreparingToLaunchRequestId;
	static const fwMvRequestId ms_LaunchingRequestId;
	static const fwMvRequestId ms_InAirRequestId;
	static const fwMvRequestId ms_TrackStandRequestId;
	static const fwMvRequestId ms_FixieSkidRequestId;
	static const fwMvRequestId ms_ToTrackStandTransitionRequestId;

	static const fwMvBooleanId ms_FreeWheelOnEnterId;
	static const fwMvBooleanId ms_PedalingOnEnterId;
	static const fwMvBooleanId ms_PreparingToLaunchOnEnterId;
	static const fwMvBooleanId ms_LaunchingOnEnterId;
	static const fwMvBooleanId ms_InAirOnEnterId;
	static const fwMvBooleanId ms_TrackStandOnEnterId;
	static const fwMvBooleanId ms_FixieSkidOnEnterId;
	static const fwMvBooleanId ms_ToTrackStandTransitionOnEnterId;

	static const fwMvBooleanId ms_FreeWheelSitOnEnterId;
	static const fwMvBooleanId ms_PedalingSitOnEnterId;
	static const fwMvBooleanId ms_PreparingToLaunchSitOnEnterId;
	static const fwMvBooleanId ms_LaunchingSitOnEnterId;
	static const fwMvBooleanId ms_InAirOnSitEnterId;
	static const fwMvBooleanId ms_TrackStandSitOnEnterId;
	static const fwMvBooleanId ms_FixieSkidSitOnEnterId;

	static const fwMvBooleanId ms_PrepareToLaunchId;
	static const fwMvBooleanId ms_PrepareToLaunchRId;
	static const fwMvBooleanId ms_PrepareToTrackStandId;
	static const fwMvBooleanId ms_PrepareToTrackStandRId;
	static const fwMvBooleanId ms_ToTrackStandTransitionBlendOutId;
	
	static const fwMvBooleanId ms_FreeWheelBreakoutId;
	static const fwMvBooleanId ms_LaunchId;
	static const fwMvBooleanId ms_UseDefaultPedalRateId;
	static const fwMvBooleanId ms_CanReverseCruisePedalToGetToFreeWheel;

	static const fwMvFloatId ms_PedalRateId;
	static const fwMvFloatId ms_InvalidBlendDurationId;
	static const fwMvFloatId ms_PedalToFreewheelBlendDurationId;
	static const fwMvFloatId ms_FreewheelToPedalBlendDurationId;
	static const fwMvClipId  ms_CruisePedalClip;
	static const fwMvClipId	 ms_FastPedalClip;
	static const fwMvClipId	 ms_DuckPrepClipId;
	static const fwMvClipId	 ms_LaunchClipId;
	static const fwMvClipId	 ms_TrackStandClipId;
	static const fwMvClipId	 ms_FixieSkidClip0Id;
	static const fwMvClipId	 ms_FixieSkidClip1Id;
	static const fwMvClipId	 ms_ToTrackStandTransitionClipId;
	static const fwMvClipId	 ms_InAirFreeWheelClipId;
	static const fwMvClipId	 ms_InAirFreeWheelClip1Id;

	static const fwMvFloatId ms_CruisePedalPhase;
	static const fwMvFloatId ms_CruisePedalLeftPhase;
	static const fwMvFloatId ms_CruisePedalRightPhase;
	static const fwMvFloatId ms_CruiseFreeWheelPhaseId;
	static const fwMvFloatId ms_CruiseFreeWheelLeftPhaseId;
	static const fwMvFloatId ms_CruiseFreeWheelRightPhaseId;
	static const fwMvFloatId ms_CruisePrepLaunchPhaseId;
	static const fwMvFloatId ms_CruiseLaunchPhaseId;
	static const fwMvFloatId ms_TuckFreeWheelPhaseId;
	static const fwMvFloatId ms_ToTrackStandTransitionPhaseId;

	static const fwMvFloatId ms_FastPedalPhase;
	static const fwMvFloatId ms_FastPedalPhaseLeft;
	static const fwMvFloatId ms_FastPedalPhaseRight;
	static const fwMvFloatId ms_FastFreeWheelPhase;
	static const fwMvFloatId ms_FastFreeWheelPhaseLeft;
	static const fwMvFloatId ms_FastFreeWheelPhaseRight;
	static const fwMvFloatId ms_FastPrepLaunchPhaseId;
	static const fwMvFloatId ms_FastLaunchPhaseId;
	static const fwMvFloatId ms_FixieSkidClip0PhaseId;
	static const fwMvFloatId ms_FixieSkidClip1PhaseId;
	static const fwMvFloatId ms_FixieSkidBlendId;
	static const fwMvFloatId ms_InitialPedalPhaseId;
	static const fwMvFloatId ms_FixieSkidToTrackStandPhaseId;

	static const fwMvFlagId ms_HasTuckId;
	static const fwMvFlagId ms_HasHandGripId;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_TimeStillToTransitionToTrackStand;
		float m_MinTimeInPedalState;
		float m_MinTimeInFreewheelState;
		float m_MinAiSpeedForStandingUp;
		float m_MaxSpeedToTriggerTrackStandTransition;
		float m_MaxSpeedToTriggerFixieSkidTransition;
		float m_MinTimeInStateToAllowTransitionFromFixieSkid;
		fwMvClipId m_CruiseDuckPrepLeftCharClipId;
		fwMvClipId m_CruiseDuckPrepRightCharClipId;
		fwMvClipId m_CruiseDuckPrepLeftBikeClipId;
		fwMvClipId m_CruiseDuckPrepRightBikeClipId;
		fwMvClipId m_FastDuckPrepLeftCharClipId;
		fwMvClipId m_FastDuckPrepRightCharClipId;
		fwMvClipId m_FastDuckPrepLeftBikeClipId;
		fwMvClipId m_FastDuckPrepRightBikeClipId;
		fwMvClipId m_LaunchLeftCharClipId;
		fwMvClipId m_LaunchRightCharClipId;
		fwMvClipId m_LaunchLeftBikeClipId;
		fwMvClipId m_LaunchRightBikeClipId;
		fwMvClipId m_TrackStandLeftCharClipId;
		fwMvClipId m_TrackStandRightCharClipId;
		fwMvClipId m_TrackStandLeftBikeClipId;
		fwMvClipId m_TrackStandRightBikeClipId;
		fwMvClipId m_FixieSkidLeftCharClip0Id;
		fwMvClipId m_FixieSkidLeftCharClip1Id;
		fwMvClipId m_FixieSkidRightCharClip0Id;
		fwMvClipId m_FixieSkidRightCharClip1Id;
		fwMvClipId m_FixieSkidLeftBikeClip0Id;
		fwMvClipId m_FixieSkidLeftBikeClip1Id;
		fwMvClipId m_FixieSkidRightBikeClip0Id;
		fwMvClipId m_FixieSkidRightBikeClip1Id;
		fwMvClipId m_FixieSkidToBalanceLeftCharClip1Id;
		fwMvClipId m_FixieSkidToBalanceRightCharClip1Id;
		fwMvClipId m_FixieSkidToBalanceLeftBikeClip1Id;
		fwMvClipId m_FixieSkidToBalanceRightBikeClip1Id;
		fwMvClipId m_CruisePedalCharClipId;
		fwMvClipId m_InAirFreeWheelCharClipId;
		fwMvClipId m_InAirFreeWheelBikeClipId;
		fwMvClipId m_DownHillInAirFreeWheelCharClipId;
		fwMvClipId m_DownHillInAirFreeWheelBikeClipId;
		fwMvClipId m_TuckFreeWheelToTrackStandRightCharClipId;
		fwMvClipId m_TuckFreeWheelToTrackStandRightBikeClipId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define MOTION_ON_BICYCLE_CONTROLLER_STATES(X)		X(State_Start),						\
														X(State_Pedaling),					\
														X(State_PedalToFreewheeling),		\
														X(State_Freewheeling),				\
														X(State_PedalToPreparingToLaunch),	\
														X(State_PreparingToLaunch),			\
														X(State_Launching),					\
														X(State_PedalToInAir),				\
														X(State_InAir),						\
														X(State_PedalToTrackStand),			\
														X(State_FixieSkid),					\
														X(State_ToTrackStandTransition),	\
														X(State_TrackStand),				\
														X(State_Finish)

	CTaskMotionOnBicycleController(CMoveNetworkHelper& rPedMoveNetworkHelper, CMoveNetworkHelper& rBicycleMoveNetworkHelper, CVehicle* pBicycle, bool bWasFreewheeling);
	virtual ~CTaskMotionOnBicycleController();
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_MOTION_ON_BICYCLE_CONTROLLER; }
	virtual aiTask* Copy() const { return rage_new CTaskMotionOnBicycleController(m_rPedMoveNetworkHelper, m_rBicycleMoveNetworkHelper, m_pBicycle, m_bWasFreewheeling); }

	bool GetShouldReversePedalRate() const { return m_bShouldReversePedalRate; }
	float GetFixieSkidBlend() const { return m_fFixieSkidBlend; }
	bool GetUseLeftFoot() const { return m_bUseLeftFoot; }
	bool CanRestartNetwork() const { return m_bMoVE_AreMoveNetworkStatesValid && (fwTimer::GetTimeInMilliseconds() - m_uTimeBicycleLastSentRequest) > 0; }
	bool ShouldLimitCameraDueToJump() const;

	// PURPOSE: FSM states
	enum eOnBicycleControllerState
	{
		MOTION_ON_BICYCLE_CONTROLLER_STATES(DEFINE_TASK_ENUM)
	};

protected:

	////////////////////////////////////////////////////////////////////////////////

	virtual s32	GetDefaultStateAfterAbort() const  { return State_Start; };
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();
	virtual bool       ProcessMoveSignals();

	// PURPOSE: Task motion base methods
	virtual bool SupportsMotionState(CPedMotionStates::eMotionState state) { return (state == CPedMotionStates::MotionState_InVehicle) ? true : false;}
	virtual bool IsInMotionState(CPedMotionStates::eMotionState state) const { return state == CPedMotionStates::MotionState_InVehicle; }
	virtual void GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds) { speeds.Zero(); }
	virtual	bool IsInMotion(const CPed* UNUSED_PARAM(pPed)) const { return false; }
	virtual void GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip);
	virtual bool IsInVehicle() { return true; }
	virtual Vec3V_Out CalcDesiredVelocity(Mat34V_ConstRef UNUSED_PARAM(updatedPedMatrix), float UNUSED_PARAM(fTimestep)) { return VECTOR3_TO_VEC3V(GetPed()->GetVelocity()); }
	virtual CTask * CreatePlayerControlTask();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return Pedaling_OnEnter();
	FSM_Return Pedaling_OnUpdate();
	void       Pedaling_OnProcessMoveSignals();
	FSM_Return PedalToFreewheeling_OnEnter();
	FSM_Return PedalToFreewheeling_OnUpdate();
	FSM_Return Freewheeling_OnEnter();
	FSM_Return Freewheeling_OnUpdate();
	FSM_Return Freewheeling_OnExit();
	void       Freewheeling_OnProcessMoveSignals();
	FSM_Return PedalToPreparingToLaunch_OnEnter();
	FSM_Return PedalToPreparingToLaunch_OnUpdate();
	FSM_Return PedalToTrackStand_OnEnter();
	FSM_Return PedalToTrackStand_OnUpdate();
	FSM_Return FixieSkid_OnEnter();
	FSM_Return FixieSkid_OnUpdate();
	void       FixieSkid_OnProcessMoveSignals();
	FSM_Return ToTrackStandTransition_OnEnter();
	FSM_Return ToTrackStandTransition_OnUpdate();
	FSM_Return TrackStand_OnEnter();
	FSM_Return TrackStand_OnUpdate();
	void       TrackStand_OnProcessMoveSignals();
	FSM_Return PreparingToLaunch_OnEnter();
	FSM_Return PreparingToLaunch_OnUpdate();
	FSM_Return PreparingToLaunch_OnExit();
	void       PreparingToLaunch_OnProcessMoveSignals();
	FSM_Return Launching_OnEnter();
	FSM_Return Launching_OnUpdate();
	FSM_Return PedalToInAir_OnEnter();
	FSM_Return PedalToInAir_OnUpdate();
	FSM_Return InAir_OnEnter();
	FSM_Return InAir_OnUpdate(const CPed& rPed);
	FSM_Return InAir_OnExit();
	void       InAir_OnProcessMoveSignals();

	// PURPOSE: Internal helper functions
	void SetTaskState(eOnBicycleControllerState eState);
	void SetMoveNetworkStates(const fwMvRequestId& rRequestId, const fwMvBooleanId& rOnEnterId);
	bool AreMoveNetworkStatesValid(fwMvRequestId requestId, fwMvFloatId blendDurationId = ms_InvalidBlendDurationId, float fBlendDuration = NORMAL_BLEND_DURATION);

	// PURPOSE: Transition checks
	bool IsTransitioningToState(eOnBicycleControllerState eState) { if (CheckForStateTransition(eState)) { SetTaskState(eState); return true; } else return false; }
	bool CheckForStateTransition(eOnBicycleControllerState eState);
	bool CheckForPedalingTransition();
	bool CheckForFreewheelingTransition() const;
	bool CheckForPedalToFreewheelingTransition() const;
	bool CheckForPreparingToLaunchTransition() const;
	bool CheckForPedalToPreparingToLaunchTransition() const;
	bool CheckForPedalToTrackStandTransition() const;
	bool CheckForLaunchingTransition() const;
	bool CheckForPedalToInAirTransition() const;
	bool CheckForInAirTransition() const;
	bool CheckForTrackStandTransition() const;
	bool CheckForFixieSkidTransition() const;
	bool CheckForNonFixiePedalTransition() const;
	bool CheckForToTrackStandTransition() const;
	bool WantsToFreeWheel(bool bIgnoreVelocityTest = false, bool bIgnoreAtFreeWheelCheck = false) const;

	void ProcessFixieSkidBlend(const CVehicle& rVeh, float fTimeStep);
	enum eSpecialMoveType
	{
		ST_Jump,
		ST_TrackStand
	};
	void ComputeSpecialMoveParams(eSpecialMoveType eMoveType);
	float FindTargetBlendPhaseForCriticalTag(const crClip& rClip, fwMvBooleanId criticalMoveTagId, float fCurrentPhase);
	float GetPedalRate() const { return static_cast<const CTaskMotionOnBicycle*>(GetParent())->GetPedalRate(); }
	void SetPedalRate(float fPedalRate) { static_cast<CTaskMotionOnBicycle*>(GetParent())->SetPedalRate(fPedalRate); }
	float GetJumpMinHeight() const { return static_cast<const CTaskMotionOnBicycle*>(GetParent())->GetJumpMinHeight(); }
	void SetJumpMinHeight(float fJumpMinHeight) { static_cast<CTaskMotionOnBicycle*>(GetParent())->SetJumpMinHeight(fJumpMinHeight); }
	float GetJumpMaxHeight() const { return static_cast<const CTaskMotionOnBicycle*>(GetParent())->GetJumpMaxHeight(); }
	void SetJumpMaxHeight(float fJumpMaxHeight) { static_cast<CTaskMotionOnBicycle*>(GetParent())->SetJumpMaxHeight(fJumpMaxHeight); }
	inline void SyncronizeFloatParameter(fwMvFloatId floatParamId) { m_rBicycleMoveNetworkHelper.SetFloat(floatParamId, m_rPedMoveNetworkHelper.GetFloat(floatParamId)); }
	bool SetClipsForState(bool bForBicycle);
	void SetInitialPedalPhase();

	////////////////////////////////////////////////////////////////////////////////

private:

	CMoveNetworkHelper& m_rPedMoveNetworkHelper;
	CMoveNetworkHelper& m_rBicycleMoveNetworkHelper;
	RegdVeh				m_pBicycle;
	const CBicycleInfo*	m_pBicycleInfo;
	float				m_fOldPedalRate;
	float				m_fTimeSinceReachingLaunchPoint;
	float				m_fCachedJumpInputValidityTime;
	float				m_fFixieSkidBlend;
	float				m_fTimeBikeStill;
	float				m_fTargetPedalPhase;
	bool				m_bIsFixieBicycle : 1;
	bool				m_bShouldReversePedalRate : 1;
	bool				m_bIsCruising : 1;
	bool				m_bCheckForSprintTestPassed : 1;
	bool				m_bWasFreewheeling : 1;
	bool				m_bUseLeftFoot : 1;
	u32					m_uTimeBicycleLastSentRequest;

	bool				m_bMoVE_AreMoveNetworkStatesValid:1;
	bool				m_bMoVE_HaveReachedLaunchPoint:1;

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	//void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, MOTION_ON_BICYCLE_CONTROLLER_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_TASK_IN_VEHICLE_H
