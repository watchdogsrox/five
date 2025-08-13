#ifndef TASK_FOLLOW_WAYPOINT_RECORDING_H
#define TASK_FOLLOW_WAYPOINT_RECORDING_H

// Framework headers
#include "fwmaths\Vector.h"

// Game headers
#include "task\system\TaskMove.h"
#include "control\WaypointRecording.h"
#include "AI/AITarget.h"

class CPed;
class CMoveBlendRatioSpeeds;
class CTaskMoveGoToPoint;

//********************************************************************************
// TaskMoveFollowWaypointRecording
// Classes to allow a ped's movement to be recorded as a series of waypoints and
// associated data.  It can then be played back via a specialized task for that
// movement mode (skiing, on-foot, etc).


class CTaskFollowWaypointRecording : public CTask
{
	enum
	{
		State_Initial,
		State_NavmeshToInitialWaypoint,
		State_NavmeshBackToRoute,
		State_AchieveHeadingBeforeResuming,
		State_FollowingRoute,
		State_FollowingRouteAiming,
		State_FollowingRouteShooting,
		State_Pause,
		State_PauseAndFacePlayer,
		State_AchieveHeadingAtEnd,
		State_ExactStop
	};

public:

	enum
	{
		// Faces in direction of final waypoint at the end
		Flag_TurnToFaceWaypointHeadingAtEnd	= 0x01,
		// Uses the navmesh to get to the initial waypoint if necessary
		Flag_NavMeshToInitialPoint			= 0x02,
		// If the ped has left the route, uses the navmesh to get back to their current waypoint
		Flag_NavMeshIfLeftRoute				= 0x04,
		// Start (or if the ped is interrupted, then resume) route following from the closest route segment
		Flag_StartFromClosestPosition		= 0x08,
		// Use AI slowdown instead of the recorded values
		Flag_VehiclesUseAISlowdown			= 0x10,
		// Prevent this ped from steering around peds whilst running this task (helps them stay on route)
		Flag_DoNotRespondToCollisionEvents	= 0x20,
		// Prevent this ped from slowing for corners
		Flag_DontSlowForCorners				= 0x40,
		// Start the task initially aiming
		Flag_StartTaskInitiallyAiming		= 0x80,
		// Use the exact stop functionality
		Flag_UseExactStop					= 0x100,
		// Tighter turn settings
		Flag_UseTighterTurnSettings			= 0x200,
		// Allow steering around peds (ped/ped avoidance)
		Flag_AllowSteeringAroundPeds		= 0x400,
		// Suppress exact stops
		Flag_SuppressExactStop				= 0x800,
		// Dials up the amount that peds can slow for corners, helps issues with peds running into doorframes, etcetera
		Flag_SlowMoreForCorners				= 0x1000
	};

	static bank_float ms_fTargetRadius_OnFoot;
	static bank_float ms_fTargetRadius_Skiing;
	static bank_float ms_fTargetRadius_SurfacingFromUnderwater;
	static bank_bool ms_bForceJumpHeading;
	static bank_bool ms_bUseHelperForces;

	static bank_float ms_fSkiHeadingTweakMultiplier;
	static bank_float ms_fSkiHeadingTweakBackwardsMultiplier;
	static bank_float ms_fSkiHeadingTweakMinSpeedSqr;

	static bank_float ms_fNavToInitialWptDistSqr;

	CTaskFollowWaypointRecording(::CWaypointRecordingRoute * pRoute, const int iInitialProgress=0, const u32 iFlags=0, const int iTargetProgress=-1);
	virtual ~CTaskFollowWaypointRecording();

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING; }
	virtual aiTask* Copy() const;
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

#if !__FINAL
    virtual void Debug() const;
	void DebugVisualiserDisplay(const CPed * pPed, const Vector3 & vWorldCoords, s32 iOffsetY) const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_NavmeshToInitialWaypoint: return "NavmeshToInitialWaypoint";
			case State_AchieveHeadingBeforeResuming: return "AchieveHeadingBeforeResuming";
			case State_FollowingRoute: return "FollowingRoute";
			case State_FollowingRouteAiming: return "FollowingRouteAiming";
			case State_FollowingRouteShooting: return "FollowingRouteShooting";
			case State_Pause: return "Pause";
			case State_PauseAndFacePlayer: return "PauseAndFacePlayer";
			case State_AchieveHeadingAtEnd: return "AchieveHeadingAtEnd";
			case State_NavmeshBackToRoute: return "NavmeshBackToRoute";
			case State_ExactStop: return "ExactStop";
		}
		return NULL;
	}
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual	void CleanUp();

	int GetProgress() const { return m_iProgress; }
	void SetProgress(const int p);
	void SetLoopPlayback(const bool b) { m_bLoopPlayback = b; }

	float GetDistanceAlongRoute() const;

	// Get/set an offset which is applied to the entire route
	const Vector3 & GetRouteOffset() const { return m_vWaypointsOffset; }
	void SetRouteOffset(const Vector3 & vOffset) { m_vWaypointsOffset = vOffset; }

	::CWaypointRecordingRoute * GetRoute() { return m_pRoute; }
	void ClearRoute() { m_pRoute = NULL; }
	void SetQuitImmediately() { m_bWantToQuitImmediately = true; }

	bool GetIsPaused();
	void SetPause(const bool bFacePlayer, const bool bStopBeforeOrientating);
	void SetResume(const bool bAchieveHeadingFirst, const int iProgressToContinueFrom = -1, const int iTimeBeforeResumingMs = 0);
	void SetOverrideMoveBlendRatio(const float fMBR, const bool bAllowSlowingForCorners);
	void UseDefaultMoveBlendRatio();
	void SetNonInterruptible(const bool bState);
	void StartAimingAtEntity(const CEntity * pTarget, const bool bUseRunAndGun=false);
	void StartAimingAtPos(const Vector3 & vTarget, const bool bUseRunAndGun=false);
	void StartShootingAtEntity(const CEntity * pTarget, const bool bUseRunAndGun=false, const u32 iFiringPatternHash=0);
	void StartShootingAtPos(const Vector3 & vTarget, const bool bUseRunAndGun=false, const u32 iFiringPatternHash=0);
	void StopAimingOrShooting();

	inline u32 GetFlags() const { return m_iFlags; }
	inline bool GetRespondsToCollisionEvents() const { return ((m_iFlags & Flag_DoNotRespondToCollisionEvents)==0); }
	static int CalculateProgressFromPosition(const Vector3& vPosition, const Vector3 & vOffset, ::CWaypointRecordingRoute* pRoute, const float fMaxZ);

protected:

	FSM_Return StateInitial_OnEnter(CPed* pPed);
	FSM_Return StateInitial_OnUpdate(CPed* pPed);
	FSM_Return StateNavmeshToInitialWaypoint_OnEnter(CPed* pPed);
	FSM_Return StateNavmeshToInitialWaypoint_OnUpdate(CPed* pPed);
	FSM_Return StateAchieveHeadingBeforeResuming_OnEnter(CPed* pPed);
	FSM_Return StateAchieveHeadingBeforeResuming_OnUpdate(CPed* pPed);
	FSM_Return StateFollowingRoute_OnEnter(CPed* pPed);
	FSM_Return StateFollowingRoute_OnUpdate(CPed* pPed);
	FSM_Return StateFollowingRouteAiming_OnEnter(CPed* pPed);
	FSM_Return StateFollowingRouteAiming_OnUpdate(CPed* pPed);
	FSM_Return StateFollowingRouteShooting_OnEnter(CPed* pPed);
	FSM_Return StateFollowingRouteShooting_OnUpdate(CPed* pPed);
	FSM_Return StatePause_OnEnter(CPed* pPed);
	FSM_Return StatePause_OnUpdate(CPed* pPed);
	FSM_Return StatePauseAndFacePlayer_OnEnter(CPed* pPed);
	FSM_Return StatePauseAndFacePlayer_OnUpdate(CPed* pPed);
	FSM_Return StateAchieveHeadingAtEnd_OnEnter(CPed* pPed);
	FSM_Return StateAchieveHeadingAtEnd_OnUpdate(CPed* pPed);
	FSM_Return StateNavmeshBackToRoute_OnEnter(CPed* pPed);
	FSM_Return StateNavmeshBackToRoute_OnUpdate(CPed* pPed);
	FSM_Return StateExactStop_OnEnter(CPed* pPed);
	FSM_Return StateExactStop_OnUpdate(CPed* pPed);

	bool ControlOnFoot(CPed * pPed);
	bool ControlSki(CPed * pPed);
	bool IsRegularWaypoint(::CWaypointRecordingRoute::RouteData wpt) const;
	CTask * DoActionAtWaypoint(::CWaypointRecordingRoute::RouteData wpt, CPed * pPed, const bool bSkippedThisWaypoint);

	float GetDistanceRemainingBeforeStopping(CPed * pPed, float fStoppingDistance);
	void OrientateToTarget(CPed* pPed);

	void CalcTargetRadius(CPed * pPed);
	//void ModifyGotoPointTask(CPed * pPed, CTaskMoveGoToPoint * pGotoTask);
	void ModifyGotoPointTask(CPed * pPed, CTaskMove * pGotoTask, float fLookAheadDist);

	bool GetPositionAhead(CPed * pPed, Vector3 & vPosAhead, Vector3 & vClosestToPed, Vector3 & vSegment, const float fDistanceAhead);
	bool CalcLookAheadTarget(CPed * pPed, Vector3 & vOut_Target);
	float AdjustWaypointMBR(const float fMBR) const;
	void SetPedMass(CPed * pPed, const float fNewMass);
	CTask * CreateAimingOrShootingTask();

	void CalculateDistanceAlongRoute();

	Vector3 m_vToTarget;

	// worldspace offset which is applied to entire route.
	// its important to apply this offset *every* time a waypoint position is read from the route
	Vector3 m_vWaypointsOffset;

	::CWaypointRecordingRoute * m_pRoute;
	int m_iProgress;
	int m_iTargetProgress;
	u32 m_iFlags;
	float m_fTargetRadius;
	float m_fDistanceAlongRoute;
	float m_fMoveBlendRatioWhenCommencingStop;
	int m_iTimeBeforeResumingPlaybackMs;

	bool m_bWantToQuitImmediately;
	bool m_bFirstTimeRun;
	bool m_bLoopPlayback;
	bool m_bWantsToPause;
	bool m_bWantsToFacePlayer;
	bool m_bWantsToResumePlayback;
	bool m_bStopBeforeOrientating;
	bool m_bAchieveHeadingBeforeResumingPlayback;
	bool m_bWantsToStartAiming;
	bool m_bWantsToStartShooting;
	bool m_bWantsToStopAimingOrShooting;
	bool m_bInitialCanFallOffSkisFlag;
	bool m_bInitialSteersAroundPedsFlag;
	bool m_bInitialSteersAroundObjectsFlag;
	bool m_bInitialSteersAroundVehiclesFlag;
	bool m_bInitialScanForClimbsFlag;
	bool m_bOverrideMoveBlendRatio;
	bool m_bJustOverriddenMBR;
	bool m_bTargetIsEntity;
	bool m_bOverrideNonInterruptible;
	float m_fOverrideMoveBlendRatio;
	bool m_bOverrideMoveBlendRatioAllowSlowingForCorners;

	// Target vars used when aiming/shooting
	CAITarget m_AimAtTarget;
	u32 m_iFiringPatternHash;
	bool m_bUseRunAndGun;

#if __BANK
	bool m_bCalculatedHelperForce;
	int m_iInitialProgress;
	Vector3 m_vHelperForce;
	Vector3 m_vClosestPosOnRoute;
#endif
};

#endif	// TASK_FOLLOW_WAYPOINT_RECORDING_H