//
// weapons/weaponcomponentvariantmodel.h
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_VARIANT_MODEL_H
#define WEAPON_COMPONENT_VARIANT_MODEL_H

// Game headers
#include "Weapons/Components/WeaponComponent.h"


////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentVariantModelInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentVariantModelInfo, CWeaponComponentInfo, WCT_VariantModel);
public:

	CWeaponComponentVariantModelInfo();
	virtual ~CWeaponComponentVariantModelInfo();

	// PURPOSE: Specifies override tint value for weapon when using this variant model component.
	s8 GetTintIndexOverride() const { return m_TintIndexOverride; }

	// PURPOSE: Returns the number of entries in the extra components array
	s32 GetExtraComponentCount() const { return m_ExtraComponents.GetCount(); }

	// PURPOSE: Returns a model hash for an additional weapon component variation, if defined.
	u32 GetExtraComponentModel(u32 weaponComponentName) const;
	u32 GetExtraComponentModelByIndex(s32 extraComponentIndex) const { return m_ExtraComponents[extraComponentIndex].m_ComponentModel; }

	// VariantModelExtras structure
	struct sVariantModelExtraComponents
	{
		atHashString m_ComponentName;
		atHashString m_ComponentModel;
		PAR_SIMPLE_PARSABLE;
	};

private:

	//
	// Members
	//
	s8 m_TintIndexOverride;
	atArray<sVariantModelExtraComponents> m_ExtraComponents;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentVariantModel : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentVariantModel, CWeaponComponent, WCT_VariantModel);
public:

	CWeaponComponentVariantModel();
	CWeaponComponentVariantModel(const CWeaponComponentVariantModelInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentVariantModel();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Access the const info
	const CWeaponComponentVariantModelInfo* GetInfo() const;

private:

	//
	// Members
	//

};
////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentVariantModelInfo* CWeaponComponentVariantModel::GetInfo() const
{
	return static_cast<const CWeaponComponentVariantModelInfo*>(superclass::GetInfo());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_LASER_SIGHT_H
