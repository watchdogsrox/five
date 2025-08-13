#ifndef AMMO_ITEM_H
#define AMMO_ITEM_H

// Game headers
#include "Weapons/Info/AmmoInfo.h"
#include "Weapons/Inventory/InventoryItem.h"

//////////////////////////////////////////////////////////////////////////
// CAmmoItem
//////////////////////////////////////////////////////////////////////////

class CAmmoItem : public CInventoryItem
{
public:

	CAmmoItem() : m_iAmmo(0), m_bUsingInfiniteAmmo(false), m_bIgnoreInfiniteAmmoFlag(false) {}
	CAmmoItem(u32 uKey);
	CAmmoItem(u32 uKey, u32 uItemHash, const CPed* pPed);

	//
	// Accessors
	//

	// Check if the hash is valid
	static bool GetIsHashValid(u32 uItemHash);

	// Access to the ammo data
	const CAmmoInfo* GetInfo() const { return static_cast<const CAmmoInfo*>(CInventoryItem::GetInfo()); }

	//
	// Ammo
	//

	// Set the current ammo
	void SetAmmo(s32 iAmmo, const CPed* pPed) { weaponFatalAssertf(GetInfo(), "m_pInfo is NULL"); m_iAmmo = Clamp(iAmmo, 0, GetInfo()->GetAmmoMax(pPed)); }

	// Add to the ammo total
	void AddAmmo(s32 iAmmo, const CPed* pPed) { SetAmmo(GetAmmo() + iAmmo, pPed); }

	// Access the ammo total
	s32 GetAmmo() const { return m_iAmmo; }

	// Set whether we are using infinite ammo
	void SetIsUsingInfiniteAmmo(bool bUsingInfiniteAmmo) { m_bUsingInfiniteAmmo = bUsingInfiniteAmmo; }

	// Are we using infinite ammo?
	bool GetIsUsingInfiniteAmmo() const { return m_bUsingInfiniteAmmo || (!m_bIgnoreInfiniteAmmoFlag && GetInfo()->GetIsUsingInfiniteAmmo()); }

	// Set whether we should ignore the metadata flag for infinite ammo
	void SetIgnoreInfiniteAmmoFlag(bool bIgnoreFlag) { m_bIgnoreInfiniteAmmoFlag = bIgnoreFlag; }

private:

	//
	// Members
	//

	// Current amount of ammo of this type
	s32 m_iAmmo;

	// Flags
	bool m_bUsingInfiniteAmmo;
	bool m_bIgnoreInfiniteAmmoFlag;

private:
	// Copy constructor and assignment operator are private
	CAmmoItem(const CAmmoItem& other);
	CAmmoItem& operator=(const CAmmoItem& other);
};

#endif // AMMO_ITEM_H
