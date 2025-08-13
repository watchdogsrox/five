
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// gta_ped_default shader:
//
//	2008/07/10	- Andrzej:		- cheap shader for ped LODs;
//
//
#define USE_PED_BASIC_SHADER
#define USE_PEDSHADER_FAT
#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#define USE_PED_DAMAGE_TARGETS
#define NO_PED_DAMAGE_NORMALS

#define CAN_BE_CUTOUT_SHADER

#include "ped_common.fxh"
