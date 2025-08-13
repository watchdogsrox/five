#ifndef GADGET_H
#define GADGET_H

// Game headers
#include "Weapons/Weapon.h"

//////////////////////////////////////////////////////////////////////////
// CGadget
//////////////////////////////////////////////////////////////////////////

class CGadget : public CWeapon
{
public:

	// Construction
	CGadget(u32 uWeaponHash, s32 iAmmoTotal = INFINITE_AMMO, CDynamicEntity* pDrawable = NULL)
		: CWeapon(uWeaponHash, iAmmoTotal, pDrawable)
	{
	}

	virtual bool Fire(const sFireParams& params);

	// Called when gadgets are activated and deactivated
	// Function can return false to stop creation / deletion
	virtual bool Created(CPed* UNUSED_PARAM(pPed)) { return true; }
	virtual bool Deleted(CPed* UNUSED_PARAM(pPed)) { return true; }
};

#endif // GADGET_H
