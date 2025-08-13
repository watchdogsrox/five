
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_paint4_enveff.fx shader;
//
// props: spec/reflect/dirt 2nd channel/decal 2nd channel/damage
//
// 10/03/2010 - Andrzej: - initial;
//
//
//
#define USE_SECOND_UV_LAYER_ANISOTROPIC	// decal use anisotropic filtering

#define USE_VEHICLESHADER_SNOW

#define USE_SPECTEX_TILEUV				// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV		(1.0f)

//#define VEHCONST_DIFFUSETEXTILEUV		(8.0f)
#include "vehicle_paint4.fx"

