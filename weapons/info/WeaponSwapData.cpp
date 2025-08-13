//
// weapons/weaponswapdata.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Info/WeaponSwapData.h"
#include "Weapons/WeaponChannel.h"

WEAPON_OPTIMISATIONS()
INSTANTIATE_RTTI_CLASS(CWeaponSwapData,0x2A13047C);

////////////////////////////////////////////////////////////////////////////////

CWeaponSwapData::CWeaponSwapData() {}

void CWeaponSwapData::GetSwapClipId(s32 iFlags, fwMvClipId& pedClipId) const
{
	weaponAssertf((iFlags & SF_Holster || iFlags & SF_UnHolster) && !(iFlags & SF_Holster && iFlags & SF_UnHolster), "Need to specify holster XOR unholster flag");
	if(iFlags & SF_Holster)
	{
		if(iFlags & SF_Crouch)
		{
			if(iFlags & SF_Discard)
			{
				pedClipId = m_PedHolsterCrouchDiscardClipId;
			}
			else
			{
				pedClipId = m_PedHolsterCrouchClipId;
			}
		}
		else if(iFlags & SF_InLowCover)
		{
			pedClipId = m_PedHolsterCoverClipId;
		}
		else
		{
			if(iFlags & SF_Discard)
			{
				pedClipId = m_PedHolsterDiscardClipId;
			}
			else
			{
				pedClipId = m_PedHolsterClipId;
			}
		}
	}
	else
	{
		if(iFlags & SF_Crouch)
		{
			pedClipId = m_PedUnHolsterCrouchClipId;
		}
		else if(iFlags & SF_InLowCover)
		{
			pedClipId = (iFlags & SF_FacingLeftInCover) ? m_PedUnHolsterLeftCoverClipId : m_PedUnHolsterRightCoverClipId;
		}
		else
		{
			pedClipId = m_PedUnHolsterClipId;
		}
	}
}

void CWeaponSwapData::GetSwapClipIdForWeapon(s32 iFlags, fwMvClipId& pedClipId) const
{
	weaponAssertf((iFlags & SF_Holster || iFlags & SF_UnHolster) && !(iFlags & SF_Holster && iFlags & SF_UnHolster), "Need to specify holster XOR unholster flag");
	if(iFlags & SF_Holster)
	{
		pedClipId = m_PedHolsterWeaponClipId;
	}
	else
	{
		pedClipId = m_PedUnHolsterWeaponClipId;
	}
}

////////////////////////////////////////////////////////////////////////////////
