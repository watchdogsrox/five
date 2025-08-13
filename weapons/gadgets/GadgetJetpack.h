#ifndef INC_GADGET_JETPACK
#define INC_GADGET_JETPACK

// Game headers
#include "Weapons/Gadgets/Gadget.h"
#include "Task/Movement/JetpackObject.h"

#if ENABLE_JETPACK

//////////////////////////////////////////////////////////////////////////
// CGadgetJetpack
//////////////////////////////////////////////////////////////////////////

class CGadgetJetpack : public CGadget
{
public:

	CGadgetJetpack(u32 uWeaponHash, s32 iAmmoTotal = INFINITE_AMMO, CDynamicEntity* pDrawable = NULL)
		: CGadget(uWeaponHash, iAmmoTotal, pDrawable)
	{
	}

};

#endif // ENABLE_JETPACK

#endif // INC_GADGET_JETPACK
