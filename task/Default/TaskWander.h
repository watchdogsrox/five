
//
// task/taskwander.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef TASK_WANDER_H
#define TASK_WANDER_H

// Game headers
#include "AI/ambient/ConditionalAnimManager.h"
#include "ModelInfo/PedModelInfo.h"
#include "Peds/PedDefines.h"
#include "Peds/PedMotionData.h"
#include "Peds/QueriableInterface.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Helpers/PavementFloodFillHelper.h"
#include "Task/Helpers/PropManagementHelper.h"
#include "Task/System/Task.h"
#include "Task/Scenario/ScenarioManager.h"

#define FIND_SHELTER 0

////////////////////////////////////////////////////////////////////////////////

enum eWanderScriptFlags
{
	EWDR_KEEP_MOVING_WHILST_WAITING_FOR_PATH					= BIT0
};

////////////////////////////////////////////////////////////////////////////////

class CTaskWander : public CTask
{
public:
	static void UpdateClass();

	// Constants
	static dev_float        TARGET_RADIUS;
	static dev_float		ACCEPT_ALL_DOT_MIN;
	static dev_float		DEFAULT_ROAD_CROSS_PED_TO_START_DOT_MIN;
	static dev_float		DEFAULT_DIST_SQ_PED_TO_START_MAX;
	static dev_float		INCREASED_ROAD_CROSS_PED_TO_START_DOT_MIN;
	static dev_float		INCREASED_DIST_SQ_PED_TO_START_MAX;
	static dev_float		DOUBLEBACK_ROAD_CROSS_PED_TO_START_DOT_MIN;
	static dev_float		DOUBLEBACK_DIST_SQ_PED_TO_START_MAX;
	static dev_float		CHAIN_ROAD_CROSS_PED_TO_START_DOT_MIN;
	static dev_float		CHAIN_ROAD_CROSSDIR_TO_CROSSDIR_DOT_MIN;
	static dev_float		CHAIN_DIST_SQ_PED_TO_START_MAX;
	static dev_float		POST_CROSSING_REPEAT_CHANCE;
	static dev_float		IDLE_AT_CROSSWALK_CHANCE;
	static bank_bool		ms_bPreventCrossingRoads;
	static bank_bool		ms_bPromoteCrossingRoads;
	static bank_bool		ms_bPreventJayWalking;
	static bank_bool		ms_bEnableChainCrossing;
	static bank_bool		ms_bEnablePavementArrivalCheck;
	static bank_bool		ms_bEnableWanderFailCrossing;
	static bank_bool		ms_bEnableArriveNoPavementCrossing;

	struct Tunables : public CTuning
	{
		Tunables();

		u32 m_uNumPedsToTransitionToRainPerPeriod;	// How many peds can transition to their rain wanders per m_fSecondsInRainTransitionPeriod
		float m_fSecondsInRainTransitionPeriod;

		PAR_PARSABLE;
	};

	// State
	enum State
	{
		State_Init = 0,
		State_ExitVehicle,
		State_Wander,
		State_WanderToStoredPosition,
		State_CrossRoadAtLights,
		State_CrossRoadJayWalking,
		State_MoveToShelterAndWait,
		State_Crime,
		State_HolsterWeapon,
		State_BlockedByVehicle,
		State_Aborted
	};

	// Construction
	CTaskWander(const float fMoveBlendRatio, const float fDir, const CConditionalAnimsGroup * pOverrideClips = NULL, const s32 pOverrideAnimIndex = -1, const bool bWanderSensibly = true, const float fTargetRadius = TARGET_RADIUS, const bool bStayOnPavements = true, bool bScanForPavements = false);

	// Destruction
	virtual ~CTaskWander();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_WANDER; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const;

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// PURPOSE: Set the wander direction
	void SetDirection(const float fHeading);

	// PURPOSE: Override the move blend ratio to use for wandering
	void SetMoveBlendRatio(const float fMbr) { m_fMoveBlendRatio = Clamp(fMbr, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT); }

	// PURPOSE: Inform task to keep moving while waiting for path
	void KeepMovingWhilstWaitingForFirstPath(const CPed* pPed);

	// PURPOSE: Try and create a prop on starting
	void CreateProp() { m_bCreateProp = true; }

	// PURPOSE: Ignore probability in prop creation.
	void ForceCreateProp() { m_bForceCreateProp = true; }
	
	// PURPOSE: Holster out weapon
	void SetHolsterWeapon(bool b) { m_bHolsterWeapon = b; }

	// PURPOSE: Do we have a brolly?
	static bool GetDoesPedHaveBrolly(const CPed* pPed);

	inline void SetLastVehicleWalkedAround(CVehicle * pVehicle) { m_LastVehicleWalkedAround = pVehicle; }
	inline CVehicle * GetLastVehicleWalkedAround() { return m_LastVehicleWalkedAround; }

	virtual CTaskInfo* CreateQueriableState() const;

	void SetBlockedByVehicle(CVehicle * pVehicle);

	// Gets the prop management helper
	virtual CPropManagementHelper* GetPropHelper() { return m_pPropHelper; }

	// Sets the prop management helper
	void SetPropHelper(CPropManagementHelper* pPropHelper) { if( AssertVerify(!(m_pPropHelper == NULL && pPropHelper == NULL)) ) { m_pPropHelper = pPropHelper; } }

	static u32 GetCommonTaskAmbientFlags() { u32 uAmbientFlags = CTaskAmbientClips::Flag_PlayBaseClip | CTaskAmbientClips::Flag_PlayIdleClips; return uAmbientFlags; }

	// Reset static variables dependent on fwTimer::GetTimeInMilliseconds()
	static void ResetStatics() { ms_uCurrentRainTransitionPeriodStartTime = 0; }

protected:

	//
	// FSM API
	//

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// PURPOSE: Override if the task can abort for new event.
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Aborted; }

	// PURPOSE:	Should return true if we know that the aborted subtasks won't be needed
	//			if we resume after getting aborted.
	virtual bool ShouldAlwaysDeleteSubTaskOnAbort() const { return true; }	// We can resume after getting aborted, but don't need the aborted subtasks to be kept around.

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

private:

	class RoadCrossScanParams
	{
	public:

		RoadCrossScanParams()
		: m_vPrevCrossDir(Vector3::ZeroType)
		, m_fCrossingAheadDotMin(CHAIN_ROAD_CROSSDIR_TO_CROSSDIR_DOT_MIN)
		, m_fPedToCrossDistSqMax(DEFAULT_DIST_SQ_PED_TO_START_MAX)
		, m_fPedAheadDotMin(DEFAULT_ROAD_CROSS_PED_TO_START_DOT_MIN)
		, m_bCheckCrossDir(false)
		, m_bCheckForTrafficLight(false)
		, m_bIncludeRoadCrossings(true)
		, m_bIncludeDriveWayCrossings(false)
		, m_bWanderTaskStuck(false)
		{
			// do nothing
		}

		// previous crossing direction
		Vector3 m_vPrevCrossDir;
		// min acceptable dot product between previous and next crossing directions
		float m_fCrossingAheadDotMin;
		// max acceptable distance squared from ped to candidate crossing start position
		float m_fPedToCrossDistSqMax;
		// min acceptable dot product between ped forward and candidate crossing start position
		float m_fPedAheadDotMin;
		// whether to test previous and candidate crossing directions
		bool m_bCheckCrossDir:1;
		// whether to check for nearby traffic light
		bool m_bCheckForTrafficLight:1;
		// whether to include road crossing nodes in the search
		bool m_bIncludeRoadCrossings:1;
		// whether to include driveway crossing nodes in the search
		bool m_bIncludeDriveWayCrossings:1;
		// whether the wander movement task is stuck, has failed to find a path
		bool m_bWanderTaskStuck:1;
	};

	class RoadCrossRecord
	{
	public:

		RoadCrossRecord()
			: m_vCrossingStartPosition(Vector3::ZeroType)
			, m_vCrossingEndPosition(Vector3::ZeroType)
			, m_StartNodeAddress()
			, m_EndNodeAddress()
			, m_ReservationSlot(-1)
			, m_AssignedCrossDelayMS(-1)
			, m_bIsTrafficLightCrossing(false)
			, m_bIsMidStreetCrossing(false)
			, m_bIsDrivewayCrossing(false)
		{
			// do nothing
		}

		void Reset()
		{
			m_vCrossingStartPosition = Vector3::ZeroType;
			m_vCrossingEndPosition = Vector3::ZeroType;
			m_StartNodeAddress.SetEmpty();
			m_EndNodeAddress.SetEmpty();
			m_ReservationSlot = -1;
			m_AssignedCrossDelayMS = -1;
			m_bIsTrafficLightCrossing = false;
			m_bIsMidStreetCrossing = false;
			m_bIsDrivewayCrossing = false;
		}

		// copy operator
		RoadCrossRecord& operator=(const RoadCrossRecord& other)
		{
			m_vCrossingStartPosition = other.m_vCrossingStartPosition;
			m_vCrossingEndPosition = other.m_vCrossingEndPosition;
			m_StartNodeAddress.Set(other.m_StartNodeAddress);
			m_EndNodeAddress.Set(other.m_EndNodeAddress);
			m_ReservationSlot = other.m_ReservationSlot;
			m_AssignedCrossDelayMS = other.m_AssignedCrossDelayMS;
			m_bIsTrafficLightCrossing = other.m_bIsTrafficLightCrossing;
			m_bIsMidStreetCrossing = other.m_bIsMidStreetCrossing;
			m_bIsDrivewayCrossing = other.m_bIsDrivewayCrossing;
			return *this;
		}

		// Start position for the crossing
		Vector3 m_vCrossingStartPosition;
		// End position for the crossing
		Vector3 m_vCrossingEndPosition;
		// Start node address
		CNodeAddress m_StartNodeAddress;
		// End node address
		CNodeAddress m_EndNodeAddress;
		// Reservation slot
		int m_ReservationSlot;
		// Assigned cross delay time
		int m_AssignedCrossDelayMS;
		// Whether the crossing is at a traffic light
		bool m_bIsTrafficLightCrossing:1;
		// Whether the crossing is mid street (not at a crosswalk)
		bool m_bIsMidStreetCrossing:1;
		// Whether the crossing is a driveway
		bool m_bIsDrivewayCrossing:1;
	};

	// Init the members
	void Init(const float fMoveBlendRatio, const float fDir, const CConditionalAnimsGroup * pOverrideClips, const bool bWanderSensibly, float fTargetRadius, const bool bStayOnPavements);

	//
	// Local State functions
	//

	FSM_Return StateInit();
	FSM_Return StateExitVehicle_OnEnter();
	FSM_Return StateExitVehicle_OnUpdate();
	FSM_Return StateWander_OnEnter();
	FSM_Return StateWander_OnUpdate();
	FSM_Return StateWander_OnExit();
	FSM_Return StateWanderToStoredPosition_OnEnter();
	FSM_Return StateWanderToStoredPosition_OnUpdate();
	FSM_Return StateCrossRoadAtLights_OnEnter();
	FSM_Return StateCrossRoadAtLights_OnUpdate();
	FSM_Return StateCrossRoadAtLights_OnExit();
	FSM_Return CrossRoadJayWalking_OnEnter();
	FSM_Return CrossRoadJayWalking_OnUpdate();
	FSM_Return StateMoveToShelterAndWait_OnEnter();
	FSM_Return StateMoveToShelterAndWait_OnUpdate();
	FSM_Return StateCrime_OnUpdate();
	FSM_Return StateHolsterWeapon_OnEnter();
	FSM_Return StateHolsterWeapon_OnUpdate();
	FSM_Return StateBlockedByVehicle_OnEnter();
	FSM_Return StateBlockedByVehicle_OnUpdate();
	FSM_Return StateAborted();

	// Handle wandering with no pavement in certain zones.
	void AnalyzePavementSituation();

	// Helpers functions to look for pavement.
	void StartPavementFloodFillRequest();
	void ProcessPavementFloodFillRequest();

	// Handle wandering in the rain
	void ProcessRaining();

#if FIND_SHELTER
	// Should we try and find shelter?
	bool ProcessMoveToShelterAndWait();

	// Determine if we can shelter at the specified position
	bool ComputeIsRoomToShelterHere(const Vector3& vPosition, const float fCheckRadius, const s32 iMaxNumPedsHere = 2);

	// Callback for ForAllEntitiesIntersecting
	static bool RoomToShelterCB(CEntity* pEntity, void* pData);
#endif //FIND_SHELTER

	// Should we cross the road?
	bool ProcessCrossRoadAtLights();

	// Should we cross a driveway ahead?
	bool ProcessCrossDriveWay();

	// Should we cross the next road ahead as soon as we finish current crossing?
	bool ProcessConsecutiveCrossRoadAtLights(const Vector3& vCrossDir);

	// Try to scan for crossing and transition into the road crossing state
	bool HelperTryToCrossRoadAtLights(const RoadCrossScanParams& scanParams, CTaskGameTimer& scanTimer);

	// Recompute the start position according to the reservation slot value in the record
	void HelperRecomputeStartPositionForReservationSlot(RoadCrossRecord& recordToModify);

	// Should we hay walk across the road?
	bool ProcessCrossRoadJayWalking();

	// Looks to see if there is are road crossings available ahead of the ped
	bool ComputeScanForRoadCrossings(CPed* pPed, CTaskGameTimer& scanTimer, RoadCrossRecord& OutCrossRecord, const RoadCrossScanParams& scanParams);

	// Looks to see if there is a suitable place to stand for the traffic light taking into account of others using the crossing.
	bool ComputeScanForTrafficLights(CPed* pPed, Vector3 &vStartPos, Vector3 &vNodePos, Vector3 &vLinkPos, float fNearbyCrossDistance);

	// Callback for ForAllEntitiesIntersecting
	static bool IsTrafficLightCB(CEntity* pEntity, void* pData);

    // Can we perform a criminal activity?
    bool ProcessCriminalOpportunities();

	// Does the given path node end position match the last road cross start, if any?
	bool DoesLinkPosMatchLastStartPos(CPed* pPed, const CPathNode* pPathNodeEnd) const;

	// Does the given path node start and end match the last road cross start and end, if any?
	bool DoesNodeAndLinkMatchLastCrossing(CPed* pPed, const CPathNode* pPathNodeStart, const CPathNode* pPathNodeEnd) const;

	// Get the conditional animation group to use (commonly, the one named "WANDER").
	const CConditionalAnimsGroup* GetConditionalAnimsGroup() const;

	// If TaskAmbientClips is running as a subtask, return its chosen animation.  Otherwise return -1.
	s32 GetActiveChosenConditionalAnimation() const;

	CTaskAmbientClips* CreateAmbientClipsTask();

	// Increase the next scan time depending on how far the ped is away from the nearest node, min 2sec max 15secs
	void ScanFailedIncreaseTimer(CTaskGameTimer& timerToAdjust,float fDistanceSqr);
	
	bool ShouldHolsterWeapon() const;

	// Determine if the Ped should be allowed to play crosswalk idling animations.
	bool ShouldAllowPedToPlayCrosswalkIdles();

	//
	// Members
	//

	// The input (base) moveblend ratio.
	float m_fMoveBlendRatio;

	// Sped up/slowed down rate
	float m_fWanderMoveRate;

	// The input direction to wander.
	float m_fDirection;

	// The target position.  This is initially the ped's position until 
	float m_fTargetRadius;

	// Peds will always try to find a wander route whose length is between these two values.
	float m_fWanderPathLengthMin;
	float m_fWanderPathLengthMax;

	// After being interrupted by some events, the wander task can be instructed to continue from its
	// previous wandering position & heading by using these variables..
	Vector3 m_vContinueWanderingPos;

#if FIND_SHELTER
	// Handle to path-request used to seek shelter from rain
	TPathHandle m_hFindShelterFloodFillPathHandle;
	u32 m_iLastFindShelterSearchTime;

	// Safe position for this ped to shelter from the rain
	Vector3 m_vShelterPos;
#endif //FIND_SHELTER

	RegdVeh m_LastVehicleWalkedAround;
	RegdVeh m_BlockedByVehicle;
	Vector3 m_vBlockedByVehicleLastPos;

	// Scenario Type
	//u32	m_uiScenarioType;

	// Animations to use, if not wanting the default ("WANDER").
	const CConditionalAnimsGroup * m_pOverrideConditionalAnimsGroup;
	
	// If m_pOverrideConditionalAnimsGroup is set, this optionally specifies the animation to use.
	const s32	m_iOverrideAnimIndex;

	// If we are keeping a prop across a state transition,
	// this is the corresponding anim index for the ambient clips subtask
	s32 m_iAnimIndexForExistingProp;

	// A smart pointer to a prop helper in case we need to keep a prop
	// across state transitions
	CPropManagementHelper::SPtr m_pPropHelper;

	// Throttle the rate of road crossing scans
	CTaskGameTimer m_ScanForRoadCrossingsTimer;
	CTaskGameTimer m_ScanForDriveWayCrossingsTimer;
	CTaskGameTimer m_ScanForJayWalkingLocationsTimer;
	CTaskGameTimer m_ScanForNearbyPedsTimer;

	// Remember the data for the last road crossing performed
	RoadCrossRecord m_RoadCrossRecord;

	// if event scanning is turned off for this ped, we need to manually update the scanner
	u32 m_iLastForcedVehiclePotentialCollisionScanTime;

	CPavementFloodFillHelper m_PavementHelper;

	// Flags
	bool m_bRestartWander							: 1;	// Flag if we want to restart the wander - e.g. if we have been given a new direction
	bool m_bWanderSensibly							: 1;	// Whether the ped is to stop for traffic lights, etc.
	bool m_bStayOnPavements							: 1;	// Does the ped prefer to stay on pavements
	bool m_bWaitingForPath							: 1;	// If this is set (and the m_hPathHandle is non-zero) then we are waiting for out path request to complete
	bool m_bKeepMovingWhilstWaitingForFirstPath		: 1;	// Passed down to the move wander task, ensure that ped keeps moving during a dummy->ped transition
	bool m_bContinueWanderingFromStoredPosition		: 1;	// This is set in MakeAbortable if the task is to continue from its previous position
	bool m_bCreateProp								: 1;	// This is set by CreateProp
	bool m_bForceCreateProp							: 1;	// Ignore probability in CTaskAmbientClips when creating the prop.
	bool m_bHolsterWeapon							: 1;	// Holster our weapon
	bool m_bPerformedLowEntityScan					: 1;	// Flag used to check if a low lod ped has been told to do an entity scan already for positioning at a road crossing.
	bool m_bMadePedRoadCrossReservation				: 1;	// Keep track of whether we made a road crossing reservation
	bool m_bIdlingAtCrossWalk						: 1;	// Keep track of if we are idling at a cross walk
	bool m_bZoneRequiresPavement					: 1;	// It looks bad for peds wandering in this zone without having pavement.
	bool m_bLacksPavement							: 1;	// This ped does not have pavement nearby.
	bool m_bRemoveIfInBadPavementSituation			: 1;	// This wandering ped should be aggressively deleted if in a zone with a pavement problem.
	bool m_bProcessedRain							: 1;
	bool m_bNeedCrossingCheckPathSegDoublesBack		: 1;

#if FIND_SHELTER
	// Counter for sheltering callback
	static s32 ms_iNumShelteringPeds;
#endif //FIND_SHELTER

	// scenario name for waiting to cross road
	static const atHashWithStringNotFinal ms_waitToCrossScenario;

	// scenario index for waiting to cross road
	static s32 ms_ScenarioIndexWaitingToCross;

	// How many peds are currently transitioning to their rain idles
	static u32		ms_uNumRainTransitioningPeds; 
	static u32		ms_uCurrentRainTransitionPeriodStartTime;
	static u32		ms_uLastRainTransitionFrame;

	static Tunables sm_Tunables;
};


class CClonedWanderInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedWanderInfo()
		: m_fMoveBlendRatio(0.0f)
		, m_fDirection(0.0f)
		, m_uOverrideAnimGroup(0)
		, m_uOverrideAnimIndex(0)
		, m_bWanderSensibly(false)
		, m_fTargetRadius(0.0f)
		, m_bStayOnPavements(false)
	{

	}

	CClonedWanderInfo(float fMoveBlendRatio, float fDirection, u32 uOverrideAnimGroup, u8 uOverrideAnimIndex, bool bWanderSensibly, float fTargetRadius, bool bStayOnPavements)
		: m_fMoveBlendRatio(fMoveBlendRatio)
		, m_fDirection(fDirection)
		, m_uOverrideAnimGroup(uOverrideAnimGroup)
		, m_uOverrideAnimIndex(uOverrideAnimIndex)
		, m_bWanderSensibly(bWanderSensibly)
		, m_fTargetRadius(fTargetRadius)
		, m_bStayOnPavements(bStayOnPavements)
	{

	}

	virtual CTask* CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
	{
		const CConditionalAnimsGroup* pOverrideAnimGroup = NULL;
		if (m_uOverrideAnimGroup)
		{
			pOverrideAnimGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_uOverrideAnimGroup);
		}

		s32 iOverrideAnimIndex = (s32)m_uOverrideAnimIndex - 1;

		return rage_new CTaskWander(m_fMoveBlendRatio, m_fDirection, pOverrideAnimGroup, iOverrideAnimIndex, m_bWanderSensibly, m_fTargetRadius, m_bStayOnPavements);
	}

	virtual s32 GetTaskInfoType() const		{ return INFO_TYPE_WANDER; }
	virtual int GetScriptedTaskType() const	{ return SCRIPT_TASK_WANDER_STANDARD; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const float MAX_MOVEBLENDRATIO = 2.0f;
		static const float MAX_DIRECTION = TWO_PI;
		static const float MAX_TARGETRADIUS = 100.0f;

		static unsigned const SIZEOF_MOVEBLENDRATIO = 8;
		static unsigned const SIZEOF_DIRECTION = 8;
		static unsigned const SIZEOF_TARGETRADIUS = 10;
		static unsigned const SIZEOF_OVERRIDEANIMINDEX = 8;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

        m_fMoveBlendRatio = Clamp(m_fMoveBlendRatio, 0.0f, MAX_MOVEBLENDRATIO);
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fMoveBlendRatio, MAX_MOVEBLENDRATIO, SIZEOF_MOVEBLENDRATIO, "Move blend ratio");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDirection, MAX_DIRECTION, SIZEOF_DIRECTION, "Direction");

		bool bHasDefaultParams = m_uOverrideAnimGroup == 0 && m_uOverrideAnimIndex == 0 && m_bWanderSensibly && m_fTargetRadius == CTaskWander::TARGET_RADIUS && m_bStayOnPavements;
		SERIALISE_BOOL(serialiser, bHasDefaultParams);

		if (!bHasDefaultParams || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_HASH(serialiser, m_uOverrideAnimGroup, "Override conditional anim group hash");
			SERIALISE_UNSIGNED(serialiser, m_uOverrideAnimIndex, SIZEOF_OVERRIDEANIMINDEX, "Override conditional anim index");

			bool bWanderSensibly = m_bWanderSensibly;
			bool bStayOnPavements = m_bStayOnPavements;

			SERIALISE_BOOL(serialiser, bWanderSensibly, "Wander sensibly");
			SERIALISE_BOOL(serialiser, bStayOnPavements, "Stay on pavements");
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTargetRadius, MAX_TARGETRADIUS, SIZEOF_TARGETRADIUS, "Target Radius");

			m_bWanderSensibly = bWanderSensibly;
			m_bStayOnPavements = bStayOnPavements;
		}
		else
		{
			m_bWanderSensibly = true;
			m_bStayOnPavements = true;
			m_fTargetRadius = CTaskWander::TARGET_RADIUS;
		}
	}

private:

	CClonedWanderInfo(const CClonedWanderInfo&);
	CClonedWanderInfo &operator=(const CClonedWanderInfo&);

	float m_fMoveBlendRatio;
	float m_fDirection;
	float m_fTargetRadius;
	u32	  m_uOverrideAnimGroup;
	u8    m_uOverrideAnimIndex;
	bool  m_bWanderSensibly : 1;
	bool  m_bStayOnPavements : 1;
};

class CTaskWanderInArea : public CTask
{
public:

	// State
	enum State
	{
		State_Init = 0,
		State_Wait,
		State_Wander,
		State_AchieveHeading
	};

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinWaitTime; // Minimum wait time between wanders, in seconds.
		float m_MaxWaitTime; // Maximum wait time between wanders, in seconds.

		PAR_PARSABLE;
	};

	// Construction
	CTaskWanderInArea(const float fMoveBlendRatio, const float fWanderRadius, const Vector3 &vWanderCenterPos, float fMinWaitTime = sm_Tunables.m_MinWaitTime, float fMaxWaitTime = sm_Tunables.m_MaxWaitTime);

	// Destruction
	~CTaskWanderInArea() {};

	// RETURNS: Task ID
	s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_WANDER_IN_AREA; }

	// PURPOSE: Create a copy of the task
	// RETURNS: The copy of the task that we created
	aiTask* Copy() const { return rage_new CTaskWanderInArea(m_fMoveBlendRatio, m_fWanderRadius, m_vWanderCenterPos, m_fMinWaitTime, m_fMaxWaitTime); }

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

protected:

	// PURPOSE: Called once per frame before FSM update
	// RETURNS: If FSM_QUIT is returned the state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Init; }

private:

	// Standard Init function
	void Init(const float fMoveBlendRatio, const float fWanderRadius, const Vector3 &vWanderCenterPos);

	//
	// Local state function
	//

	FSM_Return StateInit();
	FSM_Return StateWait_OnEnter();
	FSM_Return StateWait_OnUpdate();
	FSM_Return StateWander_OnEnter();
	FSM_Return StateWander_OnUpdate();
	FSM_Return StateAchieveHeading_OnEnter();
	FSM_Return StateAchieveHeading_OnUpdate();

	//
	// Members
	//

	// The base moveblend ratio which comes from input.
	float m_fMoveBlendRatio;

	// The modified blend ratio to have some randomness
	float m_fWanderMoveBlendRatio;

	// The radius that we want to wander in
	float m_fWanderRadius;

	// Timer for how long to wait at our current position
	float m_fWaitTimer;

	float m_fMinWaitTime;
	float m_fMaxWaitTime;

	// The center point of our wander "zone", i.e. where the radius extends from
	Vector3 m_vWanderCenterPos;

	// Current calculated target
	Vector3 m_vMoveToPos;

	static Tunables sm_Tunables;
};
#endif // TASK_WANDER_H
