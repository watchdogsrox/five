//
// weapons/ammoitemrepository.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef AMMO_ITEM_REPOSITORY_H
#define AMMO_ITEM_REPOSITORY_H

// Game headers
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Inventory/AmmoItem.h"
#include "Weapons/Inventory/InventoryItemRepository.h"

//////////////////////////////////////////////////////////////////////////
// CAmmoItemRepository
//////////////////////////////////////////////////////////////////////////

// The maximum number of storable ammos
static const s32 MAX_AMMOS = 65;

class CAmmoItemRepository : public CInventoryItemRepository<CAmmoItem, MAX_AMMOS>
{
public:

	//
	// CAmmoItemRepository
	// 

	CAmmoItemRepository();

	// Add ammo
	const CAmmoItem* AddAmmo(const u32 uAmmoNameHash, const s32 iAmmo, const CPed* pPed);

	// Set ammo amount
	void SetAmmo(const u32 uAmmoNameHash, const s32 iAmmo, const CPed* pPed);

	// Get ammo amount
	s32 GetAmmo(const u32 uAmmoNameHash) const;

	// Have we ran out of ammo
	bool GetIsOutOfAmmo(const u32 uAmmoNameHash) const;

	//
	// Infinite Ammo
	//

	// Set whether we are using infinite ammo
	void SetUsingInfiniteAmmo(const bool bUsingInfiniteAmmo);

	// Set a specific ammo type to use infinite ammo
	void SetUsingInfiniteAmmo(const u32 uAmmoNameHash, const bool bUsingInfiniteAmmo);

	// Set a specific ammo type to ignore any InfiniteAmmo flag set in metadata
	void SetIgnoreInfiniteAmmoFlag(const u32 uAmmoNameHash, const bool bIgnoreFlag);

	// Are we using infinite ammo?
	bool GetIsUsingInfiniteAmmo() const;

	// Is a specific ammo type using infinite ammo?
	bool GetIsUsingInfiniteAmmo(const u32 uAmmoNameHash) const;

	//
	// Infinite Clips
	//

	// Set whether we are using infinite/bottomless clips
	void SetUsingInfiniteClips(const bool bUsingInfiniteClips);

	// Are we using infinite/bottomless clips?
	bool GetIsUsingInfiniteClips() const;

	//
	// Helper functions to lookup ammo hash from weapon info - always pass in the weapon info if available
	//

	// Lookup specified weapon and get the ammo that it uses, then add to that amount
	const CAmmoItem* AddWeaponAmmo(const CWeaponInfo* pWeaponInfo, const s32 iAmmo, const CPed* pPed);
	const CAmmoItem* AddWeaponAmmo(const u32 uWeaponNameHash, const s32 iAmmo, const CPed* pPed);

	// Lookup specified weapon and get the ammo that it uses, then set that amount
	void SetWeaponAmmo(const CWeaponInfo* pWeaponInfo, const s32 iAmmo, const CPed* pPed);
	void SetWeaponAmmo(const u32 uWeaponNameHash, const s32 iAmmo, const CPed* pPed);

	// Lookup specified weapon and get the ammo that it uses, then return that amount
	s32 GetWeaponAmmo(const CWeaponInfo* pWeaponInfo, const CPed* pPed) const;
	s32 GetWeaponAmmo(const u32 uWeaponNameHash, const CPed* pPed) const;

	// Lookup specified weapon and check if the weapon is out of ammo
	bool GetIsWeaponOutOfAmmo(const CWeaponInfo* pWeaponInfo, const CPed* pPed) const;
	bool GetIsWeaponOutOfAmmo(const u32 uWeaponNameHash, const CPed* pPed) const;

	// Set the specified weapon's ammo to be infinite
	void SetWeaponUsingInfiniteAmmo(const CWeaponInfo* pWeaponInfo, const bool bUsingInfiniteAmmo, const CPed* pPed);
	void SetWeaponUsingInfiniteAmmo(const u32 uWeaponNameHash, const bool bUsingInfiniteAmmo, const CPed* pPed);

	// Is the specified weapon's ammo set to be infinite?
	bool GetIsWeaponUsingInfiniteAmmo(const CWeaponInfo* pWeaponInfo, const CPed* pPed) const;
	bool GetIsWeaponUsingInfiniteAmmo(const u32 uWeaponNameHash, const CPed* pPed) const;

#if __BANK
	void RenderAmmoDebug(const Vector3& vTextPos, Color32 colour) const;
#endif

private:

	//
	// Members
	//

	// Flags
	bool m_bUsingInfiniteAmmo	: 1;
	bool m_bUsingInfiniteClips	: 1;
};

////////////////////////////////////////////////////////////////////////////////

inline CAmmoItemRepository::CAmmoItemRepository()
: m_bUsingInfiniteAmmo(false)
, m_bUsingInfiniteClips(false)
{
}

////////////////////////////////////////////////////////////////////////////////

inline void CAmmoItemRepository::SetUsingInfiniteAmmo(const bool bUsingInfiniteAmmo)
{
	m_bUsingInfiniteAmmo = bUsingInfiniteAmmo;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CAmmoItemRepository::GetIsUsingInfiniteAmmo() const
{
	return m_bUsingInfiniteAmmo;
}

////////////////////////////////////////////////////////////////////////////////

inline void CAmmoItemRepository::SetUsingInfiniteClips(const bool bUsingInfiniteClips)
{
	m_bUsingInfiniteClips = bUsingInfiniteClips;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CAmmoItemRepository::GetIsUsingInfiniteClips() const
{
	return m_bUsingInfiniteClips;
}

////////////////////////////////////////////////////////////////////////////////

inline const CAmmoItem* CAmmoItemRepository::AddWeaponAmmo(const CWeaponInfo* pWeaponInfo, const s32 iAmmo, const CPed* pPed)
{
	weaponAssertf(pWeaponInfo, "pWeaponInfo is NULL");
	return (pWeaponInfo && pWeaponInfo->GetAmmoInfo(pPed)) ? AddAmmo(pWeaponInfo->GetAmmoInfo(pPed)->GetHash(), iAmmo, pPed) : NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const CAmmoItem* CAmmoItemRepository::AddWeaponAmmo(const u32 uWeaponNameHash, const s32 iAmmo, const CPed* pPed)
{
	return AddWeaponAmmo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash), iAmmo, pPed);
}

////////////////////////////////////////////////////////////////////////////////

inline void CAmmoItemRepository::SetWeaponAmmo(const CWeaponInfo* pWeaponInfo, const s32 iAmmo, const CPed* pPed)
{
	weaponAssertf(pWeaponInfo, "pWeaponInfo is NULL");
	if(pWeaponInfo && pWeaponInfo->GetAmmoInfo(pPed))
	{
		SetAmmo(pWeaponInfo->GetAmmoInfo(pPed)->GetHash(), iAmmo, pPed);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CAmmoItemRepository::SetWeaponAmmo(const u32 uWeaponNameHash, const s32 iAmmo, const CPed* pPed)
{
	SetWeaponAmmo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash), iAmmo, pPed);
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CAmmoItemRepository::GetWeaponAmmo(const CWeaponInfo* pWeaponInfo, const CPed* pPed) const
{
	if (pWeaponInfo) 
	{
		return pWeaponInfo->GetAmmoInfo(pPed) ? GetAmmo(pWeaponInfo->GetAmmoInfo(pPed)->GetHash()) : 0;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CAmmoItemRepository::GetWeaponAmmo(const u32 uWeaponNameHash, const CPed* pPed) const
{
	return GetWeaponAmmo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash), pPed);
}

////////////////////////////////////////////////////////////////////////////////

inline bool CAmmoItemRepository::GetIsWeaponOutOfAmmo(const CWeaponInfo* pWeaponInfo, const CPed* pPed) const
{
	weaponAssertf(pWeaponInfo, "pWeaponInfo is NULL");
	return (pWeaponInfo && pWeaponInfo->GetAmmoInfo(pPed)) ? GetIsOutOfAmmo(pWeaponInfo->GetAmmoInfo(pPed)->GetHash()) : false;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CAmmoItemRepository::GetIsWeaponOutOfAmmo(const u32 uWeaponNameHash, const CPed* pPed) const
{
	return GetIsWeaponOutOfAmmo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash), pPed);
}

////////////////////////////////////////////////////////////////////////////////

inline void CAmmoItemRepository::SetWeaponUsingInfiniteAmmo(const CWeaponInfo* pWeaponInfo, const bool bUsingInfiniteAmmo, const CPed* pPed)
{
	weaponAssertf(pWeaponInfo, "pWeaponInfo is NULL");
	if(pWeaponInfo && pWeaponInfo->GetAmmoInfo(pPed))
	{
		SetUsingInfiniteAmmo(pWeaponInfo->GetAmmoInfo(pPed)->GetHash(), bUsingInfiniteAmmo);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CAmmoItemRepository::SetWeaponUsingInfiniteAmmo(const u32 uWeaponNameHash, const bool bUsingInfiniteAmmo, const CPed* pPed)
{
	SetWeaponUsingInfiniteAmmo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash), bUsingInfiniteAmmo, pPed);
}

////////////////////////////////////////////////////////////////////////////////

inline bool CAmmoItemRepository::GetIsWeaponUsingInfiniteAmmo(const CWeaponInfo* pWeaponInfo, const CPed* pPed) const
{
	weaponAssertf(pWeaponInfo, "pWeaponInfo is NULL");
	return (pWeaponInfo && pWeaponInfo->GetAmmoInfo(pPed)) ? GetIsUsingInfiniteAmmo(pWeaponInfo->GetAmmoInfo(pPed)->GetHash()) : false;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CAmmoItemRepository::GetIsWeaponUsingInfiniteAmmo(const u32 uWeaponNameHash, const CPed* pPed) const
{
	return GetIsWeaponUsingInfiniteAmmo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash), pPed);
}

////////////////////////////////////////////////////////////////////////////////

#endif // AMMO_ITEM_REPOSITORY_H
