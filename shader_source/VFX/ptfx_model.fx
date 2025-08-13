
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
	#define USE_ANIMATED_UVS		// add AnimationUV support (define before gta_common.h)
	#define ISOLATE_8
#endif
//
//
// Configure the megashder
//
#define ALPHA_SHADER
#define COLORIZE
#define IGNORE_SPECULAR_MAP

#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES

//Required unlit techniques for particle shadows
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
#define USE_UNLIT_TECHNIQUES
#endif

#include "../Megashader/megashader.fxh"
