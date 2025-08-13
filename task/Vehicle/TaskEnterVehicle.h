//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_ENTER_VEHICLE_H
#define INC_TASK_ENTER_VEHICLE_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "modelinfo/VehicleModelInfo.h"
#include "peds/QueriableInterface.h"
#include "peds/ped.h"
#include "Task/Physics/NmDefines.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CVehicle;
class CPed;
class CComponentReservation;
class CEntryExitPoint;

// Some macros to reduce duplication in logging
#undef TASK_FINISH_REASONS
#define TASK_FINISH_REASONS(X)					X(TFR_PED_INTERRUPTED),\
												X(TFR_PED_UNRESERVED_DOOR),\
												X(TFR_PED_CANT_OPEN_LOCKS),\
												X(TFR_PED_ALIGN_FAILED),\
												X(TFR_NOT_ALLOWED_TO_WARP),\
												X(TFR_OTHER_PED_USING_DOOR),\
												X(TFR_OTHER_PED_USING_SEAT),\
												X(TFR_JACKED_PED_NOT_RUNNING_EXIT_TASK),\
												X(TFR_VEHICLE_IS_LOCKED_TO_PASSENGERS),\
												X(TFR_VEHICLE_LOCKED_FOR_PLAYER),\
												X(TFR_VEHICLE_FLOATING),\
												X(TFR_VEHICLE_UNDRIVEABLE),\
												X(TFR_VEHICLE_NOT_UPRIGHT),\
												X(TFR_VEHICLE_SEAT_BLOCKED),\
												X(TFR_VEHICLE_MOVING_TOO_FAST),\
												X(TFR_GOTO_DOOR_TASK_FAILED),\
												X(TFR_NEEDS_TO_CLIMB_BUT_CANT),\
												X(TFR_SHOULD_FINISH_AFTER_CLIMBUP),\
												X(TFR_FAILED_TO_REORIENTATE_VEHICLE),

#undef TARGET_ENTRY_POINT_CHANGE_REASONS
#define TARGET_ENTRY_POINT_CHANGE_REASONS(X)	X(EPR_INITIAL_ENTRYPOINT_CHOICE),\
												X(EPR_REEVALUATED_WHEN_GOING_TO_DOOR),\
												X(EPR_SETTING_PED_IN_SEAT),\
												X(EPR_CLONE_READ_QUAERIABLE_STATE)

#undef TARGET_SEAT_CHANGE_REASONS
#define TARGET_SEAT_CHANGE_REASONS(X)			X(TSR_NOT_TURRET_SEAT_WANTS_TO_JACK_DRIVER),\
												X(TSR_FORCED_DRIVER_SEAT_USAGE_ACTIVE),\
												X(TSR_FORCED_DRIVER_SEAT_USAGE_INACTIVE),\
												X(TSR_FORCED_TURRET_SEAT_USAGE_ACTIVE),\
												X(TSR_FORCED_TURRET_SEAT_USAGE_INACTIVE),\
												X(TSR_WARP_ONTO_SEAT),\
												X(TSR_DIRECT_SEAT_FOR_TARGET_ENTRY_POINT),\
												X(TSR_ENTERING_DRIVERS_SEAT),\
												X(TSR_WARP_INTO_MP_WARP_SEAT),\
												X(TSR_WARP_INTO_MP_WARP_SEAT_NO_NAV),\
												X(TSR_GO_TO_DOOR_REEVALUATION),\
												X(TSR_FORCE_PASSENGER_SEAT),\
												X(TSR_FORCE_USE_TURRET),\
												X(TSR_FORCED_USE_REAR_SEATS),\
												X(TSR_FORCED_USE_SEAT_INDEX)

#undef SEAT_REQUEST_TYPE_CHANGE_REASONS
#define SEAT_REQUEST_TYPE_CHANGE_REASONS(X)		X(SRR_OTHER_PEDS_ENTERING),\
												X(SRR_NO_OTHER_PEDS_ENTERING),\
												X(SRR_NO_PASSENGERS_ALLOWED),\
												X(SRR_WARPING_FROM_ON_SEAT),\
												X(SRR_FORCE_REEVALUATE_BECAUSE_CANT_DIRECTLY_ACCESS_DRIVER),\
												X(SRR_FORCE_GET_INTO_PASSENGER_SEAT),\
												X(SRR_OPTIONAL_TURRET_EVALUATION_HELD_DOWN),\
												X(SRR_OPTIONAL_TURRET_EVALUATION_RELEASED_DRIVER_AVAILABLE),\
												X(SRR_OPTIONAL_TURRET_EVALUATION_RELEASED_DRIVER_NOT_AVAILABLE),\
												X(SRR_OPTIONAL_JACK_EVALUATION),\
												X(SRR_TRY_ANOTHER_SEAT)

enum eTaskEnterVehicleChangeSeatRequestTypeReason
{
	SEAT_REQUEST_TYPE_CHANGE_REASONS(DEFINE_TASK_ENUM)
};

enum eTaskEnterVehicleChangeEntryPointReason
{
	TARGET_ENTRY_POINT_CHANGE_REASONS(DEFINE_TASK_ENUM)
};

enum eTaskEnterVehicleChangeSeatReason
{
	TARGET_SEAT_CHANGE_REASONS(DEFINE_TASK_ENUM)
};

enum eTaskEnterVehicleFinishReason
{
	TASK_FINISH_REASONS(DEFINE_TASK_ENUM)
};

enum eTaskEnterVehicleDebugPrintFlags
{
	VEHICLE_DEBUG_PRINT_FRAME = BIT0,
	VEHICLE_DEBUG_PRINT_THIS_PED = BIT1,
	VEHICLE_DEBUG_PRINT_TARGET_VEHICLE = BIT2,
	VEHICLE_DEBUG_PRINT_CURRENT_STATE = BIT3,
	VEHICLE_DEBUG_PRINT_PED_IN_DRIVER_SEAT = BIT4,
	VEHICLE_DEBUG_PRINT_PED_IN_DIRECT_SEAT = BIT5,
	VEHICLE_DEBUG_PRINT_TARGET_SEAT_AND_ENTRYPOINT = BIT6,
	VEHICLE_DEBUG_PRINT_JACKED_PED = BIT7,
	VEHICLE_DEBUG_PRINT_DOOR_RESERVATION = BIT8,
	VEHICLE_DEBUG_PRINT_SEAT_RESERVATION = BIT9, 
	VEHICLE_DEBUG_RENDER_RELATIVE_VELOCITY = BIT10,

	VEHICLE_DEBUG_DEFAULT_FLAGS = VEHICLE_DEBUG_PRINT_FRAME | VEHICLE_DEBUG_PRINT_THIS_PED | VEHICLE_DEBUG_PRINT_TARGET_VEHICLE | VEHICLE_DEBUG_PRINT_CURRENT_STATE | VEHICLE_DEBUG_PRINT_TARGET_SEAT_AND_ENTRYPOINT
};

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls the characters clips whilst getting in a vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskEnterVehicle : public CTaskVehicleFSM
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //	// NOTES:

		bool	m_UseCombatEntryForAiJack;
		bool	m_EnableJackRateOverride;
		bool	m_DisableDoorHandleArmIk;
		bool	m_DisableBikeHandleArmIk;
		bool	m_DisableSeatBoneArmIk;
		bool	m_DisableTagSyncIntoAlign;
		bool	m_DisableMoverFixups;
		bool	m_DisableBikePickPullUpOffsetScale;
		bool	m_EnableNewBikeEntry;
		bool	m_ForcedDoorHandleArmIk;
		bool	m_IgnoreRotationBlend;
		bool	m_EnableBikePickUpAlign;
		float	m_BikePickUpAlignBlendDuration;
		float	m_GetInRate;

		// ---	END_DEV_TO_REMOVE	--- //
	
		float m_MPEntryPickUpPullUpRate;
		float m_CombatEntryPickUpPullUpRate;
		float m_MinMagForBikeToBeOnSide;	// Magnitude of bikes side vector to consider the bike being on its side
		float m_DistanceToEvaluateDoors;	// Distance away from the vehicle before starting to evaluate the entry point
		float m_NetworkBlendDuration;		// Duration to blend the enter vehicle network in
		float m_NetworkBlendDurationOpenDoorCombat;	// Duration to blend the enter vehicle network in when door open in combat entry
		float m_DoorRatioToConsiderDoorOpenSteps;
		float m_DoorRatioToConsiderDoorOpen;
		float m_DoorRatioToConsiderDoorOpenCombat;
		float m_DoorRatioToConsiderDoorClosed;
		float m_DistToEntryToAllowForcedActionMode;
		float m_VaultDepth;
		float m_VaultHorizClearance;
		float m_VaultVertClearance;
		float m_LeftPickUpTargetLerpPhaseStart;
		float m_LeftPickUpTargetLerpPhaseEnd;
		float m_LeftPullUpTargetLerpPhaseStart;
		float m_LeftPullUpTargetLerpPhaseEnd;
		float m_RightPickUpTargetLerpPhaseStart;
		float m_RightPickUpTargetLerpPhaseEnd;
		float m_RightPullUpTargetLerpPhaseStart;
		float m_RightPullUpTargetLerpPhaseEnd;
		float m_LeftPickUpTargetLerpPhaseStartBicycle;
		float m_LeftPickUpTargetLerpPhaseEndBicycle;
		float m_LeftPullUpTargetLerpPhaseStartBicycle;
		float m_LeftPullUpTargetLerpPhaseEndBicycle;
		float m_RightPickUpTargetLerpPhaseStartBicycle;
		float m_RightPickUpTargetLerpPhaseEndBicycle;
		float m_RightPullUpTargetLerpPhaseStartBicycle;
		float m_RightPullUpTargetLerpPhaseEndBicycle;
		float m_MinSpeedToAbortOpenDoor;
		float m_MinSpeedToAbortOpenDoorCombat;
		float m_MinSpeedToAbortOpenDoorPlayer;
		float m_MinSpeedToRagdollOpenDoor;
		float m_MinSpeedToRagdollOpenDoorCombat;
		float m_MinSpeedToRagdollOpenDoorPlayer;
		float m_TimeBetweenPositionUpdates;
		float m_DefaultJackRate;
		float m_BikeEnterForce;
		float m_BicycleEnterForce;
		float m_FastEnterExitRate;
		float m_TargetRearDoorOpenRatio;
		float m_MaxOpenRatioForOpenDoorInitialOutside;
		float m_MaxOpenRatioForOpenDoorOutside;
		float m_MaxOscillationDisplacementOutside;
		float m_MaxOpenRatioForOpenDoorInitialInside;
		float m_MaxOpenRatioForOpenDoorInside;
		float m_MaxOscillationDisplacementInside;
		float m_BikeEnterLeanAngleOvershootAmt;
		float m_BikeEnterLeanAngleOvershootRate;
		float m_MaxDistanceToCheckEntryCollisionWhenIgnoring;
		float m_CombatEntryBlendDuration;
		float m_MaxDistanceToReactToJackForGoToDoor;
		float m_MaxTimeStreamClipSetInBeforeWarpSP;
		float m_MaxTimeStreamClipSetInBeforeWarpMP;
		float m_MaxTimeStreamClipSetInBeforeSkippingCloseDoor;
		float m_MaxTimeStreamShuffleClipSetInBeforeWarp;
		bool  m_UseSlowInOut;
		float m_ClimbAlignTolerance;
		float m_OpenDoorBlendDurationFromNormalAlign;
		float m_OpenDoorBlendDurationFromOnVehicleAlign;
		float m_OpenDoorToJackBlendDuration;
		float m_GroupMemberWaitMinTime;
		float m_GroupMemberSlowDownDistance;
		float m_GroupMemberWalkCloseDistance;
		float m_GroupMemberWaitDistance;
		float m_SecondsBeforeWarpToLeader;
		float m_TimeBetweenDoorChecks;
		float m_MinVelocityToConsiderMoving;
		u32 m_MinTimeStationaryToIgnorePlayerDriveTest;
		u32 m_DurationHeldDownEnterButtonToJackFriendly;
		fwMvClipId m_DefaultJackAlivePedFromOutsideClipId;
		fwMvClipId m_DefaultJackAlivePedFromOutsideAltClipId;
		fwMvClipId m_DefaultJackDeadPedFromOutsideClipId;
		fwMvClipId m_DefaultJackDeadPedFromOutsideAltClipId;
		fwMvClipId m_DefaultJackAlivePedFromWaterClipId;
		fwMvClipId m_DefaultJackDeadPedFromWaterClipId;
		fwMvClipId m_DefaultJackPedFromOnVehicleClipId;
		fwMvClipId m_DefaultJackDeadPedFromOnVehicleClipId;
		fwMvClipId m_DefaultJackPedOnVehicleIntoWaterClipId;
		fwMvClipId m_DefaultJackDeadPedOnVehicleIntoWaterClipId;
		fwMvClipId m_DefaultClimbUpClipId;
		fwMvClipId m_DefaultClimbUpNoDoorClipId;
		fwMvClipId m_CustomShuffleJackDeadClipId;
		fwMvClipId m_CustomShuffleJackClipId;
		fwMvClipId m_CustomShuffleJackDead_180ClipId;
		fwMvClipId m_CustomShuffleJack_180ClipId;
		fwMvClipId m_ShuffleClipId;
		fwMvClipId m_Shuffle_180ClipId;
		fwMvClipId m_Shuffle2ClipId;
		fwMvClipId m_Shuffle2_180ClipId;

		bool m_OnlyJackDeadPedsInTurret;
		bool m_ChooseTurretSeatWhenOnGround;
		float m_DuckShuffleBlendDuration;
		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Temp
	static const fwMvFlagId		ms_UseNewClipNamesId;
	static const fwMvFlagId		ms_FromWaterId;
	static const fwMvFlagId		ms_GetInFromStandOnId;
	static const fwMvFlagId 	ms_ClimbUpFromWaterId;
	static const fwMvFlagId 	ms_UseSeatClipSetId;
	static const fwMvRequestId	ms_AlignRequestId;
	static const fwMvFloatId	ms_ClimbUpRateId;
	static const fwMvFloatId	ms_JackRateId;
	static const fwMvFloatId	ms_GetInRateId;
	static const fwMvFloatId	ms_PickUpRateId;
	static const fwMvFloatId	ms_PullUpRateId;
	static const fwMvFloatId	ms_OpenDoorBlendInDurationId;
	static const fwMvFloatId	ms_OpenDoorToJackBlendInDurationId;
	static const fwMvBooleanId  ms_StartEngineInterruptId;
	static const fwMvBooleanId	ms_BlendOutEnterSeatAnimId;

	static s32 GetEventPriorityForJackedPed(const CPed& rJackedPed);

	static void ProcessHeliPreventGoingThroughGroundLogic(const CPed& rPed, const CVehicle& rVeh, Vector3& vNewPedPosition, float fLegIkBlendOutPhase, float fPhase);

	// PURPOSE: Start the vehicle move network
	static bool CanInterruptEnterSeat(CPed& rPed, const CVehicle& rVeh, const CMoveNetworkHelper& rMoveNetworkHelper, s32 iTargetEntryPointIndex, bool& bWantsToExit, const CTaskEnterVehicle& rEnterTask);

	static bool StartMoveNetwork(CPed& ped, const CVehicle& vehicle, CMoveNetworkHelper& moveNetworkHelper, s32 iEntryPointIndex, float fBlendInDuration, Vector3 * pvLocalSpaceEntryModifier, bool bDontRestartIfActive = false, fwMvClipSetId clipsetOverride = CLIP_SET_ID_INVALID);
	
	static bool CheckForStartEngineInterrupt(CPed& ped, const CVehicle& vehicle, bool bIgnoreDriverCheck = false);

	static bool CheckForCloseDoorInterrupt(CPed& ped, const CVehicle& vehicle, const CMoveNetworkHelper& moveNetworkHelper, const CEntryExitPoint* pEntryPoint, bool bWantsToExit, s32 iEntryPointIndex);

	static bool CheckForShuffleInterrupt(CPed& ped,const CMoveNetworkHelper& moveNetworkHelper, const CEntryExitPoint* pEntryPoint, const CTaskEnterVehicle& enterVehicletask);

	static bool PassesPlayerDriverAlignCondition(const CPed& rPed, const CVehicle& rVeh);

	static bool CanShuffleToTurretSeat(const CPed& rPed, const CVehicle& rVeh, s32& iTurretSeat, bool& BUsesAlternateShuffle);

	// PURPOSE: Checks the current clip to detect window smash events
	static void ProcessSmashWindow(CVehicle& rVeh, CPed& rPed, const s32 iEntryPointIndex, const crClip& rClip, const float fPhase, const CMoveNetworkHelper& rMoveNetworkHelper, bool& bSmashedWindow, bool bProcessIfNoTagsFound = true);
	// PURPOSE: Gets the hierarchy id for the window of the entrypoint
	static eHierarchyId GetWindowHierarchyToSmash(const CVehicle& rVeh, const s32 iEntryPointIndex);

	// PURPOSE: Bike arm IK
	enum eForceArmIk
	{
		ForceArmIk_None = 0,
		ForceArmIk_Left,
		ForceArmIk_Right,
		ForceArmIk_Both
	};

	static void ProcessBikeArmIk(const CVehicle& rVeh, CPed& rPed, s32 iEntryPointIndex, const crClip& rClip, float fPhase, bool bIsJacking = false, const CTaskEnterVehicle::eForceArmIk forceArmIk = ForceArmIk_None, bool bUseOrientation = false);

#if __ASSERT
	static void CheckForFixedPed(const CPed& ped, bool bIsWarping);
#endif // __ASSERT

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define ENTER_VEHICLE_STATES(X)	X(State_Start),						\
									X(State_ExitVehicle),				\
									X(State_GoToVehicle),				\
									X(State_PickDoor),					\
									X(State_GoToClimbUpPoint),			\
									X(State_GoToDoor),					\
									X(State_GoToSeat),					\
									X(State_ReserveSeat),				\
									X(State_ReserveDoorToOpen),			\
									X(State_StreamAnimsToAlign),		\
									X(State_StreamAnimsToOpenDoor),		\
									X(State_Align),						\
									X(State_ClimbUp),					\
									X(State_VaultUp),					\
									X(State_OpenDoor),					\
									X(State_AlignToEnterSeat),			\
									X(State_PickUpBike),				\
									X(State_PullUpBike),				\
									X(State_SetUprightBike),				\
									X(State_WaitForGroupLeaderToEnter),	\
									X(State_WaitForEntryPointReservation), \
									X(State_JackPedFromOutside),		\
									X(State_StreamAnimsToEnterSeat),	\
									X(State_EnterSeat),					\
									X(State_WaitForStreamedEntry),		\
									X(State_StreamedEntry),				\
									X(State_WaitForPedToLeaveSeat),		\
									X(State_UnReserveSeat),				\
									X(State_ReserveDoorToClose),		\
									X(State_CloseDoor),					\
									X(State_TryToGrabVehicleDoor),		\
									X(State_UnReserveDoorToShuffle),	\
									X(State_UnReserveDoorToFinish),		\
									X(State_SetPedInSeat),				\
									X(State_ShuffleToSeat),				\
									X(State_WaitForNetworkSync),		\
									X(State_WaitForValidEntryPoint),	\
									X(State_Ragdoll),					\
									X(State_Finish)

	// PURPOSE: FSM states
	enum eEnterVehicleState
	{
		ENTER_VEHICLE_STATES(DEFINE_TASK_ENUM)
	};

	CTaskEnterVehicle(CVehicle* pVehicle, SeatRequestType iRequest, s32 iTargetSeatIndex, VehicleEnterExitFlags iRunningFlags = VehicleEnterExitFlags(), float fTimerSeconds = 0.0f,
					  const float fMoveBlendRatio = MOVEBLENDRATIO_RUN, const CPed* pTargetPed = NULL, fwMvClipSetId overridenEntryClipSetId = CLIP_SET_ID_INVALID);
	
	virtual ~CTaskEnterVehicle();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_ENTER_VEHICLE; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskEnterVehicle(m_pVehicle, m_iSeatRequestType, m_iSeat, m_iRunningFlags, m_fTimerSeconds, m_fMoveBlendRatio, m_pTargetPed, m_OverridenEntryClipSetId); }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }

	// PURPOSE: Get the network blend in duration
	float GetNetworkBlendInDuration() const { return m_fBlendInDuration; }

	// PURPOSE: Clone task implementation
    virtual bool OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return GetState() == State_TryToGrabVehicleDoor || m_bLerpingCloneToDoor; }
	virtual bool OverridesCollision()		{ return GetPed()->GetAttachState() == ATTACH_STATE_PED_ENTER_CAR; }
	virtual bool ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType));
	virtual bool OverridesVehicleState() const	{ return true; }
    virtual bool AllowsLocalHitReactions() const { return false; }

	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void OnCloneTaskNoLongerRunningOnOwner();
    virtual bool IsInScope(const CPed* pPed);
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;
	virtual bool CanBeReplacedByCloneTask(u32 taskType) const;

	// PURPOSE: Does the bike need to be picked or pulled up
	bool CheckForBikeOnGround(s32& iState) const;
	bool ShouldBeInActionModeForCombatEntry(bool& bWithinRange);

	// PURPOSE: DAN TEMP - function for ensuring a valid clip transition is available based on the current move network state
	// this will be replaced when the move network interface is extended to include this
	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;

	void SetStreamedClipSetId(const fwMvClipSetId& streamedClipSetId) { m_StreamedClipSetId = streamedClipSetId; }
	void SetStreamedClipId(const fwMvClipId& streamedClipId) { m_StreamedClipId = streamedClipId; }

	void SetCachedShouldAlterEntryForOpenDoor(bool bShouldAlterEntryForOpenDoor) { m_ShouldAlterEntryForOpenDoor = bShouldAlterEntryForOpenDoor; }
	bool GetCachedShouldAlterEntryForOpenDoor() const { return m_ShouldAlterEntryForOpenDoor; }

	bool IsConsideredGettingInVehicleForCamera(float fTimeOffsetForInterrupt = 0.0f);
	bool IsConsideredGettingInVehicleForAmbientScale();

	bool CheckForClimbUp() const;
	bool ShouldClimbUpOnToVehicle(CPed* pSeatOccupier, const CVehicleEntryPointAnimInfo& rAnimInfo, const CVehicleEntryPointInfo& rEntryInfo);

	// PURPOSE: Checks to see if we should play a streamed entry anim
	bool CheckForStreamedEntry();
	bool ShouldUseCombatEntryAnimLogic(bool bCheckSubtask = true) const;

	s32 GetLastCompletedState() const { return m_iLastCompletedState; }

	Vector3 GetLocalSpaceEntryModifier() const { return m_vLocalSpaceEntryModifier; }
	void SetLocalSpaceEntryModifier(const Vector3& vLocalSpaceEntryModifier) { m_vLocalSpaceEntryModifier = vLocalSpaceEntryModifier; }

	void SetTargetPosition(const Vector3& vTarget) { m_targetPosition = vTarget; }
	Vector3 GetTargetPosition() const { return m_targetPosition; }

	// PURPOSE: Stores the offset to the target
	bool ShouldUseClimbUpTarget() const;
	bool ShouldUseIdealTurretSeatTarget() const;
	bool ShouldUseGetInTarget() const;
	bool ShouldUseEntryTarget() const;
	void StoreInitialOffsets();
	void ComputeClimbUpTarget(Vector3& vOffset, Quaternion& qOffset);
	void ComputeIdealTurretSeatTarget(Vector3& vOffset, Quaternion& qOffset);
	void ComputeGetInTarget(Vector3& vOffset, Quaternion& qOffset);
	void ComputeEntryTarget(Vector3& vOffset, Quaternion& qOffset);
	void ComputeSeatTarget(Vector3& vOffset, Quaternion& qOffset);

	float GetTimeBeforeWarp() const { return m_fTimerSeconds; }
	fwMvClipSetId GetEntryClipSetOverride() const { return m_OverridenEntryClipSetId; }

	// PURPOSE: Gets the ped in the direct access seat
	CPed* GetPedOccupyingDirectAccessSeat() const;
	CPed* GetJackedPed() const { return m_pJackedPed; }

	bool WarpIfPossible();
	bool CheckForRestartingAlign() const;
	// Used by clone ped to determine when to pop the door state if it's out of sync
	bool GetForcedDoorState() const { return m_bForcedDoorState; }
	bool GetVehicleDoorOpen() const { return m_bVehicleDoorOpen; }
	bool IsOnVehicleEntry() const;
	bool ShouldTryToPickAnotherEntry(const CPed& rPedReservingSeat);

	u32 GetTimeVehicleLastWasMoving() const { return m_uVehicleLastMovingTime; }
	s32 GetTimeStartedAlign() const { return m_iTimeStartedAlign; }

	fwMvClipSetId GetRequestedEntryClipSetIdForOpenDoor() const;
	fwMvClipSetId GetRequestedCommonClipSetId() const;

	void StoreInitialReferencePosition( CPed* pPed );

	const bool GetWantsToShuffle() const { return m_bWantsToShuffle; }

	static void SetUpRagdollOnCollision(CPed& rPed, CVehicle& rVeh);

	static void ResetRagdollOnCollision(CPed& rPed);

	// PURPOSE : Determine whether given entry point is potentially blocked by rear door
	static bool CheckIsEntryPointBlockedByRearDoor(CPhysical * pVehicle, s32 iEntryPoint);

	// PURPOSE: Calculates an optional local offset to be applied to entry position
	static Vector3 CalculateLocalSpaceEntryModifier(CPhysical * pVehicle, s32 iEntryPoint);

	static void PossiblyReportStolentVehicle(CPed& ped, CVehicle& vehicle, bool bRequireDriver);

	// PURPOSE: Returns flags which are passed in to CPed::AttachPedToEnterCar
	static u32 GetAttachToVehicleFlags(const CVehicle* pVehicle);

	static bool CNC_ShouldForceMichaelVehicleEntryAnims(const CPed& ped, bool bSkipCombatEntryFlagCheck = false);

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const;

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Clone task implementation
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

    // PURPOSE: This function returns whether changes from the specified state
	// are managed by the clone FSM update, if not the state must be
	// kept in sync with the network update
	bool StateChangeHandledByCloneFSM(s32 iState);

    //PURPOSE: Checks whether we can update the local task state based on the last state received over the network. Used when running as a clone task only.
    bool CanUpdateStateFromNetwork();

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after position is updated
	virtual bool ProcessPostMovement();

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return 	Start_OnUpdate();
    FSM_Return 	Start_OnUpdateClone();
	void	   	ExitVehicle_OnEnter();
	FSM_Return 	ExitVehicle_OnUpdate();
	FSM_Return 	ExitVehicle_OnUpdateClone();
	void		GoToVehicle_OnEnter();
	FSM_Return	GoToVehicle_OnUpdate();
	FSM_Return	PickDoor_OnEnter();
	FSM_Return	PickDoor_OnUpdate();
	void		GoToDoor_OnEnter();
	FSM_Return	GoToDoor_OnUpdate();
	FSM_Return	GoToSeat_OnEnter();
	FSM_Return	GoToSeat_OnUpdate();
	FSM_Return	GoToClimbUpPoint_OnEnter();
	FSM_Return	GoToClimbUpPoint_OnUpdate();
	FSM_Return	ReserveSeat_OnUpdate();
	FSM_Return	ReserveDoorToOpen_OnUpdate();
	void		StreamAnimsToOpenDoor_OnEnterClone();
	FSM_Return	StreamAnimsToOpenDoor_OnUpdate();
	void		StreamAnimsToOpenDoor_OnExitClone();
	FSM_Return	StreamAnimsToAlign_OnEnter();
	void		StreamAnimsToAlign_OnEnterClone();
	FSM_Return	StreamAnimsToAlign_OnUpdate();
	void		StreamAnimsToAlign_OnExitClone();
	void		Align_OnEnter();
    void		Align_OnEnterClone();
	FSM_Return	Align_OnUpdate();
	FSM_Return	Align_OnUpdateClone();
	void		Align_OnExitClone();
	FSM_Return	StreamedEntry_OnEnter();
	FSM_Return	StreamedEntry_OnUpdate();
	FSM_Return	WaitForStreamedEntry_OnUpdate();
	FSM_Return	ClimbUp_OnEnter();
	FSM_Return	ClimbUp_OnUpdate();
	FSM_Return  ClimbUp_OnExit();
	FSM_Return	VaultUp_OnEnter();
	FSM_Return	VaultUp_OnUpdate();
	FSM_Return 	OpenDoor_OnEnter(CPed& rPed);
	FSM_Return	OpenDoor_OnUpdate();
    FSM_Return	OpenDoor_OnUpdateClone();
	FSM_Return	AlignToEnterSeat_OnEnter();
	FSM_Return	AlignToEnterSeat_OnUpdate();
	void		PickUpBike_OnEnter();
	FSM_Return	PickUpBike_OnUpdate();
	void		PullUpBike_OnEnter();
	FSM_Return	PullUpBike_OnUpdate();
	FSM_Return	SetUprightBike_OnUpdate();
	FSM_Return	WaitForGroupLeaderToEnter_OnUpdate();
	FSM_Return	WaitForEntryPointReservation_OnUpdate();
	FSM_Return	JackPedFromOutside_OnEnter();
	FSM_Return	JackPedFromOutside_OnUpdate();
	FSM_Return	UnReserveDoorToShuffle_OnUpdate();
	FSM_Return	UnReserveDoorToFinish_OnUpdate();
	void		StreamAnimsToEnterSeat_OnEnterClone();
	FSM_Return	StreamAnimsToEnterSeat_OnUpdate();
	void		StreamAnimsToEnterSeat_OnExitClone();
	void		EnterSeat_OnEnter();
	FSM_Return	EnterSeat_OnUpdate();
	void		EnterSeat_OnExit();
	FSM_Return	UnReserveSeat_OnUpdate();
    void        WaitForPedToLeaveSeat_OnEnter();
    FSM_Return	WaitForPedToLeaveSeat_OnUpdate();
	FSM_Return	SetPedInSeat_OnEnter();
	FSM_Return	SetPedInSeat_OnUpdate();
	FSM_Return	ReserveDoorToClose_OnUpdate();
	void		CloseDoor_OnEnter();
	FSM_Return	CloseDoor_OnUpdate();
	void		ShuffleToSeat_OnEnter();
	FSM_Return	ShuffleToSeat_OnUpdate();
	FSM_Return	TryToGrabVehicleDoor_OnEnter();
	FSM_Return	TryToGrabVehicleDoor_OnUpdate();
    FSM_Return	WaitForNetworkSync_OnUpdate();
	FSM_Return	WaitForValidEntryPoint_OnUpdate();
	FSM_Return  Ragdoll_OnEnter();
	FSM_Return	Ragdoll_OnUpdate();
    FSM_Return	Finish_OnUpdateClone();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: If player is the leader of a group, track the time they entered the vehicle
	void PedEnteredVehicle();
	void CreateWeaponManagerIfRequired();

	// PURPOSE: Should the ped wait for the leader before getting in themselves
	bool PedShouldWaitForLeaderToEnter();
	bool PedShouldWaitForReservationToEnter();
	bool PedHasReservations();
	bool CanPedGetInVehicle(CPed* pPed);

	// PURPOSE: Ped tries to reserve the door so they can enter
	bool TryToReserveDoor(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex);
	bool TryToReserveSeat(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex, s32 iSeatIndex = -1);

	// PURPOSE: Gets the clip and phase for the clip currently playing depending on the state
	const crClip* GetClipAndPhaseForState(float& fPhase);

	// PURPOSE: Get the translation/rotational fixup phase from the current clip
	void GetTranslationBlendPhaseFromClip(const crClip* pClip, float& fStartPhase, float& fEndPhase);
	void GetOrientationBlendPhaseFromClip(const crClip* pClip, float& fStartPhase, float& fEndPhase);

	// PURPOSE: Get vehicle clip flags
	s32 GetVehicleClipFlags();

    //PURPOSE: Ensures that are locally controlled peds that are being jacked by a remote ped leave the vehicle, if certain
    //         task states are skipped due to lag issues the initial jacking request may have been missed
    void EnsureLocallyJackedPedsExitVehicle();

    //PURPOSE: Forces network synchronisation of a ped in the specified seat, peds in vehicles are not synced by default,
    //         but when another ped is entering their vehicle this is required.
    void ForceNetworkSynchronisationOfPedInSeat(s32 seatIndex);

	//PURPOSE: Called once we are in our direct seat, returns true if the indirect seat is our target seat
	bool CheckForShuffle();

	//PURPOSE: Update jacking ped status
	void ProcessJackingStatus();

	//PURPOSE: Called during alignment / open door to try to reserve the door and seat we'll be using
	bool ProcessComponentReservations(bool bRequiresOnlyDoor = false);

	// PURPOSE: Checks to see if we should open the vehicle door
	bool CheckForOpenDoor() const;

	// PURPOSE: Checks if the vehicle is locked and we shouldn't climb up
	bool CheckForClimbUpIfLocked() const;

	// PURPOSE: Checks to see if we can jack a ped from the outside
	bool CheckForJackingPedFromOutside(CPed*& pSeatOccupier);

	// PURPOSE: Check for entering seat
	bool CheckForEnteringSeat(CPed* pSeatOccupier, bool bIsWarping = false, bool bIgnoreSideCheck = false);

	// PURPOSE: Check for entering seat
	bool CheckForGoToSeat() const;

	// PURPOSE: Check for go to climb up point (if on top of the vehicle)
	bool CheckForGoToClimbUpPoint() const;

	// PURPOSE: Check for get in point being blocked on boats.
	bool CheckForBlockedOnVehicleGetIn() const;

	// PURPOSE: Warps the ped running the task to the entry position
	void WarpPedToEntryPositionAndOrientate();

	// PURPOSE: Is ped occupying set shuffling
	bool IsSeatOccupierShuffling(CPed* pSeatOccupier) const;

	// PURPOSE: Returns if this task state should alter the peds attachment position
	bool ShouldProcessAttachment() const;

	// PURPOSE: Computes the world space translation from the last phase to the current
	void ComputeGlobalTranslationDeltaFromClip(const crClip& clip, float fCurrentPhase, Vector3& vDelta, const Quaternion& qPedOrientation);

	// PURPOSE: Is the vehicle moving too fast and we should abort
	bool ShouldFinishAfterClimbUp(CPed& rPed);
	bool ShouldAbortBecauseVehicleMovingTooFast() const;
	bool ShouldTriggerRagdollDoorGrab() const;
	bool EntryClipSetLoadedAndSet();
	bool ShouldReEvaluateBecausePedEnteredOurSeatWeCantJack() const;

	// PURPOSE: Check if we made it to the climb up point
	bool IsAtClimbUpPoint() const;

	// PURPOSE: See if the vehicle we're trying to enter has a driver that can drive this ped as a passenger
	bool CheckForOptionalJackOrRideAsPassenger(const CPed& rPed, const CVehicle& rVeh) const;
	s32 CheckForOptionalForcedEntryToTurretSeatAndGetTurretSeatIndex(const CPed& rPed, const CVehicle& rVeh) const;
	s32 CheckForOptionalForcedEntryToRearSeatAndGetSeatIndex(const CPed& rPed, const CVehicle &rVeh) const;
	bool CheckShouldProcessEnterRearSeat(const CPed& rPed, const CVehicle& rVeh) const;

	bool CheckIfPlayerIsPickingUpMyBike() const;

	aiTask* CreateAlignTask();

	// PURPOSE: Set the clip playback rate
	void SetClipPlaybackRate();

	// PURPOSE: Drive the bike lean angle
	void PickOrPullUpBike(float fPhase, float fPhaseToBeginOpenDoor, float fPhaseToEndOpenDoor);

	void GivePedRewards();

	void WarpPedToGetInPointIfNecessary(CPed& rPed, const CVehicle& rVeh);

	s32 FindClosestSeatToPedWithinWarpRange(const CVehicle& rVeh, const CPed& rPed, bool& bCanWarpOnToSeat) const;

	void GetAlternateClipInfoForPed(const CPed& ped, fwMvClipSetId& clipSetId, fwMvClipId& clipId, bool& bForceFlee) const;

	// PURPOSE: Decide which clip to pass into the move network
	const crClip* SetClipsForState();

	// PURPOSE: Delay task until we determine if we are jacking a friendly ped or not.
	bool ProcessFriendlyJackDelay(CPed *pPed, CVehicle *pVehicle);
	bool ProcessOptionalForcedTurretDelay(CPed *pPed, CVehicle *pVehicle);
	bool ProcessOptionalForcedRearSeatDelay(CPed *pPed, CVehicle *pVehicle);

	void ProcessDoorSteps();

	bool StartEnterVehicleLookAt(CPed *pPed, CVehicle *pVehicle);
	bool CheckForBikeFallingOver(const CVehicle& rVeh);

	bool ShouldLeaveDoorOpenForGroupMembers(const CPed& rPed, s32 iSeatIndex, s32 iEntryPointIndex) const;
	bool CanUnreserveDoorEarly(const CPed& rPed, const CVehicle& rVeh);

	void InitExtraGetInFixupPoints();
	void UpdateExtraGetInFixupPoints(const crClip *pClip, float fPhase);

	// PURPOSE: Starts the move network for the arresting ped
	FSM_Return StartMoveNetworkForArrest();

	// PURPOSE: Returns whether climb up comes after open door (either by layout or specific entry flag)
	bool ShouldUseClimbUpAfterOpenDoor(const s32 entryPoint, const CVehicle* pVehicle) const;

	bool IsPedWarpingIntoVehicle(s32 iTargetEntryPoint) const;

#if __ASSERT
	void VerifyPairedJackClip(bool bOnVehicleJack, bool bFromWater, const CPed& rPed, fwMvClipSetId clipSetId, fwMvClipId jackerClipId, const crClip& rJackerClip) const;
#endif // __ASSERT

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

	void SetSeatRequestType(SeatRequestType seatRequestType, s32 changeReason, s32 iDebugFlags);
	void SetTargetEntryPoint(s32 iTargetEntryPoint, s32 changeReason, s32 iDebugFlags);
	void SetTargetSeat(s32 iTargetSeat, s32 changeReason, s32 iDebugFlags);
	void SetTaskFinishState(s32 iFinishReason, s32 iDebugFlags);
	
private:

	// Vectors first
	Vector3 m_vPreviousPedPosition;

	// PURPOSE : Local space modifier to entry position
	Vector3 m_vLocalSpaceEntryModifier;

	// PURPOSE: Helper to calculate a new attachment position
	CPlayAttachedClipHelper m_PlayAttachedClipHelper;

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Helps to stream vehicle clips in
	CVehicleClipRequestHelper m_VehicleClipRequestHelper;

	// PURPOSE: The current seat index of the ped
	s32	m_iCurrentSeatIndex;

	// PURPOSE: Task can warp ped after a certain time
	float m_fTimerSeconds;

	// PURPOSE: Time between door checks
	float m_fDoorCheckInterval;

	// PURPOSE: Pointer to ped being jacked by this ped
	RegdPed m_pJackedPed;

	// PURPOSE: Current clip blend
	float m_fClipBlend;

	// PURPOSE: Network blend in duration
	float m_fBlendInDuration;

	// PURPOSE: Timer to update the goto position
	CTaskSimpleTimer m_TaskTimer;

	// PURPOSE: State to wait for GetStateFromNetwork() to change from when waiting for a network update
    s32 m_iNetworkWaitState; 

    // PURPOSE:  DAN TEMP - variable for ensuring a valid clip transition is available based on the current move network state
    s16 m_iLastCompletedState;

    // PURPOSE: Time to wait for a remote ped to exit their seat, when the network game is lagging there can be delay between
    //          the local ped jacking a remote ped and the remote ped leaving the vehicle. In this case we must wait.
    s16 m_iWaitForPedTimer;

	// PURPOSE: Used to send a player into the electric task when being jacked by another player
	fwMvClipSetId m_ClipSetId;
	u32 m_uDamageTotalTime;
	eRagdollTriggerTypes m_RagdollType;

	float m_fOriginalLeanAngle;
	float m_fLastPhase;
	float m_fInterruptPhase;
	float m_fInitialHeading;
	float m_fLegIkBlendOutPhase;

	u32 m_iTimeStartedAlign;
	u32 m_uLostClimpUpPhysicalTime;
	u32 m_uInvalidEntryPointTime;
	u32 m_uVehicleLastMovingTime;

	// PURPOSE: The current get in point index.
	u32	m_iCurrentGetInIndex;
	float m_fExtraGetInFixupStartPhase;
	float m_fExtraGetInFixupEndPhase;

	fwMvClipSetId m_OverridenEntryClipSetId;
	fwMvClipSetId m_StreamedClipSetId;
	fwMvClipId m_StreamedClipId;
	const CConditionalClipSet* m_pStreamedEntryClipSet;

	// For turret
	Vector3		m_targetPosition;

	// The entity we want to try and aim at while moving to the vehicle we are going to enter
	RegdConstPed m_pTargetPed;

	// Flags
	bool m_bAnimFinished : 1;		// Used by ProcessMoveSignals() during certain states.
	bool m_bShouldTriggerElectricTask : 1;
	bool m_bWantsToExit : 1;
	bool m_bWillBlendOutHandleIk : 1;
	bool m_bWillBlendOutSeatIk : 1;
	bool m_bWantsToGoIntoCover : 1;
	bool m_bShouldBePutIntoSeat : 1;
	bool m_bWasJackedPedDeadInitially : 1;
	bool m_bAligningToOpenDoor : 1;
	bool m_bIsPickUpAlign : 1;
	bool m_bDoingExtraGetInFixup : 1;
	bool m_bFoundExtraFixupPoints : 1;
	bool m_bCouldEnterVehicleWhenOpeningDoor : 1;
	bool m_bVehicleDoorOpen : 1;
	bool m_bForcedDoorState : 1;
	bool m_ClonePlayerWaitingForLocalPedAttachExitCar : 1;
	bool m_ShouldAlterEntryForOpenDoor : 1;
	bool m_bTurnedOnDoorHandleIk;
	bool m_bTurnedOffDoorHandleIk;
	bool m_bLerpingCloneToDoor;
	bool m_bSmashedWindow;
	bool m_bEnterVehicleLookAtStarted;
	bool m_bCreatedVehicleWpnMgr;
	bool m_bWantsToShuffle;
	static const u32 m_uEnterVehicleLookAtHash;

	bool m_bFriendlyJackDelayEndedLastFrame;

	////////////////////////////////////////////////////////////////////////////////
public:
	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId	ms_BlendInSteeringWheelIkId;
	static const fwMvBooleanId	ms_JackClipFinishedId;
	static const fwMvBooleanId	ms_PickOrPullUpClipFinishedId;
	static const fwMvBooleanId	ms_SeatShuffleClipFinishedId;
	static const fwMvBooleanId	ms_AlignToEnterSeatOnEnterId;
	static const fwMvBooleanId	ms_AlignToEnterSeatAnimFinishedId;
	static const fwMvBooleanId	ms_ClimbUpOnEnterId;
	static const fwMvBooleanId	ms_ClimbUpAnimFinishedId;
	static const fwMvBooleanId	ms_JackPedFromOutsideOnEnterId;
	static const fwMvBooleanId	ms_PickUpBikeOnEnterId;
	static const fwMvBooleanId	ms_PullUpBikeOnEnterId;
	static const fwMvBooleanId	ms_StreamedEntryOnEnterId;
	static const fwMvBooleanId	ms_StreamedEntryClipFinishedId;
	static const fwMvBooleanId	ms_InterruptId;
	static const fwMvBooleanId	ms_AlignOnEnterId;
	static const fwMvBooleanId	ms_AllowBlockedJackReactionsId;
	static const fwMvClipId		ms_JackPedClipId;
	static const fwMvClipId		ms_PickPullClipId;
	static const fwMvClipId		ms_SeatShuffleClipId;
	static const fwMvClipId		ms_AlignToEnterSeatClipId;
	static const fwMvClipId		ms_ClimbUpClipId;
	static const fwMvClipId		ms_StreamedEntryClipId;
	static const fwMvFlagId		ms_FromInsideId;
	static const fwMvFlagId		ms_IsDeadId;
	static const fwMvFlagId		ms_IsFatId;
	static const fwMvFlagId		ms_IsPickUpId;
	static const fwMvFlagId		ms_IsFastGetOnId;
	static const fwMvFloatId	ms_JackPedPhaseId;
	static const fwMvFloatId	ms_PickPullPhaseId;
	static const fwMvFloatId	ms_SeatShufflePhaseId;
	static const fwMvFloatId	ms_AlignToEnterSeatPhaseId;
	static const fwMvFloatId	ms_ClimbUpPhaseId;
	static const fwMvFloatId	ms_StreamedEntryPhaseId;
	static const fwMvRequestId	ms_SeatShuffleRequestId;
	static const fwMvRequestId	ms_SeatShuffleAndJackPedRequestId;
	static const fwMvRequestId	ms_JackPedRequestId;
	static const fwMvRequestId	ms_PickOrPullUpBikeRequestId;
	static const fwMvRequestId	ms_PickUpBikeRequestId;
	static const fwMvRequestId	ms_PullUpBikeRequestId;
	static const fwMvRequestId	ms_AlignToEnterSeatRequestId;
	static const fwMvRequestId	ms_ClimbUpRequestId;
	static const fwMvRequestId	ms_StreamedEntryRequestId;
	static const fwMvRequestId	ms_ArrestRequestId;
	static const fwMvStateId	ms_SeatShuffleStateId;
	static const fwMvStateId	ms_JackPedFromOutsideStateId;
	static const fwMvStateId	ms_AlignToEnterSeatStateId;
	static const fwMvStateId	ms_ClimbUpStateId;
	static const fwMvStateId	ms_PickUpPullUpStateId;
	static const fwMvStateId	ms_StreamedEntryStateId;
	static const fwMvNetworkId  ms_AlignNetworkId;
	static const fwMvClipSetVarId  ms_SeatClipSetId;

	////////////////////////////////////////////////////////////////////////////////

	static const u32 TIME_TO_DETECT_ENTER_INPUT_TAP = 500;

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, ENTER_VEHICLE_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Enter vehicle task information for cloned peds
////////////////////////////////////////////////////////////////////////////////
class CClonedEnterVehicleInfo: public CClonedVehicleFSMInfo
{
public:
	CClonedEnterVehicleInfo();
	CClonedEnterVehicleInfo(s32 enterState,
							CVehicle* pVehicle, 
							SeatRequestType iSeatRequest, 
							s32 iSeat, 
							s32 iTargetEntryPoint, 
							VehicleEnterExitFlags iRunningFlags, 
							CPed* pJackedPed, 
							bool bShouldTriggerElectricTask, 
							const fwMvClipSetId &clipSetId, 
							u32 damageTotalTime, 
							eRagdollTriggerTypes ragdollType,
							const fwMvClipSetId &streamedClipSetId,
							const fwMvClipId & streamedClipId, 
							bool bVehicleDoorOpen, 
							const Vector3& targetPosition);

	virtual CTaskFSMClone* CreateCloneFSMTask();
    virtual CTask         *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType( ) const { return CTaskInfo::INFO_TYPE_ENTER_VEHICLE_CLONED; }
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_ENTER_VEHICLE; }

	virtual bool    HasState() const		{ return true; }
	virtual u32		GetSizeOfState() const	{ return datBitsNeeded<CTaskEnterVehicle::State_Finish>::COUNT;}

	virtual s32		GetTargetEntryExitPoint() const { return m_iTargetEntryPoint; }

	CPed*			GetJackedPed() const { return m_pJackedPed.GetPed(); }
	bool			GetShouldTriggerElectricTask() const { return m_bShouldTriggerElectricTask; }
	bool			GetVehicleDoorOpen() const { return m_bVehicleDoorOpen; }
	u32				GetDamageTotalTime() const { return m_uDamageTotalTime; }
	fwMvClipSetId	GetClipSetId() const { return m_clipSetId; }
	eRagdollTriggerTypes GetRagdollType() const { return m_RagdollType; }

	fwMvClipSetId	GetStreamedClipSetId() const { return m_StreamedClipSetId; }
	fwMvClipId		GetStreamedClipId() const { return m_StreamedClipId; }
	const Vector3&	GetTargetPosition() const { return m_targetPosition; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CClonedVehicleFSMInfo::Serialise(serialiser);

		ObjectId targetID = m_pJackedPed.GetPedID();
		m_pJackedPed.SetPedID(targetID);

		SERIALISE_OBJECTID(serialiser, targetID, "Target entity");
		SERIALISE_BOOL(serialiser, m_bShouldTriggerElectricTask, "Should Trigger Electric Task");
		SERIALISE_BOOL(serialiser, m_bVehicleDoorOpen, "Vehicle Door Open");
		SERIALISE_UNSIGNED(serialiser, m_uDamageTotalTime, SIZEOF_DAMAGE_TOTAL_TIME, "Damage Total Time");
		SERIALISE_ANIMATION_CLIP(serialiser, m_clipSetId, m_StreamedClipId, "Animation");
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_RagdollType), SIZEOF_RAGDOLL_TYPE, "Ragdoll Type");
		SERIALISE_ANIMATION_CLIP(serialiser, m_StreamedClipSetId, m_StreamedClipId, "Streamed Animation");
		SERIALISE_POSITION(serialiser, m_targetPosition, "Target Position");
	}

private:

	CSyncedPed		m_pJackedPed;

	static const unsigned int SIZEOF_DAMAGE_TOTAL_TIME	= 32;
	static const unsigned int SIZEOF_RAGDOLL_TYPE = datBitsNeeded<RAGDOLL_TRIGGER_NUM-1>::COUNT;

	bool m_bVehicleDoorOpen;
	bool m_bShouldTriggerElectricTask;
	fwMvClipSetId m_clipSetId; 
	u32 m_uDamageTotalTime;
	eRagdollTriggerTypes m_RagdollType;

	fwMvClipSetId m_StreamedClipSetId; 
	fwMvClipId m_StreamedClipId; 
	Vector3 m_targetPosition;
	float m_createdLocalTaskMBR;
};

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task used to align a ped to the entry point of a vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskEnterVehicleAlign : public CTask
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //			// NOTES:

		bool m_UseAttachDuringAlign;				// Should we compute position directly and ignore physics during the alignment, or apply forces to get us there?
		bool m_RenderDebugToTTY;					// Render debug to tty
		bool m_ApplyRotationScaling;
		bool m_ApplyTranslationScaling;
		bool m_DisableRotationOvershootCheck;
		bool m_DisableTranslationOvershootCheck;	
		bool m_ReverseLeftFootAlignAnims;
		float m_TranslationChangeRate;				// Rate the translation changes to nail the target
		float m_RotationChangeRate;					// Rate the rotation changes to nail the target
		float m_DefaultAlignRate;					// Scale the anim playback rate
		float m_FastAlignRate;					// Scale the anim playback rate
		float m_CombatAlignRate;
		float m_ActionCombatAlignRate;

		// ---	END_DEV_TO_REMOVE	--- //

		bool m_ForceStandEnterOnly;

		float m_StandAlignMaxDist;					// Distance to do stand aligns
		float m_AlignSuccessMaxDist;				// Distance to do consider align to have succeeded
		float m_DefaultAlignStartFixupPhase;		// Default starting phase for mover fixup
		float m_DefaultAlignEndFixupPhase;			// Default end phase for mover fixup
		float m_TargetRadiusForOrientatedAlignWalk;	// Target go to radius to trigger walk align
		float m_TargetRadiusForOrientatedAlignRun;	// Target go to radius to trigger run align

		float m_MinRotationalSpeedScale;			// Velocity speed scaling / range values used during the align to scale or set the desired velocity. Magic numbers.
		float m_MaxRotationalSpeedScale;
		float m_MaxRotationalSpeed;
		float m_MinTranslationalScale;
		float m_MaxTranslationalScale;
		float m_MaxTranslationalStandSpeed;
		float m_MaxTranslationalMoveSpeed;
		float m_HeadingReachedTolerance;
		float m_MinSqdDistToSetPos;

		float m_StdVehicleMinPhaseToStartRotFixup;
		float m_BikeVehicleMinPhaseToStartRotFixup;
		float m_MinDistAwayFromEntryPointToConsiderFinished;
		float m_MinPedFwdToEntryDotToClampInitialOrientation;
		float m_MinDistToAlwaysClampInitialOrientation;
		float m_VaultExtraZGroundTest;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(const CEntity* pVehicle, s32 iEntryPointIndex);

	// PURPOSE: Get the correct clipsetid which contains the entry anims
	static fwMvClipSetId GetEntryClipsetId(const CEntity* pEntity, s32 iEntryPointIndex, const CPed* pEnteringPed);
	static fwMvClipSetId GetCommonClipsetId(const CEntity* pEntity, s32 iEntryPointIndex, bool bOnVehicle = false);

	// PURPOSE: Determine if this a valid entry index
	static bool IsEntryIndexValid(const CEntity* pVehicle, s32 iEntryPointIndex);

	// PURPOSE: Check if we're close enough to consider the align to be finished
	static bool AlignSucceeded(const CEntity& vehicle, const CPed& ped, s32 iEntryPointIndex, bool bEnterOnVehicle = false, bool bCombatEntry = false, bool bInWaterEntry = false, Vector3 * pvLocalSpaceOffset=NULL, bool bUseCloserTolerance = false, bool bFinishedAlign = false, bool bFromCover = false);

	// PURPOSE: See if ped should do a standing align
	static bool ShouldDoStandAlign(const CEntity& vehicle, const CPed& ped, s32 iEntryPointIndex, Vector3 * pvLocalSpaceOffset=NULL);

	// PURPOSE: Extracts a CVehicleEntryPointAnimInfo object
	static const CVehicleEntryPointAnimInfo* GetEntryAnimInfo(const CEntity* vehicle, s32 iEntryPointIndex);

	static const CVehicleLayoutInfo* GetLayoutInfo(const CEntity* pEnt);

	static bool PassesGroundConditionForAlign(const CPed& rPed, const CEntity& rVeh);
	static bool PassesOnVehicleHeightDeltaConditionForAlign(const CPed& rPed, const CEntity& rEnt, bool bEntryOnVehicle, s32 iEntryPointIndex, const Vector3* pvLocalSpaceOffset);

	// PURPOSE: See if ped should do align
	static bool ShouldTriggerAlign(const CEntity& vehicle, const CPed& ped, s32 iEntryPointIndex, Vector3 * pvLocalSpaceOffset=NULL, bool bCombatEntry = false, bool bFromCover = false, bool bDisallowUnderWater = false, bool bIsOnVehicleEntry = false);

	// PURPOSE: Compute the align target
	static bool ShouldComputeIdealTargetForTurret(const CEntity& rEnt, bool bEntryOnVehicle, s32 iEntryPointIndex);
	static void ComputeAlignTarget(Vector3& vTransTarget, Quaternion& qRotTarget, bool bEntryOnVehicle, const CEntity& rEnt, const CPed& rPed, s32 iEntryPointIndex, const Vector3* pvLocalSpaceOffset = NULL, bool bCombatEntry = false, bool bInWaterEntry = false, bool bForcedDoorState = false, bool bForceOpen = false);

	static float LerpTowardsFromDirection(float fFrom, float fTo, bool bLeft, float fPerc);

	const CVehicleEntryPointInfo* GetEntryInfo(s32 iEntryPointIndex) const;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define ENTER_VEHICLE_ALIGN_STATES(X)	X(State_Start),					\
											X(State_StreamAssets),			\
											X(State_OrientatedAlignment),	\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eEnterVehicleAlignState
	{
		ENTER_VEHICLE_ALIGN_STATES(DEFINE_TASK_ENUM)
	};

	CTaskEnterVehicleAlign(CEntity* pVehicle, s32 iEntryPointIndex, const Vector3 & vLocalSpaceOffset, bool bUseFastRate = false);
	
	virtual ~CTaskEnterVehicleAlign();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const { return rage_new CTaskEnterVehicleAlign(m_pVehicle, m_iEntryPointIndex, m_vLocalSpaceOffset, m_bUseFastRate); }
	
	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const	{ return CTaskTypes::TASK_ENTER_VEHICLE_ALIGN; }

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }

	fwMvClipSetId GetRequestedEntryClipSetId() const;
	fwMvClipSetId GetRequestedCommonClipSetId(bool bEnterOnVehicle) const;

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
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after position is updated
	virtual bool ProcessPostMovement();

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return StreamAssets_OnUpdate();
	FSM_Return OrientatedAlignment_OnEnter();
	FSM_Return OrientatedAlignment_OnUpdate();

	void ProcessTurretTurn( CTaskEnterVehicle& rEnterVehicleTask, CVehicle& rVeh );

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Start the vehicle move network
	bool StartMoveNetwork();

	// PURPOSE: Compute attachment position and update peds attachment to the vehicle
	void ComputeOrientatedAttachPosition(float fTimeStep);

	// PURPOSE: Compute the desired velocity and use the physics to move the ped (Note this overrides the animated velocity)
	void ComputeDesiredVelocity(float fTimeStep);

	// PURPOSE: Find the mover fix up phases from the clip passed in
	void FindMoverFixUpPhases(const crClip* pClip);

	// PURPOSE: Get the active clip
	const crClip* GetActiveClip(s32 iIndex = 0) const;

	// PURPOSE: Get the active phase
	float GetActivePhase(s32 iIndex = 0) const;

	// PURPOSE: Get the active weight
	float GetActiveWeight(s32 iIndex = 0) const;

	// PURPOSE: Send the MoVE signals
	void UpdateMoVE();

	// PURPOSE: Compute alignment move signal parameter
	void ComputeAlignAngleSignal();

	// PURPOSE: Returns the clip index which is most blended in, computes it if not valid
	s32 GetMainClipIndex();

	// PURPOSE: Finds the most blended clip
	s32 FindMostBlendedClip(s32 iNumClips) const;

	// PURPOSE: Determine which way to lerp the peds heading
	bool ShouldApproachFromLeft();

	// PURPOSE: Compute the remaining translation/rotation based on the clips being blended
	void ComputeRemainingMoverChanges(s32 iNumClips, Vector3& vClipTotalDist, float fRotOffset, float fStartScalePhase);

	// PURPOSE: Have the align clips finished?
	bool HaveAlignClipsFinished() const;
	bool ShouldWaitForAllAlignAnimsToFinish() const;
	bool AreAllWeightedAnimsFinished() const;

	// PURPOSE: Set the initial move signals once when we request to align
	void SetInitialSignals();

	void SetClipPlaybackRate();

	// PURPOSE: Checks if the arm id start phase has been set
	bool IsArmIkPhaseValid() const { return m_fArmIkStartPhase >= 0.0f; }

	bool IsShortestAlignDirectionLeft() const;

	void ComputeAlignTarget(Vector3& vTransTarget, Quaternion& qRotTarget);

	bool IsEnteringStandingTurretSeat() const;

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:
	
	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper	m_MoveNetworkHelper;
	
	// PURPOSE: The vehicle we are aligning to	
	RegdEnt	m_pVehicle;

	// PURPOSE: The entry point corresponding to the vehicle
	s32	m_iEntryPointIndex;

	// PURPOSE: Initial ped position relative to vehicle
	Vector3	m_vLocalInitialPosition;

	// PURPOSE : Local space modifier to entry position
	Vector3 m_vLocalSpaceOffset;

	// PURPOSE: Initial ped heading
	float m_fInitialHeading;

	// PURPOSE: Alignment angle signal (For walk/run aligns : 0 - Front, 0.5 - side, 1 - rear)
	float m_fAlignAngleSignal;

	// PURPOSE: Last phase for align clip
	float m_fLastPhase;

	// PURPOSE: Store whether we started a stand align or a moving align
	bool m_bStartedStandAlign;

	// PURPOSE: Phase to start the arm ik at
	float m_fArmIkStartPhase;

	// PURPOSE: Decides which arm to use to reach for the door handle
	bool m_bReachRightArm; 

	float m_fPhaseAttachmentBegins;
	bool m_bComputedAttachThisFrame;

	// PURPOSE: The internal task index of the clip most blended in
	s32 m_iMainClipIndex;

	float m_fTranslationFixUpStartPhase;
	float m_fTranslationFixUpEndPhase;
	float m_fRotationFixUpStartPhase;
	float m_fRotationFixUpEndPhase;

	float m_fFirstPhysicsTimeStep;
	float m_fDestroyWeaponPhase;
	float m_fAverageSpeedDuringBlendIn;

	Vector3 m_vInitialPosition;
	Vector3 m_vLastPosition;
	float m_fLastHeading;
	bool m_bLastSituationValid;

	bool m_bRun;

	bool m_bApproachFromLeft;
	bool m_bComputedApproach;
	bool m_bUseFastRate;

	bool m_bComputedMoverTrackChanges;
	float m_fTotalHeadingChange;
	Vector3 m_vTotalTranslationChange;
	float m_fFixUpRate;
	bool m_bHeadingReached;
	bool m_bOrientatedAlignStateFinishedThisFrame;

	bool m_bProcessPhysicsCalledThisFrame;
	QuatV m_qFirstPhysicsOrientationDelta;
	Vec3V m_vFirstPhysicsTranslationDelta;
	Vec3V m_vInitialSeatOffset;

	bool m_bInWaterAlignment;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: The number of clips to check
	static const s32 ms_iNumMoveAlignClips = 3;
	static const s32 ms_iNumStandAlignClips = 5;

	// PURPOSE: Enums corresponding to align clips
	enum eStandAlignClips
	{
		eStandAlign180R		= 0,
		eStandAlign90R		= 1,
		eStandAlign0		= 2,
		eStandAlign90L		= 3,
		eStandAlign180L		= 4
	};

	enum eMoveAlignClips
	{
		eMoveAlignFront		= 0,
		eMoveAlignSide		= 1,
		eMoveAlignRear		= 2
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId	ms_ShortAlignOnEnterId;
	static const fwMvBooleanId	ms_OrientatedAlignOnEnterId;
	static const fwMvBooleanId  ms_AlignClipFinishedId;
	static const fwMvBooleanId	ms_FrontAlignClipFinishedId;	
	static const fwMvBooleanId	ms_SideAlignClipFinishedId;	
	static const fwMvBooleanId	ms_RearAlignClipFinishedId;	
	static const fwMvBooleanId	ms_DisableEarlyAlignBlendOutId;
	static const fwMvFlagId		ms_RunFlagId;
	static const fwMvFlagId		ms_LeftFootFirstFlagId;
	static const fwMvFlagId		ms_StandFlagId;
	static const fwMvFlagId		ms_BikeOnGroundId;
	static const fwMvFlagId		ms_FromSwimmingId;
	static const fwMvFlagId		ms_FromOnVehicleId;
	static const fwMvFlagId		ms_ShortAlignFlagId;
	static const fwMvFloatId	ms_AlignPhaseId;
	static const fwMvFloatId	ms_AlignAngleId;
	static const fwMvFloatId	ms_AlignFrontBlendWeightId;
	static const fwMvFloatId	ms_AlignSideBlendWeightId;
	static const fwMvFloatId	ms_AlignRearBlendWeightId;
	static const fwMvFloatId	ms_StandAlign180LBlendWeightId;
	static const fwMvFloatId	ms_StandAlign90LBlendWeightId;
	static const fwMvFloatId	ms_StandAlign0BlendWeightId;
	static const fwMvFloatId	ms_StandAlign90RBlendWeightId;
	static const fwMvFloatId	ms_StandAlign180RBlendWeightId;
	static const fwMvFloatId	ms_AlignFrontPhaseId;
	static const fwMvFloatId	ms_AlignSidePhaseId;
	static const fwMvFloatId	ms_AlignRearPhaseId;
	static const fwMvFloatId	ms_StandAlign180LPhaseId;
	static const fwMvFloatId	ms_StandAlign90LPhaseId;
	static const fwMvFloatId	ms_StandAlign0PhaseId;
	static const fwMvFloatId	ms_StandAlign90RPhaseId;
	static const fwMvFloatId	ms_StandAlign180RPhaseId;
	static const fwMvFloatId	ms_AlignRateId;
	static const fwMvClipId		ms_AlignFrontClipId;
	static const fwMvClipId		ms_AlignSideClipId;
	static const fwMvClipId		ms_AlignRearClipId;
	static const fwMvClipId		ms_AlignClipId;
	static const fwMvClipId		ms_StandAlign180LClipId;
	static const fwMvClipId		ms_StandAlign90LClipId;
	static const fwMvClipId		ms_StandAlign0ClipId;
	static const fwMvClipId		ms_StandAlign90RClipId;
	static const fwMvClipId		ms_StandAlign180RClipId;
	static const fwMvStateId	ms_ShortAlignStateId;
	static const fwMvStateId	ms_OrientatedAlignStateId;

	////////////////////////////////////////////////////////////////////////////////

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, ENTER_VEHICLE_ALIGN_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task used to make a ped try to open the vehicle door from the outside
////////////////////////////////////////////////////////////////////////////////

class CTaskOpenVehicleDoorFromOutside : public CTaskVehicleFSM
{
public:

	static const fwMvFloatId ms_OpenDoorRateId;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define OPEN_DOOR_STATES(X)	X(State_Start),					\
								X(State_TryLockedDoor),			\
								X(State_ForcedEntry),			\
								X(State_OpenDoor),				\
								X(State_OpenDoorBlend),			\
								X(State_OpenDoorCombat),		\
								X(State_OpenDoorWater),			\
								X(State_Finish)

	// PURPOSE: FSM states
	enum eEnterVehicleOpenDoorState
	{
		OPEN_DOOR_STATES(DEFINE_TASK_ENUM)
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //			// NOTES:

		bool m_EnableOpenDoorHandIk;

		// ---	END_DEV_TO_REMOVE	--- //

		float m_DefaultOpenDoorStartPhase;
		float m_DefaultOpenDoorEndPhase;
		float m_DefaultOpenDoorStartIkPhase;
		float m_DefaultOpenDoorEndIkPhase;
		float m_MinBlendWeightToUseHighClipEvents;
		float m_DefaultOpenDoorRate;
		float m_MinHandleHeightDiffVan;
		float m_MaxHandleHeightDiffVan;
		float m_MaxHandleHeightDiff;

		fwMvClipId m_DefaultOpenDoorClipId;
		fwMvClipId m_HighOpenDoorClipId;
		fwMvClipId m_CombatOpenDoorClipId;
		fwMvClipId m_DefaultTryLockedDoorClipId;
		fwMvClipId m_DefaultForcedEntryClipId;
		fwMvClipId m_WaterOpenDoorClipId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	static void ProcessOpenDoorHelper(CTaskVehicleFSM* pVehicleTask, const crClip* pClip, CCarDoor* pDoor, float fPhase, float fPhaseToBeginOpenDoor, float fPhaseToEndOpenDoor, bool& bIkTurnedOn, bool& bIkTurnedOff, bool bProcessOnlyIfTagsFound = false, bool bFromOutside = false);

	CTaskOpenVehicleDoorFromOutside(CVehicle* pVehicle, s32 iEntryPointIndex, CMoveNetworkHelper& moveNetworkHelper);
	
	virtual ~CTaskOpenVehicleDoorFromOutside();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const { return rage_new CTaskOpenVehicleDoorFromOutside(m_pVehicle, m_iTargetEntryPoint, m_MoveNetworkHelper); }

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const	{ return CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE; }

	// PURPOSE: Gets the current clip clip and current phase depending on state
	const crClip* GetClipAndPhaseForState(float& fPhase);

	// PURPOSE: DAN TEMP - function for ensuring a valid clip transition is available based on the current move network state
	//  this will be replaced when the move network interface is extended to include this
	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }

	bool IsFixupFinished() const { return m_bFixupFinished; }
	bool GetAnimFinished() const { return m_bAnimFinished; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

	// PURPOSE: Clone task implementation. When cloned, this task continuously syncs to the remote state
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return TryLockedDoor_OnEnter();
	FSM_Return TryLockedDoor_OnUpdate();
	FSM_Return ForcedEntry_OnEnter();
	FSM_Return ForcedEntry_OnUpdate();
	FSM_Return OpenDoor_CommonOnEnter();
	FSM_Return OpenDoor_CommonOnUpdate();
	FSM_Return OpenDoor_CommonOnExit();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Checks the current clip to drive the door open
	void ProcessOpenDoor(bool& bIkTurnedOn, bool& bIkTurnedOff, bool bUseHighClip = false, bool bFromOutside = false);

	// PURPOSE: Set the clip playback rate
	void SetClipPlaybackRate();

	// PURPOSE:	Helper function to see which clips we should be using.
	bool ShouldUseHighClipEvents() const;

	void GetAlternateClipInfoForPed(s32 iState, const CPed& ped, fwMvClipSetId& clipSetId, fwMvClipId& clipId) const;

	bool SetClipsForState();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper& m_MoveNetworkHelper;

	// PURPOSE: The target door position
	Vector3 m_vTargetDoorPos;

	// PURPOSE: Track whether we have smashed the window already
	bool m_bSmashedWindow;
	bool m_bTurnedOnDoorHandleIk;
	bool m_bTurnedOffDoorHandleIk;

	// PURPOSE:	Used by ProcessMoveSignals() during certain states
	bool m_bAnimFinished;
	bool m_bWantsToExit;

	bool m_bOpenDoorFinished;
	bool m_bFixupFinished;
	bool m_bProcessPhysicsCalledThisFrame;
	QuatV m_qFirstPhysicsOrientationDelta;
	Vec3V m_vFirstPhysicsTranslationDelta;
	float m_fOriginalHeading;
	float m_fCombatDoorOpenLerpEnd;

	// Relative to vehicle
	QuatV m_qInitialOrientation;
	Vec3V m_vInitialTranslation;

    // PURPOSE: DAN TEMP - variable for ensuring a valid clip transition is available based on the current move network state
    s32  m_iLastCompletedState;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvRequestId	ms_TryLockedDoorId;
	static const fwMvStateId	ms_TryLockedDoorStateId;
	static const fwMvRequestId	ms_ForcedEntryId;
	static const fwMvRequestId	ms_ForcedEntryStateId;
	static const fwMvRequestId	ms_OpenDoorId;
	static const fwMvRequestId	ms_OpenDoorStateId;
	static const fwMvBooleanId	ms_OpenDoorClipFinishedId;
	static const fwMvBooleanId	ms_TryLockedDoorOnEnterId;
	static const fwMvBooleanId	ms_TryLockedDoorClipFinishedId;
	static const fwMvBooleanId	ms_ForcedEntryOnEnterId;
	static const fwMvBooleanId	ms_OpenDoorOnEnterId;
	static const fwMvClipId		ms_ForcedEntryClipId;
	static const fwMvClipId		ms_OpenDoorClipId;
	static const fwMvClipId		ms_HighOpenDoorClipId;
	static const fwMvClipId		ms_TryLockedDoorClipId;
	static const fwMvFloatId	ms_ForcedEntryPhaseId;
	static const fwMvFloatId	ms_OpenDoorPhaseId;
	static const fwMvFloatId	ms_TryLockedDoorPhaseId;
	static const fwMvFloatId	ms_OpenDoorVehicleAngleId;
	static const fwMvFloatId	ms_OpenDoorBlendId;
	static const fwMvFlagId		ms_UseOpenDoorBlendId;
	static const fwMvClipId		ms_OpenDoorHighClipId;
	static const fwMvFloatId	ms_OpenDoorHighPhaseId;
	static const fwMvBooleanId	ms_OpenDoorHighAnimFinishedId;
	static const fwMvBooleanId	ms_CombatDoorOpenLerpEndId;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, OPEN_DOOR_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Enter vehicle open door task information for cloned peds
////////////////////////////////////////////////////////////////////////////////
class CClonedEnterVehicleOpenDoorInfo: public CClonedVehicleFSMInfo
{
public:
	CClonedEnterVehicleOpenDoorInfo();
	CClonedEnterVehicleOpenDoorInfo( s32 enterState, CVehicle* pVehicle, SeatRequestType iSeatRequest, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags );
	//virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual s32 GetTaskInfoType( ) const {return CTaskInfo::INFO_TYPE_ENTER_VEHICLE_OPEN_DOOR_CLONED;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskOpenVehicleDoorFromOutside::State_Finish>::COUNT;}
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task used to make a ped try to close the vehicle door from the inside
////////////////////////////////////////////////////////////////////////////////
class CTaskCloseVehicleDoorFromInside : public CTaskVehicleFSM
{
public:

	// PURPOSE: Checks the current clip to drive the door closed
	static bool ProcessCloseDoor(CMoveNetworkHelper& moveNetworkHelper, CVehicle& vehicle, CPed& ped, s32 iTargetEntryPointIndex, bool& bTurnedOnDoorHandleIk, bool& bTurnedOffDoorHandleIk, bool bDisableArmIk);
	static bool ProcessCloseDoorBlend(CMoveNetworkHelper& moveNetworkHelper, CVehicle& vehicle, CPed& ped, s32 iTargetEntryPointIndex, bool& bTurnedOnDoorHandleIk, bool& bTurnedOffDoorHandleIk, bool bDisableArmIk);

	// PURPOSE: Checks if the area the door will close onto is clear of peds
	static bool IsDoorClearOfPeds(const CVehicle& vehicle, const CPed& ped, s32 iEntry = -1);

	// PURPOSE: FSM states
	enum eCloseVehicleDoorState
	{
		State_Start,
		State_CloseDoor,
		State_Finish
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //			// NOTES:

		bool m_EnableCloseDoorHandIk;

		// ---	END_DEV_TO_REMOVE	--- //

		float m_CloseDoorForceMultiplier;
		float m_DefaultCloseDoorStartPhase;
		float m_DefaultCloseDoorEndPhase;
		float m_DefaultCloseDoorStartIkPhase;
		float m_DefaultCloseDoorEndIkPhase;
		float m_MinBlendWeightToUseFarClipEvents;
		float m_VehicleSpeedToAbortCloseDoor;
		float m_PedTestXOffset;
		float m_PedTestYOffset;
		float m_PedTestZStartOffset;
		float m_PedTestZOffset;
		float m_PedTestRadius;
		float m_MinOpenDoorRatioToUseArmIk;
		
		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	static const fwMvFlagId		ms_UseCloseDoorBlendId;
	static const fwMvFlagId		ms_FirstPersonModeId;
	static const fwMvFilterId	ms_CloseDoorFirstPersonFilterId;
	static const fwMvFloatId	ms_CloseDoorLengthId;
	static const fwMvFloatId	ms_CloseDoorBlendId;
	static const fwMvBooleanId	ms_CloseDoorNearAnimFinishedId;
	static const fwMvClipId		ms_CloseDoorNearClipId;
	static const fwMvFloatId	ms_CloseDoorNearPhaseId;
	static const fwMvFloatId	ms_CloseDoorRateId;

	CTaskCloseVehicleDoorFromInside(CVehicle* pVehicle, s32 iEntryPointIndex, CMoveNetworkHelper& moveNetworkHelper);
	
	virtual ~CTaskCloseVehicleDoorFromInside();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const { return rage_new CTaskCloseVehicleDoorFromInside(m_pVehicle, m_iTargetEntryPoint, m_MoveNetworkHelper); }

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const	{ return CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_INSIDE; }

	// PURPOSE: Gets the current clip clip and current phase depending on state
	const crClip* GetClipAndPhaseForState(float& fPhase);

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

	// PURPOSE: Clone task implementation. When cloned, this task continuously syncs to the remote state
    virtual CTaskInfo* CreateQueriableState() const;
    virtual void ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
    virtual FSM_Return UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return CloseDoor_OnEnter();
	FSM_Return CloseDoor_OnUpdate();
	FSM_Return CloseDoor_OnExit();

	// PURPOSE: Set the clip playback rate
	void SetClipPlaybackRate();

	////////////////////////////////////////////////////////////////////////////////

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper& m_MoveNetworkHelper;

	// PURPOSE: Target door position
	Vector3	m_vTargetDoorPos;

	// PURPOSE:	Used by ProcessMoveSignals() during certain states
	bool	m_bMoveNetworkFinished;
	bool	m_bDontUseArmIk;

	bool m_bTurnedOnDoorHandleIk;
	bool m_bTurnedOffDoorHandleIk;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId	ms_CloseDoorClipFinishedId;
	static const fwMvBooleanId	ms_CloseDoorOnEnterId;
	static const fwMvClipId		ms_CloseDoorClipId;
	static const fwMvFloatId	ms_CloseDoorPhaseId;
	static const fwMvRequestId	ms_CloseDoorRequestId;
	static const fwMvStateId	ms_CloseDoorStateId;
	static const fwMvBooleanId	ms_CloseDoorShuffleInterruptId;
	static const fwMvBooleanId	ms_CloseDoorStartEngineInterruptId;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Ped enters the vehicle seat from the outside
////////////////////////////////////////////////////////////////////////////////

class CTaskEnterVehicleSeat : public CTaskVehicleFSM
{
public:

	static const fwMvBooleanId ms_GetInShuffleInterruptId;
	static const fwMvBooleanId ms_CloseDoorInterruptId;

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinVelocityToRagdollPed;
		float m_MaxVelocityToEnterBike;

		fwMvClipId m_DefaultGetInClipId;
		fwMvClipId m_GetOnQuickClipId;
		fwMvClipId m_GetInFromWaterClipId;
		fwMvClipId m_GetInStandOnClipId;
		fwMvClipId m_GetInCombatClipId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define ENTER_VEHICLE_SEAT_STATES(X)	X(State_Start),					\
											X(State_GetIn),					\
											X(State_QuickGetOn),			\
											X(State_GetInFromSeatClipSet),	\
											X(State_GetInFromWater),		\
											X(State_GetInFromStandOn),		\
											X(State_GetInCombat),			\
											X(State_Ragdoll),				\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eEnterVehicleSeatState
	{
		ENTER_VEHICLE_SEAT_STATES(DEFINE_TASK_ENUM)
	};

	CTaskEnterVehicleSeat(CVehicle* pVehicle, s32 iEntryPointIndex, s32 iSeat, VehicleEnterExitFlags iRunningFlags, CMoveNetworkHelper& moveNetworkHelper, bool forceMoveState = false);
	
	virtual ~CTaskEnterVehicleSeat();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const { return rage_new CTaskEnterVehicleSeat(m_pVehicle, m_iTargetEntryPoint, m_iSeat, m_iRunningFlags, m_MoveNetworkHelper); }

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const	{ return CTaskTypes::TASK_ENTER_VEHICLE_SEAT; }

	// PURPOSE: Gets the current clip clip and current phase depending on state
	const crClip* GetClipAndPhaseForState(float& fPhase);

	// PURPOSE: DAN TEMP - function for ensuring a valid clip transition is available based on the current move network state
	// this will be replaced when the move network interface is extended to include this
	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_MoveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_MoveNetworkHelper; }

	bool WantsToExit() const { return m_bWantsToExit; }
	bool GetBikeWasOnSideAtBeginningOfGetOn() const { return m_bBikeWasOnSideAtBeginningOfGetOn; }

	static int IntegerSort(const s32 *a, const s32 *b);
	static float GetMultipleEntryAnimClipRateModifier(CVehicle* pVeh, const CPed* pPed, bool bExitingVehicle=false);

protected:

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE:	Communicate with Move
	virtual bool ProcessMoveSignals();

	// PURPOSE: Clone task implementation. When cloned, this task continuously syncs to the remote state
    virtual CTaskInfo* CreateQueriableState() const;
    virtual void ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo)) {}
    virtual FSM_Return UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	FSM_Return EnterSeatCommon_OnEnter();
	FSM_Return EnterSeatCommon_OnUpdate();
	FSM_Return EnterSeatCommon_OnExit();
	FSM_Return Ragdoll_OnEnter();
	FSM_Return Ragdoll_OnUpdate();
	FSM_Return Finish_OnUpdate();

	bool CheckForTransitionToRagdollState(CPed& rPed, CVehicle& rVeh) const;

	// PURPOSE: Set the clip playback rate
	void SetClipPlaybackRate();

	bool SetClipsForState();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

	////////////////////////////////////////////////////////////////////////////////

private:

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper&	m_MoveNetworkHelper;

	// PURPOSE: Ped being jacked as part of the shuffle
	RegdPed	m_pJackedPed;
	RegdPed m_pDriver;

	// PURPOSE:	Used by ProcessMoveSignals() during certain states
	bool m_bMoveNetworkFinished;

	// PURPOSE: Ped now wants to exit the vehicle
	bool m_bWantsToExit;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId ms_EnterSeatClipFinishedId;
	static const fwMvBooleanId ms_BlockRagdollActivationId;
	static const fwMvBooleanId ms_EnterSeatOnEnterId;
	static const fwMvClipId ms_EnterSeatClipId;
	static const fwMvFlagId ms_IsDeadId;
	static const fwMvFlagId ms_IsFatId;
	static const fwMvFloatId ms_EnterSeatPhaseId;
	static const fwMvRequestId ms_EnterSeatRequestId;
    static const fwMvStateId ms_EnterSeatStateId;

	////////////////////////////////////////////////////////////////////////////////

    // PURPOSE: Indicates whether the move network state must be forced, rather than requested
    //          In network games this task can be started when the move network is not at an appropriate state
    bool m_ForceMoveState;
	bool m_bAppliedForce;
	bool m_bBikeWasOnSideAtBeginningOfGetOn;
	float m_fSpringTime;
	float m_fInitialRatio;
	float m_fLegIkBlendOutPhase;

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, ENTER_VEHICLE_SEAT_STATES)

#endif // !__FINAL
};

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Ped enters the connected seat from the inside of a vehicle as part of either
// entry/exit or just a shuffle
////////////////////////////////////////////////////////////////////////////////

class CTaskInVehicleSeatShuffle : public CTaskFSMClone
{
public:

	static const fwMvBooleanId	ms_ShuffleInterruptId;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define IN_VEHICLE_SEAT_SHUFFLE_STATES(X)	X(State_Start),						\
												X(State_StreamAssets),				\
												X(State_ReserveShuffleSeat),		\
												X(State_WaitForInsert),				\
												X(State_WaitForNetwork),			\
												X(State_Shuffle),					\
												X(State_JackPed),					\
												X(State_UnReserveShuffleSeat),		\
												X(State_WaitForPedToLeaveSeat),		\
												X(State_SetPedInSeat),				\
												X(State_Finish)

	// PURPOSE: FSM states
	enum eInVehicleSeatShuffleState
	{
		IN_VEHICLE_SEAT_SHUFFLE_STATES(DEFINE_TASK_ENUM)
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(const CPed* pPed, const CVehicle* pVehicle);

	// PURPOSE: 
	static bool CanShuffleSeat(const CVehicle* pVehicle, s32 iSeatIndex, s32 iEntryPoint, s32* piShuffleSeatIndex = NULL, s32* piShuffleSeatEntryIndex = NULL, bool bOnlyUseSecondShuffleLink = false);

	static bool CanShuffleToTargetSeat(const CModelSeatInfo &rModelInfo, s32 iTargetSeatIndex, const CVehicle* pVehicle, s32* &piShuffleSeatIndex, s32* &piShuffleSeatEntryIndex);

	// PURPOSE: Get the correct clipsetid which contains the shuffle anims
	static fwMvClipSetId GetShuffleClipsetId(const CVehicle* pVehicle, s32 iSeatIndex);

    // PURPOSE: Get the ID of the seat the ped is shuffling to
    s32 GetTargetSeatIndex() const { return m_iTargetSeatIndex; }

	////////////////////////////////////////////////////////////////////////////////

	CTaskInVehicleSeatShuffle(CVehicle* pVehicle, CMoveNetworkHelper* pParentNetwork, bool bJackAnyOne = false, s32 iTargetSeatIndex = -1);
	
	virtual ~CTaskInVehicleSeatShuffle();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask*	Copy() const { return rage_new CTaskInVehicleSeatShuffle(m_pVehicle, m_pMoveNetworkHelper, m_bJackAnyOne, m_iTargetSeatIndex); }

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const	{ return CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE; }

	// PURPOSE: 
	bool ShouldAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent);

	// PURPOSE: Gets the current clip clip and current phase depending on state
	const crClip* GetClipAndPhaseForState(float& fPhase);

	// PURPOSE: Get the tasks main move network helper
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return m_pMoveNetworkHelper; }

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return m_pMoveNetworkHelper; }

    // PURPOSE: Sets whether the task needs to wait for the seat being shuffled to to become empty
    void SetWaitForSeatToBeEmpty( bool bWait ) { m_bWaitForSeatToBeEmpty = bWait; }

    // PURPOSE: Sets whether the task needs to wait for the seat being shuffled to to become occupied
	void SetWaitForSeatToBeOccupied( bool bWait ) { m_bWaitForSeatToBeOccupied = bWait; }

	// used by clone task:
	virtual bool OverridesVehicleState() const	{ return true; }

	const CVehicle* GetVehiclePtr() const { return m_pVehicle; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();
	virtual bool IsInScope(const CPed* pPed);

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool ProcessMoveSignals();

    // PURPOSE: creates a task info for this task
    virtual CTaskInfo* CreateQueriableState() const;

    // PURPOSE: reads task info from data received over the network
    virtual void ReadQueriableState(CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo));

    // PURPOSE: update function for when this task is running on a clone ped
    virtual FSM_Return UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return	Start_OnUpdate();
	FSM_Return	StreamAssets_OnUpdate();
	FSM_Return	ReserveShuffleSeat_OnUpdate();
	void		WaitForInsert_OnEnter();
	FSM_Return	WaitForInsert_OnUpdate();
	FSM_Return	WaitForNetwork_OnUpdate();
	void		Shuffle_OnEnter();
	FSM_Return	Shuffle_OnUpdate();
	FSM_Return	UnReserveShuffleSeat_OnUpdate();
	void		JackPed_OnEnter();
	FSM_Return	JackPed_OnUpdate();
    void        WaitForPedToLeaveSeat_OnEnter();
    FSM_Return	WaitForPedToLeaveSeat_OnUpdate();
	void		SetPedInSeat_OnEnter();
	FSM_Return	SetPedInSeat_OnUpdate();
    void        Finish_OnEnter();
	FSM_Return	Finish_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

private:

	// PURPOSE: Stores the offset to the target
	void StoreInitialOffsets();

	// PURPOSE: Starts the move network
	void StartMoveNetwork();

	// PURPOSE: Returns false if ped wants to quit the shuffle, otherwise sets the state
	bool DecideShuffleState(bool bGiveJackedPedEvent = true);

    //PURPOSE: Ensures that are locally controlled peds that are being jacked by a remote ped leave the vehicle, if certain
    //         task states are skipped due to lag issues the initial jacking request may have been missed
    void EnsureLocallyJackedPedsExitVehicle();

	//PURPOSE: Determines whether the motion state should be forced to the "do nothing" based on the current state
	bool ShouldMotionStateBeForcedToDoNothing();

	// PURPOSE: Set the clip playback rate
	void SetClipPlaybackRate();
	void SetClipsForState();
	bool ShouldUse180Clip(const CTurret* pTurret) const;
	fwMvClipId GetShuffleClip1(const CTurret* pTurret) const;
	fwMvClipId GetShuffleClip2(const CTurret* pTurret) const;
	fwMvClipId GetShuffleJackClip(const CTurret* pTurret) const;
	fwMvClipId GetShuffleDeadJackClip(const CTurret* pTurret) const;

	void KeepComponentReservation();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

	// PURPOSE: Move network helper, contains the interface to the move network (used if entering/exiting vehicle)
	CMoveNetworkHelper*	m_pMoveNetworkHelper;

	// PURPOSE: Move network helper, contains the interface to the move network (used if already inside vehicle)
	CMoveNetworkHelper m_MoveNetworkHelper;

	// PURPOSE: Helper to calculate a new attachment position
	CPlayAttachedClipHelper	m_PlayAttachedClipHelper;

	// PURPOSE: Vehicle ped is in
	RegdVeh m_pVehicle;

	// PURPOSE: Ped being jacked as part of shuffle
	RegdPed m_pJackedPed;

	// PURPOSE: Seat ped is shuffling to
	s32	m_iTargetSeatIndex;
	
	// PURPOSE: Seat ped is currently in
	s32	m_iCurrentSeatIndex;

    // PURPOSE: Time to wait for a remote ped to exit their seat, when the network game is lagging there can be delay between
    //          the local ped jacking a remote ped and the remote ped leaving the vehicle. In this case we must wait.
    s16 m_iWaitForPedTimer;

	// PURPOSE: Store whether the task is being run as part of an entry/exit
	bool m_bPedEnterExit;

    // PURPOSE: Store whether the task needs to wait for the seat being shuffled to to become empty
    bool m_bWaitForSeatToBeEmpty;

    // PURPOSE: Store whether the task needs to wait for the seat being shuffled to to become occupied
	bool m_bWaitForSeatToBeOccupied;
	bool m_bJackAnyOne;

	bool m_bMoveNetworkFinished : 1;
	bool m_bMoveShuffleInterrupt : 1;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId	ms_SeatShuffleClipFinishedId;
	static const fwMvBooleanId	ms_ShuffleOnEnterId;
	static const fwMvBooleanId	ms_ShuffleToEmptySeatOnEnterId;
	static const fwMvBooleanId	ms_ShuffleAndJackPedOnEnterId;
	static const fwMvBooleanId  ms_ShuffleCustomClipOnEnterId;
	static const fwMvClipId		ms_SeatShuffleClipId;
	static const fwMvFlagId		ms_FromInsideId;
	static const fwMvFlagId		ms_IsDeadId;
	static const fwMvFloatId	ms_ShuffleRateId;
	static const fwMvFloatId	ms_SeatShufflePhaseId;
	static const fwMvRequestId	ms_SeatShuffleRequestId;
	static const fwMvRequestId	ms_ShuffleToEmptySeatRequestId;
	static const fwMvRequestId	ms_ShuffleAndJackPedRequestId;
	static const fwMvStateId	ms_ShuffleStateId;
	static const fwMvStateId	ms_ShuffleToEmptySeatStateId;
	static const fwMvStateId	ms_ShuffleAndJackPedStateId;
	static const fwMvRequestId	ms_ShuffleCustomClipRequestId;
	static const fwMvClipId		ms_Clip0Id;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, IN_VEHICLE_SEAT_SHUFFLE_STATES)

#endif // !__FINAL
};

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: In vehicle shuffle seat task information for cloned peds, note this
//          is not derived from vehicle as only the state is required
////////////////////////////////////////////////////////////////////////////////
class CClonedSeatShuffleInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedSeatShuffleInfo() {}
	CClonedSeatShuffleInfo(s32 shuffleState, CVehicle *pVehicle, bool bEnteringVehicle, s32 iTargetSeatIndex);

    bool IsPedEnterExit() const { return m_bPedEnterExit; }
	s32 GetTargetSeatIndex() const { return m_iTargetSeatIndex; }

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual s32 GetTaskInfoType( ) const {return CTaskInfo::INFO_TYPE_SHUFFLE_SEAT_CLONED;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskInVehicleSeatShuffle::State_Finish>::COUNT;}

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);

        SERIALISE_BOOL(serialiser, m_bPedEnterExit);

        if(!m_bPedEnterExit)
        {
            ObjectId vehicleID = m_pVehicle.GetVehicleID();
            SERIALISE_OBJECTID(serialiser, vehicleID, "Vehicle");
		    m_pVehicle.SetVehicleID(vehicleID);
        }

		SERIALISE_INTEGER(serialiser, m_iTargetSeatIndex,  SIZEOF_SEAT_INDEX, "Seat");
    }

private:

    bool           m_bPedEnterExit;  // indicates whether this task is running as part of an enter vehicle task
    CSyncedVehicle m_pVehicle;          // target vehicle
	s32			   m_iTargetSeatIndex;

	static const unsigned int SIZEOF_SEAT_INDEX = 5;
};

#endif // INC_TASK_ENTER_VEHICLE_H
