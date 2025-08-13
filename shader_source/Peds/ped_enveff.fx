
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped_enveff shader:
//
//	2009/03/30	- Andrzej:		- initial (snow);
//
//
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP

#define USE_PEDSHADER_SNOW
#define USE_PED_CPV_WIND
#define USE_PEDSHADER_FAT

#define USE_PED_DAMAGE_TARGETS 

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#define CAN_BE_UNSKINNED_PED_PROP_SHADER

#include "ped_common.fxh"
