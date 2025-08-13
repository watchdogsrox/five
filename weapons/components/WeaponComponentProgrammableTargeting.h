//
// weapons/weaponcomponentprogrammabletargeting.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_PROGRAMMABLE_TARGETING_H
#define WEAPON_COMPONENT_PROGRAMMABLE_TARGETING_H

// Game headers
#include "Weapons/Components/WeaponComponent.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentProgrammableTargetingInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentProgrammableTargetingInfo, CWeaponComponentInfo, WCT_ProgrammableTargeting);
public:

	CWeaponComponentProgrammableTargetingInfo();
	virtual ~CWeaponComponentProgrammableTargetingInfo();

private:

	//
	// Members
	//

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentProgrammableTargeting : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentProgrammableTargeting, CWeaponComponent, WCT_ProgrammableTargeting);
public:

	CWeaponComponentProgrammableTargeting();
	CWeaponComponentProgrammableTargeting(const CWeaponComponentProgrammableTargetingInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentProgrammableTargeting();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Access the const info
	const CWeaponComponentProgrammableTargeting* GetInfo() const;

	// PURPOSE: Get the target range - this is what we "program"
	f32 GetRange() const;

	// PURPOSE: Query if the range is set
	bool GetIsRangeSet() const;

	// PURPOSE: Set the range
	void SetRange();

	// PURPOSE: Clear the current set range
	void ClearSetRange();

#if __BANK
	// PURPOSE: Debug rendering
	virtual void RenderDebug(CEntity* pFiringEntity) const;
#endif // __BANK

private:

	//
	// Members
	//

	// PURPOSE: The target range - this is what we "program"
	f32 m_Range;

	// PURPOSE: Should we set the range?
	bool m_bSetTheRange;
};

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeaponComponentProgrammableTargeting::GetRange() const
{
	return m_Range;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeaponComponentProgrammableTargeting::GetIsRangeSet() const
{
	return m_Range >= 0.f;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponentProgrammableTargeting::SetRange()
{
	m_bSetTheRange = true;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponentProgrammableTargeting::ClearSetRange()
{
	m_Range = -1.f;
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_PROGRAMMABLE_TARGETING_H
