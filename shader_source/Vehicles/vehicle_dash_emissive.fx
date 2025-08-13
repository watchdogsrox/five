
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_dash_emissive shader;
//
// props: spec/reflect/emissive only at night/damage
//
// 28/11/2013 - Andrzej: - initial;
//
//
#define ALPHA_SHADER

#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_DEFAULT_TECHNIQUES

#define USE_VEHICLE_DAMAGE 

#define USE_EMISSIVE
#define USE_EMISSIVE_NIGHTONLY
#define USE_EMISSIVE_DIMMER
#define USE_ALPHA_DOF

#define VEHCONST_SPECULAR1_FALLOFF			(100.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.0f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
//#define VEHCONST_REFLECTIVITY				(1.8f)
#define VEHCONST_FRESNEL					(1.0f)
#define VEHCONST_EMISSIVE_HDR_MULTIPLIER	(0.3f)

#include "vehicle_common.fxh"

 
