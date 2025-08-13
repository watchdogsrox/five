
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
#endif
//
//
// Configure the megashder
//
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES

#define CAN_BE_ALPHA_SHADER
#define CAN_BE_CUTOUT_SHADER

#if __SHADERMODEL >= 40
#define USE_ALPHACLIP_FADE
#endif

#include "../Megashader/megashader.fxh"
