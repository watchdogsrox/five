
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_shuts.fx shader;
//
// props: spec/reflect/bump/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES

#define USE_VEHICLE_DAMAGE
#define USE_BURNOUT_IN_MATDIFFCOLOR			// burnout sets new bodycolor

#define VEHCONST_SPECULAR1_FALLOFF			(179.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.065f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_BUMPINESS					(0.6f)
#define VEHCONST_FRESNEL					(0.83f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"

 
