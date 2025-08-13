
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
// glass_displacent.fx shader
//
//	2011/01/10	-	Andrzej:	- initial;
//
//
//
#define ALPHA_SHADER
#define DISPLACEMENT_SHADER

// Configure the megashder
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_REFLECT
#define USE_SPHERICAL_REFLECT
#define USE_DEFAULT_TECHNIQUES

#include "../../Megashader/megashader.fxh"
