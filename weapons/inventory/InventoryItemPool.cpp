// File header
#include "Weapons/Inventory/InventoryItem.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedFactory.h"
#include "Weapons/Inventory/AmmoItem.h"
#include "Weapons/Inventory/WeaponItem.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CInventoryItem
//////////////////////////////////////////////////////////////////////////

// The largest info class that we size the pool to
#define LARGEST_INVENTORY_ITEM_CLASS sizeof(CWeaponItem)

CompileTimeAssert(sizeof(CInventoryItem)	<= LARGEST_INVENTORY_ITEM_CLASS);
CompileTimeAssert(sizeof(CAmmoItem)			<= LARGEST_INVENTORY_ITEM_CLASS);
CompileTimeAssert(sizeof(CWeaponItem)		<= LARGEST_INVENTORY_ITEM_CLASS);

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CInventoryItem, CInventoryItem::MAX_STORAGE, 0.29f, atHashString("CInventoryItem",0xdf261e39), LARGEST_INVENTORY_ITEM_CLASS);

#if !__NO_OUTPUT
void CInventoryItem::SpewPoolUsage()
{
	weaponDisplayf("Starting CInventoryItem Pool Spew:");

	s32 iIdx = 0;
	s32 iCount = _ms_pPool->GetSize();
	for(s32 i = 0; i < iCount; i++)
	{
		static const s32 MAX_STRING_SIZE = 128;
		char buff[MAX_STRING_SIZE];

		const CInventoryItem* pItem = _ms_pPool->GetSlot(i);
		if(pItem)
		{
			const CWeaponItem* pWeaponItem = NULL;
			const CAmmoItem* pAmmoItem = NULL;

			// Attempt to find the ped who owns the item
			const CPed* pOwnerPed = NULL;
			s32 iPedCount = CPed::GetPool()->GetSize();
			for(s32 j = 0; j < iPedCount && pOwnerPed==NULL; j++)
			{
				const CPed* pPed = (CPed*)((fwBasePool*)CPed::GetPool())->GetSlot(j);
				if(pPed && pPed->GetInventory())
				{
					const CPedInventory* pInventory = pPed->GetInventory();

					const CWeaponItemRepository& wr = pInventory->GetWeaponRepository();
					const atArray<CWeaponItem*>& weaponItems = wr.GetAllItems();
					for(s32 k = 0; k < weaponItems.GetCount(); k++)
					{
						if(weaponItems[k] == pItem)
						{
							pWeaponItem = weaponItems[k];
							pOwnerPed = pPed;
						}
					}

					const CAmmoItemRepository& ar = pInventory->GetAmmoRepository();
					const atArray<CAmmoItem*>& ammoItems = ar.GetAllItems();
					for(s32 k = 0; k < ammoItems.GetCount(); k++)
					{
						if(ammoItems[k] == pItem)
						{
							pAmmoItem = ammoItems[k];
							pOwnerPed = pPed;
						}
					}
				}
			}

			weaponAssertf(pOwnerPed, "All inventory items should be assigned to a ped");

			Vector3 vPedPos(Vector3::ZeroType);
			if(pOwnerPed)
			{
				vPedPos = VEC3V_TO_VECTOR3(pOwnerPed->GetTransform().GetPosition());
			}

			formatf(buff, MAX_STRING_SIZE, "Idx: %d, Info: %s, Ped: %p, %s, (%.2f,%.2f,%.2f), Cached: %d", iIdx++, pItem->GetInfo() ? pItem->GetInfo()->GetName() : "NULL", pOwnerPed, pOwnerPed ? pOwnerPed->GetModelName() : "NULL", vPedPos.x, vPedPos.y, vPedPos.z, pOwnerPed ? CPedFactory::GetFactory()->IsPedInDestroyedCache(pOwnerPed) : 0);

			if(pWeaponItem)
			{
				weaponDisplayf("%s", buff);
			}
			else if(pAmmoItem)
			{
				weaponDisplayf("%s, Ammo: %d", buff, pAmmoItem->GetAmmo());
			}
			else
			{
				weaponDisplayf("%s", buff);
			}
		}
	}
}
#endif // !__NO_OUTPUT