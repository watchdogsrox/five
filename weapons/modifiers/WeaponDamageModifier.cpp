//
// weapons/weapondamagemodifier.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Modifiers/WeaponDamageModifier.h"

// Parser
#include "WeaponDamageModifier_parser.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponDamageModifier::CWeaponDamageModifier()
: m_DamageModifier(1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////
