//
// weapons/weaponswapdata.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_SWAP_DATA_H
#define WEAPON_SWAP_DATA_H

// Rage headers
#include "fwanimation/animdefines.h"

// Game headers
#include "Weapons/Components/WeaponComponentData.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponSwapData : public CWeaponComponentData
{
	DECLARE_RTTI_DERIVED_CLASS(CWeaponSwapData, CWeaponComponentData);
public:
	CWeaponSwapData();

	enum eSwapFlags
	{
		SF_Holster				= BIT0,
		SF_UnHolster			= BIT1,
		SF_Crouch				= BIT2,
		SF_InLowCover			= BIT3,
		SF_Discard				= BIT4,
		SF_FacingLeftInCover	= BIT5,
	};

	// PURPOSE: Get the appropriate swap clip for the player
	void GetSwapClipId(s32 iFlags, fwMvClipId& pedClipId) const;

	// PURPOSE: Get the appropriate swap clip for the weapon
	void GetSwapClipIdForWeapon(s32 iFlags, fwMvClipId& pedClipId) const;

	// PURPOSE: Get the playback rate defined in metadata to override default rates
	float GetAnimPlaybackRate() const {return m_AnimPlaybackRate; }

private:

	// PURPOSE: Clip ids
	fwMvClipId m_PedHolsterClipId;
	fwMvClipId m_PedHolsterCrouchClipId;
	fwMvClipId m_PedHolsterCoverClipId;
	fwMvClipId m_PedHolsterDiscardClipId;
	fwMvClipId m_PedHolsterCrouchDiscardClipId;
	fwMvClipId m_PedHolsterWeaponClipId;
	fwMvClipId m_PedUnHolsterClipId;
	fwMvClipId m_PedUnHolsterCrouchClipId;
	fwMvClipId m_PedUnHolsterLeftCoverClipId;
	fwMvClipId m_PedUnHolsterRightCoverClipId;
	fwMvClipId m_PedUnHolsterWeaponClipId;
	float m_AnimPlaybackRate;
	
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_SWAP_DATA_H
