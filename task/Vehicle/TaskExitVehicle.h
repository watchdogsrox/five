//
// Task/Vehicle/TaskExitVehicle.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_TASK_EXIT_VEHICLE_H
#define INC_TASK_EXIT_VEHICLE_H

////////////////////////////////////////////////////////////////////////////////

// Game Headers
#include "modelinfo/VehicleModelInfo.h"
#include "peds/ped.h"
#include "peds/QueriableInterface.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "VehicleAi/VehMission.h"

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CVehicle;
class CComponentReservation;
class CTaskExitVehicleSeat;
class CTaskMoveFollowNavMesh;

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Controls the characters clips whilst getting out of a vehicle
////////////////////////////////////////////////////////////////////////////////

class CTaskExitVehicle : public CTaskVehicleFSM
{
public:

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define EXIT_VEHICLE_STATES(X)	X(State_Start),							\
									X(State_WaitForCarToSlow),				\
									X(State_WaitForCarToStop),				\
									X(State_WaitForCarToSettle),			\
									X(State_DelayExitForTime),				\
									X(State_WaitForEntryToBeClearOfPeds),	\
									X(State_WaitForValidEntry),				\
									X(State_PickDoor),						\
									X(State_WaitForOtherPedToExit),			\
									X(State_ReserveSeat),					\
									X(State_ReserveDoor),					\
									X(State_ReserveShuffleSeat),			\
									X(State_StreamAssetsForShuffle),		\
									X(State_StreamAssetsForExit),			\
									X(State_ShuffleToSeat),					\
									X(State_ExitSeat),						\
									X(State_CloseDoor),						\
									X(State_ExitSeatToIdle),				\
									X(State_UnReserveSeatToIdle),			\
									X(State_UnReserveDoorToIdle),			\
									X(State_UnReserveSeatToFinish),			\
									X(State_UnReserveDoorToFinish),			\
									X(State_UnReserveSeatToClimbDown),		\
									X(State_UnReserveDoorToClimbDown),		\
									X(State_UnReserveSeatToGetUp),			\
									X(State_UnReserveDoorToGetUp),			\
									X(State_WaitForNetworkState),			\
									X(State_DetermineFleeExitPosition),		\
									X(State_WaitForFleePath),				\
									X(State_ClimbDown),						\
									X(State_SetPedOut),						\
									X(State_GetUp),							\
									X(State_Finish)

	// PURPOSE: FSM states
	enum eExitVehicleState
	{
		EXIT_VEHICLE_STATES(DEFINE_TASK_ENUM)
	};


	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_TimeSinceLastSpottedToLeaveEngineOn;
		float m_BeJackedBlendInDuration;
		float m_ExitVehicleBlendInDuration;
		float m_ThroughWindScreenBlendInDuration;
		float m_ExitVehicleBlendOutDuration;
		float m_ExitVehicleUnderWaterBlendOutDuration;
		float m_ExitVehicleAttempToFireBlendOutDuration;
		float m_FleeExitVehicleBlendOutDuration;
		float m_LeaderExitVehicleDistance;
		float m_ExitProbeDistance;
		float m_ExitDistance;
		float m_RearExitSideOffset;
		float m_MinVelocityToRagdollPed;
        float m_MaxTimeToReserveComponentBeforeWarp;
		float m_MaxTimeToReserveComponentBeforeWarpLonger;
		float m_ExtraOffsetForGroundCheck;
		u32 m_JumpOutofSubNeutralBuoyancyTime;
		fwMvClipId m_DefaultClimbDownClipId;
		fwMvClipId m_DefaultClimbDownWaterClipId;
		fwMvClipId m_DefaultClimbDownNoDoorClipId;
		fwMvClipId m_DefaultClimbDownWaterNoDoorClipId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

    friend class CTaskExitVehicleSeat;

public:

	// PURPOSE: Start the vehicle move network
	static bool StartMoveNetwork(CPed& ped, const CVehicle& vehicle, CMoveNetworkHelper& moveNetworkHelper, const fwMvClipSetId& clipsetId, s32 iEntryPointIndex, float fBlendInDuration = NORMAL_BLEND_DURATION);

	// PURPOSE: Does a line probe down and returns true if it doesn't hit anything
	static bool IsVerticalProbeClear(const Vector3& vTestPos, float fProbeLength, const CPed& ped, Vector3* pvIntersectionPos = NULL, Vector3 vOverrideEndPos = Vector3::ZeroType);
	static bool IsVehicleOnGround(const CVehicle& vehicle);

	// PURPOSE: Determines if a player is trying to aim or fire
	static bool IsPlayerTryingToAimOrFire(const CPed& rPed);

	CTaskExitVehicle(CVehicle* pVehicle, const VehicleEnterExitFlags& iRunningFlags, float fDelayTime = 0.0f, CPed* pPedJacker = NULL, s32 iTargetEntryPoint = -1);
	virtual ~CTaskExitVehicle();

	// Task required implementations
	virtual aiTask*		Copy() const						
	{ 
		CTaskExitVehicle* pExitVehicleTask = rage_new CTaskExitVehicle(m_pVehicle, m_iRunningFlags, m_fDelayTime, m_pPedJacker, m_iTargetEntryPoint);
		pExitVehicleTask->SetStreamedExitClipsetId(m_BeJackedClipsetId);
		pExitVehicleTask->SetStreamedExitClipId(m_BeJackedClipId);
		pExitVehicleTask->SetIsRappel(m_bIsRappel);
		return pExitVehicleTask; 
	}
	virtual int			GetTaskTypeInternal() const					{ return CTaskTypes::TASK_EXIT_VEHICLE; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	// Interface functions
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }

	CPed*				GetPedsJacker()						{ return m_pPedJacker; }
	const CPed*			GetPedsJacker() const				{ return m_pPedJacker; }
	const CPed*			GetDesiredTarget()					{ return m_pDesiredTarget; }
	void				SetDesiredTarget(const CPed* pTarget){ m_pDesiredTarget = pTarget; }
	void				SetIsRappel(bool bIsRappel)			{ m_bIsRappel = bIsRappel; }

	bool				GetWantsToEnterVehicle() const { return m_bWantsToEnter; }
	bool				GetWantsToEnterCover() const { return m_bWantsToGoIntoCover; }
	
	float				GetDelayTime() const { return m_fDelayTime; }
	void				SetDelayTime(float fTime) { m_fDelayTime = fTime; }
	
	void				SetMinTimeToWaitForOtherPedToExit(float fTime) { m_fMinTimeToWaitForOtherPedToExit = fTime; }

	// Clone task implementation
    virtual bool        OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return true; }
	virtual bool        OverridesCollision()		{ return GetPed()->GetAttachState() == ATTACH_STATE_PED_EXIT_CAR && GetState() != State_ClimbDown; }
    virtual bool        ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) { return GetState() <= State_WaitForOtherPedToExit; }
	virtual bool		OverridesVehicleState() const	{ return true; }
    virtual bool        AllowsLocalHitReactions() const { return false; }
	virtual CTaskInfo*	CreateQueriableState() const;
	virtual void		ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void        OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return	UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual bool		HandlesDeadPed();
    virtual bool        CanBeReplacedByCloneTask(u32 taskType) const;
	virtual bool 		IsInScope(const CPed* pPed);
    // This function returns whether changes from the specified state
	// are managed by the clone FSM update, if not the state must be
	// kept in sync with the network update
	bool				StateChangeHandledByCloneFSM(s32 iState);

	//PURPOSE: Checks whether we can update the local task state based on the last state received over the network. Used when running as a clone task only.
	bool				CanUpdateStateFromNetwork();

	void				SetRunningFlag(CVehicleEnterExitFlags::Flags flag)  { m_iRunningFlags.BitSet().Set(flag); }
	bool				IsRunningFlagSet(CVehicleEnterExitFlags::Flags flag) const { return m_iRunningFlags.BitSet().IsSet(flag); }
	bool				IsExiting() { return GetState() == State_ExitSeat; }
	bool				ShouldInterruptDueToSequenceTask() const;

	// PURPOSE: DAN TEMP - function for ensuring a valid clip transition is available based on the current move network state
	// this will be replaced when the move network interface is extended to include this
	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;

	void				SetStreamedExitClipsetId(const fwMvClipSetId& clipsetId) { m_BeJackedClipsetId = clipsetId; }
	const fwMvClipSetId& GetStreamedExitClipsetId() const { return m_BeJackedClipsetId; }

	void				SetStreamedExitClipId(const fwMvClipId& clipId) { m_BeJackedClipId = clipId; }
	const fwMvClipId&	GetStreamedExitClipId() const { return m_BeJackedClipId; }

	bool				IsConsideredGettingOutOfVehicleForCamera(float minExitSeatPhaseForCameraExit) const;
	Vec3V_ConstRef		GetSeatOffset() const { return m_vSeatOffset; }
	bool				HasValidExitPosition() const { return m_bHasValidExitPosition; }
	Vec3V_Out			GetExitPosition() const;
	CCarDoor *			GetDoor();
	bool				IsDoorOpen(float fMinRatio) const;
	// PURPOSE: Checks to see if we will exit into the air, if so try to trigger the jump and then force into nm 
	bool				CheckForInAirExit(const Vector3& vTestPos) const;
	bool				CheckForInAirExit(const Vector3& vTestPos, float fProbeLength) const;
	bool				CheckForUnderwater(const Vector3& vTestPos);
	void				SetFleeExitPosition(const Vector3& vFleePos) { m_vFleeExitPosition = vFleePos; }
	Vector3				GetFleeExitPosition() const { return m_vFleeExitPosition; }

	static void			OnExitMidAir(CPed* pPed, CVehicle* pVehicle);

	static void			TriggerFallEvent(CPed *ped, s32 nFlags = 0);

	s32					GetTimeStartedExitTask() const { return m_uTimeStartedExitTask; }

	//Purpose: Returns the appropriate rappel clipset based on the heli's seat index.
	static fwMvClipSetId SelectRappelClipsetForHeliSeat(CVehicle* pVehicle, s32 iSeatIndex);

private:

	// PURPOSE: Should the ped wait for peds potentially blocking the entry point to move away?
	bool				PedShouldWaitForPedsToMove();
	bool				IsPedConsideredTooClose(Vec3V_ConstRef vPedPos, const CPed& rOtherPed);

	bool				ShouldWaitForCarToSettle() const;

	bool				GetWillClosedoor();
	

	float				GetBlendInDuration() const;

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPostFSM();

	// PURPOSE: Manipulate the animated velocity
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE:	Communicate with Move
	virtual bool		ProcessMoveSignals();

	// FSM optional functions
	virtual	void		CleanUp			();
	virtual bool		ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);
	
	bool				ShouldAbortFromInAirEvent(const aiEvent* pEvent) const;

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void		DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// Local state function calls
	void				Start_OnEnter();
    void				Start_OnEnterClone();
	FSM_Return			Start_OnUpdate();
    FSM_Return			Start_OnUpdateClone();
	void				WaitForCarToSlow_OnEnter();
	FSM_Return			WaitForCarToSlow_OnUpdate();
	FSM_Return			WaitForEntryToBeClearOfPeds_OnUpdate();
	FSM_Return			WaitForValidEntry_OnUpdate();
	void				DelayExitForTime_OnEnter();
	FSM_Return			DelayExitForTime_OnUpdate();
	FSM_Return			PickDoor_OnEnter();
	FSM_Return			PickDoor_OnUpdate();
	FSM_Return			WaitForOtherPedToExit_OnUpdate();
	void				WaitForCarToStop_OnEnter();
	FSM_Return			WaitForCarToStop_OnUpdate();
	FSM_Return			WaitForCarToSettle_OnEnter();
	FSM_Return			WaitForCarToSettle_OnUpdate();
	void				ShuffleToSeat_OnEnter();
	FSM_Return			ShuffleToSeat_OnUpdate();
	FSM_Return			ReserveSeat_OnUpdate();
	FSM_Return			ReserveShuffleSeat_OnUpdate();
	FSM_Return			StreamAssetsForShuffle_OnUpdate();
	FSM_Return			StreamAssetsForExit_OnUpdate();
	FSM_Return			ReserveDoor_OnUpdate();
	FSM_Return			ExitSeat_OnEnter();
	FSM_Return			ExitSeat_OnUpdate();
	FSM_Return			ExitSeat_OnExit();
	void				SetPedOut_OnEnter();
	FSM_Return			SetPedOut_OnUpdate();
	void				CloseDoor_OnEnter();
	FSM_Return			CloseDoor_OnUpdate();
	FSM_Return			CloseDoor_OnExit();
	FSM_Return			ExitSeatToIdle_OnEnter();
	FSM_Return			ExitSeatToIdle_OnUpdate();
	FSM_Return			UnReserveSeatToIdle_OnUpdate();
	FSM_Return			UnReserveDoorToIdle_OnUpdate();
	FSM_Return			UnReserveSeatToFinish_OnUpdate();
	FSM_Return			UnReserveDoorToFinish_OnUpdate();
	FSM_Return			UnReserveSeatToClimbDown_OnUpdate();
	FSM_Return			UnReserveDoorToClimbDown_OnUpdate();
	FSM_Return			UnReserveSeatToGetUp_OnUpdate();
	FSM_Return			UnReserveDoorToGetUp_OnUpdate();
	FSM_Return			WaitForNetworkState_OnUpdate();
	FSM_Return			ClimbDown_OnEnter();
	FSM_Return			ClimbDown_OnUpdate();
	FSM_Return			GetUp_OnEnter();
	FSM_Return			GetUp_OnUpdate();
	FSM_Return			DetermineFleeExitPosition_OnUpdate();
	FSM_Return			WaitForFleePath_OnUpdate();
    void                Finish_OnEnterClone();
	FSM_Return			Finish_OnUpdate();
	FSM_Return			Finish_OnUpdateClone();

	void				AssignVehicleTask();

	const fwMvClipSetId GetMoveNetworkClipSet(bool bUseDirectEntryPointIndex = false);
	bool				 RequestClips();

	// PURPOSE: Checks to see if we should exit upside down
	bool				CheckForUpsideDownExit() const;
	bool				CheckForOnsideExit() const;
	bool				CheckIfFleeExitIsValid() const;
	bool				CheckIsRouteValidForFlee(const CTaskMoveFollowNavMesh* pNavTask) const;

	bool				CheckForVehicleInWater() const;
	bool				CheckForClimbDown() const;

	void				UpdateAttachOffsetsFromCurrentPosition(CPed& ped);

	// PURPOSE: Set the clip playback rate
	void				SetClipPlaybackRate();
	bool				SetClipsForState();

	// PURPOSE: Stores the offset to the target
	const crClip*		GetClipAndPhaseForState(float& fPhase) const;
	void				StoreInitialOffsets();
	bool				ShouldUseGetOutInitialOffset() const;
	void				ComputeGetOutInitialOffset(Vector3& vOffset, Quaternion& qOffset);
	void				ComputeClimbDownTarget(Vector3& vOffset, Quaternion& qOffset);
	bool				DoesProbeHitSomething(const Vector3& vStart, const Vector3& vEnd);
	s32					GetDirectEntryPointForPed() const;
	Vector3				ComputeExitPosition(const Vector3& vEntryPosition, const float fLength, const float fAngle, const bool bRunToRear, const Vector3& vSide);
	bool				CheckForTransitionToRagdollState(CPed& rPed, CVehicle& rVeh);
	bool				ShouldRagdollDueToReversing(CPed& rPed, CVehicle& rVeh);
	static bool			CheckForObstructions(const CEntity* pEntity,const Vector3& vStart, const Vector3& vEnd);
	bool				IsPlaneHatchEntry(const s32 entryIndex) const;

	// PURPOSE: Returns flags which are passed in to CPed::AttachPedToEnterCar
	u32			GetAttachToVehicleFlags(const CVehicle* pVehicle);

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	Vec3V					m_vSeatOffset;
	Vec3V					m_vExitPosition;
	Vector3					m_vFleeExitPosition;
	Vector3					m_vCachedClipTranslation;
	Quaternion				m_qCachedClipRotation;

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper		m_moveNetworkHelper;
	float					m_fDelayTime;
	float					m_fMinTimeToWaitForOtherPedToExit;
	RegdPed					m_pPedJacker;
	RegdConstPed			m_pDesiredTarget;

	u32						m_uLastTimeUnsettled;

    // DAN TEMP - variable for ensuring a valid clip transition is available based on the current move network state
    s32                     m_iLastCompletedState;

	s32						m_iNumTimesInPickDoorState;
	float					m_fTimeSinceSubTaskFinished;

	u32						m_uTimeStartedExitTask;

	float					m_fLegIkBlendInPhase;
	float					m_fDesiredFleeExitAngleOffset;
	float					m_fCachedGroundHeight;
	bool					m_bVehicleTaskAssigned;
	bool					m_bIsRappel;
	bool					m_bAimExit;
	bool					m_bWantsToEnter;
	bool					m_bCurrentAnimFinished;
	bool					m_bWantsToGoIntoCover;
    bool                    m_bHasClosedDoor;
	bool					m_bHasValidExitPosition;
	bool					m_bSetInactiveCollidesAgainstInactive;

	bool					m_bSubTaskExitRateSet : 1;
	bool					m_bCloneShouldUseCombatExitRate :1;

	u8						m_nFramesWaited;
	fwMvClipSetId			m_BeJackedClipsetId;
	fwMvClipId				m_BeJackedClipId;
	CPlayAttachedClipHelper	m_PlayAttachedClipHelper;

	static const fwMvRequestId		ms_ClimbDownRequestId;
	static const fwMvRequestId		ms_ToIdleRequestId;
	static const fwMvBooleanId		ms_ToIdleOnEnterId;
	static const fwMvBooleanId		ms_ClimbDownOnEnterId;
	static const fwMvBooleanId		ms_ToIdleAnimFinishedId;
	static const fwMvBooleanId		ms_ClimbDownAnimFinishedId;
	static const fwMvBooleanId		ms_DeathInSeatInterruptId;
	static const fwMvBooleanId		ms_DeathOutOfSeatInterruptId;
	static const fwMvBooleanId		ms_ClimbDownInterruptId;
	static const fwMvFloatId		ms_ToIdleAnimPhaseId;
	static const fwMvFloatId		ms_ClimbDownAnimPhaseId;
	static const fwMvFloatId		ms_ClimbDownRateId;
	static const fwMvFloatId		ms_ClosesDoorBlendDurationId;
	static const fwMvStateId		ms_ToIdleStateId;
	static const fwMvBooleanId		ms_NoDoorInterruptId;
	static const fwMvStateId		ms_ClimbDownStateId;
	static const fwMvClipId			ms_ClimbDownClipId;
	static const fwMvClipId			ms_ToIdleClipId;

	// PURPOSE: Handles the exit of peds when the entry point has the warp flag and we want to force immediate skydive
	static void			ProcessForcedSkyDiveExitWarp(CVehicle& rVeh, s32 iEntryPoint, u32& iFlags);

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, EXIT_VEHICLE_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

class CClonedExitVehicleInfo: public CClonedVehicleFSMInfo
{
public:

    enum
    {
        EXIT_SEAT_STILL,
        EXIT_SEAT_WALK,
        EXIT_SEAT_RUN
    };

	CClonedExitVehicleInfo();
	CClonedExitVehicleInfo( s32 enterState, 
							CVehicle* pVehicle, 
							SeatRequestType iSeatRequestType, 
							s32 iSeat, 
							s32 iTargetEntryPoint, 
							VehicleEnterExitFlags iRunningFlags, 
							CPed* pForcedOutPed, 
							unsigned exitSeatMotionState,
							const fwMvClipSetId &streamedClipSetId,
							const fwMvClipId & streamedClipId,
                            bool hasClosedDoor,
                            bool isRappel,
							bool subTaskExitRateSet,
							bool shouldUseCombatExitRate,
							Vector3 vFleeExitTargetNodePosition);
	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual s32 GetTaskInfoType( ) const {return CTaskInfo::INFO_TYPE_EXIT_VEHICLE_CLONED;}

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskExitVehicle::State_Finish>::COUNT;}

	virtual s32		GetTargetEntryExitPoint() const	{ return m_iTargetEntryPoint; }

    bool            UseWalkStart() const { return m_ExitSeatMotionState == EXIT_SEAT_WALK;}
    bool            UseRunStart()  const { return m_ExitSeatMotionState == EXIT_SEAT_RUN;}
    bool            HasClosedDoor() const { return m_HasClosedDoor; }
    bool            IsRappel() const { return m_IsRappel; }
	bool			SubTaskExitRateSet() const { return m_SubTaskExitRateSet; }
	bool			ShouldUseCombatExitRate() const { return m_ShouldUseCombatExitRate; }
	CPed*			GetPedsJacker() const { return m_pPedJacker.GetPed(); }
	fwMvClipSetId	GetStreamedClipSetId() const { return m_StreamedExitClipSetId; }
	fwMvClipId		GetStreamedClipId() const { return m_StreamedExitClipId; }
	Vector3			GetFleeTargetPosition() const { return m_FleeExitTargetPosition; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CClonedVehicleFSMInfo::Serialise(serialiser);

        const unsigned SIZE_OF_EXIT_SEAT_MOTION_STATE = 2;

        u16 exitSeatMotionState = m_ExitSeatMotionState;
        SERIALISE_UNSIGNED(serialiser, exitSeatMotionState, SIZE_OF_EXIT_SEAT_MOTION_STATE, "Exit Seat Motion State");
        m_ExitSeatMotionState = exitSeatMotionState;

        bool hasClosedDoor = m_HasClosedDoor;
        SERIALISE_BOOL(serialiser, hasClosedDoor, "Has Closed Door");
        m_HasClosedDoor = hasClosedDoor;

		bool bIsRappel = m_IsRappel;
        SERIALISE_BOOL(serialiser, bIsRappel, "Is Rappel");
		m_IsRappel = bIsRappel;

		bool bShouldUseCombatExitRates = m_ShouldUseCombatExitRate;
		SERIALISE_BOOL(serialiser, bShouldUseCombatExitRates, "Should use combat exit rate");
		m_ShouldUseCombatExitRate = bShouldUseCombatExitRates;

		bool bSubTaskExitRateSet = m_SubTaskExitRateSet;
		SERIALISE_BOOL(serialiser, bSubTaskExitRateSet, "Sub task has set the exit rate");
		m_SubTaskExitRateSet = bSubTaskExitRateSet;

		ObjectId targetID = m_pPedJacker.GetPedID();
        SERIALISE_OBJECTID(serialiser, targetID);
		m_pPedJacker.SetPedID(targetID);
		SERIALISE_ANIMATION_CLIP(serialiser, m_StreamedExitClipSetId, m_StreamedExitClipId, "Streamed Animation");

		SERIALISE_POSITION(serialiser, m_FleeExitTargetPosition, "Flee Target Position");
	}

private:

	fwMvClipSetId  m_StreamedExitClipSetId;
	fwMvClipId     m_StreamedExitClipId;
    CSyncedPed     m_pPedJacker;
	Vector3		   m_FleeExitTargetPosition;
    u16            m_ExitSeatMotionState : 15;
    u16            m_HasClosedDoor       : 1;

	bool           m_IsRappel : 1;
	bool           m_SubTaskExitRateSet : 1;
	bool           m_ShouldUseCombatExitRate : 1;
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Subtask to control open door clips
////////////////////////////////////////////////////////////////////////////////

class CTaskOpenVehicleDoorFromInside : public CTask
{
public:

	// FSM states
	enum eExitVehicleOpenDoorState
	{
		State_Start,
		State_OpenDoor,
		State_Finish
	};

public:

	CTaskOpenVehicleDoorFromInside(CVehicle* pVehicle, s32 iExitPointIndex);
	virtual ~CTaskOpenVehicleDoorFromInside();

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskOpenVehicleDoorFromInside(m_pVehicle, m_iExitPointIndex); }
	virtual int			GetTaskTypeInternal() const					{ return CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_INSIDE; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPostFSM();

	// FSM optional functions
	virtual	void		CleanUp			();

	// Local state function calls
	FSM_Return			Start_OnUpdate();
	void				OpenDoor_OnEnter();
	FSM_Return			OpenDoor_OnUpdate();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	void				DriveOpenDoorFromClip(const crClip* pOpenDoorClip, const float fOpenDoorPhase);

private:

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper*		m_pParentNetworkHelper;
	RegdVeh					m_pVehicle;
	s32						m_iExitPointIndex;

	static const fwMvBooleanId ms_OpenDoorClipFinishedId;
	static const fwMvClipId ms_OpenDoorClipId;
	static const fwMvFloatId ms_OpenDoorPhaseId;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Subtask to control exiting a vehicle seat
////////////////////////////////////////////////////////////////////////////////

class CTaskExitVehicleSeat : public CTask
{
public:

	static bool sm_bCloneVehicleFleeExitFix;
	static void InitTunables(); 
	static const fwMvBooleanId ms_NoWingEarlyRagdollId;
	static const fwMvBooleanId ms_DetachId;
	static const fwMvBooleanId ms_AimIntroInterruptId;

	// PURPOSE: Determine if this task is valid
	static bool IsTaskValid(const CVehicle* pVehicle, s32 iExitPointIndex);

	// PURPOSE: Get the correct clipsetid which contains the exit anims
	static fwMvClipSetId GetExitClipsetId(const CVehicle* pVehicle, s32 iExitPointIndex, const VehicleEnterExitFlags &flags, const CPed* pPed = NULL);

	enum eSeatPosition
	{
		SF_FrontDriverSide = 0,
		SF_FrontPassengerSide,
		SF_BackDriverSide,
		SF_BackPassengerSide
	};

	// ClipIds are defined the in the Vehicle metadata, chosen based on vehicle type and seat location
	struct ExitToAimSeatInfo
	{
		atHashString		m_ExitToAimClipsName;
		atHashString		m_OneHandedClipSetName;
		atHashString		m_TwoHandedClipSetName;
		eSeatPosition		m_SeatPosition;
		PAR_SIMPLE_PARSABLE;
	};

	struct ExitToAimClipSet
	{
		atHashString m_Name;
		atArray<fwMvClipId> m_Clips;
		PAR_SIMPLE_PARSABLE;
	};

	struct ExitToAimVehicleInfo
	{
		atHashString m_Name;
		atArray<ExitToAimSeatInfo> m_Seats;
		PAR_SIMPLE_PARSABLE;
	};

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		// ---	BEGIN_DEV_TO_REMOVE	--- //			// NOTES:

		// ---	END_DEV_TO_REMOVE	--- //

		float m_MinTimeInStateToAllowRagdollFleeExit;
		float m_AdditionalWindscreenRagdollForceFwd;
		float m_AdditionalWindscreenRagdollForceUp;
		float m_SkyDiveProbeDistance;
		float m_InAirProbeDistance;
		float m_ArrestProbeDistance;
		float m_InWaterExitDepth;
		float m_InWaterExitProbeLength;
		float m_BikeVelocityToUseAnimatedJumpOff;
		float m_BicycleVelocityToUseAnimatedJumpOff;
		float m_BikeExitForce;
		float m_RagdollIntoWaterVelocity;
		float m_GroundFixupHeight;
		float m_GroundFixupHeightLarge;
		float m_GroundFixupHeightLargeOffset;
		float m_GroundFixupHeightBoatInWaterInitial;
		float m_GroundFixupHeightBoatInWater;
		float m_ExtraWaterZGroundFixup;
		float m_FleeExitExtraRotationSpeed;
		float m_FleeExitExtraTranslationSpeed;
		float m_DefaultGetOutBlendDuration;
		float m_DefaultGetOutNoWindBlendDuration;
		float m_MaxTimeForArrestBreakout;
		float m_ThroughWindscreenDamagePlayer;
		float m_ThroughWindscreenDamageAi;

		fwMvClipId m_DefaultCrashExitOnSideClipId;
		fwMvClipId m_DefaultBeJackedAlivePedFromOutsideAltClipId;
		fwMvClipId m_DefaultBeJackedAlivePedFromOutsideClipId;
		fwMvClipId m_DefaultBeJackedDeadPedFromOutsideClipId;
		fwMvClipId m_DefaultBeJackedDeadPedFromOutsideAltClipId;
		fwMvClipId m_DefaultBeJackedAlivePedFromWaterClipId;
		fwMvClipId m_DefaultBeJackedDeadPedFromWaterClipId;
		fwMvClipId m_DefaultBeJackedAlivePedOnVehicleClipId;
		fwMvClipId m_DefaultBeJackedDeadPedOnVehicleClipId;
		fwMvClipId m_DefaultBeJackedAlivePedOnVehicleIntoWaterClipId;
		fwMvClipId m_DefaultBeJackedDeadPedOnVehicleIntoWaterClipId;
		fwMvClipId m_DefaultFleeExitClipId;
		fwMvClipId m_DefaultGetOutClipId;
		fwMvClipId m_DefaultGetOutToWaterClipId;
		fwMvClipId m_DefaultGetOutOnToVehicleClipId;
		fwMvClipId m_DefaultGetOutNoWingId;
		fwMvClipId m_DefaultJumpOutClipId;
		fwMvClipId m_DeadFallOutClipId;

		atArray<ExitToAimClipSet> m_ExitToAimClipSets;
		atArray<ExitToAimVehicleInfo> m_ExitToAimVehicleInfos;

		fwMvClipId m_CustomShuffleBeJackedDeadClipId;
		fwMvClipId m_CustomShuffleBeJackedClipId;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define EXIT_VEHICLE_SEAT_STATES(X)	X(State_Start),						\
										X(State_StreamAssets),				\
										X(State_ExitSeat),					\
										X(State_FleeExit),					\
										X(State_JumpOutOfSeat),				\
										X(State_ExitToAim),					\
										X(State_ThroughWindscreen),			\
										X(State_DetachedFromVehicle),		\
										X(State_BeJacked),					\
										X(State_WaitForStreamedBeJacked),	\
										X(State_WaitForStreamedExit),		\
										X(State_StreamedExit),				\
										X(State_StreamedBeJacked),			\
										X(State_OnSideExit),				\
										X(State_UpsideDownExit),			\
										X(State_Finish)

	// PURPOSE: FSM states
	enum eExitVehicleSeatState
	{
		EXIT_VEHICLE_SEAT_STATES(DEFINE_TASK_ENUM)
	};

public:

	CTaskExitVehicleSeat(CVehicle* pVehicle, s32 iExitPointIndex, VehicleEnterExitFlags iRunningFlags, bool bIsRappel);
	virtual ~CTaskExitVehicleSeat();

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskExitVehicleSeat(m_pVehicle, m_iExitPointIndex, m_iRunningFlags, m_bIsRappel); }
	virtual int			GetTaskTypeInternal() const					{ return CTaskTypes::TASK_EXIT_VEHICLE_SEAT; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return m_pParentNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return m_pParentNetworkHelper; }

	bool IsBeingArrested() const { return IsRunningFlagSet(CVehicleEnterExitFlags::IsArrest); }
	bool CanBreakOutOfArrest() const { return m_bCanBreakoutOfArrest; }
	
	bool	IsRunningFlagSet(CVehicleEnterExitFlags::Flags flag) const { return m_iRunningFlags.BitSet().IsSet(flag); }
	void	SetRunningFlag(CVehicleEnterExitFlags::Flags flag) { m_iRunningFlags.BitSet().Set(flag); }
	void	ClearRunningFlag(CVehicleEnterExitFlags::Flags flag) { m_iRunningFlags.BitSet().Clear(flag); }
	bool	WantsToEnter() const { return m_bWantsToEnter; }
	const crClip*		GetClipAndPhaseForState(float& fPhase) const;
	bool	HasPassedInterruptTimeForCamera(float fTimeOffset) const;
	float	GetFleeExitClipHeadingChange() const { return m_fFleeExitClipHeadingChange; }

	bool GetCloneShouldUseCombatExitRate() {return m_bCloneShouldUseCombatExitRate;}
	void SetCloneShouldUseCombatExitRate(bool bCloneShouldUseCombatExitRate) {m_bCloneShouldUseCombatExitRate = bCloneShouldUseCombatExitRate;}

	static const ExitToAimSeatInfo* GetExitToAimSeatInfo(const CVehicle* pVehicle, int iSeatIndex, int iExitPointIndex);
	static const ExitToAimClipSet* GetExitToAimClipSet(atHashString clipSetName);

	static bool			CheckForSkyDive(const CPed* pPed, const CVehicle *pVehicle, s32 nEntryPoint, bool bAllowDiveIntoWater = true);
	static bool			CheckForWaterUnderPed(const CPed* pPed, const CVehicle *pVehicle, float fProbeLength, float fWaterDepth, float &fWaterHeightOut, bool bConsiderWaves = true, Vector3 *pvOverridePos = NULL);
	static bool			CheckForDiveIntoWater(const CPed *pPed, const CVehicle *pVehicle, bool bCanParachute, float& fHeightOverWater);

	void SetPreserveMatrixOnDetach(bool bPreserve) { m_bPreserveMatrixOnDetach = bPreserve; }

private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return	ProcessPostFSM();

	// PURPOSE: Manipulate the animated velocity
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE:	Communicate with Move
	virtual bool		ProcessMoveSignals();

	// FSM optional functions
	virtual	void		CleanUp			();
	virtual bool		HandlesDeadPed() { return true; }

	// Local state function calls
	FSM_Return			Start_OnUpdate();
	FSM_Return			StreamAssets_OnUpdate();
	void				ExitSeat_OnEnter();
	FSM_Return			ExitSeat_OnUpdate();
	FSM_Return			ExitSeat_OnExit();
	FSM_Return			FleeExit_OnEnter();
	FSM_Return			FleeExit_OnUpdate();
	FSM_Return			FleeExit_OnExit();
	void				JumpOutOfSeat_OnEnter();
	FSM_Return			JumpOutOfSeat_OnUpdate();
	void				ExitToAim_OnEnter();
	FSM_Return			ExitToAim_OnUpdate();
	FSM_Return			ExitToAim_OnExit();
	FSM_Return			ThroughWindscreen_OnEnter();
	FSM_Return			ThroughWindscreen_OnUpdate();
	void				DetachedFromVehicle_OnEnter();
	FSM_Return			DetachedFromVehicle_OnUpdate();
	FSM_Return			BeJacked_OnEnter();
	FSM_Return			BeJacked_OnUpdate();
	FSM_Return			WaitForStreamedBeJacked_OnUpdateClone();
	FSM_Return			WaitForStreamedExit_OnUpdateClone();
	FSM_Return			StreamedBeJacked_OnEnter();
	FSM_Return			StreamedBeJacked_OnUpdate();
	FSM_Return			StreamedExit_OnEnter();
	FSM_Return			StreamedExit_OnUpdate();
	FSM_Return			OnSideExit_OnEnter();
	FSM_Return			OnSideExit_OnUpdate();
	FSM_Return			UpsideDownExit_OnEnter();
	FSM_Return			UpsideDownExit_OnUpdate();
	FSM_Return			Finish_OnUpdate();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	void				DriveOpenDoorFromClip(const crClip* pOpenDoorClip, const float fOpenDoorPhase);
	void				ProcessOpenDoor(bool bProcessOnlyIfTagsFound, bool bDriveRearDoorClosed = false);
	void				ProcessSetPedOutOfSeat();
	bool				ProcessExitSeatInterrupt(CPed& rPed);
	void				UpdateTarget();
	bool				CheckStreamedExitConditions();
	bool				GetIsHighInAir(CPed* pPed);
	float				CalculateAimDirection();
	bool				IsPedInCombatOrFleeing(const CPed& ped) const;
	s32					DecideInitialState();
	void				ProcessWeaponAttachPoint();
	bool				ShouldProcessWeaponAttachPoint() const;
	void                ApplyDamageThroughWindscreen();
	bool				RequestClips(s32 iDesiredState);
	void				SetUpRagdollOnCollision(CPed& rPed, CVehicle& rVeh);
	void				ResetRagdollOnCollision(CPed& rPed);
	bool				SendMoveSignalsForExitToAim();
	void				RequestProcessPhysics();
	bool				ShouldBeConsideredUnderWater();
	bool				IsStandingOnVehicle(const CPed& rPed) const;
	void				TurnTurretForBeingJacked(CPed& rPed, CPed* pJacker);
	bool				DoesClonePedWantToInterruptExitSeatAnim(const CPed& rPed);

	// PURPOSE: Returns if this task state should fixup the mover to the target
	bool ShouldApplyTargetFixup() const;

	// PURPOSE: Stores the offset to the target
	void StoreInitialOffsets();

	// PURPOSE: Set the clip playback rate
	bool SetClipsForState();
	void SetClipPlaybackRate();

	// PURPOSE: Checks if the ped possibly went through something when getting thrown and fixes it
	void CheckAndFixIfPedWentThroughSomething();

	void GetAlternateClipInfoForPed(const CPed& jackingPed, fwMvClipSetId& clipSetId, fwMvClipId& clipId, bool& bForceFlee) const;

	float GetGroundFixupHeight(bool bUseLargerGroundFixup = false) const;

	void ControlledSetPedOutOfVehicle(u32 nFlags);

	bool IsGoingToExitOnToVehicle() const;

	static bool IsEjectorSeat(const CVehicle *pVehicle);

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper*	m_pParentNetworkHelper;
	RegdVeh				m_pVehicle;
	s32					m_iExitPointIndex;
	s32					m_iSeatIndex;
	bool				m_bExitedSeat;
	bool				m_bIsRappel;
	bool				m_bFoundGroundInFixup;
	VehicleEnterExitFlags	m_iRunningFlags;
	Vector3				m_vLastUpsideDownPosition;
	Vec3V				m_vIdealTargetPosCached;
	QuatV				m_qIdealTargetRotCached;
	float				m_fInterruptPhase;
	float				m_fCachedGroundFixUpHeight;
	float				m_fLastAimDirection;
	float				m_fAimBlendReferenceHeading;
	float				m_fAimBlendCurrentHeading;
	float				m_fExitToAimDesiredHeading;
	float				m_fAimInterruptPhase;
	float				m_fUpperBodyAimPhase;
	float				m_fStartRollOrientationFixupPhase;
	float				m_fEndRollOrientationFixupPhase;
	float				m_fStartOrientationFixupPhase;
	float				m_fEndOrientationFixupPhase;
	float				m_fStartTranslationFixupPhase;
	float				m_fEndTranslationFixupPhase;
	float				m_fFleeExitClipHeadingChange;
	float				m_fFleeExitExtraZChange;
	float				m_fExtraHeadingChange;
	float				m_fStartRotationFixUpPhase;
	float				m_fLegIkBlendInPhase;
    float               m_fExitSeatClipPlaybackRate;
	s32					m_iDominantAimClip;
	bool				m_bProcessPhysicsCalledThisFrame;
	Quaternion			m_qThisFramesOrientationDelta;
	bool				m_bWantsToEnter : 1;
	bool				m_bMoveNetworkFinished : 1;		// Used by ProcessMoveSignals() during certain states.
	bool 				m_bWillBlendOutLeftHandleIk : 1;
	bool 				m_bWillBlendOutRightHandleIk : 1;
	bool				m_bHasProcessedWeaponAttachPoint : 1;
	bool				m_bComputedIdealTargets : 1;
	bool				m_bGroundFixupApplied : 1;
	bool				m_bRagdollEventGiven : 1;
	bool				m_bHasAdjustedForRoute : 1;
	bool				m_bCanBreakoutOfArrest : 1;
	bool				m_bPreserveMatrixOnDetach : 1;
	bool				m_bHasIkAllowTags : 1;
	bool				m_bWasInWaterAtBeginningOfJumpOut : 1;
	bool				m_bCloneShouldUseCombatExitRate : 1;
	bool				m_bHasTurnedTurretDueToBeingJacked : 1;
	bool				m_bNoExitAnimFound : 1;

	CPlayAttachedClipHelper	m_PlayAttachedClipHelper;

    ////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network ids corresponding to control parameters in the move network
	static const fwMvBooleanId ms_ExitSeatOnEnterId;
	static const fwMvBooleanId ms_FleeExitOnEnterId;
	static const fwMvBooleanId ms_BeJackedOnEnterId;
	static const fwMvBooleanId ms_StreamedExitOnEnterId;
	static const fwMvBooleanId ms_StreamedBeJackedOnEnterId;
	static const fwMvBooleanId ms_JumpOutOnEnterId;
	static const fwMvBooleanId ms_ThroughWindscreenOnEnterId;
	static const fwMvBooleanId ms_UpsideDownExitOnEnterId;
	static const fwMvBooleanId ms_OnSideExitOnEnterId;
	static const fwMvBooleanId ms_BeJackedClipFinishedId;
	static const fwMvBooleanId ms_StreamedExitClipFinishedId;
	static const fwMvBooleanId ms_StreamedBeJackedClipFinishedId;
	static const fwMvBooleanId ms_StandUpInterruptId;
	static const fwMvBooleanId ms_ExitSeatClipFinishedId;
	static const fwMvBooleanId ms_FleeExitAnimFinishedId;
	static const fwMvBooleanId ms_JumpOutClipFinishedId;
	static const fwMvBooleanId ms_ThroughWindscreenClipFinishedId;
	static const fwMvBooleanId ms_InterruptId;
	static const fwMvBooleanId ms_ExitSeatInterruptId;
	static const fwMvBooleanId ms_LegIkActiveId;
	static const fwMvBooleanId ms_BlendInUpperBodyAimId;
	static const fwMvBooleanId ms_ArrestedId;
	static const fwMvBooleanId ms_PreventBreakoutId;
	static const fwMvBooleanId ms_ExitToSkyDiveInterruptId;
	static const fwMvBooleanId ms_BlendBackToSeatPositionId;
	static const fwMvBooleanId ms_AllowedToAbortForInAirEventId;
	static const fwMvClipId ms_StreamedExitClipId;
	static const fwMvClipId ms_ExitSeatClipId;
	static const fwMvClipId ms_FleeExitClipId;
	static const fwMvClipId ms_JumpOutClipId;
	static const fwMvClipId ms_ThroughWindscreenClipId;
	static const fwMvClipId ms_StreamedBeJackedClipId;
	static const fwMvClipId	ms_ExitToAimLeft180ClipId;
	static const fwMvClipId	ms_ExitToAimLeft90ClipId;
	static const fwMvClipId	ms_ExitToAimFrontClipId;
	static const fwMvClipId	ms_ExitToAimRight90ClipId;
	static const fwMvClipId	ms_ExitToAimRight180ClipId;
	static const fwMvClipSetVarId ms_AdditionalAnimsClipSet;
	static const fwMvFlagId ms_FromInsideId;
	static const fwMvFlagId ms_IsDeadId;
	static const fwMvFlagId ms_WalkStartFlagId;
	static const fwMvFlagId ms_ToWaterFlagId;
	static const fwMvFlagId ms_ToAimFlagId;
	static const fwMvFlagId ms_NoWingFlagId;
	static const fwMvFloatId ms_ExitSeatPhaseId;
	static const fwMvFloatId ms_FleeExitPhaseId;
	static const fwMvFloatId ms_JumpOutPhaseId;
	static const fwMvFloatId ms_StreamedExitPhaseId;
	static const fwMvFloatId ms_StreamedBeJackedPhaseId;
	static const fwMvFloatId ms_ThroughWindscreenPhaseId;
	static const fwMvFloatId ms_ToLocomotionPhaseId;
	static const fwMvFloatId ms_ToLocomotionPhaseOutId;
	static const fwMvFloatId ms_AimBlendDirectionId;
	static const fwMvFloatId ms_AimPitchId;
	static const fwMvFloatId ms_BeJackedRateId;
	static const fwMvFloatId ms_ExitSeatRateId;
	static const fwMvFloatId ms_FleeExitRateId;
	static const fwMvFloatId ms_StreamedExitRateId;
	static const fwMvFloatId ms_ExitSeatBlendDurationId;
	static const fwMvRequestId ms_AimRequest;
    static const fwMvRequestId ms_ExitSeatRequestId;
	static const fwMvRequestId ms_FleeExitRequestId;
	static const fwMvRequestId ms_ToLocomotionRequestId;
	static const fwMvRequestId ms_JumpOutRequestId;
	static const fwMvRequestId ms_ThroughWindscreenRequestId;
	static const fwMvRequestId ms_BeJackedRequestId;
	static const fwMvRequestId ms_StreamedExitRequestId;
	static const fwMvRequestId ms_StreamedBeJackedRequestId;
	static const fwMvRequestId ms_UpsideDownExitRequestId;
	static const fwMvRequestId ms_OnSideExitRequestId;
	static const fwMvRequestId ms_BeJackedCustomClipRequestId;
	static const fwMvStateId ms_ExitSeatStateId;
	static const fwMvStateId ms_FleeExitStateId;
	static const fwMvStateId ms_ToLocomotionStateId;
	static const fwMvStateId ms_BeJackedStateId;
	static const fwMvStateId ms_StreamedExitStateId;
	static const fwMvStateId ms_StreamedBeJackedStateId;
	static const fwMvStateId ms_JumpOutStateId;
	static const fwMvStateId ms_ThroughWindscreenStateId;
	static const fwMvStateId ms_UpsideDownExitStateId;
	static const fwMvStateId ms_OnSideExitStateId;
	static const fwMvClipId	ms_Clip0Id;

	static bank_float ms_fDefaultMinWalkInterruptPhase;
	static bank_float ms_fDefaultMaxWalkInterruptPhase;
	static bank_float ms_fDefaultMinRunInterruptPhase;
	static bank_float ms_fDefaultMaxRunInterruptPhase;

	static const u32 ms_uNumExitToAimClips;

public:

	static const fwMvBooleanId ms_ExitToSwimInterruptId;

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, EXIT_VEHICLE_SEAT_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};


////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Subtask to control closing vehicle door from outside
////////////////////////////////////////////////////////////////////////////////

class CTaskCloseVehicleDoorFromOutside : public CTask
{
public:

	// FSM states
	enum eCloseVehicleDoorState
	{
		State_Start,
		State_CloseDoor,
		State_Finish
	};

public:

	CTaskCloseVehicleDoorFromOutside(CVehicle* pVehicle, s32 iExitPointIndex);
	virtual ~CTaskCloseVehicleDoorFromOutside();

	// Task required implementations
	virtual aiTask*		Copy() const						{ return rage_new CTaskCloseVehicleDoorFromOutside(m_pVehicle, m_iExitPointIndex); }
	virtual int			GetTaskTypeInternal() const					{ return CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_OUTSIDE; }
	virtual s32			GetDefaultStateAfterAbort() const	{ return State_Finish; }

	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return m_pParentNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return m_pParentNetworkHelper; }

private:

	// State Machine Update Functions
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Manipulate the animated velocity
	virtual bool		ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE:	Communicate with Move
	virtual bool		ProcessMoveSignals();

	// FSM optional functions
	virtual	void		CleanUp			();

	// Local state function calls
	FSM_Return			Start_OnUpdate();
	FSM_Return			CloseDoor_OnEnter();
	FSM_Return			CloseDoor_OnUpdate();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

private:

	void				DriveCloseDoorFromClip(const crClip* pOpenDoorClip, const float fOpenDoorPhase);
	const crClip*		GetClipAndPhaseForState(float& fPhase);
	// PURPOSE: Set the clip playback rate
	void				SetClipPlaybackRate();

private:

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper*		m_pParentNetworkHelper;
	RegdVeh					m_pVehicle;
	s32						m_iExitPointIndex;
	CPlayAttachedClipHelper	m_PlayAttachedClipHelper;
	bool					m_bMoveNetworkFinished;		// Used by ProcessMoveSignals() during certain states.
	bool					m_bTurnedOnDoorHandleIk;
	bool					m_bTurnedOffDoorHandleIk;
	bool					m_bDontUseArmIk;

	static const fwMvRequestId ms_CloseDoorRequestId;
	static const fwMvBooleanId ms_CloseDoorOnEnterId;
	static const fwMvBooleanId ms_CloseDoorClipFinishedId;
	static const fwMvBooleanId ms_CloseDoorInterruptId;
	static const fwMvClipId ms_CloseDoorClipId;
	static const fwMvFloatId ms_CloseDoorPhaseId;
	static const fwMvFloatId ms_CloseDoorRateId;
	static const fwMvStateId ms_CloseDoorStateId;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task to make ped being jacked react to being jacked, stops vehicle if ped 
//			is the driver
////////////////////////////////////////////////////////////////////////////////

class CTaskReactToBeingJacked : public CTask
{
private:

	enum RunningFlags
	{
		RF_HasTriedToSayAudio = BIT0,
	};

public:

	CTaskReactToBeingJacked(CVehicle* pJackedVehicle, CPed* pJackedPed, const CPed* pJackingPed);
	virtual ~CTaskReactToBeingJacked();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskReactToBeingJacked(m_pJackedVehicle, m_pJackedPed, m_pJackingPed); }

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_TO_BEING_JACKED; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define REACT_TO_BEING_JACKED_STATES(X)	X(State_Start),					\
											X(State_WaitForJackToStart),	\
											X(State_WaitForDriverExit),		\
											X(State_ExitVehicleOnLeft),		\
											X(State_ExitVehicleOnRight),	\
											X(State_JumpOutOfVehicle),		\
											X(State_ThreatResponse),		\
											X(State_Finish)

	// PURPOSE: FSM states
	enum eReactToBeingJackedState
	{
		REACT_TO_BEING_JACKED_STATES(DEFINE_TASK_ENUM)
	};

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	void	   WaitForJackToStart_OnEnter();
	FSM_Return WaitForJackToStart_OnUpdate();
	FSM_Return WaitForDriverExit_OnEnter();
	FSM_Return WaitForDriverExit_OnUpdate();
	FSM_Return ExitVehicleOnLeft_OnEnter();
	FSM_Return ExitVehicleOnLeft_OnUpdate();
	FSM_Return ExitVehicleOnRight_OnEnter();
	FSM_Return ExitVehicleOnRight_OnUpdate();
	FSM_Return JumpOutOfVehicle_OnEnter();
	FSM_Return JumpOutOfVehicle_OnUpdate();
	void	   ThreatResponse_OnEnter();
	FSM_Return ThreatResponse_OnUpdate();

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

	////////////////////////////////////////////////////////////////////////////////
	
private:

	bool CanCallPolice() const;
	bool HasJackStarted() const;
	void ProcessFlags();
	void ProcessHeadTracking();
	bool ShouldDisableUnarmedDrivebys() const;
	bool ShouldHeadTrack() const;
	bool ShouldPanic() const;
	bool ShouldJumpOutOfVehicle() const;

private:

	// PURPOSE: Vehicle being jacked
	RegdVeh	m_pJackedVehicle;

	// PURPOSE: Ped being jacked
	RegdPed m_pJackedPed;

	// PURPOSE: Ped doing the jacking
	RegdConstPed m_pJackingPed;

	int m_iNumPedsInSeats;

	fwFlags8 m_uRunningFlags;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	void Debug() const;

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, REACT_TO_BEING_JACKED_STATES)

#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Task to make ped being asked to leave vehicle react 
////////////////////////////////////////////////////////////////////////////////

class CTaskReactToBeingAskedToLeaveVehicle : public CTask
{
public:

	CTaskReactToBeingAskedToLeaveVehicle(CVehicle* pJackedVehicle, const CPed* pJackingPed);
	virtual ~CTaskReactToBeingAskedToLeaveVehicle();

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskReactToBeingAskedToLeaveVehicle(m_pJackedVehicle, m_pJackingPed); }

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_TO_BEING_ASKED_TO_LEAVE_VEHICLE; }

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_MaxTimeToWatchVehicle;
		float m_MaxDistanceToWatchVehicle;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	


protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Macro to allow us to define the state enum and string lists once
#define REACT_TO_BEING_ASKED_TO_LEAVE_VEHICLE_STATES(X)	X(State_Start),					\
	X(State_WaitForVehicleToLeave),		\
	X(State_EnterAbandonedVehicle),		\
	X(State_Finish)

	// PURPOSE: FSM states
	enum eReactToBeingJackedState
	{
		REACT_TO_BEING_ASKED_TO_LEAVE_VEHICLE_STATES(DEFINE_TASK_ENUM)
	};

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return Start_OnUpdate();
	void	   WaitForVehicleToLeave_OnEnter();
	FSM_Return WaitForVehicleToLeave_OnUpdate();
	void	   EnterAbandonedVehicle_OnEnter();
	FSM_Return EnterAbandonedVehicle_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

private:

	// PURPOSE: Vehicle being jacked
	RegdVeh	m_pJackedVehicle;

	// PURPOSE: Ped doing the jacking
	RegdConstPed m_pJackingPed;

	////////////////////////////////////////////////////////////////////////////////

public:

#if !__FINAL

	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, REACT_TO_BEING_ASKED_TO_LEAVE_VEHICLE_STATES)

#endif // !__FINAL

		////////////////////////////////////////////////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_TASK_EXIT_VEHICLE_H


