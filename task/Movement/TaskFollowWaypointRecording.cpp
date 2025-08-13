#include "TaskFollowWaypointRecording.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds\Ped.h"
#include "Peds\PedMoveBlend\PedMoveBlendInWater.h"
#include "Peds\PedMoveBlend\PedMoveBlendOnSkis.h"
#include "Peds\PedDebugVisualiser.h"
#include "Peds\PedIntelligence.h"
#include "Task\Movement\TaskIdle.h"
#include "Task\Movement\TaskGotoPoint.h"
#include "Task\Movement\TaskNavMesh.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task\Movement\Jumping\TaskJump.h"
#include "Task\General\TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task\Weapons\WeaponController.h"
#include "Task\Weapons\Gun\TaskGun.h"
#include "math\angmath.h"

AI_OPTIMISATIONS()

bank_float CTaskFollowWaypointRecording::ms_fTargetRadius_OnFoot					= 1.0f;
bank_float CTaskFollowWaypointRecording::ms_fTargetRadius_Skiing					= 5.0f;
bank_float CTaskFollowWaypointRecording::ms_fTargetRadius_SurfacingFromUnderwater	= 0.25f;
bank_bool CTaskFollowWaypointRecording::ms_bForceJumpHeading						= true;
bank_bool CTaskFollowWaypointRecording::ms_bUseHelperForces						= false;

bank_float CTaskFollowWaypointRecording::ms_fSkiHeadingTweakMultiplier			= 2.0f;	// 2.5f
bank_float CTaskFollowWaypointRecording::ms_fSkiHeadingTweakBackwardsMultiplier	= 4.0f;	// 4.0f
bank_float CTaskFollowWaypointRecording::ms_fSkiHeadingTweakMinSpeedSqr			= 16.0f;

bank_float CTaskFollowWaypointRecording::ms_fNavToInitialWptDistSqr					= 1.0f * 1.0f;

CTaskFollowWaypointRecording::CTaskFollowWaypointRecording(::CWaypointRecordingRoute * pRoute, const int iInitialProgress, const u32 iFlags, const int iTargetProgress) :
	m_pRoute(pRoute),
	m_iProgress(iInitialProgress),
	m_iTargetProgress(iTargetProgress),
	m_iFlags(iFlags),
	m_fTargetRadius(ms_fTargetRadius_OnFoot),
	m_vToTarget(0.0f,0.0f,0.0f),
	m_vWaypointsOffset(0.0f, 0.0f, 0.0f),
	m_fDistanceAlongRoute(0.0f),
	m_iTimeBeforeResumingPlaybackMs(0),
	m_bFirstTimeRun(false),
	m_bInitialCanFallOffSkisFlag(true),
	m_bInitialSteersAroundPedsFlag(true),
	m_bInitialSteersAroundObjectsFlag(true),
	m_bInitialSteersAroundVehiclesFlag(true),
	m_bInitialScanForClimbsFlag(true),
	m_bLoopPlayback(false),
	m_bWantsToPause(false),
	m_bWantsToFacePlayer(false),
	m_bStopBeforeOrientating(false),
	m_bWantsToResumePlayback(false),
	m_bAchieveHeadingBeforeResumingPlayback(false),
	m_bOverrideMoveBlendRatio(false),
	m_bOverrideMoveBlendRatioAllowSlowingForCorners(false),
	m_bJustOverriddenMBR(false),
	m_bWantToQuitImmediately(false),
	m_bTargetIsEntity(false),
	m_bOverrideNonInterruptible(false),
	m_iFiringPatternHash(0),
	m_bUseRunAndGun(false),
	m_bWantsToStartAiming(false),
	m_bWantsToStartShooting(false),
	m_bWantsToStopAimingOrShooting(false),
	m_fOverrideMoveBlendRatio(MOVEBLENDRATIO_STILL)
{
	SetInternalTaskType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);

	if(!pRoute)
	{
		m_bWantToQuitImmediately = true;
		return;
	}

	AssertMsg(iTargetProgress == -1 || iTargetProgress >= iInitialProgress, "target progress cannot be less than initial progress!");

	m_iTargetProgress = Min( m_iTargetProgress, m_pRoute->GetSize() );

	if (!(m_iFlags & Flag_SuppressExactStop))
		m_iFlags |= Flag_UseExactStop;

#if __BANK
	m_iInitialProgress = iInitialProgress;
	if(m_pRoute != CWaypointRecording::GetRecordingRoute())
		m_bWantToQuitImmediately = !CWaypointRecording::IncReferencesToRecording(m_pRoute);
#else
	m_bWantToQuitImmediately = !CWaypointRecording::IncReferencesToRecording(m_pRoute);
#endif
	
}
CTaskFollowWaypointRecording::~CTaskFollowWaypointRecording()
{
#if __BANK
	if(m_pRoute != CWaypointRecording::GetRecordingRoute())
		CWaypointRecording::DecReferencesToRecording(m_pRoute);
#else
	CWaypointRecording::DecReferencesToRecording(m_pRoute);
#endif
}

void CTaskFollowWaypointRecording::CleanUp()
{
	CPed *pPed = GetPed();

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverFallOffSkis, m_bInitialCanFallOffSkisFlag );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds, m_bInitialSteersAroundPedsFlag );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects, m_bInitialSteersAroundObjectsFlag );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundVehicles, m_bInitialSteersAroundVehiclesFlag );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForClimb, m_bInitialScanForClimbsFlag );

	// Causes problems with damage calculations (See B*25834)
// 	if(pPed->GetMass() != PED_MASS)
// 		pPed->SetMass(PED_MASS);
}

bool CTaskFollowWaypointRecording::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(m_bWantToQuitImmediately)
		return true;

	bool bNonInterruptible = m_bOverrideNonInterruptible;
	if(m_pRoute && m_iProgress < m_pRoute->GetSize())
	{
		const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
		if(wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_NonInterrupt)
			bNonInterruptible = true;
	}

	if(pEvent && (((CEvent*)pEvent)->GetEventType()==EVENT_SWITCH_2_NM_TASK ||
		((CEvent*)pEvent)->GetEventType()==EVENT_PED_COLLISION_WITH_PLAYER ||
		((CEvent*)pEvent)->GetEventType()==EVENT_PED_COLLISION_WITH_PED ||
		((CEvent*)pEvent)->GetEventType()==EVENT_POTENTIAL_BE_WALKED_INTO))
	{
		if(bNonInterruptible)
			return false;
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

aiTask* CTaskFollowWaypointRecording::Copy() const
{
	CTaskFollowWaypointRecording * pClone = rage_new CTaskFollowWaypointRecording(m_pRoute, m_iProgress, m_iFlags, m_iTargetProgress);
	pClone->SetRouteOffset( GetRouteOffset() );
	return pClone;
}

#if !__FINAL
void CTaskFollowWaypointRecording::Debug() const
{
#if __BANK
	if(m_bCalculatedHelperForce)
	{
		grcDebugDraw::Cross(RCC_VEC3V(m_vClosestPosOnRoute), 0.125f, Color_purple);
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		grcDebugDraw::Line(vPedPosition, vPedPosition+m_vHelperForce, Color_magenta2, Color_white);

		ASSERT_ONLY(const float fDot = DotProduct(m_vHelperForce, VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetB())) >= -0.1f;)
		Assert(fDot > -0.1f);
	}
#endif
	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
}
void CTaskFollowWaypointRecording::DebugVisualiserDisplay(const CPed * UNUSED_PARAM(pPed), const Vector3 & BANK_ONLY(vWorldCoords), s32 BANK_ONLY(iOffsetY)) const
{
#if __BANK
	char tmp[64];

	sprintf(tmp, "Progress %i", m_iProgress);
	grcDebugDraw::Text(vWorldCoords, Color_white, 0, iOffsetY, tmp);
	iOffsetY += grcDebugDraw::GetScreenSpaceTextHeight();

	sprintf(tmp, "Task Initial Progress %i", m_iInitialProgress);
	grcDebugDraw::Text(vWorldCoords, Color_white, 0, iOffsetY, tmp);
	iOffsetY += grcDebugDraw::GetScreenSpaceTextHeight();

	sprintf(tmp, "Distance along route %.1f", m_fDistanceAlongRoute);
	grcDebugDraw::Text(vWorldCoords, Color_white, 0, iOffsetY, tmp);
	iOffsetY += grcDebugDraw::GetScreenSpaceTextHeight();

	// debug tes0t
//	float fCalculatedDist = m_pRoute->GetWaypointDistanceFromStart(m_iProgress);
//	sprintf(tmp, "Calcualted distance %.1f", fCalculatedDist);
//	grcDebugDraw::Text(vWorldCoords, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight()*3, tmp);

#endif
}
#endif

void CTaskFollowWaypointRecording::CalcTargetRadius(CPed * pPed)
{
	m_fTargetRadius = ms_fTargetRadius_OnFoot;

	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
	if (pPed->GetCurrentMotionTask()->IsOnFoot())
	{
		// Use a smaller waypoint radius for drop-downs as the slide-to-coord
		// looks bad otherwise.
		if(wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_DropDown)
		{
			m_fTargetRadius = Max(0.5f, ms_fTargetRadius_OnFoot * 0.5f);
		}
		else
		{
			m_fTargetRadius = ms_fTargetRadius_OnFoot;
		}
	}
	else
	{
		if(wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater)
		{
			m_fTargetRadius = ms_fTargetRadius_SurfacingFromUnderwater;
		}
	}
}
//void CTaskFollowWaypointRecording::ModifyGotoPointTask(CPed * pPed, CTaskMoveGoToPoint * pGotoTask)
void CTaskFollowWaypointRecording::ModifyGotoPointTask(CPed * pPed, CTaskMove * pGotoTask, float fLookAheadDist)
{
	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
	if(pPed->GetIsSwimming() && (wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater))
	{
		if(pGotoTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
		{
			((CTaskMoveGoToPoint*)pGotoTask)->SetExpandHeightDeltaUnderwater(false);
		}
	}

	//static dev_float fLookAheadDist = 1.5f;

	Vector3 vTarget, vClosestToPed, vSegment;
	if(!GetPositionAhead(pPed, vTarget, vClosestToPed, vSegment, fLookAheadDist))
	{
		vTarget = wpt.GetPosition() + m_vWaypointsOffset;
	}
	pGotoTask->SetTarget(pPed, VECTOR3_TO_VEC3V(vTarget));
}

int CTaskFollowWaypointRecording::CalculateProgressFromPosition(const Vector3& vPosition, const Vector3 & vOffset, ::CWaypointRecordingRoute* pRoute, const float fMaxZ)
{
	Assert(pRoute && pRoute->GetSize()>=2);

	static const float fMaxDistSqr = FLT_MAX; // very high sentinel value
	
	s32 iClosest = -1;
	float fClosest = FLT_MAX;
	float fClosestT = 0.0f;

	for(s32 i=1; i<pRoute->GetSize(); i++)
	{
		const Vector3 vLast = pRoute->GetWaypoint(i-1).GetPosition() + vOffset;
		const Vector3 vNext = pRoute->GetWaypoint(i).GetPosition() + vOffset;
		const Vector3 vDiff = vNext-vLast;
		const float T = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vLast, vDiff, vPosition) : 0.0f;
		const Vector3 vClose = vLast + (vDiff * T);
		const float fMag2 = (vClose - vPosition).Mag2();

		if(fMag2 < fClosest && fMag2 < fMaxDistSqr && (Abs(vClose.z - vPosition.z) < fMaxZ) )
		{
			fClosest = fMag2;
			iClosest = i;
			fClosestT = T;
		}
	}

	// Handle the case where the closest position was before the start of the route
	// Normally we return the end waypoint of the closest route segment, but in this case
	// we want to return the initial point.
	if(iClosest == 1 && fClosestT == 0.0f)
		iClosest = 0;

	return iClosest;
}
float CTaskFollowWaypointRecording::GetDistanceAlongRoute() const
{
	return m_fDistanceAlongRoute;
}
void CTaskFollowWaypointRecording::CalculateDistanceAlongRoute()
{
	m_fDistanceAlongRoute = 0.0f;

	if(m_pRoute && m_pRoute->GetSize() > 0)
	{
		Vector3 vLastPos = m_pRoute->GetWaypoint(0).GetPosition() + m_vWaypointsOffset;
		for(int p=1; p<m_iProgress; p++)
		{
			Vector3 vPos = m_pRoute->GetWaypoint(p).GetPosition() + m_vWaypointsOffset;
			m_fDistanceAlongRoute += (vPos - vLastPos).XYMag();
			vLastPos = vPos;
		}
	}
}
void CTaskFollowWaypointRecording::SetProgress(const int p)
{
	Assert(m_pRoute);
	Assert(p >= 0 && p < m_pRoute->GetSize());

	m_iProgress = Clamp(p, 0, m_pRoute->GetSize()-1);
	CalculateDistanceAlongRoute();
}
bool CTaskFollowWaypointRecording::GetIsPaused()
{
	return ( GetState()==State_Pause || GetState()==State_PauseAndFacePlayer || m_bWantsToPause );
}
void CTaskFollowWaypointRecording::SetPause(const bool bFacePlayer, const bool bStopBeforeOrientating)
{
	//Assertf(GetState()!=State_Pause && GetState()!=State_PauseAndFacePlayer, "Trying to pause, but waypoint playback is already paused!");

	m_bWantsToResumePlayback = false;
	m_bStopBeforeOrientating = bStopBeforeOrientating;
	m_iTimeBeforeResumingPlaybackMs = 0;
	m_bWantsToPause = true;
	m_bWantsToFacePlayer = bFacePlayer;

	Assert(!(m_bWantsToPause&&m_bWantsToResumePlayback));
}
void CTaskFollowWaypointRecording::SetResume(const bool bAchieveHeadingFirst, const int iProgressToContinueFrom, const int iTimeBeforeResumingMs)
{
	//Assertf(GetState()==State_Pause || GetState()==State_PauseAndFacePlayer, "Trying to resume, but waypoint recording is not paused!");

	m_iTimeBeforeResumingPlaybackMs = iTimeBeforeResumingMs;
	m_bWantsToPause = false;
	m_bWantsToResumePlayback = (iTimeBeforeResumingMs <= 0);
	m_bAchieveHeadingBeforeResumingPlayback = bAchieveHeadingFirst;

	if(iProgressToContinueFrom != -1)
	{
		m_iProgress = iProgressToContinueFrom;
		m_iProgress = Clamp(m_iProgress, 0, m_pRoute->GetSize()-1);
		CalculateDistanceAlongRoute();
	}

	Assert(!(m_bWantsToPause&&m_bWantsToResumePlayback));
}
void CTaskFollowWaypointRecording::SetOverrideMoveBlendRatio(const float fMBR, const bool bAllowSlowingForCorners)
{
	m_bOverrideMoveBlendRatio = true;
	m_bJustOverriddenMBR = true;
	m_fOverrideMoveBlendRatio = Clamp(fMBR, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
	m_bOverrideMoveBlendRatioAllowSlowingForCorners = bAllowSlowingForCorners;
}
void CTaskFollowWaypointRecording::UseDefaultMoveBlendRatio()
{
	m_bOverrideMoveBlendRatio = false;
	m_bOverrideMoveBlendRatioAllowSlowingForCorners = false;
	m_bJustOverriddenMBR = true;
}
void CTaskFollowWaypointRecording::SetNonInterruptible(const bool bState)
{
	m_bOverrideNonInterruptible = bState;
}
float CTaskFollowWaypointRecording::AdjustWaypointMBR(const float fMBR) const
{
	if(m_bOverrideMoveBlendRatio)
	{
		return m_fOverrideMoveBlendRatio;
	}
	else
	{
		return Clamp(fMBR, MOVEBLENDRATIO_WALK, MOVEBLENDRATIO_SPRINT);
	}
}
void CTaskFollowWaypointRecording::StartAimingAtEntity(const CEntity * pTarget, const bool bUseRunAndGun)
{
	m_AimAtTarget.Clear();
	m_AimAtTarget.SetEntity(pTarget);
	m_bTargetIsEntity = true;
	m_bUseRunAndGun = bUseRunAndGun;

	m_bWantsToStartShooting = false;
	m_bWantsToStartAiming = true;
}
void CTaskFollowWaypointRecording::StartAimingAtPos(const Vector3 & vTarget, const bool bUseRunAndGun)
{
	m_AimAtTarget.Clear();
	m_AimAtTarget.SetPosition(vTarget);
	m_bTargetIsEntity = false;
	m_bUseRunAndGun = bUseRunAndGun;

	m_bWantsToStartShooting = false;
	m_bWantsToStartAiming = true;
	m_bWantsToStopAimingOrShooting = false;
}
void CTaskFollowWaypointRecording::StartShootingAtEntity(const CEntity * pTarget, const bool bUseRunAndGun, const u32 iFiringPatternHash)
{
	m_AimAtTarget.Clear();
	m_AimAtTarget.SetEntity(pTarget);
	m_bTargetIsEntity = true;
	m_bUseRunAndGun = bUseRunAndGun;

	const u32 FIRING_PATTERN_BURST_FIRE = ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE", 0x0d6ff6d61);
	m_iFiringPatternHash = iFiringPatternHash ? iFiringPatternHash : FIRING_PATTERN_BURST_FIRE;

	m_bWantsToStartAiming = false;
	m_bWantsToStartShooting = true;
	m_bWantsToStopAimingOrShooting = false;
}
void CTaskFollowWaypointRecording::StartShootingAtPos(const Vector3 & vTarget, const bool bUseRunAndGun, const u32 iFiringPatternHash)
{
	m_AimAtTarget.Clear();
	m_AimAtTarget.SetPosition(vTarget);
	m_bTargetIsEntity = false;
	m_bUseRunAndGun = bUseRunAndGun;

	const u32 FIRING_PATTERN_BURST_FIRE = ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE", 0x0d6ff6d61);
	m_iFiringPatternHash = iFiringPatternHash ? iFiringPatternHash : FIRING_PATTERN_BURST_FIRE;

	m_bWantsToStartAiming = false;
	m_bWantsToStartShooting = true;
	m_bWantsToStopAimingOrShooting = false;
}
void CTaskFollowWaypointRecording::StopAimingOrShooting()
{
	m_bWantsToStopAimingOrShooting = true;
}

CTask::FSM_Return CTaskFollowWaypointRecording::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// If the route this task is following has been removed by the streaming, then
	// this flag will be set - and the task must terminate immediately!
	if(m_bWantToQuitImmediately)
	{
		return FSM_Quit;
	}

	if(m_bFirstTimeRun)
	{
		m_bInitialCanFallOffSkisFlag = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverFallOffSkis );
		m_bInitialSteersAroundPedsFlag = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds );
		m_bInitialSteersAroundObjectsFlag = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects );
		m_bInitialSteersAroundVehiclesFlag = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundVehicles );
		m_bInitialScanForClimbsFlag = pPed->GetPedResetFlag( CPED_RESET_FLAG_SearchingForClimb );
		m_bFirstTimeRun = false;
	}

	if (m_iFlags & Flag_UseTighterTurnSettings)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverFallOffSkis, true );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects, false );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundVehicles, false );

	if((m_iFlags & Flag_AllowSteeringAroundPeds)==0)
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds, false );

	// If ped is off the end of the route, just quit
	if(m_pRoute && m_iProgress > m_pRoute->GetSize())
	{
		return FSM_Quit;
	}

	// This should only happen during route editing, if the recording route has been cleared via Rag whilst this task is running.
	if(m_pRoute && m_pRoute->GetSize()==0)
	{
		Assertf(false, "The waypoint route was cleared whilst CTaskFollowWaypointRecording was running.  Its ok - the task will just quit.");
		return FSM_Quit;
	}
#if __BANK
	m_bCalculatedHelperForce = false;
	m_vHelperForce.Zero();
#endif

	//******************************************************************************************************

	FSM_Begin

		FSM_State(State_Initial);
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(State_NavmeshToInitialWaypoint);
			FSM_OnEnter
				return StateNavmeshToInitialWaypoint_OnEnter(pPed);
			FSM_OnUpdate
				return StateNavmeshToInitialWaypoint_OnUpdate(pPed);

		FSM_State(State_AchieveHeadingBeforeResuming)
			FSM_OnEnter
				return StateAchieveHeadingBeforeResuming_OnEnter(pPed);
			FSM_OnUpdate
				return StateAchieveHeadingBeforeResuming_OnUpdate(pPed);

		FSM_State(State_FollowingRoute);
			FSM_OnEnter
				return StateFollowingRoute_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingRoute_OnUpdate(pPed);

		FSM_State(State_FollowingRouteAiming);
			FSM_OnEnter
				return StateFollowingRouteAiming_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingRouteAiming_OnUpdate(pPed);

		FSM_State(State_FollowingRouteShooting);
			FSM_OnEnter
				return StateFollowingRouteShooting_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowingRouteShooting_OnUpdate(pPed);

		FSM_State(State_Pause);
			FSM_OnEnter
				return StatePause_OnEnter(pPed);
			FSM_OnUpdate
				return StatePause_OnUpdate(pPed);

		FSM_State(State_PauseAndFacePlayer);
			FSM_OnEnter
				return StatePauseAndFacePlayer_OnEnter(pPed);
			FSM_OnUpdate
				return StatePauseAndFacePlayer_OnUpdate(pPed);

		FSM_State(State_AchieveHeadingAtEnd)
			FSM_OnEnter
				return StateAchieveHeadingAtEnd_OnEnter(pPed);
			FSM_OnUpdate
				return StateAchieveHeadingAtEnd_OnUpdate(pPed);

		FSM_State(State_NavmeshBackToRoute);
			FSM_OnEnter
				return StateNavmeshBackToRoute_OnEnter(pPed);
			FSM_OnUpdate
				return StateNavmeshBackToRoute_OnUpdate(pPed);

		FSM_State(State_ExactStop)
			FSM_OnEnter
				return StateExactStop_OnEnter(pPed);
			FSM_OnUpdate
				return StateExactStop_OnUpdate(pPed);

	FSM_End
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateInitial_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateInitial_OnUpdate(CPed* pPed)
{
	if( ((m_iFlags & Flag_StartFromClosestPosition) != 0) || m_iProgress == -1)
	{
		m_iProgress = CalculateProgressFromPosition(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), m_vWaypointsOffset, m_pRoute, 4.0f);
		// this can return -1 if no progress point found; in this case clamping will set it to zero
		m_iProgress = Clamp(m_iProgress, 0, m_pRoute->GetSize()-1);

		CalculateDistanceAlongRoute();
	}
	if((m_iFlags & Flag_NavMeshToInitialPoint) && !pPed->GetIsSwimming())
	{
		SetState(State_NavmeshToInitialWaypoint);
	}
	else
	{
		SetState(State_FollowingRoute);
	}

	// If instructed to start task initially aiming, then aim at a position 100m directly ahead of the ped
	if((m_iFlags & Flag_StartTaskInitiallyAiming)!=0)
	{
		StartAimingAtPos( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + (pPed->GetTransform().GetForward() * ScalarV(100.0f)) ) );
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateNavmeshToInitialWaypoint_OnEnter(CPed* pPed)
{
	// Check how far we are from the initial waypoint
	Assert(m_iProgress < m_pRoute->GetSize());
	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
	const Vector3 vToWpt = (wpt.GetPosition() + m_vWaypointsOffset) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(vToWpt.Mag2() > ms_fNavToInitialWptDistSqr)
	{
		const float fMBR = AdjustWaypointMBR(wpt.GetSpeedAsMBR());
		CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(fMBR, wpt.GetPosition()+m_vWaypointsOffset);
		pNavMeshTask->SetDontStandStillAtEnd(true);
		SetNewTask( rage_new CTaskComplexControlMovement(pNavMeshTask) );
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateNavmeshToInitialWaypoint_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FollowingRoute);
	}

	if(m_bOverrideMoveBlendRatio && GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		((CTaskMoveFollowNavMesh*)GetSubTask())->SetMoveBlendRatio(m_fOverrideMoveBlendRatio);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateFollowingRouteAiming_OnEnter(CPed* pPed)
{
	OrientateToTarget(pPed);

	return StateFollowingRoute_OnEnter(pPed);
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateFollowingRouteAiming_OnUpdate(CPed* pPed)
{
	if(!m_AimAtTarget.GetIsValid() || m_bWantsToStopAimingOrShooting)
	{
		m_bWantsToStopAimingOrShooting = false;
		SetState(State_FollowingRoute);
		return FSM_Continue;
	}

	OrientateToTarget(pPed);

	return StateFollowingRoute_OnUpdate(pPed);
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateFollowingRouteShooting_OnEnter(CPed* pPed)
{
	OrientateToTarget(pPed);

	return StateFollowingRoute_OnEnter(pPed);
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateFollowingRouteShooting_OnUpdate(CPed* pPed)
{
	if(!m_AimAtTarget.GetIsValid() || m_bWantsToStopAimingOrShooting)
	{
		m_bWantsToStopAimingOrShooting = false;
		SetState(State_FollowingRoute);
		return FSM_Continue;
	}

	OrientateToTarget(pPed);

	return StateFollowingRoute_OnUpdate(pPed);
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateAchieveHeadingBeforeResuming_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	Assert(m_iProgress < m_pRoute->GetSize());
	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);

	CTaskMoveAchieveHeading * pHeadingTask = rage_new CTaskMoveAchieveHeading(wpt.GetHeading());
	SetNewTask(rage_new CTaskComplexControlMovement(pHeadingTask));

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateAchieveHeadingBeforeResuming_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(m_bWantsToPause)
	{
		m_bWantsToPause = false;

		if(m_bWantsToFacePlayer)
			SetState(State_PauseAndFacePlayer);
		else
			SetState(State_Pause);

		return FSM_Continue;
	}
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FollowingRoute);
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateFollowingRoute_OnEnter(CPed* pPed)
{
	Assert(m_iProgress < m_pRoute->GetSize());
	
	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
	CalcTargetRadius(pPed);
	CTaskMoveGoToPoint * pGotoTask = rage_new CTaskMoveGoToPoint(AdjustWaypointMBR(wpt.GetSpeedAsMBR()), wpt.GetPosition()+m_vWaypointsOffset, m_fTargetRadius);

	CTask * pAimOrShootTask = CreateAimingOrShootingTask();
	SetNewTask( rage_new CTaskComplexControlMovement(pGotoTask, pAimOrShootTask) );

	m_vToTarget = (wpt.GetPosition()+m_vWaypointsOffset) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_vToTarget.z = 0.0f;

	return FSM_Continue;
}


CTask::FSM_Return CTaskFollowWaypointRecording::StateFollowingRoute_OnUpdate(CPed* pPed)
{
	if(m_bWantsToPause)
	{
		m_bWantsToPause = false;

		if(m_bWantsToFacePlayer)
			SetState(State_PauseAndFacePlayer);
		else
			SetState(State_Pause);

		return FSM_Continue;
	}
	if(m_bWantsToStartAiming)
	{
		m_bWantsToStartAiming = false;
		SetState(State_FollowingRouteAiming);
	}
	if(m_bWantsToStartShooting)
	{
		m_bWantsToStartShooting = false;
		SetState(State_FollowingRouteShooting);
	}

	// JB: Removed this below as it was causing problems (url:bugstar:706834)
	// Anyhow, designers should be carefult not to give peds waypoint tasks which might get them stuck.

//	if(HasLeftRoute(pPed))
//	{
//		SetState(State_NavmeshBackToRoute);
//		return FSM_Continue;
//	}

	if(m_bJustOverriddenMBR)
	{
		CTask * pTaskMove = pPed->GetPedIntelligence()->GetActiveMovementTask();
		Assert(!pTaskMove || pTaskMove->IsMoveTask());
		if(pTaskMove && pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
		{
			Assert(m_iProgress < m_pRoute->GetSize());
			const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
			((CTaskMove*)pTaskMove)->SetMoveBlendRatio(AdjustWaypointMBR(wpt.GetSpeedAsMBR()));
		}
		m_bJustOverriddenMBR = false;
	}

	bool bIncrementProgress = false;
	bool bSkipWaypoint = false;
	bool bSubTaskFinished = false;

	pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_AllowPullingPedOntoRoute, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges, true );

	static dev_float fLookAheadDist = 1.5f;

	Assert(m_iProgress < m_pRoute->GetSize());

	// If uninterruptible flag is set, disable ragdolling & steering
	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
	if((wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_NonInterrupt) || m_bOverrideNonInterruptible)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true );
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds, false );
	}

	// If we are approaching a waypoint where a jump is required, then scan for climbs
	pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForClimb, m_bInitialScanForClimbsFlag );

	int iJumpCheck = m_iProgress;
	while(iJumpCheck < m_pRoute->GetSize() && iJumpCheck < m_iProgress+2)
	{
		const ::CWaypointRecordingRoute::RouteData & lookAheadWpt = m_pRoute->GetWaypoint(iJumpCheck);
		if(lookAheadWpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump)
		{
			// Enable climb scanning
			pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForClimb, true );
			break;
		}
		iJumpCheck++;
	}


	//--------------------------------------------------------------------------------------------------

	CTaskMoveGoToPoint * pGotoPointTask = NULL;

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&
		((CTaskComplexControlMovement*)GetSubTask())->GetMoveTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT &&
		((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed) &&
		((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed)->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
	{
		pGotoPointTask = (CTaskMoveGoToPoint*) ((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed);

		const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
		
		if((wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater)==0)
		{
			pGotoPointTask->SetDontCompleteThisFrame();

			ModifyGotoPointTask(pPed, pGotoPointTask, fLookAheadDist);

			// Check for completion of current segment
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const Vector3 vDist = (wpt.GetPosition()+m_vWaypointsOffset) - vPedPos;
			if(vDist.XYMag2() < m_fTargetRadius*m_fTargetRadius)
			{
				bSubTaskFinished = true;
			}
			// Check for overshooting end of current segment (added to prevent peds circling their target)
			if(m_iProgress > 0)
			{
				const ::CWaypointRecordingRoute::RouteData & prevWpt = m_pRoute->GetWaypoint(m_iProgress-1);
				if(IsRegularWaypoint(prevWpt) && IsRegularWaypoint(wpt))
				{
					Vector3 vSeg = (wpt.GetPosition()+m_vWaypointsOffset) - (prevWpt.GetPosition()+m_vWaypointsOffset);
					if(vSeg.Mag2() > SMALL_FLOAT)
					{
						vSeg.Normalize();
						if(DotProduct(vSeg, (wpt.GetPosition()+m_vWaypointsOffset)-vPedPos)  < 0.0f)
						{
							bSubTaskFinished = true;
						}
					}
				}
			}
		}
	}

	// We have a flag which can optionally apply an extra force to a ped towards their jump target
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_JUMPVAULT)
	{
		if(m_iProgress > 0)
		{
			const ::CWaypointRecordingRoute::RouteData & prevWpt = m_pRoute->GetWaypoint(m_iProgress-1);
			if(prevWpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump &&
				prevWpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_ApplyJumpForce)
			{
				if(m_iProgress < m_pRoute->GetSize())
				{
					//static dev_float fAuthoredJumpLength = 4.0;
					static dev_float fForce = 0.5f;

					const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
					const Vector3 vNextPos = wpt.GetPosition()+m_vWaypointsOffset;
					Vector3 vPrevToNext = vNextPos - prevWpt.GetPosition()+m_vWaypointsOffset;
					vPrevToNext.z = 0.0f;
					const float fInitialLength = vPrevToNext.Mag();

					Vector3 vPedToNext = vNextPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					vPedToNext.z = 0.0f;
					const float fRemainingLength = vPedToNext.Mag();
					vPedToNext.Normalize();
					vPedToNext.Scale( (fRemainingLength/fInitialLength) * fForce );

					Vector3 vVel = pPed->GetVelocity();		
					vVel += vPedToNext;
					pPed->SetVelocity(vVel);
				}
			}
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || bSubTaskFinished)
	{
		bIncrementProgress = true;
	}
	else
	{
		if(pGotoPointTask)
		{
			//*******************************************************************************************
			// Check that we've not overshot our target.  If we're traveling fast (e.g. skis) then just
			// head towards the next waypoint, we can't mess about trying to double back during playback.

			const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
			if((wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater)==0)
			{
				Vector3 vToTarget = (wpt.GetPosition()+m_vWaypointsOffset) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				vToTarget.z = 0.0f;
				const float fDot = DotProduct(m_vToTarget, vToTarget);

				if(fDot < 0.0f)
				{
					bSkipWaypoint = true;
				}
				else
				{
					if (pPed->GetCurrentMotionTask()->IsInWater())
					{
						if(!ControlOnFoot(pPed))
						{
							bSkipWaypoint = true;
						}
					}
				}
			}
		}
	}

	if(bSkipWaypoint)
	{
		bIncrementProgress = true;
	}

	if(bIncrementProgress)
	{
		Assert(m_iProgress < m_pRoute->GetSize());

		const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);

		m_iProgress++;

		if(m_iProgress >= m_pRoute->GetSize() || (m_iTargetProgress >= 0 && m_iProgress >= m_iTargetProgress))
		{
			// Keep progress in a sane range, in case task is to restart somehow
			m_iProgress = Clamp(m_iProgress, 0, m_pRoute->GetSize()-1);

			// Reached end of recording?
			if(m_bLoopPlayback)
			{
				if(m_pRoute->GetSize() >= 2)
				{
					const ::CWaypointRecordingRoute::RouteData & wpt1 = m_pRoute->GetWaypoint(0);
					const ::CWaypointRecordingRoute::RouteData & wpt2 = m_pRoute->GetWaypoint(1);
					const float fHdg = fwAngle::GetRadianAngleBetweenPoints((wpt2.GetPosition()+m_vWaypointsOffset).x, (wpt2.GetPosition()+m_vWaypointsOffset).y, (wpt1.GetPosition()+m_vWaypointsOffset).x, (wpt1.GetPosition()+m_vWaypointsOffset).y);

					pPed->Teleport(wpt1.GetPosition()+m_vWaypointsOffset, fHdg, true);
					m_iProgress = 1;
					CalculateDistanceAlongRoute();

					SetFlag(aiTaskFlags::RestartCurrentState);
					return FSM_Continue;
				}
			}
			else
			{
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f);

				if(m_iFlags & Flag_TurnToFaceWaypointHeadingAtEnd)
				{
					static dev_float fHdgEps = 8.0f * DtoR;
					const float fHdgDelta = SubtractAngleShorter(pPed->GetCurrentHeading(), wpt.GetHeading());
					if(Abs(fHdgDelta) > fHdgEps)
					{
						SetState(State_AchieveHeadingAtEnd);
						return FSM_Continue;
					}
				}

				return FSM_Quit;
			}
		}
		else
		{
			// Ensure that progress remains in a valid range
			m_iProgress = Clamp(m_iProgress, 0, m_pRoute->GetSize()-1);

			const ::CWaypointRecordingRoute::RouteData & nextWpt = m_pRoute->GetWaypoint(m_iProgress);
			m_fDistanceAlongRoute += ((nextWpt.GetPosition()+m_vWaypointsOffset) - (wpt.GetPosition()+m_vWaypointsOffset)).XYMag();

			CTask * pActionTask = DoActionAtWaypoint(wpt, pPed, bSkipWaypoint);

			if(!pActionTask)
			{
				CalcTargetRadius(pPed);
				CTaskMoveGoToPoint * pGotoTask = rage_new CTaskMoveGoToPoint(AdjustWaypointMBR(nextWpt.GetSpeedAsMBR()), nextWpt.GetPosition()+m_vWaypointsOffset, m_fTargetRadius);

				ModifyGotoPointTask(pPed, pGotoTask, fLookAheadDist);

				CTask * pAimOrShootTask = CreateAimingOrShootingTask();
				SetNewTask( rage_new CTaskComplexControlMovement(pGotoTask, pAimOrShootTask) );

				m_vToTarget = (nextWpt.GetPosition()+m_vWaypointsOffset) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				m_vToTarget.z = 0.0f;

				// TODO : We need some way of preventing peds from being pushed around (see comments below)
				if((nextWpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_NonInterrupt) || m_bOverrideNonInterruptible)
				{
					// Causes problems with damage calculations (See B*25834)
// 					if(pPed->GetMass() != ms_fLargeMassForPed)
// 					{				
// 						SetPedMass(pPed, ms_fLargeMassForPed);
// 					}
				}
				else
				{
					// Causes problems with damage calculations (See B*25834)
// 					if(pPed->GetMass() != PED_MASS)
// 					{
// 						SetPedMass(pPed, PED_MASS);
// 					}
				}
			}
		}
	}

	//-----------------------------------------------------------
	// Handle triggering of walk/run-stops for stopping exactly

	// Consider stopping distance also
	if((m_iFlags & Flag_UseExactStop) && !(m_iFlags & Flag_SuppressExactStop))
	{
		Assert(m_iProgress < m_pRoute->GetSize());

		// Get stopping distance
		float fStoppingDist = 0.f;
		CTaskMotionBase* pCurrentMotionTask = GetPed()->GetCurrentMotionTask();
		if (pCurrentMotionTask)
		{
			s32 iFinalProgress = (m_iTargetProgress >= 0) ? m_iTargetProgress : m_pRoute->GetSize()-1;
			iFinalProgress = Min(iFinalProgress, m_pRoute->GetSize()-1);
			const CWaypointRecordingRoute::RouteData & nextWpt = m_pRoute->GetWaypoint(iFinalProgress);
			fStoppingDist = pCurrentMotionTask->GetStoppingDistance(nextWpt.GetHeading()) + 0.1f; // Adding some distance to get less latency
		}

		if(fStoppingDist > 0.0f)
		{
			float fDistLeft = GetDistanceRemainingBeforeStopping(pPed, fStoppingDist);
			if(fDistLeft <= fStoppingDist)
			{
				SetState(State_ExactStop);
				return FSM_Continue;	
			}
		}
	}

	//---------------------------------------------------
	// Handle slowing for corners if traveling at speed

	TUNE_GROUP_BOOL(TASK_FOLLOW_WAYPOINT_RECORDING, bSlowMoreForCorners, false);

	CTask * pTaskMove = pPed->GetPedIntelligence()->GetActiveMovementTask();
	Assert(!pTaskMove || pTaskMove->IsMoveTask());
	if( pTaskMove && pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT )
	{
		Assert(m_iProgress < m_pRoute->GetSize());

		const float fCurrMbr = Clamp(pPed->GetMotionData()->GetCurrentMbrY(), MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
		if( !pPed->IsStrafing() )
		{
			Vector3 vLookAheadPos, vClosestToPed, vSegment;
			CMoveBlendRatioSpeeds moveSpeeds;


			CTaskMotionBase* pMotionBase = pPed->GetCurrentMotionTask();

			pMotionBase->GetMoveSpeeds(moveSpeeds);

			const float fMoveSpeed = CTaskMotionBase::GetActualMoveSpeed(moveSpeeds.GetSpeedsArray(), fCurrMbr);
			const float fDistanceAhead = fMoveSpeed * CTaskNavBase::ms_fCorneringLookAheadTime;

			if(GetPositionAhead(pPed, vLookAheadPos, vClosestToPed, vSegment, fDistanceAhead))
			{
#if __BANK && defined(DEBUG_DRAW)
				if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging || CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText)
					grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vLookAheadPos), 0.25f, Color_orange);
#endif
				float fMBR = AdjustWaypointMBR(wpt.GetSpeedAsMBR());

				if( !pPed->IsStrafing() && (pPed->GetPedResetFlag( CPED_RESET_FLAG_SuppressSlowingForCorners )==false) && ((m_iFlags & Flag_DontSlowForCorners)==0) )
				{
					CMoveBlendRatioTurnRates turnRates;
					pMotionBase->GetTurnRates(turnRates);

					//--------------------------------------------------------------------------------------------------------------------
					// Script now has the option to make peds slow more when taking corners
					// A common problem has been getting peds to run through doorways, and this seems more prevalent in NG - perhaps
					// something do to with the consistently higher framerate?
					// When specified we halve the turn-rates passed into our corner evaluation function to encourage ped to slow down,
					// and also process a 2nd lookahead at half the distance (taking the lower value of the two), so that peds will not
					// speed up too soon once encountering a corner.
					// See url:bugstar:2069027, url:bugstar:2077746

					if( bSlowMoreForCorners || ((m_iFlags & Flag_SlowMoreForCorners)!=0) || pPed->GetPedResetFlag( CPED_RESET_FLAG_WaypointPlaybackSlowMoreForCorners ) )
					{
						turnRates.SetWalkTurnRate( turnRates.GetWalkTurnRate()*0.5f );
						turnRates.SetRunTurnRate( turnRates.GetRunTurnRate()*0.5f );
						turnRates.SetSprintTurnRate( turnRates.GetSprintTurnRate()*0.5f );

						float fMBR1 = CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(pPed, RCC_VEC3V(vLookAheadPos), CTaskNavBase::ms_fCorneringLookAheadTime, pPed->GetMotionData()->GetCurrentMbrY(), fMBR, CTaskMoveGoToPoint::ms_fAccelMBR, GetTimeStep(), moveSpeeds, turnRates);
						float fMBR2 = fMBR1;

						Vector3 vLookAheadPos2, vClosestToPed2, vSegment2;
						float fDistanceAhead2 = fMoveSpeed * CTaskNavBase::ms_fCorneringLookAheadTime*0.5f;
						if(GetPositionAhead(pPed, vLookAheadPos2, vClosestToPed2, vSegment2, fDistanceAhead2))
						{
							fMBR2 = CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(pPed, RCC_VEC3V(vLookAheadPos2), CTaskNavBase::ms_fCorneringLookAheadTime*0.5f, pPed->GetMotionData()->GetCurrentMbrY(), fMBR, CTaskMoveGoToPoint::ms_fAccelMBR, GetTimeStep(), moveSpeeds, turnRates);
						}

						fMBR = Min(fMBR1, fMBR2);
					}
					else
					{
						fMBR = CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(pPed, RCC_VEC3V(vLookAheadPos), CTaskNavBase::ms_fCorneringLookAheadTime, pPed->GetMotionData()->GetCurrentMbrY(), fMBR, CTaskMoveGoToPoint::ms_fAccelMBR, GetTimeStep(), moveSpeeds, turnRates);
					}
				}

				if(m_bOverrideMoveBlendRatio && !m_bOverrideMoveBlendRatioAllowSlowingForCorners)
					fMBR = Min(m_fOverrideMoveBlendRatio, fMBR);

				((CTaskMove*)pTaskMove)->SetMoveBlendRatio(fMBR);

				pPed->GetPedIntelligence()->SetClosestPosOnRoute(vClosestToPed, vSegment);

				// If we've created a new goto task this frame, then we also have to modify the backup task with the new MBR
				// or else the new MBR will not be seen (since the task will be created with the initial speed)
				if(bIncrementProgress)
				{
					CTask * pTask = (CTask*)GetNewTask();
					if(pTask && pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
					{
						CTask * pBackupMoveTask = ((CTaskComplexControlMovement*)GetNewTask())->GetBackupCopyOfMovementSubtask();
						Assert(pBackupMoveTask && pBackupMoveTask->IsMoveTask() && pBackupMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT);

						if(pBackupMoveTask && pBackupMoveTask->IsMoveTask() && pBackupMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
						{
							((CTaskMove*)pBackupMoveTask)->SetMoveBlendRatio(fMBR); 
							ModifyGotoPointTask(pPed, ((CTaskMoveGoToPoint*)pBackupMoveTask), fLookAheadDist);
						}
					}
				}

				pPed->SetPedResetFlag( CPED_RESET_FLAG_HasProcessedCornering, true );
			}
		}

		//else
		if( !pPed->GetPedResetFlag( CPED_RESET_FLAG_HasProcessedCornering ) )
		{
			// If no cornering is being processed, then set movement subtask back to the desired MBR
			float fMBR = AdjustWaypointMBR(wpt.GetSpeedAsMBR());
			CTaskMoveGoToPoint::ApproachMBR(fMBR, fCurrMbr, CTaskMoveGoToPoint::ms_fAccelMBR, GetTimeStep());

			if(m_bOverrideMoveBlendRatio && !m_bOverrideMoveBlendRatioAllowSlowingForCorners)
				fMBR = Min(m_fOverrideMoveBlendRatio, fMBR);

			((CTaskMove*)pTaskMove)->SetMoveBlendRatio(fMBR);

			pPed->SetPedResetFlag( CPED_RESET_FLAG_HasProcessedCornering, true );
		}
	}

	return FSM_Continue;
}

bool CTaskFollowWaypointRecording::GetPositionAhead(CPed * pPed, Vector3 & vPosAhead, Vector3 & vClosestToPed, Vector3 & vSegment, const float fDistanceAhead)
{
	if(m_iProgress > 0)
	{
		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		static dev_s32 iOffset = 3;
		// Pull back a few segments
		int iP = Max(m_iProgress-iOffset, 0);

		// Find the closest segment to the player, and get the position on the segment
		float fClosestDistSqr = FLT_MAX;
		Vector3 vClosestPos;
		//int iClosestIP = -1; //DW not used?!
		while(iP < m_iProgress && iP < m_pRoute->GetSize()-1)
		{
			const Vector3 v1 = m_pRoute->GetWaypoint(iP).GetPosition() + m_vWaypointsOffset;
			const Vector3 v2 = m_pRoute->GetWaypoint(iP+1).GetPosition() + m_vWaypointsOffset;
			const Vector3 vDiff = v2-v1;
			const float T = (vDiff.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(v1, vDiff, vPedPos) : 0.0f;
			const Vector3 pos = v1 + ((v2-v1)*T);
			const float fDistSqr = (pos-vPedPos).XYMag2();
			if(fDistSqr < fClosestDistSqr)
			{
				fClosestDistSqr = fDistSqr;
				vSegment = vDiff;
				//iClosestIP = iP; //DW : not used ?!
				vClosestPos = pos;
			}
			iP++;
		}

		vClosestToPed = vClosestPos;


		float fDistance = fDistanceAhead;
		Vector3 vLast = vClosestPos;

		// Otherwise iterate along the route
		while(fDistance >= 0.0f && iP < m_pRoute->GetSize())
		{
			// Is look-ahead point along the current route segment?
			//Vector3 vLast = m_pRoute->GetWaypoint(iP-1).GetPosition();
			const Vector3 vCurrent = m_pRoute->GetWaypoint(iP).GetPosition() + m_vWaypointsOffset;

			Vector3 vVec = vCurrent - vLast;
			const float fSegLength = vVec.Mag();

			// Point lies along this route segment
			if(fSegLength > fDistance)
			{
				vVec.Normalize();
				vPosAhead = vLast + (vVec * fDistance);
				return true;
			}

			iP++;
			fDistance -= fSegLength;
			vLast = m_pRoute->GetWaypoint(iP-1).GetPosition() + m_vWaypointsOffset;
		}

		// We reached the end of the route before finding the look-ahead position
		// Just return the last waypoint on the route
		Assert(m_pRoute->GetSize()>0);
		vPosAhead = m_pRoute->GetWaypoint( m_pRoute->GetSize()-1 ).GetPosition() + m_vWaypointsOffset;
		return true;
	}
	return false;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StatePause_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskDoNothing * pDoNothing = rage_new CTaskDoNothing(-1);
	SetNewTask(pDoNothing);
	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StatePause_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(m_iTimeBeforeResumingPlaybackMs > 0)
	{
		m_iTimeBeforeResumingPlaybackMs -= fwTimer::GetTimeStepInMilliseconds();
		if(m_iTimeBeforeResumingPlaybackMs <= 0)
		{
			m_iTimeBeforeResumingPlaybackMs = 0;
			m_bWantsToResumePlayback = true;
		}
	}
	if(m_bWantsToResumePlayback)
	{
		m_bWantsToResumePlayback = false;
		m_iTimeBeforeResumingPlaybackMs = 0;

		if(m_bAchieveHeadingBeforeResumingPlayback)
			SetState(State_AchieveHeadingBeforeResuming);
		else
			SetState(State_FollowingRoute);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StatePauseAndFacePlayer_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CPed * pPlayer = FindPlayerPed();
	Assert(pPlayer);

	if(m_bStopBeforeOrientating)
	{
		CTaskMoveStandStill * pStandStillTask = rage_new CTaskMoveStandStill(10000, true);
		SetNewTask(rage_new CTaskComplexControlMovement(pStandStillTask));
	}
	else
	{
		CTaskTurnToFaceEntityOrCoord * pTurnTask = rage_new CTaskTurnToFaceEntityOrCoord(pPlayer);
		SetNewTask(pTurnTask);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StatePauseAndFacePlayer_OnUpdate(CPed* pPed)
{
	if(m_iTimeBeforeResumingPlaybackMs > 0)
	{
		m_iTimeBeforeResumingPlaybackMs -= fwTimer::GetTimeStepInMilliseconds();
		if(m_iTimeBeforeResumingPlaybackMs <= 0)
		{
			m_iTimeBeforeResumingPlaybackMs = 0;
			m_bWantsToResumePlayback = true;
		}
	}
	if(m_bWantsToResumePlayback)
	{
		m_bWantsToResumePlayback = false;
		m_iTimeBeforeResumingPlaybackMs = 0;

		if(m_bAchieveHeadingBeforeResumingPlayback)
			SetState(State_AchieveHeadingBeforeResuming);
		else
			SetState(State_FollowingRoute);

		return FSM_Continue;
	}

	CPed * pPlayer = FindPlayerPed();
	Assert(pPlayer);
	CTask * pSubTask = GetSubTask();
	Assert(pSubTask);

	// Ensure that ped comes to a halt before achieving heading
	if(pSubTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)pSubTask;
		if(pCtrlMove->GetMoveTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL)
		{
			if(pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() < SMALL_FLOAT)
			{
				CTaskTurnToFaceEntityOrCoord * pTurnTask = rage_new CTaskTurnToFaceEntityOrCoord(pPlayer);
				SetNewTask(pTurnTask);
			}
		}
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		CTaskDoNothing * pDoNothing = rage_new CTaskDoNothing(-1);
		SetNewTask(pDoNothing);
	}
	else
	{
		if(pSubTask->GetTaskType()==CTaskTypes::TASK_DO_NOTHING)
		{
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const float fHdgToPlayer = fwAngle::GetRadianAngleBetweenPoints(vPlayerPosition.x, vPlayerPosition.y, vPedPosition.x, vPedPosition.y);
			const float fHdgDelta = SubtractAngleShorter(fHdgToPlayer, pPed->GetCurrentHeading());

			if(Abs(fHdgDelta) > 10.0f * DtoR)
			{
				CTaskTurnToFaceEntityOrCoord * pTurnTask = rage_new CTaskTurnToFaceEntityOrCoord(pPlayer);
				SetNewTask(pTurnTask);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateAchieveHeadingAtEnd_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	Assert(m_pRoute->GetSize());
	const ::CWaypointRecordingRoute::RouteData & finalWpt = m_pRoute->GetWaypoint(m_pRoute->GetSize()-1);

	CTaskMoveAchieveHeading * pHeadingTask = rage_new CTaskMoveAchieveHeading(finalWpt.GetHeading());
	SetNewTask(rage_new CTaskComplexControlMovement(pHeadingTask));

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateAchieveHeadingAtEnd_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateNavmeshBackToRoute_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	Assert(m_iProgress < m_pRoute->GetSize());
	const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);
	const float fMBR = Clamp(wpt.GetSpeedAsMBR(), MOVEBLENDRATIO_WALK, MOVEBLENDRATIO_SPRINT);
	CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(fMBR, wpt.GetPosition()+m_vWaypointsOffset);
	pNavMeshTask->SetDontStandStillAtEnd(true);
	SetNewTask( rage_new CTaskComplexControlMovement(pNavMeshTask) );

	return FSM_Continue;
}
CTask::FSM_Return CTaskFollowWaypointRecording::StateNavmeshBackToRoute_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FollowingRoute);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateExactStop_OnEnter(CPed* pPed)
{
	m_fMoveBlendRatioWhenCommencingStop = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();

	// We are at the end to we get the end waypoint hopefully
	Assert(m_pRoute->GetSize() != 0);
	// Get destination
	s32 iFinalProgress = (m_iTargetProgress >= 0) ? m_iTargetProgress : m_pRoute->GetSize()-1;
	iFinalProgress = Min(iFinalProgress, m_pRoute->GetSize()-1);
	const ::CWaypointRecordingRoute::RouteData & nextWpt = m_pRoute->GetWaypoint(iFinalProgress);
	m_iProgress = iFinalProgress;	// We have decided to stop so just stop and ignore the other waypoints

	CTaskMoveGoToPointAndStandStill* pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(AdjustWaypointMBR(nextWpt.GetSpeedAsMBR()), nextWpt.GetPosition()+m_vWaypointsOffset, 0.0f, 0.0f, false, true, nextWpt.GetHeading());
	float fDistRemaining = GetDistanceRemainingBeforeStopping(pPed, 10.0f);
	pGotoTask->SetDistanceRemaining(fDistRemaining);	// One frame less of latency

	SetNewTask(rage_new CTaskComplexControlMovement(pGotoTask));

	return FSM_Continue;
}

CTask::FSM_Return CTaskFollowWaypointRecording::StateExactStop_OnUpdate(CPed * pPed)
{
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&
		((CTaskComplexControlMovement*)GetSubTask())->GetMoveTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL &&
		((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed) &&
		((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed)->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
	{
		CTaskMoveGoToPointAndStandStill * pGotoTask = (CTaskMoveGoToPointAndStandStill*) ((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed);
		pGotoTask->SetDontCompleteThisFrame();

		const ::CWaypointRecordingRoute::RouteData & wpt = m_pRoute->GetWaypoint(m_iProgress);

		if(pGotoTask->IsStoppedAtTargetPoint())
		{
			return FSM_Quit;
		}

		float fDistRemaining = GetDistanceRemainingBeforeStopping(pPed, 10.0f);
		pGotoTask->SetDistanceRemaining(fDistRemaining);

		//Displayf("Dist remaining : %.1f\n", fDistRemaining);
		if( m_fMoveBlendRatioWhenCommencingStop < MOVEBLENDRATIO_RUN )
		{
		}
		else
		{
			// Check for completion of current segment
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const Vector3 vDist = (wpt.GetPosition()+m_vWaypointsOffset) - vPedPos;

			float fTargetHeading = wpt.GetHeading();
			if (vDist.XYMag2() > 0.1f)	// Make sure we are not close enough to the end
				fTargetHeading = fwAngle::GetRadianAngleBetweenPoints( pGotoTask->GetTarget().x, pGotoTask->GetTarget().y, vPedPos.x, vPedPos.y );

			fTargetHeading = fwAngle::LimitRadianAngleFast( fTargetHeading );
			pPed->GetMotionData()->SetDesiredHeading( fTargetHeading );

			// Current heading is not interpolated towards desired whilst ped is stopping
			// so we manually achieve this by applying extra heading change

			const float fCurrentHeading = pPed->GetMotionData()->GetCurrentHeading();
			const float fHeadingDelta = SubtractAngleShorter( fTargetHeading, fCurrentHeading );

			static dev_float fHeadingSpeed = 6.0f;
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame( fHeadingDelta * fHeadingSpeed * fwTimer::GetTimeStep() );
		}
	}

	return FSM_Continue;
}

float CTaskFollowWaypointRecording::GetDistanceRemainingBeforeStopping(CPed * pPed, float fStoppingDistance)
{
	float fInitialStoppingDistance = fStoppingDistance;

	Assert(m_iProgress < m_pRoute->GetSize());
	const ::CWaypointRecordingRoute::RouteData & currWpt = m_pRoute->GetWaypoint(m_iProgress);

	// Get destination
	const s32 iFinalProgress = (m_iTargetProgress >= 0) ? m_iTargetProgress : m_pRoute->GetSize()-1;

	Vector3 vPedToCurrent = (currWpt.GetPosition()+m_vWaypointsOffset) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fInitialSegmentMag = vPedToCurrent.XYMag();
	if(fInitialSegmentMag > fStoppingDistance)
	{
		// Dist left on the initial segment is greater than the stopping distance, so nothing to do yet
		return fInitialSegmentMag;
	}
	else
	{
		// If we're on the final route segment, and within stopping distance
		// then switch to exact stop now.
		if( m_iProgress >= iFinalProgress )
		{
			// If the ped has overshot, then clamp the distance remaining to zero
			if( m_iProgress > 0)
			{
				Vector3 vCurrentToLast = (currWpt.GetPosition()+m_vWaypointsOffset) - (m_pRoute->GetWaypoint(m_iProgress-1).GetPosition()+m_vWaypointsOffset);
				vCurrentToLast.Normalize();
				if( DotProduct( vCurrentToLast, vPedToCurrent ) < 0.0f)
				{
					return 0.0f;
				}
			}

			return fInitialSegmentMag;
		}

		// Otherwise step along the subsequent route segments, and see if we reach the target
		// within the stopping distance

		fStoppingDistance -= fInitialSegmentMag;

		s32 iLastProgress = m_iProgress;
		s32 iProgress = m_iProgress+1;
		while(iProgress <= iFinalProgress && iProgress < m_pRoute->GetSize() && fStoppingDistance > 0.0f )
		{
			Assert(iProgress < m_pRoute->GetSize());

			const ::CWaypointRecordingRoute::RouteData & wpt1 = m_pRoute->GetWaypoint(iLastProgress);
			const ::CWaypointRecordingRoute::RouteData & wpt2 = m_pRoute->GetWaypoint(iProgress);

			const float fDist = ((wpt2.GetPosition()+m_vWaypointsOffset) - (wpt1.GetPosition()+m_vWaypointsOffset)).XYMag();
			fStoppingDistance -= fDist;

			iLastProgress = iProgress;
			iProgress++;
		}
		return fInitialStoppingDistance - fStoppingDistance;
	}
}

void CTaskFollowWaypointRecording::OrientateToTarget(CPed* pPed)
{
	Assert(GetState()==State_FollowingRouteAiming || GetState()==State_FollowingRouteShooting);

	if(m_AimAtTarget.GetIsValid())
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if(pWeapon)
		{
			if(!m_bUseRunAndGun)
			{
				CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
				if(pGunTask)
				{
					pGunTask->SetTarget(m_AimAtTarget);
				}
			}
		}
	}
}
void CTaskFollowWaypointRecording::SetPedMass(CPed * pPed, const float fNewMass)
{
	Assert(fNewMass != pPed->GetMass());
	pPed->SetMass(fNewMass);
}

CTask * CTaskFollowWaypointRecording::CreateAimingOrShootingTask()
{
	CWeaponController::WeaponControllerType wcType;

	if(GetState()==State_FollowingRouteAiming)
	{
		wcType = CWeaponController::WCT_Aim;
	}
	else if(GetState()==State_FollowingRouteShooting)
	{
		wcType = CWeaponController::WCT_Fire;
	}
	else
	{
		return NULL;
	}

	Assert(!(wcType==CWeaponController::WCT_Fire && m_iFiringPatternHash==0));

	const CTaskTypes::eTaskType iAimTaskType = /*m_bUseRunAndGun ? CTaskTypes::TASK_AIM_GUN_RUN_AND_GUN :*/ CTaskTypes::TASK_AIM_GUN_ON_FOOT;

	CTaskGun* pAimTask = rage_new CTaskGun(wcType, iAimTaskType, m_AimAtTarget, 600.0f);
	pAimTask->SetFiringPatternHash(m_iFiringPatternHash);
	
	return pAimTask;
}
bool CTaskFollowWaypointRecording::ControlOnFoot(CPed * UNUSED_PARAM(pPed))
{
	return true;
}

bool CTaskFollowWaypointRecording::IsRegularWaypoint(::CWaypointRecordingRoute::RouteData wpt) const
{
	const u32 iFlags = wpt.GetFlags();
	return (iFlags == 0);
}

CTask * CTaskFollowWaypointRecording::DoActionAtWaypoint(::CWaypointRecordingRoute::RouteData wpt, CPed * pPed, const bool bSkippedThisWaypoint)
{
	if (pPed->GetCurrentMotionTask()->IsOnFoot())
	{
		if((wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump) && !bSkippedThisWaypoint)
		{
			// Force heading to orientate towards landing?
			if(ms_bForceJumpHeading && m_iProgress+1 < m_pRoute->GetSize())
			{
				const ::CWaypointRecordingRoute::RouteData & nextWpt = m_pRoute->GetWaypoint(m_iProgress+1);
				Vector3 vLandPos = nextWpt.GetPosition()+m_vWaypointsOffset;
				const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				const float fHdg = fwAngle::LimitRadianAngle( fwAngle::GetRadianAngleBetweenPoints(vLandPos.x, vLandPos.y, vPedPosition.x, vPedPosition.y) );

				pPed->SetHeading(fHdg);
				pPed->SetDesiredHeading(fHdg);
			}

			//! Use cached handhold if available.
			u32 nFlags = JF_UseCachedHandhold;

			CTaskJumpVault * pJump = rage_new CTaskJumpVault(nFlags);
			SetNewTask(pJump);
			return pJump;
		}
		else if((wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_DropDown) && !bSkippedThisWaypoint)
		{
			Assertf(false, "WptFlag_DropDown no longer supported since ANIM_GROUP_AUTODROP has been removed.");
			/*
			const Vector3 vPosAboveWpt = (wpt.GetPosition()+m_vWaypointsOffset) + Vector3(0.0f, 0.0f, 1.0f);
			CTaskDropOverEdge * pDrop = rage_new CTaskDropOverEdge(vPosAboveWpt, wpt.GetHeading());
			SetNewTask(pDrop);
			return pDrop;
			*/
			return NULL;
		}
	}
	
	return NULL;
}

/*
bool CTaskFollowWaypointRecording::HasLeftRoute(CPed * pPed)
{
	if(pPed->GetIsSwimming())
		return false;

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_JUMPVAULT)
		return false;

	// TODO: Actually check whether we've left the route by examining the distance to the current segment
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().ResetStaticCounter();
		return true;
	}
	return false;
}
*/


