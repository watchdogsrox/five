//
// weapons/vehicleweaponinfo.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef VEHICLE_WEAPON_INFO_H
#define VEHICLE_WEAPON_INFO_H

// Rage headers
#include "atl/hashstring.h"
#include "parser/macros.h"

// Framework headers
#include "fwtl/pool.h"

//////////////////////////////////////////////////////////////////////////
// CVehicleWeaponInfo
//////////////////////////////////////////////////////////////////////////

class CVehicleWeaponInfo
{
public:
	
	//
	// Accessors
	//

	u32 GetHash() const { return m_Name.GetHash(); }
	f32 GetKickbackAmplitude() const { return m_KickbackAmplitude; }
	f32 GetKickbackImpulse() const { return m_KickbackImpulse; }
	f32 GetKickbackOverrideTiming() const { return m_KickbackOverrideTiming; }

#if !__FINAL
	// Get the name
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

private:

	//
	// Members
	//

	atHashWithStringNotFinal m_Name;
	f32 m_KickbackAmplitude;
	f32 m_KickbackImpulse;
	f32 m_KickbackOverrideTiming;

	PAR_SIMPLE_PARSABLE;
};

#endif // VEHICLE_WEAPON_INFO_H
