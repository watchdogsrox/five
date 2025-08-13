
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
//
// Configure the megashader
//
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES

#define CUTOUT_SHADER
#define USE_SINGLE_LAYER_SSA
#define USE_HIGH_QUALITY_TRILINEARTHRESHOLD  

#include "../Megashader/megashader.fxh"
 
