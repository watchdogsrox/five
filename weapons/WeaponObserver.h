//
// weapons/weaponobserver.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_OBSERVER_H
#define WEAPON_OBSERVER_H

// Game headers
#include "fwtl/RegdRefs.h"

class CWeaponObserver : public fwRefAwareBase
{
public:

	CWeaponObserver() {}
	virtual ~CWeaponObserver() {}

	// Notify that the weapon has changed ammo - if returns true the weapon will decrement its own ammo counter
	virtual bool NotifyAmmoChange(u32 UNUSED_PARAM(uWeaponNameHash), s32 UNUSED_PARAM(iAmmo)) { return true; }
	virtual bool NotifyTimerChange(u32 UNUSED_PARAM(uWeaponNameHash), u32 UNUSED_PARAM(uTimer)) { return true; }

	virtual const CPed* GetOwner() const { return NULL; }
};

#endif // WEAPON_OBSERVER_H
