#ifndef TASK_GO_TO_H
#define TASK_GO_TO_H

//Game headers.
#include "Control/Route.h"
#include "Pathserver/PathServer.h"
#include "Peds/PedEventScanner.h"
#include "Task/Movement/TaskIdle.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/Tuning.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
 

// CLASS : CTaskMoveFollowPointRoute
// PURPOSE : Follow a series of waypoints without navigation

class CTaskMoveFollowPointRoute : public CTaskMove
{
public:

    static const float ms_fTargetRadius;
    static const float ms_fSlowDownDistance;
    static CPointRoute ms_pointRoute;

	enum
	{
		State_Initial,
		State_FollowingRoute
	};

	enum
	{
		TICKET_SINGLE=0,
		TICKET_RETURN,
		TICKET_SEASON,
		TICKET_LOOP
	};

	CTaskMoveFollowPointRoute(
		const float fMoveBlendRatio,
		const CPointRoute& route, 
		const int iMode=TICKET_SINGLE, 
		const float fTargetRadius=ms_fTargetRadius,
		const float fSlowDownDistance=ms_fSlowDownDistance, 
		const bool bMustOvershootTarget=false,
		const bool bUseBlending=true,
		const bool bStandStillAfterMove=true,
		const int iStartingProgress=0,
		const float fWaypointRadius=-1.0f
	); 
    ~CTaskMoveFollowPointRoute();

	void Init(const CPointRoute& route,float fTargetRadius, float fSlowDownDistance, float m_fWaypointRadius, int iStartingProgress);

	virtual aiTask* Copy() const {
		CTaskMoveFollowPointRoute * pTask = rage_new CTaskMoveFollowPointRoute(
			m_fMoveBlendRatio,
			*m_pRoute,
			m_iMode,
			m_fTargetRadius,
			m_fSlowDownDistance,
			m_bMustOvershootTarget,
			m_bUseBlending,
			m_bStandStillAfterMove,
			m_iProgress,
			m_fWaypointRadius
		);
		pTask->SetUseRunstopInsteadOfSlowingToWalk( GetUseRunstopInsteadOfSlowingToWalk() );
		return pTask;
	}
			
    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE;}

#if !__FINAL
    virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_FollowingRoute: return "FollowingRoute";
		}
		return NULL;
	}
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed* pPed);
	FSM_Return FollowingRoute_OnEnter(CPed* pPed);
	FSM_Return FollowingRoute_OnUpdate(CPed* pPed);

    void SetRoute(const CPointRoute& route, const float fTargetRadius=ms_fTargetRadius, const float fSlowDownDistance=ms_fSlowDownDistance, const bool bForce=false);

	// JB : These added to allow wander tasks to sync over a network
	inline int GetRouteSize(void) { return m_pRoute ? m_pRoute->GetSize() : 0; }
	inline int GetProgress(void) { return m_iProgress; }
	inline void SetProgress(s32 iProgress) { m_iProgress = iProgress; }
	inline const CPointRoute * GetRoute(void) { return m_pRoute; }

	float GetDistanceLeftOnRoute(const CPed * pPed) const;

	// Updates the status of the passed task so this tasks current status.
	// Used by control movement tasks to keep the stored version they have up to date, so that
	// in the case that this task is removed it can re-instate one at the same progress.
	void UpdateProgress( aiTask* pSameTask );

	// Get the current GOTO target on the route
	virtual bool HasTarget() const;
	virtual Vector3 GetTarget(s32 iProgress) const		{ return m_pRoute->Get(iProgress); }
	virtual Vector3 GetTarget(void) const					{ return m_pRoute->Get(m_iProgress); }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float   GetTargetRadius(void) const				{ return m_fTargetRadius; }
	void SetTargetRadius(const float fTargetRadius)			{ m_fTargetRadius = fTargetRadius; }
	void SetSlowDownDistance(const float fSlowDownDistance) { m_fSlowDownDistance = fSlowDownDistance; }

	// Return the closest point to the ped along their current route segment.  Returns false if not on a route.
	bool GetClosestPositionOnCurrentRouteSegment(const CPed * pPed, Vector3 & vClosestPos);

	inline void SetUseRunstopInsteadOfSlowingToWalk(const bool b) { m_bUseRunstopInsteadOfSlowingToWalk = b; }
	inline bool GetUseRunstopInsteadOfSlowingToWalk() const { return m_bUseRunstopInsteadOfSlowingToWalk; }

	inline void SetStopExactly(const bool b) { m_bStopExactly = b; }
	inline bool GetStopExactly() const { return m_bStopExactly; }

private:
    
    int m_iMode;
    int m_iRouteTraversals;

	// The radius used for each individual GoToPoint
	float m_fWaypointRadius;
	// The radius used for the final GoToPointAndStandStill
    float m_fTargetRadius;

    float m_fSlowDownDistance;
    CPointRoute* m_pRoute;
    int m_iProgress;
    
    // flags
    u32 m_bMustOvershootTarget					:1;
    u32 m_bNewRoute								:1;
    u32 m_bStandStillAfterMove					:1;    
    u32 m_bUseBlending							:1;
	u32 m_bUseRunstopInsteadOfSlowingToWalk		:1;
	u32 m_bStopExactly							:1;

    int GetSubTaskType();    
    aiTask* CreateSubTask(const int iTaskType, CPed* pPed);
	void ProcessSlowDownDistance(const CPed * pPed);

#if !__FINAL
    Vector3 m_vStartPos;
#endif    

};


// CLASS : CTaskGoToPointAnyMeans
// PURPOSE : Get to target position using whatever means possible -
// Task will navigate and commandeer vehicles where available

class CTaskGoToPointAnyMeans : public CTaskFSMClone
{
public:

	enum
	{
		State_Initial,
		State_EnteringVehicle,
		State_ExitingVehicle,
		State_VehicleTakeOff,
		State_NavigateUsingVehicle,	
		State_NavigateUsingNavMesh,
		State_WaitBetweenNavMeshTasks
	};

	enum
	{
		Flag_IgnoreVehicleHealth			=	BIT0,
		Flag_ConsiderAllNearbyVehicles		=	BIT1,
		Flag_ProperDriveableCheck			=	BIT2,
		Flag_RemainInVehicleAtDestination	=	BIT3,
		Flag_NeverAbandonVehicle			=	BIT4,
		Flag_NeverAbandonVehicleIfMoving	=	BIT5,
		Flag_UseAiTargetingForThreats		=	BIT6
	};

	static const float ms_fTargetRadius;
	static const float ms_fOnFootDistance;
	
	CTaskGoToPointAnyMeans(const float fMoveBlendRatio, const Vector3& vTarget, CVehicle* pTargetVehicle=NULL, const float fTargetRadius=ms_fTargetRadius, const int iDesiredCarModel=-1,
		const bool bUseLongRangeVehiclePathing=false, const s32 iDrivingFlags=DMode_StopForCars, const float fMaxRangeToShootTargets = -1.0f, const float fExtraVehToTargetDistToPreferVeh = 0.0f, const float fDriveStraightLineDistance=sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST, const u16 iFlags=0, float fOverrideCruiseSpeed = -1.0f, float fWarpTimerMS = -1.0f, float fTargetArriveDist = sVehicleMissionParams::DEFAULT_TARGET_REACHED_DIST);
	CTaskGoToPointAnyMeans(const CTaskGoToPointAnyMeans& other);
    ~CTaskGoToPointAnyMeans();
	
	virtual aiTask* Copy() const { return rage_new CTaskGoToPointAnyMeans(*this); }
				
    virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS; }

	// clone task implementation
	virtual CTaskInfo*		CreateQueriableState() const;
	virtual CTaskFSMClone*	CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*	CreateTaskForLocalPed(CPed* pPed);
	virtual FSM_Return		UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_EnteringVehicle : return "EnteringVehicle";
			case State_ExitingVehicle : return "ExitingVehicle";
			case State_NavigateUsingVehicle : return "NavigateUsingVehicle";
			case State_NavigateUsingNavMesh : return "NavigateUsingNavMesh";
			case State_WaitBetweenNavMeshTasks : return "WaitBetweenNavMeshTasks";
			case State_VehicleTakeOff : return "State_VehicleTakeOff";  
			default: Assert(0); return NULL;
		}
	}
#endif 
    
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }
    
    void SetTarget(const Vector3& vTarget) { m_vTarget = vTarget; } 

	inline void SetNavMeshCompletionRadius(const float fRadius) { m_fNavMeshCompletionRadius = fRadius; }
	inline float GetNavMeshCompletionRadius() const { return m_fNavMeshCompletionRadius; }
	inline void SetGoAsFarAsPossibleIfNavMeshNotLoaded(const bool b) { m_bGoAsFarAsPossibleIfNavMeshNotLoaded = b; }
	inline bool GetGoAsFarAsPossibleIfNavMeshNotLoaded() const { return m_bGoAsFarAsPossibleIfNavMeshNotLoaded; }
	inline void SetPreferToUsePavements(const bool b) { m_bPreferToUsePavements = b; }
	inline bool GetPreferToUsePavements() const { return m_bPreferToUsePavements; }

private:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateEnteringVehicle_OnEnter(CPed * pPed);
	FSM_Return StateEnteringVehicle_OnUpdate(CPed * pPed);
	FSM_Return StateExitingVehicle_OnEnter(CPed * pPed);
	FSM_Return StateExitingVehicle_OnUpdate(CPed * pPed);
	FSM_Return StateNavigateUsingVehicle_OnEnter(CPed * pPed);
	FSM_Return StateNavigateUsingVehicle_OnUpdate(CPed * pPed);

	FSM_Return StateVehicleTakeOff_OnEnter(CPed * pPed);
	FSM_Return StateVehicleTakeOff_OnUpdate(CPed * pPed);

	FSM_Return StateNavigateUsingNavMesh_OnEnter(CPed * pPed);
	FSM_Return StateNavigateUsingNavMesh_OnUpdate(CPed * pPed);
	FSM_Return StateWaitBetweenNavMeshTasks_OnEnter(CPed * pPed);
	FSM_Return StateWaitBetweenNavMeshTasks_OnUpdate(CPed * pPed);

	CPed* FindBestTargetPed();
	void FindAndRegisterThreats(CPed* pPed, CPedTargetting* pPedTargeting);
	void OnRespondedToTarget(CPed& rPed, CPed* pTarget);
	void UpdateFiringSubTask();
	
	bool IsVehicleCloseEnoughToTargetToUse(CVehicle* pVehicle, CPed* pPed) const;
	bool ShouldAbandonVehicle(const CPed* pPed) const;
	bool IsControlledVehicleUnableToGetToRoad() const;
	void WarpPedIfStuck(CPed* pPed);

	Vector3 m_vTarget;
	float m_fMoveBlendRatio;
	float m_fTargetRadius;
	float m_fNavMeshCompletionRadius;
	float m_fMaxRangeToShootTargets;
	float m_fTimeSinceLastClosestTargetSearch;
	float m_fTimeInTheAir;
	float m_fExtraVehToTargetDistToPreferVeh;
	float m_fDriveStraightLineDistance;
	float m_fWarpTimerMS;
	float m_fTimeStuckMS;
	RegdVeh m_pTargetVehicle;
	int m_iDesiredCarModel;
	s32 m_iDrivingFlags;
	float m_fTargetArriveDist;
	CTaskGameTimer m_forceMakeCarTimer;

	bool m_bGoAsFarAsPossibleIfNavMeshNotLoaded;
	bool m_bPreferToUsePavements;
	bool m_bUseLongRangeVehiclePathing;

	u16 m_iFlags;

	float m_fOverrideCruiseSpeed;
	float m_fOverrideCruiseSpeedForSync;
};


class CClonedGoToPointAnyMeansInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedGoToPointAnyMeansInfo();

	CClonedGoToPointAnyMeansInfo(s32 iState,
		const float fMoveBlendRatio, 
		const Vector3& vTarget, 
		const CVehicle* pTargetVehicle,
		const float fTargetRadius,
		const int iDesiredCarModel,
		const float m_fNavMeshCompletionRadius,
		const bool m_bGoAsFarAsPossibleIfNavMeshNotLoaded,
		const bool m_bPreferToUsePavements,
		const bool m_bUseLongRangeVehiclePathing,
		const s32 m_iDrivingFlags,
		const float m_fExtraVehToTargetDistToPreferVeh,
		const float m_fDriveStraightLineDistance,
		const u16 m_iFlags,
		const float fOverrideCruiseSpeed
		);

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);

	virtual bool    HasState() const		{ return true; }
	virtual u32		GetSizeOfState() const  { return datBitsNeeded<CTaskGoToPointAnyMeans::State_WaitBetweenNavMeshTasks>::COUNT;}
	static const char* GetStateName(s32)	{ return "FSM State";}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_GO_TO_POINT_ANY_MEANS; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GO_TO_COORD_ANY_MEANS; }

	void Serialise(CSyncDataBase& serialiser);

private:

	bool m_bHasTargetVehicle;
	bool m_bHasTargetPosition;
	bool m_bHasDesiredCarModel;
	Vector3 m_vTarget;
	float m_fMoveBlendRatio;
	float m_fTargetRadius;
	float m_fNavMeshCompletionRadius;
	float m_fExtraVehToTargetDistToPreferVeh;
	float m_fDriveStraightLineDistance;
	CSyncedEntity m_pTargetVehicle;
	int m_iDesiredCarModel;
	s32	m_iDrivingFlags;

	bool m_bGoAsFarAsPossibleIfNavMeshNotLoaded;
	bool m_bPreferToUsePavements;
	bool m_bUseLongRangeVehiclePathing;

	u16 m_iFlags;

	float m_fOverrideCruiseSpeed;
};


// CLASS : CTaskFollowPatrolRoute
// PURPOSE : Ped will patrol along the specified route

class CTaskFollowPatrolRoute : public CTask
{
public:

    static const float ms_fTargetRadius;
    static const float ms_fSlowDownDistance;
    static CPatrolRoute ms_patrolRoute;

	enum
	{
		State_Initial,
		State_FollowingPatrolRoute,
		State_NavMeshBackToRoute,
		State_PlayingClip
	};

	enum
	{
		TICKET_SINGLE = 0,
		TICKET_RETURN,
		TICKET_SEASON,
		TICKET_LOOP
	};
	
    CTaskFollowPatrolRoute(const float fMoveBlendRatio, const CPatrolRoute& route, const int iMode=TICKET_SINGLE, const float fTargetRadius=ms_fTargetRadius, const float fSlowDownDistance=ms_fSlowDownDistance); 
    ~CTaskFollowPatrolRoute();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskFollowPatrolRoute(m_fMoveBlendRatio,*m_pRoute,m_iMode,m_fTargetRadius,m_fSlowDownDistance);
	}

	virtual void DoAbort(const AbortPriority iPriority, const aiEvent * pEvent);
			
    virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_FOLLOW_PATROL_ROUTE;}
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

#if !__FINAL
    virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_FollowingPatrolRoute : return "FollowingPatrolRoute";
			case State_NavMeshBackToRoute : return "NavMeshBackToRoute";
			case State_PlayingClip : return "PlayingAnim";
			default: Assert(0); return NULL;
		}
	}
#endif 
    
    void SetRoute(const CPatrolRoute& route, const float fTargetRadius=ms_fTargetRadius, const float fSlowDownDistance=ms_fSlowDownDistance, const bool bForce=false);

private:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateFollowingPatrolRoute_OnEnter(CPed * pPed);
	FSM_Return StateFollowingPatrolRoute_OnUpdate(CPed * pPed);
	FSM_Return StateNavMeshBackToRoute_OnEnter(CPed * pPed);
	FSM_Return StateNavMeshBackToRoute_OnUpdate(CPed * pPed);
	FSM_Return StatePlayingClip_OnEnter(CPed * pPed);
	FSM_Return StatePlayingClip_OnUpdate(CPed * pPed);

    int   m_iMode;
    int   m_iRouteTraversals;
    float m_fMoveBlendRatio;
    int   m_iProgress;
    
    float m_fTargetRadius;
    float m_fSlowDownDistance;
    CPatrolRoute* m_pRoute;
    
    bool m_bNewRoute;
    bool m_bWasAborted;
    
    // the position the ped was at when this task was aborted (due to eg. investigate disturbance)
    // we will to a FollowNodeRoute to return to this point, before then doing CreateFirstSubTask()
    Vector3 m_PedPositionWhenAborted;
    
#if !__FINAL
    Vector3 m_vStartPos;
#endif    
    int GetSubTaskType();    
    aiTask* CreateSubTask(const int iTaskType);
};


// CLASS : CTaskComplexGetOutOfWater
// PURPOSE : Will attempt to get the ped out of the water to closest position on land

class CTaskGetOutOfWater : public CTask
{
public:

	static const int ms_iMaxNumAttempts;

	enum
	{
		State_Initial,
		State_Surface,
		State_SearchForExitPoint,
		State_NavigateToExitPoint,
		State_NavigateToGroupLeader,
		State_WaitAfterFailedSearch,
		State_Finished
	};

	CTaskGetOutOfWater(const float fMaxSearchRadius, const float fMoveBlendRatio);
	virtual ~CTaskGetOutOfWater();

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_GET_OUT_OF_WATER;}
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual aiTask* Copy() const
	{
		CTaskGetOutOfWater * pCopy = rage_new CTaskGetOutOfWater(m_fMaxSearchRadius, m_fMoveBlendRatio);
		return pCopy;
	}
#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	FSM_Return StateInitial_OnEnter();
	FSM_Return StateInitial_OnUpdate();
	FSM_Return StateSurface_OnEnter();
	FSM_Return StateSurface_OnUpdate();
	FSM_Return StateSearchForExitPoint_OnEnter();
	FSM_Return StateSearchForExitPoint_OnUpdate();
	FSM_Return StateNavigateToExitPoint_OnEnter();
	FSM_Return StateNavigateToExitPoint_OnUpdate();
	FSM_Return StateNavigateToGroupLeader_OnEnter();
	FSM_Return StateNavigateToGroupLeader_OnUpdate();
	FSM_Return StateWaitAfterFailedSearch_OnEnter();
	FSM_Return StateWaitAfterFailedSearch_OnUpdate();
	FSM_Return StateFinished_OnEnter();
	FSM_Return StateFinished_OnUpdate();

protected:

	Vector3 m_vClosestPositionOnLand;
	TPathHandle m_hFloodFill;
	float m_fMaxSearchRadius;
	float m_fMoveBlendRatio;
//	int m_iNumAttempts;

};

/*
class CTaskComplexGetOutOfWater : public CTaskComplex
{
public:

	static const int ms_iMaxNumAttempts;

	enum EGetOutMode
	{
		ENone,
		ETryToReachLeader,
		EGetOutAsQuicklyAsPossible
	};

	CTaskComplexGetOutOfWater(const float fMaxSearchRadius, const float fMoveBlendRatio);
	~CTaskComplexGetOutOfWater();

	virtual aiTask* Copy() const
	{
		CTaskComplexGetOutOfWater * pClone = rage_new CTaskComplexGetOutOfWater(m_fMaxSearchRadius, m_fMoveBlendRatio);
		return pClone;
	}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_COMPLEX_GET_OUT_OF_WATER;}

#if !__FINAL
	virtual void Debug() const;
#endif 

	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	virtual aiTask* CreateNextSubTask( CPed* pPed);
	virtual aiTask* CreateFirstSubTask( CPed* pPed);
	virtual aiTask* ControlSubTask( CPed* pPed);
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

private:

	aiTask* CreateSubTask(CPed* pPed, const int iTaskType);

	Vector3 m_vClosestPositionOnLand;
	TPathHandle m_hFloodFill;
	float m_fMaxSearchRadius;
	float m_fMoveBlendRatio;
	int m_iNumAttempts;
	bool m_bFoundPosition;
	EGetOutMode m_eGetOutMode;
};
*/



// CLASS : CTaskMoveAroundCoverPoints
// PURPOSE : Movement task for walking around cover points

class CTaskMoveAroundCoverPoints : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_MovingToCoverPoint
	};

	typedef enum  {LEFT_LINK, RIGHT_LINK, NO_LINK } LinkedNode;
	typedef enum  {LEFT_ONLY, RIGHT_ONLY, LEFT_OR_RIGHT} Direction;
	CTaskMoveAroundCoverPoints( Direction eDirection, const Vector3& vLocalCoverPos, LinkedNode iLinkedNode, const bool bForcePedTowardsCover, const bool bForceSingleUpdateOfPedPos, const Vector3* pvDirectionToTarget = NULL);
	~CTaskMoveAroundCoverPoints();

	virtual aiTask* Copy() const {return rage_new CTaskMoveAroundCoverPoints(m_eDirection, m_vLocalTargetPosition, m_iLinkedNode, m_bForcePedTowardsCover, m_bForceSingleUpdateOfPedOffset, &m_vDirectionToTarget );}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_AROUND_COVERPOINTS;}
	static bool CalculateLocalTargetPosition( CPed* pPed, Vector3& vLocalTargetPosition, s32 iDesiredRotationalMovement, LinkedNode& iLinkedNode, const bool bForcePosition = false  );

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial: return "Initial";
		case State_MovingToCoverPoint: return "MovingToCoverPoint";
		}
		return NULL;
	}
#endif

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const;

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Intitial_OnUpdate(CPed * pPed);
	FSM_Return MovingToCoverPoint_OnEnter(CPed * pPed);
	FSM_Return MovingToCoverPoint_OnUpdate(CPed * pPed);

private:

	bool CalculateTargetPosition( CPed* pPed );
	s32 CalculateDesiredDirection( CPed* pPed );

	Vector3 m_vTarget;
	Vector3 m_vLocalTargetPosition;
	Vector3 m_vDirectionToTarget;
	Direction m_eDirection;
	bool m_bForcePedTowardsCover;
	bool m_bForceSingleUpdateOfPedOffset;
	LinkedNode m_iLinkedNode;
};







// CLASS : CTaskMoveCrowdAroundLocation
// PURPOSE : Movement task which instructs ped to approach & stand around a location at
// a given radius.  Ped will be aware of other peds doing the same task nearby, and will
// attempt to maintain an even spacing with them.

class CTaskMoveCrowdAroundLocation : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_MovingToCrowdLocation,
		State_CrowdingAroundLocation
	};

	CTaskMoveCrowdAroundLocation(const Vector3 & vLocation, const float fRadius=ms_fDefaultRadius, const float fSpacing=ms_fDefaultSpacing);
	~CTaskMoveCrowdAroundLocation();

	static const float ms_fDefaultRadius;
	static const float ms_fDefaultSpacing;
	static const float ms_fRequiredDistFromTarget;
	static const float ms_fTargetRadiusForCloseNavMeshTask;
	static const float ms_fGoToPointDist;
	static const float ms_fLooseCrowdAroundHeadingThresholdDegrees;

	virtual aiTask* Copy() const
	{
		CTaskMoveCrowdAroundLocation * pClone = rage_new CTaskMoveCrowdAroundLocation(m_vLocation, m_fRadius);
		pClone->SetUseLineTestsToStopPedWalkingOverEdge(GetUseLineTestsToStopPedWalkingOverEdge());
		return pClone;
	}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_CROWD_AROUND_LOCATION;}

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_MovingToCrowdLocation: return "MovingToCrowdLocation";
			case State_CrowdingAroundLocation: return "CrowdingAroundLocation";
		}
		return NULL;
	}
#endif

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const;
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius(void) const;
	
	virtual void SetTarget( const CPed* pPed, Vec3V_In vTarget, const bool bForce=false);
	virtual void SetTargetRadius	( const float fTargetRadius );

	// Get the position that this ped is standing - will be m_fRadius away from the m_vLocation
	inline Vector3 GetCrowdRoundPos() const { return m_vCrowdRoundPos; }
	// Set the centre point for the crowd.  If the location of the crowd moves, this needs to be updated.
	inline void SetCrowdLocation(const Vector3 & vPos) { m_vLocation = vPos; }
	inline float GetCrowdRadius() { return m_fRadius; }
	void CalcCrowdRoundPosition(const CPed * pPed);

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed* pPed);
	FSM_Return MovingToCrowdLocation_OnEnter(CPed* pPed);
	FSM_Return MovingToCrowdLocation_OnUpdate(CPed* pPed);
	FSM_Return CrowdingAroundLocation_OnEnter(CPed* pPed);
	FSM_Return CrowdingAroundLocation_OnUpdate(CPed* pPed);

	CTask * CreateSubTask(const int iSubTaskType, CPed* pPed);

	bool IsPedInVicinityOfCrowd(const CPed * pPed);
	bool ShouldUpdatedDesiredHeading() const;

	void SetUseLineTestsToStopPedWalkingOverEdge(const bool b) { m_bUseLineTestsToStopPedWalkingOverEdge = b; }
	bool GetUseLineTestsToStopPedWalkingOverEdge() const { return m_bUseLineTestsToStopPedWalkingOverEdge; }
	void SetRemainingWithinCrowdRadiusIsAcceptable(const bool b) { m_bRemainingWithinCrowdRadiusIsAcceptable = b; }
	bool GetRemainingWithinCrowdRadiusIsAcceptable() const { return m_bRemainingWithinCrowdRadiusIsAcceptable; }

private:

	void AdjustCrowdRndPosForOtherPeds(const CPed * pPed);

	// The location of the centre of the crowd
	Vector3 m_vLocation;
	// The position this ped should take around the edge of the crowd
	Vector3 m_vCrowdRoundPos;
	// Where ped was when they hit something
	Vector3 m_vLocationWhenWalkedIntoSomething;
	// The radius (distance from m_vLocation) at which the crowd peds should stand
	float m_fRadius;

	// Optional ability to stop peds walking over edges (expensive!)
	bool m_bUseLineTestsToStopPedWalkingOverEdge;
	// Variables for handling standing still when we've walked into something
	bool m_bWalkedIntoSomething;
	// If the crowd location changes, don't move the ped unless they are outside of this radius.
	// Normally they will just try to stay close to their own crowd-around position.
	bool m_bRemainingWithinCrowdRadiusIsAcceptable;
};



// CLASS : CTaskGoToScenario
// PURPOSE : Wrapper for go to behavior that we want consistent when a
// task is telling an actor to move to a scenario point

class CTaskGoToScenario : public CTask
{
public:

	// Tuning
	struct Tunables : CTuning
	{
		Tunables();

		float	m_ClosePointDistanceSquared;
		float	m_ClosePointCounterMax;

		float	m_HeadingDiffStartBlendDegrees;
		float	m_PositionDiffStartBlend;
		float	m_ExactStopTargetRadius;

		float	m_PreferNearWaterSurfaceArrivalRadius;

		float	m_TimeBetweenBrokenPointChecks;

		PAR_PARSABLE;
	};


	// FSM States
	enum
	{
		State_GoToPosition = 0,
		State_SlideToCoord,
		State_Failed,
		State_Finish
	};

	struct TaskSettings
	{
		enum eTaskFlag
		{
			IgnoreSlideToPosition,
			UsesEntryClip,
			IgnoreStopHeading,
			eTaskFlagCount
		};
		TaskSettings( const Vector3 & vTarget, const float fHeading ) : 
						m_vTarget( vTarget ),
						m_fHeading( fHeading ),
						m_fMoveBlendRatio( MOVEBLENDRATIO_WALK ),
						m_clipDict( -1 ),
						m_ClipHash( atHashString::Null() ),
						m_GoToScenarioFlags(0)	{}

		float			GetHeading() const							{ return m_fHeading; }
		void			SetHeading( const float heading )			{ m_fHeading = heading; }
		float			GetMoveBlendRatio() const					{ return m_fMoveBlendRatio; }
		void			SetMoveBlendRatio(float mbr)				{ m_fMoveBlendRatio = mbr; }
		const Vector3 &	GetTarget() const							{ return m_vTarget; }
		void			SetTarget( const Vector3 & newTargetPos )	{ m_vTarget = newTargetPos; }
		u32				GetClipHash() const							{ return m_ClipHash; }
		void			SetClipHash( u32 clipHash )					{ m_ClipHash = clipHash; }
		s32				GetClipDict() const							{ return m_clipDict; }
		void			SetClipDict( s32 dict )						{ m_clipDict = dict; }

		bool			IsSet( eTaskFlag flag ) const				{ return m_GoToScenarioFlags.IsSet( flag ); }
		void			Set( eTaskFlag flag, bool bState )			{ m_GoToScenarioFlags.Set( flag, bState ); }

	private:
		Vector3 		m_vTarget;
		float			m_fHeading;
		float			m_fMoveBlendRatio;
		s32				m_clipDict;
		u32				m_ClipHash;
		atFixedBitSet<eTaskFlagCount>		m_GoToScenarioFlags;
	};


	CTaskGoToScenario( const TaskSettings & settings );
	~CTaskGoToScenario();

	virtual void CleanUp();

	// Task required
	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_GO_TO_SCENARIO; }
	virtual s32 GetDefaultStateAfterAbort() const { return State_GoToPosition; }

#if !__FINAL
	virtual void				Debug() const;
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	bool						GetHasFailed()				  { return GetState() == State_Failed; }
	bool						CanBlendOut() const;
	
	TaskSettings&				GetSettings()				{ return m_Settings; }

	void SetAdditionalTaskDuringApproach(const CTask* task);

	void SetNewTarget(const Vector3& vPosition, float fHeading);

private:

	// State machine update functions
	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Locale state functions
	void						GoToPosition_OnEnter();
	FSM_Return					GoToPosition_OnUpdate();
	void						SlideToCoord_OnEnter();
	FSM_Return					SlideToCoord_OnUpdate();

	// Local member functions
	bool						CalculateGoToPositionAndHeading();
	bool						IsClose() const;

	// Member variables
	float						m_fCloseTimer;		// Timer to tick while "close" to the target.
	float						m_fBrokenPointTimer; // Timer to tick for checking for broken points.
	TaskSettings				m_Settings;			// Task settings
	bool						m_bUpdateTarget;	// Tells the task when it needs to update the target position
	bool						m_bEntryClipLoaded; // Tells the task that the entry clip has been loaded
	
	CPlayAttachedClipHelper		m_PlayAttachedClipHelper;
	CClipRequestHelper			m_ClipRequestHelper;
	CTaskGameTimer				m_WaitTimer;		// Use this to wait in position at the scenario trying to stream the clip before failing.

	RegdConstAITask				m_pAdditionalTaskDuringApproach;

private:

	// Instance of tunable task params
	static Tunables sm_Tunables;
};


#endif	// TASK_GO_TO_H

