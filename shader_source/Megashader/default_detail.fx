
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
	//#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define USE_WETNESS_MULTIPLIER
#endif
//
//
// Configure the megashder
//
#define USE_DEFAULT_TECHNIQUES
#define USE_DETAIL_MAP
#define USE_ALPHACLIP_FADE

#define USE_MANUALDEPTH_TECHNIQUE
#define USE_SEETHROUGH_TECHNIQUE

#include "../Megashader/megashader.fxh"
 
