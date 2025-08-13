#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
    #define PRAGMA_DCL
#endif
//
// ped fur *expensive* shader (rendered in cutout pass):
//
//	2009/01/22	- Andrzej:	- initial;
//
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP

#define USE_PEDSHADER_FUR
#define PEDSHADER_HAIR_ORDERED

#define USE_ANISOTROPIC
#define USE_DEFAULT_TECHNIQUES
#define DISABLE_WET_EFFECT // we are running out of shader variables

#define CUTOUT_SHADER

#include "ped_common.fxh"
