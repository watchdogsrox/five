#ifndef __TILEDLIGHTINGSETTINGS_H__
#define __TILEDLIGHTINGSETTINGS_H__

#define TILED_LIGHTING_INSTANCED_TILES				(1 && (__D3D11 || RSG_ORBIS))

#define NUM_TILE_COMPONENTS           (4)
#define NUMBER_TARGETS                (1)

// ----------------------------------------------------------------------------------------------- //
#if !defined(__GTA_COMMON_FXH)
// ----------------------------------------------------------------------------------------------- //

enum tileComponentOffset
{
	TILE_DEPTH_MIN = 0,
	TILE_DEPTH_MAX,
	TILE_SPECULAR,
	TILE_INTERIOR
};

#if RSG_PC
	#define MAX_STORED_LIGHTS		(1024)
#elif RSG_ORBIS || RSG_DURANGO
	#define MAX_STORED_LIGHTS		(768)
#else
	#define MAX_STORED_LIGHTS		(512)
#endif

// ----------------------------------------------------------------------------------------------- //
#endif // !defined(__GTA_COMMON_FXH)
// ----------------------------------------------------------------------------------------------- //

#endif
