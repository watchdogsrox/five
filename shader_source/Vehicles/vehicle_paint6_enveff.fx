
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal tangent0
	#define PRAGMA_DCL
#endif
//
// vehicle_paint6_enveff.fx shader;
//
// props: spec/reflect/dirt 2nd channel/decal 2nd channel over enveff/damage
//
// 28/01/2011 - Andrzej: - initial;
//
//
//
#define USE_SECOND_UV_LAYER_ANISOTROPIC	// decal use anisotropic filtering

#define USE_VEHICLESHADER_SNOW
#define	USE_VEHICLESHADER_SNOW_BEFORE_DIFFUSE2	// enveff applied on top of diffuse1, just before diffuse2

#define USE_SPECTEX_TILEUV				// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV		(1.0f)

//#define VEHCONST_DIFFUSETEXTILEUV		(8.0f)
#include "vehicle_paint6.fx"

