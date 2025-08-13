
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse
	#define PRAGMA_DCL
#endif

// Configure the megashader
#define USE_DEFAULT_TECHNIQUES
#define NO_DIFFUSE_TEXTURE
#define UNLIT_TECHNIQUES (1)
#define USE_MINIMAP_STEREO

#include "../Megashader/megashader.fxh"
 
