// ===============
// mirror_decal.fx
// ===============

#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif

//
// Configure the megashder
//
#define ALPHA_SHADER
#define USE_SPECULAR_FRESNEL
#define USE_NORMAL_MAP
#define USE_REFLECT
#define USE_MIRROR_REFLECT
#define USE_MIRROR_DISTORTION
#define USE_DEFAULT_TECHNIQUES

#define NO_FORWARD_AMBIENT_LIGHT
#define NO_FORWARD_DIRECTIONAL_LIGHT
#define NO_FORWARD_LOCAL_LIGHTS

#define MIRROR_DECAL_FX

#if !__LOW_QUALITY
	#define USE_FOGRAY_FORWARDPASS
#endif // !__LOW_QUALITY

#include "../../Util/macros.fxh"
#include "../../Megashader/megashader.fxh"
