#ifndef INC_TASK_COWER_SCENARIO_H
#define INC_TASK_COWER_SCENARIO_H

//RAGE includes
#include "fwanimation/animdefines.h"

//GAME includes
#include "ai/AITarget.h"
#include "ai/ambient/ConditionalAnims.h"
#include "event/Event.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/System/Tuning.h"


//-------------------------------------------------------------------------
// CTaskCowerScenario
// When a ped must respond to an event but stay in one place due to navmesh
// constraints).
//-------------------------------------------------------------------------

class CTaskCowerScenario : public CTaskScenario
{
public:
	enum
	{
		State_Start,
		State_StreamIntro,
		State_Intro,
		State_DirectedIntro,
		State_Base,
		State_DirectedBase,
		State_StreamVariation,
		State_Variation,
		State_StreamFlinch,
		State_Flinch,
		State_Outro,
		State_DirectedOutro,
		State_Finish,
	};

	struct Tunables : public CTuning
	{
		Tunables();

		// How long in milliseconds before this ped can switch to directed after a gunshot
		u32			m_EventDecayTimeMS;

		// How far away the threat has to be for this ped to resume life as normal.
		float		m_ReturnToNormalDistanceSq;

		// Rate for interpolating between back left and back right when the target is moving behind the ped.
		float		m_BackHeadingInterpRate;

		// How long the ped has to be in a state (in s) before a switch of cower types due to distance.
		float		m_EventlessSwitchStateTimeRequirement;

		// How long it has to be (in ms) with nothing interesting going on before a switch of cower type due to distance.
		u32			m_EventlessSwitchInactivityTimeRequirement;

		// How far away the player can be to trigger a cower type switch without an event.
		float		m_EventlessSwitchDistanceRequirement;

		// How far ped must be from the player to delete if cowering forever
		float		m_MinDistFromPlayerToDeleteCoweringForever;

		// How far ped must be from the player to delete if cowering forever when recording a replay.
		float		m_MinDistFromPlayerToDeleteCoweringForeverRecordingReplay;

		// How long the ped must be offscreen for deletion minimum (milliseconds)
		u32			m_CoweringForeverDeleteOffscreenTimeMS_MIN;

		// How long the ped must be offscreen for deletion maximum (milliseconds)
		u32			m_CoweringForeverDeleteOffscreenTimeMS_MAX;

		// How long the ped has after an event to flinch (ms).
		u32			m_FlinchDecayTime;

		// The min time in milliseconds between consecutive flinches (ms).
		u32			m_MinTimeBetweenFlinches;

		PAR_PARSABLE;

	};

	CTaskCowerScenario(s32 iScenarioIndex, CScenarioPoint* pScenarioPoint, const CConditionalAnims* pConditionalAnims, const CAITarget& target, s16 iStartingEventType=-1, s16 iPriorityOfEvent=-1);
	~CTaskCowerScenario();
	virtual aiTask* Copy() const;

	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_COWER_SCENARIO;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return			ProcessPostFSM();
	virtual bool				ProcessMoveSignals();


	FSM_Return					Start_OnUpdate();

	FSM_Return					StreamIntro_OnUpdate();

	void						Intro_OnEnter();
	FSM_Return					Intro_OnUpdate();
	void						Intro_OnProcessMoveSignals();

	void						DirectedIntro_OnEnter();
	FSM_Return					DirectedIntro_OnUpdate();
	void						DirectedIntro_OnProcessMoveSignals();

	void						Base_OnEnter();
	FSM_Return					Base_OnUpdate();
	void						Base_OnProcessMoveSignals();

	void						DirectedBase_OnEnter();
	FSM_Return					DirectedBase_OnUpdate();
	void						DirectedBase_OnProcessMoveSignals();

	void						StreamVariation_OnEnter();
	FSM_Return					StreamVariation_OnUpdate();

	void						Variation_OnEnter();
	FSM_Return					Variation_OnUpdate();
	void						Variation_OnExit();
	void						Variation_OnProcessMoveSignals();
	
	void						StreamFlinch_OnEnter();
	FSM_Return					StreamFlinch_OnUpdate();

	void						Flinch_OnEnter();
	FSM_Return					Flinch_OnUpdate();
	void						Flinch_OnProcessMoveSignals();

	void						Outro_OnEnter();
	FSM_Return					Outro_OnUpdate();
	void						Outro_OnProcessMoveSignals();

	void						DirectedOutro_OnEnter();
	FSM_Return					DirectedOutro_OnUpdate();
	void						DirectedOutro_OnProcessMoveSignals();

	virtual s32					GetDefaultStateAfterAbort() const	{ return State_Finish; }

	virtual	void				CleanUp();

	// Determine whether or not the task should abort
	virtual bool				ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	// Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void OnCloneTaskNoLongerRunningOnOwner();
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool IsInScope(const CPed* pPed);

	bool	NetworkCheckSetMigrateStateAndClips();

public:

	// Public accessors / helper functions.

	// Restrict the task to only have the ped leave the scenario using the undirected exit.
	void						ForceUndirectedExit(bool bDoUndirectedExit) { m_bOnlyDoUndirectedExit = bDoUndirectedExit; }

	// Returns the default time length for cowering for the given event type.
	static float				GetTimeToCower(int iEventType);

	// Called in AffectsPedCore() of certain events to notify the cowering ped that the event has occured, even if responding to some
	// event with higher priority.
	static void					NotifyCoweringPedOfEvent(CPed& rPed, const CEvent& rEvent);

	// Set whether the cower timer should decrease or not.
	void						SetCowerForever(bool bForever) { m_bCowerForever = bForever; }

	// Allow this event not to respond with flinches to a particular event.
	void						SetFlinchIgnoreEvent(const CEvent* pEvent) { m_FlinchIgnoreEvent = pEvent; }

	// Return true if this event type is associated with a directed cower animation.
	static bool					ShouldPlayDirectedCowerAnimation(int iEventType);

	// Return true if the cowering ped should change their cower status for this event.
	bool						ShouldUpdateCowerStatus(const aiEvent& rEvent);

	// Stop cowering by playing either the normal outro or the panic outro.
	void						TriggerNormalExit()				{ m_fCowerTime = 0.0f; m_bSkipExitDistanceCheck = true; }
	void						TriggerPanicExit()				{ m_fCowerTime = 0.0f; m_bDoPanicExit = true; m_bSkipExitDistanceCheck = true; }
	void						TriggerImmediateExit()			{ m_bQuitTaskOnEvent = true; m_bSkipExitDistanceCheck = true; }

	// Perform logic to change cowering in response to a new event.
	void						UpdateCowerStatus(const aiEvent& rEvent);

	static const Tunables& GetTunables() { return sm_Tunables; }

	// For performance, if this ped is cowering forever, try to remove it.
	static void PossiblyDestroyPedCoweringOffscreen(CPed* pPed, CTaskGameTimer& inout_timer);

	static void	ResetLastTimeAnyoneFlinched() { sm_uLastTimeAnyoneFlinched = 0; }

private:

	// Internal helper functions.

	// Cache off what anims this cowering scenario supports so we don't have to requery constantly and waste processing time.
	void						CacheOffAnimationSupport();

	// Check if the conditions warrant the ped leaving their undirected cower pose.
	bool						CanPedLeaveUndirectedCowering();

	// Ways of switching between cower types that deal with distance instead of events.
	void						CheckForUndirectedCowerSwitchDueToDistance();
	void						CheckForDirectedCowerSwitchDueToDistance();

	// Choose a conditional clip set to use for this task.
	fwMvClipSetId				ChooseConditionalClipSet(CConditionalAnims::eAnimType animType, CScenarioCondition::sScenarioConditionData& conditionData) const;

	// Set the out clip from the clipset provided.  If choose best directed is true, then pick the clip that best rotates towards the cower target.
	bool						ChooseClip(const fwMvClipSetId& rClipSetId, fwMvClipId& rOutClipId, bool bChooseBestDirected);

	// Return the relative angle from the perspective of the cowering ped to the threat (in radians).
	float						GetAngleToTarget();

	// Return the rate that the cowering animation should play at.
	float						GetCoweringRate();
	
	// Control unusual exits from cowering - mainly new scripted tasks or taking damage.
	bool						ShouldStopCowering(const aiEvent& rEvent);
	void						StopCowering(const aiEvent& rEvent);

	// Check if the ped's current event has a ragdoll response.
	bool						IsRagdollEvent();

	// Alter the ped's cowering based on the event type received.
	void						HandleEventType(s16 iEventType, s16 iEventPriority, const CEvent* pEvent=NULL);

	// Send clip information to MoVE - rClipId will be tagged as moveSignalClipId
	void						SendClipInformation(fwMvClipSetId& rClipSetId, const fwMvClipId& rClipId, const fwMvClipId& moveSignalClipId);

	// Tag the appropriate directed base clips with their slots in the MoVE network. 
	bool						HasDirectedBaseAnimations() const;
	void						SendDirectedClipInformation(fwMvClipSetId& rClipSetId);

	void						SendForwardClipInformation(const crClip* pClip);
	void						SendLeftClipInformation(const crClip* pClip);
	void						SendRightClipInformation(const crClip* pClip);
	void						SendBackLeftClipInformation(const crClip* pClip);
	void						SendBackRightClipInformation(const crClip* pClip);

	// Variation anim helpers.
	void						SetTimeUntilNextVariation();
	bool						HasAVariationAnimation() const;
	bool						ShouldPlayVariation() const;
	void						PickVariationAnimation();

	// Flinch anim helpers.
	bool						HasAFlinchAnimation() const;
	bool						ShouldPlayAFlinch() const;
	void						PickFlinchAnimation();
	bool						DoesEventTriggerFlinch(const CEvent* pEvent=NULL) const;

	// Look at the event and update the cowering target.
	void						UpdateCowerTarget(const aiEvent& rEvent);

	void						UpdateHeadIK();

	// Send the direction signal to MoVE based on where the threat is relative to the cowering ped.
	void						UpdateDirectedCowerBlend();

	void						KeepStreamingBaseClip();

	// Have the ped let go of any ambient props.
	void						DropProps();

	// Audio helpers.
	void						PlayDirectedAudio();
	void						PlayInitialAudio();
	void						PlayIdleAudio();
	void						PlayFlinchAudio();

private:

	//Member Variables

	CMoveNetworkHelper				m_MoveNetworkHelper;

	fwClipSetRequestHelper			m_BaseClipSetRequestHelper;
	fwClipSetRequestHelper			m_OtherClipSetRequestHelper;

	// What is making this ped cower.
	CAITarget						m_Target;

	// Store a pointer to an event that we don't want this ped to respond to with flinches.
	RegdConstEvent					m_FlinchIgnoreEvent;

	// Timer for ped offscreen to be deleted.
	CTaskGameTimer				m_OffscreenDeletePedTimer;

	// When cowering was started, this is where the target was standing.
	Vec3V							m_vTargetOriginalPosition;

	// The clipsets being played - base is always streamed in.
	// OtherClipSetId can be the intro/outro/variation.
	fwMvClipSetId					m_BaseClipSetId;
	fwMvClipSetId					m_OtherClipSetId;

	// The conditional animations associated with the scenario we are cowering from.
	const CConditionalAnims*		m_pConditionalAnims;

	// What event the cowering ped is responding to.
	s16								m_iCurrentEventType;

	// The priority value of the event that the ped is currently responding to.
	s16								m_iCurrentEventPriority;

	// How long the ped should continue to cower for.
	float							m_fCowerTime;

	// How long from the start of the base state a ped can decide to play a variation animation.
	float							m_fVariationTimer;

	// The last time the directed anims blended in, this is the relative heading to the target.
	float							m_fLastDirection;

	// Interpolation value used for transitioning between back left and back right when the target is behind the ped.
	float							m_fApproachDirection;

	// When an event happened that caused this ped to cower.
	u32								m_uTimeOfCowerTrigger;

	// When the target/source entity is denied target change until - used in MP
	u32								m_uTimeDenyTargetChange;

	// Whether the cower animations are directed towards the target.
	bool							m_bPlayDirectedCower;

	// Whether this ped is switching between directed/undirected cowering.
	bool							m_bSwitchingBetweenCowerTypes;

	// Is the heading being interpolated?
	bool							m_bApproaching;

	// Is the ped cowering forever?
	bool							m_bCowerForever;

	// Should the ped do a panic outro or a normal outro?
	bool							m_bDoPanicExit;

	// Does this scenario type support variations?
	bool							m_bHasAVariationAnimation;

	// Does this scenario type support directed cowering?
	bool							m_bHasDirectedBaseAnimations;

	// Does this scenario type support flinching animations?
	bool							m_bHasAFlinchAnimation;

	// Should this scenario quit when it gets an event?
	bool							m_bQuitTaskOnEvent;

	// The ped received an event that wants flinching to occur.
	bool							m_bEventTriggersFlinch;

	// Is the MoVE network ready for a potential transition from variation to flinch.
	bool							m_bFullyInVariationState;

	// Cached bool from MoVE for timeslicing support.
	bool							m_bMoveInTargetState;

	// When this ped exits, should it only use the undirected exit to leave cowering when the time comes?
	bool							m_bOnlyDoUndirectedExit;

	// Disregard the check for distance when leaving cowering, used for script triggered exits.
	bool							m_bSkipExitDistanceCheck;

	// Records the state of the parent TaskUseScenario migrating
	bool							m_bMigrateDontClearMoveNetwork : 1;
	bool							m_bMigrateMoveNetworkForceStateChange : 1;

private:

	//Move signals

	//State need for network migrating
	static const fwMvStateId	ms_State_Base;

	// Requests
	static const fwMvRequestId	ms_PlayBaseIntroRequest;
	static const fwMvRequestId	ms_PlayDirectedIntroRequest;
	static const fwMvRequestId	ms_PlayBaseRequest;
	static const fwMvRequestId	ms_PlayVariationRequest;
	static const fwMvRequestId	ms_PlayFlinchRequest;
	static const fwMvRequestId	ms_PlayBaseOutroRequest;
	static const fwMvRequestId	ms_PlayDirectedOutroRequest;

	// Signals from MoVE that a clip is done playing.
	static const fwMvBooleanId	ms_IntroDoneId;
	static const fwMvBooleanId	ms_OutroDoneId;
	static const fwMvBooleanId	ms_VariationDoneId;
	static const fwMvBooleanId	ms_FlinchDoneId;

	// State transition checks.
	static const fwMvBooleanId	ms_OnEnterIntroId;
	static const fwMvBooleanId	ms_OnEnterBaseId;
	static const fwMvBooleanId	ms_OnEnterDirectedBaseId;
	static const fwMvBooleanId	ms_OnEnterOutroId;
	static const fwMvBooleanId	ms_OnEnterVariationId;

	// Clips
	static const fwMvClipId		ms_IntroClipId;
	static const fwMvClipId		ms_BaseClipId;
	// Blended to form directed cowering.
	static const fwMvClipId		ms_DirectedForwardTransClipId;
	static const fwMvClipId		ms_DirectedLeftTransClipId;
	static const fwMvClipId		ms_DirectedRightTransClipId;
	static const fwMvClipId		ms_DirectedBackLeftTransClipId;
	static const fwMvClipId		ms_DirectedBackRightTransClipId;
	static const fwMvClipId		ms_DirectedForwardBaseClipId;
	static const fwMvClipId		ms_DirectedLeftBaseClipId;
	static const fwMvClipId		ms_DirectedRightBaseClipId;
	static const fwMvClipId		ms_DirectedBackLeftBaseClipId;
	static const fwMvClipId		ms_DirectedBackRightBaseClipId;
	static const fwMvClipId		ms_VariationClipId;
	static const fwMvClipId		ms_FlinchClipId;
	static const fwMvClipId		ms_OutroClipId;

	// Used to control how the directed clips blend in.
	static const fwMvFloatId	ms_TargetHeadingId;

	// Rate for the clips.
	static const fwMvFloatId	ms_RateId;

private:

	// Tuning

	// PURPOSE:	Task tuning data.
	static Tunables				sm_Tunables;

	// Statics

	static float				sm_fLastCowerRate;

	static u32					sm_uLastTimeAnyoneFlinched;
};

//////////////////////////////////////////////////////////////////////////
// CClonedCowerScenarioInfo
// Purpose: Task info for cower scenario task
//////////////////////////////////////////////////////////////////////////

class CClonedCowerScenarioInfo : public CClonedScenarioFSMInfo
{
public:
	CClonedCowerScenarioInfo();
	CClonedCowerScenarioInfo(s32 cowerState, s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int worldScenarioPointIndex);
	~CClonedCowerScenarioInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_SCENARIO_COWER;}

	virtual bool    HasState() const { return true; }
	virtual u32	    GetSizeOfState() const { return SIZEOF_COWER_SCENARIO;}
	virtual const char*	GetStatusName(u32) const { return "Status";}

	void Serialise(CSyncDataBase& serialiser);

private:

	static const unsigned int SIZEOF_COWER_SCENARIO = 4;
};

#endif//!INC_TASK_COWER_SCENARIO_H
