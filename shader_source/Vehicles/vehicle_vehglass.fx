
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 texcoord2 normal
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_vehglass.fx shader;
//
// props: spec/reflect/dirt 2nd channel/alpha/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//
#define ALPHA_SHADER

#define USE_VEHICLE_ALPHA
#define USE_DIFFUSE_COLOR_TINT	// RGBA tint for vehicle glass

#define USE_SPECULAR
#define TWEAK_SPECULAR_ON_ALPHA
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define	USE_DIRT_UV_LAYER		// dirt 2nd channel
#define USE_VEHICLE_DAMAGE

#define USE_SHADOW_FAST_NO_FADE
#define USE_VERTEX_FOG

#define VEHCONST_SPECULAR1_FALLOFF			(510.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(1.0f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
//#define VEHCONST_REFLECTIVITY				(1.8f)
#define VEHCONST_FRESNEL					(0.96f)

#define USE_GLASS_LIGHTING

#define USE_HIGHQUALITY_FORWARD_TECHNIQUES
#define VEHICLE_GLASS_SHADER
#define FORCE_RAIN_COLLISION_TECHNIQUE // force shadow technique so that heightmap blocks rain (BS#1158788)

#include "vehicle_common.fxh"
#include "../../vfx/vfx_shared.h"
