
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_mesh.fx shader;
//
// props: spec/reflect/bump/dirt 2nd channel/damage
//
// 10/01/2007 - Andrzej: - initial;
//  4/10/2010 - Andrzej: - mesh2 shader;
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
#define USE_VEHICLE_DAMAGE
#define USE_TWIDDLE
#define USE_SUPERSAMPLING

#define VEHCONST_SPECULAR1_FALLOFF			(460.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(0.6f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_SPECULAR2_FALLOFF			(20.0f)
#define VEHCONST_SPECULAR2_INTENSITY		(0.6f)
#define VEHCONST_FRESNEL					(1.0f)
#define VEHCONST_BUMPINESS					(1.0f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"
 
