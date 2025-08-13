//
// vfx_decal_rotate_towards_camera.fx - shader for immediate mode projected textures
//  (bullet holes, explosion holes, etc.)
//

#ifndef PRAGMA_DCL
	#pragma dcl position normal tangent0 texcoord0
	#define PRAGMA_DCL
	#define ISOLATE_8
#endif

//#define USE_PARALLAX_REVERTED				// use reverted parallax mode
//#define USE_PARALLAX_CLAMP				// force procedural clamping on UVs for diffuse&bump map
#define DECAL_ROTATE_TOWARDS_CAMERA			// decal automatically rotates towards camera

#include "vfx_decal.fxh"
