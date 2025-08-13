// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "atl/array.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/console.h"
#include "grcore/quads.h"
#include "grmodel/shader.h"
#include "file/asset.h"
#include "system/system.h"
#include "system/memory.h"

// framework
#include "grcore/debugdraw.h"
#include "fwrenderer/RenderPhaseBase.h"

// game
#include "Debug/Rendering/DebugRendering.h"
#include "Debug/Editing/LightEditing.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/viewportmanager.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/GtaDrawable.h"
#include "renderer/rendertargets.h"
#include "renderer/SpuPM/SpuPmMgr.h"
#include "renderer/Util/ShaderUtil.h"
#include "optimisations.h"
#include "renderer/lights/lights.h"
#include "Renderer/Lights/LightEntity.h"
#include "scene/FileLoader.h"
#include "shaders/ShaderEdit.h"
#include "peds/Ped.h"
#include "debug/DebugScene.h"
#include "scene/world/GameWorld.h"
#include "Debug/Rendering/DebugLights.h"
#include "Debug/Rendering/DebugView.h"
#include "Debug/Rendering/DebugDeferred.h"
#include "Debug/Rendering/DebugLighting.h"
#include "renderer/Util/RenderDocManager.h"

RENDER_OPTIMISATIONS()

#if __BANK

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

extern bool gLastGenMode;

// =============================================================================================== //
// DebugRenderingMgr
// =============================================================================================== //

void DebugRenderingMgr::Init()
{
	sysMemUseMemoryBucket b(MEMBUCKET_RENDER);

	DebugRendering::SetShader(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DEBUG]);

	DebugRendering::SetTechnique(DR_TECH_DEFERRED, "deferred");
	DebugRendering::SetTechnique(DR_TECH_LIGHT, "light");
	DebugRendering::SetTechnique(DR_TECH_COST, "cost");
	DebugRendering::SetTechnique(DR_TECH_DEBUG, "debug");

	DebugRendering::SetShaderVar(DR_SHADER_VAR_STANDARD_TEXTURE, "standardTexture");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_STANDARD_TEXTURE1, "standardTexture1");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_POINT_TEXTURE, "pointTexture");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_OVERRIDE_COLOR0, "overrideColor0");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_LIGHT_CONSTANTS, "lightConstants");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_COST_CONSTANTS, "costConstants");

	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_CONSTS, "deferredLightConsts");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PARAMS0, "deferredLightParams0");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PARAMS1, "deferredLightParams1");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PARAMS2, "deferredLightParams2");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PARAMS3, "deferredLightParams3");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PARAMS4, "deferredLightParams4");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PARAMS5, "deferredLightParams5");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_POSITION, "deferredLightPosition");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_DIRECTION, "deferredLightDirection");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_TANGENT, "deferredLightTangent");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_COLOUR, "deferredLightColourAndIntensity");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_TEXTURE, "deferredLightTexture");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_SCREEN_SIZE, "deferredLightScreenSize");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFFERED_SHADOW_PARAM0, "dShadowParam0");

	DebugRendering::SetShaderGlobalVar(DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE0, "gbufferTexture0Global");
	DebugRendering::SetShaderGlobalVar(DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE1, "gbufferTexture1Global");
	DebugRendering::SetShaderGlobalVar(DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE2, "gbufferTexture2Global");
	DebugRendering::SetShaderGlobalVar(DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE3, "gbufferTexture3Global");
	DebugRendering::SetShaderGlobalVar(DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE_DEPTH, "gbufferTextureDepthGlobal");

	DebugRendering::SetShaderVar(DR_SHADER_VAR_WINDOW_PARAMS, "windowParams");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_WINDOW_MOUSE_PARAMS, "mouseParams");

	DebugRendering::SetShaderVar(DR_SHADER_VAR_DEFERRED_PROJECTION_PARAMS, "deferredProjectionParams");

	DebugRendering::SetShaderVar(DR_SHADER_VAR_RANGE_LOWER_COLOR, "diffuseRangeLowerColour");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_RANGE_MID_COLOR, "diffuseRangeMidColour");
	DebugRendering::SetShaderVar(DR_SHADER_VAR_RANGE_UPPER_COLOR, "diffuseRangeUpperColour");
	
	// Init for other classes
	DebugDeferred::Init();
	DebugLights::Init();
	DebugView::Init();
	DebugLighting::Init();
}

// ----------------------------------------------------------------------------------------------- //

void DebugRenderingMgr::Shutdown()
{
	DebugRendering::DeleteShader();
}

// ----------------------------------------------------------------------------------------------- //

void DebugRenderingMgr::Update()
{
	DebugLights::Update();
	DebugView::Update();
	DebugDeferred::Update();
	DebugLighting::Update();
}

// ----------------------------------------------------------------------------------------------- //

void DebugRenderingMgr::Draw()
{
	DebugLights::Draw();
	DebugView::Draw();
	DebugDeferred::Draw();
	DebugLighting::Draw();
}

// ----------------------------------------------------------------------------------------------- //

void DebugRenderingMgr::AddWidgets(bkBank *bk)
{
	bk->PushGroup("Debug");
		DebugDeferred::AddWidgets(bk);
		DebugView::AddWidgets(bk);
		RENDERPHASEMGR.AddWidgets(*bk);
		UpsampleHelper::AddWidgets(bk);

#if RENDERDOC_ENABLED
		RenderDocManager::AddWidgets(bk);
#endif

	bk->PopGroup();
}

// =============================================================================================== //
// DEBUG RENDERING
// =============================================================================================== //

grmShader *DebugRendering::m_shader = NULL;
grcEffectTechnique DebugRendering::m_techniques[DR_TECH_COUNT] = { grcetNONE };
grcEffectVar DebugRendering::m_shaderVars[DR_SHADER_VAR_COUNT] = { grcevNONE };
grcEffectGlobalVar DebugRendering::m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_COUNT] = { grcegvNONE };

// ----------------------------------------------------------------------------------------------- //

void DebugRendering::SetShaderVar(DR_SHADER_VARS shaderVar, const char* shaderVarName)
{
	m_shaderVars[shaderVar] = m_shader->LookupVar(shaderVarName);
}

// ----------------------------------------------------------------------------------------------- //

void DebugRendering::SetShaderGlobalVar(DR_SHADER_GLOBAL_VARS shaderVar, const char* shaderVarName)
{
	m_shaderGlobalVars[shaderVar] = grcEffect::LookupGlobalVar(shaderVarName);
}


// ----------------------------------------------------------------------------------------------- //

void DebugRendering::SetTechnique(DR_TECHNIQUES tech, const char* techniqueName)
{
	m_techniques[tech] = m_shader->LookupTechnique(techniqueName, false);
}

// ----------------------------------------------------------------------------------------------- //

void DebugRendering::SetShaderParams()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	Vector4 ooscreenSize = Vector4(float(GRCDEVICE.GetWidth()),
								   float(GRCDEVICE.GetHeight()),
								   1.0f / float(GRCDEVICE.GetWidth()), 
								   1.0f / float(GRCDEVICE.GetHeight()));

	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_DEFERRED_SCREEN_SIZE], ooscreenSize);

	Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_DEFERRED_PROJECTION_PARAMS], projParams);

}

// ----------------------------------------------------------------------------------------------- //

void DebugRendering::SetStencil(DR_SHADER_GLOBAL_VARS shaderVar)
{
#if __PS3 || __XENON
	grcEffect::SetGlobalVar(m_shaderGlobalVars[shaderVar], CRenderTargets::GetDepthBufferAsColor() );
#else
	grcEffect::SetGlobalVar(m_shaderGlobalVars[shaderVar], grcTexture::None);
#endif
}

// ----------------------------------------------------------------------------------------------- //

void DebugRendering::RenderCost(grcRenderTarget* renderTarget, int pass, float opacity, int overdrawMin, int overdrawMax, const char* title)
{
	SetShaderParams();
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_POINT_TEXTURE], renderTarget);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_COST_CONSTANTS], Vec4f((float)overdrawMin, (float)Max<int>(overdrawMin, overdrawMax), 0.0f, opacity));

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, (opacity < 1.0f) ? grcStateBlock::BS_Normal : grcStateBlock::BS_Default);

	if (ShaderUtil::StartShader("Cost reveal", m_shader, m_techniques[DR_TECH_COST], pass))
	{
		ShaderUtil::DrawNormalizedQuad();
		ShaderUtil::EndShader(m_shader);
	}

	if (title)
	{
		RenderCostLegend(true, overdrawMin, overdrawMax, title);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugRendering::RenderCostLegend(bool useGradient, int overdrawMin, int overdrawMax, const char* title)
{
	overdrawMax = Max<int>(overdrawMin, overdrawMax);

	const float startX = 0.05f;
	const float endX = 0.95f;
	const float height = 0.03f;
	const float startY = 0.1f;
	const float borderWidth = 0.001f;
	const float spacing = 0.001f;
	const float dX = (endX - startX)/(float)(overdrawMax - overdrawMin + 1);
	const float width = dX - spacing*2.0f;

	const Color32 white = Color32(255,255,255,255);
	const Vec4V colour0 = Vec4V(V_Z_AXIS_WONE); // blue
	const Vec4V colour1 = Vec4V(V_Y_AXIS_WONE); // green
	const Vec4V colour2 = Vec4V(1.0f, 1.0f, 0.0f, 1.0f); // yellow
	const Vec4V colour3 = Vec4V(V_X_AXIS_WONE); // red

	float currX = startX;
	float currY = startY + height + spacing + 1.0f/(float)GRCDEVICE.GetHeight();

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
	grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

	for (int i = overdrawMin; i <= overdrawMax; i++)
	{
		CSprite2d::DrawRect(fwRect(currX, startY, currX + width, startY + height), white); // border color
		float cost = 1.0f;
		Vec4V legendColour(V_ZERO_WONE);

		if (overdrawMax > overdrawMin)
		{
			cost = (float)(i - overdrawMin)/(float)(overdrawMax - overdrawMin); // [0..1]
		}

		if (useGradient)
		{
			// 0.00f - 0.33f = blue to green
			// 0.33f - 0.66f = green to yellow
			// 0.66f - 1.00f = yellow to red

			if      (cost*3.0f < 1.0f) { legendColour = Lerp(ScalarV(cost*3.0f - 0.0f), colour0, colour1); }
			else if (cost*3.0f < 2.0f) { legendColour = Lerp(ScalarV(cost*3.0f - 1.0f), colour1, colour2); }
			else                       { legendColour = Lerp(ScalarV(cost*3.0f - 2.0f), colour2, colour3); }
		}
		else
		{
			legendColour = Vec4V(ScalarV(cost));
			legendColour.SetW(ScalarV(V_ONE));
		}

		CSprite2d::DrawRect(fwRect(currX + borderWidth, startY + borderWidth, currX + width - borderWidth, startY + height - borderWidth), Color32(legendColour));

		if (i == overdrawMin)
		{
			grcDebugDraw::Text(Vec2V(currX, startY - 10.0f/(float)GRCDEVICE.GetHeight()), white, title, false);
		}

		if (i == overdrawMin || i == overdrawMax || (overdrawMax - overdrawMin + 1) <= 50) // text gets too crowded with more than 50 labels
		{
			grcDebugDraw::Text(Vec2V(currX, currY), white, atVarString("%d%s", i, i == overdrawMax ? "+" : "").c_str(), false);
		}

		currX += dX;
	}

	grcDebugDraw::TextFontPop();
}

// ----------------------------------------------------------------------------------------------- //

#endif // __BANK
