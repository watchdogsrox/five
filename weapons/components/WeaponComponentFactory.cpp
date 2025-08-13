//
// weapons/weaponcomponentfactory.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentFactory.h"

// Game headers
#include "Weapons/Components/WeaponComponentClip.h"
#include "Weapons/Components/WeaponComponentFlashLight.h"
#include "Weapons/Components/WeaponComponentGroup.h"
#include "Weapons/Components/WeaponComponentLaserSight.h"
#include "Weapons/Components/WeaponComponentProgrammableTargeting.h"
#include "Weapons/Components/WeaponComponentScope.h"
#include "Weapons/Components/WeaponComponentSuppressor.h"
#include "Weapons/Components/WeaponComponentVariantModel.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

namespace WeaponComponentFactory
{

CWeaponComponent* Create(const CWeaponComponentInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
{
	weaponFatalAssertf(pInfo, "pInfo is NULL");

	if(weaponVerifyf(CWeaponComponent::GetPool()->GetNoOfFreeSpaces() > 0, "Weapon component pool is full.  Size [%d]", CWeaponComponent::GetPool()->GetSize()))
	{
		switch(pInfo->GetClassId())
		{
		case WCT_Base:
			return rage_new CWeaponComponent(pInfo, pWeapon, pDrawable);

		case WCT_Clip:
			return rage_new CWeaponComponentClip(static_cast<const CWeaponComponentClipInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_FlashLight:
			return rage_new CWeaponComponentFlashLight(static_cast<const CWeaponComponentFlashLightInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_LaserSight:
			return rage_new CWeaponComponentLaserSight(static_cast<const CWeaponComponentLaserSightInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_ProgrammableTargeting:
			return rage_new CWeaponComponentProgrammableTargeting(static_cast<const CWeaponComponentProgrammableTargetingInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_Scope:
			return rage_new CWeaponComponentScope(static_cast<const CWeaponComponentScopeInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_Suppressor:
			return rage_new CWeaponComponentSuppressor(static_cast<const CWeaponComponentSuppressorInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_VariantModel:
			return rage_new CWeaponComponentVariantModel(static_cast<const CWeaponComponentVariantModelInfo*>(pInfo), pWeapon, pDrawable);

		case WCT_Group:
			return rage_new CWeaponComponentGroup(static_cast<const CWeaponComponentGroupInfo*>(pInfo), pWeapon, pDrawable);

		default:
			weaponAssertf(0, "Unhandled component type [%d]", pInfo->GetClassId());
			break;
		}
	}
#if __BANK
	else
	{
		CWeaponComponent::GetPool()->PoolFullCallback();
	}
#endif // __BANK
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

}
