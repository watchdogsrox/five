
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_paint7.fx shader;
//
// props: spec/reflect/dirt 2nd channel/decal 2nd channel/damage
//
// 20/10/2016 - Andrzej: - initial;
//
//
#define VEHICLE_PAINT_SHADER

#define USE_SPECULAR
#define USE_DEFAULT_TECHNIQUES

#define USE_DIFFUSETEX_TILEUV			// tileUV control on diffuseTex
//#define VEHCONST_DIFFUSETEXTILEUV		(8.0f)

#define USE_SECOND_UV_LAYER				// decal 2nd channel
#define USE_SECOND_UV_LAYER_ANISOTROPIC	// decal use anisotropic filtering
#define USE_SPECULAR_UV1				// specular UV's are taken from 2nd diffuse layer 

#define USE_DIRT_LAYER
//#define	USE_DIRT_UV_LAYER			// dirt 2nd channel
#define USE_SECOND_SPECULAR_LAYER		// 2nd layer of "wide" specular

#define USE_VEHICLE_DAMAGE
#define USE_SUPERSAMPLING

#define USE_TWIDDLE

//#define VEHCONST_SPECULAR1_FALLOFF			(510.0f)
//#define VEHCONST_SPECULAR1_INTENSITY		(0.8f)
//#define VEHCONST_SPECTEXTILEUV			(1.0f)
#define VEHCONST_SPECULAR2_FALLOFF			(15.0f)
#define VEHCONST_SPECULAR2_INTENSITY		(0.75f)
//#define VEHCONST_FRESNEL					(0.96f)

#define USE_PAINT_RAMP

#include "vehicle_common.fxh"

 
