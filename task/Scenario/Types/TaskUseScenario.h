#ifndef INC_TASK_USE_SCENARIO_H
#define INC_TASK_USE_SCENARIO_H

#include "ai/AITarget.h"
#include "Event/Event.h"
#include "fwanimation/animdefines.h"		// fwSyncedSceneId
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Helpers/PropManagementHelper.h"
#include "Task/Helpers/PavementFloodFillHelper.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Scenario/TaskScenario.h"
#include "task/Scenario/ScenarioAnimDebugHelper.h"
#include "Task/System/Tuning.h"

class CTaskAmbientMigrateHelper;
class CTaskUseScenario;

/*
PURPOSE
	Extension we add to CDummyObjects, for keeping track of all CTaskUseScenarios which
	are attached to the object.
*/
class CTaskUseScenarioEntityExtension : public fwExtension
{
public:
	FW_REGISTER_CLASS_POOL(CTaskUseScenarioEntityExtension);
	EXTENSIONID_DECL(CTaskUseScenarioEntityExtension, fwExtension);

	void AddTask(CTaskUseScenario& task);
	void RemoveTask(CTaskUseScenario& task);

	bool IsEmpty() const { return m_Tasks.IsEmpty(); }

	void NotifyTasksOfNewObject(CObject& obj);

protected:
	static const int kMaxScenarioTasksPerObject = 8;

	int FindIndexOfTaskInArray(CTaskUseScenario* pTask);

	atFixedArray<RegdTask, kMaxScenarioTasksPerObject> m_Tasks;
};

class CTaskUseScenario : public CTaskScenario
{
public:
	friend class CClonedTaskUseScenarioInfo;
	friend class CClonedTaskUnalertedInfo;
	friend class CTaskUnalerted;

	// PURPOSE: Max Idle Time. This is used when serializing the task info.
	static const float fMAX_IDLE_TIME;
	
	// PURPOSE:	Global task tuning data.
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	When spawning ambient peds in place, this controls how much extra randomness there is
		//			in the time before they leave the scenario. 0 means no randomness and 1 would mean that
		//			they could potentially get up right after spawning.
		float	m_AdvanceUseTimeRandomMaxProportion;

		// PURPOSE: If attached to an entity, this is the threshold its velocity must exceed to break the attach status with ragdoll.
		float	m_BreakAttachmentMoveSpeedThreshold;

		// PURPOSE: If attached to an entity, this is the threshold its up vector's z component must fall below to break the attach status with ragdoll.
		float	m_BreakAttachmentOrientationThreshold;

		// PURPOSE: If attached to an entity, this is the threshold its velocity must exceed to exit normally.
		float	m_ExitAttachmentMoveSpeedThreshold;

		// PURPOSE: While moving towards the scenario point, this is a threshold against which to compare the route length to re-evaluate the path.
		float	m_RouteLengthThresholdForFinalApproach;

		// PURPOSE: The Z-threshold for the difference of the ped and scenario's z position when offsetting the goto target.
		float	m_ZThresholdForApproachOffset;

		// PURPOSE: When moving towards the scenario point, this is the route length threshold to determine if the ped is close enough to offset the goto target.
		float	m_RouteLengthThresholdForApproachOffset;
		
		// PURPOSE: Default phase value for detaching from a scenario when exiting to a fall (used when clip is not tagged).
		float	m_DetachExitDefaultPhaseThreshold;

		// PURPOSE: Default phase value for when the ped is doing a normal exit in a stressed mode (combat or to cower).
		float	m_FastExitDefaultPhaseThreshold;
		
		// PURPOSE: How much to add to the distance between the ped and aggressor when determining flee safe distance.
		float	m_ExtraFleeDistance;

		// PURPOSE:	Max distance at which to locate props in the environment to use from the scenario.
		float	m_FindPropInEnvironmentDist;
		
		// PURPOSE: Minimum play rate of the cower animation
		float  m_MinRateToPlayCowerReaction;

		// PURPOSE: Maximum play rate of the cower animation
		float  m_MaxRateToPlayCowerReaction;

		// PURPOSE: The minimum amount two subsequent cower reaction rates can differ by.
		float  m_MinDifferenceBetweenCowerReactionRates;

		// PURPOSE:	When playing a panic exit animation to transition to CTaskReactAndFlee, end
		//			this task when less than the exit clip phase passes this mark.
		float	m_ReactAndFleeBlendOutPhase;

		// PURPOSE: When playing a normal exit animation to transition into some other task, end
		//			the exit clip when the exit clip phase passes this mark.
		float	m_RegularExitDefaultPhaseThreshold;

		// PURPOSE:	To make it look more natural when the time of day limit has been reached,
		//			allow for up to this amount of extra hours of random usage time before leaving.
		float	m_TimeOfDayRandomnessHours;

		// PURPOSE:	Minimum time in seconds that must have elapsed since any ped naturally left a scenario
		//			before another one can do it, used to desynchronize animations.
		float	m_TimeToLeaveMinBetweenAnybody;

		// PURPOSE:	Time in seconds for how much the time until leaving can vary randomly.
		//			If m_TimeToLeaveRandomFraction yields a larger time, that will be used instead.
		float	m_TimeToLeaveRandomAmount;

		// PURPOSE:	Specifies how much the time until leaving can vary randomly, as a fraction
		//			of the total use time. If this comes out to a smaller time than m_TimeToLeaveRandomAmount,
		//			m_TimeToLeaveRandomAmount will be used instead.
		float	m_TimeToLeaveRandomFraction;

		// PURPOSE: Defines a radius to use when doing flood fill searches for nearby pavement.
		float	m_PavementFloodFillSearchRadius;

		// PURPOSE: Time between doing pavement searches.
		float	m_DelayBetweenPavementFloodFillSearches;

		// PURPOSE: When starting an early navmesh flee, this is the lower bound for the initial MBR.
		float	m_FleeMBRMin;

		// PURPOSE: When starting an early navmesh flee, this is the upper bound for the initial MBR.
		float	m_FleeMBRMax;

		// PURPOSE: When playin a panic exit animation from a scenario, this is the min route length the ped can have to consider this exit point as valid.
		float	m_MinPathLengthForValidExit;

		// PURPOSE: The min distance the pathserver can snap the path from this point if the scenario entity exists and is not fixed.
		float	m_MaxDistanceNavmeshMayAdjustPath;

		// PURPOSE: When cowering, this is how long in seconds between attempts to leave the point.
		float	m_TimeBetweenChecksToLeaveCowering;

		// PURPOSE: When initially heading to the scenario the XY distance that means they will just skip the locomotion and blend into the intro.
		float	m_SkipGotoXYDist;

		// PURPOSE: When initially heading to the scenario the Z distance that means they will just skip the locomotion and blend into the intro.
		float	m_SkipGotoZDist;

		// PURPOSE: When initially heading to the scenario the heading delta acceptable to skip the locomotion and blend into the intro.
		float	m_SkipGotoHeadingDeltaDegrees;

		// PURPOSE: Minimum amount of extra money granted when Ped uses a scenario that grants money.
		s32		m_MinExtraMoney;

		// PURPOSE: Maximum amount of extra money granted when Ped uses a scenario that grants money.
		s32		m_MaxExtraMoney;

		// PURPOSE: PlayAmbients_OnUpdate will manage the ped bound after X updates...previous value was too soon and causing them to slip into the ground
		s8		m_UpdatesBeforeShiftingBounds;

		PAR_PARSABLE;
	};


	// Scenario Flags
	enum
	{
		// Config Flags
		SF_Warp						= BIT0,
		SF_StartInCurrentPosition	= BIT1,
		SF_SkipEnterClip			= BIT2,
		SF_AttachToWorld			= BIT3,
		SF_CheckShockingEvents		= BIT4,		// Check for shocking events while running the task. The actual decision-making response needs to be handled outside of the task.
		SF_SynchronizedMaster		= BIT5,
		SF_SynchronizedSlave		= BIT6,
		SF_AddToHistory				= BIT7,		// Try to register this ped and scenario point with the scenario spawn history mechanism.
		SF_CheckBlockingAreas		= BIT8,		// Should the scenario users check to see if the point is within a blocking area CTaskUseScenario::ProcessPreFSM
												//  Usage is for ambient peds so if a blocking area is placed they will stop using the scenario
		SF_CheckConditions			= BIT9,		// If set, leave the scenario if the CScenarioCondition in CScenarioInfo is unsatisfied.
		SF_IdleForever				= BIT10,	// Ped will never leave this based on idletime
		SF_AdvanceUseTimeRandomly	= BIT11,	// For use when spawning in place, advance the use time for the scenario by a random
												// amount, as if the player came across the ped while it has already used the scenario for some time.
		// Panic flags
		SF_NeverUseRegularExit		= BIT12,	// If set, skip exit animations if there is no panic exits available.

		SF_UseCowardEnter									= BIT13,	// Whether or not this ped should try and use a coward enter when starting this scenario.
		SF_ForceHighPriorityStreamingForNormalExit			= BIT14,	// When a ped is playing their scenario exit, force streaming to be high priority.
		
		// Condition flags
		SF_IgnoreTimeCondition								= BIT15,	//Ignore the time condition.

		SF_ExitingHangoutConversation						= BIT16, 	// The ambient clips task is being forced out of a hangout clip. [8/7/2012 mdawe]
		SF_CanExitForNearbyPavement							= BIT17,	// Does this ped have a wanderable space nearby to exit to?
		SF_PerformedPavementFloodFill						= BIT18,	// Have we searched for nearby pavement here?
		SF_CanExitToChain									= BIT19,	// This ped is exiting to another chained scenario point and should bypass most checks for exit conditions.

		SF_ExitToScriptCommand								= BIT20,
		SF_ScenarioResume									= BIT21,
		SF_CapsuleNeedsToBeRestored							= BIT22,

		// Running Flags
		SFE_PlayedEnterAnimation	= BIT23,	// Set when the Ped task plays an Enter anim, and cleared on scenario exit.
		SF_DontLeaveThisFrame		= BIT24,		// If set by the user, don't time out and leave on the next update.
		SF_GotoPositionTaskSet		= BIT25,
		SF_ShouldAbort				= BIT26,
		SF_WouldHaveLeftLastFrame	= BIT27,	// Set when SF_DontLeaveThisFrame was set, but if it had not been, the task would have timed out.
		SF_SlaveNotReadyForSync		= BIT28,	// Set on the master, by slaves calling NotifySlaveNotReadyForSyncedAnim().
		SF_WasAddedToHistory		= BIT29,	// Set if SF_AddToHistory was set and we have actually added to the history (or determined that we're already there).

		SF_UNUSED_30										= BIT30,
		SF_UNUSED_31										= BIT31,

		// Sets of flags for different types of usages
		SF_FlagsForAmbientUsage		= SF_CheckBlockingAreas | SF_CheckConditions | SF_CheckShockingEvents,
		SF_LastSyncedFlag			= SF_ScenarioResume
	};

#if __BANK
	// Scenario Debug Flags
	enum
	{
		// Debug flags
		SFD_UseDebugAnimData		= BIT0,
		SFD_WantNextDebugAnimData	= BIT1,
		SFD_SkipPanicIntro			= BIT2, //Used for playing AT_PANIC_BASE clips with debug clip functionality.
		SFD_SkipPanicIdle			= BIT3, //Used for playing AT_PANIC_OUTRO clips with debug clip functionality.
	};
#endif

	// Scenario Temporary Reaction Flags
	enum
	{
		SREF_RespondingToEvent			= BIT0,

		SREF_AggroAgitation				= BIT1,
		SREF_CowardAgitation			= BIT2,
		
		SREF_CowardReaction				= BIT3,

		SREF_ExitScenario				= BIT4,

		SREF_AllowEventsToKillThisTask	= BIT5,

		SREF_AgitationWithNormalExit	= BIT6,

		SREF_ExitingToCombat			= BIT7,	// The task is exiting for the ped to go into combat.

		SREF_ExitingToFlee				= BIT8,	// The task is being forced to exit to a flee.

		SREF_TempInPlaceReaction		= BIT9,

		SREF_ScriptedCowerInPlace		= BIT10,	// Script is telling this ped to cower in place.

		SREF_WaitingOnBlackboard		= BIT11,	// The ped has picked a valid reaction clipset and clip, but must wait until the blackboard system says it can play it.

		SREF_DirectedScriptExit			= BIT12,	// Playing a normal exit, but directed for script.

		SREF_LastSyncedFlag = SREF_ScriptedCowerInPlace,

		SREF_AggroOrCowardAgitationReaction	= SREF_CowardAgitation | SREF_AggroAgitation,
		SREF_AggroOrCowardReaction = SREF_AggroOrCowardAgitationReaction | SREF_CowardReaction,
	};

	// Scenario Exit clip Flags - 
	enum
	{
		// PURPOSE:	True if m_exitClipSetId identifies a clip set for exit animations which should
		//			be played back as a synchronized scene.
		eExitClipsAreSynchronized	= BIT0,

		// PURPOSE:	True if m_exitClipSetId and other variables relating to the exit animations
		//			have been determined, i.e. if we have made a decision on which exit animations to use.
		eExitClipsDetermined		= BIT1,

		// PURPOSE:	True if m_exitClipSetId identifies a clip set for exit animations which end in a walk.
		eExitClipsEndsInWalk		= BIT2,

		// PURPOSE:	True if LeaveScenario() was called and requested the exit clips to be
		//			stream in, ever. Used so the slave knows to start streaming them in once
		//			the master is thinking about leaving.
		eExitClipsRequested			= BIT3,
	};

	// Scenario Mode
	enum eScenarioMode
	{
		SM_EntityEffect,
		SM_WorldEffect,
		SM_WorldLocation
	};

	// FSM states
	enum
	{
		State_Start = 0,
		State_GoToStartPosition,
		State_Enter,
		State_PlayAmbients,
		State_Exit,
		State_AggroOrCowardIntro,
		State_AggroOrCowardOutro,
		State_WaitingOnExit,
		State_StreamWaitClip,
		State_WaitForProbes,
		State_WaitForPath,
		State_PanicExit,
		State_NMExit,
		State_PanicNMExit,
		State_ScenarioTransition,
		State_Finish
	};

	// Constructors

	// Just start the scenario in place
	CTaskUseScenario(s32 iScenarioIndex, s32 iFlags = 0);

	// Specify the scenario location directly
	CTaskUseScenario(s32 iScenarioIndex, const Vector3& vPosition, float fHeading, s32 iFlags = 0);

	// Specify the scenario location by entity and effect (entity may have several spawn points)
	//  NOTE: Specify the scenario location by effect (world location) by using a scenarioPoint that has not entity attachement
	CTaskUseScenario(s32 iScenarioIndex, CScenarioPoint* pScenarioPoint, s32 iFlags = 0);

	// Copy constructor
	CTaskUseScenario(const CTaskUseScenario& other);

	// Destructor
	~CTaskUseScenario();

	virtual void CleanUp();
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	
	// PURPOSE: called to process animation triggers - creating/destroying/dropping props
	virtual bool ProcessMoveSignals();

	// Task required implementations
	virtual aiTask* Copy() const { return rage_new CTaskUseScenario(*this); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_USE_SCENARIO;}
	virtual s32 GetDefaultStateAfterAbort() const	{ return State_Start; }

	virtual u32 GetAmbientFlags();

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	//Allow overriding of the clipset and clip that will be used as the base clip...
	// this is passed down to the CTaskAmbientClips task when it is created as the base anim data is not used here
	// except for pre-streaming the clip set.
	void OverrideBaseClip(fwMvClipSetId m_ClipSetId, fwMvClipId m_ClipId);

	// PURPOSE:	Override the speed to use for approaching the scenario.
	void OverrideMoveBlendRatio(float mbr)			{ m_OverriddenMoveBlendRatio = mbr; }

	// Copy a prop management helper. Use this to transistion ownership of a prop helper from one CTaskUseScenario to another. [1/16/2013 mdawe]
	void SetPropManagmentHelper(const CPropManagementHelper::SPtr propHelper) { m_pPropHelper = propHelper; }
	const CPropManagementHelper::SPtr GetPropManagementHelper() const { return m_pPropHelper; }

	inline virtual void	SetTimeToLeave(float timeToLeave)	{ taskAssertf(timeToLeave <= fMAX_IDLE_TIME, "This value should be sync with CClonedTaskUseScenarioInfo::SIZEOF_IDLE_TIME"); m_fIdleTime = timeToLeave; }
    inline void SetDontLeaveEver()					{ m_iFlags.SetFlag(SF_IdleForever); }
	inline void SetDontLeaveThisFrame()				{ m_iFlags.SetFlag(SF_DontLeaveThisFrame); }
	inline void SetIgnoreTimeCondition()			{ m_iFlags.SetFlag(SF_IgnoreTimeCondition); }
	inline void SetShouldAbort()					{ m_iFlags.SetFlag(SF_ShouldAbort); }
	inline bool GetWouldHaveLeftLastFrame() const	{ return m_iFlags.IsFlagSet(SF_WouldHaveLeftLastFrame); }
	void SetCanExitToChainedScenario();
	
	// PURPOSE:	Static helper function for setting or resetting CPED_CONFIG_FLAG_CowerInsteadOfFlee
	//			and other things (combat movement).
	static void SetStationaryReactionsEnabledForPed(CPed& ped, bool enabled, atHashWithStringNotFinal sStationaryReactHash=atHashWithStringNotFinal("CODE_HUMAN_STAND_COWER",0x161530CA));

	static bool SpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed);

	// PURPOSE: For a player switch, find scenario peds in the area that are not yet streamed in and ready
	//			Should be coupled with a timeout at a higher level to ensure we don't get stuck forever
	static bool AreAnyPedsInAreaStreamingInScenarioAnims(Vec3V_In pos, ScalarV_In r);

	void SetAdditionalTaskDuringApproach(const CTask* task);

	// PURPOSE:	Set who the partner should be, if synchronizing animations
	void SetSynchronizedPartner(CPed* pPed)			{ m_SynchronizedPartner = pPed; }
	
	bool GetExitClipsDetermined() const { return m_iExitClipsFlags.IsFlagSet(eExitClipsDetermined); }
	bool GetExitClipsEndsInWalk() const { taskAssert(m_iExitClipsFlags.IsFlagSet(eExitClipsDetermined) ); return m_iExitClipsFlags.IsFlagSet(eExitClipsEndsInWalk); }

	const CConditionalAnimsGroup* GetConditionalAnimsGroup() const { return m_pConditionalAnimsGroup; }
	const s32 GetConditionalAnimIndex() const { return m_ConditionalAnimsChosen; }

	virtual const CEntity* GetTargetPositionEntity() const
	{ 
		if(GetSubTask() && !m_Target.GetEntity())
		{
			return GetSubTask()->GetTargetPositionEntity();
		}

		return m_Target.GetEntity();
	}

	bool IsEntering() const
	{
		return (GetState() == State_Enter);
	}

	bool IsExiting() const
	{
		switch(GetState())
		{
			case State_Exit:
			case State_NMExit:
			case State_PanicNMExit:
			case State_PanicExit:
			{
				return true;
			}
			default:
			{
				return false;
			}
		}
	}

	bool IsExitingAndHasANewPosition() const { return ((m_bHasExitStartPos && (GetState() == State_WaitForPath)) || (GetState() == State_PanicExit)); };	// Possibly in more states!
	void GetExitPosition(Vector3& vStartPos) const { m_PlayAttachedClipHelper.GetTargetPosition(vStartPos); }

#if SCENARIO_DEBUG
	inline bool ReadyForNextOverride() const	{ return m_iDebugFlags.IsFlagSet(SFD_WantNextDebugAnimData); }
	void SetOverrideAnimData(const ScenarioAnimDebugInfo& info);
	void ShouldIgnorePavementChecksWhenLeavingScenario(const bool bShouldIgnore) { m_bIgnorePavementCheckWhenLeavingScenario = bShouldIgnore; }
#endif // SCENARIO_DEBUG

	////////////////////////////////////////////////////////////////////////////////
	// Prop helper functions.
	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Overridden from CTask. Works in conjunction with querying the task tree for any
	//			prop management helpers currently in use.
	virtual CPropManagementHelper* GetPropHelper() { return m_pPropHelper; }
	virtual const CPropManagementHelper* GetPropHelper() const { return m_pPropHelper; }

	// PURPOSE:	Get the prop from the environment to use, if any.
	CObject* GetPropInEnvironmentToUse() const { if (m_pPropHelper) { return m_pPropHelper->GetPropInEnvironment(); } else { return NULL; } }

	// PURPOSE:	Directly specify a prop that should be used with this scenario.
	void SetPropInEnvironmentToUse(CObject* pProp) { if (m_pPropHelper) { m_pPropHelper->SetPropInEnvironment(pProp); } }

	// PURPOSE:	Stand-alone helper function for looking for a suitable prop for a given scenario point.
	static CObject* FindPropInEnvironmentNearPoint(const CScenarioPoint& point, int realScenarioType, bool allowUprooted);

	// PURPOSE:	Stand-alone helper function for looking for a suitable prop near a position.
	static CObject* FindPropInEnvironmentNearPosition(Vec3V_In scanPos, int realScenarioType, bool allowUprooted);

	///////////////////////////////////////////////////////////////////////////////////
	// Temporary reaction helper functions.
	// These are called by other tasks to change how CTaskUseScenario runs in some way.
	///////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:  Play a small shocking reaction animation.
	void						TriggerShockAnimation(const CEvent& rEvent);

	// PURPOSE:  Do head IK.
	void						TriggerHeadLook(const CEvent& rEvent);

	// PURPOSE:  Have the ped cower in place until the next script command is given to them.
	void						TriggerScriptedCowerInPlace();

	// PURPOSE:  Have the ped stop cowering in place to respond to some new stimuli.
	void						TriggerStopScriptedCowerInPlace();

	// PURPOSE:  Helper function to true if the ped using this scenario has responded to this event recently.
	bool						HasRespondedToRecently(const CEvent& rEvent);

	// PURPOSE:  Helper function to return true if this scenario point has a valid shocking reaction animation.
	bool						HasAValidShockingReactionAnimation(const CEvent& rEvent) const;

	// PURPOSE:  Helper function to return true if this scenario point has a valid coward reaction animation.
	bool						HasAValidCowardReactionAnimation(const CEvent& revent) const;

	// PURPOSE:  Helper function to return true if this scenario point has a valid normal exit animation.
	bool						HasAValidNormalExitAnimation() const;

	// PURPOSE:  Cache off the threat response decision made so that if the decision has to be made again later it can be consistent.
	void						CacheOffThreatResponseDecision(CTaskThreatResponse::ThreatResponse iResponse) { m_iCachedThreatResponse = iResponse; }

	// PURPOSE:  Helper function to check if we have triggered an exit
	bool						HasTriggeredExit() const;

	// PURPOSE:  Return true if the ambient position has yet to be set by script or code.
	bool						IsAmbientPositionInvalid() const;

	// PURPOSE:  Set the stored position to be the passed in position.
	void						SetAmbientPosition(Vec3V_In vPos);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Scenario exit triggers.
	// These are called by tasks/actions/script to cause the ped to leave the scenario point and quit the task.
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:  Tell the ped to do an aggro reaction and then exit the task.
	void						TriggerAggroThenExit(CPed* pPedAgitator);

	// PURPOSE:  Start a coward reaction and then exit the scenario, afterwards starting the event response for the task.
	void						TriggerCowardThenExitToEvent(const CEvent& rEvent);

	// PURPOSE:  Set should abort to exit normally.
	void						TriggerNormalExit();

	// PURPOSE:  Set should abort to exit normally, but allow for a chosen direction to play the animations towards.
	void						TriggerDirectedExitFromScript();

	// PURPOSE:  Set should abort to exit normally, but ignore wander pavement checks.
	void						TriggerNormalExitFromScript();

	// Common code utilized by both normal and directed script exits.
	void						TriggerNormalScriptExitCommon();

	// PURPOSE:  Do a flee exit on the next script command.
	void						TriggerFleeExitFromScript();

	// PURPOSE:  Set should abort to exit normally, but do so as flagged for disputes.
	void						TriggerExitForCowardDisputes(bool bFast, CPed* pPedAgitator);

	// PURPOSE:  Set should abort to do a flee exit flagged for disputes.
	void						TriggerFleeExitForCowardDisputes(Vec3V_In vPos);

	// PURPOSE:  Play the regular exit and then respond to the event like a normal ped.
	void						TriggerNormalExitToEvent(const CEvent& rEvent);

	// PURPOSE:  Set the flag to allow the ped to allow any event to cause the scenario ped to immediately quit.
	// The ped will only exit if they are currently responding to some event.
	void						TriggerImmediateExitOnEvent();

	//  PURPOSE:  Set the flags for panicking.
	void						TriggerExitToFlee(const CEvent& rEvent);

	//  PURPOSE:  Set the flags to exit for combat.
	void						TriggerExitToCombat(const CEvent& rEvent);

	// PURPOSE: Called by script to force a ped to panic flee a scenario.
	//			vAmbientPosition - which direction to react towards
	void						ScriptForcePanicExit(Vec3V_In vAmbientPosition, bool skipOutroIfNoPanic);

	// PURPOSE: Removes all the set ped config flags that direct how peds exit scenarios. 
	static void					ClearScriptedTriggerFlags(CPed& ped);

	void SetExitingHangoutConversation() { m_iFlags.SetFlag(SF_ExitingHangoutConversation); }

	// getter/setter/checker
	inline void SetPlayedEnterAnimation() { m_iFlags.SetFlag(SFE_PlayedEnterAnimation); }
	inline void ClearPlayedEnterAnimation() { m_iFlags.ClearFlag(SFE_PlayedEnterAnimation); }
	inline bool HasPlayedEnterAnimation() { return m_iFlags.IsFlagSet(SFE_PlayedEnterAnimation); }
	void SetDoCowardEnter()					{ m_iFlags.SetFlag(SF_UseCowardEnter);	}
	bool ShouldUseCowardEnter()				{ return m_iFlags.IsFlagSet(SF_UseCowardEnter); }
	void SetIsDoingResume()					{ m_iFlags.SetFlag(SF_ScenarioResume); }
	bool IsAScenarioResume() const			{ return m_iFlags.IsFlagSet(SF_ScenarioResume); }

	// PURPOSE:	Get the task's tunables.
	static const Tunables& GetTunables() { return sm_Tunables; }

	// helper for network migrating Clone to local
	void SetUpMigratingCloneToLocalCowerTask(CClonedFSMTaskInfo* pTaskInfo);
	void NetworkSetUpPedPositionAndAttachments();

	void SetMigratingCowerExitingTask()		{	mbOldTaskMigratedWhileCowering = true;	}
	bool GetMigratingCowerExitingTask()		const {	return mbOldTaskMigratedWhileCowering ;	}
	void SetMigratingCowerStartingTask()	{	mbNewTaskMigratedWhileCowering = true;	}
	bool GetMigratingCowerStartingTask()	const { return mbNewTaskMigratedWhileCowering; }

	// Reset static variables dependent on fwTimer::GetTimeInMilliseconds()
	static void ResetStatics()				{ sm_NextTimeAnybodyCanGetUpMS = 0; }

	static void InitTunables(); 

private:
	friend class CTaskUseScenarioEntityExtension;

	bool CheckAttachStatus();
	bool ShouldBeAttachedWhileUsing() const;

	// PURPOSE:	Call SetIgnoreLowPriShockingEvents() with true or false, matching the
	//			IgnoreLowShockingEvents flag in the current scenario.
	void SetIgnoreLowPriShockingEventsFromScenarioInfo(const CScenarioInfo& rInfo);

	// PURPOSE:	Specify whether we should be ignoring low priority shocking events or not.
	//			This updates a count in CPedIntelligence, so it's important that it's not bypassed.
	void SetIgnoreLowPriShockingEvents(bool b);

	// PURPOSE:	Helper function to call SetStationaryReactionsEnabledForPed() with true or
	//			false, depending on the StationaryReactions flag in the current point (if it exists).
	void SetStationaryReactionsFromScenarioPoint(const CScenarioInfo& rInfo);

	// PURPOSE:	Helper function to set CPED_CONFIG_FLAG_UsingScenario.
	void SetUsingScenario(bool b);

	// PURPOSE: Check to see if normal scenario exits have been globally disabled.
	bool CanAnyScenarioPedsLeave() const;

	// PURPOSE:	Check to see if it's too soon to leave, because some other ped just left.
	bool IsTooSoonToLeaveAfterSomebodyElseLeft() const;

	// PURPOSE: Return true if the ped may exit due to timeout - possibly they may want to finish their idle anim first.
	// PARAMS:	timedExit - True if we are actually considering leaving due to timeout of the use time, as opposed to a change in conditions, etc.
	bool ReadyToLeaveScenario(bool timedExit);

	// PURPOSE:	Update sm_NextTimeAnybodyCanGetUpMS for when the next scenario user can leave.
	void SetTimeUntilNextLeave();

	// PURPOSE:  Sets the appropriate flags necessary whenever the ped using the scenario is temporarily responding to some event.
	void						StartEventReact(CEvent& rEvent);

	// PURPOSE:  Clears the appropriate flags whenever the ped is done temporarily reacting to some event.
	void						EndEventReact();

	// PURPOSE:  Sets flags telling the ped how to exit the scenario for a flee.
	void						SetPanicFleeFlags();

	// PURPOSE:  Sets flags tellign the ped hwo to exit the scenario for combat.
	void						SetCombatExitFlags();

	// PURPOSE:  Return true if the ambient position for the reaction should be towards the source entity.
	bool						ShouldRespondToSourceEntity(int iEventType) const;

	//	PURPOSE:  Set the stored position based on that of the event being reacted to.
	void						SetAmbientPosition(const aiEvent* pEvent);

	// PURPOSE:  Set the stored position to be the ped's current position.
	void						SetAmbientPosition(CPed* pPed);

	// PURPOSE:  Return true if the ped using the scenario is actively responding to some event.
	bool						IsRespondingToEvent() { return m_iScenarioReactionFlags.IsFlagSet(SREF_RespondingToEvent); }

	// PURPOSE:  Start a navmesh task on the ped during the panic animation, so that when the ped is done playing the anim they can move onto it for SmartFlee.
	void						StartNavMeshTask();

	// PURPOSE:  Check the navmesh task started on the ped so that it does not get deleted.
	void						KeepNavMeshTaskAlive();

	// PURPOSE:  Tell the ped using the scenario to look at the agitation entity/position for a specified amount of time.
	void						LookAtTarget(float fTime);

	//Disable idle turns and fixup the ped's desired heading during animations in certain states.
	void						HandleAnimatedTurns();

	// PURPOSE:  Toggle fixed physics on the scenario entity (if it exists).
	void						ToggleFixedPhysicsForScenarioEntity(bool bFixed);

	// PURPOSE:  Check and process ScenarioInfoFlags that set properties of the ped when PlayAmbients_OnEnter() runs.
	void						HandleOnEnterAmbientScenarioInfoFlags();


	// Bound offset helper functions.

	// PURPOSE:  Move the ped bound according to the offset stored in the scenario info.
	// bInitial = adjust the bound forward by the offset, false restores the ped to its state before the bound was adjusted.
	void						ManagePedBoundForScenario(bool bInitial);

	// Cowering helper functions.
	float						GetTimeToCower(int iEventType);

#if !HACK_RDR3
	// DO NOT PORT THIS CODE TO PROJECTS OTHER THAN GTAV
	// See B* 1745645
	bool						StationaryReactionsHack();
#endif
		
	// When the scenario point is removed, check to see if this ped should also be destroyed.
	void						FlagToDestroyPedIfApplicable();

	// Returns the scenario transition info for current m_pConditionalAnimsGroup and m_ConditionalAnimsChosen.
	// Returns NULL if the info does not exist or is invalid.
	const CScenarioTransitionInfo* GetScenarioTransitionInfoFromConditionalAnims() const;

	// Requests the transition animation clipset and clip to be streamed in based on the CScenarioTransitionInfo.
	// Clears our SF_Warp flag and sets our SF_StartInCurrentPosition flag.
	// Sets up prop helper for transition animation.
	bool SetupForScenarioTransition(const CScenarioTransitionInfo* pTransitionInfo);

	// If we haven't already, start playing the Transition clip
	// This uses INSTANT_BLEND_IN_DELTA and CClipHelper::TerminationType_OnDelete.
	void PlayScenarioTransitionClip();

	// Returns true if the chain info is valid and new m_pConditionalAnimsGroup and m_ConditionalAnimsChosen have been set by this method.
	bool UpdateConditionalAnimsGroupAndChosenFromTransitionInfo(const CScenarioTransitionInfo* pTransitionInfo);

	// Helper function to see if an event requires a valid target.
	static bool EventRequiresValidTarget(s32 iEventType);

	void OnLeavingScenarioDueToAgitated();
	void SetLastUsedScenarioPoint();

	// Handle the ped capsule during exits.
	void TogglePedCapsuleCollidesWithInactives(bool bCollide);

private:
	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);	
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
    virtual bool                IsInScope(const CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	CTaskAmbientMigrateHelper*	GetTaskAmbientMigrateHelper();
	CScenarioPoint*				m_pClonesParamedicScenarioPoint;
	void						CheckKeepCloneCowerSubTaskAtStateTransition(); //helper to check on State transitions that clones keep the sub task after transition

	bool						IsMedicScenarioPoint() const;

	bool						CreateMedicScenarioPoint(Vector3 &vPosition, float &fDirection, int iType);


	

	// Getups
	virtual bool				UseCustomGetup() { return m_iScenarioReactionFlags.IsFlagSet(SREF_ExitingToFlee); }
	virtual bool				AddGetUpSets (CNmBlendOutSetList& sets, CPed* pPed);


	// State Machine Update Functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			ProcessPostFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool				ProcessPostMovement();

	bool						IsInProcessPostMovementAttachState();

	// Local state function calls
	FSM_Return		 			Start_OnEnter				();
	FSM_Return					Start_OnUpdate				();
	void						Start_OnExit				();
	void						GoToStartPosition_OnEnter	();
	FSM_Return					GoToStartPosition_OnUpdate	();
	void						GoToStartPosition_OnExit	();
	void						Enter_OnEnter				();
	FSM_Return					Enter_OnUpdate				();
	void						PlayAmbients_OnEnter		();
	FSM_Return					PlayAmbients_OnUpdate		();
	void						PlayAmbients_OnExit			();
	void						Exit_OnEnter				();
	FSM_Return					Exit_OnUpdate				();
	void						Exit_OnExit					();
	void						AggroOrCowardIntro_OnEnter	();
	FSM_Return					AggroOrCowardIntro_OnUpdate	();
	void						AggroOrCowardIntro_OnExit	();
	void						AggroOrCowardOutro_OnEnter	();
	FSM_Return					AggroOrCowardOutro_OnUpdate	();
	void						AggroOrCowardOutro_OnExit	();
	void						WaitingOnExit_OnEnter		();
	FSM_Return					WaitingOnExit_OnUpdate		();
	void						WaitingOnExit_OnExit		();
	void						StreamWaitClip_OnEnter		();
	FSM_Return					StreamWaitClip_OnUpdate		();
	void						StreamWaitClip_OnExit		();
	void						WaitForProbes_OnEnter		();
	FSM_Return					WaitForProbes_OnUpdate		();
	void						WaitForPath_OnEnter			();
	FSM_Return					WaitForPath_OnUpdate		();
	void						PanicExit_OnEnter			();
	FSM_Return					PanicExit_OnUpdate			();
	void						PanicExit_OnExit			();
	void						NMExit_OnEnter				();
	FSM_Return					NMExit_OnUpdate				();
	void						PanicNMExit_OnEnter			();
	FSM_Return					PanicNMExit_OnUpdate		();
	void						ScenarioTransition_OnEnter	();
	FSM_Return					ScenarioTransition_OnUpdate	();

	bool						AreBaseClipsStreamed();
	bool						AreEnterClipsStreamed();
	void						CalculateStartPosition( Vector3 & pedPosition, float & pedRotation ) const;
	float						CalculateBaseHeadingOffset() const;
	bool						ConsiderOrientation(const CPed* pPed, const Vector3& vWorldPosition, float fScenarioHeading, fwMvClipSetId clipSetId, fwMvClipId& chosenClipId) const;
	float						GetClipOrientationProperty(const crClip* pClip) const;
	
	void						GetScenarioPositionAndHeading(Vector3& vWorldPosition, float& fWorldHeading) const;
	void						UpdateTargetSituation(bool bAdjustFallStatus=true);
	void						Attach(const Vector3& vWorldPosition, const Quaternion& vWorldOrientation);
	bool						IsDynamic() const;
	void						SetGoToAnimationTaskSettings(CTaskGoToScenario::TaskSettings& settings);

	void						ResetConditionalAnimsChosen()  {m_ConditionalAnimsChosen=-1;}  //When copying tasks reset this when copied task starts from initial state
	void						ClearChosenClipId()  {SetChosenClipId(CLIP_ID_INVALID, (CPed*)GetEntity(), this);}//When copying tasks reset this when copied task starts from initial state

	// PURPOSE:	If m_ConditionalAnimsChosen hasn't been set yet, try to update it from a running CTaskAmbientClips subtask.
	void						DetermineConditionalAnims();

	// PURPOSE: Returns the enter clipset this ped should use to enter the point.
	fwMvClipSetId				DetermineEnterAnimations(CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalClipSet** ppOutConClipSet = NULL, u32* const pOutPropSetHash = NULL);

	// PURPOSE:	If eExitClipsDetermined, m_exitClipSetId, etc haven't been set already, try to make a decision now
	//			in terms of which exit animations to use.
	void						DetermineExitAnimations(bool bIsEventReaction=false);

	// PURPOSE:	Reset eExitClipsDetermined and other variables related to the exit animations to use.
	void						ResetExitAnimations();

private:

	void						CalculateTargetSituation(Vector3& vTargetPosition, Quaternion& qTargetOrientation, fwMvClipSetId clipSetId, fwMvClipId clipId, bool bAdjustFallStatus);
	void						HandlePanicSetup();

	// Guess at the next phase for the currently playing clip based on the timestep.
	float						ComputeNextPhase(const CClipHelper& clipHelper, const crClip& clip) const;

	// Search the clip for Interruptible tags, and see if the phase supplied exceeds them.
	bool						CheckForInterruptibleTag(float fPhase, const crClip& clip) const;

	//	If we want to stream in a wait clip before waiting on the probe checks.
	bool						ShouldStreamWaitClip();

	// Slightly different from the above - when entering the wait for probe state do we want to create a new cowering subtask?
	// One might already exist.
	bool						ShouldCreateCoweringSubtaskInWaitForProbes() const;

	// Make a cowering subtask to play while waiting for probes.
	void						CreateCoweringSubtask();

	// Look at the reaction flags and decide how to exit the point.
	s32							DecideOnStateForExit() const;

	// PURPOSE:  Disable agitation for the ped entirely.
	void						ForgetAboutAgitation();
	
	// PURPOSE:  Override the streaming priority to be high so the ped is guaranteed to be able to stream in their animation - otherwise they will appear not to react in some high-demand situations
	void						OverrideStreaming();
	void						LeaveScenario(bool bAllowClones=false);
	
	// PURPOSE:  Find the clip in the clipset that avoids collision, and if specified, rotates to the target.
	CScenarioClipHelper::ScenarioClipCheckType		PickClipFromSet(const CPed* ped, fwMvClipSetId clipSetId, fwMvClipId& chosenClipId, 
		bool bRotateToTarget, bool bDoProbeChecks, bool bForceOneFrameCheck);
	CScenarioClipHelper::ScenarioClipCheckType		PickReEnterClipFromSet(const CPed* pPed, fwMvClipSetId clipSetId, fwMvClipId& chosenClipId);
	void											PickEntryClipIfNeeded();

	// PURPOSE:  Modify vWorldPosition and fWorldHeading to account for the entrypoint of the closest matching entryclip.
	// The best matching clip is the one with the closest start orientation to that of the ped.
	// Returns true if the target is valid.
	bool						AdjustTargetForClips(Vector3& vWorldPosition, float& fWorldHeading);

	void						AdjustGotoPositionForPedHeight(Vector3& vWorldPosition);

	// PURPOSE: Start a TaskGoToScenario subtask to move towards an entry point.
	void						StartInitialGotoSubtask(const Vector3& vWorldPosition, float fWorldHeading);

	// PURPOSE: Return if the ped is close enough to the scenario point to run a final goto task.
	bool 						CloseEnoughForFinalApproach() const;

	// PURPOSE: Return true if the scenario point is obstructed
	bool						IsScenarioPointObstructed(const CScenarioPoint* pScenarioPoint) const;

	// PURPOSE: Helper function to determine if the ped is using a prop scenario with no entry animations (a bad situation to be in).
	bool						IsUsingAPropScenarioWithNoEntryAnimations() const;

	// PURPOSE:	Look around the scenario to see if there is a prop we can use, and if so, return it.
	CObject*					FindPropInEnvironment() const;

	// PURPOSE:	Callback used by FindPropInEnvironment().
	static bool					FindPropInEnvironmentCB(CEntity* pEntity, void* pData);

	void						DisposeOfPropIfInappropriate();

	// PURPOSE:	Initialize an sScenarioConditionData object with values appropriate for this task. 
	void						InitConditions(CScenarioCondition::sScenarioConditionData& conditionDataOut) const;

	// PURPOSE:	Attempt to create a new synchronized scene with this scenario as its origin, storing its
	//			ID in m_SyncedSceneId if successful.
	bool						CreateSynchronizedSceneForScenario();

	// PURPOSE:	Release the reference to the scene identified by m_SyncedSceneId, if any.
	void						ReleaseSynchronizedScene();

	// PURPOSE:	Start playing a synchronized animation, using CTaskSynchronizedScene as a subtask.
	bool						StartPlayingSynchronized(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float blendInDelta, bool tagSyncOut);

	// PURPOSE:	If set up to be the slave in a synchronized scenario, look for the corresponding task
	//			running on the master ped.
	CTaskUseScenario*			FindSynchronizedMasterTask() const;

	// PURPOSE:	Try to find the blend-out phase of a clip (probably to be used for a synchronized exit animation).
	float						FindSynchronizedSceneBlendOutPhase(const crClip& clip) const;

	// PURPOSE:	Called on the master's task by a slave, to tell the master that the
	//			slave is not ready yet to play a synchronized animation.
	void						NotifySlaveNotReadyForSyncedAnim()
	{
		taskAssert(m_iFlags.IsFlagSet(SF_SynchronizedMaster));
		m_iFlags.SetFlag(SF_SlaveNotReadyForSync);
	}

	// PURPOSE:  When exiting to a small fall, update the target situation so the exit clip interpolates towards the ground.
	void						ReassessExitTargetForSmallFalls();

	// PURPOSE:  When exiting to a fall, return if the ped can stop playing an exit animation.
	bool						EndBecauseOfFallTagging();

	// PURPOSE:  Give the ped a ragdoll event to terminate the scenario usage.
	// This is done when a ped's scenario entity has been moved.
	// Return true if the ragdoll event was successfully added to the ped.
	bool						RagdollOutOfScenario(); 


	// PURPOSE:	Choose a conditional clip set to use for this task.
	fwMvClipSetId									ChooseConditionalClipSet(CConditionalAnims::eAnimType animType, CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalClipSet** ppOutConClipSet = NULL, u32* const pOutPropSetHash = NULL) const;
	CScenarioClipHelper::ScenarioClipCheckType		ChooseClipInClipSet(CConditionalAnims::eAnimType animType, const fwMvClipSetId& clipSet, fwMvClipId& outClipId);
	
	bool						IsEventConsideredAThreat(const CEvent& rEvent) const;
	bool						IsLosClearToThreatEvent(const CEvent& rEvent) const;

	void StartNewScenarioAfterHangout();

	bool HasNearbyPavement();

	float	RandomizeRate(float fMinRate, float fMaxRate, float fMinDifferenceFromLastRate, float& fLastRate) const;
	void	RandomizeRateAndUpdateLastRate();
	void	RandomizeCowerPanicRateAndUpdateLastRate();

	// Returns the priority slot that the exit clip should use when playing (which is determined by matching whatever slot the ambient clips task is using).
	s32		GetExitClipPriority();

	bool AttemptToPlayEnterClip();

	// PURPOSE:	Notify the task that the CDummyObject we are attached to has turned into a CObject again,
	//			so we can re-attach properly.
	void NotifyNewObjectFromDummyObject(CObject& obj);

	// PURPOSE:	Update the pointer we store to a CDummyObject, and maintain extensions accordingly.
	void SetDummyObjectForAttachedEntity(CDummyObject* pDummyObject);

	CPlayAttachedClipHelper m_PlayAttachedClipHelper;

	// PURPOSE:  Position of the ambient event we are reacting to
	CAITarget				m_Target;

	Vector3					m_vPosition;				// Reused for intro/idle position

	CPrioritizedClipSetRequestHelper m_ClipSetRequestHelper;
	fwClipSetRequestHelper m_EnterClipSetRequestHelper;

	const CConditionalAnimsGroup * m_pConditionalAnimsGroup;

	// PURPOSE:	Pointer to the CDummyObject for the entity we are attached to.
	RegdDummyObject		m_DummyObjectForAttachedEntity;

	// PURPOSE:	Relative position (XYZ) and scenario type index (W), for binding when we go from CDummyObject->CObject.
	Vec4V				m_DummyObjectScenarioPtOffsetAndType;

	// PURPOSE:	If synchronizing animations, this is the partner
	RegdPed				m_SynchronizedPartner;

	eScenarioMode		m_eScenarioMode;

	float				m_fIdleTime;				// time to stay in play ambients / idle 
	float				m_fHeading;					// Reused for intro/idle heading
	float				m_fBaseHeadingOffset;		// Difference between base heading and scenario heading
	float				m_fBlockingAreaCheck;		// Time since last check of scenario blocking areas

	// PURPOSE:	Helper to hold on to a prop when CTaskAmbientClips isn't running.
	CPropManagementHelper::SPtr	m_pPropHelper;

	RegdConstAITask		m_pAdditionalTaskDuringApproach;

	// Enter and exit clip dictionaries
	fwMvClipSetId		m_baseClipSetId;
	fwMvClipSetId		m_enterClipSetId;
	fwMvClipSetId		m_exitClipSetId;

	fwMvClipSetId		m_panicClipSetId;

	//Overrides
	fwMvClipSetId		m_OverriddenBaseClipSet;
	fwMvClipId			m_OverriddenBaseClip;

	// PURPOSE:  Store the panic base clipset.
	// As this is streamed in while the panic intro is still playing, they need to be separate variables.
	fwMvClipSetId		m_panicBaseClipSetId;

	fwMvClipId			m_ChosenClipId;			// used for the entry and exit clips since only one will be used at a time

	CScenarioClipHelper	m_ExitHelper;

	// PURPOSE:	The phase at which we may start blending out a synchronized exit animation.
	float				m_SyncedSceneBlendOutPhase;

	// PURPOSE:	If set (>= 0.0f), this is the speed to use when moving to this scenario.
	float				m_OverriddenMoveBlendRatio;

	// PURPOSE:  How long ago the ped responded to the last event.
	float				m_fLastEventTimer;

	float				m_fLastPavementCheckTimer;

	// path blocking handle
	TDynamicObjectIndex m_TDynamicObjectIndex;

	CPavementFloodFillHelper m_PavementHelper;

	// PURPOSE:  Make sure that if we exit to threat response, it keeps the same response that we initially saw.
	CTaskThreatResponse::ThreatResponse	m_iCachedThreatResponse;

	fwFlags32			m_iFlags;
	s32					m_ConditionalAnimsChosen;
	u32					m_uCachedBoundsFlags;

	// PURPOSE:	If synchronizing animations with another ped (with us as the master), this
	//			is the ID for a synchronized scene we have created.
	fwSyncedSceneId		m_SyncedSceneId;

	// PURPOSE:  Which event triggered the ped to panic.
	s16					m_iEventTriggeringPanic;

	// PURPOSE:  The priority value of the event triggering the panic.
	s16					m_iPriorityOfEventTriggeringPanic;

	// PURPOSE:  What the last event responded to was.
	s16					m_iLastEventType;
	
	// Control temporary reactions while in the scenario.
	fwFlags16			m_iScenarioReactionFlags;

	// Control exit clip flags.
	fwFlags8			m_iExitClipsFlags;

	// PURPOSE: Frame counter to delay shifting the bounds in PlayAmbients_OnUpdate()
	s8					m_iShiftedBoundsCountdown;

	// PURPOSE:	True if we're currently ignoring low priority shocking events, due to this task.
	//			In that case, we owe CPedIntelligence a call to EndIgnoreLowPriShockingEvents().
	bool				m_bIgnoreLowPriShockingEvents:1;

	// PURPOSE;	True if this task may require cleanup such as detaching or re-enabling collisions.
	bool				m_bNeedsCleanup:1;

	// PURPOSE:	Used on slaves to control calls to NotifySlaveNotReadyForSyncedIdleAnim().
	bool				m_bSlaveReadyForSyncedExit:1;

	// PURPOSE:	Used to determine if the clipset should randomize or use orientation to pick a animation
	bool				m_bConsiderOrientation:1;

	// PURPOSE: Has the ped already started a final approach to the scenario point.
	bool				m_bFinalApproachSet:1;

	// PURPOSE Is the ped exiting to a low fall (i.e. should reassess the ground at a certain phase of the exit anim).
	bool				m_bExitToLowFall:1;

	// PURPOSE: Is the ped exiting to a high fall (i.e. should detach at a certain phase of the exit anim and let the in-air event take over).
	bool				m_bExitToHighFall:1;

	// 
	bool				m_bHasExitStartPos:1;
	
	// PURPOSE: Let us know that we are continuing into a new CConditionalAnimation inside the same CConditionalAnimsGroup.
	bool				m_bDefaultTransitionRequested:1;

	// PURPOSE: Let us know that we have already checked the scenario point for obstructions.
	bool				m_bCheckedScenarioPointForObstructions:1;

	//For clones migrating while cowering
	bool mbNewTaskMigratedWhileCowering : 1;
	bool mbOldTaskMigratedWhileCowering : 1;
	bool m_bMedicTypeScenario : 1;

	// PURPOSE: Allow destruction of this scenario ped when the entity goes away even in multiplayer.
	bool m_bAllowDestructionEvenInMPForMissingEntity : 1;

	// PURPOSE: Used to prevent the ped being left attached after local migration
	bool				m_bHasTaskAmbientMigrateHelper:1;

	// PURPOSE: Ignore pavement check when checking if the Ped should leave the scenario if this is set.
	BANK_ONLY(bool		m_bIgnorePavementCheckWhenLeavingScenario:1;)

	// PURPOSE: Enter timeout check bool.
	BANK_ONLY(bool		m_bCheckedEnterAnim:1;)

	bool m_WasInScope : 1;

	// PURPOSE:	Task tuning data.
	static Tunables		sm_Tunables;

	// PURPOSE:	Time stamp in ms until any ped can decide to leave.
	static u32			sm_NextTimeAnybodyCanGetUpMS;

	// PURPOSE: Last used animation rate on a cower or panic.
	static float		sm_fLastCowerPanicRate;

	static float		sm_fLastRate;

	// PURPOSE: Value to set fIdleTime to by default.
	static float sm_fUninitializedIdleTime;

	static bool sm_bDetachScenarioPeds;
	static bool sm_bAggressiveDetachScenarioPeds;

#if SCENARIO_DEBUG
	//Debug Anim Overrides
	ScenarioAnimDebugInfo*	m_pDebugInfo;
	fwFlags8				m_iDebugFlags;
	static bool				sm_bDoPavementCheck;
#endif // SCENARIO_DEBUG

	static bool				sm_bIgnoreBlackboardForScenarioReactions;

#if !__FINAL
public:
	virtual void		Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	fwMvClipId const& GetChosenClipId(CPed const* pPed, CTaskUseScenario const* pTask) const;
	void SetChosenClipId(fwMvClipId const& clipId, CPed const* pPed, CTaskUseScenario const* pTask);


	void SetEnterClipSetId(fwMvClipSetId const& clipSetId, CPed const* pPed, CTaskUseScenario const* pTask);

	fwMvClipSetId const& GetEnterClipSetId(CPed const* pPed, CTaskUseScenario const* pTask) const;
	fwMvClipSetId const& GetEnterClipSetId() const { return m_enterClipSetId; }
	fwMvClipSetId const& GetExitClipSetId() const { return m_exitClipSetId; }
	fwMvClipSetId const& GetBaseClipSetId() const { return m_baseClipSetId; }
};

// Info for syncing TaskUseScenario  
class CClonedTaskUseScenarioInfo : public CClonedScenarioFSMInfo
{
public:
		CClonedTaskUseScenarioInfo(	s32 state, 
								s16 scenarioType,
								CScenarioPoint* pScenarioPoint,
								bool bIsWorldScenario,
								int worldScenarioPointIndex,
								u32 flags,
								u16 SREFflags,
								u32 scenarioMode,
								float fHeading,
								const Vector3& worldLocationPos,
								const CAITarget& target,
								bool bCreateProp,
								bool bMedicType,
								float fIdleTime);

		CClonedTaskUseScenarioInfo();
		~CClonedTaskUseScenarioInfo(){}

		virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskUseScenario::State_Finish>::COUNT;}
		virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_USE_SCENARIO;}
		virtual int GetScriptedTaskType() const { return SCRIPT_TASK_USE_NEAREST_SCENARIO_TO_POS; }
		virtual bool    HasState()							const { return true; }
		virtual const char*	GetStatusName(u32)						const { return "Task Use Scenario State";}

		u32				GetFlags()								{ return m_Flags;  }
		u16				GetSREFFlags()							{ return m_SREFFlags;  }
		u32				GetScenarioMode()						{ return m_ScenarioMode; }
		Vector3&		GetWorldLocationPos()					{ return m_WorldLocationPos; }
		bool			GetCreateProp()							{ return m_bCreateProp; }
		bool			GetMedicType()							{ return m_bMedicType; }
		float			GetHeading()							{ return m_fHeading; }
		float			GetIdleTime()							{ return m_fIdleTime; }

		inline CClonedTaskInfoWeaponTargetHelper const &	GetWeaponTargetHelper() const	{ return m_weaponTargetHelper; }
		inline CClonedTaskInfoWeaponTargetHelper&			GetWeaponTargetHelper()			{ return m_weaponTargetHelper; }

		virtual CTaskFSMClone *CreateCloneFSMTask();
		virtual CTask *CreateLocalTask(fwEntity* pEntity);

	  void Serialise(CSyncDataBase& serialiser)
	  {
		  CClonedScenarioFSMInfo::Serialise(serialiser);

		  SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS,				"Task Use Scenario Flags");
		  SERIALISE_UNSIGNED(serialiser, m_ScenarioMode,SIZEOF_SCENARIO_MODES, "Scenario mode");
		  SERIALISE_PACKED_FLOAT(serialiser, m_fIdleTime, CTaskUseScenario::fMAX_IDLE_TIME, SIZEOF_IDLE_TIME, "Idle Time");

		  bool bMedicType = m_bMedicType;
		  SERIALISE_BOOL(serialiser, bMedicType, "Medic scenario type");
		  m_bMedicType = bMedicType;

		  if( ((m_ScenarioMode==CTaskUseScenario::SM_WorldLocation) && !(m_Flags & CTaskUseScenario::SF_StartInCurrentPosition))
			  || serialiser.GetIsMaximumSizeSerialiser() )
		  {
			  SERIALISE_PACKED_FLOAT(serialiser, m_fHeading,	TWO_PI, SIZEOF_HEADING,		"Heading");
			  SERIALISE_POSITION(serialiser, m_WorldLocationPos,							"World Location position");
		  }
		  else if(m_bMedicType)
		  {
			  SERIALISE_PACKED_FLOAT(serialiser, m_fHeading,	TWO_PI, SIZEOF_HEADING,		"Medic scenario heading");
		  }

		  m_weaponTargetHelper.Serialise(serialiser);

		  SERIALISE_UNSIGNED(serialiser, m_SREFFlags,SIZEOF_SREF_FLAGS, "SREF flags");

		  bool bCreateProp = m_bCreateProp;
		  SERIALISE_BOOL(serialiser, bCreateProp, "Create Prop");
		  m_bCreateProp = bCreateProp;


	  }

private:
	static const unsigned int SIZEOF_SREF_FLAGS							= datBitsNeeded<CTaskUseScenario::SREF_LastSyncedFlag>::COUNT;
	static const unsigned int SIZEOF_FLAGS								= datBitsNeeded<CTaskUseScenario::SF_LastSyncedFlag>::COUNT;;
	static const unsigned int SIZEOF_SCENARIO_MODES						= datBitsNeeded<CTaskUseScenario::SM_WorldLocation>::COUNT;
	static const unsigned int SIZEOF_CLIP_HASH							= 32;
	static const unsigned int SIZEOF_DICT_HASH							= 32;
	static const unsigned int SIZEOF_HEADING							= 8;
	static const unsigned int SIZEOF_IDLE_TIME							= 20;

	u32				m_Flags;
	u16				m_SREFFlags;
	bool			m_bCreateProp	: 1;
	bool			m_bMedicType	: 1;

	u32				m_ScenarioMode;

	float				m_fHeading;
	Vector3				m_WorldLocationPos;			// For world location scenarios without a scenario point
	float				m_fIdleTime;

	CClonedTaskInfoWeaponTargetHelper m_weaponTargetHelper;
};


#endif//!INC_TASK_USE_SCENARIO_H
