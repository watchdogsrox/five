
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 texcoord2 texcoord3 normal
	#define PRAGMA_DCL
#endif
// 
//	trees_lod2d.fx - simple billboarding tree shader for drawable LODs
//
//	2012/09/20	-	Andrzej:	- initial;
//
//

#define USE_TREE_LOD2
#define USE_TREE_LOD2_FOR_DRAWABLES
#define USE_ALPHACLIP_FADE
#define SHADOW_USE_DOUBLE_SIDED

#if __XENON || __PS3
	#define MIPMAP_LOD_BIAS				-1.0
#else
	#define MIPMAP_LOD_BIAS				0.0
#endif

#include "trees_common.fxh"
