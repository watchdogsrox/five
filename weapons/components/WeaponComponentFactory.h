//
// weapons/weaponcomponentfactory.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_FACTORY_H
#define WEAPON_COMPONENT_FACTORY_H

// Forward declarations
class CDynamicEntity;
class CWeapon;
class CWeaponComponent;
class CWeaponComponentInfo;

////////////////////////////////////////////////////////////////////////////////

namespace WeaponComponentFactory
{

	// PURPOSE: Create the correct type of weapon component
	CWeaponComponent* Create(const CWeaponComponentInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
};

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_FACTORY_H
