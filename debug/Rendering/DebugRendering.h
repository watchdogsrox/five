#ifndef DEBUG_RENDERING_H_
#define DEBUG_RENDERING_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// framework
#include "fwscene/world/InteriorLocation.h"

// rage
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "grcore/effect.h"
#include "grmodel/shader.h"
#include "renderer/Util/Util.h"

namespace rage
{
	class bkBank;
	class grmShader;
}

class CLightSource;
class CLightEntity;

#if __BANK

// =============================================================================================== //
// DEFINITIONS
// =============================================================================================== //

enum DR_SHADER_VARS
{
	DR_SHADER_VAR_STANDARD_TEXTURE = 0,
	DR_SHADER_VAR_STANDARD_TEXTURE1,
	DR_SHADER_VAR_POINT_TEXTURE,
	DR_SHADER_VAR_OVERRIDE_COLOR0,
	DR_SHADER_VAR_DEFERRED_PROJECTION_PARAMS,
	DR_SHADER_VAR_DEFERRED_SCREEN_SIZE,
	DR_SHADER_VAR_LIGHT_CONSTANTS,
	DR_SHADER_VAR_COST_CONSTANTS,
	DR_SHADER_VAR_DEFERRED_CONSTS,
	DR_SHADER_VAR_DEFERRED_PARAMS0,
	DR_SHADER_VAR_DEFERRED_PARAMS1,
	DR_SHADER_VAR_DEFERRED_PARAMS2,
	DR_SHADER_VAR_DEFERRED_PARAMS3,
	DR_SHADER_VAR_DEFERRED_PARAMS4,
	DR_SHADER_VAR_DEFERRED_PARAMS5,
	DR_SHADER_VAR_DEFERRED_POSITION,
	DR_SHADER_VAR_DEFERRED_DIRECTION,
	DR_SHADER_VAR_DEFERRED_TANGENT,
	DR_SHADER_VAR_DEFERRED_COLOUR,
	DR_SHADER_VAR_DEFERRED_TEXTURE,
	DR_SHADER_VAR_DEFFERED_SHADOW_PARAM0,
	DR_SHADER_VAR_WINDOW_PARAMS,
	DR_SHADER_VAR_WINDOW_MOUSE_PARAMS,
	DR_SHADER_VAR_RANGE_LOWER_COLOR,
	DR_SHADER_VAR_RANGE_MID_COLOR,
	DR_SHADER_VAR_RANGE_UPPER_COLOR,
	DR_SHADER_VAR_COUNT
};

enum DR_SHADER_GLOBAL_VARS
{
	DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE0,
	DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE1,
	DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE2,
	DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE3,
	DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE_DEPTH,
	DR_SHADER_GLOBAL_VAR_COUNT

};

enum DR_TECHNIQUES
{
	DR_TECH_DEFERRED = 0,
	DR_TECH_LIGHT,
	DR_TECH_COST,
	DR_TECH_DEBUG,
	DR_TECH_COUNT
};

enum DR_COST_PASS
{
	DR_COST_PASS_OVERDRAW = 0,
	DR_COST_PASS_COUNT
};

// =============================================================================================== //
// DEBUG RENDERING
// =============================================================================================== //

class DebugRendering
{
public:

	static void SetShader(grmShader *shader) { m_shader = shader; }
	static grmShader* GetShader() { return m_shader; }
	static void DeleteShader() { SAFE_DELETE(m_shader); }

	static void SetShaderVar(DR_SHADER_VARS shaderVar, const char* shaderVarName);
	static void SetShaderGlobalVar(DR_SHADER_GLOBAL_VARS shaderVar, const char* shaderVarName);
	static void SetTechnique(DR_TECHNIQUES tech, const char* techniqueName);

	static void RenderCost(grcRenderTarget* renderTarget, int pass, float opacity, int overdrawMin, int overdrawMax, const char* title);
	static void RenderCostLegend(bool useGradient, int overdrawMin, int overdrawMax, const char* title);

protected:

	// Functions
	static void SetShaderParams();
	static void SetStencil(DR_SHADER_GLOBAL_VARS shaderVar);
	
	// Variables
	static grmShader *m_shader;
	static grcEffectTechnique m_techniques[];
	static grcEffectVar m_shaderVars[];
	static grcEffectGlobalVar m_shaderGlobalVars[];
};

// =============================================================================================== //
// DEBUG RENDERING MANAGER
// =============================================================================================== //

class DebugRenderingMgr
{
public:

	// Functions
	static void Init();
	static void Shutdown();
	static void Update();
	static void Draw();
	static void AddWidgets(bkBank *bk);
};

#endif // __BANK

#endif // DEBUG_RENDERING_H_
