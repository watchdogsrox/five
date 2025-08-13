// File header
#include "Weapons/Inventory/AmmoItem.h"

// Game headers
#include "Weapons/Info/WeaponInfoManager.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CAmmoItem
//////////////////////////////////////////////////////////////////////////

CAmmoItem::CAmmoItem(u32 uKey)
: CInventoryItem(uKey)
, m_iAmmo(0)
, m_bUsingInfiniteAmmo(false)
, m_bIgnoreInfiniteAmmoFlag(false)
{
}

CAmmoItem::CAmmoItem(u32 uKey, u32 uItemHash, const CPed* pPed)
: CInventoryItem(uKey, uItemHash, pPed)
, m_iAmmo(0)
, m_bUsingInfiniteAmmo(false)
, m_bIgnoreInfiniteAmmoFlag(false)
{
#if !__NO_OUTPUT
	weaponFatalAssertf(GetInfo()->GetIsClassId(CAmmoInfo::GetStaticClassId()), "CAmmoItem::CAmmoItem: m_pInfo is not type ammo, ClassId[%d][%s][%s]",GetInfo()->GetClassId().GetHash(),GetInfo()->GetName(),GetInfo()->GetModelName());
#else
	weaponFatalAssertf(GetInfo()->GetIsClassId(CAmmoInfo::GetStaticClassId()), "CAmmoItem::CAmmoItem: m_pInfo is not type ammo, ClassId[%d]",GetInfo()->GetClassId().GetHash());
#endif
}

// Check if the hash is valid
bool CAmmoItem::GetIsHashValid(u32 uItemHash)
{
	const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(uItemHash);
#if !__NO_OUTPUT
	return pInfo && weaponVerifyf(pInfo->GetIsClassId(CAmmoInfo::GetStaticClassId()),"Item hash is not an ammo type ClassId[%d][%s][%s]",pInfo->GetClassId().GetHash(),pInfo->GetName(),pInfo->GetModelName());
#else
	return pInfo && weaponVerifyf(pInfo->GetIsClassId(CAmmoInfo::GetStaticClassId()),"Item hash is not an ammo type ClassId[%d]",pInfo->GetClassId().GetHash());
#endif
}
