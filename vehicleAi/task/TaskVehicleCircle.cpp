#include "TaskVehicleCircle.h"

// Framework headers

// Game headers
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/heli.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoTo.h"
#include "VehicleAi\task\TaskVehicleGoToHelicopter.h"
#include "VehicleAi\task\TaskVehiclePark.h"
#include "game/modelindices.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////

CTaskVehicleCircle::CTaskVehicleCircle(const sVehicleMissionParams& params)
: CTaskVehicleMissionBase(params)
, m_fHeliRequestedOrientation(-1.0f)
, m_iFlightHeight(7)
, m_iMinHeightAboveTerrain(20)
, m_iHeliFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CIRCLE);
}

////////////////////////////////////////////////////////////////////

CTaskVehicleCircle::~CTaskVehicleCircle()
{

}

////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleCircle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Startstate
		FSM_State(State_Start)
			FSM_OnUpdate
			return Start_OnUpdate(pVehicle);

		// Circle state
		FSM_State(State_Circle)
			FSM_OnUpdate
				return Circle_OnUpdate(pVehicle);

		FSM_State(State_CircleHeli)
			FSM_OnEnter
				CircleHeli_OnEnter(pVehicle);
			FSM_OnUpdate
				return CircleHeli_OnUpdate(pVehicle);
	
		// Stop state
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);
	FSM_End
}


aiTask::FSM_Return CTaskVehicleCircle::Start_OnUpdate( CVehicle* pVehicle )
{
	if( pVehicle->InheritsFromHeli() )
	{
		SetState(State_CircleHeli);
		return FSM_Continue;
	}
	else
	{
		SetState(State_Circle);
		return FSM_Continue;
	}
}

void CTaskVehicleCircle::CircleHeli_OnEnter( CVehicle* ASSERT_ONLY(pVehicle) )
{
	aiFatalAssertf(dynamic_cast<CHeli*>(pVehicle), "Vehicle not a heli!");
	sVehicleMissionParams params = m_Params;
	params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);
	SetNewTask(rage_new CTaskVehicleGoToHelicopter(params, m_iHeliFlags | CTaskVehicleGoToHelicopter::HF_DontClampProbesToDestination, m_fHeliRequestedOrientation, m_iMinHeightAboveTerrain));
}

aiTask::FSM_Return CTaskVehicleCircle::CircleHeli_OnUpdate( CVehicle* pVehicle )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER) )
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	if( m_Params.GetTargetEntity().GetEntity() != NULL )
	{
		Vector3 vHeliPosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		//Vector3 vTargetPosition = VEC3V_TO_VECTOR3(m_Params.m_TargetEntity.GetEntity()->GetTransform().GetPosition());
		Vector3	vTargetPosition = VEC3V_TO_VECTOR3(m_Params.GetTargetEntity().GetEntity()->GetTransform().Transform(VECTOR3_TO_VEC3V(m_Params.GetTargetPosition())));
	
		Vector3 vToTarget = vTargetPosition - vHeliPosition;
		vToTarget.z = 0.0f;
		vToTarget.Normalize();
		float fHeadingToTarget = fwAngle::GetATanOfXY(vToTarget.x, vToTarget.y);

		aiFatalAssertf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER, "Unexpected subtask!");
		CTaskVehicleGoToHelicopter* pSubtask = static_cast<CTaskVehicleGoToHelicopter*>(GetSubTask());

		const bool bInRange = (vHeliPosition - vTargetPosition).XYMag2() < rage::square(m_Params.m_fTargetArriveDist + 20.0f);
		const bool bAttainRequestedOrientation = m_iHeliFlags & CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation;

		if( bInRange )
		{
			if( bAttainRequestedOrientation )
			{
				pSubtask->SetOrientation(m_fHeliRequestedOrientation+fHeadingToTarget);
			}
			pSubtask->SetOrientationRequested(m_iHeliFlags & CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation);
			pSubtask->SetSlowDownDistance(Max(m_Params.m_fTargetArriveDist, 20.0f));

			static float s_StepAngle = PI / 8; // about 15 degrees
			if (m_iHeliFlags & CTaskVehicleGoToHelicopter::HF_CircleOppositeDirection)
			{
				vToTarget.RotateZ(-s_StepAngle);
			}
			else
			{
				vToTarget.RotateZ(s_StepAngle);
			}
		}
		else
		{
			pSubtask->SetOrientationRequested(false);
		}

		const Vector3 vStrafePosition = vTargetPosition - (vToTarget*m_Params.m_fTargetArriveDist);

		if( bInRange && !bAttainRequestedOrientation )
		{
			const Vec2V vForward = pVehicle->GetVehicleForwardDirection().GetXY();

			const Vec2V vToStrafePosition = NormalizeSafe(VECTOR3_TO_VEC3V(vStrafePosition - vHeliPosition).GetXY(), vForward);
			const ScalarV fToStrafePosition = Dot(vForward, vToStrafePosition);

			// If the strafe position is greater than 90 degrees from our current heading
			if( IsLessThanAll(fToStrafePosition, ScalarV(V_ZERO)) )
			{
				const Vec2V vRight = Vec2V(-vForward.GetY(), vForward.GetX());

				const Vec2V vTargetEntity = m_Params.GetTargetEntity().GetEntity()->GetTransform().GetPosition().GetXY();
				const Vec2V vToTargetEntity = NormalizeSafe(vTargetEntity - VECTOR3_TO_VEC3V(vHeliPosition).GetXY(), vForward);

				const ScalarV fRightToStrafePosition = Dot(vRight, vToStrafePosition);
				const ScalarV fRightToTargetEntity = Dot(vRight, vToTargetEntity);

				const BoolV bOnOppositeSides = Or(And(IsLessThan(fRightToStrafePosition, ScalarV(V_ZERO)), IsGreaterThan(fRightToTargetEntity, ScalarV(V_ZERO))), 
													And(IsGreaterThan(fRightToStrafePosition, ScalarV(V_ZERO)), IsLessThan(fRightToTargetEntity, ScalarV(V_ZERO))));

				// And the target entity is on the opposite side
				if( IsTrue(bOnOppositeSides) )
				{
					// Turn toward the target entity first
					const float fToTargetEntity = fwAngle::GetATanOfXY(vToTargetEntity.GetXf(), vToTargetEntity.GetYf());
					pSubtask->SetOrientation(fToTargetEntity);
					pSubtask->SetOrientationRequested(true);
				}
			}
		}

		pSubtask->SetTargetPosition(&vStrafePosition);
		pSubtask->SetTargetEntity(NULL);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCircle::Circle_OnUpdate				(CVehicle* pVehicle)
{
	// Sometimes the target may have been removed for a frame or so.
	// In that case don't do anything.
	if(!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	FindTargetCoors(pVehicle, m_Params);

	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	const Vector3& vTargetPos = m_Params.GetTargetPosition();

	float Distance = rage::Sqrtf( (vTargetPos.x - vVehiclePosition.x) * (vTargetPos.x - vVehiclePosition.x) +
		(vTargetPos.y - vVehiclePosition.y) * (vTargetPos.y - vVehiclePosition.y) );


	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_BOAT:
		if ( Distance < 30.0f)
		{
			SetCruiseSpeed(1.0f);		// Police boats circling a target looks a bit silly. Make them go super slow.
		}
		
		Boat_SteerCirclingTarget( pVehicle, &pVehicle->m_vehControls, GetTargetEntity(), GetCruiseSpeed());

		break;
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		Helicopter_SteerCirclingTarget((CHeli*)pVehicle, GetTargetEntity());
		break;
	default:
		Assertf(false,"Vehicle type not supported by TaskVehicleCircle");
		break;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////

void		CTaskVehicleCircle::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

//////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCircle::Stop_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SteerCirclingTarget
// PURPOSE :  Circle the player so that the heli's front guns have a good shot.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleCircle::Helicopter_SteerCirclingTarget(CHeli *pHeli, const CEntity *pTarget)
{
	Assertf(pTarget, "SteerCirclingTarget has a NULL target Entity");

	const Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
	const Vector3 vHeliPosition = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());
	const float fDesiredHeight = vTargetPosition.z + 7.0f;

	static dev_float inFuture = 3.0f;
	float	dX = vTargetPosition.x - (vHeliPosition.x + pHeli->GetVelocity().x * inFuture);
	float	dY = vTargetPosition.y - (vHeliPosition.y + pHeli->GetVelocity().y * inFuture);
	float TargetOrientation = fwAngle::GetATanOfXY(dX, dY);

	Vector3	forwardVec = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB());
	float HeliOrientation = fwAngle::GetATanOfXY(forwardVec.x, forwardVec.y);
	float OrientationExtraDiff = HeliOrientation - pHeli->GetHeliIntelligence()->GetOldOrientation();
	OrientationExtraDiff = fwAngle::LimitRadianAngleSafe(OrientationExtraDiff);
	static	float	orientationTime = 0.3f;
	HeliOrientation += OrientationExtraDiff * (orientationTime / fwTimer::GetTimeStep());

	float PredictedHeight = vHeliPosition.z + pHeli->GetVelocity().z * 2.0f;
	float HeightDiff = fDesiredHeight - PredictedHeight;

	float Throttle = 0.0f;		// Guess this to stay at same height. (was 0.3f)
	if(HeightDiff > 0)
	{		// We want to climb
		Throttle += HeightDiff / 10.0f;
	}
	else
	{		// We can be pretty rough in the descend since the heli physics slow the guy down a lot anyway.
		Throttle += HeightDiff / 5.0f;
	}

	//if(bLimitDescentSpeed)
	{
		float	limitThrottle;
		static	float	limitSpeed = -3.0f;		// limit in m/s
		limitThrottle = (limitSpeed - pHeli->GetVelocity().z);

		Throttle = rage::Max(Throttle, limitThrottle);
	}

	Throttle = Clamp(Throttle, -0.5f, 1.0f);

	// Make the heli face the target coordinates.
	float OrientationDiff = TargetOrientation - HeliOrientation;
	OrientationDiff = fwAngle::LimitRadianAngleSafe(OrientationDiff);
	const float fYawControlUnclamped = OrientationDiff * -0.5f;
	const float fYawControl = rage::Clamp(fYawControlUnclamped, -1.0f, 1.0f);
	pHeli->SetYawControl(fYawControl);

	Vector3	VecToTarget = vTargetPosition - vHeliPosition;
	VecToTarget.z = 0.0f;

	Vector3	MoveSpeed = pHeli->GetVelocity();
	MoveSpeed.z = 0.0f;

	Vector3	rightward = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetA());
	rightward.z = 0.0f;
	rightward.Normalize();
	forwardVec = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB());
	forwardVec.z = 0.0f;
	forwardVec.Normalize();

	{		
		// We'd like to move sideways a little bit to circle the target
		static dev_float desiredRightSpeed = 3.0f;

		static float fTweak2 = -0.025f;
		pHeli->SetRollControl((desiredRightSpeed - rightward.Dot(MoveSpeed))  * fTweak2);	// was -0.002
	}
	//	Assert(steeringDownMult >= 0.0f && steeringDownMult <= 1.0f);
	pHeli->SetRollControl(rage::Clamp(pHeli->GetRollControl(), -0.75f, 0.75f));


	float	distToTarget = rage::Sqrtf(dX*dX + dY*dY);

	{		// Move forward to regulate our targetDistanceFlat to the target.
		static float fTweak4 = -0.005f;
		pHeli->SetPitchControl((distToTarget - 30.0f) * fTweak4);
	}
	pHeli->SetPitchControl(rage::Clamp(pHeli->GetPitchControl(), -0.7f, 0.7f));
	pHeli->SetThrottleControl(1.0f + Throttle);

}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SteerAIBoatCirclingPlayer
// PURPOSE :  Circle the player so that the gunner on the bow has a good shot.
/////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleCircle::Boat_SteerCirclingTarget(CVehicle *pVehicle, CVehControls* pVehControls, const CEntity *pTarget, float fCruiseSpeed)
{
	Assertf(pVehicle, "SteerAIBoatHeadingForTarget expected a valid set vehicle.");
	Assertf(pVehControls, "SteerAIBoatHeadingForTarget expected a valid set of vehicle controls.");

	float	TargetOrientation, BoatOrientation, AngleDiff;
	float	BoatDirX, BoatDirY, Length, Ratio, Speed, SpeedDiff;
	Vector3	SpeedVector;

	Assertf(pTarget, "SteerAIBoatCirclingTarget has a NULL target Entity");

	const Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

	// Calculate the target point (either to the left or to the right of the player)
	Vector3	VecTo = (vTargetPosition - vVehiclePosition);
	VecTo.z = 0.0f;
	VecTo.Normalize();
	float	Temp;
	Temp = VecTo.x;
	VecTo.x = VecTo.y;
	VecTo.y = -Temp;
	if (pVehicle->GetRandomSeed() & 1)	// Some boats go clockwise, some counter clockwise.
	{
		VecTo *= -12.0f;
	}
	else
	{
		VecTo *= 26.0f;
	}
	Vector3 TargetCoors = vTargetPosition + VecTo;

	// Vector in the direction of the car
	Vector3 vecBoatDir(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
	BoatDirX = vecBoatDir.x;
	BoatDirY = vecBoatDir.y;
	Length = rage::Sqrtf( BoatDirX*BoatDirX + BoatDirY*BoatDirY );
	if (Length)
	{
		BoatDirX /= Length;
		BoatDirY /= Length;
	}
	else
	{
		BoatDirX = 1.0f;
	}

	// Work out the proper steering angle.	
	TargetOrientation = fwAngle::GetATanOfXY(TargetCoors.x - vVehiclePosition.x, TargetCoors.y - vVehiclePosition.y);
	BoatOrientation = fwAngle::GetATanOfXY(BoatDirX, BoatDirY);

	AngleDiff = TargetOrientation - BoatOrientation;
	AngleDiff = fwAngle::LimitRadianAngle(AngleDiff);

	// Work out the speed so that we can adjust it with gas/brakes.
	SpeedVector = pVehicle->GetVelocity();
	SpeedVector.z = 0.0f;	// Only in horizontal plane
	Speed = SpeedVector.Mag();

	SpeedDiff = fCruiseSpeed - Speed;	// Speed is in m/s. 40km/u=11.1m/s 60=16.7 80=22.2 100=27.8 120=33.3 140=38.9
	{
		if (SpeedDiff <= 0.0f)
		{	// We're going too fast. Slow down a bit.
			pVehControls->m_throttle = -0.1f;
			if (SpeedDiff < -5.0f)
			{
				pVehControls->m_throttle = -0.2f;
			}
		}
		else
		{	// We're going not quite fast enough
			Ratio = SpeedDiff / fCruiseSpeed;
			if (Ratio > 0.25f)
			{	// Way too slow->full throttle
				pVehControls->m_throttle = 1.0f;
			}
			else
			{	// Release gas as we approach desired speed
				pVehControls->m_throttle = 1.0f - (0.25f - Ratio) * 4.0f;
			}
		}
	}

	pVehControls->m_brake = 0.0f;
	pVehControls->SetSteerAngle( rage::Clamp(AngleDiff, -HALF_PI, HALF_PI) );
	pVehControls->m_handBrake = false;
}


void CTaskVehicleCircle::SetHelicopterSpecificParams( float fHeliRequestedOrientation, int iFlightHeight, int iMinHeightAboveTerrain, s32 iHeliFlags )
{
	m_fHeliRequestedOrientation = fHeliRequestedOrientation;
	m_iFlightHeight				= iFlightHeight; 
	m_iMinHeightAboveTerrain	= iMinHeightAboveTerrain;
	m_iHeliFlags				= iHeliFlags;
}

aiTask* CTaskVehicleCircle::Copy() const
{
	CTaskVehicleCircle* pTask = rage_new CTaskVehicleCircle(m_Params);
	pTask->SetHelicopterSpecificParams(m_fHeliRequestedOrientation, m_iFlightHeight, m_iMinHeightAboveTerrain, m_iHeliFlags);
	return pTask;
}

////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleCircle::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Circle",
		"State_CircleHeli",
		"State_Stop"
	};

	return aStateNames[iState];
}

#endif
