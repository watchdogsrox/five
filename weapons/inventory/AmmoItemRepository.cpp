//
// weapons/ammoitemrepository.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Inventory/AmmoItemRepository.h"

// Game headers
#include "Weapons/Weapon.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Info/WeaponInfoManager.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CAmmoItemRepository
//////////////////////////////////////////////////////////////////////////

const CAmmoItem* CAmmoItemRepository::AddAmmo(const u32 uAmmoNameHash, const s32 iAmmo, const CPed* pPed)
{
	CAmmoItem* pAmmo = NULL;

	if(uAmmoNameHash != 0)
	{
		pAmmo = GetItem(uAmmoNameHash);
		if(!pAmmo)
		{
			// Ammo doesn't exist yet - attempt to add it
			if(weaponVerifyf(AddItem(uAmmoNameHash, pPed), "Failed to add Ammo with hash [%d]", uAmmoNameHash))
			{
				pAmmo = GetItem(uAmmoNameHash);
			}
		}

		if(pAmmo)
		{
			if((!GetIsUsingInfiniteAmmo() && !pAmmo->GetIsUsingInfiniteAmmo()) || iAmmo > 0)
			{
				pAmmo->AddAmmo(iAmmo, pPed);
			}
		}
	}

	return pAmmo;
}

////////////////////////////////////////////////////////////////////////////////

void CAmmoItemRepository::SetAmmo(const u32 uAmmoNameHash, const s32 iAmmo, const CPed* pPed)
{
	if(uAmmoNameHash != 0)
	{
		CAmmoItem* pAmmo = GetItem(uAmmoNameHash);
		if(pAmmo)
		{
			if((!GetIsUsingInfiniteAmmo() && !pAmmo->GetIsUsingInfiniteAmmo()) || iAmmo > 0)
			{
				pAmmo->SetAmmo(iAmmo, pPed);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

s32 CAmmoItemRepository::GetAmmo(const u32 uAmmoNameHash) const
{
	if(uAmmoNameHash != 0)
	{
		if (GetIsUsingInfiniteAmmo())
		{
			return CWeapon::INFINITE_AMMO;
		}

		const CAmmoItem* pAmmo = GetItem(uAmmoNameHash);
		if(pAmmo)
		{
			s32 iAmmo = pAmmo->GetAmmo();
			if(pAmmo->GetIsUsingInfiniteAmmo() && iAmmo == 0)
			{
				return CWeapon::INFINITE_AMMO;
			}
			else
			{
				return iAmmo;
			}
		}
	}

	// Default to 0 ammo
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

bool CAmmoItemRepository::GetIsOutOfAmmo(const u32 uAmmoNameHash) const
{
	if(uAmmoNameHash == 0)
	{
		// This weapon doesn't use ammo, so it can never run out
		return false;
	}

	return GetAmmo(uAmmoNameHash) == 0;
}

////////////////////////////////////////////////////////////////////////////////

void CAmmoItemRepository::SetUsingInfiniteAmmo(const u32 uAmmoNameHash, const bool bUsingInfiniteAmmo)
{
	if(uAmmoNameHash != 0)
	{
		CAmmoItem* pAmmo = GetItem(uAmmoNameHash);
		if(pAmmo)
		{
			pAmmo->SetIsUsingInfiniteAmmo(bUsingInfiniteAmmo);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CAmmoItemRepository::SetIgnoreInfiniteAmmoFlag(const u32 uAmmoNameHash, const bool bIgnoreFlag)
{
	if (uAmmoNameHash != 0)
	{
		CAmmoItem* pAmmo = GetItem(uAmmoNameHash);
		if (weaponVerifyf(pAmmo, "SetIgnoreInfiniteAmmoFlag - No ammo of this types exists in inventory, please give weapon to ped first"))
		{
			if (weaponVerifyf(pAmmo->GetInfo() && pAmmo->GetInfo()->GetIsUsingInfiniteAmmo(), "SetIgnoreInfiniteAmmoFlag - Cannot call on an ammo type not flagged as InfiniteAmmo in metadata"))
			{
				pAmmo->SetIgnoreInfiniteAmmoFlag(bIgnoreFlag);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CAmmoItemRepository::GetIsUsingInfiniteAmmo(const u32 uAmmoNameHash) const
{
	if(GetIsUsingInfiniteAmmo())
	{
		return true;
	}

	if(uAmmoNameHash != 0)
	{
		const CAmmoItem* pAmmo = GetItem(uAmmoNameHash);
		if(pAmmo)
		{
			return pAmmo->GetIsUsingInfiniteAmmo();
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CAmmoItemRepository::RenderAmmoDebug(const Vector3& vTextPos, Color32 colour) const
{
	char itemText[50];
	s32 iLineCount = 0;

	if(GetIsUsingInfiniteAmmo())
		grcDebugDraw::Text(vTextPos, Color_red, -320, grcDebugDraw::GetScreenSpaceTextHeight() * --iLineCount, "Infinite ammo");

	if(GetIsUsingInfiniteClips())
		grcDebugDraw::Text(vTextPos, Color_orange, -320, grcDebugDraw::GetScreenSpaceTextHeight() * --iLineCount, "Infinite clips");

	iLineCount = 0;

	for(s32 i = 0; i < GetItemCount(); i++)
	{
		const CAmmoItem* pItem = GetItemByIndex(i);
		if(pItem && pItem->GetInfo())
		{
			formatf(itemText, 50, "%-22s %5i%s\n", pItem->GetInfo()->GetName(), pItem->GetAmmo(), pItem->GetIsUsingInfiniteAmmo()?" (inf)":"");

			grcDebugDraw::Text(vTextPos, colour, -320, grcDebugDraw::GetScreenSpaceTextHeight() * iLineCount++, itemText);
		}
	}
}

#endif
