// File header
#include "Weapons/Inventory/WeaponItemRepository.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CWeaponItemRepository
//////////////////////////////////////////////////////////////////////////

const CInventoryItem* CWeaponItemRepository::AddComponent(u32 uWeaponNameHash, u32 uComponentNameHash)
{
	CWeaponItem* pItem = GetItem(uWeaponNameHash);
	if(weaponVerifyf(pItem, "Item [0x%X] does not exist in inventory", uWeaponNameHash))
	{
		if(weaponVerifyf(pItem->AddComponent(uComponentNameHash), "Failed to add attachment [0x%X] to item [%s]", uComponentNameHash, pItem->GetInfo()->GetName()))
		{
			weaponDebugf1("CWeaponItemRepository::AddComponent: [%d][%s]: [%d][%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), uComponentNameHash, atHashString(uComponentNameHash).TryGetCStr());
		}
	}

	return pItem;
}

bool CWeaponItemRepository::RemoveComponent(u32 uWeaponNameHash, u32 uComponentNameHash)
{
	CWeaponItem* pItem = GetItem(uWeaponNameHash);
	if(weaponVerifyf(pItem, "Item [0x%X] does not exist in inventory", uWeaponNameHash))
	{
		if(weaponVerifyf(pItem->RemoveComponent(uComponentNameHash), "Failed to remove attachment [0x%X] to item [%s]", uComponentNameHash, pItem->GetInfo()->GetName()))
		{
			weaponDebugf1("CWeaponItemRepository::RemoveComponent: [%d][%s], [%d][%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), uComponentNameHash, atHashString(uComponentNameHash).TryGetCStr());
		}
	}

	return pItem != NULL;
}

u32 CWeaponItemRepository::GetNumGuns(bool bCountAllWeapons) const
{
	//Keep track of the gun count.
	u32 uCount = 0;
	
	//Iterate over the items.
	for(s32 i = 0; i < GetItemCount(); i++)
	{
		//Grab the weapon.
		const CWeaponItem* pWeapon = GetItemByIndex(i);
		if(!pWeapon)
		{
			continue;
		}
		
		//Grab the weapon info.
		const CWeaponInfo* pInfo = pWeapon->GetInfo();
		if(!pInfo)
		{
			continue;
		}
		
		//Ensure the weapon is a gun.
		if(!pInfo->GetIsGun() && !bCountAllWeapons)
		{
			continue;
		}
		
		//Increase the count.
		++uCount;
	}
	
	return uCount;
}

#if __BANK // Only included in __BANK builds
void CWeaponItemRepository::RenderWeaponDebug(const Vector3& vTextPos, Color32 colour, bool bRenderComponents) const
{
	s32 iLineCount = 0;
	char itemText[64];

	for(s32 i = 0; i < GetItemCount(); i++)
	{
		const CWeaponItem* pItem = GetItemByIndex(i);
		if(pItem && pItem->GetInfo())
		{
			formatf(itemText, COUNTOF(itemText), "Item: %s (0x%X)\n", pItem->GetInfo()->GetName(), pItem->GetInfo()->GetHash());
			grcDebugDraw::Text(vTextPos, colour, 0, grcDebugDraw::GetScreenSpaceTextHeight() * iLineCount++, itemText);

			if(bRenderComponents && pItem->GetComponents())
			{
				for(s32 i = 0; i < pItem->GetComponents()->GetCount(); i++)
				{
					if ((*pItem->GetComponents())[i].pComponentInfo)
					{
						formatf(itemText, COUNTOF(itemText), "Component: %s (0x%p)\n", (*pItem->GetComponents())[i].pComponentInfo->GetName(), (*pItem->GetComponents())[i].pComponentInfo);
						grcDebugDraw::Text(vTextPos, Color_red, 20, grcDebugDraw::GetScreenSpaceTextHeight() * iLineCount++, itemText);
					}		
				}
			}
		}
	}
}
#endif // __BANK