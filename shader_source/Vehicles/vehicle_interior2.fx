
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_interior.fx shader;
//
// props: spec/bump/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//

#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define IS_VEHICLE_INTERIOR

#define VEHCONST_SPECULAR1_FALLOFF			(10.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.0f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)

#include "vehicle_common.fxh"

 
