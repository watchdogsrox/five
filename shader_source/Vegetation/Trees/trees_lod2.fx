
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 texcoord2 texcoord3 normal
	#define PRAGMA_DCL
#endif
// 
//	trees_lod2.fx - simple billboarding tree shader
//
//	2011/03/14	-	Andrzej:	- initial;
//
//

#define USE_TREE_LOD2
#define USE_ALPHACLIP_FADE
#define SHADOW_USE_DOUBLE_SIDED

#include "trees_common.fxh"
