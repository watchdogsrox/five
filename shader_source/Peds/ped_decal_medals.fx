
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped_decal shader:
//
//	2009/01/30	- Andrzej:		- initial;
//
//
//
#define DECAL_SHADER			// 360: necessary for correct DeferredGBuffer's col0.a output - see "#if __XENON && !defined(DECAL_SHADER)" hack in PackGBuffer()

// #define USE_PED_DECAL_DIFFUSE_ONLY

#define USE_SPECULAR
#define USE_PEDSHADER_FAT

#define USE_PED_DAMAGE_TARGETS
#define USE_PED_DAMAGE_MEDALS

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES
#define USE_DIFFUSE_SAMPLER_NOSTRIP

#include "ped_common.fxh"
