
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define USE_UI_TECHNIQUES
	#define USE_WETNESS_MULTIPLIER
#endif
//
//
// Configure the megashder
//
#define USE_DEFAULT_TECHNIQUES

#define USE_ALPHACLIP_FADE

#if !defined(ALPHA_SHADER)
	#define CAN_BE_ALPHA_SHADER
	#define CAN_BE_CUTOUT_SHADER
#endif

#define USE_MANUALDEPTH_TECHNIQUE
#define USE_SEETHROUGH_TECHNIQUE

#include "../Megashader/megashader.fxh"
 
