
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 normal 
	#define PRAGMA_DCL
#endif
//
// vehicle_paint1_enveff shader;
//
// props: spec/reflect/dirt 2nd channel/damage
//
// 02/04/2009 - Andrzej: - initial;
//
//
#define  USE_VEHICLESHADER_SNOW

#define USE_SPECTEX_TILEUV				// tileUV control on specTex
//#define VEHCONST_SPECTEXTILEUV		(1.0f)

//#define VEHCONST_ENVEFFTEXTILEUV		(8.0f)
#include "vehicle_paint1.fx"
