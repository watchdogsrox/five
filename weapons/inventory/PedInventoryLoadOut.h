//
// weapons/pedinventoryloadout.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef PED_INVENTORY_LOADOUT_H
#define PED_INVENTORY_LOADOUT_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Forward declarations
class CPed;
class CPedModelInfo;

////////////////////////////////////////////////////////////////////////////////

class CLoadOutItem
{
public:

	// Constructor
	CLoadOutItem();
	virtual ~CLoadOutItem();

	// Give item to ped
	virtual void AddToPed(CPed* pPed) const = 0;

	// Check if this CLoadOutItem is a weapon with the given hash, with EquipThisWeapon set.
	virtual bool ShouldEquipWeapon(u32 UNUSED_PARAM(uWeaponHash)) const { return false; }

	virtual u32 GetHash() {return 0;}

private:

	//
	// Members
	//

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CLoadOutWeapon : public CLoadOutItem
{
public:

	enum Flags
	{
		FL_EquipThisWeapon = BIT0,
		FL_SelectThisWeapon = BIT1,
		FL_InfiniteAmmo = BIT2,
		FL_SPOnly = BIT3,
		FL_MPOnly = BIT4,
	};

	// Constructor
	CLoadOutWeapon();

	// Give item to ped
	virtual void AddToPed(CPed* pPed) const;

	u32 GetHash() {return m_WeaponName.GetHash();}

	// Check if this CLoadOutItem is a weapon with the given hash, with EquipThisWeapon set.
	virtual bool ShouldEquipWeapon(u32 uWeaponHash) const { return m_WeaponName.GetHash() == uWeaponHash && (m_Flags & FL_EquipThisWeapon) != 0; }

private:

	//
	// Members
	//

	// The weapon name
	atHashWithStringNotFinal m_WeaponName;

	// Any ammo
	u32 m_Ammo;

	// Flags
	u32 m_Flags;
	
	// Component names
	atArray<atHashWithStringNotFinal> m_ComponentNames;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CLoadOutRandom : public CLoadOutItem
{
public:

	// Item and percentage chance
	struct sRandomItem
	{
		CLoadOutItem* Item;
		f32 Chance;
		PAR_SIMPLE_PARSABLE;
	};

	// Constructor
	CLoadOutRandom();
	virtual ~CLoadOutRandom();

	// Give item to ped
	virtual void AddToPed(CPed* pPed) const;

	// Check if this CLoadOutItem is a weapon with the given hash, with EquipThisWeapon set.
	virtual bool ShouldEquipWeapon(u32 uWeaponHash) const;

	// Called after loading
	void PostLoad();

private:

	//
	// Members
	//

	// Items to select from
	atArray<sRandomItem> m_Items;

	// Total chance
	f32 m_fTotalChance;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CPedInventoryLoadOut
{
public:

	// Constructor
	CPedInventoryLoadOut();
	~CPedInventoryLoadOut();

	//Assignment operator
	CPedInventoryLoadOut& operator=(const CPedInventoryLoadOut& rhs );

	// Give load out to ped
	void AddToPed(CPed* pPed) const;

	// Delete the data
	void Reset();

	// Should equip the specified weapon?
	bool ShouldEquipWeapon(u32 uWeaponHash) const;

	// Add weapon to loadout
	void MergeLoadout(CPedInventoryLoadOut& loadOut);

	// Insert new loadout
	void CopyLoadout(CPedInventoryLoadOut& loadOut);

	// Get the item hash
	u32 GetHash() const { return m_Name.GetHash(); }

#if !__FINAL || __FINAL_LOGGING
	// Get the name
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	static bool ms_bDontFreeMemory;

private:

	//
	// Members
	//

	// Name
	atHashWithStringNotFinal m_Name;

	// Items in the load out
	atArray<CLoadOutItem*> m_Items;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CPedInventoryLoadOutManager
{
public:

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Add the items specified in the default load out
	// A load out will clear all current inventory items
	static bool SetDefaultLoadOut(CPed* pPed, const CPedModelInfo* pPassedPedModelInfo=NULL);

	// Add the items specified in a particular load out
	// A load out will clear all current inventory items
	static bool SetLoadOut(CPed* pPed, u32 uLoadOutNameHash);

	// Add an alias for this loadout, so if it is applied, the new one gets applied instead
	static void AddLoadOutAlias(u32 uOriginalLoadOutNameHash, u32 uNewLoadOutNameHash);

	// Remove an alias for this loadout
	static void RemoveLoadOutAlias(u32 uLoadOutNameHash);

	// Check if a given weapon has the EquipThisWeapon flag set, in the given loadout.
	static bool LoadOutShouldEquipWeapon(u32 uLoadOutNameHash, u32 uWeaponHash);

	// Check if a loadout exists
	static bool HasLoadOut(u32 uLoadOutNameHash);

	//Append additional loadouts 
	static bool Append(const char *fname);

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load();

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

#if __BANK
	// Debug functions
	static void InitLoadOutNames();
	static void DebugGiveLoadOutCB();
#endif // __BANK

	// Find a loadout with the given hash, with support for aliases.
	static CPedInventoryLoadOut* FindLoadOut(u32 uLoadOutNameHash);

	//
	// Members
	//

	// Load outs
	atArray<CPedInventoryLoadOut> m_LoadOuts;

	// Load out aliasing for script overrides
	struct sLoadOutAlias
	{
		sLoadOutAlias() {}
		sLoadOutAlias(u32 uOriginalLoadOutNameHash, u32 uNewLoadOutNameHash) : uOriginalLoadOutNameHash(uOriginalLoadOutNameHash), uNewLoadOutNameHash(uNewLoadOutNameHash) {}
		u32 uOriginalLoadOutNameHash;
		u32 uNewLoadOutNameHash;
	};
	atArray<sLoadOutAlias> m_LoadOutAliases;

	// Static manager object
	static CPedInventoryLoadOutManager ms_Instance;

#if __BANK
	// Debug members
	typedef atArray<const char*> LoadOutNames;
	static LoadOutNames ms_LoadOutNames;
	static s32 ms_iLoadOutComboIndex;
#endif // __BANK

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

#endif // PED_INVENTORY_LOADOUT_H
