//
//
//
//
#if __SHADERMODEL >= 40
	#pragma dcl position

	#define MEMORYCS_COMPILING_SHADER   1
	#include "../../../rage/base/src/shaderlib/memorycs.fxh"
#else
	#pragma dcl position
	#define PRAGMA_DCL
	technique dummy{ pass dummy {} }
#endif
