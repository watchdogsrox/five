#ifndef PED_WEAPON_SELECTOR_H
#define PED_WEAPON_SELECTOR_H

#include "grcore/debugdraw.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Inventory/PedInventorySelectOption.h"
#include "system/control.h"

// Forward declarations
class CPed;
class CWeaponItem;

//////////////////////////////////////////////////////////////////////////
// CPedWeaponSelector
//////////////////////////////////////////////////////////////////////////

class CPedWeaponSelector
{
public:

	CPedWeaponSelector();

	// Processing
	void Process(CPed* pPed);

	// Select a new weapon based on the specified option
	bool SelectWeapon(CPed* pPed, ePedInventorySelectOption option, const bool bFromHud = false, u32 uWeaponNameHash = 0, s32 iVehicleWeaponIndex = -1, bool bIgnoreAmmoCheck = true);

	// Clear selected weapons
	void ClearSelectedWeapons() { m_uSelectedWeaponHash.Clear(); m_iSelectedVehicleWeaponIndex = -1; }

	// Get the current selected weapon
	u32 GetSelectedWeapon() const { return m_uSelectedWeaponHash; }

	// Get the current selected weapon info
	const CWeaponInfo* GetSelectedWeaponInfo() const { return m_pSelectedWeaponInfo; }

	// Get the current selected vehicle weapon
	s32 GetSelectedVehicleWeapon() const { return m_iSelectedVehicleWeaponIndex; }

	// Get the weapon info for a particular index
	const CWeaponInfo* GetSelectedVehicleWeaponInfo(const CPed* pPed) const;

	// Has a new weapon been selected?
	bool GetIsNewWeaponSelected(CPed* pPed) const;

	// Has a new weapon been selected?
	bool GetIsNewEquippableWeaponSelected(CPed* pPed) const;

	// Statics
	static bank_s32 RESET_TIME;

	// Get the best valid weapon in the list
	u32 GetBestPedWeapon(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList, const bool bIgnoreVehicleCheck, const u32 uExcludedHash = 0, bool bIgnoreAmmoCheck = false, bool bIgnoreWaterCheck = false, bool bIgnoreFullAmmo = false, bool bIgnoreProjectile = false) const;
	u32 GetBestPedWeaponSlot(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList, const bool bIgnoreVehicleCheck, const u32 uExcludedHash = 0, bool bIgnoreAmmoCheck = false) const;

	// Get the best valid weapon in the list
	u32 GetBestPedWeaponByGroup(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList, const u32 uGroupHash, bool bIgnoreAmmoCheck = false) const;

	// Get the best valid melee weapon in the list
	u32 GetBestPedMeleeWeapon(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList) const;

	// Get the best valid non lethal weapon in the list
	u32 GetBestPedNonLethalWeapon(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList) const;

	// Returns the item if in the item exists/is valid
	bool GetIsWeaponValid(const CPed* pPed, const CWeaponInfo* pWeaponInfo, bool bIgnoreVehicleCheck, bool bIgnoreAmmoCheck, bool bIgnoreWaterCheck = false, bool bIgnoreFullAmmo = false, bool bIgnoreProjectile = false) const;

	// Get is the player selecting a vehicle weapon
	bool GetIsPlayerSelectingNextVehicleWeapon(const CPed* pPed) const;
	bool GetIsPlayerSelectingPreviousVehicleWeapon(const CPed* pPed) const;

	// Get is the player selecting a parachute weapon
	bool GetIsPlayerSelectingNextParachuteWeapon(const CPed* pPed) const;
	bool GetIsPlayerSelectingPreviousParachuteWeapon(const CPed* pPed) const;

	// Set the selected weapon
	void SetSelectedWeapon(const CPed* pPed, u32 uSelectedWeaponHash);

#if KEYBOARD_MOUSE_SUPPORT
	void SetLastWeaponType(eWeaponWheelSlot eType) { m_eLastSelectedType = eType; }
#else
	void SetLastWeaponType(eWeaponWheelSlot UNUSED_PARAM(eType)) { }
#endif

private:


	// Get the index in pList for the specified slot
	s32 GetSlotIndex(const CPed* pPed, u32 uSlot, const CWeaponInfoBlob::SlotList& slotList) const;

	//
	// Ped Weapon Functions
	//

	// Get the next valid weapon in the list, starting at iStartingIndex
	u32 GetNextPedWeapon(const CPed* pPed, s32 iStartingIndex, const CWeaponInfoBlob::SlotList& slotList, bool bIgnoreAmmoCheck = true) const;

	// Get the previous valid weapon in the list, starting at iStartingIndex
	u32 GetPreviousPedWeapon(const CPed* pPed, s32 iStartingIndex, const CWeaponInfoBlob::SlotList& slotList, bool bIgnoreAmmoCheck = true) const;

#if KEYBOARD_MOUSE_SUPPORT
	// Gets the next weapon from the weapon wheels memory
	u32 GetNextWeaponFromMemory();

	// Gets the previous weapon from the weapon wheels memory
	u32 GetPreviousWeaponFromMemory();
#endif // KEYBOARD_MOUSE_SUPPORT

	//
	// Vehicle Weapon Functions
	//

	// Get the next valid vehicle weapon, starting at iStartingIndex
	s32 GetNextVehicleWeapon(const CPed* pPed, s32 iStartingIndex, u32 &uWeaponHash) const;

	// Get the previous valid vehicle weapon, starting at iStartingIndex
	s32 GetPreviousVehicleWeapon(const CPed* pPed, s32 iStartingIndex, u32 &uWeaponHash) const;

	// Get the best valid vehicle weapon in the list
	s32 GetBestVehicleWeapon(const CPed* pPed) const;

	bool CheckForSearchLightValid(const CPed* pPed, const CWeaponInfo* pWeaponInfo) const;

	// Check if enough time has passed between weapon switch checks
	// Returns TRUE if weapon switch checks may proceed, and updates time member variable
	bool HasCheckForWeaponSwitchTimeElapsed();

	// Check if the given ped should consider switching to or from RPG weapon
	bool ShouldPedConsiderWeaponSwitch_RPG(const CPed* pPed) const;

	// Check if the given ped's current target is a ped and that ped is in a vehicle
	bool IsCurrentTargetVehicleOrPedInVehicle(const CPed* pPed) const;

	// Check if a driveby weapon is disabled by a particular attached vehicle modification
	bool GetIsWeaponDisabledByVehicleMod(const CPed* pPed, const CWeaponInfo* pWeaponInfo, const eVehicleModType modType) const;

	//
	// Members
	//

	// The current selected weapon
	atHashWithStringNotFinal m_uSelectedWeaponHash;

	// The current selected weapon info
	RegdConstWeaponInfo m_pSelectedWeaponInfo;

	// The current selected vehicle weapon
	s32 m_iSelectedVehicleWeaponIndex;

	// Time weapon was selected
	u32 m_uWeaponSelectTime;

	// Next time to consider switching weapon
	u32 m_uNextConsiderSwitchTimeMS;

#if KEYBOARD_MOUSE_SUPPORT
	// Get the next valid weapon of a group in the list, starting at iStartingIndex
	u32 GetNextPedWeaponInGroup(const CPed* pPed, eWeaponWheelSlot eWeaponGroup, const CWeaponInfoBlob::SlotList& slotList);

	// PC specific weapon switching.
	s32 m_iWeaponCatagoryIndex[MAX_WHEEL_SLOTS];
	eWeaponWheelSlot m_eLastSelectedType;
#endif // KEYBOARD_MOUSE_SUPPORT

	// Time to wait between weapon switch checks
	static u32 ms_uConsiderSwitchPeriodMS;
};

#endif // PED_WEAPON_SELECTOR_H
