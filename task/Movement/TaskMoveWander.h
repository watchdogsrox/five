#ifndef TASK_MOVE_WANDER_H
#define TASK_MOVE_WANDER_H

#include "Control/Route.h"
#include "Task/Default/TaskWander.h"
#include "Task/Helpers/PavementFloodFillHelper.h"
#include "Task/Movement/TaskNavBase.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"

class CEntity;
class CPed;

class CWanderParams
{
public:
	CWanderParams()
	{
	}
	~CWanderParams() { }

	float m_fMoveBlendRatio;
	float m_fHeading;
	float m_fTargetRadius;
	bool m_bWanderSensibly;
	bool m_bStayOnPavements;
	bool m_bContinueFromNetwork;
};

class CWanderFlags
{
public:
	CWanderFlags() { m_iFlags = 0; }
	~CWanderFlags() { }

	inline bool IsSet(const u32 i) const { return ((m_iFlags & i)!=0); }
	inline void Set(const u32 iFlags) { m_iFlags |= iFlags; }
	inline void Set(const u32 iFlags, const bool bState)
	{
		if(bState) m_iFlags |= iFlags;
		else m_iFlags &= ~iFlags;
	}
	inline void Reset(const u32 iFlags) { m_iFlags &= ~iFlags; }
	inline void ResetAll() { m_iFlags = 0; }

	u32 GetAsPathServerFlags() const;

	enum
	{
		MW_WanderSensibly					= 0x01,
		MW_StayOnPavements					= 0x02,
		MW_ContinueFromNetwork				= 0x04,
		MW_HasNewWanderDirection			= 0x08,
		MW_AbortedForWalkRoundVehicle		= 0x10
	};

private:

	u32 m_iFlags;
};


class CTaskMoveWander : public CTaskNavBase
{
public:

	static const float ms_fMinLeftBeforeRequestingNextPath;
	static const u32 ms_iUpdateWanderHeadingFreq;
	static const float ms_fMinWanderDist;
	static const float ms_fMaxWanderDist;
	static const u32 ms_iMaxNumAttempts;
	static const float ms_fWanderHeadingModifierInc;
	static const u32 m_iTimeToPauseBetweenRepeatSearches;
	static const s32 ms_iMaxTimeDeltaToRejectDoubleBackedPathsMS;

	enum
	{
		State_WaitingBetweenRepeatSearches = CTaskNavBase::NavBaseState_NumStates
	};

public:

	CTaskMoveWander(const CWanderParams & wanderParams);
	// Legacy constructors
	CTaskMoveWander(const float fMoveBlendRatio, const float fHeading, const bool bWanderSensibly, const float fTargetRadius, const bool bStayOnPavements, const bool bContinueFromNetwork);
	virtual ~CTaskMoveWander();

	void Init(const CWanderParams & wanderParams);

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_WANDER; }
	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget() const { Assertf(false, "CTaskMoveWander::GetTarget() shouldn't be called, please attach callstack to bug."); return Vector3(0.0f, 0.0f, 0.0f); }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return m_fTargetRadius; }

	virtual aiTask* Copy() const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_WaitingBetweenRepeatSearches: return "WaitingBetweenRepeatSearches";
		default: return GetBaseStateName(iState);
		}
	}
	virtual void Debug() const;
#if DEBUG_DRAW
	virtual void DrawPedText(const CPed * pPed, const Vector3 & vWorldCoords, const Color32 iTxtCol, int & iTextStartLineOffset) const;
#endif
#endif 

	virtual s32 GetDefaultStateAfterAbort() const;
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return StateFollowingPath_OnUpdate(CPed * pPed);
	FSM_Return StateWaitingBetweenRepeatSearches_OnEnter(CPed * pPed);
	FSM_Return StateWaitingBetweenRepeatSearches_OnUpdate(CPed * pPed);

	// Initialise the structure which is passed into the CPathServer::RequestPath() function
	virtual void InitPathRequestPathStruct(CPed * pPed, TRequestPathStruct & reqPathStruct, const float fDistanceAheadOfPed=0.0f);

	virtual float GetRouteDistanceFromEndForNextRequest() const { return ms_fMinLeftBeforeRequestingNextPath; }

	virtual bool ShouldKeepMovingWhilstWaitingForPath(CPed * pPed, bool & bMayUsePreviousTarget) const;

	virtual bool OnSuccessfulRoute(CPed * pPed);
	virtual FSM_Return OnFailedRoute(CPed * pPed, const TPathResultInfo & pathResultInfo);

	int GetCurrentRouteProgress();
	// This returns the final point of the point route, or the ped's current position if there is no point route.
	bool GetWanderTarget(Vector3 & vTarget);
	inline float GetWanderHeading() { return m_fWanderHeading; }
	// Set the min & max ranges of distance for the wander paths we will request from the pathfinder
	inline void SetWanderPathDistances(const float fMinDist, const float fMaxDist) { m_fMinWanderDist = fMinDist; m_fMaxWanderDist = fMaxDist; }
	inline int GetNumRoutePoints() { return m_pRoute ? m_pRoute->GetSize() : 0; }
	inline const Vector3 & GetRoutePoint(const int i) { return m_pRoute->GetPosition(i); }
	inline const TNavMeshWaypointFlag GetRouteWayPoint(const int i)
	{
		Assert(m_pRoute && i >= 0 && i <= m_pRoute->GetSize());
		return m_pRoute->GetWaypoint(i);
	}
	inline bool HasFailedRouteAttempt() const { return GetState() == State_WaitingBetweenRepeatSearches; }
	
	void ClearNextPathDoublesBack() { m_bNextPathDoublesBack = false; }
	bool GetNextPathDoublesBack() const { return m_bNextPathDoublesBack; }

protected:

	bool DoesWanderPathDoubleBack(const CPed * pPed);

	bool DoesNextWanderPathDoubleBack();

	// Builds a list of influence spheres which wandering peds can use to avoid shocking event locations
	static int BuildInfluenceSpheres(const CPed * pPed, const float& fRadiusFromPed, TInfluenceSphere * pSpheres, const int iMaxNum);

	void UpdateWanderHeadingModifier();

private:

	CWanderFlags m_WanderFlags;
	CTaskGameTimer m_UpdateWanderHeadingTimer;
	u32 m_iTimeWasLastWanderingMS;
	float m_fWanderHeading;
	float m_fWanderHeadingModifier;
	float m_fMinWanderDist;
	float m_fMaxWanderDist;
	float m_fTimeWanderingOffNavmesh;
	RegdVeh m_LastVehicleWanderedAround;
	bool m_bHadFailedInAllDirections;
	bool m_bNextPathDoublesBack;
};







class CTaskMoveCrossRoadAtTrafficLights : public CTaskMove
{
public:

	struct WaitingOffset
	{
		WaitingOffset()
			: m_Pos()
		{}

		~WaitingOffset()
		{}

		Vector3	m_Pos;

		PAR_SIMPLE_PARSABLE;
	};

	struct Tunables : public CTuning
	{
		Tunables();

		bool m_bTrafficLightPositioning;
		atArray<WaitingOffset> m_WaitingOffsets;
		u32 m_iMaxPedsAtTrafficLights;
		float m_fMinDistanceBetweenPeds;
		float m_fDecideToRunChance;
		float m_fPlayerObstructionCheckRadius;
		float m_fPlayerObstructionRadius;
		bool m_bDebugRender;

		PAR_PARSABLE;
	};

	enum
	{
		State_InitialDecide,
		State_WalkingToCrossingPoint,
		State_TurningToFaceCrossing,
		State_WaitingForLightsToChange,
		State_WaitingForGapInTraffic,
		State_CrossingRoad
	};

	enum JunctionStatus
	{
		eNotRegistered,
		eWaiting,
		eCrossing
	};

	static const float ms_fDefaultTargetRadius;
	static const s32 ms_iCheckLightsMinFreqMS;
	static const s32 ms_iCheckLightsMaxFreqMS;

	CTaskMoveCrossRoadAtTrafficLights(const Vector3 & vStartPos, const Vector3 & vEndPos, const Vector3 * pExistingMoveTarget, int crossDelayMS, bool bIsTrafficLightCrossing, bool bIsMidRoadCrossing, bool bIsDriveWayCrossing);
	~CTaskMoveCrossRoadAtTrafficLights();

	virtual aiTask* Copy() const;
	virtual void CleanUp();
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS; }

	inline const Vector3 & GetStartPos() const { return m_vStartPos; }
	inline const Vector3 & GetEndPos() const { return m_vEndPos; }
	inline const Vector3 & GetCrossingDir() const { return m_vCrossingDir; }
	inline bool IsCrossingRoad() const { return m_bCrossingRoad; }
	inline bool IsWaitingToCrossRoad() const { return m_bWaiting; }
	inline bool IsReadyToChainNextCrossing() const { return m_bReadyToChainNextCrossing; }
	inline bool IsArrivingAtPavement() const { return m_bArrivingAtPavement; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_InitialDecide: return "InitialDecide";
			case State_WalkingToCrossingPoint: return "WalkingToCrossingPoint";
			case State_TurningToFaceCrossing: return "TurningToFaceCrossing";
			case State_WaitingForLightsToChange: return "WaitingForLightsToChange";
			case State_WaitingForGapInTraffic: return "WaitingForGapInTraffic";
			case State_CrossingRoad: return "CrossingRoad";
		}
		return NULL;
	}
#endif
#if DEBUG_DRAW
	void UpdateDebugLightPosition() const;
#endif // DEBUG_DRAW

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_InitialDecide; }

	FSM_Return InitialDecide_OnUpdate(CPed * pPed);
	FSM_Return WalkingToCrossingPoint_OnEnter(CPed * pPed);
	FSM_Return WalkingToCrossingPoint_OnUpdate(CPed * pPed);
	FSM_Return TurningToFaceCrossing_OnEnter(CPed * pPed);
	FSM_Return TurningToFaceCrossing_OnUpdate(CPed * pPed);
	FSM_Return WaitingForGapInTraffic_OnEnter(CPed * pPed);
	FSM_Return WaitingForGapInTraffic_OnUpdate(CPed * pPed);
	FSM_Return WaitingForGapInTraffic_OnExit(CPed * pPed);
	FSM_Return WaitingForLightsToChange_OnEnter(CPed * pPed);
	FSM_Return WaitingForLightsToChange_OnUpdate(CPed * pPed);
	FSM_Return WaitingForLightsToChange_OnExit(CPed * pPed);
	FSM_Return CrossingRoad_OnEnter(CPed * pPed);
	FSM_Return CrossingRoad_OnUpdate(CPed * pPed);
	FSM_Return CrossingRoad_OnExit(CPed * pPed);

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget() const { return m_vEndPos; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return ms_fDefaultTargetRadius; }

	static Tunables &GetTunables() { return sm_Tunables; }

private:

	// Helper methods to add and remove a path blocking object for the ped
	void AddPathBlockingObject(const CPed* pPed);
	void RemovePathBlockingObject();

	// The position on this side of the road to start from (we may not use this in the end)
	Vector3 m_vStartPos;
	// The position on the far side of the road, which the ped is going to walk towards
	Vector3 m_vEndPos;
	// The direction of the crossing
	Vector3 m_vCrossingDir;
	// Because checking lights can be expensive if we are at a hand-edited junction, we timeslice checks
	CTaskGameTimer m_CheckLightsTimer;
	// This (optional) vector is the position that the ped was moving towards before they decided to cross the road.
	// If 'm_bPedHasExistingMoveTarget' is set, we will set the CTaskComplexMoveFollowNavMeshRoute task to move towards
	// this target whilst it tries to find a route towards the m_vStartPos for the crossing..
	Vector3 m_vExistingMoveTarget;

	CPavementFloodFillHelper m_pavementHelper;

#if DEBUG_DRAW
	// The position to draw what the ped thinks the light status is for crossing.
	// The color will indicate red/amber/green and position reflects which crossing is considered.
	mutable Vector3 m_vDebugLightPosition;
#endif // DEBUG_DRAW

	// The distance squared between start and end positions
	float m_fCrossingDistSq;

	// The traffic light state
	int m_eLastLightState;

	// The index for the junction we are crossing at
	s32 m_iCrossingJunctionIndex;

	// How long to delay crossing for aesthetics
	int m_iCrossDelayMS;

	// The dynamic object index for blocking bound while waiting to cross
	TDynamicObjectIndex m_pathBlockingObjectIndex;

	//What current junction knows about ped
	JunctionStatus m_junctionStatus;

	// The ped is crossing obeying a traffic light
	u32 m_bIsTrafficLightCrossing			:1;
	
	// This flag is set when the ped is walking across the road (using CTaskComplexMoveFollowNavMeshRoute).
	// This is to distinguish between when the ped is using the same task in order to position themselves
	// prior to walking across.
	u32 m_bIsMidRoadCrossing				:1;

	// The ped is crossing between driveway nodes, no junction/traffic light in play
	u32 m_bIsDriveWayCrossing				:1;

	u32 m_bFirstTimeRun						:1;
	u32 m_bCrossingRoad						:1;
	u32 m_bWaiting							:1;
	u32 m_bPedHasExistingMoveTarget			:1;
	u32 m_bHasDecidedToRun					:1;
	u32 m_bCrossWithoutStopping				:1;
	u32 m_bPedIsNearToJunction				:1;
	u32 m_bPedIsJayWalking					:1;
	u32 m_bReadyToChainNextCrossing			:1;
	u32 m_bArrivingAtPavement				:1;
#if DEBUG_DRAW
	u32 m_bIsResumingCrossing				:1;
#endif // DEBUG_DRAW

	int SetWaitOrCrossRoadState(CPed* pPed);
	bool CanJayWalkAtLights(CPed* pPed);
	bool CanBeginCrossingRoadDirectly(CPed* pPed) const;
	bool CanBeginCrossingDriveWayDirectly(CPed* pPed) const;
	bool CanBeginCrossingPosAndHeading(CPed* pPed) const;
	bool IsCrossingAtLights() const;
	bool IsPedCloseToPosition(const Vec3V& queryPosition, float fQueryDistance) const;
	bool IsPlayerObstructingPosition(const Vec3V& queryPosition) const;
	CTask * CreateWaitTask();
	CTask * CreateCrossRoadTask(CPed* pPed);
	void HelperUpdateKnownLightState(bool bConsiderTimeRemaining);
	void HelperNotifyJunction(JunctionStatus newStatus);

	static Tunables	sm_Tunables;
};


class CTaskMoveWaitForTraffic : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_WaitingUntilSafeToCross
	};

	CTaskMoveWaitForTraffic(const Vector3 & vStartPos, const Vector3 & vEndPos, const bool bMidRoadCrossing);
	~CTaskMoveWaitForTraffic();

	virtual aiTask* Copy() const {
		return rage_new CTaskMoveWaitForTraffic(
			m_vStartPos,
			m_vEndPos,
			m_bIsMidRoadCrossing );
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_WAIT_FOR_TRAFFIC; }

#if !__FINAL
	virtual void Debug() const;
	virtual atString GetName() const {return atString("WaitForTraffic");}
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "State_Initial";
			case State_WaitingUntilSafeToCross: return "State_WaitingUntilSafeToCross";
		}
		return NULL;
	}
#endif    

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed * pPed);
	FSM_Return WaitingUntilSafeToCross_OnEnter(CPed * pPed);
	FSM_Return WaitingUntilSafeToCross_OnUpdate(CPed * pPed);

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget() const { return Vector3(0.0f,0.0f,0.0f); }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return 0.0f; }

	inline bool GetWasSafeToCross() { return m_bWasSafe; }

private:    

	static const u32 ms_iScanTime;
	static const u32 ms_iDefaultGiveUpTime;

	CTaskGameTimer m_GiveUpTimer;
	CTaskGameTimer m_ScanTimer;
	Vector3 m_vStartPos;
	Vector3 m_vEndPos;

	bool m_bWasSafe;
	bool m_bIsMidRoadCrossing;

	bool IsSafeToCross(CPed * pPEd);
};


class CTaskMoveGoToShelterAndWait : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_WaitingToFindShelter,
		State_MovingToShelter,
		State_WaitingInShelter
	};

	static const float ms_fShelterTargetRadius;

	CTaskMoveGoToShelterAndWait(const float fMoveBlendRatio, const float fMaxSearchDist, s32 iMaxWaitTime);
	CTaskMoveGoToShelterAndWait(const float fMoveBlendRatio, const Vector3 & vTargetPos, s32 iMaxWaitTime);
	virtual ~CTaskMoveGoToShelterAndWait();

	virtual aiTask * Copy() const
	{
		if(m_bHasSpecifiedTarget)
		{
			return rage_new CTaskMoveGoToShelterAndWait(m_fMoveBlendRatio, m_vShelterPosition, m_iMaxWaitTime);
		}
		else
		{
			return rage_new CTaskMoveGoToShelterAndWait(m_fMoveBlendRatio, m_fMaxShelterSearchDist, m_iMaxWaitTime);
		}
	}

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed * pPed);
	FSM_Return WaitingToFindShelter_OnEnter(CPed * pPed);
	FSM_Return WaitingToFindShelter_OnUpdate(CPed * pPed);
	FSM_Return MovingToShelter_OnEnter(CPed * pPed);
	FSM_Return MovingToShelter_OnUpdate(CPed * pPed);
	FSM_Return WaitingInShelter_OnEnter(CPed * pPed);
	FSM_Return WaitingInShelter_OnUpdate(CPed * pPed);

	inline virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GOTO_SHELTER_AND_WAIT; }

#if !__FINAL
	virtual void Debug() const;
	virtual atString GetName() const {return atString("GoToShelterAndWait");}
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_WaitingToFindShelter: return "WaitingToFindShelter";
		case State_MovingToShelter: return "MovingToShelter";
		case State_WaitingInShelter: return "WaitingInShelter";
		}
		return NULL;
	}
#endif

	// Movement interface members
	virtual bool HasTarget() const;
	virtual Vector3 GetTarget() const { return m_vShelterPosition; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return ms_fShelterTargetRadius; }
	virtual void SetTarget(const CPed* UNUSED_PARAM(pPed), Vec3V_In UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce)) { }
	virtual void SetTargetRadius(const float UNUSED_PARAM(fTargetRadius)) { }

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

private:

	CTask * CreateSubTask(int iTaskType, const CPed * pPed);

	Vector3 m_vShelterPosition;
	CTaskGameTimer m_WaitTimer;

	float m_fMoveBlendRatio;
	float m_fMaxShelterSearchDist;
	s32 m_iMaxWaitTime;
	TPathHandle m_hFloodFillPathHandle;

	// May need to do special stuff the first time run
	u32 m_bFirstTime				:1;
	// If this task was called with a Vector3 target pointer, then we go automatically to that position.
	// If not, then this task will perform a cover search of its own.
	u32 m_bHasSpecifiedTarget	:1;
};






#endif	// TASK_MOVE_WANDER_H