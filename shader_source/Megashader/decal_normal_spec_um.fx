
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
#endif

#define DECAL_SHADER
#include "normal_spec_um.fx"
