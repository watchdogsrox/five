
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal
	#define PRAGMA_DCL
#endif
//
// vehicle_blurredrotor.fx shader;
//
// props: spec/reflect/dirt 2nd channel/alpha/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//
#define ALPHA_SHADER

#define USE_VEHICLE_ALPHA
#define USE_ALPHA_SHADOWS

#define SHADOW_USE_TEXTURE
#define USE_SPECULAR
#define TWEAK_SPECULAR_ON_ALPHA
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define DISABLE_DEPTH_WRITE
#define USE_DEFAULT_TECHNIQUES

#define VEHCONST_SPECULAR1_FALLOFF			(512.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.4f)
#define VEHCONST_FRESNEL					(0.968f)

#include "vehicle_common.fxh"
 
