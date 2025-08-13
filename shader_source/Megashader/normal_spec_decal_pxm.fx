#if 0 // Fix for BS#2219485
	#ifndef PRAGMA_DCL
		#pragma dcl position diffuse texcoord0 texcoord1 texcoord2 normal tangent0 
		#define PRAGMA_DCL
		#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
		#if __SHADERMODEL >= 40
			#define USE_EDGE_WEIGHT
		#endif // __SHADERMODEL >= 40
	#endif
#else
	#ifndef PRAGMA_DCL
		#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
		#define PRAGMA_DCL
		#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
		#if __SHADERMODEL >= 40
			#define USE_EDGE_WEIGHT
			#define SUPRESS_PADDING
		#endif // __SHADERMODEL >= 40
	#endif
#endif // Fix for BS#2219485

#define USE_PARALLAX_MAP_V2 (__SHADERMODEL >= 40)

#include "normal_spec_decal.fx"
