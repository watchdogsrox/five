
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
//
// Configure the megashder
//
#define USE_NORMAL_MAP
#define USE_REFLECT
#define USE_SPHERICAL_REFLECT
#define USE_DEFAULT_TECHNIQUES

#define CAN_BE_ALPHA_SHADER
#define CAN_BE_CUTOUT_SHADER

#if __SHADERMODEL >= 40
#define USE_ALPHACLIP_FADE
#endif

#include "../Megashader/megashader.fxh"

 
