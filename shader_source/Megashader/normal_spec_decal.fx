
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
#endif

#define USE_WETNESS_MULTIPLIER
#define DECAL_SHADER
#define DECAL_WRITE_SLOPE
#include "normal_spec.fx"
