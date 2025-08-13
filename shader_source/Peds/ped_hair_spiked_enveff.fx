
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal tangent0 
	#define PRAGMA_DCL
#endif
//
// ordered spiked (normal perp) ped hair shader (rendered in cutout pass) with enveff:
// extra normal layer is rendered in pass#1 to overwrite underlying normals; 
//
//
//	2012/03/02	- Andrzej:	- initial;
//
//
//
#define USE_PEDSHADER_SNOW

#define CUTOUT_SHADER

#include "ped_hair_spiked.fx"
