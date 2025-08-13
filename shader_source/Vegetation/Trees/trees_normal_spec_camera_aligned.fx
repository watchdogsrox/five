
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 tangent0 tangent1 normal
	// Binormal 0 - Centre XYZ
	#define PRAGMA_DCL
#endif
// 
//	trees_normal_spec_camera_aligned.fx
//
//	2009/09/10 - Oscar V:	- initial;
//	2013/12/04 - Andrzej:	- up and running;
//
//
//
//
#define USE_CAMERA_ALIGNED
#define USE_NORMAL_MAP
#define USE_SPECULAR
#define USE_ALPHACLIP_FADE
#define SHADOW_USE_DOUBLE_SIDED

#include "trees_common.fxh"
