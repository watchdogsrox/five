#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Movement/TaskGotoPointAvoidance.h"
#include "Task/Movement/TaskGoto.h"

#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskQuadLocomotion.h"
#include "Task/Movement/TaskNavBase.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Peds/MovingGround.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"

// Rage headers
#include "Math/angmath.h"

AI_OPTIMISATIONS()
AI_NAVIGATION_OPTIMISATIONS()

const float CTaskMoveGoToPoint::ms_fTargetRadius=0.5f;
float CTaskMoveGoToPoint::ms_fTargetHeightDelta = 2.0f;

//////////////////////////////////////////////////////////////////////////
//
//sAvoidanceSettings
//
//////////////////////////////////////////////////////////////////////////

sAvoidanceSettings::sAvoidanceSettings()
: m_fMaxAvoidanceTValue(CTaskMoveGoToPoint::ms_MaxAvoidanceTValue)
, m_bAllowNavmeshAvoidance(true)
, m_fDistanceFromPathToEnableNavMeshAvoidance(FLT_MAX)
, m_fDistanceFromPathToEnableObstacleAvoidance(FLT_MAX)
, m_fDistanceFromPathToEnableVehicleAvoidance(FLT_MAX)
, m_fDistanceFromPathTolerance(1.0f)
{
}

//////////////////////////////////////////////////////////////////////////
//
//CTaskMoveGoToPoint
//
//////////////////////////////////////////////////////////////////////////

CTaskMoveGoToPoint::CTaskMoveGoToPoint
(const float fMoveBlend, const Vector3& vTarget, const float fTargetRadius, const bool bSlowSmoothlyApproachTarget)
: CTaskMoveBase(fMoveBlend),
  m_vTarget(vTarget),
  m_vLookAheadTarget(VEC3_LARGE_FLOAT),
  m_fTargetRadius(fTargetRadius),
  m_bSlowSmoothlyApproachTarget(bSlowSmoothlyApproachTarget)  
{
#if __ASSERT
	AssertIsValidGoToTarget(vTarget);
#endif

	Init();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GO_TO_POINT);
}

#if __ASSERT
void CTaskMoveGoToPoint::AssertIsValidGoToTarget(const Vector3 & vPos)
{
	Assert(rage::FPIsFinite(vPos.x)&&rage::FPIsFinite(vPos.y)&&rage::FPIsFinite(vPos.z));
}
#endif

void CTaskMoveGoToPoint::Init()
{
	m_bFirstTime = true;
	m_bAborting = false;
	m_bSuccessful = false;
	m_bDontCompleteThisFrame = false;
	m_bNewTarget = false;
	m_bDoingIK = false;
	m_iTargetSideFlags = 0;
	m_bHasDoneProcessMove = false;
	m_bFoundGroundUnderLastLineTest = true;
	m_bUseLineTestForDrops = false;
	m_bFoundGroundUnderLastLineTest = false;
	m_bHeadOnCollisionImminent = false;
	m_bExpandHeightDeltaInWater = true;
	m_bAvoidPeds = true;
	m_bDontExpandHeightDeltaInWater = false;

	m_vLastLineTestPos = Vector3(9999.0f,9999.0f,9999.0f);
	m_fTargetHeightDelta = ms_fTargetHeightDelta;

#if !__FINAL
	m_pSeparationDebug = NULL; 
#endif

	m_vSegmentStart = Vector3(0.0f, 0.0f, 0.0f);
	m_vSeparationVec = Vector3(0.0f, 0.0f, 0.0f);
	m_GenerateSepVecTimer.Set(fwTimer::GetTimeInMilliseconds(), 0);

	m_pDontAvoidEntity = NULL;
}


CTaskMoveGoToPoint::~CTaskMoveGoToPoint()
{
#if !__FINAL
	if(m_pSeparationDebug)
		delete m_pSeparationDebug;
#endif
}

#if !__FINAL
void CTaskMoveGoToPoint::Debug() const
{
#if DEBUG_DRAW
	const Vector3 pedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());;
	
	static bool s_RunLocalAvoidance = true;
	if ( s_RunLocalAvoidance )
	{
		if(GetPed() == CPedDebugVisualiserMenu::GetFocusPed())
		{
			TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawPedAvoidanceTarget, true);
			if(s_DrawPedAvoidanceTarget)
			{
				Vector3 vModifiedTarget = m_vTarget;
				ModifyTargetForPathTolerance(vModifiedTarget, *GetPed());

				grcDebugDraw::Line(pedPos, vModifiedTarget, Color_yellow);
				grcDebugDraw::Sphere(vModifiedTarget, m_fTargetRadius, Color_yellow, false);

				Vector3 vToTarget = vModifiedTarget - pedPos;
				vToTarget.Normalize();

				TaskGotoPointAvoidance::sAvoidanceResults r;
				r.vAvoidanceVec = vToTarget;
				TaskGotoPointAvoidance::sAvoidanceParams avoidanceParams(m_AvoidanceSettings);
				avoidanceParams.pPed = GetPed();
				avoidanceParams.bWandering = GetPed()->GetPedResetFlag(CPED_RESET_FLAG_Wandering);
				avoidanceParams.bShouldAvoidDeadPeds = (GetPed()->GetPedResetFlag( CPED_RESET_FLAG_Wandering ) && !GetPed()->PopTypeIsMission()) || GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_SteerAroundDeadBodies );
				avoidanceParams.vPedPos3d = GetPed()->GetTransform().GetPosition();
				avoidanceParams.vUnitSegment = Vec2V(m_vUnitPathSegment.x, m_vUnitPathSegment.y);
				avoidanceParams.vUnitSegmentRight = Vec2V(m_vUnitPathSegment.y, -m_vUnitPathSegment.x);
				avoidanceParams.vSegmentStart = Vec2V(m_vSegmentStart.x, m_vSegmentStart.y);
				avoidanceParams.fScanDistance = ScalarV(CTaskMoveGoToPoint::ms_fAvoidanceScanAheadDistance);
				avoidanceParams.fDistanceFromPath = ScalarV(m_vClosestPosOnSegment.Dist(pedPos));
				avoidanceParams.vToTarget = Vec2V(vToTarget.x, vToTarget.y);
				avoidanceParams.pDontAvoidEntity = m_pDontAvoidEntity;
				TaskGotoPointAvoidance::ProcessAvoidance(r, avoidanceParams);

				const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
				grcDebugDraw::Line(pedPos + z_Offset, pedPos + r.vAvoidanceVec + z_Offset, Color_LightBlue);
			}
		}
	}
	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawPedToTarget, true);
	if ( s_DrawPedToTarget )
	{	
		grcDebugDraw::Line(pedPos, m_vTarget, Color_BlueViolet);
	}
	
	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawTarget, true);
	if ( s_DrawTarget )
	{
		grcDebugDraw::Sphere(m_vTarget, m_fTargetRadius, Color_blue, false);
	}

	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawLookAhead, true);
	if ( s_DrawTarget )
	{
		grcDebugDraw::Sphere(m_vLookAheadTarget, m_fTargetRadius, Color_brown, false);
	}
	
	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawSegment, false);
	if ( s_DrawSegment )
	{
		grcDebugDraw::Line(m_vSegmentStart, m_vTarget, Color_CadetBlue4);
	}

	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawNewSegment, true);
	if ( s_DrawNewSegment )
	{
		if ( GetPed()->GetPedIntelligence()->GetHasClosestPosOnRoute() )
		{
			grcDebugDraw::Line(GetPed()->GetPedIntelligence()->GetClosestPosOnRoute(), GetPed()->GetPedIntelligence()->GetClosestPosOnRoute() + GetPed()->GetPedIntelligence()->GetClosestSegmentOnRoute(), Color_CadetBlue4);
		}
	}

	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawSeperationTarget, false);
	if ( s_DrawSeperationTarget )
	{
		grcDebugDraw::Line(pedPos, m_vSeparationTarget, Color_brown);
	}

	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawMaxTValue, false);
	if ( s_DrawMaxTValue )
	{
		Vector3 Velocity = GetPed()->GetVelocity();
		Velocity.z = 0.0f;
		Vector3 Direction = Velocity;
		Direction.Normalize();
		float length = m_AvoidanceSettings.m_fMaxAvoidanceTValue * Velocity.XYMag();
		grcDebugDraw::Line(pedPos, pedPos + Direction * length, Color_purple);
	}
	
	TUNE_GROUP_BOOL(TASK_GOTO_POINT_DISPLAY, s_DrawSteering, false);
	if ( s_DrawSteering )
	{
		// check for same edge
		for ( int j = 0; j < GetPed()->GetNavMeshTracker().m_NavMeshLosChecks.GetCount(); j++ )
		{
			const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
			grcDebugDraw::Sphere(GetPed()->GetNavMeshTracker().m_NavMeshLosChecks[j].m_Vertex1 + z_Offset, 0.125f, Color_blue, false);
			grcDebugDraw::Sphere(GetPed()->GetNavMeshTracker().m_NavMeshLosChecks[j].m_Vertex2 + z_Offset, 0.125f, Color_green, false);
			grcDebugDraw::Line(GetPed()->GetNavMeshTracker().m_NavMeshLosChecks[j].m_Vertex1 + z_Offset, GetPed()->GetNavMeshTracker().m_NavMeshLosChecks[j].m_Vertex2 + z_Offset, Color_brown);			
		}
	}


#if __BANK
	if(GetPed()->GetPedIntelligence()->GetHasClosestPosOnRoute())
		grcDebugDraw::Cross(VECTOR3_TO_VEC3V(GetPed()->GetPedIntelligence()->GetClosestPosOnRoute()), 0.125f, Color_LightBlue);
#endif

#endif
}
#endif

bool CTaskMoveGoToPoint::CheckHasAchievedTarget( const Vector3 & vTarget, CPed * pPed )
{
	CTaskMotionBase * pMotionTask = pPed->GetCurrentMotionTask();

	static const ScalarV fZero(V_ZERO);
	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	Vec3V vDiff = RCC_VEC3V(vTarget) - vPedPos;

	ScalarV fTargetHeightDelta( m_fTargetHeightDelta ); 

	if( !m_bDontExpandHeightDeltaInWater && ((m_bExpandHeightDeltaInWater && pPed->GetIsSwimming()) || pMotionTask->IsUnderWater()) )
	{
		fTargetHeightDelta = Max( fTargetHeightDelta, ScalarV(V_FOUR) );
	}

	ScalarV fDist = MagXYFast( vDiff );
	ScalarV fExtraRadius( pMotionTask->GetExtraRadiusForGotoPoint() );
	ScalarV fTargetRadius( Add( ScalarV(m_fTargetRadius), fExtraRadius) );

	if( !m_bDontCompleteThisFrame )
	{
		// Within target height delta
		if( (Abs(vDiff.GetZ()) < fTargetHeightDelta).Getb() )
		{
			// Within XY distance ?
			if( (fDist < fTargetRadius).Getb() )
			{
				const bool bStopping =
					pMotionTask->IsOnFoot() &&
					pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION &&
					((CTaskMotionBasicLocomotion*)pMotionTask)->GetIsStopping();

				if(!bStopping)
					return true;
			}

			// Test sides
			if( (vDiff.GetX() < fZero).Getb() ) m_iTargetSideFlags |= SIDEFLAG_RIGHT;
			else if( (vDiff.GetX() > fZero).Getb() ) m_iTargetSideFlags |= SIDEFLAG_LEFT;
			if( (vDiff.GetY() < fZero).Getb() ) m_iTargetSideFlags |= SIDEFLAG_FRONT;
			else if( (vDiff.GetY() > fZero).Getb() ) m_iTargetSideFlags |= SIDEFLAG_BACK;

			// Have we circled the target ?
			if( m_iTargetSideFlags == SIDEFLAG_ALL )
			{
				const bool bStopping =
					pMotionTask->IsOnFoot() &&
					pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION &&
					((CTaskMotionBasicLocomotion*)pMotionTask)->GetIsStopping();

				if(!bStopping)
					return true;
			}
		}
	}

	return false;
}

bank_float CTaskMoveGoToPoint::ms_AngleRelToForwardAvoidanceWeight = 1.0f;
bank_float CTaskMoveGoToPoint::ms_TimeOfCollisionAvoidanceWeight = 1.0f;
bank_float CTaskMoveGoToPoint::ms_DistanceFromPathAvoidanceWeight = 1.0f;
bank_float CTaskMoveGoToPoint::ms_AngleRelToDesiredAvoidanceWeight = 1.0f;
bank_float CTaskMoveGoToPoint::ms_MaxAvoidanceTValue = TWO_PI;
bank_float CTaskMoveGoToPoint::ms_CollisionRadiusExtra = 0.2f;
bank_float CTaskMoveGoToPoint::ms_CollisionRadiusExtraTight = 0.0f;
bank_float CTaskMoveGoToPoint::ms_TangentRadiusExtra = 0.25f;
bank_float CTaskMoveGoToPoint::ms_TangentRadiusExtraTight = -0.1f;
bank_float CTaskMoveGoToPoint::ms_MaxDistanceFromPath = 4.0f;
bank_float CTaskMoveGoToPoint::ms_MinDistanceFromPath = 1.5f;

bank_float CTaskMoveGoToPoint::ms_NoTangentAvoidCollisionAngleMin = 60.0f;
bank_float CTaskMoveGoToPoint::ms_NoTangentAvoidCollisionAngleMax = 90.0f;
bank_float CTaskMoveGoToPoint::ms_LOSBlockedAvoidCollisionAngle = 15.0f;
bank_float CTaskMoveGoToPoint::ms_MaxAngleAvoidanceRelToDesired = 180.0f;
bank_float CTaskMoveGoToPoint::ms_MaxRelToTargetAngleFlee = 30.0f;

bank_bool CTaskMoveGoToPoint::ms_bEnableSteeringAroundPedsBehindMe = false;

bank_bool CTaskMoveGoToPoint::ms_bEnableSteeringAroundPeds = true;
bank_bool CTaskMoveGoToPoint::ms_bEnableSteeringAroundObjects = true;
bank_bool CTaskMoveGoToPoint::ms_bEnableSteeringAroundVehicles = true;

bank_bool CTaskMoveGoToPoint::ms_bAllowStoppingForOthers			= true;


bank_float CTaskMoveGoToPoint::ms_fMaxCollisionTimeToEnableSlowdown = 1.0f;
bank_float CTaskMoveGoToPoint::ms_fMinCollisionSpeedToEnableSlowdown = 2.0f;

bank_float CTaskMoveGoToPoint::ms_fCollisionMinSpeedWalk = 0.4f;
bank_float CTaskMoveGoToPoint::ms_fCollisionMinSpeedStop = 0.0f;

bank_float CTaskMoveGoToPoint::ms_fAvoidanceScanAheadDistance			= 7.5f;
bank_float CTaskMoveGoToPoint::ms_fPedLosAheadNavMeshLineProbeDistance	= 1.0f;
bank_float CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeDistanceMax = 1.5f;
bank_float CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeDistanceMin = 0.5f;

bank_float CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeAngle	= 45.0f;

bank_float CTaskMoveGoToPoint::ms_fMinCorneringMBR					= 1.3f;
bank_float CTaskMoveGoToPoint::ms_fAccelMBR							= 100.0f; // effectively instant

bank_float CTaskMoveGoToPoint::ms_fMinMass = 4.0f;


const float CTaskMoveGoToPoint::ms_fLineTestPosDeltaSqr = (0.25f*0.25f);
const float CTaskMoveGoToPoint::ms_fTestAheadDistance = 0.5f;
bank_bool CTaskMoveGoToPoint::ms_bPerformLocalAvoidance = true;

bool CTaskMoveGoToPoint::ProcessLineTestUnderTarget(CPed* pPed, const Vector3 & vTarget)
{
	if(!m_bUseLineTestForDrops || pPed->GetIsInWater())
		return true;

	static const Vector3 vTestVec(0.0f,0.0f,-4.0f);

	Vector3 vToTarget = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vToTarget.z = 0.0f;
	if(vToTarget.Mag2() < 0.01f)
	{
		m_vLastLineTestPos = vTarget;
		m_bFoundGroundUnderLastLineTest = true;
		return true;
	}

	const float fDistToTarget = NormalizeAndMag(vToTarget);
	const Vector3 vTestPos = vTarget + (vToTarget* rage::Min(fDistToTarget, ms_fTestAheadDistance));
	const float fDiff2 = (vTestPos - m_vLastLineTestPos).XYMag2();

	if(fDiff2 < ms_fLineTestPosDeltaSqr)
		return m_bFoundGroundUnderLastLineTest;

#if __BANK && DEBUG_DRAW
	if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging||CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText)
	{
		grcDebugDraw::Line(vTestPos, vTestPos+vTestVec, Color_WhiteSmoke, Color_wheat);
	}
#endif

	const u32 iTestFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vTestPos, vTestPos+vTestVec);
	probeDesc.SetExcludeEntity(pPed);
	probeDesc.SetIncludeFlags(iTestFlags);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	m_bFoundGroundUnderLastLineTest = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	m_vLastLineTestPos = vTestPos;
	return m_bFoundGroundUnderLastLineTest;
}

void CTaskMoveGoToPoint::SetTarget(const CPed* UNUSED_PARAM(pPed), Vec3V_In vTarget, const bool bForce) 
{
#if __ASSERT
	AssertIsValidGoToTarget(VEC3V_TO_VECTOR3(vTarget));
#endif

	if ( bForce || !m_vTarget.IsClose(VEC3V_TO_VECTOR3(vTarget), 0.01f))
	{
		SetTargetDirect(vTarget);
	}
}

void CTaskMoveGoToPoint::SetLookAheadTarget(const CPed* UNUSED_PARAM(pPed), const Vector3& vTarget, const bool UNUSED_PARAM(bForce)) 
{
#if __ASSERT
	AssertIsValidGoToTarget(vTarget);
#endif

	m_vLookAheadTarget = vTarget;
}

void CTaskMoveGoToPoint::SetTargetRadius(const float fTargetRadius ) 
{
	m_fTargetRadius = fTargetRadius;
}

float CTaskMoveGoToPoint::ApproachMBR(const float fMBR, const float fTargetMBR, const float fSpeed, const float fTimeStep)
{
	float fNewMBR = fMBR;
	Approach(fNewMBR, fTargetMBR, fSpeed, fTimeStep); 
	return fNewMBR;
}

float CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(const CPed * pPed, Vec3V_ConstRef vPosAhead, const float fLookAheadTime, const float fInputCurrentMBR, const float fMaxMBR, const float fAccelMBR, float fTimeStep, const CMoveBlendRatioSpeeds& moveSpeeds, const CMoveBlendRatioTurnRates& turnRates)
{
	// For peds who are just starting to move, we should return the MBR exactly as required by the move task
	// Otherwise we risk triggering a slower movestart than is required by the AI.
	CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
	if(pMotionTask && pMotionTask->ShouldDisableSlowingDownForCornering())
	{
		return fMaxMBR;
	}

	const Vector3 & vAnimatedVel = pPed->GetPreviousAnimatedVelocity();
	const float fForwardsVel = Max(vAnimatedVel.y, 1.0f);	// assert if negative?

	float fCurrentMBR = Clamp(CTaskMotionBase::GetMoveBlendRatioForDesiredSpeed(moveSpeeds.GetSpeedsArray(), fForwardsVel), MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);

	// use the maximum of our input MBR, and the MBR we've calculated from our velocity
	fCurrentMBR = Max(fCurrentMBR, fInputCurrentMBR);

	// The old code was doing this:
	//	const float fMaxTurnSpeedAtThisMBR = 
	//		(fCurrentMBR <= 1.0f) ? ms_fTurnRateAtMoveBlendRatio[0] :
	//			(fCurrentMBR <= 2.0f) ? Lerp(fCurrentMBR-1.0f, ms_fTurnRateAtMoveBlendRatio[0], ms_fTurnRateAtMoveBlendRatio[1]) :
	//				Lerp(fCurrentMBR-2.0f, ms_fTurnRateAtMoveBlendRatio[1], ms_fTurnRateAtMoveBlendRatio[2]);
	// but it should be faster to keep the pipeline going by just lerping all the values and selecting between them.
	const float fCurrentMbrMin1 = fCurrentMBR - 1.0f;
	const float fCurrentMbrMin2 = fCurrentMBR - 2.0f;
	const float fTurnRate0 = turnRates.GetWalkTurnRate();
	const float fTurnRate1 = Lerp(fCurrentMbrMin1, fTurnRate0, turnRates.GetRunTurnRate());
	const float fTurnRate2 = Lerp(fCurrentMbrMin2, turnRates.GetRunTurnRate(), turnRates.GetSprintTurnRate());
	const float fMaxTurnSpeedAtThisMBR = Selectf(-fCurrentMbrMin1, fTurnRate0, Selectf(-fCurrentMbrMin2, fTurnRate1, fTurnRate2));

	const float fMoveSpeed = CTaskMotionBase::GetActualMoveSpeed(moveSpeeds.GetSpeedsArray(), fCurrentMBR);

	// Get the matrix by reference, rather than using GetTransform().GetPosition() - that way, we shouldn't
	// have to go through vector registers and can load straight as floats from memory.
	const Mat34V& mtrx = pPed->GetMatrixRef();

	// Calc the heading towards to look ahead target

	const float posAheadX = vPosAhead.GetXf();
	const float posAheadY = vPosAhead.GetYf();
	const float pedPositionX = mtrx.GetCol3ConstRef().GetXf();
	const float pedPositionY = mtrx.GetCol3ConstRef().GetYf();

	const float pedToLookAheadPosX = posAheadX - pedPositionX;
	const float pedToLookAheadPosY = posAheadY - pedPositionY;

	const float distToLookAheadPosSq = square(pedToLookAheadPosX) + square(pedToLookAheadPosY);
	if(distToLookAheadPosSq > 0.0001f)
	{
		const float fLookAheadHdg = fwAngle::GetRadianAngleBetweenPoints(posAheadX, posAheadY, pedPositionX, pedPositionY);

		// Find the delta between that and out current heading
		const float fAngleDelta = SubtractAngleShorter(pPed->GetCurrentHeading(), fLookAheadHdg);

		// This shouldn't be needed, since we should already have an angle between -PI and PI:
		//	fAngleDelta = fwAngle::LimitRadianAngleSafe(fAngleDelta);

		const float fAbsAngleDelta = Abs(fAngleDelta);

		// Note: the code below would be an alternate way to compute fAbsAngleDelta, using a dot product
		// and acos() instead. It's probably a little faster now, but:
		// - A. GetRadianAngleBetweenPoints() needs to be optimized anyway, and after that,
		//		it may not be too much worse to do the atan2() than the acos().
		// - B. Using acos() here may be a bit inaccurate for angles close to zero, since the
		//		dot product we are using is close to one over some range.
		// - C. We open ourselves up to problems due to discrepancies between GetCurrentHeading()
		//		(does it always match the current matrix, and is it computed from the front direction
		//		or in the somewhat funky way it's done by fwMatrixTransform::GetHeading()?
		//	const float fwdX = mtrx.GetCol1ConstRef().GetXf();
		//	const float fwdY = mtrx.GetCol1ConstRef().GetYf();
		//	const float dot = pedToLookAheadPosX*fwdX + pedToLookAheadPosY*fwdY;
		//	const float magSq = distToLookAheadPosSq*(square(fwdX) + square(fwdY));
		//	const float fAbsAngleDelta = rage::AcosfSafe(dot*invsqrtf(Max(magSq, SMALL_FLOAT)));

		const float fTurnRate = fMaxTurnSpeedAtThisMBR;

		const float fReqTurnTime = fAbsAngleDelta / fTurnRate;

		//printf("***************************************\n");
		//printf("fAngleDelta: %.1f fReqHdgSpeed: %.1f fHdgChangeRate: %.1f fRatio: %.1f\n", fAngleDelta, fReqHdgSpeed, fReqHdgSpeed, fRatioToMaxTurn);

		// Note: this used to be
		//	const float fRatioToMaxTurn = (fReqTurnTime / fLookAheadTime) * fMoveSpeed;
		//	if(fRatioToMaxTurn > SMALL_FLOAT)
		//	{
		//		const float fDesiredSpeed = fMoveSpeed / fRatioToMaxTurn;
		// but we can find an equivalent division for fDesiredSpeed, and then fRatioToMaxTurn would only be used
		// for the if check, in which case we can multiply instead of dividing, which should be cheaper.
		if(fReqTurnTime*fMoveSpeed > SMALL_FLOAT*fLookAheadTime)
		{
			// If running, prohibit us from crossing boundary between run/walk unless the delta angle is above some threshold
			static dev_float fAngleDeltaThreshold = PI * 0.5f;
			const float fRunThreshold = MBR_RUN_BOUNDARY + 0.001f;

			const float fMinMBR = Selectf(fMaxMBR - fRunThreshold, Selectf(fAbsAngleDelta - fAngleDeltaThreshold, MOVEBLENDRATIO_WALK, fRunThreshold), MOVEBLENDRATIO_WALK);

			const float fDesiredSpeed = fLookAheadTime/fReqTurnTime;

			float fNewMbr = CTaskMotionBase::GetMoveBlendRatioForDesiredSpeed(moveSpeeds.GetSpeedsArray(), fDesiredSpeed);

			fNewMbr = Clamp( fNewMbr, MOVEBLENDRATIO_WALK, Min(fMaxMBR, MOVEBLENDRATIO_SPRINT) );
			fNewMbr = Max( fNewMbr, fMinMBR );
			return fNewMbr;
		}
	}
	float fNewMbr = Clamp( ApproachMBR(fInputCurrentMBR, fMaxMBR, fAccelMBR, fTimeStep), MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT );
	return fNewMbr;
}

void CTaskMoveGoToPoint::ModifyTargetForPathTolerance(Vector3 &io_targetPos, const CPed& in_Ped ) const
{
	static float s_OffsetScalar = .6f;
		
	const Vec2V vUnitSegment = VECTOR3_TO_VEC3V(m_vUnitPathSegment).GetXY();
	const Vec2V vUnitSegmentRight = Vec2V(vUnitSegment.GetY(), -vUnitSegment.GetX());

	const Vec2V vPedPos = in_Ped.GetTransform().GetPosition().GetXY();
	const Vec2V vClosestPos = VECTOR3_TO_VEC3V(m_vClosestPosOnSegment).GetXY();
	const Vec2V vClosesPosToPed = vPedPos - vClosestPos;

	// basically assume the segment is s_MaxDistanceFromPath wide.  compute
	// closest point within that range and apply a scalar to pull us back to the center
	const ScalarV vProjectRight = Dot(vUnitSegmentRight, vClosesPosToPed);

	Vec2V vTargetSegmentOffsetRight = vUnitSegmentRight;
	Vector3 vTargetSegment = m_vLookAheadTarget - m_vTarget;
	if(vTargetSegment.XYMag2() > 0.01f)
	{
		vTargetSegmentOffsetRight = Normalize(Vec2V(vTargetSegment.y, -vTargetSegment.x));
	}

	const ScalarV vSignRight = IsGreaterThan(vProjectRight, ScalarV(0.0f)).Getb() ? ScalarV(1.0f) : ScalarV(-1.0f);
	const ScalarV vRightOffset = Min(ScalarV(m_AvoidanceSettings.m_fDistanceFromPathTolerance), Abs( vProjectRight * ScalarV(s_OffsetScalar) ) ) * vSignRight;
	const Vec2V vOffset = vRightOffset * vTargetSegmentOffsetRight;

	io_targetPos += Vector3(vOffset.GetXf(), vOffset.GetYf(), 0.0f);
}

bool CTaskMoveGoToPoint::ProcessMove(CPed* pPed)
{
#if DEBUG_DRAW
	if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAvoidance)
	{
		if(!m_pSeparationDebug)
		{
			m_pSeparationDebug = rage_new TSeparationDebug; 
			memset(m_pSeparationDebug, 0, sizeof(TSeparationDebug));
		}
		m_pSeparationDebug->m_iNumNearbyPeds = 0;
		m_pSeparationDebug->m_iNumNearbyObjects = 0;
		m_pSeparationDebug->m_vToTarget.Zero();
		m_pSeparationDebug->m_vCurrentTarget = m_vTarget;
		m_pSeparationDebug->m_vPullToRoute.Zero();
	}
	else
	{
		if(m_pSeparationDebug)
		{
			delete m_pSeparationDebug;
			m_pSeparationDebug = NULL;
		}
	}
#endif

	//----------------------------------------------------------------------------------------------

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, true);

	m_bHasDoneProcessMove = true;
	m_bHeadOnCollisionImminent = false;

	bool bFirstTime = false;
	if(m_bFirstTime)
	{
		// Store m_vSegmentStart so that we know the start/end of this line segment
		m_vSegmentStart = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
		m_vUnitSegment = m_vTarget - m_vSegmentStart;
		m_fSegmentLength = m_vUnitSegment.Mag();
		m_vUnitSegment.NormalizeSafe( VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), SMALL_FLOAT );

		// The first time we run this task, scan for potential collisions.  This helps if ped has just left, or is in close proximity to a vehicle
		pPed->GetPedIntelligence()->GetEventScanner()->ScanForEventsNow(*pPed, CEventScanner::VEHICLE_POTENTIAL_COLLISION_SCAN);

		bFirstTime = true;
		m_bFirstTime = false;
	}
	
	//---------------------------------
	// Check for successful completion

	if(m_bAborting || CheckHasAchievedTarget(m_vTarget, pPed))
	{
		m_bSuccessful = true;
		QuitIK(pPed);
		return true;
	}

	//--------------------------------------------------------------------------------------------
	// Test whether we are beyond the target, with respect to starting position.
	// Sometimes this can occur without the ped achieving the target radius - for example when the
	// ped had to steer wide to avoid pedestrians or obstacles

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	const Vector3 vStartToPed = vPedPos - m_vSegmentStart;
	if(DotProduct(m_vUnitSegment, vStartToPed) > m_fSegmentLength && !m_bDontCompleteThisFrame)
	{
		m_bSuccessful = true;
		QuitIK(pPed);
		return true;
	}

	if ( pPed->GetPedIntelligence()->GetHasClosestPosOnRoute() )
	{
		m_vClosestPosOnSegment = pPed->GetPedIntelligence()->GetClosestPosOnRoute();
		m_vUnitPathSegment = pPed->GetPedIntelligence()->GetClosestSegmentOnRoute();
	}
	else
	{
		m_vClosestPosOnSegment = vPedPos;
		m_vUnitPathSegment = m_vTarget - m_vSegmentStart;
	}
		
	m_vUnitPathSegment.z = 0;
	m_vUnitPathSegment.NormalizeSafe( VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), SMALL_FLOAT );

	//------------------------------------------------------------------------------------
	// Set the moveblendratio to be the initial value.
	// It will be over-ridden below if necessary (for cornering, slowing for peds, etc)
	m_fMoveBlendRatio = m_fInitialMoveBlendRatio;
	m_vSeparationTarget = m_vTarget;
	m_vSeparationVec = m_vTarget - vPedPos;
	float fTimeToCollide = FLT_MAX;
	float fCollisionSpeed = 0.0f;

	// If we are running with low LOD physics, then we use the ProcessPhysics() of this task to apply movement forces to the ped
	if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
	}
	else
	{
		Vector3 vModifiedTarget = m_vSeparationTarget;
		ModifyTargetForPathTolerance(vModifiedTarget, *pPed);

		Vector3 vToTarget = vModifiedTarget - vPedPos;

		if(!pPed->GetIsSwimming())
			vToTarget.z = 0.0f;

		if(vToTarget.XYMag2() > 0.001f)
		{
			NormalizeAndMag(vToTarget);

			if(m_GenerateSepVecTimer.IsOutOfTime() && ms_bPerformLocalAvoidance && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePedAvoidance) )
			{
				if (!m_bAvoidPeds)
				{
					pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableSteeringAroundPeds, true);
				}

				TaskGotoPointAvoidance::sAvoidanceResults results;
				results.vAvoidanceVec = vToTarget;
				TaskGotoPointAvoidance::sAvoidanceParams avoidanceParams(m_AvoidanceSettings);
				avoidanceParams.pPed = pPed;
				avoidanceParams.bWandering = pPed->GetPedResetFlag(CPED_RESET_FLAG_Wandering);
				avoidanceParams.bShouldAvoidDeadPeds = (pPed->GetPedResetFlag( CPED_RESET_FLAG_Wandering ) && !pPed->PopTypeIsMission()) || GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_SteerAroundDeadBodies );
				avoidanceParams.vPedPos3d = pPed->GetTransform().GetPosition();
				avoidanceParams.vUnitSegment = Vec2V(m_vUnitPathSegment.x, m_vUnitPathSegment.y);
				avoidanceParams.vUnitSegmentRight = Vec2V(m_vUnitPathSegment.y, -m_vUnitPathSegment.x);
				avoidanceParams.vSegmentStart = Vec2V(m_vSegmentStart.x, m_vSegmentStart.y);
				avoidanceParams.fScanDistance = ScalarV(CTaskMoveGoToPoint::ms_fAvoidanceScanAheadDistance);	
				avoidanceParams.fDistanceFromPath = ScalarV(m_vClosestPosOnSegment.Dist(vPedPos));
				avoidanceParams.vToTarget = Vec2V(vToTarget.x, vToTarget.y);
				avoidanceParams.pDontAvoidEntity = m_pDontAvoidEntity;
					
				if ( bFirstTime )
				{
					// probe in runstart direction
					pPed->GetNavMeshTracker().RequestLosCheck(m_vSeparationVec * ms_fAvoidanceScanAheadDistance);	
				}

				if ( TaskGotoPointAvoidance::ProcessAvoidance(results, avoidanceParams ) )
				{
					fTimeToCollide = results.fOutTimeToCollide;
					fCollisionSpeed = results.fOutCollisionSpeed;
					m_vSeparationVec = results.vAvoidanceVec;
					m_vSeparationTarget = vPedPos + m_vSeparationVec;			
					pPed->GetNavMeshTracker().RequestLosCheck(m_vSeparationVec * ms_fAvoidanceScanAheadDistance);						
				}					
			}
		}
	}


	ComputeMoveBlend(pPed, m_vSeparationTarget);
	ComputeHeadingAndPitch(pPed, m_vSeparationTarget);

	//------------------------------------------------------------------------------------------------------------
	// Adjust moveblendratio based on angle to target.
	// i.e. if ped is sprinting & their target is 90degs to right, then slow down so can turn better
	// slow down by a maximum of 1.0 moveblendratio, which is the difference between run->walk, or sprint->run

	if(!pPed->IsStrafing() && !pPed->GetPedResetFlag( CPED_RESET_FLAG_HasProcessedCornering ) && !pPed->GetPedResetFlag( CPED_RESET_FLAG_SuppressSlowingForCorners ) )
	{
		CMoveBlendRatioSpeeds moveSpeeds;
		CMoveBlendRatioTurnRates turnRates;
		CTaskMotionBase* pMotionBase = pPed->GetCurrentMotionTask();

		pMotionBase->GetMoveSpeeds(moveSpeeds);
		pMotionBase->GetTurnRates(turnRates);

		const float fMBR = CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(pPed, RCC_VEC3V(m_vSeparationTarget), CTaskNavBase::ms_fCorneringLookAheadTime, pPed->GetMotionData()->GetCurrentMbrY(), m_fInitialMoveBlendRatio, CTaskMoveGoToPoint::ms_fAccelMBR, GetTimeStep(), moveSpeeds, turnRates);
		m_fMoveBlendRatio = fMBR;
	}
	else
	{
		m_fMoveBlendRatio = m_fInitialMoveBlendRatio;
	}

	// If this ped has decided to stop for another ped, then zero MBR
	if(ms_bAllowStoppingForOthers && !pPed->IsStrafing() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning))
	{
		if ( fTimeToCollide < ms_fMaxCollisionTimeToEnableSlowdown )
		{
			if ( fCollisionSpeed > ms_fMinCollisionSpeedToEnableSlowdown )
			{
				const Vector3 vPedFwd = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());

				static float s_MaxSlowDownDot = 0.707f; // must be smaller than 1
				float t = Clamp( fTimeToCollide, 0.0f, ms_fMaxCollisionTimeToEnableSlowdown );
				float d = Clamp( (m_vSeparationVec.Dot(vPedFwd) - s_MaxSlowDownDot) / ( 1.0f - s_MaxSlowDownDot ), 0.0f, 1.0f);

				float minSpeed = ms_fCollisionMinSpeedWalk;
				if(pPed->GetPedResetFlag( CPED_RESET_FLAG_WanderingStoppedForOtherPed ))
				{
					minSpeed = ms_fCollisionMinSpeedStop;
				}

				float tvalue = Clamp(Max(square(t), d), 0.0f, 1.0f);
				m_fMoveBlendRatio = Lerp(tvalue, minSpeed, m_fMoveBlendRatio);
			}
		}
	}

	//----------------------------------------------------------------------------
	// We can optionally test whether moving towards the target will cause this
	// ped to fall over an edge, and zero the move speed if so.
	if(!ProcessLineTestUnderTarget(pPed, m_vSeparationTarget))
	{
		m_fMoveBlendRatio = MOVEBLENDRATIO_STILL;
	}

	//-------------------------------------
	// Set IK to look at the goto target

	if(m_bNewTarget)
	{
		QuitIK(pPed);
		m_bNewTarget = false;
	}
  
	//--------------------------------------------------
	// Only do IK when running high LOD event scanning

	if( !pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning) )
	{
		SetUpHeadTrackingIK(pPed);
	}

	m_bDontCompleteThisFrame = false;

    return false;
}

void CTaskMoveGoToPoint::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// quit IK on abort
	QuitIK(pPed);

	m_bAborting=true;

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}

void CTaskMoveGoToPoint::ComputeMoveBlend(CPed* pPed, const Vector3 & vTarget)
{
	if(pPed->IsPlayer())
	{
		if(m_bSlowSmoothlyApproachTarget && !pPed->GetIsCrouching() && !pPed->GetPlayerInfo()->AreControlsDisabled())
		{
			m_fMoveBlendRatio = ComputePlayerMoveBlendRatio(*pPed, &vTarget);
		}
		else
		{
			m_fMoveBlendRatio = ComputePlayerMoveBlendRatio(*pPed, NULL);
		}
	}
}

float CTaskMoveBase::ComputePlayerMoveBlendRatio(const CPed& ped, const Vector3 *pvecTarget) const
{
	float fMoveBlend;

	if(pvecTarget)
	{
//		Vector3 vec3DDiff = Vector3(pvecTarget->x - ped.GetPosition().x, pvecTarget->y - ped.GetPosition().y, pvecTarget->z - ped.GetPosition().z);
		Vector3 vec3DDiff = *pvecTarget - VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

		fMoveBlend= vec3DDiff.Mag() * 2.0f;
		float fMaxRatio = m_fInitialMoveBlendRatio;
		if(fMoveBlend > fMaxRatio)
		{
			fMoveBlend = fMaxRatio;
		}
	}
	else
	{
		fMoveBlend = m_fMoveBlendRatio;
	}

	return fMoveBlend;
}

bool CTaskMoveGoToPoint::ProcessPhysics( float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice) )
{
	CPed* pPed = GetPed();

    if( !pPed->IsNetworkClone() && pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) )
	{
		Vec3V vVelocity = VECTOR3_TO_VEC3V(m_vSeparationTarget) - pPed->GetTransform().GetPosition();
		if (IsGreaterThan(MagSquared(vVelocity), ScalarV(V_FLT_SMALL_6)).Getb())
		{
			vVelocity = Normalize(vVelocity);

			if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask) )
			{
				const float fTargetHeading = fwAngle::LimitRadianAngle(rage::Atan2f(-vVelocity.GetXf(), vVelocity.GetYf()));
				const float fDeltaWithCurrent = SubtractAngleShorter(pPed->GetMotionData()->GetCurrentHeading(), fTargetHeading);

				if(Abs(fDeltaWithCurrent) < CTaskMotionBasicLocomotionLowLod::ms_fHeadingMoveThreshold)
				{
					vVelocity = Scale(vVelocity, Mag(NMovingGround::GetPedDesiredVelocity(pPed)));
					NMovingGround::SetPedDesiredVelocity(pPed, vVelocity);
				}
				else
				{
					NMovingGround::SetPedDesiredVelocity(pPed, Vec3V(V_ZERO));
				}
			}
			else
			{
				vVelocity = Scale(vVelocity, Mag(NMovingGround::GetPedDesiredVelocity(pPed)));
				NMovingGround::SetPedDesiredVelocity(pPed, vVelocity);
			}
		}
	}
	return true;
}

void CTaskMoveGoToPoint::ComputeHeadingAndPitch(CPed* pPed, const Vector3 & vTarget)
{
	// Code before optimization, the stuff that's below should produce the same results
	// but much faster:
	//	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	//	Vector3 vDiff = vTarget - vPedPos;
	//	Vector2 vDir;
	//	vDiff.GetVector2XY(vDir);
	//	Vector2 vDirNorm = vDir;
	//
	//	static const float fEps = 0.0001f;
	//
	//	if(vDirNorm.Mag2() > fEps)
	//	{
	//		vDirNorm.Normalize();
	//
	//		//Set the desired heading angle for the ped.
	//		m_fTheta = fwAngle::GetRadianAngleBetweenPoints(vDirNorm.x, vDirNorm.y, 0.0f, 0.0f);
	//		m_fTheta = fwAngle::LimitRadianAngleSafe(m_fTheta);
	//	}
	//
	//	//Set the desired pitch angle for the ped.
	//	if(vDiff.Mag2() > fEps)
	//	{
	//		m_fPitch = Sign(vDiff.z) * acosf( Clamp(vDir.Mag() / vDiff.Mag(), -1.0f, 1.0f) );
	//		m_fPitch = fwAngle::LimitRadianAngleSafe(m_fPitch);
	//
	//		const bool bSwimmingOnSurface = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming) &&
	//			pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest() &&
	//			pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_MOTION_SWIMMING;
	//
	//		if(bSwimmingOnSurface)
	//			m_fPitch = Min(m_fPitch, 0.0f);
	//	}

	// Note: we could potentially use the vector pipeline here, but most of the operations
	// would have dependencies that would stall the pipeline and probably not be much faster
	// than using the floating point pipeline. An exception may be for the acos/atan2 computation,
	// which perhaps would be inlined and better pipelined using vectors.

	static const float fEps = 0.0001f;

	// Load all the floats that we need.
	Vec3V_ConstRef pedPos = pPed->GetMatrixRef().GetCol3ConstRef();
	const float oldTheta = m_fTheta;
	const float oldPitch = m_fPitch;
	const float pedPosX = pedPos.GetXf();
	const float pedPosY = pedPos.GetYf();
	const float pedPosZ = pedPos.GetZf();
	const float tgtPosX = vTarget.x;
	const float tgtPosY = vTarget.y;
	const float tgtPosZ = vTarget.z;

	// Compute the delta to the target, the squared magnitude in the XY plane,
	// and the squared magnitude of the whole vector.
	const float negDeltaX = pedPosX - tgtPosX;
	const float deltaY = tgtPosY - pedPosY;
	const float deltaZ = tgtPosZ - pedPosZ;
	const float deltaMagXYSq = negDeltaX*negDeltaX + deltaY*deltaY;
	const float deltaMagSq = deltaMagXYSq + deltaZ*deltaZ;

	// Compute the new theta angle. If deltaMagXYSq <= fEps, we use the old value.
	// Note that atan2 operations should be tolerant to (0, 0) vectors, so it should be OK
	// to pass that in, but we won't use the result in that case.
	const float newTheta = Selectf(fEps - deltaMagXYSq, oldTheta, rage::Atan2f(negDeltaX, deltaY));

	// Compute the cosine of the pitch value. We clamp using fEps here to avoid
	// division by zero, but in that case, the result won't actually be used anyway.
	const float cosPitch = Min(sqrtf(deltaMagXYSq/Max(deltaMagSq, fEps)), 1.0f);

	// Get the absolute value of the pitch angle, and then set the appropriate sign.
	const float absPitch = acosf(cosPitch);
	float newPitch = Selectf(deltaZ, absPitch, -absPitch);

	// If we are swimming on the surface, only allow downward pitch.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming))
	{
		CTask* pMotionTask = pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest();
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING)
		{
			newPitch = Min(newPitch, 0.0f);
		}
	}

	// If deltaMagSq <= fEps, we want to keep the old pitch.
	newPitch = Selectf(fEps - deltaMagSq, oldPitch, newPitch);

	m_fTheta = newTheta;
	m_fPitch = newPitch;

	// Check for NANs
#if __ASSERT
	if(m_fTheta!=m_fTheta || !rage::FPIsFinite(m_fTheta))
	{
		pPed->GetPedIntelligence()->PrintTasks();
	}
	Assertf(m_fTheta==m_fTheta, "%f is not a valid value for m_fTheta (please include TTY)", m_fTheta);
	Assertf(rage::FPIsFinite(m_fTheta), "%f is not a valid value for m_fTheta (please include TTY)", m_fTheta);

	if(m_fPitch!=m_fPitch || !rage::FPIsFinite(m_fPitch))
	{
		pPed->GetPedIntelligence()->PrintTasks();
	}
	Assertf(m_fPitch==m_fPitch, "%f is not a valid value for m_fPitch (please include TTY)", m_fPitch);
	Assertf(rage::FPIsFinite(m_fPitch), "%f is not a valid value for m_fPitch (please include TTY)", m_fPitch);
#endif
}

void CTaskMoveGoToPoint::SetUpHeadTrackingIK(CPed * pPed) 
{
	// only apply IK, if the ped is onscreen not already looking at an entity and not doing an avoid task -
	// which requires them to look at their desired destination and not their detour target
	if(!pPed->GetIsVisibleInSomeViewportThisFrame() || m_bDoingIK || pPed->GetIkManager().GetLookAtEntity())
	{
		return;
	}

	// if we're the player, only do this if we're not in a cutscene
	if(pPed->IsLocalPlayer() && CControlMgr::IsDisabledControl(&CControlMgr::GetMainPlayerControl()))
	{
		return;
	}

	// In certain situations this task will be created by a parent task which will itself have control of the 
	// IK logic.  (eg. when avoiding another ped we want to look at that ped, rather than at our target.)
	if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_COMPLEX_AVOID_ENTITY)
	{
		return;
	}

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vecToTarget = m_vTarget - vPedPos;
	float distToTargetSqr = vecToTarget.Mag2();
		
	// only if over 3 metres away
	static const float fMaxLookDistSqr = 3.0f * 3.0f;
	static const float fPedIkDiffDot = rage::Cosf(( DtoR * 22.5f));

	if(distToTargetSqr > fMaxLookDistSqr)
	{
		vecToTarget.Normalize();
		const Matrix34 mat = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		float dot = DotProduct(vecToTarget, mat.b);
		
		if(dot < fPedIkDiffDot)
		{
			Vector3 lookAtPos = m_vTarget;
			lookAtPos.z = vPedPos.z;
			// this line ensures look-at target isn't too close
			lookAtPos += vecToTarget * 2.0f;
			lookAtPos.z += 0.5f;
			
			Vector3 offsetPos = lookAtPos;
			m_bDoingIK = true;
		}
	}
}

void CTaskMoveGoToPoint::QuitIK(CPed * pPed)
{
	if(m_bDoingIK && pPed->GetIkManager().IsLooking())
	{
		pPed->GetIkManager().AbortLookAt(250);
	}
}











const float CTaskMoveGoToPointOnRoute::ms_fMinMoveBlendRatio=1.1f;
const float CTaskMoveGoToPointOnRoute::ms_fSlowDownDistance=3.0f;

CTaskMoveGoToPointOnRoute::CTaskMoveGoToPointOnRoute
(const float fMoveBlend, const Vector3& vTarget, const Vector3& vNextTarget, const bool bUseBlending, const float fTargetRadius, const bool bSlowSmoothlyApproachTarget, const float fSlowDownDistance)
: CTaskMoveGoToPoint(fMoveBlend,vTarget,fTargetRadius,bSlowSmoothlyApproachTarget),
m_vNextTarget(vNextTarget),
m_bUseBlending(bUseBlending),
m_fSlowDownDistance(fSlowDownDistance)
{
	m_fInitialMoveBlendRatio=m_fMoveBlendRatio;

#if __ASSERT
	AssertIsValidGoToTarget(vTarget);
#endif
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE);
}

void CTaskMoveGoToPointOnRoute::SetMoveBlendRatio(const float fMoveBlendRatio, const bool UNUSED_PARAM(bFlagAsChanged))
{
	m_fInitialMoveBlendRatio = Clamp(fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
}

void CTaskMoveGoToPointOnRoute::ComputeMoveBlend(CPed* pPed, const Vector3 & vTarget)
{
	if(!m_bSlowSmoothlyApproachTarget)
	{
		return;
	}
	// IF strafing, or at any MBR less or equal to walk, then we continue at that speed
	if(pPed->IsStrafing() || m_fInitialMoveBlendRatio <= MOVEBLENDRATIO_WALK)
	{
		m_fMoveBlendRatio = m_fInitialMoveBlendRatio;
		return;
	}

	//const float fSlowDownDistance = 3.0f;

	//Work out the distance from the target
	Vector3 v1 = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	v1.z = 0;
	float d = NormalizeAndMag(v1) - m_fTargetRadius;
	d = Max(d, 0.0f);

	//Test if ped is near the target.
	if(d < m_fSlowDownDistance)
	{
		const float fRatio = d / m_fSlowDownDistance;
		const float fMBR = 1.0f + ((m_fInitialMoveBlendRatio-1.0f) * fRatio);

		m_fMoveBlendRatio = fMBR;
		m_fMoveBlendRatio = Min(m_fInitialMoveBlendRatio, m_fMoveBlendRatio);
		m_fMoveBlendRatio = Max(ms_fMinMoveBlendRatio, m_fMoveBlendRatio);
	}
	else
	{
		m_fMoveBlendRatio = m_fInitialMoveBlendRatio;
	}
}

CTaskMoveGoToPointOnRoute::~CTaskMoveGoToPointOnRoute()
{
}

#if !__FINAL

void CTaskMoveGoToPointOnRoute::Debug() const
{
#if DEBUG_DRAW
	const Vector3 v1 = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	const Vector3& v2=m_vTarget;
	const Vector3& v3=m_vNextTarget;
	grcDebugDraw::Line(v1,v2,Color32(0x00,0xff,0x00,0xff));
	grcDebugDraw::Line(v2,v3,Color32(0x00,0x8f,0x00,0xff));

	CTaskMoveGoToPoint::Debug();
#endif
}
#endif











//***********************************
// CTaskMoveGoToPointAndStandStill
//

#define CHECK_HOW_CLOSE_PEDS_GET_TO_TARGET	0

const float CTaskMoveGoToPointAndStandStill::ms_fTargetRadius = 0.5f;
bank_float CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance = 2.0f;
const int CTaskMoveGoToPointAndStandStill::ms_iDefaultStandStillTime = 1;
// At 0.0f the ped will try to stop at the centre of the target, at 1.0f it will be at the outside of the target radius
const float CTaskMoveGoToPointAndStandStill::ms_fStopInsideRadiusRatio = 0.0f;
const int CTaskMoveGoToPointAndStandStill::ms_iMaximumStopTime = 1000;

CTaskMoveGoToPointAndStandStill::CTaskMoveGoToPointAndStandStill(const float fMoveBlendRatio, const Vector3& vTarget, const float fTargetRadius, const float fSlowDownDistance, const bool bMustOvershootTarget, const bool bStopExactly, const float targetHeading )
: CTaskMove(fMoveBlendRatio),
	m_fSlowDownDistance(fSlowDownDistance),
	m_fTargetHeightDelta(CTaskMoveGoToPoint::ms_fTargetHeightDelta),
	m_fInitialDistanceToTarget(0.0f),
	m_bMustOverShootTarget(bMustOvershootTarget),
	m_bStopExactly(bStopExactly),	
	m_bIsSlowingDown(false),
	m_bIsStopping(false),
	m_fSlowingDownMoveBlendRatio(fMoveBlendRatio),
	m_bTrackingEntity(false),
	m_bWasSuccessful(false),
	m_bHasOvershotTarget(false),
	m_bAbortTryingToNailPoint(false),
	m_bDontCompleteThisFrame(false),
	m_bJustSlideLastBit(false),
	m_bUseRunstopInsteadOfSlowingToWalk(true),
	m_bStopClipNoLongerMoving(false),
	m_fTargetHeading(targetHeading),
	m_fTimeSliding(0.0f),
	m_iStandStillDuration(ms_iDefaultStandStillTime),
	m_bParentTaskOveridesDistanceRemaining(false),
	m_bAvoidPeds(true),
	m_bLenientAchieveHeadingForExactStop(false),
	m_bRunStop(false),
	m_bRestartNextFrame(false),
	m_bStopDueToInsideRadius(false),
	m_fDistanceRemaining(FLT_MAX)
{
#if __ASSERT
	CTaskMoveGoToPoint::AssertIsValidGoToTarget(vTarget);
#endif

	if (m_fTargetHeading<DEFAULT_NAVMESH_FINAL_HEADING)
	{
		m_bTargetHeadingSpecified = true;
		m_bUseRunstopInsteadOfSlowingToWalk = false;
		m_fTargetHeading = fwAngle::LimitRadianAngleSafe(m_fTargetHeading);
	}
	else
	{
		m_bTargetHeadingSpecified = false;
	}
    SetTarget(NULL, VECTOR3_TO_VEC3V(vTarget));
	SetTargetRadius(fTargetRadius);
	SetSlowDownDistance(fSlowDownDistance);

	m_vLookAheadTarget = VEC3_LARGE_FLOAT;

	SetInternalTaskType(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);
}

CTaskMoveGoToPointAndStandStill::~CTaskMoveGoToPointAndStandStill()
{
}

#if !__FINAL
void CTaskMoveGoToPointAndStandStill::Debug() const
{
    if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
    {
        GetSubTask()->Debug();
    }
#if DEBUG_DRAW
	if(GetState()==EStopping)
	{
		const CPed * pPed = GetPed();
		const float fGroundHeight = pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		Vector3 vFootPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vFootPos.z -= fGroundHeight;
		Vector3 vTarget = m_vTarget;
		vTarget.z = vFootPos.z;

		grcDebugDraw::Arrow(RCC_VEC3V(vFootPos), RCC_VEC3V(vTarget), 0.1f, Color_orange);
	}
#endif
}
#endif

void CTaskMoveGoToPointAndStandStill::SetTarget(const CPed *pPed, Vec3V_In vTarget, const bool bForce) 
{
#if __ASSERT
	CTaskMoveGoToPoint::AssertIsValidGoToTarget(RCC_VECTOR3(vTarget));
#endif

    if(bForce || !IsCloseAll(VECTOR3_TO_VEC3V(m_vTarget), vTarget, ScalarV(V_FLT_SMALL_2 /*0.01f*/)))
    {
		if(pPed)
		{
			const Vec3V pedPosV = pPed->GetMatrixRef().GetCol3();
			const Vec3V deltaV = Subtract(pedPosV, vTarget);
			Vec3V deltaXYV = deltaV;
			deltaXYV.SetZ(ScalarV(V_ZERO));
			const Vec3V targetPlaneNormalV = Scale(deltaXYV, InvMag(Vec2V(deltaXYV.GetIntrin128())));
			m_vTargetPlaneNormal = VEC3V_TO_VECTOR3(targetPlaneNormalV);

			// Used to be
			//	m_vTargetPlaneNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(vTarget);
			//	m_vTargetPlaneNormal.z = 0.0f;
			//	m_vTargetPlaneNormal.Normalize();
		}

		if(m_bParentTaskOveridesDistanceRemaining)
		{
			CTask* pSubTask = GetSubTask();
			if(pSubTask && pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
			{
				// Here, we used to do this
				//	((CTaskMoveGoToPoint*)pSubTask)->SetTarget(pPed, vTarget, bForce);
				// but we basically know that the CTaskMoveGoToPoint shouldn't be further
				// derived, so we don't really need the virtual function call. Also,
				// it's unlikely that we really need the distance check that's done
				// inside of SetTarget() - if the old target was the same as the new
				// target and we didn't want to force it, we should have caught it in the
				// check we did at the beginning of this function.
				taskAssert(IsFiniteAll(vTarget));
				((CTaskMoveGoToPoint*)pSubTask)->SetTargetDirect(vTarget);
			}
		}
		else
		{
			// If we're currently trying to nail a stopping position by scaling the ped's extracted velocity,
			// then give up now.  If the target is moved whilst this happens its akin to pulling the carpet 
			// from under us - the ped may end up sliding all over the place.
			if(m_bUseRunstopInsteadOfSlowingToWalk && m_bIsStopping)
				m_bAbortTryingToNailPoint = true;
		}

        m_vTarget = VEC3V_TO_VECTOR3(vTarget);
        m_bNewTarget = true;
    }
}

void CTaskMoveGoToPointAndStandStill::SetLookAheadTarget(const CPed * pPed, const Vector3 & vTarget, const bool bForce) 
{
	m_vLookAheadTarget = vTarget;
	if(m_bParentTaskOveridesDistanceRemaining && GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
	{
		((CTaskMoveGoToPoint*)GetSubTask())->SetLookAheadTarget(pPed, vTarget, bForce);
	}
}

CTaskInfo *CTaskMoveGoToPointAndStandStill::CreateQueriableState() const
{
    return rage_new CClonedGoToPointAndStandStillTimedInfo(m_fMoveBlendRatio, m_vTarget, -1, m_fTargetHeading);
}

bool CTaskMoveGoToPointAndStandStill::CanStartIdle() const
{
	// no longer in a moving locomotion state and we failed to nail desired heading with the walk / runstop
	static dev_float s_fMaxExactStopDist = 0.025f;
	static dev_float s_fMaxExactHeadingDiff = 0.025f;
	static dev_float s_fMaxExactLenientHeadingDiff = 0.05f;

	const float fDistRemainingSqrXY =
		m_bParentTaskOveridesDistanceRemaining ?
			m_fDistanceRemaining*m_fDistanceRemaining :
			(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - m_vTarget).XYMag2();

	const float fMaxExactHeadingDiff = m_bLenientAchieveHeadingForExactStop ? s_fMaxExactLenientHeadingDiff : s_fMaxExactHeadingDiff;
	float fHeadingDiff = (m_fTargetHeading < DEFAULT_NAVMESH_FINAL_HEADING ? Abs(SubtractAngleShorter(m_fTargetHeading, GetPed()->GetCurrentHeading())) : 0.f);
	if (fDistRemainingSqrXY < s_fMaxExactStopDist*s_fMaxExactStopDist && (!m_bTargetHeadingSpecified || (fHeadingDiff < fMaxExactHeadingDiff)))
		return true;

	return false;
}

void CTaskMoveGoToPointAndStandStill::SetDistanceRemaining(const float fDist)
{
	m_bParentTaskOveridesDistanceRemaining = true;
	m_fDistanceRemaining = fDist;
}

void CTaskMoveGoToPointAndStandStill::ResetDistanceRemaining(CPed* pPed)
{
	m_bParentTaskOveridesDistanceRemaining = true;
	Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
	if (!pPed->GetIsSwimming())
		vDiff.z = 0;

	m_fDistanceRemaining = vDiff.Mag();
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::GoingToPoint_OnEnter(CPed * pPed)
{
	// Disable exact stops if we have turned off runstops
	if(!m_bUseRunstopInsteadOfSlowingToWalk && !m_bTargetHeadingSpecified)
		m_bStopExactly = false;

	m_vTargetPlaneNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
	m_vTargetPlaneNormal.z = 0.0f;
	const float fDistSqr = m_vTargetPlaneNormal.Mag2();
	if(fDistSqr > 0.01f)
		m_fInitialDistanceToTarget = NormalizeAndMag(m_vTargetPlaneNormal);
	else
		m_fInitialDistanceToTarget = 0.0f;

	m_bIsStopping = false;
	m_bNewTarget = false;

	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		Assertf(false, "%s given CTaskMoveGoToPointAndStandStill while in a vehicle!", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "Ped");
	}
	else
	{
		CTaskMoveGoToPoint * pGotoTask = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, m_vTarget, m_fTargetRadius, false );
		pGotoTask->SetTargetHeightDelta(m_fTargetHeightDelta);
		pGotoTask->SetAvoidanceSettings(m_AvoidanceSettings);
		pGotoTask->SetAvoidPeds(m_bAvoidPeds);

		if(m_bTrackingEntity || m_bDontCompleteThisFrame)
			pGotoTask->SetDontCompleteThisFrame();

		// If we start off by running but is within slowdowndistance already, don't bother run in the first place
		if(m_fSlowDownDistance > 0.0f && !m_bIsStopping && m_fMoveBlendRatio >= MOVEBLENDRATIO_RUN)
		{
			if(m_bParentTaskOveridesDistanceRemaining)
			{
				// do nothing
			}
			else
			{
				Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - pGotoTask->GetTarget();
				if(!pPed->GetIsSwimming())
					vDiff.z = 0;

				m_fDistanceRemaining = vDiff.Mag();
			}

			if (m_fDistanceRemaining < m_fSlowDownDistance)
				m_fMoveBlendRatio = 1.f;
		}

		if(m_fMoveBlendRatio >= 2.0f && m_fMoveBlendRatio < 3.0f)
		{
			SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 99999999.0f);
		}
		else
		{
			SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10.0f);
		}

		SetNewTask(pGotoTask);
	}

	m_bStopDueToInsideRadius = false;

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToPointAndStandStill::GoingToPoint_OnUpdate(CPed * pPed)
{
	if(!GetSubTask())
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	// just copy over avoidance settings in case they've changed
	if ( AssertVerify(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT) )
    {
		static_cast<CTaskMoveGoToPoint*>(GetSubTask())->SetAvoidanceSettings(m_AvoidanceSettings);
	}

	if(m_bDontCompleteThisFrame)
		((CTaskMoveGoToPoint*)GetSubTask())->SetDontCompleteThisFrame();

	if(!m_bIsStopping)
		m_bAbortTryingToNailPoint = false;

	if(m_bNewTarget && !m_bParentTaskOveridesDistanceRemaining && (!GetSubTask() || GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL)))
	{
		m_bIsStopping = false;
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_bStopExactly)
		{
			m_bIsStopping = true;
			m_bStopDueToInsideRadius = true;
			SetState(EStopping);
		}
		else
		{
			SetState(EStandingStill);
		}
	}
	else
	{
		//If the ped is near the target then make the ped walk the remainder.
		//If within 10m and sprinting, switch from sprint to run
		CTaskMoveGoToPoint * pGotoTask = (CTaskMoveGoToPoint*)GetSubTask();

		if (!pGotoTask)
		{
			SetState(EStandingStill);
			return FSM_Continue;
		}

		// I think some 10 meter is hardcoded somewhere for "m_fDistanceRemaining" when used from follow route task
		// if m_bParentTaskOveridesDistanceRemaining is set we probably did this stuff elsewhere
		if (!m_bParentTaskOveridesDistanceRemaining && m_fDistanceRemaining < CTaskMoveGoToPoint::ms_fAvoidanceScanAheadDistance)
		{
			float XYSpeed = pPed->GetVelocity().XYMag();
			if ( XYSpeed > 0.0f)
			{
				if ( GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT )
				{
					CTaskMoveGoToPoint * pGotoTask = (CTaskMoveGoToPoint*)GetSubTask();
					const ScalarV XYSpeedV = LoadScalar32IntoScalarV(XYSpeed);
					const ScalarV TValueV = InvScale( LoadScalar32IntoScalarV(m_fDistanceRemaining), XYSpeedV);
					const ScalarV maxAvoidanceTValueV = InvScale(LoadScalar32IntoScalarV(CTaskMoveGoToPoint::ms_fAvoidanceScanAheadDistance), XYSpeedV);
					pGotoTask->SetMaxAvoidanceTValue(Min(maxAvoidanceTValueV, TValueV));
				}
			}
		}

		if (m_bStopExactly)
		{
			// we want to process the stopping behaviour even when moving as this is where we determine when to stop and whether to do any sliding to nail the stop point
			ProcessStoppingBehaviour(pPed);

			if (m_bIsStopping)
			{
				pGotoTask->SetMoveBlendRatio(0.0f);

				// Disable timeslicing so that peds may stop exactly (this requires processing every frame)
				// An alternative would be to have a low lod task update function for such things.
				if(CPedAILodManager::IsTimeslicingEnabled())
				{
					pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
				}
			}
			else
			{
				if (m_fMoveBlendRatio >= MOVEBLENDRATIO_WALK && !m_bUseRunstopInsteadOfSlowingToWalk)
				{
					if(m_fMoveBlendRatio == 3.0f)
					{
						SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10.0f);
					}
					else
					{
						SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10000.0f);
					}
				}			
			}

			if(m_bIsStopping && GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, 0))
			{
				SetState(EStopping);
			}
		}
		else
		{
			bool bContinueMoving = true;

			if(m_fMoveBlendRatio == 3.0f)
			{
				bContinueMoving = SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10.0f);
			}
			else if(m_fMoveBlendRatio >= 2.0f)
			{
				bContinueMoving = SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10000.0f);
			}
			else if(m_fMoveBlendRatio >= 1.0f)
			{
				bContinueMoving = SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10000.0f);
			}
			else if(m_fMoveBlendRatio == 0.0f)
			{
				bContinueMoving = SelectMoveBlendRatio(pGotoTask, pPed, m_fSlowDownDistance, 10000.0f);
			}
			else
			{
				pGotoTask->SetMoveBlendRatio(m_fMoveBlendRatio);
			}

			if(!bContinueMoving && GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, 0))
			{
				SetState(EStandingStill);
			}
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::Stopping_OnEnter(CPed * pPed)
{
	Assert(GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT ||
		GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL));

	m_fTimeSliding = 0.0f;
	CTaskMoveStandStill* pStandTask = rage_new CTaskMoveStandStill(ms_iMaximumStopTime, false);

	SetNewTask(pStandTask);

	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();

	if (pTask)
	{
		if (pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
		{
			CTaskHumanLocomotion* pLocomotionTask = static_cast<CTaskHumanLocomotion*>(pTask);
			m_bRunStop = pLocomotionTask->IsRunning();
		}
		else if (pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_QUAD)
		{
			CTaskQuadLocomotion* pLocomotionTask = static_cast<CTaskQuadLocomotion*>(pTask);
			m_bRunStop = pLocomotionTask->IsInMotionState(CPedMotionStates::MotionState_Run) || pLocomotionTask->IsInMotionState(CPedMotionStates::MotionState_Sprint);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::Stopping_OnUpdate(CPed * pPed)
{
	if(m_bRestartNextFrame)
	{
		m_bRestartNextFrame = false;
		SetState(EGoingToPoint);
		return FSM_Continue;
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, true);

	// Disable timeslicing so that peds may stop exactly (this requires processing every frame)
	// An alternative would be to have a low lod task update function for such things.
	if(CPedAILodManager::IsTimeslicingEnabled())
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	// If a new target has been set whilst we are stopping - then restart the task.
	if(m_bNewTarget && !m_bParentTaskOveridesDistanceRemaining && (!GetSubTask() || GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL)))
	{
		SetState(EGoingToPoint);
		return FSM_Continue;
	}

	ProcessStoppingBehaviour(pPed);

	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	CTaskHumanLocomotion* pLocomotionTask = pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION ? static_cast<CTaskHumanLocomotion*>(pTask) : NULL;
	// Block changes in speed, as this will mess up our stop prediction
	if(pLocomotionTask)
		pLocomotionTask->SetBlockSpeedChangeWhilstMoving(true);

	// if a target heading has been specified, set the desired heading
	// to make the clips try to reach it (and slide if necessary)
	if (m_bIsStopping && m_bTargetHeadingSpecified)
	{
		if(!pLocomotionTask || pLocomotionTask->IsOkToSetDesiredStopHeading(this))
		{
			pPed->SetDesiredHeading(m_fTargetHeading);
		}
	}

	// no longer in a moving locomotion state
	static const float fMaxExactStopDist = 0.01f;
	static const float fMaxExactHeadingDif = 0.02f;

	const float fDistRemainingSqrXY =
		m_bParentTaskOveridesDistanceRemaining ?
			m_fDistanceRemaining*m_fDistanceRemaining :
			(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget).XYMag2();

	if (AreClipsStillMoving())
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

		if (fDistRemainingSqrXY > fMaxExactStopDist*fMaxExactStopDist || m_bJustSlideLastBit)
		{
			if ( m_bStopClipNoLongerMoving || m_bJustSlideLastBit)
			{
				// Ran out of extracted velocity in the clip! Slide to the target.
				//Printf("Slide to target in move stop state!\n");
				m_iExtractedVelOperation = ExtractedVel_SlideToTarget;
			}
			else
			{
				m_iExtractedVelOperation = ExtractedVel_OrientateAndScale;
			}
		}
		else
		{
			m_iExtractedVelOperation = ExtractedVel_ZeroXY;
		}
	}
	else
	{
		if(m_bTargetHeadingSpecified)
			pPed->GetMotionData()->SetDesiredHeading(m_fTargetHeading);

		if ( fDistRemainingSqrXY > fMaxExactStopDist*fMaxExactStopDist || (m_bTargetHeadingSpecified && (abs(SubtractAngleShorter(m_fTargetHeading, pPed->GetCurrentHeading()))>fMaxExactHeadingDif)) )
		{
			// Ran out of extracted velocity in the clip! Slide to the target.
			//Printf("final slide to target\n");
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );
			m_iExtractedVelOperation = ExtractedVel_SlideToTarget;
		}
		else
		{
			// we are there and done 
			if (GetSubTask())
				GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL );
			SetState(EStandingStill);
			//Printf("exit3\n");
		}
	}

	// Detect and workaround cases where this ped is stuck trying to slide to coord; possibly sliding up against collision
	if(m_iExtractedVelOperation == ExtractedVel_SlideToTarget || m_iExtractedVelOperation == ExtractedVel_OrientateAndScale)
	{
		static dev_float fMaxSlideTime = 6.0f;
		if( m_fTimeSliding > fMaxSlideTime )
		{
			Displayf("Ped \"%s\" (0x%p) stuck whilst in slide to coord (%i), task was forced to quit.", pPed->GetModelName(), pPed, m_iExtractedVelOperation);
			return FSM_Quit;
		}
		m_fTimeSliding += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimeSliding = 0.0f;
	}

	//------------------------------------------------------------------------------

	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopping, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling, CanStartIdle());

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::Stopping_OnExit(CPed * pPed)
{
	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
	{
		CTaskHumanLocomotion* pTaskLocomotion = static_cast<CTaskHumanLocomotion*>(pTask);
		pTaskLocomotion->SetBlockSpeedChangeWhilstMoving(false);
	}

	m_bRunStop = false;
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::StandingStill_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	if(m_iStandStillDuration==0)
	{
		return FSM_Quit;
	}

	Assert(!GetSubTask() ||
		(GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT ||
		GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL)));
	
	CTaskMoveStandStill* pStandTask = NULL;

	pStandTask = rage_new CTaskMoveStandStill(m_iStandStillDuration, false);

	SetNewTask(pStandTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::StandingStill_OnUpdate(CPed * pPed)
{
	if(m_bRestartNextFrame)
	{
		m_bRestartNextFrame = false;
		SetState(EGoingToPoint);
		return FSM_Continue;
	}

	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	// If a new target has been set whilst we are standing still - then restart the task.
	if(m_bNewTarget)
	{
		SetState(EGoingToPoint);
		return FSM_Continue;
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );
	m_iExtractedVelOperation = ExtractedVel_ZeroXY;
	m_bJustSlideLastBit = false;

	if(m_bNewTarget && (!GetSubTask() || GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL)))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	return FSM_Continue;
}

// Changes the move-state based upon how far ped is from target.
// Returns true if the ped is to continue moving, or false if they are to stop straight way (PEDMOVE_STILL)
bool CTaskMoveGoToPointAndStandStill::SelectMoveBlendRatio(CTaskMoveGoToPoint * pGoToPoint, CPed * pPed, float fRunDistance, float fSprintDistance)
{
	if(m_bParentTaskOveridesDistanceRemaining)
	{

	}
	else
	{
		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - pGoToPoint->GetTarget();
		if(!pPed->GetIsSwimming())
			vDiff.z = 0;
	
		m_fDistanceRemaining = vDiff.Mag();
	}

	Vector2 vCurrentMoveBlendRatio;
	Vector2 vDesiredMoveBlendRatio;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMoveBlendRatio);
	pPed->GetMotionData()->GetDesiredMoveBlendRatio(vDesiredMoveBlendRatio);

	float fDesiredMoveBlend = m_fMoveBlendRatio;

	CClipPlayer * pStopClip = NULL;
	bool bInterruptibleFlagReached = false;


	if( !m_bUseRunstopInsteadOfSlowingToWalk )
	{
		//--------------------------------------------------------
		// Non-runstop codepath, slows ped down to walkspeed

		if( m_fSlowDownDistance > 0.0f && !m_bIsStopping && m_fMoveBlendRatio > 0.0f )
		{
			// Should we be somewhere between walking & running ?
			if(m_fDistanceRemaining < fRunDistance)
			{
				float s = m_fDistanceRemaining / fRunDistance;
				s = Clamp(s, 0.0f, 1.0f);
				fDesiredMoveBlend = MOVEBLENDRATIO_WALK + s;
			}
			// Should we be somewhere between running & sprinting ?
			else if(m_fDistanceRemaining < fSprintDistance)
			{
				float s = (m_fDistanceRemaining-fRunDistance) / (fSprintDistance-fRunDistance);
				s = Clamp(s, 0.0f, 1.0f);
				fDesiredMoveBlend = MOVEBLENDRATIO_RUN + s;
			}
			// Else we should be sprinting..
			else
			{
				fDesiredMoveBlend = MOVEBLENDRATIO_SPRINT;
			}

			// Never move faster than the maximum specified move blend ratio
			fDesiredMoveBlend = Min(fDesiredMoveBlend, m_fMoveBlendRatio);

			m_fSlowingDownMoveBlendRatio = fDesiredMoveBlend;
			m_bIsSlowingDown = (fDesiredMoveBlend < m_fMoveBlendRatio);
		}
	}

	else
	{
		//--------------------
		// Runstop codepath

		if( m_bDontCompleteThisFrame == false )
		{
			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			taskAssert(pTask);
			if(!pPed->IsStrafing() && pTask->IsOnFoot() && !m_bIsStopping)
			{
				float fStoppingDist = pTask->GetStoppingDistance(m_fTargetHeading);
				if(m_fDistanceRemaining <= fStoppingDist + (m_fTargetRadius*ms_fStopInsideRadiusRatio))
				{
					if( fStoppingDist > m_fInitialDistanceToTarget )
					{
						m_bUseRunstopInsteadOfSlowingToWalk = false;
					}
					else
					{
						m_bIsStopping = true;
					}
				}
			}
			if(!pPed->IsStrafing())
			{
				Vector3 vToTarget = m_vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				vToTarget.z = 0.0f;
				m_bHasOvershotTarget = DotProduct(m_vTargetPlaneNormal, vToTarget) > 0.0f;
			}

			if(m_bIsStopping)
			{
				fDesiredMoveBlend = MOVEBLENDRATIO_STILL;
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
				pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

				if(m_bHasOvershotTarget)
				{
					m_iExtractedVelOperation = ExtractedVel_Zero;
				}
				else
				{
					m_iExtractedVelOperation = ExtractedVel_OrientateAndScale;
				}
			}
		}
	}

	pGoToPoint->SetMoveBlendRatio(fDesiredMoveBlend);

	const bool bComeToStop = (m_bIsStopping && vCurrentMoveBlendRatio.x == 0.0f && vCurrentMoveBlendRatio.y == 0.0f);

	if(!m_bDontCompleteThisFrame && (m_fDistanceRemaining < pGoToPoint->GetTargetRadius() || bComeToStop || m_bHasOvershotTarget) )
	{
		m_fMoveBlendRatio = 0.0f;
		m_fSlowingDownMoveBlendRatio = 0.0f;
		pGoToPoint->SetMoveBlendRatio(0.0f);
		m_bIsStopping = true;

		if(m_bUseRunstopInsteadOfSlowingToWalk)
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

			if(m_bHasOvershotTarget)
			{
				m_iExtractedVelOperation = ExtractedVel_Zero;
			}
			else
			{
				m_iExtractedVelOperation = ExtractedVel_OrientateAndScale;
			}
		}
		else
		{
			if(m_bStopExactly)
			{
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
				pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

				m_iExtractedVelOperation = ExtractedVel_ZeroXY;
			}
		}

		//**************************************************************************************************
		// When ped has stopped moving, then we can start the CTaskSimpleStandStill task.
		// This is only when both the desired & current speeds are zero, and a stop clip is not playing.
		// Additionally, we have to check that the ProcessMove() function is CTaskMoveBase has been
		// called yet - because this is where the desired & current speeds are actually set.

		if(pGoToPoint->HasDoneProcessMove() &&
			((!pStopClip && pPed->GetAnimatedVelocity().XYMag2() < 0.05f) || bInterruptibleFlagReached) &&
			vDesiredMoveBlendRatio.x == 0.0f && vDesiredMoveBlendRatio.y == 0.0f &&
			vCurrentMoveBlendRatio.x == 0.0f && vCurrentMoveBlendRatio.y == 0.0f)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	return true;
}

void CTaskMoveGoToPointAndStandStill::ProcessStoppingBehaviour(CPed * pPed)
{
	if(m_bParentTaskOveridesDistanceRemaining)
	{

	}
	else
	{
		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
		if(!pPed->GetIsSwimming())
			vDiff.z = 0;
		//float fDistSqr = vDiff.Mag2();
		m_fDistanceRemaining = vDiff.Mag();
	}

	if( m_bStopExactly )
	{
		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		taskAssert(pTask);

		if(!m_bIsStopping && pPed->GetMotionData()->GetCurrentMbrY() > 0.f)
		{
			const float fStoppingDist = pTask->GetStoppingDistance(m_fTargetHeading);
			if(m_fDistanceRemaining <= fStoppingDist + (m_fTargetRadius*ms_fStopInsideRadiusRatio))
			{
				bool bDoStop = true;
				if (!pPed->IsStrafing())
				{
					Vector3 vDiff = m_vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					vDiff.z = 0.f;
					vDiff.Normalize();
				
					if (vDiff.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward())) < 0.7f )
					{
						pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);
						bDoStop = false;
					}
				}

				if (bDoStop)
				{
					if( !m_bParentTaskOveridesDistanceRemaining && fStoppingDist > m_fInitialDistanceToTarget )
					{
						m_bUseRunstopInsteadOfSlowingToWalk = false;
					}
					else
					{
						m_bIsStopping = true;
					}
				}
			}
		}

		if(!pPed->IsStrafing() && !m_bParentTaskOveridesDistanceRemaining)
		{
			Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
			m_bHasOvershotTarget = DotProduct(m_vTargetPlaneNormal, vDiff) < 0.0f;
		}

		if(m_bHasOvershotTarget)
		{
			// JB: note that 'm_iExtractedVelOperation' will have no effect if ProcessPhysics() is not flagged to be called.
			// However I'm a bit worried about adding in the lines below in case it prevents peds from completing their task,
			// in the case that they are outside of their target radius and their velocity is then set to zero.

			//pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
			//pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

			m_iExtractedVelOperation = ExtractedVel_Zero;	//mjc - should we do the slide here? or just leave to slide if we overshot and not doing m_bStopExactly
		}
	}

	Vector2 vCurrentMoveBlendRatio;
	Vector2 vDesiredMoveBlendRatio;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMoveBlendRatio);
	pPed->GetMotionData()->GetDesiredMoveBlendRatio(vDesiredMoveBlendRatio);

	if(m_bIsStopping || m_bHasOvershotTarget)
	{
		m_fMoveBlendRatio = 0.0f;
		m_fSlowingDownMoveBlendRatio = 0.0f;
		m_bIsStopping = true;

		if(m_bStopExactly)
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMovement, true );
			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

			if(m_bHasOvershotTarget || m_bJustSlideLastBit)
			{
				m_iExtractedVelOperation = ExtractedVel_SlideToTarget;
			}
			else
			{
				m_iExtractedVelOperation = ExtractedVel_OrientateAndScale;
			}
		}
	}
}

void CTaskMoveGoToPointAndStandStill::UpdateAngularVelocity(CPed* pPed, float fRate, float fTimeStep)
{
	// Use the animated velocity to linearly interpolate the heading.
	float fDesiredHeadingChange =  SubtractAngleShorter(
		fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetDesiredHeading()), 
		fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading())
		);

	if (!IsNearZero(fDesiredHeadingChange))
	{
		float fAngVel = Mag(NMovingGround::GetPedAngVelocity(pPed)).Getf();

		TUNE_GROUP_FLOAT (PED_NAVIGATION, fSlideHeadingThreshold, 0.45f, 0.0f, PI, 0.001f);
		if (fAngVel < 0.01f &&	// Only do this when there is no anim rot vel left
			abs(fDesiredHeadingChange) < fSlideHeadingThreshold) // Only do this for the last bit
		{
			TUNE_GROUP_FLOAT (PED_NAVIGATION, fSlideHeadingChangeRate, 1.2f, 0.0f, 100.0f, 0.001f);
			float fRotateSpeed;

			// try not to overshoot
			if ((fRate*fTimeStep) > abs(fDesiredHeadingChange))
				fRotateSpeed = abs(fDesiredHeadingChange) / fTimeStep;
			else
				fRotateSpeed = fRate;

			if (fDesiredHeadingChange<0.0f)
				fRotateSpeed *= -1.0f;

			NMovingGround::SetPedDesiredAngularVelocity(pPed, Vec3V(0.0f, 0.0f, fRotateSpeed));
		}
		else if (fAngVel * fTimeStep > abs(fDesiredHeadingChange))	// Don't let anim overshoot last bit!
		{
			fAngVel = fDesiredHeadingChange / fTimeStep;
			NMovingGround::SetPedDesiredAngularVelocity(pPed, Vec3V(0.0f, 0.0f, fAngVel));
		}
	}
	else
	{
		NMovingGround::SetPedDesiredAngularVelocity(pPed, Vec3V(V_ZERO));	// Must clear or we will keep velocity from last frames
	}
}

bool CTaskMoveGoToPointAndStandStill::AreClipsStillMoving()
{
	const CPed * pPed = GetPed();
	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();

	if (pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
	{
		if (pTask->GetState() == CTaskHumanLocomotion::State_Start || 
			pTask->GetState() == CTaskHumanLocomotion::State_Moving)
		{
			return true;
		}
		else if (pTask->GetState() == CTaskHumanLocomotion::State_Stop)
		{
			if (m_bStopClipNoLongerMoving)
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	}
	else if (pTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
	{
		if (pTask->GetState() == CTaskMotionAiming::State_StartStrafing ||
			pTask->GetState() == CTaskMotionAiming::State_Strafing || 
			pTask->GetState() == CTaskMotionAiming::State_StrafingIntro)
		{
			return true;
		}
		else if (pTask->GetState() == CTaskMotionAiming::State_StopStrafing)
		{
			// Allow exact stops while aiming / strafing
			if (m_bStopClipNoLongerMoving)
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	}
	else if (pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_QUAD)
	{
		s32 iState = pTask->GetState();
		if (iState == CTaskQuadLocomotion::State_StartLocomotion ||
			iState == CTaskQuadLocomotion::State_Locomotion)
		{
			return true;
		}
		else if (iState == CTaskQuadLocomotion::State_StopLocomotion)
		{
			if (m_bStopClipNoLongerMoving)
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskMoveGoToPointAndStandStill::IsStoppedAtTargetPoint()
{
	static const float fMaxExactStopDist = 0.01f;
	static const float fMaxExactHeadingDif = 0.02f;

	if (!m_bStopExactly)
	{
		//we're not worried about hitting the point - return true
		return true;
	}

	const CPed * pPed = GetPed();

	if(m_bParentTaskOveridesDistanceRemaining)
	{

	}
	else
	{
		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTarget;
		m_fDistanceRemaining = vDiff.XYMag();
	}


	if (AreClipsStillMoving())
	{
		return false;
	}
	else if (m_fDistanceRemaining > fMaxExactStopDist || (m_bTargetHeadingSpecified && (abs(SubtractAngleShorter(m_fTargetHeading, pPed->GetCurrentHeading()))>fMaxExactHeadingDif)))
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CTaskMoveGoToPointAndStandStill::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	TUNE_GROUP_FLOAT (PED_NAVIGATION, fHeadingSlideSpeed, 2.0f, 0.0f, 100.0f, 0.001f);

	switch(m_iExtractedVelOperation)
	{
		case ExtractedVel_Zero:
		{
			NMovingGround::SetPedDesiredVelocity(pPed, Vec3V(V_ZERO));
			return true;
		}
		case ExtractedVel_ZeroXY:
		{
			Vec3V vDesiredVel = NMovingGround::GetPedDesiredVelocity(pPed);
			const ScalarV zeroV(V_ZERO);
			vDesiredVel.SetX(zeroV);
			vDesiredVel.SetY(zeroV);
			NMovingGround::SetPedDesiredVelocity(pPed, vDesiredVel);
			
			if (m_bTargetHeadingSpecified)
				UpdateAngularVelocity(pPed, fHeadingSlideSpeed, fTimeStep);

			float fDesiredHeadingChange =  SubtractAngleShorter(
				fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetDesiredHeading()), 
				fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading()));

			static const float fMinDist = 0.01f;
			static const float fMinAnimTranslation = 0.01f;

			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vToTarget = m_vTarget - vPedPos;

			bool bAcceptParentDist = true;
			if (vToTarget.z < 1.0f && vToTarget.z > -0.5f && vToTarget.XYMag2() < 0.5f && m_fDistanceRemaining < 1.0f)
				bAcceptParentDist = false;

			vToTarget.z = 0.0f;						// slide flat only

			// We might accept parent dist but we want the smallest of them two otherwise
			float fDist;
			if (m_bParentTaskOveridesDistanceRemaining)
				fDist = (bAcceptParentDist ? m_fDistanceRemaining : Min(vToTarget.Mag(), m_fDistanceRemaining));
			else
				fDist = vToTarget.Mag();

			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			float fStoppingDist = pTask->GetStoppingDistance(m_fTargetHeading);

			if ((fDist < fMinDist) && (fStoppingDist < fMinAnimTranslation) && (!m_bTargetHeadingSpecified || IsNearZero(fDesiredHeadingChange)))
			{
				m_bStopClipNoLongerMoving = true;
				NMovingGround::SetPedDesiredVelocity(pPed, Vec3V(V_ZERO));
				return false;
			}

			return true;
		}
		case ExtractedVel_SlideToTarget:
		{
			// Add a velocity to get the ped to the end point
			static const float fMinDist = 0.01f;
			static const float fMinVelocity2 = 0.02f*0.02f;
			TUNE_GROUP_FLOAT (PED_NAVIGATION, fDefaultSlideSpeed, 1.3f, 0.0f, 100.0f, 0.001f);

			bool bAcceptParentDist = true;
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vToTarget = m_vTarget - vPedPos;
			if (!pPed->GetIsSwimming())
			{
				if (vToTarget.z < 1.0f && vToTarget.z > -0.5f && vToTarget.XYMag2() < 0.5f && m_fDistanceRemaining < 1.0f)
					bAcceptParentDist = false;

				vToTarget.z = 0.0f;						// slide flat only
			}

			Vector3 vToTargetNorm;
			vToTargetNorm.Normalize(vToTarget);

			// We might accept parent dist but we want the smallest of them two otherwise
			float fDist;
			if (m_bParentTaskOveridesDistanceRemaining)
				fDist = (bAcceptParentDist ? m_fDistanceRemaining : Min(vToTarget.Mag(), m_fDistanceRemaining));
			else
				fDist = vToTarget.Mag();

			Vector3 vDesiredVelocity;
			float fSlideSpeed = (fDist < fTimeStep*fDefaultSlideSpeed) ? (fDist/fTimeStep) : fDefaultSlideSpeed;	// try not to overshoot
			vDesiredVelocity = vToTargetNorm * fSlideSpeed;

			UpdateAngularVelocity(pPed, fHeadingSlideSpeed, fTimeStep);

			// Check if we are also facing the proper direction
			float fDesiredHeadingChange =  SubtractAngleShorter(
				fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetDesiredHeading()), 
				fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading()));

			if((vDesiredVelocity.Mag2() < fMinVelocity2 || fDist < fMinDist) && IsNearZero(fDesiredHeadingChange))
			{
				NMovingGround::SetPedDesiredVelocity(pPed, Vec3V(V_ZERO));
				return false;
			}
			else
			{
				NMovingGround::SetPedDesiredVelocity(pPed, VECTOR3_TO_VEC3V(vDesiredVelocity));
				return true;
			}
		}
		case ExtractedVel_OrientateAndScale:
		{
			//static const float fMinMag2 = 0.005f*0.005f;
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vToTarget = m_vTarget - vPedPos;

			//*************************************************************************************************
			//	Scale the extracted velocity to make the stop clip take the ped exactly to their target

			bool bAcceptParentDist = true;
			if (vToTarget.z < 1.0f && vToTarget.z > -0.5f && vToTarget.XYMag2() < 0.5f && m_fDistanceRemaining < 1.0f)
				bAcceptParentDist = false;

			const float fParentDistRemaining = Max(0.0f, m_fDistanceRemaining - (m_fTargetRadius*ms_fStopInsideRadiusRatio));
			const float fDistRemaining = Max(0.0f, vToTarget.XYMag() - (m_fTargetRadius*ms_fStopInsideRadiusRatio));

			// We might accept parent dist but we want the smallest of them two otherwise
			float fDistToTargetXY;
			if (m_bParentTaskOveridesDistanceRemaining)
				fDistToTargetXY = (bAcceptParentDist ? fParentDistRemaining : Min(fDistRemaining, fParentDistRemaining));
			else
				fDistToTargetXY = fDistRemaining;

			float vInitialDesiredZ = 0.0f;

			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			taskAssert(pTask);

			bool bApplyScale = true;
			switch(pTask->GetTaskType())
			{
			case CTaskTypes::TASK_HUMAN_LOCOMOTION:
				bApplyScale = static_cast<CTaskHumanLocomotion*>(pTask)->IsFullyInStop() ||	pTask->GetState() == CTaskHumanLocomotion::State_Stop;
				break;
			case CTaskTypes::TASK_MOTION_AIMING:
				bApplyScale = static_cast<CTaskMotionAiming*>(pTask)->IsFullyInStop() || pTask->GetState() == CTaskMotionAiming::State_StopStrafing;
				break;
			case CTaskTypes::TASK_ON_FOOT_QUAD:
				bApplyScale = static_cast<CTaskQuadLocomotion*>(pTask)->GetState() == CTaskQuadLocomotion::State_StopLocomotion;
				break;
			default:
				break;
			}

			if (bApplyScale)
			{
				bool bIsClipActive = false;
				float fStopClipTranslationRemaining = pTask->GetStoppingDistance(m_fTargetHeading, &bIsClipActive);

				float fStopClipScale = (fStopClipTranslationRemaining == 0.0f) ? 0.0f : fDistToTargetXY / fStopClipTranslationRemaining;

				Vector3 vDesiredInitial = pPed->GetAnimatedVelocity();
				vDesiredInitial.z = 0.0f;
				if (bIsClipActive && IsNearZero(vDesiredInitial.XYMag2()))
				{
					// Our stop clip appears to have run out of translation
					// start sliding if necessary
					m_bStopClipNoLongerMoving = true;
				}

				if (bIsClipActive)
				{
					static dev_float fStopAnimationMinScale = 0.1f;
					static dev_float fStopAnimationMaxScale = 2.0f;
					fStopClipScale = Clamp(fStopClipScale, fStopAnimationMinScale, fStopAnimationMaxScale);

					MAT34V_TO_MATRIX34(pPed->GetTransform().GetMatrix()).Transform3x3(vDesiredInitial);

					//Keep track of initial desired velocity, as we are clearing the z part of velocity.
					vInitialDesiredZ = NMovingGround::GetPedDesiredVelocity(pPed).GetZf();
					ASSERT_ONLY(Vector3 vDesired = vDesiredInitial * fStopClipScale;)
					Assertf(vDesired.Mag2() < DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT, "Extracted vel (ExtractedVel_OrientateAndScale) was invalid(%f).", vDesired.Mag2());

					Vector3 vNewVel = vDesiredInitial * fStopClipScale;

					vNewVel.z = NMovingGround::GetPedDesiredVelocity(pPed).GetZf();
					NMovingGround::SetPedDesiredVelocity(pPed, VECTOR3_TO_VEC3V(vNewVel));
				}
			}

			//*************************************************************************************
			//	Orientate the extracted velocity so that the ped hits the center of their target

			const Vector3& vDesired = VEC3V_TO_VECTOR3(NMovingGround::GetPedDesiredVelocity(pPed));
			const float fAngleToTarget = fwAngle::LimitRadianAngle(fwAngle::GetRadianAngleBetweenPoints(m_vTarget.x, m_vTarget.y, vPedPos.x, vPedPos.y));
			const float fAngleExtractedVel = fwAngle::LimitRadianAngle(fwAngle::GetRadianAngleBetweenPoints(vDesired.x, vDesired.y, 0.0f, 0.0f));
			const float fDelta = SubtractAngleShorter(fAngleToTarget, fAngleExtractedVel);

			Matrix33 rotMat;
			Vector3 vDesiredFinal;
			rotMat.MakeRotateZ(fDelta);
			rotMat.Transform(vDesired, vDesiredFinal);
			NMovingGround::SetPedDesiredVelocity(pPed, VECTOR3_TO_VEC3V(vDesiredFinal));

			// Has overshoot?
			Vector3 vToTargetNew = vToTarget + vDesiredFinal;
			vToTargetNew.z = 0.0f;
			if (DotProduct(m_vTargetPlaneNormal, vToTargetNew) > 0.0f || fDistToTargetXY < 0.01f) 
			{
				vToTarget.z = 0.f;
				vToTarget = vToTarget / fTimeStep * 0.5f;
				vToTarget.ClampMag(0.f, 4.f);	// Make sure we don't get crazy high values here

				vToTarget.z = vDesired.z;
				NMovingGround::SetPedDesiredVelocity(pPed, VECTOR3_TO_VEC3V(vToTarget)); // Hit that target ye
			}

			const float START_SLIDE_DIST = 0.01f;	// We don't have much left to blend with so just slide last part for accuracy
			if (m_fDistanceRemaining < START_SLIDE_DIST)
			{
				m_iExtractedVelOperation = ExtractedVel_SlideToTarget;	// Just slide the last bit while we play the anim looks better than doing it after
				m_bJustSlideLastBit = true;
			}

			return true;
		}
		default:
		{
			return false;
		}
	}
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	m_bIsSlowingDown = false;
	m_fSlowingDownMoveBlendRatio = m_fMoveBlendRatio;
	m_iExtractedVelOperation = ExtractedVel_DoNothing;
	//pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true); // This will cause a wiggling / snaking effect :/

	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopping, false);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling, false);

FSM_Begin

	FSM_State(EGoingToPoint)
		FSM_OnEnter
			return GoingToPoint_OnEnter(pPed);
		FSM_OnUpdate
			return GoingToPoint_OnUpdate(pPed);

	FSM_State(EStopping)
		FSM_OnEnter
			return Stopping_OnEnter(pPed);
		FSM_OnUpdate
			return Stopping_OnUpdate(pPed);
		FSM_OnExit
			return Stopping_OnExit(pPed);

	FSM_State(EStandingStill)
		FSM_OnEnter
			return StandingStill_OnEnter(pPed);
		FSM_OnUpdate
			return StandingStill_OnUpdate(pPed);
FSM_End
}

CTask::FSM_Return CTaskMoveGoToPointAndStandStill::ProcessPostFSM()
{
	m_bDontCompleteThisFrame = false;
	return FSM_Continue;
}

void CTaskMoveGoToPointAndStandStill::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopping, false);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling, false);
	}

	CTaskMove::CleanUp();
}


CTaskMoveGoToPointRelativeToEntityAndStandStill::CTaskMoveGoToPointRelativeToEntityAndStandStill(const CEntity * pEntity, const float fMoveBlendRatio, const Vector3 & vLocalSpaceTarget, const float fTargetRadius, const s32 iMaxTime) :
CTaskMove(fMoveBlendRatio),
m_pEntity(pEntity),
m_iMaxTime(iMaxTime),
m_vLocalSpaceTarget(vLocalSpaceTarget),
m_fTargetRadius(fTargetRadius)
{
	Assert(m_pEntity);
	UpdateTarget();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GOTO_POINT_RELATIVE_TO_ENTITY_AND_STAND_STILL);
}

CTaskMoveGoToPointRelativeToEntityAndStandStill::~CTaskMoveGoToPointRelativeToEntityAndStandStill()
{
}

#if !__FINAL
void
CTaskMoveGoToPointRelativeToEntityAndStandStill::Debug() const
{
	if(GetSubTask()) GetSubTask()->Debug();
}
#endif

CTask::FSM_Return CTaskMoveGoToPointRelativeToEntityAndStandStill::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
		FSM_OnUpdate
		return Initial_OnUpdate();
	FSM_State(State_MovingToEntity)
		FSM_OnEnter
		return MovingToEntity_OnEnter(pPed);
	FSM_OnUpdate
		return MovingToEntity_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return
CTaskMoveGoToPointRelativeToEntityAndStandStill::Initial_OnUpdate()
{
	if(!m_pEntity)
		return FSM_Quit;

	if(m_iMaxTime > 0)
		m_GiveUpTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iMaxTime);

	UpdateTarget();

	SetState(State_MovingToEntity);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToPointRelativeToEntityAndStandStill::MovingToEntity_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_vWorldSpaceTarget, m_fTargetRadius));
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToPointRelativeToEntityAndStandStill::MovingToEntity_OnUpdate(CPed * pPed)
{
	if(!m_pEntity || !GetSubTask())
	{
		if(GetSubTask())
			GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL);
		return FSM_Quit;
	}

	UpdateTarget();

	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
	{
		((CTaskMoveGoToPointAndStandStill*)GetSubTask())->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vWorldSpaceTarget));
	}

	if(GetSubTask() && GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	// Warp to destination if time is up
	if(m_GiveUpTimer.IsSet() && m_GiveUpTimer.IsOutOfTime() && MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
	{
		float fHeading = pPed->GetTransform().GetHeading();
		pPed->Teleport(m_vWorldSpaceTarget, fHeading, true);
		return FSM_Quit;
	}

	return FSM_Continue;
}










//***********************************************
// CTaskMoveGoToPointStandStillAchieveHeading
//

const float CTaskMoveGoToPointStandStillAchieveHeading::ms_fSlowDownDistance = 4.0f;

CTaskMoveGoToPointStandStillAchieveHeading::CTaskMoveGoToPointStandStillAchieveHeading(
	const float fMoveBlendRatio,
	const Vector3 & vTarget,
	const Vector3 & vPositionToFace,
	const float fTargetRadius,
	const float fSlowDownDistance,
	const float fHeadingChangeRate,
	const float fHeadingTolerance,
	const float fAchieveHeadingTime)
	: CTaskMove(fMoveBlendRatio),
	m_vTarget(vTarget),
	m_vPositionToFace(vPositionToFace),
	m_fTargetRadius(fTargetRadius),
	m_fSlowDownDistance(fSlowDownDistance),
	m_bUsingHeading(false),
	m_fHeadingChangeRate(fHeadingChangeRate),
	m_fHeadingTolerance(fHeadingTolerance),
	m_bNewTarget(false),
	m_fAchieveHeadingTime(fAchieveHeadingTime)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GOTO_POINT_STAND_STILL_ACHIEVE_HEADING);
}

CTaskMoveGoToPointStandStillAchieveHeading::CTaskMoveGoToPointStandStillAchieveHeading(
	const float fMoveBlendRatio,
	const Vector3 & vTarget,
	const float fHeading,
	const float fTargetRadius,
	const float fSlowDownDistance,
	const float fHeadingChangeRate,
	const float fHeadingTolerance,
	const float fAchieveHeadingTime)
	: CTaskMove(fMoveBlendRatio),
	m_vTarget(vTarget),
	m_fTargetRadius(fTargetRadius),
	m_fHeading(fHeading),
	m_fSlowDownDistance(fSlowDownDistance),
	m_bUsingHeading(true),
	m_fHeadingChangeRate(fHeadingChangeRate),
	m_fHeadingTolerance(fHeadingTolerance),
	m_bNewTarget(false),
	m_fAchieveHeadingTime(fAchieveHeadingTime)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GOTO_POINT_STAND_STILL_ACHIEVE_HEADING);
}

CTaskMoveGoToPointStandStillAchieveHeading::~CTaskMoveGoToPointStandStillAchieveHeading()
{

}

#if !__FINAL
void
CTaskMoveGoToPointStandStillAchieveHeading::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();

	DEBUG_DRAW_ONLY(grcDebugDraw::Cross(RCC_VEC3V(m_vPositionToFace), 0.25f, Color_magenta2));
}
#endif

CTask::FSM_Return
CTaskMoveGoToPointStandStillAchieveHeading::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Initial)
		FSM_OnUpdate
			return Initial_OnUpdate(pPed);
	FSM_State(State_MovingToPoint)
		FSM_OnUpdate
			return MovingToPoint_OnUpdate(pPed);
		FSM_OnEnter
			return MovingToPoint_OnEnter(pPed);
	FSM_State(State_AchievingHeading)
		FSM_OnUpdate
			return AchieveHeading_OnUpdate(pPed);
		FSM_OnEnter
			return AchieveHeading_OnEnter(pPed);
FSM_End
}

CTask::FSM_Return
CTaskMoveGoToPointStandStillAchieveHeading::Initial_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_MovingToPoint);

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToPointStandStillAchieveHeading::MovingToPoint_OnEnter(CPed* pPed)
{
	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseGoToPointForScenarioNavigation ))
	{
		SetNewTask(CreateSubTask(pPed,CTaskTypes::TASK_MOVE_GO_TO_POINT));
	}
	else
	{
		SetNewTask(CreateSubTask(pPed, CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	}
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToPointStandStillAchieveHeading::MovingToPoint_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(m_bNewTarget)
	{
		m_bNewTarget = false;
		SetState(State_Initial);
		return FSM_Continue;
	}

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_AchievingHeading);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToPointStandStillAchieveHeading::AchieveHeading_OnEnter(CPed * pPed)
{
	SetNewTask(CreateSubTask(pPed, CTaskTypes::TASK_MOVE_ACHIEVE_HEADING));
	return FSM_Continue;
}

CTask::FSM_Return
CTaskMoveGoToPointStandStillAchieveHeading::AchieveHeading_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_ACHIEVE_HEADING);

	if(m_bNewTarget)
	{
		m_bNewTarget = false;
		SetState(State_Initial);
		return FSM_Continue;
	}
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

bool CTaskMoveGoToPointStandStillAchieveHeading::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_CLIMB_LADDER_ON_ROUTE)
	{
		return false;
	}

	// Base class
	return CTaskMove::ShouldAbort(iPriority, pEvent);
}

aiTask *
CTaskMoveGoToPointStandStillAchieveHeading::CreateSubTask(const CPed * pPed, const int iSubTaskType)
{
	switch(iSubTaskType)
	{
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{	
			CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_vTarget, m_fTargetRadius, m_fSlowDownDistance);
			pMoveTask->SetUseLargerSearchExtents(true);
			return pMoveTask;			
		}
		case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
		{
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const float fHeading = m_bUsingHeading ?
				m_fHeading : fwAngle::GetRadianAngleBetweenPoints(m_vPositionToFace.x, m_vPositionToFace.y, vPedPos.x, vPedPos.y);

			return rage_new CTaskMoveAchieveHeading(fHeading, m_fHeadingChangeRate, m_fHeadingTolerance, m_fAchieveHeadingTime);
		}
		case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			return rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio,m_vTarget, m_fTargetRadius);
		}
		case CTaskTypes::TASK_FINISHED:
			return NULL;

		default:
			Assert(0);
			return NULL;
	}
}


void CTaskMoveGoToPointStandStillAchieveHeading::SetTargetAndHeading(
	const Vector3& vTarget,
	const float fTargetRadius, 
	const float fHeading,
	const float fHeadingChangeRateFrac,
	const float fHeadingTolerance, 
	const bool bForce)
{
    if(bForce || m_vTarget!=vTarget || m_fTargetRadius!=fTargetRadius || m_fHeading!=fHeading ||
       m_fHeadingChangeRate!=fHeadingChangeRateFrac || m_fHeadingTolerance!=fHeadingTolerance)
    {
        m_vTarget = vTarget;
        m_fTargetRadius = fTargetRadius;
        m_fHeading = fHeading;
        m_fHeadingChangeRate = fHeadingChangeRateFrac;
        m_fHeadingTolerance = fHeadingTolerance;
        m_bNewTarget = true;
		m_bUsingHeading = true;
    }
}

CClonedGoToPointAndStandStillTimedInfo::CClonedGoToPointAndStandStillTimedInfo(const float    moveBlendRatio,
                                                                               const Vector3 &targetPos,
                                                                               const int      time,
                                                                               const float    targetHeading) :
m_MoveBlendRatio(moveBlendRatio)
, m_TargetPos(targetPos)
, m_Time(time)
, m_TargetHeading(targetHeading)
{
    // adjust target heading to be in range -PI to PI
	if (m_TargetHeading < 40000.f)	// Scripted no heading provided
		m_TargetHeading = fwAngle::LimitRadianAngle(m_TargetHeading);
}

CClonedGoToPointAndStandStillTimedInfo::CClonedGoToPointAndStandStillTimedInfo() :
m_MoveBlendRatio(0.0f)
, m_TargetPos(VEC3_ZERO)
, m_Time(0)
, m_TargetHeading(0.0f)
{
}

CTask *CClonedGoToPointAndStandStillTimedInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    CTaskMoveGoToPointAndStandStill *pGotoTask = 0;

    float fMainTaskTimer = 0.0f;

    if(m_Time == -1)
    {
        pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(m_MoveBlendRatio,
                                                             m_TargetPos,
                                                             CTaskMoveGoToPointAndStandStill::ms_fTargetRadius,
                                                             CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance,
                                                             false,
                                                             true,
                                                             m_TargetHeading);
    }
    else
    {
        fMainTaskTimer = static_cast<float>(m_Time) / 1000.0f;

        if( fMainTaskTimer > 0.0f )
        {
            fMainTaskTimer += 2.0f;
        }

        pGotoTask = rage_new CTaskComplexGoToPointAndStandStillTimed(m_MoveBlendRatio,
                                                                     m_TargetPos,
                                                                     CTaskMoveGoToPointAndStandStill::ms_fTargetRadius,
                                                                     CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance);
    }

    CTask *pTask = rage_new CTaskComplexControlMovement(pGotoTask, NULL, CTaskComplexControlMovement::TerminateOnMovementTask, fMainTaskTimer );
    return pTask;
}

const int CTaskComplexGoToPointAndStandStillTimed::ms_iTime=20000;

CTaskComplexGoToPointAndStandStillTimed::CTaskComplexGoToPointAndStandStillTimed(const float fMoveBlendRatio, const Vector3& vTarget, const float fTargetRadius, const float fSlowDownDistance, const int iTime, bool stopExactly, float targetHeading)
: CTaskMoveGoToPointAndStandStill(fMoveBlendRatio,vTarget,fTargetRadius,fSlowDownDistance,false, stopExactly, targetHeading),
m_iTime(iTime)
{
}

CTaskComplexGoToPointAndStandStillTimed::~CTaskComplexGoToPointAndStandStillTimed()
{
}

CTask::FSM_Return CTaskComplexGoToPointAndStandStillTimed::GoingToPoint_OnEnter( CPed* pPed)
{	
	CTaskMoveGoToPointAndStandStill::GoingToPoint_OnEnter(pPed);

	if(m_iTime>=0)
	{
		m_timer.Set(fwTimer::GetTimeInMilliseconds(),m_iTime);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskComplexGoToPointAndStandStillTimed::GoingToPoint_OnUpdate( CPed* pPed)
{	
	CTaskMoveGoToPointAndStandStill::GoingToPoint_OnUpdate(pPed);

	if(m_timer.IsOutOfTime()&& (GetSubTask()->GetTaskType()!=CTaskTypes::TASK_MOVE_STAND_STILL))
	{	
		const float fSecondSurfaceInterp=0.0f;

		if(CPedPlacement::FindZCoorForPed(fSecondSurfaceInterp,&m_vTarget,0))
		{
			Vector3 vecDelta = m_vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			pPed->SetPosition(m_vTarget, true, true, vecDelta.Mag2() > 9.0f);
			if (m_fTargetHeading<DEFAULT_NAVMESH_FINAL_HEADING)
			{
				pPed->SetHeading(m_fTargetHeading);
			}
		}
	}

	return FSM_Continue;
}

void CTaskComplexGoToPointAndStandStillTimed::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Don't stop the timer for temporary events.
	if(!pEvent || !((CEvent*)pEvent)->IsTemporaryEvent())
	{
		m_timer.Stop();
	}

	// Base class
	CTaskMoveGoToPointAndStandStill::DoAbort(iPriority, pEvent);
}

CTaskInfo *CTaskComplexGoToPointAndStandStillTimed::CreateQueriableState() const
{
    return rage_new CClonedGoToPointAndStandStillTimedInfo(m_fMoveBlendRatio, m_vTarget, m_iTime, m_fTargetHeading);
}
