
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ped hair cutout shader (rendered in cutout pass):
//
//	2006/06/16	- Andrzej:	- cleanup;
//
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP

#define PEDSHADER_HAIR_CUTOUT
#define PEDSHADER_HAIR_ANISOTROPIC
#define USE_PED_CPV_WIND
#define USE_PED_CLOTH

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#define CUTOUT_SHADER
#if !__LOW_QUALITY
	#define USE_FOGRAY_FORWARDPASS
#endif // !__LOW_QUALITY

#include "ped_common.fxh"
