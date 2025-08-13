
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_paint2.fx shader;
//
// props: spec/reflect/dirt/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//
#define VEHICLE_PAINT_SHADER

#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define USE_SECOND_SPECULAR_LAYER	// 2nd layer of "wide" specular

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define USE_TWIDDLE

//#define VEHCONST_SPECULAR1_FALLOFF			(510.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.8f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_SPECULAR2_FALLOFF			(15.0f)
#define VEHCONST_SPECULAR2_INTENSITY		(0.75f)
//#define VEHCONST_FRESNEL					(0.96f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"
 

