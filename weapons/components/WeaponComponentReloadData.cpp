//
// weapons/weaponcomponentreloaddata.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentReloadData.h"

WEAPON_OPTIMISATIONS()
INSTANTIATE_RTTI_CLASS(CWeaponComponentReloadData,0x18C29BB);
INSTANTIATE_RTTI_CLASS(CWeaponComponentReloadLoopedData,0xD9949A9F);

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentReloadData::CWeaponComponentReloadData() {}

void CWeaponComponentReloadData::GetReloadClipIds(s32 iFlags, fwMvClipId& pedClipId, fwMvClipId& weaponClipId) const
{
	if(iFlags & RF_FacingLeftInCover)
	{
		if(iFlags & RF_OutOfAmmo && iFlags & RF_LoopedReload)
		{
			pedClipId = m_PedLowCoverReloadEmptyClipId;
			weaponClipId = m_WeaponLowCoverReloadEmptyClipId;
		}
		else
		{
			pedClipId = m_PedLowLeftCoverReloadClipId;
			weaponClipId = m_WeaponLowLeftCoverReloadClipId;
		}
	}
	else if(iFlags & RF_FacingRightInCover)
	{
		if(iFlags & RF_OutOfAmmo && iFlags & RF_LoopedReload)
		{			
			pedClipId = m_PedLowCoverReloadEmptyClipId;
			weaponClipId = m_WeaponLowCoverReloadEmptyClipId;
		}
		else
		{
			pedClipId = m_PedLowRightCoverReloadClipId;
			weaponClipId = m_WeaponLowRightCoverReloadClipId;
		}
	}
	else if(iFlags & RF_Idle)
	{
		if(iFlags & RF_OutOfAmmo)
		{
			pedClipId = m_PedIdleReloadEmptyClipId;
			weaponClipId = m_WeaponIdleReloadEmptyClipId;
		}
		else
		{
			pedClipId = m_PedIdleReloadClipId;
			weaponClipId = m_WeaponIdleReloadClipId;
		}
	}
	else
	{
		if(iFlags & RF_OutOfAmmo)
		{
			pedClipId = m_PedAimReloadEmptyClipId;
			weaponClipId = m_WeaponAimReloadEmptyClipId;
		}
		else
		{
			pedClipId = m_PedAimReloadClipId;
			weaponClipId = m_WeaponAimReloadClipId;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
