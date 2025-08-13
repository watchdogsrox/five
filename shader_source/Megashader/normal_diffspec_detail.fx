
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
#endif
//
//
// Configure the megashder
//
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_DIFFUSE_AS_SPEC
#define USE_DEFAULT_TECHNIQUES
#define USE_DETAIL_MAP

#if __SHADERMODEL >= 40
#define USE_ALPHACLIP_FADE
#endif

#include "../Megashader/megashader.fxh"


 
