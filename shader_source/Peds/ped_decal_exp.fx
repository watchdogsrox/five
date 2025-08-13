
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped_decal_exp shader:
//
//	22/12/2010	- Andrzej:		- initial;
//
//
//
#define DECAL_SHADER			// 360: necessary for correct DeferredGBuffer's col0.a output - see "#if __XENON && !defined(DECAL_SHADER)" hack in PackGBuffer()
#define USE_PED_DECAL

#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_PEDSHADER_FAT

#define USE_PED_DAMAGE_TARGETS

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#include "ped_common.fxh"
