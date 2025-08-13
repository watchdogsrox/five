//
// weapons/weapondamagemodifier.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_DAMAGE_MODIFIER_H
#define WEAPON_DAMAGE_MODIFIER_H

// Rage headers
#include "parser/macros.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponDamageModifier
{
public:

	CWeaponDamageModifier();

	// PURPOSE: Modify the passed in damage
	void ModifyDamage(f32& inOutDamage) const;

private:

	//
	// Members
	//

	// PURPOSE: Scale the damage by this value
	f32 m_DamageModifier;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponDamageModifier::ModifyDamage(f32& inOutDamage) const
{
	inOutDamage *= m_DamageModifier;
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_DAMAGE_MODIFIER_H
