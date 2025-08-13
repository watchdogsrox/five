
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped_wrikle_cs shader:
// 2 wrinkle maps, each with 3 mask maps with 4 variables each (24 wrinkle variables)
//
//	2011/08/10	- Andrzej:		- initial;
//
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_PED_WRINKLE
#define USE_PED_WRINKLE_CS		// extra wrinkle maps and vars
#define USE_PED_CPV_WIND
#define USE_PED_DAMAGE_TARGETS 

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#include "ped_common.fxh"
 

