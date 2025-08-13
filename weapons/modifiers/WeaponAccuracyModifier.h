//
// weapons/weaponaccuracymodifier.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_ACCURACY_MODIFIER_H
#define WEAPON_ACCURACY_MODIFIER_H

// Rage headers
#include "parser/macros.h"

// Game headers
#include "Weapons/WeaponAccuracy.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponAccuracyModifier
{
public:

	CWeaponAccuracyModifier();

	// PURPOSE: Modify the passed in accuracy
	void ModifyAccuracy(sWeaponAccuracy& inOutAccuracy) const;

private:

	//
	// Members
	//

	// PURPOSE: Scale the accuracy by this value
	f32 m_AccuracyModifier;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponAccuracyModifier::ModifyAccuracy(sWeaponAccuracy& inOutAccuracy) const
{
	inOutAccuracy.fAccuracy    *= m_AccuracyModifier;
	inOutAccuracy.fAccuracyMin *= m_AccuracyModifier;
	inOutAccuracy.fAccuracyMax *= m_AccuracyModifier;
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_ACCURACY_MODIFIER_H
