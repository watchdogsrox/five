// File header
#include "Weapons/WeaponFactory.h"

// Game headers
#include "Weapons/Gadgets/Gadget.h"
#include "Weapons/Gadgets/GadgetParachute.h"
#include "Weapons/Gadgets/GadgetJetpack.h"
#include "Weapons/Gadgets/GadgetSkis.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/WeaponTypes.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// WeaponFactory
//////////////////////////////////////////////////////////////////////////

namespace WeaponFactory
{

CWeapon* Create(u32 uWeaponNameHash, s32 iAmmo, CDynamicEntity* pDrawable, const char* UNUSED_PARAM(debugstring), u8 tintIndex)
{
	if(weaponVerifyf(CWeapon::GetPool()->GetNoOfFreeSpaces() > 0, "Weapon pool is full.  Size [%d]", CWeapon::GetPool()->GetSize()))
	{
		// Get the weapon info from the hash
		const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(uWeaponNameHash);
		if(pItemInfo)
		{
			if(pItemInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
			{
				CWeapon* pCreatedItem = NULL;

				if(uWeaponNameHash == GADGETTYPE_SKIS)
				{
					pCreatedItem = rage_new CGadgetSkis(uWeaponNameHash, iAmmo, pDrawable);
				}
				else if(uWeaponNameHash == GADGETTYPE_PARACHUTE)
				{
					pCreatedItem = rage_new CGadgetParachute(uWeaponNameHash, iAmmo, pDrawable);
				}
#if ENABLE_JETPACK
				else if(uWeaponNameHash == GADGETTYPE_JETPACK)
				{
					pCreatedItem = rage_new CGadgetJetpack(uWeaponNameHash, iAmmo, pDrawable);
				}
#endif
				else
				{
					pCreatedItem = rage_new CWeapon(uWeaponNameHash, iAmmo, pDrawable, false, tintIndex);
				}

//Disable this, but retain it as it was useful in finding a problem where pickups couldn't be created any longer because they were filling up pool.
//#if !__FINAL
//				if (NetworkInterface::IsGameInProgress())
//					weaponDisplayf("WeaponFactory::Create rage_new CWeapon GetNoOfUsedSpaces[%d]/GetSize[%d] invoker[%s]",CWeapon::GetPool()->GetNoOfUsedSpaces(),CWeapon::GetPool()->GetSize(),debugstring);
//#endif

				return pCreatedItem;
			}
			else
			{
				weaponAssertf(0, "Invalid type [%s] passed to WeaponFactory::Create", pItemInfo->GetClassId().GetCStr());
			}
		}
	}
#if __DEV
	else
	{
		CWeapon::GetPool()->PoolFullCallback();
	}
#endif // __DEV
	return NULL;
}

} // namespace WeaponFactory
