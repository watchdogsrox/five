
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0
	#define PRAGMA_DCL
#endif
// 
//	trees_normal_diffspec.fx
//
//	2013/10/01 - Oscar V:	- initial;
//	2013/12/04 - Andrzej:	- up and running;
//
//
//
//
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_DIFFUSE_AS_SPEC
#define USE_ALPHACLIP_FADE

#include "trees_common.fxh"
