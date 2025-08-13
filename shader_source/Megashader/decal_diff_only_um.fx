
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
#endif

#define DECAL_SHADER
#define DECAL_DIFFUSE_ONLY
#include "default_um.fx"
