
#ifndef PRAGMA_DCL
// Add texcoord1 to DCL if required - no need to add texcoord0 as they`ll be packed into texcoord0 by ragebuilder. Here for future expansion.
#if defined(USE_SECONDARY_TEXCOORDS) 

	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0 

#else // defined(USE_SECONDARY_TEXCOORDS)

	// 360: VS decompresses tint into specular interpolator for PS
	#pragma dcl position diffuse texcoord0 normal tangent0 

#endif // defined(USE_SECONDARY_TEXCOORDS)

	#define PRAGMA_DCL
	#define ISOLATE_8
	#define USE_WETNESS_MULTIPLIER
	#define USE_UI_TECHNIQUES
#endif

#define USE_PALETTE_TINT

#include "normal_spec.fx"
