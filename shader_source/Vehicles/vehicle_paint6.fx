
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_paint6.fx shader;
//
// props: spec/reflect/dirt 2nd channel/decal 2nd channel/damage
//
// 28/01/2011 - Andrzej: - initial;
//
// layers:
// - diffuse		- uv#0 + tileUV
// - decal			- uv#1
// - env			- uv#0 + tileUV (enveff version only)
// - dirt			- uv#0
// - dirt bumpmap	- uv#0
//
//
#define VEHICLE_PAINT_SHADER

#define USE_SPECULAR
#ifndef USE_SPECTEX_TILEUV
	#define USE_SPECTEX_TILEUV				// tileUV control on specTex
	//#define VEHCONST_SPECTEXTILEUV		(1.0f)
#endif
#define USE_DEFAULT_TECHNIQUES


#define USE_DIFFUSETEX_TILEUV			// tileUV control on diffuseTex
//#define VEHCONST_DIFFUSETEXTILEUV		(8.0f)

#define USE_SECOND_UV_LAYER				// decal 2nd channel (uses UV1)
#define USE_SECOND_UV_LAYER_NORMAL_MAP	// decal 2nd channel bump map (uses UV1)

#define USE_DIRT_LAYER					// dirt (uses UV0)
#define USE_DIRT_NORMALMAP				// dirt uses normalmap
#define USE_SECOND_SPECULAR_LAYER		// 2nd layer of "wide" specular

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define USE_TWIDDLE

//#define VEHCONST_SPECULAR1_FALLOFF		(190.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.150f)
//#define VEHCONST_SPECULAR2_FALLOFF		(6.8f)
//#define VEHCONST_SPECULAR2_INTENSITY		(1.0f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"
