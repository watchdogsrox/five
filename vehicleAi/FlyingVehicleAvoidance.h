//
// FlyingVehicleAvoidance
//

#pragma once

#include "scene/RegdRefTypes.h"
#include "atl/array.h"
#include "vectormath/vec3v.h"
#include "vector/vector3.h"
#include "atl/vector.h"

class CVehicle;

class CFlyingVehicleAvoidanceManager
{
private:
	struct CloseVehicle 
	{
		CloseVehicle():otherVehicle(0), distSqr(0.0f){}
		CloseVehicle(CVehicle* other, float dist2): otherVehicle(other), distSqr(dist2)
		{
		}

		CVehicle* otherVehicle;
		float distSqr;
	};

public:
	CFlyingVehicleAvoidanceManager();
	~CFlyingVehicleAvoidanceManager();

	void Init(CVehicle* vehicle, float avoidanceScaler /* = 1.0f */)
	{
		m_vehicle = vehicle;
		m_fAvoidanceScalar = avoidanceScaler;
	}
	
	static void SteerDesiredVelocity_Old(Vector3& o_Velocity, const RegdVeh& in_Vehicle, const Vector3& in_Velocity, float in_fAvoidanceScalar);
	static void SlideDestinationTarget(Vector3& o_Target, const RegdVeh& in_Vehicle, const Vector3& in_Target, float in_fAvoidanceScalar, bool bAddTargetToList = true);

	bool SteerAvoidCollisions(Vector3& o_Velocity, bool& preventYaw, const Vector3& in_Velocity, const Vector3& targetPos);
	void SteerAvoidCollisions(Vector3& outTargetPosition, const Vector3& in_currentTargetVelocity, const Vector3& targetPos);

	//could be useful as static
	static bool FindTargetVelocityAvoidingPosition(Vector3& outDirection, const Vector3& startPosition, const Vector3& endPosition, const Vector3& avoidPoint, float avoidRadius);
	static int CountCloseVehicles(const RegdVeh& in_Vehicle, float radius);
	static int GetCloseVehicles(const CVehicle* in_Vehicle, float radius, atVector<CloseVehicle>& out_closeVehicles, bool exludeJets, bool bOnlyPlayers = false);

protected:
	bool SteerDesiredVelocity();
	
	bool SteerAvoidOthers(bool& preventYaw);
	int CheckFutureCollisions(bool& usAggressor);
	bool SteerAvoidVehicle(const CVehicle& in_OtherVehicle,bool usAggressor, bool& preventYaw, float& velocityScaler);
	void UpdateHeliAvoidance(bool& preventYaw);
	void UpdatePlaneAvoidance(bool& preventYaw);
	bool SteerLargeTargetChange();
	
	CVehicle* m_vehicle;
	float m_fAvoidanceScalar;
	atVector<CloseVehicle> m_closeVehicles;
	Vector3 m_outTargetPosition;
	Vector3 m_outTargetVelocity;
	Vector3 m_currentTargetVelocity;
	Vector3 m_currentTargetPos;
	Vector3 m_holdTargetVelocity;
	float m_fOutWayTimer;

	//make these tunables
	static float s_fMinSpeedFlockAvoid;
	static float s_fLookAheadTime;
	static float s_fAvoidDistMultiplier;
	static float s_fSightConeAngle;
	static float s_fGetOutWayTime;

	//MANAGER STUFF
public:
	static void Update();
	static void AddVehicle(const RegdVeh& in_Vehicle);
	static void RemoveVehicle(const RegdVeh& in_Vehicle);

#define MAX_FLYING_VEHICLES_AVOIDANCE 20
#define MAX_FLYING_VEHICLE_DESTINATIONS 20
	static atFixedArray<RegdVeh, MAX_FLYING_VEHICLES_AVOIDANCE> m_Vehicles;
	static atFixedArray<Vector3, MAX_FLYING_VEHICLE_DESTINATIONS> m_Destinations;
	static bool s_drawAvoid;
};