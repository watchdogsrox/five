#ifndef PED_WEAPON_MANAGER_H
#define PED_WEAPON_MANAGER_H

// Game headers
#include "Peds/PedWeapons/PedAccuracy.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Inventory/InventoryListener.h"
#include "Weapons/Inventory/PedEquippedWeapon.h"
#include "Weapons/Inventory/PedWeaponSelector.h"

// Forward declarations
class CGadget;
class CVehicleWeapon;

//////////////////////////////////////////////////////////////////////////
// CPedWeaponManager
//////////////////////////////////////////////////////////////////////////

class CPedWeaponManager : public CInventoryListener
{
public:

	CPedWeaponManager();
	virtual ~CPedWeaponManager();

	// Init
	void Init(CPed* pPed);

	// Processing
	void ProcessPreAI();

	// Processing
	void ProcessPostAI();

	//
	// Weapon selection
	//

	// Will attempt to select the specified weapon
	// It will be created at the next opportunity
	bool EquipWeapon(u32 uWeaponNameHash, s32 iVehicleIndex = -1, bool bCreateWeaponWhenLoaded = false, bool bProcessWeaponInstructions = false, CPedEquippedWeapon::eAttachPoint attach = CPedEquippedWeapon::AP_RightHand);

	// Will equip the best weapon we have available
	bool EquipBestWeapon(bool bForceIntoHand = false, bool bProcessWeaponInstructions = false);

	// Will equip the best melee weapon we have available
	bool EquipBestMeleeWeapon(bool bForceIntoHand = false, bool bProcessWeaponInstructions = false);

	// Will equip the best non-lethal weapon we have available
	bool EquipBestNonLethalWeapon(bool bForceIntoHand = false, bool bProcessWeaponInstructions = false);

	//
	// Equipped weapon querying
	//

	CPedEquippedWeapon* GetPedEquippedWeapon()  { return &m_equippedWeapon; }
	const CPedEquippedWeapon* GetPedEquippedWeapon() const { return &m_equippedWeapon; }

	// Get the currently equipped weapon hash
	u32 GetEquippedWeaponHash() const { return m_equippedWeapon.GetWeaponHash(); }

	// Get the currently equipped weapon object hash
	u32 GetEquippedWeaponObjectHash() const { return m_equippedWeapon.GetObjectHash(); }

	// Get the current weapon info for the object in the peds hand
	const CWeaponInfo* GetCurrentWeaponInfoForHeldObject() const 
	{ 
		if (GetEquippedWeaponObjectHash() != 0) 
			return CWeaponInfoManager::GetInfo<CWeaponInfo>(GetEquippedWeaponObjectHash()); 
		else
			return NULL;
	}

	// Get the currently equipped weapon info
	const CWeaponInfo* GetEquippedWeaponInfo() const { return m_equippedWeapon.GetWeaponInfo(); }

	// Get the last equipped weapon info
	const CWeaponInfo* GetLastEquippedWeaponInfo() const { return m_pLastEquippedWeaponInfo; }

	// Get the current weapon
	const CWeapon* GetEquippedWeapon() const;
	CWeapon* GetEquippedWeapon();

	// Get the best weapon
	// Get the currently equipped weapon hash
	u32 GetBestWeaponHash(u32 uExcludedHash = 0, bool bIgnoreAmmoCheck = false) const;
	u32 GetBestWeaponSlotHash(u32 uExcludedHash = 0, bool bIgnoreAmmoCheck = false) const;
	const CWeaponInfo* GetBestWeaponInfo(bool bIgnoreVehicleCheck = false, bool bIgnoreWaterCheck = false, bool bIgnoreFullAmmo = false, bool bIgnoreProjectile = false) const;
	const CWeaponInfo* GetBestWeaponInfoByGroup(u32 uGroupHash, bool bIgnoreAmmoCheck = false) const;

	// Get the current weapon object
	const CObject* GetEquippedWeaponObject() const { return m_equippedWeapon.GetObject(); }
	CObject* GetEquippedWeaponObject() { return m_equippedWeapon.GetObject(); }
	const CWeaponInfo* GetEquippedWeaponInfoFromObject() const { return m_equippedWeapon.GetObject() ? m_equippedWeapon.GetObject()->GetWeapon() ? m_equippedWeapon.GetObject()->GetWeapon()->GetWeaponInfo() : NULL : NULL; }
	const CWeapon* GetEquippedWeaponFromObject() const { return m_equippedWeapon.GetObject() ? m_equippedWeapon.GetObject()->GetWeapon() : NULL; }

	// Get the current weapon object
	const CWeapon* GetUnarmedEquippedWeapon() const { return m_equippedWeapon.GetUnarmedEquippedWeapon(); }
	CWeapon* GetUnarmedEquippedWeapon() { return m_equippedWeapon.GetUnarmedEquippedWeapon(); }

	const CWeapon* GetUnarmedKungFuEquippedWeapon() const { return m_equippedWeapon.GetUnarmedKungFuEquippedWeapon(); }
	CWeapon* GetUnarmedKungFuEquippedWeapon() { return m_equippedWeapon.GetUnarmedKungFuEquippedWeapon(); }

	// Get the equipped weapon slot
	u32 GetEquippedWeaponSlot() const { const CWeaponInfo* pInfo = GetEquippedWeaponInfo(); return pInfo ? pInfo->GetSlot() : 0; }

	// Get the currently active vehicle weapon index
	s32 GetEquippedVehicleWeaponIndex() const { return m_iEquippedVehicleWeaponIndex; }

	// Get the active vehicle weapon
	const CVehicleWeapon* GetEquippedVehicleWeapon() const;
	CVehicleWeapon* GetEquippedVehicleWeapon();

	// Return the weapon info for the equipped vehicle weapon, but only if it is a turret.
	const CWeaponInfo* GetEquippedTurretWeaponInfo() const;

	// Should we un-equip the equipped weapon when we are finished with it?
	bool GetShouldUnEquipWeapon() const;

	// Is the weapon we have equipped usable in cover?
	bool IsEquippedWeaponUsableInCover() const { return GetEquippedWeaponInfo() ? GetEquippedWeaponInfo()->GetUsableInCover() : false; }

	// Does the currently equipped weapon have a first person scope (either via weapon flag or via individual component)
	bool GetEquippedWeaponHasFirstPersonScope() const { return GetEquippedWeapon() ? GetEquippedWeapon()->GetHasFirstPersonScope() : false; }

	//
	// Selected weapon querying - for use by the HUD only
	//

	// Get the currently selected weapon hash
	u32 GetSelectedWeaponHash() const { return m_selector.GetSelectedWeapon(); }

	// Get the currently selected weapon info
	const CWeaponInfo* GetSelectedWeaponInfo() const { return m_selector.GetSelectedWeaponInfo(); }

	// Get the currently selected vehicle weapon index
	s32 GetSelectedVehicleWeaponIndex() const { return m_selector.GetSelectedVehicleWeapon(); }

	// Get the currently selected vehicle weapon info
	const CWeaponInfo* GetSelectedVehicleWeaponInfo() const { return m_selector.GetSelectedVehicleWeaponInfo(m_pPed); }

	//
	// Do we want to change weapons?
	//

	// Do we have a weapon wanting to be instantiated?
	bool GetRequiresWeaponSwitch() const;

	// Do we want to change weapons
	bool GetIsNewEquippableWeaponSelected() const { return m_selector.GetIsNewEquippableWeaponSelected(m_pPed); }

	//
	// Weapon creation/destruction
	//

	// Create the specified object
	bool CreateEquippedWeaponObject(CPedEquippedWeapon::eAttachPoint attach = CPedEquippedWeapon::AP_RightHand);

	// Destroy the equipped weapon object
	void DestroyEquippedWeaponObject(bool bAllowDrop = true, const char* debugstr = NULL, bool bResetAttachPoint = false);

	// Destroy the specified equipped gadget
	//  if bForceDeletion == true then the CGadget::Destroy callback will be bypassed
	void DestroyEquippedGadget(u32 uGadgetHash, bool bForceDeletion = false);

	// Destroy all equipped gadgets
	void DestroyEquippedGadgets();

	// Releases the specified weapon to physics and optionally removes it from the inventory
	CObject* DropWeapon(u32 uDroppedHash, bool bRemoveFromInventory, bool bTryCreatePickup = true, bool bIsDummyObjectForProjectileQuickThrow = false, bool bTriggerScriptEvent = false);

	// Get the attach point the weapon is attached too
	CPedEquippedWeapon::eAttachPoint GetEquippedWeaponAttachPoint() const { return m_equippedWeapon.GetAttachPoint(); }

	// Switch attach points on currently equipped weapon
	void AttachEquippedWeapon(CPedEquippedWeapon::eAttachPoint attach);

	//
	// Backup/Restore
	//

	// Store the equipped weapon hash
	void BackupEquippedWeapon(bool allowClones = false);

	// Restore the backup weapon
	void RestoreBackupWeapon();

	// Explicitly set the backup weapon
	void SetBackupWeapon(u32 uBackupWeapon);

	// Clear the backup weapon
	void ClearBackupWeapon();

	// Se what weapon if any is currently backed up
	u32 GetBackupWeapon() const {return m_uEquippedWeaponHashBackup;}

	//
	// Blanks
	//

	// Set chance to fire blanks
	void SetChanceToFireBlanks(f32 fMin, f32 fMax) { m_fChanceToFireBlanksMin = fMin; m_fChanceToFireBlanksMax = fMax; }

	// Get chance to fire blanks
	f32 GetChanceToFireBlanksMin() const { return m_fChanceToFireBlanksMin; }
	f32 GetChanceToFireBlanksMax() const { return m_fChanceToFireBlanksMax; }

	//
	// Aim position - used for network code
	//

	// Set the aim position
	void SetWeaponAimPosition(const Vector3& aimPosition) { m_vWeaponAimPosition = aimPosition; }

	// Get the aim position
	const Vector3& GetWeaponAimPosition() const { return m_vWeaponAimPosition; }

	//
	// Impacts - used for script
	//

	// Set the last impact position
	void SetLastWeaponImpactPos(const Vector3& vNewImpactPos) { m_vImpactPosition = vNewImpactPos; m_iImpactTime = fwTimer::GetTimeInMilliseconds(); } 

	// Get the last impact position
	const Vector3& GetLastWeaponImpactPos() const { return m_vImpactPosition; }

	// Get the last time of impact
	const s32 GetLastWeaponImpactTime() const { return m_iImpactTime; }

	//
	// Accessors
	//

	// Flag queries
	inline bool GetIsArmed() const { return m_equippedWeapon.GetIsArmed(); }
	bool GetIsArmedMelee() const;
	bool GetIsArmedGun() const;
	bool GetIsArmedGunOrCanBeFiredLikeGun() const;
	bool GetIsArmed1Handed() const;
	bool GetIsArmed2Handed() const;
	bool GetIsArmedHeavy() const;

	u32 GetWeaponGroup() const;

	//Weapon Selector
	CPedWeaponSelector* GetWeaponSelector() {return &m_selector; }
	const CPedWeaponSelector* GetWeaponSelector() const {return &m_selector; }

	// Is the specified gadget active?
	bool GetIsGadgetEquipped(u32 uGadgetHash) const;

	// Accessors for currently equipped gadgets
	u32 GetNumEquippedGadgets() const { return m_equippedGadgets.GetCount(); }
	const CGadget *GetEquippedGadget(u32 gadgetIndex) const { return m_equippedGadgets[gadgetIndex]; }

	//
	// Event handling
	//

	// Handle ragdoll event
	void SwitchToRagdoll();

	// Handle animated event
	void SwitchToAnimated();

	//
	// CInventoryListener API
	//

	// Called when an item is added to the inventory
	virtual void ItemAdded(u32 uItemNameHash);

	// Called when an item is removed from the inventory
	virtual void ItemRemoved(u32 uItemNameHash);

	// Called when all items are removed from the inventory
	virtual void AllItemsRemoved();

	//
	// Misc.
	//

	// CS: Temp?
	// Set whether the detonator is currently working
	void SetDetonatorActive(bool bActive)
	{
		if(bActive)
			m_flags.ClearFlag(Flag_DetonatorDeactivated);
		else
			m_flags.SetFlag(Flag_DetonatorDeactivated);
	}

	// Need to let NM know when NM ragdolls are dropping/unequipping a weapon
	void UnregisterNMWeapon();

	// CS: Temp?
	// Query if the detonator is currently working
	bool GetIsDetonatorActive() const { return !m_flags.IsFlagSet(Flag_DetonatorDeactivated); }

	// Tez: Temp?
	// Set whether to create the weapon object when loaded
	void SetCreateWeaponObjectWhenLoaded(bool bCreate)
	{
		if(!bCreate)
			m_flags.ClearFlag(Flag_CreateWeaponWhenLoaded);
		else
			m_flags.SetFlag(Flag_CreateWeaponWhenLoaded);
	}

	bool GetCreateWeaponObjectWhenLoaded() const { return m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded); }

	// Accessor for ped accuracy
	const CPedAccuracy&			GetAccuracy() const { return m_accuracy; }
	CPedAccuracy&				GetAccuracy() { return m_accuracy; }

	// Settor/Gettor for driveby select or fire time
	void SetLastEquipOrFireTime(u32 uTime) { m_uLastEquipOrFireTime = uTime; }
	u32 GetLastEquipOrFireTime() const { return m_uLastEquipOrFireTime; }

	//
	// Instructions
	//

	enum eWeaponInstruction {WI_None, WI_AmbientTaskDestroyWeapon, WI_AmbientTaskDestroyWeapon2ndAttempt, WI_RevertToStoredWeapon, WI_AmbientTaskDropAndRevertToStored, WI_AmbientTaskDropAndRevertToStored2ndAttempt};

	void ProcessWeaponInstructions(CPed* pPed);
	eWeaponInstruction GetWeaponInstruction() const { return m_nInstruction; }
	void SetWeaponInstruction(eWeaponInstruction nInstruction) { m_nInstruction = nInstruction; }

	bool GetForceReloadOnEquip() const { return m_bForceReloadOnEquip; }
	void SetForceReloadOnEquip(bool bValue) { m_bForceReloadOnEquip = bValue; }

private:

	bool CanAlterEquippedWeapon() const;
	//
	// Members
	//

	// Owner ped
	CPed* m_pPed;

	// Input handler - manages pad input and weapon selection
	CPedWeaponSelector m_selector;

	// Equipped weapon
	CPedEquippedWeapon m_equippedWeapon;

	// Last equipped weapon
	const CWeaponInfo* m_pLastEquippedWeaponInfo;

	// Ped Accuracy
	CPedAccuracy m_accuracy;

	// Equipped vehicle weapon
	// Vehicle weapons and ped weapons are mutually exclusive
	s32 m_iEquippedVehicleWeaponIndex;

	// Equipped gadgets
	atArray<CGadget*> m_equippedGadgets;

	// Backed up equipped weapon hash - stored when in vehicles, so it can be restored
	u32 m_uEquippedWeaponHashBackup;

	// Chance of firing blanks
	f32 m_fChanceToFireBlanksMin;
	f32 m_fChanceToFireBlanksMax;

	// Handle to any active constraint from SwitchToRagdoll
	phConstraintHandle m_RagdollConstraintHandle;

	// The time when we equipped or fired our weapon
	u32 m_uLastEquipOrFireTime;

	bool m_bForceReloadOnEquip;

	//
	// Aiming
	//

	// The position we are aiming our weapon at
	Vector3 m_vWeaponAimPosition;

	//
	// Impacts
	//

	// The last position we hit with our weapon
	Vector3 m_vImpactPosition;

	// The time the impact position was recorded
	s32 m_iImpactTime;

	//
	// Flags
	//

	enum Flags
	{
		Flag_CreateWeaponWhenLoaded		= BIT0,
		Flag_DetonatorDeactivated		= BIT1,
		Flag_SyncAmmo					= BIT2,
		Flag_DelayedWeaponInLeftHand	= BIT3,
	};

	// Flags
	fwFlags32 m_flags;

	//
	// Instructions
	//

	eWeaponInstruction m_nInstruction;
};

#endif // PED_WEAPON_MANAGER_H
