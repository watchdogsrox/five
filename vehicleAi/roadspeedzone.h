/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    roadspeedzone.h
// PURPOSE : Contains the zones on the map that slow cars down.
// AUTHOR :  JMart.
// CREATED : 13/7/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _ROADSPEEDZONE_H_
#define _ROADSPEEDZONE_H_

#include "spatialdata/sphere.h"
#include "atl/array.h"

class CRoadSpeedZone
{
public:
	CRoadSpeedZone()
	{
		Invalidate();
	}
	void Invalidate()
	{
		//an uninitialized zone shouldn't bound our speed at all
		m_fMaxSpeed = FLT_MAX;
		m_bValid = false;
		m_bAllowAffectMissionVehs = false;
	}
	spdSphere m_Sphere;
	float m_fMaxSpeed;
	bool m_bAllowAffectMissionVehs;
	bool m_bValid;
};

class CRoadSpeedZoneManager
{
public:
	enum {MAX_ROAD_SPEED_ZONES=220};
	CRoadSpeedZoneManager();

	s32 AddZone(spdSphere& sphere, float fMaxSpeed, bool bAllowAffectMissionVehs);
	s32 AddZone(Vector3& vSphereCenter, float fSphereRadius, float fMaxSpeed, bool bAllowAffectMissionVehs);

	bool RemoveZone(u32 index);

	float FindMaxSpeedForThisPosition(const Vector3& vSearchPosition, bool bIsMissionVeh) const;

#if __BANK
	void DebugDraw() const;
	static void InitWidgets(bkBank& bank);
	static bool ms_bEnableDebugDraw;
#endif //__BANK

	static CRoadSpeedZoneManager& GetInstance() { return ms_instance; }
private:
	atRangeArray<CRoadSpeedZone, MAX_ROAD_SPEED_ZONES> m_RoadSpeedZones;
	int m_ValidZoneLimit; // all valid zones are guaranteed to be in the range [0, m_ValidZoneLimit)
	static CRoadSpeedZoneManager ms_instance;
};

#endif //_ROADSPEEDZONE_H_