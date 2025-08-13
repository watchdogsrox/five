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
#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_PED_CPV_WIND

#define DECAL_SHADER

#define USE_PED_DAMAGE_TARGETS
#define USE_PED_DAMAGE_NO_SKIN   // this is meant to be used for the MP player jackets, so they are always cloth (saves skin check code code)
#define USE_PED_DAMAGE_DECORATION_DECAL

#define USE_UI_TECHNIQUES
#define USE_DEFAULT_TECHNIQUES

#define CAN_BE_UNSKINNED_PED_PROP_SHADER


#include "ped_common.fxh"
