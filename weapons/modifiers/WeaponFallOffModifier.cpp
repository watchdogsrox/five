//
// weapons/weaponfalloffmodifier.cpp
//
// Copyright (C) 1999-2017 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Modifiers/WeaponFallOffModifier.h"

// Parser
#include "WeaponFallOffModifier_parser.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponFallOffModifier::CWeaponFallOffModifier()
: m_RangeModifier(1.0f)
, m_DamageModifier(1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////
