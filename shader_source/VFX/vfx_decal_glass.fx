//
// vfx_decal_specmap.fx - shader for immediate mode projected textures
//  (bullet holes, explosion holes, etc.)
//
// 2006/02/08 - Andrzej: - initial;
//
//
//
//
#ifndef PRAGMA_DCL
	#pragma dcl position normal tangent0 texcoord0
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif

// Support two sided lighting for the decal
#define USE_GLASS_LIGHTING

#define FORWARD_LOCAL_LIGHT_SHADOWING					// both these are needed to forwardlighting shadows to work on broken glass (also CParaboloidShadow::m_EnableForwardShadowsOnVehicleGlass must be set in the Code side)
#define FORWARD_LOCAL_GLASS_LIGHT_SHADOWING

#include "vfx_decal.fxh"
