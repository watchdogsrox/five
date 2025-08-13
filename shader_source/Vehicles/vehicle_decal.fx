
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// vehicle_decal.fx shader;
//
// props: spec/reflect/alpha/damage
//
// 08/09/2011 - Andrzej: - initial;
//
//
#define ALPHA_SHADER

#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_DEFAULT_TECHNIQUES

#define USE_VEHICLE_DAMAGE

#define VEHCONST_SPECULAR1_FALLOFF			(15.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.250f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
//#define VEHCONST_REFLECTIVITY				(1.8f)
#define VEHCONST_FRESNEL					(0.90f)

#include "vehicle_common.fxh"
 
