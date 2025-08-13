
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// vehicle_track_emissive.fx shader;
//
// props: spec/reflect/bump/No damage
//
// 26/08/2016 - Andrzej: - initial;
//
//

#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_VEHICLE_ANIMATED_TRACK_UV		// track UV anim
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define USE_DIRT_UV_LAYER					// dirt 2nd channel

#define USE_EMISSIVE

//#define VEHCONST_SPECTEXTILEUV		(1.0f)

#define CAN_BE_CUTOUT_SHADER

#include "vehicle_common.fxh"
