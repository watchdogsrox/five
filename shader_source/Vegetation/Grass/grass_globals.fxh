#ifndef _GRASS_GLOBALS_FXH
#define _GRASS_GLOBALS_FXH

#include "grass_regs.h"

//
// Constants shared amongst all grass objects
//
//
CBSHARED BeginConstantBufferPagedDX10(grassglobals, GRASS_BREG_GRASSGLOBALS)

shared bool	bVehColl0Enabled : bVehColl0Enabled0 REGISTER(GRASS_BREG_VEHCOLL0ENABLED) = false;
shared bool	bVehColl1Enabled : bVehColl1Enabled0 REGISTER(GRASS_BREG_VEHCOLL1ENABLED) = false;
shared bool	bVehColl2Enabled : bVehColl2Enabled0 REGISTER(GRASS_BREG_VEHCOLL2ENABLED) = false;
shared bool	bVehColl3Enabled : bVehColl3Enabled0 REGISTER(GRASS_BREG_VEHCOLL3ENABLED) = false;

#if __WIN32PC || PLANTS_CAST_SHADOWS
shared float4 depthValueBias : depthValueBias0 < int nostrip=1; >;
#endif

//Need to define this here because we are not defining USE_INSTANCING.
#if __XENON || __PS3
//Instanced parameters for instancing shaders overlap skinning constants.
//Xbox instancing requires an additional constant which defines how many instances are drawn so the proper offsets can be computed.
SHARED float4 gInstancingParams		REGISTER2(vs, PASTE2(c, INSTANCE_SHADER_CONTROL_SLOT));
#endif

EndConstantBufferDX10(grassglobals)

#endif // _GRASS_GLOBALS_FXH
