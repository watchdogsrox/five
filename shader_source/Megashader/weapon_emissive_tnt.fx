
#ifndef PRAGMA_DCL
#if __PS3
	// ps3: EDGE decompresses tint directly to specular channel for VS
	#pragma dcl position diffuse specular texcoord0 normal 
#else
	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse texcoord0 normal 
#endif
	#define PRAGMA_DCL	// pragma dcl defined
#endif

//
// weapon_emissive_tnt shader
//
// 12/11/2018 - Andrzej: initital;
//
//
#define WEAPON_SHADER
#define USE_PALETTE_TINT
#include "emissive.fx"
