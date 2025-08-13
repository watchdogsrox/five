//
// weapons/weaponcomponentclip.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_CLIP_H
#define WEAPON_COMPONENT_CLIP_H

// Rage headers
#include "atl/hashstring.h"

// Game headers
#include "Weapons/Components/WeaponComponent.h"

// Forward declarations
class CWeaponComponentData;

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentClipInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentClipInfo, CWeaponComponentInfo, WCT_Clip);
public:

	CWeaponComponentClipInfo();
	virtual ~CWeaponComponentClipInfo();

	// PURPOSE: Get the ammo for the clip
	s32 GetClipSize() const;

	// PURPOSE: Get reload data
	const CWeaponComponentData* GetReloadData() const;

	// PURPOSE: Get ammo info hash
	u32 GetAmmoInfo() const;

	// PURPOSE: Get the overriden tracer particle effect
	atHashWithStringNotFinal GetTracerFxHashName() const { return m_TracerFx; }

	// PURPOSE: How often the tracer plays (every N shots)
	s32 GetTracerFxSpacing() const { return m_TracerFxSpacing; }

	// PURPOSE: Force tracers for the last N shots
	s32 GetTracerFxForceLast() const { return m_TracerFxForceLast; }

	// PURPOSE: Get the overriden shell eject particle effect
	atHashWithStringNotFinal GetShellFxHashName() const { return m_ShellFx; }

	// PURPOSE: Get the overriden shell eject particle effect for first person
	atHashWithStringNotFinal GetShellFxFPHashName() const { return m_ShellFxFP; }

	// PURPOSE: Get the overriden number of bullets in batch when firing
	u32 GetBulletsInBatch() const { return m_BulletsInBatch; }

private:

	//
	// Members
	//

	// PURPOSE: Maximum amount of ammo in clip
	s32 m_ClipSize;

	// PURPOSE: Any reload component info associated with this clip
	const CWeaponComponentData* m_ReloadData;

	// PURPOSE: An ammo info hash that overrides the base weapon's ammo info
	atHashWithStringNotFinal m_AmmoInfo;

	// PURPOSE: The tracer particle effect name that overrides any on the weapon
	atHashWithStringNotFinal m_TracerFx;

	// PURPOSE: How often the tracer plays (every N shots)
	s32 m_TracerFxSpacing;

	// PURPOSE: Force tracers for the last N shots
	s32 m_TracerFxForceLast;

	// PURPOSE: The shell eject particle effect name that overrides any on the weapon
	atHashWithStringNotFinal m_ShellFx;

	// PURPOSE: The shell eject particle effect name for first person that overrides any on the weapon
	atHashWithStringNotFinal m_ShellFxFP;

	// PURPOSE: Overrides the number of bullets in batch defined in WeaponInfo
	u32 m_BulletsInBatch;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentClip : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentClip, CWeaponComponent, WCT_Clip);
public:

	CWeaponComponentClip();
	CWeaponComponentClip(const CWeaponComponentClipInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentClip();

	// PURPOSE: Get the ammo for the clip
	s32 GetClipSize() const;

	// PURPOSE: Get the reload data for the clip
	const CWeaponComponentData* GetReloadData() const;

	// PURPOSE: Get ammo info hash
	u32 GetAmmoInfo() const;

	// PURPOSE: Access the const info
	const CWeaponComponentClipInfo* GetInfo() const;
};

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeaponComponentClipInfo::GetClipSize() const
{
	return m_ClipSize;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentData* CWeaponComponentClipInfo::GetReloadData() const
{
	return m_ReloadData;
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentClipInfo::GetAmmoInfo() const
{
	return m_AmmoInfo;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeaponComponentClip::GetClipSize() const
{
	return GetInfo()->GetClipSize();
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentData* CWeaponComponentClip::GetReloadData() const
{
	return GetInfo()->GetReloadData();
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentClip::GetAmmoInfo() const
{
	return GetInfo()->GetAmmoInfo();
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentClipInfo* CWeaponComponentClip::GetInfo() const
{
	return static_cast<const CWeaponComponentClipInfo*>(superclass::GetInfo());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_CLIP_H
