// Title	:	VehicleExplosionInfo.cpp
// Author	:	Stefan Bachman
// Started	:	18/03/2013

#include "VehicleExplosionInfo.h"
#include "VehicleExplosionInfo_parser.h"

// Game includes
#include "fwmaths/random.h"

VEHICLE_OPTIMISATIONS()

#if __BANK
float CVehicleExplosionLOD::ms_PartDeletionChanceOverride = 1.0f;
bool CVehicleExplosionLOD::ms_EnablePartDeletionChanceOverride = false;

bool CVehicleExplosionInfo::ms_DrawVehicleExplosionInfoNames = false;
bool CVehicleExplosionInfo::ms_DrawVehicleExplosionInfoLodSpheres = false;

bool CVehicleExplosionInfo::ms_EnableAdditionalPartVelocityOverride = false;
float CVehicleExplosionInfo::ms_AdditionalPartVelocityAngleOverride = 0.0f;
float CVehicleExplosionInfo::ms_AdditionalPartVelocityMagnitudeOverride = 10.0f;
#endif // __BANK

const CVehicleExplosionLOD& CVehicleExplosionInfo::GetVehicleExplosionLODFromDistance(float distance) const
{
	// Keep increasing the LOD index until the next LOD starts at a radius beyond the given distance
	int bestLodIndex = 0;
	while(bestLodIndex < GetNumVehicleExplosionLODs() - 1 && distance > m_VehicleExplosionLODs[bestLodIndex + 1].GetRadius())
	{
		++bestLodIndex;
	}
	return m_VehicleExplosionLODs[bestLodIndex];
}
const CVehicleExplosionLOD& CVehicleExplosionInfo::GetVehicleExplosionLODFromDistanceSq(float distanceSq) const
{
	// Keep increasing the LOD index until the next LOD starts at a radius beyond the given distance
	int bestLodIndex = 0;
	while(bestLodIndex < GetNumVehicleExplosionLODs() - 1 && distanceSq > m_VehicleExplosionLODs[bestLodIndex + 1].GetRadiusSq())
	{
		++bestLodIndex;
	}
	return m_VehicleExplosionLODs[bestLodIndex];
}

const CVehicleExplosionLOD& CVehicleExplosionInfo::GetVehicleExplosionLOD(int vehicleExplosionLODIndex) const
{
	Assert(vehicleExplosionLODIndex < GetNumVehicleExplosionLODs());
	return m_VehicleExplosionLODs[vehicleExplosionLODIndex];
}

ScalarV_Out CVehicleExplosionInfo::ComputeAdditionalPartVelocityAngle() const
{
#if __BANK
	if(ms_EnableAdditionalPartVelocityOverride)
	{
		return ScalarVFromF32(ms_AdditionalPartVelocityAngleOverride);
	}
	else
#endif // __BANK
	{
		return ScalarVFromF32(fwRandom::GetRandomNumberInRange(m_AdditionalPartVelocityMinAngle,m_AdditionalPartVelocityMaxAngle));
	}
}

ScalarV_Out CVehicleExplosionInfo::ComputeAdditionalPartVelocityMagnitude() const
{
#if __BANK
	if(ms_EnableAdditionalPartVelocityOverride)
	{
		return ScalarVFromF32(ms_AdditionalPartVelocityMagnitudeOverride);
	}
	else
#endif // __BANK
	{
		return ScalarVFromF32(fwRandom::GetRandomNumberInRange(m_AdditionalPartVelocityMinMagnitude,m_AdditionalPartVelocityMaxMagnitude));
	}
}

Vec3V_Out CVehicleExplosionInfo::ComputeAdditionalPartVelocity(Vec3V_In explosionDirection) const
{
	// Compute the angle we will rotate towards the Z-axis
	const Vec3V globalUp(V_Z_AXIS_WZERO);
	const Vec3V globalRight(V_X_AXIS_WZERO);
	const ScalarV angleToRotateTowardsGlobalUp = Scale(ComputeAdditionalPartVelocityAngle(),ScalarV(V_TO_RADIANS));
	const ScalarV angleToGlobalUp = ArccosFast(Clamp(Dot(explosionDirection,globalUp),ScalarV(V_NEGONE),ScalarV(V_ONE)));
	const ScalarV finalAngleToRotateTowardsGlobalUp = Min(angleToGlobalUp,angleToRotateTowardsGlobalUp);
	
	// Compute a matrix where the X-axis is the rotation axis and the Y-axis is the explosion direction we want to rotate
	const Vec3V rotationAxis = NormalizeSafe(Cross(explosionDirection,globalUp),globalRight);
	Mat34V explosionMatrix(rotationAxis,explosionDirection,Cross(rotationAxis,explosionDirection),Vec3V(V_ZERO));

	// Rotate about the rotation axis and use the new explosion direction as a velocity direction.
	Mat34VRotateLocalX(explosionMatrix,finalAngleToRotateTowardsGlobalUp);
	Vec3V velocityDirection = explosionMatrix.GetCol1();

	// The x-axis is now the velocity direction 
	// Compute a final velocity
	const ScalarV velocityMagnitude = ComputeAdditionalPartVelocityMagnitude();
	return Scale(velocityDirection,velocityMagnitude);
}
