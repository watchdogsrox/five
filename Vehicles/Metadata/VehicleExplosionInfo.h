// Title	:	VehicleExplosionInfo.h
// Author	:	Stefan Bachman
// Started	:	18/03/2013

#ifndef _VEHICLE_EXPLOSION_INFO_H_
#define _VEHICLE_EXPLOSION_INFO_H_

#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "vector/vector3.h"
#include "vectormath/classes.h"

#include "Weapons/WeaponEnums.h"

class CVehicleExplosionLOD
{
public:
	float GetRadius() const { return m_Radius; }
	float GetRadiusSq() const { return m_Radius*m_Radius; }
	float GetPartDeletionChance() const 
	{ 
#if __BANK
		if(ms_EnablePartDeletionChanceOverride)
		{
			return ms_PartDeletionChanceOverride;
		}
		else
#endif // __BANK
		{
			return m_PartDeletionChance; 
		}
	}

#if __BANK
	static float ms_PartDeletionChanceOverride;
	static bool ms_EnablePartDeletionChanceOverride;
#endif // __BANK

private:
	float m_Radius; // The radius at which this LOD takes effect
	float m_PartDeletionChance; // 0 to 1 value representing the chance a vehicle part gets deleted rather than broken off by an explosion at this LOD

	PAR_SIMPLE_PARSABLE
};

struct CExplosionData
{
	eExplosionTag m_ExplosionTag;
	bool m_PositionAtPetrolTank;
	bool m_PositionInBoundingBox;
	Vec3V m_PositionOffset;
	u32 m_DelayTimeMs;
	float m_Scale;

	PAR_SIMPLE_PARSABLE;
};

class CVehicleExplosionInfo
{
public:
	// NOTE: If you change this number you need to update VehicleExplosionInfo.psc
	static const int sm_NumVehicleExplosionLODs = 3;

	static int GetNumVehicleExplosionLODs() { return sm_NumVehicleExplosionLODs; }
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// explosions
	const int GetNumExplosions() const {return m_ExplosionData.GetCount();}
	const CExplosionData* GetExplosion(int idx) const {return &m_ExplosionData[idx];}

	const CVehicleExplosionLOD& GetVehicleExplosionLODFromDistance(float distance) const;
	const CVehicleExplosionLOD& GetVehicleExplosionLODFromDistanceSq(float distance) const;
	const CVehicleExplosionLOD& GetVehicleExplosionLOD(int vehicleExplosionLODIndex) const;

	float GetAdditionalPartVelocityMinAngle() const { return m_AdditionalPartVelocityMinAngle; }
	float GetAdditionalPartVelocityMaxAngle() const { return m_AdditionalPartVelocityMaxAngle; }
	ScalarV_Out ComputeAdditionalPartVelocityAngle() const;

	float GetAdditionalPartVelocityMinMagnitude() const { return m_AdditionalPartVelocityMinMagnitude; }
	float GetAdditionalPartVelocityMaxMagnitude() const { return m_AdditionalPartVelocityMaxMagnitude; }
	ScalarV_Out ComputeAdditionalPartVelocityMagnitude() const;

	Vec3V_Out ComputeAdditionalPartVelocity(Vec3V_In explosionDirection) const;

#if __BANK
	static bool ms_DrawVehicleExplosionInfoNames;
	static bool ms_DrawVehicleExplosionInfoLodSpheres;

	static bool ms_EnableAdditionalPartVelocityOverride;
	static float ms_AdditionalPartVelocityAngleOverride;
	static float ms_AdditionalPartVelocityMagnitudeOverride;
#endif // __BANk
private:
	atHashWithStringNotFinal m_Name;
	CVehicleExplosionLOD m_VehicleExplosionLODs[sm_NumVehicleExplosionLODs];

	// explosion tag
	atArray<CExplosionData> m_ExplosionData;

	// Parameters for the additional impulse we apply to parts breaking off through explosions
	float m_AdditionalPartVelocityMinAngle;
	float m_AdditionalPartVelocityMaxAngle;
	float m_AdditionalPartVelocityMinMagnitude;
	float m_AdditionalPartVelocityMaxMagnitude;

	PAR_SIMPLE_PARSABLE;
};

#endif // _VEHICLE_EXPLOSION_INFO_H_