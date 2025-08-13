
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// vehicle_track_ammo.fx shader;
//
// props: spec/reflect/bump/No damage
//
// 02/03/2017 - Andrzej: - initial;
//
//

#define USE_SPECULAR
#define USE_NORMAL_MAP
#define USE_VEHICLE_ANIMATED_TRACKAMMO_UV	// track ammo UV anim
#define USE_DEFAULT_TECHNIQUES

#define USE_DIRT_LAYER
#define USE_DIRT_UV_LAYER					// dirt 2nd channel

//#define VEHCONST_SPECTEXTILEUV			(1.0f)

#define CAN_BE_CUTOUT_SHADER

#include "vehicle_common.fxh"
