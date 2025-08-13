//
// weapons/pedinventoryloadout.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Inventory/PedInventoryLoadOut.h"

// Rage headers
#include "parser/manager.h"

// Game headers
#include "Control/replay/Replay.h"
#include "control/replay/ReplaySettings.h"
#include "Peds/Ped.h"
#include "Peds/PedType.h"
#include "Scene/DataFileMgr.h"
#include "Weapons/WeaponDebug.h"

// Parser headers
#include "PedInventoryLoadOut_parser.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

bool CPedInventoryLoadOut::ms_bDontFreeMemory = false;

////////////////////////////////////////////////////////////////////////////////

CLoadOutItem::CLoadOutItem()
{
}

////////////////////////////////////////////////////////////////////////////////

CLoadOutItem::~CLoadOutItem()
{
}

////////////////////////////////////////////////////////////////////////////////

CLoadOutWeapon::CLoadOutWeapon()
{
}

////////////////////////////////////////////////////////////////////////////////

void CLoadOutWeapon::AddToPed(CPed* pPed) const
{
	weaponDebugf1("--Attempting to add Weapon %s to Ped 0x%p [%s]", m_WeaponName.TryGetCStr(), pPed, pPed->GetModelName());
	weaponAssert(pPed->GetInventory());

	// Skip weapons flagged as SP only if we're in MP
	if(NetworkInterface::IsGameInProgress() && (m_Flags & FL_SPOnly))
	{
		weaponDebugf1("----Rejecting Weapon due to SPOnly");
		return;
	}
	// Skip weapons flagged as MP only if we're in SP
	else if(!NetworkInterface::IsGameInProgress() && (m_Flags & FL_MPOnly))
	{
		weaponDebugf1("----Rejecting Weapon due to MPOnly");
		return;
	}

	//Add the weapon and ammo.
	if(!pPed->GetInventory()->AddWeaponAndAmmo(m_WeaponName.GetHash(), m_Ammo))
	{
		weaponDebugf1("----Failed to add weapon to ped");
	}
	
	//Add the components.
	for(int i = 0; i < m_ComponentNames.GetCount(); ++i)
	{
		pPed->GetInventory()->GetWeaponRepository().AddComponent(m_WeaponName.GetHash(), m_ComponentNames[i].GetHash());
	}

	// If we equipped a special ammo magazine as part of the loadout, give the ammo amount again into the right pool
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_WeaponName.GetHash());
	const CAmmoInfo* pAmmoInfo = pWeaponInfo ? pWeaponInfo->GetAmmoInfo(pPed) : NULL;
	if (pAmmoInfo && pAmmoInfo->GetIsSpecialType())
	{
		pPed->GetInventory()->AddAmmo(pAmmoInfo->GetHash(), m_Ammo);
	}

	// Equip?
	if(m_Flags & FL_EquipThisWeapon)
	{
		weaponAssert(pPed->GetWeaponManager());
		pPed->GetWeaponManager()->EquipWeapon(m_WeaponName.GetHash(), -1, pPed->GetVehiclePedInside() == NULL);
	}
	else if(m_Flags & FL_SelectThisWeapon)
	{
		weaponAssert(pPed->GetWeaponManager());
		pPed->GetWeaponManager()->EquipWeapon(m_WeaponName.GetHash());
	}

	// Infinite ammo
	if(m_Flags & FL_InfiniteAmmo)
	{
		pPed->GetInventory()->SetWeaponUsingInfiniteAmmo(m_WeaponName.GetHash(), true);
	}
}

////////////////////////////////////////////////////////////////////////////////

CLoadOutRandom::CLoadOutRandom()
: m_fTotalChance(0.f)
{
}

////////////////////////////////////////////////////////////////////////////////

CLoadOutRandom::~CLoadOutRandom()
{
	for(s32 i = 0; i < m_Items.GetCount(); i++)
	{
		delete m_Items[i].Item;
	}
	m_Items.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CLoadOutRandom::AddToPed(CPed* pPed) const
{
	if(m_Items.GetCount() > 0)
	{
		f32 fRandomChance = fwRandom::GetRandomNumberInRange(0.f, m_fTotalChance);
		for(s32 i = 0; i < m_Items.GetCount(); i++)
		{
			if(fRandomChance <= m_Items[i].Chance)
			{
				if(weaponVerifyf(m_Items[i].Item, "Loadout item is NULL"))
				{
					m_Items[i].Item->AddToPed(pPed);
				}
				return;
			}
			fRandomChance -= m_Items[i].Chance;
		}

		// Ensure we pick something
		m_Items[m_Items.GetCount()-1].Item->AddToPed(pPed);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CLoadOutRandom::ShouldEquipWeapon(u32 uWeaponHash) const
{
	for(s32 i = 0; i < m_Items.GetCount(); i++)
	{
		if(m_Items[i].Item->ShouldEquipWeapon(uWeaponHash))
		{
			return true;
		}
	}
	return false;
}

void CLoadOutRandom::PostLoad()
{
	// Tot up the total chance
	m_fTotalChance = 0.f;
	for(s32 i = 0; i < m_Items.GetCount(); i++)
	{
		m_fTotalChance += m_Items[i].Chance;
	}
}

////////////////////////////////////////////////////////////////////////////////

CPedInventoryLoadOut::CPedInventoryLoadOut()
{
}

////////////////////////////////////////////////////////////////////////////////

CPedInventoryLoadOut& CPedInventoryLoadOut::operator=(const CPedInventoryLoadOut& rhs )
{
	if(this == &rhs)
		return *this;

	m_Name = rhs.m_Name;
	m_Items = rhs.m_Items;

	return *this;
}

////////////////////////////////////////////////////////////////////////////////

CPedInventoryLoadOut::~CPedInventoryLoadOut()
{
	//This was necessary when implementing the ability to add new loadouts through DLC mount interface,
	//so existing elements wouldn't be destroyed when growing and reallocating the array of loadouts
	if (!ms_bDontFreeMemory)
		Reset();
}

////////////////////////////////////////////////////////////////////////////////
	
void CPedInventoryLoadOut::AddToPed(CPed* pPed) const
{
	weaponDebugf1("Setting LoadOut %s on Ped 0x%p [%s]", GetName(), pPed, pPed->GetModelName());
	for(s32 i = 0; i < m_Items.GetCount(); i++)
	{
		m_Items[i]->AddToPed(pPed);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOut::Reset()
{
	for(s32 i = 0; i < m_Items.GetCount(); i++)
	{
		delete m_Items[i];
	}
	m_Items.Reset();
}

////////////////////////////////////////////////////////////////////////////////

bool CPedInventoryLoadOut::ShouldEquipWeapon(u32 uWeaponHash) const
{
	const int numItems = m_Items.GetCount();
	for(int i = 0; i < numItems; i++)
	{
		if(m_Items[i]->ShouldEquipWeapon(uWeaponHash))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOut::MergeLoadout(CPedInventoryLoadOut& loadOut)
{
	const int numItemsToLoad = loadOut.m_Items.GetCount();
	for(int i = 0; i < numItemsToLoad; i++)
	{
		CLoadOutItem* weapon = loadOut.m_Items[i];
		//if weapon isn't in loadout already
		if(!ShouldEquipWeapon(weapon->GetHash()))
		{
			//add item
			m_Items.PushAndGrow(weapon);
			loadOut.m_Items[i] = NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOut::CopyLoadout(CPedInventoryLoadOut& loadOut)
{
	m_Name = loadOut.m_Name;
	m_Items.Assume(loadOut.m_Items);
}


////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

// Statics
CPedInventoryLoadOutManager CPedInventoryLoadOutManager::ms_Instance;

#if __BANK
CPedInventoryLoadOutManager::LoadOutNames CPedInventoryLoadOutManager::ms_LoadOutNames;
s32 CPedInventoryLoadOutManager::ms_iLoadOutComboIndex = 0;
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOutManager::Init()
{
	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOutManager::Shutdown()
{
	// Delete any existing data
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

bool CPedInventoryLoadOutManager::SetDefaultLoadOut(CPed* pPed, const CPedModelInfo* pPassedPedModelInfo)
{
	static const atHashWithStringNotFinal DEFAULT_LOADOUT("LOADOUT_DEFAULT",0xAFEF2A99);

	if( weaponVerifyf(pPed, "pPed is NULL") && pPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		// Use the supplied ped model info as an override - if it is NULL then lookup the model.
		// See B* 2039015 - the inventory is set before the ped model index is set, so we won't have that value yet otherwise.
		const CPedModelInfo* pPedModelInfo = pPassedPedModelInfo ? pPassedPedModelInfo : pPed->GetPedModelInfo();
		if(pPedModelInfo)
		{
			return SetLoadOut(pPed, pPedModelInfo->GetPersonalitySettings().GetDefaultWeaponLoadoutHash());
		}
	}
	
	return SetLoadOut(pPed, DEFAULT_LOADOUT.GetHash());
}

////////////////////////////////////////////////////////////////////////////////

bool CPedInventoryLoadOutManager::SetLoadOut(CPed* pPed, u32 uLoadOutNameHash)
{
#if GTA_REPLAY
	// Replay doesn't want peds with non recorded weapons, force default loadout.
	if(CReplayMgr::IsReplayInControlOfWorld() && pPed && pPed->GetPedType() != PEDTYPE_ANIMAL)
	{
		static const atHashWithStringNotFinal DEFAULT_LOADOUT("LOADOUT_DEFAULT",0xAFEF2A99);
		uLoadOutNameHash = DEFAULT_LOADOUT.GetHash();
	}
#endif

	if(weaponVerifyf(pPed, "pPed is NULL") && pPed->GetInventory() && uLoadOutNameHash != 0)
	{
		const CPedInventoryLoadOut* pLoadOut = FindLoadOut(uLoadOutNameHash);
		if(weaponVerifyf(pLoadOut, "Unable to find loadout: %s.", atHashWithStringNotFinal(uLoadOutNameHash).GetCStr()))
		{
			// Clear existing items
			weaponAssert(pPed->GetInventory());
			pPed->GetInventory()->RemoveAll();

			// Add new load out
			pLoadOut->AddToPed(pPed);
			
			// Check if the weapon manager is valid
			CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
			if(pWeaponManager)
			{
				// Check if a weapon is equipped
				if(pWeaponManager->GetEquippedWeaponHash() == 0)
				{
					// Equip unarmed, by default
					pWeaponManager->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);
				}
			}
			
			return true;
		}
	}

	// No load out found with this hash
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOutManager::AddLoadOutAlias(u32 uOriginalLoadOutNameHash, u32 uNewLoadOutNameHash)
{
	// Check if there is already an alias
	for(s32 i = 0; i < ms_Instance.m_LoadOutAliases.GetCount(); i++)
	{
		if(ms_Instance.m_LoadOutAliases[i].uOriginalLoadOutNameHash == uOriginalLoadOutNameHash)
		{
			// Just replace what this points at
			ms_Instance.m_LoadOutAliases[i].uNewLoadOutNameHash = uNewLoadOutNameHash;
			return;
		}
	}

	// Add a new entry
	ms_Instance.m_LoadOutAliases.PushAndGrow(sLoadOutAlias(uOriginalLoadOutNameHash, uNewLoadOutNameHash));
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOutManager::RemoveLoadOutAlias(u32 uLoadOutNameHash)
{
	// Check if there is already an alias
	for(s32 i = 0; i < ms_Instance.m_LoadOutAliases.GetCount(); i++)
	{
		if(ms_Instance.m_LoadOutAliases[i].uOriginalLoadOutNameHash == uLoadOutNameHash)
		{
			ms_Instance.m_LoadOutAliases.DeleteFast(i);
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CPedInventoryLoadOutManager::LoadOutShouldEquipWeapon(u32 uLoadOutNameHash, u32 uWeaponHash)
{
	const CPedInventoryLoadOut* pLoadOut = CPedInventoryLoadOutManager::FindLoadOut(uLoadOutNameHash);
	if(pLoadOut)
	{
		return pLoadOut->ShouldEquipWeapon(uWeaponHash);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CPedInventoryLoadOutManager::HasLoadOut(u32 uLoadOutNameHash)
{
	return (FindLoadOut(uLoadOutNameHash) != NULL);
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CPedInventoryLoadOutManager::AddWidgets(bkBank& bank)
{
	// Update the names list
	InitLoadOutNames();

	bank.PushGroup("Load Outs");

	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	if(ms_LoadOutNames.GetCount() > 0)
	{
		bank.AddCombo("Load Out", &ms_iLoadOutComboIndex, ms_LoadOutNames.GetCount(), &ms_LoadOutNames[0]);
		bank.AddButton("Give Load Out", DebugGiveLoadOutCB);
	}

	bank.PushGroup("Load Outs");
	for(s32 i = 0; i < ms_Instance.m_LoadOuts.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_LoadOuts[i].GetName());
		PARSER.AddWidgets(bank, &ms_Instance.m_LoadOuts[i]);
		bank.PopGroup();
	}
	bank.PopGroup(); // "Load Outs"

	bank.PopGroup(); // "Load Outs"
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOutManager::Reset()
{
	// Delete data
	for(s32 i = 0; i < ms_Instance.m_LoadOuts.GetCount(); i++)
	{
		ms_Instance.m_LoadOuts[i].Reset();
	}
	ms_Instance.m_LoadOuts.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CPedInventoryLoadOutManager::Load()
{
	// Delete any existing data
	Reset();

	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::LOADOUTS_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// Load
		PARSER.LoadObject(pData->m_filename, "meta", ms_Instance);

		// Get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

#if __BANK
	// Init the widgets
	CWeaponDebug::InitBank();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CPedInventoryLoadOutManager::Save()
{
	weaponVerifyf(PARSER.SaveObject("common:/data/ai/loadouts", "meta", &ms_Instance, parManager::XML), "Failed to save loadouts");
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CPedInventoryLoadOutManager::InitLoadOutNames()
{
	// Clear the array of names
	ms_LoadOutNames.Reset();

	for(s32 i = 0; i < ms_Instance.m_LoadOuts.GetCount(); i++)
	{
		ms_LoadOutNames.PushAndGrow(ms_Instance.m_LoadOuts[i].GetName());
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CPedInventoryLoadOutManager::DebugGiveLoadOutCB()
{
	if(ms_iLoadOutComboIndex >= 0 && ms_iLoadOutComboIndex < ms_LoadOutNames.GetCount())
	{
		CPed* pFocusPed = CWeaponDebug::GetFocusPed();
		if(pFocusPed)
		{
			SetLoadOut(pFocusPed, atStringHash(ms_LoadOutNames[ms_iLoadOutComboIndex]));
		}
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

bool CPedInventoryLoadOutManager::Append(const char *fname)
{
	CPedInventoryLoadOutManager temp_inst;

	if(Verifyf(PARSER.LoadObject(fname, NULL, temp_inst), "Load %s failed!\n", fname))
	{
		CPedInventoryLoadOut* loudOut;

		CPedInventoryLoadOut::ms_bDontFreeMemory = true;
		for(int i = 0; i < temp_inst.m_LoadOuts.GetCount(); i++)
		{
			loudOut = FindLoadOut(temp_inst.m_LoadOuts[i].GetHash());
			if(loudOut!=NULL)
			{
				loudOut->MergeLoadout(temp_inst.m_LoadOuts[i]);
			}
			else
			{
				ms_Instance.m_LoadOuts.Grow().CopyLoadout(temp_inst.m_LoadOuts[i]);
			}
		}
		CPedInventoryLoadOut::ms_bDontFreeMemory = false;
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CPedInventoryLoadOut* CPedInventoryLoadOutManager::FindLoadOut(u32 uLoadOutNameHash)
{
	if(uLoadOutNameHash != 0)
	{
		// Is this aliased?
		for(s32 i = 0; i < ms_Instance.m_LoadOutAliases.GetCount(); i++)
		{
			if(ms_Instance.m_LoadOutAliases[i].uOriginalLoadOutNameHash == uLoadOutNameHash)
			{
				uLoadOutNameHash = ms_Instance.m_LoadOutAliases[i].uNewLoadOutNameHash;
				break;
			}
		}

		// Find the load out
		for(s32 i = 0; i < ms_Instance.m_LoadOuts.GetCount(); i++)
		{
			if(uLoadOutNameHash == ms_Instance.m_LoadOuts[i].GetHash())
			{
				return &ms_Instance.m_LoadOuts[i];
			}
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
