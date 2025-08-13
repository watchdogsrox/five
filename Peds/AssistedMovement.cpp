// Title	:	AssistedMovement.cpp
// Author	:	James Broad
// Started	:	18/02/09
// System for helping the player movement by guiding him gently
// along 'rails' which have been placed in the map.

#include "AssistedMovement.h"

// Rage includes
#include "Math/angmath.h"

// Framework includes
#include "fwmaths/vectorutil.h"

// Game includes
#include "Camera/CamInterface.h"
#include "Camera/Debug/DebugDirector.h"
#include "Camera/Helpers/Frame.h"
#include "pathserver/ExportCollision.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnSkis.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGotoPoint.h"

AI_OPTIMISATIONS()

#if 1 //__PLAYER_ASSISTED_MOVEMENT

bank_bool CPlayerAssistedMovementControl::ms_bAssistedMovementEnabled			= true;
bank_bool CPlayerAssistedMovementControl::ms_bRouteScanningEnabled				= true;
bank_bool CPlayerAssistedMovementControl::ms_bLoadAllRoutes						= false;
bank_bool CPlayerAssistedMovementControl::ms_bDrawRouteStore					= true;
bank_float CPlayerAssistedMovementControl::ms_fDefaultTeleportedRescanTime		= 4.0f;

#if __DEV
bool CPlayerAssistedMovementControl::ms_bDebugOutCapsuleHits					= false;
#endif

CPlayerAssistedMovementControl::CPlayerAssistedMovementControl()
{
	m_bIsUsingRoute = false;
	m_bOnStickyRoute = false;
	m_bWasOnStickyRoute = false;
	m_bUseWideLanes = false;
	m_fScanForRoutesFreq = 0.4f;
	m_fTimeUntilNextScanForRoutes = m_fScanForRoutesFreq;
	m_fTeleportedRescanTimer = ms_fDefaultTeleportedRescanTime;
	m_fLocalRouteCurvature = 0.0f;
	m_vLookAheadRouteDirection.Zero();
}
CPlayerAssistedMovementControl::~CPlayerAssistedMovementControl()
{

}

struct SteeringPathConfig
{
	enum
	{
		CONFIG_NORMAL, CONFIG_WEAK, CONFIG_STRONG, CONFIG_WIDE, CONFIG_NUM
	};

	SteeringPathConfig() { }

	SteeringPathConfig(
		float fWalkMaxInfluenceDist,
		float fWalkMaxDistanceFromSpline,
		float fWalkDeltaHeadingTolerance,
		float fWalkLookAheadDist,
		float fRunMaxInfluenceDist,
		float fRunMaxDistanceFromSpline,
		float fRunDeltaHeadingTolerance,
		float fRunLookAheadDist)
	{
		WALK_MAX_INFLUENCE_DISTANCE = fWalkMaxInfluenceDist;
		WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED = fWalkMaxDistanceFromSpline;
		WALK_DELTA_HEADING_TOLERANCE = fWalkDeltaHeadingTolerance;
		WALK_LOOK_AHEAD_DIST = fWalkLookAheadDist;

		RUN_MAX_INFLUENCE_DISTANCE = fRunMaxInfluenceDist;
		RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED = fRunMaxDistanceFromSpline;
		RUN_DELTA_HEADING_TOLERANCE = fRunDeltaHeadingTolerance;
		RUN_LOOK_AHEAD_DIST = fRunLookAheadDist;
	}

	float WALK_MAX_INFLUENCE_DISTANCE;
	float WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED;
	float WALK_DELTA_HEADING_TOLERANCE;
	float WALK_LOOK_AHEAD_DIST;
	float RUN_MAX_INFLUENCE_DISTANCE;
	float RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED;
	float RUN_DELTA_HEADING_TOLERANCE;
	float RUN_LOOK_AHEAD_DIST;

	void GetConfig(float fTension, bool bWideLanes = false);

	static float GetMaxLookAheadDist();
	static float GetMaxInfluenceDist();
	static float GetMaxDistFromSplineAllowed();
};

static SteeringPathConfig steeringPathConfigs[SteeringPathConfig::CONFIG_NUM] =
{
	SteeringPathConfig( 0.5f, 0.5f, 70.0f, 0.5f, 1.0f, 1.0f, 90.0f, 0.65f ),	// CONFIG_NORMAL
	SteeringPathConfig( 0.5f, 0.5f, 50.0f, 0.5f, 1.0f, 1.0f, 70.0f, 0.65f ),	// CONFIG_WEAK
	SteeringPathConfig( 0.5f, 0.5f, 90.0f, 0.5f, 1.0f, 1.0f, 90.0f, 0.65f ),		// CONFIG_STRONG
	SteeringPathConfig( 5.0f, 5.0f, 90.0f, 5.0f, 5.0f, 5.0f, 90.0f, 5.65f )		// CONFIG_WIDE
};

float SteeringPathConfig::GetMaxLookAheadDist()
{
	float fMax = 0.0f;
	for(int i=0; i<SteeringPathConfig::CONFIG_WIDE; i++)
	{
		fMax = Max(fMax, steeringPathConfigs[i].WALK_LOOK_AHEAD_DIST);
		fMax = Max(fMax, steeringPathConfigs[i].RUN_LOOK_AHEAD_DIST);
	}
	return fMax;
}
float SteeringPathConfig::GetMaxInfluenceDist()
{
	float fMax = 0.0f;
	for(int i=0; i<SteeringPathConfig::CONFIG_WIDE; i++)
	{
		fMax = Max(fMax, steeringPathConfigs[i].WALK_MAX_INFLUENCE_DISTANCE);
		fMax = Max(fMax, steeringPathConfigs[i].RUN_MAX_INFLUENCE_DISTANCE);
	}
	return fMax;
}
float SteeringPathConfig::GetMaxDistFromSplineAllowed()
{
	float fMax = 0.0f;
	for(int i=0; i<SteeringPathConfig::CONFIG_WIDE; i++)
	{
		fMax = Max(fMax, steeringPathConfigs[i].WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED);
		fMax = Max(fMax, steeringPathConfigs[i].RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED);
	}
	return fMax;
}

void SteeringPathConfig::GetConfig(float fTension, bool bWideLanes)
{
	if (bWideLanes)
	{
		*this = steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE];
		return;
	}

	static const float fAverageTension = 0.7f;
	static const float fMinTension = 0.0f;
	static const float fFullTension = 1.0f;

	fTension = Clamp(fTension, 0.0f, 1.0f);

	if(fTension <= fMinTension)
	{
		*this = steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK];
		return;
	}
	if(fTension >= fFullTension)
	{
		*this = steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG];
		return;
	}

	if(fTension < fAverageTension)
	{
		const float T = (fTension - fMinTension) / (fAverageTension - fMinTension);

		WALK_MAX_INFLUENCE_DISTANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].WALK_MAX_INFLUENCE_DISTANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_MAX_INFLUENCE_DISTANCE);
		WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED);
		WALK_DELTA_HEADING_TOLERANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].WALK_DELTA_HEADING_TOLERANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_DELTA_HEADING_TOLERANCE);
		WALK_LOOK_AHEAD_DIST = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].WALK_LOOK_AHEAD_DIST, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_LOOK_AHEAD_DIST);
		RUN_MAX_INFLUENCE_DISTANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].RUN_MAX_INFLUENCE_DISTANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_MAX_INFLUENCE_DISTANCE);
		RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED);
		RUN_DELTA_HEADING_TOLERANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].RUN_DELTA_HEADING_TOLERANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_DELTA_HEADING_TOLERANCE);
		RUN_LOOK_AHEAD_DIST = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_WEAK].RUN_LOOK_AHEAD_DIST, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_LOOK_AHEAD_DIST);
	}
	else
	{
		const float T = (fTension - fAverageTension) / (fFullTension - fAverageTension);

		WALK_MAX_INFLUENCE_DISTANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_MAX_INFLUENCE_DISTANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].WALK_MAX_INFLUENCE_DISTANCE);
		WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED);
		WALK_DELTA_HEADING_TOLERANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_DELTA_HEADING_TOLERANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].WALK_DELTA_HEADING_TOLERANCE);
		WALK_LOOK_AHEAD_DIST = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_LOOK_AHEAD_DIST, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].WALK_LOOK_AHEAD_DIST);
		RUN_MAX_INFLUENCE_DISTANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_MAX_INFLUENCE_DISTANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].RUN_MAX_INFLUENCE_DISTANCE);
		RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED);
		RUN_DELTA_HEADING_TOLERANCE = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_DELTA_HEADING_TOLERANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].RUN_DELTA_HEADING_TOLERANCE);
		RUN_LOOK_AHEAD_DIST = Lerp(T, steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_LOOK_AHEAD_DIST, steeringPathConfigs[SteeringPathConfig::CONFIG_STRONG].RUN_LOOK_AHEAD_DIST);
	}
}

bool CPlayerAssistedMovementControl::GetDesiredHeading(CPed * pPlayer, float & fOutDesiredHeading, float & fOutDesiredPitch, float & fOutDesiredMBR)
{
	const bool bProcess3d = pPlayer->GetCurrentMotionTask()->IsUnderWater() && !pPlayer->IsStrafing();
	const bool bRunning = pPlayer->GetMotionData()->GetCurrentMoveBlendRatio().Mag() >= MOVEBLENDRATIO_RUN;
	const float distAhead = bRunning ? steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].RUN_LOOK_AHEAD_DIST : steeringPathConfigs[SteeringPathConfig::CONFIG_NORMAL].WALK_LOOK_AHEAD_DIST;

	Vec3V vMoveDir;
	pPlayer->GetCurrentMotionTask()->GetMoveVector(RC_VECTOR3(vMoveDir));

	const Vec3V vPlayerPosition = pPlayer->GetTransform().GetPosition();
	Vec3V vFocusPos = vPlayerPosition + (vMoveDir * ScalarVFromF32(distAhead));

	if(pPlayer->IsStrafing())
		vFocusPos = vPlayerPosition;

	Vec3V closestPos, routeNormal, tangentForceDir;
	float fTension;
	u32 iPointFlags = 0;

	const float closestDistAlong = m_Route.GetClosestPos( RCC_VECTOR3(vFocusPos), &RC_VECTOR3(closestPos), &RC_VECTOR3(routeNormal), &RC_VECTOR3(tangentForceDir), &fTension, &iPointFlags );

	if(closestDistAlong <= 0.0f || closestDistAlong >= m_Route.GetLength())
	{
		return false;
	}

	SteeringPathConfig config;
	config.GetConfig(fTension, m_bUseWideLanes);

	float maxInfluenceDistance, maxDistanceFromSplineAllowed, deltaHeadingTolerance;

	if ( bRunning )
	{
		maxInfluenceDistance = config.RUN_MAX_INFLUENCE_DISTANCE + distAhead;
		maxDistanceFromSplineAllowed = config.RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED + distAhead;
		deltaHeadingTolerance = config.RUN_DELTA_HEADING_TOLERANCE;
	}
	else
	{
		maxInfluenceDistance = config.WALK_MAX_INFLUENCE_DISTANCE + distAhead;
		maxDistanceFromSplineAllowed = config.WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED + distAhead;
		deltaHeadingTolerance = config.WALK_DELTA_HEADING_TOLERANCE;
	}

	UpdateLocalCurvatureAndLookAheadDirection(vPlayerPosition, tangentForceDir, closestDistAlong, bProcess3d);

	//-----------------------------------------------------------------------------

	fOutDesiredMBR = pPlayer->GetMotionData()->GetDesiredMoveBlendRatio().y;

	if( !pPlayer->IsStrafing() )
	{
		if( (iPointFlags & CAssistedMovementRoute::RPF_ReduceSpeedForCorners)!=0 )
		{
			CTaskMotionBase * pMotionTask = pPlayer->GetCurrentMotionTask();
			if(aiVerify(pMotionTask))
			{
				const float fCurrentMBR = pPlayer->GetMotionData()->GetCurrentMbrY();
				const float fDesiredMBR = pPlayer->GetMotionData()->GetDesiredMbrY();

				// TODO: We should probably work this out accurately based upon distAhead & the movement speed
				// (however when I tried this initially, the results were worse & that's why I hardcoded a value which works well)
				static dev_float fLookAheadTime = 0.5f;	
				
				CMoveBlendRatioSpeeds moveSpeeds;
				CMoveBlendRatioTurnRates turnRates;

				CTaskMotionBase* pMotionBase = pPlayer->GetCurrentMotionTask();
				
				pMotionBase->GetMoveSpeeds(moveSpeeds);
				pMotionBase->GetTurnRates(turnRates);

				const float fNewBMR = CTaskMoveGoToPoint::CalculateMoveBlendRatioForCorner(pPlayer, closestPos, fLookAheadTime, fCurrentMBR, fDesiredMBR, CTaskMoveGoToPoint::ms_fAccelMBR, fwTimer::GetTimeStep(), moveSpeeds, turnRates);
				fOutDesiredMBR = Min(fNewBMR, fOutDesiredMBR);
			}
		}
	}

	if( Abs(m_fLocalRouteCurvature) > PI*0.25 ) // 45 degs
	{
		pPlayer->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );
	}

	//-----------------------------------------------------------------------------

#if DEBUG_DRAW
	if( CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAssistedMovement )
	{
		grcDebugDraw::Cross(closestPos, 0.1f, Color_azure);
		grcDebugDraw::Cross(vFocusPos, 0.1f, Color_SpringGreen1);
	}
#endif

	Vec3V pullForceDir = closestPos - vFocusPos;

	if(!bProcess3d)
	{
		pullForceDir.SetZ(ScalarV(V_ZERO));
		tangentForceDir.SetZ(ScalarV(V_ZERO));
	}

	const float distanceFromSpline = Mag(pullForceDir).Getf();
	// If we're too far away from the spline, we're not influence by it
	if ( distanceFromSpline >= maxInfluenceDistance )
	{
		return false;
	}

	const float distanceRatio = Clamp( distanceFromSpline / maxDistanceFromSplineAllowed, 0.0f, 1.0f );

	Vec3V vPlayerDesiredHeading, vPlayerCurrentHeading;

	if(pPlayer->IsStrafing())
	{
		// Only allow strafing on route which have been flagged as appropriate for this
		if((m_Route.GetFlags() & CAssistedMovementRoute::RF_IsActiveWhenStrafing)==0)
			return false;

		Vector2 vCurrentMBR = pPlayer->GetMotionData()->GetCurrentMoveBlendRatio();
		Vector2 vDesiredMBR = pPlayer->GetMotionData()->GetDesiredMoveBlendRatio();

		vCurrentMBR.Rotate(pPlayer->GetCurrentHeading());
		vDesiredMBR.Rotate(pPlayer->GetCurrentHeading());

		vPlayerDesiredHeading = Vec3V( vDesiredMBR.x, vDesiredMBR.y, 0.0f );
		vPlayerCurrentHeading = Vec3V( vCurrentMBR.x, vCurrentMBR.y, 0.0f );

		fOutDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vPlayerDesiredHeading.GetXf(), vPlayerDesiredHeading.GetYf(), 0.0f, 0.0f);

#if __BANK && DEBUG_DRAW
		if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAssistedMovement)
		{
			const Vec3V vUp = VECTOR3_TO_VEC3V(ZAXIS * 1.2f);
			grcDebugDraw::Line(vPlayerPosition + vUp, vPlayerPosition + vUp + vPlayerCurrentHeading, Color_green, Color_green);
			grcDebugDraw::Line(vPlayerPosition + vUp, vPlayerPosition + vUp + vPlayerDesiredHeading, Color_red, Color_red);
		}
#endif
	}
	else
	{
		vPlayerDesiredHeading = Vec3V(-rage::Sinf( pPlayer->GetDesiredHeading() ), rage::Cosf( pPlayer->GetDesiredHeading() ), 0.0f );
		vPlayerCurrentHeading = Vec3V(-rage::Sinf( pPlayer->GetCurrentHeading() ), rage::Cosf( pPlayer->GetCurrentHeading() ), 0.0f );
	}

	tangentForceDir = NormalizeSafe( tangentForceDir, vPlayerDesiredHeading );
	pullForceDir = NormalizeSafe( pullForceDir, tangentForceDir );

	// If processing in 3d, then rotate headings by pitch
	if(bProcess3d)
	{
		Mat34V mat(V_IDENTITY);

		Vec3V vPitchAxis = CrossZAxis(vPlayerDesiredHeading);
		Mat34VFromAxisAngle(mat, vPitchAxis, ScalarVFromF32(pPlayer->GetDesiredPitch()));
		vPlayerDesiredHeading = Transform(mat, vPlayerDesiredHeading);
		vPlayerDesiredHeading = Normalize(vPlayerDesiredHeading);

		vPitchAxis = CrossZAxis(vPlayerCurrentHeading);
		Mat34VFromAxisAngle(mat, vPitchAxis, ScalarVFromF32(pPlayer->GetCurrentPitch()));
		vPlayerCurrentHeading = Transform(mat, vPlayerCurrentHeading);
		vPlayerCurrentHeading = Normalize(vPlayerCurrentHeading);
	}

	// If we're pushing in the opposite direction to the tangent, reverse the direction the spline wants to take us
	float angleBetween = Angle(vPlayerDesiredHeading, tangentForceDir).Getf();
	angleBetween = fabs( RtoD * angleBetween );
	if ( angleBetween > 90.f )
	{
		tangentForceDir *= ScalarV(V_NEGONE);
	}

	// If the route is flagged to only be active in the forwards/reverse directions, then quit if we're pushing in these directions
	if(angleBetween <= 90.0f && (m_Route.GetFlags() & CAssistedMovementRoute::RF_DisableInForwardsDirection)!=0)
	{
		return false;
	}
	if(angleBetween > 90.0f && (m_Route.GetFlags() & CAssistedMovementRoute::RF_DisableInReverseDirection)!=0)
	{
		return false;
	}

	//----------------------------------------------------------------------------------------------

    // There are two components to the steering path here. One acts tangential to the
    // spline, the other pulls the player back toward the spline. The magnitude of the
    // force pulling the player back to the spline is determined by the distance from
	// the spline. The tangential force magnitude is given by sqrt( 1 - pullforce ^2 )
	const float pullForceMagnitude = distanceRatio;
	const ScalarV pullForceMagnitudeV = ScalarVFromF32(distanceRatio);
    const ScalarV tangentialForceMagnitude = ScalarVFromF32(Sqrtf( 1.0f - ( pullForceMagnitude*pullForceMagnitude ) ));

    // Calculate the new desired heading
    Vec3V desiredHeading = pullForceMagnitudeV * pullForceDir + tangentialForceMagnitude * tangentForceDir;
	if(!bProcess3d)
		desiredHeading.SetZ(ScalarV(V_ZERO));
    desiredHeading = NormalizeSafe( desiredHeading, vPlayerDesiredHeading );


	// If the tangent heading is too different to the player's desired heading, don't
	// use the steering path's heading
	float finalAngleBetweenSplineAndDesired = Angle(tangentForceDir, vPlayerDesiredHeading).Getf();
	finalAngleBetweenSplineAndDesired = fabs( RtoD * finalAngleBetweenSplineAndDesired );
	if ( finalAngleBetweenSplineAndDesired > deltaHeadingTolerance )
	{
		return false;
	}

	// If the new tangent heading is too different to the player's current heading, don't
	// use the steering path's heading.
	float finalAngleBetweenSplineAndCurrent = Angle(tangentForceDir, vPlayerCurrentHeading).Getf();
	finalAngleBetweenSplineAndCurrent = fabs( RtoD * finalAngleBetweenSplineAndCurrent );
	if ( finalAngleBetweenSplineAndCurrent > deltaHeadingTolerance )
	{
		return false;
	}

	float inputHeading = fOutDesiredHeading;

	fOutDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(desiredHeading.GetXf(), desiredHeading.GetYf(), 0.0f, 0.0f);
	fOutDesiredPitch = pPlayer->GetDesiredPitch();

	//Apply some previous input
	static dev_float forceScale= 2.0f;
	float t = Max(Min( 1.0f, pullForceMagnitude*forceScale), 0.5f);	
	fOutDesiredHeading = fwAngle::LerpTowards(inputHeading, fOutDesiredHeading, t);
	//Displayf("Pull %f: t: %f, in: %f, goal: %f out: %f", pullForceMagnitude, t, inputHeading*RtoD, saved*RtoD, fOutDesiredHeading*RtoD);

	if(bProcess3d)
	{
		fOutDesiredPitch = AsinfSafe(desiredHeading.GetZf()); 
	}

	pPlayer->SetPedResetFlag( CPED_RESET_FLAG_Prevent180SkidTurns, true );

	fOutDesiredHeading = fwAngle::LimitRadianAngle(fOutDesiredHeading);
	fOutDesiredPitch = fwAngle::LimitRadianAngle(fOutDesiredPitch);

	return true;
}


void CPlayerAssistedMovementControl::UpdateLocalCurvatureAndLookAheadDirection(Vec3V_In vPlayerPosition, Vec3V_In vTangentForceDir, float closestDistAlong, bool bProcess3d)
{
	// Makes an estimate of the local curvature of the steering path. This is used by the follow-ped camera to ensure that the camera
	// swings around much faster in sections of increased curvature.

	m_fLocalRouteCurvature = 0.0f;

	const float actorDistAlong = m_Route.GetClosestPos( RCC_VECTOR3(vPlayerPosition) );
	const float fDistAlongDelta = closestDistAlong - actorDistAlong;
	const float fDistInc = 0.5f * (float)Sign(fDistAlongDelta);

	Vec3V vPreviousDelta(V_ZERO);

	const int iNumStepsPerDirection = 4;

	for(int i=-iNumStepsPerDirection; i<iNumStepsPerDirection; i++)
	{
		const float fStartDistToTestAlong = closestDistAlong + (i * fDistInc);
		const float fEndDistToTestAlong = fStartDistToTestAlong + fDistInc;

		Vec3V vStartPosToTest;
		if(!m_Route.GetAtDistAlong( fStartDistToTestAlong, &RC_VECTOR3(vStartPosToTest) ))
		{
			continue;
		}

		Vec3V vEndPosToTest;
		if(!m_Route.GetAtDistAlong( fEndDistToTestAlong, &RC_VECTOR3(vEndPosToTest) ))
		{
			continue;
		}

#if DEBUG_DRAW
		if((i >= 0) && (CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAssistedMovement))
		{
			grcDebugDraw::Cross(vEndPosToTest, 0.1f, Color_aquamarine);
		}
#endif

		Vec3V vDelta(vEndPosToTest - vStartPosToTest);
		if(!bProcess3d)
		{
			vDelta.SetZ(ScalarV(V_ZERO));
		}

		vDelta = NormalizeSafe( vDelta, vPreviousDelta );

		if( Dot(vPreviousDelta, vDelta ).Getf() > 0.001f)
		{
			if(!bProcess3d)
			{
				const float fLastAngle = fwAngle::GetRadianAngleBetweenPoints(vPreviousDelta.GetXf(), vPreviousDelta.GetYf(), 0.0f, 0.0f);
				const float fNewAngle = fwAngle::GetRadianAngleBetweenPoints(vDelta.GetXf(), vDelta.GetYf(), 0.0f, 0.0f);
				const float fAngle = SubtractAngleShorter(fNewAngle, fLastAngle);
				m_fLocalRouteCurvature += fAngle;
			}
			else
			{
				const float fAngle = AcosfSafe( Dot(vPreviousDelta, vDelta).Getf() );
				m_fLocalRouteCurvature += fAngle;
			}
		}

		vPreviousDelta = vDelta;

		// NOTE: We only update the look-ahead route direction if the farthest look-ahead position is valid. Otherwise, we persist the cached
		// direction.
		if(i == (iNumStepsPerDirection - 1))
		{
			m_vLookAheadRouteDirection = VEC3V_TO_VECTOR3(vEndPosToTest - vPlayerPosition);
			m_vLookAheadRouteDirection.NormalizeSafe(RCC_VECTOR3(vTangentForceDir));
		}
	}
}


void CPlayerAssistedMovementControl::Process(CPed * pPed, Vector2 UNUSED_PARAM(vecStickInput))
{
	bool bForceRoutesUpdate = false;

	if(m_fTeleportedRescanTimer > 0.0f)
	{
		m_fTeleportedRescanTimer -= fwTimer::GetTimeStep();
		if(m_fTeleportedRescanTimer < 0.0f)
		{
			bForceRoutesUpdate = true;
			m_fTeleportedRescanTimer = 0.0f;
		}
	}

	CAssistedMovementRouteStore::Process(pPed, bForceRoutesUpdate);

	//-------------------------------------------------------------------------------------------

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		m_bIsUsingRoute = false;
		return;
	}
#endif // FPS_MODE_SUPPORTED

	//-------------------------------------------------------------------------------------------

	bool bWasOnRoute = m_bIsUsingRoute;
	bool bOnRoute = false;
	float fDesiredHeading = pPed->GetDesiredHeading();
	float fDesiredPitch = pPed->GetDesiredPitch();
	float fDesiredMBR = pPed->GetMotionData()->GetDesiredMbrY();

	if(m_Route.GetSize())
	{
		bOnRoute = GetDesiredHeading(pPed, fDesiredHeading, fDesiredPitch, fDesiredMBR);
	}

	m_bIsUsingRoute = bOnRoute;

	if( bOnRoute )
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);

		fDesiredHeading = fwAngle::LimitRadianAngleSafe(fDesiredHeading);
		fDesiredPitch = fwAngle::LimitRadianAngleSafe(fDesiredPitch);

		//-------------------------------------------------------------------------------------------

		m_bIsUsingRoute = bOnRoute;

		if(pPed->IsStrafing())
		{
			// Construct strafe vector & back-rotate relative to player so it can be applied as a moveblendratio..
			Vector2 vStrafeDir( -rage::Sinf(fDesiredHeading), rage::Cosf(fDesiredHeading) );
			vStrafeDir.Scale( pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag() );
			vStrafeDir.Rotate( -(pPed->GetMotionData()->GetCurrentHeading() + TWO_PI) );
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(vStrafeDir.y, vStrafeDir.x);
		}
		else
		{
			pPed->SetDesiredHeading(fDesiredHeading);
			pPed->SetDesiredPitch(fDesiredPitch);
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(fDesiredMBR);
		}
	}

	//-------------------------------------------------------------------------------------------

	if(!m_bIsUsingRoute)
	{
		m_Route.Clear();

		// If we have just come off the rails, ensure we scan again instantly next frame
		if(bWasOnRoute)
			m_fTimeUntilNextScanForRoutes = 0.0f;

		m_bWasOnStickyRoute = false;

		m_fLocalRouteCurvature = 0.0f;
		m_vLookAheadRouteDirection.Zero();
	}
	else
	{
		m_bWasOnStickyRoute = m_bOnStickyRoute;
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsOnAssistedMovementRoute, m_bIsUsingRoute );
}


bool CPlayerAssistedMovementControl::CanIgnoreBlockingIntersection(WorldProbe::CShapeTestResults * pShapeTestResults) const
{
	if(!pShapeTestResults)
		return true;

	int iNumIntersections = 0;

	for(WorldProbe::CShapeTestResults::Iterator iter = pShapeTestResults->begin(); iter != pShapeTestResults->end(); iter++)
	{
		// If this is a door, and is not fixed, then ignore any intersections with it
		if(iter->GetHitInst() && ((CEntity*)(iter->GetHitInst()->GetUserData()))->GetIsTypeObject())
		{
			CObject * pObj = (CObject*) ((CEntity*)(iter->GetHitInst()->GetUserData()));
			if(pObj->IsADoor() && !pObj->GetIsAnyFixedFlagSet())
				continue;
		}
		iNumIntersections++;
	}
	return (iNumIntersections == 0);
}

void CPlayerAssistedMovementControl::ScanForSnapToRoute(CPed * pPed, const bool bForceScan)
{
	// If already snapped to some rails, then don't scan for any others
	if(m_bIsUsingRoute || !ms_bRouteScanningEnabled)
		return;

	m_fTimeUntilNextScanForRoutes -= fwTimer::GetTimeStep();

	if(m_fTimeUntilNextScanForRoutes <= 0.0f || bForceScan)
	{
		const bool b3d = pPed->GetCurrentMotionTask()->IsUnderWater();

		m_fTimeUntilNextScanForRoutes = m_fScanForRoutesFreq;

		Vector2 vMBR;
		pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMBR);
		if(vMBR.Mag() < 0.1f && !bForceScan)
			return;

		const bool bUnderwater = pPed->GetCurrentMotionTask()->GetTaskType()==CTaskTypes::TASK_MOTION_DIVING;

		Vector3 vMoveVec;
		pPed->GetCurrentMotionTask()->GetMoveVector(vMoveVec);
		if(vMoveVec.Mag2())
			vMoveVec.Normalize();

		const float fMaxDistFromRoute = m_bUseWideLanes ? Max(steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE].WALK_MAX_DISTANCE_FROM_SPLINE_ALLOWED, steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE].RUN_MAX_DISTANCE_FROM_SPLINE_ALLOWED) : 
			SteeringPathConfig::GetMaxDistFromSplineAllowed();

		const float fMaxLookAheadDist = m_bUseWideLanes ? Max(steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE].WALK_LOOK_AHEAD_DIST, steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE].RUN_LOOK_AHEAD_DIST) : 
			SteeringPathConfig::GetMaxLookAheadDist();
		const float fMaxScanDist = fMaxLookAheadDist + Max(fMaxDistFromRoute, 
			m_bUseWideLanes ? Max(steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE].WALK_MAX_INFLUENCE_DISTANCE, steeringPathConfigs[SteeringPathConfig::CONFIG_WIDE].RUN_MAX_INFLUENCE_DISTANCE) : 
			SteeringPathConfig::GetMaxInfluenceDist()) + 4.0f;
		const Vector3 vScanSize(fMaxScanDist, fMaxScanDist, fMaxScanDist);

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		const Vector3 vScanMin = vPedPosition - vScanSize;
		const Vector3 vScanMax = vPedPosition + vScanSize;

		// If we already have a nearby set of rails, then see if we are still intersecting
		// If so then we can just quit here.  If not then we need to reset our stored route.
		if(m_Route.GetSize())
		{
			const Vector3 & vRouteMin = m_Route.GetMin();
			const Vector3 & vRouteMax = m_Route.GetMax();

			if(vScanMin.x >= vRouteMax.x || vScanMin.y >= vRouteMax.y || vScanMin.z >= vRouteMax.z ||
				vRouteMin.x >= vScanMax.x || vRouteMin.y >= vScanMax.y || vRouteMin.z >= vScanMax.z)
			{
				m_Route.Clear();
			}
			else
			{
				return;
			}
		}

		float fClosestSoFar = FLT_MAX;
		int iBestRoute = -1;

		// Create an exclusion list containing the player ped and any attached props.
		static const int nTestTypes = ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
		const CEntity* exclusionList[MAX_NUM_ENTITIES_ATTACHED_TO_PED+2];
		int nExclusions = 0;
		pPed->GeneratePhysExclusionList(exclusionList, nExclusions, MAX_NUM_ENTITIES_ATTACHED_TO_PED, nTestTypes, TYPE_FLAGS_ALL);
		exclusionList[nExclusions++] = pPed;

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc1, capsuleDesc2;
		const Vector3 vFirstCapsuleOffset(0.0f,0.0f,-0.25f);
		const Vector3 vSecondCapsuleOffset(0.0f,0.0f,0.25f);

		capsuleDesc1.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
		capsuleDesc1.SetExcludeEntities(exclusionList, nExclusions);
		capsuleDesc1.SetIsDirected(false);

		capsuleDesc2.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
		capsuleDesc2.SetExcludeEntities(exclusionList, nExclusions);
		capsuleDesc2.SetIsDirected(false);

		// Scan through the stored routes to see if we are in the vicinity of one
		for(int r=0; r<CAssistedMovementRouteStore::MAX_ROUTES; r++)
		{
			const CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(r);
			if(pRoute->GetSize())
			{
				// Only allow strafing on route which have been flagged as appropriate for this
				if(pPed->IsStrafing() && ((pRoute->GetFlags() & CAssistedMovementRoute::RF_IsActiveWhenStrafing)==0) )
					continue;

				const Vector3 & vRouteMin = pRoute->GetMin();
				const Vector3 & vRouteMax = pRoute->GetMax();

				if(vScanMin.x >= vRouteMax.x || vScanMin.y >= vRouteMax.y || vScanMin.z >= vRouteMax.z ||
					vRouteMin.x >= vScanMax.x || vRouteMin.y >= vScanMax.y || vRouteMin.z >= vScanMax.z)
				{
					// Disjoint
				}
				else
				{
					float fClosestOnThisRoute = FLT_MAX;
					Vector3 vClosestOnThisRoute;

					for(int p=0; p<pRoute->GetSize()-1; p++)
					{
						const Vector3 vPos = pRoute->GetPoint(p);
						const Vector3 vVec = pRoute->GetPoint(p+1) - vPos;
						const float fCurrSegTVal = (vVec.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vPos, vVec, vPedPosition) : 0.0f;
						const Vector3 vClosest = vPos + (vVec * fCurrSegTVal);
						Vector3 vToClosest = vClosest - vPedPosition;
						const float fDistToClosest = vToClosest.Mag2();

						if(fDistToClosest < (fMaxDistFromRoute*fMaxDistFromRoute) && fDistToClosest < fClosestOnThisRoute)
						{
							if(!b3d)
								vToClosest.z = 0.0f;
							vToClosest.Normalize();

							const bool bValidSnap = pPed->IsStrafing() ?
								(fDistToClosest < 1.0f && DotProduct(vToClosest, vMoveVec) > 0.0) :
								(fDistToClosest < 1.0f || DotProduct(vToClosest, vMoveVec) > -0.707f);

							if(bValidSnap)
							{
								fClosestOnThisRoute = fDistToClosest;
								vClosestOnThisRoute = vClosest;
							}
						}
					}

					if(fClosestOnThisRoute < FLT_MAX)
					{
						bool bLosClear = false;
						const float fPedRadius = pPed->GetCapsuleInfo()->GetHalfWidth(); 
						if(bUnderwater)
						{
							capsuleDesc1.SetCapsule(vPedPosition, vClosestOnThisRoute, fPedRadius);
#if __BANK
							if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAssistedMovement)
							{
								grcDebugDraw::Line(vPedPosition, vClosestOnThisRoute, Color_cyan, Color_cyan);
							}
#endif
							bLosClear = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc1) ||
								CanIgnoreBlockingIntersection(capsuleDesc1.GetResultsStructure());
						}
						else
						{
							// Check that there is a LOS to this closest position
							capsuleDesc1.SetCapsule(vPedPosition+vFirstCapsuleOffset, vClosestOnThisRoute+vFirstCapsuleOffset, fPedRadius);
							capsuleDesc2.SetCapsule(vPedPosition+vSecondCapsuleOffset, vClosestOnThisRoute+vSecondCapsuleOffset, fPedRadius);
#if __BANK
							if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAssistedMovement)
							{
								grcDebugDraw::Line(vPedPosition, vClosestOnThisRoute, Color_cyan, Color_cyan);
								grcDebugDraw::Line(vPedPosition + vSecondCapsuleOffset, vClosestOnThisRoute + vSecondCapsuleOffset, Color_cyan, Color_cyan);
							}
#endif
							const bool bLosClear1 = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc1) ||
								CanIgnoreBlockingIntersection(capsuleDesc1.GetResultsStructure());

							const bool bLosClear2 = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc2) ||
								CanIgnoreBlockingIntersection(capsuleDesc2.GetResultsStructure());

							bLosClear = bLosClear1 && bLosClear2;
						}

						if(bLosClear)
						{
							if(fClosestOnThisRoute < fClosestSoFar)
							{
								fClosestSoFar = fClosestOnThisRoute;
								iBestRoute = r;
							}
						}
					}
				}
			}
		}
		if(iBestRoute != -1)
		{
			const CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(iBestRoute);
			m_Route.CopyFrom(*pRoute);

#if AI_OPTIMISATIONS_OFF
			Printf("Latching onto route #%i (hash-ID: %x)\n", iBestRoute, pRoute->GetPathStreetNameHash());
#endif
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------

#if __BANK
void CPlayerAssistedMovementControl::AddPointAtPlayerPos()
{
	AddPoint(VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()));
}
void CPlayerAssistedMovementControl::AddPointAtCameraPos()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	AddPoint(vOrigin);
}
void CPlayerAssistedMovementControl::AddPoint(const Vector3 & vPos)
{
	CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(CAssistedMovementRouteStore::MAX_ROUTES-1);

	if(pRoute->GetSize() < CAssistedMovementRoute::MAX_NUM_ROUTE_ELEMENTS)
	{
		pRoute->AddPoint(vPos);
	}
}
void CPlayerAssistedMovementControl::ClearPoints()
{
	CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(CAssistedMovementRouteStore::MAX_ROUTES-1);
	pRoute->Clear();
}
void CPlayerAssistedMovementControl::DumpScript()
{
	CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(CAssistedMovementRouteStore::MAX_ROUTES-1);

	Printf("\n\n");
	Printf("ASSISTED_MOVEMENT_OPEN_ROUTE(N)\n");
	Printf("ASSISTED_MOVEMENT_FLUSH_ROUTE()\n");
	for(int i=0; i<pRoute->GetSize(); i++)
	{
		const Vector3 & vPos = pRoute->GetPoint(i);
		Printf("ASSISTED_MOVEMENT_ADD_POINT(%.2f, %.2f, %.2f)\n", vPos.x, vPos.y, vPos.z);
	}
	Printf("ASSISTED_MOVEMENT_CLOSE_ROUTE()\n");
	Printf("\n\n");
}

void CPlayerAssistedMovementControl::Debug(const CPed * UNUSED_PARAM(pPed)) const
{
	if(ms_bAssistedMovementEnabled)
	{
		for(int p=0; p<m_Route.GetSize()-1; p++)
		{
			grcDebugDraw::Line(m_Route.GetPoint(p), m_Route.GetPoint(p+1), Color_LightBlue, Color_LightBlue);
		}

	}
	if(m_Route.GetSize())
	{
		grcDebugDraw::BoxAxisAligned(RCC_VEC3V(m_Route.GetMin()), RCC_VEC3V(m_Route.GetMax()), Color_LightSeaGreen, false);
	}
}
#endif	// __BANK

#endif	// __PLAYER_ASSISTED_MOVEMENT
