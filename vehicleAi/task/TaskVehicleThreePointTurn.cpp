#include "TaskVehicleThreePointTurn.h"

#include "math/angmath.h"

// Game headers
#include "Vehicles/vehicle.h"
#include "VehicleAi/vehicleintelligence.h"
#include "debug/DebugScene.h"
#include "control/trafficLights.h"
#include "renderer/Water.h"
#include "scene/world/gameWorld.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "physics/physics.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////

dev_float CTaskVehicleThreePointTurn::s_fMinFacingAngleRadians = 90.0f * DtoR;
dev_float CTaskVehicleThreePointTurn::s_fMinFacingAngleRadiansSmallRoad = 75.0f * DtoR;

CTaskVehicleThreePointTurn::CTaskVehicleThreePointTurn(const sVehicleMissionParams& params, const bool bStartedBecauseUTurn) :
CTaskVehicleMissionBase(params)
, m_fMaxThrottleFromTraction(1.0f)
, m_bStartedBecauseOfUTurn(bStartedBecauseUTurn)
, m_vPositionWhenlastChangedDirection(Vector3::ZeroType)
{
	m_eThreePointTurnState = TURN_NONE;
	m_iBlind3PointTurnIterations = 0;
	m_LastTime3PointTurnChangedDirection = 0;

	m_fPreviousForwardSpeed = 0.0f;

	m_wasOnRoad[0] = m_wasOnRoad[1] = false;
	m_relaxPavementCheckCount = 0;
	m_iChangeDirectionsCount = 0;
	m_LastTimeRelaxedPavementCheckCount = 0;

	Assertf(m_Params.GetTargetEntity().GetEntity() == NULL, "Entity Target being passed down to a low-level drive task that requires a position.");
	Assertf(m_Params.GetTargetPosition().Dist2(ORIGIN) > SMALL_FLOAT, "Low-level drive task being told to drive to the origin. You probably don't want this.");
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN);
}


/////////////////////////////////////////////////////////////////////////////

CTaskVehicleThreePointTurn::~CTaskVehicleThreePointTurn()
{

}


/////////////////////////////////////////////////////////////////////////////

aiTask* CTaskVehicleThreePointTurn::Copy() const 
{
	return rage_new CTaskVehicleThreePointTurn(m_Params, m_bStartedBecauseOfUTurn);
}

/////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleThreePointTurn::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if(!pVehicle)
	{
		return FSM_Quit;
	}

	pVehicle->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = true;
	pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing = false;
	pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = false;
	pVehicle->m_nVehicleFlags.bLodForceUpdateThisTimeslice = true;

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleThreePointTurn::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// State_ThreePointTurn
		FSM_State(State_ThreePointTurn)
			FSM_OnEnter
				ThreePointTurn_OnEnter(pVehicle);
			FSM_OnUpdate
				return ThreePointTurn_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void	CTaskVehicleThreePointTurn::ThreePointTurn_OnEnter			(CVehicle* pVehicle)
{
	// We need this for next frames getting stuck calculations
	m_fPreviousForwardSpeed = DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()), pVehicle->GetAiVelocity());

	m_vPositionWhenlastChangedDirection = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());

	pVehicle->SwitchEngineOn();
}


/////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleThreePointTurn::ThreePointTurn_OnUpdate				(CVehicle* pVehicle)
{	
	// Remove any cheat increase in power as it makes this task unstable:
	pVehicle->SetCheatPowerIncrease(1.0f);

	const Vector3 vehDriveDir		(VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, IsDrivingFlagSet(DF_DriveInReverse))));
	float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
	vehDriveOrientation = fwAngle::LimitRadianAngle(vehDriveOrientation);

	// Work out the proper steering angle.
	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	float dirToTargetOrientation = fwAngle::GetATanOfXY(m_Params.GetTargetPosition().x - vVehiclePosition.x, m_Params.GetTargetPosition().y - vVehiclePosition.y);
	dirToTargetOrientation = fwAngle::LimitRadianAngle(dirToTargetOrientation);

	//CVehicle::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(vVehiclePosition), VECTOR3_TO_VEC3V(vVehiclePosition + (vehDriveDir * 3.0f)), 0.5f, Color_DarkOrange);
	//CVehicle::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vVehiclePosition), 0.2f, Color_aquamarine);
	//CVehicle::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(m_Params.GetTargetPosition()), 0.2f, Color_aquamarine);
	/////////////////////////

	float desiredSteerAngle = SubtractAngleShorter(dirToTargetOrientation, vehDriveOrientation);
	desiredSteerAngle = fwAngle::LimitRadianAngle(desiredSteerAngle);
	
	float pDesiredSpeed = GetCruiseSpeed();

	const bool bOnSmallRoad = pVehicle->GetIntelligence()->IsOnSmallRoad();

	if(!ThreePointTurn(pVehicle, &dirToTargetOrientation, &vehDriveOrientation, &desiredSteerAngle, &pDesiredSpeed, bOnSmallRoad))
	{
		SetState(State_Exit);//done with three point turn.
	}
	
	AdjustControls(pVehicle, &pVehicle->m_vehControls, desiredSteerAngle, pDesiredSpeed);

	// The values we found may be completely different from the ones last frame.
	// The following function smooths out the values and in some cases clips them.
	const float fCurrentVelocitySqr = pVehicle->GetVelocity().Mag2();
	CTaskVehicleGoToPointAutomobile::HumaniseCarControlInput( pVehicle, &pVehicle->m_vehControls,  pVehicle->GetIntelligence()->GetHumaniseControls(), (fCurrentVelocitySqr <(5.0f*5.0f)), GetTimeStep(), pDesiredSpeed);

	// We need this for next frames getting stuck calculations
	m_fPreviousForwardSpeed = DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()), pVehicle->GetAiVelocity());

	return FSM_Continue;
}





// Stuff that looks like it's common to both 
// 1 - CTaskVehicleThreePointTurn::ThreePointTurn()
// 2 - CTaskVehicleGoToPointWithAvoidanceAutomobile::ShouldDoThreePointTurn()
bool	CTaskVehicleThreePointTurn::ShouldDoThreePointTurn(CVehicle* pVehicle, float* pDirToTargetOrientation, float* pVehDriveOrientation, bool /*b_StopAtLights*/, bool &isStuck, float &desiredAngleDiff, float &absDesiredAngleDiff, u32 &impatienceTimer, const bool bOnSmallRoad, const bool bHasUTurn )
{
	//TUNE_GROUP_BOOL(	VEH_AI_3_POINT_TURNS, threePointTurnInit_alwaysDoImpatienceCheck, true);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_minSpeed, 1.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_minSpeedFlatTires, 0.25f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_maxSpeedForUTurn, 10.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnInit_impatienceTimeDefault, 3000, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnInit_impatienceTimeForBoats, 2000, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnInit_impatienceTimeForPlanes, 10000, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnInit_impatienceTimeForNonBoats, 2000, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnInit_impatienceTimeForTrailers, 3000, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnInit_impatienceTimeForCombat, 1000, 0, 100000, 10);

	//just don't even try to do one of these things if the target is non-finite
	if (!FPIsFinite(*pDirToTargetOrientation))
	{
		return false;
	}

	desiredAngleDiff = *pDirToTargetOrientation - *pVehDriveOrientation;
	desiredAngleDiff = fwAngle::LimitRadianAngle(desiredAngleDiff);
	absDesiredAngleDiff = rage::Abs(desiredAngleDiff);

	//B* 2160704 - don't quite trust AiVelocity when just migrated
	float moveSpeedXY = rage::Max(pVehicle->GetAiVelocity().XYMag(), pVehicle->GetVelocity().XYMag());
	float minSpeedThreePointTurn = (pVehicle->GetNumFlatTires() > 0) ? threePointTurnInit_minSpeedFlatTires : threePointTurnInit_minSpeed;
	if(((moveSpeedXY > minSpeedThreePointTurn) && !bHasUTurn) ||
		((moveSpeedXY > threePointTurnInit_maxSpeedForUTurn) && bHasUTurn))
	{
		// Going too fast to start a 3 point turn
		pVehicle->GetIntelligence()->MillisecondsNotMoving = 0;
		return false;
	}
	isStuck = false;
	impatienceTimer = threePointTurnInit_impatienceTimeDefault;

// 	if(b_StopAtLights && !threePointTurnInit_alwaysDoImpatienceCheck)
// 	{
// 		//empty
// 	}
/* 	else*/ 
	if(pVehicle->GetIntelligence()->MillisecondsNotMoving > 0)
	{
		isStuck = true;
		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			// Boats need to be given more time to accelerate.
			impatienceTimer = threePointTurnInit_impatienceTimeForBoats;
		}
		else if (pVehicle->InheritsFromPlane() )
		{
			impatienceTimer = threePointTurnInit_impatienceTimeForPlanes;
		}
		else if (pVehicle->HasTrailer() || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG))
		{
			impatienceTimer = threePointTurnInit_impatienceTimeForTrailers;
		}
		else
		{
			const CPed* pDriver = pVehicle->GetDriver();
			if(pDriver && pDriver->GetPedResetFlag(CPED_RESET_FLAG_IsInCombat))
			{
				impatienceTimer = threePointTurnInit_impatienceTimeForCombat;
			}
			else
			{
				impatienceTimer = threePointTurnInit_impatienceTimeForNonBoats;
			}
		}

		if (pVehicle->GetIntelligence()->GetImpatienceTimerOverride() >= 0)
		{
			impatienceTimer = pVehicle->GetIntelligence()->GetImpatienceTimerOverride();
		}
	}

	if (isStuck && pVehicle->GetIntelligence()->MillisecondsNotMoving >= impatienceTimer)
	{
		return true;
	}

	//boats don't try and three point turn for going to a point around them--only for getting stuck
	if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		return false;
	}

	// Start a 3 point turn if we are facing too far from our target and not really moving.
	// If we're in some vehicle with super high turning, don't let this clamp us
	const float fTempFacingAngleRads = !bOnSmallRoad ? s_fMinFacingAngleRadians : s_fMinFacingAngleRadiansSmallRoad;
	const float fMinFacingAngleRads = rage::Max(fTempFacingAngleRads, pVehicle->pHandling->m_fSteeringLock);
	if((absDesiredAngleDiff <= fMinFacingAngleRads) || 
		((pVehicle->GetAiVelocity().Mag() >= minSpeedThreePointTurn) && !bHasUTurn) || 
		((moveSpeedXY > threePointTurnInit_maxSpeedForUTurn) && bHasUTurn))
		return false;
	else
		return true;
}


void CTaskVehicleThreePointTurn::UpdateDesiredSpeedAndSteerAngle(const float fForwardMoveSpeed, float* pDesiredSpeed, float* pDesiredSteerAngle, const bool bIgnoreSpeed) const
{
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnPerfom_maxCruiseSpeedSpeed, 8.0f, 0.0f, 100.0f, 1.0f);

	// Set the actual values to control the car.
	switch(m_eThreePointTurnState)
	{
	case TURN_COUNTERCLOCKWISE_GOING_FORWARD:
		*pDesiredSpeed = rage::Min(threePointTurnPerfom_maxCruiseSpeedSpeed, (*pDesiredSpeed));
		if(bIgnoreSpeed || fForwardMoveSpeed >= -0.05f)
		{
			*pDesiredSteerAngle = 1.0f;
		}
		else
		{
			*pDesiredSteerAngle = -1.0f;
		}
		break;
	case TURN_COUNTERCLOCKWISE_GOING_BACKWARD:
		*pDesiredSpeed = rage::Max(-threePointTurnPerfom_maxCruiseSpeedSpeed, -(*pDesiredSpeed));
		if(!bIgnoreSpeed && fForwardMoveSpeed >= -0.05f)
		{
			*pDesiredSteerAngle = 1.0f;
		}
		else
		{
			*pDesiredSteerAngle = -1.0f;
		}
		break;
	case TURN_CLOCKWISE_GOING_FORWARD:
		*pDesiredSpeed = rage::Min(threePointTurnPerfom_maxCruiseSpeedSpeed, (*pDesiredSpeed));
		if(bIgnoreSpeed || fForwardMoveSpeed >= -0.05f)
		{
			*pDesiredSteerAngle = -1.0f;
		}
		else
		{
			*pDesiredSteerAngle = 1.0f;
		}
		break;
	case TURN_CLOCKWISE_GOING_BACKWARD:
		*pDesiredSpeed = rage::Max(-threePointTurnPerfom_maxCruiseSpeedSpeed, -(*pDesiredSpeed));
		if(!bIgnoreSpeed && fForwardMoveSpeed >= -0.05f)
		{
			*pDesiredSteerAngle = -1.0f;
		}
		else
		{
			*pDesiredSteerAngle = 1.0f;
		}
		break;
	case TURN_STRAIGHT_GOING_FORWARD:
		*pDesiredSpeed = rage::Min(threePointTurnPerfom_maxCruiseSpeedSpeed, (*pDesiredSpeed));
		*pDesiredSteerAngle = 0.0f;
		break;
	case TURN_STRAIGHT_GOING_BACKWARD:
		*pDesiredSpeed = rage::Max(-threePointTurnPerfom_maxCruiseSpeedSpeed, -(*pDesiredSpeed));
		*pDesiredSteerAngle = 0.0f;
		break;
	default:
		Assert(0);
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateVehDrivingAndPossiblyDo3PointTurn
// PURPOSE :  If we're headed the wrong direction and not going too fast we will
//			  consider doing a three point turn. We'll favour heading towards the centre
//			  of the road.
// RETURNS:   Whether or not a three point turn was initiated.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleThreePointTurn::ThreePointTurn(CVehicle* pVeh, float* pDirToTargetOrientation, float* pVehDriveOrientation, float *pDesiredSteerAngle, float *pDesiredSpeed, const bool bOnSmallRoad)
{
	//TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_minFacingAngleDegs, 75.0f, 0.0f, 360.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_minSpeed, 1.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnInit_minSpeedFlatTires, 0.25f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnPerfom_minTimeMsBetweenDirChanges, 500, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnPerfom_minTimeMsBetweenDirChangesFlatTires, 2000, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnPerfom_minTimeMsBetweenStuckChecks, 250, 0, 100000, 10);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnPerfom_minSpeedChangeBetweenStuckChecks, 0.001f, 0.0f, 10.0f, 0.001f);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnPerfom_minTimeMsBetweenBlindDirChanges, 1500, 0, 100000, 10);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnPerfom_minTimeMsBeforeAllowedToQuitBlindTurn, 750, 0, 100000, 10);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnCancel_minFacingAngleDegs, 75.0f, 0.0f, 360.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnCollision_probeLengthMin, 0.2f, 0.0f, 360.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnCollision_probeLengthMax, 5.0f, 0.0f, 360.0f, 0.1f);
	TUNE_GROUP_FLOAT(	VEH_AI_3_POINT_TURNS, threePointTurnCollision_probeLengthSpeedScale, 0.2f, 0.0f, 360.0f, 0.1f);
	TUNE_GROUP_INT(		VEH_AI_3_POINT_TURNS, threePointTurnPerform_timeToPauseBetweenDirChanges, 500, 0, 5000, 1 );
	
	float desiredAngleDiff;
	float absDesiredAngleDiff;

	const float fTempFacingAngleRads = !bOnSmallRoad ? s_fMinFacingAngleRadians : s_fMinFacingAngleRadiansSmallRoad;
	const float fMinFacingAngleRads = rage::Max(fTempFacingAngleRads * DtoR, pVeh->pHandling->m_fSteeringLock);

	bool isCurrentlyDoing3PointTurn =
		(m_eThreePointTurnState == TURN_CLOCKWISE_GOING_FORWARD ||
		m_eThreePointTurnState == TURN_CLOCKWISE_GOING_BACKWARD ||
		m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_FORWARD ||
		m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_BACKWARD ||
		m_eThreePointTurnState == TURN_STRAIGHT_GOING_FORWARD ||
		m_eThreePointTurnState == TURN_STRAIGHT_GOING_BACKWARD);

	static const dev_u32 MaxBlindPointTurnIterations = 2;

	// If we are already doing a 3 point turn continue to do so.
	if(isCurrentlyDoing3PointTurn)
	{
		desiredAngleDiff = SubtractAngleShorter(*pDirToTargetOrientation, *pVehDriveOrientation);
		desiredAngleDiff = fwAngle::LimitRadianAngle(desiredAngleDiff);
		absDesiredAngleDiff = rage::Abs(desiredAngleDiff);

		//static dev_u8 s_iNumBlindTurnsLeft = 0;
		TUNE_GROUP_INT(VEH_AI_3_POINT_TURNS, s_iNumBlindTurnsLeftToQuit, 1, 0, 2, 1);
		if(absDesiredAngleDiff < ( DtoR * threePointTurnCancel_minFacingAngleDegs) && m_iBlind3PointTurnIterations <= s_iNumBlindTurnsLeftToQuit)
		{	
			// Pretty much heading in the right direction. Cancel doing 3 point turn.
			return false;
		}
		pVeh->GetIntelligence()->MillisecondsNotMoving = 0;
	}
	else
	{
		// Start a 3 point turn if we're facing away from target and currently not really moving.
		m_iBlind3PointTurnIterations = 0;

		bool	isStuck;
		u32		impatienceTimer;
		bool ret = ShouldDoThreePointTurn(pVeh, pDirToTargetOrientation, pVehDriveOrientation, IsDrivingFlagSet(DF_StopAtLights), isStuck, desiredAngleDiff, absDesiredAngleDiff, impatienceTimer, bOnSmallRoad, m_bStartedBecauseOfUTurn );
		if( ret == false )
		{
			return false;
		}
				
		if(!isStuck)
		{
			pVeh->GetIntelligence()->MillisecondsNotMoving = 0;
		}
		else
		{
			if(pVeh->GetIntelligence()->MillisecondsNotMoving >= impatienceTimer)
			{
				m_iBlind3PointTurnIterations = MaxBlindPointTurnIterations;

				if (IsDrivingFlagSet(DF_DriveInReverse))
				{
					if (absDesiredAngleDiff < DtoR * threePointTurnCancel_minFacingAngleDegs)
					{
						m_eThreePointTurnState = TURN_STRAIGHT_GOING_FORWARD;
					}
					else if(desiredAngleDiff < 0.0f)
					{
						m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_FORWARD;
					}
					else
					{
						m_eThreePointTurnState = TURN_CLOCKWISE_GOING_FORWARD;
					}
				}
				else
				{
					if (absDesiredAngleDiff < DtoR * threePointTurnCancel_minFacingAngleDegs)
					{
						m_eThreePointTurnState = TURN_STRAIGHT_GOING_BACKWARD;
					}
					else if(desiredAngleDiff < 0.0f)
					{
						m_eThreePointTurnState = TURN_CLOCKWISE_GOING_BACKWARD;
					}
					else
					{
						m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_BACKWARD;
					}
				}

				m_LastTime3PointTurnChangedDirection = fwTimer::GetTimeInMilliseconds();
				isCurrentlyDoing3PointTurn = true;
			}
		}

		// Start a 3 point turn if we are facing too far from our target and not really moving.
		if(!isCurrentlyDoing3PointTurn)
		{
			float minSpeedThreePointTurn = (pVeh->GetNumFlatTires() > 0) ? threePointTurnInit_minSpeedFlatTires : threePointTurnInit_minSpeed;
			if(absDesiredAngleDiff <= fMinFacingAngleRads || (pVeh->GetAiVelocity().Mag() >= minSpeedThreePointTurn && !m_bStartedBecauseOfUTurn))
			{
				return false;
			}
			else
			{
				// Go in the direction that we're closest to.
				if(desiredAngleDiff > 0.0f)
				{
					if(IsDrivingFlagSet(DF_DriveInReverse))
					{
						m_eThreePointTurnState = TURN_CLOCKWISE_GOING_BACKWARD;
					}
					else
					{
						m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_FORWARD;
					}
				}
				else
				{
					if(IsDrivingFlagSet(DF_DriveInReverse))
					{
						m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_BACKWARD;
					}
					else
					{
						m_eThreePointTurnState = TURN_CLOCKWISE_GOING_FORWARD;
					}
				}

				m_LastTime3PointTurnChangedDirection = fwTimer::GetTimeInMilliseconds();
			} // END else [if(absDesiredAngleDiff <= ( DtoR * 90.0f) || pVeh->GetAiVelocity().Mag() >= 1.0f)]
		}// END if(!isCurrentlyDoing3PointTurn)
	}// END else [if(isCurrentlyDoing3PointTurn)]

	const float	forwardMoveSpeed = DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()), pVeh->GetAiVelocity());

	const bool bShouldStillPauseDueToDirectionChange = fwTimer::GetTimeInMilliseconds() <  m_LastTimeRelaxedPavementCheckCount + threePointTurnPerform_timeToPauseBetweenDirChanges
		&& fwTimer::GetTimeInMilliseconds() > threePointTurnPerform_timeToPauseBetweenDirChanges;
	if (bShouldStillPauseDueToDirectionChange)
	{		
		pVeh->GetIntelligence()->MillisecondsNotMoving = 0;

		//reset this, since it's really just used for measuring
		//time in a given state
		m_LastTime3PointTurnChangedDirection = fwTimer::GetTimeInMilliseconds();

		//CVehicle::ms_debugDraw.AddLine(pVeh->GetVehiclePosition(), pVeh->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 4.0f), Color_red);

		//Still update the steer direction here, so it's ready to go when we stop pausing
		UpdateDesiredSpeedAndSteerAngle(forwardMoveSpeed, pDesiredSpeed, pDesiredSteerAngle, true);
		*pDesiredSpeed = 0.0f;

		return true;
	}

	Assert(	m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_FORWARD ||
		m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_BACKWARD ||
		m_eThreePointTurnState == TURN_CLOCKWISE_GOING_FORWARD ||
		m_eThreePointTurnState == TURN_CLOCKWISE_GOING_BACKWARD ||
		m_eThreePointTurnState == TURN_STRAIGHT_GOING_FORWARD ||
		m_eThreePointTurnState == TURN_STRAIGHT_GOING_BACKWARD);

	// Throw some line scans to test whether we want to continue in the current direction.
	Vector3 scanDir;
	Vector3	scanBase[3];
	Vector3 boundBoxMin = pVeh->GetBoundingBoxMin();
	Vector3 boundBoxMax = pVeh->GetBoundingBoxMax();
	bool	bDoScan = false;
	bool	bHitSomething = false;
	bool	bChangeCauseStuck = false;
	bool	bChangeBecauseTimeOut = false;

	// Don't consider a direction change if we just did one
	int timeSince3PointTurnChangedDirrection = fwTimer::GetTimeInMilliseconds() - m_LastTime3PointTurnChangedDirection;
	int minTimeBetweenDirChanges = (pVeh->GetNumFlatTires() > 0) ? threePointTurnPerfom_minTimeMsBetweenDirChangesFlatTires : threePointTurnPerfom_minTimeMsBetweenDirChanges;
	if(timeSince3PointTurnChangedDirrection > minTimeBetweenDirChanges)
	{
		static dev_float s_fScanBaseHeightFromGround = 0.5f;
		const float fTotalHeightBelowOrigin = -pVeh->GetHeightAboveRoad() + s_fScanBaseHeightFromGround;
		const float scanLength = rage::Min((threePointTurnCollision_probeLengthMin + (rage::Abs(forwardMoveSpeed) * threePointTurnCollision_probeLengthSpeedScale)), threePointTurnCollision_probeLengthMax );

		switch(m_eThreePointTurnState)
		{
		case TURN_COUNTERCLOCKWISE_GOING_FORWARD:
		case TURN_CLOCKWISE_GOING_FORWARD:
		case TURN_STRAIGHT_GOING_FORWARD:
			if(forwardMoveSpeed >= -0.05f)
			{
				if(m_eThreePointTurnState == TURN_CLOCKWISE_GOING_FORWARD)
				{
					scanDir = VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()) + 0.2f * VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection());
				}
				else if (m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_FORWARD)
				{
					scanDir = VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()) - 0.2f * VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection());
				}
				else
				{
					scanDir = VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection());
				}
				scanDir *= scanLength;

				scanBase[0] = pVeh->TransformIntoWorldSpace(Vector3(boundBoxMin.x, boundBoxMax.y, fTotalHeightBelowOrigin));
				scanBase[1] = pVeh->TransformIntoWorldSpace(Vector3(boundBoxMax.x, boundBoxMax.y, fTotalHeightBelowOrigin));
				scanBase[2] = pVeh->TransformIntoWorldSpace(Vector3(0.0f, boundBoxMax.y, fTotalHeightBelowOrigin));

				m_wasOnRoadIndex = 0;		// Forward

				bDoScan = true;
			}
			break;
		case TURN_COUNTERCLOCKWISE_GOING_BACKWARD:
		case TURN_CLOCKWISE_GOING_BACKWARD:
		case TURN_STRAIGHT_GOING_BACKWARD:
			if(forwardMoveSpeed <= 0.05f)
			{
				if(m_eThreePointTurnState == TURN_CLOCKWISE_GOING_BACKWARD)
				{
					scanDir = -VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()) - 0.2f * VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection());
				}
				else if (m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_BACKWARD)
				{
					scanDir = -VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()) + 0.2f * VEC3V_TO_VECTOR3(pVeh->GetVehicleRightDirection());
				}
				else
				{
					scanDir = -VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection());
				}
				scanDir *= scanLength;

				scanBase[0] = pVeh->TransformIntoWorldSpace(Vector3(boundBoxMin.x, boundBoxMin.y, fTotalHeightBelowOrigin));
				scanBase[1] = pVeh->TransformIntoWorldSpace(Vector3(boundBoxMax.x, boundBoxMin.y, fTotalHeightBelowOrigin));
				scanBase[2] = pVeh->TransformIntoWorldSpace(Vector3(0.0f, boundBoxMin.y, fTotalHeightBelowOrigin));

				m_wasOnRoadIndex = 1;	// Backward

				bDoScan = true;
			}
			break;
		default:
			Assert(0);
			break;
		}

		if(bDoScan)
		{
			phIntersection testIntersection;
			static phSegment testSegment[3];
			testSegment[0].A = scanBase[0];
			testSegment[0].B = scanBase[0] + scanDir;
			testSegment[1].A = scanBase[1];
			testSegment[1].B = scanBase[1] + scanDir;
			testSegment[2].A = scanBase[2];
			testSegment[2].B = scanBase[2] + scanDir;

			if( m_relaxPavementCheckCount < 20 )
			{
				// Check to see if we've left the road
				bHitSomething = !IsPointOnRoad(testSegment[0].A, pVeh);
				if(!bHitSomething)
				{
					bHitSomething = !IsPointOnRoad(testSegment[1].A, pVeh);
				}

				if( !bHitSomething )
				{
					m_wasOnRoad[m_wasOnRoadIndex] = true;
				}

				// If we have and we were never on the road, ignore
				if( m_wasOnRoad[m_wasOnRoadIndex] == false )
				{
					bHitSomething = false;
				}
			}

#if __DEV
			bool	offRoadHitSomething = bHitSomething;
#endif // __DEV

			if(!bHitSomething && !pVeh->InheritsFromBoat())
			{
				// Check if we hit water
				float fWaterHeight = 0.0f;
				if(Water::GetWaterLevel(testSegment[0].B, &fWaterHeight, true, POOL_DEPTH, 999999.9f, NULL) &&
					(fWaterHeight >= testSegment[0].B.z))
				{
					bHitSomething = true;
				}
				else if(Water::GetWaterLevel(testSegment[1].B, &fWaterHeight, true, POOL_DEPTH, 999999.9f, NULL) &&
					(fWaterHeight >= testSegment[1].B.z))
				{
					bHitSomething = true;
				}
			}

			if(!bHitSomething)
			{
				const int testTypeFlagsCorners = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_PED_TYPE;

				//optimization: we're unlikely to hit map collision in the center of the bumper that we miss on the corners,
				//so don't test for Mover collision from the center of the bumper. this is really here to pick up on peds
				//and other tiny dynamic objects that the corner probes may have missed
				const int testTypeFlagsCenter = ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_PED_TYPE;

				bHitSomething = CPhysics::GetLevel()->TestProbe(testSegment[0], &testIntersection, pVeh->GetCurrentPhysicsInst(), testTypeFlagsCorners) != 0;
				if(!bHitSomething)
				{
					bHitSomething = CPhysics::GetLevel()->TestProbe(testSegment[1], &testIntersection, pVeh->GetCurrentPhysicsInst(), testTypeFlagsCorners) != 0;
				}

				if (!bHitSomething && CVehicleIntelligence::m_bEnableThreePointTurnCenterProbe)
				{
					bHitSomething = CPhysics::GetLevel()->TestProbe(testSegment[2], &testIntersection, pVeh->GetCurrentPhysicsInst(), testTypeFlagsCenter) != 0;
				}
			}

			if (bHitSomething && m_iBlind3PointTurnIterations > 0)
			{
				m_iBlind3PointTurnIterations--;
			}

#if __DEV
			if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
			{
				Color32 col = Color32(0, 255, 0, 255);

				if( offRoadHitSomething )
				{
					col = Color32(0, 0, 255, 255); 
				}
				else

				if(bHitSomething)
				{
					col = Color32(255, 0, 0, 255); 
				}
				grcDebugDraw::Line(testSegment[0].A, testSegment[0].B, col, -1);
				grcDebugDraw::Line(testSegment[1].A, testSegment[1].B, col, -1);

				if (CVehicleIntelligence::m_bEnableThreePointTurnCenterProbe)
				{
					grcDebugDraw::Line(testSegment[2].A, testSegment[2].B, col, -1);
				}
			}
#endif // __DEV
		}

		// It is possible we've hit something but the line scans don't catch it.
		// If our speed doesn't increase in the direction we need it to we reverse also
		if(fwTimer::GetTimeInMilliseconds() > m_LastTime3PointTurnChangedDirection + threePointTurnPerfom_minTimeMsBetweenStuckChecks)
		{
			const float forwardSpeed = DotProduct(VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection()), pVeh->GetAiVelocity());

			switch(m_eThreePointTurnState)
			{
			case TURN_COUNTERCLOCKWISE_GOING_FORWARD:
			case TURN_CLOCKWISE_GOING_FORWARD:
			case TURN_STRAIGHT_GOING_FORWARD:
				if(forwardSpeed <= m_fPreviousForwardSpeed + threePointTurnPerfom_minSpeedChangeBetweenStuckChecks
					&& fabsf(forwardSpeed) < 1.0f)
				{
					bChangeCauseStuck = true;
				}
				break;
			case TURN_COUNTERCLOCKWISE_GOING_BACKWARD:
			case TURN_CLOCKWISE_GOING_BACKWARD:
			case TURN_STRAIGHT_GOING_BACKWARD:
				if(forwardSpeed >= m_fPreviousForwardSpeed - threePointTurnPerfom_minSpeedChangeBetweenStuckChecks
					&& fabsf(forwardSpeed) < 1.0f)
				{
					bChangeCauseStuck = true;
				}
				break;
			default:
				break;
			}

			if (bChangeCauseStuck && m_iBlind3PointTurnIterations > 0)
			{
				m_iBlind3PointTurnIterations--;
			}
		}

		int changeDirTimer = threePointTurnPerfom_minTimeMsBetweenBlindDirChanges;

		//double reverse timer for big vehicles which are off road or reversing uphill roughly 10 degrees
		if(pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG))
		{
			Vec3V vForward = pVeh->GetTransform().GetForward();
			ScalarV dotUp = Dot(vForward, Vec3V(V_Z_AXIS_WZERO));
			if(IsLessThanAll(dotUp, ScalarV(-0.1745f)) || !IsPointOnRoad(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), pVeh))
			{
				changeDirTimer *= 2;
			}			
		}

		if((m_iBlind3PointTurnIterations > 0) 
			&& fwTimer::GetTimeInMilliseconds() >(m_LastTime3PointTurnChangedDirection + changeDirTimer))
		{	
			// Change direction.
			m_iBlind3PointTurnIterations--;

			if(m_iBlind3PointTurnIterations)
			{
				bChangeBecauseTimeOut = true;
			}
			else
			{
				// Quit the 3 point turn thing altogether
				return false;
			}
		}
	}

	bool bKeepDoingThreePointTurn = true;
	if(bHitSomething || bChangeCauseStuck || bChangeBecauseTimeOut)
	{
		const int iTimeInPrevState = fwTimer::GetTimeInMilliseconds() - m_LastTime3PointTurnChangedDirection;
		if (absDesiredAngleDiff < ( DtoR * threePointTurnCancel_minFacingAngleDegs)
			&& iTimeInPrevState > threePointTurnPerfom_minTimeMsBeforeAllowedToQuitBlindTurn)
		{
			bKeepDoingThreePointTurn = false;
		}

		switch(m_eThreePointTurnState)
		{
		case TURN_COUNTERCLOCKWISE_GOING_FORWARD:
			m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_BACKWARD;
			break;
		case TURN_COUNTERCLOCKWISE_GOING_BACKWARD:
			m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_FORWARD;
			break;
		case TURN_CLOCKWISE_GOING_FORWARD:
			m_eThreePointTurnState = TURN_CLOCKWISE_GOING_BACKWARD;
			break;
		case TURN_CLOCKWISE_GOING_BACKWARD:
			m_eThreePointTurnState = TURN_CLOCKWISE_GOING_FORWARD;
			break;
		case TURN_STRAIGHT_GOING_FORWARD:
			//Assertf(!bKeepDoingThreePointTurn, "Somehow ended up facing the wrong direction after a straight reverse. This is unexpected but we'll recover gracefully.");
			// Go in the direction that we're closest to.
			if (absDesiredAngleDiff < DtoR * threePointTurnCancel_minFacingAngleDegs)
			{
				m_eThreePointTurnState = TURN_STRAIGHT_GOING_BACKWARD;
			}
			else if(desiredAngleDiff > 0.0f)
			{	
				m_eThreePointTurnState = TURN_CLOCKWISE_GOING_BACKWARD;
			}
			else
			{
				m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_BACKWARD;
			}
			break;
		case TURN_STRAIGHT_GOING_BACKWARD:
			//Assertf(!bKeepDoingThreePointTurn, "Somehow ended up facing the wrong direction after a straight reverse. This is unexpected but we'll recover gracefully.");
			// Go in the direction that we're closest to.
			if (absDesiredAngleDiff < DtoR * threePointTurnCancel_minFacingAngleDegs)
			{
				m_eThreePointTurnState = TURN_STRAIGHT_GOING_FORWARD;
			}
			else if(desiredAngleDiff > 0.0f)
			{
				m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_FORWARD;
			}
			else
			{
				m_eThreePointTurnState = TURN_CLOCKWISE_GOING_FORWARD;
			}
			break;
		default:
			Assert(0);
			break;
		}

		// A count of the number of tries. If it gets above a certain number (20), then don't check the pavements
		m_relaxPavementCheckCount++;

		m_LastTimeRelaxedPavementCheckCount = fwTimer::GetTimeInMilliseconds();
		m_LastTime3PointTurnChangedDirection = fwTimer::GetTimeInMilliseconds();

		const Vector3 vPosThisFrame = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());

		static dev_float s_fMovementThresholdForChangeDirectionSqr = 0.25f * 0.25f;
		const float fPositionDeltaSqr = vPosThisFrame.Dist2(m_vPositionWhenlastChangedDirection);
		if (fPositionDeltaSqr < s_fMovementThresholdForChangeDirectionSqr)
		{
			//grcDebugDraw::Line(vPosThisFrame, vPosThisFrame + Vector3(0.0f, 0.0f, 5.0f), Color_yellow4, -1);

			m_iChangeDirectionsCount++;
		}
		else
		{
			//success, reset this counter
			m_iChangeDirectionsCount = 0;
		}

		TUNE_GROUP_INT(VEH_AI_3_POINT_TURNS, triesBeforeChangeDirection, 4, 0, 64, 1);
		if (m_iChangeDirectionsCount >= triesBeforeChangeDirection)
		{
			m_iChangeDirectionsCount = 0;

			//grcDebugDraw::Line(vPosThisFrame, vPosThisFrame + Vector3(0.0f, 0.0f, 5.0f), Color_red, -1);

			//we will have already swapped forward/reverse on this frame
			//above, so just change the rotational direction
			switch (m_eThreePointTurnState)
			{
			case TURN_CLOCKWISE_GOING_FORWARD:
				m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_FORWARD;
				break;
			case TURN_CLOCKWISE_GOING_BACKWARD:
				m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_BACKWARD;
				break;
			case TURN_COUNTERCLOCKWISE_GOING_FORWARD:
				m_eThreePointTurnState = TURN_CLOCKWISE_GOING_FORWARD;
				break;
			case TURN_COUNTERCLOCKWISE_GOING_BACKWARD:
				m_eThreePointTurnState = TURN_CLOCKWISE_GOING_BACKWARD;
				break;
			case TURN_STRAIGHT_GOING_FORWARD:
				m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_FORWARD;
				break;
			case TURN_STRAIGHT_GOING_BACKWARD:
				m_eThreePointTurnState = TURN_COUNTERCLOCKWISE_GOING_BACKWARD;
				break;
			default:
				break;
			}
		}

		m_vPositionWhenlastChangedDirection = vPosThisFrame;

	}// END if(bHitSomething || bChangeCauseStuck || bChangeBecauseTimeOut)

	//if we're doing a turn, and we end up getting within the tolerance distance,
	//change to a straight line move
	//but don't update any of the "changed direction" timers or anything,
	//we want to cnosider it part of the same move, just preventing ourselves
	//from oversteering
	TUNE_GROUP_FLOAT(VEH_AI_3_POINT_TURNS, fBailOutOfTurnThresholdMultiplier, 0.5f, 0.0f, 1.0f, 0.01f);
	if (absDesiredAngleDiff <= ( fMinFacingAngleRads * fBailOutOfTurnThresholdMultiplier))
	{
		if (m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_FORWARD
			|| m_eThreePointTurnState == TURN_CLOCKWISE_GOING_FORWARD)
		{
			m_eThreePointTurnState = TURN_STRAIGHT_GOING_FORWARD;
		}
		else if (m_eThreePointTurnState == TURN_COUNTERCLOCKWISE_GOING_BACKWARD
			|| m_eThreePointTurnState == TURN_CLOCKWISE_GOING_BACKWARD)
		{
			m_eThreePointTurnState = TURN_STRAIGHT_GOING_BACKWARD;
		}
	}

	UpdateDesiredSpeedAndSteerAngle(forwardMoveSpeed, pDesiredSpeed, pDesiredSteerAngle, false);

	const bool bShouldPauseDueToDirectionChange = fwTimer::GetTimeInMilliseconds() <  m_LastTimeRelaxedPavementCheckCount + threePointTurnPerform_timeToPauseBetweenDirChanges
		&& fwTimer::GetTimeInMilliseconds() > threePointTurnPerform_timeToPauseBetweenDirChanges;
	if (bShouldPauseDueToDirectionChange)
	{
		*pDesiredSpeed = 0.0f;
		pVeh->GetIntelligence()->MillisecondsNotMoving = 0;

		//CVehicle::ms_debugDraw.AddLine(pVeh->GetVehiclePosition(), pVeh->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 4.0f), Color_red);
	}

	return bKeepDoingThreePointTurn;
}


/////////////////////////////////////////////////////////////////////////////////
// AdjustControls
// Clamp the steer angle to the vehicles ranges and adjust the gas
// and brake pedal input to try to achieve the desired speed (taking
// into account slopes).
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleThreePointTurn::AdjustControls(const CVehicle* const pVeh, CVehControls* pVehControls, const float desiredSteerAngle, const float desiredSpeed)
{
	Assertf(pVeh, "AdjustControls expected a valid set vehicle.");
	Assertf(pVehControls, "AdjustControls expected a valid set of vehicle controls.");

#if __DEV
	if(CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{
		TUNE_GROUP_BOOL(CAR_AI, showSteerDir, false);
		if(showSteerDir)
		{
			// Get the current drive direction and orientation.
			const Vector3 vehDriveDir(VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse))));
			float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);

			TUNE_GROUP_FLOAT(CAR_AI, alpha, 1.0f, 0.0f, 1.0f, 0.01f);
			Color32 col(0.0f, 1.0f, 0.0f, alpha);
			Vector3 startPos = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
			Vector3 endPos = startPos +(Vector3(rage::Cosf(desiredSteerAngle + vehDriveOrientation), rage::Sinf(desiredSteerAngle + vehDriveOrientation), 0.0f) * 5.0f);
			grcDebugDraw::Line(startPos, endPos, col);
		}
	}
#endif // __DEV

	// Clip steering to sensible values
	float newSteerAngle = desiredSteerAngle;
	const float maxSteerAngle = pVeh->GetIntelligence()->FindMaxSteerAngle();
	newSteerAngle = rage::Clamp(newSteerAngle, -maxSteerAngle, maxSteerAngle);
	Assert(newSteerAngle > -2.0f && newSteerAngle < 2.0f);

	const float fAiXYSpeed = pVeh->GetAiXYSpeed();
	if(fAiXYSpeed < 5.0f)
	{
		// Humanize the steering
		//const bool bStopped = fAiXYSpeed < 0.1;
		const float steerAngleDiff = newSteerAngle - pVehControls->m_steerAngle;
		static dev_float fMultiplier = 2.0f;
		const float maxsteerAngleChange = (fMultiplier) * GetTimeStep();
		newSteerAngle = pVehControls->m_steerAngle + Clamp(steerAngleDiff, -maxsteerAngleChange, maxsteerAngleChange);
	}

	const float portionOfSteerMaxAngle = rage::Abs(newSteerAngle / maxSteerAngle);
	const float maxGasAllowed = 1.0f -(0.7f * portionOfSteerMaxAngle); // Only allowed 30% gas when at maximum steering angle.

	// Work out the speed so that we can adjust it with gas/brakes.
	float forwardSpeed = DotProduct(pVeh->GetAiVelocity(), VEC3V_TO_VECTOR3(pVeh->GetTransform().GetB()));

	//not sure about this line
// 	if(IsDrivingFlagSet(DF_DriveInReverse))
// 	{
// 		forwardSpeed = -forwardSpeed;
// 	}

	const float speedDiffDesired = desiredSpeed - forwardSpeed;	// forwardSpeed is in m/s. 40km/u=11.1m/s 60=16.7 80=22.2 100=27.8 120=33.3 140=38.9

	float slopeGasMultiplier = 1.0f;
	float slopeBrakeMultiplier = 1.0f;
	const float slope = pVeh->GetTransform().GetB().GetZf();// Positive = uphill.
	const float slopeGasMultiplierForward		= 1.0f + rage::Max(0.0f, slope * 6.0f);
	const float slopeGasMultiplierBackwards		= 1.0f + rage::Max(0.0f, -slope * 5.0f);
	const float slopeBrakeMultiplierForward		= 1.0f + rage::Max(0.0f, -slope * 4.0f);
	const float slopeBrakeMultiplierBackwards	= 1.0f + rage::Max(0.0f, slope * 4.0f);

	const float maxGas = 1.0f;
	float maxBrake = 1.0f;
	const float minGas = -1.0f;
	const float minBrake = -1.0f;

	pVehControls->m_brake = 0.0f;

	// Special case if we want to stop and are going slowly. Use full brakes.
	// This stops cars from sneaking past traffic lights.
	if(rage::Abs(desiredSpeed) < 0.05f && rage::Abs(speedDiffDesired) < 0.5f)		// was 0.03f
	{
		pVehControls->m_brake = 1.0f;
		pVehControls->m_throttle = 0.0f;
	}	
	else
	{
		if(desiredSpeed < 0.0f)
		{	
			// Car is trying to go backwards
			if(forwardSpeed > 2.0f)
			{
				pVehControls->m_brake = 1.0f;
				pVehControls->m_throttle = 0.0f;
			}
			else if(speedDiffDesired < 0.0f)
			{	
				// We should accelerate backwards
				if(forwardSpeed > -2.0f) // Accelerate quickly initially so that reverse fudge doesn't kick in
				{
					slopeGasMultiplier = slopeGasMultiplierBackwards;
					pVehControls->m_throttle = speedDiffDesired / 4.0f;
				}
				else
				{
					slopeGasMultiplier = slopeGasMultiplierBackwards;
					pVehControls->m_throttle = speedDiffDesired / 9.0f;
				}
			}
			else
			{	
				// We should slow down a bit
				pVehControls->m_throttle = 0.0f;

				// Going too fast. Use brake to slow down
				slopeBrakeMultiplier = slopeBrakeMultiplierBackwards;
				maxBrake = 0.5f;
				pVehControls->m_brake = speedDiffDesired / 12.0f;
			}
		}
		else // Traveling forward
		{
			if(speedDiffDesired > 0.0f)
			{
				if (forwardSpeed < -2.0f)
				{
					pVehControls->m_brake = 1.0f;
					//slopeGasMultiplier = slopeGasMultiplierForward;
					//pVehControls->m_throttle = speedDiffDesired / 4.0f;
				}
				else if(forwardSpeed < 2.0f) // Accelerate quickly initially so that reverse fudge doesn't kick in
				{
					slopeGasMultiplier = slopeGasMultiplierForward;
					pVehControls->m_throttle = speedDiffDesired / 4.0f;
				}
				else
				{
					slopeGasMultiplier = slopeGasMultiplierForward;
					pVehControls->m_throttle = speedDiffDesired / 9.0f;
				}
			}
			else
			{
				pVehControls->m_throttle = 0.0f;

				// Going too fast. Use brake to slow down
				slopeBrakeMultiplier = slopeBrakeMultiplierForward;
				maxBrake = 0.75f;
				pVehControls->m_brake = -speedDiffDesired / 10.0f;
			}
		}

		// Depending on our steering angle we're only allowed to accelerate a certain amount.
		// This should avoid cars skidding out when they accelerate out of bends.
		pVehControls->m_throttle = rage::Clamp(pVehControls->m_throttle, -maxGasAllowed, maxGasAllowed);

		pVehControls->m_throttle *= slopeGasMultiplier;
		pVehControls->m_brake *= slopeBrakeMultiplier;

		pVehControls->m_throttle = rage::Clamp((pVehControls->m_throttle), minGas, maxGas);
		pVehControls->m_brake = rage::Clamp((pVehControls->m_brake), minBrake, maxBrake);
	}

	// We have the problem where the car is steering fully and accelerating at the same time.
	// This causes traction to be used up by the acceleration and the steering to not happen.
	// Fix this here(if steering extremely don't accelerate)
	const float gasDownMult = portionOfSteerMaxAngle * rage::Clamp(((forwardSpeed - 8.0f) / 8.0f), 0.0f, 1.0f);

	TUNE_GROUP_FLOAT(VEH_AI_3_POINT_TURNS, SLIP_THROTTLE_REDUCTION_LERP,		0.3f, 0.0f, 1.0f, 0.01f);
	// Finally apply reduction based on the traction available through the wheels
	const float fMaxThrottleFromTraction = CTaskVehicleGoToPointAutomobile::CalculateMaximumThrottleBasedOnTraction(pVeh);
	const float reducedMaxThrottle = Lerp(SLIP_THROTTLE_REDUCTION_LERP, m_fMaxThrottleFromTraction, fMaxThrottleFromTraction);
	m_fMaxThrottleFromTraction = reducedMaxThrottle;

	float fDesiredThrottle = (pVehControls->m_throttle) *(1.0f - gasDownMult);
	pVehControls->m_throttle = rage::Min(fDesiredThrottle, reducedMaxThrottle);
	pVehControls->SetSteerAngle( newSteerAngle );
	pVehControls->m_handBrake = false;

	// Boats will use their handbrake of they need to make a tight corner.
	if(pVeh->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		if(forwardSpeed > 2.0f && rage::Abs(desiredSteerAngle) > ( DtoR * 25.0f))
		{
			pVehControls->m_handBrake = true;
		}
	}

	Assert(pVehControls->m_steerAngle >= -HALF_PI && pVehControls->m_steerAngle <= HALF_PI && pVehControls->m_steerAngle == pVehControls->m_steerAngle);
	Assert(pVehControls->m_throttle >= -1.0f && pVehControls->m_throttle <= 1.0f);
	Assert(pVehControls->m_brake >= 0.0f && pVehControls->m_brake <= 1.0f);
}

/*
bool	CTaskVehicleThreePointTurn::RejectNoneCB(CPathNode * UNUSED_PARAM(pPathNode), void * UNUSED_PARAM(data))
{
	return true;
}
*/

bool	CTaskVehicleThreePointTurn::IsPointOnRoad(const Vector3 &pos, const CVehicle* DEV_ONLY(pVehicle), const bool bReturnTrueIfInJunctionAABB)
{

	const CNodeAddress iNode = ThePaths.FindNodeClosestToCoors(pos, NULL, NULL, 50.0f);
	if(iNode.IsEmpty())
		return false;

	const CPathNode * pOldNode = ThePaths.FindNodePointerSafe(iNode);

	if(!pOldNode)
		return false;
	if(!pOldNode->NumLinks())
		return false;

	for(int i = 0; i<pOldNode->NumLinks(); i++ )
	{
		const CPathNodeLink & link = ThePaths.GetNodesLink(pOldNode, i);

		const CPathNode * pNewNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
		if(!pNewNode)
			continue;

		Vector3 vOldNode, vNewNode, vToNext, vRight;
		pOldNode->GetCoors(vOldNode);
		pNewNode->GetCoors(vNewNode);
		vToNext = vNewNode - vOldNode;
		vToNext.Normalize();
		vRight = CrossProduct(vToNext, ZAXIS);
		vRight.Normalize();

		float fDistToRightSide = 0.0f;
		float fDistToLeftSide = 0.0f;
		const bool bAllLanesThoughCentre = (link.m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
		ThePaths.FindRoadBoundaries(link.m_1.m_LanesToOtherNode, link.m_1.m_LanesFromOtherNode, bAllLanesThoughCentre ? 0.0f : link.m_1.m_Width, link.m_1.m_NarrowRoad, bAllLanesThoughCentre, fDistToLeftSide, fDistToRightSide);

		if (link.IsDontUseForNavigation() || pOldNode->IsJunctionNode() || pNewNode->IsJunctionNode())
		{
			fDistToLeftSide += link.GetLaneWidth();
			fDistToRightSide += link.GetLaneWidth();
		}

		if (bReturnTrueIfInJunctionAABB)
		{
			const CJunction* pOldJunction = pOldNode->IsJunctionNode() ? CJunctions::FindJunctionFromNode(iNode) : NULL;
			const CJunction* pNewJunction = pNewNode->IsJunctionNode() ? CJunctions::FindJunctionFromNode(link.m_OtherNode) : NULL;

			if ((pOldJunction && pOldJunction->IsPosWithinBounds(pos,10.0f))
				|| (pNewJunction && pNewJunction->IsPosWithinBounds(pos, 10.0f)))
			{
				return true;
			}
		}

		Vector3 vCorner;
		Vector3	v1, v2;
		Vector3 vToPoint;

		vCorner = vOldNode - ( vRight * fDistToLeftSide );
		v1 = vNewNode - vOldNode;
		v2 = (vOldNode + ( vRight * fDistToRightSide )) - vCorner;
		vToPoint = pos - vCorner;

		float	dot1,dot2;
		dot1 = Dot(vToPoint, v1);
		dot2 = Dot(v1, v1);
		if(0 <= dot1 && dot1 <= dot2 )
		{
			dot1 = Dot(vToPoint, v2);
			dot2 = Dot(v2, v2);
			if( 0 <= dot1 && dot1 <= dot2 )
			{
				// Draw the rectangle
#if __DEV
				if(pVehicle && CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
				{
					Vector3	vTopLeft, vTopRight, vBotLeft, vBotRight;

					Color32 col = Color32(255, 0, 0, 255);
					vTopLeft = vNewNode - (fDistToLeftSide * vRight );
					vTopLeft.z += 1.0f;
					vTopRight = vNewNode + (fDistToRightSide * vRight );
					vTopRight.z += 1.0f;
					vBotLeft = vOldNode - (fDistToLeftSide * vRight );
					vBotLeft.z += 1.0f;
					vBotRight = vOldNode + (fDistToRightSide * vRight );
					vBotRight.z += 1.0f;
					grcDebugDraw::Line(vTopLeft, vTopRight, col);
					grcDebugDraw::Line(vTopRight, vBotRight, col);
					grcDebugDraw::Line(vBotRight, vBotLeft, col);
					grcDebugDraw::Line(vBotLeft, vTopLeft, col);

					// Draw the line between the links
					Vector3 vNew = vNewNode;
					Vector3 vOld = vOldNode;

					vOld.z += 1.0f;
					vNew.z += 1.0f;
					col = Color32(0, 0, 255, 255);
					grcDebugDraw::Line(vOld, vNew, col);
				}

#endif	//__DEV

				return true;
			}			
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleThreePointTurn::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_ThreePointTurn&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_ThreePointTurn",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif
