#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 texcoord2 normal tangent0
	#define PRAGMA_DCL
#endif
//
// vehicle_paint8.fx shader;
//
// 12/06/2017 - Andrzej: - initial;
//
//
// Uses following UV channel mapping:
// - Diffuse:	UV1;
// - Specular:	UV1;
// - Diffuse2:	UV1;
// - EnvEff0:	UV0;
// - DirtTex:	UV2;
// - NormalMap2:UV0;
//
#define VEHICLE_PAINT_SHADER


#define USE_DIFFUSETEX_USE_UV1					// main diffuse tex uses UV1
#define USE_DIFFUSETEX_TILEUV					// tileUV control on diffuseTex
//#define VEHCONST_DIFFUSETEXTILEUV				(8.0f)

#define USE_SPECULAR
#define USE_SPECTEX_TILEUV						// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV				(1.0f)
#define USE_SECOND_SPECULAR_LAYER				// 2nd layer of "wide" specular

#define USE_NORMAL_MAP
#define USE_NORMAL_MAP_USE_UV0					// normal map uses UV0

#define USE_SECOND_UV_LAYER						// decal 2nd channel (uses UV1)
#define USE_SECOND_UV_LAYER_ANISOTROPIC			// decal use anisotropic filtering

#define USE_VEHICLESHADER_SNOW
#define USE_VEHICLESHADER_SNOW_USE_UV0			// enveff uses UV0
#define	USE_VEHICLESHADER_SNOW_BEFORE_DIFFUSE2	// enveff applied on top of diffuse1, just before diffuse2


#define USE_DIRT_LAYER							// dirt
#define USE_DIRT_UV_LAYER						// dirt uses UV2 (next free slot after Diffuse2's UV1)
#define USE_DIRT_NORMALMAP						// dirt uses normalmap


#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define USE_TWIDDLE

#define USE_DEFAULT_TECHNIQUES

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"

 
