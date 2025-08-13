
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_tire.fx shader;
//
// props: spec/bump/dirt/NO damage
//
// 26/01/2018 - Andrzej: - initial;
//
//

#define USE_SPECULAR
#define USE_SPEC_MAP_INTFALLOFFFRESNEL_PACK
#define USE_NORMAL_MAP
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define	USE_DIRT_UV_LAYER						// dirt 2nd channel
#define USE_SECOND_SPECULAR_LAYER				// 2nd layer of "wide" specular
#define USE_SECOND_SPECULAR_LAYER_LOCKCOLOR		// color of 2nd specular is constant white(float3(1,1,1))
#define USE_SECOND_SPECULAR_LAYER_NOFRESNEL		// 2nd spec layer not scaled by fresnel

#define USE_EMISSIVE

#define USE_VEHICLE_INSTANCED_WHEEL_SHADER		// wheels use true instancing (only as NON-SKINNED geometry)

#define VEHICLE_TYRE_DEFORM
#if !defined(SHADER_FINAL)
//#define VEHICLE_TYRE_DEBUG_MEASUREMENT
#endif // !!defined(SHADER_FINAL)


#define VEHCONST_SPECULAR1_FALLOFF			(512.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(1.0f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_SPECULAR2_FALLOFF			(7.0f)
#define VEHCONST_SPECULAR2_INTENSITY		(1.5f)
#define VEHCONST_FRESNEL					(1.0f)
#define VEHCONST_BUMPINESS					(1.0f)

//#if (__WIN32PC && __SHADERMODEL >= 40)
//#define DO_WHEEL_ROUNDING_TESSELLATION 1 // Allow radial pushing out tessellation to make stuff rounder.
//#endif

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"

 
