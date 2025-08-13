//
// weapons/pedinventory.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef PED_INVENTORY_H
#define PED_INVENTORY_H

// Game headers
#include "Weapons/Inventory/AmmoItemRepository.h"
#include "Weapons/Inventory/WeaponItemRepository.h"
#include "Weapons/WeaponObserver.h"

// Forward declarations
class CPed;
class CPedModelInfo;
class CInventoryListener;

//////////////////////////////////////////////////////////////////////////
// CPedInventory
//////////////////////////////////////////////////////////////////////////

class CPedInventory : public CWeaponObserver
{
public:

	// Constructor
	CPedInventory();

	// Init
	void Init(CPed* pPed, const CPedModelInfo* pPedModelInfo=NULL);

	// Processing
	void Process();

	// Listeners
	void RegisterListener(CInventoryListener* pListener);

	//
	// Inventory Item API
	//

	// Adds the specified weapon into the inventory
	CWeaponItem* AddWeapon(u32 uWeaponNameHash);

	// Adds the specified weapon into the inventory and the amount of ammo specified for the weapon, if it uses ammo
	CWeaponItem* AddWeaponAndAmmo(u32 uWeaponNameHash, s32 iAmmo);

	// Get the specified weapon from the inventory
	const CWeaponItem* GetWeapon(u32 uWeaponNameHash) const { return m_weaponRepository.GetItem(uWeaponNameHash); }

	// Get the specified weapon from the inventory
	CWeaponItem* GetWeapon(u32 uWeaponNameHash) { return m_weaponRepository.GetItem(uWeaponNameHash); }

	// Get the an weapon by it's slot
	const CWeaponItem* GetWeaponBySlot(u32 uSlot) const { return m_weaponRepository.GetItemByKey(uSlot); }

	// Removes the specified item from the inventory
	bool RemoveWeapon(u32 uWeaponNameHash);

	// Removes the specified item from the inventory
	bool RemoveWeaponAndAmmo(u32 uWeaponNameHash);

	// Removes all inventory items except specified one
	void RemoveAllWeaponsExcept(u32 uWeaponNameHash, u32 uNextWeaponNameHash = 0);

	// Removes all inventory items
	void RemoveAll();

	// Is the selected item streamed in?
	bool GetIsStreamedIn(u32 uItemNameHash);

	// Are all items in the inventory streamed in?
	bool GetIsStreamedIn();

	// Get the number of weapons referencing this weapons ammo type
	s32 GetNumWeaponAmmoReferences(u32 uWeaponNameHash) const;

	//
	// Weapon API
	//

	const CWeaponItemRepository& GetWeaponRepository() const { return m_weaponRepository; }
	CWeaponItemRepository& GetWeaponRepository() { return m_weaponRepository; }

	//
	// Ammo API
	//

	// Direct access to AmmoItemRepository. Try to use one of the direct API functions below, as we need ped pointers.
	const CAmmoItemRepository& GetAmmoRepository() const { return m_ammoRepository; }
	CAmmoItemRepository& GetAmmoRepository() { return m_ammoRepository; }

	// Adds the specified amount of ammo
	const CAmmoItem* AddAmmo(u32 uAmmoNameHash, s32 iAmmo);

	// Sets the specified amount of ammo
	void SetAmmo(u32 uAmmoNameHash, s32 iAmmo);

	// Gets the specified amount of ammo
	s32 GetAmmo(u32 uAmmoNameHash) const { return m_ammoRepository.GetAmmo(uAmmoNameHash); }

	// Adds ammo for the specified weapon
	const CAmmoItem* AddWeaponAmmo(u32 uWeaponNameHash, s32 iAmmo);

	// Sets ammo count for the specified weapon
	void SetWeaponAmmo(u32 uWeaponNameHash, s32 iAmmo);

	// Gets the amount of ammo associated with the specified weapon
	s32 GetWeaponAmmo(const CWeaponInfo* pWeaponInfo) const { return m_ammoRepository.GetWeaponAmmo(pWeaponInfo, m_pPed); }
	s32 GetWeaponAmmo(u32 uWeaponNameHash) const { return m_ammoRepository.GetWeaponAmmo(uWeaponNameHash, m_pPed); }

	// Checks whether the specified weapon is out of ammo
	bool GetIsWeaponOutOfAmmo(const CWeaponInfo* pWeaponInfo) const { return m_ammoRepository.GetIsWeaponOutOfAmmo(pWeaponInfo, m_pPed); }
	bool GetIsWeaponOutOfAmmo(u32 uWeaponNameHash) const { return m_ammoRepository.GetIsWeaponOutOfAmmo(uWeaponNameHash, m_pPed); }

	// Sets infinite ammo for the specified weapon
	void SetWeaponUsingInfiniteAmmo(u32 uWeaponNameHash, bool bInfinite);

	// Gets whether the specified weapon's ammo is set to be infinite
	bool GetIsWeaponUsingInfiniteAmmo(const CWeaponInfo* pWeaponInfo) const { return m_ammoRepository.GetIsWeaponUsingInfiniteAmmo(pWeaponInfo, m_pPed); }
	bool GetIsWeaponUsingInfiniteAmmo(u32 uWeaponNameHash) const { return m_ammoRepository.GetIsWeaponUsingInfiniteAmmo(uWeaponNameHash, m_pPed); }


	//
	// WeaponObserver API
	//

	// Notify that the weapon has changed ammo - if returns true the weapon will decrement its own ammo counter
	virtual bool NotifyAmmoChange(u32 uWeaponNameHash, s32 iAmmo);

	// Notify that the weapon has changed timer - if returns true the weapon will decrement its own timer counter
	virtual bool NotifyTimerChange(u32 uWeaponNameHash, u32 uTimer);

	virtual const CPed* GetOwner() const { return m_pPed; }

	//
	// Debug
	//

#if __BANK
	// Widgets
	static void AddWidgets(bkBank& bank);

	// Debug rendering
	static void RenderDebug();
#endif // __BANK

private:

	//
	// Debug
	//

#if __BANK
	// Widget callbacks
	static void DebugDetectTypesCB();
	static void DebugWeaponCB();
	static void DebugGiveWeaponCB();
	static void DebugRemoveWeaponCB();
	static void DebugEquipWeaponCB();
	static void DebugDropWeaponCB();
	static void DebugTintIndexCB();
	static void DebugComponentTintIndexCB();
	static void DebugGiveAmmoCB();
	static void DebugRemoveAmmoCB();
	static void DebugGiveComponentCB();
	static void DebugRemoveComponentCB();
	static void DebugRemoveAllItemsCB();
	static void DebugRemoveAllItemsExceptUnarmedCB();
	static void DebugEnableAllWeaponSelectionCB();
	static void DebugDisableAllWeaponSelectionCB();
#endif // __BANK

	//
	// Members
	//

	// Owner ped - not a RegdPed as it is owned by CPed
	CPed* m_pPed;

	// Weapon repository
	CWeaponItemRepository m_weaponRepository;

	// Ammo repository
	CAmmoItemRepository m_ammoRepository;

	// Keep track of how many one handed/two handed weapons we currently have
	//s32 m_numOneHandedWeapons;
	//s32 m_numTwoHandedWeapons;

	u32 m_uCachedRechargeTime;
	u32 m_uCachedWeaponState;

#if __BANK
	// Debug members
	static bool  ms_bRenderComponentsDebug;
	static bool  ms_bRenderDebug;
	static bool  ms_bEquipImmediately;
	static s32 ms_iWeaponComboIndex;
	static s32 ms_iAmmoComboIndex;
	static s32 ms_iComponentComboIndex;
	static s32 ms_iAmmoToGive;
	static u8 ms_uTintIndex;
	static u8 ms_uComponentTintIndex;
#endif // __BANK
};

#endif // PED_INVENTORY_H
