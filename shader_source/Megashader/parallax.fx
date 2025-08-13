
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal tangent0 
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif
//
//	gta_parallax shader;
//
//	2006/03/02	-	initial;
//
//
//
//
//
//
//
#define USE_SPECULAR
#if defined(GTA_PARALLAX_SPECMAP_SHADER)
	#undef IGNORE_SPECULAR_MAP
#else
	#define IGNORE_SPECULAR_MAP
#endif
#define USE_NORMAL_MAP
#define USE_PARALLAX_MAP
#define	USE_DEFAULT_TECHNIQUES

#include "../Megashader/megashader.fxh"


 
