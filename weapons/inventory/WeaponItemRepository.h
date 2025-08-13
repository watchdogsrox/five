#ifndef WEAPON_ITEM_REPOSITORY_H
#define WEAPON_ITEM_REPOSITORY_H

// Game headers
#include "Weapons/Inventory/InventoryItemRepository.h"
#include "Weapons/Inventory/WeaponItem.h"

//////////////////////////////////////////////////////////////////////////
// CWeaponItemRepository
//////////////////////////////////////////////////////////////////////////

// The maximum number of storable weapons
static const s32 MAX_WEAPONS = 105;

class CWeaponItemRepository : public CInventoryItemRepository<CWeaponItem, MAX_WEAPONS>
{
public:

	//
	// CWeaponItemRepository
	// 

	CWeaponItemRepository()
	{
	}

	// Add the specified component to the specified weapon
	const CInventoryItem* AddComponent(u32 uWeaponNameHash, u32 uComponentNameHash);

	// remove the specified component from the specified weapon
	bool RemoveComponent(u32 uWeaponNameHash, u32 uComponentNameHash);
	
	// Retrieves the number of guns
	u32 GetNumGuns(bool bCountAllWeapons = false) const;

#if __BANK // Only included in __BANK builds
	// Debug rendering
	void RenderWeaponDebug(const Vector3& vTextPos, Color32 colour, bool bRenderComponents) const;
#endif // __BANK

private:

	//
	// Members
	//
};

#endif // WEAPON_ITEM_REPOSITORY_H
