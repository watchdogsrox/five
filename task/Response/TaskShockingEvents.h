#ifndef TASK_SHOCKING_EVENTS_H
#define TASK_SHOCKING_EVENTS_H

// Game headers
#include "Event/ShockingEvents.h"
#include "Event/Event.h"
#include "Task/Helpers/PavementFloodFillHelper.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

class CTaskMove;

// For reference, this is the old response task for fleeing from a shocking event.
// We currently just use CTaskThreatResponse/CTaskSmartFlee for this, but there may
// still be parts of this task that we want to adopt (such as the mobile phone stuff).
#if 0

//-------------------------------------------------------------------------
// CTaskShockingEventFlee
//-------------------------------------------------------------------------

class CTaskShockingEventFlee : public CTaskShockingEvent
{
public:

	CTaskShockingEventFlee(const CShockingEvent& event, bool bKeepMovingWhilstWaitingForFleePath = false);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskShockingEventFlee(m_shockingEvent, m_bKeepMovingWhilstWaitingForFleePath); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_FLEE; }

	// Returns the response type to this event
	virtual eSEventResponseTypes GetShockingEventResponseType() const { return S_EVENT_REPOSNSE_FLEE; }

	// Reset the static timer
	static void ResetStatics() { ms_uiTimeTillCanDoNextGunShotMobileChat = 0; }

protected:

	typedef enum
	{
		State_Init = 0,
		State_FleeFromUnfriendly,
		State_FleeFromFriendly,
		State_LeaveCarAndFlee,
		State_Quit,
	} State;

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	FSM_Return StateInitOnUpdate(CPed* pPed);
	FSM_Return StateFleeFromUnfriendlyOnEnter(CPed* pPed);
	FSM_Return StateFleeFromFriendlyOnEnter(CPed* pPed);
	FSM_Return StateLeaveCarAndFleeOnEnter(CPed* pPed);

	//
	// Members
	//

	// Should we keep moving whilst waiting for the path request?
	bool m_bKeepMovingWhilstWaitingForFleePath;

	// Static timer shared between all events of this type
	static u32 ms_uiTimeTillCanDoNextGunShotMobileChat;
};

#endif	// 0

//-------------------------------------------------------------------------
// CTaskShockingEvent - Base implementation, keeps event information
//-------------------------------------------------------------------------

class CTaskShockingEvent : public CTask
{

protected:

	enum RunningFlags
	{
		RF_DisableSanityCheckEntities	= BIT0,
		RF_DisableCheckCanStillSense	= BIT1,
	};

public:
	struct Tunables : CTuning
	{
		Tunables();

		// Heading fixup constants.
		float	m_MinRemainingRotationForScaling;

		float	m_MinAngularVelocityScaleFactor;

		float	m_MaxAngularVelocityScaleFactor;

		PAR_PARSABLE;
	};

	explicit CTaskShockingEvent(const CEventShocking* event);
	~CTaskShockingEvent();

public:

	bool IsPlayerCriminal(const CPed& rPed) const
	{
		return (&rPed == GetPlayerCriminal());
	}

public:

	CPed*	GetPlayerCriminal() const;
	CPed*	GetSourcePed() const;
	void	SetCrimesAsWitnessed();

protected:

	virtual FSM_Return ProcessPreFSM();

	// Returns whether the ped is still in range of the event
	bool CheckCanStillSense() const;

	// Make sure the entities are still valid
	bool SanityCheckEntities() const;

	// Helper function to have the task react to a new shocking event.
	void SwapOutShockingEvent(const CEventShocking* pOther);

#if !__FINAL
	virtual void Debug() const;

	// This can be used by tasks to combine the provided state string with the shocking event type string
	const char* GetCombinedDebugStateName(const char* state) const;
#endif // !__FINAL

	//
	// Members
	//

	// PURPOSE:	Pointer to owned copy of the shocking event.
	eventRegdRef<const CEventShocking>	m_ShockingEvent;

	u32				m_TimeNotSensedMs;
	u32				m_TimeNotSensedThresholdMs;

	fwFlags8		m_uRunningFlags;

	// Common function for telling the IK manager to look at the shocking event.
	void			LookAtEvent();

	// Common function for saying the line associated with this shocking event.
	bool			SayShockingSpeech(float fRandom = 1.0f, u32 iVoice = 0);

	// Common function for heading alignment during response clips.
	void			AdjustAngularVelocityForIntroClip(float fTimeStep, const crClip& rClip, float fPhase, const Quaternion& qClipTotalMoverTrackRotation);

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: Checks distance if the other event is a shocking event with equal priority, otherwise return default behaviour as defined in task.h
	virtual bool	ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

public:

	// Helper functions for speech reactions.
	void			SetShouldPlayShockingSpeech(bool bPlay)		{ m_bShouldPlayShockingSpeech = bPlay; }
	bool			ShouldPlayShockingSpeech() const			{ return m_bShouldPlayShockingSpeech; }
	bool			CanWePlayShockingSpeech() const;
	virtual bool	IsValidForMotionTask(CTaskMotionBase& task) const;

private:

	//				Whether or not this shocking event reaction should play the shocking speech hash audio in response to the event.
	bool			m_bShouldPlayShockingSpeech;

	// PURPOSE:	Tuning parameters for the task.
	static Tunables	sm_Tunables;
};

//-------------------------------------------------------------------------
// CTaskShockingEventGoto
//-------------------------------------------------------------------------

/*
PURPOSE
	Approach a shocking event of some sort, and watch it for a moment.
	This task also makes some attempts at preventing peds from clumping up
	if a crowd forms.
*/
class CTaskShockingEventGoto : public CTaskShockingEvent
{
public:
	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : CTuning
	{
		Tunables();

		float	m_DistSquaredThresholdAtCrowdRoundPos;
		float	m_DistSquaredThresholdMovingToCrowdRoundPos;
		float	m_DistVicinityOfCrowd;
		float	m_ExtraDistForGoto;
		float	m_MinDistFromOtherPeds;
		float	m_MoveBlendRatioForFarGoto;
		float	m_TargetRadiusForCloseNavMeshTask;
		float	m_ExtraToleranceForStopWatchDistance;

		PAR_PARSABLE;
	};

	explicit CTaskShockingEventGoto(const CEventShocking* event);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskShockingEventGoto(m_ShockingEvent); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_GOTO; }

	virtual void CleanUp();

protected:

	enum
	{
		State_Initial = 0,
		State_Goto,
		State_GotoWithinCrowd,
		State_Watch,
		State_Quit,

		kNumStates
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	virtual void Debug() const;

	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	FSM_Return StateInitialOnUpdate();
	FSM_Return StateGotoOnEnter();
	FSM_Return StateGotoOnUpdate();
	FSM_Return StateGotoWithinCrowdOnEnter();
	FSM_Return StateGotoWithinCrowdOnUpdate();
	FSM_Return StateWatchOnEnter();
	FSM_Return StateWatchOnUpdate();

	// PURPOSE:	Code shared between the Goto and GotoWithinCrowd states.
	FSM_Return StateGotoCommonUpdate(Vec3V_In gotoTgt);

	// PURPOSE:	Check if we need to move or if we can watch from the current position,
	//			and switch to an appropriate state.
	void EvaluatePositionAndSwitchState();

	// PURPOSE:	Recompute and set m_CrowdRoundPos, based on the current position and nearby peds.
	void UpdateCrowdRoundPosition();

	// PURPOSE:	Check if we're standing close enough to m_CrowdRoundPos that we shouldn't move.
	unsigned int IsAtDesiredPosition(bool alreadyThere) const;

	// PURPOSE:	Check if we're somewhat close to the distance from target where we should watch from.
	unsigned int IsPedCloseToEvent() const;

	// PURPOSE:  Check if we're close enough to the target to keep watching.
	unsigned int IsPedTooFarToReact();

	// PURPOSE:	Get the center position of the event that we should watch.
	Vec3V_Out GetCenterLocation() const;

	// PURPOSE:	Create a CTaskComplexControlMovement with the supplied movement task and appropriate
	//			ambient clips, and SetNewTask() it.
	void SetMoveTaskWithAmbientClips(CTaskMove* pMoveTask);

	// PURPOSE:	Speech played when the task starts.
	void PlayInitialSpeech();

	// PURPOSE:	Speech potentially played during movement.
	void PlayContinuousSpeech();

	// PURPOSE:	Timer for how long time to watch, started once in position.
	CTaskGameTimer	m_Timer;

	// PURPOSE:	A good position we should stand at, computed by UpdateCrowdRoundPosition(). If
	//			within range of the target and not too close to anybody else, this would be the
	//			ped's current position.
	Vec3V			m_CrowdRoundPos;

	// PURPOSE:	The distance we should try to get to to watch.
	float			m_WatchRadius;

	// PURPOSE: The distance beyond which the Goto task will be stopped because the target is too far away.
	float			m_StopWatchDistance;

	// PURPOSE:	Tuning parameters for the task.
	static Tunables	sm_Tunables;
};

//-------------------------------------------------------------------------
// CTaskShockingEventHurryAway
//-------------------------------------------------------------------------

class CTaskShockingEventHurryAway : public CTaskShockingEvent
{
public:

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : CTuning
	{
		Tunables();

		// PURPOSE:  How far ahead on the path to check if the ped should try to play back away anims.
		float	m_LookAheadDistanceForBackAway;
		float	m_ChancesToCallPolice;
		float	m_MinTimeToCallPolice;
		float	m_MaxTimeToCallPolice;
		float	m_ChancePlayingInitalTurnAnimSmallReact;
		float	m_ChancePlayingCustomBackAwayAnimSmallReact;
		float	m_ChancePlayingInitalTurnAnimBigReact;
		float	m_ChancePlayingCustomBackAwayAnimBigReact;
		float	m_ShouldFleeDistance;
		float	m_ShouldFleeVehicleDistance;
		float	m_ShouldFleeFilmingDistance;
		u32		m_EvasionThreshold;

		// Speed up because of local player tuning.
		float	m_ClosePlayerSpeedupDistanceSquaredThreshold;
		float	m_ClosePlayerSpeedupTimeThreshold;

		// Destroy ped tunables.
		float m_MinDistanceFromPlayerToDeleteHurriedPed;
		float m_TimeUntilDeletionWhenHurrying;
		PAR_PARSABLE;
	};

	CTaskShockingEventHurryAway(const CEventShocking* event);

public:

	float	GetTimeHurryingAway() const;
	bool	HasCalledPolice() const { return m_bHasCalledPolice; }
	bool	IsBackingAway() const;
	bool	IsHurryingAway() const;
	void	SetHurryAwayMaxTime(float fMaxTime) { m_fHurryAwayMaxTime = fMaxTime; }
	void	SetForcePavementNotRequired(bool b) { m_bForcePavementNotRequired = b; }
	void	SetGoDirectlyToWaitForPath(bool bDirect) { m_bGoDirectlyToWaitForPath = bDirect; }
	bool	SetSubTaskForHurryAway(CTask* pTask);
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	bool    IsPedfarEnoughToMigrate(); //used in network by control passing checks

public:

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const;

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY; }

protected:

	enum
	{
		State_Init = 0,
		State_React,
		State_WaitForPath,
		State_Watch,
		State_BackAway,
		State_HurryAway,
		State_Cower,
		State_Quit,

		kNumStates
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_WaitForPath; }

	virtual	void	CleanUp();

#if !__FINAL
	virtual void Debug() const;

	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:
	// State functions
	FSM_Return				StateInitOnUpdate();

	void					StateReactOnEnter();
	FSM_Return				StateReactOnUpdate();

	void					StateWatchOnEnter();
	FSM_Return				StateWatchOnUpdate();

	void					StateWaitForPathOnEnter();
	FSM_Return				StateWaitForPathOnUpdate();

	FSM_Return				StateBackAwayOnUpdate();

	void					StateHurryAwayOnEnter();
	FSM_Return				StateHurryAwayOnUpdate();

	void					StateCowerOnEnter();
	FSM_Return				StateCowerOnUpdate();


	CPed*	GetPedToCallPoliceOn() const;
	bool	ShouldFleeFromShockingEvent();
	bool	ShouldFleeFromEntity(const CEntity* pEntity) const;
	u32		GetRandomFilmTimeOverrideMS() const;

	void	AnalyzePavementSituation();
	bool	CaresAboutPavement() const { return m_bCaresAboutPavement; }

	bool	ActAsIfWeHavePavement() const { return !CaresAboutPavement() || m_bOnPavement; }

private:

	s32		DetermineConditionalAnimsForHurryAway(const CConditionalAnimsGroup* pGroup);
	void	UpdateNavMeshTargeting();
	void	UpdateAmbientClips();
	void	UpdateHurryAwayMBR();
	bool	ShouldSpeedUpBecauseOfLocalPlayer();
	bool	ShouldUpdateTargetPosition();
	void	PossiblyCallPolice();
	void	PossiblyDestroyHurryingAwayPed();

	// Helper functions for the ambient clip being changed.

	// Determine if the ped was allowed to change this clip at the start of the task.
	bool	CheckIfAllowedToChangeMovementClip() const;
	bool	ShouldChangeMovementClip() const		{ return m_bShouldChangeMovementClip; }
	void	SetShouldChangeMovementClip(bool bSet)	{ m_bShouldChangeMovementClip =  bSet; }

	bool	IsOnTrain() const;
	bool	CheckForCower();


	CPavementFloodFillHelper m_pavementHelper;

	float	m_fTimeToCallPolice;
	float	m_fTimeOffScreenWhenHurrying;
	float	m_fClosePlayerCounter;
	s32		m_iConditionalAnimsChosen;
	bool	m_bCallPolice;
	bool	m_bHasCalledPolice;
	bool	m_bFleeing;
	bool	m_bResumePreviousAction;
	bool	m_bGoDirectlyToWaitForPath;
	bool	m_bOnPavement;
	bool	m_bCaresAboutPavement;
	bool	m_bForcePavementNotRequired;
	bool	m_bZoneRequiresPavement;
	bool	m_bEvadedRecentlyWhenTaskStarted;
	bool	m_bShouldChangeMovementClip;
	bool	m_bDisableShortRouteCheck;
	float	m_fHurryAwayMaxTime;

	CTaskGameTimer m_HurryAwayTimer;

private:

	// PURPOSE:	Tuning parameters for the task.
	static Tunables	sm_Tunables;

	// PURPOSE:  Static hack to ensure some variety among chosen clips in hurry away.
	static s32 sm_iLastChosenAnim;


};

//-------------------------------------------------------------------------
// CTaskShockingEventWatch
//-------------------------------------------------------------------------

class CTaskShockingEventWatch : public CTaskShockingEvent
{
public:
	struct Tunables : CTuning
	{
		Tunables();

		// PURPOSE:	After the face task has finished, in addition to check if the
		//			target is ahead of us, we also check if the target's velocity
		//			perpendicular to us is below this angular velocity. The purpose
		//			of this is to not kick off the watch scenario if we're likely
		//			to have to break out if it very soon again in order to turn.
		// NOTES:	Should be a value in radians per second.
		float	m_MaxTargetAngularMovementForWatch;

		float	m_ThresholdWatchAfterFace;
		float	m_ThresholdWatchStop;
		
		// PURPOSE: Minimum distance away from any other filming ped we must be to start filming a shocking event ourselves.
		float m_MinDistanceBetweenFilmingPeds;

		// PURPOSE: Minimum distance away from any other ped we must be to start filming a shocking event.
		float m_MinDistanceFromOtherPedsToFilm;

		// PURPOSE: Minimum distance away from the event to start filming.
		float m_MinDistanceAwayForFilming;

		PAR_PARSABLE;
	};

	explicit CTaskShockingEventWatch(const CEventShocking* event, u32 uiWatchTime = 0, u32 uPhoneFilmTimeOverride = 0, bool bResumePreviousAction = false, bool watchForever = false);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const;

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_WATCH; }

	virtual bool SupportsTerminationRequest() { return true; }

	bool IsFilming() const { return m_bFilming; }

	void FleeFromEntity();

	static void ResetStatics() { sm_NextTimeAnyoneCanStopWatching = 0; }

protected:

	enum
	{
		State_Start = 0,
		State_Facing,
		State_Watching,
		State_Finish,

		kNumStates
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

protected:

	FSM_Return StateStartOnUpdate();
	FSM_Return StateFacingOnEnter();
	FSM_Return StateFacingOnUpdate();
	FSM_Return StateWatchingOnEnter();
	FSM_Return StateWatchingOnUpdate();

	bool IsFacingTarget(float cosThreshold) const;
	bool IsTargetMovingAroundUs(const float &thresholdRadiansPerSecond) const;
	void UpdateHeadTrack();
	bool UpdateWatchTime();
	bool IsTooCloseToPlayerToFilm() const;
	bool IsTooCloseToOtherPeds() const;
	bool IsTooCloseToFilm() const;
	void SayFilmShockingSpeech();

	// Extra precaution for random watch times not being quite enough to prevent twinning.
	bool IsToSoonToLeaveAfterSomeoneElse() const;
	void SetLastTimeOfWatchExit();

	//
	// Members
	//

	// PURPOSE:	The amount of time we will watch, counting down during update.
	u32		m_RandomWatchTime;

	// PURPOSE: If set to non-zero and this task decides to film the event on a mobile phone, use this time instead of m_RandomWatchTime
	u32   m_PhoneFilmTimeOverride;

	// PURPOSE:	True if the countdown of m_RandomWatchTime has started.
	bool	m_WatchTimeStarted;
	bool	m_bWatchForever;
	bool  m_bFilming;
	bool  m_bResumePreviousAction;

	static Tunables		sm_Tunables;

private:

	static const fwMvClipSetId sm_WatchClipSet;

	static u32 sm_NextTimeAnyoneCanStopWatching;
};


//-------------------------------------------------------------------------
// CTaskShockingEventReact
// - Turns toward the target and play a series of reaction animations.
//-------------------------------------------------------------------------


class CTaskShockingEventReact : public CTaskShockingEvent
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		// PURPOSE: How far away our heading can be from the target heading to be considered facing.
		float	m_TurningTolerance;

		// PURPOSE:	The maximum rate we can change our heading per frame.
		float	m_TurningRate;

		// PURPOSE:  The energy magnitude of the animated velocity we require to be exceeded in order to turn (to hide foot sliding).
		float	m_TurningEnergyUpperThreshold;

		// PURPOSE:  The energy magnitude of the animated velocity that if we fall below, then stop all turning (to hide foot sliding).
		float	m_TurningEnergyLowerThreshold;

		// PUPROSE:  Time between reaction idles min bound.
		float	m_TimeBetweenReactionIdlesMin;

		// PURPOSE:  Time between reaction idles max bound.
		float	m_TimeBetweenReactionIdlesMax;

		// PURPOSE:  How far into the animation we should start blending out.
		float	m_BlendoutPhase;

		PAR_PARSABLE;
	};

	explicit CTaskShockingEventReact(const CEventShocking* event);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const;

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_REACT; }

	// Used by other tasks to tell the React Task to stop playing the animations early.
	void	StopReacting();

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool ProcessMoveSignals();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

	// FSM optional functions
	virtual	void	CleanUp();

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

protected:

	enum
	{
		State_StreamingIntro = 0,
		State_PlayingIntro,
		State_PlayingGeneric,
		State_StreamingReact,
		State_PlayingReact,
		State_Finish,

		kNumStates
	};

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

protected:

	//FSM functions
	void			StateStreamingIntroOnEnter();
	FSM_Return		StateStreamingIntroOnUpdate();

	void			StatePlayingIntroOnEnter();
	FSM_Return		StatePlayingIntroOnUpdate();
	void			StatePlayingIntroOnProcessMoveSignals();

	void			StatePlayingGenericOnEnter();
	FSM_Return		StatePlayingGenericOnUpdate();

	void			StateStreamingReactOnEnter();
	FSM_Return		StateStreamingReactOnUpdate();

	void			StatePlayingReactOnEnter();
	FSM_Return		StatePlayingReactOnUpdate();
	void			StatePlayingReactOnExit();
	void			StatePlayingReactOnProcessMoveSignals();

	//Helper functions

	//Pick Intro clip based on our orientation and the target's position
	void PickIntroClipFromFacing();
	
	//Inform the MoVE network which intro clip we are using
	void SetFacingFlag(CMoveNetworkHelper& helper);
	
	//If we need to transition back to the small react state, reset some flags needed to go back through the FSM
	void ResetMoveNetworkFlags();

	// Set the ped's desired heading to be facing the event.
	void UpdateDesiredHeading();

	void GetTarget(CAITarget& rTarget) const;

private:

	//Member Variables

	// PURPOSE:	Timer for how long time to watch, started once in position.
	CTaskGameTimer	m_Timer;
	
	// Animation Helpers
	static const u32 MAX_STREAMED_REACT_CLIPSETS				= 3;
	CMoveNetworkHelper											m_MoveNetworkHelper;
	CMultipleClipSetRequestHelper<MAX_STREAMED_REACT_CLIPSETS>	m_ClipSetHelper;

	// Clipsets
	fwMvClipSetId			m_IntroClipSet;
	fwMvClipSetId			m_ExitClipSet;
	fwMvClipSetId			m_ReactClipSet;

	// MOVE NETWORK VARIABLES

	// Vary clips chosen within clipsets
	static const fwMvFloatId		ms_IntroVariationId;
	static const fwMvFloatId		ms_ReactVariationId;

	// Booleans set from move network to inform us of current state
	static const fwMvBooleanId		ms_IntroDoneId;
	static const fwMvBooleanId		ms_ReactBaseDoneId;
	static const fwMvBooleanId		ms_ReactIdleDoneId;

	// Inform Move Network which clipset to pick from
	static const fwMvFlagId			ms_ReactBackId;
	static const fwMvFlagId			ms_ReactLeftId;
	static const fwMvFlagId			ms_ReactFrontId;
	static const fwMvFlagId			ms_ReactRightId;

	// Control which direction the FSM goes inside the move network
	static const fwMvRequestId		ms_PlayIntroRequest;
	static const fwMvRequestId		ms_PlayBaseRequest;
	static const fwMvRequestId		ms_PlayIdleRequest;

	static const fwMvFlagId			ms_IsBigId;
	static const fwMvFlagId			ms_IsFemaleId;

	static const fwMvClipId			ms_IntroClipId;
	static const fwMvFloatId		ms_IntroPhaseId;
	static const fwMvFloatId		ms_IdlePhaseId;
	static const fwMvFloatId		ms_TransPhaseId;

	// PURPOSE:  Store the total rotation of the clip so it doesn't have to be recalculated each frame when extra rotation is applied.
	Quaternion						m_qCachedClipTotalRotation;

	// PURPOSE:  Countdown until next idle time.
	float							m_fTimeUntilNextReactionIdle;

	// PURPOSE:  Timeslicing phase cache.
	float							m_fAnimPhase;

	// PURPOSE:  Whether or not we are permitted (based on properties of the animation) to change the heading of the ped this frame.
	bool							m_bCanChangeHeading;

	// PURPSOE:  Have we cached off the rotation of the clip yet?
	bool							m_bHasCachedClipRotation;

	// PURPOSE:  Timeslicing move cache.
	bool							m_bMoveIsInTargetState;

	static Tunables					sm_Tunables;
};

//-------------------------------------------------------------------------
// CTaskShockingEventReact
// - Play an animation and back away from the event.
//-------------------------------------------------------------------------


class CTaskShockingEventBackAway : public CTaskShockingEvent
{
public:
	struct Tunables : CTuning
	{
		Tunables();

		// PURPOSE:  Speed for heading adjustments when trying to back away in the direction of a point.
		float	m_MaxHeadingAdjustmentRate;

		// PURPOSE:  How small of an angle difference is acceptable to even attempt heading changes with.
		float	m_MinHeadingAlignmentCosThreshold;

		// PURPOSE:  How large of an angle is acceptable to still be considered on target for the goal point.
		float	m_MaxHeadingAlignmentCosThreshold;

		// PURPOSE:  How long to take blend out the move network upon task completion.
		float	m_MoveNetworkBlendoutDuration;

		// PURPOSE:  When calling GetDefaultBackAwayPositionForPed, this is how far behind the ped the target point is projected.
		float	m_DefaultBackwardsProjectionRange;

		// PURPOSE:  When checking if a ped should back away, this is the tolerance to check against the ideal position to be backing towards.
		float	m_AxesFacingTolerance;

		// PURPOSE:  The minimum distance between navmesh waypoints to support playing the back away animations.
		float	m_MinDistanceForBackAway;

		// PURPOSE:  The maximum distance before hitting the next route point before the backaway animation is forced to blend out.
		float	m_MaxDistanceForBackAway;

		// PURPOSE:  The maximum dot product between the ped and their waypoint.
		float	m_MaxWptAngle;

		// PURPOSE:  What phase the back away anim should blend out at.
		float	m_BlendOutPhase;

		PAR_PARSABLE;
	};

	explicit CTaskShockingEventBackAway(const CEventShocking* event, Vec3V_ConstRef vTargetPosition, bool bAllowHurryWhenBumped=true);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const;

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_BACK_AWAY; }

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

public:

	// Static helper functions

	static Vec3V_Out GetDefaultBackAwayPositionForPed(const CPed& rPed);

	// Helper functions for sticking to a route.
	void SetRouteWaypoints(const Vector3& vPrev, const Vector3& vCurr) { m_vPrevWaypoint = vPrev; m_vNextWaypoint = vCurr; m_bHasWaypoints = true;}

	// PURPOSE:  Return true if the target position is close enough to the back away axes to make this task a valid reaction.
	static bool	ShouldBackAwayTowardsRoutePosition(Vec3V_ConstRef vPedPosition, Vec3V_ConstRef vPrevWaypointPos, Vec3V_ConstRef vNextWaypointPos);
	static bool IsFacingRightDirectionForBackAway(Vec3V_ConstRef vPedPosition, Vec3V_ConstRef vTargetPosition);

protected:

	enum
	{
		State_Streaming = 0,
		State_Playing,
		State_HurryAway,
		State_Finish,

		kNumStates
	};


	virtual FSM_Return	ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool ProcessMoveSignals();

	//FSM functions
	void		StateStreamingOnEnter();
	FSM_Return	StateStreamingOnUpdate();
	
	void		StatePlayingOnEnter();
	FSM_Return	StatePlayingOnUpdate();
	void		StatePlayingOnProcessMoveSignals();

	void		StateHurryAwayOnEnter();
	FSM_Return	StateHurryAwayOnUpdate();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

	// FSM optional functions
	virtual	void	CleanUp();

	//Helper functions
	void PickClipSet();
	void SetFacingFlag(CMoveNetworkHelper& helper);

	// Return true if we are about to leave our path because of the animations.
	bool ShouldBlendOutBecauseOffPath();

	// Return true if we have experienced a collision at some point during the playback of the animation
	bool HaveHadACollision();

	// Return true if we are within an acceptable phase range to quit the animation based on tags
	bool CanQuitAnimationNow();

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	//Member variables
	static Tunables		sm_Tunables;

private:

	//Member Variables

	CMoveNetworkHelper		m_MoveNetworkHelper;

	fwClipSetRequestHelper	m_ClipSetHelper;

	static const fwMvFlagId		ms_React0Id;
	static const fwMvFlagId		ms_React90Id;
	static const fwMvFlagId		ms_ReactNeg90Id;
	static const fwMvFlagId		ms_React180Id;

	static const fwMvRequestId	ms_PlayAnimRequest;
	
	static const fwMvBooleanId	ms_AnimDoneId;

	static const fwMvFloatId	ms_AnimPhaseId;

	static const fwMvClipId		ms_ReactionClip;

	// Store the total rotation of the clip so it doesn't have to be recalculated each frame when extra rotation is applied.
	Quaternion				m_qCachedClipTotalRotation;

	// Where the ped wants to back away towards.
	Vec3V					m_vTargetPoint;

	Vector3					m_vPrevWaypoint;
	Vector3					m_vNextWaypoint;

	// Anim phase cache for timeslicing.
	float					m_fAnimPhase;

	// Which clip to stream in based on properties of the ped
	fwMvClipSetId			m_ClipSetId;

	// Whether we had a collision so the ped should try and blend out the animation when possible
	bool					m_bShouldTryToQuit;

	// Whether the ped can switch to a hurry away if they bump into something.
	bool					m_bAllowHurryWhenBumped;

	// Force the ped to use the shocking event reaction facing towards the target.
	bool					m_bUsingOverideTargetPosition;

	// Was the task supplied waypoints to check routes with?
	bool					m_bHasWaypoints;

	// Have we stored off the rotation in the intro clip yet?
	bool					m_bHasCachedClipRotation;

	// MoVE state cache for timeslicing.
	bool					m_bMoveInTargetState;

};


class CTaskShockingEventReactToAircraft : public CTaskShockingEvent
{
public:
	struct Tunables : CTuning
	{
		Tunables();

		// PURPOSE: How far away the aircraft should be for us to watch it.
		float	m_ThresholdWatch;

		// PURPOSE:	How far away the aircraft should be before we start running.
		float	m_ThresholdRun;

		PAR_PARSABLE;
	};

	explicit CTaskShockingEventReactToAircraft(const CEventShocking* event);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskShockingEventReactToAircraft(m_ShockingEvent); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_REACT_TO_AIRCRAFT; }

protected:

	enum
	{
		State_Start = 0,
		State_Watching,
		State_RunningAway,
		State_Finish,

		kNumStates
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	FSM_Return StateStartOnUpdate();
	FSM_Return StateWatchingOnEnter();
	FSM_Return StateWatchingOnUpdate();
	FSM_Return StateRunningAwayOnEnter();
	FSM_Return StateRunningAwayOnUpdate();

	void DetermineNextStateFromDistance();

	//
	// Members
	//

	static Tunables		sm_Tunables;
};


class CTaskShockingPoliceInvestigate : public CTaskShockingEvent
{
public:

	//Tunables
	struct Tunables : CTuning
	{
		Tunables();

		float	m_ExtraDistForGoto;
		float	m_MoveBlendRatioForFarGoto;

		float	m_MinDistFromPlayerToDeleteOffscreen;
		u32		m_DeleteOffscreenTimeMS_MIN;
		u32		m_DeleteOffscreenTimeMS_MAX;

		PAR_PARSABLE;
	};

	// Constructor
	explicit CTaskShockingPoliceInvestigate(const CEventShocking* event);

	// Destructor
	virtual ~CTaskShockingPoliceInvestigate();

	// Task required functions
	virtual aiTask*	Copy() const;
	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_POLICE_INVESTIGATE; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Start; }

	virtual void CleanUp();

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	static int GetNumberTaskUses() { return m_iNumberTaskUses;}
private:

	// FSM states
	enum 
	{ 
		State_Start = 0,
		State_GotoLocationInVehicle,
		State_ExitVehicle,
		State_GotoLocationOnFoot,
		State_FaceDirection,
		State_Investigate,
		State_Finish
	};

	// State Functions
	FSM_Return Start_OnUpdate();

	void GotoLocationInVehicle_OnEnter();
	FSM_Return GotoLocationInVehicle_OnUpdate();

	void ExitVehicle_OnEnter();
	FSM_Return ExitVehicle_OnUpdate();

	void GotoLocationOnFoot_OnEnter();
	FSM_Return GotoLocationOnFoot_OnUpdate();

	void FaceDirection_OnEnter();
	FSM_Return FaceDirection_OnUpdate();

	void Investigate_OnEnter();
	FSM_Return Investigate_OnUpdate();
	
	// PURPOSE:	Get the center position of the event that we should watch.
	Vec3V_Out GetCenterLocation() const;

	// PURPOSE:	Create a CTaskComplexControlMovement with the supplied movement task and appropriate
	//			ambient clips, and SetNewTask() it.
	void SetMoveTaskWithAmbientClips(CTaskMove* pMoveTask);

	FSM_Return StateGotoCommonUpdate(Vec3V_In gotoTgt);

	bool AssignVehicleTask();

	// For performance, if this ped is standing forever, try to remove it.
	void PossiblyDestroyPedInvestigatingForever();

	// Check if my vehicle is blocking traffic (used to hurry up when that is the case)
	bool IsMyVehicleBlockingTraffic() const;

	// PURPOSE:	Timer for how long time to watch, started once in position.
	CTaskGameTimer	m_Timer;

	// Timer for ped offscreen to be deleted.
	CTaskGameTimer	m_OffscreenDeletePedTimer;

	// PURPOSE: 
	bool m_bFacingIncident;
	bool m_bVehicleTaskGiven;

	static int m_iNumberTaskUses;

	// PURPOSE:	Tuning parameters for the task.
	static Tunables	sm_Tunables;
};

//-------------------------------------------------------------------------
// CTaskShockingEventStopAndStare
// - The ped stops their car and looks at the shocking event for a while.
//-------------------------------------------------------------------------


class CTaskShockingEventStopAndStare : public CTaskShockingEvent
{
public:

	//Tunables
	struct Tunables : CTuning
	{
		Tunables();

		float	m_BringVehicleToHaltDistance;

		PAR_PARSABLE;
	};


	explicit CTaskShockingEventStopAndStare(const CEventShocking* event);

	~CTaskShockingEventStopAndStare() {}

	virtual aiTask* Copy() const { return rage_new CTaskShockingEventStopAndStare(m_ShockingEvent); }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_EVENT_STOP_AND_STARE; }

protected:


	// States
	enum
	{
		State_Stopping,
		State_Finish,

		kNumStates
	};

	// FSM Implementation
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	void				Stopping_OnEnter();
	FSM_Return			Stopping_OnUpdate();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }
	virtual bool MayDeleteOnAbort() const { return true; }	// This task doesn't resume if it gets aborted, should be safe to delete.

#if !__FINAL

	friend class CTaskClassInfoManager;

	static const char* GetStaticStateName(s32 state);

#endif	//!__FINAL

private:

	// PURPOSE:	Tuning parameters for the task.
	static Tunables	sm_Tunables;

};

class CTaskShockingNiceCarPicture : public CTaskShockingEvent
{
public:

	// Constructor
	explicit CTaskShockingNiceCarPicture(const CEventShocking* event);

	// Destructor
	virtual ~CTaskShockingNiceCarPicture() { }

	// Task required functions
	virtual aiTask*	Copy() const { return rage_new CTaskShockingNiceCarPicture(m_ShockingEvent); }
	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_SHOCKING_NICE_CAR_PICTURE; }

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Quit; }

	// Debug functions
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// FSM states
	enum 
	{ 
		State_TurnTowardsVehicle,
		State_TakePicture,
		State_Quit,

		kNumStates
	};

	// State Functions
	void TurnTowardsVehicle_OnEnter();
	FSM_Return TurnTowardsVehicle_OnUpdate();

	void TakePicture_OnEnter();
	FSM_Return TakePicture_OnUpdate();
};

//-------------------------------------------------------------------------
// CTaskShockingEventThreatResponse
//-------------------------------------------------------------------------

class CTaskShockingEventThreatResponse : public CTaskShockingEvent
{

public:

	enum State
	{
		State_Start = 0,
		State_ReactOnFoot,
		State_ReactInVehicle,
		State_ThreatResponse,
		State_Finish,
	};

public:

	explicit CTaskShockingEventThreatResponse(const CEventShocking* event);
	virtual ~CTaskShockingEventThreatResponse();

public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask* Copy() const
	{
		return rage_new CTaskShockingEventThreatResponse(m_ShockingEvent);
	}

	virtual s32 GetDefaultStateAfterAbort() const
	{
		return State_ThreatResponse;
	}

	virtual int GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_SHOCKING_EVENT_THREAT_RESPONSE;
	}

private:

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return	Start_OnUpdate();
	void		ReactOnFoot_OnEnter();
	FSM_Return	ReactOnFoot_OnUpdate();
	void		ReactInVehicle_OnEnter();
	FSM_Return	ReactInVehicle_OnUpdate();
	void		ThreatResponse_OnEnter();
	FSM_Return	ThreatResponse_OnUpdate();

private:

	RegdPed m_pThreat;

};


#endif //TASK_SHOCKING_EVENTS_H
