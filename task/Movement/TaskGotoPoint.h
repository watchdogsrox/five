#ifndef TASK_GOTO_POINT_H
#define TASK_GOTO_POINT_H

#include "Peds/PedEventScanner.h"
#include "scene/RegdRefTypes.h"
#include "Task/Movement/TaskIdle.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"
#include "Task/System/TaskTypes.h" 

/////////////////
//Go to point
/////////////////

#define DEFAULT_NAVMESH_FINAL_HEADING				40000.0f // keep in sync with commands_task.sch
#define __ALWAYS_STOP_EXACTLY						1
 
#if !__FINAL
struct TSeparationDebug
{
	Vector3 m_vCurrentTarget;
	Vector3 m_vToTarget;
	Vector3 m_vSeparation;
	Vector3 m_vObjSeparation;
	Vector3 m_vPullToRoute;
	Vector2 m_vImpulses[CEntityScanner::MAX_NUM_ENTITIES];
	s32 m_iNumNearbyPeds;
	Vector2 m_vObjImpulses[CEntityScanner::MAX_NUM_ENTITIES];
	s32 m_iNumNearbyObjects;
	u32 m_bClippedToObject	:1;
};
#endif

struct sAvoidanceSettings
{
	sAvoidanceSettings();
	
	float m_fMaxAvoidanceTValue;
	float m_fDistanceFromPathToEnableObstacleAvoidance;	
	float m_fDistanceFromPathToEnableVehicleAvoidance;
	float m_fDistanceFromPathToEnableNavMeshAvoidance;
	float m_fDistanceFromPathTolerance;
	u32 m_bAllowNavmeshAvoidance:1;
};

// CLASS : CTaskMoveGoToPoint
// PURPOSE : Movement task which takes a ped directly to a coordinate with no navigation
// Performs local steering behaviour versus other peds & objects

class CTaskMoveGoToPoint : public CTaskMoveBase
{
public:

	static const float ms_fTargetRadius;

	static float ms_fTargetHeightDelta;
	static const float ms_fLineTestPosDeltaSqr;
	static const float ms_fTestAheadDistance;
	static bank_bool ms_bPerformLocalAvoidance;
	

	CTaskMoveGoToPoint(const float fMoveBlend, const Vector3& vTarget, const float fTargetRadius=ms_fTargetRadius, const bool bSlowSmoothlyApproachTarget=false);
	virtual ~CTaskMoveGoToPoint();

	void Init();

	virtual aiTask* Copy() const
	{
		CTaskMoveGoToPoint * pClone = rage_new CTaskMoveGoToPoint(
			m_fMoveBlendRatio,
			m_vTarget,
			m_fTargetRadius,
			m_bSlowSmoothlyApproachTarget
		);
		pClone->SetTargetHeightDelta(m_fTargetHeightDelta);
		pClone->SetUseLineTestForDrops(GetUseLineTestForDrops());
		pClone->SetAvoidanceSettings(m_AvoidanceSettings);
		pClone->SetDontExpandHeightDeltaUnderwater(m_bDontExpandHeightDeltaInWater);
		return pClone;
	}
	      
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_GO_TO_POINT;}

#if !__FINAL
	virtual void Debug() const;

	TSeparationDebug * m_pSeparationDebug;
#endif

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const {return m_vTarget;}
	virtual Vector3 GetLookAheadTarget(void) const {return m_vLookAheadTarget;}
	virtual float GetTargetRadius() const {return m_fTargetRadius;}

	virtual bool ProcessMove(CPed* pPed);

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	inline u32 GetTargetSideFlags() { return m_iTargetSideFlags; }

	bool CheckHasAchievedTarget(const Vector3 & vTarget, CPed * pPed);
	bool CheckHasAchievedDetourTarget(CPed * pPed);

#if __ASSERT
	static void AssertIsValidGoToTarget(const Vector3 & vPos);
#endif

	virtual void SetTarget(const CPed * pPed, Vec3V_In vTarget, const bool bForce=false);
	virtual void SetLookAheadTarget(const CPed * pPed, const Vector3& vTarget, const bool bForce=false);
	virtual void SetTargetRadius(const float fTargetRadius );

	inline void SetTargetDirect(Vec3V_In vTarget);
	inline void SetTargetHeightDelta(const float fDelta) { m_fTargetHeightDelta = fDelta; }
	inline float GetTargetHeightDelta() const { return m_fTargetHeightDelta; }
	

	inline void SetAvoidanceSettings(const sAvoidanceSettings& in_Settings) { m_AvoidanceSettings = in_Settings; }
	inline void SetMaxAvoidanceTValue(float in_MaxTValue) { m_AvoidanceSettings.m_fMaxAvoidanceTValue = in_MaxTValue; }
	inline void SetMaxAvoidanceTValue(ScalarV_In in_MaxTValueV) { StoreScalar32FromScalarV(m_AvoidanceSettings.m_fMaxAvoidanceTValue, in_MaxTValueV); }
	inline void SetAllowNavmeshAvoidance(bool in_AllowNavMeshAvoidance ) { m_AvoidanceSettings.m_bAllowNavmeshAvoidance = in_AllowNavMeshAvoidance; }

	inline void SetDistanceFromPathToEnableObstacleAvoidance(float in_DistanceFromPathToEnableObstacleAvoidance ) { m_AvoidanceSettings.m_fDistanceFromPathToEnableObstacleAvoidance = in_DistanceFromPathToEnableObstacleAvoidance; }
	inline void SetDistanceFromPathToEnableVehicleAvoidance(float in_DistanceFromPathToEnableVehicleAvoidance ) { m_AvoidanceSettings.m_fDistanceFromPathToEnableVehicleAvoidance = in_DistanceFromPathToEnableVehicleAvoidance; }
	inline void SetDistanceFromPathToEnableNavMeshAvoidance(float in_DistanceFromPathToEnableNavMeshAvoidance ) { m_AvoidanceSettings.m_fDistanceFromPathToEnableNavMeshAvoidance = in_DistanceFromPathToEnableNavMeshAvoidance; }
	inline void SetDistanceFromPathTolerance(float in_DistanceFromPathTolerance ) { m_AvoidanceSettings.m_fDistanceFromPathTolerance = in_DistanceFromPathTolerance; }
	

	inline bool WasTaskSuccessful(void) { return m_bSuccessful; }
	inline void SetAvoidPeds(bool bVal) { m_bAvoidPeds = bVal; }

	/*
    inline void SetMustOverShootTarget(const bool b) { m_bMustOverShootTarget = b; }
    inline bool GetMustOverShootTarget(void) { return m_bMustOverShootTarget; }
    */

    inline void SetDontCompleteThisFrame(void) { m_bDontCompleteThisFrame = true; }

	inline bool HasDoneProcessMove(void) { return m_bHasDoneProcessMove; }

	inline void SetUseLineTestForDrops(const bool b) { m_bUseLineTestForDrops = b; }
	inline bool GetUseLineTestForDrops() const { return m_bUseLineTestForDrops; }

	inline void SetExpandHeightDeltaUnderwater(const bool b) { m_bExpandHeightDeltaInWater = b; }
	inline void SetDontExpandHeightDeltaUnderwater(const bool b) { m_bDontExpandHeightDeltaInWater = b; }

	inline void SetDontAvoidEntity(const CEntity* pEntity) { m_pDontAvoidEntity = pEntity; }

	bool ProcessLineTestUnderTarget(CPed * pPed, const Vector3 & vTarget);

	static bank_float ms_AngleRelToForwardAvoidanceWeight;
	static bank_float ms_TimeOfCollisionAvoidanceWeight;
	static bank_float ms_DistanceFromPathAvoidanceWeight;
	static bank_float ms_AngleRelToDesiredAvoidanceWeight;
	static bank_float ms_MaxAvoidanceTValue;
	static bank_float ms_CollisionRadiusExtra;
	static bank_float ms_CollisionRadiusExtraTight;
	static bank_float ms_TangentRadiusExtra;
	static bank_float ms_TangentRadiusExtraTight;
	static bank_float ms_MaxDistanceFromPath;
	static bank_float ms_MinDistanceFromPath;
	static bank_float ms_NoTangentAvoidCollisionAngleMin;
	static bank_float ms_NoTangentAvoidCollisionAngleMax;
	static bank_float ms_LOSBlockedAvoidCollisionAngle;
	static bank_float ms_MaxAngleAvoidanceRelToDesired;
	static bank_float ms_MaxRelToTargetAngleFlee;

	static bank_float ms_fMaxCollisionTimeToEnableSlowdown;
	static bank_float ms_fMinCollisionSpeedToEnableSlowdown;
	static bank_float ms_fCollisionMinSpeedWalk;
	static bank_float ms_fCollisionMinSpeedStop;

	static bank_bool ms_bEnableSteeringAroundPedsBehindMe;
	static bank_bool ms_bEnableSteeringAroundPeds;
	static bank_bool ms_bEnableSteeringAroundObjects;
	static bank_bool ms_bEnableSteeringAroundVehicles;
	static bank_float ms_fAvoidanceScanAheadDistance;
	static bank_bool ms_bAllowStoppingForOthers;
	static bank_float ms_fPedLosAheadNavMeshLineProbeDistance;
	static bank_float ms_fPedAvoidanceNavMeshLineProbeDistanceMax;
	static bank_float ms_fPedAvoidanceNavMeshLineProbeDistanceMin;
	static bank_float ms_fPedAvoidanceNavMeshLineProbeAngle;

	static bank_float ms_fMinCorneringMBR;
	static bank_float ms_fAccelMBR;

	static bank_float ms_fMinMass;

	enum
	{
		SIDEFLAG_LEFT					= 0x01,
		SIDEFLAG_RIGHT					= 0x02,
		SIDEFLAG_FRONT					= 0x04,
		SIDEFLAG_BACK					= 0x08,

		SIDEFLAG_ALL					= (SIDEFLAG_LEFT+SIDEFLAG_RIGHT+SIDEFLAG_FRONT+SIDEFLAG_BACK)
	};


	//------------------------------------------------
	// Helper functions for use by movmeent systems
	static float ApproachMBR(const float fMBR, const float fTargetMBR, const float fSpeed, const float fTimeStep);
	static float CalculateMoveBlendRatioForCorner(const CPed * pPed, Vec3V_ConstRef vPosAhead, const float fLookAheadTime, const float fCurrentMBR, const float fMaxMBR, const float fAccelMBR, float fTimeStep, const CMoveBlendRatioSpeeds& moveSpeeds, const CMoveBlendRatioTurnRates& turnRates);


protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	void ModifyTargetForPathTolerance(Vector3 &io_targetPos, const CPed& in_Ped ) const;
	virtual void ComputeMoveBlend(CPed* pPed, const Vector3 & vTarget);
	virtual void ComputeHeadingAndPitch(CPed* pPed, const Vector3 & vTarget);
	virtual const Vector3 & GetCurrentTarget() { return m_vSeparationTarget; }



    Vector3 m_vTarget;			// The target position
	Vector3 m_vLookAheadTarget;

	Vector3 m_vSegmentStart;
	Vector3 m_vUnitSegment;		// (target-start).Normalize
	Vector3 m_vUnitPathSegment;  // (target - closestonpath).normalize
	Vector3 m_vLastLineTestPos;
	Vector3 m_vClosestPosOnSegment;

	// Separation vectors for avoiding peds & objects.
	// These are stored when calculated, since we may choose not to recalculate every frame.
	Vector3 m_vSeparationVec;


	// The current target, modified by the ped/object separation vector in order to implement avoidance.
	// Anyone querying this task's target will be interested in the m_vTarget above, but within this
	// task the movement system is driven by m_vSeparationTarget once it has been calculated every frame.
	Vector3 m_vSeparationTarget;

	RegdConstEnt m_pDontAvoidEntity;

	// settings describing how we should avoid stuff
	sAvoidanceSettings m_AvoidanceSettings;

	// The radius that the ped must get to the target for the task to complete
    float m_fTargetRadius;
	float m_fTargetHeightDelta;
	float m_fSegmentLength;

	CTaskGameTimer m_GenerateSepVecTimer;

    u32 m_bSlowSmoothlyApproachTarget	:1;
    //u32 m_bMustOverShootTarget			:1;

private:

	u32 m_bFirstTime							:1;
	u32 m_bAborting								:1;
    u32 m_bSuccessful							:1;
    u32 m_bDontCompleteThisFrame				:1;		// this sometimes gets set when tracking an entity
	u32 m_bHasDoneProcessMove					:1;		// we need to know whether ProcessMove() has been called yet..
    u32 m_bNewTarget							:1;
	u32 m_bDoingIK								:1;
	u32 m_bUseLineTestForDrops					:1;		// use a linetest to stop ped from walking over edges (expensive!!)
    u32 m_iTargetSideFlags						:4;		// these flags are used to determine when we've circled a target
	u32 m_bFoundGroundUnderLastLineTest			:1;
	u32 m_bHeadOnCollisionImminent				:1;
	u32 m_bExpandHeightDeltaInWater				:1;
	u32 m_bAvoidPeds							:1;
	u32 m_bDontExpandHeightDeltaInWater			:1;

    bool HasCircledTarget(CPed * pPed, const Vector3 & vTarget, u32 & iFlags);


    void SetUpHeadTrackingIK(CPed * pPed);
    void QuitIK(CPed * pPed); 
};


inline void CTaskMoveGoToPoint::SetTargetDirect(Vec3V_In vTarget)
{
#if __ASSERT
	CTaskMoveGoToPoint::AssertIsValidGoToTarget(RCC_VECTOR3(vTarget));
#endif

	m_vTarget = VEC3V_TO_VECTOR3(vTarget);
	m_iTargetSideFlags = 0;
	m_bNewTarget = true;
}


// CLASS : CTaskMoveGoToPointOnRoute
// PURPOSE : Movement task which handles locomotion to a point on a route, with awareness of the next target
// Performs slow-down approaching the target in order to take the corner without overshoot

class CTaskMoveGoToPointOnRoute : public CTaskMoveGoToPoint
{
public:

	static const float ms_fSlowDownDistance;
	static const float ms_fMinMoveBlendRatio;

	CTaskMoveGoToPointOnRoute(const float fMoveBlend, const Vector3& vTarget, const Vector3& vNextTarget, const bool bUseBlending, const float fTargetRadius=CTaskMoveGoToPoint::ms_fTargetRadius, const bool bSlowSmoothlyApproachTarget=false, const float fSlowDownDistance=ms_fSlowDownDistance);
	virtual ~CTaskMoveGoToPointOnRoute();

	virtual aiTask* Copy() const
	{
		CTaskMoveGoToPointOnRoute* pClone = rage_new CTaskMoveGoToPointOnRoute
		(
			m_fMoveBlendRatio,
			m_vTarget,
			m_vNextTarget,
			m_bUseBlending,
			m_fTargetRadius,
			m_bSlowSmoothlyApproachTarget,
			m_fSlowDownDistance
		);

		pClone->SetAvoidanceSettings(m_AvoidanceSettings);
		return pClone;
	}

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const { return m_vTarget; }
	      
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE;}

	virtual void SetMoveBlendRatio(const float fMoveBlendRatio, const bool bFlagAsChanged=true);

#if !__FINAL
	virtual void Debug() const;
#endif

protected:

	virtual void ComputeMoveBlend(CPed* pPed, const Vector3 & vTarget);

private:

	Vector3 m_vNextTarget;
	float m_fSlowDownDistance;
	bool m_bUseBlending;
};





// CLASS : CTaskMoveGoToPointAndStandStill
// PURPOSE : Movement tasks which goes directly to a target, and stands still
// Ensures that the ped stops exactly on the target position, taking into account movement clip stopping distance

class CTaskMoveGoToPointAndStandStill : public CTaskMove
{
public:

	enum eState
	{
		EGoingToPoint,
		EStopping,
		EStandingStill
	};

    static const float ms_fTargetRadius;
    static bank_float ms_fSlowDownDistance;
	static const int ms_iDefaultStandStillTime;
	static const int ms_iMaximumStopTime;
	static const float ms_fStopInsideRadiusRatio;

	CTaskMoveGoToPointAndStandStill(const float fMoveBlendRatio, const Vector3 & vTarget, const float fTargetRadius=ms_fTargetRadius, const float fSlowDownDistance=ms_fSlowDownDistance, const bool bMustOvershootTarget=false, const bool bStopExactly=__ALWAYS_STOP_EXACTLY, const float TargetHeading=DEFAULT_NAVMESH_FINAL_HEADING);
    virtual ~CTaskMoveGoToPointAndStandStill();

	virtual aiTask* Copy() const
	{
		CTaskMoveGoToPointAndStandStill * pClone = rage_new CTaskMoveGoToPointAndStandStill(
			m_fMoveBlendRatio,
			m_vTarget,
			m_fTargetRadius,
			m_fSlowDownDistance,
			m_bMustOverShootTarget,
			m_bStopExactly,
			m_fTargetHeading
		);
      		pClone->SetAvoidanceSettings(m_AvoidanceSettings);
		pClone->SetStandStillDuration(m_iStandStillDuration);
		pClone->SetTargetHeightDelta(m_fTargetHeightDelta);
		pClone->SetUseRunstopInsteadOfSlowingToWalk(m_bUseRunstopInsteadOfSlowingToWalk);
		pClone->m_fDistanceRemaining = m_fDistanceRemaining;
		pClone->m_bParentTaskOveridesDistanceRemaining = m_bParentTaskOveridesDistanceRemaining;
		return pClone;
	}
    virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL; }
	
#if !__FINAL
    virtual void Debug() const;
#endif

	//*******************************************
	// Virtual members inherited From CTask
	//*******************************************

	virtual s32 GetDefaultStateAfterAbort() const { return EGoingToPoint; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPostFSM();
	virtual void CleanUp();

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case EGoingToPoint: { return "Going To Point"; }
			case EStopping: { return "Stopping"; }
			case EStandingStill: { return "Standing Still"; }
		}
		Assert(false);
		return NULL;
	}
	virtual atString GetName() const
	{
		atString name = CTaskMove::GetName();
		switch(m_iExtractedVelOperation)
		{
		case ExtractedVel_DoNothing:         name += ":DoNothing"; break;
		case ExtractedVel_Zero:              name += ":Zero"; break;
		case ExtractedVel_ZeroXY:            name += ":ZeroXY"; break;
		case ExtractedVel_SlideToTarget:     name += ":SlideToTarget"; break;
		case ExtractedVel_OrientateAndScale: name += ":OrientateAndScale"; break;
		}
		return name;
	}
#endif

	virtual FSM_Return GoingToPoint_OnEnter(CPed * pPed);
	virtual FSM_Return GoingToPoint_OnUpdate(CPed * pPed);
	FSM_Return Stopping_OnEnter(CPed * pPed);
	FSM_Return Stopping_OnUpdate(CPed * pPed);
	FSM_Return Stopping_OnExit(CPed * pPed);
	FSM_Return StandingStill_OnEnter(CPed * pPed);
	FSM_Return StandingStill_OnUpdate(CPed * pPed);

	bool IsStoppedAtTargetPoint();

    virtual void SetTarget(const CPed *pPed, Vec3V_In vTarget, const bool bForce=false);
	virtual void SetLookAheadTarget(const CPed * pPed, const Vector3 & vTarget, const bool bForce=false);
	virtual void SetTargetRadius(const float fTargetRadius) 
	{
		if( m_fTargetRadius != fTargetRadius )
		{
			m_fTargetRadius = fTargetRadius;
		}
	}
	virtual void SetSlowDownDistance(const float fSlowDownDistance=ms_fSlowDownDistance) 
	{
		m_fSlowDownDistance = fSlowDownDistance;
	}
	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const { return m_vTarget; }
	virtual Vector3 GetLookAheadTarget(void) const {return m_vLookAheadTarget;}
	virtual float GetTargetRadius(void) const {return m_fTargetRadius;}
    
	inline void SetTargetHeightDelta(const float fDelta) { m_fTargetHeightDelta = fDelta; }
	inline float GetTargetHeightDelta() const { return m_fTargetHeightDelta; }
    inline void SetMustOverShootTarget(const bool b) { m_bMustOverShootTarget = b; }
    inline bool GetMustOverShootTarget(void) { return m_bMustOverShootTarget; }
	void SetTrackingEntity(bool b) { m_bTrackingEntity = b; }
	inline bool WasTaskSuccessful() const { return m_bWasSuccessful; }
	inline bool IsSlowingDown() const { return m_bIsSlowingDown; }
	inline float GetSlowingDownMoveBlendRatio() const { return m_fSlowingDownMoveBlendRatio; }
	inline float GetSlowDownDistance() const { return m_fSlowDownDistance; }
	inline void SetDontCompleteThisFrame(void) { m_bDontCompleteThisFrame = true; }
	inline void SetStandStillDuration(const s32 i) { m_iStandStillDuration = i; }
	inline s32 GetStandStillDuration() const { return m_iStandStillDuration; }
	inline void SetUseRunstopInsteadOfSlowingToWalk(const bool b) { m_bUseRunstopInsteadOfSlowingToWalk = b; }
	inline bool GetUseRunstopInsteadOfSlowingToWalk() const { return m_bUseRunstopInsteadOfSlowingToWalk; }
	inline void SetTargetHeading(const float f) { m_fTargetHeading = f; }
	inline float GetTargetHeading() const { return m_fTargetHeading; }
	inline bool GetIsStopping() const { return m_bIsStopping; }
	inline bool GetIsStopDueToInsideRadius() const { return m_bStopDueToInsideRadius; }
	inline bool GetStopExactly() const { return m_bStopExactly; }
	inline void SetStopExactly(const bool b) { m_bStopExactly = b; }
	inline void SetLenientAchieveHeadingForExactStop(bool bVal) { m_bLenientAchieveHeadingForExactStop = bVal; }
	bool CanStartIdle() const;

	// set the avoidance settings for use by taskgotopoint
	inline void SetAvoidanceSettings(const sAvoidanceSettings& in_Settings) { m_AvoidanceSettings = in_Settings; }
	inline void SetMaxAvoidanceTValue(float in_MaxTValue) { m_AvoidanceSettings.m_fMaxAvoidanceTValue = in_MaxTValue; }
	inline void SetMaxAvoidanceTValue(ScalarV_In in_MaxTValueV) { StoreScalar32FromScalarV(m_AvoidanceSettings.m_fMaxAvoidanceTValue, in_MaxTValueV); }
	inline void SetAllowNavmeshAvoidance(bool in_AllowNavMeshAvoidance ) { m_AvoidanceSettings.m_bAllowNavmeshAvoidance = in_AllowNavMeshAvoidance; }
	inline void SetDistanceFromPathToEnableObstacleAvoidance(float in_DistanceFromPathToEnableObstacleAvoidance ) { m_AvoidanceSettings.m_fDistanceFromPathToEnableObstacleAvoidance = in_DistanceFromPathToEnableObstacleAvoidance; }
	inline void SetDistanceFromPathToEnableVehicleAvoidance(float in_DistanceFromPathToEnableVehicleAvoidance ) { m_AvoidanceSettings.m_fDistanceFromPathToEnableVehicleAvoidance = in_DistanceFromPathToEnableVehicleAvoidance; }
	inline void SetDistanceFromPathToEnableNavMeshAvoidance(float in_DistanceFromPathToEnableNavMeshAvoidance ) { m_AvoidanceSettings.m_fDistanceFromPathToEnableNavMeshAvoidance = in_DistanceFromPathToEnableNavMeshAvoidance; }
	inline void SetDistanceFromPathTolerance(float in_DistanceFromPathTolerance ) { m_AvoidanceSettings.m_fDistanceFromPathTolerance = in_DistanceFromPathTolerance; }
	inline void SetAvoidPeds(bool bVal) { m_bAvoidPeds = bVal; }

	// Used to override the distance remaining calculation used internally to trigger slowing/walkstops
	// If this is not called, the task will calculate the distance itself by using the ped's position & the target position.
	// This function is only used by navigation tasks.
	void SetDistanceRemaining(const float fDist);
	inline void SetDistanceRemaining(ScalarV_In distV);
	void ResetDistanceRemaining(CPed* pPed);

    // Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

	float GetDistanceRemaining() const { return m_fDistanceRemaining; }
	inline bool GetIsRunStop() const { return m_bRunStop; }

	void RestartNextFrame() { m_bRestartNextFrame = true; }

protected:

	bool AreClipsStillMoving();

	void UpdateAngularVelocity(CPed* pPed, float fRate, float fTimeStep);
	bool SelectMoveBlendRatio(CTaskMoveGoToPoint * pGoToPoint, CPed * pPed, float fRunDistance, float fSprintDistance);
	void ProcessStoppingBehaviour(CPed * pPed);

	// settings describing how we should avoid stuff
	sAvoidanceSettings m_AvoidanceSettings;

	Vector3 m_vTarget;
	Vector3 m_vLookAheadTarget;
	Vector3 m_vTargetPlaneNormal;
	float m_fTargetRadius;
	float m_fTargetHeightDelta;
	float m_fSlowDownDistance;
	float m_fSlowingDownMoveBlendRatio;	// set by task when slowing
	float m_fInitialDistanceToTarget;
	float m_fDistanceRemaining;
	s32 m_iStandStillDuration;


	u32 m_bMustOverShootTarget						:1;
	u32 m_bStopExactly								:1;
	u32 m_bNewTarget								:1;
	u32 m_bTrackingEntity							:1;
	u32 m_bWasSuccessful							:1;
	u32 m_bIsSlowingDown							:1;
	u32 m_bIsStopping								:1;
	u32 m_bHasOvershotTarget						:1;
	u32 m_bUseRunstopInsteadOfSlowingToWalk			:1;
	u32 m_bAbortTryingToNailPoint					:1;
	u32 m_bStopClipNoLongerMoving					:1;
	u32 m_bTargetHeadingSpecified					:1;
	u32 m_bDontCompleteThisFrame					:1;
	u32 m_bJustSlideLastBit							:1;
	u32 m_bParentTaskOveridesDistanceRemaining		:1;
	u32 m_bAvoidPeds								:1;
	u32 m_bLenientAchieveHeadingForExactStop		:1;
	u32 m_bRunStop									:1;
	u32 m_bRestartNextFrame							:1;
	u32 m_bStopDueToInsideRadius					:1;

	enum
	{
		ExtractedVel_DoNothing,
		ExtractedVel_Zero,
		ExtractedVel_ZeroXY,
		ExtractedVel_SlideToTarget,
		ExtractedVel_OrientateAndScale
	};
	int m_iExtractedVelOperation;
	float m_fTimeSliding;
	float m_fTargetHeading;

    aiTask* CreateSubTask(const int iSubTaskType, CPed* pPed);
};

inline void CTaskMoveGoToPointAndStandStill::SetDistanceRemaining(ScalarV_In distV)
{
	m_bParentTaskOveridesDistanceRemaining = true;
	StoreScalar32FromScalarV(m_fDistanceRemaining, distV);
}

// CLASS: CTaskMoveGoToPointRelativeToEntityAndStandStill
// PURPOSE : Moves directly to a position relative to the given entity

class CTaskMoveGoToPointRelativeToEntityAndStandStill : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_MovingToEntity
	};

	static const float ms_fTargetRadius;

	CTaskMoveGoToPointRelativeToEntityAndStandStill(const CEntity * pEntity, const float fMoveBlendRatio, const Vector3 & vLocalSpaceTarget, const float fTargetRadius=ms_fTargetRadius, const s32 iMaxTime=20000);

	virtual ~CTaskMoveGoToPointRelativeToEntityAndStandStill();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskMoveGoToPointRelativeToEntityAndStandStill(
			m_pEntity,
			m_fMoveBlendRatio,
			m_vLocalSpaceTarget,
			m_fTargetRadius,
			m_iMaxTime
			);
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GOTO_POINT_RELATIVE_TO_ENTITY_AND_STAND_STILL; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial : return "Initial";
		case State_MovingToEntity : return "MovingToEntity";
		}
		return NULL;
	}
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate();
	FSM_Return MovingToEntity_OnUpdate(CPed * pPed);
	FSM_Return MovingToEntity_OnEnter(CPed * pPed);

	void SetTarget(const CPed* UNUSED_PARAM(pPed), Vec3V_In UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce)) 
	{
		Assert(0);
	}

	void SetTargetRadius(const float fTargetRadius) 
	{
		if( m_fTargetRadius!=fTargetRadius )
		{
			m_fTargetRadius=fTargetRadius;
		}
	}

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget() const
	{
		return m_vWorldSpaceTarget;
	}

	virtual Vector3 GetLookAheadTarget() const
	{
		return GetTarget();
	}

	virtual float GetTargetRadius(void) const { return m_fTargetRadius; }

protected:

	Vector3 m_vLocalSpaceTarget;
	Vector3 m_vWorldSpaceTarget;
	RegdConstEnt m_pEntity;
	float m_fTargetRadius;
	s32 m_iMaxTime;
	CTaskGameTimer m_GiveUpTimer;

	u32 m_bNewTarget				:1;

	inline bool UpdateTarget()
	{
		if(m_pEntity)
			m_pEntity->TransformIntoWorldSpace(m_vWorldSpaceTarget, m_vLocalSpaceTarget);
		return (m_pEntity.Get()!=NULL);
	}
};



// CLASS : CTaskMoveGoToPointStandStillAchieveHeading
// PURPOSE : Moves directly to given position, stops exactly and assumes the given heading

class CTaskMoveGoToPointStandStillAchieveHeading : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_MovingToPoint,
		State_AchievingHeading
	};

	static const float ms_fSlowDownDistance;

	CTaskMoveGoToPointStandStillAchieveHeading(
		const float fMoveBlendRatio, const Vector3 & vTarget, const Vector3 & vPositionToFace,
		const float fTargetRadius = CTaskMoveGoToPointAndStandStill::ms_fTargetRadius,
		const float fSlowDownDistance = CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance,
		const float fHeadingChangeRate = CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac,
		const float fHeadingTolerace = CTaskMoveAchieveHeading::ms_fHeadingTolerance,
		const float fAchieveHeadingTime = 0.0f);

	CTaskMoveGoToPointStandStillAchieveHeading(
		const float fMoveBlendRatio, const Vector3 & vTarget, const float fHeading,
		const float fTargetRadius = CTaskMoveGoToPointAndStandStill::ms_fTargetRadius,
		const float fSlowDownDistance = CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance,
		const float fHeadingChangeRate = CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac,
		const float fHeadingTolerace = CTaskMoveAchieveHeading::ms_fHeadingTolerance,
		const float fAchieveHeadingTime = 0.0f);

	virtual ~CTaskMoveGoToPointStandStillAchieveHeading();

	virtual aiTask* Copy() const
	{
		if(m_bUsingHeading)
		{
			return rage_new CTaskMoveGoToPointStandStillAchieveHeading(
				m_fMoveBlendRatio,
				m_vTarget,
				m_fHeading,
				m_fTargetRadius,
				m_fSlowDownDistance,
				m_fHeadingChangeRate,
				m_fHeadingTolerance,
				m_fAchieveHeadingTime
				);
		}
		else
		{
			return rage_new CTaskMoveGoToPointStandStillAchieveHeading(
				m_fMoveBlendRatio,
				m_vTarget,
				m_vPositionToFace,
				m_fTargetRadius,
				m_fSlowDownDistance,
				m_fHeadingChangeRate,
				m_fHeadingTolerance,
				m_fAchieveHeadingTime
				);
		}
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GOTO_POINT_STAND_STILL_ACHIEVE_HEADING; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
		case State_Initial : return "Initial";
		case State_MovingToPoint : return "MovingToPoint";
		case State_AchievingHeading : return "AchievingHeading";
		}
		return NULL;
	}
#endif

	FSM_Return Initial_OnUpdate(CPed* pPed);
	FSM_Return MovingToPoint_OnEnter(CPed* pPed);
	FSM_Return MovingToPoint_OnUpdate(CPed* pPed);
	FSM_Return AchieveHeading_OnEnter(CPed * pPed);
	FSM_Return AchieveHeading_OnUpdate(CPed * pPed);

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const { return m_vTarget; }
	virtual Vector3 GetLookAheadTarget(void) const { return GetTarget(); }
	virtual float GetTargetRadius(void) const { return m_fTargetRadius; }

	void SetTargetAndHeading(const Vector3& vTarget, const float fTargetRadius,  const float fHeading, const float fHeadingChangeRateFrac, const float fHeadingTolerance, const bool bForce);

protected:

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	aiTask* CreateSubTask(const CPed * pPed, const int iSubTaskType);

	Vector3 m_vTarget;
	Vector3 m_vPositionToFace;
	float m_fTargetRadius;
	float m_fHeading;
	float m_fSlowDownDistance;
	float m_fHeadingChangeRate;
	float m_fHeadingTolerance;
	float m_fAchieveHeadingTime;
	bool m_bUsingHeading;
	bool m_bNewTarget;
};

class CClonedGoToPointAndStandStillTimedInfo : public CSerialisedFSMTaskInfo
{
public:

    CClonedGoToPointAndStandStillTimedInfo(const float    moveBlendRatio,
                                           const Vector3 &targetPos,
                                           const int      time,
                                           const float    targetHeading);
    CClonedGoToPointAndStandStillTimedInfo();

    virtual CTask  *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_GOTO_POINT_AND_STAND_STILL_TIMED; }
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GO_STRAIGHT_TO_COORD; }

    void Serialise(CSyncDataBase& serialiser)
	{
        const unsigned SIZEOF_MOVE_BLEND_RATIO = 8;
        const unsigned SIZEOF_HEADING          = 8;
        const unsigned SIZEOF_TIME             = 16;

        SERIALISE_POSITION(serialiser, m_TargetPos, "Target Pos");

        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_MoveBlendRatio, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVE_BLEND_RATIO, "Move Blend Ratio");

        bool targetHeadingValid = m_TargetHeading < DEFAULT_NAVMESH_FINAL_HEADING;

        SERIALISE_BOOL(serialiser, targetHeadingValid);

        if(targetHeadingValid)
        {
            SERIALISE_PACKED_FLOAT(serialiser, m_TargetHeading, PI, SIZEOF_HEADING, "Target Heading");
        }
        else
        {
            m_TargetHeading = DEFAULT_NAVMESH_FINAL_HEADING;
        }

        SERIALISE_INTEGER(serialiser, m_Time, SIZEOF_TIME);
    }

private:

	float    m_MoveBlendRatio; // move blend ratio to use when navigating
    Vector3  m_TargetPos;      // target position to navigate to
    int      m_Time;           // time before ped is warped to target position
    float    m_TargetHeading;  // target heading when stopping
};

// CLASS : CTaskComplexGoToPointAndStandStillTimed
// PURPOSE : Move directly to a position, stops exactly and waits for a given time before completing
class CTaskComplexGoToPointAndStandStillTimed : public CTaskMoveGoToPointAndStandStill
{
public:

	static const int ms_iTime;

	CTaskComplexGoToPointAndStandStillTimed(const float fMoveBlendRatio, const Vector3& vTarget, const float fTargetRadius=ms_fTargetRadius, const float fSlowDownDistance=ms_fSlowDownDistance, const int iTime=ms_iTime, bool stopExactly = false, float targetHeading = DEFAULT_NAVMESH_FINAL_HEADING);
	~CTaskComplexGoToPointAndStandStillTimed();

	virtual aiTask*	Copy() const {
		return rage_new CTaskComplexGoToPointAndStandStillTimed(
			m_fMoveBlendRatio,
			m_vTarget,
			m_fTargetRadius,
			m_fSlowDownDistance,
			m_iTime,
			m_bStopExactly,
			m_fTargetHeading
			);
	}

	FSM_Return GoingToPoint_OnEnter( CPed * pPed );
	FSM_Return GoingToPoint_OnUpdate( CPed* pPed );

    // Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void	DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

private:

	int m_iTime;
	CTaskGameTimer m_timer;
};


#endif // TASK_GOTO_POINT_H

