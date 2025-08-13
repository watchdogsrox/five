
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_detail2 shader;
//
//
// 16/08/2013 - Andrzej: - initial;
//
//
//

#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define USE_DETAIL_MAP
#define USE_DETAIL_MAP_UV_LAYER				// detail map 2nd channel

#define VEHCONST_SPECULAR1_FALLOFF			(15.0f)
#define VEHCONST_SPECULAR1_INTENSITY		(1.0f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_FRESNEL					(0.8f)

#include "vehicle_common.fxh"
 
