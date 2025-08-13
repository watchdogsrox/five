
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8 // required for directional + shadow
#endif
//
// gta_normal_spec_cubemap_reflect.fx shader;
//
// 26/10/2006 - Andrzej:	- initial;
//
// This shader uses cubemap sampler.
// To work correctly it requires:
// 1. vertex normals have to be parallel to face normals;
// 2. reflection texture must be a valid cubemap texture
// (faces on sides are +x/-x and +z/-z, +y/-y is top/down face (can be white by default)).
//
//

// Configure the megashder
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_REFLECT
#define USE_DEFAULT_TECHNIQUES

#if __SHADERMODEL >= 40
#define USE_ALPHACLIP_FADE
#endif

#include "../Megashader/megashader.fxh"

 
