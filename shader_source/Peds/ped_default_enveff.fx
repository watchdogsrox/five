
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal
	#define PRAGMA_DCL
#endif
//
// ped_default_enveff shader:
//
//	2011/06/08	- Andrzej:	- cheap enveff shader for ped LODs;
//
//
#define USE_PED_BASIC_SHADER

#define USE_PEDSHADER_SNOW
#define USE_PEDSHADER_FAT

#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#include "ped_common.fxh"
