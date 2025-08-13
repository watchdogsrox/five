#include "Peds/Ped.h"
#include "vehicleAi/FlyingVehicleAvoidance.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "grcore/debugdraw.h"
#include <float.h>

//////////////////////////////////////////////////////////////////////////
// Helper class used to prevent flying vehicles from colliding with each other
// If you have a lot of vehicles in the air doing the same thing, you should be calling SlideDestinationTarget each frame to adjust the target position
// Generally, SteerAvoidCollisions should be called each frame which will adjust the target velocity based on various criteria
//////////////////////////////////////////////////////////////////////////

bool CFlyingVehicleAvoidanceManager::s_drawAvoid = false;
atFixedArray<RegdVeh, MAX_FLYING_VEHICLES_AVOIDANCE> CFlyingVehicleAvoidanceManager::m_Vehicles;
atFixedArray<Vector3, MAX_FLYING_VEHICLE_DESTINATIONS> CFlyingVehicleAvoidanceManager::m_Destinations;

//make tuneables
float CFlyingVehicleAvoidanceManager::s_fMinSpeedFlockAvoid = 4.0f;
float CFlyingVehicleAvoidanceManager::s_fLookAheadTime = 2.5f;
float CFlyingVehicleAvoidanceManager::s_fAvoidDistMultiplier = 10.0f;
float CFlyingVehicleAvoidanceManager::s_fSightConeAngle = 30.0f;
float CFlyingVehicleAvoidanceManager::s_fGetOutWayTime = 0.15f;

Vector3 RandomDirections[20] = 
{
	Vector3(0.0f, 0.0f, 1.0f),
	Vector3(0.0f, 0.0f, 0.0f), //-1.0f),
	Vector3(0.0f, 1.0f, 0.0f),
	Vector3(0.0f, -1.0f, 0.0f),
	Vector3(1.0f, 0.0f, 0.0f),
	Vector3(-1.0f, 0.0f, 0.0f),
	Vector3(0.0f, 0.707f, 0.707f),
	Vector3(0.0f, 0.707f, 0.0f), //-0.707f),	
	Vector3(0.0f, -0.707f, 0.707f),
	Vector3(0.0f, -0.707f, 0.0f), //-0.707f),
	Vector3(0.0f, 0.707f, 0.0f), //-0.707f),
	Vector3(0.707f, 0.0f, 0.707f),
	Vector3(0.707f, 0.0f, 0.0f), //-0.707f),	
	Vector3(-0.707f, 0.0f, 0.707f),
	Vector3(-0.707f, 0.0f, 0.0f), //-0.707f),
	Vector3(0.707f, 0.0f, 0.0f), //-0.707f),
	Vector3(0.707f, 0.707f, 0.0f ),
	Vector3(0.707f, -0.707f, 0.0f ),	
	Vector3(-0.707f, 0.707f, 0.0f ),
	Vector3(-0.707f, -0.707f, 0.0f ),
};


CFlyingVehicleAvoidanceManager::CFlyingVehicleAvoidanceManager():
m_vehicle(0)
,m_fAvoidanceScalar(0.0f)
,m_closeVehicles()
,m_fOutWayTimer(0.0f)
{
	m_outTargetPosition.Zero();
	m_outTargetVelocity.Zero();
	m_currentTargetVelocity.Zero();
	m_currentTargetPos.Zero();
	m_holdTargetVelocity.Zero();
}

CFlyingVehicleAvoidanceManager::~CFlyingVehicleAvoidanceManager()
{

}

void CFlyingVehicleAvoidanceManager::SteerAvoidCollisions(Vector3& outTargetPosition, const Vector3& in_currentTargetVelocity, const Vector3& targetPos)
{
	if(m_vehicle->InheritsFromPlane())
	{
		m_currentTargetPos = targetPos;
		m_currentTargetVelocity = in_currentTargetVelocity;
		m_outTargetPosition = m_currentTargetPos;

		bool bPreventYaw;
		UpdatePlaneAvoidance(bPreventYaw);
#if __BANK
		if (s_drawAvoid)
		{
			if(m_outTargetPosition.Dist(outTargetPosition) > 0.0f)
			{
				grcDebugDraw::Sphere(m_outTargetPosition, 2.0f, Color_green, false, -1);
				grcDebugDraw::Line(m_vehicle->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(m_outTargetPosition), Color_orange2, -1);
			}
		}
#endif
		outTargetPosition = m_outTargetPosition;	
	}
}

//main entry for flying vehicle avoidance
bool CFlyingVehicleAvoidanceManager::SteerAvoidCollisions(Vector3& outTargetVelocity, bool& preventYaw, const Vector3& in_currentTargetVelocity, const Vector3& targetPos)
{
	TUNE_GROUP_BOOL(FLYING_AVOIDANCE, USE_OLD_AVOIDANCE, false);
	if(USE_OLD_AVOIDANCE)
	{
		//keeping old system around for now
		SteerDesiredVelocity_Old(outTargetVelocity, RegdVeh(m_vehicle), in_currentTargetVelocity, m_fAvoidanceScalar);
	}
	else
	{
		m_currentTargetVelocity = in_currentTargetVelocity;
		m_outTargetVelocity = outTargetVelocity;
		m_currentTargetPos = targetPos;
		m_closeVehicles.clear();

		if(m_vehicle->InheritsFromPlane())
		{
			UpdatePlaneAvoidance(preventYaw);
		}
		else if(m_vehicle->InheritsFromBlimp())
		{
			//we're a blimp....lets hope other flying vehicles avoid us!
		}
		else if(m_vehicle->InheritsFromHeli())
		{
			UpdateHeliAvoidance(preventYaw);	
		}

		outTargetVelocity = m_outTargetVelocity;
	}

	return outTargetVelocity.Dist2(in_currentTargetVelocity) > FLOAT_EPSILON;
}

//helis can move in 3axis, so we'll be setting a desired velocity that we want them to go in depending on their local state
void CFlyingVehicleAvoidanceManager::UpdateHeliAvoidance(bool& preventYaw)
{
	preventYaw = false;

	if(m_fOutWayTimer > 0.0f)
	{
		m_fOutWayTimer -= fwTimer::GetTimeStep();
		m_outTargetVelocity = m_holdTargetVelocity;
		preventYaw = true;
		return;
	}

	float boundRadius = m_vehicle->GetBoundRadius();
	boundRadius = square(boundRadius * 4 * m_fAvoidanceScalar * 2);

	//B* 2114685, include planes. Player could be flying one in which case we have to do the avoiding ourself
	int closeCount = GetCloseVehicles(m_vehicle, boundRadius, m_closeVehicles, false);

	if(closeCount > 0)
	{
#if __BANK
		if (s_drawAvoid)
		{
			Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetPosition());
			char text[32];
			sprintf(text, "Num Close: %d",closeCount);
			grcDebugDraw::Text(vMyVehiclePosition, Color_orange2, 0, 10, text);
			grcDebugDraw::Circle(vMyVehiclePosition, Sqrtf(boundRadius), Color_green, Vector3(1.f,0.f,0.f), Vector3(0.f,1.f,0.f), false, false, -1);
			for(int i = 0; i < m_closeVehicles.size(); ++i)
			{	
				grcDebugDraw::Line(VECTOR3_TO_VEC3V(vMyVehiclePosition), m_closeVehicles[i].otherVehicle->GetTransform().GetPosition(), Color_orange2, -1);
			}
		}
#endif

		float velocityScaler = 1.0f;
		bool weAggressor;
		bool reactingToVehicle = false;
		int collideIndex = CheckFutureCollisions( weAggressor);
		if( collideIndex >= 0 )
		{
			//we're going to collide with a vehicle soon, better do something about it		
			reactingToVehicle = SteerAvoidVehicle(*m_closeVehicles[collideIndex].otherVehicle, weAggressor, preventYaw, velocityScaler);
		}

		if(!reactingToVehicle)
		{
			//check if we suddenly want to go opposite direction
			bool hasSteeringChange = SteerLargeTargetChange();

			if(!hasSteeringChange)
			{
				hasSteeringChange = SteerAvoidOthers(preventYaw );

				if(!hasSteeringChange)
				{
					//few vehicles nearby; steer to avoid the closest ahead
					SteerDesiredVelocity();
				}
			}
		}

		//adjust target speed by a scaler set when we are avoiding close vehicles
		//so we try and move to a better location, but also slow down if we need to
		m_outTargetVelocity *= velocityScaler;
	}
}

//We have a vehicles that's very close to us, or in our way, in this case we really just want to slow down and let it be
bool CFlyingVehicleAvoidanceManager::SteerLargeTargetChange()
{
	//This needs to be target position
	float currentSpeed = m_vehicle->GetAiXYSpeed();
	Vector3 ourDirection = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetForward());
	Vector3 toTarget = m_currentTargetPos - VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetPosition());
	toTarget.Normalize();
	float directionDot = ourDirection.Dot(toTarget);
	if( directionDot < 0.0f && m_currentTargetVelocity.Mag2() > 100.0f && currentSpeed < s_fMinSpeedFlockAvoid)
	{
		//in this case - allow us to keep moving, but only slowly - the vehicle AI will turn us and then we can move again
		m_outTargetVelocity = m_currentTargetVelocity;
		m_outTargetVelocity.Normalize();
		m_outTargetVelocity *= s_fMinSpeedFlockAvoid;
		return true;
	}

	return false;
}

//determine if we're likely to hit another vehicle in the future
int CFlyingVehicleAvoidanceManager::CheckFutureCollisions(bool& usAggresor)
{
	TUNE_GROUP_BOOL(FLYING_AVOIDANCE, FUTURECOLLISIONGETOUTWAY, true);

	int foundIndex = -1;

	Vector3 vDirection;
	vDirection.Normalize(m_vehicle->GetVelocity());
	Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetPosition());
	float boundRadius = m_vehicle->GetBoundRadius();

	for(int i = 0; i < m_closeVehicles.GetCount(); i++)
	{
		float velDiff = (m_vehicle->GetVelocity() - m_closeVehicles[i].otherVehicle->GetVelocity()).Mag2();
		if(velDiff > s_fMinSpeedFlockAvoid*s_fMinSpeedFlockAvoid )
		{
			//we have a high speed difference - we might need to do some aggressive avoidance
				
			Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_closeVehicles[i].otherVehicle->GetTransform().GetPosition());
			Vector3 vOtherVehicleFuturePos = vOtherVehiclePosition + ( m_closeVehicles[i].otherVehicle->GetVelocity() * s_fLookAheadTime );
			Vector3 vFuturePosition = vMyVehiclePosition + ( m_vehicle->GetVelocity() * s_fLookAheadTime );

#if __BANK
			if (s_drawAvoid)
			{
				grcDebugDraw::Sphere(vOtherVehicleFuturePos, 0.1f, Color_red, false, -1);
				grcDebugDraw::Sphere(vFuturePosition, 0.1f, Color_blue, false, -1);
			}
#endif
		
			if(m_vehicle->GetAiXYSpeed() > 2.0f)
			{
#if __BANK
				if (s_drawAvoid)
				{
					grcDebugDraw::Line(VECTOR3_TO_VEC3V(vMyVehiclePosition), VECTOR3_TO_VEC3V(vFuturePosition), Color_green, -1);
				}
#endif

				//check our movement against their future position
				Vector3 toFuturePos =  vFuturePosition - vMyVehiclePosition;
				const float vTValue = geomTValues::FindTValueOpenSegToPoint(vMyVehiclePosition, toFuturePos, vOtherVehicleFuturePos);
				if ( vTValue > 0.0f && vTValue < 1.0f)
				{
					Vector3 vClosestPoint = vMyVehiclePosition + (toFuturePos * vTValue);
					Vector3 vCenterToClosestDir = vClosestPoint - vOtherVehicleFuturePos;
					float distance2 = vCenterToClosestDir.Mag2();
					if ( distance2 > FLT_EPSILON  && distance2 < boundRadius*boundRadius)
					{
#if __BANK
						if (s_drawAvoid)
						{
							grcDebugDraw::Sphere(vClosestPoint, 0.1f, Color_green, false, -1);
						}
#endif

						//we are going to hit
						usAggresor = true;
						foundIndex = i;
						break;							
					}
				}
			}
			else if(FUTURECOLLISIONGETOUTWAY)
			{
#if __BANK
				if (s_drawAvoid)
				{
					grcDebugDraw::Line(VECTOR3_TO_VEC3V(vOtherVehiclePosition), VECTOR3_TO_VEC3V(vOtherVehicleFuturePos), Color_red, -1);
				}
#endif

				//check their future movement against our position
				Vector3 toFuturePos = vOtherVehicleFuturePos - vOtherVehiclePosition;
				const float vTValue = geomTValues::FindTValueOpenSegToPoint(vOtherVehiclePosition, toFuturePos, vFuturePosition);
				if ( vTValue > 0.0f && vTValue < 1.0f)
				{
					Vector3 vClosestPoint = vOtherVehiclePosition + (toFuturePos * vTValue);
					Vector3 vCenterToClosestDir = vClosestPoint - vFuturePosition;
					float distance2 = vCenterToClosestDir.Mag2();
					if ( distance2 > FLT_EPSILON  && distance2 < boundRadius*boundRadius)
					{
#if __BANK
						if (s_drawAvoid)
						{
							grcDebugDraw::Sphere(vClosestPoint, 0.1f, Color_red, false, -1);
						}
#endif
						//we are going to hit
						usAggresor = false;
						foundIndex = i;
						break;							
					}
				}
			}

// 				Vector3 futurePosDirection = vFuturePosition - vOtherVehicleFuturePos;
// 				float futureDistance2 = futurePosDirection.Mag2();
// 				futurePosDirection.Normalize();
// 				float futurePosDot = futurePosDirection.Dot(vDirection);
// 
// 				bool bAheadAndVeryClose = futureDistance2 < boundRadius && (FUTURECOLLISIONGETOUTWAY || futurePosDot > 0.0f);
//				float fClosestThreshold2 = square(boundRadius * 2);
// 				//check they are close, and ahead of us within travel cone
// 				if (bAheadAndVeryClose || (futureDistance2 < fClosestThreshold2 && (futurePosDot > 0.0f && futurePosDot <= cos(s_fSightConeAngle*DtoR))))
// 				{
// 					usAggresor = m_vehicle->GetAiXYSpeed() > 2.0f;
// 					foundIndex = i;
// 					break;
// 				}
		}
	}

	return foundIndex;
}


//We have a vehicles that's very close to us, or in our way, in this case we really just want to slow down and let it be
bool CFlyingVehicleAvoidanceManager::SteerAvoidVehicle(const CVehicle& in_OtherVehicle, bool usAggressor, bool& preventYaw, float& velocityScaler)
{
	if(usAggressor)
	{
		//we are the ones moving fast, we should slow down
		//m_outTargetVelocity *= velocityScaler;

		float boundRadius = m_vehicle->GetBoundRadius();

		float toTargetDist = VEC3V_TO_VECTOR3(in_OtherVehicle.GetTransform().GetPosition() - m_vehicle->GetTransform().GetPosition()).Mag();
		velocityScaler = RampValue(toTargetDist, boundRadius, boundRadius * 4 * m_fAvoidanceScalar, 0.0f, 1.0f );

		//check case where both vehicles are facing each other and not moving
		Vector3 theirDirection = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetForward());
		if(theirDirection.Dot(m_currentTargetVelocity) < 0.0f)
		{
			if( (m_vehicle->GetAiXYSpeed() * in_OtherVehicle.GetAiXYSpeed()) < 1.0f)
			{
				//move to the right
				m_outTargetVelocity = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetRight());
				m_outTargetVelocity.z = 0.0f;
				return true;
			}
		}
	}
	else
	{
		//they are coming at us fast - we need to get out the way!
		//for now, just steer away from vehicle in shortest normal
		Vector3 ourFowardVel;
		ourFowardVel.Normalize(m_vehicle->GetVelocity());
		Vector3 toTarget = VEC3V_TO_VECTOR3(in_OtherVehicle.GetTransform().GetPosition() - m_vehicle->GetTransform().GetPosition());
		Vector3 toTargetRight(toTarget.y, -toTarget.x, 0.0f);
		toTarget.Normalize();	
		m_outTargetVelocity = toTargetRight * 10.0f;
		if(ourFowardVel.Dot(toTargetRight) < 0.0f)
		{
			m_outTargetVelocity *= -1.0f;
		}

		m_outTargetVelocity.z = 0.0f;

		preventYaw = true;
		m_fOutWayTimer = s_fGetOutWayTime;
		m_holdTargetVelocity = m_outTargetVelocity;

		return true;

		//TODO - we could set our target position using FindTargetVelocityAvoidingPosition
		//to move us to point away line at bounds radius
	}

	return false;
}


//steers away from all local vehicles, value returned is new target position which is average of avoidance of all nearby vehicles
//in most cases, we'll want to slow down the vehicle alot until the nearby vehicles have gone away
//vehicles that rely on forward thrust cannot use this (i.e planes)
bool CFlyingVehicleAvoidanceManager::SteerAvoidOthers(bool& preventYaw)
{
	TUNE_GROUP_FLOAT(FLYING_AVOIDANCE, AVOIDOTHERBOUNDRADIUS, 2.5f, 0.0f, 10.0, 0.1f);
	TUNE_GROUP_BOOL(FLYING_AVOIDANCE, AVOIDOTHERALSOSLOW, true);
	
	preventYaw = false;

	Vector3 toTarget = m_currentTargetPos - VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetPosition());
	if( m_vehicle->GetAiXYSpeed() < s_fMinSpeedFlockAvoid && toTarget.Mag2() < 100.0f)
	{
		Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetPosition());
		float boundRadius = m_vehicle->GetBoundRadius();

		Vector3 desiredVelocity = Vector3(Vector3::ZeroType);

		int groupCount = 0;
		float closestDistance = FLT_MAX;
		for(int i = 0; i < m_closeVehicles.GetCount(); i++)
		{
			//if(!AVOIDOTHERALSOSLOW || m_closeVehicles[i].otherVehicle->GetAiXYSpeed() < s_fMinSpeedFlockAvoid )
			{
				float currentDist2 = m_closeVehicles[i].distSqr;
				if ( currentDist2 < square(boundRadius * AVOIDOTHERBOUNDRADIUS)  )
				{		
					closestDistance = Min(closestDistance, currentDist2);
					Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_closeVehicles[i].otherVehicle->GetTransform().GetPosition());			
					Vector3 vMeToOtherVehicle = vOtherVehiclePosition - vMyVehiclePosition;
					vMeToOtherVehicle.Normalize();

					//float dotVelocity = m_currentTargetVelocity.Dot(vMeToOtherVehicle);
					//if(dotVelocity > QUARTER_PI)
					{
						//close vehicles have more force
						float distActual = Sqrtf(currentDist2) - boundRadius;
						float scale = s_fAvoidDistMultiplier/(Max(1.0f, distActual));
						vMeToOtherVehicle.Scale(Max(scale, 1.0f));
						desiredVelocity -= vMeToOtherVehicle;
						++groupCount;
#if __BANK
						if (s_drawAvoid)
						{
							grcDebugDraw::Line(VECTOR3_TO_VEC3V(vMyVehiclePosition), VECTOR3_TO_VEC3V(vOtherVehiclePosition), Color_red, -1);
							grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vMyVehiclePosition), VECTOR3_TO_VEC3V(vMyVehiclePosition + vMeToOtherVehicle), 2, Color_blue, -1);
						}
#endif
					}
				}
			}
		}

		if( groupCount > 0)
		{
			desiredVelocity /= (float)groupCount;

			//lerp with target velocity based on distance to closest vehicle
//			float targetRatio = RampValue(Sqrtf(closestDistance), boundRadius, boundRadius * 2, 1.0f, 0.0f);
// 			desiredVelocity = desiredVelocity * ( 1.0f - targetRatio) + m_currentTargetVelocity * targetRatio;

			//we only want slow movement when avoiding others
			//desiredVelocity.Normalize();
			//scale out velocity by factor of the closest vehicle to us
			//float scaler = RampValue(sqrt(closestDistance),boundRadius, square(boundRadius * AVOIDOTHERBOUNDRADIUS), 1.0f, 0.0f);
			//desiredVelocity *= scaler;
			m_outTargetVelocity = desiredVelocity;
#if __BANK
			if (s_drawAvoid)
			{
				grcDebugDraw::Circle(vMyVehiclePosition, boundRadius * AVOIDOTHERBOUNDRADIUS, Color_blue, Vector3(1.f,0.f,0.f),Vector3(0.f,1.f,0.f), false, false, -1);
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vMyVehiclePosition), VECTOR3_TO_VEC3V(vMyVehiclePosition + m_outTargetVelocity), 2, Color_green, -1);
			}
#endif

			//we aren't trying to go to a target here, but are avoiding a position
			//so we don't want to turn to position, we just want to make the vehicle start moving in that direction
			preventYaw = true; //targetRatio < 0.25f;
			return true;
		}	
	}

	return false;
}

//sets a new target position that touches edge of bounding circle around closest entity ahead of us in direction of original target
//only handles avoiding single vehicle; used when we've no nearby vehicles, but want to avoid those ahead of us premptivly
bool CFlyingVehicleAvoidanceManager::SteerDesiredVelocity()
{
	bool steeringAdjusted = false;
	TUNE_GROUP_BOOL(FLYING_AVOIDANCE, STEER_USE_POSITIONS, true);

	m_outTargetVelocity = m_currentTargetVelocity;
	float fSpeed = m_currentTargetVelocity.Mag();
	if ( fSpeed >= s_fMinSpeedFlockAvoid )
	{
		Vector3 vDirection = m_currentTargetVelocity * (1 / fSpeed);
		Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(m_vehicle->GetTransform().GetPosition());

		float fClosestThreshold2 = FLT_MAX;

		for(int i = 0; i < m_closeVehicles.GetCount(); i++)
		{
			if ( m_closeVehicles[i].distSqr < fClosestThreshold2 )
			{
				fClosestThreshold2 = m_closeVehicles[i].distSqr;
				float boundRadius = 0.0f;
				if(m_vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
				{
					boundRadius = (m_vehicle->GetBoundRadius() + m_closeVehicles[i].otherVehicle->GetBoundRadius()) * 2.0f;
				}
				else
				{
					boundRadius = m_vehicle->GetBoundRadius() + (m_closeVehicles[i].otherVehicle->GetBoundRadius() * 0.5f);
				}
				
				if(STEER_USE_POSITIONS)
				{
					//steers away from vehicles in our current direction of travel

					Vector3 vFuturePosition = vMyVehiclePosition + vDirection * sqrt(m_closeVehicles[i].distSqr);
					Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_closeVehicles[i].otherVehicle->GetTransform().GetPosition());

					bool bAdjusted = FindTargetVelocityAvoidingPosition(m_outTargetVelocity, vMyVehiclePosition, vFuturePosition, vOtherVehiclePosition, boundRadius);
					if(bAdjusted)
					{
						//planes use a target position to steer to, not a velocity, push target position to edge of radius around avoided vehicle
						if(m_vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
						{
							float fDistToCurrentTarget = (vOtherVehiclePosition - vMyVehiclePosition).Mag();
							m_outTargetPosition = vMyVehiclePosition + (m_outTargetVelocity * (fDistToCurrentTarget + 5.0f));
						}

						//push new direction away from us by our desired speed
						m_outTargetVelocity *= fSpeed;
						steeringAdjusted = true;
					}
				}
				else
				{
					//steers away from vehicles in our desired direction of travel
					Vector3 vFuturePosition = vMyVehiclePosition + ( m_vehicle->GetVelocity() * s_fLookAheadTime );
					Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_closeVehicles[i].otherVehicle->GetTransform().GetPosition());

					bool bAdjusted = FindTargetVelocityAvoidingPosition(m_outTargetVelocity, vFuturePosition, m_currentTargetVelocity, vOtherVehiclePosition, boundRadius);
					if(bAdjusted)
					{
						m_outTargetVelocity *= fSpeed;
						steeringAdjusted = true;
					}
				}
			}
		}
	}

	return steeringAdjusted;
}


//planes in VTOL can avoid the same as helicopters, otherwise they only have forward thrust, so can only avoid vehicles ahead
void CFlyingVehicleAvoidanceManager::UpdatePlaneAvoidance(bool& preventYaw)
{
	//Assertf(false,"This code has not been tested at all. It's up to you to test it before using it!");

	preventYaw = false;
	float boundRadius = m_vehicle->GetBoundRadius();
	boundRadius = square(boundRadius * 4 * m_fAvoidanceScalar);

	m_closeVehicles.clear();

	CPlane* pPlane = static_cast<CPlane*>(m_vehicle);
	if (pPlane->GetVerticalFlightModeAvaliable() && pPlane->GetVerticalFlightModeRatio() >= 1.0f)
	{
		//in VTOL mode
		GetCloseVehicles(m_vehicle, boundRadius, m_closeVehicles, false);

		//check soon to happen collisions
		bool weAggressor;
		int collideIndex = CheckFutureCollisions(weAggressor);
		if( collideIndex >= 0 )
		{
			float tmp;
			SteerAvoidVehicle(*m_closeVehicles[collideIndex].otherVehicle, weAggressor, preventYaw, tmp);
		}
		else
		{
			//no dangerous collisions - just stay wary of nearby vehicles
			SteerAvoidOthers(preventYaw);
		}
	}
	else
	{
		//TODO: Planes will only avoid player planes at the moment as risky enabling full avoidance
		GetCloseVehicles(m_vehicle, boundRadius, m_closeVehicles, false, true);

		//non-VTOL, only has forward thrust, so have to adjust target position
		//and we'll be going fast - so best just to avoid vehicles we're going to collide with as last resort 
		bool weAggressor;
		int collideIndex = CheckFutureCollisions(weAggressor);
		if( collideIndex >= 0 )
		{				
			SteerDesiredVelocity();
		}	
	}
}

//return normalized direction to a point on circle with radius of avoidRadius around avoidPoint 
//that is tangential to us and closest to direction of endPosition - startPosition
bool CFlyingVehicleAvoidanceManager::FindTargetVelocityAvoidingPosition(Vector3& outDirection, const Vector3& startPosition, 
																		const Vector3& endPosition, const Vector3& avoidPoint, float avoidRadius)
{
	Vector3 toFuturePos = endPosition - startPosition;
	const float vTValue = geomTValues::FindTValueOpenSegToPoint(startPosition, toFuturePos, avoidPoint);
	if ( vTValue > 0.0f ) //point is ahead of us
	{
		//gets point on line from start to end closest to avoid point
		Vector3 vClosestPoint = startPosition + (toFuturePos * vTValue);
		//offset from avoidPoint to point on line
		Vector3 vCenterToClosestDir = vClosestPoint - avoidPoint;
		float distance2 = vCenterToClosestDir.Mag2();
		if ( distance2 > FLT_EPSILON  && distance2 < avoidRadius*avoidRadius) //check the closest point is within our checking radius
		{
			float distance = sqrt(distance2);
			vCenterToClosestDir *= 1/distance;

			//push point on line away from avoidpoint to distance of avoidRadius
			Vector3 vTargetOffset = avoidPoint + vCenterToClosestDir * avoidRadius; 
			//get offset from us to that point
			Vector3 vAwayFromTarget = vTargetOffset - startPosition;

#if __BANK
			if (s_drawAvoid)
			{
				grcDebugDraw::Circle(avoidPoint, avoidRadius, Color_green, Vector3(1.f,0.f,0.f),Vector3(0.f,1.f,0.f), false, false, -1);
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(avoidPoint), VECTOR3_TO_VEC3V(vTargetOffset), 0.2f, Color_blue, -1);
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(startPosition), VECTOR3_TO_VEC3V(vTargetOffset), 0.2f, Color_red, -1);
			}
#endif
			//don't allow us to go down
			vAwayFromTarget.z = Max(vAwayFromTarget.z, 0.0f);

			//normalize final result
			outDirection.Normalize(vAwayFromTarget);

			return true;
		}
	}

	return false;
}

int CFlyingVehicleAvoidanceManager::GetCloseVehicles(const CVehicle* currentVehicle, float radius, atVector<CloseVehicle>& out_closeVehicles, bool excludeJets, bool bOnlyPlayers)
{
	int count = 0;

	for(int i = 0; i < m_Vehicles.GetCount(); i++)
	{
		if (m_Vehicles[i] && m_Vehicles[i] != currentVehicle && m_Vehicles[i]->IsInAir())
		{
			if( !excludeJets || !m_Vehicles[i]->InheritsFromPlane())
			{
				if(!bOnlyPlayers || !m_Vehicles[i]->GetDriver() || m_Vehicles[i]->GetDriver()->IsPlayer())
				{
					Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_Vehicles[i]->GetTransform().GetPosition());
					Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(currentVehicle->GetTransform().GetPosition());
					Vector3 vMeToOtherVehicle = vOtherVehiclePosition - vMyVehiclePosition;
					float fDistance2 = vMeToOtherVehicle.Mag2();
					if ( fDistance2 < radius )
					{
						++count;
						out_closeVehicles.push_back(CloseVehicle(m_Vehicles[i], fDistance2));
					}
				}
			}
		}
	}

	return count;
}

int CFlyingVehicleAvoidanceManager::CountCloseVehicles(const RegdVeh& currentVehicle, float radius)
{
	int count = 0;
	for(int i = 0; i < m_Vehicles.GetCount(); i++)
	{
		if (m_Vehicles[i] && m_Vehicles[i] != currentVehicle && m_Vehicles[i]->IsInAir())
		{
			Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_Vehicles[i]->GetTransform().GetPosition());
			Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(currentVehicle->GetTransform().GetPosition());
			Vector3 vMeToOtherVehicle = vOtherVehiclePosition - vMyVehiclePosition;
			float fDistance2 = vMeToOtherVehicle.Mag2();
			if ( fDistance2 < radius )
			{
				++count;
			}
		}
	}

	return count;
}

void CFlyingVehicleAvoidanceManager::SteerDesiredVelocity_Old(Vector3& o_Velocity, const RegdVeh& in_Vehicle, const Vector3& in_Velocity, float in_fAvoidanceScalar)
{
	o_Velocity = in_Velocity;
	float fSpeed = in_Velocity.Mag();
	if ( fSpeed >= SMALL_FLOAT )
	{
		Vector3 vDirection = in_Velocity * (1 / fSpeed);
		Vector3 vMyVehiclePosition = VEC3V_TO_VECTOR3(in_Vehicle->GetTransform().GetPosition());
		float boundRadius = in_Vehicle->GetBoundRadius();

		float fClosestThreshold2 = square(boundRadius * 4 * in_fAvoidanceScalar);

		for(int i = 0; i < m_Vehicles.GetCount(); i++)
		{
			if (m_Vehicles[i] && m_Vehicles[i] != in_Vehicle )
			{
				Vector3 vOtherVehiclePosition = VEC3V_TO_VECTOR3(m_Vehicles[i]->GetTransform().GetPosition());
				Vector3 vMeToOtherVehicle = vOtherVehiclePosition - vMyVehiclePosition;
				float fDistance2 = vMeToOtherVehicle.Mag2();
				if ( fDistance2 < fClosestThreshold2 )
				{
					fClosestThreshold2 = fDistance2;

					Vector3 vFuturePosition = vMyVehiclePosition + vDirection * fDistance2;
					Vector3 vToFuturePosition = vFuturePosition - vMyVehiclePosition;

					const float vTValue = geomTValues::FindTValueOpenSegToPoint(vMyVehiclePosition, vToFuturePosition, vOtherVehiclePosition);
					if ( vTValue > 0.0f )
					{
						Vector3 vClosestPoint = vMyVehiclePosition + (vToFuturePosition * vTValue); 
						Vector3 vCenterToClosestDir = vClosestPoint - vOtherVehiclePosition;
						float distance2 = vCenterToClosestDir.Mag2();
						if ( distance2 > FLT_EPSILON && distance2 < boundRadius*boundRadius )
						{
							float distance = sqrt(distance2);
							vCenterToClosestDir *= 1/distance;

							Vector3 vTargetOffset = vOtherVehiclePosition + vCenterToClosestDir * boundRadius; 
							Vector3 vAwayFromTarget = vTargetOffset - vMyVehiclePosition;
							vAwayFromTarget.z = Max(vAwayFromTarget.z, 0.0f);
							vDirection.Normalize(vAwayFromTarget);
						}
					}
				}
			}
		}
		o_Velocity = vDirection* fSpeed;
	}
}

//handles multiple vehicles that have the same target destination by pushing out destinations from the desired position
void CFlyingVehicleAvoidanceManager::SlideDestinationTarget(Vector3& o_Target, const RegdVeh& m_vehicle, const Vector3& in_Target, float in_fAvoidanceScalar, bool addTargetToList)
{
	static float s_fTargetAvoidanceBoundScalar = 4;
	static float s_fTargetAvoidanceStrengthScalar = 3;

	o_Target = in_Target;
	Vector3 vRepulsion = Vector3(0.0f, 0.0f, 0.0f);
	float boundRadius = m_vehicle->GetBoundRadius();
	float count = 0;

	for(int i = 0; i < m_Destinations.GetCount(); i++)
	{
		Vector3 vDestinationToDestination = in_Target - m_Destinations[i];
		vDestinationToDestination.z = Max(vDestinationToDestination.z * 1.5f, 0.0f); // don't allow negative z and add additional + z to compensate
		if ( vDestinationToDestination.Mag2() <= FLT_EPSILON )
		{
			//exact same destination as us, so offset in random direction
			vDestinationToDestination = RandomDirections[i];
		}

		//push destination away from other destinations
		float fDistanceToMe2 = vDestinationToDestination.Mag2();
		if ( fDistanceToMe2 < square(boundRadius*s_fTargetAvoidanceBoundScalar))
		{
			float fDistanceToMe = sqrt(Max(fDistanceToMe2, 0.01f));
			float fBaseStrength = boundRadius * s_fTargetAvoidanceStrengthScalar * in_fAvoidanceScalar;
			float fStrength = fBaseStrength / fDistanceToMe;
			vRepulsion += vDestinationToDestination * fStrength;
			count += 1.0f;
		}
	}

	if ( count > 0)
	{
		vRepulsion *= 1.0f / count;
		o_Target += vRepulsion;
	}

	//this is cleared every update
	if(addTargetToList && m_Destinations.GetAvailable())
	{
		m_Destinations.Push(in_Target);
	}
}

void CFlyingVehicleAvoidanceManager::RemoveVehicle(const RegdVeh& vehicle)
{
	int index = m_Vehicles.Find(vehicle);
	if(index != -1)
	{
		m_Vehicles.DeleteFast(index);
	}
}

void CFlyingVehicleAvoidanceManager::AddVehicle(const RegdVeh& vehicle)
{
	if(m_Vehicles.GetAvailable() && m_Vehicles.Find(vehicle) == -1)
	{
		m_Vehicles.Push(vehicle);
	}

	//	Assertf(m_Vehicles.GetAvailable(), "Run out of space in flying vehicle avoidance. You might want to consider increasing number of vehicles");

#if __DEV
	if(!m_Vehicles.GetAvailable())
	{
		Warningf("Run out of space in flying vehicle avoidance. You might want to consider increasing number of vehicles");
	}
#endif
}

void CFlyingVehicleAvoidanceManager::Update()
{
	for(int i = 0; i < m_Vehicles.GetCount(); )
	{
		if (!m_Vehicles[i] )
		{	
			m_Vehicles.DeleteFast(i);
		}
		else
		{
			i++;
		}
	}

	m_Destinations.clear();
}