#ifndef GADGET_SKIS_H
#define GADGET_SKIS_H

// Game headers
#include "Weapons/Gadgets/Gadget.h"

//////////////////////////////////////////////////////////////////////////
// CGadgetSkis
//////////////////////////////////////////////////////////////////////////

class CGadgetSkis : public CGadget
{
public:

	// Construction
	CGadgetSkis(u32 uWeaponHash, s32 iAmmoTotal = INFINITE_AMMO, CDynamicEntity* pDrawable = NULL)
		: CGadget(uWeaponHash, iAmmoTotal, pDrawable)
	{
	}

	virtual bool Created(CPed* pPed);
	virtual bool Deleted(CPed* pPed);
};

#endif // GADGET_SKIS_H
