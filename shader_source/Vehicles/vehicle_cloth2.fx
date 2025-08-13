#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif

//
// vehicle_cloth2 - shader with 2nd UV layer;
//
// 2014/08/12 - Andrzej:	- initial;
//
//
//
//
#define USE_BACKLIGHTING_HACK
#define USE_CLOTH

#define USE_DIRT_LAYER
#define USE_SECOND_UV_LAYER				// decal
#define USE_SECOND_UV_LAYER_UV0			// decal 1st channel (uses UV0)

#include "vehicle_interior.fx"

 
