//
// weapons/weaponcomponentvariantmodel.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentVariantModel.h"

// Game headers


WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentVariantModelInfo::CWeaponComponentVariantModelInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentVariantModelInfo::~CWeaponComponentVariantModelInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponComponentVariantModelInfo::GetExtraComponentModel(u32 weaponComponentHash) const
{
	for (int i = 0; i < m_ExtraComponents.GetCount(); i++)
	{
		if (weaponComponentHash == m_ExtraComponents[i].m_ComponentName.GetHash())
		{
			return m_ExtraComponents[i].m_ComponentModel;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentVariantModel::CWeaponComponentVariantModel()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentVariantModel::CWeaponComponentVariantModel(const CWeaponComponentVariantModelInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
	: superclass(pInfo, pWeapon, pDrawable)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentVariantModel::~CWeaponComponentVariantModel()
{
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentVariantModel::Process(CEntity* UNUSED_PARAM(pFiringEntity))
{

}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentVariantModel::ProcessPostPreRender(CEntity* UNUSED_PARAM(pFiringEntity))
{
}

////////////////////////////////////////////////////////////////////////////////