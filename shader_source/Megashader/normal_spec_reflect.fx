
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	//#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define ISOLATE_8
#endif
//
//
// Configure the megashder
//
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_REFLECT
#define USE_SPHERICAL_REFLECT
#define USE_DEFAULT_TECHNIQUES
#define CAN_BE_ALPHA_SHADER
#define USE_ALPHACLIP_FADE

#include "../Megashader/megashader.fxh"

 
