#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
//
// Configure the megashder
//
#define USE_SPECULAR
#define USE_REFLECT
#define USE_SPHERICAL_REFLECT
#define USE_DEFAULT_TECHNIQUES

#define CAN_BE_ALPHA_SHADER
#define CAN_BE_CUTOUT_SHADER

#include "../Megashader/megashader.fxh"

 
