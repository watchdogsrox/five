//
// weapons/weaponcomponentprogrammabletargeting.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentProgrammableTargeting.h"

// Game headers
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "System/ControlMgr.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Weapons/WeaponDebug.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentProgrammableTargetingInfo::CWeaponComponentProgrammableTargetingInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentProgrammableTargetingInfo::~CWeaponComponentProgrammableTargetingInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentProgrammableTargeting::CWeaponComponentProgrammableTargeting()
: m_Range(-1.f)
, m_bSetTheRange(false)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentProgrammableTargeting::CWeaponComponentProgrammableTargeting(const CWeaponComponentProgrammableTargetingInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: superclass(pInfo, pWeapon, pDrawable)
, m_Range(-1.f)
, m_bSetTheRange(false)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentProgrammableTargeting::~CWeaponComponentProgrammableTargeting()
{
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentProgrammableTargeting::Process(CEntity* pFiringEntity)
{
	if(pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer())
	{
		const CPed* pFiringPlayerPed = static_cast<const CPed*>(pFiringEntity);
		if(!pFiringPlayerPed->GetPlayerInfo()->IsAiming())
		{
			// Clear any set values if we are not aiming
			ClearSetRange();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentProgrammableTargeting::ProcessPostPreRender(CEntity* pFiringEntity)
{
	if(m_bSetTheRange)
	{
		if(pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer())
		{
			CPed* pFiringPlayerPed = static_cast<CPed*>(pFiringEntity);

			CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pFiringPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
			if(!pAimGunTask)
			{
				pAimGunTask = static_cast<CTaskAimGun*>(pFiringPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
			}

			if(pAimGunTask)
			{
				// Determine the firing vector
				Vector3 vStart;
				Vector3 vEnd;
				pAimGunTask->CalculateFiringVector(pFiringPlayerPed, vStart, vEnd);

				const s32 iFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE;
				
				// Determine if we've hit anything
				WorldProbe::CShapeTestFixedResults<> probeResult;
				WorldProbe::CShapeTestProbeDesc probeData;
				probeData.SetResultsStructure(&probeResult);
				probeData.SetStartAndEnd(vStart, vEnd);
				probeData.SetIncludeFlags(iFlags);
				probeData.SetExcludeEntity(pFiringEntity);
				probeData.SetIsDirected(true);
				WorldProbe::GetShapeTestManager()->SubmitTest(probeData);

				// Store the distance from the start position to the intersection
				m_Range = vStart.Dist(probeResult[0].GetHitDetected() ? probeResult[0].GetHitPosition() : vEnd);

				// Add on a bit more distance
				static dev_float EXTRA_DIST = 1.f;
				m_Range += EXTRA_DIST;
			}
		}

		m_bSetTheRange = false;
	}

#if DEBUG_DRAW
	if(m_Range > 0.f)
	{
		char buff[32];
		sprintf(buff, "Distance: %.2f", m_Range);
		static Vector2 v(0.525f, 0.5f);
		grcDebugDraw::Text(v, Color_red, buff);
	}
#endif // DEBUG_DRAW
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeaponComponentProgrammableTargeting::RenderDebug(CEntity* pFiringEntity) const
{
	if(m_Range > 0.f)
	{
		if(pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);

			if(pFiringPed->IsLocalPlayer())
			{
				CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
				if(!pAimGunTask)
				{
					pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
				}

				if(pAimGunTask)
				{
					// Determine the firing vector
					Vector3 vStart;
					Vector3 vEnd;
					pAimGunTask->CalculateFiringVector(pFiringPed, vStart, vEnd);

					Vector3 vRange(vEnd - vStart);
					vRange.NormalizeFast();
					vRange *= m_Range;
					vRange += vStart;

					grcDebugDraw::Sphere(vRange, 0.1f, Color_red);

					// Draw a line to the ground, so its easier to get a frame of reference
					Vector3 vLineStart(vRange);
					Vector3 vLineEnd(vLineStart);
					vLineEnd.z -= 20.0f;
					grcDebugDraw::Line(vLineStart, vLineEnd, Color_red);

					// Do a line test, and draw a sphere where it intersects the ground
					phSegment seg(vLineStart, vLineEnd);
					phIntersection isect;
					const s32 iFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;

					// Determine if we've hit anything
					if(CPhysics::GetLevel()->TestProbe(seg, &isect, NULL, iFlags))
					{
						grcDebugDraw::Sphere(RCC_VECTOR3(isect.GetPosition()), 0.1f, Color_red);
					}
				}
			}
		}
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
