
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse specular texcoord0 normal
	#define PRAGMA_DCL
#endif
// 
//	trees.fx
//
//	2007/02/15	-				- initial;
//	2009/04/14	-	Andrzej:	- code cleanup;
//	2009/04/14	-	Andrzej:	- tool technique;	
//	2009/04/20	-	Andrzej:	- tree micromovements;
//	2009/04/24	-	Andrzej:	- global wind bending;
//	2009/11/30	-	Andrzej:	- trees_common.fxh added;
//
//
//
//
//

#define USE_ALPHACLIP_FADE

#include "trees_common.fxh"
