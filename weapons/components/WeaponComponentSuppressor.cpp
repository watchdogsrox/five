//
// weapons/weaponcomponentsuppressor.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentSuppressor.h"

// Game headers
#include "Peds/Ped.h"
#include "vehicles/vehicle.h"
#include "modelInfo/vehiclemodelinfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentSuppressorInfo::CWeaponComponentSuppressorInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentSuppressorInfo::~CWeaponComponentSuppressorInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentSuppressor::CWeaponComponentSuppressor()
: m_iMuzzleBoneIdx(-1)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentSuppressor::CWeaponComponentSuppressor(const CWeaponComponentSuppressorInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: superclass(pInfo, pWeapon, pDrawable)
, m_iMuzzleBoneIdx(-1)
{
	if(GetDrawable())
	{
		m_iMuzzleBoneIdx = GetInfo()->GetMuzzleBone().GetBoneIndex(GetDrawable()->GetModelIndex());
		if(!weaponVerifyf(m_iMuzzleBoneIdx == -1 || m_iMuzzleBoneIdx < (s32)GetDrawable()->GetSkeleton()->GetBoneCount(), "Invalid m_iMuzzleBoneIdx bone index %d. Should be between 0 and %d", m_iMuzzleBoneIdx, GetDrawable()->GetSkeleton()->GetBoneCount()-1))
			m_iMuzzleBoneIdx = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentSuppressor::~CWeaponComponentSuppressor()
{
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentSuppressor::ProcessPostPreRender(CEntity* pFiringEntity)
{
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed *pPed = static_cast<CPed*>(pFiringEntity);
		if (pPed)
		{
			CVehicle* pVehicle = pPed->GetVehiclePedInside();
			if(pVehicle && (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || 
							pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
							pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || 
							pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE ||
							pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT) && GetDrawable())
			{
				//B*1753507: Don't hide suppressor if we're hanging onto the side of the vehicle
				const s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				if (pVehicle->IsSeatIndexValid(iSeatIndex))
				{
					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
					if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
					{
						return;
					}
				}
				GetDrawable()->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
			}
		}
	}
}
