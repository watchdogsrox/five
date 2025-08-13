
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
#define USE_PED_WRINKLE
#define USE_PED_CPV_WIND
#define USE_PED_DAMAGE_TARGETS 

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#include "ped_common.fxh"
 

