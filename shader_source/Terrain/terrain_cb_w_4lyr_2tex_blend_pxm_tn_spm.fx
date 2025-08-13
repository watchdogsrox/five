
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 texcoord2 texcoord3 normal tangent0
	#define PRAGMA_DCL
#endif

#define PARALLAX_MAP 1
#define SPECULAR_MAP 1
#define TINT_TEXTURE 1
#define TINT_TEXTURE_NORMAL 1
#define USE_EDGE_WEIGHT 1

#include "terrain_cb_w_4lyr_2tex_blend.fx"
