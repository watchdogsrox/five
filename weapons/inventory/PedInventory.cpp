//
// weapons/pedinventory.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Inventory/PedInventory.h"

// Game headers
#include "modelinfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Weapons/Inventory/PedInventoryLoadOut.h"
#include "Weapons/WeaponDebug.h"
#include "Stats/StatsInterface.h"
#if __ASSERT
#include "Stats/StatsMgr.h"
#include "Stats/StatsTypes.h"
#endif 

// Macro to disable optimisations if set
AI_COVER_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPedInventory
//////////////////////////////////////////////////////////////////////////

CPedInventory::CPedInventory()
: m_pPed(NULL)
, m_uCachedRechargeTime(0)
, m_uCachedWeaponState(0)
{
}

void CPedInventory::Init(CPed* pPed, const CPedModelInfo* pPedModelInfo)
{
	// Assign the owner ped
	m_pPed = pPed;

	// Initialise default load out
	CPedInventoryLoadOutManager::SetDefaultLoadOut(m_pPed, pPedModelInfo);
}

void CPedInventory::Process()
{
	if(!m_weaponRepository.GetIsStreamedIn())
	{
		m_weaponRepository.ProcessStreaming();
	}
	if(!m_ammoRepository.GetIsStreamedIn())
	{
		m_ammoRepository.ProcessStreaming();
	}
}

void CPedInventory::RegisterListener(CInventoryListener* pListener)
{
	m_weaponRepository.RegisterListener(pListener);
	m_ammoRepository.RegisterListener(pListener);
}

CWeaponItem* CPedInventory::AddWeapon(u32 uWeaponNameHash)
{
	CWeaponItem* pWeaponItem = m_weaponRepository.GetItem(uWeaponNameHash);
	if(!pWeaponItem)
	{
		pWeaponItem = m_weaponRepository.AddItem(uWeaponNameHash, m_pPed);
		if(pWeaponItem)
		{
			weaponDebugf1("CPedInventory::AddWeapon: [%d][%s]: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), m_pPed, m_pPed->GetModelName());

			if(m_pPed->IsLocalPlayer())
			{
				if(uWeaponNameHash == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3))
				{
					pWeaponItem->SetTimer(m_uCachedRechargeTime);
					pWeaponItem->SetCachedWeaponState(m_uCachedWeaponState);
				}
				
				const CWeaponInfo* pWeaponInfo = pWeaponItem->GetInfo();
				if(pWeaponInfo)
				{
					g_WeaponAudioEntity.AddPlayerInventoryWeapon();
				}
			}
		}
	}

	return pWeaponItem;
}

const CAmmoItem* CPedInventory::AddAmmo(u32 uAmmoNameHash, s32 iAmmo)
{
	if(const CAmmoItem* pAmmoItem = m_ammoRepository.AddAmmo(uAmmoNameHash, iAmmo, m_pPed))
	{
		weaponDebugf1("CPedInventory::AddAmmo: [%d][%s], Ammo:%d: Ped 0x%p [%s]", uAmmoNameHash, atHashString(uAmmoNameHash).TryGetCStr(), iAmmo, m_pPed, m_pPed->GetModelName());
		return pAmmoItem;
	}
	return NULL;
}

void CPedInventory::SetAmmo(u32 uAmmoNameHash, s32 iAmmo)
{
	m_ammoRepository.SetAmmo(uAmmoNameHash, iAmmo, m_pPed);
	weaponDebugf1("CPedInventory::SetAmmo: [%d][%s], Ammo:%d: Ped 0x%p [%s]", uAmmoNameHash, atHashString(uAmmoNameHash).TryGetCStr(), iAmmo, m_pPed, m_pPed->GetModelName());
}

CWeaponItem* CPedInventory::AddWeaponAndAmmo(u32 uWeaponNameHash, s32 iAmmo)
{
	CWeaponItem* pItem = AddWeapon(uWeaponNameHash);
	if(pItem)
	{
		weaponDebugf1("CPedInventory::AddWeaponAndAmmo: [%d][%s], Ammo:%d: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), iAmmo, m_pPed, m_pPed->GetModelName());

		if (iAmmo < 0)	// treat -1 ammo as being a call for infinite ammo
		{
			AddWeaponAmmo(uWeaponNameHash, 0);
			m_ammoRepository.SetWeaponUsingInfiniteAmmo(uWeaponNameHash, true, m_pPed);
		}
		else
			AddWeaponAmmo(uWeaponNameHash, iAmmo);	
	}

	return pItem;
}

const CAmmoItem* CPedInventory::AddWeaponAmmo(u32 uWeaponNameHash, s32 iAmmo)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
#if !__NO_OUTPUT
	const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(m_pPed);
#endif // !__NO_OUTPUT
	weaponDebugf1("CPedInventory::AddWeaponAmmo: [%d][%s], Ammo:%d [%s]: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), iAmmo, pAmmoInfo ? pAmmoInfo->GetName() : "", m_pPed, m_pPed->GetModelName());
	return m_ammoRepository.AddWeaponAmmo(pWeaponInfo, iAmmo, m_pPed);
}

void CPedInventory::SetWeaponAmmo(u32 uWeaponNameHash, s32 iAmmo)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
#if !__NO_OUTPUT
	const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(m_pPed);
#endif // !__NO_OUTPUT
	weaponDebugf1("CPedInventory::SetWeaponAmmo: [%d][%s], Ammo:%d [%s]: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), iAmmo, pAmmoInfo ? pAmmoInfo->GetName() : "", m_pPed, m_pPed->GetModelName());

	m_ammoRepository.SetWeaponAmmo(pWeaponInfo, iAmmo, m_pPed);
}

void CPedInventory::SetWeaponUsingInfiniteAmmo(u32 uWeaponNameHash, bool bInfinite)
{
	m_ammoRepository.SetWeaponUsingInfiniteAmmo(uWeaponNameHash, bInfinite, m_pPed);
}

bool CPedInventory::RemoveWeapon(u32 uWeaponNameHash)
{
	// Cache stungun recharge time and state so we can reapply it when we add the weapon again, url:bugstar:7461554
	if(uWeaponNameHash == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3))
	{
		const CWeaponItem* pWeaponItem = GetWeapon(uWeaponNameHash);
		if(pWeaponItem)
		{
			m_uCachedRechargeTime = pWeaponItem->GetTimer();
			m_uCachedWeaponState = pWeaponItem->GetCachedWeaponState();
		}
	}

	if(m_weaponRepository.RemoveItem(uWeaponNameHash))
	{
		weaponDebugf1("CPedInventory::RemoveWeapon: [%d][%s]: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), m_pPed, m_pPed->GetModelName());
		return true;
	}
	return false;
}

bool CPedInventory::RemoveWeaponAndAmmo(u32 uWeaponNameHash)
{
	if(RemoveWeapon(uWeaponNameHash))
	{
		weaponDebugf1("CPedInventory::RemoveWeaponAndAmmo: [%d][%s]: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), m_pPed, m_pPed->GetModelName());

		if(GetNumWeaponAmmoReferences(uWeaponNameHash) == 0)
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
			if(pWeaponInfo && pWeaponInfo->GetAmmoInfo())
			{
				if(m_ammoRepository.RemoveItem(pWeaponInfo->GetAmmoInfo()->GetHash()))
				{
					weaponDebugf1("CPedInventory::RemoveWeaponAndAmmo: No weapon references left to %s, removed ammo type", pWeaponInfo->GetAmmoInfo()->GetName());					
				}
			}
		}
		return true;
	}
	return false;
}

void CPedInventory::RemoveAllWeaponsExcept(u32 uWeaponNameHash, u32 uNextWeaponNameHash /* = 0 */)
{
	weaponDebugf1("CPedInventory::RemoveAllWeaponsExcept [%d][%s], [%d][%s]: Ped 0x%p [%s]", uWeaponNameHash, atHashString(uWeaponNameHash).TryGetCStr(), uNextWeaponNameHash, atHashString(uNextWeaponNameHash).TryGetCStr(), m_pPed, m_pPed->GetModelName());

	CWeaponItem* pExcludedWeapon	 = NULL;
	if(uWeaponNameHash != 0)
	{
		pExcludedWeapon = GetWeapon(uWeaponNameHash);
	}

	CWeaponItem* pExcludedNextWeapon = NULL;
	if(uNextWeaponNameHash != 0)
	{
		pExcludedNextWeapon = GetWeapon(uNextWeaponNameHash);
	}

	if((!pExcludedWeapon) && (!pExcludedNextWeapon))
	{
		RemoveAll();
	}
	else
	{
		m_weaponRepository.RemoveAllItemsExcept(uWeaponNameHash, uNextWeaponNameHash);

		u32 uAmmoNameHash = 0;
		if(pExcludedWeapon && pExcludedWeapon->GetInfo() && pExcludedWeapon->GetInfo()->GetAmmoInfo(m_pPed))
		{
			uAmmoNameHash = pExcludedWeapon->GetInfo()->GetAmmoInfo(m_pPed)->GetHash();
		}

		u32 uAmmoNextNameHash = 0;
		if(pExcludedNextWeapon && pExcludedNextWeapon->GetInfo() && pExcludedNextWeapon->GetInfo()->GetAmmoInfo(m_pPed))
		{
			uAmmoNextNameHash = pExcludedNextWeapon->GetInfo()->GetAmmoInfo(m_pPed)->GetHash();
		}

		m_ammoRepository.RemoveAllItemsExcept(uAmmoNameHash, uAmmoNextNameHash);
	}
}

void CPedInventory::RemoveAll()
{
	weaponDebugf1("CPedInventory::RemoveAll: Ped 0x%p [%s]", m_pPed, m_pPed->GetModelName());

	m_weaponRepository.RemoveAllItems();
	m_ammoRepository.RemoveAllItems();
}

bool CPedInventory::GetIsStreamedIn(u32 uItemNameHash)
{
	if(m_weaponRepository.GetIsStreamedIn(uItemNameHash) || m_ammoRepository.GetIsStreamedIn(uItemNameHash))
	{
		return true;
	}

	return false;
}

bool CPedInventory::GetIsStreamedIn()
{
	if(!m_weaponRepository.GetIsStreamedIn())
	{
		return false;
	}

	if(!m_ammoRepository.GetIsStreamedIn())
	{
		return false;
	}

	return true;
}

s32 CPedInventory::GetNumWeaponAmmoReferences(u32 uWeaponNameHash) const
{
	s32 iAmmoReferences = 0;
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
	if(pWeaponInfo)
	{
		const atArray<CWeaponItem*>& weaponItems = m_weaponRepository.GetAllItems();
		for(s32 i = 0; i < weaponItems.GetCount(); i++)
		{
			if(weaponItems[i]->GetInfo()->GetAmmoInfo() == pWeaponInfo->GetAmmoInfo())
			{
				iAmmoReferences++;
			}
		}
	}
	return iAmmoReferences;
}

bool CPedInventory::NotifyAmmoChange(u32 uWeaponNameHash, s32 iAmmo)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
	if(!m_ammoRepository.GetIsWeaponUsingInfiniteAmmo(pWeaponInfo, m_pPed))
	{
		m_ammoRepository.SetWeaponAmmo(pWeaponInfo, iAmmo, m_pPed);
		return true;
	}
	return false;
}

bool CPedInventory::NotifyTimerChange(u32 uWeaponNameHash, u32 uTimer)
{
	CWeaponItem* pWeaponItem = GetWeapon(uWeaponNameHash);
	if(pWeaponItem)
	{
		pWeaponItem->SetTimer(uTimer);
		return true;
	}
	return false;
}
