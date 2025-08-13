#ifndef PED_INTELLIGENCE_H
#define PED_INTELLIGENCE_H

struct tGameplayCameraSettings;

class CClimbHandHold;
class CEventDecisionMaker;
class CNetObjPed;
class CTaskCombat;
class CTaskGun;
class CTaskMelee;
class CTaskWrithe;
class CTaskMeleeActionResult;
class CTaskSmartFlee;
class CTaskNMBehaviour;
class CTaskClimbLadder;
class CTaskSimpleClimb;
class CTaskCrouch;
class CTaskSimpleInAir;
class CTaskSimpleMoveSwim;
class CTaskMove;

// Game headers
#include "AI/EntityScanner.h"
#include "Event/EventGroup.h"
#include "Event/EventHandler.h"
#include "Event/decision/EventDecisionMakerResponse.h"
#include "PathServer/PathServer.h"
#include "Peds/CombatBehaviour.h"
#include "Peds/FleeBehaviour.h"
#include "Peds/NavCapabilities.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedIntelligence/PedMotivation.h"
#include "Peds/PedIntelligence/PedPerception.h"
#include "Peds/DefensiveArea.h"
#include "Peds/PedEventScanner.h"
#include "Peds/PedStealth.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/QueriableInterface.h"
#include "Peds/Relationships.h"
#include "Scene/RegdRefTypes.h"
#include "Task/Combat/CombatDirector.h"
#include "Task/Default/AmbientAudio.h"
#include "Task/Movement/Climbing/ClimbDetector.h"
#include "task/Movement/Climbing/TaskDropDown.h"
#include "task/Scenario/ScenarioPoint.h"
#include "Task/System/Task.h"
#include "Task/System/TaskManager.h"
#include "Task/System/TaskTreeClone.h"
#include "Weapons/FiringPattern.h"
#include "Templates/CircularArray.h"

//
// Config
//
#define DEBUG_EVENT_HISTORY __BANK
#if DEBUG_EVENT_HISTORY
#define DEBUG_EVENT_HISTORY_ONLY(X) X
#else 
#define DEBUG_EVENT_HISTORY_ONLY(X)
#endif // DEBUG_EVENT_HISTORY
//
//
//

///////////////
//Ped intelligence
///////////////

///////////////////////////////////////////////////
// CLASS: CPedEventDecisionMaker
// PURPOSE: Ped decision making instance data
///////////////////////////////////////////////////
class CPedEventDecisionMaker
{
public:

	// STRUCT: Ped Event Response
	// PURPOSE: Stores the particulars of one ped response to events (cooldowns, etc.)
	struct SResponse
	{
		SResponse() : eventType(EVENT_INVALID), uEventTS(0u)	{ /* ... */ }

		float GetCooldownTimeLeft() const 
		{
			const u32 uCooldownEndTS = uEventTS + static_cast<u32>(Cooldown.Time * 1000.0f);
			const u32 uNowTS = fwTimer::GetTimeInMilliseconds();
			return Cooldown.Time >= 0.0f && uCooldownEndTS > uNowTS ? (uCooldownEndTS - uNowTS) / 1000.0f : 0.0f; 
		}

		bool HasCooldownExpired() const { return GetCooldownTimeLeft() <= 0.0f; }

		eEventType eventType;
		Vec3V vEventPosition;
		u32 uEventTS;

		CEventDecisionMakerResponse::sCooldown Cooldown;
	};

	// Intialization / Destruction
	CPedEventDecisionMaker()
		: m_uDecisionMakerId(0u)
	{ /* ... */	}

	void SetDecisionMakerId(const u32 uId) { m_uDecisionMakerId = uId; }
	u32  GetDecisionMakerId() const { return m_uDecisionMakerId; }

#if !__FINAL
	const char* GetDecisionMakerName() const;
#endif // !__FINAL
	
	bool HandlesEventOfType(eEventType eventType) const;
	void MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, CEventResponseTheDecision &theDecision);

	void Update(const CPed& rPed, bool bFullUpdate);

	// Responses management
	u32 GetNumResponses() const { return m_aResponses.GetCount(); }
	const SResponse& GetResponseAt(u32 uResponseIdx) const { aiAssert(uResponseIdx < GetNumResponses()); return m_aResponses[uResponseIdx]; }

private:

	// Decision making
	bool IsEventResponseDisabled(const eEventType eventType) const;
	void OnDecisionMade(const CEvent& rEvent, const CEventResponseTheDecision& rDecision);

	// Decision maker
	const CEventDecisionMaker* GetDecisionMaker() const;
	CEventDecisionMaker* GetDecisionMaker();

	// Responses management
	void UpdateResponses(const CPed& rPed);
	SResponse* AddResponse(const eEventType eventType, const CEventDecisionMakerResponse::sCooldown& rCooldown, Vec3V_In vPosition, u32 uTS);
	SResponse* FindResponse(const eEventType eventType);
	const SResponse* FindResponse(const eEventType eventType) const;

	//
	typedef atArray<SResponse> TResponseArray;

	u32 m_uDecisionMakerId;
	TResponseArray m_aResponses;
};

///////////////
//Ped intelligence
///////////////

class CPedIntelligence
{
public:

	FW_REGISTER_CLASS_POOL(CPedIntelligence);

	friend class CNetObjPed;
    friend class CPedDebugVisualiser;

	CPedIntelligence(CPed* pPed);
	~CPedIntelligence();

	// -----------------------------------------------------------------------------
	// DECISION MAKER INTERFACE

	//Get/set decision maker (the index into the array of decision makers).
	const CPedEventDecisionMaker& GetPedDecisionMaker() const { return m_PedDecisionMaker; }
	CPedEventDecisionMaker& GetPedDecisionMaker() { return m_PedDecisionMaker; }

	void SetDecisionMakerId(u32 uDecisionMakerId);
	u32 GetDecisionMakerId() const { return m_PedDecisionMaker.GetDecisionMakerId(); }

#if !__FINAL
	const char* GetDecisionMakerName() const { return m_PedDecisionMaker.GetDecisionMakerName(); }
#endif // !__FINAL

	// -----------------------------------------------------------------------------
	// TASK MANAGER INTERFACE

	CTaskManager* GetTaskManager() { return(&m_taskManager); }

	// CLEAR TASKS AND RESPONSES ---------------------------------
	void ClearPrimaryTask();
	void ClearSecondaryTask(s32 iType);
	void ClearPrimaryTaskAtPriority(s32 iPriority);
	void ClearTasks(const bool bClearMainTask=true, const bool bClearSecondaryTask=true);
	void FlushImmediately(const bool bRestartDefaultTasks=false, const bool bAbortMotionTasks=true, const bool bAbortRagdoll=true, const bool bProcessTaskRecord=true);

	//Clear the event response task.
	void ClearTaskEventResponse();
	void ClearTaskTempEventResponse();

	// Clear the event ragdoll response
	void ClearRagdollEventResponse() { GetEventHandler()->ClearRagdollResponse(); }

	void ClearTasksAbovePriority(const s32 iPriority);

	// ADD TASKS --------------------------------------------------
	//Add all the types of task.
	void AddTaskAtPriority(aiTask* pTask, s32 iPriority, const bool bForce=false);
	void AddTaskDefault(aiTask* pTask, const bool bForce=false);
	void AddTaskSecondary(aiTask* pTask, const int iType);
	void AddTaskMovement(aiTask* pTask);
	void AddTaskMovementResponse(aiTask* pTask);
	void AddLocalCloneTask(CTaskFSMClone* pCloneTask, u32 const iPriority);
	bool HasPendingLocalCloneTask() const;
	CTask* GetActiveCloneTask();

	// QUERY/GET TASKS IN SLOTS -----------------------------------
	// Get all the types of task.
	CTask* GetTaskEventResponse() const;
	void GetTasksEventRespone(	CTask*& pOutTaskEventResponse, 
								CTask*& pOutTempTaskEventResponse, 
								CTask*& pOutNonTempTaskEventResponse, 
								CTask*& pOutMovementTaskEventResponse) const;
	CTask* GetTaskAtPriority(s32 iPriority) const;
	CTask* GetTaskDefault() const;
	s32  GetActiveTaskPriority() const;
	CTask* GetTaskActive() const; //Get the task that is being processed.
	CTask* GetTaskActiveSimplest() const;   //Get the leaf of the task chain of the task that is being processed.

	// Secondary
	CTask* GetTaskSecondaryPartialAnim() const;
	CTask* GetTaskSecondary(const int iType) const;
	CTask* GetTaskSecondaryActive() const;

	// Movement
	CTask* GetMovementTaskAtPriority(s32 iPriority) const;
	CTaskMove* GetActiveSimplestMovementTask() const;   //Get the leaf of the movement task chain of the task that is being processed.   
	CTask* GetActiveMovementTask() const;  //Get the movement task that is being processed.     
	CTask* GetMovementResponseTask() const;
	CTask* GetGeneralMovementTask() const;
	CTask* GetDefaultMovementTask() const;

	// Motion 
	CTask* GetMotionTaskActiveSimplest() const; 

	// FIND TASKS BY TASKTYPE---------------------------------

	//Find a task in the task chain of a specific type (returns
	//null if no task is found of the specific type).
	CTask* FindTaskActiveByTreeAndType(const int iTree, const int iType) const;
	CTask* FindTaskActiveByType(const int iType) const;
	CTask* FindTaskEventResponseByType(const int iType) const;
	CTask* FindTaskPhysicalResponseByType(const int iType) const;
	CTask* FindTaskPrimaryByType(const int iType) const;
	CTask* FindTaskSecondaryByType(const int iType) const;
	CTask* FindTaskDefaultByType(const int iType) const;
	CTask* FindTaskByType(const int iType) const;
	CTask* FindMovementTaskByType(const int iType) const;
	CTask* FindTaskActiveMovementByType(const int iType) const;
	CTask* FindTaskActiveMotionByType(const int iType) const;
	bool HasTaskSecondary(const aiTask* pTask) const;

	// FIND SPECIFIC TASKS  -----------------------------------

	// Query functions for specific task types.
	CTaskClimbLadder*				GetTaskClimbLadder() const;
	CTaskCombat*					GetTaskCombat() const;
	CTaskGun*						GetTaskGun() const;
	CTaskCombat*					FindTaskCombat() const;
	CTaskGun*						FindTaskGun() const;
	CTaskWrithe*					GetTaskWrithe() const;
	CTaskMelee*						GetTaskMelee() const;
	CTaskMeleeActionResult*			GetTaskMeleeActionResult() const;
	CTaskSmartFlee*					GetTaskSmartFlee() const;
	CTaskSmartFlee*					FindTaskSmartFlee() const;
	CTaskSimpleMoveSwim*			GetTaskSwim() const;
	bool IsPedSwimming() const;
	bool IsPedInAir() const;
	bool IsPedClimbing() const;
	bool IsPedVaulting() const;
	bool IsPedClimbingLadder() const;
	bool IsPedInMelee() const;
	bool IsPedBlindFiring() const;
	bool IsPedFalling() const;
	bool IsPedJumping() const;
	bool IsPedLanding() const;
	bool IsPedDiving() const;
	bool IsPedGettingUp() const;
	static float DISTANCE_TO_START_COVER_CAM_WHEN_SLIDING;
	bool GetPedCoverStatus(s32& iCoverState, bool& bCanFire) const;

	float GetMoveBlendRatioFromGoToTask(void) const;
	
	// -----------------------------------------------------------------------------
	// COMBAT INTERFACE
	
	CCombatDirector*	GetCombatDirector()		const { return m_pCombatDirector; }

	void SetCombatDirector(CCombatDirector* pCombatDirector) { m_pCombatDirector = pCombatDirector; }

	void RefreshCombatDirector();
	void ReleaseCombatDirector();
	void RequestCombatDirector();
	
	CCombatOrders*		GetCombatOrders() const;

	// -----------------------------------------------------------------------------
	// EVENT INTRERFACE


	void FlushEvents(void);

	CEvent* AddEvent(const CEvent& rEvent, const bool bForcePersistence=false, const bool bRemoveOtherEventsToMakeSpace = true);
	void RemoveEvent(CEvent* pEvent);
	bool HasEventOfType(const CEvent* pEvent) const;
	bool HasEventOfType(const int iEventType) const;
	bool HasDuplicateEvent(const CEvent* pEvent) const;
	CEvent* GetEventOfType(const int iEventType) const;
	int CountEventGroupEvents() const;
	CEvent* GetEventByIndex(const int iEventType) const;
	bool HasEvent(const CEvent* pEvent) const;
	bool HasRagdollEvent() const;
	bool HasPairedDamageEvent() const;
	void TickEvents();
	void UnTickEvents();
	void RemoveEventsOfType(const int iEventType);
	void FlagEventForDeletion(CEvent* pEvent);
	void RemoveExpiredEvents();
	void RemoveInvalidEvents(bool bEverythingButScriptEvents=false, bool bFlagForDeletion = false);
	void UpdateCommunicationEvents(CPed* pPed);
	CEvent* GetHighestPriorityEvent() const;
	int GetCurrentEventType() const {return GetEventHandler()->GetCurrentEventType();}
	CEvent* GetCurrentEvent() const {return GetEventHandler()->GetCurrentEvent();}

#if __ASSERT
	bool IsRunningRagdollTask();
#endif //__ASSERT

	// Natural Motion event handling.
	CTaskNMBehaviour* GetLowestLevelNMTask(CPed* pThisPed) const;
	
	// Natural Motion event handling.
	CTask* GetLowestLevelRagdollTask(CPed* pThisPed) const;

	// Record current response event
	void OverrideCurrentEventResponseEvent( CEvent& event ) {return GetEventHandler()->OverrideCurrentEventResponseEvent(event);}


	aiTask*	GetEventResponseTask() {return GetEventHandler()->GetResponse().GetEventResponse(CEventResponse::EventResponse);}
	void	RemoveEventResponseTask() {return GetEventHandler()->GetResponse().ClearEventResponse(CEventResponse::EventResponse);}
	aiTask*	GetPhysicalEventResponseTask() { return GetEventHandler()->GetResponse().GetEventResponse(CEventResponse::PhysicalResponse); }
	void	RemovePhysicalEventResponseTask() {return GetEventHandler()->GetResponse().ClearEventResponse(CEventResponse::PhysicalResponse);}

	int GetNumEventsInQueue() const {return m_eventGroup.GetNumEvents();}
	CEventGroupPed& GetEventGroup() {return m_eventGroup;}

	bool IsRespondingToEvent(const int iEventType) const;

	// returns true if the ped's decision maker has a response to this event type (defined in EventData.h)
	bool HasResponseToEvent(int iEventType) const;

	// -----------------------------------------------------------------------------
	// RELATIONSHIP INTERFACE

	static bool AreFriends(const CPed& ped1, const CPed& ped2);
	bool IsFriendlyWithEntity(const CEntity* pEntity, bool bIncludeFriendsDueToScenarios, bool bDebug = false) const;
	bool IsFriendlyWithVehicleOccupants(const CVehicle& vehicle, bool bIncludeFriendsDueToScenarios, bool bDebug = false) const;
	bool IsFriendlyWith(const CPed& otherPed, bool bDebug = false) const ;
	bool IsFriendlyWithByAnyMeans(const CPed& otherPed) const ;
	bool IsFriendlyWithDueToSameVehicle(const CPed& otherPed) const ;
	bool IsFriendlyWithDueToScenarios(const CPed& otherPed) const ;
	bool IsThreatenedBy(const CPed& otherPed, bool bIncludeDislike = true, bool bIncludeNonFriendly = false, bool bForceInCombatCheck = false) const;
	bool IsPedThreatening(const CPed& rOtherPed, bool bIncludeNonViolentWeapons) const;
	bool Respects(const CPed& otherPed) const;
	bool Ignores(const CPed& otherPed) const;

	u8 CountFriends(float fMaxDistance, bool bDueToScenarios = false, bool bDueToSameVehicle = false) const;

	// -----------------------------------------------------------------------------
	// GETNEARBY X INTERFACE

	// Peds
	const CEntityScannerIterator		GetNearbyPeds() const				{ return m_pedScanner.GetIterator(); }
	CEntityScannerIterator				GetNearbyPeds()						{ return m_pedScanner.GetIterator(); }
	CPed*								GetClosestPedInRange()				{ return m_pedScanner.GetClosestPedInRange(); }
	// Vehicles
	//const CEntityScannerIterator		GetNearbyVehicles() const			{ return m_vehicleScanner.GetIterator(); }
	CEntityScannerIterator				GetNearbyVehicles(); //					{ return m_vehicleScanner.GetIterator(); }
	CVehicle*							GetClosestVehicleInRange(bool bForce = false); //			{ return m_vehicleScanner.GetClosestVehicleInRange(); }
	// Doors
	const CEntityScannerIterator		GetNearbyDoors() const			{ return m_doorScanner.GetIterator(); }
	CEntityScannerIterator				GetNearbyDoor()					{ return m_doorScanner.GetIterator(); }
	CDoor*								GetClosestDoorInRange()			{ return m_doorScanner.GetClosestDoorInRange(); }
	// Objects
	CEntityScannerIterator				GetNearbyObjects();
	CObject*							GetClosestObjectInRange();
	// Scanners
	const CPedScanner*					GetPedScanner() const		{ return &m_pedScanner;    }
	const CDoorScanner*					GetDoorScanner() const		{ return &m_doorScanner;   }
	const CVehicleScanner*				GetVehicleScanner() const	{ return &m_vehicleScanner;}
	const CObjectScanner*				GetObjectScanner() const	{ return &m_objectScanner; }
	CPedScanner*						GetPedScanner()				{ return &m_pedScanner;    }
	CDoorScanner*						GetDoorScanner()			{ return &m_doorScanner;   }
	CVehicleScanner*					GetVehicleScanner()			{ return &m_vehicleScanner;}
	CObjectScanner*						GetObjectScanner()			{ return &m_objectScanner; }
	const CClimbDetector&				GetClimbDetector() const	{ return m_climbDetector;  }
	CClimbDetector&						GetClimbDetector()			{ return m_climbDetector;  }
	const CDropDownDetector&			GetDropDownDetector() const	{ return m_dropDownDetector;}
	CDropDownDetector&					GetDropDownDetector()		{ return m_dropDownDetector;}
	void								ClearScanners();

	// -----------------------------------------------------------------------------
	// TARGETTING

	bool CanAttackPed(const CPed* pPed);

	// Returns a pointer to the peds targeting system
	inline CPedTargetting* GetTargetting( const bool bWakeUpIfInactive = true );
	// Returns if the targeting system is active
	bool GetIsTargetingSystemActive() const { return m_pPedTargetting != NULL; }
	// Shuts down the peds targeting if it is active
	void ShutdownTargetting( void );
	// Updates the peds targeting system if active
	bool UpdateTargetting( bool bDoFullUpdate );
	// returns the best target targets, activating the targeting system if it is disabled
	CPed* GetBestTarget( void );
	// Returns a pointer to the current target if targeting is active
	CPed* GetCurrentTarget( void );
	// Adds a specific threat into the targeting system
	void RegisterThreat(const CPed* pThreat, bool bTargetSeen = true );
	// Sets and gets this peds default target scoring function
	void  SetDefaultTargetScoringFunction( CPedTargetting::TargetScoringFunction* pScoringFn );
	CPedTargetting::TargetScoringFunction* GetDefaultTargetScoringFunction( void ) { return m_pDefaultScoringFn; }	
	///////////////////////////// END TARGETTING /////////////////////////////////

	// check if peds are in inclosed dispatch
	// search reason
	void UpdateEnclosedSearchRegions(bool bDoFullUpdate);

	// -----------------------------------------------------------------------------
	// COVER FINDER

	// Updates the peds targeting system if active
	void UpdateCoverFinder( bool bDoFullUpdate );

	// Returns a pointer to the peds targeting system
	CCoverFinder* GetCoverFinder( void );

	// Update the defensive areas based on the results of a cover search
	void UpdateDefensiveAreaFromCoverFinderResult( s32 iCoverSearchResult );

	///////////////////////////// END COVER FINDER /////////////////////////////////

	// -----------------------------------------------------------------------------
	// COMMUNICATION

	void SetInformRespectedFriends(const float f, const u8 N) { aiAssert(f <= MAX_INFORM_FRIEND_DISTANCE_MAX_VALUE); m_fMaxInformFriendDistance=f; m_uMaxNumFriendsToInform=N;}
	u8 GetMaxNumRespectedFriendsToInform() const {return m_uMaxNumFriendsToInform;}
	float GetMaxInformRespectedFriendDistance() const {return m_fMaxInformFriendDistance;}
	bool FindRespectedFriendInInformRange();

	// -----------------------------------------------------------------------------
	// CORE UPDATE FUNCTIONS

	void Process(bool fullUpdate, float fOverrideTimeStep = -1.0f);		// main intelligence process

	bool ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly);	// called for each physics update timeslice, so simple tasks can continuously apply forces, returns true if task wants to keep ped awake
	void ProcessPostPhysics();
	void ProcessPostMovement();	 // needs processed AFTER all mvoement (physics and then script)
	void ProcessPostCamera();	 // needs processed AFTER camManager::Update
	void ProcessPreRender2();		// processed after skeleton is updated but before Ik
	void ProcessPostPreRender();	// needs processed AFTER PreRender (so ped's matricies have been calculated)
	void ProcessPostPreRenderAfterAttachments();	// needs processed AFTER attachments

	// -----------------------------------------------------------------------------
	// QUERIABLE INTERFACE
	void BuildQueriableState();
	CQueriableInterface* GetQueriableInterface() {return &m_queriableInterface; }
	const CQueriableInterface* GetQueriableInterface() const {return &m_queriableInterface; }


	// -----------------------------------------------------------------------------
	// GENERIC AI INTERFACE

	// Alertness 
	typedef enum
	{
		AS_NotAlert = 0,
		AS_Alert,
		AS_VeryAlert,
		AS_MustGoToCombat
	} AlertnessState;

	void SetAlertnessState(AlertnessState alertState) { m_alertState = alertState; }
	AlertnessState GetAlertnessState() const { return m_alertState; }
	void IncreaseAlertnessState();
	void DecreaseAlertnessState();
	u32 GetLastReactedToDeadPedTime() const { return m_iLastReactedToDeadPedTime; }
	void SetLastReactedToDeadPedTime(const u32 iLastReactedToDeadPedTime ) { m_iLastReactedToDeadPedTime = iLastReactedToDeadPedTime; }
	u32 GetLastHeardGunFireTime() const { return m_iLastHeardGunFireTime; }
	void SetLastHeardGunFireTime(const u32 iLastHeardGunFireTime) { m_iLastHeardGunFireTime = iLastHeardGunFireTime; }
	u32 GetLastSeenGunFireTime() const { return m_iLastSeenGunFireTime; }
	void SetLastSeenGunFireTime(const u32 iLastSeenGunFireTime) { m_iLastSeenGunFireTime = iLastSeenGunFireTime; }
	u32 GetLastPinnedDownTime() const { return m_iLastPinnedDownTime; }
	void SetLastPinnedDownTime(const u32 iLastPinnedDownTime) { m_iLastPinnedDownTime = iLastPinnedDownTime; }
	u32 GetLastFiringVariationTime() const { return m_iLastFiringVariationTime; }
	void SetLastFiringVariationTime(const u32 iLastFiringVariationTime) { m_iLastFiringVariationTime = iLastFiringVariationTime; }
	void SetDoneCombatRoll() { m_iLastCombatRollTime = fwTimer::GetTimeInMilliseconds(); }
	bool GetCanCombatRoll() const;
	u32 GetLastCombatRollTime() const { return m_iLastCombatRollTime; }
	void SetLastGetUpTime() { m_iLastGetUpTime = fwTimer::GetTimeInMilliseconds(); }
	u32 GetLastGetUpTime() const { return m_iLastGetUpTime; }
	void SetDoneAimGrenadeThrow() { m_iLastAimGrenadeThrowTime = fwTimer::GetTimeInMilliseconds(); }
	bool GetCanDoAimGrenadeThrow() const;
	u32 GetLastAimGrenadeThrowTime() const { return m_iLastAimGrenadeThrowTime; }

	CDefensiveAreaManager* GetDefensiveAreaManager() { return &m_defensiveAreaManager; }
	CDefensiveArea* GetDefensiveArea() { return m_defensiveAreaManager.GetCurrentDefensiveArea(); }
	CDefensiveArea* GetPrimaryDefensiveArea() { return m_defensiveAreaManager.GetPrimaryDefensiveArea(); }
	CDefensiveArea* GetDefensiveAreaForCoverSearch() { return m_defensiveAreaManager.GetDefensiveAreaForCoverSearch(); }

	// Get and set for our bullet reaction timer
	float GetTimeUntilNextBulletReaction() { return m_fTimeTilNextBulletReaction; }
	void SetTimeUntilNextBulletReaction(float fTimeUntilBulletReaction) { m_fTimeTilNextBulletReaction = fTimeUntilBulletReaction; }

	// Gun aimed at functions
	float GetTimeSinceLastAimedAtByPlayer() const;
	void SetTimeLastAimedAtByPlayer(u32 uTime) { m_uLastAimedAtByPlayerTime = uTime; }

	u32 GetTimeLastRevvedAt() const {return m_uLastTimeRevvedAt;}

	//! Battle awareness. Used to put player ped into Action Mode.
	void IncreaseBattleAwareness(float fIncrement, bool bLocalPedTriggered = false, bool bForceRun = true);
	void DecreaseBattleAwareness(float fDecrement);
	void ResetBattleAwareness();
	bool IsBattleAware() const;
	bool IsBattleAwareForcingRun() const;
	void RecordPedSeenDeadPed();
	bool HasPedSeenDeadPedRecently(u32 uDiffMS = 5000) const;
	u32  GetTimeLastSeenDeadPed() const { return m_nLastTimeDeadPedSeen; }

	//! Return a value in R[0-1].
	float GetBattleAwareness() const;
	float GetBattleAwarenessSecs() const;

	//! Get Last battle event time.
	u32 GetLastBattleEventTime(bool bIncludeLocalEvents = false) const;

	bool CanEntityIncreaseBattleAwareness(CEntity *pEntity) const;

	// (REFACTOR)
	// increases, decreases, Sets and gets the amount that this ped feels pinned down
	// is affected by things like being shot and shots whizzing by or seeing
	// other peds shot and killed
	bool CanBePinnedDown() const;
	bool CanPinDownOthers() const;
	float GetAmountPinnedDown		( void ) const { return m_fPinnedDown; }
	void  IncreaseAmountPinnedDown	( float fIncrease );
	void  DecreaseAmountPinnedDown	( float fDecrease );	
	void  SetAmountPinnedDown		( float fPinnedDown, bool bForce = false );
	// Force the ped to be pinned down for the given time in seconds
	void  ForcePedPinnedDown		( const bool bPinned, float fTime );

	//Shocking Events - MGG
	void SetCheckShockFlag(bool bCheck);
	bool GetCheckShockFlag() const			{ return m_Flags.IsFlagSet(PIFlag_CheckShockingEvents); }

	void SetStartNewHangoutScenario(bool bOnOff);
	bool IsStartNewHangoutScenario() const	{ return m_Flags.IsFlagSet(PIFlag_StartNewHangoutScenario); }

	void SetTakenNiceCarPicture(bool bOnOff);
	bool HasTakenNiceCarPicture() const { return m_Flags.IsFlagSet(PIFlag_TakenNiceCarPicture); }

	bool ShouldIgnoreLowPriShockingEvents() const	{ return m_iIgnoreLowPriShockingEventsCount > 0; }
	void StartIgnoreLowPriShockingEvents()			{ Assert(m_iIgnoreLowPriShockingEventsCount < 0xff); m_iIgnoreLowPriShockingEventsCount++; }
	void EndIgnoreLowPriShockingEvents()			{ Assert(m_iIgnoreLowPriShockingEventsCount > 0); m_iIgnoreLowPriShockingEventsCount--; }
	
	u8		GetNumPedsFiringAt() const	{ return m_uNumPedsFiringAt; }
	void	DecreaseNumPedsFiringAt()	{ Assert(m_uNumPedsFiringAt > 0x00); --m_uNumPedsFiringAt; }
	void	IncreaseNumPedsFiringAt()	{ Assert(m_uNumPedsFiringAt < 0xff); ++m_uNumPedsFiringAt; }

	CVehicle *IsInACarOrEnteringOne();

	// PURPOSE:	Queries the Active task tree for prop management helpers
	CPropManagementHelper* GetActivePropManagementHelper();
	// PURPOSE: Recursive function to query the task hierarchy for the first prop management helper
	CPropManagementHelper* GetPropManagementHelper(CTask* pTask);

	// -----------------------------------------------------------------------------
	// EVENT SCANNERS

	void RecordEventForScript(const int iEventType, const int iEventPriority);
	u8 GetRecordedEventForScript() const {return m_iHighestPriorityEventType;}

	CEventScanner* GetEventScanner( ){return &m_eventScanner;}
	const CEventScanner* GetEventScanner() const {return &m_eventScanner;}

	inline u32 GetLastClimbTime(void) const { return m_iLastClimbTime; }
	inline void SetLastClimbTime(u32 t) { m_iLastClimbTime = t; }

	inline float GetClimbStrength() const { return m_fClimbStrength; }
	inline void SetClimbStrength(float fStrength) { m_fClimbStrength = fStrength; }
	inline void SetClimbStrengthRate(float fRate) { m_fClimbStrengthRate = fRate; }

	// This is called every time a climb is attempted when following a navmesh route
	void RecordAttemptedClimbOnRoute(const Vector3 & vClimbPosition);
	inline void GetLastAttemptedClimbOnRoutePosition(Vector3 & vClimbPosition) const { vClimbPosition = m_vLastAttemptedClimbPosition; }
	inline u32 GetLastAttemptedClimbOnRouteTime() const { return m_iLastAttemptedClimbTime; }

	// This is called if a climb has failed a number of times
	void RecordFailedClimbOnRoute(const Vector3 & vClimbPosition);

	s32 RecordStaticCountReachedMax(const Vector3& vPosition);

	void SetWillForceScanOnSwitchToHighPhysLod(bool b) { m_bWillForceScanOnSwitchToHighPhysLod = b; }
	bool GetWillForceScanOnSwitchToHighPhysLod() { return m_bWillForceScanOnSwitchToHighPhysLod; }

	void SetClosestPosOnRoute(const Vector3 & vPos, const Vector3 & vSegment)
	{
		m_bHasClosestPosOnRoute = true;
		m_vClosestPosOnRoute = vPos;
		m_vSegmentOnRoute = vSegment;
	}
	// Returns whether a path-following ped has a closest position on their route
	inline bool GetHasClosestPosOnRoute() const { return m_bHasClosestPosOnRoute; }
	// Returns the closest position on a ped's route
	inline const Vector3 & GetClosestPosOnRoute() const { Assert(m_bHasClosestPosOnRoute); return m_vClosestPosOnRoute; }
	// Returns the closest segment vector of the ped's current route (not normalised)
	inline const Vector3 & GetClosestSegmentOnRoute() const { Assert(m_bHasClosestPosOnRoute); return m_vSegmentOnRoute; }

	// Crossing the road records for recovery by tasks to handle interruptions and prevent doubling back
	void RecordBeganCrossingRoad(const Vector3& vStartPos, const Vector3& vEndPos);
	void RecordFinishedCrossingRoad();
	bool ShouldResumeCrossingRoad() const;
	inline const Vector3& GetCrossRoadStartPos() const { return m_vCrossRoadStartPos; }
	inline const Vector3& GetCrossRoadEndPos() const { return m_vCrossRoadEndPos; }
	inline const u32 GetLastFinishedCrossRoadTime() const { return m_uLastFinishedCrossRoadTimeMS; }

	// Record the last time this ped was standing on a non-isolated patch of navmesh
	inline void SetLastMainNavMeshPosition(const Vector3 & v) { m_vLastMainNavMeshPoly = v; }
	inline const Vector3 & GetLastMainNavMeshPosition() const { return m_vLastMainNavMeshPoly; }

	// If we contain any tasks which are navigating through the given region, then flag them to restart/recalculate
	void RestartNavigationThroughRegion(const Vector3 & vMin, const Vector3 & vMax);

	// Accessor for the combat behaviour struct - stores the definition of combat behaviour
	CCombatBehaviour& GetCombatBehaviour() { return m_combatBehaviour; }
	// Accessor for the flee struct - stores the definition of flee behaviour
	CFleeBehaviour&	  GetFleeBehaviour() { return m_fleeBehaviour; }
	const CFleeBehaviour&	GetFleeBehaviour() const { return m_fleeBehaviour; }

	//////////////////////////////////////////////////////////////////////////
	// Relationship and Acquaintance group
	// Change the relationship group this ped is associated with
	void				SetRelationshipGroupDefault(CRelationshipGroup* pRelationshipGroup);
	CRelationshipGroup* GetRelationshipGroupDefault() const { return m_pRelationshipGroupDefault; }
	void				SetRelationshipGroup(CRelationshipGroup* pRelationshipGroup);
	CRelationshipGroup* GetRelationshipGroup() const;
	int					GetRelationshipGroupIndex() const;
	void				SetDefaultRelationshipGroup();
	void				FixRelationshipGroup();

	const CFiringPattern& GetFiringPattern() const { return m_combatBehaviour.GetFiringPattern(); }
	CFiringPattern& GetFiringPattern() { return m_combatBehaviour.GetFiringPattern(); }
	// Ped stealth attributes
	const CPedStealth& GetPedStealth() const { return m_pedStealth; }
	CPedStealth& GetPedStealth() { return m_pedStealth; }
	// Ped perception class
	const CPedPerception& GetPedPerception() const { return m_pedPerception; }
	CPedPerception& GetPedPerception() { return m_pedPerception; }

	//Parse the given ped's model index and set the perception components accordingly.
	void SetPedPerception(const CPed* pPed);

	// B*2343564: Set cop ped perception values based on script-specified overrides (from SET_COP_PERCEPTION_OVERRIDES).
	void SetCopPedOverridenPerception(const CPed *pPed);

	// Get current order
	const COrder* GetOrder() const { return m_pOrder; }
	COrder* GetOrder() { return m_pOrder; }
	void	SetOrder(COrder* pOrder) { m_pOrder = pOrder; }

	// Camera system interfaces
	void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	// -----------------------------------------------------------------------------
	// PED MOTIVATION INTERFACE

	const CPedMotivation& GetPedMotivation() const { return m_pedMotivation; }
	CPedMotivation& GetPedMotivation() { return m_pedMotivation; }
	void SetPedMotivation(const CPed* pPed); 

	// ------------------------------------------------------------------------------
	// PED NAV CAPABILITIES INTERFACE

	const CPedNavCapabilityInfo& GetNavCapabilities() const {return m_navCapabilities;	}
	CPedNavCapabilityInfo& GetNavCapabilities() {return m_navCapabilities;	}
	void SetNavCapabilities(const CPed* pPed);

	// ------------------------------------------------------------------------------
	// NETWORK

	bool   NetworkMigrationAllowed(const netPlayer& player, eMigrationType migrationType) const;
    CTaskFSMClone *CreateCloneTaskForTaskType(u32 taskType) const;
    CTask* CreateCloneTaskFromInfo(CTaskInfo *taskInfo) const;
    void RecalculateCloneTasks();
	void UpdateTaskSpecificData();
	bool NetworkAttachmentIsOverridden();
	bool NetworkBlenderIsOverridden();
	bool NetworkHeadingBlenderIsOverridden();
    bool NetworkCollisionIsOverridden();
    bool NetworkLocalHitReactionsAllowed();
    bool NetworkUseInAirBlender();
	bool ControlPassingAllowed(const netPlayer& player, eMigrationType migrationType);

	void InstantTaskUpdate(bool bForceFullUpdate);

	bool CanForcefullyAllowControlPass() const;
	// ------------------------------------------------------------------------------
	// PERSONALITY OVERRIDE
	float GetDriverAbilityOverride() const {return m_fDriverAbilityOverride;}
	float GetDriverAggressivenessOverride() const {return m_fDriverAggressivenessOverride;}
	void SetDriverAbilityOverride(float fAbility) {m_fDriverAbilityOverride = fAbility;}
	void SetDriverAggressivenessOverride(float fAggressiveness) {m_fDriverAggressivenessOverride = fAggressiveness;}
	float GetDriverRacingModifier() const {return m_fDriverRacingModifier;}
	void SetDriverRacingModifier(float fRacingModifier) {m_fDriverRacingModifier = fRacingModifier;}
	
	// -----------------------------------------------------------------------------
	// HONK
	void HonkedAt(const CVehicle& rVehicle);

	// -----------------------------------------------------------------------------
	// REV
	void RevvedAt(const CVehicle& rVehicle);
	
	// -----------------------------------------------------------------------------
	// KNOCKED TO GROUND
	const CEntity* GetLastEntityThatKnockedUsToTheGround() const { return m_pLastEntityThatKnockedUsToTheGround; }
	u32 GetLastTimeWeWereKnockedToTheGround() const { return m_uLastTimeWeWereKnockedToTheGround; }
	void KnockedToGround(const CEntity* pEntity);

	// -----------------------------------------------------------------------------
	// RAMMED IN VEHICLE
	const CPed* GetLastPedThatRammedUsInVehicle() const { return m_pLastPedThatRammedUsInVehicle; }
	u32 GetLastTimeWeWereRammedInVehicle() const { return m_uLastTimeWeWereRammedInVehicle; }
	void RammedInVehicle(const CPed* pPed);

	// -----------------------------------------------------------------------------
	// TALKING
	struct TalkInfo
	{
		TalkInfo()
		: m_pPed(NULL)
		, m_uTime(0)
		{}

		TalkInfo(CPed* pPed)
		: m_pPed(pPed)
		, m_uTime(0)
		{}

		TalkInfo(CPed* pPed, const CAmbientAudio& rAudio)
		: m_pPed(pPed)
		, m_Audio(rAudio)
		, m_uTime(fwTimer::GetTimeInMilliseconds())
		{}

		RegdPed			m_pPed;
		CAmbientAudio	m_Audio;
		u32				m_uTime;
	};

	const TalkInfo&	GetPedTalkedToUsInfo()	const { return m_PedTalkedToUsInfo; }
	const TalkInfo&	GetTalkedToPedInfo()	const { return m_TalkedToPedInfo; }

	void SetPedTalkedToUsInfo(const TalkInfo& rInfo)	{ m_PedTalkedToUsInfo = rInfo; }
	void SetTalkedToPedInfo(const TalkInfo& rInfo)		{ m_TalkedToPedInfo = rInfo; }

	void ClearTalkResponse() { m_TalkResponse.Clear(); }
	const CAmbientAudio& GetTalkResponse() const { return m_TalkResponse; }
	void SetTalkResponse(const CAmbientAudio& rAudio) { m_TalkResponse = rAudio; }

	const CAmbientAudio* GetLastAudioSaidToPed(const CPed& rPed) const;
	bool HasRecentlyTalkedToPed(const CPed& rPed, float fMaxTime) const;
	bool HasRecentlyTalkedToPed(const CPed& rPed, float fMaxTime, const CAmbientAudio& rAudio) const;

	// -----------------------------------------------------------------------------
	// SHOT
	u32 GetLastTimeShot() const { return m_uLastTimeShot; }
	void SetLastTimeShot(u32 uTime) { m_uLastTimeShot = uTime; }

	// -----------------------------------------------------------------------------
	// POSITION
	u32 GetLastTimeOurPositionWasShouted() const { return m_uLastTimeOurPositionWasShouted; }
	void SetLastTimeOurPositionWasShouted() { m_uLastTimeOurPositionWasShouted = fwTimer::GetTimeInMilliseconds(); }

	// SCENARIO
	enum LastUsedScenarioFlags
	{
		LUSF_ExitedDueToAgitated	= BIT0,
	};
	CScenarioPoint* GetLastUsedScenarioPoint() const { return m_pLastUsedScenarioPoint; }
	int GetLastUsedScenarioPointType() const { return m_iLastUsedScenarioPointType; }
	fwFlags8& GetLastUsedScenarioFlags() { return m_uLastUsedScenarioFlags; }
	void SetLastUsedScenarioPoint(CScenarioPoint* pScenarioPoint) { m_pLastUsedScenarioPoint = pScenarioPoint; }
	void SetLastUsedScenarioPointType(int iType) { m_iLastUsedScenarioPointType = iType; }

	const CAnimBlackboard::AnimBlackboardItem GetLastBlackboardAnimPlayed() const { return m_lastBlackboardAnimPlayed; }
	void SetLastBlackboardAnimPlayed(const CAnimBlackboard::AnimBlackboardItem& item) { m_lastBlackboardAnimPlayed = item; }

	// Charge goal position override
	void SetChargeGoalPositionOverride(Vec3V_In vOverridePosition) { m_vChargeGoalPosOverride = vOverridePosition; }
	Vec3V_Out GetChargeGoalPositionOverride() const { return m_vChargeGoalPosOverride; }

	// BUMPED
	u32 GetLastTimeBumpedWhenStill() const { return m_uLastTimeBumpedWhenStill; }
	void Bumped(const CEntity& rEntity, float fEntitySpeedSq = -1.0f);

	// DODGED
	void Dodged(const CEntity& rEntity, float fEntitySpeedSq = -1.0f);

	// EVASION
	u32 GetLastTimeEvaded() const { return m_uLastTimeEvaded; }
	CEntity* GetLastEntityEvaded() const { return m_pLastEntityEvaded; }
	void Evaded(CEntity* pEntity);

	// COLLISION
	u32 GetLastTimeCollidedWithPlayer() const { return m_uLastTimeCollidedWithPlayer; }
	void CollidedWithPlayer();

	// AUDIO TIMERS
	u32 GetLastTimeTriedToSayAudioForDamage() const { return m_uLastTimeTriedToSayAudioForDamage; }
	void TriedToSayAudioForDamage();

	// CALLED POLICE
	u32 GetLastTimeCalledPolice() const { return m_uLastTimeCalledPolice; }
	void CalledPolice();

	// FRIENDLY
	const CPed* GetFriendlyException() const { return m_pFriendlyException; }
	void SetFriendlyException(const CPed* pPed) { m_pFriendlyException = pPed; }

			CExpensiveProcess& GetAudioDistributer()		{ return m_audioDistributer; }
	const	CExpensiveProcess& GetAudioDistributer() const	{ return m_audioDistributer; }

	// AMBIENT FRIEND PED INTERFACE
	CPed* GetAmbientFriend() const					{ return m_pAmbientFriend; }
	void SetAmbientFriend(CPed* pPed)				{ m_pAmbientFriend = pPed; m_bHadAmbientFriend = true; }
	bool HadAmbientFriend() const					{ return m_bHadAmbientFriend; }

	// PARACHUTE
	s32 GetTintIndexForParachute() const { return m_uTintIndexForParachute; }
	void SetTintIndexForParachute(s32 uIndex) { m_uTintIndexForParachute = uIndex; }
	s32 GetTintIndexForReserveParachute() const { return m_uTintIndexForReserveParachute; }
	void SetTintIndexForReserveParachute(s32 uIndex) { m_uTintIndexForReserveParachute = uIndex; }
	u32 GetTintIndexForParachutePack() const { return m_uTintIndexForParachutePack; }
	void SetTintIndexForParachutePack(u32 uIndex) { m_uTintIndexForParachutePack = uIndex; }

	u32 GetCurrentTintIndexForParachute() const;

	// VEHICLE
	u32 GetLastTimeLeftVehicle() const { return m_uLastTimeLeftVehicle; }
	void LeftVehicle() { m_uLastTimeLeftVehicle = fwTimer::GetTimeInMilliseconds(); }
	CVehicle* GetLastVehiclePedInside() const { return m_pLastVehiclePedInside; }
	void SetLastVehiclePedInside(CVehicle *pVehicle) { m_pLastVehiclePedInside = pVehicle; }
	void StartedEnterVehicle() { m_uLastTimeStartedToEnterVehicle = fwTimer::GetTimeInMilliseconds(); }
	u32 GetLastTimeStartedEnterVehicle() const { return m_uLastTimeStartedToEnterVehicle; }

	// COMBAT VICTORY
	const CPed* GetAchievedCombatVictoryOver() const { return m_pAchievedCombatVictoryOver; }
	void AchievedCombatVictoryOver(const CPed* pPed) { m_pAchievedCombatVictoryOver = pPed; }

	// DECISION MAKER 
	void UpdatePedDecisionMaker(bool bFullUpdate);

	// -----------------------------------------------------------------------------
	// DEBUG INTERFACE

#if ENABLE_NETWORK_LOGGING
	const char* GetLastMigrationFailTaskReason()
	{
		return m_lastMigrationFailTaskReason;
	}
#endif // ENABLE_NETWORK_LOGGING

#if __DEV
	void PrintTasks();
	void PrintTasksAndEvents();
	void PrintEvents() const;
	void PrintScriptTaskHistory() const;
#define MAX_DEBUG_SCRIPT_TASK_HISTORY 4
	void AddScriptHistoryString(const char* pStaticScriptTaskString, const char* pStaticTaskString, const char* scriptName, const s32 iProgramCounter);
	
	const char* m_aScriptHistoryName[MAX_DEBUG_SCRIPT_TASK_HISTORY];
	const char* m_aScriptHistoryTaskName[MAX_DEBUG_SCRIPT_TASK_HISTORY];
	u32 m_aScriptHistoryTime[MAX_DEBUG_SCRIPT_TASK_HISTORY];
	char m_szScriptName[MAX_DEBUG_SCRIPT_TASK_HISTORY][32];
	s32 m_aProgramCounter[MAX_DEBUG_SCRIPT_TASK_HISTORY];
	s32 m_iCurrentHistroyTop;
	bool m_bNewTaskThisFrame;
	bool m_bAssertIfRouteNotFound;	// assert if navigation task fails to find a route for this ped

	void Process_Debug();
#endif

	// -----------------------------------------------------------------------------
	// EVENT HISTORY

#if DEBUG_EVENT_HISTORY

	struct SEventHistoryEntry
	{
		struct SState
		{
			enum Enum
			{
				CREATED,
				DISCARDED_PRIORITY,
				DISCARDED_CANNOT_INTERRUPT_SAME_EVENT,
				TRYING_TO_ABORT_TASK,
				ACCEPTED,
				REMOVED,
				DELETED,
				EXPIRED,
				//...
				COUNT,
				INVALID = 0x7FFFFFFF
			};

			static const char* GetName(Enum state);
		};

		SEventHistoryEntry();
		SEventHistoryEntry(const CEvent& rEvent);

		SState::Enum GetState() const 
		{
			return (eState == CPedIntelligence::SEventHistoryEntry::SState::CREATED && (pEvent == NULL)) ? SState::DELETED : eState;
		}

		float GetDelayTimerMax() const { return fDelayTimerMax; }
		float GetDelayTimerLast() const { return pEvent ? pEvent->GetDelayTimer() : fDelayTimerLast; }

		eEventType eType;
		atString sEventDescription;
		atString sSourceDescription;
		Vec3V vPosition;
		u32 uCreatedTS;
		u32 uModifiedTS;
		u32 uProcessedTS;
		//TODO[egarcia]: We need to get the value without modifying the event.
		//bool bProcessedReady;
		bool bProcesedValidForPed;
		//TODO[egarcia]: We need to get the value without modifying the event.
		//bool bProcessedAffectsPed;
		bool bProcessedCalculatedResponse;
		bool bProcessedWillRespond;
		u32 uSelectedAsHighestTS;
		SState::Enum eState;
		float fDelayTimerMax;
		float fDelayTimerLast;
		CTaskTypes::eTaskType eBeforeAcceptedActiveTaskType[PED_TASK_TREE_MAX];
		CTaskTypes::eTaskType eAfterAcceptedActiveTaskType[PED_TASK_TREE_MAX];
		RegdConstEvent pEvent;
	};

	static const u32 uEVENT_HISTORY_MAX_NUM_EVENTS = 20;
	TCircularArray<SEventHistoryEntry, uEVENT_HISTORY_MAX_NUM_EVENTS> m_EventHistory;

	void AddToEventHistory(const CEvent& rEvent);
	
	void SetEventHistoryEntryState(const CEvent& rEvent, SEventHistoryEntry::SState::Enum eState);
	void SetEventHistoryEntryProcessed(const CEvent& rEvent);
	void SetEventHistoryEntrySelectedAsHighest(const CEvent& rEvent);

	void EventHistoryOnEventSetTaskBegin(const CEvent& rEvent);
	void EventHistoryOnEventSetTaskEnd(const CEvent& rEvent);
	void EventHistoryOnEventRemoved(const CEvent& rEvent);

	u32 GetEventHistoryCount() const { return m_EventHistory.GetCount(); }
	const SEventHistoryEntry& GetEventHistoryEntryAt(u32 uEntryIdx) { aiAssert(uEntryIdx < m_EventHistory.GetCount()); return m_EventHistory.Get(uEntryIdx); }
	CPedIntelligence::SEventHistoryEntry* FindEventHistoryEntry(const CEvent& rEvent);

	void EventHistoryMarkExpiredEvents();
	void EventHistoryMarkProcessedEvents();

#endif // DEBUG_EVENT_HISTORY

	enum PedIntelligenceFlag
	{
		PIFlag_CheckShockingEvents			= BIT0,
		PIFlag_StartNewHangoutScenario	= BIT1,
		PIFlag_TakenNiceCarPicture		  = BIT2,
	};

protected:
	void Process_UpdateVariables(float fTimeStep);
	void Process_Tasks(bool fullUpdate, float fTimeStep);
	void Process_UpdateVariablesAfterTaskProcess(float fTimeStep);
	void Process_NearbyEntityLists();
	bool ShouldForcePedScanThisFrame() const;
	void Process_FacialDataPreTasks();
	void Process_FacialDataPostTasks(float fTimeStep);
#if LAZY_RAGDOLL_BOUNDS_UPDATE
	void Process_BoundUpdateRequest();
#endif

	void UpdateCopPedVariables();

	CEventHandler* GetEventHandler() { return &m_eventHandler; }
	const CEventHandler* GetEventHandler() const { return &m_eventHandler; }

private:
#if ENABLE_NETWORK_LOGGING
	static const int TASK_MIGRATE_FAIL_SIZE = 256;
	mutable char m_lastMigrationFailTaskReason[TASK_MIGRATE_FAIL_SIZE];
#endif // ENABLE_NETWORK_LOGGING

	// The position of the last climb the ped attempted as part of a navmesh route.
	Vector3 m_vLastAttemptedClimbPosition;
	Vector3 m_vFailedClimb;
	Vector3 m_vStuckPosition;

	// Record the last known position at which the ped was on a non-isolated navmesh polygon (used for recovery code)
	Vector3 m_vLastMainNavMeshPoly;

	// Used by path-following code;
	// Used to be in CTaskGoToPoint but setting it from the navigation tasks was problematic when a new task was created during the task update
	Vector3 m_vClosestPosOnRoute;
	Vector3 m_vSegmentOnRoute;

	// Used by road crossing task
	// (i) in case the task tree is interrupted, to resume moving to end position
	// (ii) to prevent crossing back over to where we just came from
	Vector3 m_vCrossRoadStartPos;
	Vector3 m_vCrossRoadEndPos;

	CDefensiveAreaManager m_defensiveAreaManager;

	CEventScanner m_eventScanner;

	TalkInfo		m_PedTalkedToUsInfo;
	TalkInfo		m_TalkedToPedInfo;
	CAmbientAudio	m_TalkResponse;

	// Defines how stealthly the ped is currently being - i.e. how hard they are to spot.
	CPedStealth m_pedStealth;
	CPed* m_pPed;

	// PURPOSE:	Time passed since the last full AI update, in seconds. Used to compute
	//			the proper update time step during AI timeslicing.
	float m_fTimeSinceLastAiUpdate;

	CPedTaskManager m_taskManager;
	CEventGroupPed m_eventGroup;
	CEventHandler m_eventHandler;
	CAnimBlackboard::AnimBlackboardItem m_lastBlackboardAnimPlayed;

	CPedEventDecisionMaker m_PedDecisionMaker;
	
	u32 m_iLastReactedToDeadPedTime;
	u32 m_iLastHeardGunFireTime;
	u32 m_iLastSeenGunFireTime;
	u32 m_iLastPinnedDownTime;
	u32 m_iLastFiringVariationTime;
	u32 m_iLastCombatRollTime;
	u32 m_iLastGetUpTime;
	u32 m_iLastAimGrenadeThrowTime;
	
	u32 m_uLastTimeHonkedAt;
	u32 m_uLastTimeHonkAgitatedUs;
	u32 m_uLastTimeRevvedAt;
	u32 m_uLastTimeRevAgitatedUs;
	
	u32 m_uLastTimeWeWereKnockedToTheGround;
	u32 m_uLastTimeWeWereRammedInVehicle;

	u32 m_uLastTimeShot;
	u32 m_uLastTimeOurPositionWasShouted;

	u32 m_uLastTimeBumpedWhenStill;
	u32 m_uLastTimeEvaded;
	u32 m_uLastTimeCollidedWithPlayer;

	u32 m_uLastTimeTriedToSayAudioForDamage;
	u32 m_uLastTimeCalledPolice;
	u32 m_uLastTimeLeftVehicle;
	u32 m_uLastTimeStartedToEnterVehicle;

	s32	m_uTintIndexForParachute;
	s32 m_uTintIndexForReserveParachute;
	u32	m_uTintIndexForParachutePack;

	AlertnessState m_alertState;

	// NOTE[egarcia]: m_uMaxNumFriendsToInform type must be changed together with MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE (they are used in CPedScriptGameStateDataNode::Serialise) 
	u8 m_uMaxNumFriendsToInform;
	float m_fMaxInformFriendDistance; 

	// time until the next time we can check for a cop ped update for this ped
	float m_fTimeUntilNextCopVariableUpdate;

	// Scanners
	CVehicleScanner m_vehicleScanner;    
	CPedScanner m_pedScanner;    
	CObjectScanner m_objectScanner;     

	// When were we last aimed at by the player?
	u32	m_uLastAimedAtByPlayerTime;

	CClimbDetector m_climbDetector;
	CDropDownDetector m_dropDownDetector;
	CDoorScanner m_doorScanner;
	CPedNavCapabilityInfo m_navCapabilities;

	CExpensiveProcess m_audioDistributer;

	// a pointer to this peds targeting system, may be null
	CPedTargetting*	m_pPedTargetting;
	// A pointer to this peds default target scoring fn
	CPedTargetting::TargetScoringFunction*	m_pDefaultScoringFn;

	// This is our cover finder for this ped, it will be acquired and released as needed
	CCoverFinder* m_pCoverFinder;

	RegdScenarioPnt m_pLastUsedScenarioPoint;	
	fwFlags8 m_uLastUsedScenarioFlags;

	// Ped alertness
	u8 m_iPedAlertness;

	u8 m_iHighestPriorityEventType;
	u8 m_iHighestPriorityEventPriority;

	static const u32 MAX_CONTROL_PASS_FAIL_TIME = 5000;
	static const u32 RESET_CONTROL_PASS_FAIL_TIME = 10000;
	mutable u32 m_ControlPassingFailTime;

	// The last time at which the ped was climbing or on a ladder.  This is used to disable IK until the ped is safely placed on ground.
	u32 m_iLastClimbTime;
	// The remaining strength of the ped - determines how long they can hang on for
	float m_fClimbStrength;
	// The rate at which the ped gains/looses climb strength
	float m_fClimbStrengthRate;

	float m_fDriverAbilityOverride;
	float m_fDriverAggressivenessOverride;
	float m_fDriverRacingModifier;
	u32 m_uLastFinishedCrossRoadTimeMS;
	int m_iLastUsedScenarioPointType;
	fwFlags8		m_Flags;

	bool	m_bForcePinned : 1; // Specifies whether the timer m_fForcedPinnedTimer should set the pinned to true or false
	bool m_bHasClosestPosOnRoute : 1;
	bool m_bWillForceScanOnSwitchToHighPhysLod : 1;
	bool m_bBattleAwarenessForcingRun : 1;
	bool m_bHadAmbientFriend : 1;

	// Used by scripts to override the charge goal position
	Vec3V m_vChargeGoalPosOverride;

	u32 m_iLastAttemptedClimbTime;
	s32 m_iNumTimesClimbFailed;
	s32 m_iNumTimesStuck;
	u32 m_iFirstStuckTime;

	u32 m_iEnclosedSearchVolumeCheckIndex;
	// time until we can do a bullet reaction next
	float m_fTimeTilNextBulletReaction;

	float m_fBattleAwareness;

	u32	  m_nLastBattleEventTime;
	u32	  m_nLastBattleEventTimeLocalPlayer;
	u32	  m_nLastTimeDeadPedSeen;

	float			m_fForcedPinnedTimer;	//! When > 0 the ped is forced to be pinned whilst it counts down
	float			m_fPinnedDown;			// the extent to which this ped is pinned down.	

	u8				m_uNumPedsFiringAt;

	// PURPOSE:	Keep track of how many users want us to ignore low priority shocking events.
	//			Each user (running task, etc) that calls StartIgnoreLowPriShockingEvents()
	//			is obligated to match with a call to EndIgnoreLowPriShockingEvents().
	u8				m_iIgnoreLowPriShockingEventsCount;

	// PURPOSE:	This should be the same as m_pRelationshipGroup->GetIndex(), stored off separately to reduce cache misses when accessed by another ped.
	u16				m_uRelationshipGroupIndex;

	CQueriableInterface m_queriableInterface;

	CCombatBehaviour m_combatBehaviour;
	CFleeBehaviour	 m_fleeBehaviour;

	// Relationship groups
	RegdRelationshipGroup	m_pRelationshipGroupDefault; // Default, used to initialize actual and accessible to scripts
	RegdRelationshipGroup	m_pRelationshipGroup; // Actual, used by the ped for all relationship checks

	// Perception class - main interface to checking visibility
	CPedPerception m_pedPerception;

	// Motivation class - main interface for checking what is motivating pedss
	CPedMotivation m_pedMotivation; 

	// Current order
	RegdOrder		m_pOrder;
	
	RegdCombatDirector m_pCombatDirector;
	
	RegdConstEnt m_pLastEntityThatKnockedUsToTheGround;

	RegdConstPed m_pLastPedThatRammedUsInVehicle;

	RegdEnt m_pLastEntityEvaded;

	RegdConstPed m_pFriendlyException;

	RegdConstPed m_pAchievedCombatVictoryOver;

	// PURPOSE: Used to link peds created by clusters together.
	// i.e. a dog might need to know about its owner.
	RegdPed m_pAmbientFriend;

	// B*1925487: Stores vehicle was last inside. Used by CTaskSmartFlee so we don't re-enter the same vehicle.
	RegdVeh			m_pLastVehiclePedInside;
	
public:

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif // __BANK

	//////////////////////
	// Statics go at the bottom please

	static bool ms_bPedsRespondToObjectCollisionEvents;
	static const u32 ms_iMinFreqToAcceptBuildingCollisionEvents;

	// It is handy to be able to turn this off when debugging
	static bool ms_bAllowTeleportingWhenRoutesTimeOut;

	// Battle Awareness attributes for player.
	static bank_float BATTLE_AWARENESS_BULLET_WHIZBY;
	static bank_float BATTLE_AWARENESS_BULLET_IMPACT;
	static bank_float BATTLE_AWARENESS_GUNSHOT;
	static bank_float BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER;
	static bank_float BATTLE_AWARENESS_DAMAGE;	
	static bank_float BATTLE_AWARENESS_MELEE_FIGHT;	
	static bank_float BATTLE_AWARENESS_SHOVED;	
	static bank_float BATTLE_AWARENESS_MAX;
	static bank_float BATTLE_AWARENESS_MIN;
	static bank_float BATTLE_AWARENESS_DEPLETION_TIME_FROM_MAX;
	static bank_u32 BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER_COOLDOWN;
	static bank_float BATTLE_AWARENESS_MIN_TIME;

	// Alertness threshold
	const static u8 ALERTNESS_RESPONSE_THRESHOLD;
	// The minimum time before we accept a new ms_iLastHeardGunFireTolerance
	const static u32 ms_iLastHeardGunFireTolerance;
	const static u32 ms_iLastPinnedDownTolerance;
	// Gunfire Alertness Modifiers, alertness increased in response to particular events
// 	const static u8 ms_iIncreaseDueToShotWizzed;
// 	const static u8 ms_iIncreaseDueToBulletImpact;
// 	const static u8 ms_iIncreaseDueToGunShot;

	const static u16 ms_MinFramesPerRagdollBoundUpdate;

	// The default climb strength regeneration rate
	static dev_float DEFAULT_CLIMB_STRENGTH_RATE;

	static dev_float ms_fFrequentScanVehicleVelocitySquaredThreshold;

#if !__FINAL
	static bool ms_bWarnAboutRouteTaskBuildingCollisions;
#endif

	static const u8  MAX_NUM_FRIENDS_TO_INFORM;
	// NOTE[egarcia]: These two values must be changed together with m_uMaxNumFriendsToInform type (they are used in CPedScriptGameStateDataNode::Serialise) 
	static const u8  MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE;
	static const int MAX_NUM_FRIENDS_TO_INFORM_NUM_BITS; 

	static const float MAX_INFORM_FRIEND_DISTANCE;
	// NOTE[egarcia]: These two values are used to serialise m_fMaxInformFriendDistance (in CPedScriptGameStateDataNode::Serialise)
	// and must be changed together
	static const float MAX_INFORM_FRIEND_DISTANCE_MAX_VALUE; 
	static const int   MAX_INFORM_FRIEND_DISTANCE_NUM_BITS; 
	//

private:

	// Prevent accidental and unwanted copying.
	CPedIntelligence( const CPedIntelligence &rhs);

};


//
// Inline functions
//

//
// interface functions to CTaskManager
//

inline void CPedIntelligence::AddTaskAtPriority(aiTask* pTask, s32 iPriority, const bool bForce)
{
	m_taskManager.SetTask( PED_TASK_TREE_PRIMARY, pTask, iPriority, bForce );
}


inline void CPedIntelligence::AddTaskDefault(aiTask* pTask, const bool bForce)
{
	taskAssertf(pTask, "Invalid to set a NULL default ped task");
	m_taskManager.SetTask( PED_TASK_TREE_PRIMARY, pTask, PED_TASK_PRIORITY_DEFAULT, bForce );
}

inline void CPedIntelligence::AddTaskSecondary(aiTask* pTask, const int iType)
{
	m_taskManager.SetTask( PED_TASK_TREE_SECONDARY, pTask, iType );
}

inline void CPedIntelligence::AddTaskMovement(aiTask* pTask)
{
	m_taskManager.SetTask( PED_TASK_TREE_MOVEMENT, pTask, PED_TASK_MOVEMENT_GENERAL );
}

inline void CPedIntelligence::AddTaskMovementResponse(aiTask* pTask)
{
	m_taskManager.SetTask( PED_TASK_TREE_MOVEMENT, pTask, PED_TASK_MOVEMENT_EVENT_RESPONSE );
}

inline void CPedIntelligence::AddLocalCloneTask(CTaskFSMClone* pCloneTask, u32 const iPriority /* = PED_TASK_PRIORITY_DEFAULT */)
{
	CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));

	if (cloneTaskTree && pCloneTask)
	{
		cloneTaskTree->StartLocalCloneTask(pCloneTask, iPriority);
	}
}

inline bool CPedIntelligence::HasPendingLocalCloneTask() const
{
	CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));

	if (cloneTaskTree)
	{
		return cloneTaskTree->HasPendingLocalCloneTask();
	}

	return false;
}

inline CTask* CPedIntelligence::GetActiveCloneTask()
{
	CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));

	if (cloneTaskTree)
		return cloneTaskTree->GetActiveCloneTask();
	
	return NULL;
}

inline CTask* CPedIntelligence::GetTaskAtPriority(s32 iPriority) const
{
	aiTask* pTask=m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, iPriority );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetMovementTaskAtPriority(s32 iPriority) const
{
	aiTask* pTask=m_taskManager.GetTask( PED_TASK_TREE_MOVEMENT, iPriority );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence:: GetTaskEventResponse() const
{
	aiTask* pTask=m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_PHYSICAL_RESPONSE );
	if(0==pTask)
	{
		pTask=m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP );
	}
	if(0==pTask)
	{
		pTask=m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP );
	}
	/*
	CTask* p=m_taskManager.GetTask(TASK_PRIORITY_EVENT_RESPONSE_TEMP);
	if(0==p)
	{
	p=m_taskManager.GetTask(TASK_PRIORITY_EVENT_RESPONSE_NONTEMP);
	}
	*/
	CTask *p = static_cast<CTask*>(pTask);
	return p;
}

inline void CPedIntelligence::GetTasksEventRespone(	CTask*& pOutTaskEventResponse, 
													CTask*& pOutTempTaskEventResponse, 
													CTask*& pOutNonTempTaskEventResponse, 
													CTask*& pOutMovementTaskEventResponse) const
{	
	aiTask* pEventPhysicalResponse = m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_PHYSICAL_RESPONSE );
	aiTask* pEventResponseTask = pEventPhysicalResponse;
	
	aiTask* pTempTaskEventResponse = m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP );
	pOutTempTaskEventResponse = static_cast<CTask*>(pTempTaskEventResponse);

	if(0==pEventResponseTask)
	{
		pEventResponseTask = pTempTaskEventResponse;
	}

	aiTask* pNonTempTaskEventResponse = m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP );
	if(0==pEventResponseTask)
	{
		pEventResponseTask = pNonTempTaskEventResponse;
	}

	// If no non-temp event response task and a valid physical response task is present, 
	// set to the physical response as it is still reacting to the same event
	if(0==pNonTempTaskEventResponse)
	{
		pNonTempTaskEventResponse = pEventPhysicalResponse;
	}
	pOutNonTempTaskEventResponse = static_cast<CTask*>(pNonTempTaskEventResponse);
	
	pOutTaskEventResponse = static_cast<CTask*>(pEventResponseTask);

	aiTask* pMovementEventTask = m_taskManager.GetTask( PED_TASK_TREE_MOVEMENT, PED_TASK_MOVEMENT_EVENT_RESPONSE );
	pOutMovementTaskEventResponse =  static_cast<CTask*>(pMovementEventTask);
}


inline CTask* CPedIntelligence::GetTaskDefault() const
{
	aiTask *pTask = m_taskManager.GetTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_DEFAULT );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetGeneralMovementTask() const
{
	aiTask *pTask = m_taskManager.GetTask( PED_TASK_TREE_MOVEMENT, PED_TASK_MOVEMENT_GENERAL );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetMovementResponseTask() const
{
	aiTask *pTask = m_taskManager.GetTask( PED_TASK_TREE_MOVEMENT, PED_TASK_MOVEMENT_EVENT_RESPONSE );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetDefaultMovementTask() const
{
	aiTask *pTask = m_taskManager.GetTask( PED_TASK_TREE_MOVEMENT, PED_TASK_MOVEMENT_DEFAULT );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetTaskSecondaryPartialAnim() const
{
	aiTask *pTask = m_taskManager.GetTask(PED_TASK_TREE_SECONDARY, PED_TASK_SECONDARY_PARTIAL_ANIM );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetTaskSecondary(const int iType) const
{
	aiTask *pTask = m_taskManager.GetTask( PED_TASK_TREE_SECONDARY, iType );
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetTaskSecondaryActive() const
{
	return static_cast<CTask*>(m_taskManager.GetActiveTask(PED_TASK_TREE_SECONDARY));
}

inline s32  CPedIntelligence::GetActiveTaskPriority( void ) const
{
	return m_taskManager.GetActiveTaskPriority(PED_TASK_TREE_PRIMARY);
}

inline CTask* CPedIntelligence::GetTaskActive() const
{
	aiTask *pTask = m_taskManager.GetActiveTask(PED_TASK_TREE_PRIMARY);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::GetTaskActiveSimplest() const
{
	aiTask *pTask = m_taskManager.GetActiveLeafTask(PED_TASK_TREE_PRIMARY);
	return static_cast<CTask*>(pTask);
}     

inline CTask* CPedIntelligence::GetMotionTaskActiveSimplest() const
{
	aiTask *pTask = m_taskManager.GetActiveLeafTask(PED_TASK_TREE_MOTION);
	return static_cast<CTask*>(pTask);
}     

inline CTaskMove* CPedIntelligence::GetActiveSimplestMovementTask() const
{
	Assert( !m_taskManager.GetActiveLeafTask(PED_TASK_TREE_MOVEMENT) || ((CTask*)m_taskManager.GetActiveLeafTask(PED_TASK_TREE_MOVEMENT))->IsMoveTask() );
	aiTask * pSimplestMoveTask = m_taskManager.GetActiveLeafTask(PED_TASK_TREE_MOVEMENT);

	if(pSimplestMoveTask)
	{
		return (CTaskMove*)pSimplestMoveTask;
	}
	else
	{
		return NULL;
	}
}  

inline CTask* CPedIntelligence::GetActiveMovementTask() const
{
	Assert( !m_taskManager.GetActiveTask(PED_TASK_TREE_MOVEMENT) || ((CTask*)m_taskManager.GetActiveTask(PED_TASK_TREE_MOVEMENT))->IsMoveTask() );
	aiTask *pTask = m_taskManager.GetActiveTask(PED_TASK_TREE_MOVEMENT);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskActiveByTreeAndType(const int iTree, const int iType) const
{
	aiTask *pTask = m_taskManager.FindTaskByTypeActive(iTree, iType);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskActiveByType(const int iType) const
{
    aiTask *pTask = m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, iType);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskActiveMovementByType(const int iType) const
{
	aiTask *pTask = m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_MOVEMENT, iType);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskActiveMotionByType(const int iType) const
{
	aiTask *pTask = m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_MOTION, iType);

	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskEventResponseByType(const int iType) const
{
	aiTask *pTask = m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, iType, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
	if(0==pTask)
	{
		pTask=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, iType, PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP);
	}

	CTask *p = static_cast<CTask*>(pTask);

	return p;
}

inline CTask* CPedIntelligence::FindTaskPhysicalResponseByType(const int iType) const
{
	aiTask* pTask=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, iType, PED_TASK_PRIORITY_PHYSICAL_RESPONSE);
	if(0==pTask)
	{
		pTask=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, iType, PED_TASK_PRIORITY_PHYSICAL_RESPONSE);
	}

	CTask *p = static_cast<CTask*>(pTask);

	return p;
}

inline CTask* CPedIntelligence::FindTaskPrimaryByType(const int iType) const
{
	aiTask* pTask=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, iType, PED_TASK_PRIORITY_PRIMARY);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskSecondaryByType(const int iType) const
{
	aiTask* pTask=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_SECONDARY, iType, PED_TASK_SECONDARY_PARTIAL_ANIM);
	return static_cast<CTask*>(pTask);
}

inline CTask* CPedIntelligence::FindTaskDefaultByType(const int iType) const
{
	aiTask* pTask=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, iType, PED_TASK_PRIORITY_DEFAULT);
	return static_cast<CTask*>(pTask);
}

inline bool CPedIntelligence::HasTaskSecondary(const aiTask* pTask) const
{
	return m_taskManager.GetHasTask(PED_TASK_TREE_SECONDARY, pTask);
}

inline void CPedIntelligence::ClearTaskEventResponse()
{
	//clear the temp response task, if there isn't one clear the non temp response
	if(m_taskManager.ClearTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP ) == false)
		m_taskManager.ClearTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP );
}

inline void CPedIntelligence::ClearTaskTempEventResponse()
{
	m_taskManager.ClearTask( PED_TASK_TREE_PRIMARY, PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP );
}

//
// interface functions to CEventGroup
//

inline CEvent* CPedIntelligence::AddEvent(const CEvent& rEvent, const bool bForcePersistence, const bool bRemoveOtherEventsToMakeSpace)
{
	AI_EVENT_GROUP_LOCK(&m_eventGroup);

	CEvent* pNewEvent = NULL;

	if (!(m_eventGroup.IsFull() && !bRemoveOtherEventsToMakeSpace))
	{
		pNewEvent = static_cast<CEvent*>(m_eventGroup.Add(rEvent));

		if (pNewEvent)
		{
			pNewEvent->ForcePersistence(bForcePersistence);
#if DEBUG_EVENT_HISTORY
			AddToEventHistory(*pNewEvent);
#endif // DEBUG_EVENT_HISTORY
		}
	}

	return pNewEvent;
}

inline void CPedIntelligence::RemoveEvent(CEvent* pEvent)
{
	if (AssertVerify(pEvent))
	{
		m_eventGroup.Remove(*pEvent);

#if DEBUG_EVENT_HISTORY
		EventHistoryOnEventRemoved(*pEvent);
#endif // DEBUG_EVENT_HISTORY
	}
}

inline void CPedIntelligence::FlagEventForDeletion( CEvent* pEvent )
{
	if (AssertVerify(pEvent))
	{
		pEvent->SetFlagForDeletion(true);
	}
}

inline bool CPedIntelligence::HasEventOfType(const CEvent* pEvent) const 
{
	if (AssertVerify(pEvent))
	{
		return m_eventGroup.HasEventOfType(*pEvent);
	}

	return false;
}

inline bool CPedIntelligence::HasEventOfType(const int iEventType) const
{
	return m_eventGroup.GetEventOfType(iEventType) != NULL;
}


inline bool CPedIntelligence::HasDuplicateEvent(const CEvent* pEvent) const 
{
	if (AssertVerify(pEvent))
	{
		return m_eventGroup.HasDuplicateEvent(*pEvent);
	}

	return false;
}

inline CEvent* CPedIntelligence::GetEventByIndex(const int iEventIndex) const 
{
	return static_cast<CEvent*>(m_eventGroup.GetEventByIndex(iEventIndex));
}

inline int CPedIntelligence::CountEventGroupEvents() const 
{
	return m_eventGroup.GetNumEvents();
}

inline CEvent* CPedIntelligence::GetEventOfType(const int iEventType) const 
{
	return static_cast<CEvent*>(m_eventGroup.GetEventOfType(iEventType));
}

inline void CPedIntelligence::RemoveEventsOfType(const int iEventType) 
{
	return m_eventGroup.FlushEventsOfType(iEventType);
}

inline void CPedIntelligence::RemoveExpiredEvents() 
{
#if DEBUG_EVENT_HISTORY
	EventHistoryMarkExpiredEvents();
#endif // DEBUG_EVENT_HISTORY

	return m_eventGroup.FlushExpiredEvents();
}

inline bool CPedIntelligence::HasEvent(const CEvent* pEvent) const
{
	if (AssertVerify(pEvent))
	{
		return m_eventGroup.HasEvent(*pEvent);
	}

	return false;
}

inline bool CPedIntelligence::HasRagdollEvent() const
{
	return m_eventGroup.HasRagdollEvent();
}

inline bool CPedIntelligence::HasPairedDamageEvent() const
{
	return m_eventGroup.HasPairedDamageEvent();
}

inline void CPedIntelligence::TickEvents()
{
	m_eventGroup.TickEvents();
}

inline void CPedIntelligence::UnTickEvents()
{
	m_eventGroup.UnTickEvents();
}

inline void CPedIntelligence::RemoveInvalidEvents(bool bEverythingButScriptEvents, bool bFlagForDeletion)
{
	m_eventGroup.RemoveInvalidEvents(bEverythingButScriptEvents, bFlagForDeletion);
}

inline void CPedIntelligence::UpdateCommunicationEvents(CPed* pPed)
{
	m_eventGroup.UpdateCommunicationEvents(pPed);
}

inline CEvent* CPedIntelligence:: GetHighestPriorityEvent() const
{
	return m_eventGroup.GetHighestPriorityEvent();
}

//-------------------------------------------------------------------------
// Returns a pointer to the peds targeting system, waking up if
// inactive if bWakeUpIfInactive is true
//-------------------------------------------------------------------------
inline CPedTargetting* CPedIntelligence::GetTargetting( const bool bWakeUpIfInactive )
{
	if( !m_pPedTargetting && bWakeUpIfInactive )
	{
		m_pPedTargetting = rage_checked_pool_new(CPedTargetting) (m_pPed);
#if __BANK
		if(!m_pPedTargetting)
		{
			dev_float fTargetingActiveTime = 2.0f;
			int iNumOldTargeting = 0;
			CPedTargetting::Pool* pTargetingPool = CPedTargetting::GetPool();

			for(int i = 0; i < pTargetingPool->GetSize(); i++)
			{
				CPedTargetting* pPedTargeting = pTargetingPool->GetSlot(i);
				if( aiVerifyf(pPedTargeting, "Our pool ran out so all the slots should be used, what went wrong?") &&
					(CPedTargetting::GetTunables().m_fTargetingInactiveDisableTime - pPedTargeting->GetActiveTimer()) > fTargetingActiveTime)
				{
					iNumOldTargeting++;
				}
			}

			aiAssertf(0, "Could not create a new targeting, we had %d active targeting systems more than %.02f seconds old.", iNumOldTargeting, fTargetingActiveTime);
		}
#endif
	}
	return m_pPedTargetting;
}

inline bool CPedIntelligence::NetworkAttachmentIsOverridden()
{
	if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Checking if the network attachment is overridden on a local ped!"))
	{
		CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
		if(cloneTaskTree)
		{
			return cloneTaskTree->NetworkAttachmentIsOverridden();
		}
	}

	return false;
}

inline bool CPedIntelligence::NetworkBlenderIsOverridden()
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Checking if the network blender is overridden on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            return cloneTaskTree->NetworkBlenderIsOverridden();
        }
    }

    return false;
}

inline bool CPedIntelligence::NetworkHeadingBlenderIsOverridden()
{
	if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Checking if the network heading blender is overridden on a local ped!"))
	{
		CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
		if(cloneTaskTree)
		{
			return cloneTaskTree->NetworkHeadingBlenderIsOverridden();
		}
	}

	return false;
}

inline bool CPedIntelligence::NetworkCollisionIsOverridden()
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Checking if the network collision is overridden on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            return cloneTaskTree->NetworkCollisionIsOverridden();
        }
    }

    return false;
}

inline bool CPedIntelligence::NetworkLocalHitReactionsAllowed()
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Checking if the network local hit reactions allowed on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            return cloneTaskTree->NetworkLocalHitReactionsAllowed();
        }
    }

    return false;
}

inline bool CPedIntelligence::NetworkUseInAirBlender()
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Checking whether to use in-air blending on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            return cloneTaskTree->NetworkUseInAirBlender();
        }
    }

    return false;
}

//This class is used to shutdown all the singletons used by the ai.
class CAi
{
public:

	CAi(){}
	~CAi(){}

	static void Init();
	static void Shutdown();
private:
};

#endif





