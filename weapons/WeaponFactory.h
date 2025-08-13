#ifndef WEAPON_FACTORY_H
#define WEAPON_FACTORY_H

// Game headers
#include "Weapons/Weapon.h"

//////////////////////////////////////////////////////////////////////////
// WeaponFactory
//////////////////////////////////////////////////////////////////////////

namespace WeaponFactory
{
	CWeapon* Create(u32 uWeaponNameHash, s32 iAmmo = CWeapon::INFINITE_AMMO, CDynamicEntity* pDrawable = NULL, const char* debugstring = NULL, u8 tintIndex = 0);
}

#endif // WEAPON_FACTORY_H
