// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "bank/bank.h"
#include "math/float16.h"
#include "vectormath/vectormath.h"
#include "vectormath/vec4v.h"
#include "system/param.h"
#include "profile/profiler.h"
#include "profile/timebars.h"
#include "system/nelem.h"
#include "grcore/allocscope.h"
#include "grcore/font.h"
#include "grprofile/timebars.h"

// framework
#include "fwrenderer/renderlistbuilder.h"
#include "fwtl/pool.h"
#include "fwutil/xmacro.h"
#include "fwutil/orientator.h"

// game
#include "Camera/CamInterface.h"
#include "camera/helpers/frame.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/GtaPicker.h"
#include "debug/Rendering/DebugCamera.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugRendering.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/Lights/Lights.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/PostProcessFX.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Util/Util.h"
#include "Scene/Building.h"
#include "Scene/AnimatedBuilding.h"
#include "scene/EntityBatch.h"
#include "Scene/portals/InteriorInst.h"
#include "system/controlMgr.h"
#include "Objects/DummyObject.h"
#include "Objects/Object.h"

RENDER_OPTIMISATIONS()

#if __BANK

PF_PAGE(TimebarInfoPage, "Timebar");
PF_GROUP(TimebarInfo);
PF_LINK(TimebarInfoPage, TimebarInfo);
PF_VALUE_FLOAT(Min, TimebarInfo);
PF_VALUE_FLOAT(Average, TimebarInfo);
PF_VALUE_FLOAT(Current, TimebarInfo);
PF_VALUE_FLOAT(Max, TimebarInfo);

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

enum deferred_passes
{
	pass_override_gbuffer_texture = 0,
	pass_show_gbuffer,
	pass_desat_albedo,
	pass_show_diffuse_range,
	pass_show_swatch,
	pass_show_chart,
	pass_show_focus_chart,
	pass_count
};

// ----------------------------------------------------------------------------------------------- //

static const char *deferred_pass_names[] = 
{
	"override_gbuffer_texture",
	"show_gbuffer",
	"desaturate_albedo",
	"show_diffuse_range",
	"show_swatch",
	"show_chart",
	"show_focus_chart"
};
CompileTimeAssert(NELEM(deferred_pass_names) == pass_count);

// ----------------------------------------------------------------------------------------------- //

/*static u32 Address2DTiledOffset(
	u32 x,
	u32 y,
	u32 Width
	)
{
	u32 AlignedWidth;
	u32 LogBpp;
	u32 Macro;
	u32 Micro;
	u32 Offset;

	AlignedWidth = (Width + 31) & ~31;
	LogBpp       = 2;
	Macro        = ((x >> 5) + (y >> 5) * (AlignedWidth >> 5)) << (LogBpp + 7);
	Micro        = (((x & 7) + ((y & 6) << 2)) << LogBpp);
	Offset       = Macro + ((Micro & ~15) << 1) + (Micro & 15) + ((y & 8) << (3 + LogBpp)) + ((y & 1) << 4);

	return (((Offset & ~511) << 3) + ((Offset & 448) << 2) + (Offset & 63) +
		((y & 16) << 7) + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)) >> LogBpp;
}*/

// ----------------------------------------------------------------------------------------------- //

PARAM(tiled_debug, "Enable tiled lighting debug");
PARAM(tiled_classification_only, "Enable only classification and not rendering");
PARAM(tiled_tile_index, "Set tile index");

// ----------------------------------------------------------------------------------------------- //

#if __XENON
bool DebugDeferred::m_switchZPass = false;
#endif // __XENON

bool DebugDeferred::m_timeBarToEKG = false;

s32	DebugDeferred::m_GBufferIndex = 0;
bool DebugDeferred::m_GBufferFilter[] = {true, true, true, false};
bool DebugDeferred::m_GBufferShowTwiddle = false;
bool DebugDeferred::m_GBufferShowDepth = false;

bool DebugDeferred::m_directEnable = true;
bool DebugDeferred::m_ambientEnable = true;
bool DebugDeferred::m_specularEnable = true;
bool DebugDeferred::m_emissiveEnable = true;
bool DebugDeferred::m_shadowEnable  = true;

#if DYNAMIC_GB
bool DebugDeferred::m_useLateESRAMGBufferEviction = true;
#endif // RSG_DURANGO && DYNAMIC_GB

#if DEVICE_EQAA
bool DebugDeferred::m_disableEqaaDuringCutoutPass = true;
#endif // DEVICE_EQAA

float DebugDeferred::m_overrideDiffuse_desat = 0.0f;
bool DebugDeferred::m_overrideDiffuse = false;
float DebugDeferred::m_overrideDiffuse_intensity = 1.0f;
float DebugDeferred::m_overrideDiffuse_amount = 1.0f;

bool DebugDeferred::m_overrideEmissive = false;
float DebugDeferred::m_overrideEmissive_intensity = 0.0f;
float DebugDeferred::m_overrideEmissive_amount = 1.0f;

bool DebugDeferred::m_overrideShadow = false;
float DebugDeferred::m_overrideShadow_intensity	= 1.0f;
float DebugDeferred::m_overrideShadow_amount = 1.0f;

bool DebugDeferred::m_overrideAmbient = false;
Vector2 DebugDeferred::m_overrideAmbientParams = Vector2(1.0f, 1.0f);

bool DebugDeferred::m_overrideSpecular = false;
Vector3 DebugDeferred::m_overrideSpecularParams	= Vector3(1.0f, 512.0f, 0.96f);

bool DebugDeferred::m_forceSetAll8Lights = false;

bool DebugDeferred::m_diffuseRangeShow = false;
float DebugDeferred::m_diffuseRangeMin = 0.25f;
float DebugDeferred::m_diffuseRangeMax = 0.75f;
Vector3	DebugDeferred::m_diffuseRangeLowerColour = Vector3(0.0, 0.0, 1.0);
Vector3	DebugDeferred::m_diffuseRangeMidColour = Vector3(0.0, 1.0, 0.0);
Vector3	DebugDeferred::m_diffuseRangeUpperColour = Vector3(1.0, 0.0, 0.0);

bool DebugDeferred::m_swatchesEnable = false;
bool DebugDeferred::m_swatches2DEnable = false;
bool DebugDeferred::m_swatches3DEnable = false;

float DebugDeferred::m_swatchesNumX = 6.0f;
float DebugDeferred::m_swatchesNumY = 4.0f;
float DebugDeferred::m_swatchesSpacing = 10.0f;
float DebugDeferred::m_swatchesCentreX = 640.0f;
float DebugDeferred::m_swatchesCentreY = 360.0f;
atArray<Color32> DebugDeferred::m_swatchesColours;
float DebugDeferred::m_swatchesSize = 64.0f;
float DebugDeferred::m_chartOffset = 10.0f;

bool DebugDeferred::m_focusChartEnable = false;
grcTexture* DebugDeferred::m_focusChartTexture = NULL;
u16	DebugDeferred::m_focusChartMipLevel = 0;
float DebugDeferred::m_focusChartOffsetX = 0;
float DebugDeferred::m_focusChartOffsetY = 0;

float DebugDeferred::m_lightVolumeExperimental = 0.0f;

bool DebugDeferred::m_decalDepthWrite = false;

#if __PPU
bool DebugDeferred::m_spupmEnableReloadingElf = false;
bool DebugDeferred::m_spupmDoReloadElf = false;
char DebugDeferred::m_spupmReloadElfName[255];
u32 DebugDeferred::m_spupmReloadElfSlot = RELOAD_NONE;
#endif // __PPU

bool DebugDeferred::m_enableTiledRendering  = false;
s32 DebugDeferred::m_classificationThread = 0;

bool DebugDeferred::m_enableSkinPass	= true; 
bool DebugDeferred::m_enableSkinPassNG	= true;
Vector4 DebugDeferred::m_skinPassParams = DEFERRED_LIGHTING_DEFAULT_skinPassParams;
Vector3 DebugDeferred::m_SubSurfaceColorTweak = DEFERRED_LIGHTING_DEFAULT_subsurfaceColorTweak;

bool DebugDeferred::m_enablePedLight = true;
bool DebugDeferred::m_enablePedLightOverride = false;
Vector3	DebugDeferred::m_pedLightDirection = Vector3(0.0f, 1.0f, 0.0f);
Vector3	DebugDeferred::m_pedLightColour = Vector3(1.0f, 1.0, 1.0f);
float DebugDeferred::m_pedLightIntensity = 1.0f;
float DebugDeferred::m_pedLightFadeUpTime = 4.0f;

bool DebugDeferred::m_enableDynamicAmbientBakeOverride = false;
Vector4 DebugDeferred::m_dynamicBakeParams = DEFERRED_LIGHTING_DEFAULT_dynamicBakeParams;

bool DebugDeferred::m_enableFoliageLightPass	= true;

// Cubemap instancing by default for RSG_PC
bool DebugDeferred::m_enableCubeMapReflection = (__PS3 || __XENON || RSG_PC || RSG_DURANGO || RSG_ORBIS);
bool DebugDeferred::m_copyCubeToParaboloid = DebugDeferred::m_enableCubeMapReflection;
bool DebugDeferred::m_enableCubeMapDebugMode = false;

bool DebugDeferred::m_forceTiledDirectional = false;

bool DebugDeferred::m_overrideReflectionMapFreeze = false;
bool DebugDeferred::m_freezeReflectionMap = false;

bool DebugDeferred::m_useTexturedLights = true;
bool DebugDeferred::m_useGeomPassStencil = DEFERRED_LIGHTING_DEFAULT_useGeomPassStencil;
bool DebugDeferred::m_useLightsHiStencil = DEFERRED_LIGHTING_DEFAULT_useLightsHiStencil;
bool DebugDeferred::m_useSinglePassStencil = DEFERRED_LIGHTING_DEFAULT_useSinglePassStencil;

bool DebugDeferred::m_enableReflectiobTweakOverride = false;
Vector4 DebugDeferred::m_reflectionTweaks = Vector4(2.0f, 2.0f, 12.0f, 0.0f);

float DebugDeferred::m_ReflectionBlurThreshold = DEFERRED_LIGHTING_DEFAULT_ReflectionBlurThreshold;

float DebugDeferred::m_reflectionLodRangeMultiplier = 1.0f;

// State blocks
grcDepthStencilStateHandle DebugDeferred::m_override_DS;

grcRasterizerStateHandle DebugDeferred::m_override_R;

grcBlendStateHandle DebugDeferred::m_overrideDiffuse_B;
grcBlendStateHandle DebugDeferred::m_overrideDiffuseDesat_B;
grcBlendStateHandle DebugDeferred::m_overrideEmissive_B;
grcBlendStateHandle DebugDeferred::m_overrideShadow_B;
grcBlendStateHandle DebugDeferred::m_overrideAmbient_B;
grcBlendStateHandle DebugDeferred::m_overrideSpecular_B;
grcBlendStateHandle DebugDeferred::m_overrideAll_B;
bool DebugDeferred::m_bForceHighQualityLighting;

#if MSAA_EDGE_PASS
bool DebugDeferred::m_EnableEdgePass = true;
# if MSAA_EDGE_PROCESS_FADING
bool DebugDeferred::m_EnableEdgeFadeProcessing = true;
# endif
bool DebugDeferred::m_IgnoreEdgeDetection = false;
bool DebugDeferred::m_OldEdgeDetection = false;
bool DebugDeferred::m_AlternativeEdgeDetection = false;
bool DebugDeferred::m_AggressiveEdgeDetection = false;
bool DebugDeferred::m_DebugEdgePassColor = false;
bool DebugDeferred::m_OptimizeShadowReveal = true;
bool DebugDeferred::m_EnableShadowRevealEdge = true;
bool DebugDeferred::m_EnableShadowRevealFace = true;
bool DebugDeferred::m_OptimizeShadowProcessing = true;
bool DebugDeferred::m_EnableShadowProcessingEdge = true;
bool DebugDeferred::m_EnableShadowProcessingFace = true;
bool DebugDeferred::m_OptimizeDirectional = true;
bool DebugDeferred::m_EnableDirectionalEdge0 = true;
bool DebugDeferred::m_EnableDirectionalEdge1 = true;
bool DebugDeferred::m_EnableDirectionalFace0 = true;
bool DebugDeferred::m_EnableDirectionalFace1 = true;
bool DebugDeferred::m_OptimizeDirectionalFoliage = false; // performance gain is inconsistent
bool DebugDeferred::m_EnableDirectionalFoliageEdge = true;
bool DebugDeferred::m_EnableDirectionalFoliageFace = true;
unsigned DebugDeferred::m_OptimizeSceneLights = 0xF;
unsigned DebugDeferred::m_OptimizeDepthEffects = 0xF;
unsigned DebugDeferred::m_OptimizeDofBlend = 0xF;
unsigned DebugDeferred::m_OptimizeComposite = 0xF;
Vector4 DebugDeferred::m_edgeMarkParams = DEFERRED_LIGHTING_DEFAULT_edgeMarkParams;
#endif

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void DebugDeferred::Init()
{
#if TILED_LIGHTING_ENABLED
	m_enableTiledRendering = false;
#endif

	m_enableCubeMapReflection = true;
	m_copyCubeToParaboloid = true;

	// State blocks
	grcDepthStencilStateDesc overrideDepthStencilState;
	overrideDepthStencilState.DepthEnable = FALSE;
	overrideDepthStencilState.DepthWriteMask = FALSE;
	overrideDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	m_override_DS = grcStateBlock::CreateDepthStencilState(overrideDepthStencilState);

	grcRasterizerStateDesc overrideRasterizerState;
	overrideRasterizerState.CullMode = grcRSV::CULL_NONE;
	m_override_R = grcStateBlock::CreateRasterizerState(overrideRasterizerState);

	grcBlendStateDesc overrideDiffuseBlendState;
	overrideDiffuseBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	overrideDiffuseBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	overrideDiffuseBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_BLENDFACTOR;
	overrideDiffuseBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVBLENDFACTOR;
	overrideDiffuseBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	m_overrideDiffuse_B = grcStateBlock::CreateBlendState(overrideDiffuseBlendState);

	grcBlendStateDesc overrideDiffuseDesatBlendState;
	overrideDiffuseDesatBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	overrideDiffuseDesatBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	overrideDiffuseDesatBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_BLENDFACTOR;
	overrideDiffuseDesatBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVBLENDFACTOR;
	overrideDiffuseDesatBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	m_overrideDiffuseDesat_B = grcStateBlock::CreateBlendState(overrideDiffuseDesatBlendState);

	grcBlendStateDesc overrideEmissiveBlendState;
	overrideEmissiveBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	overrideEmissiveBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	overrideEmissiveBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_BLENDFACTOR;
	overrideEmissiveBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVBLENDFACTOR;
	overrideEmissiveBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_BLUE;
	m_overrideEmissive_B = grcStateBlock::CreateBlendState(overrideEmissiveBlendState);

	grcBlendStateDesc overrideShadowBlendState;
	overrideShadowBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	overrideShadowBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	overrideShadowBlendState.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_BLENDFACTOR;
	overrideShadowBlendState.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVBLENDFACTOR;
	overrideShadowBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
	m_overrideShadow_B = grcStateBlock::CreateBlendState(overrideShadowBlendState);

	grcBlendStateDesc overrideAmbientBlendState;
	overrideAmbientBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	overrideAmbientBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	overrideAmbientBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_BLENDFACTOR;
	overrideAmbientBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVBLENDFACTOR;
	overrideAmbientBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RED + grcRSV::COLORWRITEENABLE_GREEN;
	m_overrideAmbient_B = grcStateBlock::CreateBlendState(overrideAmbientBlendState);

	grcBlendStateDesc overrideSpecularBlendState;
	overrideSpecularBlendState.BlendRTDesc[0].BlendEnable = TRUE;
	overrideSpecularBlendState.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	overrideSpecularBlendState.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_BLENDFACTOR;
	overrideSpecularBlendState.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVBLENDFACTOR;
	overrideSpecularBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	m_overrideSpecular_B = grcStateBlock::CreateBlendState(overrideSpecularBlendState);

	grcBlendStateDesc overrideAllBlendState;
	overrideAllBlendState.IndependentBlendEnable = TRUE;
	overrideAllBlendState.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	overrideAllBlendState.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	overrideAllBlendState.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	overrideAllBlendState.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
	m_overrideAll_B = grcStateBlock::CreateBlendState(overrideAllBlendState, "m_overrideAll_B");

	// Colours for swatches
	m_swatchesColours.ResizeGrow(u32(m_swatchesNumX * m_swatchesNumY));

	// Macbeth chart colours
	m_swatchesColours[0] = Color32(115,82,68);
	m_swatchesColours[1] = Color32(195,149,128);
	m_swatchesColours[2] = Color32(93,123,157);
	m_swatchesColours[3] = Color32(91,108,65);
	m_swatchesColours[4] = Color32(130,129,175);
	m_swatchesColours[5] = Color32(99,191,171);
	m_swatchesColours[6] = Color32(220,123,46);
	m_swatchesColours[7] = Color32(72,92,168);
	m_swatchesColours[8] = Color32(194,84,97);
	m_swatchesColours[9] = Color32(91,59,104);
	m_swatchesColours[10] = Color32(161,189,62);
	m_swatchesColours[11] = Color32(229,161,40);
	m_swatchesColours[12] = Color32(42,63,147);
	m_swatchesColours[13] = Color32(72,149,72);
	m_swatchesColours[14] = Color32(175,50,57);
	m_swatchesColours[15] = Color32(238,200,22);
	m_swatchesColours[16] = Color32(188,84,150);
	m_swatchesColours[17] = Color32(0,137,166);
	m_swatchesColours[18] = Color32(245,245,240);
	m_swatchesColours[19] = Color32(201,202,201);
	m_swatchesColours[20] = Color32(161,162,162);
	m_swatchesColours[21] = Color32(120,121,121);
	m_swatchesColours[22] = Color32(83,85,85);
	m_swatchesColours[23] = Color32(50,50,51);

	// Setup all the data for tiled lighting
	USE_DEBUG_MEMORY();
	
	m_bForceHighQualityLighting = false;

	s32 txdSlot =  CShaderLib::GetTxdId();
	m_focusChartTexture = g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("focus_chart", 0x5c17bf5a));
}

// ----------------------------------------------------------------------------------------------- //

static void TriggerPedLightFadeUp()
{
	Lights::StartPedLightFadeUp(DebugDeferred::m_pedLightFadeUpTime);
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::AddWidgets_GBuffer(bkBank *bk)
{
	bk->PushGroup("GBuffer");
		const char* GBufferNames[] = { "None", "Albedo", "Normals", "Specular/Shadow", "Ambient/Emissive", "Depth/Stencil", "Invert Shadow" };
		bk->AddCombo("GBuffer", &m_GBufferIndex, NELEM(GBufferNames), GBufferNames);
		bk->AddToggle("Red",    &m_GBufferFilter[0]);
		bk->AddToggle("Green",  &m_GBufferFilter[1]);
		bk->AddToggle("Blue",   &m_GBufferFilter[2]);
		bk->AddToggle("Alpha",  &m_GBufferFilter[3]);
		bk->AddToggle("Show twiddle", &m_GBufferShowTwiddle);
		bk->AddToggle("Show depth", &m_GBufferShowDepth);
	bk->PopGroup();

	bk->PushGroup("Diffuse texture");
		bk->AddSlider("Desaturate", &m_overrideDiffuse_desat, 0.0f, 1.0f, 0.01f);
		bk->AddToggle("Override diffuse textures", &m_overrideDiffuse);
		bk->AddSlider("Override intensity", &m_overrideDiffuse_intensity, 0.0f, 1.0f, 0.01f);
		bk->AddSlider("Override amount", &m_overrideDiffuse_amount, 0.0f, 1.0f, 0.01f);
	bk->PopGroup();

	bk->PushGroup("Ambient");
		bk->AddToggle("Enable", &m_ambientEnable);
		bk->AddToggle("Override Ambient", &m_overrideAmbient);
		bk->AddSlider("Natural", &m_overrideAmbientParams.x, 0.0f, 1.0f, 0.01f);
		bk->AddSlider("Artificial", &m_overrideAmbientParams.y, 0.0f, 1.0f, 0.01f);
	bk->PopGroup();

	bk->PushGroup("Specular");
		bk->AddToggle("Enable", &m_specularEnable);
		bk->AddToggle("Override Specular", &m_overrideSpecular);
		bk->AddSlider("Diffuse / Specular mix", &m_overrideSpecularParams.x, 0.0f, 1.0f, 0.01f);
		bk->AddSlider("Falloff", &m_overrideSpecularParams.y, 0.0f, 512.0f, 0.1f);
		bk->AddSlider("Fresnel", &m_overrideSpecularParams.z, 0.0f, 1.0f, 0.01f);
	bk->PopGroup();

	bk->PushGroup("Emissive");
		bk->AddToggle("Enable", &m_emissiveEnable);
		bk->AddToggle("Override Emissive", &m_overrideEmissive);
		bk->AddSlider("Intensity", &m_overrideEmissive_intensity, 0.0f, 1.0f, 0.01f);
		bk->AddSlider("Amount", &m_overrideEmissive_amount, 0.0f, 1.0f, 0.01f);
	bk->PopGroup();

	bk->PushGroup("Shadow");
		bk->AddToggle("Enable", &m_shadowEnable);
		bk->AddToggle("Override shadow", &m_overrideShadow);
		bk->AddSlider("Intensity", &m_overrideShadow_intensity, 0.0f, 1.0f, 0.01f);
		bk->AddSlider("Amount", &m_overrideShadow_amount, 0.0f, 1.0f, 0.01f);
	bk->PopGroup();
}

// ----------------------------------------------------------------------------------------------- //
#if __XENON
// ----------------------------------------------------------------------------------------------- //

static void SetGammaRamp()
{
	u16 adjustmentCurve[256] = {
		0,768,1472,2240,2944,3584,4160,4672,5120,5568,5952,6336,6720,7104,7424,7744,8064,8320,8640,8896,9152,9408,9664,9920,10176,
		10496,10752,11008,11264,11520,11776,12032,12288,12544,12800,13056,13312,13568,13824,14080,14336,14592,14848,15104,15360,15552,
		15808,16064,16320,16576,16832,17088,17344,17600,17856,18112,18368,18624,18816,19072,19328,19584,19840,20096,20352,20608,20864,
		21056,21312,21568,21824,22080,22336,22592,22784,23040,23296,23552,23808,24064,24320,24512,24768,25024,25280,25536,25792,25984,
		26240,26496,26752,27008,27200,27456,27712,27968,28224,28416,28672,28928,29184,29440,29632,29888,30144,30400,30592,30848,31104,
		31360,31552,31808,32064,32320,32576,32768,33024,33280,33536,33728,33984,34240,34432,34688,34944,35200,35392,35648,35904,36160,
		36352,36608,36864,37120,37312,37568,37824,38016,38272,38528,38784,38976,39232,39488,39680,39936,40192,40384,40640,40896,41152,
		41344,41600,41856,42048,42304,42560,42752,43008,43264,43456,43712,43968,44160,44416,44672,44864,45120,45376,45568,45824,46080,
		46272,46528,46784,46976,47232,47488,47680,47936,48192,48384,48640,48896,49088,49344,49536,49792,50048,50240,50496,50752,50944,
		51200,51456,51648,51904,52096,52352,52608,52800,53056,53312,53504,53760,53952,54208,54464,54656,54912,55168,55360,55616,55808,
		56064,56320,56512,56768,56960,57216,57472,57664,57920,58112,58368,58624,58816,59072,59264,59520,59776,59968,60224,60416,60672,
		60864,61120,61376,61568,61824,62016,62272,62528,62720,62976,63168,63424,63616,63872,64128,64320,64576,64768,65024,65216,65472 };

	D3DGAMMARAMP ramp;
	for (u32 i = 0; i < 256; i++)
	{
		ramp.red[i] = adjustmentCurve[i];
		ramp.green[i] = adjustmentCurve[i];
		ramp.blue[i] = adjustmentCurve[i];
	}
	GRCDEVICE.GetCurrent()->SetGammaRamp(0, 0, &ramp);	
}

// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::AddWidgets(bkBank *bk)
{
	bk->AddToggle("Send timebar to EKG", &m_timeBarToEKG);
#if __XENON
	bk->AddButton("Set gamma ramp", SetGammaRamp);
#endif

	#if TILED_LIGHTING_ENABLED
		bk->PushGroup("Tiled Lighting");
			bk->AddToggle("GPU Rendering", &m_enableTiledRendering);
			bk->AddToggle("GPU Classification", CTiledLighting::GetClassificationEnabledPtr());
			const char* threadNames[] = { "Update thread", "Render Thread" };
			bk->AddCombo("Thread for classification", &m_classificationThread, 2, threadNames);
		bk->PopGroup();
	#endif

	bk->PushGroup("Deferred");

		bk->AddToggle("Enable Foliage Light Pass", &m_enableFoliageLightPass);
		#if __XENON
		#if __DEV 
			bk->AddToggle("Z Prepass", &m_switchZPass);		
			bk->AddToggle("Decal Depth Write", &m_decalDepthWrite);
		#endif // __DEV
		#endif // __XENON
			bk->AddToggle("Use Geom Pass Stencil", &m_useGeomPassStencil);
			bk->AddToggle("Use Lights Hi Stencil", &m_useLightsHiStencil);
			bk->AddToggle("Use Single Pass Stencil", &m_useSinglePassStencil);
			bk->AddSlider("Reflection Blur Threshold", &m_ReflectionBlurThreshold, 0.0f, 1000.0f, 0.1f);
			bk->AddToggle("Enable Textured Lights", &m_useTexturedLights);
#if DYNAMIC_GB
			bk->AddToggle("Enable Late GBuffer ESRAM Eviction", &m_useLateESRAMGBufferEviction);
#endif // RSG_DURANGO && DYNAMIC_GB
#if DEVICE_EQAA
			bk->AddToggle("Disable EQAA for Cutout Pass", &m_disableEqaaDuringCutoutPass);
#endif // DEVICE_EQAA
		#if __PPU
			AddWidgets_SpuPmElfReload(bk);
		#endif // __PPU

		bk->PushGroup("Cubemap Reflections");
			bk->AddToggle("Enable cube-map reflection", &m_enableCubeMapReflection);
			bk->AddToggle("Force tiled directional", &m_forceTiledDirectional);
			bk->AddToggle("Copy cube to paraboloid", &m_copyCubeToParaboloid);
			bk->AddToggle("Debug mode", &m_enableCubeMapDebugMode);
			bk->AddToggle("Override reflection map freeze",&m_overrideReflectionMapFreeze);
			bk->AddToggle("Freeze reflection map",&m_freezeReflectionMap);
			bk->AddSlider("LOD distance multiplier", &m_reflectionLodRangeMultiplier, 0.0f, 16.0f, 0.1f);
		bk->PopGroup();

		bk->PushGroup("Ped specific");
			#if PED_SKIN_PASS
				bk->PushGroup("Skin Pass");
					bk->AddToggle("Enable", &m_enableSkinPass);
					bk->AddSlider("Blur Strength", &m_skinPassParams.x, -10.0f, 10.0f, 0.01f);
					bk->AddSlider("Blur Size", &m_skinPassParams.w, -10.0f, 10.0f, 0.01f);
					bk->AddColor( "Sub surface Color Tweaks", &m_SubSurfaceColorTweak);
					bk->AddToggle("Enable Next Gen Blur", &m_enableSkinPassNG);
				bk->PopGroup();
			#endif // PED_SKIN_PASS
			bk->PushGroup("Ped light");
				bk->AddToggle("Enable", &m_enablePedLight);
				bk->AddToggle("Override", &m_enablePedLightOverride);
				bk->AddColor("Color", &m_pedLightColour, NullCB, 0, NULL, true);
				bk->AddSlider("Intensity", &m_pedLightIntensity, 0.0f, 64.0f, 0.01f);
				bk->AddVector("Direction", &m_pedLightDirection, -1.0f, 1.0f, 0.01f);
				bk->AddButton("Trigger fade-up", TriggerPedLightFadeUp);
				bk->AddSlider("Fade up time", &m_pedLightFadeUpTime, 0.0f, 128.0f, 0.01f);
			bk->PopGroup();
		bk->PopGroup();

		bk->PushGroup("Dynamic Ambient bake");
			bk->AddToggle("Enable override", &m_enableDynamicAmbientBakeOverride);
			bk->AddSlider("Start", &m_dynamicBakeParams.x, 0.0f, 2.0f, 0.01f);
			bk->AddSlider("End", &m_dynamicBakeParams.y, 0.0f, 2.0f, 0.01f);
		bk->PopGroup();

		CRenderPhaseDebugOverlayInterface::StencilMaskOverlayAddWidgets(*bk);

	bk->PopGroup();

	AddWidgets_GBuffer(bk);

	bk->PushGroup("Lights");
		bk->AddToggle("Light Shafts Enabled", &DebugLighting::m_drawLightShafts);
		bk->AddToggle("Light Shafts Draw Always", &DebugLighting::m_drawLightShaftsAlways);
		bk->AddToggle("Light Shafts Draw Normal", &DebugLighting::m_drawLightShaftsNormal);
		bk->AddToggle("Light Shafts Offset Enabled", &DebugLighting::m_drawLightShaftsOffsetEnabled);
		bk->AddVector("Light Shafts Offset", &DebugLighting::m_drawLightShaftsOffset, -20.0f, 20.0f, 1.0f/32.0f);
		bk->AddSlider("Light Shaft Intensity", &DebugLighting::m_LightShaftParams.x, 0.0f, 10.0f, 0.001f);
		bk->AddSlider("Light Shaft Length", &DebugLighting::m_LightShaftParams.y, 0.0f, 10.0f, 0.001f);
		bk->AddSlider("Light Shaft Radius", &DebugLighting::m_LightShaftParams.z, 0.0f, 10.0f, 0.001f);
		bk->AddToggle("Light Shaft Near Clip Enabled", &DebugLighting::m_LightShaftNearClipEnabled);
		bk->AddSlider("Light Shaft Near Clip Exp", &DebugLighting::m_LightShaftNearClipExponent, 0.0f, 20.0f, 1.0f/256.0f);
		bk->AddSeparator();
		bk->AddToggle("Light Volumes Enabled", &DebugLighting::m_drawSceneLightVolumes);
		bk->AddToggle("Light Volumes Always (disable conditional test)", &DebugLighting::m_drawSceneLightVolumesAlways);
		bk->AddToggle("Light Volumes Use Pre Alpha Depth", &DebugLighting::m_usePreAlphaDepthForVolumetricLight);
		bk->AddSlider("Light Volume Low Near Plane Fade Distance", &DebugLighting::m_VolumeLowNearPlaneFade, 0.0f, 20.0f, 0.001f);
		bk->AddSlider("Light Volume High Near Plane Fade Distance", &DebugLighting::m_VolumeHighNearPlaneFade, 0.0f, 20.0f, 0.001f);
#if LIGHT_VOLUMES_IN_FOG
		bk->AddSeparator();
		bk->AddToggle("Fog Light Volumes Enabled", &DebugLighting::m_FogLightVolumesEnabled);
#endif //LIGHT_VOLUMES_IN_FOG
		bk->AddSeparator();
		#if DEPTH_BOUNDS_SUPPORT
			bk->AddToggle("Use Lights Depth Bounds", &DebugLighting::m_useLightsDepthBounds);
		#endif // DEPTH_BOUNDS_SUPPORT
		bk->AddToggle("Force Hight Quality Lighting", &m_bForceHighQualityLighting);
	bk->PopGroup();

	bk->PushGroup("Diffuse Range Visualisation");
		bk->AddToggle("Enable visualisation", &m_diffuseRangeShow);
		bk->AddSlider("Lower range", &m_diffuseRangeMin, 0.0f, 1.0f, 1.0f);
		bk->AddSlider("Upper range", &m_diffuseRangeMax, 0.0f, 1.0f, 1.0f);;
		bk->AddColor("Lower range colour", &m_diffuseRangeLowerColour);
		bk->AddColor("Mid range colour", &m_diffuseRangeMidColour);
		bk->AddColor("Upper range colour", &m_diffuseRangeUpperColour);
	bk->PopGroup();

	bk->PushGroup("Colour Swatches");
		bk->AddToggle("Enable swatches", &m_swatchesEnable);
		bk->AddSeparator();
		bk->AddToggle("2D", &m_swatches2DEnable);
		bk->AddToggle("3D", &m_swatches3DEnable);
		bk->AddSeparator();
		bk->AddSlider("Position (X)", &m_swatchesCentreX, 0, 1280, 1);
		bk->AddSlider("Position (Y)", &m_swatchesCentreY, 0, 720, 1);
		bk->AddSlider("Size",  &m_swatchesSize, 0, 256, 1);
		bk->AddSlider("3D Chart offset",  &m_chartOffset, 0, 256.0f, 0.1f);
	bk->PopGroup();

	bk->PushGroup("Focus Chart");
		bk->AddToggle("Enable focus chart", &m_focusChartEnable);
		bk->AddSlider("Focus chart mip level", &m_focusChartMipLevel, 0, 3, 1);
		bk->AddSlider("Focus chart offset X", &m_focusChartOffsetX, -1.0f, 1.0f, 0.01f); 
		bk->AddSlider("Focus chart offset Y", &m_focusChartOffsetY, -1.0f, 1.0f, 0.01f); 
	bk->PopGroup();

	bk->PushGroup("Ambient Occlusion");
		bk->AddToggle("Test Sphere Ambient Volumes",&DebugLighting::m_testSphereAmbVol);
		bk->AddToggle("Draw Ambient Occlusion", &DebugLighting::m_drawAmbientOcclusion);
		bk->AddToggle("Draw Ambient Scaling Volumes", &DebugLighting::m_drawAmbientVolumes);
	bk->PopGroup();

#if MSAA_EDGE_PASS
	bk->PushGroup("MSAA Edge Mark Pass");
		bk->AddToggle("Enable edge pass (general)",	&DebugDeferred::m_EnableEdgePass);
		bk->AddToggle("Ignore edge detection",		&DebugDeferred::m_IgnoreEdgeDetection);
		bk->AddToggle("Old edge detection (bad)",	&DebugDeferred::m_OldEdgeDetection);
		bk->AddToggle("Alternative edge detection",	&DebugDeferred::m_AlternativeEdgeDetection);
		bk->AddToggle("Aggressive edge detection",	&DebugDeferred::m_AggressiveEdgeDetection);
		bk->AddToggle("Show edges",					&DebugDeferred::m_DebugEdgePassColor);
# if MSAA_EDGE_PROCESS_FADING
		bk->AddToggle("Enable fade processing",		&DebugDeferred::m_EnableEdgeFadeProcessing);
# endif
		bk->AddSeparator();
		bk->AddToggle("Optimize shadow reveal",		&DebugDeferred::m_OptimizeShadowReveal);
		bk->AddToggle("Enable reveal-edge pass",	&DebugDeferred::m_EnableShadowRevealEdge);
		bk->AddToggle("Enable reveal-face pass",	&DebugDeferred::m_EnableShadowRevealFace);
		bk->AddSeparator();
		bk->AddToggle("Optimize shadow processing",		&DebugDeferred::m_OptimizeShadowProcessing);
		bk->AddToggle("Enable processing-edge pass",	&DebugDeferred::m_EnableShadowProcessingEdge);
		bk->AddToggle("Enable processing-face pass",	&DebugDeferred::m_EnableShadowProcessingFace);
		bk->AddSeparator();
		bk->AddToggle("Optimize directional light",	&DebugDeferred::m_OptimizeDirectional);
		bk->AddToggle("Enable dir-edge0 pass",		&DebugDeferred::m_EnableDirectionalEdge0);
		bk->AddToggle("Enable dir-edge1 pass",		&DebugDeferred::m_EnableDirectionalEdge1);
		bk->AddToggle("Enable dir-face0 pass",		&DebugDeferred::m_EnableDirectionalFace0);
		bk->AddToggle("Enable dir-face1 pass",		&DebugDeferred::m_EnableDirectionalFace1);
		bk->AddSeparator();
		bk->AddToggle("Optimize directional foliage light", &DebugDeferred::m_OptimizeDirectionalFoliage);
		bk->AddToggle("Enable foliage-edge pass",			&DebugDeferred::m_EnableDirectionalFoliageEdge);
		bk->AddToggle("Enable foliage-face pass",			&DebugDeferred::m_EnableDirectionalFoliageFace);
		bk->AddSeparator();
		bk->AddToggle("Optimize scene lights",		&DebugDeferred::m_OptimizeSceneLights, 1<<EM_IGNORE);
		bk->AddToggle("Enable lights-edge pass",	&DebugDeferred::m_OptimizeSceneLights, 1<<EM_EDGE0);
		bk->AddToggle("Enable lights-face pass",	&DebugDeferred::m_OptimizeSceneLights, 1<<EM_EDGE0);
		bk->AddSeparator();
		bk->AddToggle("Optimize depth effects",		&DebugDeferred::m_OptimizeDepthEffects, 1<<EM_IGNORE);
		bk->AddToggle("Enable depth-edge pass",		&DebugDeferred::m_OptimizeDepthEffects, 1<<EM_EDGE0);
		bk->AddToggle("Enable depth-face pass",		&DebugDeferred::m_OptimizeDepthEffects, 1<<EM_FACE0);
		bk->AddSeparator();
		bk->AddToggle("Optimize DoF blend",		&DebugDeferred::m_OptimizeDofBlend, 1<<EM_IGNORE);
		bk->AddToggle("Enable dof-edge pass",	&DebugDeferred::m_OptimizeDofBlend, 1<<EM_EDGE0);
		bk->AddToggle("Enable dof-face pass",	&DebugDeferred::m_OptimizeDofBlend, 1<<EM_FACE0);
		bk->AddSeparator();
		bk->AddToggle("Optimize PostFX composite",	&DebugDeferred::m_OptimizeComposite, 1<<EM_IGNORE);
		bk->AddToggle("Enable composite-edge pass",	&DebugDeferred::m_OptimizeComposite, 1<<EM_EDGE0);
		bk->AddToggle("Enable composite-face pass",	&DebugDeferred::m_OptimizeComposite, 1<<EM_FACE0);
		bk->AddSeparator();
		bk->AddSlider("Minumum depth difference",		&DebugDeferred::m_edgeMarkParams.w, 0.f, 1.f, 0.01f);
		bk->AddSlider("Maximum diffuse correlation",	&DebugDeferred::m_edgeMarkParams.x, 0.f, 1.f, 0.1f);
		bk->AddSlider("Maximum normal correlation",		&DebugDeferred::m_edgeMarkParams.y, -1.f, 1.f, 0.1f);
		bk->AddSlider("Minumum specularity difference",	&DebugDeferred::m_edgeMarkParams.z, 0.f, 1.f, 0.1f);
	bk->PopGroup();
#endif
}

// ----------------------------------------------------------------------------------------------- //
#if __PPU
// ----------------------------------------------------------------------------------------------- //

static void ReloadClassificationElfCB()
{
	DebugDeferred::m_spupmReloadElfSlot = RELOAD_CLASSIFICATION;
	sprintf(DebugDeferred::m_spupmReloadElfName, "x:/gta5/src/dev/game/vs_project2_lib/spu-obj/LightClassificationSPU.elf");
	DebugDeferred::m_spupmDoReloadElf = true;
}

void DebugDeferred::AddWidgets_SpuPmElfReload(bkBank *bk)
{
	bk->PushGroup("Elf reload");
		bk->AddToggle("Enable hot elf reloading",		&m_spupmEnableReloadingElf);
		bk->AddButton("Reload LightClassification elf",	ReloadClassificationElfCB);
	bk->PopGroup();
}

// ----------------------------------------------------------------------------------------------- //
#endif //__PPU...
// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::GBufferOverride()
{
	RenderColourSwatches();

	// Gbuffer updates
	if (m_diffuseRangeShow)
	{
		DiffuseRange();
	}

	// Diffuse desaturate
	if (m_overrideDiffuse_desat > 0.01f)
	{
		DesaturateAlbedo(m_overrideDiffuse_desat, 0.0f);
	}

	// Focus chart
	if(m_focusChartEnable)
	{
		RenderFocusChart();
	}

	// Diffuse
	if (m_overrideDiffuse && m_overrideDiffuse_amount != 0.0f)
	{
		OverrideGBufferTarget(
			OVERRIDE_DIFFUSE,
			Vector4(
				m_overrideDiffuse_intensity,
				m_overrideDiffuse_intensity,
				m_overrideDiffuse_intensity,
				0.0f),
			m_overrideDiffuse_amount);
	}

	// Emissive
	if (!m_emissiveEnable)
	{
		OverrideGBufferTarget(
			OVERRIDE_EMISSIVE,
			Vector4(0.0f, 0.0f, 0.0f, 0.0f),
			1.0f);
	}
	else if (m_overrideEmissive)
	{
		OverrideGBufferTarget(
			OVERRIDE_EMISSIVE,
			Vector4(
				m_overrideEmissive_intensity,
				m_overrideEmissive_intensity,
				m_overrideEmissive_intensity,
				m_overrideEmissive_intensity),
			m_overrideEmissive_amount);
	}

	// Shadow
	if (!m_shadowEnable)
	{
		OverrideGBufferTarget(
			OVERRIDE_SHADOW,
			Vector4(0.0f, 0.0f, 0.0f, 0.0f),
			1.0f);
	}
	else if (m_overrideShadow)
	{
		OverrideGBufferTarget(
			OVERRIDE_SHADOW,
			Vector4(
				m_overrideShadow_intensity,
				m_overrideShadow_intensity,
				m_overrideShadow_intensity,
				m_overrideShadow_intensity),
			m_overrideShadow_amount);
	}

	// Ambient
	if (!m_ambientEnable)
	{
		OverrideGBufferTarget(
			OVERRIDE_AMBIENT,
				Vector4(0.0f, 0.0f, 0.0f, 0.0f),
			1.0f);
	}
	else if (m_overrideAmbient)
	{
		OverrideGBufferTarget(
			OVERRIDE_AMBIENT,
				Vector4(m_overrideAmbientParams.x, m_overrideAmbientParams.y, 0.0f, 0.0f),
			1.0f);
	}

	// Specular
	if (!m_specularEnable)
	{
		OverrideGBufferTarget(
			OVERRIDE_SPECULAR,
			Vector4(0.0f, 0.0f, 0.0f, 0.0f),
			1.0f);
	}
	else if (m_overrideSpecular)
	{
		OverrideGBufferTarget(
			OVERRIDE_SPECULAR,
			Vector4(
				Clamp(m_overrideSpecularParams.x, 0.0f, 1.0f),
				Sqrtf(m_overrideSpecularParams.y / 512.0f),
				m_overrideSpecularParams.z,
				0.0f),
			1.0f);
	}
}

// ----------------------------------------------------------------------------------------------- //

static grcDepthStencilStateHandle	g_StencilMaskOverlay_ds_prev = grcStateBlock::DSS_Invalid;
static u8							g_StencilMaskOverlay_sr_prev = 0x00;

bool DebugDeferred::StencilMaskOverlaySelectedBegin(bool bIsSelectedEntity)
{
	if (CRenderPhaseDebugOverlayInterface::StencilMaskOverlayIsSelectModeEnabled())
	{
		if ((bIsSelectedEntity || CRenderPhaseDebugOverlayInterface::StencilMaskOverlayIsSelectAll()) && GBuffer::IsRenderingTo())
		{
			static grcDepthStencilStateHandle DS_handle = grcStateBlock::DSS_Invalid;

			if (DS_handle == grcStateBlock::DSS_Invalid)
			{
				grcDepthStencilStateDesc desc;

				desc.DepthEnable = true;
				desc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
				desc.DepthWriteMask = true;
				desc.StencilEnable = true;
				desc.StencilReadMask = DEFERRED_MATERIAL_TYPE_MASK;
				desc.StencilWriteMask = DEFERRED_MATERIAL_TYPE_MASK;
				desc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
				desc.FrontFace.StencilFailOp = grcRSV::STENCILOP_REPLACE;
				desc.BackFace = desc.FrontFace;

				DS_handle = grcStateBlock::CreateDepthStencilState(desc);
			}

			g_StencilMaskOverlay_ds_prev = grcStateBlock::DSS_Active;
			g_StencilMaskOverlay_sr_prev = grcStateBlock::ActiveStencilRef;

			grcStateBlock::SetDepthStencilState(DS_handle, DEFERRED_MATERIAL_SELECTED);

			return true;
		}
	}

	return false;
}

void DebugDeferred::StencilMaskOverlaySelectedEnd(bool bNeedsDepthStateRestore)
{
	if (bNeedsDepthStateRestore)
	{
		grcStateBlock::SetDepthStencilState(g_StencilMaskOverlay_ds_prev, g_StencilMaskOverlay_sr_prev);
	}
}

void DebugDeferred::StencilMaskOverlay(u8 ref, u8 mask, bool bFlipResult, Vec4V_In colour, float depth, int depthFunc)
{
	static atMap<u16,grcDepthStencilStateHandle> depthStateMap;
	grcDepthStencilStateHandle depthStateHandle = grcStateBlock::DSS_Invalid;
	const u16 depthStateKey = ((u16)mask) | (bFlipResult ? 0x0100 : 0) | ((u16)depthFunc << 9);

	const grcDepthStencilStateHandle* dsh = depthStateMap.Access(depthStateKey);

	if (dsh == NULL)
	{
		grcDepthStencilStateDesc desc;

		desc.DepthEnable = depthFunc != grcRSV::CMP_ALWAYS;
		desc.DepthFunc = depthFunc;
		desc.DepthWriteMask = false;
		desc.StencilEnable = true;
		desc.StencilReadMask = mask;
		desc.StencilWriteMask = 0x00;
		desc.FrontFace.StencilFunc = bFlipResult ? grcRSV::CMP_EQUAL : grcRSV::CMP_NOTEQUAL;
		desc.BackFace.StencilFunc = desc.FrontFace.StencilFunc;

		depthStateHandle = grcStateBlock::CreateDepthStencilState(desc);
		depthStateMap[depthStateKey] = depthStateHandle;
	}
	else
	{
		depthStateHandle = *dsh;
	}

	const grcRasterizerStateHandle   RS_prev = grcStateBlock::RS_Active;
	const grcDepthStencilStateHandle DS_prev = grcStateBlock::DSS_Active;
	const grcBlendStateHandle        BS_prev = grcStateBlock::BS_Active;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(depthStateHandle, ref);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], colour);

	ShaderUtil::StartShader(
		deferred_pass_names[pass_override_gbuffer_texture],
		m_shader,
		m_techniques[DR_TECH_DEFERRED],
		pass_override_gbuffer_texture);
	grcDrawSingleQuadf(0, 0, 1, 1, depth, 0, 0, 1, 1, Color32(255,255,255,255));
	ShaderUtil::DrawNormalizedQuad();
	ShaderUtil::EndShader(m_shader);

	grcStateBlock::SetStates(RS_prev, DS_prev, BS_prev); // restore states
}

// ----------------------------------------------------------------------------------------------- //

void DrawTileFrustum(Vec4V_In tile, const float minD, const float maxD, grcViewport &viewport, Vec4V_In invScreenSize)
{
	const ScalarV minDepth = ScalarV(minD);
	const ScalarV maxDepth = ScalarV(maxD);

	const Vec4V cameraFOV(
		viewport.GetTanHFOV(), -viewport.GetTanVFOV(),
		viewport.GetTanHFOV(), -viewport.GetTanVFOV());
	const Vec4V invScreenSize2 = Scale(Vec4V(V_TWO), invScreenSize);
	Vec4V tileF = Scale(Subtract(Scale(tile, invScreenSize2), Vec4V(V_ONE)), cameraFOV);

	const Vec2V ss_p00 = tileF.Get<Vec::X, Vec::Y>();
	const Vec2V ss_p10 = tileF.Get<Vec::Z, Vec::Y>();
	const Vec2V ss_p01 = tileF.Get<Vec::X, Vec::W>();
	const Vec2V ss_p11 = tileF.Get<Vec::Z, Vec::W>();

	Vec3V ws_p00 = Transform(viewport.GetCameraMtx(), Vec3V(ss_p00, ScalarV(V_NEGONE)));
	Vec3V ws_p10 = Transform(viewport.GetCameraMtx(), Vec3V(ss_p10, ScalarV(V_NEGONE)));
	Vec3V ws_p01 = Transform(viewport.GetCameraMtx(), Vec3V(ss_p01, ScalarV(V_NEGONE)));
	Vec3V ws_p11 = Transform(viewport.GetCameraMtx(), Vec3V(ss_p11, ScalarV(V_NEGONE)));

	Vec3V apex = viewport.GetCameraMtx().GetCol3();

	Vec3V n_p00 = AddScaled(apex, Subtract(ws_p00, apex), minDepth);
	Vec3V n_p10 = AddScaled(apex, Subtract(ws_p10, apex), minDepth);
	Vec3V n_p01 = AddScaled(apex, Subtract(ws_p01, apex), minDepth);
	Vec3V n_p11 = AddScaled(apex, Subtract(ws_p11, apex), minDepth);

	Vec3V f_p00 = AddScaled(apex, Subtract(ws_p00, apex), maxDepth);
	Vec3V f_p10 = AddScaled(apex, Subtract(ws_p10, apex), maxDepth);
	Vec3V f_p01 = AddScaled(apex, Subtract(ws_p01, apex), maxDepth);
	Vec3V f_p11 = AddScaled(apex, Subtract(ws_p11, apex), maxDepth);

	grcDebugDraw::Line(n_p00, f_p00, Color32(0, 255, 0, 255));
	grcDebugDraw::Line(n_p10, f_p10, Color32(0, 255, 0, 255));
	grcDebugDraw::Line(n_p01, f_p01, Color32(0, 255, 0, 255));
	grcDebugDraw::Line(n_p11, f_p11, Color32(0, 255, 0, 255));
}

void calcFrustumPlanes(
	Vec4V_In tileF, 
	Vec4V_In screenSize, 
	Vec4V_In cameraFOV, 
	Mat34V_In viewToWorld,
	Vec3V_In cameraDir,
	Vec4V transposedPlanes0[4],
	Vec4V transposedPlanes1[4],
	ScalarV_In nearDist,
	ScalarV_In farDist)
{
	const Vec4V invScreenSize2 = InvScale(Vec4V(V_TWO), screenSize);
	Vec4V tile = Scale(Subtract(Scale(tileF, invScreenSize2), Vec4V(V_ONE)), cameraFOV);

	// Compute plane frustum
	Vec3V cs_p00 = Vec3V(tile.Get<Vec::X, Vec::Y>(), ScalarV(V_NEGONE));
	Vec3V cs_p10 = Vec3V(tile.Get<Vec::Z, Vec::Y>(), ScalarV(V_NEGONE));
	Vec3V cs_p01 = Vec3V(tile.Get<Vec::X, Vec::W>(), ScalarV(V_NEGONE));
	Vec3V cs_p11 = Vec3V(tile.Get<Vec::Z, Vec::W>(), ScalarV(V_NEGONE));

	Vec3V p00 = Transform(viewToWorld, cs_p00);
	Vec3V p10 = Transform(viewToWorld, cs_p10);
	Vec3V p01 = Transform(viewToWorld, cs_p01);
	Vec3V p11 = Transform(viewToWorld, cs_p11);

	const Vec3V apex = viewToWorld.GetCol3();
	/// const Vec3V camDir = viewToWorld.GetCol2();

	Vec4V planes0[4];
	planes0[0] = BuildPlane(apex, p10, p11);
	planes0[1] = BuildPlane(apex, p00, p10);
	planes0[2] = BuildPlane(apex, p01, p00);	
	planes0[3] = BuildPlane(apex, p11, p01);

	Transpose4x4(
		transposedPlanes0[0],
		transposedPlanes0[1],
		transposedPlanes0[2],
		transposedPlanes0[3],
		planes0[0],
		planes0[1],
		planes0[2],
		planes0[3]
	);

	// Create near and far clip
	Vec4V nearPlane = BuildPlane(AddScaled(apex, Subtract(p00, apex), nearDist), cameraDir);
	Vec4V farPlane = BuildPlane(AddScaled(apex, Subtract(p00, apex), farDist), Negate(cameraDir));

	Vec4V planes1[4];
	planes1[0] = nearPlane;
	planes1[1] = farPlane;
	planes1[2] = nearPlane;
	planes1[3] = farPlane;

	Transpose4x4(
		transposedPlanes1[0],
		transposedPlanes1[1],
		transposedPlanes1[2],
		transposedPlanes1[3],
		planes1[0],
		planes1[1],
		planes1[2],
		planes1[3]);
}

float timebarValues[30];
int timebarIndex = 0;
int timebarSize = 0;

void DebugDeferred::Draw()
{
#if RAGE_TIMEBARS
	if (m_timeBarToEKG)
	{
		float currentTime = g_pfTB.GetSelectedTime();
		timebarValues[timebarIndex] = currentTime;
		timebarIndex = (timebarIndex + 1) % 30;
		timebarSize	= rage::Max<int>(timebarSize, timebarIndex);

		float minVal = FLT_MAX;
		float maxVal = -FLT_MAX;
		float avgVal = 0.0f;

		for (int i = 0; i < timebarSize; i++)
		{
			minVal = rage::Min<float>(minVal, timebarValues[i]);
			maxVal = rage::Max<float>(maxVal, timebarValues[i]);
			avgVal += timebarValues[i];
		}
		avgVal /= (float)timebarSize;

		PF_SET(Average, avgVal);
		PF_SET(Min, minVal);
		PF_SET(Max, maxVal);
		PF_SET(Current, currentTime);
	}
#endif //RAGE_TIMEBARS
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::Update()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_G, KEYBOARD_MODE_DEBUG_ALT)) // cycle through gbuffer indices
	{
		m_GBufferIndex = (m_GBufferIndex + 1)%7;
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_D, KEYBOARD_MODE_DEBUG_ALT)) // toggle decals (this doesn't really belong here, but i couldn't find a better place ..)
	{
		fwRenderListBuilder::GetRenderPassDisableBits() ^= BIT(RPASS_DECAL);
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_X, KEYBOARD_MODE_DEBUG_ALT)) // toggle alpha ..
	{
		fwRenderListBuilder::GetRenderPassDisableBits() ^= BIT(RPASS_ALPHA);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::OverrideGBufferTarget(eGBufferOverride type, const Vector4 &params, float amount)
{
	GBufferRT index = GBUFFER_RT_NUM;

	grcBlendStateHandle blendState = grcStateBlock::BS_Invalid;
	switch (type)
	{
		case OVERRIDE_DIFFUSE: 
		{
			index = GBUFFER_RT_0; 
			blendState = m_overrideDiffuse_B;
			break;
		}

		case OVERRIDE_EMISSIVE:
		{ 
			index = GBUFFER_RT_3; 
			blendState = m_overrideEmissive_B;
			break; 
		}
		
		case OVERRIDE_SHADOW: 
		{ 
			index = GBUFFER_RT_2; 
			blendState = m_overrideShadow_B;
			break; 
		}

		case OVERRIDE_AMBIENT:    
		{
			index = GBUFFER_RT_3; 
			blendState = m_overrideAmbient_B;
			break;
		}

		case OVERRIDE_SPECULAR:
		{
			index = GBUFFER_RT_2; 
			blendState = m_overrideSpecular_B;
			break;
		}

		default:
		{
			Assertf(false, "Tried to override a gbuffer RT that doesn't exist");
			break;
		}
	}

	GBuffer::LockSingleTarget(index, false);
	XENON_ONLY(GBuffer::CopyBuffer(index));

	SetShaderParams();
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], params);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_POINT_TEXTURE], GBuffer::GetTarget(index));

	float blendFactors[4] = {1.0f, 1.0f, 1.0f, amount};

	grcStateBlock::SetBlendState(blendState, blendFactors, ~0ULL);
	grcStateBlock::SetDepthStencilState(m_override_DS);
	grcStateBlock::SetRasterizerState(m_override_R);

	ShaderUtil::StartShader(
		deferred_pass_names[pass_override_gbuffer_texture], 
		m_shader, 
		m_techniques[DR_TECH_DEFERRED], 
		pass_override_gbuffer_texture); 
	ShaderUtil::DrawNormalizedQuad();	
	ShaderUtil::EndShader(m_shader);			

	GBuffer::UnlockSingleTarget(index);
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::ShowGBuffer(GBufferRT index)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	Vector4 filter = Vector4(
		m_GBufferFilter[0] ? 1.0f : 0.0f,
		m_GBufferFilter[1] ? 1.0f : 0.0f,
		m_GBufferFilter[2] ? 1.0f : 0.0f,
		m_GBufferFilter[3] ? 1.0f : 0.0f);

	if (m_GBufferShowTwiddle)
	{
		index = GBUFFER_RT_1;
		filter = Vector4(0.0f, 0.0f, 0.0f, 2.0f); // special-case PS_showMRT to display normal twiddle
	}
	else if (m_GBufferShowDepth)
	{
		index = GBUFFER_DEPTH;
		filter = Vector4(0.0f, 0.0f, 0.0f, 3.0f); // special-case depth (not stencil)
	}
	else if (index == 5)
	{
		index = GBUFFER_RT_2;
		filter = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	SetShaderParams();
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], filter);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_POINT_TEXTURE], (index <= GBUFFER_RT_3) ? GBuffer::GetTarget(index) : GBuffer::PS3_SWITCH(GetDepthTargetAsColor, GetDepthTarget)());

#if __XENON
	//set LDR exp bias for 7e3 format
	GRCDEVICE.SetColorExpBiasNow(CRenderer::GetBackbufferColorExpBiasLDR());
#endif

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_override_DS);
	grcStateBlock::SetRasterizerState(m_override_R);

	ShaderUtil::StartShader(
		deferred_pass_names[pass_show_gbuffer], 
		m_shader, 
		m_techniques[DR_TECH_DEFERRED], 
		pass_show_gbuffer); 
	ShaderUtil::DrawNormalizedQuad();
	ShaderUtil::EndShader(m_shader);			
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::DiffuseRange()
{
	GBuffer::LockSingleTarget(GBUFFER_RT_0, false);
	XENON_ONLY(GBuffer::CopyBuffer(GBUFFER_RT_0));

	SetShaderParams();
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_POINT_TEXTURE], GBuffer::GetTarget(GBUFFER_RT_0));
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], Vector4(m_diffuseRangeMin, m_diffuseRangeMax, 0.0f, 0.0f));
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_RANGE_LOWER_COLOR], m_diffuseRangeLowerColour);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_RANGE_MID_COLOR], m_diffuseRangeMidColour);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_RANGE_UPPER_COLOR], m_diffuseRangeUpperColour);

	grcStateBlock::SetBlendState(m_overrideDiffuseDesat_B);

	grcStateBlock::SetDepthStencilState(m_override_DS);
	grcStateBlock::SetRasterizerState(m_override_R);

	ShaderUtil::StartShader(
		"Diffuse Range", 
		m_shader, 
		m_techniques[DR_TECH_DEFERRED], 
		pass_show_diffuse_range); 
	ShaderUtil::DrawNormalizedQuad();	
	ShaderUtil::EndShader(m_shader);			

	GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::RenderMacbethChart()
{
	if (!m_swatchesEnable || !m_swatches3DEnable)
		return;

	Vector3 cameraPos = camInterface::GetPos();

	grcViewport::SetCurrentWorldIdentity();
	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, CShaderLib::DSS_TestOnly_LessEqual_Invert, m_overrideAll_B);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], Vector4((float)m_swatchesSize, (float)m_swatchesSpacing, m_chartOffset, 0.0f));

	ShaderUtil::StartShader("Swatches", m_shader, m_techniques[DR_TECH_DEFERRED], pass_show_chart); 
	{
		grcBegin(drawQuads, u32(m_swatchesNumX * m_swatchesNumY) * 4);
		{
			for (u32 y = 0; y < (u32)m_swatchesNumY; y++)
			{
				for (u32 x = 0; x < (u32)m_swatchesNumX; x++)
				{
					const u32 index = y * (u32)m_swatchesNumX + x;
					Color32 col = (index < m_swatchesColours.size()) ? m_swatchesColours[index] : Color32(0,0,0,255);

					grcVertex(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, col, (float)x, (float)y);
					grcVertex(-1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, col, (float)x, (float)y);
					grcVertex( 1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, col, (float)x, (float)y);
					grcVertex( 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, col, (float)x, (float)y);
				}
			}
		}
		grcEnd();
	}
	ShaderUtil::EndShader(m_shader);			
}

void DebugDeferred::RenderColourSwatches()
{
	if (!m_swatchesEnable || !m_swatches2DEnable)
		return;

	GBuffer::LockSingleTarget(GBUFFER_RT_0, false);
	XENON_ONLY(GBuffer::CopyBuffer(GBUFFER_RT_0));

	SetShaderParams();
	grcStateBlock::SetBlendState(m_overrideDiffuseDesat_B);
	grcStateBlock::SetDepthStencilState(m_override_DS);
	grcStateBlock::SetRasterizerState(m_override_R);

	Vector2 invScreenSize = Vector2(1.0f / GRCDEVICE.GetWidth(), 1.0f / GRCDEVICE.GetHeight());
	Vector2 swatchSize = Vector2(m_swatchesSize, m_swatchesSize);

	Vector2 window = Vector2(m_swatchesNumX, m_swatchesNumY) * m_swatchesSize + 
					 Vector2(m_swatchesNumX - 1, m_swatchesNumY - 1) * m_swatchesSpacing;

	Vector2 start = Vector2(m_swatchesCentreX, m_swatchesCentreY) - window * 0.5;
	Vector2 point = start;

	ShaderUtil::StartShader("Swatches", m_shader, m_techniques[DR_TECH_DEFERRED], pass_show_swatch); 
	{
		grcBeginQuads(u32(m_swatchesNumX * m_swatchesNumY));
		{
			for (u32 y = 0; y < (u32)m_swatchesNumY; y++)
			{
				for (u32 x = 0; x < (u32)m_swatchesNumX; x++)
				{
					const u32 index = y * (u32)m_swatchesNumX + x;
					
					Vector2 swatchMin = point * invScreenSize;
					Vector2 swatchMax = (point + swatchSize) * invScreenSize;
					
					Color32 col = (index < m_swatchesColours.size()) ? m_swatchesColours[index] : Color32(0,0,0,255);

					grcDrawQuadf(swatchMin.x, swatchMin.y, swatchMax.x, swatchMax.y, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, col);
					
					point.x += m_swatchesSpacing + m_swatchesSize;
				}
				
				point.y += m_swatchesSpacing + m_swatchesSize;
				point.x = start.x;
			}
		}
		grcEndQuads();
	}
	ShaderUtil::EndShader(m_shader);			

	GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
}


void DebugDeferred::RenderFocusChart()
{
	if(Verifyf(m_focusChartTexture, "Error, focus chart texture not found"))
	{
		u32 widthOfCurrentMip = m_focusChartTexture->GetWidth();
		widthOfCurrentMip = widthOfCurrentMip >> m_focusChartMipLevel;

		u32 heightOfCurrentMip = m_focusChartTexture->GetHeight();
		heightOfCurrentMip = heightOfCurrentMip >> m_focusChartMipLevel;

		Vec2 textureCoord;
		textureCoord.x = ((float)GRCDEVICE.GetWidth() / (float)widthOfCurrentMip) + m_focusChartOffsetX;
		textureCoord.y = ((float)GRCDEVICE.GetHeight() / (float)heightOfCurrentMip) + m_focusChartOffsetY;

		GBuffer::LockSingleTarget(GBUFFER_RT_0, false);
		XENON_ONLY(GBuffer::CopyBuffer(GBUFFER_RT_0));

		SetShaderParams();
		grcStateBlock::SetBlendState(m_overrideDiffuseDesat_B);
		grcStateBlock::SetDepthStencilState(m_override_DS);
		grcStateBlock::SetRasterizerState(m_override_R);

		m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_STANDARD_TEXTURE], m_focusChartTexture);

		ShaderUtil::StartShader(
			deferred_pass_names[pass_override_gbuffer_texture],
			m_shader,
			m_techniques[DR_TECH_DEFERRED],
			pass_show_focus_chart);

		grcDrawSingleQuadf(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, m_focusChartOffsetX, m_focusChartOffsetY, textureCoord.x, textureCoord.y, Color32(255,255,255,255));

		ShaderUtil::EndShader(m_shader);

		GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::DesaturateAlbedo(float amount, float whiteness)
{
	GBuffer::LockSingleTarget(GBUFFER_RT_0, false);
	XENON_ONLY(GBuffer::CopyBuffer(GBUFFER_RT_0));

	SetShaderParams();
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], Vector4(amount, whiteness, 0.0f, 0.0f));
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_POINT_TEXTURE], GBuffer::GetTarget(GBUFFER_RT_0));

	grcStateBlock::SetBlendState(m_overrideDiffuseDesat_B);
	grcStateBlock::SetDepthStencilState(m_override_DS);
	grcStateBlock::SetRasterizerState(m_override_R);

	ShaderUtil::StartShader(
		"Albedo Desaturation", 
		m_shader, 
		m_techniques[DR_TECH_DEFERRED], 
		pass_desat_albedo); 
	ShaderUtil::DrawNormalizedQuad();	
	ShaderUtil::EndShader(m_shader);			

	GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
}

// ----------------------------------------------------------------------------------------------- //


template<typename poolType>
bool RefreshAmbient(void* pItem, void* UNUSED_PARAM(data))
{
	poolType *pEntity = static_cast<poolType*>(pItem);

	if (pEntity && pEntity->GetBaseModelInfo() != NULL)
	{
		pEntity->CalculateAmbientScales();
	}

	return (true);
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::RefreshAmbientValues()
{
	CEntity* entity = (CEntity*)g_PickerManager.GetSelectedEntity();

	if (entity != NULL)
	{
		RefreshAmbient<CEntity>(entity, NULL);
	}
	else
	{
		CBuilding::GetPool()->ForAll(RefreshAmbient<CBuilding>, NULL);
		CAnimatedBuilding::GetPool()->ForAll(RefreshAmbient<CAnimatedBuilding>, NULL);
		CDummyObject::GetPool()->ForAll(RefreshAmbient<CDummyObject>, NULL);
		CObject::GetPool()->ForAll(RefreshAmbient<CObject>, NULL);
		CEntityBatch::GetPool()->ForAll(RefreshAmbient<CEntityBatch>, NULL);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::OverrideDynamicBakeValues(Vec4V& params)
{
	if (m_enableDynamicAmbientBakeOverride)
	{
		params = RCC_VEC4V(m_dynamicBakeParams);
	}
	else
	{
		m_dynamicBakeParams = RCC_VECTOR4(params);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::OverrideReflectionTweaks(Vector4& params)
{
	if (m_enableReflectiobTweakOverride)
	{
		params = m_reflectionTweaks;
	}
	else
	{
		m_reflectionTweaks = params;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::OverridePedLight(Vector3& colour, float &intensity, Vector3& direction)
{
	m_pedLightDirection.Normalize();

	if (m_enablePedLightOverride)
	{
		colour = m_pedLightColour;
		direction = m_pedLightDirection;
		intensity = m_pedLightIntensity;
	}
	else
	{
		m_pedLightColour = colour;
		m_pedLightDirection = direction;
		m_pedLightIntensity = intensity;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugDeferred::SetupGBuffers()
{
	grcEffect::SetGlobalVar(m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE0], GBuffer::GetTarget(GBUFFER_RT_0));
	grcEffect::SetGlobalVar(m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE1], GBuffer::GetTarget(GBUFFER_RT_1));
	grcEffect::SetGlobalVar(m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE2], GBuffer::GetTarget(GBUFFER_RT_2));
	grcEffect::SetGlobalVar(m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE3], GBuffer::GetTarget(GBUFFER_RT_3));
#if __PPU
	grcEffect::SetGlobalVar(m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE_DEPTH], CRenderTargets::GetDepthBufferAsColor());
#else
	grcEffect::SetGlobalVar(m_shaderGlobalVars[DR_SHADER_GLOBAL_VAR_DEFERRED_GBUFFER_TEXTURE_DEPTH], GBuffer::GetDepthTarget());
#endif
}

#endif // __BANK
