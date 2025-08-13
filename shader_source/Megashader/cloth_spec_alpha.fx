
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
//
// Configure the megashder
//
#define CAN_BE_ALPHA_SHADER
#define CAN_BE_CUTOUT_SHADER
#define SHADER_USES_ONLY_ALPHA_AND_CUTOUT_BUCKETS

#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_REFLECT
#define USE_DYNAMIC_REFLECT
#define USE_DEFAULT_TECHNIQUES
#define USE_CLOTH

#define USE_DUAL_PARAB_ALTERNATE_LOOKUP
#define USE_BACKLIGHTING_HACK


#include "../Megashader/megashader.fxh"
