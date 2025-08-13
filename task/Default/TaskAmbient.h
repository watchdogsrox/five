// FILE :    TaskAmbient.h
// PURPOSE : General ambient task run in unison with movement task that manages
//			 Streaming and playback of clips based on the file Ambient.dat
// AUTHOR :  Phil H
// CREATED : 08-08-2006

#ifndef TASK_AMBIENT_H
#define TASK_AMBIENT_H

// Framework headers
#include "fwutil/Flags.h"
#include "event/event.h"

// Game headers
#include "AI/AITarget.h"
#include "ik/IkRequest.h"
#include "Task/Helpers/PropManagementHelper.h"
#include "task/Scenario/ScenarioAnimDebugHelper.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"

// Forward declarations
struct CAmbientAudioExchange;
class CEvent;
class CEventShocking;
class CScenarioClips;
class CConditionalAnimsGroup;
class CTaskAmbientClips;
class CTaskAmbientMigrateHelper;
class CTaskConversationHelper;

//////////////////////////////////////////////////////////////////////////////////////////////
// CLookAtHistory - container class for storing what entities have been previously looked at.
//////////////////////////////////////////////////////////////////////////////////////////////

class CLookAtHistory
{
public:

	// History tuning data.
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:  Used to test the lookat angle of a new entity compared to the history entity.
		float m_HistoryCosineThreshold;

		// PURPOSE:  How long (in milliseconds) an entity should stay in memory.
		u32 m_MemoryDuration;

		PAR_PARSABLE;
	};
	
	CLookAtHistory() : m_qLastLookAts(), m_qLastTimes() {}
	~CLookAtHistory() {}

	// PURPOSE:  Add pEnt to the tracked history of targets.
	void AddHistory(const CEntity* pEnt);

	// PURPOSE:  Return true if the entity has been looked at recently.
	// Will also return true if the angle to the given entity is close to any remembered entities.
	bool CheckHistory(const CPed& pOrigin, const CEntity& pEnt);

	// PURPOSE: Kick things out of memory if they have been there for too long.
	void ProcessHistory();

private:

	static const u32 MAX_HISTORY_SIZE = 3;

	atQueue<RegdConstEnt, MAX_HISTORY_SIZE>		m_qLastLookAts;

	atQueue<u32, MAX_HISTORY_SIZE>				m_qLastTimes;

	// Static tuning data.
	static Tunables sm_Tunables;
};



////////////////////////////////////////////////////////////////////////////////

class CAmbientLookAt
{
public:
	FW_REGISTER_CLASS_POOL(CAmbientLookAt);

	// Blocking states to determine if the ped should continue LookAts.
	enum LookAtBlockingState
	{
		LBS_NoBlocking = 0,
		LBS_BlockLookAts,
		LBS_BlockLookAtsInConversation	// block some lookAts in a conversation.
	};

	// Task tuning data
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:  What the lookat score needs to exceed to be considered a valid lookat target.
		float m_DefaultLookAtThreshold;

		// PURPOSE:	 How close a ped needs to be to the world center in order to allow lookats.
		float m_DefaultDistanceFromWorldCenter;

		// PURPOSE:	 Extended distance (under certain conditions) a ped needs to be within from world center to allow lookats.
		float m_ExtendedDistanceFromWorldCenter;

		// PURPOSE:  How far around the ped entities can be to be considered valid lookat targets.
		float m_MaxDistanceToScanLookAts;

		// PURPOSE:  The base time in seconds to perform a lookat for.
		float m_BaseTimeToLook;

		// PURPOSE:  The min range (s) for how long until the next lookat for AI peds.
		float m_AITimeBetweenLookAtsFailureMin;

		// PURPOSE:  The max range (s) for how long until the next lookat for AI peds.
		float m_AITimeBetweenLookAtsFailureMax;

		// PURPOSE:  The min range (s) for how long until the next lookat for player peds.
		float m_PlayerTimeBetweenLookAtsMin;

		// PURPOSE:  The max range (s) for how long to look at my vehicle for player peds.
		float m_PlayerTimeMyVehicleLookAtsMax;

		// PURPOSE:  The min range (s) for how long to look at my vehicle for player peds.
		float m_PlayerTimeMyVehicleLookAtsMin;

		// PURPOSE:  The max range (s) for how long until the next lookat for player peds.
		float m_PlayerTimeBetweenLookAtsMax;

		// PURPOSE:  The time (s) between scans for a nearby lookat scenario point.
		float m_TimeBetweenScenarioScans;

		// PURPOSE:  How far away from the ped the scenario scan for lookats should take place.
		float m_ScenarioScanOffsetDistance;
		
		// PURPOSE:  How far the scenario finder should look when searching for a scenario lookat point.
		float m_ScenarioScanRadius;

		// PURPOSE:  The max ambient score for player peds.
		float m_MaxPlayerScore;

		// Basic entity scores

		// PURPOSE:  Initial value for a basic ped in the lookat scoring system.
		float m_BasicPedScore;

		// PURPOSE:  Initial value for a basic vehicle in the lookat scoring system.
		float m_BasicVehicleScore;

		// PURPOSE:  Initial value for a basic object in the lookat scoring system.
		float m_BasicObjectScore;

		// The factors below are multiplied by the initial entity score.

		// PURPOSE:  Factor for objects behind ped doing the scoring.
		float m_BehindPedModifier;

		// PURPOSE:  Factor for peds to score the player.
		float m_PlayerPedModifier;

		// PURPOSE:  Factor for peds who are walking around the ped doing the scoring.
		float m_WalkingRoundPedModifier;

		// PURPOSE:  Factor for peds who are running.
		float m_RunningPedModifier;

		// PURPOSE:  Factor for peds who are climbing or jumping.
		float m_ClimbingOrJumpingPedModifier;

		// PURPOSE:  Factor for peds that are fighting.
		float m_FightingModifier;

		// PURPOSE:  Factor for peds who are stealing a car.
		float m_JackingModifier;

		// PURPOSE:  Factor for peds hanging around our vehicle.
		float m_HangingAroundVehicleModifier;

		// PURPOSE:  Factor for scenario peds looking at other scenario peds.
		float m_ScenarioToScenarioPedModifier;

		// PURPOSE: Factor used to make gang members using a scenario more likely to look at the player.
		float m_GangScenarioPedToPlayerModifier;

		// PURPOSE: Factor used to scale up the weight of a player approaching this ped.
		float m_ApproachingPlayerModifier;

		// PURPOSE: Factor used to scale up the weight of a player extremely close to this ped.
		float m_ClosePlayerModifier;

		// PURPOSE: Factor used to scale up the weight of a player close to this ped.
		float m_InRangePlayerModifier;

		// PURPOSE: Factor used to scale up the weight of a player close to this ped while driving a car.
		float m_InRangeDrivingPlayerModifier;

		// PURPOSE: Factor used to scale up the weight of a player holding a weapon.
		float m_HoldingWeaponPlayerModifier;
		
		// PURPOSE: Factor used to scale up the weight of a player covered in blood.
		float m_CoveredInBloodPlayerModifier;

		// PURPOSE: Factor for peds ragdolling.
		float m_RagdollingModifier;

		// PURPOSE: Factor for peds picking up a on ground bike.
		float m_PickingUpBikeModifier;

		// PURPOSE:  Factor for cars that are being recklessly driven.
		float m_RecklessCarModifier;
		
		// PURPOSE:  Scaling constant used in increasing the value of recklessly driven cars.
		float m_RecklessCarSpeedMin;

		// PURPOSE:  Scaling constant used in increasing the value of recklessly driven cars.
		float m_RecklessCarSpeedMax;

		// PURPOSE:  Factor for cars that have sirens activated.
		float m_CarSirenModifier;

		// PURPOSE:  Factor used for players to make them more likely to look at cops.
		float m_PlayerCopModifier;

		// PURPOSE:  Factor used for players to make them more likely to look at sexy peds.
		float m_PlayerSexyPedModifier;

		// PURPOSE:  Factor used for players to make them more likely to look at swanky cars.
		float m_PlayerSwankyCarModifier;

		// PURPOSE:  Factor used for players to make them more likely to look at cop cars.
		float m_PlayerCopCarModifier;

		// PURPOSE:  Factor used for players to make them more likely to look at peds hassling them.
		float m_PlayerHasslingModifier;

		// PURPOSE: Factor used for a player that the ped has looked at recently.
		float m_RecentlyLookedAtPlayerModifier;

		// PURPOSE: Factor used for an entity that the ped has looked at recently.
		float m_RecentlyLookedAtEntityModifier;

		// PURPOSE: Factor used for high-importance lookat scenarios.
		float m_HighImportanceModifier;

		// PURPOSE: Factor used for medium-importance lookat scenarios.
		float m_MediumImportanceModifier;

		// PURPOSE: Factor used for low-importance lookat scenarios.
		float m_LowImportanceModifier;

		// PURPOSE: Model names to consider players.
		atArray<atHashWithStringNotFinal> m_ModelNamesToConsiderPlayersForScoringPurposes;

		//Other tunables

		// PURPOSE: Min distance to the hot ped from the jeering ped
		float m_HotPedMinDistance;

		// PURPOSE: Max distance to the hot ped from the jeering ped
		float m_HotPedMaxDistance;

		// PURPOSE: Min dot product angle to the hot ped from the jeering ped
		float m_HotPedMinDotAngle;

		// PURPOSE: Max dot product angle to the hot ped from the jeering ped
		float m_HotPedMaxDotAngle;

		// PURPOSE: Max height difference between hot ped and jeering ped
		float m_HotPedMaxHeightDifference;

		// PURPOSE: Try to look at the player within this range even if the ped is looking at some non-player target.
		float m_InRangePlayerDistanceThreshold;
		float m_InRangePlayerInRaceDistanceThreshold;	// used in race mode

		// PURPOSE: Min distance to consider the player as extremely close.
		float m_ClosePlayerDistanceThreshold;

		// PURPOSE: Min distance to consider the player as approaching.
		float m_ApproachingPlayerDistanceThreshold;

		// PURPOSE: Threshold for the dot product between the straight line to the player and the player's forward vector.
		float m_ApproachingPlayerCosineThreshold;

		// PURPOSE: Threshold distance to try to look at player if he is ragdolling.
		float m_RagdollPlayerDistanceThreshold;

		// PURPOSE: Max dot product angle to the in-range player to ignore the IK FOV.
		float m_LookingInRangePlayerMaxDotAngle;

		// PURPOSE:  The maximum velocity a vehicle can travel to be valid for lookats.
		float m_MaxVelocityForVehicleLookAtSqr;

		// PURPOSE:  The minimum swankiness of a car that the player is interested in.
		u8 m_PlayerSwankyCarMin;

		// PURPOSE:  The maximum swankiness of a car that the player is interested in.
		u8 m_PlayerSwankyCarMax;

		// PURPOSE: Render debug of when a jeering can look at and comment on a jeering ped
		bool m_HotPedRenderDebug;

		// PURPOSE: Disable hot ped and jeering flag checks
		bool m_HotPedDisableSexinessFlagChecks;

		// PURPOSE: The minimum time before switching to camera/motion look at from POI. Used to stop targets changing too soon
		float m_MinTimeBeforeSwitchLookAt;

		// PURPOSE: The maximum angle the player can look behind at where camera is pointing.
		float m_MaxLookBackAngle;

		// PURPOSE: The minimum turn speed to use motion direction over POI.
		float m_MinTurnSpeedMotionOverPOI;

		// PURPOSE: The angles and speed used to compute the angle range for POI pick.
		float m_SpeedForNarrowestAnglePickPOI;
		float m_MaxAnglePickPOI;
		float m_MinAnglePickPOI;

		// PURPOSE: The maximum pitching angle for POI pick.
		float m_MaxPitchingAnglePickPOI;

		// PURPOSE: Debug draw for player look at.
		bool m_PlayerLookAtDebugDraw;

		LookIkTurnRate m_CameraLookAtTurnRate;
		LookIkTurnRate m_POILookAtTurnRate;
		LookIkTurnRate m_MotionLookAtTurnRate;
		LookIkTurnRate m_VehicleJumpLookAtTurnRate;
		LookIkBlendRate m_CameraLookAtBlendRate;
		LookIkBlendRate m_POILookAtBlendRate;
		LookIkBlendRate m_MotionLookAtBlendRate;
		LookIkBlendRate m_VehicleJumpLookAtBlendRate;
		LookIkRotationLimit m_CameraLookAtRotationLimit;
		LookIkRotationLimit m_POILookAtRotationLimit;
		LookIkRotationLimit m_MotionLookAtRotationLimit;

		float m_AITimeWaitingToCrossRoadMin;
		float m_AITimeWaitingToCrossRoadMax;

		u32 m_uAITimeBetweenGreeting;
		float m_fAIGreetingDistanceMin;
		float m_fAIGreetingDistanceMax;
		float m_fAIGreetingPedModifier;

		u32 m_uTimeBetweenLookBacks;
		u32 m_uTimeToLookBack;

		u32 m_uAimToIdleLookAtTime;
		float m_fAimToIdleBreakOutAngle;
		float m_fAimToIdleAngleLimitLeft;
		float m_fAimToIdleAngleLimitRight;

		u32 m_uExitingCoverLookAtTime;

		PAR_PARSABLE;
	};

	CAmbientLookAt();

	// PURPOSE: Perform look at - return the blocking state.
	LookAtBlockingState UpdateLookAt( CPed* pPed, CTaskAmbientClips* pAmbientClipsTask, float fTimeStep );

	// PURPOSE: Fn to set a specific look at target
	void SetSpecificLookAtTarget( const CAITarget& tgt ) { m_SpecifiedLookAtTarget = tgt; }

	// PURPOSE: Get any specified look at target
	const CAITarget& GetSpecificLookAtTarget() const { return m_SpecifiedLookAtTarget; }

	// PURPOSE: Get the current look at target
	const CAITarget& GetCurrentLookAtTarget() const { return m_CurrentLookAtTarget; }

	// PURPOSE: Set if we should use the extended head look distance
	void SetUseExtendedDistance(bool bShouldUseExtendedDist) { m_bUseExtendedDistance = bShouldUseExtendedDist; }

	void SetCanSayAudio(bool bValue) { m_bCanSayAudio = bValue; }

	void SetCanLookAtMyVehicle(bool bValue);

	bool IsLookingAtRoofOpenClose() const {return m_bUsingRoofLookAt;}

	// PURPOSE: Score the entity for purposes of look at
	static float ScoreInterestingLookAt( CPed* pPed, const CEntity* pEntity );

	// PURPOSE: Score the entity for purposes of look at
	static float ScoreInterestingLookAtPlayer( CPed* pPlayerPed, const CEntity* pEntity );

	// PURPOSE: Check if the blend helper flags allow lookat.
	static bool DoBlendHelperFlagsAllowLookAt(const CPed* pPed);

	static bool IsLookAtAllowedForPed(const CPed* pPed, const CTaskAmbientClips* pAmbientClipTask, bool bUseExtendedDistance);

	// PURPOSE: Look at blocking state query functions
	static LookAtBlockingState GetLookAtBlockingState(const CPed* pPed, const CTaskAmbientClips* pAmbientClipTask, bool bUseExtendedDistance);

	static bool ShouldConsiderPlayerForScoringPurposes(const CPed& rPed);

	static float GetMovingSpeed(const CPed* pPed);


protected:

	static const Tunables& GetTunables() { return sm_Tunables; }


private:

	// PURPOSE: look at helper functions
	bool LookAtDestination( CPed* pPed );
	void UpdateLookAtAI( CPed* pPed, LookAtBlockingState iLookatBlockingState, float fTimeStep );
	void UpdateLookAtPlayer( CPed* pPlayerPed, LookAtBlockingState iLookatBlockingState, float fTimeStep );
	bool PickInterestingLookAt( CPed* pPed, bool bPickBest );
	bool PickInterestingLookAtPlayer( CPed* pPlayerPed, bool bPickBest );
	static CEntity* GetAmbientLookAtTaskTarget(const CPed& ped);
	static bool ShouldIgnoreFovDueToAmbientLookAtTask(const CPed& rPed);
	bool UpdateCommentOnNiceVehicle(CPed* pPed);
	bool CanDriverHeadlookAtOtherEntities(CPed* pPed) const;
	static float UpdateJeerAtHotPed(const CPed* pPed,const CPed* pOtherPed);

	bool LookAtNearbyScenarioPoint(const CPed& rPed);
	void ScanForNewScenarioLookAtPoint(const CPed& rPed);

	float GetModifiedScoreBasedOnHistory(CPed& rLookingPed, CEntity& rEntity, float fCurrentScore);

	bool CanLoadAudioForExchange(const CPed& rPed, const CPed& rTarget) const;
	bool LoadAudioExchangeForEvent(const CPed& rPed, CAmbientAudioExchange& rExchange) const;
	bool LoadAudioExchangeBetweenPeds(const CPed& rPed, const CPed& rTarget, CAmbientAudioExchange& rExchange) const;
	void SayAudioToTarget(CPed& rPed, CEntity& rTarget) const;
	bool CanSayAudioToTarget(CPed& rPed, CEntity& rTarget, CAmbientAudioExchange& rExchange) const;

	bool PickCameraLookAtPlayer(CPed* pPlayerPed);
	bool PickMotionLookAtPlayer(CPed* pPlayerPed);
	bool VehicleJumpLookAtPlayer(CPed* pPlayerPed);
	bool ConvertibleRoofOpenCloseLookAtPlayer(CPed* pPlayerPed);
	
	bool LookAtMyVehicle(CPed* pPed);

	CPed* ShouldCheckNearbyPlayer(CPed* pPed);

	bool ShouldIgnoreFOVWhenLookingAtPlayer(CPed* pPed, CPed* pPlayerPed);

	void PickWaitingToCrossRoadLookAt(CPed* pPed, const Vector3& vEndPos);

	void ResetLookAtTarget();

	static float GetInRangePlayerDistanceThreshold(const CPed* pPlayerPed);

	bool LookAtCarJacker(CPed* pPed);

	bool ShouldStopAimToIdleLookAt(CPed* pPed);
	void UpdateAimToIdleLookAtTarget(CPed* pPed);

	//
	// Members
	//

	// PURPOSE: Desired look at target
	CAITarget			m_SpecifiedLookAtTarget;

	// PURPOSE: Current look at target
	CAITarget			m_CurrentLookAtTarget;

	// PURPOSE: The last place looked at.  Remember this so repeating a look in this direction is penalized in certain cases.
	CLookAtHistory		m_LookAtHistory;

	// Look at request
	CIkRequestBodyLook	m_LookRequest;

	// PURPOSE: Timers
	float m_fLookAtTimer;
	float m_fTimeSinceLastLookAt;
	float m_fTimeSinceLastScenarioScan;
	float m_fTimeSinceLookInterestPoint;

	// The current scenario point we are considering looking at.
	RegdScenarioPnt		m_pLookAtScenarioPoint;
	u32 m_uLookBackStartTime;
	u32 m_uLastGreetingTime;

	// PURPOSE: Should we be using the extended distance?
	bool m_bUseExtendedDistance : 1;

	bool m_bCanSayAudio : 1;

	// is looking at the camera direction
	bool m_bUsingCameraLookAt : 1;
	
	// is looking at the camera direction
	bool m_bUsingMotionLookAt : 1;
	
	// is looking at he vehicle velocity direction
	bool m_bUsingVehicleJumpLookAt : 1;

	// is looking at the car roof open/close
	bool m_bUsingRoofLookAt : 1;

	bool m_bCanLookAtMyVehicle : 1;
	bool m_bCheckedMyVehicle : 1;

	bool m_bIgnoreFOV : 1;

	bool m_bWaitingToCrossRoadLookLeft : 1;
	bool m_bWaitingToCrossRoadLookRight : 1;
	bool m_bFoundGreetingTarget : 1;
	
	bool m_bLookBackStarted : 1;

	bool m_bAimToIdleLookAtFinished : 1;

	bool m_bExitCoverLookAtFinished : 1;
	
	// PURPOSE: Static constants
	static const u8   MAX_ENTITIES_TO_LOOKAT;

	// Static tuning data.
	static Tunables sm_Tunables;
};

////////////////////////////////////////////////////////////////////////////////

class CAmbientEvent
{
public:

	CAmbientEvent();
	CAmbientEvent(AmbientEventType nType, Vec3V_In vPosition, float fTimeToLive, float fTimeToPlayAnimation);
	~CAmbientEvent();
	
	void										Clear();
	Vec3V_Out									GetPosition() const { return m_vPosition; }
	float										GetTimeToLive() const { return m_fTimeToLive; }
	AmbientEventType							GetType() const { return m_nType; }
	bool										IsReacting() const;
	bool										IsValid() const;
	void										SetReacting(bool bReacting) { m_bReacting = bReacting; }
	bool										TickAnimation(float fTimeStep);
	bool										TickLifetime(float fTimeStep);
	
private:

	Vec3V										m_vPosition;
	float										m_fTimeToLive;
	float										m_fTimeToPlayAnimation;
	AmbientEventType							m_nType;
	bool										m_bReacting;
};

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Task to be run in conjunction with movement tasks, selects and plays
// random ambient clips
//-------------------------------------------------------------------------
class CTaskAmbientClips : public CTaskFSMClone
{
	friend class CClonedTaskAmbientClipsInfo;
	friend class CTaskAmbientMigrateHelper;

public:
	// Static constants
	static bank_bool  BLOCK_ALL_AMBIENT_IDLES;

	// Task tuning data
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Clip set containing fallback animations to be used instead of base animations that are not streamed in on time.
		fwMvClipSetId m_LowLodBaseClipSetId;

		// PURPOSE: If we don't get the chance of standing from conditional animations, this value will be used as a probability between 0..1.
		float m_DefaultChanceOfStandingWhileMoving;

		// PURPOSE: If we don't get the time between idles from conditional animations, this value (in seconds) will be used.
		float m_DefaultTimeBetweenIdles;

		// PURPOSE:	When blocking idle animations after gunshots, this is the number of seconds to wait.
		float m_TimeAfterGunshotToPlayIdles;

		// PURPOSE:	Like m_TimeAfterGunshotToPlayIdles, but used any time for the player, not just when wandering.
		float m_TimeAfterGunshotForPlayerToPlayIdles;

		// PURPOSE: If the player is within this distance (squared) to a scenario with the BreakForNearbyPlayer flag, we start a timer to force the ped to do something else.
		float m_playerNearToHangoutDistanceInMetersSquared;

		// PURPOSE: Minimum time in seconds the player must be within the above distance until the Ped is forced into a conversation or different scenario. (Randomized between min/max)
		float m_minSecondsNearPlayerUntilHangoutQuit;

		// PURPOSE: Maximum time in seconds the player must be within the above distance until the Ped is forced into a conversation or different scenario. (Randomized between min/max)
		float m_maxSecondsNearPlayerUntilHangoutQuit;

		// PURPOSE: Maximum distance (squared) peds can be and still perform hangout-style chats.
		float m_maxHangoutChatDistSq;

		// PURPOSE:	Scaling factor to apply to VFX cull ranges for peds that are not visible.
		float m_VFXCullRangeScaleNotVisible;

		// PURPOSE: Replaces SECONDS_SINCE_IN_WATER_THAT_COUNTS_AS_WET in CScenarioConditionWet, which allows modification to how long a ped is allowed to be wet after leaving the water.
		float m_SecondsSinceInWaterThatCountsAsWet;

		// PURPOSE:	The maximum velocity in meters per second the vehicle we're in can travel at before we stop ambient idles from playing
		float m_MaxVehicleVelocityForAmbientIdles;

		float m_MaxBikeVelocityForAmbientIdles;

		// PURPOSE:	The max absolute steering angle before we stop ambient idles from playing
		float m_MaxSteeringAngleForAmbientIdles;

		// PURPOSE:	The ms time since getting up that a dust off idle could occur
		u32 m_MaxTimeSinceGetUpForAmbientIdles;

		//PURPOSE: Minimum distance from the player in meters to start an argument between peds if they choose to have one.
		float m_fArgumentDistanceMinSq;

		//PURPOSE: Maximum distance from the player in meters to start an argument between peds if they choose to have one.
		float m_fArgumentDistanceMaxSq;

		//PURPOSE: Chance that two peds near each other with AllowConversations will get into an argument.
		float m_fArgumentProbability;

		PAR_PARSABLE;
	};

	// Flags passed in to alter the task behaviour
	enum Flags
	{
		Flag_PlayBaseClip													= BIT0,
		Flag_InstantlyBlendInBaseClip							= BIT1,
		Flag_PlayIdleClips												= BIT2,
		Flag_PickPermanentProp										= BIT3,
		Flag_OverrideInitialIdleTime							= BIT4,
		Flag_ForcePropInPickPermanent							= BIT5,		// If set, bypass the normal probability check when picking a prop.
		Flag_BlockIdleClipsAfterGunshots					= BIT6,		// If set, block all idle animations for a certain time after a gunfight.
		Flag_CanSayAudioDuringLookAt							= BIT7, 
		Flag_QuitAfterBase												= BIT8,		// If true, quit this task once the base anim has played.		
		Flag_HasOnFootClipSetOverride							= BIT9,		// True if we're currently overriding the whole movement clip set (for full body movement animations).
		Flag_IgnoreLowPriShockingEvents						= BIT10,	// True if we're currently ignoring low priority shocking events, due to this task.
																													// In that case, we owe CPedIntelligence a call to EndIgnoreLowPriShockingEvents().
		Flag_UserSpecifiedConditionalAnimChosen		= BIT11,	// If true, m_ConditionalAnimChosen was passed in by the user, rather than determined internally.
		Flag_ForceFootClipSetRestart							= BIT12,	// If true, when the foot clipset is restored in CleanUp() force a restart to ensure it is cleared.
		Flag_SynchronizedMaster										= BIT13,	// This ped is considered the master when synchronizing animations with a partner, 
		Flag_SynchronizedSlave										= BIT14,	// This ped is considered the slave when synchronizing animations with a partner, 
		Flag_SlaveNotReadyForSyncedIdleAnim				= BIT15,
		Flag_UseExtendedHeadLookDistance					= BIT16,	// Tells the ambient head look to let peds to be further from world center and still do their head ik
		Flag_IsInVehicle												= BIT17,	// Used when creating clone peds task to prevent in vehicle idles being played on foot
		Flag_IdleClipIdsOverridden								= BIT18,	// Used to force the playing of an idle animation 
		Flag_PlayingIdleOverride									= BIT19,	// Used to let us know that we are now playing the overriden anim.
		Flag_OverriddenBaseAnim										= BIT20,	// Used to tell the task that an override has been provided for the base clipset/clip
		Flag_AllowsConversation										= BIT21,	// Set from the scenario if this ambient clip can start a conversation. NOT USED FOR MOBILE PHONE CONVERSTAIONS.
		Flag_CanLookAtMyVehicle										= BIT22,	// Allow to check my vehicle after exit.
		Flag_IsInConversation											= BIT23,	// True if this ped is currently engaged in a conversation. Needed because two peds talking to each other will only have a single ConversationHelper.
		Flag_InBreakoutScenario										= BIT24,	// True if this ped is in a scenario that moves to a Conversation, other scenario, or wander if the player is nearby for too long.
		Flag_PlayerNearbyTimerSet									= BIT25,	// True if we're currently timing the closeness of the player for Breakout scenarios
		Flag_PhoneConversationShouldPlayEnterAnim	= BIT26,	// True if we should play the Enter animation for a phone conversation.
		Flag_ForcePickNewIdleAnim									= BIT27,	// If set, will ignore the nextIdle timer and pick a new random idle next frame
		Flag_QuitAfterIdle												= BIT28,	// If set, this task will quit when the next Idle animation finishes
#if __BANK
		Flag_UseDebugAnimData											= BIT29,	// If true, m_ClipSetId and m_ClipId were passed in by the user, rather than determined internally.
		Flag_WantNextDebugAnimData								= BIT30,
#endif
		Flag_DontRandomizeBaseOnStartup						= BIT31,	// If set, this task will not randomize the Base initially
	};

	CTaskAmbientClips( u32 iFlags, const CConditionalAnimsGroup * pClips, s32 ConditionalAnimChosen = -1, CPropManagementHelper* propHelper = NULL);
	~CTaskAmbientClips();

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_AMBIENT_CLIPS; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const;

#if !__NO_OUTPUT
	// PURPOSE: Get the name of this task as a string
	virtual atString GetName() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);
	
	// Clone task implementation for CTaskAmbientClips
	virtual CTaskInfo*					CreateQueriableState() const;
	virtual void								ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return					UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void								OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	//virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	//virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	bool	ParentTaskScenarioIsTransitioningToLeaving();
	////////////////////////////////////////////////////////////////////////////////

	virtual bool SupportsTerminationRequest() { return true; }

	// PURPOSE: Returns the full set of flags currently being used
	const fwFlags32& GetFlags() const { return m_Flags; }
	fwFlags32& GetFlags() { return m_Flags; }
	
	// Check if a base clip is playing
	bool IsBaseClipPlaying() const { return !m_LowLodBaseClipPlaying && (m_BaseClip.IsPlayingClip() || m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID); }

	// Check if the base clip is fully blended
	bool IsBaseClipFullyBlended() const { return !m_LowLodBaseClipPlaying && ((m_BaseClip.IsPlayingClip() && m_BaseClip.IsFullyBlended()) || m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID); }

	CAmbientClipRequestHelper& GetBaseClipRequestHelper() { return m_BaseClipRequestHelper; }

	const CConditionalAnimsGroup * GetConditionalAnimsGroup() const { return m_pConditionalAnimsGroup; }
	void SetConditionalAnims( const CConditionalAnimsGroup * pClips ) { m_pConditionalAnimsGroup = pClips; }
	void FindAndSetBestConditionalAnimsGroupForProp();

	s32 GetConditionalAnimChosen() const { return m_ConditionalAnimChosen; }

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Accessor for playing clip
	bool GetIsPlayingAmbientClip() const { return GetClipHelper() != NULL; }

	// PURPOSE:
	const CClipHelper* GetBaseClip() const { return m_BaseClip.GetClipHelper(); }
	CClipHelper* GetBaseClip() { return m_BaseClip.GetClipHelper(); }

	// PURPOSE:
	void SetTimeTillNextClip( const float fTime ) { m_Flags.SetFlag(Flag_OverrideInitialIdleTime); m_fNextIdleTimer = fTime; }

	// Get the look at helper
	CAmbientLookAt* GetLookAtHelper() const { return m_pLookAt; }

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Get any forced prop
	strStreamingObjectName GetForcedProp() const { return (m_pPropHelper && m_pPropHelper->IsForcedLoading()) ? m_pPropHelper->GetPropNameHash() : strStreamingObjectName(); }
	
	// PURPOSE: Give ped a prop - this should be part of the weapon manager, not the ambient task
	bool GivePedProp( const strStreamingObjectName& prop, bool bCreatePropInLeftHand = false, bool bDestroyPropInsteadOfDropping = false );

	void SetBlendInDelta(const float b) { m_fBlendInDelta = b; }

	void SetCleanupBlendOutDelta(float fDelta) { m_fCleanupBlendOutDelta = fDelta; }

	// PURPOSE:	Specify a synchronized scene we can use for the base clip, if we are the master for synchronized animation playback.
	void SetExternalSyncedSceneId(fwSyncedSceneId id) { m_ExternalSyncedSceneId = id; }

	// PURPOSE:	Set who the partner should be, if synchronizing animations
	void SetSynchronizedPartner(CPed* pPed) { m_SynchronizedPartner = pPed; }
	
	void AddEvent(const CAmbientEvent& rEvent);

	//Returns true if this ConditionalAnims group associated with the task supports reaction animations.
	bool HasReactionClip(const CEventShocking* pEvent) const;

	// Returns true if the idle clip is done playing.
	inline bool IsIdleFinished() const { return GetState() != State_PlayIdleAnimation; }

	inline bool IsStreamingOrPlayingIdle() const { return GetState() >= State_StreamIdleAnimation && GetState() <= State_PlayIdleAnimation; }

	//Allow overriding of the clipset and clip that will be used as the base clip...
	void OverrideBaseClip(fwMvClipSetId clipSetId, fwMvClipId clipId);

	void PlayAnimAsIdle(fwMvClipSetId clipSetId, fwMvClipId clipId);

	// Returns true if we are in a MobilePhone or Hangout conversation.
	inline bool IsInConversation() const { return m_Flags.IsFlagSet(Flag_IsInConversation); }
	bool IsInPhoneConversation() const;
	bool IsInHangoutConversation() const;
	bool IsInArgumentConversation() const;
	void SetInConversation(bool bOnOff);

	inline bool CanHangoutChat() const { return m_Flags.IsFlagSet(Flag_AllowsConversation); }
	
	inline bool ShouldPlayPhoneEnterAnim() const { return m_Flags.IsFlagSet(Flag_PhoneConversationShouldPlayEnterAnim); }

	inline void ForcePickNewIdleAnim() 	{ m_Flags.SetFlag(Flag_ForcePickNewIdleAnim); }
	inline void QuitAfterBase()				{ m_Flags.SetFlag(Flag_QuitAfterBase); }
	inline void QuitAfterIdle()    		{ m_Flags.SetFlag(Flag_QuitAfterIdle); }
#if SCENARIO_DEBUG
	void SetOverrideAnimData(const ScenarioAnimDebugInfo& info);
	void ResetConditionalAnimGroup();

	// PURPOSE:	Debug option to not start the regular base animation, for testing
	//			the low LOD fallback animations.
	static bool sm_DebugPreventBaseAnimStreaming;
#endif // SCENARIO_DEBUG

	// PURPOSE:	Get a pointer to the current prop helper object, if any.
	CPropManagementHelper* GetPropHelper() { return m_pPropHelper; }
	const CPropManagementHelper* GetPropHelper() const { return m_pPropHelper; }

	// PURPOSE:	Release the current prop helper object, if any.
	void ClearPropHelper() { m_pPropHelper = NULL; }

	bool TerminateIfChosenConditionalAnimConditionsAreNotMet();
	void TerminateIfBaseConditionsAreNotMet();

	//! Force a re-evaluation. Note: Reset when re-evaulation takes place.
	void ForceReevaluateConditionalAnim() { m_bForceReevaluateChosenConditionalAnim = true; }

	bool ShouldDestroyProp() const { if (m_pPropHelper) { return m_pPropHelper->ShouldDestroyProp(); } else { return false; } }

	static float GetSecondsSinceInWaterThatCountsAsWet() { return sm_Tunables.m_SecondsSinceInWaterThatCountsAsWet; }
	static u32 GetMaxTimeSinceGetUpForAmbientIdles() { return sm_Tunables.m_MaxTimeSinceGetUpForAmbientIdles; }

	CAmbientLookAt* CreateLookAt(const CPed* pPed);

#if !__FINAL
	virtual void Debug() const;
#endif

	// Return the priority slot of the animation which is currently active.
	s32 GetActiveClipPriority();

	// B*2203677: Script control to block/remove ambient idles.
	void SetBlockAmbientIdles(bool bShouldBlock) { m_bShouldBlockIdles = bShouldBlock; }
	void SetRemoveAmbientIdles(bool bShouldRemove) { m_bShouldRemoveIdles = bShouldRemove; }

protected:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE:	For this task, ProcessMoveSignals() handles VFX updates and prop tags.
	virtual bool ProcessMoveSignals();

	// Attempts to start a new conversation helper for nearby conversations. Returns true if a conversation helper is created, false otherwise [8/9/2012 mdawe]
	bool CreateHangoutConversationHelper(CPed** pOutOtherPed);
	
	// Start a hangout conversation. Returns true if the conversation is started.
	bool StartHangoutConversation();

	// Start a hangout argument. Returns true if the conversation is started.
	bool StartHangoutArgumentConversation();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM( const s32 iState, const FSM_Event iEvent );

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort()	const {	return State_NoClipPlaying; }

	////////////////////////////////////////////////////////////////////////////////

	// States
	enum State
	{
		State_Start,
		State_PickPermanentProp,
		State_NoClipPlaying,
		State_StreamIdleAnimation,
		State_SyncFoot,
		State_PlayIdleAnimation,
		State_Exit,
	};

	// FSM function implementations
	FSM_Return Start_OnUpdate();
	FSM_Return PickPermanentProp_OnEnter();
	FSM_Return PickPermanentProp_OnUpdate();
	FSM_Return NoClipPlaying_OnEnter();
	FSM_Return NoClipPlaying_OnUpdate();
	FSM_Return StreamIdleAnimation_OnEnter();
	FSM_Return StreamIdleAnimation_OnUpdate();
	FSM_Return StreamIdleAnimation_OnExit();
	FSM_Return SyncFoot_OnUpdate();
	FSM_Return PlayingIdleAnimation_OnEnter();
	FSM_Return PlayingIdleAnimation_OnUpdate();
	FSM_Return PlayingIdleAnimation_OnExit();

	////////////////////////////////////////////////////////////////////////////////
	
	// Accessors to calculate the timings for the current contexts
	float GetNextIdleTime() const;

	// PURPOSE:	Set the value of m_ConditionalAnimChosen, and update related variables.
	void SetConditionalAnimChosen(int anim);

	// PURPOSE:	Specify whether we should be ignoring low priority shocking events or not.
	//			This updates a count in CPedIntelligence, so it's important that it's not bypassed.
	void SetIgnoreLowPriShockingEvents(bool b);

	// PURPOSE:	If we are a synchronized slave, get the CTaskAmbientClips object from the ped that's the master.
	CTaskAmbientClips* FindSynchronizedMasterTask() const;

	// PURPOSE:	Called on the master's task by a slave, to tell the master that the
	//			slave is not ready yet to play a synchronized idle animation.
	void NotifySlaveNotReadyForSyncedIdleAnim()
	{
		taskAssert(m_Flags.IsFlagSet(Flag_SynchronizedMaster));
		m_Flags.SetFlag(Flag_SlaveNotReadyForSyncedIdleAnim);
	}

	// PURPOSE:	Called on the master's task by a slave, to determine if a synchronized
	//			idle animation has been requested.
	bool IsRequestingSlavesToPlayIdle() const
	{
		return (GetState() == State_StreamIdleAnimation && m_AmbientClipRequestHelper.IsClipSetSynchronized());
	}

private:

	// Idle blocking state query functions
	enum IdleBlockingState
	{
		IBS_NoBlocking = 0,
		IBS_BlockNewIdles,
		IBS_BlockAndRemoveIdles,
	};

	// Idle blocking state query functions
	IdleBlockingState GetIdleBlockingState(float fTimeStep);

	// Update the prop members
	void UpdateProp();

	// Initializes prop management helper
	void InitPropHelper();

	// Frees prop management helper
	void FreePropHelper(bool bClearHelperOnWanderTask);

	bool IsFocusPedTask() const;

	// Update base clip playback
	void UpdateBaseClip();

	void UpdateBaseClipPlayback();

	// Start playing a new base clip, after it has been streamed in
	void StartBaseClip();

	// Start playing the low LOD fallback base clip, for use when the regular base clip isn't available yet
	void StartLowLodBaseClip();

	// Create a conversationHelper if the chosen base clip requires it and there is not an existing conversationHelper. Returns true is a new conversation helper is created. [8/16/2012 mdawe]
	bool CreatePhoneConversationHelper();

	// Start playing a new base clip as an override of the on-foot clip set in the motion task
	void StartOnFootClipSetOverride(const fwMvClipSetId& clipSetIndex);

	// Start or stop playing the clipset as weapon clipset override in the motion task
	void ProcessWeaponClipSetOverride(const fwMvClipSetId& clipSetIndex);

	// Start playing back the base clip using a synchronized scene
	bool StartSynchronizedBaseClip(const fwMvClipSetId& clipSetIndex, const fwMvClipId& iClipID, float fBlend);

	// Start playing a base clip as a regular animation
	bool StartUnsynchronizedBaseClip(const fwMvClipSetId& clipSetIndex, const fwMvClipId& iClipID, float fBlend, bool bExistingClipFound, float fExistingPhase);

	// Stop the base clip, if it's playing
	bool StopBaseClip(float fBlend = NORMAL_BLEND_OUT_DELTA);

	// Check if the base clip that's about to start should be overriding the on-foot clip set in the motion task
	bool ShouldPlayOnFootClipSetOverride() const;

	// Check if the base clip should override the weapon clipset in the motion task
	bool ShouldSetWeaponClipSetOverride() const;

	// Check if the base clip we are about to start needs to be played back as a synchronized scene
	bool ShouldPlaySynchronizedBaseClip() const;

	// Check if the conditions on the currently playing base clip are fulfilled
	bool CheckConditionsOnPlayingBaseClip(const CScenarioCondition::sScenarioConditionData& conditionData) const;

	// Populate a sScenarioConditionData object for use with this task
	void InitConditionData(CScenarioCondition::sScenarioConditionData &conditionDataOut) const;

	// Check if the currently playing base clip should be reevaluated, which happens periodically or when certain conditions change
	bool ShouldReevaluateBaseClip(CScenarioCondition::sScenarioConditionData& conditionDataInOut) const;

	// Update the streaming of base clips
	bool StreamBaseClips(const CScenarioCondition::sScenarioConditionData& conditionData);

	// Check if any of the synchronization flags are set
	bool IsSynchronized() const { return m_Flags.IsFlagSet(Flag_SynchronizedMaster) || m_Flags.IsFlagSet(Flag_SynchronizedSlave); }

	// PURPOSE:	Make appropriate calls to UpdateAnimationVFX() to update any effects that may be playing.
	void UpdateAllAnimationVFX();

	void UpdateAnimationVFX(const CClipHelper* clipHelper);
	void UpdateConditionalAnimVFX(const CConditionalClipSetVFX& vfxData, CPed& ped, int uniqueID, bool isTriggered);
	void UpdateLoopingFlag();
	void ScanClipForPhoneRingRequests();
	void InitIdleTimer();

	// PURPOSE:	Check if a given clip has any tags indicating a need to call UpdateAnimationVFX().
	static bool CheckClipHasVFXTags(const crClip& clip);

	// PURPOSE:	Check if a given clip has any tags indicating a need to call ProcessMoveSignals().
	static bool CheckClipHasPropTags(const crClip& clip);

	// PURPOSE:	Helper function to extract BLEND_OUT_RANGE tag info from a clip.
	static bool ParseBlendOutTagsHelper(const crClip& clip, float& startPhaseOut, float& endPhaseOut);

	// PURPOSE:	Scan the clip for BLEND_OUT_RANGE or Interruptible tags, setting m_fBlendOutPhase and m_BlendOutRangeTagsFound.
	void ParseBlendOutTags(const crClip& clip);

	// PURPOSE:	Clear out any stored blend phase (m_BlendOutPhaseFound/m_fBlendOutPhase).
	void ResetBlendOutPhase() { m_BlendOutRangeTagsFound = false; m_fBlendOutPhase = 1.0f; }

	bool ShouldHangoutBreakup() const;
	
	void PlayConversationIfApplicable();

	// Update the argument conversation being had.
	void ProcessArgumentConversation();
	
	// Update the normal hangout conversation being had.
	void ProcessNormalConversation(CPed* pPlayerPed);

	// If our "player nearby" timer has expired, this handles breaking the Ped out of this scenario and into something else.
	void BreakupHangoutScenario();

	// Find a conditional anim based on type and clipset id
	const CConditionalClipSet* FindConditionalAnim(CConditionalAnims::eAnimType animType, const u32 clipSetId);

	// Head-bob functions 
	void HeadbobSyncWithRadio(CPed *pPed);
	float CalculateHeadbobAnimRate();

	//
	// Members
	//

	////////////////////////////////////////////////////////////////////////////////
	//Network clone to local, local to clone, migrate helper functions
	////////////////////////////////////////////////////////////////////////////////
	bool MigrateProcessPreFSM();
	bool MigrateUpdateFSM(const s32 iState, const FSM_Event iEvent);
	bool MigrateStart_OnUpdate();
	bool MigrateNoClipPlaying_OnEnter();
	bool MigratePlayingIdleAnimation_OnEnter();
	bool MigratePlayingIdleAnimation_OnUpdate();
	CTaskAmbientMigrateHelper *GetTaskAmbientMigrateHelper();

	bool IsInAllowedStateForMigrate() { return (GetState()==State_NoClipPlaying || GetState()==State_StreamIdleAnimation || GetState()==State_PlayIdleAnimation ); }
	
	// Lowrider alternate (left arm on window) network functions/helpers.
	bool			ShouldInsertLowriderArmNetwork(bool bReturnTrueIfNotUsingAltClipset = false) const;
	bool			InsertLowriderArmNetworkCB(fwMoveNetworkPlayer* pPlayer, float fBlendDuration, u32);
	void			BlendInLowriderClipCB(CSimpleBlendHelper& helper, float blendDelta, u32 uFlags);
	void			BlendOutLowriderClipCB(CSimpleBlendHelper& helper, float blendDelta, u32 uFlags);
	void			SendLowriderArmClipMoveParams(float fBlendDuration);
	const crClip*	GetLowriderLowDoorClip(fwMvClipSetId clipSetId, bool bIsBaseClip BANK_ONLY(, const char* &desiredClipName)) const;
	void			ClearLowriderArmNetwork(const float fDuration, u32 uFlags = 0);
	CMoveNetworkHelper m_LowriderArmMoveNetworkHelper;
	bool			m_bInsertedLowriderCallbacks;

	// Event helper
	CAmbientEvent m_Event;

	// Look at helper
	CAmbientLookAt* m_pLookAt;

	// Conversation helper [7/18/2012 mdawe]
	CTaskConversationHelper* m_pConversationHelper;

	const CConditionalAnimsGroup * m_pConditionalAnimsGroup;

	// PURPOSE:	Helper to hold on to a prop
	CPropManagementHelper::SPtr m_pPropHelper;

	// Timer to track how long a player has been nearby scenarios that could break up. [8/8/2012 mdawe]
	float m_fPlayerNearbyTimer;

	// Timers used to randomly play clips
	float m_fNextIdleTimer;
	float m_fTimeLastSearchedForBaseClip;
	
	float m_fBlendInDelta;
	float m_fCleanupBlendOutDelta;

	// PURPOSE:	Track at what phase we can blend out an idle animation.
	float m_fBlendOutPhase;

	// Animations - prop, and base
	CClipHelper m_BaseClip;

	// Animation information - selected clip to be played
	fwMvClipSetId m_ClipSetId;
	fwMvClipId m_ClipId;
	fwMvClipId m_PropClipId;
	
	//Overrides
	fwMvClipSetId m_OverriddenBaseClipSet;
	fwMvClipId m_OverriddenBaseClip;

#if SCENARIO_DEBUG
	ScenarioAnimDebugInfo m_DebugInfo;
#endif // SCENARIO_DEBUG

	//For network cloning
	s32			m_CloneInitialConditionalAnimChosen;
	fwFlags32	m_CloneInitialFlags;

	// PURPOSE:	Optional pointer to a condition that needs to be checked on each
	//			update in order for the base animation to continue to play.
	// NOTES:	Could be turned into an array if we have a need to check multiple conditions.
	const CScenarioCondition* m_BaseAnimImmediateCondition;

	// PURPOSE:	Similar to m_BaseAnimImmediateCondition, but applies to the currently
	//			playing idle animation. Mostly needed to avoid playing walk idles while
	//			standing still, and vice versa.
	const CScenarioCondition* m_IdleAnimImmediateCondition;

	// Streaming request helpers
	CAmbientClipRequestHelper m_AmbientClipRequestHelper;
	CAmbientClipRequestHelper m_BaseClipRequestHelper;

	// Distance the player is away from here if this is a conversation scenario
	float				m_fPlayerConversationScenarioDistSq;

	// Flags
	fwFlags32 m_Flags;

	// PURPOSE:	If synchronizing animations, this is the partner
	RegdPed m_SynchronizedPartner;

	// PURPOSE:	If we are currently playing back the base clip as a synchronized scene
	//			(or would do so, if we weren't playing an idle animation), this is the
	//			synchronized scene ID we are using.
	// NOTES:	This comes from m_ExternalSyncedSceneId if we are the master, or from the
	//			master if we are a slave.
	fwSyncedSceneId		m_BaseSyncedSceneId;

	// PURPOSE:	ID for a synchronized scene we can use for the base clip, if we are the
	//			master for synchronized animation playback.
	// NOTES:	Must be set by the user, in order to support synchronized animations.
	fwSyncedSceneId		m_ExternalSyncedSceneId;

	// PURPOSE:	While playing a synchronized idle animation, this is the scene to use.
	//			It gets set only during State_PlayIdleAnimation, is reference-counted,
	//			and is created by the master.
	fwSyncedSceneId		m_IdleSyncedSceneId;

	// PURPOSE:	Slot in ptfxAssetStore for the assets we need for effects, if m_VfxRequested is true.
	strLocalIndex		m_VfxStreamingSlot;

	s16					m_ConditionalAnimChosen;

	// PURPOSE:	Used to prevent unnecessarily frequent checks for idle animation conditions.
	u8					m_UpdatesUntilNextConditionalAnimCheck;

	// PURPOSE:	True if we found BLEND_OUT_RANGE tags in the clip that's playing.
	bool				m_BlendOutRangeTagsFound:1;

	// PURPOSE:	True if the base clip we are currently using is the low LOD fallback clip
	//			we use when the animation we want isn't streamed in.
	bool				m_LowLodBaseClipPlaying;

	// PURPOSE:	True if the base clip we wanted wasn't streamed in, and we have considered
	//			using a low LOD fallback clip. Used so we don't have to check for it again
	//			if we don't want one.
	bool				m_LowLodBaseClipConsidered;

	// PURPOSE:	Used on slaves to control calls to NotifySlaveNotReadyForSyncedIdleAnim().
	bool				m_SlaveReadyForSyncedIdleAnim:1;

	// PURPOSE:	If the current animations have some VFX, this is true if those are currently considered in range to be actively updated.
	bool				m_VfxInRange:1;

	// PURPOSE:	True if we have done a streaming request for VFX, or, at least tried to do so.
	bool				m_VfxRequested:1;

	// PURPOSE:	True if we have detected that the currently playing base clip has
	//			VFX tags and requires calls to UpdateAnimationVFX().
	bool				m_VfxTagsExistBase:1;

	// PURPOSE:	True if we have detected that the currently playing idle clip has
	//			VFX tags and requires calls to UpdateAnimationVFX().
	bool				m_VfxTagsExistIdle:1;

	// PURPOSE:	True if we have detected that the currently playing base clip has
	//			prop tags and requires calls to ProcessMoveSignals().
	bool				m_PropTagsExistBase:1;

	// PURPOSE:	True if we have detected that the currently playing idle clip has
	//			prop tags and requires calls to ProcessMoveSignals().
	bool				m_PropTagsExistIdle:1;

	// PURPOSE:	Set by ProcessMoveSignals() when in the StreamIdleAnimation state
	//			and we have reached the end of the base animation.
	bool				m_ReachedEndOfBaseAnim:1;

	// PURPOSE: True if the stored ranges for allowing head IK are associated with the base clip.  False for the idle clip.
	bool				m_bUseBaseClipForHeadIKCheck:1;

	// PURPOSE: Intended to force ped to re-evaluate the currently selected conditional anim group. I.e. something external has modified ped state
	// that may require a new group.
	bool				m_bForceReevaluateChosenConditionalAnim:1;

	// PURPOSE: Track whether the requested idle clip has been loaded by m_AmbientClipRequestHelper.
	bool				m_bIdleClipLoaded:1;

	// PURPOSE: True if this ped has met the conditions to start a confrontational conversation.
	bool				m_bCanHaveArgument:1;

	// PURPOSE: Set by script (CommandSetCanPlayAmbientIdles) to block/remove ambient idle clips.
	bool				m_bShouldBlockIdles;
	bool				m_bShouldRemoveIdles;

	// Head bob parameters.
	// PURPOSE: Keep head bob anims in sync with music on radio
	float				m_fBeatTimeS;
	float				m_fBPM;
	s32					m_iBeatNumber;
	bool				m_bFlaggedToPlayBaseClip;
	bool				m_bAtBeatMarker;
	bool				m_bAtRockOutMarker;
	bool				m_bPlayBaseHeadbob;
	s32					m_sRockoutStart;
	s32					m_sRockoutEnd;
	s32					m_iTimeLeftToRockout;
	s32					m_iVehVelocityTimerStart;
	float				m_fRandomBaseDuration;
	float				m_fRandomBaseStartTime;
	float				m_fRandomBaseTimer;
	bool				m_bBlendOutHeadbob;
	bool				m_bNextBeat;
	bool				m_bReevaluateHeadbobBase;
	float				m_fTimeToPlayBaseAnimBeforeAfterRockout;
	int					m_sBlockedRockoutSection;
	bool				m_bBlockHeadbobNextFrame;

	// PURPOSE: Forces anim re-evaluation when modified.
	bool				m_bUsingAlternateLowriderAnims;

	// PURPOSE: Used to detect whether we need to block/remove idles from clone ped in vehicle when doing LookAt.
	Vector3				m_vClonePreviousLookAtPosInVehicle;
	float				m_fCloneLookAtTimer;

#if __BANK
	// used to prevent queueing multiple base animations when working with debug animation data.
	bool m_DebugStartedBaseAnim:1;
#endif

	// Static tuning data.
	static Tunables sm_Tunables;
};


/*
PURPOSE
	This task is a helper to be used to manipulate CTaskAmbientClips tasks 
	when migrating from local to clone or clone to local ownership. 
	It will check the idle and base clip status of a current CTaskAmbientClips task
	and store a snapshot of its anims and phase/rate so they can be applied to
	the incoming clone/local task after migrate.
*/


class CTaskAmbientMigrateHelper 
{
public:
	CTaskAmbientMigrateHelper(const CTaskAmbientClips* pAmbientTask);
	CTaskAmbientMigrateHelper();
	~CTaskAmbientMigrateHelper();
	
	bool	MigrateInProgress() {return GetHasBaseClipMigrateData(); }

	bool GetValidBaseClipRunningAtMigrate() {return  (m_BaseClipSet!=CLIP_SET_ID_INVALID && m_BaseClip!=CLIP_ID_INVALID) ;}
	bool GetHasBaseClipMigrateData() {return  ( GetValidBaseClipRunningAtMigrate() || m_BaseClipRequestHelper.GetState() != CAmbientClipRequestHelper::AS_noGroupSelected ) ;}
	bool GetHasIdleClipMigrateData() {return  (m_IdleClipSet!=CLIP_SET_ID_INVALID && m_IdleClip!=CLIP_ID_INVALID) ;}
	bool GetHasMigrateData() {return ( GetHasBaseClipMigrateData() || GetHasIdleClipMigrateData() );}

	void ClearHasBaseClipMigrateData() { m_BaseClipSet=CLIP_SET_ID_INVALID; m_BaseClip=CLIP_ID_INVALID ;  m_BaseClipRequestHelper.ReleaseClips() ;}
	void ClearHasIdleClipMigrateData() { m_IdleClipSet=CLIP_SET_ID_INVALID; m_IdleClip=CLIP_ID_INVALID ;}

	void GetClipMigrateDataFromTaskAmbient(const CTaskAmbientClips* pAmbientTask);
	void GetConditionalAnimsGroupDataFromTaskAmbient(const CTaskAmbientClips* pAmbientTask);
	bool ApplyConditionalAnimsGroupDataToTaskAmbient(CTaskAmbientClips* pAmbientTask);
	bool ApplyClipMigrateDataToTaskAmbient( CTaskAmbientClips* pAmbientTask);
	void ApplyMigrateIdleClip( CTaskAmbientClips* pAmbientTask );
	void ApplyMigrateBaseClip( CTaskAmbientClips* pAmbientTask, bool bDeferredStart=false);
	
	void Reset();
	void NetObjPedCleanUp(CPed* pPed);

	enum
	{
		WaitStateNone,
		WaitStateWaiting,
	};

//private:

	float m_fNextIdleTimer;
	float m_fTimeLastSearchedForBaseClip;

	//If parent task is a scenario task containing the conditional anim group
	int m_RealScenarioType;

	s16	m_ConditionalAnimChosen;
	u32 m_ConditionalAnimGroupHash;
#if __ASSERT
	static const char* GetAssertNetObjPedName(const CTaskAmbientClips* pAmbientTask);
#endif

	// Prop helper
	CPropManagementHelper::SPtr m_pPropHelper;

	s32	  m_State;
	float m_BaseClipPhase;
	float m_BaseClipRate;

	float m_IdleClipPhase;
	float m_IdleClipRate;

	// Flags
	fwFlags32 m_InitialFlags;  //use these to keep track of the initial create flags when creating new clone peds

	//Overrides
	fwMvClipSetId	m_BaseClipSet;
	fwMvClipId		m_BaseClip;
	fwMvClipSetId	m_IdleClipSet;
	fwMvClipId		m_IdleClip;

	fwMvClipSetId	m_ClipSetId;
	fwMvClipId		m_ClipId;

	//m_ParentTaskTypeAtMigrate keeps a record of what task was parent at migrate in case  
	//a new ambient is applied in between migrate from another task e.g when fleeing so allowing
	//us to check post migrate and abort applying migrate data from previous task if this is different
	int				m_ParentTaskTypeAtMigrate;

	// Streaming request helpers
	CAmbientClipRequestHelper	m_AmbientClipRequestHelper;
	CAmbientClipRequestHelper	m_BaseClipRequestHelper;

private:
	s32	  m_WaitState;

	void	SetWaiting()		{m_WaitState = WaitStateWaiting;}
	bool	GetResetWaiting()	{s32 sWaiting = m_WaitState;  m_WaitState = WaitStateNone; return (sWaiting==WaitStateWaiting); }

};

////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	This task is intended to run on the secondary task tree. If CTaskAmbientClips
	and CAmbientLookAt are running, it makes the ped look at an event that's taking
	place in the world, while carrying out other tasks. For example, a pedestrian
	who sees the player holding a weapon might look at it while walking by.
*/
class CTaskAmbientLookAtEvent : public CTask
{
public:
	enum State
	{
		State_Start,
		State_Looking,

		kNumStates
	};

	explicit CTaskAmbientLookAtEvent(const CEvent* pEventToLookAt);
	~CTaskAmbientLookAtEvent();

	virtual aiTask* Copy() const;
	virtual int	GetTaskTypeInternal() const { return CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT; }
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Start; }
	virtual bool MakeAbortable(const AbortPriority priority, const aiEvent* pEvent);

	CEntity* GetLookAtTarget() const;
	CEvent* GetEventToLookAt() const; 
	bool GetCommentOnVehicle() const { return m_CommentOnVehicle; }
	void SetCommentOnVehicle(bool val) { m_CommentOnVehicle = val; }
	bool GetIgnoreFov() const { return m_bIgnoreFov; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

protected:
	FSM_Return Start_OnUpdate();
	FSM_Return Looking_OnUpdate();

	// PURPOSE: Comment on Vehicles
	bool m_CommentOnVehicle;
	bool m_bIgnoreFov;

	RegdEvent	m_pEventToLookAt;	// Owned copy.
};

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Task info for ambient clips
//-------------------------------------------------------------------------
class CClonedTaskAmbientClipsInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskAmbientClipsInfo();
	CClonedTaskAmbientClipsInfo(u32 flags, u32 conditionalAnimGroupHash, s32 conditionalAnimChosen );
	~CClonedTaskAmbientClipsInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_AMBIENT_CLIPS;}

	u32			GetConditionalAnimGroupHash()			{ return m_conditionalAnimGroupHash;  }

	s32			GetConditionalAnimChosen()				{ return m_ConditionalAnimChosen;  }
	u32			GetFlags()								{ return m_Flags;  }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return false; }
	virtual const char*	GetStatusName(u32) const { return "Ambient Clips State";}

	void Serialise(CSyncDataBase& serialiser)
	{
		taskAssertf( (m_ConditionalAnimChosen >= -1) && (m_ConditionalAnimChosen <= MAX_CONDITIONAL_ANIM_CHOSEN ), "m_ConditionalAnimChosen %d ", m_ConditionalAnimChosen );

		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_UNSIGNED(serialiser, m_conditionalAnimGroupHash,	SIZEOF_CONDITIONAL_ANIM_GROUP_HASH, "Conditional Anim Group hash");
		SERIALISE_INTEGER(serialiser, m_ConditionalAnimChosen,		SIZEOF_CONDITIONAL_ANIM_CHOSEN,		"Conditional anim chosen");
		SERIALISE_UNSIGNED(serialiser, m_Flags,						SIZEOF_FLAGS,						"Flags");
	}

private:
	static const unsigned int SIZEOF_CONDITIONAL_ANIM_GROUP_HASH		= 32;

	static const		  int MAX_CONDITIONAL_ANIM_CHOSEN				= 255;
	static const unsigned int SIZEOF_CONDITIONAL_ANIM_CHOSEN			= datBitsNeeded<MAX_CONDITIONAL_ANIM_CHOSEN>::COUNT + 1; //Allows for conditional anims between -1 and 255 
	static const unsigned int SIZEOF_FLAGS								= 32;  //Max needed flag
	

	u32		m_conditionalAnimGroupHash;
	s32		m_ConditionalAnimChosen;
	u32		m_Flags;
};

#endif // TASK_AMBIENT_H
