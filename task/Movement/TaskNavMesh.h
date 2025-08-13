#ifndef TASK_NAVMESH_H
#define TASK_NAVMESH_H

#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskNavBase.h"
#include "Task/Movement/TaskIdle.h"

#define __PROPER_CORNERING 1

enum eNavMeshRouteResult
{
	NAVMESHROUTE_TASK_NOT_FOUND,
	NAVMESHROUTE_ROUTE_NOT_YET_TRIED,
	NAVMESHROUTE_ROUTE_NOT_FOUND,
	NAVMESHROUTE_ROUTE_FOUND
};

enum eNavMeshScriptFlags
{
	ENAV_NO_STOPPING											= BIT0,
	ENAV_ADVANCED_SLIDE_TO_COORD_AND_ACHIEVE_HEADING_AT_END		= BIT1,
	ENAV_GO_FAR_AS_POSSIBLE_IF_TARGET_NAVMESH_NOT_LOADED		= BIT2,
	ENAV_UNUSED													= BIT3,
	ENAV_KEEP_TO_PAVEMENTS										= BIT4,
	ENAV_NEVER_ENTER_WATER										= BIT5,
	ENAV_DONT_AVOID_OBJECTS										= BIT6,
	ENAV_ADVANCED_USE_MAX_SLOPE_NAVIGABLE						= BIT7,
	ENAV_SUPRESS_START_ANIMATION								= BIT8,
	ENAV_STOP_EXACTLY											= BIT9,
	ENAV_ACCURATE_WALKRUN_START									= BIT10,
	ENAV_DONT_AVOID_PEDS										= BIT11,
	ENAV_DONT_ADJUST_TARGET_POSITION							= BIT12,
	ENAV_SUPPRESS_EXACT_STOP									= BIT13,
	ENAV_ADVANCED_USE_CLAMP_MAX_SEARCH_DISTANCE					= BIT14,
	ENAV_PULL_FROM_EDGE_EXTRA									= BIT15,

	ENAV_NUM_FLAGS = 15,
};

struct TNavMeshScriptStruct
{
	TNavMeshScriptStruct()
	{
		m_fSlideToCoordHeading = 0.0f;
		m_fMaxSlopeNavigable = 0.0f;
		m_fClampMaxSearchDistance = 0.0f;
	}
	float m_fSlideToCoordHeading;
	float m_fMaxSlopeNavigable;
	float m_fClampMaxSearchDistance;
};

class CFollowNavMeshFlags
{
public:
	CFollowNavMeshFlags() { m_iFlags = 0; }
	~CFollowNavMeshFlags() { }

	inline bool IsSet(const u32 i) const { return ((m_iFlags & i)!=0); }
	inline void Set(const u32 iFlags) { m_iFlags |= iFlags; }
	inline void Set(const u32 iFlags, const bool bState)
	{
		if(bState) m_iFlags |= iFlags;
		else m_iFlags &= ~iFlags;
	}
	inline void Reset(const u32 iFlags) { m_iFlags &= ~iFlags; }
	inline void ResetAll() { m_iFlags = 0; }

	u64 GetAsPathServerFlags() const;

	enum
	{
		FNM_FleeFromTarget								= 0x01,
		FNM_GoAsFarAsPossibleIfNavmeshNotLoaded			= 0x02,
		FNM_PreferPavements								= 0x04,
		FNM_NeverLeavePavements							= 0x08,
		FNM_NeverEnterWater								= 0x10,
		FNM_NeverStartInWater							= 0x20,
		FNM_FleeNeverEndInWater							= 0x40,
		FNM_GoStraightToTargetIfPathNotFound			= 0x80,
		FNM_Unused										= 0x100,
		FNM_QuitTaskIfRouteNotFound						= 0x200,
		FNM_SupressStartAnimation						= 0x400,
		FNM_PreserveHeightInformationInPath				= 0x800,
		FNM_PullBackFleeTargetFromPed					= 0x1000,
		FNM_PolySearchForward							= 0x2000,
		FNM_ReEvaluateInfluenceSpheres					= 0x4000,
		FNM_RestartKeepMoving							= 0x8000,
		FNM_AvoidTrainTracks							= 0x10000
	};

private:

	u32 m_iFlags;
};

class CCachedInfluenceSpheres
{
public:
	s32 m_iNumSpheres;
	TInfluenceSphere m_Spheres[MAX_NUM_INFLUENCE_SPHERES];
};

class CClonedMoveFollowNavMeshInfo : public CSerialisedFSMTaskInfo
{
public:

    CClonedMoveFollowNavMeshInfo(const float    moveBlendRatio,
                                 const Vector3 &targetPos,
                                 const float    targetRadius,
                                 const int      time,
                                 const unsigned flags,
                                 const float    targetHeading);
    CClonedMoveFollowNavMeshInfo();

    virtual CTask  *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_FOLLOW_NAVMESH_TO_COORD; }
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_FOLLOW_NAV_MESH_TO_COORD; }

    void Serialise(CSyncDataBase& serialiser)
	{
        const float    MAX_TARGET_RADIUS       = 100.0f;
        const unsigned SIZEOF_MOVE_BLEND_RATIO = 8;
        const unsigned SIZEOF_TARGET_RADIUS    = 8;
        const unsigned SIZEOF_HEADING          = 8;
        const unsigned SIZEOF_TIME             = 16;
        const unsigned SIZEOF_FLAGS            = 12;
        
        SERIALISE_POSITION(serialiser, m_TargetPos, "Target Pos");
        
		bool moveBlendRatioValid = !IsClose(m_MoveBlendRatio, 1.0f);

		SERIALISE_BOOL(serialiser, moveBlendRatioValid);

		if (moveBlendRatioValid || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_MoveBlendRatio, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVE_BLEND_RATIO, "Move Blend Ratio");
		}
		else
		{
			m_MoveBlendRatio = 1.0f;
		}

		bool targetRadiusValid = !IsClose(m_TargetRadius, 0.0f);
		SERIALISE_BOOL(serialiser, targetRadiusValid);

		if (targetRadiusValid || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_TargetRadius,   MAX_TARGET_RADIUS,     SIZEOF_TARGET_RADIUS,       "Target Radius");
		}
		else
		{
			m_TargetRadius = 0.0f;
		}

        bool targetHeadingValid = m_TargetHeading < DEFAULT_NAVMESH_FINAL_HEADING;

        SERIALISE_BOOL(serialiser, targetHeadingValid);

        if(targetHeadingValid || serialiser.GetIsMaximumSizeSerialiser())
        {
            SERIALISE_PACKED_FLOAT(serialiser, m_TargetHeading, PI, SIZEOF_HEADING, "Target Heading");
        }
        else
        {
            m_TargetHeading = DEFAULT_NAVMESH_FINAL_HEADING;
        }

		bool timeValid = (m_Time != -1);

		SERIALISE_BOOL(serialiser, timeValid);

		if (timeValid || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_INTEGER(serialiser, m_Time,   SIZEOF_TIME, "Delay Time");
		}
		else
		{
			m_Time = -1;
		}

        SERIALISE_UNSIGNED(serialiser, m_Flags, SIZEOF_FLAGS, "Flags");
    }

private:

    float    m_MoveBlendRatio; // move blend ratio to use when navigating
    Vector3  m_TargetPos;      // target position to navigate to
    float    m_TargetRadius;   // radius around the target position before stopping
    int      m_Time;           // time before ped is warped to target position
    unsigned m_Flags;          // flags configuring task behaviour
    float    m_TargetHeading;  // target heading when stopping
};

class CNavParams;

class CTaskMoveFollowNavMesh : public CTaskNavBase
{
public:
	struct Tunables : public CTuning
	{
		Tunables();
		u8 uRepeatedAttemptsBeforeTeleportToLeader;
		PAR_PARSABLE;
	};
	static Tunables				sm_Tunables;


	static const float ms_fTargetRadius;
	static const float ms_fSlowDownDistance;
	static const float ms_fDefaultCompletionRadius;
	static dev_s32 ms_iReEvaluateInfluenceSpheresFrequency;

#if __BANK
	static bool ms_bRenderDebug;
#endif

public:
	CTaskMoveFollowNavMesh(const CNavParams & navParams);
	// Legacy constructors
	CTaskMoveFollowNavMesh(const float fMoveBlendRatio, const Vector3& vTarget, const float fTargetRadius=ms_fTargetRadius, const float fSlowDownDistance=ms_fSlowDownDistance, const int iTime=-1, const bool bUseBlending=true, const bool bFleeFromTarget=false, SpheresOfInfluenceBuilder* pfnSpheresOfInfluenceBuilder = NULL, float fPathCompletionRadius = ms_fDefaultCompletionRadius, bool bSlowDownAtEnd = true); 
	virtual ~CTaskMoveFollowNavMesh();

	void Init(const CNavParams & navParams);

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH; }
	virtual bool HasTarget() const;
	virtual Vector3 GetTarget() const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return m_fTargetRadius; }

	virtual aiTask* Copy() const;

	enum
	{
		State_WaitingBetweenSearchAttempts = CTaskNavBase::NavBaseState_NumStates,
		State_WaitingBetweenRepeatSearches,
		State_SlidingToCoord,
		State_GetOntoMainNavMesh,
		State_AttemptTeleportToLeader
	};

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_WaitingBetweenSearchAttempts: return "WaitingBetweenSearchAttempts";
			case State_WaitingBetweenRepeatSearches: return "WaitingBetweenRepeatSearches";
			case State_SlidingToCoord: return "SlidingToCoord";
			default: return GetBaseStateName(iState);
		}
	}
	virtual void Debug() const;
#if DEBUG_DRAW
	virtual void DrawPedText(const CPed * pPed, const Vector3 & vWorldCoords, const Color32 iTxtCol, int & iTextStartLineOffset) const;
#endif
#endif 

	virtual s32 GetDefaultStateAfterAbort() const;
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

    // Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

	// Initialise the structure which is passed into the CPathServer::RequestPath() function
	virtual void InitPathRequestPathStruct(CPed * pPed, TRequestPathStruct & reqPathStruct, const float fDistanceAheadOfPed=0.0f);

	virtual float GetRouteDistanceFromEndForNextRequest() const;
	virtual float GetDistanceAheadForSetNewTarget() const;

	int GetNavMeshRouteMethod() const { return m_iNavMeshRouteMethod; }
	eNavMeshRouteResult GetNavMeshRouteResult() const { return (eNavMeshRouteResult)m_iNavMeshRouteResult; }

	void SetFleeSafeDist(const float f) { m_fFleeSafeDist = f; }
	float GetFleeSafeDist() const { return m_fFleeSafeDist; }
	void SetCompletionRadius(const float f) { m_fPathCompletionRadius = f; }
	float GetCompletionRadius() const { return m_fPathCompletionRadius; }

	void SetInfluenceSphereBuilderFn(CTaskNavBase::SpheresOfInfluenceBuilder* pFn) { m_pInfluenceSphereBuilderFn = pFn; }

	bool IsFollowingNavMeshRoute() const;
	bool IsMovingWhilstWaitingForPath() const;
	bool IsWaitingToLeaveVehicle() const;
	bool IsUsingLadder() const;
	bool IsUnableToFindRoute() const;

	// This is the time to wait between trying different route methods (eg. normal, reduced bbox, no objects, etc)
	inline void SetTimeToPauseBetweenSearchAttempts(s16 t) { m_iTimeToPauseBetweenSearchAttempts = t; }
	inline s16 GetTimeToPauseBetweenSearchAttempts() const { return m_iTimeToPauseBetweenSearchAttempts; }
	// This is the time to wait before starting again, if all route methods have failed
	inline void SetTimeToPauseBetweenRepeatSearches(s16 t) { m_iTimeToPauseBetweenRepeatSearches = t; }
	inline s16 GetTimeToPauseBetweenRepeatSearches() const { return m_iTimeToPauseBetweenRepeatSearches; }

	// Set behaviour flags as passed in from script
	// Flags are a combination of bits from the eNavMeshScriptFlags enum
	void SetScriptBehaviourFlags(const int iScriptFlags, TNavMeshScriptStruct * pScriptStruct);

	//*****************************************************************************
	// Legacy accessors which should now go via the CFollowNavMeshFlags interface

	void SetGoAsFarAsPossibleIfNavMeshNotLoaded(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_GoAsFarAsPossibleIfNavmeshNotLoaded, b); }
	bool GetGoAsFarAsPossibleIfNavMeshNotLoaded() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_GoAsFarAsPossibleIfNavmeshNotLoaded); }
	void SetKeepToPavements(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_PreferPavements, b); }
	bool GetKeepToPavements() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PreferPavements); }
	void SetNeverLeavePavements(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_NeverLeavePavements, b); }
	bool GetNeverLeavePavements() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_NeverLeavePavements); }
	void SetDontStandStillAtEnd(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontStandStillAtEnd, b); }
	bool GetDontStandStillAtEnd() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontStandStillAtEnd); }
	void SetKeepMovingWhilstWaitingForPath(bool b, float fDist=4.0f) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath, b); m_fDistanceToMoveWhileWaitingForPath = fDist; }
	bool GetKeepMovingWhilstWaitingForPath() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_KeepMovingWhilstWaitingForPath); }
	void SetAvoidObjects(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidObjects, b); }
	bool GetAvoidObjects() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidObjects); }
	void SetAvoidPeds(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AvoidPeds, b); }
	bool GetAvoidPeds() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AvoidPeds); }
	void SetNeverEnterWater(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_NeverEnterWater, b); }
	bool GetNeverEnterWater() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_NeverEnterWater); }
	void SetNeverStartInWater(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_NeverStartInWater, b); }
	bool GetNeverStartInWater() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_NeverStartInWater); }
	void SetFleeNeverEndInWater(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_FleeNeverEndInWater, b); }
	bool GetFleeNeverEndInWater() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_FleeNeverEndInWater); }
	void SetGoStraightToCoordIfPathNotFound(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_GoStraightToTargetIfPathNotFound, b); }
	bool GetGoStraightToCoordIfPathNotFound() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_GoStraightToTargetIfPathNotFound); }
	void SetQuitTaskIfRouteNotFound(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_QuitTaskIfRouteNotFound, b); }
	bool GetQuitTaskIfRouteNotFound() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_QuitTaskIfRouteNotFound); }
	void SetIsFleeing(bool b) { return m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_FleeFromTarget, b); }	
	bool GetIsFleeing() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_FleeFromTarget); }	
	void SetSupressStartAnimations(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_SupressStartAnimation, b); }
	bool GetSupressStartAnimations() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_SupressStartAnimation); }
	void SetPreserveHeightInformationInPath(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_PreserveHeightInformationInPath, b); }
	bool GetPreserveHeightInformationInPath() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PreserveHeightInformationInPath); }
	void SetStopExactly(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_StopExactly, b); }
	bool GetStopExactly() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StopExactly); }
	void SetSlowDownInsteadOfUsingRunStops(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SlowDownInsteadOfUsingRunStops, b); }
	bool GetSlowDownInsteadOfUsingRunStops() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SlowDownInsteadOfUsingRunStops); }
	void SetPullBackFleeTargetFromPed(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_PullBackFleeTargetFromPed, b); }
	bool GetPullBackFleeTargetFromPed() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PullBackFleeTargetFromPed); }
	void SetPolySearchForward(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_PolySearchForward, b); }
	bool GetPolySearchForward() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_PolySearchForward); }
	void SetReEvaluateInfluenceSpheres(bool b) { m_FollowNavMeshFlags.Set(CFollowNavMeshFlags::FNM_ReEvaluateInfluenceSpheres, b); }
	bool GetReEvaluateInfluenceSpheres() const { return m_FollowNavMeshFlags.IsSet(CFollowNavMeshFlags::FNM_ReEvaluateInfluenceSpheres); }

protected:

	FSM_Return StateWaitingBetweenSearchAttempts_OnEnter(CPed * pPed);
	FSM_Return StateWaitingBetweenSearchAttempts_OnUpdate(CPed * pPed);
	FSM_Return StateWaitingBetweenRepeatSearches_OnEnter(CPed * pPed);
	FSM_Return StateWaitingBetweenRepeatSearches_OnUpdate(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnEnter(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnUpdate(CPed * pPed);
	FSM_Return StateAttemptTeleportToLeader_OnEnter(CPed* pPed);
	FSM_Return StateAttemptTeleportToLeader_OnUpdate(CPed* pPed);

	virtual bool OnSuccessfulRoute(CPed * pPed);
	virtual FSM_Return OnFailedRoute(CPed * pPed, const TPathResultInfo & pathResultInfo);

	bool ShouldTryToGetOnNavmesh( CPed * pPed );
	virtual bool ShouldKeepMovingWhilstWaitingForPath(CPed * pPed, bool & bMayUsePreviousTarget) const;
	virtual bool HandleNewTarget(CPed * pPed);

	virtual u32 GetInitialScanForObstructionsTime();

	bool ReEvaluateInfluenceSpheres(CPed * pPed) const;

	bool TeleportFollowerToLeaderReversePath(CPed* pPed, const Vector3& vPointToTeleportTo) const;
	bool CanPedTeleportToPlayer(CPed* pPed, bool bTryingToGetBackOnNavmesh) const;
private:

	CFollowNavMeshFlags m_FollowNavMeshFlags;
	CTaskNavBase::SpheresOfInfluenceBuilder * m_pInfluenceSphereBuilderFn;
	CCachedInfluenceSpheres * m_pCachedInfluenceSpheres;
	CTaskGameTimer m_ReEvaluateInfluenceSpheresTimer;
	TPathHandle m_iFollowerTeleportPathRequestHandle;
	float m_fPathCompletionRadius;
	float m_fFleeSafeDist;
	float m_fEntityRadiusOverride;
	s16 m_iNavMeshRouteResult;
	s16 m_iTimeToPauseBetweenSearchAttempts;
	s16 m_iTimeToPauseBetweenRepeatSearches;
};


class CNavParams
{
public:
	CNavParams()
	{
#if __DEV
		MakeNan(m_vTarget.x);
		MakeNan(m_vTarget.y);
		MakeNan(m_vTarget.z);
#endif
		m_fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
		m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
		m_iWarpTimeMs = 0;
		m_bFleeFromTarget = false;
		m_fCompletionRadius = CTaskMoveFollowNavMesh::ms_fDefaultCompletionRadius;
		m_fSlowDownDistance = CTaskMoveFollowNavMesh::ms_fSlowDownDistance;
		m_fFleeSafeDistance = 0.0f;
		m_fEntityRadiusOverride = -1.0f;
		m_pInfluenceSphereCallbackFn = NULL;
	}
	~CNavParams() { }

	Vector3 m_vTarget;
	float m_fTargetRadius;
	float m_fMoveBlendRatio;
	s32 m_iWarpTimeMs;
	bool m_bFleeFromTarget;
	float m_fCompletionRadius;
	float m_fSlowDownDistance;
	float m_fFleeSafeDistance;
	float m_fEntityRadiusOverride;
	CTaskNavBase::SpheresOfInfluenceBuilder * m_pInfluenceSphereCallbackFn;
};



class CTaskMoveWaitForNavMeshSpecialActionEvent : public CTaskMoveStandStill
{
public:
	static const int ms_iStandStillTime;

	CTaskMoveWaitForNavMeshSpecialActionEvent(const int iDuration=ms_iStandStillTime, const bool bForever=false);
	virtual ~CTaskMoveWaitForNavMeshSpecialActionEvent();

	virtual aiTask* Copy() const { return rage_new CTaskMoveWaitForNavMeshSpecialActionEvent(m_iDuration, m_bForever); }

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_WAIT_FOR_NAVMESH_SPECIAL_ACTION_EVENT;}

	virtual bool HasTarget() const { return false; }

#if !__FINAL
	virtual void Debug() const {};
	virtual atString GetName() const {return atString("WaitForNavMeshSpecialActionEvent");}
#endif
};


#define MAX_SLIDE_ON_ROUTE_TIME		1000

//**************************************************************************
//	CTaskUseClimbOnRoute
//	Task which performs a climb as part of a navmesh route task.
//	Created as an event-response task to a CEventClimbNavMeshOnRoute event.

class CTaskUseClimbOnRoute : public CTask
{
public:

	enum
	{
		State_Initial,
		State_SlidingToCoord,
		State_Climbing,
		State_GoingToPoint
	};

	static const s32 ms_iDefaultTimeOutMs;

	CTaskUseClimbOnRoute(const float fMoveBlendRatio, const Vector3 & vClimbPos, const float fClimbHeading, const Vector3 & vClimbTarget, CEntity * pEntityRouteIsOn, const s32 iWarpTimer, const Vector3 & vWarpTarget, bool bForceJump = false);
	~CTaskUseClimbOnRoute();

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual aiTask* Copy() const { return rage_new CTaskUseClimbOnRoute(m_fMoveBlendRatio, m_vClimbPos, m_fClimbHeading, m_vClimbTarget, m_pEntityRouteIsOn, m_iWarpTimer, m_vWarpTarget, m_bForceJump); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_USE_CLIMB_ON_ROUTE;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return ProcessPreFSM();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_SlidingToCoord : return "SlidingToCoord";
			case State_Climbing : return "Climbing";
			case State_GoingToPoint : return "GoingToPoint";
			default: Assert(0); return NULL;
		}
	}
	virtual void Debug() const;
#endif

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnEnter(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnUpdate(CPed * pPed);
	FSM_Return StateClimbing_OnEnter(CPed * pPed);
	FSM_Return StateClimbing_OnUpdate(CPed * pPed);
	FSM_Return StateGoingToPoint_OnEnter(CPed * pPed);
	FSM_Return StateGoingToPoint_OnUpdate(CPed * pPed);

private:

	Vector3 m_vClimbPos;
	Vector3 m_vClimbTarget;
	float m_fClimbHeading;
	RegdEnt m_pEntityRouteIsOn;
	float m_fMoveBlendRatio;
	CTaskGameTimer m_TimeOutTimer;
	s32 m_iWarpTimer;
	Vector3 m_vWarpTarget;
	s32 m_iFramesActive;
	TNavMeshAndPoly m_InitialNavMeshUnderfoot;
	bool m_bUseFasterClimb;
	bool m_bTryAvoidSlide;
	bool m_bAttemptedSecondClimb;
	bool m_bForceJump;
};

//**************************************************************************
//	CTaskUseDropDownOnRoute
//	Task which performs a drop-down as part of a navmesh route task.
//	Created as an event-response task to a CEventClimbNavMeshOnRoute event.

class CTaskUseDropDownOnRoute : public CTask
{
public:

	enum
	{
		State_Initial,
		State_SlidingToCoord,
		State_DroppingDown,
		State_DroppingDownWithTask
	};

	static const s32 ms_iDefaultTimeOutMs;

	CTaskUseDropDownOnRoute(const float fMoveBlendRatio, const Vector3 & vDropDownPos, const float fDropDownHeading, const Vector3 & vDropDownTarget, CEntity * pEntityRouteIsOn, const s32 iWarpTimer, const Vector3 & vWarpTarget, const bool bStepOffCarefully, const bool bUseDropDownTaskIfPossible);
	~CTaskUseDropDownOnRoute();

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual aiTask* Copy() const { return rage_new CTaskUseDropDownOnRoute(m_fMoveBlendRatio, m_vDropDownPos, m_fDropDownHeading, m_vDropDownTarget, m_pEntityRouteIsOn, m_iWarpTimer, m_vWarpTarget, m_bStepOffCarefully, m_bUseDropDownTaskIfPossible); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_USE_DROPDOWN_ON_ROUTE;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_SlidingToCoord; }

	FSM_Return ProcessPreFSM();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_SlidingToCoord : return "SlidingToCoord";
			case State_DroppingDown : return "DroppingDown";
			case State_DroppingDownWithTask : return "DroppingDownWithTask";
			default: Assert(0); return NULL;
		}
	}
	virtual void Debug() const;
#endif 

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnEnter(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnUpdate(CPed * pPed);
	FSM_Return StateDroppingDown_OnEnter(CPed * pPed);
	FSM_Return StateDroppingDown_OnUpdate(CPed * pPed);
	FSM_Return StateDroppingDownWithTask_OnEnter(CPed * pPed);
	FSM_Return StateDroppingDownWithTask_OnUpdate(CPed * pPed);

private:

	Vector3 m_vDropDownPos;
	Vector3 m_vDropDownTarget;
	Vector3 m_vWarpTarget;
	float m_fDropDownHeading;
	float m_fMoveBlendRatio;
	RegdEnt m_pEntityRouteIsOn;
	s32 m_iWarpTimer;
	CTaskGameTimer m_TimeOutTimer;
	bool m_bStepOffCarefully;
	bool m_bUseDropDownTaskIfPossible;
};

//**************************************************************************
//	CTaskUseLadderOnRoute
//	Task which performs a climb as part of a navmesh route task.
//	Created as an event-response task to a CEventClimbNavMeshOnRoute event.

class CTaskUseLadderOnRoute : public CTask
{
public:

	enum
	{
		State_Initial,
		State_SlidingToCoord,
		State_OrientatingToLadder,
		State_UsingLadder
	};

	static const s32 ms_iDefaultTimeOutMs;

	CTaskUseLadderOnRoute(const float fMoveBlendRatio, const Vector3 & vGetOnPos, const float fLadderHeading, CEntity * pEntityRouteIsOn=NULL);
	~CTaskUseLadderOnRoute();

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual aiTask* Copy() const { return rage_new CTaskUseLadderOnRoute(m_fMoveBlendRatio, m_vGetOnPos, m_fLadderHeading, m_pEntityRouteIsOn); }
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_USE_LADDER_ON_ROUTE;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return ProcessPreFSM();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_SlidingToCoord : return "SlidingToCoord";
			case State_OrientatingToLadder : return "OrientatingToLadder";
			case State_UsingLadder : return "UsingLadder";
			default: Assert(0); return NULL;
		}
	}
	virtual void Debug() const;
#endif 

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnEnter(CPed * pPed);
	FSM_Return StateSlidingToCoord_OnUpdate(CPed * pPed);
	FSM_Return StateOrientatingToLadder_OnEnter(CPed * pPed);
	FSM_Return StateOrientatingToLadder_OnUpdate(CPed * pPed);
	FSM_Return StateUsingLadder_OnEnter(CPed * pPed);
	FSM_Return StateUsingLadder_OnUpdate(CPed * pPed);

private:

	Vector3 m_vGetOnPos;
	float m_fLadderHeading;
	RegdEnt m_pEntityRouteIsOn;
	float m_fMoveBlendRatio;
	CTaskGameTimer m_TimeOutTimer;
	class CLadderClipSetRequestHelper* m_pLadderClipRequestHelper;	// Initiate from pool
};

//*********************************************************************************
//	CTaskMoveReturnToRoute
//	If a ped has strayed too far from their route during a navmesh/wander task,
//	this task can be used to return them to their route.
//	This task will be generated as a subtask of the navmesh/wander task when that
//	task receives a CEventBuildingCollision event (but *NOT* as a response task).
//*********************************************************************************

class CTaskMoveReturnToRoute : public CTaskMove
{
public:

	enum
	{
		State_Initial
	};

	static const float ms_fTargetRadius;

	CTaskMoveReturnToRoute(const float fMoveBlendRatio, const Vector3 & vTarget, const float fTargetRadius=ms_fTargetRadius);
	virtual ~CTaskMoveReturnToRoute();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskMoveReturnToRoute(m_fMoveBlendRatio, m_vTarget, m_fTargetRadius);
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_RETURN_TO_ROUTE; }

#if !__FINAL
	virtual void Debug() const;
	virtual atString GetName() const {return atString("ReturnToRoute");}
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 )
	{
		return "none";
	}
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed * pPed);

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const { return m_vTarget; }
	virtual Vector3 GetLookAheadTarget(void) const { return m_vTarget; }
	virtual float GetTargetRadius(void) const { return m_fTargetRadius; }

protected:

	Vector3 m_vTarget;
	float m_fTargetRadius;

	aiTask* CreateSubTask(const int iSubTaskType, CPed* pPed);
};



//*********************************************************************************
//	CTasMoveGoToSafePositionOnNavMesh
//	This task finds a safe position nearby to the ped, and attempts to get them
//	there using basic (non-navmesh) movement tasks.

class CTasMoveGoToSafePositionOnNavMesh : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_Waiting,
		State_Moving
	};

	static const float ms_fTargetRadius;
	static const s32 ms_iStandStillTime;
	static const s32 ms_iGiveUpTime;

	CTasMoveGoToSafePositionOnNavMesh(const float fMoveBlendRatio, bool bOnlyNonIsolatedPoly=true, s32 iTimeOutMs=ms_iGiveUpTime);

	void Init();

	virtual ~CTasMoveGoToSafePositionOnNavMesh();

	virtual aiTask* Copy() const
	{
		return rage_new CTasMoveGoToSafePositionOnNavMesh(m_fMoveBlendRatio, m_bOnlyNonIsolatedPoly, ms_iGiveUpTime);
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GOTO_SAFE_POSITION_ON_NAVMESH; }

#if !__FINAL
	virtual void Debug() const;
	virtual atString GetName() const { return atString("GoToSafePosOnNavMesh"); }
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_Waiting: return "Waiting";
		case State_Moving: return "Moving";
		}
		Assert(false);
		return NULL;
	}
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed * pPed);
	FSM_Return Waiting_OnUpdate();
	FSM_Return Moving_OnUpdate(CPed * pPed);

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget() const
	{
		return m_vSafePos;
	}
	virtual Vector3 GetLookAheadTarget() const
	{
		return GetTarget();
	}

	virtual float GetTargetRadius() const { return ms_fTargetRadius; }

protected:

	Vector3 m_vSafePos;
	s32 m_iTimeOutMs;
	CTaskGameTimer m_TimeOutTimer;

	u32 m_bOnlyNonIsolatedPoly	:1;
	u32 m_bFoundSafePos			:1;

	aiTask* CreateSubTask(const int iSubTaskType, CPed* pPed);
};

#endif
