//
// weapons/weaponcomponentsuppressor.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_SUPPRESSOR_H
#define WEAPON_COMPONENT_SUPPRESSOR_H

// Game headers
#include "Weapons/Components/WeaponComponent.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentSuppressorInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentSuppressorInfo, CWeaponComponentInfo, WCT_Suppressor);
public:

	CWeaponComponentSuppressorInfo();
	virtual ~CWeaponComponentSuppressorInfo();

	// PURPOSE: Query if we are only allowed one of this component type
	virtual bool GetIsUnique() const;

	// PURPOSE: Get the muzzle bone helper
	const CWeaponBoneId& GetMuzzleBone() const;

	// PURPOSE: Get the overriden muzzle flash particle effect
	atHashWithStringNotFinal GetMuzzleFlashPtFxHashName() const { return m_FlashFx; }

	bool GetShouldSilence() const { return m_ShouldSilence; }

	float GetRecoilShakeAmplitudeModifier() const { return m_RecoilShakeAmplitudeModifier; }

private:

	//
	// Members
	//

	// PURPOSE: The corresponding bone index for the muzzle bone
	CWeaponBoneId m_MuzzleBone;

	// PURPOSE: The muzzle flash particle effect name that overrides any on the weapon
	atHashWithStringNotFinal m_FlashFx;

	// Allows us to mark some attachments as 'suppressors' but non-silenced (muzzle breaks, compensators, etc.)
	bool m_ShouldSilence;
	
	// Modify vertical recoil shake for muzzle breaks / compensators
	float m_RecoilShakeAmplitudeModifier;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentSuppressor : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentSuppressor, CWeaponComponent, WCT_Suppressor);
public:

	CWeaponComponentSuppressor();
	CWeaponComponentSuppressor(const CWeaponComponentSuppressorInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentSuppressor();

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Get the muzzle bone index
	s32 GetMuzzleBoneIndex() const;

	// PURPOSE: Access the const info
	const CWeaponComponentSuppressorInfo* GetInfo() const;

private:

	//
	// Members
	//

	// PURPOSE: The corresponding bone index for the muzzle bone
	s32 m_iMuzzleBoneIdx;
};

////////////////////////////////////////////////////////////////////////////////

inline bool CWeaponComponentSuppressorInfo::GetIsUnique() const
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponBoneId& CWeaponComponentSuppressorInfo::GetMuzzleBone() const
{
	return m_MuzzleBone;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeaponComponentSuppressor::GetMuzzleBoneIndex() const
{
	return m_iMuzzleBoneIdx;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentSuppressorInfo* CWeaponComponentSuppressor::GetInfo() const
{
	return static_cast<const CWeaponComponentSuppressorInfo*>(superclass::GetInfo());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_SUPPRESSOR_H
