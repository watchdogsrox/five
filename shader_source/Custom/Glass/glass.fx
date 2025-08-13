
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
	#define DISABLE_ALPHA_DOF
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define USE_REFLECT
#define USE_SPHERICAL_REFLECT
#define USE_DEFAULT_TECHNIQUES

#include "../../Megashader/megashader.fxh"

 
