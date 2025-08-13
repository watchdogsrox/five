
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

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#define USE_REFLECT
#define USE_DYNAMIC_REFLECT

#define CAN_BE_UNSKINNED_PED_PROP_SHADER
#define ALPHA_SHADER

#define FORWARD_LOCAL_LIGHT_SHADOWING
#if !__LOW_QUALITY
	#define USE_FOGRAY_FORWARDPASS
#endif // !__LOW_QUALITY

#include "ped_common.fxh"

 
