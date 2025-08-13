
#ifndef PRAGMA_DCL
#if __PS3
	// ps3: EDGE decompresses tint directly to specular channel for VS
	#pragma dcl position diffuse specular texcoord0 normal
#else
	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse specular texcoord0 normal 
#endif
	#define PRAGMA_DCL
#endif
// 
//	trees_lod_tnt.fx
//
//	2012/01/30	-	Andrzej:	- initial;
//
//
#define USE_INSTANCED
#define USE_TREE_LOD
#define USE_PALETTE_TINT
#define USE_ALPHACLIP_FADE

#include "trees_common.fxh"
