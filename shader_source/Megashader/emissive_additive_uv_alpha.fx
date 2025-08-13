#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL	// pragma dcl defined
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
#define USE_EMISSIVE_ADDITIVE

#define USE_DEFAULT_TECHNIQUES

#define USE_MANUALDEPTH_TECHNIQUE
#define USE_SEETHROUGH_TECHNIQUE

#include "../Megashader/megashader.fxh"
