#define ISOLATE_8
#define USE_DETAIL_MAP

#define USE_DISPLACEMENT_MAP


#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	//#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define ISOLATE_4
	#define USE_WETNESS_MULTIPLIER
#endif
//
//
// Configure the megashader
//
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES
#define USE_DETAIL_MAP
#define USE_ALPHACLIP_FADE
#define USE_PALETTE_TINT
#define CAN_BE_CUTOUT_SHADER

#include "../Megashader/megashader.fxh"
