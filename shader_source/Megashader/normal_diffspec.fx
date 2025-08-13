
#ifndef PRAGMA_DCL
#ifdef USE_WIND_DISPLACEMENT
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
#else
	#pragma dcl position diffuse texcoord0 normal tangent0 
#endif 

	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
	#define USE_UI_TECHNIQUES
#endif
//
//
// Configure the megashder
//
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_DIFFUSE_AS_SPEC
#define USE_DEFAULT_TECHNIQUES
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT

#define CAN_BE_ALPHA_SHADER
#define CAN_BE_CUTOUT_SHADER

#if __SHADERMODEL >= 40
#define USE_ALPHACLIP_FADE
#endif

#define USE_ALPHACLIP_FADE

#include "../Megashader/megashader.fxh"


 
