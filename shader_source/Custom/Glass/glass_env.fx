
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define ISOLATE_8
	#define DISABLE_ALPHA_DOF
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
#define USE_ALPHA_SHADOWS
#define SHADOW_USE_TEXTURE
#define FORCE_RAIN_COLLISION_TECHNIQUE // force shadow technique so that heightmap blocks rain (BS#1158788)
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_DEFAULT_TECHNIQUES
#define USE_NORMAL_MAP
#define USE_ALPHA_FRESNEL_ADJUST
#define USE_GLASS_LIGHTING

#include "../../Megashader/megashader.fxh"

 
