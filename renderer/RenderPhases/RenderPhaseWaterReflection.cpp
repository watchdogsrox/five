// ====================================================
// renderer/RenderPhases/RenderPhaseWaterReflection.cpp
// (c) 2012 RockstarNorth
// ====================================================

// C headers
#include <stdarg.h>

// Rage headers
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grmodel/shaderfx.h"
#include "profile/timebars.h"
#if __PS3
#include "edge/geom/edgegeom_structs.h"
#include "math/vecshift.h" // for BIT64
#include "system/ppu_symbol.h"
#endif // __PS3
#include "system/memops.h"

// Suite Headers
#include "softrasterizer/scanwaterreflection.h"

// Framework Headers
#include "fwdebug/picker.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwmaths/PortalCorners.h"
#include "fwmaths/vector.h"
#include "fwmaths/vectorutil.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanDebug.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/TiledScreenCapture.h"
#include "game/clock.h"
#include "peds/Ped.h"
#include "profile/profiler.h"
#include "renderer/Debug/Raster.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/HorizonObjects.h"
#include "renderer/Lights/lights.h"
#include "renderer/Lights/LightGroup.h"
#include "renderer/Mirrors.h"
#include "renderer/occlusion.h"
#include "renderer/PostProcessFX.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/sprite2d.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/portals/Portal.h"
#include "scene/portals/FrustumDebug.h"
#include "scene/scene.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/world/GameWorldWaterHeight.h"
#include "system/controlMgr.h"
#include "timecycle/TimeCycle.h"
#include "Vfx/Sky/Sky.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"
#include "vfx/VfxHelper.h"
#include "Vfx/VisualEffects.h"

/*
TODO --
- in GameScan.cpp, i'm passing the stencil from my system if SCAN_DEBUG_USE_NEW_WATER_STENCIL_TEST
  is enabled, but this determination should be made by g_debugWaterReflectionRasteriseCopyFromOcclusion

- also, Ray's stencil is not double-buffered, so it would flicker if we used it for edge culling ..
  need to resolve this somehow ..
  in function SetEdgeStencilTestEnable we need to get the renderthread stencil data ptr

here's what i think we should do
- remove the double-buffering from CRaster
- in UpdateViewport, after everything has been rasterised (and after Ray's rasteriser has finished,
  which we should assert by the way..), we would copy the stencil to a double buffer like this:
  
static Vec4V g_Stencil[2][72];
UpdateViewport
	...
	rasterise stuff ..
	const int b = render.updatebuffer
	sysMemCpy(g_Stencil[b], stencil from either my system or ray's system, 72*sizeof(Vec4V));

then in static void SetEdgeStencilData()
	const int b = render.renderbuffer
	g_EdgeStencilData = g_Stencil[b]

===============================================================
visibility ideas which might help, but i'm not considering atm:

- use edge clip planes to get per-triangle culling against the kDOP8 visibility bounds
	we have 4 edge clip planes implemented for the shadows which we could use for the
	diagonal kDOP8 planes. i'd rather rely on the edge stencil test, but if that doesn't
	work out we could try this instead.

- do a stencil test at the drawablespu level, to cull geometry boxes
	some models have several smaller geometries so we could get better culling this way,
	plus the drawablespu culling is done in object-space instead of worldspace so the
	bounds can be tighter. however this would introduce a lot of "overdraw" in the stencil
	test since the geometry boxes usually overlap quite a bit.
*/

// hack around this shelved code ..
#ifndef EDGE_STENCIL_TEST
#define EDGE_STENCIL_TEST 0
#endif

#ifndef ENTITY_DESC_KDOP
#define ENTITY_DESC_KDOP 0
#endif

EXT_PF_GROUP(BuildRenderList);
PF_TIMER(CRenderPhaseWaterReflectionRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);
PF_TIMER(CRenderPhaseWaterReflection,RenderPhase);

static CRenderPhaseScanned* g_waterReflectionRenderPhase                  = NULL;
static int                  g_waterReflectionTechniqueGroupId             = -1;
static int					g_waterReflectionCutoutTechniqueGroupId		  = -1;

static bool               g_waterReflectionEnabled                        = false;
static bool               g_waterReflectionIsCameraUnderwater             = false;
#if RASTER
static char               g_waterReflectionStencilDebugMsg[256]           = "";
static Color32            g_waterReflectionStencilDebugMsgColour          = Color32(0);
#endif // RASTER
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
static grcEffectGlobalVar g_waterReflectionClipPlaneVarId                 = grcegvNONE;
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
static grcViewport*       g_waterReflectionViewport                       = NULL;
static float              g_waterReflectionVisBoundsScissorX0             = 0.0f;
static float              g_waterReflectionVisBoundsScissorY0             = 0.0f;
static float              g_waterReflectionVisBoundsScissorX1             = 1.0f;
static float              g_waterReflectionVisBoundsScissorY1             = 1.0f;
static bool               g_waterReflectionCurrentScissorEnabled          = false;
static int                g_waterReflectionCurrentScissorFlippedX0        = -1;
static int                g_waterReflectionCurrentScissorX0               = -1;
static int                g_waterReflectionCurrentScissorY0               = -1;
static int                g_waterReflectionCurrentScissorX1               = -1;
static int                g_waterReflectionCurrentScissorY1               = -1;
static Vec4V              g_waterReflectionCurrentWaterPlane              = Vec4V(V_ZERO);
static float              g_waterReflectionCurrentWaterHeight             = 0.0f;
static Vec4V              g_waterReflectionCurrentClosestPool             = Vec4V(V_ZERO);
static float              g_waterReflectionScriptWaterHeight              = 0.0f;
static bool               g_waterReflectionScriptWaterHeightEnabled       = false;
static bool               g_waterReflectionScriptObjectVisibility         = false;
static float              g_waterReflectionPlaneHeightOffset_TCVAR        = 0.0f; // timecycle modifier TCVAR_WATER_REFLECTION_HEIGHT_OFFSET, for BS#1290256
BANK_ONLY(static bool     g_waterReflectionPlaneHeightOffset_TCVAREnabled = true;)// this controls both TCVAR_WATER_REFLECTION_HEIGHT_OFFSET and TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE
static float              g_waterReflectionPlaneHeightOverride_TCVAR      = 0.0f; // timecycle modifier TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE
static float              g_waterReflectionPlaneHeightOverrideAmount_TCVAR= 0.0f; // timecycle modifier TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE_AMOUNT
static float              g_waterReflectionDistLightIntensity_TCVAR       = 1.0f; // timecycle modifier TCVAR_WATER_REFLECTION_DISTANT_LIGHT_INTENSITY
BANK_ONLY(static bool     g_waterReflectionDistLightIntensity_TCVAREnabled= true;)
static float              g_waterReflectionCoronaIntensity_TCVAR          = 1.0f; // timecycle modifier TCVAR_WATER_REFLECTION_CORONA_INTENSITY
BANK_ONLY(static bool     g_waterReflectionCoronaIntensity_TCVAREnabled   = true;)
static float              g_waterReflectionSkyFLODRange_TCVAR             = 0.0f;
static float              g_waterReflectionLODRange_TCVAR[][2]            = {{0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}};
static bool               g_waterReflectionLODRangeEnabled_TCVAR          = false;
static float              g_waterReflectionLODRangeCurrent[][2]           = {{0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}};
static float              g_waterReflectionLODRangeCurrentEnabled         = false;
CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(g_waterReflectionLODRange_TCVAR));
CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(g_waterReflectionLODRangeCurrent));
#if WATER_REFLECTION_OFFSET_TRICK
static atMap<u32,float>   g_waterReflectionOffsetModelHeightMap;
#endif // WATER_REFLECTION_OFFSET_TRICK
static int                g_waterReflectionHighestDrawableLOD             = dlDrawListMgr::DLOD_MED;
static int                g_waterReflectionHighestFragmentLOD             = dlDrawListMgr::DLOD_MED;
static int                g_waterReflectionHighestVehicleLOD              = dlDrawListMgr::VLOD_HIGH;
static int                g_waterReflectionHighestPedLOD                  = dlDrawListMgr::PLOD_LOD;
static int                g_waterReflectionHighestEntityLOD               = 0;
static int                g_waterReflectionHighestEntityLODCurrentLOD     = LODTYPES_DEPTH_LOD;
BANK_ONLY(static char     g_waterReflectionHighestEntityLODCurrentLODStr[8] = "LOD";)
static int                g_waterReflectionHighestEntityLOD_TCVAR         = 0; // timecycle modifier TCVAR_WATER_REFLECTION_LOD, for BS#1122926
BANK_ONLY(static bool     g_waterReflectionHighestEntityLOD_TCVAREnabled  = true;)
static bool               g_waterReflectionMaxPedDistanceEnabled          = true;
static float              g_waterReflectionMaxPedDistance                 = 25.0f;
static float              g_waterReflectionMaxVehDistanceEnabled          = true;
static float              g_waterReflectionMaxVehDistance                 = 50.0f;
static float              g_waterReflectionMaxObjDistanceEnabled          = true;
static float              g_waterReflectionMaxObjDistance                 = 30.0f;
static bool               g_waterReflectionFarClipEnabled                 = true;
static bool               g_waterReflectionFarClipOverride                = false; // overrides forcing water reflection to be sky-only when TCVAR_WATER_REFLECTION_FAR_CLIP is close to zero
static float              g_waterReflectionFarClipDistance                = 7000.0f;
static bool               g_waterReflectionNoStencilPixels                = false;
static bool               g_waterReflectionOcclusionDisabledForOceanWaves = false;
static Vec4V              g_waterReflectionSkyColour                      = Vec4V(V_ZERO);

static u32 g_waterReflectionRenderSettings =
//	RENDER_SETTING_RENDER_PARTICLES          |
//	RENDER_SETTING_RENDER_WATER              |
//	RENDER_SETTING_STORE_WATER_OCCLUDERS     |
	RENDER_SETTING_RENDER_ALPHA_POLYS        |
	RENDER_SETTING_RENDER_SHADOWS            |
//	RENDER_SETTING_RENDER_DECAL_POLYS        | // not applicable
	RENDER_SETTING_RENDER_CUTOUT_POLYS       |
//	RENDER_SETTING_RENDER_UI_EFFECTS         |
//	RENDER_SETTING_ALLOW_PRERENDER           |
	RENDER_SETTING_RENDER_DISTANT_LIGHTS     |
	RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS |
	0;

static u32 g_waterReflectionVisMaskFlags =
//	VIS_ENTITY_MASK_OTHER             |
	VIS_ENTITY_MASK_PED               |
	VIS_ENTITY_MASK_OBJECT            |
	VIS_ENTITY_MASK_DUMMY_OBJECT      |
	VIS_ENTITY_MASK_VEHICLE           |
	VIS_ENTITY_MASK_ANIMATED_BUILDING |
	VIS_ENTITY_MASK_LOLOD_BUILDING    |
	VIS_ENTITY_MASK_HILOD_BUILDING    |
//	VIS_ENTITY_MASK_LIGHT             |
//	VIS_ENTITY_MASK_TREE              | // we do render "low LOD" trees, which are actually buildings
	0;

static u32 g_waterReflectionDrawFilter =
	CRenderListBuilder::RENDER_OPAQUE             |
	CRenderListBuilder::RENDER_ALPHA              |
	CRenderListBuilder::RENDER_FADING             |
	CRenderListBuilder::RENDER_DONT_RENDER_PLANTS |
	0;

static float                      g_waterReflectionBlendStateAlphaTestAlphaRef = 128.f/255.f;
static grcDepthStencilStateHandle g_waterReflectionDepthStencilClear = grcStateBlock::DSS_Invalid;
#if WATER_REFLECTION_PRE_REFLECTED
static grcRasterizerStateHandle   g_waterReflectionCullFrontState = grcStateBlock::RS_Invalid;
static grcRasterizerStateHandle   g_waterReflectionCullNoneState  = grcStateBlock::RS_Invalid;
#endif // WATER_REFLECTION_PRE_REFLECTED

static u32 g_waterReflectionAlphaBlendingPasses =
//	BIT(RPASS_VISIBLE) |
//	BIT(RPASS_LOD    ) |
	BIT(RPASS_CUTOUT ) |
//	BIT(RPASS_DECAL  ) | // not applicable
	BIT(RPASS_FADING ) |
	BIT(RPASS_ALPHA  ) |
//	BIT(RPASS_WATER  ) | // not applicable
//	BIT(RPASS_TREE   ) |
	0;

static u32 g_waterReflectionDepthWritingPasses =
	BIT(RPASS_VISIBLE) |
	BIT(RPASS_LOD    ) |
	BIT(RPASS_CUTOUT ) |
//	BIT(RPASS_DECAL  ) | // not applicable
	BIT(RPASS_FADING ) | // enabled on 01-26-2013, this should fix the coronas and distant lights being visible though geometry .. might introduce other problems though
//	BIT(RPASS_ALPHA  ) |
//	BIT(RPASS_WATER  ) | // not applicable
	BIT(RPASS_TREE   ) |
	0;

grcRenderTarget* CRenderPhaseWaterReflection::m_color;
grcRenderTarget* CRenderPhaseWaterReflection::m_depth;
#if RSG_PC
grcRenderTarget* CRenderPhaseWaterReflection::m_MSAAcolor = NULL;
#endif
#if __D3D11
grcRenderTarget* CRenderPhaseWaterReflection::m_depthCopy = NULL;
grcRenderTarget* CRenderPhaseWaterReflection::m_depthReadOnly = NULL;
#endif // __D3D11

bool g_TestWaterReflectionOn	= false;

void CRenderPhaseWaterReflection::InitClass()
{
	g_waterReflectionTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("waterreflection");
	g_waterReflectionCutoutTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("waterreflectionalphaclip");
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	g_waterReflectionClipPlaneVarId = grcEffect::LookupGlobalVar("gLight0PosX");
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
}

#if __BANK
static void SetupWaterReflectionParams();
static void HighQualityMode_button();
#endif // __BANK

CRenderPhaseWaterReflection::CRenderPhaseWaterReflection(CViewport* pGameViewport) : CRenderPhaseScanned(pGameViewport, "Water Reflection", DL_RENDERPHASE_WATER_REFLECTION, 0.0f)
{
	g_waterReflectionRenderPhase = this;

#if RSG_DEV && RSG_ASSERT
	GenerateMask128_TEST();
#endif // RSG_DEV && RSG_ASSERT

	SetRenderFlags(g_waterReflectionRenderSettings);
	GetEntityVisibilityMask().SetAllFlags((u16)g_waterReflectionVisMaskFlags);
	SetVisibilityType(VIS_PHASE_WATER_REFLECTION);
	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase(RPTYPE_REFLECTION, this));

	grcDepthStencilStateDesc dss_clear;
	dss_clear.DepthEnable                  = true;
	dss_clear.DepthWriteMask               = true;
	dss_clear.DepthFunc                    = grcRSV::CMP_ALWAYS;
	dss_clear.StencilEnable                = true;
	dss_clear.StencilReadMask              = 0x00;
	dss_clear.StencilWriteMask             = 0xff;
	dss_clear.FrontFace.StencilFailOp      = grcRSV::STENCILOP_REPLACE; 
	dss_clear.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_REPLACE; 
	dss_clear.FrontFace.StencilPassOp      = grcRSV::STENCILOP_REPLACE; 
	dss_clear.FrontFace.StencilFunc        = grcRSV::CMP_ALWAYS;
	dss_clear.BackFace                     = dss_clear.FrontFace;
	g_waterReflectionDepthStencilClear     = grcStateBlock::CreateDepthStencilState(dss_clear);

#if WATER_REFLECTION_PRE_REFLECTED
	grcRasterizerStateDesc rs_cullfront;
	rs_cullfront.CullMode                  = grcRSV::CULL_FRONT;
	rs_cullfront.HalfPixelOffset           = XENON_SWITCH(true, false);
	g_waterReflectionCullFrontState        = grcStateBlock::CreateRasterizerState(rs_cullfront);

	grcRasterizerStateDesc rs_cullnone;
	rs_cullnone.CullMode                   = grcRSV::CULL_NONE;
	rs_cullnone.HalfPixelOffset            = XENON_SWITCH(true, false);
	g_waterReflectionCullNoneState         = grcStateBlock::CreateRasterizerState(rs_cullnone);
#endif // WATER_REFLECTION_PRE_REFLECTED

#if __BANK
	SetupWaterReflectionParams();
#endif // __BANK
}

CRenderPhaseWaterReflection::~CRenderPhaseWaterReflection()
{
	if (m_color)         { delete m_color; m_color = NULL; }
	if (m_depth)         { delete m_depth; m_depth = NULL; }
#if RSG_PC
	if (m_MSAAcolor)     { delete m_MSAAcolor; m_MSAAcolor = NULL; }
#endif
#if __D3D11
	if (m_depthCopy)     { delete m_depthCopy; m_depthCopy = NULL; }
	if (m_depthReadOnly) { delete m_depthReadOnly; m_depthReadOnly = NULL; }
#endif // __D3D11
}

#define WATER_REFLECTION_DEFAULT_NEAR_CLIP                         0.05f
#define WATER_REFLECTION_DEFAULT_OBLIQUE_PROJ_SCALE                1.00f
#define WATER_REFLECTION_DEFAULT_POOL_SPHERE_RADIUS_SCALE          4.00f
#define WATER_REFLECTION_DEFAULT_DISABLE_OCCLUSION_THRESHOLD       0.15f // when camera is <= this height above water (and <= 20 meteres above zero), water occlusion is disabled
#define WATER_REFLECTION_DEFAULT_CULL_PLANES                       true
#define WATER_REFLECTION_DEFAULT_CULL_NEAR_PLANE                   false // TODO -- this near plane may be incorrect, i've disabled it to fix BS#1403024 (it probably doesn't help much anyway)
#define WATER_REFLECTION_DEFAULT_SCISSOR                           true
#define WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND                4
#define WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND_Y0             8
#define WATER_REFLECTION_DEFAULT_SCISSOR_EXPAND                    0
#define WATER_REFLECTION_DEFAULT_SCISSOR_EXPAND_Y0                 4
#define WATER_REFLECTION_DEFAULT_RENDER_SKY_BACKGROUND_WITH_SHADER true//!__PS3 // TODO -- would like to render sky background using GRCDEVICE.Clear on ps3, but this seems to not work
#define WATER_REFLECTION_DEFAULT_RENDER_SKY_OFFSET_X               PS3_SWITCH(0.0f, -0.9375f) // tested on 360's 4xMSAA
#define WATER_REFLECTION_DEFAULT_RENDER_SKY_OFFSET_Y               PS3_SWITCH(0.0f, +0.9375f)

#if __BANK

enum
{
	WATER_REFLECTION_TIMECYCLE_MODE_DISABLED,
	WATER_REFLECTION_TIMECYCLE_MODE_OVERRIDE,
	WATER_REFLECTION_TIMECYCLE_MODE_TIMECYCLE,
	WATER_REFLECTION_TIMECYCLE_MODE_COUNT,
};

static const char* g_waterReflectionTimecycleModeStrings[] =
{
	"Disabled",
	"Override",
	"Timecycle (default)",
};
CompileTimeAssert(NELEM(g_waterReflectionTimecycleModeStrings) == WATER_REFLECTION_TIMECYCLE_MODE_COUNT);

static bool    g_debugWaterReflectionShowInfo                       = false;
static bool    g_debugWaterReflectionUseWaterReflectionTechnique    = true;
static bool    g_debugWaterReflectionForceDefaultRenderBucket       = false;
static bool    g_debugWaterReflectionIsUsingMirrorWaterSurface      = false;
#if RASTER
static bool    g_debugWaterReflectionRaster                         = false;
static bool    g_debugWaterReflectionRasteriseOcclBoxes             = false;
static bool    g_debugWaterReflectionRasteriseOcclMeshes            = false;
static bool    g_debugWaterReflectionRasteriseOcclWater             = false;
static float   g_debugWaterReflectionRasteriseOcclGeomOpacity       = 0.0f;
static bool    g_debugWaterReflectionRasteriseTest                  = false;
static bool    g_debugWaterReflectionRasteriseTestStencil           = false;
static bool    g_debugWaterReflectionRasteriseTestViewportClip      = true;
static Vec2V   g_debugWaterReflectionRasteriseTestP[3]              = {Vec2V(20.0f,12.0f), Vec2V(V_FOUR), Vec2V(24.0f,28.0f)};//{Vec2V(40,20), Vec2V(100,30), Vec2V(80,50)};
static Vec2V   g_debugWaterReflectionRasteriseTestOffset            = Vec2V(V_ZERO);
static float   g_debugWaterReflectionRasteriseTestAngle             = 0.0f;
static ScalarV g_debugWaterReflectionRasteriseTestZ[3]              = {ScalarV(V_ONE), ScalarV(V_ONE), ScalarV(V_ONE)};
static char    g_debugWaterReflectionDumpOccludersPath[80]          = "assets:/non_final/occluders.obj";
static bool    g_debugWaterReflectionDumpOccluders                  = false;
static bool    g_debugWaterReflectionDumpOccludersWaterOnly         = true;
#endif // RASTER
static bool    g_debugWaterReflectionRemoveIsolatedStencil          = false; // remove 1-pixel thick horizontal spans
static bool    g_debugWaterReflectionDisableOcclusionCloseToOcean   = true;
static float   g_debugWaterReflectionDisableOcclusionThreshold      = WATER_REFLECTION_DEFAULT_DISABLE_OCCLUSION_THRESHOLD;
static int     g_debugWaterReflectionVisBoundsMode                  = WRVB_DEFAULT; // eWaterReflectionVisibilityBounds
static bool    g_debugWaterReflectionVisBoundsDraw                  = false;
static bool    g_debugWaterReflectionVisBoundsCullPlanes            = WATER_REFLECTION_DEFAULT_CULL_PLANES;
static bool    g_debugWaterReflectionVisBoundsCullPlanesWater       = true;
static bool    g_debugWaterReflectionVisBoundsCullPlanesNear        = WATER_REFLECTION_DEFAULT_CULL_NEAR_PLANE;
static bool    g_debugWaterReflectionVisBoundsCullPlanesShow        = false;
#if __PS3
static bool    g_debugWaterReflectionVisBoundsClipPlanesEdge        = true;
#endif // __PS3
static bool    g_debugWaterReflectionVisBoundsScissor               = WATER_REFLECTION_DEFAULT_SCISSOR;
#if __PS3
static bool    g_debugWaterReflectionVisBoundsScissorEdgeOnly       = false;
#endif // __PS3
static bool    g_debugWaterReflectionVisBoundsScissorSky            = true;
static int     g_debugWaterReflectionVisBoundsScissorSkyExpand      = WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND;
static int     g_debugWaterReflectionVisBoundsScissorSkyExpandY0    = WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND_Y0;
static int     g_debugWaterReflectionVisBoundsScissorExpand         = WATER_REFLECTION_DEFAULT_SCISSOR_EXPAND;
static int     g_debugWaterReflectionVisBoundsScissorExpandY0       = WATER_REFLECTION_DEFAULT_SCISSOR_EXPAND_Y0;
static bool    g_debugWaterReflectionVisBoundsScissorOverride       = false;
static int     g_debugWaterReflectionVisBoundsScissorX0             = 0;
static int     g_debugWaterReflectionVisBoundsScissorY0             = 0;
static int     g_debugWaterReflectionVisBoundsScissorX1             = 512;
static int     g_debugWaterReflectionVisBoundsScissorY1             = 256;
static bool    g_debugWaterReflectionCullingPlanesEnabled           = true;
#if !__PS3
static bool    g_debugWaterReflectionUseHWClipPlane                 = true;
#endif // !__PS3
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
static bool    g_debugWaterReflectionUseClipPlaneInPixelShader      = true;
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
#if WATER_REFLECTION_PRE_REFLECTED
static bool    g_debugWaterReflectionNoPixelOrHWClipPreReflected    = true;
static bool    g_debugWaterReflectionHidePreReflected               = false;
#endif // WATER_REFLECTION_PRE_REFLECTED
static bool    g_debugWaterReflectionClipPlaneOffsetEnabled         = false;
static float   g_debugWaterReflectionClipPlaneOffset                = 0.0f;
static bool    g_debugWaterReflectionUseObliqueProjection           = __PS3 && !WATER_REFLECTION_CLIP_IN_PIXEL_SHADER;
static float   g_debugWaterReflectionObliqueProjectionOffset        = 0.0f;
static float   g_debugWaterReflectionObliqueProjectionScale         = WATER_REFLECTION_DEFAULT_OBLIQUE_PROJ_SCALE;
static bool    g_debugWaterReflectionViewportReflectInWaterPlane    = true;
static bool    g_debugWaterReflectionViewportSetNormWindow          = false; // do we ever actually need to do this?
static bool    g_debugWaterReflectionWaterOcclusionEnabled          = true;
static bool    g_debugWaterReflectionRenderOnlyWithPrimary          = false;
static bool    g_debugWaterReflectionRenderExterior                 = true;
static bool    g_debugWaterReflectionRenderInterior                 = true;
static bool    g_debugWaterReflectionPortalTraversal                = true;
static bool    g_debugWaterReflectionUsePoolSphere                  = true;
static float   g_debugWaterReflectionPoolSphereRadiusScale          = WATER_REFLECTION_DEFAULT_POOL_SPHERE_RADIUS_SCALE;
static float   g_debugWaterReflectionNearClip                       = WATER_REFLECTION_DEFAULT_NEAR_CLIP;
static bool    g_debugWaterReflectionOverrideWaterHeightEnabled     = false;
static float   g_debugWaterReflectionOverrideWaterHeight            = 0.0f;
static float   g_debugWaterReflectionHeightOffset                   = 0.0f;
#if WATER_REFLECTION_OFFSET_TRICK
static Vec3V   g_debugWaterReflectionOffsetGlobal                   = Vec3V(V_ZERO);
static float   g_debugWaterReflectionOffsetCameraTrack              = 0.0f;
static bool    g_debugWaterReflectionOffsetEnabled                  = true;
#endif // WATER_REFLECTION_OFFSET_TRICK
static int     g_debugWaterReflectionSkyFLODMode                    = WATER_REFLECTION_TIMECYCLE_MODE_TIMECYCLE;
static float   g_debugWaterReflectionSkyFLODRangeOverride           = 0.0f;
static bool    g_debugWaterReflectionLODMaskEnabled                 = true;
static bool    g_debugWaterReflectionLODMaskOverrideEnabled         = false;
static u8      g_debugWaterReflectionLODMaskOverride                = fwRenderPhase::LODMASK_NONE;
static int     g_debugWaterReflectionLODRangeMode                   = WATER_REFLECTION_TIMECYCLE_MODE_TIMECYCLE;
static bool    g_debugWaterReflectionLODRangeEnabled[]              = {true, true, true, true, true, true, true};
static float   g_debugWaterReflectionLODRange[][2]                  = {{0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}, {0,16000}};
static bool    g_debugWaterReflectionForceUpAlphaForLODTypeAlways[] = {false, false, false, false, false, false, false};
static bool    g_debugWaterReflectionForceUpAlphaForLODTypeNever[]  = {false, false, false, false, false, false, false};
static bool    g_debugWaterReflectionForceUpAlphaForSelected        = false;
CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(g_debugWaterReflectionLODRangeEnabled));
CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(g_debugWaterReflectionLODRange));
CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(g_debugWaterReflectionForceUpAlphaForLODTypeAlways));
CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(g_debugWaterReflectionForceUpAlphaForLODTypeNever));
static bool    g_debugWaterReflectionWireframeOverride              = false;
static Color32 g_debugWaterReflectionClearColour                    = Color32(255,0,0,255);
static bool    g_debugWaterReflectionClearColourAlways              = false; // clear even when sky is rendering
static bool    g_debugWaterReflectionRenderProxiesWhenFarClipIsZero = true;
static bool    g_debugWaterReflectionRenderSkyBackground            = true;
static bool    g_debugWaterReflectionRenderSkyBackgroundToneMapped  = true;
static bool    g_debugWaterReflectionRenderSkyBackgroundWithShader  = WATER_REFLECTION_DEFAULT_RENDER_SKY_BACKGROUND_WITH_SHADER;
static float   g_debugWaterReflectionRenderSkyBackgroundOffsetX     = WATER_REFLECTION_DEFAULT_RENDER_SKY_OFFSET_X;
static float   g_debugWaterReflectionRenderSkyBackgroundOffsetY     = WATER_REFLECTION_DEFAULT_RENDER_SKY_OFFSET_Y;
static float   g_debugWaterReflectionRenderSkyBackgroundHDRScale    = 1.0f;
static bool    g_debugWaterReflectionRenderSky                      = true;
static bool    g_debugWaterReflectionRenderSkyOnly                  = false;
static bool    g_debugWaterReflectionRenderDistantLights            = true;
static float   g_debugWaterReflectionDistLightFadeoutMinHeight      = 30.0f;
static float   g_debugWaterReflectionDistLightFadeoutMaxHeight      = 90.0f;
static float   g_debugWaterReflectionDistLightFadeoutSpeed          = 0.1f;
static bool    g_debugWaterReflectionRenderCoronas                  = true;
static bool    g_debugWaterReflectionDirectionalLightAndShadows     = true;
#if WATER_REFLECTION_PRE_REFLECTED
static bool    g_debugWaterReflectionDirLightReflectPreReflected    = true;
static bool    g_debugWaterReflectionDirShadowDisablePreReflected   = true;
#endif // WATER_REFLECTION_PRE_REFLECTED
#if __PS3
static bool    g_debugWaterReflectionEdgeCulling                    = true;
static bool    g_debugWaterReflectionEdgeCullingNoPixelTest         = false;
#if EDGE_STENCIL_TEST && RASTER
static bool    g_debugWaterReflectionEdgeCullingStencilTest         = false;
static int     g_debugWaterReflectionEdgeCullingStencilTestMode     = 1;
static bool    g_debugWaterReflectionDisableNonEdgeGeometry         = false;
#endif // EDGE_STENCIL_TEST && RASTER
static bool    g_debugWaterReflectionSetZMinMaxControl              = false;
#endif // __PS3
static bool    g_debugWaterReflectionDrawListPrototypeEnabled       = false;
static bool    g_debugWaterReflectionCanAlphaCullInPostScan         = true;
static bool    g_debugWaterReflectionRenderAllPhysicalEntities      = true;

PARAM(waternovis50,"");
PARAM(watervisenable,"");
PARAM(waternofarclip,"");
PARAM(waterhiquality,"");

static bool WaterReflectionVisEnabled = true;
static void WaterReflectionVisEnabled_changed()
{
	if (WaterReflectionVisEnabled)
	{
		g_debugWaterReflectionVisBoundsCullPlanes   = true;
		g_debugWaterReflectionVisBoundsScissor      = true;
		g_debugWaterReflectionWaterOcclusionEnabled = true;
		g_waterReflectionFarClipOverride            = false;
		g_waterReflectionFarClipDistance            = 7000.0f;
	}
	else
	{
		g_debugWaterReflectionVisBoundsCullPlanes   = false;
		g_debugWaterReflectionVisBoundsScissor      = false;
		g_debugWaterReflectionWaterOcclusionEnabled = false;
		g_waterReflectionFarClipOverride            = true;
		g_waterReflectionFarClipDistance            = 7000.0f; // will update via timecycle
	}
}

static void SetupWaterReflectionParams()
{
	if (PARAM_waternovis50.Get())
	{
		g_debugWaterReflectionOverrideWaterHeightEnabled = true;
		g_debugWaterReflectionOverrideWaterHeight = 50.0f;

		WaterReflectionVisEnabled = false;
		WaterReflectionVisEnabled_changed();
	}

	if (PARAM_watervisenable.Get())
	{
		if (0) // testing water occlusion stencil test in ScanEntities
		{
			g_debugWaterReflectionVisBoundsCullPlanes = false;
			g_debugWaterReflectionVisBoundsScissor    = false;
			g_scanDebugFlagsPortals |= SCAN_DEBUG_USE_NEW_WATER_STENCIL_TEST_DEBUG;
			HighQualityMode_button();
		}
	}

	if (PARAM_waternofarclip.Get())
	{
		g_waterReflectionFarClipOverride = true;
	}

	if (PARAM_waterhiquality.Get())
	{
		HighQualityMode_button();
	}
}

#endif // __BANK

// this gets called in UpdateViewport, which is the first renderphase function to be called in the frame
static bool IsWaterReflectionEnabled(bool bHasEntityListIndex)
{
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_WaterQuality == (CSettings::eSettingsLevel) (0)) return false;
#endif

#if __BANK
	bool bShowInfo = g_debugWaterReflectionShowInfo || CRenderTargets::IsShowingRenderTargetWaterDraw();

#if RASTER
	if (CRasterInterface::GetControlRef().m_displayOpacity > 0.0f)
	{
		bShowInfo = true;
	}

	if (bShowInfo)
	{
		grcDebugDraw::AddDebugOutput(g_waterReflectionStencilDebugMsgColour, g_waterReflectionStencilDebugMsg);
	}
#endif // RASTER

	if (bShowInfo)
	{
		const int numWaterPixelsOcean = Water::GetWaterPixelsRenderedOcean();
		const int numWaterPixelsRiver = Water::GetWaterPixelsRendered(Water::query_riverplanar, true);
		const int numWaterPixelsEnv   = Water::GetWaterPixelsRendered(Water::query_riverenv, true);

		const int numWaterPixels = Water::GetWaterPixelsRendered(-1, true);
		const int numScreenPixels = GRCDEVICE.GetWidth()*GRCDEVICE.GetHeight();

		const float dh = gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition().GetZf() - g_waterReflectionCurrentWaterHeight;

		grcDebugDraw::AddDebugOutput("water pixels = %d (%.2f%%), ocean = %d, river = %d, env = %d", numWaterPixels, 100.0f*(float)numWaterPixels/(float)numScreenPixels, numWaterPixelsOcean, numWaterPixelsRiver, numWaterPixelsEnv);
		grcDebugDraw::AddDebugOutput("water height = %f (%f), %s%.2f", g_waterReflectionCurrentWaterHeight, Water::GetWaterLevel(), dh > 0.0f ? "+" : "", dh);
		grcDebugDraw::AddDebugOutput("water plane = %f,%f,%f,%f", VEC4V_ARGS(g_waterReflectionCurrentWaterPlane));
		grcDebugDraw::AddDebugOutput("water far clip = %f", g_waterReflectionFarClipDistance);
		grcDebugDraw::AddDebugOutput("water interior = %s (%s/%s)", Water::IsUsingMirrorWaterSurface() ? "true" : "false", fwScan::GetScanResults().GetWaterSurfaceVisible() ? "wsv" : "-", CMirrors::GetIsMirrorVisible(true) ? "mv" : "-");
	}
#endif // __BANK

	float waterReflection = g_timeCycle.GetStaleWaterReflection(); // TCVAR_WATER_REFLECTION

#if __BANK
	if (Water::m_bForceWaterReflectionON)  { waterReflection = 1.0f; }
	if (Water::m_bForceWaterReflectionOFF) { waterReflection = 0.0f; }
#endif // __BANK

	if (waterReflection <= 0.01f)
	{
#if __BANK
		if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("water reflection - skipping because water reflection is %f in timecycle", waterReflection);
		}
#endif // __BANK
		return false;
	}
	
#if __BANK
	if (CMirrors::GetScanIsWaterReflectionDisabled())
	{
		if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("water reflection - PROBABLY skipping because water reflection is disabled (mirror is visible)");
		}
	}
#endif // __BANK

#if __BANK
	if (Water::IsCameraUnderwater() && !Water::m_bUnderwaterPlanarReflection)
	{
		if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("water reflection - skipping because camera is underwater (using screen reflection)");
		}

		return false;
	}
#endif // __BANK

	if (!bHasEntityListIndex)
	{
#if __BANK
		if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("water reflection - skipping because no entity list");
		}
#endif // __BANK
		return false;
	}

	return true;
}

u32 CRenderPhaseWaterReflection::GetCullFlags() const
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return 0;
	}
#endif // __BANK

	if (!g_waterReflectionEnabled)
	{
		return 0;
	}

	u32 flags =
		CULL_SHAPE_FLAG_WATER_REFLECTION |
		CULL_SHAPE_FLAG_RENDER_EXTERIOR |
		CULL_SHAPE_FLAG_RENDER_INTERIOR |
		CULL_SHAPE_FLAG_TRAVERSE_PORTALS |
		CULL_SHAPE_FLAG_GEOMETRY_FRUSTUM |
		CULL_SHAPE_FLAG_USE_MAX_PED_VEH_OBJ_DISTANCES |
		CULL_SHAPE_FLAG_ENABLE_WATER_OCCLUSION;

#if __BANK
	if (!g_debugWaterReflectionWaterOcclusionEnabled) { flags &= ~CULL_SHAPE_FLAG_ENABLE_WATER_OCCLUSION; }
	if (!g_debugWaterReflectionWaterOcclusionEnabled) { flags &= ~CULL_SHAPE_FLAG_ENABLE_WATER_OCCLUSION; }
	if ( g_debugWaterReflectionRenderOnlyWithPrimary) { flags |=  CULL_SHAPE_FLAG_RENDER_ONLY_IF_PRIMARY_RENDERS; }
	if (!g_debugWaterReflectionRenderExterior)        { flags &= ~CULL_SHAPE_FLAG_RENDER_EXTERIOR; }
	if (!g_debugWaterReflectionRenderInterior)        { flags &= ~CULL_SHAPE_FLAG_RENDER_INTERIOR; }
	if (!g_debugWaterReflectionPortalTraversal)       { flags &= ~CULL_SHAPE_FLAG_TRAVERSE_PORTALS; }
#endif // __BANK

	if (BANK_SWITCH(g_debugWaterReflectionUsePoolSphere, true) && g_waterReflectionCurrentClosestPool.GetWf() > 0.0f)
	{
		flags |= CULL_SHAPE_FLAG_WATER_REFLECTION_USE_SEPARATE_SPHERE;
	}

	if (!g_waterReflectionMaxPedDistanceEnabled &&
		!g_waterReflectionMaxVehDistanceEnabled &&
		!g_waterReflectionMaxObjDistanceEnabled)
	{
		flags &= ~CULL_SHAPE_FLAG_USE_MAX_PED_VEH_OBJ_DISTANCES; // provide a way to turn this off for debugging .. normally we want it enabled though
	}

	if (g_waterReflectionLODRangeCurrentEnabled)
	{
		flags |= CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR;
	}

	if (g_waterReflectionFarClipDistance <= 0.01f && BANK_SWITCH(g_debugWaterReflectionRenderProxiesWhenFarClipIsZero, true))
	{
		flags |= CULL_SHAPE_FLAG_WATER_REFLECTION_PROXIES_ONLY;
	}

	return flags;
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::IsWaterReflectionEnabled()
{
	return g_waterReflectionEnabled;
}

static rstScanWaterReflectionParamsOut g_rstScanOut;

// This code used to live in SetCullShape (actually it originally lived in UpdateViewport. We need
// to access the stencil data in order to construct the cull shape, however this is problematic
// because we don't want to have to wait on the stencilwriter before kicking off the visibility job.
// So for now, I'll call this separately .. eventually we might need to move this code to the SPU.
__COMMENT(static) void CRenderPhaseWaterReflectionInterface::SetCullShapeFromStencil(bool bRunOnSPU)
{
	const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();
#if RASTER
	const int w = 128; // width of stencil buffer
	const int h = 72; // height of stencil buffer

	Vec3V test[3];

	if (g_debugWaterReflectionRaster)
	{
		if (!CRasterInterface::IsInited())
		{
			CRasterInterface::Init(&gameVP, w, h);
		}

		CRasterInterface::Clear(&gameVP);

		if (g_debugWaterReflectionRasteriseTest ||
			g_debugWaterReflectionRasteriseTestStencil)
		{
			// screenspace points
			Vec3V p0 = Vec3V(g_debugWaterReflectionRasteriseTestP[0] + g_debugWaterReflectionRasteriseTestOffset, g_debugWaterReflectionRasteriseTestZ[0]);
			Vec3V p1 = Vec3V(g_debugWaterReflectionRasteriseTestP[1] + g_debugWaterReflectionRasteriseTestOffset, g_debugWaterReflectionRasteriseTestZ[1]);
			Vec3V p2 = Vec3V(g_debugWaterReflectionRasteriseTestP[2] + g_debugWaterReflectionRasteriseTestOffset, g_debugWaterReflectionRasteriseTestZ[2]);

			if (g_debugWaterReflectionRasteriseTestAngle != 0.0f)
			{
				const Vec3V centroid = (p0 + p1 + p2)*ScalarVConstant<FLOAT_TO_INT(1.0f/3.0f)>();

				Mat33V rot;
				rot.SetCol0(Vec3V(cosf(g_debugWaterReflectionRasteriseTestAngle*DtoR), -sinf(g_debugWaterReflectionRasteriseTestAngle*DtoR), 0.0f));
				rot.SetCol1(Vec3V(sinf(g_debugWaterReflectionRasteriseTestAngle*DtoR), +cosf(g_debugWaterReflectionRasteriseTestAngle*DtoR), 0.0f));
				rot.SetCol2(Vec3V(V_Z_AXIS_WZERO));

				p0 = Multiply(rot, p0 - centroid) + centroid;
				p1 = Multiply(rot, p1 - centroid) + centroid;
				p2 = Multiply(rot, p2 - centroid) + centroid;
			}

			const Vec3V axisY(sinf(g_debugWaterReflectionRasteriseTestAngle*DtoR), +cosf(g_debugWaterReflectionRasteriseTestAngle*DtoR), 0.0f);

			const Vec2V scale(+2.0f/(float)w, -2.0f/(float)h);
			const Vec2V offset(-1.0f, +1.0f);

			const Vec2V screen0 = AddScaled(offset, scale, p0.GetXY());
			const Vec2V screen1 = AddScaled(offset, scale, p1.GetXY());
			const Vec2V screen2 = AddScaled(offset, scale, p2.GetXY());

			Mat44V invCompMat;
			InvertFull(invCompMat, gameVP.GetCompositeMtx());

			test[0] = TransformProjective(invCompMat, Vec3V(screen0, ShaderUtil::GetSampleDepth(&gameVP, p0.GetZ())));
			test[1] = TransformProjective(invCompMat, Vec3V(screen1, ShaderUtil::GetSampleDepth(&gameVP, p1.GetZ())));
			test[2] = TransformProjective(invCompMat, Vec3V(screen2, ShaderUtil::GetSampleDepth(&gameVP, p2.GetZ())));

			if (g_debugWaterReflectionRasteriseTestStencil)
			{
				CRasterInterface::RasteriseStencilPoly(test, NELEM(test), g_debugWaterReflectionRasteriseTestViewportClip);
			}
			else
			{
				CRasterInterface::RasteriseOccludePoly(test, NELEM(test), g_debugWaterReflectionRasteriseTestViewportClip);
			}
		}

		CRasterInterface::RasteriseOccludersAndWater(
			g_debugWaterReflectionRasteriseOcclBoxes,
			g_debugWaterReflectionRasteriseOcclMeshes,
			g_debugWaterReflectionRasteriseOcclWater
		);
	}

	bool bShowInfo = g_debugWaterReflectionShowInfo || CRenderTargets::IsShowingRenderTargetWaterDraw();

	if (CRasterInterface::GetControlRef().m_displayOpacity > 0.0f)
	{
		bShowInfo = true;
	}

	if (bShowInfo)
	{
		if (!CRasterInterface::IsInited())
		{
			CRasterInterface::Init(&gameVP, w, h);
		}

		const int stencilStride = (w + 63)/64;
		const u64* stencil = CRasterInterface::GetStencilData();
		int ones = 0;
		int zeros = 0;

		for (int j = 0; j < h; j++)
		{
			const u64* stencilRow = &stencil[j*stencilStride];

			for (int i = 0; i < w; i++)
			{
				if (stencilRow[i/64] & BIT64(63 - i%64)) { ones++; }
				else                                     { zeros++; }
			}
		}

		const int waterPixels = Water::GetWaterPixelsRenderedPlanarReflective(true);

		if (waterPixels > 0 && ones == 0)
		{
			g_waterReflectionStencilDebugMsgColour = Color32(255,0,0,255); // error - water pixels are visible but stencil is empty
		}
		else if (waterPixels == 0 && ones > 0)
		{
			g_waterReflectionStencilDebugMsgColour = Color32(255,128,0,255); // warning - water is not visible but stencil is not empty
		}
		else if (waterPixels == 0 && ones == 0)
		{
			g_waterReflectionStencilDebugMsgColour = Color32(128,128,128,255); // water is completely not visible, and should be disabled ..
		}
		else
		{
			g_waterReflectionStencilDebugMsgColour = Color32(0.9f, 0.9f, 0.9f); // default colour from debugdraw.cpp
		}

		sprintf(g_waterReflectionStencilDebugMsg, "water stencil data: %d ones, %d zeros (%d water pixels)", ones, zeros, waterPixels);
	}
	else
	{
		strcpy(g_waterReflectionStencilDebugMsg, "");
		g_waterReflectionStencilDebugMsgColour = Color32(0);
	}

	if (CRasterInterface::GetControlRef().m_displayOpacity > 0.0f)
	{
		CRasterInterface::DebugDraw();
		CRasterInterface::DebugDrawOccludersAndWater(
			g_debugWaterReflectionRasteriseOcclBoxes,
			g_debugWaterReflectionRasteriseOcclMeshes,
			g_debugWaterReflectionRasteriseOcclWater,
			g_debugWaterReflectionRasteriseOcclGeomOpacity
		);
	}

	if (g_debugWaterReflectionRasteriseTest ||
		g_debugWaterReflectionRasteriseTestStencil)
	{
		grcDebugDraw::Poly(test[0], test[1], test[2], Color32(255,0,0,64), true, true);
	}

	CRasterInterface::DumpOccludersToOBJ_Update(g_debugWaterReflectionDumpOccludersWaterOnly);
#endif // RASTER

	const Vec3V camPos = gameVP.GetCameraPosition();
	const float camPosZ = camPos.GetZf();

	const float dh = camPosZ - g_waterReflectionCurrentWaterHeight;

	if (camPosZ <= 20.0f && // only do this if we're close to the ocean, not a lake .. lakes don't have huge waves
		dh <= BANK_SWITCH(g_debugWaterReflectionDisableOcclusionThreshold, WATER_REFLECTION_DEFAULT_DISABLE_OCCLUSION_THRESHOLD) &&
		BANK_SWITCH(g_debugWaterReflectionDisableOcclusionCloseToOcean, true))
	{
		g_waterReflectionOcclusionDisabledForOceanWaves = true;
	}
	else if (dh < 0.01f && BANK_SWITCH(g_debugWaterReflectionDisableOcclusionCloseToOcean, true))
	{
		// also disable occlusion if we're actually under the water height, which can happen on the big lake in some cases (BS#1407370)
		g_waterReflectionOcclusionDisabledForOceanWaves = true;
	}
	else
	{
		g_waterReflectionOcclusionDisabledForOceanWaves = false;
	}

	if (!bRunOnSPU) // run ScanWaterReflection on PPU ..
	{
		rstScanWaterReflectionParamsIn paramsIn;

		paramsIn.m_gameVPCompositeMtx = gameVP.GetCompositeMtx();
		paramsIn.m_cameraPos = camPos;
		paramsIn.m_nearPlane = BuildPlane(camPos, PlaneReflectVector(g_waterReflectionCurrentWaterPlane, gameVP.GetCameraMtx().GetCol2())); // reflected
		paramsIn.m_waterPlane = g_waterReflectionCurrentWaterPlane;
		paramsIn.m_stencil = COcclusion::GetHiStencilBuffer();
		paramsIn.m_controlFlags = 0;

#if __BANK
		if (!g_debugWaterReflectionCullingPlanesEnabled)
		{
			// don't set cull planes on spu (keep whatever we created in SetCullShape)
		}
		else
#endif // __BANK
		if (!g_waterReflectionIsCameraUnderwater && BANK_SWITCH(g_debugWaterReflectionVisBoundsCullPlanes, WATER_REFLECTION_DEFAULT_CULL_PLANES))
		{
			paramsIn.m_controlFlags |= SCAN_WATER_REFLECTION_CONTROL_SET_CULL_PLANES;
		}
		else
		{
			// don't set cull planes on spu (keep whatever we created in SetCullShape)
		}

#if __BANK
		paramsIn.m_mode = g_debugWaterReflectionVisBoundsMode;
		paramsIn.m_debugFlags = 0;

		if (g_debugWaterReflectionRemoveIsolatedStencil)
		{
			paramsIn.m_debugFlags |= SCAN_WATER_REFLECTION_DEBUG_REMOVE_ISOLATED_STENCIL;
		}
#endif // __BANK

		if (BANK_SWITCH(g_debugWaterReflectionVisBoundsCullPlanesWater, true))
		{
			paramsIn.m_controlFlags |= SCAN_WATER_REFLECTION_CONTROL_EXTRA_CULL_PLANE_WATER;
		}

		if (BANK_SWITCH(g_debugWaterReflectionVisBoundsCullPlanesNear, WATER_REFLECTION_DEFAULT_CULL_NEAR_PLANE))
		{
			paramsIn.m_controlFlags |= SCAN_WATER_REFLECTION_CONTROL_EXTRA_CULL_PLANE_NEAR;
		}

		ScanWaterReflection(g_rstScanOut, paramsIn);
	}

#if __BANK
	if (g_debugWaterReflectionVisBoundsDraw)
	{
		const Vec2V pointScale(1.0f/128.0f, 1.0f/72.0f);
		Vec2V points[8];
		const int numPoints = g_rstScanOut.m_visBounds.GetUniquePoints(points);

		const Color32 edgeColour = (g_waterReflectionFarClipDistance <= 0.01f) ? Color32(0,0,255,32) : Color32(255,0,0,32);
		const Color32 fillColour = (g_waterReflectionFarClipDistance <= 0.01f) ? Color32(0,0,255,255) : Color32(255,0,0,255);

		for (int i = 2; i < numPoints; i++)
		{
			grcDebugDraw::Poly(pointScale*points[0], pointScale*points[i - 1], pointScale*points[i], edgeColour, true);
		}

		for (int i = 0; i < numPoints; i++)
		{
			grcDebugDraw::Line(pointScale*points[i], pointScale*points[(i + 1)%numPoints], fillColour);
		}

		if (g_debugWaterReflectionUsePoolSphere && g_waterReflectionCurrentClosestPool.GetWf() > 0.0f)
		{
			grcDebugDraw::Sphere(g_waterReflectionCurrentClosestPool.GetXYZ(), g_waterReflectionCurrentClosestPool.GetWf(), Color32(255,0,0,255), false, 1, 16, true);
			grcDebugDraw::Sphere(g_waterReflectionCurrentClosestPool.GetXYZ(), g_waterReflectionCurrentClosestPool.GetWf()*g_debugWaterReflectionPoolSphereRadiusScale, Color32(0,0,255,255), false, 1, 16, true);
		}
	}
#endif // __BANK

	if (!g_waterReflectionIsCameraUnderwater && BANK_SWITCH(g_debugWaterReflectionVisBoundsScissor, WATER_REFLECTION_DEFAULT_SCISSOR))
	{
		const Vec2V pointScale(1.0f/128.0f, 1.0f/72.0f);
		const Vec2V boundsMin = pointScale*g_rstScanOut.m_visBounds.GetMin();
		const Vec2V boundsMax = pointScale*g_rstScanOut.m_visBounds.GetMax();

		if (IsLessThanAll(boundsMin, boundsMax))
		{
			g_waterReflectionVisBoundsScissorX0 = boundsMin.GetXf();
			g_waterReflectionVisBoundsScissorY0 = boundsMin.GetYf();
			g_waterReflectionVisBoundsScissorX1 = boundsMax.GetXf();
			g_waterReflectionVisBoundsScissorY1 = boundsMax.GetYf();
		}
		else
		{
			g_waterReflectionVisBoundsScissorX0 = 0.0f;
			g_waterReflectionVisBoundsScissorY0 = 0.0f;
			g_waterReflectionVisBoundsScissorX1 = 0.0f;
			g_waterReflectionVisBoundsScissorY1 = 0.0f;
		}
	}
	else
	{
		g_waterReflectionVisBoundsScissorX0 = 0.0f;
		g_waterReflectionVisBoundsScissorY0 = 0.0f;
		g_waterReflectionVisBoundsScissorX1 = 1.0f;
		g_waterReflectionVisBoundsScissorY1 = 1.0f;
	}
}

void CRenderPhaseWaterReflection::SetCullShape(fwRenderPhaseCullShape& cullShape)
{
	if (g_waterReflectionLODRangeCurrentEnabled)
	{
		cullShape.InvalidateLodRanges();

		for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
		{
			if (BANK_SWITCH(g_debugWaterReflectionLODRangeEnabled[i], true))
			{
				cullShape.EnableLodRange(i, g_waterReflectionLODRangeCurrent[i][0], g_waterReflectionLODRangeCurrent[i][1]);
			}
		}
	}

#if __BANK
	if (g_debugWaterReflectionLODMaskOverrideEnabled)
	{
		SetLodMask(g_debugWaterReflectionLODMaskOverride);
	}
	else
#endif // __BANK
	if (BANK_SWITCH(g_debugWaterReflectionLODMaskEnabled, true) && !g_waterReflectionLODRangeCurrentEnabled)
	{
		u8 entityLODMask = 0;

		entityLODMask |= (g_waterReflectionHighestEntityLODCurrentLOD <= LODTYPES_DEPTH_HD   ) ? fwRenderPhase::LODMASK_HD    : 0;
		entityLODMask |= (g_waterReflectionHighestEntityLODCurrentLOD <= LODTYPES_DEPTH_LOD  ) ? fwRenderPhase::LODMASK_LOD   : 0;
		entityLODMask |= (g_waterReflectionHighestEntityLODCurrentLOD <= LODTYPES_DEPTH_SLOD1) ? fwRenderPhase::LODMASK_SLOD1 : 0;
		entityLODMask |= (g_waterReflectionHighestEntityLODCurrentLOD <= LODTYPES_DEPTH_SLOD2) ? fwRenderPhase::LODMASK_SLOD2 : 0;
		entityLODMask |= (g_waterReflectionHighestEntityLODCurrentLOD <= LODTYPES_DEPTH_SLOD3) ? fwRenderPhase::LODMASK_SLOD3 : 0;
		entityLODMask |= (g_waterReflectionHighestEntityLODCurrentLOD <= LODTYPES_DEPTH_SLOD4) ? fwRenderPhase::LODMASK_SLOD4 : 0;

		if (!BANK_SWITCH(g_debugWaterReflectionUseWaterReflectionTechnique, true))
		{
			entityLODMask |= fwRenderPhase::LODMASK_LIGHT;
		}

		SetLodMask(entityLODMask);
	}
	else
	{
		SetLodMask(fwRenderPhase::LODMASK_NONE);
	}

	cullShape.SetMaxPedDistance    (g_waterReflectionMaxPedDistanceEnabled ? g_waterReflectionMaxPedDistance : 16000.0f);
	cullShape.SetMaxVehicleDistance(g_waterReflectionMaxVehDistanceEnabled ? g_waterReflectionMaxVehDistance : 16000.0f);
	cullShape.SetMaxObjectDistance ((g_waterReflectionMaxObjDistanceEnabled && !g_waterReflectionScriptObjectVisibility) ? g_waterReflectionMaxObjDistance : 16000.0f);

	if (BANK_SWITCH(g_debugWaterReflectionUsePoolSphere, true) && g_waterReflectionCurrentClosestPool.GetWf() > 0.0f)
	{
		const spdSphere poolSphere(g_waterReflectionCurrentClosestPool.GetXYZ(), g_waterReflectionCurrentClosestPool.GetW()*ScalarV(BANK_SWITCH(g_debugWaterReflectionPoolSphereRadiusScale, WATER_REFLECTION_DEFAULT_POOL_SPHERE_RADIUS_SCALE)));
		cullShape.SetInteriorSphere(poolSphere);
	}

	// ===============================================================================================

	CRenderPhaseWaterReflectionInterface::SetCullShapeFromStencil();

	// ===============================================================================================

	const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();
	const Vec3V camPos = +gameVP.GetCameraMtx().GetCol3();
	const Vec3V camDir = -gameVP.GetCameraMtx().GetCol2();
	spdTransposedPlaneSet8 frustum;

	Vec4V farClipPlane = Vec4V(V_ZERO);

	if (g_waterReflectionFarClipEnabled)
	{
		farClipPlane = BuildPlane(camPos + camDir*ScalarV(g_waterReflectionFarClipDistance), camDir);
	}

	g_waterReflectionNoStencilPixels = false;

#if __BANK
	if (!g_debugWaterReflectionCullingPlanesEnabled)
	{
#if __PS3
		g_rstScanOut.m_edgeClipPlaneCount = 0;
#endif // __PS3
		sysMemSet(&frustum, 0, sizeof(frustum)); // no cull planes whatsoever
	}
	else
#endif // __BANK
	if (!g_waterReflectionIsCameraUnderwater && BANK_SWITCH(g_debugWaterReflectionVisBoundsCullPlanes, WATER_REFLECTION_DEFAULT_CULL_PLANES))
	{
		if (g_rstScanOut.m_outputFlags & SCAN_WATER_REFLECTION_OUT_NO_STENCIL)
		{
			g_waterReflectionNoStencilPixels = true;
		}

#if __BANK
		if (g_debugWaterReflectionVisBoundsCullPlanesShow)
		{
			for (int i = 0; i < NELEM(g_rstScanOut.m_reflectedPlanes); i++)
			{
				char notes[64] = "";

				if (i == g_rstScanOut.m_debugWaterPlaneIndex) { strcat(notes, " [WATER PLANE]"); }
				if (i == g_rstScanOut.m_debugNearPlaneIndex) { strcat(notes, " [NEAR PLANE]"); }

				grcDebugDraw::AddDebugOutput("water cull plane %d = %f,%f,%f,%f%s", i, VEC4V_ARGS(g_rstScanOut.m_reflectedPlanes[i]), notes);
			}
#if __PS3
			for (int i = 0; i < NELEM(g_rstScanOut.m_edgeClipPlanes); i++)
			{
				grcDebugDraw::AddDebugOutput("water edge plane %d = %f,%f,%f,%f", i, VEC4V_ARGS(g_rstScanOut.m_edgeClipPlanes[i]));
			}
#endif // __PS3
		}
#endif // __BANK

		// ====================================================================================
		// Note that the cull shape frustum planes will be overwritten on the spu _iff_ we're
		// running through spu codepath (or more generally a parallel visibility task), in this
		// case g_rstScanOut will be a frame late here but that won't matter since the planes
		// will be overwritten .. if we're not running through spu codepath, then g_rstScanOut
		// will not be a frame late, so we're ok in this case too.
		//
		// If we're running through spu codepath, we need to do something like this on spu:
		// 
		//	spdTransposedPlaneSet8 cullShapeFrustum = ...;
		//	if (paramsOut.m_controlFlags & SCAN_WATER_REFLECTION_CONTROL_SET_CULL_PLANES)
		//	{
		//		cullShapeFrustum.SetPlanes(paramsOut.m_reflectedPlanes);
		//	}
		//	else
		//	{
		//		// don't set frustum, keep whatever we created in SetCullShape
		//	}
		// ====================================================================================
		frustum.SetPlanes(g_rstScanOut.m_reflectedPlanes);
	}
	else
	{
#if __PS3
		g_rstScanOut.m_edgeClipPlaneCount = 0;
#endif // __PS3
		frustum.Set(GetGrcViewport(), false, false, farClipPlane);
	}

	cullShape.SetFrustum(frustum);
}

#if __PS3 && EDGE_STENCIL_TEST
const Vec4V* g_EdgeStencilData = NULL;
//DECLARE_PPU_SYMBOL(const rage::Vec4V*, g_EdgeStencilData);
#endif // __PS3 && EDGE_STENCIL_TEST

#if __BANK
static void WaterReflectionShowRT_button();
#endif // __BANK

void CRenderPhaseWaterReflection::UpdateViewport()
{
	g_waterReflectionIsCameraUnderwater = Water::IsCameraUnderwater() && BANK_SWITCH(Water::m_bUnderwaterPlanarReflection, true);

#if __BANK
	g_debugWaterReflectionIsUsingMirrorWaterSurface = Water::IsUsingMirrorWaterSurface(); // cached for rendertarget viewer (one frame late, but shouldn't matter)

	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
	{
		WaterReflectionShowRT_button();
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
	{
#if RASTER && RDR_VERSION
		float& stencilOpacity = CRasterInterface::GetControlRef().m_displayOpacity;

		if (stencilOpacity == 0.0f)
		{
			stencilOpacity = 0.5f;
			g_debugWaterReflectionVisBoundsDraw = true;
		}
		else
		{
			stencilOpacity = 0.0f;
			g_debugWaterReflectionVisBoundsDraw = false;
		}
#else
		COcclusion::ToggleGBUFStencilOverlay();
		g_debugWaterReflectionVisBoundsDraw = COcclusion::IsGBUFStencilOverlayEnabled();
#endif
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT))
	{
		bool& bOverride = CPostScanDebug::GetOverrideGBufBitsEnabledRef();

		bOverride = !bOverride;

		if (bOverride)
		{
			if (fwScan::GetScanResults().GetWaterSurfaceVisible())
			{
				CPostScanDebug::SetOverrideGBufBits("Mirror Reflection");
			}
			else
			{
				CPostScanDebug::SetOverrideGBufBits("Water Reflection");
			}
		}
	}

	// cycle reflection source
	{
		static const char* message = "";
		static int messageTimer = 0;

		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT))
		{
			switch (Water::m_debugReflectionSource)
			{
			case Water::WATER_REFLECTION_SOURCE_DEFAULT:
				message = "Water reflection source -> BLACK";
				messageTimer = 60;
				Water::m_debugReflectionSource = Water::WATER_REFLECTION_SOURCE_BLACK;
				g_debugWaterReflectionRenderSkyOnly = false;
#if WATER_SHADER_DEBUG
				Water::m_debugReflectionBrightnessScale = 0.0f;
#endif // WATER_SHADER_DEBUG
				break;
			case Water::WATER_REFLECTION_SOURCE_BLACK:
				message = "Water reflection source -> WHITE";
				messageTimer = 60;
				Water::m_debugReflectionSource = Water::WATER_REFLECTION_SOURCE_WHITE;
				g_debugWaterReflectionRenderSkyOnly = false;
#if WATER_SHADER_DEBUG
				Water::m_debugReflectionBrightnessScale = 16.0f;
#endif // WATER_SHADER_DEBUG
				break;
			case Water::WATER_REFLECTION_SOURCE_WHITE:
				message = "Water reflection source -> SKY ONLY";
				messageTimer = 60;
				Water::m_debugReflectionSource = Water::WATER_REFLECTION_SOURCE_SKY_ONLY;
				g_debugWaterReflectionRenderSkyOnly = true;
#if WATER_SHADER_DEBUG
				Water::m_debugReflectionBrightnessScale = 1.0f;
#endif // WATER_SHADER_DEBUG
				break;
			case Water::WATER_REFLECTION_SOURCE_SKY_ONLY:
				message = "Water reflection source -> DEFAULT";
				messageTimer = 60;
				Water::m_debugReflectionSource = Water::WATER_REFLECTION_SOURCE_DEFAULT;
				g_debugWaterReflectionRenderSkyOnly = false;
#if WATER_SHADER_DEBUG
				Water::m_debugReflectionBrightnessScale = 1.0f;
#endif // WATER_SHADER_DEBUG
				break;
			}
		}

		if (messageTimer > 0)
		{
			grcDebugDraw::AddDebugOutput(Color32(128,128,255,255), message);
			messageTimer--;
		}
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT))
	{
#if WATER_SHADER_DEBUG
		if (Water::m_debugHighlightAmount == 0.0f)
		{
			Water::m_debugHighlightAmount = 1.0f;
		}
		else
		{
			Water::m_debugHighlightAmount = 0.0f;
		}
#endif // WATER_SHADER_DEBUG
	}
#endif // __BANK

	// because this comes before mirror reflection, and it relies on the results of the mirror scan visibility, we need to call this here
	CMirrors::UpdateViewportForScan();

	g_waterReflectionEnabled = IsWaterReflectionEnabled(HasEntityListIndex());

	if (!g_waterReflectionEnabled)
	{
		return;
	}

	SetActive(true);

	CRenderPhaseScanned::UpdateViewport();
	grcViewport& viewport = GetGrcViewport();

	CRenderPhaseWaterReflectionInterface::UpdateDistantLights(viewport.GetCameraPosition());

	if (g_waterReflectionScriptWaterHeightEnabled) // save this so we can use it in BuildDrawList
	{
		g_waterReflectionCurrentWaterHeight = g_waterReflectionScriptWaterHeight;
	}
	else if (CGameWorldWaterHeight::IsEnabled())
	{
		g_waterReflectionCurrentWaterHeight = CGameWorldWaterHeight::GetWaterHeight();
		g_waterReflectionCurrentClosestPool = CGameWorldWaterHeight::GetClosestPoolSphere();
	}
	else // use the height from the water system ..
	{
		g_waterReflectionCurrentWaterHeight = Water::GetWaterLevel();
	}

#if __BANK
	if (g_debugWaterReflectionOverrideWaterHeightEnabled)
	{
		g_waterReflectionCurrentWaterHeight = g_debugWaterReflectionOverrideWaterHeight;
	}
	else
#endif
	if (!Water::IsCameraUnderwater())
	{
		g_waterReflectionCurrentWaterHeight += Clamp<float>(Water::GetAverageWaterSimHeight(), 0.0f, 1.0f);
	}

#if __BANK
	if (g_debugWaterReflectionVisBoundsMode == WRVB_WATER_QUADS ||
		g_debugWaterReflectionVisBoundsMode == WRVB_WATER_QUADS_USE_OCCLUSION)
	{
		Water::PreRenderVisibility_Debug(g_rstScanOut.m_visBounds, viewport, g_debugWaterReflectionVisBoundsMode == WRVB_WATER_QUADS_USE_OCCLUSION, false);
	}
#endif // __BANK

#if __PS3
	// stop the oblique clipping flipping out - stops reflection camera getting too near the water surface
	const float nearClip = BANK_SWITCH(g_debugWaterReflectionNearClip, WATER_REFLECTION_DEFAULT_NEAR_CLIP);

	viewport.SetNearClip(nearClip);
#endif // __PS3

	const float waterHeightOffset = BANK_SWITCH(g_debugWaterReflectionHeightOffset, 0.0f) + (BANK_SWITCH(g_waterReflectionPlaneHeightOffset_TCVAREnabled, true) ? g_waterReflectionPlaneHeightOffset_TCVAR : 0.0f);
	const float waterHeightMax = g_waterReflectionIsCameraUnderwater ? FLT_MAX : viewport.GetCameraPosition().GetZf();
	const float waterHeight = Min<float>(waterHeightMax, g_waterReflectionCurrentWaterHeight) - waterHeightOffset;

	float waterHeightAdjusted = waterHeight;

	if (BANK_SWITCH(g_waterReflectionPlaneHeightOffset_TCVAREnabled, true)) // this controls both height offset and height override for now .. not sure which one of these we'll end up using
	{
		waterHeightAdjusted += (g_waterReflectionPlaneHeightOverride_TCVAR - waterHeightAdjusted)*g_waterReflectionPlaneHeightOverrideAmount_TCVAR;
	}

	Vec4V waterPlane = Vec4V(0.0f, 0.0f, 1.0f, -waterHeightAdjusted);

	if (Water::IsCameraUnderwater())
	{
		waterPlane = -waterPlane;
	}

	g_waterReflectionCurrentWaterPlane = waterPlane; // save water plane (we need it in SetCullShape)

	// reflect
	{
		Mat34V cameraMtx = viewport.GetCameraMtx();

		if (BANK_SWITCH(g_debugWaterReflectionViewportReflectInWaterPlane, true))
		{
			cameraMtx = PlaneReflectMatrix(waterPlane, cameraMtx);
		}

		cameraMtx.SetCol0(-cameraMtx.GetCol0()); // flip x
		viewport.SetCameraMtx(cameraMtx);
	}

	if (BANK_SWITCH(g_debugWaterReflectionViewportSetNormWindow, false))
	{
		const Vector2 vShear = viewport.GetPerspectiveShear(); // store the old shear
		viewport.SetNormWindow(0.0f, 0.0f, 1.0f, 1.0f);
		viewport.SetPerspectiveShear(-vShear.x, vShear.y); // re-apply the shear
	}

#if __PS3 || __BANK
	bool bUseObliqueProjection = false;

#if __PS3
	bUseObliqueProjection = !WATER_REFLECTION_CLIP_IN_PIXEL_SHADER;
#elif __BANK
	if (!g_debugWaterReflectionUseHWClipPlane)
	{
		bUseObliqueProjection = true;
	}

	if (!g_debugWaterReflectionUseObliqueProjection)
	{
		bUseObliqueProjection = false;
	}
#endif // __BANK

	if (bUseObliqueProjection)
	{
		const ScalarV projOffset = ScalarV(BANK_SWITCH(g_debugWaterReflectionObliqueProjectionOffset, V_ZERO));
		const ScalarV projScale  = ScalarV(BANK_SWITCH(g_debugWaterReflectionObliqueProjectionScale, WATER_REFLECTION_DEFAULT_OBLIQUE_PROJ_SCALE));

		viewport.ApplyObliqueProjection((waterPlane + Vec4V(Vec3V(V_ZERO), projOffset))*projScale);
	}
#endif // __PS3 || __BANK

	Water::SetReflectionMatrix(viewport.GetCompositeMtx());

	g_waterReflectionViewport = &viewport;

	// 5/4/13 - cthomas - Set the occlusion transform here for water reflection, after the water reflection's 
	// viewport has been updated. Previously, we were setting it in COcclusion::KickOcclusionJobs(), but that 
	// happens immediately after the camera update, and our water reflection viewport isn't updated until now. 
	// This was causing one-frame pops in the water reflection.
	COcclusion::SetOcclusionTransform( SCAN_OCCLUSION_TRANSFORM_WATER_REFLECTION, *g_waterReflectionViewport );
}

void CRenderPhaseWaterReflection::BuildRenderList()
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

	if (!g_waterReflectionEnabled)
	{
		return;
	}

	PF_FUNC(CRenderPhaseWaterReflectionRL);

#if __BANK
	SetRenderFlags(g_waterReflectionRenderSettings);
#endif // __BANK

	u32 visMaskFlags = g_waterReflectionVisMaskFlags;

	if (g_waterReflectionHighestEntityLODCurrentLOD == LODTYPES_DEPTH_HD)
	{
		visMaskFlags |= VIS_ENTITY_MASK_TREE;
	}

	GetEntityVisibilityMask().SetAllFlags((u16)visMaskFlags);

	COcclusion::BeforeSetupRender(true, false, SHADOWTYPE_NONE, false, GetGrcViewport());
	CScene::SetupRender(GetEntityListIndex(), GetRenderFlags(), GetGrcViewport(), this);
	COcclusion::AfterPreRender();
}

static void RenderStateSetupWaterReflection(fwRenderPassId pass, fwRenderBucket bucket, fwRenderMode, int, fwRenderGeneralFlags)
{
	if (pass == RPASS_CUTOUT && g_waterReflectionBlendStateAlphaTestAlphaRef < 1.f)
	{
		DLC(dlCmdSetBlendState, (grcStateBlock::BS_Default));
		DLC_Add(CShaderLib::SetAlphaTestRef, (g_waterReflectionBlendStateAlphaTestAlphaRef));
	}
	else if (BIT(pass) & g_waterReflectionAlphaBlendingPasses)
	{
		DLC(dlCmdSetBlendState, (grcStateBlock::BS_Normal));
	}
	else
	{
		DLC(dlCmdSetBlendState, (grcStateBlock::BS_Default)); // no alpha blending
	}

	if (BIT(pass) & g_waterReflectionDepthWritingPasses)
	{
		DLC(dlCmdSetDepthStencilState, (grcStateBlock::DSS_LessEqual));
	}
	else
	{
		DLC(dlCmdSetDepthStencilState, (grcStateBlock::DSS_TestOnly_LessEqual)); // no depth writing
	}

	if(pass == RPASS_CUTOUT)
	{
		DLC(dlCmdSetRasterizerState, (g_waterReflectionCullNoneState)); // disable backface culling (for tree BBs)
		DLC(dlCmdShaderFxPushForcedTechnique, (g_waterReflectionCutoutTechniqueGroupId));
	}
	else
	{
		DLC(dlCmdSetRasterizerState, (grcStateBlock::XENON_SWITCH(RS_Default_HalfPixelOffset, RS_Default)));
	}

	u32 rb = CRenderer::RB_MODEL_WATER;

#if __BANK
	if (g_debugWaterReflectionForceDefaultRenderBucket)
	{
		rb = CRenderer::RB_MODEL_DEFAULT;
		DLC_Add(dlCmdNewDrawList::SetCurrDrawListLODFlagShift, LODDrawable::LODFLAG_SHIFT_DEFAULT);
	}
#endif // __BANK

	DLC(dlCmdGrcLightStateSetEnabled, (!BANK_SWITCH(g_debugWaterReflectionUseWaterReflectionTechnique, true)));
	DLC(dlCmdSetBucketsAndRenderMode, (bucket, CRenderer::GenerateSubBucketMask(rb), rmStandard)); // damn lies.
}

void RenderStateCleanupWaterReflection(fwRenderPassId pass, fwRenderBucket bucket, fwRenderMode renderMode, int subphase, fwRenderGeneralFlags generalFlags)
{
	if (pass == RPASS_CUTOUT)
	{
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	}

	CRenderListBuilder::RenderStateCleanupSimpleFlush(pass, bucket, renderMode, subphase, generalFlags);
}


//#if __BANK
static void WaterReflectionSetDirLightAndShadows(bool bEnable, Vec4V_In waterPlane)
{
	Lights::m_lightingGroup.SetDirectionalLightingGlobals(1.0f, bEnable, IsZeroAll(waterPlane) ? NULL : &waterPlane);
	Lights::m_lightingGroup.SetAmbientLightingGlobals();
}
//#endif // __BANK

#if WATER_REFLECTION_PRE_REFLECTED
static u32 SetWaterReflectionClipPlanes(bool bFlipPlane, bool bNoClip);
static void RenderStateSetupCustomFlagsChangedWaterReflection(fwRenderPassId pass, fwRenderBucket DEV_ONLY(bucket), fwRenderMode DEV_ONLY(renderMode), int DEV_ONLY(subphasePass), fwRenderGeneralFlags DEV_ONLY(generalFlags), u16 previousCustomFlags, u16 currentCustomFlags)
{
	const bool bPreReflectedPrev = (previousCustomFlags & fwEntityList::CUSTOM_FLAGS_WATER_REFLECTION_PRE_REFLECTED) != 0;
	const bool bPreReflectedCurr = (currentCustomFlags & fwEntityList::CUSTOM_FLAGS_WATER_REFLECTION_PRE_REFLECTED) != 0;

	// TODO -- put this junk in a function, can be shared by multiple renderphases which use custom flag sorting
#if __DEV
	if (g_debugWaterReflectionShowInfo)
	{
		const char* passStr = "?";
		const char* bucketStr = "?";
		const char* renderModeStr = "?";
		char generalFlagsStr[64] = "";

		switch (pass)
		{
		case RPASS_VISIBLE : passStr = "RPASS_VISIBLE"; break;
		case RPASS_LOD     : passStr = "RPASS_LOD"; break;
		case RPASS_CUTOUT  : passStr = "RPASS_CUTOUT"; break;
		case RPASS_DECAL   : passStr = "RPASS_DECAL"; break;
		case RPASS_FADING  : passStr = "RPASS_FADING"; break;
		case RPASS_ALPHA   : passStr = "RPASS_ALPHA"; break;
		case RPASS_WATER   : passStr = "RPASS_WATER"; break;
		case RPASS_TREE    : passStr = "RPASS_TREE"; break;
		}

		switch (bucket)
		{
		case CRenderer::RB_OPAQUE           : bucketStr = "RB_OPAQUE"; break;
		case CRenderer::RB_ALPHA            : bucketStr = "RB_ALPHA"; break;
		case CRenderer::RB_DECAL            : bucketStr = "RB_DECAL"; break;
		case CRenderer::RB_CUTOUT           : bucketStr = "RB_CUTOUT"; break;
		case CRenderer::RB_NOSPLASH         : bucketStr = "RB_NOSPLASH"; break;
		case CRenderer::RB_NOWATER          : bucketStr = "RB_NOWATER"; break;
		case CRenderer::RB_WATER            : bucketStr = "RB_WATER"; break;
		case CRenderer::RB_DISPL_ALPHA      : bucketStr = "RB_DISPL_ALPHA"; break;
		case CRenderer::RB_MODEL_DEFAULT    : bucketStr = "RB_MODEL_DEFAULT"; break;
		case CRenderer::RB_MODEL_SHADOW     : bucketStr = "RB_MODEL_SHADOW"; break;
		case CRenderer::RB_MODEL_REFLECTION : bucketStr = "RB_MODEL_REFLECTION"; break;
		case CRenderer::RB_MODEL_MIRROR     : bucketStr = "RB_MODEL_MIRROR"; break;
		case CRenderer::RB_MODEL_WATER      : bucketStr = "RB_MODEL_WATER"; break;
		case CRenderer::RB_UNUSED_13        : bucketStr = "RB_UNUSED_13"; break;
		}

		switch (renderMode)
		{
		case rmNIL                : renderModeStr = "rmNIL"; break;
		case rmStandard           : renderModeStr = "rmStandard"; break;
		case rmGBuffer            : renderModeStr = "rmGBuffer"; break;
		case rmSimple             : renderModeStr = "rmSimple"; break;
		case rmSimpleNoFade       : renderModeStr = "rmSimpleNoFade"; break;
		case rmSimpleDistFade     : renderModeStr = "rmSimpleDistFade"; break;
#if HAS_RENDER_MODE_SHADOWS
		case rmShadows            : renderModeStr = "rmShadows"; break;
		case rmShadowsSkinned     : renderModeStr = "rmShadowsSkinned"; break;
#endif // HAS_RENDER_MODE_SHADOWS
		case rmSpecial            : renderModeStr = "rmSpecial"; break;
		case rmSpecialSkinned     : renderModeStr = "rmSpecialSkinned"; break;
		case rmNoLights           : renderModeStr = "rmNoLights"; break;
		case rmDebugMode          : renderModeStr = "rmDebugMode"; break;
		};

		if (generalFlags & RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA) { strcat(generalFlagsStr, ",ssa"); }
		if (generalFlags & RENDERFLAG_CUTOUT_FORCE_CLIP         ) { strcat(generalFlagsStr, ",clip"); }
		if (generalFlags & RENDERFLAG_PUDDLE                    ) { strcat(generalFlagsStr, ",puddle"); }
		if (generalFlags & RENDERFLAG_BEGIN_RENDERPASS_QUERY    ) { strcat(generalFlagsStr, ",begin"); }
		if (generalFlags & RENDERFLAG_END_RENDERPASS_QUERY      ) { strcat(generalFlagsStr, ",end"); }

		grcDebugDraw::AddDebugOutput("water: custom flags changed 0x%04x -> 0x%04x (prereflected = %s), pass=%s, bucket=%s, mode=%s, subphase=%d, flags=%s", previousCustomFlags, currentCustomFlags, bPreReflectedCurr ? "TRUE" : "FALSE", passStr, bucketStr, renderModeStr, subphasePass, generalFlagsStr);
	//	grcDebugDraw::AddDebugOutput("water: custom flags changed 0x%04x -> 0x%04x (prereflected = %s), pass=%d, bucket=%d, mode=%d, subphase=%d, flags=%d", previousCustomFlags, currentCustomFlags, bPreReflectedCurr ? "TRUE" : "FALSE", pass, bucket, renderMode, subphasePass, generalFlags);
	}
#endif // __DEV

	if (bPreReflectedCurr != bPreReflectedPrev)
	{
		CDrawListPrototypeManager::Flush();
		extern CRenderPhaseScanned* g_SceneToGBufferPhaseNew;

		if (bPreReflectedCurr)
		{
			grcViewport viewport = g_SceneToGBufferPhaseNew->GetGrcViewport();
			Mat34V cameraMtx = viewport.GetCameraMtx();

			if (g_waterReflectionIsCameraUnderwater BANK_ONLY(|| g_debugWaterReflectionHidePreReflected))
			{
				// TODO POST GTAV -- this function should return false, indicating that the remaining entities should be skipped (instead of zeroing out the camera matrix)
				cameraMtx.SetCol0(Vec3V(V_ZERO));
				viewport.SetCameraMtx(cameraMtx);
				DLC(dlCmdSetCurrentViewport, (&viewport));
				return;
			}
			else
			{
				cameraMtx.SetCol0(-cameraMtx.GetCol0()); // flip x
				viewport.SetCameraMtx(cameraMtx);
				viewport.SetNearFarClip(g_waterReflectionViewport->GetNearClip(), g_waterReflectionViewport->GetFarClip());
				DLC(dlCmdSetCurrentViewport, (&viewport));
				DLC(dlCmdSetRasterizerState, ((pass == RPASS_CUTOUT) ? g_waterReflectionCullNoneState : g_waterReflectionCullFrontState));
				DLC_Add(grmModel::SetModelCullback, (grmModel::ModelCullbackType)NULL);

				SetWaterReflectionClipPlanes(true, BANK_SWITCH(g_debugWaterReflectionNoPixelOrHWClipPreReflected, true));

				if (BANK_SWITCH(g_debugWaterReflectionDirectionalLightAndShadows, true))
				{
					if (BANK_SWITCH(g_debugWaterReflectionDirLightReflectPreReflected, true))
					{
						DLC_Add(WaterReflectionSetDirLightAndShadows, true, g_waterReflectionCurrentWaterPlane);
					}

					if (BANK_SWITCH(g_debugWaterReflectionDirShadowDisablePreReflected, true))
					{
#if SHADOW_MATRIX_REFLECT
						DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVarsShadowMatrixOnly, g_waterReflectionCurrentWaterPlane);
#else
						DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap, (const grcTexture*)NULL);
#endif
					}
				}
			}
		}
		else // restore reflected viewport
		{
			DLC(dlCmdSetCurrentViewport, (g_waterReflectionViewport));
			DLC(dlCmdSetRasterizerState, ((pass == RPASS_CUTOUT) ? g_waterReflectionCullNoneState : grcStateBlock::XENON_SWITCH(RS_Default_HalfPixelOffset, RS_Default))); // backface culling
			DLC_Add(grmModel::SetModelCullback, (grmModel::ModelCullbackType)grmModel::ModelVisibleWithAABB);

			SetWaterReflectionClipPlanes(false, false);

			if (BANK_SWITCH(g_debugWaterReflectionDirectionalLightAndShadows, true))
			{
				if (BANK_SWITCH(g_debugWaterReflectionDirLightReflectPreReflected, true))
				{
					DLC_Add(WaterReflectionSetDirLightAndShadows, true, Vec4V(V_ZERO));
				}

				if (BANK_SWITCH(g_debugWaterReflectionDirShadowDisablePreReflected, true))
				{
#if SHADOW_MATRIX_REFLECT
					DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVarsShadowMatrixOnly, Vec4V(V_ZERO));
#else
					DLC_Add(CRenderPhaseCascadeShadowsInterface::RestoreSharedShadowMap);
#endif
				}
			}
		}

		if (g_waterReflectionCurrentScissorEnabled) // re-send the scissor, since changing viewports clears it ..
		{
			const int sx = g_waterReflectionCurrentScissorFlippedX0;
			const int sy = g_waterReflectionCurrentScissorY0;
			const int sw = g_waterReflectionCurrentScissorX1 - g_waterReflectionCurrentScissorX0;
			const int sh = g_waterReflectionCurrentScissorY1 - g_waterReflectionCurrentScissorY0;

			DLC(dlCmdGrcDeviceSetScissor, (sx, sy, sw, sh));

#if __BANK && __PS3
			if (g_debugWaterReflectionVisBoundsScissorEdgeOnly)
			{
				class ResetGCMScissorWithoutAffectingEdge { public: static void func()
				{
					GCM_DEBUG(GCM::cellGcmSetScissor(GCM_CONTEXT,0,0,4096,4096));
				}};

				DLC_Add(ResetGCMScissorWithoutAffectingEdge::func);
			}
#endif // __BANK && __PS3
		}
	}
}
#endif // WATER_REFLECTION_PRE_REFLECTED

#if __BANK && __PS3 && EDGE_STENCIL_TEST && RASTER
static void SetEdgeStencilTestEnable(u8 enable)
{
	g_EdgeStencilData = (const Vec4V*)CRasterInterface::GetStencilData(true);
#if SPU_GCM_FIFO
	SPU_SIMPLE_COMMAND(grcDevice__SetEdgeStencilTestEnable, enable);
#endif // SPU_GCM_FIFO
}
#endif // __BANK && __PS3 && EDGE_STENCIL_TEST && RASTER

static u32 SetWaterReflectionClipPlanes(bool bFlipPlane, bool bNoClip)
{
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
	if (bNoClip BANK_ONLY(|| !g_debugWaterReflectionUseClipPlaneInPixelShader))
	{
		DLC(dlCmdSetGlobalVar_V4, (g_waterReflectionClipPlaneVarId, Vec4V(V_ZERO)));
	}
	else
	{
		Vec4V waterClipPlane = g_waterReflectionCurrentWaterPlane;

		if (bFlipPlane)
		{
			waterClipPlane = -waterClipPlane;
		}

#if __BANK
		if (g_debugWaterReflectionClipPlaneOffsetEnabled)
		{
			waterClipPlane += Vec4V(0.0f, 0.0f, 0.0f, -g_debugWaterReflectionClipPlaneOffset);
		}
#endif // __BANK

		DLC(dlCmdSetGlobalVar_V4, (g_waterReflectionClipPlaneVarId, CMirrors::GetAdjustedClipPlaneForInteriorWater(waterClipPlane)));
	}
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER

#if __PS3

	if (BANK_SWITCH(g_debugWaterReflectionVisBoundsClipPlanesEdge, true) && g_rstScanOut.m_edgeClipPlaneCount > 0)
	{
		u32 clipPlaneMask = 0;

		if (bFlipPlane)
		{
			// don't use edge clip planes if we're flipped, they aren't valid ..
		}
		else
		{
			for (int i = 0; i < NELEM(g_rstScanOut.m_edgeClipPlanes); i++)
			{
				DLC(dlCmdSetClipPlane, (i, -g_rstScanOut.m_edgeClipPlanes[i])); // expects inwards-facing planes
			}

			clipPlaneMask = (u8)(BIT(NELEM(g_rstScanOut.m_edgeClipPlanes)) - 1);
		}

		DLC(dlCmdSetClipPlaneEnable, (clipPlaneMask));
		return clipPlaneMask;
	}

#else // not __PS3

	const bool bUseHWClipPlane = BANK_SWITCH(g_debugWaterReflectionUseHWClipPlane, true);

	if (bUseHWClipPlane && !bNoClip)
	{
		Vec4V waterClipPlane = g_waterReflectionCurrentWaterPlane;

		if (bFlipPlane)
		{
			waterClipPlane = -waterClipPlane;
		}

#if __BANK
		if (g_debugWaterReflectionClipPlaneOffsetEnabled)
		{
			waterClipPlane += Vec4V(0.0f, 0.0f, 0.0f, -g_debugWaterReflectionClipPlaneOffset);
		}
#endif // __BANK

		DLC(dlCmdSetClipPlane, (0, CMirrors::GetAdjustedClipPlaneForInteriorWater(waterClipPlane)));
		DLC(dlCmdSetClipPlaneEnable, (0x1));
		return 0x1;
	}
	else
	{
		DLC(dlCmdSetClipPlaneEnable, (0x0));
	}

#endif // not __PS3

	return 0;
}

#if RSG_PC
grcRenderTarget* CRenderPhaseWaterReflection::GetDepthReadOnly()
{
	/* Created in viewportmanager.cpp
	if ((m_depthReadOnly == NULL) && GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 
		m_depthReadOnly = textureFactory11.CreateRenderTarget("WATER_REFLECTION_DEPTH", m_depth->GetTexturePtr(), grctfD32FS8, DepthStencilReadOnly);
	}
	*/
	return m_depthReadOnly;
}

grcRenderTarget* CRenderPhaseWaterReflection::GetDepthCopy()
{
	/* Created in viewportmanager.cpp
	if ((m_depthCopy == NULL) && !GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		m_depthCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "WATER_REFLECTION_DEPTH_COPY",	grcrtDepthBuffer,	m_depth->GetWidth(), m_depth->GetHeight(), 32);
	}
	*/
	return m_depthCopy;
}
#endif

void CRenderPhaseWaterReflection::BuildDrawList()											
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
		return;
#endif // __BANK

	g_TestWaterReflectionOn = false;

	if (!g_waterReflectionEnabled)
		return;

	//Cannot do this in update viewport since I don't have occlusion query data yet
	if (!Water::UsePlanarReflection())
		return;

	//Cannot do this in update viewport since I don't have mirror visibility data yet
	if(Water::IsUsingMirrorWaterSurface())
		return;


	g_TestWaterReflectionOn = true;

#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif // MULTIPLE_RENDER_THREADS

#if __BANK
	bool bShowInfo = g_debugWaterReflectionShowInfo || CRenderTargets::IsShowingRenderTargetWaterDraw();

#if RASTER
	if (CRasterInterface::GetControlRef().m_displayOpacity > 0.0f)
	{
		bShowInfo = true;
	}
#endif // RASTER
#endif // __BANK

	if (BANK_SWITCH(g_debugWaterReflectionRenderSkyBackground, true))
	{
		const Vec3V skyZenithTransitionColour = g_timeCycle.GetCurrUpdateKeyframe().GetVarV3(TCVAR_SKY_ZENITH_TRANSITION_COL_R);
		const float skyZenithTransitionInten  = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SKY_ZENITH_TRANSITION_COL_INTEN);
		const float skyHDR                    = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SKY_HDR);
		const float skyHDRScale               = BANK_SWITCH(g_debugWaterReflectionRenderSkyBackgroundHDRScale, 1.0f);

		const Vec4V skyColour(skyZenithTransitionColour*skyZenithTransitionColour*ScalarV(skyZenithTransitionInten*skyHDR*skyHDRScale), ScalarV(V_ZERO));

		g_waterReflectionSkyColour = skyColour;
	}

	g_waterReflectionHighestEntityLOD_TCVAR = (int)(0.5f + g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD));
	g_waterReflectionSkyFLODRange_TCVAR                  = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_SKY_FLOD_RANGE);
	g_waterReflectionLODRangeEnabled_TCVAR               = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_ENABLED) > 0.5f;

	// [HACK GTAV] BS#1571266 - disable water reflection LOD range for cutscene 'fbi_5_int'
	if (CutSceneManager::GetInstance() &&
		CutSceneManager::GetInstance()->IsRunning() &&
		CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == ATSTRINGHASH("fbi_5_int", 0xEC10B50D))
	{
		g_waterReflectionLODRangeEnabled_TCVAR = false;

#if __BANK
		if (g_debugWaterReflectionShowInfo)
		{
			grcDebugDraw::AddDebugOutput("WATER REFLECTION - DISABLING LOD RANGE FOR CUTSCENE");
		}
#endif // __BANK
	}

	if (g_waterReflectionLODRangeEnabled_TCVAR)
	{
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_HD      ][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_HD_START);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_ORPHANHD][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_ORPHANHD_START);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_LOD     ][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_LOD_START);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD1   ][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD1_START);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD2   ][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD2_START);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD3   ][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD3_START);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD4   ][0] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD4_START);

		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_HD      ][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_HD_END);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_ORPHANHD][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_ORPHANHD_END);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_LOD     ][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_LOD_END);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD1   ][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD1_END);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD2   ][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD2_END);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD3   ][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD3_END);
		g_waterReflectionLODRange_TCVAR[LODTYPES_DEPTH_SLOD4   ][1] = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_LOD_RANGE_SLOD4_END);
	}

	int entityLOD = Max<int>(g_waterReflectionHighestEntityLOD, g_waterReflectionHighestEntityLOD_TCVAR BANK_ONLY(*(g_waterReflectionHighestEntityLOD_TCVAREnabled ? 1 : 0)));
	const int LODs[] =
	{
		LODTYPES_DEPTH_LOD, // default (cull HD non-proxies)
		LODTYPES_DEPTH_HD,  // allow everything including HD
		LODTYPES_DEPTH_LOD, // same as default
		LODTYPES_DEPTH_SLOD1,
		LODTYPES_DEPTH_SLOD2,
		LODTYPES_DEPTH_SLOD3,
		LODTYPES_DEPTH_SLOD4,
	};
	const int sizeLODs = sizeof(LODs)/sizeof(int);
	entityLOD = (entityLOD<sizeLODs)? entityLOD : 0; // fix for BS#6198302 - Retail Crash in CRenderPhaseWaterReflection::BuildDrawList();

	g_waterReflectionHighestEntityLODCurrentLOD = LODs[entityLOD];

#if __BANK
	strcpy(g_waterReflectionHighestEntityLODCurrentLODStr, fwLodData::ms_apszLevelNames[g_waterReflectionHighestEntityLODCurrentLOD]);

	if (g_debugWaterReflectionLODRangeMode == WATER_REFLECTION_TIMECYCLE_MODE_OVERRIDE)
	{
		g_waterReflectionLODRangeCurrentEnabled = true; // yes we'll be using g_waterReflectionLODRange (from rag)

		for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
		{
			g_waterReflectionLODRangeCurrent[i][0] = g_debugWaterReflectionLODRange[i][0];
			g_waterReflectionLODRangeCurrent[i][1] = g_debugWaterReflectionLODRange[i][1];
		}
	}
	else
#endif // __BANK
	if (g_waterReflectionLODRangeEnabled_TCVAR BANK_ONLY(&& g_debugWaterReflectionLODRangeMode == WATER_REFLECTION_TIMECYCLE_MODE_TIMECYCLE))
	{
		g_waterReflectionLODRangeCurrentEnabled = true; // yes we'll be using g_waterReflectionLODRangeCurrent (from timecycle)

		for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
		{
			g_waterReflectionLODRangeCurrent[i][0] = g_waterReflectionLODRange_TCVAR[i][0];
			g_waterReflectionLODRangeCurrent[i][1] = g_waterReflectionLODRange_TCVAR[i][1];
		}
	}
	else
	{
		g_waterReflectionLODRangeCurrentEnabled = false;
	}

	g_waterReflectionPlaneHeightOffset_TCVAR         = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_HEIGHT_OFFSET);
	g_waterReflectionPlaneHeightOverride_TCVAR       = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE);
	g_waterReflectionPlaneHeightOverrideAmount_TCVAR = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE_AMOUNT);
	g_waterReflectionDistLightIntensity_TCVAR        = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_DISTANT_LIGHT_INTENSITY);
	g_waterReflectionCoronaIntensity_TCVAR           = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_CORONA_INTENSITY);

	if (!g_waterReflectionFarClipOverride)
	{
		g_waterReflectionFarClipDistance = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_REFLECTION_FAR_CLIP);

		// [HACK GTAV] BS#1571266 - override water reflection far clip for cutscene 'fbi_5_int'
		if (CutSceneManager::GetInstance() &&
			CutSceneManager::GetInstance()->IsRunning() &&
			CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == ATSTRINGHASH("fbi_5_int", 0xEC10B50D))
		{
			g_waterReflectionFarClipDistance = 7000.0f;

#if __BANK
			if (g_debugWaterReflectionShowInfo)
			{
				grcDebugDraw::AddDebugOutput("WATER REFLECTION - OVERRIDDING FAR CLIP FOR CUTSCENE");
			}
#endif // __BANK
		}

#if __BANK
		if (Abs<float>(g_waterReflectionFarClipDistance - 7000.0f) <= 0.01f)
		{
			g_waterReflectionFarClipDistance = 7000.0f; // snap to 7000 to prevent jittery timecycle from affecting rag (not sure if this is still necessary)
		}
#endif // __BANK
	}

	bool bSkyOnly = BANK_SWITCH(g_debugWaterReflectionRenderSkyOnly, false);

	if (g_waterReflectionFarClipDistance <= 0.01f)
	{
		bSkyOnly = bSkyOnly || !BANK_SWITCH(g_debugWaterReflectionRenderProxiesWhenFarClipIsZero, true);

#if __BANK
		if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("water reflection - rendering %s-only because far clip = %f", bSkyOnly ? "sky" : "proxies", g_waterReflectionFarClipDistance);
		}
#endif // __BANK
	}
	else if (g_waterReflectionNoStencilPixels)
	{
		bSkyOnly = true;

#if __BANK
		if (bShowInfo)
		{
			grcDebugDraw::AddDebugOutput("water reflection - rendering sky-only because no stencil pixels are set");
		}
#endif // __BANK
	}

	PS3_ONLY(CBubbleFences::WaitForBubbleFenceDLC(CBubbleFences::BFENCE_GBUFFER));

	// don't use prototype - not sure how this works, and it's causing the water reflection to go bonkers
	const bool bOldProtoTypeBatchVal = g_prototype_batch;
#if __BANK
	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	g_prototype_batch = CRenderPhaseWaterReflectionInterface::DrawListPrototypeEnabled() && (renderMode != rmDebugMode);
#else
	g_prototype_batch = false;
#endif

	DrawListPrologue();

	DLC_Add(CShaderLib::SetUseFogRay, false);
	DLC_Add(CShaderLib::SetFogRayTexture);

	const bool bUseScissor = !g_waterReflectionIsCameraUnderwater && BANK_SWITCH(g_debugWaterReflectionVisBoundsScissor, WATER_REFLECTION_DEFAULT_SCISSOR);

	// compute current scissor ..
	if (bUseScissor)
	{
		const int w = m_color->GetWidth();
		const int h = m_color->GetHeight();
#if __BANK
		if (g_debugWaterReflectionVisBoundsScissorOverride)
		{
			g_waterReflectionCurrentScissorX0 = Clamp<int>(g_debugWaterReflectionVisBoundsScissorX0, 0, w - 1);
			g_waterReflectionCurrentScissorY0 = Clamp<int>(g_debugWaterReflectionVisBoundsScissorY0, 0, h - 1);
			g_waterReflectionCurrentScissorX1 = Clamp<int>(g_debugWaterReflectionVisBoundsScissorX1, g_waterReflectionCurrentScissorX0, w);
			g_waterReflectionCurrentScissorY1 = Clamp<int>(g_debugWaterReflectionVisBoundsScissorY1, g_waterReflectionCurrentScissorY0, h);
		}
		else
#endif // __BANK
		{
			g_waterReflectionCurrentScissorX0 = Clamp<int>((int)floorf(g_waterReflectionVisBoundsScissorX0*(float)w) - BANK_SWITCH(g_debugWaterReflectionVisBoundsScissorSkyExpand  , WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND   ), 0, w - 1);
			g_waterReflectionCurrentScissorY0 = Clamp<int>((int)floorf(g_waterReflectionVisBoundsScissorY0*(float)h) - BANK_SWITCH(g_debugWaterReflectionVisBoundsScissorSkyExpandY0, WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND_Y0), 0, h - 1);
			g_waterReflectionCurrentScissorX1 = Clamp<int>((int)ceilf (g_waterReflectionVisBoundsScissorX1*(float)w) + BANK_SWITCH(g_debugWaterReflectionVisBoundsScissorSkyExpand  , WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND   ), g_waterReflectionCurrentScissorX0, w);
			g_waterReflectionCurrentScissorY1 = Clamp<int>((int)ceilf (g_waterReflectionVisBoundsScissorY1*(float)h) + BANK_SWITCH(g_debugWaterReflectionVisBoundsScissorSkyExpand  , WATER_REFLECTION_DEFAULT_SCISSOR_SKY_EXPAND   ), g_waterReflectionCurrentScissorY0, h);
		}

		// flip horizontally
		g_waterReflectionCurrentScissorFlippedX0 = w - g_waterReflectionCurrentScissorX1;
		g_waterReflectionCurrentScissorEnabled = true;
	}
	else
	{
		g_waterReflectionCurrentScissorEnabled = false;
	}

	grcRenderTarget* dst = m_color;

#if RSG_PC
	dst = m_MSAAcolor ? m_MSAAcolor : dst;
#endif

#if __PS3
	if (m_msaacolor)
	{
		dst = m_msaacolor;
	}
#endif // __PS3

	Assertf(dst->GetMSAA() == m_depth->GetMSAA(), "colourRT MSAA=%d, depthRT MSAA=%d", (int)dst->GetMSAA(), (int)m_depth->GetMSAA());
	DLC(dlCmdLockRenderTarget, (0, dst, m_depth));
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

	PF_FUNC(CRenderPhaseWaterReflection);

	DLC_Add(CTimeCycle::SetShaderParams);

	XENON_ONLY(DLC_Add(dlCmdSetHiZ, false, true));

#if __XENON
	DLC_Add(PostFX::SetLDR10bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR8bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

	const Vec4V clearColour  = BANK_SWITCH(g_debugWaterReflectionRenderSkyBackground, true) ? g_waterReflectionSkyColour : BANK_SWITCH_NT(g_debugWaterReflectionClearColour, Color32(0)).GetRGBA();
	const float clearDepth   = 1.0f;
	const u32   clearStencil = DEFERRED_MATERIAL_MOTIONBLUR|DEFERRED_MATERIAL_CLEAR; // Added clear Material so clouds can render. I'm not sure why DEFERRED_MATERIAL_MOTIONBLUR is needed, but it was here before I added the Clear Material...

	const float ox = BANK_SWITCH(g_debugWaterReflectionRenderSkyBackgroundOffsetX, WATER_REFLECTION_DEFAULT_RENDER_SKY_OFFSET_X)/(float)m_color->GetWidth();
	const float oy = BANK_SWITCH(g_debugWaterReflectionRenderSkyBackgroundOffsetY, WATER_REFLECTION_DEFAULT_RENDER_SKY_OFFSET_Y)/(float)m_color->GetHeight();

	class WaterReflectionRenderSkyBackground { public: static void func(Vec4V_In clearColour, float clearDepth, u32 clearStencil, float ox, float oy)
	{
		const float toneMap = BANK_SWITCH(g_debugWaterReflectionRenderSkyBackground && g_debugWaterReflectionRenderSkyBackgroundToneMapped, true) ? (1.0f / 32.0f) : 1.0f;
		const bool bClearColour = BANK_SWITCH(!g_debugWaterReflectionRenderSky || g_debugWaterReflectionClearColourAlways || g_debugWaterReflectionRenderSkyBackground, true);
		const Vec4V clearColourToneMapped = Sqrt(Sqrt(clearColour*ScalarV(toneMap)));

		if (bClearColour && BANK_SWITCH(g_debugWaterReflectionRenderSkyBackgroundWithShader, WATER_REFLECTION_DEFAULT_RENDER_SKY_BACKGROUND_WITH_SHADER))
		{
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
			grcStateBlock::SetDepthStencilState(g_waterReflectionDepthStencilClear, clearStencil);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

			CSprite2d::SetGeneralParams(RCC_VECTOR4(clearColourToneMapped), ShaderUtil::CalculateProjectionParams());
			CSprite2d sprite;
			sprite.BeginCustomList(CSprite2d::CS_BLIT_PARABOLOID, CRenderPhaseReflection::GetRenderTargetColor());
			grcDrawSingleQuadf(-1.0f + ox, 1.0f + oy, 1.0f + ox, -1.0f + oy, clearDepth, 0.0f, 0.0f, 0.0f, 0.0f, Color32(0));
			sprite.EndCustomList();
		}
		else
		{
			GRCDEVICE.Clear(
				bClearColour,
				Color32(clearColourToneMapped),
				true,
				clearDepth,
				true,
				clearStencil
			);
		}
	}};

	DLC_Add(WaterReflectionRenderSkyBackground::func, clearColour, clearDepth, clearStencil, ox, oy);

	float skyFLODRange = g_waterReflectionSkyFLODRange_TCVAR;

#if __BANK
	if (g_debugWaterReflectionSkyFLODMode == WATER_REFLECTION_TIMECYCLE_MODE_DISABLED)
	{
		skyFLODRange = 0.0f;
	}
	else if (g_debugWaterReflectionSkyFLODMode == WATER_REFLECTION_TIMECYCLE_MODE_OVERRIDE)
	{
		skyFLODRange = g_debugWaterReflectionSkyFLODRangeOverride;
	}
#endif // __BANK

	if (skyFLODRange > 0.0f)
	{
		const int sx = BANK_SWITCH(g_debugWaterReflectionVisBoundsScissorSky, true) ? g_waterReflectionCurrentScissorFlippedX0 : -1; // pass -1 to not scissor the sky
		const int sy = g_waterReflectionCurrentScissorY0;
		const int sw = g_waterReflectionCurrentScissorX1 - g_waterReflectionCurrentScissorX0;
		const int sh = g_waterReflectionCurrentScissorY1 - g_waterReflectionCurrentScissorY0;

		DLC_Add(CHorizonObjects::RenderEx, skyFLODRange, false, true, sx, sy, sw, sh);
	}

	if (GetRenderFlags() & RENDER_SETTING_RENDER_SHADOWS)
	{
		DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	}

	DLC_Add(Lights::SetupDirectionalAndAmbientGlobals);
	DLC_Add(CShaderLib::ApplyDayNightBalance);

	DLC_Add(CShaderLib::SetFogRayTexture);

#if __BANK
	if (g_debugWaterReflectionWireframeOverride)
	{
		DLC_Add(grcStateBlock::SetWireframeOverride, true);
	}
#endif // __BANK

	if (BANK_SWITCH(g_debugWaterReflectionRenderSky, true))
	{
		const bool bSkyScissorEnabled = g_waterReflectionCurrentScissorEnabled BANK_ONLY(&& g_debugWaterReflectionVisBoundsScissorSky);

		const int sx = bSkyScissorEnabled ? g_waterReflectionCurrentScissorFlippedX0 : -1; // pass -1 to not scissor the sky
		const int sy = g_waterReflectionCurrentScissorY0;
		const int sw = g_waterReflectionCurrentScissorX1 - g_waterReflectionCurrentScissorX0;
		const int sh = g_waterReflectionCurrentScissorY1 - g_waterReflectionCurrentScissorY0;

		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, false, m_depth, GetDepthCopy(), GetDepthReadOnly()));
		DLC_Add(CVisualEffects::RenderSky, CVisualEffects::RM_WATER_REFLECTION, false, false, sx, sy, sw, sh, true, 1.0f);
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, false, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL));
	}

	if (!bSkyOnly)
	{
		if (g_waterReflectionCurrentScissorEnabled)
		{
			const int sx = g_waterReflectionCurrentScissorFlippedX0;
			const int sy = g_waterReflectionCurrentScissorY0;
			const int sw = g_waterReflectionCurrentScissorX1 - g_waterReflectionCurrentScissorX0;
			const int sh = g_waterReflectionCurrentScissorY1 - g_waterReflectionCurrentScissorY0;

			DLC(dlCmdGrcDeviceSetScissor, (sx, sy, sw, sh));

#if __BANK && __PS3
			if (g_debugWaterReflectionVisBoundsScissorEdgeOnly)
			{
				class ResetGCMScissorWithoutAffectingEdge { public: static void func()
				{
					GCM_DEBUG(GCM::cellGcmSetScissor(GCM_CONTEXT,0,0,4096,4096));
				}};

				DLC_Add(ResetGCMScissorWithoutAffectingEdge::func);
			}
#endif // __BANK && __PS3
		}

		CRenderListBuilder::eDrawMode drawMode = (CRenderListBuilder::eDrawMode)0;
		bool bForceTechnique = false;
		bool bForceLightweight0 = false;

		if (BANK_SWITCH(g_debugWaterReflectionUseWaterReflectionTechnique, true))
		{
			DLC(dlCmdShaderFxPushForcedTechnique, (g_waterReflectionTechniqueGroupId));
			drawMode = CRenderListBuilder::USE_SIMPLE_LIGHTING;
			bForceTechnique = true;
		}
		else
		{
			DLC_Add(Lights::BeginLightweightTechniques);
			DLC_Add(Lights::SelectLightweightTechniques, 0, false);
			drawMode = CRenderListBuilder::USE_FORWARD_LIGHTING;
			bForceLightweight0 = true;
		}

		DLC(dlCmdSetStates, (grcStateBlock::XENON_SWITCH(RS_Default_HalfPixelOffset, RS_Default), grcStateBlock::DSS_LessEqual, grcStateBlock::BS_Normal));

#if __BANK
		if (!g_debugWaterReflectionDirectionalLightAndShadows)
		{
			DLC_Add(WaterReflectionSetDirLightAndShadows, false, Vec4V(V_ZERO));
		}

#if __PS3
		if (!g_debugWaterReflectionEdgeCulling)
		{
			DLC_Add(grcEffect::SetEdgeViewportCullEnable, false);
		}
#endif // __PS3
#endif // __BANK

#if __PS3
		if (!BANK_SWITCH(g_debugWaterReflectionEdgeCullingNoPixelTest, false))
		{
			extern bool SetEdgeNoPixelTestEnabled(bool bEnabled);
			SetEdgeNoPixelTestEnabled(false);
		}
#endif // __PS3

		dlDrawListMgr::SetHighestLOD(
			false,
			(dlDrawListMgr::eHighestDrawableLOD)g_waterReflectionHighestDrawableLOD,
			(dlDrawListMgr::eHighestDrawableLOD)g_waterReflectionHighestFragmentLOD,
			(dlDrawListMgr::eHighestVehicleLOD )g_waterReflectionHighestVehicleLOD,
			(dlDrawListMgr::eHighestPedLOD     )g_waterReflectionHighestPedLOD
		);

#if __BANK && __PS3 && EDGE_STENCIL_TEST && RASTER
		if (g_debugWaterReflectionEdgeCullingStencilTest && CRasterInterface::GetStencilData())
		{
			DLC_Add(SetEdgeStencilTestEnable, g_debugWaterReflectionEdgeCullingStencilTestMode);
		}

		if (g_debugWaterReflectionDisableNonEdgeGeometry)
		{
			DLC_Add(grmModel::SetModelCullerDebugFlags, (u8)CULLERDEBUGFLAG_SPUGEOMETRYQB_DRAW);
		}
#endif // __BANK && __PS3 && EDGE_STENCIL_TEST && RASTER

#if __BANK && __PS3
		if (g_debugWaterReflectionSetZMinMaxControl)
		{
			// ========================================================================================
			// TODO -- this was a temp fix for BS#722018,736673,742471,755186 ..
			// it should no longer be necessary since we restore zclamp to false in rendertargets.cpp,
			// but i'm seeing some issues with oblique projection so let's see if this affects it.
			// see BS#846308
			// ========================================================================================

			class RestoreZClamp { public: static void func()
			{
				GRCDEVICE.SetZMinMaxControl(true, false, false);
			}};
			DLC_Add(RestoreZClamp::func);
		}
#endif // __BANK && __PS3

		XENON_ONLY(DLC_Add(dlCmdEnableHiZ, true));

		const u32 clipPlaneMask = SetWaterReflectionClipPlanes(false, false);
		const u32 opaqueMask =
			CRenderListBuilder::RENDER_OPAQUE |
			CRenderListBuilder::RENDER_ALPHA |
			CRenderListBuilder::RENDER_FADING |
			CRenderListBuilder::RENDER_DONT_RENDER_PLANTS;

		DLCPushTimebar("Render Opaque/Alpha/Fading");
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseWaterReflection");
		CRenderListBuilder::AddToDrawList(
			GetEntityListIndex(),
			g_waterReflectionDrawFilter & opaqueMask,
			drawMode,
			SUBPHASE_NONE,
			RenderStateSetupWaterReflection,
			RenderStateCleanupWaterReflection
#if WATER_REFLECTION_PRE_REFLECTED
			, RenderStateSetupCustomFlagsChangedWaterReflection
#endif // WATER_REFLECTION_PRE_REFLECTED
		);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLCPopTimebar();

		if (clipPlaneMask)
		{
			DLC(dlCmdSetClipPlaneEnable, (0x0));
		}

#if __BANK && __PS3 && EDGE_STENCIL_TEST && RASTER
		if (g_debugWaterReflectionEdgeCullingStencilTest)
		{
			DLC_Add(SetEdgeStencilTestEnable, 0);
		}

		if (g_debugWaterReflectionDisableNonEdgeGeometry)
		{
			DLC_Add(grmModel::SetModelCullerDebugFlags, 0);
		}
#endif // __BANK && __PS3 && EDGE_STENCIL_TEST && RASTER

		dlDrawListMgr::ResetHighestLOD(false);

#if __BANK
		if (!g_debugWaterReflectionDirectionalLightAndShadows)
		{
			DLC_Add(WaterReflectionSetDirLightAndShadows, true, Vec4V(V_ZERO));
		}

#if __PS3
		if (!g_debugWaterReflectionEdgeCulling)
		{
			DLC_Add(grcEffect::SetEdgeViewportCullEnable, true);
		}
#endif // __PS3
#endif // __BANK

#if __PS3
		if (!BANK_SWITCH(g_debugWaterReflectionEdgeCullingNoPixelTest, false))
		{
			extern bool SetEdgeNoPixelTestEnabled(bool bEnabled);
			SetEdgeNoPixelTestEnabled(true);
		}
#endif // __PS3

		if (bForceTechnique)
		{
			DLC(dlCmdShaderFxPopForcedTechnique, ());
		}

		if (bForceLightweight0)
		{
			DLC_Add(Lights::EndLightweightTechniques);
		}

		if (!g_waterReflectionIsCameraUnderwater)
		{
			if (BANK_SWITCH(g_debugWaterReflectionRenderDistantLights, true))
			{
				const float distLightIntensity = BANK_SWITCH(g_waterReflectionDistLightIntensity_TCVAREnabled, true) ? g_waterReflectionDistLightIntensity_TCVAR : 1.0f;
				DLC_Add(CVisualEffects::RenderDistantLights, CVisualEffects::RM_WATER_REFLECTION, GetRenderFlags(), distLightIntensity);
			}

			if (BANK_SWITCH(g_debugWaterReflectionRenderCoronas, true))
			{
				const float coronaIntensity = BANK_SWITCH(g_waterReflectionCoronaIntensity_TCVAREnabled, true) ? g_waterReflectionCoronaIntensity_TCVAR : 1.0f;
				DLC_Add(CVisualEffects::RenderCoronas, CVisualEffects::RM_WATER_REFLECTION, 1.0f, coronaIntensity);
			}
		}
	}

	if (bUseScissor)
	{
		DLC(dlCmdGrcDeviceDisableScissor, ());
	}

#if __BANK
	if (g_debugWaterReflectionWireframeOverride)
	{
		DLC_Add(grcStateBlock::SetWireframeOverride, false);
	}
#endif // __BANK

	DLC(dlCmdUnLockRenderTarget, (0));

#if RSG_PC
	if(m_MSAAcolor)
	{
		//	resolve
		DLC_Add(ResolveMSAA);
	}
#endif

#if RSG_ORBIS
	DLC_Add(ResolveRenderTarget);
#endif

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DLC_Add(CShaderLib::SetUseFogRay, true);

	DrawListEpilogue();

	// Restore the old prototype batch value.
	g_prototype_batch = bOldProtoTypeBatchVal;

	PS3_ONLY(CBubbleFences::InsertBubbleFenceDLC(CBubbleFences::BFENCE_WATERREFLECT));
}

#if RSG_PC
void CRenderPhaseWaterReflection::ResolveMSAA()
{
	static_cast<grcRenderTargetMSAA*>(m_MSAAcolor)->Resolve(m_color);
	m_color->GenerateMipmaps();
}
#endif

#if __BANK

static void WaterReflectionShowRT_button()
{
	const char* rtName = "WATER_REFLECTION_COLOUR";

#if 0&&__XENON
	if (Water::IsUsingMirrorWaterSurface())
	{
		rtName = "MIRROR_RT";
	}
#endif // __XENON

	if (!CRenderTargets::IsShowingRenderTarget(rtName))
	{
		CRenderTargets::ShowRenderTarget(rtName);
		CRenderTargets::ShowRenderTargetBrightness(1.0f);
		CRenderTargets::ShowRenderTargetNativeRes(true, true, true);
		CRenderTargets::ShowRenderTargetFullscreen(true);
		CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
	}
	else
	{
		CRenderTargets::ShowRenderTargetReset();
	}
}

static void WaterReflectionShowDT_button()
{
	const char* rtName = "WATER_REFLECTION_DEPTH";

#if 0&&__XENON
	if (Water::IsUsingMirrorWaterSurface())
	{
		rtName = "MIRROR_DT";
	}
#endif // __XENON

	if (!CRenderTargets::IsShowingRenderTarget(rtName))
	{
		CRenderTargets::ShowRenderTarget(rtName);
		CRenderTargets::ShowRenderTargetBrightness(1.0f);
		CRenderTargets::ShowRenderTargetNativeRes(true, true, true);
		CRenderTargets::ShowRenderTargetFullscreen(true);
		CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
	}
	else
	{
		CRenderTargets::ShowRenderTargetReset();
	}
}

template <int i> static void WaterReflectionShowRenderingStats_button()
{
	if (CPostScanDebug::GetCountModelsRendered())
	{
		CPostScanDebug::SetCountModelsRendered(false);
		gDrawListMgr->SetStatsHighlight(false);
	}
	else
	{
		CPostScanDebug::SetCountModelsRendered(true, DL_RENDERPHASE_WATER_REFLECTION);
		gDrawListMgr->SetStatsHighlight(true, "DL_WATER_REFLECTION", i == 0, i == 1, false);
		grcDebugDraw::SetDisplayDebugText(true);
	}
}

static void HighQualityMode_button()
{
	g_waterReflectionVisMaskFlags =
//	VIS_ENTITY_MASK_OTHER             |
	VIS_ENTITY_MASK_PED               |
	VIS_ENTITY_MASK_OBJECT            |
	VIS_ENTITY_MASK_DUMMY_OBJECT      |
	VIS_ENTITY_MASK_VEHICLE           |
	VIS_ENTITY_MASK_ANIMATED_BUILDING |
	VIS_ENTITY_MASK_LOLOD_BUILDING    |
	VIS_ENTITY_MASK_HILOD_BUILDING    |
//	VIS_ENTITY_MASK_LIGHT             |
	VIS_ENTITY_MASK_TREE              |
	0;

	g_debugWaterReflectionRenderAllPhysicalEntities = true;
	g_debugWaterReflectionLODMaskEnabled            = false;

	g_waterReflectionHighestDrawableLOD    = dlDrawListMgr::DLOD_DEFAULT;
	g_waterReflectionHighestFragmentLOD    = dlDrawListMgr::DLOD_DEFAULT;
	g_waterReflectionHighestVehicleLOD     = dlDrawListMgr::VLOD_DEFAULT;
	g_waterReflectionHighestPedLOD         = dlDrawListMgr::PLOD_DEFAULT;
	g_waterReflectionHighestEntityLOD      = 0;
	g_waterReflectionMaxPedDistanceEnabled = false;
	g_waterReflectionMaxVehDistanceEnabled = false;
	g_waterReflectionMaxObjDistanceEnabled = false;
}

static void DefaultQualityMode_button()
{
	g_waterReflectionVisMaskFlags =
//	VIS_ENTITY_MASK_OTHER             |
	VIS_ENTITY_MASK_PED               |
	VIS_ENTITY_MASK_OBJECT            |
	VIS_ENTITY_MASK_DUMMY_OBJECT      |
	VIS_ENTITY_MASK_VEHICLE           |
	VIS_ENTITY_MASK_ANIMATED_BUILDING |
	VIS_ENTITY_MASK_LOLOD_BUILDING    |
	VIS_ENTITY_MASK_HILOD_BUILDING    |
//	VIS_ENTITY_MASK_LIGHT             |
//	VIS_ENTITY_MASK_TREE              |
	0;

	g_debugWaterReflectionRenderAllPhysicalEntities = true;
	g_debugWaterReflectionLODMaskEnabled            = true;

	g_waterReflectionHighestDrawableLOD    = dlDrawListMgr::DLOD_MED;
	g_waterReflectionHighestFragmentLOD    = dlDrawListMgr::DLOD_MED;
	g_waterReflectionHighestVehicleLOD     = dlDrawListMgr::VLOD_HIGH;
	g_waterReflectionHighestPedLOD         = dlDrawListMgr::PLOD_LOD;
	g_waterReflectionHighestEntityLOD      = 0;
	g_waterReflectionMaxPedDistanceEnabled = true;
	g_waterReflectionMaxVehDistanceEnabled = true;
	g_waterReflectionMaxObjDistanceEnabled = true;
}

static void LowQualityMode_button()
{
	g_waterReflectionVisMaskFlags =
//	VIS_ENTITY_MASK_OTHER             |
//	VIS_ENTITY_MASK_PED               |
//	VIS_ENTITY_MASK_OBJECT            |
//	VIS_ENTITY_MASK_DUMMY_OBJECT      |
	VIS_ENTITY_MASK_VEHICLE           |
	VIS_ENTITY_MASK_ANIMATED_BUILDING |
	VIS_ENTITY_MASK_LOLOD_BUILDING    |
	VIS_ENTITY_MASK_HILOD_BUILDING    |
//	VIS_ENTITY_MASK_LIGHT             |
//	VIS_ENTITY_MASK_TREE              |
	0;

	g_debugWaterReflectionRenderAllPhysicalEntities = false;
	g_debugWaterReflectionLODMaskEnabled            = true;

	g_waterReflectionHighestDrawableLOD    = dlDrawListMgr::DLOD_VLOW;
	g_waterReflectionHighestFragmentLOD    = dlDrawListMgr::DLOD_VLOW;
	g_waterReflectionHighestVehicleLOD     = dlDrawListMgr::VLOD_LOW;
	g_waterReflectionHighestPedLOD         = dlDrawListMgr::PLOD_LOD;
	g_waterReflectionHighestEntityLOD      = 1 + LODTYPES_DEPTH_SLOD1;
	g_waterReflectionMaxPedDistanceEnabled = true;
	g_waterReflectionMaxVehDistanceEnabled = true;
	g_waterReflectionMaxObjDistanceEnabled = true;
}

#if RASTER
static void DumpOccluders()
{
	if (g_debugWaterReflectionDumpOccluders)
	{
		CRasterInterface::DumpOccludersToOBJ_Begin(g_debugWaterReflectionDumpOccludersPath);
	}
	else
	{
		CRasterInterface::DumpOccludersToOBJ_End();
	}
}
#endif // RASTER

void CRenderPhaseWaterReflection::AddWidgets(bkGroup& group)
{
	bkGroup& advanced = *group.AddGroup("Advanced", false);
	{
		advanced.AddToggle("Show Info"                     , &g_debugWaterReflectionShowInfo);
		advanced.AddToggle("Use Water Reflection Technique", &g_debugWaterReflectionUseWaterReflectionTechnique);
		advanced.AddToggle("Force Default Render Bucket"   , &g_debugWaterReflectionForceDefaultRenderBucket);
		advanced.AddToggle("Underwater Planar Reflection"  , &Water::m_bUnderwaterPlanarReflection);
		advanced.AddButton("Make Water Flat"               , Water::MakeWaterFlat);
#if RASTER
		bkGroup& raster = *advanced.AddGroup("Raster Test", false);
		{
			raster.AddToggle("Raster Test Enabled"         , &g_debugWaterReflectionRaster);
#if ENTITY_DESC_KDOP
			raster.AddToggle("Use kDOP"                    , &g_scanDebugFlagsPortals, SCAN_DEBUG_USE_ENTITY_DESC_KDOP);
#endif // ENTITY_DESC_KDOP
			raster.AddToggle("Use Coverage"                , &CRasterInterface::GetControlRef().m_useCoverage);
			raster.AddToggle("Use Corners"                 , &CRasterInterface::GetControlRef().m_useCorners);
			raster.AddToggle("Use Linear Z"                , &CRasterInterface::GetControlRef().m_useLinearZ);
			raster.AddButton("Perfect Clip"                , &CRasterInterface::PerfectClip);
			raster.AddSlider("Display Opacity"             , &CRasterInterface::GetControlRef().m_displayOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			raster.AddToggle("Display Stencil Only"        , &CRasterInterface::GetControlRef().m_displayStencilOnly);
			raster.AddSlider("Display Linear Z0"           , &CRasterInterface::GetControlRef().m_displayLinearZ0, 0.0f, 1024.0f, 1.0f);
			raster.AddSlider("Display Linear Z1"           , &CRasterInterface::GetControlRef().m_displayLinearZ1, 0.0f, 1024.0f, 1.0f);
			raster.AddToggle("Rasterise Occl Boxes"        , &g_debugWaterReflectionRasteriseOcclBoxes);
			raster.AddToggle("Rasterise Occl Meshes"       , &g_debugWaterReflectionRasteriseOcclMeshes);
			raster.AddToggle("Rasterise Occl Water"        , &g_debugWaterReflectionRasteriseOcclWater);
			raster.AddSlider("Rasterise Occl Geom Opacity" , &g_debugWaterReflectionRasteriseOcclGeomOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			raster.AddButton("Toggle Occlusion Overlay"    , &COcclusion::ToggleGBUFStencilOverlay);
			raster.AddToggle("Test Triangle"               , &g_debugWaterReflectionRasteriseTest);
			raster.AddToggle("Test Triangle (stencil)"     , &g_debugWaterReflectionRasteriseTestStencil);
			raster.AddToggle(" - Test Clip"                , &g_debugWaterReflectionRasteriseTestViewportClip);
			raster.AddVector(" - P0"                       , &g_debugWaterReflectionRasteriseTestP[0], 0.0f, 128.0f, 1.0f/4.0f);
			raster.AddVector(" - P1"                       , &g_debugWaterReflectionRasteriseTestP[1], 0.0f, 128.0f, 1.0f/4.0f);
			raster.AddVector(" - P2"                       , &g_debugWaterReflectionRasteriseTestP[2], 0.0f, 128.0f, 1.0f/4.0f);
			raster.AddVector(" - Offset"                   , &g_debugWaterReflectionRasteriseTestOffset, -128.0f, 128.0f, 1.0f/32.0f);
			raster.AddSlider(" - Angle"                    , &g_debugWaterReflectionRasteriseTestAngle, -360.0f, 360.0f, 1.0f/8.0f);
			raster.AddSlider(" - z0"                       , &g_debugWaterReflectionRasteriseTestZ[0], -1000.0f, 1000.0f, 1.0f/64.0f);
			raster.AddSlider(" - z1"                       , &g_debugWaterReflectionRasteriseTestZ[1], -1000.0f, 1000.0f, 1.0f/64.0f);
			raster.AddSlider(" - z2"                       , &g_debugWaterReflectionRasteriseTestZ[2], -1000.0f, 1000.0f, 1.0f/64.0f);
			raster.AddText  ("Dump Occluders Path"         , &g_debugWaterReflectionDumpOccludersPath[0], NELEM(g_debugWaterReflectionDumpOccludersPath), false);
			raster.AddToggle("Dump Occluders"              , &g_debugWaterReflectionDumpOccluders, DumpOccluders);
			raster.AddToggle("Dump Occluders (water only)" , &g_debugWaterReflectionDumpOccludersWaterOnly);

			raster.AddTitle("Occluder Statistics");

			raster.AddSlider(STRING(m_numPixelsPassed )    , &CRasterInterface::GetOccludeStatsRef().m_numPixelsPassed , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPixels       )    , &CRasterInterface::GetOccludeStatsRef().m_numPixels       , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPixelsBounds )    , &CRasterInterface::GetOccludeStatsRef().m_numPixelsBounds , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPolysVisible )    , &CRasterInterface::GetOccludeStatsRef().m_numPolysVisible , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPolys        )    , &CRasterInterface::GetOccludeStatsRef().m_numPolys        , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPolysScene   )    , &CRasterInterface::GetOccludeStatsRef().m_numPolysScene   , 0, 99999, 0);
			raster.AddSlider(STRING(m_numModelsVisible)    , &CRasterInterface::GetOccludeStatsRef().m_numModelsVisible, 0, 99999, 0);
			raster.AddSlider(STRING(m_numModels       )    , &CRasterInterface::GetOccludeStatsRef().m_numModels       , 0, 99999, 0);
			raster.AddSlider(STRING(m_numModelsScene  )    , &CRasterInterface::GetOccludeStatsRef().m_numModelsScene  , 0, 99999, 0);
			raster.AddSlider(STRING(m_numBoxesVisible )    , &CRasterInterface::GetOccludeStatsRef().m_numBoxesVisible , 0, 99999, 0);
			raster.AddSlider(STRING(m_numBoxes        )    , &CRasterInterface::GetOccludeStatsRef().m_numBoxes        , 0, 99999, 0);
			raster.AddSlider(STRING(m_numBoxesScene   )    , &CRasterInterface::GetOccludeStatsRef().m_numBoxesScene   , 0, 99999, 0);

			raster.AddTitle("Water Statistics");

			raster.AddSlider(STRING(m_numPixelsPassed )    , &CRasterInterface::GetStencilStatsRef().m_numPixelsPassed , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPixels       )    , &CRasterInterface::GetStencilStatsRef().m_numPixels       , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPixelsBounds )    , &CRasterInterface::GetStencilStatsRef().m_numPixelsBounds , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPolysVisible )    , &CRasterInterface::GetStencilStatsRef().m_numPolysVisible , 0, 99999, 0);
			raster.AddSlider(STRING(m_numPolys        )    , &CRasterInterface::GetStencilStatsRef().m_numPolys        , 0, 99999, 0);
		}
#endif // RASTER

		advanced.AddToggle("Rasterize Water Quads into Stencil", &Water::GetRasterizeWaterQuadsIntoStencilRef());
		advanced.AddToggle("Remove Isolated Stencil Strips"    , &g_debugWaterReflectionRemoveIsolatedStencil);
		advanced.AddToggle("Disable Occlusion Close To Ocean"  , &g_debugWaterReflectionDisableOcclusionCloseToOcean);
		advanced.AddSlider("Disable Occlusion Threshold Height", &g_debugWaterReflectionDisableOcclusionThreshold, 0.0f, 1.0f, 1.0f/128.0f);

		const char* visBoundsModeStrings[] =
		{
			"WRVB_NONE",
			"WRVB_WATER_QUADS",
			"WRVB_WATER_QUADS_USE_OCCLUSION",
			"WRVB_STENCIL",
			"WRVB_STENCIL_VECTORISED",
			"WRVB_STENCIL_FAST",
		};
		CompileTimeAssert(NELEM(visBoundsModeStrings) == WRVB_COUNT);

		advanced.AddCombo ("Visibility Bounds Mode"             , &g_debugWaterReflectionVisBoundsMode, NELEM(visBoundsModeStrings), visBoundsModeStrings);
		advanced.AddToggle("Visibility Bounds Draw"             , &g_debugWaterReflectionVisBoundsDraw);
		advanced.AddToggle("Visibility Bounds Cull Planes"      , &g_debugWaterReflectionVisBoundsCullPlanes);
		advanced.AddToggle("Visibility Bounds Cull Planes Water", &g_debugWaterReflectionVisBoundsCullPlanesWater);
		advanced.AddToggle("Visibility Bounds Cull Planes Near" , &g_debugWaterReflectionVisBoundsCullPlanesNear);
		advanced.AddToggle("Visibility Bounds Cull Planes Show" , &g_debugWaterReflectionVisBoundsCullPlanesShow);
#if __PS3
		advanced.AddToggle("Visibility Bounds Clip Edge"        , &g_debugWaterReflectionVisBoundsClipPlanesEdge);
#endif // __PS3
		advanced.AddToggle("Scissor"                            , &g_debugWaterReflectionVisBoundsScissor);
#if __PS3
		advanced.AddToggle("Scissor Edge Only"                  , &g_debugWaterReflectionVisBoundsScissorEdgeOnly);
#endif // __PS3
		advanced.AddToggle("Scissor Sky"                        , &g_debugWaterReflectionVisBoundsScissorSky);
		advanced.AddSlider("Scissor Sky Expand"                 , &g_debugWaterReflectionVisBoundsScissorSkyExpand, 0, 64, 1);
		advanced.AddSlider("Scissor Sky Expand Y0"              , &g_debugWaterReflectionVisBoundsScissorSkyExpandY0, 0, 64, 1);
		advanced.AddSlider("Scissor Expand"                     , &g_debugWaterReflectionVisBoundsScissorExpand, 0, 64, 1);
		advanced.AddSlider("Scissor Expand Y0"                  , &g_debugWaterReflectionVisBoundsScissorExpandY0, 0, 64, 1);
		advanced.AddToggle("Scissor Override"                   , &g_debugWaterReflectionVisBoundsScissorOverride);
		advanced.AddSlider("Scissor X0"                         , &g_debugWaterReflectionVisBoundsScissorX0, 0, 1024, 1);
		advanced.AddSlider("Scissor Y0"                         , &g_debugWaterReflectionVisBoundsScissorY0, 0, 2048, 1);
		advanced.AddSlider("Scissor X1"                         , &g_debugWaterReflectionVisBoundsScissorX1, 0, 2048, 1);
		advanced.AddSlider("Scissor Y1"                         , &g_debugWaterReflectionVisBoundsScissorY1, 0, 2048, 1);
		advanced.AddToggle("Culling Planes"                     , &g_debugWaterReflectionCullingPlanesEnabled);
#if !__PS3
		advanced.AddToggle("Use HW Clip Plane"                  , &g_debugWaterReflectionUseHWClipPlane);
#endif // !__PS3
#if WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
		advanced.AddToggle("Use Clip Plane In Pixel Shader"     , &g_debugWaterReflectionUseClipPlaneInPixelShader);
#endif // WATER_REFLECTION_CLIP_IN_PIXEL_SHADER
#if WATER_REFLECTION_PRE_REFLECTED
		advanced.AddToggle("No Pixel or HW Clip Pre-Reflected"  , &g_debugWaterReflectionNoPixelOrHWClipPreReflected);
		advanced.AddToggle("Hide Pre-Reflected"                 , &g_debugWaterReflectionHidePreReflected);
#endif // WATER_REFLECTION_PRE_REFLECTED
		advanced.AddToggle("Clip Plane Offset Enabled"          , &g_debugWaterReflectionClipPlaneOffsetEnabled);
		advanced.AddSlider("Clip Plane Offset"                  , &g_debugWaterReflectionClipPlaneOffset, -100.0f, 100.0f, 1.0f/32.0f);
		advanced.AddToggle("Use Oblique Projection"             , &g_debugWaterReflectionUseObliqueProjection);
		advanced.AddSlider("Oblique Projection Offset"          , &g_debugWaterReflectionObliqueProjectionOffset, -64.0f, 64.0f, 1.0f/32.0f);
		advanced.AddSlider("Oblique Projection Scale"           , &g_debugWaterReflectionObliqueProjectionScale, -64.0f, 64.0f, 1.0f/256.0f);
		advanced.AddToggle("Viewport Reflect In Water Plane"    , &g_debugWaterReflectionViewportReflectInWaterPlane);
		advanced.AddToggle("Viewport Set Norm Window"           , &g_debugWaterReflectionViewportSetNormWindow);
		advanced.AddToggle("Water Occlusion"                    , &g_debugWaterReflectionWaterOcclusionEnabled);
		advanced.AddToggle(" - reference code"                  , &g_scanDebugFlagsPortals, SCAN_DEBUG_USE_NEW_WATER_STENCIL_TEST_REFERENCE_CODE);
		advanced.AddToggle(" - non-vector code"                 , &g_scanDebugFlagsPortals, SCAN_DEBUG_USE_NEW_WATER_STENCIL_TEST_NONVECTOR_CODE);
		advanced.AddToggle(" - debug"                           , &g_scanDebugFlagsPortals, SCAN_DEBUG_USE_NEW_WATER_STENCIL_TEST_DEBUG);
		advanced.AddToggle("Flip Water Occlusion Results"       , &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_FLIP_WATER_OCCLUSION_RESULT);
		advanced.AddToggle("Render Only If Primary Renders"     , &g_debugWaterReflectionRenderOnlyWithPrimary);
		advanced.AddToggle("Render Exterior"                    , &g_debugWaterReflectionRenderExterior);
		advanced.AddToggle("Render Interior"                    , &g_debugWaterReflectionRenderInterior);
		advanced.AddToggle("Portal Traversal"                   , &g_debugWaterReflectionPortalTraversal);
		advanced.AddToggle("Use Pool Sphere"                    , &g_debugWaterReflectionUsePoolSphere);
		advanced.AddSlider("Pool Sphere Radius Scale"           , &g_debugWaterReflectionPoolSphereRadiusScale, 1.0f, 32.0f, 1.0f/32.0f);
		advanced.AddSlider("Near Clip"                          , &g_debugWaterReflectionNearClip, 0.001f, 0.5f, 1.0f/256.0f);
		advanced.AddToggle("Override Water Height Enabled"      , &g_debugWaterReflectionOverrideWaterHeightEnabled);
		advanced.AddSlider("Override Water Height"              , &g_debugWaterReflectionOverrideWaterHeight, -20.0f, 400.0f, 1.0f/32.0f);
		advanced.AddSlider("Height Offset"                      , &g_debugWaterReflectionHeightOffset, -100.0f, 100.0f, 1.0f/32.0f);
		advanced.AddText  ("Height Offset TCVAR"                , &g_waterReflectionPlaneHeightOffset_TCVAR);
		advanced.AddToggle("Height Offset TCVAR Enabled"        , &g_waterReflectionPlaneHeightOffset_TCVAREnabled);
		advanced.AddText  ("Height Override TCVAR"              , &g_waterReflectionPlaneHeightOverride_TCVAR);
		advanced.AddText  ("Height Override Amount TCVAR"       , &g_waterReflectionPlaneHeightOverrideAmount_TCVAR);
		advanced.AddText  ("Dist Light Intensity TCVAR"         , &g_waterReflectionDistLightIntensity_TCVAR);
		advanced.AddToggle("Dist Light Intensity TCVAR Enabled" , &g_waterReflectionDistLightIntensity_TCVAREnabled);
		advanced.AddText  ("Corona Intensity TCVAR"             , &g_waterReflectionCoronaIntensity_TCVAR);
		advanced.AddToggle("Corona Intensity TCVAR Enabled"     , &g_waterReflectionCoronaIntensity_TCVAREnabled);
		advanced.AddCombo ("Sky FLOD Mode"                      , &g_debugWaterReflectionSkyFLODMode, NELEM(g_waterReflectionTimecycleModeStrings), g_waterReflectionTimecycleModeStrings);
		advanced.AddSlider("Sky FLOD Range Override"            , &g_debugWaterReflectionSkyFLODRangeOverride, 0.0f, 1.0f, 0.001f);
		advanced.AddText  ("Sky FLOD Range TCVAR"               , &g_waterReflectionSkyFLODRange_TCVAR);
#if WATER_REFLECTION_OFFSET_TRICK
		advanced.AddVector("Offset Global"                      , &g_debugWaterReflectionOffsetGlobal, -100.0f, 100.0f, 1.0f/32.0f);
		advanced.AddSlider("Offset Camera Track"                , &g_debugWaterReflectionOffsetCameraTrack, 0.0f, 1.0f, 1.0f/32.0f);

		class UpdateMap { public: static void func()
		{
			if (g_waterReflectionOffsetModelHeightMap.GetNumUsed() == 0)
			{
				class AddEntry { public: static void func(const char* modelName, float height)
				{
					const u32 modelIndex = CModelInfo::GetModelIdFromName(modelName).GetModelIndex();

					if (AssertVerify(modelIndex != fwModelId::MI_INVALID))
					{
						g_waterReflectionOffsetModelHeightMap[modelIndex] = height;
					}
				}};

				// TODO -- load some text file with a list of {modelName, height} pairs
				AddEntry::func("foo", 1.0f);
				AddEntry::func("bar", 2.0f);
			}
		}};

		advanced.AddToggle("Offset Enabled"                     , &g_debugWaterReflectionOffsetEnabled, UpdateMap::func);
#endif // WATER_REFLECTION_OFFSET_TRICK
		advanced.AddToggle("Wireframe Override"                 , &g_debugWaterReflectionWireframeOverride);
		advanced.AddColor ("Clear Colour"                       , &g_debugWaterReflectionClearColour);
		advanced.AddToggle("Clear Colour Always"                , &g_debugWaterReflectionClearColourAlways);
		advanced.AddToggle("Proxies Ignore LOD Mask"            , &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_WATER_REFLECTION_PROXIES_IGNORE_LOD_MASK);
		advanced.AddToggle("Proxies Ignore LOD Ranges"          , &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_WATER_REFLECTION_PROXIES_IGNORE_LOD_RANGES);
		advanced.AddToggle("Render Proxies When Far Clip = 0"   , &g_debugWaterReflectionRenderProxiesWhenFarClipIsZero);
		advanced.AddToggle("Render Sky Background"              , &g_debugWaterReflectionRenderSkyBackground);
		advanced.AddToggle("Render Sky Background Tone Mapped"  , &g_debugWaterReflectionRenderSkyBackgroundToneMapped);
		advanced.AddToggle("Render Sky Background With Shader"  , &g_debugWaterReflectionRenderSkyBackgroundWithShader);
		advanced.AddSlider("Render Sky Background Offset X"     , &g_debugWaterReflectionRenderSkyBackgroundOffsetX, -32.0f, 32.0f, 1.0f/16.0f);
		advanced.AddSlider("Render Sky Background Offset Y"     , &g_debugWaterReflectionRenderSkyBackgroundOffsetY, -32.0f, 32.0f, 1.0f/16.0f);
		advanced.AddSlider("Render Sky Background HDR Scale"    , &g_debugWaterReflectionRenderSkyBackgroundHDRScale, 0.0f, 16.0f, 1.0f/128.0f);
		advanced.AddToggle("Render Sky"                         , &g_debugWaterReflectionRenderSky);
		advanced.AddToggle("Render Sky Only"                    , &g_debugWaterReflectionRenderSkyOnly);
		advanced.AddToggle("Render Distant Lights"              , &g_debugWaterReflectionRenderDistantLights);
		advanced.AddSlider(" - fadeout min height"              , &g_debugWaterReflectionDistLightFadeoutMinHeight, 0.0f, 1000.0f, 1.0f/32.0f);
		advanced.AddSlider(" - fadeout max height"              , &g_debugWaterReflectionDistLightFadeoutMaxHeight, 0.0f, 1000.0f, 1.0f/32.0f);
		advanced.AddSlider(" - fadeout speed"                   , &g_debugWaterReflectionDistLightFadeoutSpeed, 0.0f, 1.0f, 0.01f);
		advanced.AddToggle("Render Coronas"                     , &g_debugWaterReflectionRenderCoronas);
		advanced.AddToggle("Dir Light+Shadows"                  , &g_debugWaterReflectionDirectionalLightAndShadows);
#if WATER_REFLECTION_PRE_REFLECTED
		advanced.AddToggle("Dir Light Reflect Pre-Reflected"    , &g_debugWaterReflectionDirLightReflectPreReflected);
		advanced.AddToggle("Dir Shadow Disable Pre-Reflected"   , &g_debugWaterReflectionDirShadowDisablePreReflected);
#endif // WATER_REFLECTION_PRE_REFLECTED
#if __PS3
		advanced.AddToggle("EDGE Culling"                       , &g_debugWaterReflectionEdgeCulling);
		advanced.AddToggle("EDGE Culling - NoPixel Test"        , &g_debugWaterReflectionEdgeCullingNoPixelTest);
#if EDGE_STENCIL_TEST && RASTER
		advanced.AddToggle("EDGE Culling - Stencil Test"        , &g_debugWaterReflectionEdgeCullingStencilTest);
		advanced.AddSlider(" - stencil test mode"               , &g_debugWaterReflectionEdgeCullingStencilTestMode, 1, 4, 1);
		advanced.AddToggle(" - disable non-edge geometry"       , &g_debugWaterReflectionDisableNonEdgeGeometry);
#endif // EDGE_STENCIL_TEST && RASTER
		advanced.AddToggle("Set ZMinMaxControl"                 , &g_debugWaterReflectionSetZMinMaxControl);
#endif // __PS3
		advanced.AddToggle("Draw List Prototype Enabled"        , &g_debugWaterReflectionDrawListPrototypeEnabled);
		advanced.AddToggle("Render All Physical Entities"       , &g_debugWaterReflectionRenderAllPhysicalEntities);
		advanced.AddSlider("Script Water Height"                , &g_waterReflectionScriptWaterHeight, -20.0f, 400.0f, 1.0f/32.0f);
		advanced.AddToggle("Script Water Height Enabled"        , &g_waterReflectionScriptWaterHeightEnabled);
		advanced.AddToggle("Script Object Visibility"           , &g_waterReflectionScriptObjectVisibility);

		advanced.AddTitle("Vis Entity Mask");
		advanced.AddToggle("VIS_ENTITY_MASK_OTHER            ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_OTHER            );
		advanced.AddToggle("VIS_ENTITY_MASK_PED              ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_PED              );
		advanced.AddToggle("VIS_ENTITY_MASK_OBJECT           ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_OBJECT           );
		advanced.AddToggle("VIS_ENTITY_MASK_DUMMY_OBJECT     ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_DUMMY_OBJECT     );
		advanced.AddToggle("VIS_ENTITY_MASK_VEHICLE          ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_VEHICLE          );
		advanced.AddToggle("VIS_ENTITY_MASK_ANIMATED_BUILDING", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_ANIMATED_BUILDING);
		advanced.AddToggle("VIS_ENTITY_MASK_LOLOD_BUILDING   ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_LOLOD_BUILDING   );
		advanced.AddToggle("VIS_ENTITY_MASK_HILOD_BUILDING   ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_HILOD_BUILDING   );
		advanced.AddToggle("VIS_ENTITY_MASK_LIGHT            ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_LIGHT            );
		advanced.AddToggle("VIS_ENTITY_MASK_TREE             ", &g_waterReflectionVisMaskFlags, VIS_ENTITY_MASK_TREE             );

		advanced.AddTitle("Render Settings");
	//	advanced.AddToggle("RENDER_SETTING_RENDER_PARTICLES         ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_PARTICLES         );
	//	advanced.AddToggle("RENDER_SETTING_RENDER_WATER             ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_WATER             );
	//	advanced.AddToggle("RENDER_SETTING_STORE_WATER_OCCLUDERS    ", &g_waterReflectionRenderSettings, RENDER_SETTING_STORE_WATER_OCCLUDERS    );
		advanced.AddToggle("RENDER_SETTING_RENDER_ALPHA_POLYS       ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_ALPHA_POLYS       );
		advanced.AddToggle("RENDER_SETTING_RENDER_SHADOWS           ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_SHADOWS           );
	//	advanced.AddToggle("RENDER_SETTING_RENDER_DECAL_POLYS       ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_DECAL_POLYS       );
		advanced.AddToggle("RENDER_SETTING_RENDER_CUTOUT_POLYS      ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_CUTOUT_POLYS      );
	//	advanced.AddToggle("RENDER_SETTING_RENDER_UI_EFFECTS        ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_UI_EFFECTS        );
	//	advanced.AddToggle("RENDER_SETTING_ALLOW_PRERENDER          ", &g_waterReflectionRenderSettings, RENDER_SETTING_ALLOW_PRERENDER          );
		advanced.AddToggle("RENDER_SETTING_RENDER_DISTANT_LIGHTS    ", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_DISTANT_LIGHTS    );
		advanced.AddToggle("RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS", &g_waterReflectionRenderSettings, RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS);
	//	advanced.AddToggle("RENDER_SETTING_NO_VISIBILITY_CHECK      ", &g_waterReflectionRenderSettings, (u32)RENDER_SETTING_NO_VISIBILITY_CHECK );

		advanced.AddTitle("Render Flags");
		advanced.AddToggle("RENDER_OPAQUE                    ", &g_waterReflectionDrawFilter, CRenderListBuilder::RENDER_OPAQUE                    );
		advanced.AddToggle("RENDER_ALPHA                     ", &g_waterReflectionDrawFilter, CRenderListBuilder::RENDER_ALPHA                     );
		advanced.AddToggle("RENDER_FADING                    ", &g_waterReflectionDrawFilter, CRenderListBuilder::RENDER_FADING                    );
		advanced.AddToggle("RENDER_FADING_ONLY               ", &g_waterReflectionDrawFilter, CRenderListBuilder::RENDER_FADING_ONLY               );
		advanced.AddToggle("RENDER_DONT_RENDER_TREES         ", &g_waterReflectionDrawFilter, CRenderListBuilder::RENDER_DONT_RENDER_TREES         );
		advanced.AddToggle("RENDER_DONT_RENDER_DECALS_CUTOUTS", &g_waterReflectionDrawFilter, CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS);

		advanced.AddTitle("Alpha-Blending Passes");
		advanced.AddToggle("RPASS_VISIBLE", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_VISIBLE));
		advanced.AddToggle("RPASS_LOD    ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_LOD    ));
		advanced.AddToggle("RPASS_CUTOUT ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_CUTOUT ));
		advanced.AddSlider("RPASS_CUTOUT Alpha Ref", &g_waterReflectionBlendStateAlphaTestAlphaRef, 0.f, 1.f, 0.01f);
	//	advanced.AddToggle("RPASS_DECAL  ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_DECAL  ));
		advanced.AddToggle("RPASS_FADING ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_FADING ));
		advanced.AddToggle("RPASS_ALPHA  ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_ALPHA  ));
	//	advanced.AddToggle("RPASS_WATER  ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_WATER  ));
		advanced.AddToggle("RPASS_TREE   ", &g_waterReflectionAlphaBlendingPasses, BIT(RPASS_TREE   ));

		advanced.AddTitle("Depth-Writing Passes");
		advanced.AddToggle("RPASS_VISIBLE", &g_waterReflectionDepthWritingPasses, BIT(RPASS_VISIBLE));
		advanced.AddToggle("RPASS_LOD    ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_LOD    ));
		advanced.AddToggle("RPASS_CUTOUT ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_CUTOUT ));
	//	advanced.AddToggle("RPASS_DECAL  ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_DECAL  ));
		advanced.AddToggle("RPASS_FADING ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_FADING ));
		advanced.AddToggle("RPASS_ALPHA  ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_ALPHA  ));
	//	advanced.AddToggle("RPASS_WATER  ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_WATER  ));
		advanced.AddToggle("RPASS_TREE   ", &g_waterReflectionDepthWritingPasses, BIT(RPASS_TREE   ));

		advanced.AddTitle("LOD");
		gDrawListMgr->SetupLODWidgets(
			advanced,
			&g_waterReflectionHighestDrawableLOD,
			&g_waterReflectionHighestFragmentLOD,
			&g_waterReflectionHighestVehicleLOD,
			&g_waterReflectionHighestPedLOD
		);

		const char* highestEntityLODStrings[] = {"Default", "HD", "LOD", "SLOD1", "SLOD2", "SLOD3", "SLOD4"};

		advanced.AddCombo ("Highest Entity LOD"              , &g_waterReflectionHighestEntityLOD, NELEM(highestEntityLODStrings), highestEntityLODStrings);
		advanced.AddCombo ("Highest Entity LOD TCVAR"        , &g_waterReflectionHighestEntityLOD_TCVAR, NELEM(highestEntityLODStrings), highestEntityLODStrings);
		advanced.AddToggle("Highest Entity LOD TCVAR Enabled", &g_waterReflectionHighestEntityLOD_TCVAREnabled);
		advanced.AddToggle("Max Ped Distance Enabled"        , &g_waterReflectionMaxPedDistanceEnabled);
		advanced.AddSlider("Max Ped Distance"                , &g_waterReflectionMaxPedDistance, 0.0f, 200.0f, 1.0f/4.0f);
		advanced.AddToggle("Max Vehicle Distance Enabled"    , &g_waterReflectionMaxVehDistanceEnabled);
		advanced.AddSlider("Max Vehicle Distance"            , &g_waterReflectionMaxVehDistance, 0.0f, 200.0f, 1.0f/4.0f);
		advanced.AddToggle("Max Object Distance Enabled"     , &g_waterReflectionMaxObjDistanceEnabled);
		advanced.AddSlider("Max Object Distance"             , &g_waterReflectionMaxObjDistance, 0.0f, 200.0f, 1.0f/4.0f);
		advanced.AddToggle("Timecycle Enable Force ON"       , &Water::m_bForceWaterReflectionON);
		advanced.AddToggle("Timecycle Enable Force OFF"      , &Water::m_bForceWaterReflectionOFF);
		advanced.AddToggle("Far Clip Enabled"                , &g_waterReflectionFarClipEnabled);
		advanced.AddToggle("Far Clip Override"               , &g_waterReflectionFarClipOverride);
		advanced.AddSlider("Far Clip Distance"               , &g_waterReflectionFarClipDistance, 0.0f, 7000.0f, 1.0f/4.0f);;
		advanced.AddToggle("Can Alpha Cull In PostScan"      , &g_debugWaterReflectionCanAlphaCullInPostScan);

		advanced.AddSeparator();
		advanced.AddButton("Show WATER_REFLECTION_COLOUR", WaterReflectionShowRT_button);
		advanced.AddButton("Show WATER_REFLECTION_DEPTH" , WaterReflectionShowDT_button);
		advanced.AddButton("Show Rendering Stats"        , WaterReflectionShowRenderingStats_button<0>);
		advanced.AddButton("Show Context Stats"          , WaterReflectionShowRenderingStats_button<1>);
		advanced.AddButton("High Quality Mode"           , HighQualityMode_button);
		advanced.AddButton("Default Quality Mode"        , DefaultQualityMode_button);
		advanced.AddButton("Low Quality Mode"            , LowQualityMode_button);

		// quick toggle for all main visibility features
		{
			advanced.AddSeparator();
			advanced.AddToggle("Visibility Enabled", &WaterReflectionVisEnabled, WaterReflectionVisEnabled_changed);
		}
	}

	bkGroup& lodRanges = *group.AddGroup("LOD Range Control", false);
	{
		lodRanges.AddText("Current Entity LOD", &g_waterReflectionHighestEntityLODCurrentLODStr[0], sizeof(g_waterReflectionHighestEntityLODCurrentLODStr), true);

		lodRanges.AddToggle("LOD Mask Enabled", &g_debugWaterReflectionLODMaskEnabled);
		lodRanges.AddToggle("LOD Mask Force Orphan HD", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_WATER_REFLECTION_LOD_MASK_FORCE_ORPHAN_HD);
		lodRanges.AddToggle("LOD Mask Override Enabled", &g_debugWaterReflectionLODMaskOverrideEnabled);
		lodRanges.AddToggle("LOD Mask Override - HD_EXTERIOR", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_HD_EXTERIOR);
		lodRanges.AddToggle("LOD Mask Override - HD_INTERIOR", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_HD_INTERIOR);
		lodRanges.AddToggle("LOD Mask Override - LOD", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_LOD);
		lodRanges.AddToggle("LOD Mask Override - SLOD1", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_SLOD1);
		lodRanges.AddToggle("LOD Mask Override - SLOD2", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_SLOD2);
		lodRanges.AddToggle("LOD Mask Override - SLOD3", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_SLOD3);
		lodRanges.AddToggle("LOD Mask Override - SLOD4", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_SLOD4);
		lodRanges.AddToggle("LOD Mask Override - LIGHT", &g_debugWaterReflectionLODMaskOverride, fwRenderPhase::LODMASK_LIGHT);

		lodRanges.AddSeparator();

		class LODRangeReset_button { public: static void func()
		{
			for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
			{
				g_debugWaterReflectionLODRangeEnabled[i] = true;
				g_debugWaterReflectionLODRange[i][0] = 0.0f;
				g_debugWaterReflectionLODRange[i][1] = 16000.0f;
			}
		}};

		class LODRangeCopyTC_button { public: static void func()
		{
			if (g_waterReflectionLODRangeEnabled_TCVAR)
			{
				for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
				{
					if (g_waterReflectionLODRange_TCVAR[i][1] != -1.0f)
					{
						g_debugWaterReflectionLODRangeEnabled[i] = true;
						g_debugWaterReflectionLODRange[i][0] = g_waterReflectionLODRange_TCVAR[i][0];
						g_debugWaterReflectionLODRange[i][1] = g_waterReflectionLODRange_TCVAR[i][1];
					}
					else
					{
						g_debugWaterReflectionLODRangeEnabled[i] = false;
						g_debugWaterReflectionLODRange[i][0] = 0.0f;
						g_debugWaterReflectionLODRange[i][1] = 16000.0f;
					}
				}
			}
		}};

		lodRanges.AddCombo("LOD Range Mode", &g_debugWaterReflectionLODRangeMode, NELEM(g_waterReflectionTimecycleModeStrings), g_waterReflectionTimecycleModeStrings);
		lodRanges.AddButton("LOD Range Reset", LODRangeReset_button::func);
		lodRanges.AddButton("LOD Range Copy TC", LODRangeCopyTC_button::func);

		for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
		{
			// note that these are not in the same order as eLodType
			const char* LODTypeStrings[] = {"HD", "ORPHANHD", "LOD", "SLOD1", "SLOD2", "SLOD3", "SLOD4"};
			const int   LODTypes[] = {LODTYPES_DEPTH_HD, LODTYPES_DEPTH_ORPHANHD, LODTYPES_DEPTH_LOD, LODTYPES_DEPTH_SLOD1, LODTYPES_DEPTH_SLOD2, LODTYPES_DEPTH_SLOD3, LODTYPES_DEPTH_SLOD4};
			CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(LODTypeStrings));
			CompileTimeAssert(LODTYPES_DEPTH_TOTAL == NELEM(LODTypes));

			lodRanges.AddSeparator();
			lodRanges.AddToggle(atVarString("%s enabled"              , LODTypeStrings[i]).c_str(), &g_debugWaterReflectionLODRangeEnabled[LODTypes[i]]);
			lodRanges.AddSlider(atVarString("%s start"                , LODTypeStrings[i]).c_str(), &g_debugWaterReflectionLODRange[LODTypes[i]][0], -1.0f, 16000.0f, 1.0f/4.0f);
			lodRanges.AddSlider(atVarString("%s end"                  , LODTypeStrings[i]).c_str(), &g_debugWaterReflectionLODRange[LODTypes[i]][1], -1.0f, 16000.0f, 1.0f/4.0f);
			lodRanges.AddText  (atVarString("%s start TCVAR"          , LODTypeStrings[i]).c_str(), &g_waterReflectionLODRange_TCVAR[LODTypes[i]][0]);
			lodRanges.AddText  (atVarString("%s end TCVAR"            , LODTypeStrings[i]).c_str(), &g_waterReflectionLODRange_TCVAR[LODTypes[i]][1]);
			lodRanges.AddText  (atVarString("%s start current"        , LODTypeStrings[i]).c_str(), &g_waterReflectionLODRangeCurrent[LODTypes[i]][0]);
			lodRanges.AddText  (atVarString("%s end current"          , LODTypeStrings[i]).c_str(), &g_waterReflectionLODRangeCurrent[LODTypes[i]][1]);
			lodRanges.AddToggle(atVarString("%s always force up alpha", LODTypeStrings[i]).c_str(), &g_debugWaterReflectionForceUpAlphaForLODTypeAlways[LODTypes[i]]);
			lodRanges.AddToggle(atVarString("%s never force up alpha" , LODTypeStrings[i]).c_str(), &g_debugWaterReflectionForceUpAlphaForLODTypeNever[LODTypes[i]]);
		}

		lodRanges.AddSeparator();
		lodRanges.AddToggle("Force Up Alpha For Selected Entity", &g_debugWaterReflectionForceUpAlphaForSelected);
	}
}

#endif // __BANK

__COMMENT(static) const grcViewport* CRenderPhaseWaterReflectionInterface::GetViewport()
{
	return g_waterReflectionViewport;
}

__COMMENT(static) void CRenderPhaseWaterReflectionInterface::ResetViewport()
{
	g_waterReflectionViewport = NULL;
}

__COMMENT(static) Vec4V_Out CRenderPhaseWaterReflectionInterface::GetCurrentWaterPlane()
{
	return g_waterReflectionCurrentWaterPlane;
}

__COMMENT(static) const CRenderPhaseScanned* CRenderPhaseWaterReflectionInterface::GetRenderPhase()
{
	return g_waterReflectionRenderPhase;
}

__COMMENT(static) int CRenderPhaseWaterReflectionInterface::GetWaterReflectionTechniqueGroupId()
{
	return g_waterReflectionTechniqueGroupId;
}

#if __BANK

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::DrawListPrototypeEnabled()
{
	return g_debugWaterReflectionDrawListPrototypeEnabled;
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::IsUsingMirrorWaterSurface()
{
	return g_debugWaterReflectionIsUsingMirrorWaterSurface;
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::IsSkyEnabled()
{
	return g_debugWaterReflectionRenderSky;
}

__COMMENT(static) u32 CRenderPhaseWaterReflectionInterface::GetWaterReflectionClearColour()
{
	if (BANK_SWITCH(g_debugWaterReflectionRenderSkyBackground, true))
	{
		return Color32(g_waterReflectionSkyColour).GetColor();
	}
	else
	{
		return g_debugWaterReflectionClearColour.GetColor();
	}
}

#endif // __BANK

#if WATER_REFLECTION_OFFSET_TRICK
__COMMENT(static) void CRenderPhaseWaterReflectionInterface::ApplyWaterReflectionOffset(Vec4V_InOut pos, u32 modelIndex)
{
#if __BANK
	pos += Vec4V(g_debugWaterReflectionOffsetGlobal, ScalarV(V_ZERO));

	if (g_debugWaterReflectionOffsetCameraTrack != 0.0f)
	{
		pos += Vec4V(0.0f, 0.0f, g_debugWaterReflectionOffsetCameraTrack*gVpMan.GetRenderGameGrcViewport()->GetCameraPosition().GetZf(), 0.0f);
	}

	if (g_debugWaterReflectionOffsetEnabled)
#endif // __BANK
	{
		const float* height = g_waterReflectionOffsetModelHeightMap.Access(modelIndex);

		if (height)
		{
			// TODO -- implement reprojection based on reflection of current camera in model's water height
			// this will be a somewhat complex bit of math but it would only be done on a small number of entities
			// for now i'm just offsetting the position as a test ..
			pos += Vec4V(0.0f, 0.0f, *height, 0.0f);
		}
	}
}
#endif // WATER_REFLECTION_OFFSET_TRICK

static float g_waterReflectionDistLightFadeout = 0.0f;

__COMMENT(static) void CRenderPhaseWaterReflectionInterface::UpdateDistantLights(Vec3V_In camPos)
{
	const float x = camPos.GetXf();
	const float y = camPos.GetYf();
	const float z = camPos.GetZf();
	const float r = 5.0f;
	const float h = Max<float>(0.0f, z - CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(x - r, y - r, x + r, y + r));

	const float heightAboveGroundFadeStart = BANK_SWITCH(g_debugWaterReflectionDistLightFadeoutMinHeight, 30.0f);
	const float heightAboveGroundFadeStop  = BANK_SWITCH(g_debugWaterReflectionDistLightFadeoutMaxHeight, 90.0f);
	const float interpolateAmount          = BANK_SWITCH(g_debugWaterReflectionDistLightFadeoutSpeed, 0.1f);

	static float heightAboveGround = 0.0f;

	if (camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation))
	{
		heightAboveGround = h; // snap it
	}
	else
	{
		heightAboveGround += (h - heightAboveGround)*interpolateAmount; // smooth it
	}

	g_waterReflectionDistLightFadeout = Clamp<float>((heightAboveGround - heightAboveGroundFadeStart)/(heightAboveGroundFadeStop - heightAboveGroundFadeStart), 0.0f, 1.0f);
}

__COMMENT(static) void CRenderPhaseWaterReflectionInterface::AdjustDistantLights(float& intensityScale)
{
	intensityScale *= 1.0f - g_waterReflectionDistLightFadeout;
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::IsOcclusionDisabledForOceanWaves()
{
	return g_waterReflectionOcclusionDisabledForOceanWaves;
}

__COMMENT(static) void CRenderPhaseWaterReflectionInterface::UpdateInWater(CPhysical* pPhysical, bool bInWater)
{
	if (BANK_SWITCH(g_debugWaterReflectionRenderAllPhysicalEntities, true))
	{
		bInWater = true;
	}
	else if (pPhysical->GetIsTypeVehicle() && static_cast<CVehicle*>(pPhysical)->InheritsFromBoat())
	{
		bInWater = true; // boats are always considered to be in water
	}

	pPhysical->GetRenderPhaseVisibilityMask().ChangeFlag(VIS_PHASE_MASK_WATER_REFLECTION, bInWater);
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(const CEntity* pEntity)
{
	const fwLodData& lodData = pEntity->GetLodData();

#if __BANK
	if (g_debugWaterReflectionForceUpAlphaForSelected)
	{
		if (pEntity == g_PickerManager.GetSelectedEntity())
		{
			return true;
		}
	}

	if (g_debugWaterReflectionForceUpAlphaForLODTypeAlways[lodData.GetLodType()])
	{
		return true;
	}
	else if (g_debugWaterReflectionForceUpAlphaForLODTypeNever[lodData.GetLodType()])
	{
		return false;
	}
#endif // __BANK

	if (g_waterReflectionLODRangeCurrentEnabled)
	{
		return true; // assume lod ranges are basically non-overlapping i guess
	}

	if (lodData.IsHighDetail() == false &&
		lodData.GetLodType() == (u32)g_waterReflectionHighestEntityLODCurrentLOD &&
		lodData.GetChildDrawing() && // camera is close enough so that children might be drawing
		pEntity->IsBaseFlagSet(fwEntity::HAS_NON_WATER_REFLECTION_PROXY_CHILD))
	{
		return true;
	}

	return false;
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::CanAlphaCullInPostScan(const CEntity* pEntity)
{
	if (BANK_SWITCH(g_debugWaterReflectionCanAlphaCullInPostScan, true))
	{
		return !ShouldEntityForceUpAlpha(pEntity);
	}

	return false;
}

__COMMENT(static) bool CRenderPhaseWaterReflectionInterface::CanAlphaCullInPostScan()
{
	return BANK_SWITCH(g_debugWaterReflectionCanAlphaCullInPostScan, true);
}

__COMMENT(static) void CRenderPhaseWaterReflectionInterface::SetScriptWaterHeight(bool bEnable, float height)
{
	g_waterReflectionScriptWaterHeightEnabled = bEnable;
	g_waterReflectionScriptWaterHeight = height;
}

__COMMENT(static) void CRenderPhaseWaterReflectionInterface::SetScriptObjectVisibility(bool bForceVisible)
{
	g_waterReflectionScriptObjectVisibility = bForceVisible;
}
