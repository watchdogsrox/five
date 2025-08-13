#include "TaskVehicleGoToPlane.h"

// Game headers
#include "Vehicles/Planes.h"
#include "VehicleAi\task\TaskVehicleFlying.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "vehicleAi/FlyingVehicleAvoidance.h"
#include "pheffects/wind.h"
#include "scene/world/GameWorldHeightMap.h"

VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
 
/////////////////////////////////////////////////////////////////////

CTaskVehicleGoToPlane::Tunables CTaskVehicleGoToPlane::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleGoToPlane, 0x8935eceb);

CTaskVehicleGoToPlane::CTaskVehicleGoToPlane(  const sVehicleMissionParams& params,
											 const int iFlightHeight /*= 30*/, 
											 const int iMinHeightAboveTerrain /*= 20*/, 
											 const bool bPrecise,
											 const bool bUseDesiredOrientation,
											 const float fDesiredOrientation) :
CTaskVehicleGoTo(params),
m_iFlightHeight(iFlightHeight),
m_iMinHeightAboveTerrain(iMinHeightAboveTerrain),
m_bPrecise(bPrecise),
m_bUseDesiredOrientation(bUseDesiredOrientation),
m_fDesiredOrientation(fDesiredOrientation),
m_bForceHeightUpdate(true),
m_fDesiredPitch(0.0f),
m_fDesiredTurnRadius(10000.0f),
m_bIgnoreZDeltaForEndCondition(false),
m_VirtualSpeedControlMode(VirtualSpeedControlMode_On),
m_fVirtualSpeedDescentRate(1.5f),
m_bTurnOnEngineAtStart(false),
m_fMinTurnRadiusPadding(Max(0.0f, params.m_fTargetArriveDist/2.0f)), 
m_bForceDriveOnGround(false),
m_fMaxThrottle(sm_Tunables.m_maxThrottle),
m_fMaxOverriddenRoll(-1.0f),
m_bAvoidTerrainMaxZThisFrame(true),
m_fMinDistanceToTargetForRollComputation(sm_Tunables.m_minMinDistanceForRollComputation),
m_fFlyingVehicleAvoidanceScalar(2.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_PLANE);
}

/////////////////////////////////////////////////////////////////////

CTaskVehicleGoToPlane::~CTaskVehicleGoToPlane()
{

}

/////////////////////////////////////////////////////////////////////

aiTask* CTaskVehicleGoToPlane::Copy() const
{
	CTaskVehicleGoToPlane* pCopy = rage_new CTaskVehicleGoToPlane(m_Params, m_iFlightHeight,
		m_iMinHeightAboveTerrain,
		m_bPrecise,
		m_bUseDesiredOrientation,
	m_fDesiredOrientation);

	pCopy->m_bForceHeightUpdate = m_bForceHeightUpdate;
	pCopy->m_fDesiredPitch = m_fDesiredPitch;
	pCopy->m_fDesiredTurnRadius = m_fDesiredTurnRadius;
	pCopy->m_bIgnoreZDeltaForEndCondition = m_bIgnoreZDeltaForEndCondition;
	pCopy->m_VirtualSpeedControlMode = m_VirtualSpeedControlMode;
	pCopy->m_fVirtualSpeedDescentRate = m_fVirtualSpeedDescentRate;
	pCopy->m_bTurnOnEngineAtStart = m_bTurnOnEngineAtStart;
	pCopy->m_fMinTurnRadiusPadding = m_fMinTurnRadiusPadding;
	pCopy->m_bForceDriveOnGround = m_bForceDriveOnGround;
	pCopy->m_fMaxThrottle = m_fMaxThrottle;
	pCopy->m_fMaxOverriddenRoll = m_fMaxOverriddenRoll;
	pCopy->m_bAvoidTerrainMaxZThisFrame = m_bAvoidTerrainMaxZThisFrame;
	pCopy->m_fMinDistanceToTargetForRollComputation = m_fMinDistanceToTargetForRollComputation;
	pCopy->m_fFlyingVehicleAvoidanceScalar = m_fFlyingVehicleAvoidanceScalar;

	return pCopy;
}

/////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::CleanUp()
{
	// Script can run this task without a pilot via a parameter in TASK_PLANE_GOTO_PRECISE_VTOL. We need to clear this off the vehicle when any other task is set.
	if (GetVehicle() && GetVehicle()->GetIntelligence())
	{
		GetVehicle()->GetIntelligence()->SetUsingScriptAutopilot(false);
	}
}

/////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToPlane::ProcessPreFSM()
{
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane->GetPlaneIntelligence()->CheckTimeToRetractLandingGear() )
	{
		pPlane->GetPlaneIntelligence()->SetRetractLandingGearTime(0xffffffff);
		CLandingGear::eLandingGearPublicStates eGearState = pPlane->GetLandingGear().GetPublicState();
		if ( eGearState == CLandingGear::STATE_LOCKED_DOWN 
			|| eGearState == CLandingGear::STATE_DEPLOYING )
		{
			pPlane->GetLandingGear().ControlLandingGear(pPlane, CLandingGear::COMMAND_RETRACT);
		}
	}
	pPlane->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToPlane::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// 
		FSM_State(State_GoTo)
			FSM_OnEnter
				Goto_OnEnter(pVehicle);
			FSM_OnUpdate
				return Goto_OnUpdate(pVehicle);

		//FSM_State(State_Land)
		//	FSM_OnEnter
		//		Land_OnEnter(pVehicle);
		//	FSM_OnUpdate
		//		return Land_OnUpdate(pVehicle);
	FSM_End
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::SetOrientation(float fRequestedOrientation)
{
	m_fDesiredOrientation = fRequestedOrientation;
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::SetOrientationRequested(bool bOrientationRequested)
{
	m_bUseDesiredOrientation = bOrientationRequested;
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::SetIsPrecise(bool bPrecise)
{
	m_bPrecise = bPrecise;
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::SetFlightHeight(int flightHeight)
{
	if(m_iFlightHeight != flightHeight)
	{
		m_iFlightHeight = flightHeight;	

		//make sure we update the flight height on the next update
		//this stops a bug where the plane doesn't try to get to the flightheight until it does a linescan test to the ground.
		m_bForceHeightUpdate = true;
	}
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::SetMinHeightAboveTerrain(int minHeightAboveTerrain)
{
	m_iMinHeightAboveTerrain = minHeightAboveTerrain;
}

void CTaskVehicleGoToPlane::SetIgnoreZDeltaForEndCondition( bool in_Value)
{
	m_bIgnoreZDeltaForEndCondition = in_Value;
}

void CTaskVehicleGoToPlane::SetVirtualSpeedControlMode( VirtualSpeedControlMode in_Mode )
{
	m_VirtualSpeedControlMode = in_Mode;
}

void CTaskVehicleGoToPlane::SetVirtualSpeedMaxDescentRate(float in_rate)
{
	m_fVirtualSpeedDescentRate = in_rate;
}

void CTaskVehicleGoToPlane::SetForceDriveOnGround(bool in_Value)
{
	m_bForceDriveOnGround = in_Value;
}

void CTaskVehicleGoToPlane::SetMaxThrottle(float in_fValue)
{
	m_fMaxThrottle = in_fValue;
}

void CTaskVehicleGoToPlane::SetMinDistanceToTargetForRollComputation(float in_Value)
{ 
	m_fMinDistanceToTargetForRollComputation = in_Value; 
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::Goto_OnEnter	(CVehicle* pVehicle)
{
	Assert(pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE);
	CPlane* plane = static_cast<CPlane*>(pVehicle);
	const bool useVTOL = m_bPrecise && plane->GetVerticalFlightModeAvaliable();
	plane->SetDesiredVerticalFlightModeRatio(useVTOL ? 1.0f : 0.0f);

	if (m_bTurnOnEngineAtStart && !pVehicle->IsEngineOn())
	{
		pVehicle->SwitchEngineOn(true);
	}

	m_flyingAvoidanceHelper.Init(plane, m_fFlyingVehicleAvoidanceScalar);
}

//////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToPlane::Goto_OnUpdate	(CVehicle* pVehicle)
{
	//update the target coords
	FindTargetCoors(pVehicle, m_Params);
	Vector3 targetPosition = m_Params.GetTargetPosition();

	// use the nose of the plane to determine when we've arrived at target
	// large planes have trouble navigation on the ground and other when steering from middle of plane
	//Vector3 vPlaneNosePos = VEC3V_TO_VECTOR3(static_cast<CPlane*>(pVehicle)->ComputeNosePosition());
	Vector3 vPlaneNosePos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(GetVehicle(), false));	

	float distance2;
	if ( m_bIgnoreZDeltaForEndCondition )
	{
		distance2 = (targetPosition - vPlaneNosePos).XYMag2();	
	}
	else
	{
		distance2 = (targetPosition - vPlaneNosePos).Mag2();
	}

	const bool bAllowAvoidance = true;
	const float fTargetModDistance = 90000.0f;
	if ( bAllowAvoidance && distance2 > fTargetModDistance )
	{
		float ratioToUse = Clamp((distance2 - fTargetModDistance) / 250000.0f, 0.0f, 1.0f );
		float finalScaler = Lerp( ratioToUse, 0.0f, m_fFlyingVehicleAvoidanceScalar);
		CFlyingVehicleAvoidanceManager::SlideDestinationTarget(targetPosition, RegdVeh(pVehicle), targetPosition, finalScaler);
		distance2 = (targetPosition - vPlaneNosePos).Mag2();
	}

	if ( distance2 < square(m_Params.m_fTargetArriveDist) )
	{
		if(!IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved))
		{
			m_bMissionAchieved = true;
			//SetState(State_Land);//not implemented yet. so just quit.
			return FSM_Quit;
		}
	}

	Assert(pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE);

	Plane_SteerToTargetCoors((CAutomobile *)pVehicle, targetPosition);

	return FSM_Continue;
}

//not implemented yet.
/////////////////////////////////////////////////////////////////////////////////
//
//void		CTaskVehicleGoToPlane::Land_OnEnter						(CVehicle* UNUSED_PARAM(pVehicle))
//{
//	SetNewTask(rage_new CTaskVehicleLand(&m_vTargetPos));
//}
//
///////////////////////////////////////////////////////////////////////////////////
//
//CTask::FSM_Return	CTaskVehicleGoToPlane::Land_OnUpdate						(CVehicle* UNUSED_PARAM(pVehicle))
//{
//	return FSM_Continue;
//}

/////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToPlane::Plane_SteerToTargetCoors(CAutomobile *pVeh, const Vector3 &targetPos)
{
	CPlane *pPlane = (CPlane *)pVeh;

	// Find the direction we want the plane to go into.
	const Vector3 vPlanePosition = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());

	if (m_bPrecise && pPlane->GetVerticalFlightModeAvaliable() && pPlane->GetVerticalFlightModeRatio() >= 1.0f)
	{
		if (m_bUseDesiredOrientation)
		{
			pPlane->m_FlightDirection = m_fDesiredOrientation;
			Plane_SteerToTargetPrecise_FixedOrientation(pPlane, pPlane->m_FlightDirection, targetPos, false, false, false);
		}
		else
		{
			pPlane->m_FlightDirection = fwAngle::GetATanOfXY(targetPos.x - vPlanePosition.x, targetPos.y - vPlanePosition.y);
			Plane_SteerToTargetPrecise(pPlane);
		}
	}
	else
	{
		pPlane->m_FlightDirection = fwAngle::GetATanOfXY(targetPos.x - vPlanePosition.x, targetPos.y - vPlanePosition.y);
		Plane_SteerToTarget(pPlane, targetPos);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FlyAIPlaneInCertainDirection
// PURPOSE :  Makes an AI Plane go towards its target coordinates.
/////////////////////////////////////////////////////////////////////////////////
static dev_float PLANE_CRUISE_SPEED_MULTIPLIER = 2.0f;
void CTaskVehicleGoToPlane::Plane_SteerToTargetPrecise(CPlane *pPlane)
{
	const Vector3	target				(m_Params.GetTargetPosition());
	const Vector3	targetDelta			(target - VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition()));
	const float		targetDeltaAngle	= fwAngle::GetATanOfXY(targetDelta.x, targetDelta.y);
	const Vector3	targetDeltaFlat		(targetDelta.x, targetDelta.y, 0.0f);
	const float		targetDistanceFlat	= targetDeltaFlat.Mag();

	Vector3	forwardDirFlat = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
	forwardDirFlat.z = 0.0f;
	forwardDirFlat.Normalize();

	// Once in a while the heli will do a line scan ahead. If we hit the map we need to find an alternative point
	// to fly to first.
	if( m_bForceHeightUpdate == true || (((fwTimer::GetTimeInMilliseconds() + pPlane->GetRandomSeed()) / CTaskVehicleMissionBase::AIUPDATEFREQHELI) != ((fwTimer::GetPrevElapsedTimeInMilliseconds() + pPlane->GetRandomSeed()) / CTaskVehicleMissionBase::AIUPDATEFREQHELI)) )
	{
		// Read the height from the map at the coordinates of the ground found by a 45 degrees line.
		const float estimatedGroundHeight = Vehicle_TestForFutureWorldCollisions(pPlane, forwardDirFlat, targetDeltaAngle);

		// Set the flight height to be lower as we're getting closer to the target
		const float desiredHeight = rage::Clamp(target.z, static_cast<float>(m_iFlightHeight), target.z + targetDistanceFlat);
		pPlane->m_DesiredHeight = rage::Max(desiredHeight, estimatedGroundHeight + m_iMinHeightAboveTerrain);

		m_bForceHeightUpdate = false;
	}

	// Predict our future orientation.
	static float orientationPredictionTime = 0.3f;
	Vector3 vecForward(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB()));
	const float currOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
	const float orientationDeltaPrev = fwAngle::LimitRadianAngleSafe(currOrientation - pPlane->m_OldOrientation);
	const float predictedOrientation = currOrientation + orientationDeltaPrev * (orientationPredictionTime / fwTimer::GetTimeStep());

	// Make the heli face the target coordinates.
	const float toTargetPredictedOrienationDelta = fwAngle::LimitRadianAngleSafe(targetDeltaAngle - predictedOrientation);
	const float fYawControlUnclamped = toTargetPredictedOrienationDelta * -2.0f;
	const float fYawControl = rage::Clamp(fYawControlUnclamped, -1.0f, 1.0f);
	pPlane->SetYawControl(fYawControl);

	// Calculate the amount of height delta to not crash into the ground.
	static float heightPredictionTime = 2.0f;
	float predictedHeight = pPlane->GetTransform().GetPosition().GetZf() + (pPlane->GetVelocity().z * heightPredictionTime);
	float predictedDeltaToHeightDesired = pPlane->m_DesiredHeight - predictedHeight;

	// Calculate the desired speed we want to go at.
	static float desiredSpeedToDistanceRatio = 0.2f;
	const float fCruiseSpeed = GetCruiseSpeed();
	float desiredSpeed = targetDistanceFlat * desiredSpeedToDistanceRatio;
	desiredSpeed = rage::Min(desiredSpeed, fCruiseSpeed * PLANE_CRUISE_SPEED_MULTIPLIER);

	// If we're not facing the target we reduce our desired speed.
	float toTargetPredictedOrienationMag = ABS(toTargetPredictedOrienationDelta);
	if(toTargetPredictedOrienationMag > HALF_PI)
	{
		// If we're facing the wrong direction we stop altogether.
		desiredSpeed = 0.0f;
	}
	else
	{
		desiredSpeed *= rage::Max(0.05f, 1.0f - ((toTargetPredictedOrienationMag / HALF_PI) * 0.3f));
	}
	const float currentSpeedForward = forwardDirFlat.Dot(pPlane->GetVelocity());
	const float speedDesired = desiredSpeed - currentSpeedForward;

	float throttle = 0.05f + predictedDeltaToHeightDesired / 10.0f;
	static float minThrottle = -2.0f;
	throttle = rage::Max(minThrottle, throttle);
	static	float throttleToSpeedDeltaDesiredRatio = 0.1f;
	throttle += rage::Clamp((speedDesired * throttleToSpeedDeltaDesiredRatio), 0.0f, 1.0f);
	throttle = rage::Clamp(throttle, -1.0f, 1.0f);

	TUNE_GROUP_FLOAT(PLANE_AI, sfExtraThrottle, 0.0f, -10.0f, 10.0f, 0.01f);
	pPlane->SetThrottleControl( sfExtraThrottle + throttle);

	// In this new code we calculate the pitch that we want first.
	static float pitchToDesiredSpeedRatio	= -0.1f;
	static float minDesiredPitch			= -0.8f;
	static float maxDesiredPitch			= 0.8f;
	static float pitchPredictionTime		= 1.5f;
	static float pitchControlMultiplier		= -2.0f;
	static float maxPitch					= 1.0f;
	static float minPitch					= -1.0f;
	float desiredPitch = speedDesired * pitchToDesiredSpeedRatio;
	desiredPitch = rage::Clamp(desiredPitch, minDesiredPitch, maxDesiredPitch);
	vecForward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
	const float pitch = rage::Atan2f(vecForward.z, vecForward.XYMag());
	float predictedPitch = pitch + (pitch - pPlane->m_OldTilt) * (pitchPredictionTime / fwTimer::GetTimeStep());
	float pitchControl = (predictedPitch - desiredPitch) * pitchControlMultiplier;
	pitchControl = rage::Clamp(pitchControl, minPitch, maxPitch);
	// Now we reduce the pitch control if we want to increase height.
	float pitchMultiplierForDeltaToHeightDesired = rage::Clamp((1.0f - (predictedDeltaToHeightDesired / 20.0f)), 0.0f, 1.0f);
	pitchControl *= pitchMultiplierForDeltaToHeightDesired;

	// We still may want to apply a little bit of pitch to stabilize the heli as the physics does not do that anymore.
	static float pitchPredictionTime2 = 0.25f;
	const float predictedPitch2 = pitch + (pitch - pPlane->m_OldTilt) * (pitchPredictionTime2 / fwTimer::GetTimeStep());
	static float stabilisingPitchMult = -2.0f;
	const float stabilisingPitch = rage::Clamp((predictedPitch2 * stabilisingPitchMult), -1.0f, 1.0f);
	pitchControl += stabilisingPitch * (1.0f - pitchMultiplierForDeltaToHeightDesired);
	pPlane->SetPitchControl(pitchControl);

	// Use roll control to avoid sideways drift. This causes the heli to circle the target sometimes.
	const float currentSpeedSideways = DotProduct(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetA()), pPlane->GetVelocity());
	const float rollControlDistMultiplier = rage::Max(0.0f, (1.0f - targetDistanceFlat / 150.0f));		// Only do this stuff near the target as it makes heli sway
	static float rollControlToSideSpeedRatio = 0.1f;
	float rollControl = currentSpeedSideways * rollControlToSideSpeedRatio * rollControlDistMultiplier;
	static float maxRollControl = 0.3f;
	rollControl = rage::Clamp(rollControl, -maxRollControl, maxRollControl);
	rollControl = -rollControl;

	pPlane->SetRollControl(rollControl);

	pPlane->m_OldTilt = asinf(vecForward.z);
	pPlane->m_OldOrientation = currOrientation;

	// If we're almost there we go for the fixed orientation mode.
	// DEBUG!! -AC, Added a block here, just to make sure stuff isn't weird.
	TUNE_GROUP_BOOL(PLANE_AI, Heli_holdOrientationWhenClose, false);
	if(Heli_holdOrientationWhenClose)
	{
		if(targetDistanceFlat < 30.0f )
		{
			//SetOrientationRequested(true);
			m_bUseDesiredOrientation = true;
			vecForward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
			m_fDesiredOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y) /*- (90.0f*DtoR)*/;
		}
	}
	// END DEBUG!!
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SteerToTarget_FixedOrientation
// PURPOSE :  Makes an AI heli go towards a target. (whilst maintaining a certain orientation)
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToPlane::Plane_SteerToTargetPrecise_FixedOrientation(CPlane *pPlane, float orientationToAchieveAndKeep, const Vector3& target, bool bLanding, bool bForceHeight /*= false*/, bool bLimitDescendSpeed /*= false*/)
{
	const Vector3	targetDelta			(target - VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition()));
	const Vector3	targetDeltaFlat		(targetDelta.x, targetDelta.y, 0.0f);

	Vector3	forwardDirFlat = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
	forwardDirFlat.z = 0.0f;
	forwardDirFlat.Normalize();

	if(bForceHeight)
	{
		pPlane->m_DesiredHeight = target.z;
	}
	else
	{
		// Once in a while the heli will do a line scan ahead. If we hit the map we need to find an alternative point
		// to fly to first.
		if(m_bForceHeightUpdate == true || ( ((fwTimer::GetTimeInMilliseconds() + pPlane->GetRandomSeed()) / CTaskVehicleMissionBase::AIUPDATEFREQHELI) != ((fwTimer::GetPrevElapsedTimeInMilliseconds() + pPlane->GetRandomSeed()) / CTaskVehicleMissionBase::AIUPDATEFREQHELI)) )
		{
			const float		targetDeltaAngle	= fwAngle::GetATanOfXY(targetDelta.x, targetDelta.y);
			// Read the height from the map at the coordinates of the ground found by a 45 degrees line.
			const float estimatedGroundHeight = Vehicle_TestForFutureWorldCollisions(pPlane, forwardDirFlat, targetDeltaAngle);

			const float		targetDistanceFlat	= targetDeltaFlat.Mag();
			// Set the flight height to be lower as we're getting closer to the target

			const float desiredHeight = rage::Clamp(static_cast<float>(m_iFlightHeight), target.z, target.z + targetDistanceFlat);
			pPlane->m_DesiredHeight = rage::Max(desiredHeight, estimatedGroundHeight + m_iMinHeightAboveTerrain);

			m_bForceHeightUpdate = false;
		}
	}

	// Predict our future orientation.
	static float orientationPredictionTime = 0.3f;
	Vector3 vecForward(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB()));
	const float currOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
	const float orientationDeltaPrev = fwAngle::LimitRadianAngleSafe(currOrientation - pPlane->m_OldOrientation);
	const float predictedOrientation = currOrientation + orientationDeltaPrev * (orientationPredictionTime / fwTimer::GetTimeStep());

	//// Make the heli face the target coordinates.
	//const float toTargetPredictedOrienationDelta = fwAngle::LimitRadianAngleSafe(targetDeltaAngle - predictedOrientation);
	//const float fYawControlUnclamped = toTargetPredictedOrienationDelta * -2.0f;
	//const float fYawControl = rage::Clamp(fYawControlUnclamped, -1.0f, 1.0f);
	//pHeli->SetYawControl(fYawControl);

	// Make the heli face the target coordinates.
	const float orientationDeltaDesired = fwAngle::LimitRadianAngleSafe(orientationToAchieveAndKeep - predictedOrientation);
	const float fYawControlUnclamped = -orientationDeltaDesired;
	const float fYawControl = rage::Clamp(fYawControlUnclamped, -1.0f, 1.0f);
	pPlane->SetYawControl(fYawControl);

	// Calculate the amount of height delta to not crash into the ground.
	static float heightPredictionTime = 2.0f;
	float predictedHeight = pPlane->GetTransform().GetPosition().GetZf() + (pPlane->GetVelocity().z * heightPredictionTime);
	float desiredHeight = pPlane->m_DesiredHeight;
	float predictedDeltaToHeightDesired = desiredHeight - predictedHeight;


	float throttle = 0.0f;		// Guess this to stay at same height. (was 0.3f)
	if(predictedDeltaToHeightDesired > 0.0f)
	{	// We want to climb
		throttle = predictedDeltaToHeightDesired / 10.0f;
	}
	else
	{	// We can be pretty rough in the descend since the heli physics slow the guy down a lot anyway.
		throttle = predictedDeltaToHeightDesired / 5.0f;
	}
	if(bLimitDescendSpeed)
	{
		static float limitSpeed = -3.0f; // limit in m/s
		const float limitThrottle = (limitSpeed - pPlane->GetVelocity().z);
		throttle = rage::Max(throttle, limitThrottle);
	}
	float steeringDownMult = 1.0f;
	if(throttle > 5.0f)
	{
		steeringDownMult = rage::Max(0.0f, (1.0f - (throttle - 5.0f) * 0.05f));
	}
	Assert(steeringDownMult >= 0.0f && steeringDownMult <= 1.0f);
	throttle = rage::Clamp(throttle, -0.5f, 1.0f);

	TUNE_GROUP_FLOAT(PLANE_AI, sfExtraThrottle2, 0.0f, -10.0f, 10.0f, 0.01f);
	pPlane->SetThrottleControl( sfExtraThrottle2 + throttle);

	Vector3	rightward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetA());
	rightward.z = 0.0f;
	rightward.Normalize();

	//BL - New way to calculate roll, using a PID controller
	TUNE_GROUP_FLOAT(PLANE_AI, rollPIDConstantKp, 0.016f, -5.0f, 5.0f, 0.001f);
	TUNE_GROUP_FLOAT(PLANE_AI, rollPIDConstantKi, 0.0f,	-5.0f, 5.0f, 0.001f);
	TUNE_GROUP_FLOAT(PLANE_AI, rollPIDConstantKd, 0.048f, -5.0f, 5.0f, 0.001f);

	const float error = DotProduct(targetDeltaFlat, rightward);

	if( pPlane->GetVtolRollVariablesInitialised() == false)//make sure the previous error is a sensible value other wise it may overshoot
	{
		pPlane->SetVtolRollPreviousError(error);
		pPlane->SetVtolRollVariablesInitialised(true);
	}

	const float heliRollIntegral = pPlane->GetVtolRollIntegral() + (error*( fwTimer::GetTimeStep() ));//calculate error over time
	pPlane->SetVtolRollIntegral( heliRollIntegral ); 
	const float derivative = (error - pPlane->GetVtolRollPreviousError())/( fwTimer::GetTimeStep());
	const float roll = ((rollPIDConstantKp*error) + ((rollPIDConstantKi/1000.0f)*pPlane->GetVtolRollIntegral()) + (rollPIDConstantKd*derivative));//output from PID controller

	const float rollClamped = steeringDownMult * rage::Clamp(roll, -0.75f, 0.75f);//clamp the output 
	pPlane->SetVtolRollPreviousError(error); //keep track of the error 

	pPlane->SetRollControl(rollClamped);

	// Adjust the yaw.  this is necessary because the controls are
	// currently set up to automatically apply some yaw whenever there
	// is some roll.
	//float currentYawContol = pHeli->GetYawControl();
	//TUNE_GROUP_FLOAT(HELI_AI, rollControlToYawControlRatio, -0.25f, -5.0, 5.0f, 0.1f);
	//float adjustedYawControl = currentYawContol + (rollControl * rollControlToYawControlRatio);
	//pHeli->SetYawControl(adjustedYawControl);

	// Move forward/back to regulate our distance to the target.
	float targetForwardDist = DotProduct(targetDeltaFlat, forwardDirFlat);
	float forwardSpeed = DotProduct(pPlane->GetVelocity(), forwardDirFlat);
	static float forwardPredictionTime = 4.0f;
	targetForwardDist -= forwardSpeed * forwardPredictionTime;
	static dev_float fTweak4 = -0.005f;		// This was -0.005f but to stop overshooting on the landing it needed to be -0.01f
	static dev_float fTweak4Landing = -0.01f;
	static dev_float fTweak5 = 0.0f;
	vecForward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
	float	pitch = rage::Atan2f(vecForward.z, vecForward.XYMag());
	float pitchControl = 0.0f;
	if(bLanding)
	{
		pitchControl = ((targetForwardDist + pitch * fTweak5) * fTweak4Landing);
	}
	else
	{
		pitchControl = ((targetForwardDist + pitch * fTweak5) * fTweak4);
	}
	pPlane->SetPitchControl(steeringDownMult * Clamp(pitchControl, -0.7f, 0.7f));

	pPlane->m_OldOrientation = currOrientation;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FlyAISpaceShipInCertainDirection
// PURPOSE :  Makes an AI plane go towards its target coordinates.
/////////////////////////////////////////////////////////////////////////////////

#define AIUPDATEFREQ (1000)

float CTaskVehicleGoToPlane::ComputeMaxRollForPitch( float in_Pitch, float maxRoll )
{
	static float sm_PitchToRollRatio =  1.0f/ 60.0f;
	// linearly decrease roll as pitch approaches 45 degrees
	float fRollStrength = Clamp(1.0f - in_Pitch * sm_PitchToRollRatio, 0.0f, 1.0f);

	return maxRoll * fRollStrength;
}

float CTaskVehicleGoToPlane::GetMinAirSpeed(const CPlane *pPlane)
{
	float minSpeed = 0.0f;
	CFlyingHandlingData* pFlyingHandling = pPlane->pHandling->GetFlyingHandlingData();
	if(pFlyingHandling)
	{
		minSpeed = sqrt( 1.0f / pFlyingHandling->m_fFormLiftMult );
	}
	return minSpeed;
}


float CTaskVehicleGoToPlane::ComputeRollForTurnRadius(CPlane *pPlane, float in_Speed, float in_Radius)
{
	float roll = 0.0f;
	CFlyingHandlingData* pFlyingHandling = pPlane->pHandling->GetFlyingHandlingData();
	if(pFlyingHandling)
	{
		// first compute our wing acceleration
		float minSpeed = sqrt( 1.0f / pFlyingHandling->m_fFormLiftMult );
		float virtualspeed = Max(minSpeed, in_Speed);
		float fHealthMult = pPlane->GetAircraftDamage().GetLiftMult(pPlane);
		float fFormLift = pFlyingHandling->m_fFormLiftMult;
		float fWingAcceleration = (fFormLift)*square(virtualspeed)*-GRAVITY;
		fWingAcceleration *= fHealthMult;

		// compute roll given speed radius and acceleration
		roll = asin( Clamp(square(in_Speed) / ( Sign(in_Radius) * Max(Abs(in_Radius), 0.5f)  * fWingAcceleration), -1.0f, 1.0f)) * RtoD;

		static float s_HackTune = 1.0f;
		roll *= s_HackTune;
	}
	return roll;
}

float CTaskVehicleGoToPlane::ComputeTurnRadiusForRoll(const CPlane *pPlane, float in_Speed, float in_RollDegrees, bool bIgnoreHealth)
{
	float posRollDegrees = fabsf(in_RollDegrees);
	if ( posRollDegrees >= 0.01f )
	{
		float radius = 0.0f;
		CFlyingHandlingData* pFlyingHandling = const_cast<CPlane*>(pPlane)->pHandling->GetFlyingHandlingData(); // just ignore the const_cast horribleness
		if(pFlyingHandling)
		{
			// first compute our wing acceleration
			float minSpeed = sqrt( 1.0f / pFlyingHandling->m_fFormLiftMult );
			float virtualspeed = Max(minSpeed, in_Speed);
			float fHealthMult = bIgnoreHealth ? 1.0f : pPlane->GetAircraftDamage().GetLiftMult(const_cast<CPlane*>(pPlane));
			float fFormLift = pFlyingHandling->m_fFormLiftMult;
			float fWingAcceleration = (fFormLift)*square(virtualspeed)*-GRAVITY;
			fWingAcceleration *= fHealthMult;
			float deNominator = (fWingAcceleration * rage::Sinf(posRollDegrees * DtoR));
			if ( deNominator > 0 )
			{
				// compute radius give speed acceleration and roll
				radius = square(in_Speed) / deNominator;
				static float s_HackTune = 1.0f;
				radius *= s_HackTune;
			}
		}
		return radius;
	}

	// just return really big number
	return 10000000.0f;
}

float CTaskVehicleGoToPlane::ComputeTurnRadiusForTarget(const CPlane* pPlane, const Vector3& vTargetPos, float fMinDistanceToTarget)
{
	static float sm_MaxTurnRadius = 10000000.0f; // essentially a straight line in short distance
	const Vector3 vPlanePos = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());
	const Vector3 vPlaneFwd = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
	Vector3 vPlaneToTarget = vTargetPos-vPlanePos;
	Vector3 vDirPlaneToTarget = vPlaneToTarget;
	vDirPlaneToTarget.NormalizeSafe(vPlaneFwd);

	// going to compute a right triangle where the 
	// hypotenuse is the radius and the adjacent
	// is half the length to the target and the
	// angle is the complement of the angle from fwd

	float angleToTarget = 0.0f;

	// this is asserting with a nan so lets gaurd the anglez check
	float mag2 = (vPlaneFwd.x * vPlaneFwd.x + vPlaneFwd.y * vPlaneFwd.y) * (vDirPlaneToTarget.x * vDirPlaneToTarget.x + vDirPlaneToTarget.y * vDirPlaneToTarget.y);
	if ( mag2 > 0 )
	{
		angleToTarget = vPlaneFwd.AngleZ(vDirPlaneToTarget) * RtoD;
	}

	float distance = vPlaneToTarget.Mag();
	distance = Max(distance, fMinDistanceToTarget );

	float adjacent = distance / 2;
	float complementAngleToTarget = (90.0f - fabsf(angleToTarget));
	float cosAngleToTarget = rage::Cosf(complementAngleToTarget* DtoR); 
	float signAngle = (angleToTarget >= 0.0f) ? 1.0f : -1.0f;
	float fDesiredTurnRadius = sm_MaxTurnRadius;
	if ( fabsf(cosAngleToTarget) > 0 )
	{
		fDesiredTurnRadius = signAngle * (adjacent / cosAngleToTarget);
	}
	
	return fDesiredTurnRadius;
}

float CTaskVehicleGoToPlane::ComputeSpeedForRadiusAndRoll(CPlane& in_Plane, float in_TurnRadius, float in_RollDegrees)
{
	float speed = 0.0f;
	CFlyingHandlingData* pFlyingHandling = in_Plane.pHandling->GetFlyingHandlingData(); // just ignore the const_cast horribleness
	if(pFlyingHandling)
	{
		// first compute our wing acceleration
		float minSpeed = sqrt( 1.0f / pFlyingHandling->m_fFormLiftMult );
		float maxSpeed =  in_Plane.pHandling->m_fEstimatedMaxFlatVel;
		float virtualspeed = minSpeed;
		float fHealthMult = in_Plane.GetAircraftDamage().GetLiftMult(&in_Plane);
		float fFormLift = pFlyingHandling->m_fFormLiftMult;

		float speedNoVSpeed = sqrt(in_TurnRadius * fFormLift * -GRAVITY * fHealthMult * rage::Sinf(in_RollDegrees * DtoR));

		// best guess
		if ( speedNoVSpeed > 1.0f )
		{
			virtualspeed = maxSpeed;
		}
		speed = speedNoVSpeed * virtualspeed;
	}

	return speed;
}


ScalarV OneMinusNormalDot(Vec2V_In a, Vec2V_In b)
{
	ScalarV vDot = Dot(a, b);
	ScalarV vNormalDot = (vDot + ScalarV(1.0f)) / ScalarV(2.0f);
	return ScalarV(1.0f) - vNormalDot;
}

//
// compute location on circle that will line us up with target
//
bool CTaskVehicleGoToPlane::ComputeApproachTangentPosition(Vector3& o_TangentPos, const CPlane* pPlane, const Vector3& in_vTargetPos, const Vector3& in_vDirection, float in_Speed, float in_MaxRoll, bool UNUSED_PARAM(bIgnoreAlignMent), bool DEBUG_DRAW_ONLY(in_bDebugDraw ))
{
	o_TangentPos = in_vTargetPos;

	const Vec2V vPlanePosition = pPlane->GetTransform().GetPosition().GetXY();
	const Vec2V vPlaneForward = Normalize(pPlane->GetTransform().GetForward().GetXY());
	const Vec2V vTargetPos = VECTOR3_TO_VEC3V(in_vTargetPos).GetXY();
	Vec2V vDesiredDirection = VECTOR3_TO_VEC3V(in_vDirection).GetXY();
	Vec2V vPlaneToTargetDirection = NormalizeSafe(vTargetPos - vPlanePosition, vPlaneForward);

	static float s_RollMult = 0.9f;
	static float s_RadiusMultMax = 1.2f;
	static float s_RadiusMultMin = 1.0f;
	static float s_MinDistanceToTangent = 1.0f;

	ScalarV vExpandRadiusMult = Lerp( OneMinusNormalDot(vPlaneToTargetDirection, vDesiredDirection), ScalarV(s_RadiusMultMin), ScalarV(s_RadiusMultMax) );
	ScalarV vMinRadius = ScalarV(CTaskVehicleGoToPlane::ComputeTurnRadiusForRoll(pPlane, in_Speed, in_MaxRoll * s_RollMult));
	ScalarV vExpandedRadius = vMinRadius * vExpandRadiusMult;

	Vec2V vApproachCircleOffsetRight = Vec2V(vDesiredDirection.GetY(), -vDesiredDirection.GetX());
	Vec2V vApproachCircleOffsetLeft = Negate(vApproachCircleOffsetRight);

	Vec2V vApproachCircleCenterRight = vTargetPos + vApproachCircleOffsetRight * vMinRadius;
	Vec2V vApproachCircleCenterLeft = vTargetPos + vApproachCircleOffsetLeft * vMinRadius;

	Vec2V vPlaneToRightCircleCenter = vApproachCircleCenterRight - vPlanePosition;
	Vec2V vPlaneToLeftCircleCenter = vApproachCircleCenterLeft - vPlanePosition;

	ScalarV vDistancePlaneToRightCircleCenter = Mag(vPlaneToRightCircleCenter);
	ScalarV vDistancePlaneToLeftCircleCenter = Mag(vPlaneToLeftCircleCenter);

	ScalarV vDotToLeftTangent = ScalarV(-2.0f); // negative 2.0 means that it is always less than a valid dot value
	ScalarV vDotToRightTangent = ScalarV(-2.0f);
	ScalarV vMinDistanceToTangent = ScalarV(s_MinDistanceToTangent);

	Vec2V vPlaneLeftCircleTangent;
	Vec2V vPlaneRightCircleTangent;

	if ( (vDistancePlaneToLeftCircleCenter >= vMinRadius).Getb() )
	{
		ScalarV hypotenuse = vDistancePlaneToLeftCircleCenter;
		ScalarV adjacent = vExpandedRadius;
		ScalarV vAngle = ScalarV(acos(adjacent.Getf()/hypotenuse.Getf()));

		Vec2V vDirCircleCenterToPlane = Normalize(Negate(vPlaneToLeftCircleCenter));
		Vec2V vDirection = Rotate(vDirCircleCenterToPlane, vAngle);

		vPlaneLeftCircleTangent = vApproachCircleCenterLeft + vDirection * vExpandedRadius;	

		ScalarV vDistanceToTangent = Mag(vPlaneLeftCircleTangent - vPlanePosition);

		if ( ( vDistanceToTangent >= vMinDistanceToTangent).Getb() )
		{
			Vec2V vDirectionToTangent = NormalizeSafe(vPlaneLeftCircleTangent - vPlanePosition, vPlaneForward);
			vDotToLeftTangent = Dot(vDirectionToTangent, vPlaneForward);
		}
	}
	
	if ( (vDistancePlaneToRightCircleCenter >= vMinRadius).Getb() )
	{
		ScalarV hypotenuse = vDistancePlaneToRightCircleCenter;
		ScalarV adjacent = vExpandedRadius;
		ScalarV vAngle = ScalarV(acos(adjacent.Getf()/hypotenuse.Getf()));

		Vec2V vDirCircleCenterToPlane = Normalize(Negate(vPlaneToRightCircleCenter));
		Vec2V vDirection = Rotate(vDirCircleCenterToPlane, Negate(vAngle));

		vPlaneRightCircleTangent = vApproachCircleCenterRight + vDirection * vExpandedRadius;	

		ScalarV vDistanceToTangent = Mag(vPlaneRightCircleTangent - vPlanePosition);

		if ( ( vDistanceToTangent >= vMinDistanceToTangent).Getb() )
		{
			Vec2V vDirectionToTangent = NormalizeSafe(vPlaneRightCircleTangent - vPlanePosition, vPlaneForward);
			vDotToRightTangent = Dot(vDirectionToTangent, vPlaneForward);
		}
	}
	
	if ( ( vDotToRightTangent > vDotToLeftTangent ).Getb() )
	{
		o_TangentPos = VEC3V_TO_VECTOR3(Clamp( Vec3V(vPlaneRightCircleTangent.GetXf(), vPlaneRightCircleTangent.GetYf(),  in_vTargetPos.z), Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX)));

	}
	else if ( ( vDotToLeftTangent > vDotToRightTangent ).Getb() )
	{
		o_TangentPos = VEC3V_TO_VECTOR3(Clamp( Vec3V(vPlaneLeftCircleTangent.GetXf(), vPlaneLeftCircleTangent.GetYf(),  in_vTargetPos.z), Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX)));

	}

#if DEBUG_DRAW
	if (in_bDebugDraw)
	{
		Vector3 leftCircle(vApproachCircleCenterLeft.GetXf(), vApproachCircleCenterLeft.GetYf(), in_vTargetPos.z);
		grcDebugDraw::Circle(leftCircle, vMinRadius.Getf(), Color_green, XAXIS, YAXIS );

		Vector3 rightCircle(vApproachCircleCenterRight.GetXf(), vApproachCircleCenterRight.GetYf(), in_vTargetPos.z);
		grcDebugDraw::Circle(rightCircle, vMinRadius.Getf(), Color_red, XAXIS, YAXIS );


		Vec3V vTargetPos3d = Vec3V(vTargetPos.GetXf(), vTargetPos.GetYf(), in_vTargetPos.z);
		Vec3V vDirection3d = Vec3V(vDesiredDirection.GetXf(), vDesiredDirection.GetYf(), 0.0f);
		grcDebugDraw::Sphere(o_TangentPos, 1.0f, Color_purple, true );
		grcDebugDraw::Arrow(vTargetPos3d,  vTargetPos3d + vDirection3d * ScalarV(V_TWO), 1.0f, Color_green);

	}
#endif

	return true;
}

//
// sample the heightmap and return the largest slope amongst the samples
// 
float CTaskVehicleGoToPlane::ComputeSlopeToAvoidTerrain(CPlane *pPlane, float in_Speed, float turnRadius, float in_HeightAboveTerrain, bool in_bMaxHeight)
{
	Vector3 vPlanePos = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());
	Vector3 vPlaneRight = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetA());
	vPlaneRight.z = 0;
		vPlaneRight.Normalize();

	Vector3 vCircleCenter = vPlanePos + ( vPlaneRight * -turnRadius);
	Vector3 vCircleCenterToPlane = vPlanePos - vCircleCenter;
	float angleToPlane = Atan2f(vCircleCenterToPlane.y, vCircleCenterToPlane.x) * RtoD;

	float maxSlope = -FLT_MAX;

	for (int i = 0; i < sm_Tunables.m_numFutureSamples; i++)
	{
		float t = (i+1) * sm_Tunables.m_futureSampleTime;

		// using arclength equation to get angle
		// arclength = 2 * PI * radius * (angle/360.0f)
		float arclengh = in_Speed * t;
		float relAngle = fabsf(turnRadius) > FLT_MIN ? (360.0f * arclengh) / ( 2 * PI * turnRadius) : 0.0f;

		// convert local angle to world
		float angle = fmod(relAngle + angleToPlane, 360.0f);

		// compute offset given the angle, radius and center of the circle
		Vector3 vOffsetWorld = Vector3::ZeroType;
		vOffsetWorld.x = rage::Cosf(angle * DtoR) * fabsf(turnRadius);		
		vOffsetWorld.y = rage::Sinf(angle * DtoR) * fabsf(turnRadius);
		Vector3 position = vCircleCenter + vOffsetWorld;

		// sample heightmap
		if ( in_bMaxHeight )
		{
			position.z = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(position.x, position.y);
		}
		else
		{
			position.z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(position.x, position.y);
		}
		
		float offsetAboveTerrainToAvoidCollisions = Min(in_HeightAboveTerrain, 10.0f);
		position.z += offsetAboveTerrainToAvoidCollisions;

#if DEBUG_DRAW
		static bool s_DrawHeightMapTests = false;
		if ( s_DrawHeightMapTests )	grcDebugDraw::Sphere(position, 1.5f, Color_red, true, 1);
#endif
		
		// compute slope and replace max slope if new slope is greater
		float deltaheight = position.z - vPlanePos.z;
		float slope = deltaheight / arclengh;
		if ( slope > maxSlope )
		{
			maxSlope = slope;
		}

		float MaxDesiredSlopeTValue = 1.0f - Clamp(-deltaheight / 40.0f, 0.0f, 1.0f);

		// recompute for desired height above terrain
		position.z += in_HeightAboveTerrain - offsetAboveTerrainToAvoidCollisions;
		deltaheight = position.z - vPlanePos.z;
		static float s_MaxDesiredSlope = 0.5f;
		 
		float fMaxDesiredSlope = Lerp(MaxDesiredSlopeTValue, 0.0f, s_MaxDesiredSlope);
		slope = Min(deltaheight / arclengh, fMaxDesiredSlope);
		if ( slope > maxSlope )
		{
			maxSlope = slope;
		}
	}
	return maxSlope;
}

//
// This is going to compute the roll of the aircraft
// 
void CTaskVehicleGoToPlane::Plane_SteerXYUsingRoll(CPlane *pPlane, const Vector3& vTargetPos)
{
	static float sm_MinPitchToAllowRoll = -60.0f;
	static float sm_MaxPitchToAllowRoll =  60.0f;
	static float sm_MaxTurnRadius = 1000000.0f; // essentially a straight line in short distance

	float fDesiredRoll = 0;
	m_fDesiredTurnRadius = sm_MaxTurnRadius;

	const Vector3 vPlanePos = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());

	// only allowing rolling to turn if we're mostly flying level
	// don't allow downward pitch while rolling
	if ( m_fDesiredPitch > sm_MinPitchToAllowRoll && m_fDesiredPitch < sm_MaxPitchToAllowRoll )
	{
		float maxRoll = ComputeMaxRollForPitch(m_fDesiredPitch, GetMaxRoll());
		if ( maxRoll > 0.0f)
		{
			float speed = pPlane->GetAiXYSpeed();
			//ignore damaged when computing radius otherwise damaged planes just fly in straight lines forever
			float minRadius = ComputeTurnRadiusForRoll(pPlane, speed, maxRoll, true);
			float fDesiredTurnRadius = ComputeTurnRadiusForTarget(pPlane, vTargetPos, m_fMinDistanceToTargetForRollComputation);
			fDesiredTurnRadius = Clamp(fDesiredTurnRadius, -sm_MaxTurnRadius, sm_MaxTurnRadius);

			// only roll the aircraft to turn if the turn radius is our min turn
			// radius or larger.  
			if ( fabsf(fDesiredTurnRadius) >= minRadius - m_fMinTurnRadiusPadding )
			{				
				const Vector3 vPlaneFwd = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB());
				Vector3 vPlaneToTarget = vTargetPos-vPlanePos;
				Vector3 vDirPlaneToTarget = vPlaneToTarget;
				vDirPlaneToTarget.NormalizeSafe();

				static float s_MinAngleToTarget = 15.0f;
				static float s_MaxAngleToTarget = 45.0f;
				
				float angleToTarget = 0.0f; 

				// guard this angle z check also
				float mag2 = (vPlaneFwd.x * vPlaneFwd.x + vPlaneFwd.y * vPlaneFwd.y) * (vDirPlaneToTarget.x * vDirPlaneToTarget.x + vDirPlaneToTarget.y * vDirPlaneToTarget.y);
				if ( mag2 > 0 )
				{
					angleToTarget = fabsf(vPlaneFwd.AngleZ(vDirPlaneToTarget) * RtoD);
				}
				
				float fTurnTValue = Clamp( (angleToTarget - s_MinAngleToTarget) / (s_MaxAngleToTarget - s_MinAngleToTarget), 0.0f, 1.0f);

				m_fDesiredTurnRadius = Lerp(fTurnTValue, fDesiredTurnRadius, Sign(fDesiredTurnRadius) * minRadius );

				fDesiredRoll = Clamp(ComputeRollForTurnRadius(pPlane, speed, m_fDesiredTurnRadius), -GetMaxRoll(), GetMaxRoll());
			}
		}
	}

	const float fCurrentRoll = pPlane->GetTransform().GetRoll() * RtoD;
	if( Abs(fCurrentRoll) >= 20.0f)
	{
		CEntityScannerIterator entityList = pPlane->GetIntelligence()->GetVehicleScanner().GetIterator();
		for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
		{
			Assert(pEntity->GetIsTypeVehicle());
			const CVehicle* pVehicle = static_cast<const CVehicle *>(pEntity);

			if(pVehicle->InheritsFromPlane())
			{
				static float s_fMinAvoidDistance = 400.0f;
				const Vector3 otherPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
				Vector3 usToThem = otherPos - vPlanePos;
				float distance2 = usToThem.Mag2();
				if( distance2 < s_fMinAvoidDistance)
				{				
					const Vector3 ourUp = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetUp());
					usToThem.Normalize();
					float dotUp = ourUp.Dot(usToThem);
					if( dotUp >= 0.0f)
					{
						//flatten out our roll if we are the outside most of the two planes
						float change = Lerp(distance2 / s_fMinAvoidDistance, 5.0f,1.0f);
						fDesiredRoll = fCurrentRoll - Sign(fCurrentRoll) * change;
						break;
					}
				}
			}
		}
	}
	
	// use PID controller to smoothly achieve target roll 
	const float fDeltaRoll = fDesiredRoll - fCurrentRoll;
	const float fRoll = pPlane->GetPlaneIntelligence()->GetRollController().Update(fDeltaRoll);
	const float fClampedRoll = Clamp(fRoll, -sm_Tunables.m_maxRoll, sm_Tunables.m_maxRoll);
	static float s_HackTune = -1.0f;
	pPlane->SetRollControl(fClampedRoll * s_HackTune);
	pPlane->SetYawControl(0.0f); 
}

void CTaskVehicleGoToPlane::Plane_SteerHeightZ(CPlane *pPlane, const Vector3& targetPos, bool bEnableHeightmapAvoidance)
{
	// compute our slope to the target
	Vector3 vPlanePos = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());
	Vector3 vToTarget = targetPos - vPlanePos;
	float xyDist = vToTarget.XYMag();
	float desiredSlope = 0;
	if ( xyDist > 0 && fabsf(vToTarget.z) > 0 )
	{
		static float s_MinSlopeRatio = 10.0f; // 
		float fMinDistance = s_MinSlopeRatio * fabsf(vToTarget.z);
		desiredSlope = vToTarget.z / Min(xyDist, fMinDistance);
	}

	if ( bEnableHeightmapAvoidance )
	{
		// 50% current 50% desired turn radius (m_fDesiredTurnRadius is only ever positive)
		float turnRadius = Lerp(0.5f, m_fDesiredTurnRadius, Sign(m_fDesiredTurnRadius) * m_fCurrentTurnRadius);
	
		// compute our slope to the terrain, unless we don't want to avoid the ground
		float terrainSlope = ComputeSlopeToAvoidTerrain(pPlane, GetCruiseSpeed(), turnRadius, static_cast<float>(m_iMinHeightAboveTerrain), m_bAvoidTerrainMaxZThisFrame);

		// take the larger of the two
		desiredSlope = Max(desiredSlope, terrainSlope);

		m_bAvoidTerrainMaxZThisFrame = true;
	}

	float currentPitch = pPlane->GetTransform().GetPitch() * RtoD;
	const float fCurrentRoll = pPlane->GetTransform().GetRoll() * RtoD;
	if( Abs(fCurrentRoll) < 20.0f)
	{
		CEntityScannerIterator entityList = pPlane->GetIntelligence()->GetVehicleScanner().GetIterator();
		for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
		{
			Assert(pEntity->GetIsTypeVehicle());
			const CVehicle* pVehicle = static_cast<const CVehicle *>(pEntity);
			if(pVehicle->InheritsFromPlane())
			{
				static float s_fMinAvoidDistance = 400.0f;
				const Vector3 otherPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
				float distance2 = (otherPos - vPlanePos).Mag2();
				if( distance2 < s_fMinAvoidDistance)
				{				
					if( otherPos.z < vPlanePos.z)
					{
						desiredSlope = Lerp( distance2 / s_fMinAvoidDistance, 0.2f, 0.05f);
						break;
					}
					//else we'll let other vehicle move up, as we don't want to risk pitching down if too close to ground
				}
			}
		}
	}

	if (pPlane->GetIntelligence()->m_bIgnorePlanesSmallPitchChange && abs(desiredSlope < 0.5))
	{
		desiredSlope = 0.0f;
	}

	// convert slope into pitch
	m_fDesiredPitch = atan(desiredSlope) * RtoD;

	// use PID controller to smoothly achieve target pitch 
	
	float deltaPitch = m_fDesiredPitch - currentPitch;
	const float fPitch = pPlane->GetPlaneIntelligence()->GetPitchController().Update(deltaPitch);
	const float fClampedPitch = Clamp(fPitch, -sm_Tunables.m_maxPitch, sm_Tunables.m_maxPitch);
	static float s_HackTune = 1.0f;
	pPlane->SetPitchControl(fClampedPitch * s_HackTune);
}

void CTaskVehicleGoToPlane::Plane_ModifyTargetForArrivalTolerance(Vector3 &io_targetPos, const CPlane *pPlane ) const
{
	Vector3 vForward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetForward());
	Vector3 vPosition = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition()); // CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(GetVehicle(), false));

	if ( m_bUseDesiredOrientation )
	{
		Vector3 vDirection(1.0f, 0.0f, 0.0f);
		vDirection.RotateZ(m_fDesiredOrientation);
		vForward = vDirection;
	}
	
	Vector3 vToTarget = io_targetPos - vPosition;
	float distance2 = vToTarget.Mag2();

	static float s_Tune = 6.0f;
	float fMaxDistanceToEnableTolerance =  m_Params.m_fTargetArriveDist * s_Tune;

	if ( distance2 < square(fMaxDistanceToEnableTolerance))
	{
		Vector3 vFuturePosition = vPosition + vForward * distance2;
		Vector3 vToFuturePosition = vFuturePosition - vPosition;

		// find the closest point between us and our future position and the target
		// project a ray from the target to this closest point some arbitrary amount
		// and this new point will be our modified target
		const float vTValue = geomTValues::FindTValueOpenSegToPoint(vPosition, vToFuturePosition, io_targetPos);
		if ( vTValue > 0.0f )
		{
			Vector3 vClosestPoint = vPosition + (vToFuturePosition * vTValue);
			Vector3 vCenterToClosestDir = vClosestPoint - io_targetPos;
			float distanceCenterToClosest2 = vCenterToClosestDir.Mag2();
			if ( distanceCenterToClosest2 > FLT_EPSILON )
			{
				float distanceCenterToClosest = sqrt(distanceCenterToClosest2);
				vCenterToClosestDir *= 1/distanceCenterToClosest;

				static float s_PullBackToTargetFactor = 0.5f;
				static float s_ArrivalDistanceToMaxToleranceMap = 0.5f;
				float MaxTolerance = m_Params.m_fTargetArriveDist * s_ArrivalDistanceToMaxToleranceMap;
				float Radius = Min(distanceCenterToClosest * s_PullBackToTargetFactor, MaxTolerance); 
			
				io_targetPos = io_targetPos + vCenterToClosestDir * Radius; 
			}
		}
	}
}

void CTaskVehicleGoToPlane::Plane_ModifyTargetForOrientation(Vector3 &io_targetPos, const CPlane *pPlane ) const
{
	if ( m_bUseDesiredOrientation )
	{
		Vector3 vDirection(1.0f, 0.0f, 0.0f);
		vDirection.RotateZ(m_fDesiredOrientation);
		float maxRoll = ComputeMaxRollForPitch(m_fDesiredPitch, GetMaxRoll());
		ComputeApproachTangentPosition(io_targetPos, pPlane, io_targetPos, vDirection, GetCruiseSpeed(), maxRoll, false, false);
	}
}

void CTaskVehicleGoToPlane::Plane_SteerXYUsingYawAndWheels(CPlane *pPlane, const Vector3& vTargetPos)
{
	Vector3 vForward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetForward());
	Vector3 vPosition = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());
	Vector3 vToTarget = vTargetPos - vPosition;

	static dev_float s_MinMag2ToChangeOrientation = 0.001f;
	static dev_float s_OrientationDeltaToTurnMax = 10.0f;
	const float fDesiredVelocityXYMag2 = GetCruiseSpeed();
	const float fCurrentOrientation = fwAngle::GetATanOfXY(vForward.x, vForward.y);
	const float fDesiredOrientation = fDesiredVelocityXYMag2 > s_MinMag2ToChangeOrientation ?	fwAngle::GetATanOfXY(vToTarget.x, vToTarget.y) : fCurrentOrientation;
	const float fOrientationDelta = fwAngle::LimitRadianAngleSafe(fDesiredOrientation - fCurrentOrientation);
	const float fOrientationDeltaScaled = fOrientationDelta / ( s_OrientationDeltaToTurnMax * DtoR );

	const float fYawControl = -pPlane->GetPlaneIntelligence()->GetYawController().Update(fOrientationDeltaScaled);
	const float fYawControlClamped = Clamp(fYawControl, -5.0f, 5.0f);
	pPlane->SetYawControl(fYawControlClamped);
	pPlane->SetSteerAngle(fOrientationDelta);
	
	const float fDesiredRoll = 0.0f;
	const float fCurrentRoll = pPlane->GetTransform().GetRoll() * RtoD;
	const float fDeltaRoll = fDesiredRoll - fCurrentRoll;
	const float fRoll = pPlane->GetPlaneIntelligence()->GetRollController().Update(fDeltaRoll);
	const float fClampedRoll = Clamp(fRoll, -sm_Tunables.m_maxRoll, sm_Tunables.m_maxRoll);
	static float s_HackTune = -1.0f;
	pPlane->SetRollControl(fClampedRoll * s_HackTune);
}

void CTaskVehicleGoToPlane::Plane_SteerToTarget(CPlane *pPlane, const Vector3 &targetPos)
{
	// configure our PID controllers
	pPlane->GetPlaneIntelligence()->GetYawController().Setup(sm_Tunables.m_yawKp, sm_Tunables.m_yawKi, sm_Tunables.m_yawKd);
	pPlane->GetPlaneIntelligence()->GetPitchController().Setup(sm_Tunables.m_pitchKp, sm_Tunables.m_pitchKi, sm_Tunables.m_pitchKd);
	pPlane->GetPlaneIntelligence()->GetRollController().Setup(sm_Tunables.m_rollKp, sm_Tunables.m_rollKi, sm_Tunables.m_rollKd);
	pPlane->GetPlaneIntelligence()->GetThrottleController().Setup(sm_Tunables.m_throttleKp, sm_Tunables.m_throttleKi, sm_Tunables.m_throttleKd);

	//Vector3 vForward = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetForward());
	Vector3 vPosition = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());
	Vector3 vToTarget = targetPos - vPosition;

	// compute current turn radius
	m_fCurrentTurnRadius = ComputeTurnRadiusForRoll(pPlane, pPlane->GetAiXYSpeed(), Abs(pPlane->GetTransform().GetRoll()) * RtoD);

	bool bSteerUsingRoll = pPlane->IsInAir() && !m_bForceDriveOnGround;
	bool bEnableVirtualSpeed = pPlane->IsInAir() || vToTarget.z > 5.0f;

	Vector3 vModifiedTarget = targetPos;
	Plane_ModifyTargetForArrivalTolerance(vModifiedTarget, pPlane);

	m_flyingAvoidanceHelper.SteerAvoidCollisions(vModifiedTarget, vToTarget, vModifiedTarget);

	Plane_SteerHeightZ(pPlane, vModifiedTarget, !pPlane->m_nVehicleFlags.bDisableHeightMapAvoidance && bSteerUsingRoll );
	if ( bSteerUsingRoll )
	{
		Plane_ModifyTargetForOrientation(vModifiedTarget, pPlane);
		Plane_SteerXYUsingRoll(pPlane, vModifiedTarget);
	}
	else
	{
		Plane_SteerXYUsingYawAndWheels(pPlane, vModifiedTarget);
	}

	Plane_SteerToSpeed(pPlane, GetCruiseSpeed(), bEnableVirtualSpeed);
}

void CTaskVehicleGoToPlane::Plane_SteerToSpeed(CPlane *pPlane, float fDesiredSpeed, bool bEnableVirtualSpeedControl)
{
	// better make sure we've got some handling data, otherwise we're screwed for flying
	CFlyingHandlingData* pFlyingHandling = pPlane->pHandling->GetFlyingHandlingData();
	if(pFlyingHandling)
	{
		// compute our current airspeed
		const Vector3 vPlaneForward = VEC3V_TO_VECTOR3(pPlane->GetVehicleForwardDirection());
		Vector3 vecAirSpeed(0.0f, 0.0f, 0.0f);
		Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
		WIND.GetLocalVelocity(pPlane->GetTransform().GetPosition(), RC_VEC3V(vecWindSpeed), false, false);
		vecAirSpeed -= pFlyingHandling->m_fWindMult*vecWindSpeed;		
		vecAirSpeed += pPlane->GetVelocity();
		const float fAirSpeed = vecAirSpeed.Dot(vPlaneForward);
		const float fDeltaAirSpeed = fDesiredSpeed - fAirSpeed;

		// use PID controller to smoothly achieve target airspeed
		const float fThrottle = pPlane->GetPlaneIntelligence()->GetThrottleController().Update(fDeltaAirSpeed);

		static float sLargeMaxOnGroundToGetPlanesToMove = 10.0f;
		const float fMaxThrottle = pPlane->IsInAir() ? m_fMaxThrottle : sLargeMaxOnGroundToGetPlanesToMove;

		const float fClampedThrottle = Clamp(fThrottle, -fMaxThrottle, fMaxThrottle);

		float fFinalThrottle = fClampedThrottle;
		if ( m_fDesiredPitch > 0 && vPlaneForward.z	>= 0.0f )
		{ 
			// increase throttle when trying to go up
			// and pitched up
			fFinalThrottle += m_fDesiredPitch / 45.0f;
		}
			
		pPlane->SetThrottleControl(fFinalThrottle);
		
		static float s_BrakeAirSpeedDelta = -10.0f;
		static float s_BrakeDeltaAirSpeedtoBreak = 10.0f;
		float fFinalAirBrake = Clamp( ( fDeltaAirSpeed + s_BrakeDeltaAirSpeedtoBreak ) / s_BrakeAirSpeedDelta, 0.0f, 10.0f);
		pPlane->SetAirBrakeControl( fFinalAirBrake  );

		//
		// compute the values for the Virtual speed control
		// this is basically what value times desired speed
		// will give us our minspeed
		//
		if ( bEnableVirtualSpeedControl )
		{
			if ( m_VirtualSpeedControlMode == VirtualSpeedControlMode_On )
			{
				float minSpeed = sqrt( 1.0f / pFlyingHandling->m_fFormLiftMult );
				float speedControl = Max(minSpeed / fDesiredSpeed, 1.0f);
				pPlane->SetVirtualSpeedControl(speedControl);
			}
			else if (m_VirtualSpeedControlMode == VirtualSpeedControlMode_SlowDescent )
			{	
				if ( vecAirSpeed.z <= -m_fVirtualSpeedDescentRate )
				{
					float minSpeed = sqrt( 1.0f / pFlyingHandling->m_fFormLiftMult );
					float speedControl = Max(minSpeed / fDesiredSpeed, 1.0f);			
					pPlane->SetVirtualSpeedControl(speedControl);	
				}
			}	
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TestForFutureWorldCollisions
// PURPOSE :  Check for possible collisions against the ground.
/////////////////////////////////////////////////////////////////////////////////
float CTaskVehicleGoToPlane::Vehicle_TestForFutureWorldCollisions(const CPhysical* pEntity, const Vector3& forwardDirFlat, const float targetDeltaAngle)
{
	float estimatedGroundHeight = 0.0f;// Assume water level of 0.0f globally

	if (!pEntity)
	{
		return estimatedGroundHeight;
	}

	TUNE_GROUP_FLOAT(HELI_AI, minAngleDegs, 30.0f, 0.0f, 90.0f, 0.1f);
	TUNE_GROUP_FLOAT(HELI_AI, maxAngleDegs, 60.0f, 0.0f, 90.0f, 0.1f);
	const float collsionTestDownwardAngle = ( DtoR * fwRandom::GetRandomNumberInRange(minAngleDegs, maxAngleDegs));
	const float collsionTestDirZ = -rage::Atan2f(collsionTestDownwardAngle, 1.0f);

	TUNE_GROUP_FLOAT(HELI_AI, minEndPointDist, 60.0f, 0.0f, 1000.0f, 1.0f);
	TUNE_GROUP_FLOAT(HELI_AI, maxEndPointDist, 60.0f, 0.0f, 1000.0f, 1.0f);
	const float endPointDist = fwRandom::GetRandomNumberInRange(60.0f, 60.0f);

	// test current positions movements.
	{
		//static float posPredictionTime = 1.0f;
		const Vector3 currentPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

		float forwardDirGroundTestHeight = 0.0f;// Assume water level of 0.0f globally
		{
			Vector3 collisionTestDir = Vector3(forwardDirFlat.x, forwardDirFlat.y, collsionTestDirZ);
			collisionTestDir.Normalize();
			const Vector3 vEnd = currentPos + (collisionTestDir * endPointDist);
			Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
			if(Vehicle_WorldCollsionTest(currentPos, vEnd, intersectionPoint))
			{
				forwardDirGroundTestHeight = intersectionPoint.z;
			}
		}
		estimatedGroundHeight = rage::Max(estimatedGroundHeight, forwardDirGroundTestHeight);

		float movementDirGroundTestHeight = 0.0f;// Assume water level of 0.0f globally
		{
			Vector3 collisionTestDir = Vector3(pEntity->GetVelocity().x, pEntity->GetVelocity().y, collsionTestDirZ);
			collisionTestDir.Normalize();
			const Vector3 vEnd = currentPos + (collisionTestDir * endPointDist);
			Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
			if(Vehicle_WorldCollsionTest(currentPos, vEnd, intersectionPoint))
			{
				movementDirGroundTestHeight = intersectionPoint.z;
			}
		}
		estimatedGroundHeight = rage::Max(estimatedGroundHeight, movementDirGroundTestHeight);

		float toTargetGroundTestHeight = 0.0f;// Assume water level of 0.0f globally
		{
			Vector3 collisionTestDir = Vector3(rage::Cosf(targetDeltaAngle), rage::Sinf(targetDeltaAngle), collsionTestDirZ);
			collisionTestDir.Normalize();
			const Vector3 vEnd = currentPos + (collisionTestDir * endPointDist);
			Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
			if(Vehicle_WorldCollsionTest(currentPos, vEnd, intersectionPoint))
			{
				toTargetGroundTestHeight = intersectionPoint.z;
			}
		}
		estimatedGroundHeight = rage::Max(estimatedGroundHeight, toTargetGroundTestHeight);
	}

	// Test future positions movements.
	{
		static float posPredictionTime = 1.0f;
		const Vector3 predictedPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) + (pEntity->GetVelocity() * posPredictionTime);

		float forwardDirGroundTestHeight = 0.0f;// Assume water level of 0.0f globally
		{
			Vector3 collisionTestDir = Vector3(forwardDirFlat.x, forwardDirFlat.y, collsionTestDirZ);
			collisionTestDir.Normalize();
			const Vector3 vEnd = predictedPos + (collisionTestDir * endPointDist);
			Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
			if(Vehicle_WorldCollsionTest(predictedPos, vEnd, intersectionPoint))
			{
				forwardDirGroundTestHeight = intersectionPoint.z;
			}
		}
		estimatedGroundHeight = rage::Max(estimatedGroundHeight, forwardDirGroundTestHeight);

		float movementDirGroundTestHeight = 0.0f;// Assume water level of 0.0f globally
		{
			Vector3 collisionTestDir = Vector3(pEntity->GetVelocity().x, pEntity->GetVelocity().y, collsionTestDirZ);
			collisionTestDir.Normalize();
			const Vector3 vEnd = predictedPos + (collisionTestDir * endPointDist);
			Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
			if(Vehicle_WorldCollsionTest(predictedPos, vEnd, intersectionPoint))
			{
				movementDirGroundTestHeight = intersectionPoint.z;
			}
		}
		estimatedGroundHeight = rage::Max(estimatedGroundHeight, movementDirGroundTestHeight);

		float toTargetGroundTestHeight = 0.0f;// Assume water level of 0.0f globally
		{
			Vector3 collisionTestDir = Vector3(rage::Cosf(targetDeltaAngle), rage::Sinf(targetDeltaAngle), collsionTestDirZ);
			collisionTestDir.Normalize();
			const Vector3 vEnd = predictedPos + (collisionTestDir * endPointDist);
			Vector3 intersectionPoint(0.0f, 0.0f, 0.0f);
			if(Vehicle_WorldCollsionTest(predictedPos, vEnd, intersectionPoint))
			{
				toTargetGroundTestHeight = intersectionPoint.z;
			}
		}
		estimatedGroundHeight = rage::Max(estimatedGroundHeight, toTargetGroundTestHeight);
	}

	return estimatedGroundHeight;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : WorldCollsionTest
// PURPOSE :  Test a line segment against the world.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleGoToPlane::Vehicle_WorldCollsionTest(const Vector3 &vStart, const Vector3 &vEnd, Vector3& intersectionPoint)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		intersectionPoint = probeResult[0].GetHitPosition();
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleGoToPlane::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_GoTo&&iState<=State_GoTo);
	static const char* aStateNames[] = 
	{
		"State_GoTo",
		//"State_Land",
	};

	return aStateNames[iState];
}

void CTaskVehicleGoToPlane::Debug() const
{
#if DEBUG_DRAW
	Vec3V vPlanePos = GetVehicle()->GetTransform().GetPosition();
	Vec3V vPlaneBonnetPos = CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(GetVehicle(), false);
	Vec3V vPlaneNosePos = static_cast<const CPlane*>(GetVehicle())->ComputeNosePosition();
	Vector3 vTargetPos;
	FindTargetCoors(GetVehicle(), vTargetPos);

	Vector3 vModifiedTarget = vTargetPos;
	Plane_ModifyTargetForArrivalTolerance(vModifiedTarget, static_cast<const CPlane*>(GetVehicle()));
	grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vModifiedTarget), 0.5f, Color_yellow, false);
	grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vTargetPos), VECTOR3_TO_VEC3V(vModifiedTarget), 1.0f, Color_yellow);

	grcDebugDraw::Arrow(vPlanePos, VECTOR3_TO_VEC3V(vTargetPos), 1.0f, Color_red2);
	grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vTargetPos), m_Params.m_fTargetArriveDist, Color_red2, false);	
	grcDebugDraw::Sphere(vPlaneBonnetPos, 0.5f, Color_brown, false);
	grcDebugDraw::Sphere(vPlaneNosePos, 0.5f, Color_pink, false);

	if ( m_bUseDesiredOrientation )
	{
		Vector3 vDirection(1.0f, 0.0f, 0.0f);
		vDirection.RotateZ(m_fDesiredOrientation);
		Vector3 vTarget(0.0f, 0.0f, 0.0f);
		float maxRoll = ComputeMaxRollForPitch(m_fDesiredPitch, GetMaxRoll());
		float radius = ComputeTurnRadiusForRoll(static_cast<const CPlane*>(GetVehicle()), GetCruiseSpeed(), maxRoll);
		ComputeApproachTangentPosition(vTarget, static_cast<const CPlane*>(GetVehicle()), vTargetPos, vDirection, GetCruiseSpeed(), maxRoll, false, true);

		Vec3V vPlaneRight = GetVehicle()->GetTransform().GetRight();
		Vec3V vPlaneCircleCenter = vPlanePos + ( vPlaneRight * ScalarV(Sign(-m_fDesiredTurnRadius) * radius));
		grcDebugDraw::Circle(VEC3V_TO_VECTOR3(vPlaneCircleCenter), radius, Color_yellow, XAXIS, YAXIS );
	}

	static bool s_DrawModifiedTarget = true;
	if ( s_DrawModifiedTarget )
	{
		Vector3 vModifiedTarget;
		CFlyingVehicleAvoidanceManager::SlideDestinationTarget(vModifiedTarget, RegdVeh(const_cast<CVehicle*>(GetVehicle())), vTargetPos, m_fFlyingVehicleAvoidanceScalar, false);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vTargetPos), VECTOR3_TO_VEC3V(vModifiedTarget), 1.0f, Color_black);
	}

	Vec3V vPlaneRight = GetVehicle()->GetTransform().GetA();
	vPlaneRight.SetZf(0);
	vPlaneRight = Normalize(vPlaneRight);
	Vec3V vPlaneCircleCenter = vPlanePos + ( vPlaneRight * ScalarV(-m_fCurrentTurnRadius));

	static bool s_DrawTurnRadii = false;
	if(s_DrawTurnRadii)
	{
		vPlaneCircleCenter = vPlanePos + ( vPlaneRight * ScalarV(-m_fDesiredTurnRadius));
		grcDebugDraw::Sphere(vPlaneCircleCenter, 1.0f, Color_blue);
		grcDebugDraw::Circle(VEC3V_TO_VECTOR3(vPlaneCircleCenter), m_fDesiredTurnRadius, Color_blue, XAXIS, YAXIS );

		vPlaneCircleCenter = vPlanePos + ( vPlaneRight * ScalarV(-m_fCurrentTurnRadius));
		grcDebugDraw::Sphere(vPlaneCircleCenter, 1.0f, Color_OrangeRed);
		grcDebugDraw::Circle(VEC3V_TO_VECTOR3(vPlaneCircleCenter), m_fCurrentTurnRadius, Color_OrangeRed, XAXIS, YAXIS );
	}

	static bool s_drawControlInput = true;
	if( s_drawControlInput )
	{
		CPlaneIntelligence* pPlaneIntelligence = static_cast<CPlaneIntelligence*>(GetVehicle()->GetIntelligence());
		CVehPIDController& pitchContr = pPlaneIntelligence->GetPitchController();
		CVehPIDController& rollContr = pPlaneIntelligence->GetRollController();
		CVehPIDController& yawContr = pPlaneIntelligence->GetYawController();
		CVehPIDController& throttleContr = pPlaneIntelligence->GetThrottleController();

		char controllerDebugText[128];

		sprintf(controllerDebugText, "Pitch: %.3f, %.3f, %.3f -> %.3f",
			pitchContr.GetPreviousInput(), pitchContr.GetIntegral(), pitchContr.GetPreviousDerivative(), pitchContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 20, controllerDebugText);

		sprintf(controllerDebugText, "Roll: %.3f, %.3f, %.3f -> %.3f",
			rollContr.GetPreviousInput(), rollContr.GetIntegral(), rollContr.GetPreviousDerivative(), rollContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 30, controllerDebugText);

		sprintf(controllerDebugText, "Yaw: %.3f, %.3f, %.3f -> %.3f",
			yawContr.GetPreviousInput(), yawContr.GetIntegral(), yawContr.GetPreviousDerivative(), yawContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 40, controllerDebugText);

		sprintf(controllerDebugText, "Throttle: %.3f, %.3f, %.3f -> %.3f",
			throttleContr.GetPreviousInput(), throttleContr.GetIntegral(), throttleContr.GetPreviousDerivative(), throttleContr.GetPreviousOutput());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 50, controllerDebugText);

		sprintf(controllerDebugText, "Speed: %.3f", GetVehicle()->GetVelocity().Mag());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 60, controllerDebugText);

		sprintf(controllerDebugText, "Turn Radius: %.3f, Roll %.3f", m_fDesiredTurnRadius, GetVehicle()->GetTransform().GetRoll()*RtoD);
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 70, controllerDebugText);
	}

	static bool s_DrawFutureLocations = false;
	if ( s_DrawFutureLocations )
	{
		Vec3V vCircleCenterToPlane = vPlanePos - vPlaneCircleCenter;
		float angleToPlane = Atan2f(vCircleCenterToPlane.GetYf(), vCircleCenterToPlane.GetXf()) * RtoD;

		for (int i = 0; i < sm_Tunables.m_numFutureSamples; i++)
		{
			float t = (i+1) * sm_Tunables.m_futureSampleTime;

			// using arclength equation to get angle
			// arclength = 2 * PI * radius * (angle/360.0f)
			float arclengh = GetCruiseSpeed() * t;
			float relAngle = (360.0f * arclengh) / ( 2 * PI * m_fDesiredTurnRadius);

			// convert local angle to world
			float angle = fmod(relAngle + angleToPlane, 360.0f);

			// compute offset given the angle, radius and center of the circle
			Vec3V vOffsetWorld(0.0f, 0.0f, 0.0f);
			vOffsetWorld.SetXf(rage::Cosf(angle * DtoR) * fabsf(m_fDesiredTurnRadius));		
			vOffsetWorld.SetYf(rage::Sinf(angle * DtoR) * fabsf(m_fDesiredTurnRadius));
			Vec3V position = vPlaneCircleCenter + vOffsetWorld;

			// sample heightmap
			position.SetZf(CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(position.GetXf(), position.GetYf()) + m_iMinHeightAboveTerrain);

			grcDebugDraw::Sphere(position, 1.0f, Color_green);
		}
	}

#endif //__BANK

	CTaskVehicleGoTo::Debug();
}
#endif

