
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
	#define ISOLATE_8 // required for directional + shadow
#endif
//
//
// Configure the megashder
//
#define USE_SPECULAR
#define IGNORE_SPECULAR_MAP
#define USE_ALPHACLIP_FADE
#define USE_DEFAULT_TECHNIQUES
#define USE_UI_TECHNIQUES

#include "../Megashader/megashader.fxh"
 
