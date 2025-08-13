/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    drawlist.cpp
// PURPOSE : intermediate data between game entitites and command buffers. Gives us
// independent data which can be handed off to the render thread
// AUTHOR :  john.
// CREATED : 30/8/06
//
/////////////////////////////////////////////////////////////////////////////////
#include "renderer/DrawLists/drawListNY.h"

#include "fragment/tune.h"
#include "grcore/allocscope.h"
#include "grcore/config.h"
#include "grcore/device.h"
#include "grcore/viewport.h"
#include "grcore/texture.h"
#include "rmcore/instance.h"
#include "system/xtl.h"

#if __D3D
#include "system/d3d9.h"
#endif

#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/HudTools.h"
#include "scene/entity.h"
#include "peds/ped.h"
#include "peds/ped.h"
#include "peds/rendering/pedVariationPack.h"
#include "physics/breakable.h"
#include "vehicles/vehicle.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/PostProcessFX.h"
#include "renderer/renderThread.h"
#include "script/script.h"
#include "shaders/customshadereffectbase.h"
#include "shaders/CustomShaderEffectTint.h"
#include "shaders/shaderLib.h"
#include "system/system.h"
#include "scene/world/GameWorld.h"
#include "text/textconversion.h"
#include "renderer/lights/lights.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseStdNY.h"

#include "fwdrawlist/drawlist.h"

// GTA4 includes
#include "frontend/mobilePhone.h"

#include "data/aes_init.h"
AES_INIT_0;

RENDER_OPTIMISATIONS()

// initialise correct sizes of debug structs
#if __DEV
void InitNYDrawListDebug(void){

	dlCmdBase::InitDebugMem(DC_NumNYDrawCommands);
}

void ShutdownNYDrawListDebug(void) { 
	
	dlCmdBase::ResetDebugMem(); 
}
#else //__DEV
void InitNYDrawListDebug(void){}
void ShutdownNYDrawListDebug(void) {}
#endif //__DEV

void InitNYDrawList(void)
{
	DLC_REGISTER_EXTERNAL(CDrawPhoneDC_NY);
	DLC_REGISTER_EXTERNAL(CDrawNetworkPlayerName_NY);
#if __BANK
	DLC_REGISTER_EXTERNAL(CDrawNetworkDebugOHD_NY);
#endif // __BANK
}

#define PHONESCREEN_USES_FIXED_LUMINOSITY 1

CDrawPhoneDC_NY::CDrawPhoneDC_NY(
	u32 modelInfoIdx, 
	Vector3& pos, 
	float scale, 
	Vector3& angle, 
	EulerAngleOrder RotationOrder, 
	float brightness, 
	float screenBrightness,
	float naturalAmbientScale,
	float artificialAmbientScale,
	int paletteId,
	Matrix34 &camMatrix,
	eDrawPhoneMode drawMode,
	bool inInterior)
{
	gDrawListMgr->AddArchetypeReference(modelInfoIdx);

	m_modelInfoIdx = modelInfoIdx;
	m_pos = pos;
	m_scale = scale;
	m_angle = angle;
	m_RotationOrder = RotationOrder;
	m_brightness = brightness;
	m_screenBrightness = screenBrightness;
	m_camMatrix = camMatrix;
	m_artificialAmbientScale = artificialAmbientScale;
	m_naturalAmbientScale = naturalAmbientScale;
	m_paletteId = paletteId;
	m_inInterior = inInterior;
	m_DrawMode = drawMode;
}

void CDrawPhoneDC_NY::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_modelInfoIdx)));
	Assert(pModelInfo);
	Assert(pModelInfo->GetDrawable());

	rmcDrawable *drawable = pModelInfo->GetDrawable();
	Assert(drawable);

	// Set up viewport
	grcViewport vp;
#if USE_INVERTED_PROJECTION_ONLY
	vp.SetInvertZInProjectionMatrix(true,false);
#endif // USE_INVERTED_PROJECTION_ONLY
	CRenderPhasePhoneModel::SetupPhoneViewport(&vp, m_pos);
	grcViewport::SetCurrent(&vp);
	
	// Force UI technique
	s32 prevForceTechnique = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(grmShaderFx::FindTechniqueGroupId("ui"));
	
	 //turn off fog and depthfx
	CShaderLib::DisableGlobalFogAndDepthFx();

	// Set the light for mobile phone

	CShaderLib::SetGlobalNaturalAmbientScale(m_naturalAmbientScale);
	CShaderLib::SetGlobalArtificialAmbientScale(m_artificialAmbientScale);
	CShaderLib::SetGlobalInInterior(m_inInterior);
	
	// Set up the tint
	CCustomShaderEffectBaseType *type = pModelInfo->GetCustomShaderEffectType();
	if( type && type->GetType() == CSE_TINT)
	{ 
		atUserArray<CCustomShaderEffectTintType::structTintShaderInfo> tintInfos;
		CCustomShaderEffectTintType *tintType = (CCustomShaderEffectTintType *)type;
		tintType->GetTintInfos(&tintInfos);
		const u32 numTintShaderInfos = tintInfos.GetCount();
		u8 *tintPal = (u8*)alloca(sizeof(u8)*numTintShaderInfos);
		CCustomShaderEffectTint::SelectTintPalette(u8(m_paletteId&0xff), tintPal, tintType, NULL);
		CCustomShaderEffectTint::SetShaderVariables(tintPal, tintType, drawable);
	}

	Matrix34 M;
	CScriptEulers::MatrixFromEulers(M, m_angle, m_RotationOrder);
	M.d = Vector3(0.0f, 0.0f, m_pos.z);
	float fTripleHeadScale = 1.0f;
#if SUPPORT_MULTI_MONITOR
	float fWidth = (float)VideoResManager::GetUIWidth();
	float fHeight = (float)VideoResManager::GetUIHeight();
	float fAspect = fWidth / fHeight;

	if(GRCDEVICE.GetMonitorConfig().isMultihead() && fAspect > 3.5f)
		fTripleHeadScale = 0.33333f;
#endif // SUPPORT_MULTI_MONITOR
	M.Scale(m_scale * fTripleHeadScale, m_scale, m_scale);

	// Set the mobile with directional light
	grcLightState::SetEnabled(false);

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetBlendState(CPhoneMgr::GetDefaultBS());

	// Force it on the ppu
#if __PS3
	if (m_DrawMode != DRAWPHONE_ALL)
	{
		drawable->MakePpuOnly();
	}
#endif

	CShaderLib::SetGlobalToneMapScalers(Vector4(1.0, 1.0, 1.0, 1.0));

#if PHONESCREEN_USES_FIXED_LUMINOSITY
		Lights::SetFakeLighting(m_brightness, m_brightness * 0.1f, m_camMatrix);
		const float oldGES = CShaderLib::GetGlobalEmissiveScale();
		CShaderLib::SetGlobalEmissiveScale(m_screenBrightness, true);
#else
		Lights::SetFakeLighting(m_brightness, m_brightness * 0.1f, m_camMatrix);
#endif // PHONESCREEN_USES_FIXED_LUMINOSITY

	CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap(NULL);
	CRenderPhaseReflection::SetReflectionMapExt(grcTexture::NoneBlack);

	u32 prevBucketMasks[6];
	grmShaderGroup *shaderGroup = &(drawable->GetShaderGroup());
	int shaderCount = Min(shaderGroup->GetCount(), 6);

	if (m_DrawMode != DRAWPHONE_ALL)
	{
		for (int i = 0; i <  shaderCount; i++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(i);

			if (shader == NULL)
				continue;

			prevBucketMasks[i] = shader->GetDrawBucketMask();

			if (m_DrawMode == DRAWPHONE_SCREEN_ONLY)
			{
				shader->SetDrawBucketMask(0U);
			}

			if (atHashString(shader->GetName()) == atHashString("emissive"))
			{
				if (m_DrawMode == DRAWPHONE_BODY_ONLY)
					shader->SetDrawBucketMask(0U);
				else
					shader->SetDrawBucketMask(prevBucketMasks[i]);
			}
		}
	}
	
	drawable->Draw(M, CRenderer::GenerateBucketMask(CRenderer::RB_OPAQUE), 0, 0);

	if (m_DrawMode != DRAWPHONE_ALL)
	{
		for (int i = 0; i < shaderCount; i++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(i);
			if (shader == NULL)
				continue;

			shader->SetDrawBucketMask(prevBucketMasks[i]);
		}
	}

#if PHONESCREEN_USES_FIXED_LUMINOSITY
	CShaderLib::SetGlobalEmissiveScale(oldGES);
#endif // PHONESCREEN_USES_FIXED_LUMINOSITY

	grcStateBlock::SetBlendState(CPhoneMgr::GetRGBBS());
	
	if (m_DrawMode != DRAWPHONE_BODY_ONLY)
	{
		drawable->Draw(M, CRenderer::GenerateBucketMask(CRenderer::RB_ALPHA), 0, 0);
	}
	grcStateBlock::SetBlendState(CPhoneMgr::GetDefaultBS());

	// We don't want anything to screw up our nice alpha layer,
	// For the msaa resolve!
	grmShaderFx::SetForcedTechniqueGroupId(prevForceTechnique);
	grcLightState::SetEnabled(false);
	CShaderLib::EnableGlobalFogAndDepthFx();
	Lights::ResetLightingGlobals();
	
	//
	// screen fade:
	//
	grcStateBlock::SetBlendState(CPhoneMgr::GetRGBBS());
	camInterface::RenderFade();

	grcViewport::SetCurrent(NULL);
	
	grcStateBlock::SetBlendState(CPhoneMgr::GetDefaultBS());
}

CDrawNetworkPlayerName_NY::CDrawNetworkPlayerName_NY(const Vector2 &pos, const Color32 &col, float scale, const char *playerName) :
m_pos(pos)
, m_col(col)
, m_scale(scale)
{
    m_playerName[0] = '\0';

    if(AssertVerify(playerName) && AssertVerify(strlen(playerName) < MAX_PLAYER_NAME))
    {
        safecpy(m_playerName, playerName, MAX_PLAYER_NAME);
    }
}

void CDrawNetworkPlayerName_NY::Execute()
{
    CTextLayout playerNameText;

    NetworkUtils::SetFontSettingsForNetworkOHD(&playerNameText, m_scale, false);
    playerNameText.SetColor(m_col);

	playerNameText.SetWrap(Vector2(m_pos.x, m_pos.x+playerNameText.GetStringWidthOnScreen(m_playerName, true)));
	playerNameText.SetEdge(0.0f);
	playerNameText.SetBackground(true, true);
	playerNameText.SetBackgroundColor(Color32(0,0,0,150));
	playerNameText.SetRenderUpwards(true);

    playerNameText.Render(&m_pos, m_playerName);
}

#if __BANK

CDrawNetworkDebugOHD_NY::CDrawNetworkDebugOHD_NY(const Vector2 &pos, const Color32 &col, float scale, const char *debugText) :
m_pos(pos)
, m_col(col)
, m_scale(scale)
{
    m_debugText[0] = '\0';

    if(AssertVerify(debugText) && AssertVerify(strlen(debugText) < MAX_TEXT_LEN))
    {
        safecpy(m_debugText, debugText, MAX_TEXT_LEN);
    }
}

void CDrawNetworkDebugOHD_NY::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PUSH_DEFAULT_SCREEN();
    CTextLayout debugTextLayout;

    NetworkUtils::SetFontSettingsForNetworkOHD(&debugTextLayout, m_scale, false);
    debugTextLayout.SetColor(m_col);

    debugTextLayout.Render(&m_pos, m_debugText);
	POP_DEFAULT_SCREEN();
}

#endif
