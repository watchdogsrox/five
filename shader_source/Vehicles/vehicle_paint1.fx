
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_paint1.fx shader;
//
// props: spec/reflect/dirt 2nd channel/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//
#define VEHICLE_PAINT_1_SHADER
#define VEHICLE_PAINT_SHADER

#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP_VALUES	// SpecMap not used for basic Specular, but only 2nd Specular
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define	USE_DIRT_UV_LAYER			// dirt 2nd channel
#define USE_SECOND_SPECULAR_LAYER	// 2nd layer of "wide" specular

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define METALLIC_SPECMAP_MIP_BIAS
#define USE_TWIDDLE

//#define VEHCONST_SPECULAR1_FALLOFF			(510.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.8f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_SPECULAR2_FALLOFF			(15.0f)
#define VEHCONST_SPECULAR2_INTENSITY		(0.75f)
//#define VEHCONST_FRESNEL					(0.96f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"
 
