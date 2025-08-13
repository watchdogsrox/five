//
//
//
//
#ifndef _TREES_SHARED_H
#define _TREES_SHARED_H

#define TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS	(1 && ((1 && RSG_PC) || (1 && RSG_DURANGO) || (1 && RSG_ORBIS)))

// Overrides original uMovements with cut down version of bending wind.
#define TREES_USE_ALPHA_CARD_ONLY_BENDING						(1 && TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS)
// Don`t use stiffness multiplier (umGlobalParams.z).
#define TREES_USE_GLOBAL_STIFFNESS_RAW							(0)

// Number of smoothed triangle waves we sum to form final micro-movement wave.
#define TREES_NUMBER_OF_BASIS_WAVES			(3)

#define TREES_INCLUDE_SFX_WIND				(1 && ((1 && RSG_PC) || (1 && RSG_DURANGO) || (1 && RSG_ORBIS)))
#define TREES_SFX_WIND_FIELD_TEXTURE_SIZE	(4) // Must be power of 2.
#define TREES_SFX_WIND_FIELD_CACHE_SIZE		(8)

#endif //_TREES_SHARED_H
