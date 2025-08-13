
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal 
	#define PRAGMA_DCL
	//#define USE_ANIMATED_UVS			// add AnimationUV support (define before gta_common.h)
#endif
//
//
// Configure the megashder
//
#define USE_DEFAULT_TECHNIQUES

#define CAN_BE_CUTOUT_SHADER

#define USE_UMOVEMENTS
#define USE_UMOVEMENTS_INV_BLUE		// use inverted vert movement scale

#define USE_MANUALDEPTH_TECHNIQUE
#define USE_SEETHROUGH_TECHNIQUE

#include "../Megashader/megashader.fxh"

