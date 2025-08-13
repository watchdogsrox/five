
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 texcoord2 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// vehicle_mesh2_enveff.fx shader;
//
// props: spec/reflect/bump/dirt 3rd channel/damage
// UV channel mapping:
// 1: diffuse
// 2: enveff
// 3: dirt
//
// 4/09/2018 - Andrzej: - initial;
//
//
#define USE_VEHICLESHADER_SNOW

#define USE_SPECTEX_TILEUV				// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV		(1.0f)

#define	USE_VEHICLESHADER_SNOW_USE_UV1	// enveff uses UV channel 1
//#define VEHCONST_ENVEFFTEXTILEUV		(8.0f)

#define USE_SPECULAR
#define USE_SPEC_MAP_INTFALLOFFFRESNEL_PACK
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define	USE_DIRT_UV_LAYER				// dirt on last (3rd) UV channel
#define USE_VEHICLE_DAMAGE

#define VEHCONST_SPECULAR1_FALLOFF			(512.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.4f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_FRESNEL					(1.0f)
#define VEHCONST_BUMPINESS					(1.0f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"
