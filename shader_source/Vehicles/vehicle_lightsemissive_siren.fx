
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_lightsemissive_siren.fx shader;
//
// props: spec/reflect/dirt 2nd channel/emissive only at night/damage
//
// 28/01/2020 - Andrzej: - initial;
//
//
#define ALPHA_SHADER

#define USE_SPECULAR
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define	USE_DIRT_UV_LAYER		// dirt 2nd channel
//#define USE_DIRT_ON_TOP
#define USE_VEHICLE_DAMAGE 

#define USE_EMISSIVE
#define USE_EMISSIVE_NIGHTONLY
#define USE_EMISSIVE_DIMMER
#define USE_EMISSIVE_DIMMER_19	// *special case* siren dimmer using index 19 exclusively

#define VEHCONST_SPECULAR1_FALLOFF			(510.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.8f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
//#define VEHCONST_REFLECTIVITY				(1.8f)
#define VEHCONST_FRESNEL					(0.96f)

#include "vehicle_common.fxh"

 
