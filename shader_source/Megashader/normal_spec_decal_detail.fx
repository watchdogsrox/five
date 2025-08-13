
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif

#define DECAL_SHADER
#define USE_DETAIL_MAP
#define DECAL_WRITE_SLOPE
#include "normal_spec.fx"
