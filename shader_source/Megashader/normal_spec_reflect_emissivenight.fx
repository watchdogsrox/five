
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
// gta_normal_spec_reflect_emissivenight shader - night-only emissive shader;
//
// 2006/09/04 - Andrzej: - initial;
//
//
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES
#define USE_EMISSIVE
#define USE_EMISSIVE_NIGHTONLY
#define USE_DONT_TRUST_DIFFUSE_ALPHA
#define USE_ALPHACLIP_FADE

#define CAN_BE_ALPHA_SHADER

#include "../Megashader/megashader.fxh" 
