
#ifndef PRAGMA_DCL
#if __PS3
	// ps3: EDGE decompresses tint directly to specular channel for VS
	#pragma dcl position diffuse specular texcoord0 texcoord1 texcoord2  normal tangent0 
#else
	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse texcoord0 texcoord1 texcoord2  normal tangent0 
#endif
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
#endif

#define USE_PARALLAX_MAP_V2 (__SHADERMODEL >= 40) || __MAX || __WIN32PC
#define USE_EDGE_WEIGHT (__SHADERMODEL >= 40) || __MAX || __WIN32PC

#include "../Megashader/normal_tnt.fx"
