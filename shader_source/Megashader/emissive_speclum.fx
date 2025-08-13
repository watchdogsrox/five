
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL	// pragma dcl defined
	#define ISOLATE_8
#endif

#define USE_EMISSIVE

#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_INV_DIFFUSE_AS_SPECULAR_MAP	// use inverted diffuse luminocity as spec map
#define EMISSIVE_SPECLUM_SHADER

#include "default.fx"
