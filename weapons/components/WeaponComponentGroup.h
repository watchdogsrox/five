//
// weapons/weaponcomponentgroup.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_GROUP_H
#define WEAPON_COMPONENT_GROUP_H

// Game headers
#include "Weapons/Components/WeaponComponent.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentGroupInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentGroupInfo, CWeaponComponentInfo, WCT_Group);
public:

	// The maximum infos in a group
	static const s32 MAX_INFOS = 2;

	typedef atFixedArray<CWeaponComponentInfo*, MAX_INFOS> Infos;

	CWeaponComponentGroupInfo();
	virtual ~CWeaponComponentGroupInfo();

	// PURPOSE: Get the component info array
	const Infos& GetInfos() const;

private:

	//
	// Members
	//

	Infos m_Infos;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentGroup : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentGroup, CWeaponComponent, WCT_Group);
public:

	typedef atFixedArray<CWeaponComponent*, CWeaponComponentGroupInfo::MAX_INFOS> Components;

	CWeaponComponentGroup();
	CWeaponComponentGroup(const CWeaponComponentGroupInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentGroup();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Modify the passed in accuracy
	virtual void ModifyAccuracy(sWeaponAccuracy& inOutAccuracy);

	// PURPOSE: Modify the passed in damage
	virtual void ModifyDamage(f32& inOutDamage);

#if __BANK
	// PURPOSE: Debug rendering
	virtual void RenderDebug(CEntity* pFiringEntity) const;
#endif // __BANK

	// PURPOSE: Get the components in the group
	const Components& GetComponents() const;

private:

	//
	// Members
	//

	Components m_Components;
};

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentGroupInfo::Infos& CWeaponComponentGroupInfo::GetInfos() const
{
	return m_Infos;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentGroup::Components& CWeaponComponentGroup::GetComponents() const
{
	return m_Components;
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_GROUP_H
