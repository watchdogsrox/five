//
// weapons/weapon.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_ACCURACY_H
#define WEAPON_ACCURACY_H

// Game headers

// Forward declarations

////////////////////////////////////////////////////////////////////////////////

struct sWeaponAccuracy
{
	sWeaponAccuracy()
		: fAccuracy( 1.0f )
		, fAccuracyMin( 0.0f )
		, fAccuracyMax( 1.0f )
		, bShouldAlwaysMiss( false )
	{
		// Default constructor does not initialise members
	}

	sWeaponAccuracy(f32 fAccuracy, f32 fAccuracyMin, f32 fAccuracyMax)
		: fAccuracy(fAccuracy)
		, fAccuracyMin(fAccuracyMin)
		, fAccuracyMax(fAccuracyMax)
		, bShouldAlwaysMiss(false)
	{
	}

	f32 fAccuracy;
	f32 fAccuracyMin;
	f32 fAccuracyMax;
	bool bShouldAlwaysMiss;
};

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_ACCURACY_H
