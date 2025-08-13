
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// vehicle_track_siren.fx shader (vehicle_lightsemissive + vehicle_track2);
//
// props: spec/reflect/bump/No damage
//
// 6/11/2019 - Andrzej: - initial;
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

#define USE_VEHICLE_ANIMATED_TRACK2_UV		// track2 UV anim

#include "vehicle_common.fxh"

