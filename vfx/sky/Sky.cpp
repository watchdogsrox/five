//
// vfx/sky/Sky.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "rmcore/drawable.h"

// INCLUDES
#include "Vfx/Sky/Sky.h"

// rage
#include "grmodel/modelfactory.h"
#include "grcore/allocscope.h"
#include "grcore/light.h"
#include "profile/timebars.h"

#include "system/nelem.h"
#if __D3D
#include "system/xtl.h"
#include "system/d3d9.h"
#endif

#if __BANK
#include "bank/widget.h"
#include "bank/bkmgr.h"
#endif

// framework
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"
#include "fwscene/stores/txdstore.h"
#include "fwutil/xmacro.h"

// game
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "Core/Game.h"
#include "debug/Rendering/DebugDeferred.h"
#include "renderer/GtaDrawable.h"
#include "renderer/renderthread.h"
#include "renderer/lights/lights.h"
#include "renderer/RenderSettings.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"
#include "scene/Entity.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "timecycle/TimeCycle.h"
#include "timecycle/TimeCycleConfig.h"
#include "vfx/vfxHelper.h"
#include "vfx/vfx_channel.h"
#include "Vfx/Misc/Coronas.h"
#include "vfx/sky/SkySettings.h"
#include "vfx/VfxSettings.h"

VFX_MISC_OPTIMISATIONS()

//-----------------------------------------------------------------------------------------
RAGE_DEFINE_CHANNEL(sky)
//-----------------------------------------------------------------------------------------

#define NUM_VERTS_FOR_SKY_RENDER (VFX_SINCOS_LOOKUP_TOTAL_ENTRIES + 2)
#define NUM_VERTS_FOR_SUN_MOON_RENDER (VFX_SINCOS_LOOKUP_TOTAL_ENTRIES/2 + 2)
#define DOME_SCALE_FOR_VERTS (15000.0f)

// GLOBAL VARIABLES
CSky g_sky;
static const strStreamingObjectName SKY_DOME_FILENAME("platform:/models/skydome",0x189213E7);

#if __DEV
extern bool g_AllowUncompressedTexture;
#endif

// ------------------------------------------------------------------------------------------------------------------//
//														CSky														 //
// ------------------------------------------------------------------------------------------------------------------//
CSky::CSky() 
	: m_skyDomeDictionary(NULL)
	, m_overwriteTimeCycle(false)
	, m_fSunDiscSizeMul(250.0f)
{
#if __BANK
	m_overridePassSelection = false;
	m_enableCloudDebugging = false;
	m_RenderSky = true;
	m_bEnableForcedDepth = true;
	m_bMirrorForceInfiniteProjectionON = false;
	m_bMirrorForceInfiniteProjectionOFF = false;
	m_mirrorInfiniteProjectionEpsilonExponent = -6.0f;
	m_bMirrorForceSetClipON = false;
	m_bMirrorForceSetClipOFF = false;
	m_bMirrorEnableSkyPlane = false;
	m_cloudDebuggingMode = 0;
	m_cloudDebuggingTexcoordScale = 1.0f;
	m_cloudDebuggingParamsId = grcevNONE;
#endif
}

// ------------------------------------------------------------------------------------------------------------------//
//														~CSky														 //
// ------------------------------------------------------------------------------------------------------------------//
CSky::~CSky()
{
	m_skySettings.Shutdown();
}

// ------------------------------------------------------------------------------------------------------------------//
//													Update															 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::Update()
{

}

// ------------------------------------------------------------------------------------------------------------------//
//													PreRender														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::PreRender()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// Generate our perlin noise
	PF_PUSH_TIMEBAR_DETAIL("Generate Perlin");
	if (!g_sky.m_overwriteTimeCycle)
	{
		g_timeCycle.GetNoiseSkySettings(&g_sky.m_skySettings);
	}
	g_sky.GeneratePerlin();
	PF_POP_TIMEBAR_DETAIL();
}

// ------------------------------------------------------------------------------------------------------------------//
//													UpdateSettings													 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::UpdateSettings()
{
	// Update sky settings from time cycle
	if (!m_overwriteTimeCycle)
	{
		g_timeCycle.GetSkySettings(&m_skySettings);
	}
	else
	{
		g_timeCycle.GetMinimalSkySettings(&m_skySettings);
	}

	m_skySettings.m_time = (float)g_timeCycle.GetCurrCycleTime();

	// Update sky settings
	m_skySettings.UpdateRenderThread();
}

// ------------------------------------------------------------------------------------------------------------------//
//														Init														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		// Create model and setup shaders
		ASSET.PushFolder("common:/shaders");
			m_skyShader = grmShaderFactory::GetInstance().Create();
			m_skyShader->Load("sky_system");
		ASSET.PopFolder();

		// Set techniques
		m_perlinNoiseTechniqueIdx =  m_skyShader->LookupTechnique("draw_perlin_noise", true);
		m_skyPlaneTechniqueIdx =  m_skyShader->LookupTechnique("sky_plane", true);

		LoadSkyDomeModel();

		// Init the texture dictionary
#if __DEV
		g_AllowUncompressedTexture = true;
#endif
		strLocalIndex txdSlot = strLocalIndex(vfxUtil::InitTxd("skydome"));
		g_TxdStore.PushCurrentTxd();
		g_TxdStore.SetCurrentTxd(txdSlot);                         
#if __DEV
		g_AllowUncompressedTexture = false;
#endif

		m_skySettings.Init(m_skyShader); // Wrap all shader variables in one struct


		g_TxdStore.PopCurrentTxd();  

		// Sky State blocks
		grcBlendStateDesc m_skyMoonSunBlendState;
		m_skyMoonSunBlendState.BlendRTDesc[0].BlendEnable = TRUE;
		m_skyMoonSunBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		m_skyMoonSunBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_DESTALPHA;
		m_skyMoonSunBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
		m_skyMoonSun_B = grcStateBlock::CreateBlendState(m_skyMoonSunBlendState);

		grcDepthStencilStateDesc skyCubeReflection_DS;
		skyCubeReflection_DS.DepthEnable = TRUE;
		skyCubeReflection_DS.DepthWriteMask = FALSE;
		skyCubeReflection_DS.DepthFunc = grcRSV::CMP_LESSEQUAL;
		skyCubeReflection_DS.StencilEnable = TRUE;
		skyCubeReflection_DS.StencilReadMask = 0xff;
		skyCubeReflection_DS.StencilWriteMask = 0x00;
		skyCubeReflection_DS.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		skyCubeReflection_DS.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
		m_skyCubeReflection_DS = grcStateBlock::CreateDepthStencilState(skyCubeReflection_DS);

		grcDepthStencilStateDesc m_skyReflectionUseStencilMaskDepthStencilState;
		m_skyReflectionUseStencilMaskDepthStencilState.DepthEnable = TRUE;
		m_skyReflectionUseStencilMaskDepthStencilState.DepthWriteMask = FALSE;
		m_skyReflectionUseStencilMaskDepthStencilState.DepthFunc = grcRSV::CMP_LESSEQUAL;
		m_skyReflectionUseStencilMaskDepthStencilState.StencilEnable = TRUE;
		m_skyReflectionUseStencilMaskDepthStencilState.StencilReadMask = DEFERRED_MATERIAL_CLEAR;
		m_skyReflectionUseStencilMaskDepthStencilState.StencilWriteMask = 0x00;
		m_skyReflectionUseStencilMaskDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		m_skyReflectionUseStencilMaskDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
		m_skyReflectionUseStencilMask_DS = grcStateBlock::CreateDepthStencilState(m_skyReflectionUseStencilMaskDepthStencilState);

		grcDepthStencilStateDesc m_skyStandardDepthStencilState;
		m_skyStandardDepthStencilState.DepthEnable = TRUE;
		m_skyStandardDepthStencilState.DepthWriteMask = FALSE;
		m_skyStandardDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		m_skyStandardDepthStencilState.StencilEnable = TRUE;
		m_skyStandardDepthStencilState.StencilReadMask = 0x07;
		m_skyStandardDepthStencilState.StencilWriteMask = 0x00;
		m_skyStandardDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		m_skyStandardDepthStencilState.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
		m_skyStandard_DS = grcStateBlock::CreateDepthStencilState(m_skyStandardDepthStencilState);

		grcDepthStencilStateDesc m_skyPlaneDepthStencilState;
		m_skyPlaneDepthStencilState.DepthEnable = TRUE;
		m_skyPlaneDepthStencilState.DepthWriteMask = FALSE;
		m_skyPlaneDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		m_skyPlane_DS = grcStateBlock::CreateDepthStencilState(m_skyPlaneDepthStencilState);


		// Multiple techniques
		m_techniques[CVisualEffects::RM_DEFAULT]				[RT_SKY] = m_skyShader->LookupTechnique("sky", true);
		m_techniques[CVisualEffects::RM_DEFAULT]				[RT_MOON] = m_skyShader->LookupTechnique("moon", true);
		m_techniques[CVisualEffects::RM_DEFAULT]				[RT_SUN] = m_skyShader->LookupTechnique("sun", true);

		m_techniques[CVisualEffects::RM_WATER_REFLECTION]		[RT_SKY] = m_skyShader->LookupTechnique("sky_water_reflection", true);
		m_techniques[CVisualEffects::RM_WATER_REFLECTION]		[RT_MOON] = m_skyShader->LookupTechnique("moon_water_reflection", true);
		m_techniques[CVisualEffects::RM_WATER_REFLECTION]		[RT_SUN] = m_skyShader->LookupTechnique("sun_water_reflection", true);

#if GS_INSTANCED_CUBEMAP
		m_techniques[CVisualEffects::RM_CUBE_REFLECTION]		[RT_SKY] = m_skyShader->LookupTechnique("sky_cubemap", true);
		m_techniques[CVisualEffects::RM_CUBE_REFLECTION]		[RT_MOON] = m_skyShader->LookupTechnique("moon_cubemap", true);
		m_techniques[CVisualEffects::RM_CUBE_REFLECTION]		[RT_SUN] = m_skyShader->LookupTechnique("sun_cubemap", true);
#else
		m_techniques[CVisualEffects::RM_CUBE_REFLECTION]		[RT_SKY] = m_techniques[CVisualEffects::RM_WATER_REFLECTION][RT_SKY];
		m_techniques[CVisualEffects::RM_CUBE_REFLECTION]		[RT_MOON] = m_techniques[CVisualEffects::RM_WATER_REFLECTION][RT_MOON];
		m_techniques[CVisualEffects::RM_CUBE_REFLECTION]		[RT_SUN] = m_techniques[CVisualEffects::RM_WATER_REFLECTION][RT_SUN];
#endif
		m_techniques[CVisualEffects::RM_MIRROR_REFLECTION]		[RT_SKY] = m_skyShader->LookupTechnique("sky_mirror_reflection", true);
		m_techniques[CVisualEffects::RM_MIRROR_REFLECTION]		[RT_MOON] = m_skyShader->LookupTechnique("moon_mirror_reflection", true);
		m_techniques[CVisualEffects::RM_MIRROR_REFLECTION]		[RT_SUN] = m_skyShader->LookupTechnique("sun_mirror_reflection", true);

		m_techniques[CVisualEffects::RM_SEETHROUGH]				[RT_SKY] = grcetNONE;
		m_techniques[CVisualEffects::RM_SEETHROUGH]				[RT_MOON] = grcetNONE;
		m_techniques[CVisualEffects::RM_SEETHROUGH]				[RT_SUN] = m_skyShader->LookupTechnique("sun_seethrough", true);
	}
}

// ------------------------------------------------------------------------------------------------------------------//
//												LoadSkyDomeModel													 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::LoadSkyDomeModel()
{
	strLocalIndex modelSlot = strLocalIndex(g_DwdStore.FindSlot(SKY_DOME_FILENAME));
	bool modelLoaded = CStreaming::LoadObject(modelSlot, g_DwdStore.GetStreamingModuleId());
	if (modelLoaded)
	{
		g_DwdStore.AddRef(modelSlot, REF_OTHER);
		m_skyDomeDictionary = g_DwdStore.Get(modelSlot);
		Assert(m_skyDomeDictionary);

		m_pDrawable = static_cast<gtaDrawable*>(m_skyDomeDictionary->Lookup(ATSTRINGHASH("skydome", 0x09d63c9b9)));
		Assertf(m_pDrawable, "Skydome could not be loaded\n");
	}
}

// ------------------------------------------------------------------------------------------------------------------//
//												UpdateVisualSettings												 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::UpdateVisualSettings()
{
	m_skySettings.m_cloudEdgeDetailScale = g_visualSettings.Get("cloudgen.edge.detail.scale", 16.0f);
	m_skySettings.m_cloudOverallDetailScale = g_visualSettings.Get("cloudgen.overlay.detail.scale", 8.0f);

	m_skySettings.m_noiseFrequency.setValue(g_visualSettings.Get("cloudgen.frequency", 2.0f));
	m_skySettings.m_noiseScale.setValue(g_visualSettings.Get("cloudgen.scale", 48.0f));

	m_skySettings.m_cloudSpeed = g_visualSettings.Get("cloud.speed.large", 5.0f);
	m_skySettings.m_smallCloudSpeed = g_visualSettings.Get("cloud.speed.small", 1.0f);
	m_skySettings.m_overallDetailSpeed = g_visualSettings.Get("cloud.speed.overall", 1.0f);
	m_skySettings.m_edgeDetailSpeed = g_visualSettings.Get("cloud.speed.edge", 1.0f);
	m_skySettings.m_cloudHatSpeed = g_visualSettings.Get("cloud.speed.hats", 1.0f);

	m_skySettings.m_skyPlaneOffset = g_visualSettings.Get("skyplane.offset", 512.0f);
	m_skySettings.m_skyPlaneFogFadeStart = g_visualSettings.Get("skyplane.fog.fade.start", 64.0f);
	m_skySettings.m_skyPlaneFogFadeEnd = g_visualSettings.Get("skyplane.fog.fade.end", 128.0f);

}


// ------------------------------------------------------------------------------------------------------------------//
//													Shutdown														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		if(m_skyShader)
		{
			delete m_skyShader;
			m_skyShader = NULL;
		}

		// Delete drawable
		if (m_pDrawable != NULL) 
		{
			delete m_pDrawable;
		}
		m_pDrawable = NULL;

		// Shutdown the texture dictionary
		vfxUtil::ShutdownTxd("skydome");

		// Delete ref and dictionary
		g_DwdStore.RemoveRef(strLocalIndex(g_DwdStore.FindSlot(SKY_DOME_FILENAME)), REF_OTHER);
		m_skyDomeDictionary = NULL;
	}
}

// ------------------------------------------------------------------------------------------------------------------//
//													Shutdown														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::ReloadSkyDomeModel()
{
	// Delete current references
	delete m_pDrawable;
	m_pDrawable = NULL;

	g_DwdStore.RemoveRef(strLocalIndex(g_DwdStore.FindSlot(SKY_DOME_FILENAME)), REF_OTHER);
	m_skyDomeDictionary = NULL;

	// Load skydome model again
	LoadSkyDomeModel();
}

static void SetInfiniteProjection(grcViewport* view, float epsilon = 1e-6f)
{
	// Infinite projection matrix
	Mat44V proj = view->GetProjection();

	// Take projection matrix far-plane to infinity and 
	// adjust appropriately
#if USE_INVERTED_PROJECTION_ONLY
	if( view->GetInvertZInProjectionMatrix() )
	{
		proj.GetCol2Ref().SetZf(-(epsilon - 1.0f)-1.0f);
		proj.GetCol2Ref().SetW(ScalarV(V_NEGONE));
		proj.GetCol3Ref().SetZ(ScalarV(V_ZERO));
	}
	else
#endif // USE_INVERTED_PROJECTION_ONLY
	{
		proj.GetCol2Ref().SetZf(epsilon - 1.0f);
		proj.GetCol2Ref().SetW(ScalarV(V_NEGONE));
		proj.GetCol3Ref().SetZ(ScalarV(V_ZERO));
	}

	// This ensures that before the perspective divide 
	// we that [x, y, z, z] -> [x/z, y/z, 1.0, 1.0]
	view->SetProjection(proj);
}

// ------------------------------------------------------------------------------------------------------------------//
//														Render														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::Render(CVisualEffects::eRenderMode mode, bool bUseStencilMask, bool bUseRestrictedProjection, int scissorX, int scissorY, int scissorW, int scissorH, bool bFirstCallThisDrawList)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	//If false, do not render sky
	if(!m_RenderSky)
	{
		return;
	}
	if (CRenderPhaseDebugOverlayInterface::DrawEnabledRef())
	{
		// set directional light again here, since debug overlay shares some of these shader constants
		Lights::m_lightingGroup.SetDirectionalLightingGlobals(true);
	}
#endif // __BANK

	if (bFirstCallThisDrawList)
	{
		UpdateSettings();
	}

#if USE_INVERTED_PROJECTION_ONLY
	bool invertProj = mode == CVisualEffects::RM_DEFAULT;
#endif // USE_INVERTED_PROJECTION_ONLY

#if GS_INSTANCED_CUBEMAP
	bool bInst = grcViewport::GetInstancing();

	grcViewport *view_grp[6];
	grcViewport view_prev[6];
	int uNumInstVP = bInst ? grcViewport::GetNumInstVP() : 1;
	for (int i = 0; i < uNumInstVP; i++)
	{
		view_grp[i] = bInst ? grcViewport::GetCurrentInstVP(i) : grcViewport::GetCurrent();
		view_prev[i] = *(view_grp[i]);
#if USE_INVERTED_PROJECTION_ONLY
		view_grp[i]->SetInvertZInProjectionMatrix(invertProj);
#endif // USE_INVERTED_PROJECTION_ONLY
	}
	grcViewport* view = view_grp[0];
#else
	// store viewport settings and matrices as they are changed
	grcViewport* view = grcViewport::GetCurrent();
	grcViewport view_prev = *view; // save the viewport so we can restore it when we're done
#if USE_INVERTED_PROJECTION_ONLY
	view->SetInvertZInProjectionMatrix(invertProj);
#endif // USE_INVERTED_PROJECTION_ONLY
	// end viewport / matrix store
#endif

	bool bSetClip = true;
	
	if (mode == CVisualEffects::RM_MIRROR_REFLECTION)
	{
		bSetClip = !__PS3; // i'm not sure why mirror reflection can't do this while water reflection is fine .. but this fixes BS#865615
#if __BANK
		if      (m_bMirrorForceSetClipON)  { bSetClip = true; }
		else if (m_bMirrorForceSetClipOFF) { bSetClip = false; }
#endif // __BANK
	}

	if (bSetClip)
	{
#if GS_INSTANCED_CUBEMAP
		grcViewport::SetCurrentNearFarClip(1.0f,25000.0f);
#else
		// Move far-clip plane
		view->SetNearFarClip(1.0f, 25000.0f);
#endif
	}

	// Reset world matrix
#if GS_INSTANCED_CUBEMAP
	grcViewport::SetCurrentWorldIdentity();
#else
	view->SetWorldIdentity();
#endif

#if __PS3
	bool disableEdgeCulling = false;
	bool disableEdgeCullingPrevEnabled = false;
#endif // __PS3

	if (mode == CVisualEffects::RM_DEFAULT || mode == CVisualEffects::RM_SEETHROUGH)
	{
		SetInfiniteProjection(view);
	}
	else if (mode == CVisualEffects::RM_MIRROR_REFLECTION)
	{
		bool bUseInfiniteProjection = bUseRestrictedProjection && __PS3; // TODO -- ps3 needs this, but it causes problems on 360 .. can we fix this?

#if __BANK
		if      (m_bMirrorForceInfiniteProjectionON)  { bUseInfiniteProjection = true; }
		else if (m_bMirrorForceInfiniteProjectionOFF) { bUseInfiniteProjection = false; }
#endif // __BANK

		if (bUseInfiniteProjection)
		{
			SetInfiniteProjection(view BANK_PARAM(powf(10.0f, m_mirrorInfiniteProjectionEpsilonExponent)));
		}
	}

	BANK_ONLY(if(m_bEnableForcedDepth))
	{
		float depth = 1.0f;
#if USE_INVERTED_PROJECTION_ONLY
		if( invertProj )
			depth = 0.0f;
#endif // USE_INVERTED_PROJECTION_ONLY
#if GS_INSTANCED_CUBEMAP
		grcViewport::SetCurrentWindowMinMaxZ(depth,depth);
#else
		view->SetWindowMinMaxZ(depth, depth);
#endif
	}

	// Need to set scissor here after SetWindowMinMaxZ, since that function will reset the scissor on ps3
	if (scissorX > -1)
	{
		GRCDEVICE.SetScissor(scissorX, scissorY, scissorW, scissorH);
	}

	// Get the offset based on the camera position but reset the up (z value)
	// to always be at the horizon height. Also scale
	Vec3V domeOffset = view->GetCameraPosition();
	domeOffset.SetZf(g_vfxSettings.GetWaterLevel());
	const Vec3V scaler = Vec3V(Vec::V4VConstantSplat<DOME_SCALE>()); //u32 version of 20000.0f

	// Scale the world matrix so that the sky dome is scaled appropriately 
	Mat34V worldMtx;
	Mat34VFromScale(worldMtx, scaler, domeOffset);

	s32 pass = -1;

#if __BANK
	if (m_overridePassSelection)
	{
		pass = PF_CLOUDS | PF_SUN | PF_MOON | PF_STARS;
	}
	else
#endif
	{
		// Lets work out whats on and what isn't
		u32 passKey = PF_CLOUDS | PF_SUN | PF_MOON | PF_STARS;
		if (!(ShouldRenderLargeClouds() || ShouldRenderSmallClouds())) { passKey &= ~PF_CLOUDS; }
		if (m_skySettings.m_sunHdrIntensity < 0.0001f) { passKey &= ~PF_SUN; }
		if (m_skySettings.m_moonIntensity.m_value < 0.0001f) { passKey &= ~PF_MOON; }
		if (m_skySettings.m_starfieldIntensity.m_value < 0.0001f) { passKey &= ~PF_STARS; }
		if (IsLessThan(m_skySettings.m_sunDirection.GetZ(), -ScalarV(V_FLT_SMALL_1)).Getb()) { passKey &= ~PF_SUN; }
		if (IsLessThan(m_skySettings.m_moonDirection.GetZ(), -ScalarV(V_FLT_SMALL_1)).Getb()) { passKey &= ~PF_MOON; }
		pass = passKey;
//		Assertf(passKey != -1, "Unknown pass selected for the sky");
	}

	// We only want the sun disk in RM_SEETHROUGH
	if (mode == CVisualEffects::RM_SEETHROUGH)
	{
		pass &= ~(PF_CLOUDS | PF_MOON | PF_STARS);
	}

	const grcRasterizerStateHandle   RS_prev = grcStateBlock::RS_Active;
	const grcDepthStencilStateHandle DS_prev = grcStateBlock::DSS_Active;
	const grcBlendStateHandle        BS_prev = grcStateBlock::BS_Active;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	// Select which technique to use and set state
	switch(mode)
	{
		default:
		case CVisualEffects::RM_DEFAULT:
		{
			grcStateBlock::SetDepthStencilState(m_skyStandard_DS, DEFERRED_MATERIAL_CLEAR);
			break;
		}

		case CVisualEffects::RM_CUBE_REFLECTION:
		{
			grcStateBlock::SetDepthStencilState(m_skyCubeReflection_DS, 0xFF);
			break;
		}

		case CVisualEffects::RM_MIRROR_REFLECTION:
		case CVisualEffects::RM_WATER_REFLECTION:
		{
			grcStateBlock::SetDepthStencilState(bUseStencilMask ? m_skyReflectionUseStencilMask_DS : grcStateBlock::DSS_TestOnly_LessEqual, bUseStencilMask ? DEFERRED_MATERIAL_CLEAR : DEFERRED_MATERIAL_ALL);
			break;
		}
		
		case CVisualEffects::RM_SEETHROUGH:
		{
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_TestOnly_LessEqual);
			break;
		}
	}


	// Set initial state
	grcBindTexture(NULL); 
	grcLightState::SetEnabled(false);


	// Set shader vars
	m_skySettings.SendToShader(mode);
	m_skySettings.SendSunMoonDirectionAndPosition(domeOffset);

#if __BANK
	bool bEnableCloudDebugging = false;
	bool bEnableCloudWireframe = false;

	if (mode == CVisualEffects::RM_DEFAULT)
	{
		bEnableCloudDebugging = m_enableCloudDebugging && mode == CVisualEffects::RM_DEFAULT;
		bEnableCloudWireframe = bEnableCloudDebugging && m_cloudDebuggingMode == CLOUD_DEBUGGING_MODE_WIREFRAME;

		if (bEnableCloudDebugging && !bEnableCloudWireframe)
		{
			Vec4V cloudDebuggingParams[] =
			{
				Vec4V(V_ZERO),
				Vec4V(V_ZERO),
				Vec4V(m_cloudDebuggingTexcoordScale, 0.0f, 0.0f, 0.0f),
			};

			// note: perlin gets rendered every frame and comes from textures/skydome.itd.zip/baseperlinnoise3channel.dds
			// and detail comes from textures/skydome.itd.zip/noise16_p.dds
			switch (m_cloudDebuggingMode)
			{
			case CLOUD_DEBUGGING_MODE_DENSITY                 : cloudDebuggingParams[2].SetY(ScalarV(V_TWO)); break;

			case CLOUD_DEBUGGING_MODE_PERLIN_X                : cloudDebuggingParams[0].SetX(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_PERLIN_Z                : cloudDebuggingParams[0].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_PERLIN_OFFSET_X         : cloudDebuggingParams[0].SetZ(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_PERLIN_OFFSET_Y         : cloudDebuggingParams[0].SetW(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DETAIL_EDGE_X           : cloudDebuggingParams[1].SetX(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DETAIL_EDGE_Y           : cloudDebuggingParams[1].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DETAIL_OVERALL          : cloudDebuggingParams[1].SetZ(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DITHER                  : cloudDebuggingParams[1].SetW(ScalarV(V_ONE)); break;

			case CLOUD_DEBUGGING_MODE_PERLIN_TEXCOORD         : cloudDebuggingParams[0].SetX(ScalarV(V_ONE)); cloudDebuggingParams[2].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_PERLIN_OFFSET_TEXCOORD  : cloudDebuggingParams[0].SetY(ScalarV(V_ONE)); cloudDebuggingParams[2].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DETAIL_EDGE_X_TEXCOORD  : cloudDebuggingParams[1].SetX(ScalarV(V_ONE)); cloudDebuggingParams[2].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DETAIL_EDGE_Y_TEXCOORD  : cloudDebuggingParams[1].SetY(ScalarV(V_ONE)); cloudDebuggingParams[2].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DETAIL_OVERALL_TEXCOORD : cloudDebuggingParams[1].SetZ(ScalarV(V_ONE)); cloudDebuggingParams[2].SetY(ScalarV(V_ONE)); break;
			case CLOUD_DEBUGGING_MODE_DITHER_TEXCOORD         : cloudDebuggingParams[1].SetW(ScalarV(V_ONE)); cloudDebuggingParams[2].SetY(ScalarV(V_ONE)); break;
			}

			if (m_cloudDebuggingParamsId == grcevNONE)
			{
				m_cloudDebuggingParamsId = m_skyShader->LookupVar("debugCloudsParams");
			}

			m_skyShader->SetVar(m_cloudDebuggingParamsId, cloudDebuggingParams, NELEM(cloudDebuggingParams));
			pass = PF_DEBUG_CLOUDS;
		}
	}
#endif // __BANK

#if GS_INSTANCED_CUBEMAP
	grcViewport::SetInstMatrices();
#endif

	// Draw actual skydome, but not in RM_SEETHROUGH
	if (mode != CVisualEffects::RM_SEETHROUGH && m_pDrawable != NULL)
	{
		grcViewport::SetCurrentWorldMtx(worldMtx);

		#if __PS3
		if (disableEdgeCulling) { disableEdgeCullingPrevEnabled = grcEffect::SetEdgeViewportCullEnable(false); }
		#endif

		if (m_skyShader->BeginDraw(grmShader::RMC_DRAW, true, m_techniques[mode][RT_SKY]))
		{
			m_skyShader->Bind(pass);
			m_pDrawable->DrawNoShaders(RCC_MATRIX34(worldMtx), CRenderer::GenerateBucketMask(CRenderer::RB_OPAQUE), 0);
			m_skyShader->UnBind();
			m_skyShader->EndDraw();
		}

#if __BANK
		if (bEnableCloudWireframe)
		{
			Vec4f cloudDebuggingParams[] =
			{
				Vec4f(0.0f, 0.0f, 0.0f, 0.0f),
				Vec4f(0.0f, 0.0f, 0.0f, 0.0f),
				Vec4f(0.0f, 4.0f, 0.0f, 0.0f), // params[2].y == 4, this should cause the wireframe to be magenta .. doesn't seem to work though
			};

			m_skyShader->SetVar(m_cloudDebuggingParamsId, cloudDebuggingParams, NELEM(cloudDebuggingParams));

			//const grcDepthStencilStateHandle DS_prev = grcStateBlock::DSS_Active;
			//const u8 stencilRef_prev = grcStateBlock::ActiveStencilRef;
			//grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetWireframeOverride(true);

			if (m_skyShader->BeginDraw(grmShader::RMC_DRAW, true, m_techniques[mode][RT_SKY]))
			{
				m_skyShader->Bind(PF_DEBUG_CLOUDS);
				m_pDrawable->DrawNoShaders(RCC_MATRIX34(worldMtx), CRenderer::GenerateBucketMask(CRenderer::RB_OPAQUE), 0);
				m_skyShader->UnBind();
				m_skyShader->EndDraw();
			}

			grcStateBlock::SetWireframeOverride(false);
			//grcStateBlock::SetDepthStencilState(DS_prev, stencilRef_prev);
		}
#endif // __BANK

		#if __PS3
		if (disableEdgeCulling) { grcEffect::SetEdgeViewportCullEnable(disableEdgeCullingPrevEnabled); }
		#endif

		if (IsGreaterThan(m_skySettings.m_skyPlaneColor.m_value.GetW(),ScalarV(V_ZERO)).Getb() && !Water::IsCameraUnderwater() && mode != CVisualEffects::RM_WATER_REFLECTION)
		{
			if (mode != CVisualEffects::RM_MIRROR_REFLECTION || BANK_SWITCH(m_bMirrorEnableSkyPlane, false))
			{
				grcViewport::SetCurrentWorldIdentity();

				#if __PS3
					if (mode == CVisualEffects::RM_DEFAULT)
					{
						grcStateBlock::SetDepthStencilState(m_skyPlane_DS, DEFERRED_MATERIAL_CLEAR);
					}
				#endif

				if (m_skyShader->BeginDraw(grmShader::RMC_DRAW, true, m_skyPlaneTechniqueIdx))
				{
					u32 pass = 0;
					pass = (mode == CVisualEffects::RM_CUBE_REFLECTION) ? 1 : pass;

					m_skyShader->Bind(pass);
					{

						grcBegin(drawTriFan, NUM_VERTS_FOR_SKY_RENDER);
						{
							grcTexCoord2f(0.5f, 0.5f);
							grcVertex3f(0.0, 0.0, -m_skySettings.m_skyPlaneOffset);

							Vector2 vSinCos;
							const int numVertsOnCircumference = NUM_VERTS_FOR_SKY_RENDER - 2;
							const int numVertsIterations = NUM_VERTS_FOR_SKY_RENDER - 1;
							//Doing the mod to get the first vertex again on the last iteration
							for(int i = 0; i < numVertsIterations; i++)
							{
								const Vec2V& vSinCos = CVfxHelper::GetSinCosValue(numVertsOnCircumference, i % numVertsOnCircumference );	
								grcVertex3f(vSinCos.GetYf() * DOME_SCALE_FOR_VERTS, vSinCos.GetXf() * DOME_SCALE_FOR_VERTS, -m_skySettings.m_skyPlaneOffset);
							}
						}

						grcEnd();
						m_skyShader->UnBind();
					}
					m_skyShader->EndDraw();
				}

				#if __PS3
					if (mode == CVisualEffects::RM_DEFAULT)
					{
						grcStateBlock::SetDepthStencilState(m_skyStandard_DS, DEFERRED_MATERIAL_CLEAR);
					}
				#endif
			}
		}
	}

	grcStateBlock::SetBlendState(m_skyMoonSun_B);

	// Draw the moon
	if ((pass & PF_MOON) != 0)
	{
		grcViewport::SetCurrentWorldIdentity();

		if (m_skyShader->BeginDraw(grmShader::RMC_DRAW, true, m_techniques[mode][RT_MOON]))
		{
			m_skyShader->Bind(0);
			CVfxHelper::DrawTriangleFan(NUM_VERTS_FOR_SUN_MOON_RENDER, m_skySettings.m_moonDiscSize * 250.0f, 0.50f);
			m_skyShader->UnBind();
			m_skyShader->EndDraw();
		}
	}

	// Draw sun
	if ((pass & PF_SUN) != 0 && mode == CVisualEffects::RM_DEFAULT)
	{
		grcViewport::SetCurrentWorldIdentity();

		if (m_skyShader->BeginDraw(grmShader::RMC_DRAW, true, m_techniques[mode][RT_SUN]))
		{
			m_skyShader->Bind(0);
			CVfxHelper::DrawTriangleFan(NUM_VERTS_FOR_SUN_MOON_RENDER, m_skySettings.m_sunDiscSize * m_fSunDiscSizeMul, 1.0f);
			m_skyShader->UnBind();
			m_skyShader->EndDraw();
		}
	}

	// Revert settings
#if GS_INSTANCED_CUBEMAP
	for (int i = 0; i < uNumInstVP; i++)
	{
		*(view_grp[i]) = view_prev[i];
		if (bInst)
			grcViewport::SetInstVP(view_grp[i],i);
		else
			grcViewport::SetCurrent(view_grp[i]);
	}
#else
	*view = view_prev;
	grcViewport::SetCurrent(view); // i think this is required, right?
#endif

	if (mode == CVisualEffects::RM_MIRROR_REFLECTION)
	{
		grcStateBlock::SetStates(RS_prev, DS_prev, BS_prev); // restore states
	}
	else // other phases might "rely" on the behaviour of setting the states to default, so i'll keep this around
	{
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	}

	// end settings revert
}

// ------------------------------------------------------------------------------------------------------------------//
//												GeneratePerlin														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::GeneratePerlin()
{
#if __BANK
	// don't render perlin if we're viewing a gbuffer (this would overwrite GBUFFER2 on 360, we don't want to do that)
	if (DebugDeferred::m_GBufferIndex > 0)
	{
		return;
	}
#endif // __BANK

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_skySettings.m_perlinRT.m_value, NULL);
		
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	m_skySettings.SetPerlinParameters();

	if (m_skyShader->BeginDraw(grmShader::RMC_DRAW, true, m_perlinNoiseTechniqueIdx))
	{
		m_skyShader->Bind(0);
		grcBegin(drawTris, 6);
		grcVertex2f(0,0);
		grcVertex2f(1,1);
		grcVertex2f(0,1);
		grcVertex2f(0,0);
		grcVertex2f(1,0);
		grcVertex2f(1,1);
		grcEnd();
		m_skyShader->UnBind();
		m_skyShader->EndDraw();
	}

	grcResolveFlags resolveFlags;
	resolveFlags.ClearColor = true;

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags); 
	
}

grcTexture* CSky::GetPerlinTexture()
{
	return m_skySettings.m_perlinRT.m_value;
}

grcTexture* CSky::GetHighDetailNoiseTexture() // cloud shadows 2
{
	return m_skySettings.m_highDetailNoiseTex.m_value;
}

float CSky::GetCloudDensityMultiplier()
{
	return m_skySettings.m_cloudDensityMultiplier;
}

float CSky::GetCloudDensityBias()
{
	return m_skySettings.m_cloudDensityBias;
}


bool CSky::ShouldRenderLargeClouds() 
{ 
	return 
		(m_skySettings.m_cloudDensityMultiplier > m_skySettings.m_cloudDensityBias); 
}
bool CSky::ShouldRenderSmallClouds() 
{ 
	return 
		(m_skySettings.m_smallCloudDensityMultiplier > m_skySettings.m_smallCloudDensityBias) && 
		(m_skySettings.m_smallCloudDetailStrength > 0.0f); 

}

const Vec4V* CSky::GetCloudShaderConstants(Vec4V v[6], int ASSERT_ONLY(n), float largeCloudAmount, float smallCloudAmount) // cloud shadows 2
{
	Assert(n == 6);

	v[0] = m_skySettings.m_cloudDetailConstants.m_value;
	v[1] = m_skySettings.m_cloudConstants1.m_value;
	v[2] = m_skySettings.m_cloudConstants2.m_value;
	v[3] = m_skySettings.m_smallCloudConstants.m_value;
	v[4] = Vec4V(m_skySettings.m_speedConstants.m_value);
	v[5] = Vec4V(largeCloudAmount, smallCloudAmount, 0.0f, 0.0f);

	return v;
}

#if __BANK
// ------------------------------------------------------------------------------------------------------------------//
//													InitWidgets														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::InitWidgets()
{
	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->PushGroup("Sky", false);
	{
		pVfxBank->AddToggle("Overwrite Time Cycle", &m_overwriteTimeCycle);
		pVfxBank->AddToggle("Forced Depth To Back Plane", &m_bEnableForcedDepth);

		pVfxBank->AddSeparator();
		pVfxBank->AddToggle("Mirror Force Infinite Projection ON", &m_bMirrorForceInfiniteProjectionON);
		pVfxBank->AddToggle("Mirror Force Infinite Projection OFF", &m_bMirrorForceInfiniteProjectionOFF);
		pVfxBank->AddSlider("Mirror Inf Proj Epsilon Exponent", &m_mirrorInfiniteProjectionEpsilonExponent, -12.0f, 0.0f, 1.0f/64.0f);
		pVfxBank->AddToggle("Mirror Force Set Clip ON", &m_bMirrorForceSetClipON);
		pVfxBank->AddToggle("Mirror Force Set Clip OFF", &m_bMirrorForceSetClipOFF);
		pVfxBank->AddToggle("Mirror Enable Sky Plane", &m_bMirrorEnableSkyPlane);

		pVfxBank->AddSeparator();
		pVfxBank->AddToggle("Override pass selection", &m_overridePassSelection);

		pVfxBank->AddSeparator();
		pVfxBank->AddButton("Reload SkyDome Model", datCallback(CFA1(CSky::ReloadSkyDome), this));

		pVfxBank->AddSeparator();
		pVfxBank->AddToggle("Render Sky", &m_RenderSky);
		m_skySettings.InitWidgets();
	}
	pVfxBank->PopGroup();

	pVfxBank->PushGroup("Cloud Debugging", false);
	{
		const char* cloudDebuggingModeStrings[] =
		{
			"DENSITY",
			"WIREFRAME",
			"PERLIN_X",
			"PERLIN_Z",
			"PERLIN_TEXCOORD",
			"PERLIN_OFFSET_X",
			"PERLIN_OFFSET_Y",
			"PERLIN_OFFSET_TEXCOORD",
			"DETAIL_EDGE_X",
			"DETAIL_EDGE_X_TEXCOORD",
			"DETAIL_EDGE_Y",
			"DETAIL_EDGE_Y_TEXCOORD",
			"DETAIL_OVERALL",
			"DETAIL_OVERALL_TEXCOORD",
			"DITHER",
			"DITHER_TEXCOORD",
		};
		CompileTimeAssert(NELEM(cloudDebuggingModeStrings) == CLOUD_DEBUGGING_MODE_COUNT);

		pVfxBank->AddToggle("Enable Cloud Debugging", &m_enableCloudDebugging);
		pVfxBank->AddCombo("Cloud Debugging Mode", &m_cloudDebuggingMode, NELEM(cloudDebuggingModeStrings), cloudDebuggingModeStrings);
		pVfxBank->AddSlider("Cloud Texcoord Scale", &m_cloudDebuggingTexcoordScale, 0.0f, 4.0f, 1.0f/32.0f);
	}
	pVfxBank->PopGroup();
}

// ------------------------------------------------------------------------------------------------------------------//
//												ReloadSkydome														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSky::ReloadSkyDome(CSky *instance)
{
	instance->ReloadSkyDomeModel();
}

#endif
