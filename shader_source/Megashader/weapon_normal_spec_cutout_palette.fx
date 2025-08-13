
#ifndef PRAGMA_DCL
	#if __PS3
		// ps3: EDGE decompresses tint directly to specular channel for VS
		#pragma dcl position diffuse specular texcoord0 normal tangent0
	#else
		// 360: VS decompresses tint into specular interpolator for PS
		#pragma dcl position diffuse texcoord0 normal tangent0 
	#endif
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
//
// Configure the megashder
//
// 2014/07/16 - Andrzej: 	initial;
//
//
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_SECOND_SPECULAR_LAYER	// 2nd specular
#define USE_SPEC_MAP_INTFALLOFF_PACK
#define USE_DIFFUSE_EXTRA			// extra fake diffuse slot for weapon icon texture
#define USE_DEFAULT_TECHNIQUES
#define USE_ALPHACLIP_FADE

#define USE_COLOR_VARIATION_TEXTURE

#define CUTOUT_SHADER
#define WEAPON_SHADER

#include "../Megashader/megashader.fxh"


 
