
#ifndef PRAGMA_DCL
	#pragma dcl position diffuse texcoord0 texcoord1 texcoord2 normal
	#define PRAGMA_DCL
#endif
//
// gta_vehicle_vehglass.fx shader;
//
// props: spec/reflect/dirt 2nd channel/alpha/damage
//
// 10/01/2007 - Andrzej: - initial;
//
//

#define USE_NO_MATERIAL_DIFFUSE_COLOR

#define VEHICLE_GLASS_CRACK_TEXTURE

#define USE_VEHICLE_DAMAGE_GLASS
#define VEHICLE_DAMAGE_SCALE IN.damageValue.r

#define DIRT_LAYER_LEVEL_LIMIT				(0.2)

#define FORWARD_LOCAL_LIGHT_SHADOWING					// both these are needed to forwardlighting shadows to work on broken glass (also CParaboloidShadow::m_EnableForwardShadowsOnVehicleGlass must be set in the Code side)
#define FORWARD_LOCAL_GLASS_LIGHT_SHADOWING

#include "vehicle_vehglass.fx"
