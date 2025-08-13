//
// vfx_decal_specmap.fx - shader for immediate mode projected textures
//  (bullet holes, explosion holes, etc.)
//
// 2006/02/08 - Andrzej: - initial;
//
//
//
//
#ifndef PRAGMA_DCL
	#pragma dcl position normal tangent0 texcoord0
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif

// Support two sided lighting for the decal
#define USE_GLASS_LIGHTING

#define USE_PARALLAX_REVERTED				// use reverted parallax mode
#define USE_PARALLAX_STEEP
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_NORMAL_MAP
#define USE_PARALLAX_MAP

#include "vfx_decal.fxh"
