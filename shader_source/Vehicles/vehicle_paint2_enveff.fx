
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_paint2_enveff.fx shader;
//
// props: spec/reflect/dirt/damage
//
// 02/04/2009 - Andrzej: - initial;
//
//
#define  USE_VEHICLESHADER_SNOW

#define USE_SPECTEX_TILEUV				// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV		(1.0f)

//#define VEHCONST_ENVEFFTEXTILEUV		(8.0f)
#include "vehicle_paint2.fx"
