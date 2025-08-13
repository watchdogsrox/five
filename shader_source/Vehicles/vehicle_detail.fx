
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_detail shader;
//
//
// 28/02/2012 - Andrzej: - initial;
//
//
//

#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define	USE_DIRT_UV_LAYER			// dirt 2nd channel

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define USE_DETAIL_MAP

//#define VEHCONST_SPECULAR1_FALLOFF		(500.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.5f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
//#define VEHCONST_SPECULAR2_FALLOFF		(4.0f)
//#define VEHCONST_SPECULAR2_INTENSITY		(0.550f)
//#define VEHCONST_FRESNEL					(0.95f)

#include "vehicle_common.fxh"
 
