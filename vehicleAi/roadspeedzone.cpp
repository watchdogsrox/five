#include "roadspeedzone.h"
#include "grcore/debugdraw.h"
#include "bank/bank.h"
#include "vector/colors.h"

#include <stdio.h>

//static initialization
CRoadSpeedZoneManager CRoadSpeedZoneManager::ms_instance;
#if __BANK
bool CRoadSpeedZoneManager::ms_bEnableDebugDraw = false;
#endif
CRoadSpeedZoneManager::CRoadSpeedZoneManager()
	: m_ValidZoneLimit(0)
{
}

s32 CRoadSpeedZoneManager::AddZone(spdSphere& sphere, float fMaxSpeed, bool bAllowAffectMissionVehs)
{
	s32 newIndex = -1;
	//look for a free space
	for (int i = 0; i < MAX_ROAD_SPEED_ZONES; i++)
	{
		if (!m_RoadSpeedZones[i].m_bValid)
		{
			//found one
			m_RoadSpeedZones[i].m_Sphere = sphere;
			m_RoadSpeedZones[i].m_fMaxSpeed = fMaxSpeed;
			m_RoadSpeedZones[i].m_bValid = true;
			m_RoadSpeedZones[i].m_bAllowAffectMissionVehs = bAllowAffectMissionVehs;

			newIndex = i;
			m_ValidZoneLimit = Max(m_ValidZoneLimit, i+1);

			break;
		}
	}

	Assertf(newIndex >= 0, "CRoadSpeedZoneManager::AddZone--No free space for new speed zone! Not adding.");

	return newIndex;
}

s32 CRoadSpeedZoneManager::AddZone(Vector3& vSphereCenter, float fSphereRadius, float fMaxSpeed, bool bAllowAffectMissionVehs)
{
	spdSphere sphere(VECTOR3_TO_VEC3V(vSphereCenter), ScalarV(fSphereRadius));
	return AddZone(sphere, fMaxSpeed, bAllowAffectMissionVehs);
}

//returns true if we found and removed a zone
bool CRoadSpeedZoneManager::RemoveZone(u32 index)
{
	if (index < MAX_ROAD_SPEED_ZONES && m_RoadSpeedZones[index].m_bValid)
	{
		m_RoadSpeedZones[index].Invalidate();

		// Recompute the "high water mark" for the valid zones
		m_ValidZoneLimit = 0;
		for(int i = MAX_ROAD_SPEED_ZONES-1; i >= 0; i--)
		{
			if (m_RoadSpeedZones[i].m_bValid)
			{
				m_ValidZoneLimit = i+1;
				break;
			}
		}


		return true;
	}

	return false;
}

float CRoadSpeedZoneManager::FindMaxSpeedForThisPosition(const Vector3 &vSearchPosition, bool bIsMissionVeh) const
{
	float fMaxSpeed = FLT_MAX;
	Vec3V searchPos = RCC_VEC3V(vSearchPosition);
	const CRoadSpeedZone* end = m_RoadSpeedZones.begin() + m_ValidZoneLimit;
	for (const CRoadSpeedZone* iter = m_RoadSpeedZones.begin(); iter != end; ++iter)
	{
		// bitwise operators here instead of boolean give fewer branches
		bool checkThisOne = (iter->m_bValid & (!bIsMissionVeh | iter->m_bAllowAffectMissionVehs));
		if (checkThisOne && iter->m_fMaxSpeed < fMaxSpeed &&
		iter->m_Sphere.ContainsPoint(searchPos))
		{
			fMaxSpeed = iter->m_fMaxSpeed;
		}
	}

	return fMaxSpeed;
}

#if __BANK

void CRoadSpeedZoneManager::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Road Speed Zones", false);
	bank.AddToggle("Enable Debug Draw", &ms_bEnableDebugDraw);
	bank.PopGroup();
}
void CRoadSpeedZoneManager::DebugDraw() const
{
	if (ms_bEnableDebugDraw)
	{
		for (int i = 0; i < MAX_ROAD_SPEED_ZONES; i++)
		{
			if (m_RoadSpeedZones[i].m_bValid)
			{
				grcDebugDraw::Sphere(m_RoadSpeedZones[i].m_Sphere.GetCenter(), m_RoadSpeedZones[i].m_Sphere.GetRadiusf(), Color_orange, false);

				char sMaxSpeed[16];
				sprintf(sMaxSpeed, "%.2f", m_RoadSpeedZones[i].m_fMaxSpeed);
				grcDebugDraw::Text(m_RoadSpeedZones[i].m_Sphere.GetCenter(), Color_WhiteSmoke, sMaxSpeed);
			}
		}
	}
}
#endif //__BANK