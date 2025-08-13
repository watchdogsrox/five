
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped_decal_nodiff shader:
//
//	2011/10/24	- Andrzej:		- initial;
//
//
//
#define DECAL_SHADER			// 360: necessary for correct DeferredGBuffer's col0.a output - see "#if __XENON && !defined(DECAL_SHADER)" hack in PackGBuffer()
#define USE_PED_DECAL_NO_DIFFUSE

#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_PEDSHADER_FAT

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#include "ped_common.fxh"
