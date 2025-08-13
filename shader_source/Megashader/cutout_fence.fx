
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
#endif
//
//
// Configure the megashder
//
#define USE_DEFAULT_TECHNIQUES

#define CUTOUT_SHADER

#if __LOW_QUALITY
#define USE_GRID_BASED_DISCARDS
#endif // __LOW_QUALITY
#define USE_HIGH_QUALITY_TRILINEARTHRESHOLD  
#define USE_SINGLE_LAYER_SSA
#include "../Megashader/megashader.fxh"
 
