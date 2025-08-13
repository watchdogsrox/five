
#ifndef PRAGMA_DCL
#if __PS3
	// ps3: EDGE decompresses tint directly to specular channel for VS
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
#else
	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
#endif
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

#define USE_UMOVEMENTS_TEX			// um H/V scale+phase in texCoord0.zw
#define USE_UMOVEMENTS_INV_BLUE		// use inverted vert movement scale
#define USE_BACKLIGHTING_HACK

#define USE_PALETTE_TINT

#include "../Megashader/megashader.fxh"
