#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 texcoord6 texcoord7 normal tangent0
	#define PRAGMA_DCL
#endif

#define SPECULAR 1
#define NORMAL_MAP 1

#define USE_ALPHACLIP_FADE
#define LAYER_COUNT 4
#define USE_PER_MATERIAL_WETNESS_MULTIPLIER

#define TINT_TEXTURE 1
#define TINT_TEXTURE_NORMAL 1
#define TINT_TEXTURE_DISTANCE_BLEND 1

#define PARALLAX_MAP 1

#define USE_RDR_STYLE_BLENDING 1

#include "terrain_cb_common.fxh"
