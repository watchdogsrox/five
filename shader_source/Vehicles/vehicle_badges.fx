
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_badges.fx shader;
//
// props: spec/reflect/bump/alpha/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//
#define ALPHA_SHADER

#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES

#define USE_VEHICLE_DAMAGE

//#define VEHCONST_SPECULAR1_FALLOFF		(190.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.225f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
//#define VEHCONST_REFLECTIVITY				(1.8f)

#include "vehicle_common.fxh"
 
