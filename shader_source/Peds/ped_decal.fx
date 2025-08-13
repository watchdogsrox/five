
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped_decal shader:
//
//	2009/01/30	- Andrzej:		- initial;
//
//
//
#define DECAL_SHADER					// 360: necessary for correct DeferredGBuffer's col0.a output - see "#if __XENON && !defined(DECAL_SHADER)" hack in PackGBuffer()
#define USE_PED_DECAL_DIFFUSE_ONLY
#define USE_WET_EFFECT_DIFFUSE_ONLY		// shader has no spec, but do support diffuse wetness

#define USE_PED_DAMAGE_TARGETS

#define USE_PEDSHADER_FAT

#define USE_UI_TECHNIQUES
#define USE_DEFAULT_TECHNIQUES

#include "ped_common.fxh"
