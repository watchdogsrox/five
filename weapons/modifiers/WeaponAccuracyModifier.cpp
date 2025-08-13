//
// weapons/weaponaccuracymodifier.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Modifiers/WeaponAccuracyModifier.h"

// Parser
#include "WeaponAccuracyModifier_parser.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponAccuracyModifier::CWeaponAccuracyModifier()
: m_AccuracyModifier(1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////
