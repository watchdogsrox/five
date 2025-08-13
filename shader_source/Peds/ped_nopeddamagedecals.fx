
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// gta_ped shader:
//
//	2005/08/04	- Andrzej:		- initial;
//	2007/02/06	- Andrzej:		- ped damage support added for deferred renderer;
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_PED_CPV_WIND

#define USE_PED_DAMAGE_TARGETS
#define USE_PED_DAMAGE_BLOOD_ONLY
#define USE_PED_DAMAGE_NO_SKIN   // this is meant to be used for the MP player jackets, so they are always cloth (saves skin check code code)

#define USE_UI_TECHNIQUES
#define USE_DEFAULT_TECHNIQUES

#define CAN_BE_UNSKINNED_PED_PROP_SHADER

#include "ped_common.fxh"

 
