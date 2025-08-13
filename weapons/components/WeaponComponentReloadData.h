//
// weapons/weaponcomponentreloaddata.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_RELOAD_DATA_H
#define WEAPON_COMPONENT_RELOAD_DATA_H

// Rage headers
#include "fwanimation/animdefines.h"

// Game headers
#include "Weapons/Components/WeaponComponentData.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentReloadData : public CWeaponComponentData
{
	DECLARE_RTTI_DERIVED_CLASS(CWeaponComponentReloadData, CWeaponComponentData);
public:
	CWeaponComponentReloadData();

	enum eReloadFlags
	{
		RF_Idle					= BIT0,
		RF_OutOfAmmo			= BIT1,
		RF_FacingLeftInCover	= BIT2,
		RF_FacingRightInCover	= BIT3,
		RF_LoopedReload			= BIT4,
	};

	// PURPOSE: Get the appropriate reload clip
	void GetReloadClipIds(s32 iFlags, fwMvClipId& pedClipId, fwMvClipId& weaponClipId) const;

	// PURPOSE: Get the reload animation rate modifier
	float GetReloadAnimRateModifier() const {return m_AnimRateModifier;}

private:

	// PURPOSE: Clip ids
	fwMvClipId m_PedIdleReloadClipId;
	fwMvClipId m_PedIdleReloadEmptyClipId;
	fwMvClipId m_PedAimReloadClipId;
	fwMvClipId m_PedAimReloadEmptyClipId;
	fwMvClipId m_PedLowCoverReloadEmptyClipId;
	fwMvClipId m_PedLowLeftCoverReloadClipId;
	fwMvClipId m_PedLowRightCoverReloadClipId;
	fwMvClipId m_WeaponIdleReloadClipId;
	fwMvClipId m_WeaponIdleReloadEmptyClipId;
	fwMvClipId m_WeaponAimReloadClipId;
	fwMvClipId m_WeaponAimReloadEmptyClipId;
	fwMvClipId m_WeaponLowCoverReloadEmptyClipId;
	fwMvClipId m_WeaponLowLeftCoverReloadClipId;
	fwMvClipId m_WeaponLowRightCoverReloadClipId;

	// PURPOSE: Reload rate modifier
	float m_AnimRateModifier;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentReloadLoopedData : public CWeaponComponentData
{
	DECLARE_RTTI_DERIVED_CLASS(CWeaponComponentReloadLoopedData, CWeaponComponentData);
public:

	// PURPOSE: Type of sections
	enum LoopSection
	{
		Intro,
		Loop,
		Outro,

		MaxLoopSections,
	};

	// PURPOSE: Get a one shot section
	const CWeaponComponentReloadData& GetSection(LoopSection section) const;

private:

	// PURPOSE: Section storage
	CWeaponComponentReloadData m_Sections[MaxLoopSections];

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentReloadData& CWeaponComponentReloadLoopedData::GetSection(LoopSection section) const
{
	return m_Sections[section];
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_RELOAD_DATA_H
