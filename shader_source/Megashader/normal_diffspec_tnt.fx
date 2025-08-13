
#ifndef PRAGMA_DCL
#if __PS3
	// ps3: EDGE decompresses tint directly to specular channel for VS
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
#else
	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse texcoord0 normal tangent0 
#endif
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
#endif

#define USE_PALETTE_TINT

#include "normal_diffspec.fx"
