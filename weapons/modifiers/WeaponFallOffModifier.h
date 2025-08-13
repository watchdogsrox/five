//
// weapons/weaponfalloffmodifier.h
//
// Copyright (C) 1999-2017 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_FALLOFF_MODIFIER_H
#define WEAPON_FALLOFF_MODIFIER_H

// Rage headers
#include "parser/macros.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponFallOffModifier
{
public:

	CWeaponFallOffModifier();

	// PURPOSE: Modify the passed in fall-off range
	void ModifyRange(f32& inOutRange) const;

	// PURPOSE: Modify the passed in fall-off damage
	void ModifyDamage(f32& inOutDamage) const;

private:

	//
	// Members
	//

	// PURPOSE: Scale the fall-off range by this value
	f32 m_RangeModifier;

	// PURPOSE: Scale the fall-off damage by this value
	f32 m_DamageModifier;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponFallOffModifier::ModifyRange(f32& inOutRange) const
{
	inOutRange *= m_RangeModifier;
}

inline void CWeaponFallOffModifier::ModifyDamage(f32& inOutDamage) const
{
	inOutDamage *= m_DamageModifier;
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_FALLOFF_MODIFIER_H
