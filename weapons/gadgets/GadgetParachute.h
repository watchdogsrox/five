#ifndef GADGET_PARACHUTE_H
#define GADGET_PARACHUTE_H

// Game headers
#include "Weapons/Gadgets/Gadget.h"

//////////////////////////////////////////////////////////////////////////
// CGadgetParachute
//////////////////////////////////////////////////////////////////////////

class CGadgetParachute : public CGadget
{
public:

	// Construction
	CGadgetParachute(u32 uWeaponHash, s32 iAmmoTotal = INFINITE_AMMO, CDynamicEntity* pDrawable = NULL)
		: CGadget(uWeaponHash, iAmmoTotal, pDrawable)
	{
	}

	virtual bool Created(CPed* pPed);
	virtual bool Deleted(CPed* pPed);
};

#endif