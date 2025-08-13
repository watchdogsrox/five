
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
//
// Configure the megashder
//
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT

#define USE_UMOVEMENTS
#define USE_UMOVEMENTS_INV_BLUE		// use inverted vert movement scale

#define CAN_BE_CUTOUT_SHADER

#include "../Megashader/megashader.fxh"
