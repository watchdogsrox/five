
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_paint5_enveff.fx shader;
//
// props: spec/reflect/dirt 2nd channel/decal 2nd channel over enveff/damage
//
// 10/08/2010 - Andrzej: - initial;
//
// layers:
// - diffuse		- uv#0 + tileUV
// - decal			- uv#1
// - env			- uv#0 + tileUV (enveff version only)
// - dirt			- uv#0
// - dirt bumpmap	- uv#0
//
#define VEHICLE_PAINT_SHADER

#define USE_SECOND_UV_LAYER_ANISOTROPIC	// decal use anisotropic filtering

#define USE_VEHICLESHADER_SNOW
#define	USE_VEHICLESHADER_SNOW_BEFORE_DIFFUSE2	// enveff applied on top of diffuse1, just before diffuse2

#define USE_SPECTEX_TILEUV				// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV		(1.0f)

//#define VEHCONST_DIFFUSETEXTILEUV		(8.0f)


// vehicle_paint5.fx:
#define USE_SPECULAR
#ifndef USE_SPECTEX_TILEUV
	#define USE_SPECTEX_TILEUV				// tileUV control on specTex
	//#define VEHCONST_SPECTEXTILEUV		(1.0f)
#endif
#define USE_DEFAULT_TECHNIQUES


#define USE_DIFFUSETEX_TILEUV			// tileUV control on diffuseTex
//#define VEHCONST_DIFFUSETEXTILEUV		(8.0f)

#define USE_SECOND_UV_LAYER				// decal 2nd channel (uses UV1)

#define USE_DIRT_LAYER					// dirt (uses UV0)
#define USE_DIRT_NORMALMAP				// dirt uses normalmap
#define USE_SECOND_SPECULAR_LAYER		// 2nd layer of "wide" specular

#define USE_VEHICLE_DAMAGE

#define USE_TWIDDLE

//#define VEHCONST_SPECULAR1_FALLOFF		(190.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.150f)
//#define VEHCONST_SPECULAR2_FALLOFF		(6.8f)
//#define VEHCONST_SPECULAR2_INTENSITY		(1.0f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"

