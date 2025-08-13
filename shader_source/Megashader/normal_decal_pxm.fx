
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 texcoord2  normal tangent0 
	#define PRAGMA_DCL
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define USE_WETNESS_MULTIPLIER
#endif

#define USE_PARALLAX_MAP_V2 (__SHADERMODEL >= 40) || __MAX || __WIN32PC
#define USE_EDGE_WEIGHT (__SHADERMODEL >= 40) || __MAX || __WIN32PC

#include "../Megashader/normal_decal.fx"
 
