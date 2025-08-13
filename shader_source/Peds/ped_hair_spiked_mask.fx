
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 texcoord1 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ordered spiked (normal perp) ped hair shader (rendered in cutout pass):
// - extra normal layer is rendered in pass#1 to overwrite underlying normals; 
// - also contains extra independent mask layer (for crew logos, etc.);
//
//
//	20/10/2016	- Andrzej:	- initial;
//
//
//
#define USE_SPECULAR
#define USE_NORMAL_MAP

#define PEDSHADER_HAIR_SPIKED
#define PEDSHADER_HAIR_ORDERED
#define PEDSHADER_HAIR_DIFFUSE_MASK
#define USE_PED_CPV_WIND
#define PEDSHADER_HAIR_ANISOTROPIC
#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES
#define SHADOW_USE_TEXTURE
#define USE_SSTAA
#define USE_HAIR_TINT_TECHNIQUES

#define CAN_BE_CUTOUT_SHADER		// ped_hair_spiked_noalpha requires standard techniques

#include "ped_common.fxh"
