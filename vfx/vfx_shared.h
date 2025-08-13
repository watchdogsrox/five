
#include "../../rage/base/src/grcore/config_switches.h"

#define GLINTS_ENABLED			(0 && __D3D11 && !RSG_DURANGO)

#define ENABLE_DISTANT_CARS				(1 && (__D3D11 || RSG_ORBIS || __XENON || __PS3))

#define USE_COMBINED_PUDDLEMASK_PASS	(1 && (__D3D11 || RSG_ORBIS))

#define USE_DISTANT_LIGHTS_2			(1 && (__D3D11 || RSG_ORBIS))

#define ENABLE_PED_PASS_AA_SOURCE		(0 && (RSG_ORBIS || __D3D11)) // Don't turn this on, it triggers expensive copies and decompression and doesn't provide much extra quality

#define USE_SHADED_PTFX_MAP				(0) //now sample directly from shadow map in VS

#define PTFX_APPLY_DOF_TO_PARTICLES		(1 && (RSG_ORBIS || RSG_DURANGO || (RSG_PC && __D3D11)))

//Render to the 4 cascades with one draw call, manually viewport and scissor correctly.
#define PTFX_SHADOWS_INSTANCING					(1 && RAGE_INSTANCED_TECH)
//Same as above but use a geometry shader to viewport/scissor them.
#define PTFX_SHADOWS_INSTANCING_GEOMSHADER		(0 && PTFX_SHADOWS_INSTANCING)

//Calculate correct dof for certain alpha objects and decals - needs adding to shaders on a case by case.
#define APPLY_DOF_TO_ALPHA_DECALS		(1 && (RSG_ORBIS || RSG_DURANGO || (RSG_PC && __D3D11)))
#define APPLY_DOF_TO_GPU_PARTICLES		(1 && (RSG_ORBIS || RSG_DURANGO || (RSG_PC && __D3D11)))
#define LOCK_DOF_TARGETS_FOR_VFX		(PTFX_APPLY_DOF_TO_PARTICLES || APPLY_DOF_TO_ALPHA_DECALS || APPLY_DOF_TO_GPU_PARTICLES)

#if LOCK_DOF_TARGETS_FOR_VFX
#define LOCK_DOF_TARGETS_FOR_VFX_SWITCH(_if_DOF_,_else_) _if_DOF_
#else
#define LOCK_DOF_TARGETS_FOR_VFX_SWITCH(_if_DOF_,_else_) _else_
#endif

#ifdef __SHADERMODEL

// Shader side.
#if __SHADERMODEL >= 40
#define LINEAR_PIECEWISE_FOG_X64_EXTRA_CONDITION 1
#else // __SHADERMODEL >= 40
#define LINEAR_PIECEWISE_FOG_X64_EXTRA_CONDITION 0
#endif //__SHADERMODEL >= 40

#else //__SHADERMODEL

// C++ side.
#if RSG_PC && __D3D11
#define LINEAR_PIECEWISE_FOG_X64_EXTRA_CONDITION 1
#else // RSG_PC && __D3D11
#define LINEAR_PIECEWISE_FOG_X64_EXTRA_CONDITION 0
#endif // RSG_PC && __D3D11

#endif //__SHADERMODEL

#define LINEAR_PIECEWISE_FOG (0 && ((RSG_PC && LINEAR_PIECEWISE_FOG_X64_EXTRA_CONDITION) || RSG_DURANGO || RSG_ORBIS))

#define LINEAR_PIECEWISE_FOG_NUM_CONTOL_POINTS		8
#define LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES	8
#define LINEAR_PIECEWISE_FOG_NUM_FOG_SHAPES			4
