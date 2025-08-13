//
//
//
//
#ifndef __PLANTSGRASSRENDERERSWITCHES_H__
#define __PLANTSGRASSRENDERERSWITCHES_H__

// Switches for shadow and use of LOD settings.
#if (1 && (RSG_PC || RSG_DURANGO || RSG_ORBIS))
	#define PLANTS_CAST_SHADOWS			(1)
#else
	#define PLANTS_CAST_SHADOWS			(0)
#endif

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	#define PLANTS_USE_LOD_SETTINGS		(1)
#else
	#define PLANTS_USE_LOD_SETTINGS		(0)
#endif

#include "shader_source/vegetation/grass/grass.fxh"

#endif //__PLANTSGRASSRENDERERSWITCHES_H__
