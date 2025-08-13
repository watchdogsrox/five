//
//
//
//
#include "bank/bank.h"
#include "grcore/allocscope.h"
#include "profile/cputrace.h"
#include "profile/timebars.h"
#include "system/nelem.h"

// RAGE framework includes
#include "fwdrawlist/drawlistmgr.h"
#include "fwpheffects/ropemanager.h"
#include "fwrenderer/renderlistbuilder.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/VisibilityFlags.h"
#include "fwscene/world/EntityDesc.h"
#include "fwutil/xmacro.h"
#include "fwscene/scan/Scan.h"

// GTA includes
#include "entities/EntityDrawHandler.h"
#include "physics/physics.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/Entities/InstancedEntityRenderer.h"
#include "renderer/lights/lights.h"
#include "renderer/PlantsGrassRenderer.h"
#include "renderer/PlantsMgr.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/shadows/shadows.h"
#include "renderer/TreeImposters.h"
#include "renderer/PlantsGrassRendererSwitches.h"
#include "scene/lod/LodDrawable.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/world/GameWorld.h"
#include "scene/world/VisibilityMasks.h"
#include "shaders/CustomShaderEffectTree.h"
#include "vfx/misc/DistantLights.h"
#include "Objects/object.h"
#include "Vehicles/vehicle.h"
#include "renderer/rendertargets.h"
#include "vfx/particles/ptfxmanager.h"

RENDER_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

//////////////////////
// Extern & globals //
//////////////////////

static grcBlendStateHandle s_normal_alphatest_BS;
static grcBlendStateHandle s_normal_cutout_BS;
static grcBlendStateHandle s_shadows_cutout_BS;
static grcBlendStateHandle s_shadows_cutout_alpha_BS; // uses AlphaToCoverage

float s_DebugShadowsLowerLODFactor=0.25f;

#if __BANK

bool s_DebugEnableTessellationInShadows=false;
bool s_DebugAlwaysRenderShadow=false;
bool s_DebugAlwaysRenderReflection=false;
bool s_DebugOnlyRenderCutoutShadow=false;
bool s_DisableDynamicObjects = false;
bool s_DebugSkipTestPasses=true;
bool CRenderListBuilder::s_RenderListBuilderEnableScissorClamping = true;

static const char *s_RenderPassNames[] = {
	"Visible",
	"LOD",
	"Cutout",
	"Decal",
	"Fading",
	"Alpha",
	"Water",
	"Tree",
};
CompileTimeAssert(NELEM(s_RenderPassNames) == RPASS_NUM_RENDER_PASSES);

#endif // __BANK

// Internal shortcuts to make the list easier to read.
//const u32 FORWARD_RENDER				= CRenderListBuilder::USE_FORWARD_LIGHTING;
const u32	DEFERRED_LIGHT				= CRenderListBuilder::USE_DEFERRED_LIGHTING BANK_ONLY(|CRenderListBuilder::USE_DEBUGMODE);
const u32	DEFERRED_LIGHT_NO_DEBUG		= CRenderListBuilder::USE_DEFERRED_LIGHTING;
const u32 	SIMPLE_LIGHT				= CRenderListBuilder::USE_SIMPLE_LIGHTING;
const u32	SIMPLE_LIGHTFADE			= CRenderListBuilder::USE_SIMPLE_LIGHTING_DISTANCE_FADE;
const u32	NO_LIGHTING					= CRenderListBuilder::USE_NO_LIGHTING;

const u32	RENDER_FADING				= CRenderListBuilder::RENDER_FADING;
const u32	FADING_ONLY					= CRenderListBuilder::RENDER_FADING_ONLY;
const u32	DONT_RENDER_TREES			= CRenderListBuilder::RENDER_DONT_RENDER_TREES;
const u32	DONT_RENDER_FOLIAGE			= CRenderListBuilder::RENDER_DONT_RENDER_TREES | CRenderListBuilder::RENDER_DONT_RENDER_PLANTS;
const u32	DONT_RENDER_DECALS_CUTOUTS	= CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS;

	// Internal render filters
// #if !__FINAL
//const u32	RENDER_FILTER_UNUSED_FLAG					= 0x0010000;
const u32	RENDER_FADE_PROPS							= 0x0020000;

#if USE_EDGE
const u32	RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS		= 0x0040000;
const u32	RENDER_FILTER_DISABLE_EDGE_SHADOWS			= 0x0080000;
#endif // USE_EDGE

const u32	RENDER_FILTER_POINT_LIGHT_SHADOWS			= 0x0100000;

const u32	RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS	= 0x0200000;
const u32	RENDER_FILTER_DISABLE_TREE_LEAF_SHADOWS		= 0x0400000;

#if !USE_EDGE
const u32	RENDER_FILTER_DISABLE_VEHICLE_SHADOWS		= 0x0800000;
const u32	RENDER_FILTER_DISABLE_CUTOUT_SHADOWS		= 0x1000000;
const u32	RENDER_FILTER_DISABLE_FADING_SHADOWS		= 0x2000000;
#endif

const u32	RENDER_FORCE_ALPHA							= 0x4000000;

// #endif // !__FINAL

#define SUPPORT_TESSELLATION_IN_SHADOWS __BANK

// Only issue the SSA conditional queries on platforms where we will use the results
#define SSA_QUERY_BEGIN (SSA_USES_CONDITIONALRENDER ? RENDERFLAG_BEGIN_RENDERPASS_QUERY : 0)
#define SSA_QUERY_END   (SSA_USES_CONDITIONALRENDER ? RENDERFLAG_END_RENDERPASS_QUERY   : 0)

fwDrawListAddPass s_OpaquePass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, RENDERFLAG_CUTOUT_FORCE_CLIP},

	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		~(SIMPLE_LIGHTFADE | NO_LIGHTING),	0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, 0},
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHTFADE,					0,					DONT_RENDER_DECALS_CUTOUTS | FADING_ONLY,	RM_NORMAL, 0},

	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~NO_LIGHTING,						RENDER_FADING,		0,											RM_NO_FADE, 0},
	{ RPASS_FADING,		CRenderer::RB_CUTOUT,		~NO_LIGHTING,						RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NO_FADE, 0},
};

fwDrawListAddPass s_OpaquePassSSA[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, RENDERFLAG_CUTOUT_FORCE_CLIP},

	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},

	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		~(SIMPLE_LIGHTFADE | NO_LIGHTING),	0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, SSA_QUERY_BEGIN},										// SSA Alpha
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHTFADE,					0,					DONT_RENDER_DECALS_CUTOUTS | FADING_ONLY,	RM_NORMAL, 0},														// SSA Alpha
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		 DEFERRED_LIGHT,					0,					DONT_RENDER_DECALS_CUTOUTS | FADING_ONLY,	RM_NORMAL, RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA},					// SSA Color
	{ RPASS_FADING,		CRenderer::RB_CUTOUT,		~NO_LIGHTING,						RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NO_FADE, 0},														// SSA Alpha
	{ RPASS_FADING,		CRenderer::RB_CUTOUT,		~NO_LIGHTING,						RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NO_FADE, RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA|SSA_QUERY_END},	// SSA Color

	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~NO_LIGHTING,						RENDER_FADING,		0,											RM_NO_FADE, 0}, 
};

fwDrawListAddPass s_OpaquePass_SinglePassSSA[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, RENDERFLAG_CUTOUT_FORCE_CLIP},

	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		~(SIMPLE_LIGHTFADE | NO_LIGHTING),	0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA},
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHTFADE,					0,					DONT_RENDER_DECALS_CUTOUTS | FADING_ONLY,	RM_NORMAL, 0},

	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~NO_LIGHTING,						RENDER_FADING,		0,											RM_NO_FADE, 0},
	{ RPASS_FADING,		CRenderer::RB_CUTOUT,		~NO_LIGHTING,						RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NO_FADE, RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA},
};

#if __BANK
fwDrawListAddPass s_OpaquePass_SSA_Foliage[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHT | DEFERRED_LIGHT,		0,					DONT_RENDER_TREES,							RM_NORMAL, 0},

	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		 DEFERRED_LIGHT,					0,					DONT_RENDER_TREES,							RM_NORMAL, RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA},

	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~SIMPLE_LIGHTFADE,					0,					0,											RM_NORMAL, 0},
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		 SIMPLE_LIGHTFADE,					0,					FADING_ONLY,								RM_NORMAL, 0},
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		~(SIMPLE_LIGHTFADE | NO_LIGHTING),	0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, 0},
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		 SIMPLE_LIGHTFADE,					0,					DONT_RENDER_DECALS_CUTOUTS | FADING_ONLY,	RM_NORMAL, 0},

	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		 DEFERRED_LIGHT,					0,					DONT_RENDER_DECALS_CUTOUTS | FADING_ONLY,	RM_NORMAL, RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA},

	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~NO_LIGHTING,						RENDER_FADING,		0,											RM_NO_FADE, 0},
	{ RPASS_FADING,		CRenderer::RB_CUTOUT,		~NO_LIGHTING,						RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NO_FADE, 0},
};
#endif // __BANK

fwDrawListAddPass s_DecalPass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_DECAL,		CRenderer::RB_DECAL,		 DEFERRED_LIGHT_NO_DEBUG,			0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, 0},
	{ RPASS_FADING,		CRenderer::RB_DECAL,		 DEFERRED_LIGHT_NO_DEBUG,			RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, 0},
};

fwDrawListAddPass s_AlphaPass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_FADING,		CRenderer::RB_ALPHA,		~0U,								FADING_ONLY,		0,											RM_NOT_DEFERRED,	0},	// TODO: No alpha as cut-outs in deferred
	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		 DEFERRED_LIGHT_NO_DEBUG,			RENDER_FADE_PROPS,	0,											RM_NOT_DEFERRED,	RENDERFLAG_SKIP_FORCE_ALPHA},
	{ RPASS_ALPHA,		CRenderer::RB_ALPHA,		~0U,								0,					0,											RM_NOT_DEFERRED,	0},	// TODO: No alpha as cut-outs in deferred
#if __BANK
	{ RPASS_FADING,		CRenderer::RB_DECAL,		CRenderListBuilder::USE_DEBUGMODE,	RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NO_FADE,			0 },
	{ RPASS_DECAL,		CRenderer::RB_DECAL,		CRenderListBuilder::USE_DEBUGMODE,	0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL,			0 },
#endif // __BANK
};

// same as s_AlphaPass, except that FADING OPAQUE is allowed in forward lighting
fwDrawListAddPass s_MirrorAlphaPass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes											* Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_FADING,		CRenderer::RB_ALPHA,		~0U,														FADING_ONLY,		0,											RM_NOT_DEFERRED,	0},	// TODO: No alpha as cut-outs in deferred
	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~NO_LIGHTING,												RENDER_FADE_PROPS,	0,											RM_NOT_DEFERRED,	0},
	{ RPASS_ALPHA,		CRenderer::RB_ALPHA,		~0U,														0,					0,											RM_NOT_DEFERRED,	0},	// TODO: No alpha as cut-outs in deferred
	{ RPASS_DECAL,		CRenderer::RB_DECAL,		~0U,														0,					DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, 0},
	{ RPASS_FADING,		CRenderer::RB_DECAL,		~0U,														RENDER_FADING,		DONT_RENDER_DECALS_CUTOUTS,					RM_NORMAL, 0},
};

fwDrawListAddPass s_DisplAlphaPass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_FADING,		CRenderer::RB_DISPL_ALPHA,	~0U,								FADING_ONLY,		0,											RM_NOT_DEFERRED, 0},
	{ RPASS_ALPHA,		CRenderer::RB_DISPL_ALPHA,	~0U,								0,					0,											RM_NOT_DEFERRED, 0}
};

fwDrawListAddPass s_WaterPass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_WATER,		CRenderer::RB_WATER,		~0U,								0,					0,											RM_NOT_DEFERRED, 0},
};

//Water refraction alpha has only force alpha for now
fwDrawListAddPass s_ForcedAlphaPass[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NOT_DEFERRED,	RENDERFLAG_ONLY_FORCE_ALPHA},
};


#if USE_EDGE

fwDrawListAddPass s_ShadowNonEdgePPU[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
/**/{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS,		RM_NORMAL, 0},
};

fwDrawListAddPass s_ShadowEdgePPU[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_EDGE_SHADOWS,			RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_EDGE_SHADOWS,			RM_NORMAL, 0},	// TODO: No alpha as cut-outs in deferred
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_EDGE_SHADOWS | RENDER_FILTER_POINT_LIGHT_SHADOWS | RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS,		RM_NORMAL, 0},	// TODO: No alpha as cut-outs in deferred
};

fwDrawListAddPass s_OtherShadowEdgePPU[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		~0U,								0,					RENDER_FILTER_DISABLE_EDGE_SHADOWS,			RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		~0U,								0,					RENDER_FILTER_DISABLE_EDGE_SHADOWS | RENDER_FILTER_POINT_LIGHT_SHADOWS | RENDER_FILTER_DISABLE_TREE_LEAF_SHADOWS,		RM_NORMAL, 0},	// TODO: No alpha as cut-outs in deferred
};

#else // not USE_EDGE

fwDrawListAddPass s_UnskinnedShadows[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS | RENDER_FILTER_POINT_LIGHT_SHADOWS,		RM_NORMAL, 0},
};

fwDrawListAddPass s_SkinnedShadows[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter              Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NORMAL, 0},
};

#if __D3D11
fwDrawListAddPass s_MixedTechniqueShadows[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NORMAL, 0},
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~0U,								0,					0,											RM_NORMAL, 0},
	{ RPASS_TREE,		CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS,		RM_NORMAL, 0},
};
#endif // __D3D11

fwDrawListAddPass s_OtherOpaqueFadeShadows[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	// Vehicle Shadows
	{ RPASS_VISIBLE,	CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_VEHICLE_SHADOWS,		RM_NORMAL, 0},
	{ RPASS_LOD,		CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_VEHICLE_SHADOWS,		RM_NORMAL, 0},
	// Fading Shadows
	{ RPASS_FADING,		CRenderer::RB_OPAQUE,		~0U,								0,					RENDER_FILTER_DISABLE_FADING_SHADOWS,		RM_NORMAL, 0},
};

fwDrawListAddPass s_OtherShadows[] = {
	//Render Pass     * Render Bucket              * Allowed draw modes               * Filter            * Exclusion Filter                          * Render Mode Mapping, Flags
	// Cutout Shadows
	{ RPASS_CUTOUT,		CRenderer::RB_CUTOUT,		~0U,								0,					RENDER_FILTER_DISABLE_CUTOUT_SHADOWS,		RM_NORMAL, 0},
	// Fading Shadows
	{ RPASS_FADING,		CRenderer::RB_CUTOUT,		~0U,								0,					RENDER_FILTER_DISABLE_FADING_SHADOWS,		RM_NORMAL, 0},
	// Tree Leaf Shadows
	{ RPASS_TREE,		CRenderer::RB_CUTOUT,		~0U,								0,					RENDER_FILTER_DISABLE_TREE_LEAF_SHADOWS | RENDER_FILTER_POINT_LIGHT_SHADOWS,		RM_NORMAL, 0},
};

fwDrawListAddPass s_AlphaShadows[] = {
	{ RPASS_VISIBLE,	CRenderer::RB_ALPHA,		~0U,								0,					DONT_RENDER_FOLIAGE | RENDER_FILTER_DISABLE_CUTOUT_SHADOWS,	RM_NOT_DEFERRED,	0},	// TODO: No alpha as cut-outs in deferred
};
#endif // not USE_EDGE

// These are global RENDER_FILTER_* flags that are always OR'ed to the other flags.
// This variable can be modified either through RAG, or through the debugger.
u32 s_GlobalRenderListFlags = RENDER_FADE_PROPS PS3_ONLY(| RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS);

#if ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK	
DECLARE_MTR_THREAD fwRenderListBuilder::AddEntityCallback CRenderListBuilder::sm_AddEntityCallbackLocalShadow=NULL;
#endif // ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK	

#if RENDERLIST_DUMP_PASS_INFO

int  g_RenderListDumpPassInfo = 0;
bool g_RenderListDumpPassFlag = false;


void CRenderListBuilder::DumpPassInfo(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, const fwRenderListBuilder::AddEntityFlags& addEntityFlags, fwRenderGeneralFlags generalFlags, int count, int total)
{
	if (g_RenderListDumpPassFlag)
	{
		const char* rpassStrTable[] =
		{
			"RPASS_VISIBLE",
			"RPASS_LOD    ",
			"RPASS_CUTOUT ",
			"RPASS_DECAL  ",
			"RPASS_FADING ",
			"RPASS_ALPHA  ",
			"RPASS_WATER  ",
			"RPASS_TREE   ",
		};
		CompileTimeAssert(NELEM(rpassStrTable) == RPASS_NUM_RENDER_PASSES);

		const char* bucketStrTable[] =
		{
			"RB_OPAQUE  ",
			"RB_ALPHA   ",
			"RB_DECAL   ",
			"RB_CUTOUT  ",
			"RB_NOSPLASH",
			"RB_NOWATER ",
			"RB_WATER   ",
			"RB_DISPL   ", // RB_DISPL_ALPHA
		};
		CompileTimeAssert(NELEM(bucketStrTable) == CRenderer::RB_NUM_BASE_BUCKETS);

		const char* rmStr = "";
		switch (renderMode)
		{
		case rmNIL            : rmStr = "rmNIL"           ; break;
		case rmStandard       : rmStr = "rmStandard"      ; break;
		case rmSimple         : rmStr = "rmSimple"        ; break;
		case rmGBuffer        : rmStr = "rmGBuffer"       ; break;
		case rmSimpleNoFade   : rmStr = "rmSimpleNoFade"  ; break;
		case rmSimpleDistFade : rmStr = "rmSimpleDistFade"; break;
#if HAS_RENDER_MODE_SHADOWS
		case rmShadows        : rmStr = "rmShadows"       ; break;
		case rmShadowsSkinned : rmStr = "rmShadowsSkinned"; break;
#endif // HAS_RENDER_MODE_SHADOWS
		case rmSpecial        : rmStr = "rmSpecial"       ; break;
		case rmNoLights       : rmStr = "rmNoLights"      ; break;
//#if __BANK
		case rmDebugMode      : rmStr = "rmDebugMode"     ; break;
//#endif // __BANK
		}

		atString baseReqStr = atString("");
		atString baseExclStr = atString("");
		atString renderReqStr = atString("");
		atString renderExclStr = atString("");

		const char* baseFlagStrTable[] = // fwEntity flags
		{
			"IS_VISIBLE",
			"IS_SEARCHABLE",
			"HAS_OPAQUE",
			"HAS_ALPHA",
			"HAS_DECAL",
			"HAS_CUTOUT",
			"HAS_WATER",
			"HAS_DISPL_ALPHA",
			"HAS_PRERENDER",
			"IS_LIGHT",
			"USE_SCREENDOOR",
			"SUPPRESS_ALPHA",
			"FORCE_ALPHA",
			"RENDER_SMALL_SHADOW",
			"OK_TO_PRERENDER",
			"HAS_NON_WATER_REFLECTION_PROXY_CHILD",
			"IS_DYNAMIC",
			"IS_FIXED",
			"IS_FIXED_BY_NETWORK",
			"SPAWN_PHYS_ACTIVE",
			"IS_TIMED_ENTITY",
			"DONT_STREAM",
			"LOW_PRIORITY_STREAM",
			"HAS_PHYSICS_DICT",
			"REMOVE_FROM_WORLD",
			"FORCED_ZERO_ALPHA",
			"DRAW_LAST",
			"DRAW_FIRST_SORTED",
			"NO_INSTANCED_PHYS",
			"HAS_HD_TEX_DIST",
			"PROTECT_ARCHETYPE",
		};

		for (int i = 0; i < 32; i++)
		{
			if ((addEntityFlags.requiredBaseFlags >> i)&1)
			{
				if (baseReqStr.length() > 0) { baseReqStr += ","; }
				if (i < NELEM(baseFlagStrTable)) { baseReqStr += baseFlagStrTable[i]; } else { baseReqStr += "?"; }
			}

			if ((addEntityFlags.excludeBaseFlags >> i)&1)
			{
				if (baseExclStr.length() > 0) { baseExclStr += ","; }
				if (i < NELEM(baseFlagStrTable)) { baseExclStr += baseFlagStrTable[i]; } else { baseExclStr += "?"; }
			}
		}

		const char* renderFlagStrTable[] =
		{
			// subphase flags
			"SUBPHASE_CASCADE_0",
			"SUBPHASE_CASCADE_1",
			"SUBPHASE_CASCADE_2",
			"SUBPHASE_CASCADE_3",
			"SUBPHASE_REFLECT_FACET0",
			"SUBPHASE_REFLECT_FACET1",
			"SUBPHASE_REFLECT_FACET2",
			"SUBPHASE_REFLECT_FACET3",
			"SUBPHASE_REFLECT_FACET4",
			"SUBPHASE_REFLECT_FACET5",

			// CEntity flags
#if HAS_RENDER_MODE_SHADOWS
			"RenderFlag_USE_CUSTOMSHADOWS",
#endif // HAS_RENDER_MODE_SHADOWS
			"RenderFlag_IS_DYNAMIC",
			"RenderFlag_IS_PED",
			"RenderFlag_IS_GROUND",
			"RenderFlag_IS_BUILDING",
			"RenderFlag_IS_SKINNED",
		};

		for (int i = 0; i < 32; i++)
		{
			if ((addEntityFlags.requiredRenderFlags >> i)&1)
			{
				if (renderReqStr.length() > 0) { renderReqStr += ","; }
				if (i < NELEM(renderFlagStrTable)) { renderReqStr += renderFlagStrTable[i]; } else { renderReqStr += "?"; }
			}

			if ((addEntityFlags.excludeRenderFlags >> i)&1)
			{
				if (renderExclStr.length() > 0) { renderExclStr += ","; }
				if (i < NELEM(renderFlagStrTable)) { renderExclStr += renderFlagStrTable[i]; } else { renderExclStr += "?"; }
			}
		}

		Displayf(
			"  EntityList: count=%04d/%04d, id=%s, bucket=%s, mode=%s, baseReq=[%s], baseExcl=[%s], renderReq=[%s], renderExcl=[%s], useAlpha=%s%s",
			count,
			total,
			rpassStrTable[id],
			bucketStrTable[bucket],
			rmStr,
			baseReqStr.c_str(),
			baseExclStr.c_str(),
			renderReqStr.c_str(),
			renderExclStr.c_str(),
			addEntityFlags.useAlpha ? "TRUE" : "FALSE",
			generalFlags ? atVarString(" generalFlags=0x%02x", generalFlags).c_str() : ""
		);
	}
}

#endif // RENDERLIST_DUMP_PASS_INFO

// Custom draw list commands
class dlCmdPlantMgrRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_PlantMgrRender,
	};

	s32 GetCommandSize()							{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)	{ CPlantMgr::Render(); }
};

class dlCmdPlantMgrRenderDecal : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_PlantMgrRenderDecal,
	};

	s32 GetCommandSize()							{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)	{ CPlantMgr::RenderDecal(); }
};

#if ENABLE_DISTANT_CARS
class dlCmdDistantCarsRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_DistantCarsRender,
	};

	s32 GetCommandSize()							{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)		{ tDistantLightRenderer::RenderDistantCars(); }
};
#endif //ENABLE_DISTANT_CARS


#if PLANTS_CAST_SHADOWS

class dlCmdPlantMgrShadowRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_PlantMgrShadowRender,
	};

	s32 GetCommandSize()							{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)	{ CPlantMgr::ShadowRender(); }
};

#endif //PLANTS_CAST_SHADOWS

class dlCmdRopeShadowRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_RopeShadowRender,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & /*cmd*/)
	{ 
		CPhysics::GetRopeManager()->ShadowDraw(RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx()));
	}
};

class dlCmdParticleShadowRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ParticleShadowRender,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & cmd) { ((dlCmdParticleShadowRender &) cmd).Execute(); }

	void Execute()
	{ 
#if RMPTFX_USE_PARTICLE_SHADOWS
		g_ptFxManager.RenderShadows(m_CascadeIndex);
#endif
	}

	dlCmdParticleShadowRender( const s32 cascadeIndex ) {m_CascadeIndex = cascadeIndex;}
private:
	s32 m_CascadeIndex;
};


class dlCmdParticleShadowRenderAllCascades : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ParticleShadowRenderAllCascades,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase & cmd) { ((dlCmdParticleShadowRenderAllCascades &) cmd).Execute(); }

	void Execute()
	{ 
#if RMPTFX_USE_PARTICLE_SHADOWS && PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
		g_ptFxManager.RenderShadowsAllCascades(m_noOfCascades, m_viewports, m_windows, m_bUseInstancedShadow);
#endif
	}

	dlCmdParticleShadowRenderAllCascades(s32 noOfCascades, grcViewport *pViewports, Vec4V *pWindows, bool bUseInstancedShadow) 
	{
		s32 i;
		m_noOfCascades = noOfCascades;
		m_bUseInstancedShadow = bUseInstancedShadow;

		for(i=0; i<m_noOfCascades; i++)
		{
			m_viewports[i] = pViewports[i];
			m_windows[i] = pWindows[i];
		}
	}
private:
	s32 m_noOfCascades;
	grcViewport m_viewports[4];
	Vec4V m_windows[4];
	bool m_bUseInstancedShadow;
};


//
// name:		CRenderListBuilder::InitDLCCommands()
// description:	Register all draw list commands for this system.
/** PURPOSE: Initialization for the render list builder system.
 */
void CRenderListBuilder::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdPlantMgrRender);
	DLC_REGISTER_EXTERNAL(dlCmdPlantMgrRenderDecal);
#if ENABLE_DISTANT_CARS
	DLC_REGISTER_EXTERNAL(dlCmdDistantCarsRender);
#endif //ENABLE_DISTANT_CARS
#if PLANTS_CAST_SHADOWS
	DLC_REGISTER_EXTERNAL(dlCmdPlantMgrShadowRender);
#endif 
	DLC_REGISTER_EXTERNAL(dlCmdRopeShadowRender);

#if RMPTFX_USE_PARTICLE_SHADOWS
	DLC_REGISTER_EXTERNAL(dlCmdParticleShadowRender);
#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
	DLC_REGISTER_EXTERNAL(dlCmdParticleShadowRenderAllCascades);
#endif
#endif

	fwRenderListBuilder::Init(RPASS_NUM_RENDER_PASSES, eDrawMode_MAX, CRenderer::RB_HIGHEST_BUCKET);

	// one for each eDrawMode (normal, no fading, forward)
	fwRenderListBuilder::SetDrawModeInfo(USE_FORWARD_LIGHTING             , rmStandard      , rmStandard      , rmStandard      , "Forward Lighting");
	fwRenderListBuilder::SetDrawModeInfo(USE_SIMPLE_LIGHTING              , rmSimple        , rmSimpleNoFade  , rmSimpleNoFade  , "Simple Lighting");
	fwRenderListBuilder::SetDrawModeInfo(USE_DEFERRED_LIGHTING            , rmGBuffer       , rmGBuffer       , rmStandard      , "Deferred Lighting");
	fwRenderListBuilder::SetDrawModeInfo(USE_SIMPLE_LIGHTING_DISTANCE_FADE, rmSimpleDistFade, rmSimpleDistFade, rmSimpleDistFade, "Simple Lighting Distance Fade");
	fwRenderListBuilder::SetDrawModeInfo(USE_NO_LIGHTING                  , rmNoLights      , rmNoLights      , rmNoLights      , "No Lighting");
#if HAS_RENDER_MODE_SHADOWS
	fwRenderListBuilder::SetDrawModeInfo(USE_SHADOWS                      , rmShadows       , rmShadows       , rmShadows       , "Unskinned Shadows");
	fwRenderListBuilder::SetDrawModeInfo(USE_SKINNED_SHADOWS              , rmShadowsSkinned, rmShadowsSkinned, rmShadowsSkinned, "Skinned Shadows");
#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	fwRenderListBuilder::SetDrawModeInfo(USE_SHADOWS_SPECIAL              , rmSpecial       , rmSpecial       , rmSpecial       , "Special Unskinned Shadows");
	fwRenderListBuilder::SetDrawModeInfo(USE_SKINNED_SHADOWS_SPECIAL      , rmSpecialSkinned, rmSpecialSkinned, rmSpecialSkinned, "Special Skinned Shadows");
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES
#endif // HAS_RENDER_MODE_SHADOWS
#if __BANK
	fwRenderListBuilder::SetDrawModeInfo(USE_DEBUGMODE                    , rmDebugMode, rmDebugMode, rmDebugMode, "Debug Mode");
#endif // __BANK

#if __BANK
	// Debugging render pass names
	for (int x=0; x<RPASS_NUM_RENDER_PASSES; x++) {
		fwRenderListBuilder::SetRenderPassName(x, s_RenderPassNames[x]);
	}

	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_OPAQUE,		"Opaque");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_ALPHA,		"Alpha");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_DECAL,		"Decal");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_CUTOUT,		"Cutout");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_NOSPLASH,	"NoSplash");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_NOWATER,		"NoWater");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_WATER,		"Water");
	fwRenderListBuilder::SetRenderBucketName(CRenderer::RB_DISPL_ALPHA,	"Displacement Alpha");
#endif // __BANK

	// Create state blocks while we're at it...
	{
		grcBlendStateDesc BSDesc;
		
		BSDesc.BlendRTDesc[0].BlendEnable = false;
		
		BSDesc.AlphaToCoverageEnable = true;
		BSDesc.AlphaToMaskOffsets = grcRSV::ALPHATOMASKOFFSETS_DITHERED;

		s_normal_cutout_BS = grcStateBlock::CreateBlendState(BSDesc);	
	}
	{
		grcBlendStateDesc BSDesc;


		grcBlendStateDesc::grcRenderTargetBlendDesc& rt = BSDesc.BlendRTDesc[0];
		rt.BlendEnable = true;
		rt.DestBlend = rt.DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		rt.SrcBlend = rt.SrcBlendAlpha = grcRSV::BLEND_SRCALPHA; 
		rt.BlendOp = rt.BlendOpAlpha = grcRSV::BLENDOP_ADD;

#if APPLY_DOF_TO_ALPHA_DECALS
		if (GRCDEVICE.GetDxFeatureLevel() > 1000)
			PostFX::SetupBlendForDOF(BSDesc, true);
#endif //APPLY_DOF_TO_ALPHA_DECALS

		s_normal_alphatest_BS = grcStateBlock::CreateBlendState(BSDesc);	
	}
	

	{
		grcBlendStateDesc BSDesc;
		
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
#endif
		
		s_shadows_cutout_BS = grcStateBlock::CreateBlendState(BSDesc);
	}
	
	{
		grcBlendStateDesc BSDesc;
		
		BSDesc.AlphaToCoverageEnable = true;
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
#endif
		
		s_shadows_cutout_alpha_BS = grcStateBlock::CreateBlendState(BSDesc);
	}
}


int CRenderListBuilder::GetOpaqueDrawListAddPassRowCount()
{
	int ret;
	GetOpaqueDrawListAddPassAndSize(ret);
	return ret;
}

fwDrawListAddPass *CRenderListBuilder::GetOpaqueDrawListAddPass()
{
	int nelements;
	return GetOpaqueDrawListAddPassAndSize(nelements);
}

fwDrawListAddPass *CRenderListBuilder::GetOpaqueDrawListAddPassAndSize(int &nelements)
{
	if(PostFX::UseSubSampledAlpha())
	{
		if ( PostFX::UseSinglePassSSA() )
		{
			nelements = NELEM(s_OpaquePass_SinglePassSSA);
			return s_OpaquePass_SinglePassSSA;
		}
#if __BANK
		else if ( PostFX::UseSSAOnFoliage() )
		{
			nelements = NELEM(s_OpaquePass_SSA_Foliage);
			return s_OpaquePass_SSA_Foliage;
		}
		else
#endif // __BANK
		{
			nelements = NELEM(s_OpaquePassSSA);
			return s_OpaquePassSSA;
		}
	}
	else
	{
		nelements = NELEM(s_OpaquePass);
		return s_OpaquePass;
	}
}


int CRenderListBuilder::GetEntityListSize(s32 list)
{
	int ret = 0;

	for(int i=RPASS_VISIBLE; i<RPASS_NUM_RENDER_PASSES; i++)
	{
		ret += fwRenderListBuilder::EntityListSize(list, (fwRenderPassId)i);
	}
	return ret;
}

static DECLARE_MTR_THREAD fwRenderListBuilder::RenderStateCallback s_CustomSetupCallback = NULL;
static DECLARE_MTR_THREAD fwRenderListBuilder::RenderStateCallback s_CustomCleanupCallback = NULL;
static DECLARE_MTR_THREAD fwRenderListBuilder::CustomFlagsChangedCallback s_CustomFlagsChangedCallback = NULL;
//
// name:		CScene::AddToDrawList()
// description:	Render game
/** PURPOSE: This is the main entry point to create a draw list that renders the entire scene
 * (using certain filters) in a non-shadow pass.
 *
 *  PARAMS:
 *  list - This is the index of the render list whose entities will be scanned and added if they are
 *         not rejected.
 *  fullFilter - This is a bitmask with flags that will decide which render list passes will and will not be executed.
 *               These are typically RENDER_* values. This bitmask also includes which parts of the scene to
 *               render (all bits that fall in the RENDER_MASK mask).
 *  drawMode - The draw mode to use for rendering.
 */
void CRenderListBuilder::AddToDrawList(s32 list, int fullFilter, eDrawMode drawmode, int subphasePass, 
									   fwRenderListBuilder::RenderStateCallback customSetupCallback, 
									   fwRenderListBuilder::RenderStateCallback customCleanupCallback,
									   fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback,
									   int RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY(entityStart), 
									   int RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY(entityStride))
{
	const int filter			=  (fullFilter&RENDER_MASK);
	const bool bDrawPlantsMgr	= !(fullFilter&RENDER_DONT_RENDER_PLANTS) != 0;

	PF_PUSH_TIMEBAR_DETAIL("Render Scene");

	BANK_ONLY(if(drawmode != USE_DEBUGMODE))
	{
		PF_START_TIMEBAR_DETAIL("Lights");
		Lights::AddToDrawList();
	}

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	s_CustomSetupCallback = customSetupCallback;
	s_CustomCleanupCallback = customCleanupCallback;
	s_CustomFlagsChangedCallback = customFlagsChangedCallback;
	
	fullFilter |= s_GlobalRenderListFlags;

	#define EXECUTE_PASS_LIST(name,passes) \
	{ \
		PF_START_TIMEBAR_DETAIL(name); \
		DLC_GRCDBG_AUTOPUSH(name); \
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
		fwRenderListBuilder::ExecutePassList(passes, NELEM(passes), list, fullFilter, drawmode, AddToDrawListEntityList, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStride)); \
	}

	#define EXECUTE_PASS_LIST_WITH_SIZE(name,passes,size) \
	{ \
		PF_START_TIMEBAR_DETAIL(name); \
		DLC_GRCDBG_AUTOPUSH(name); \
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
		fwRenderListBuilder::ExecutePassList(passes, size, list, fullFilter, drawmode, AddToDrawListEntityList, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStride)); \
	}

	if(filter&RENDER_OPAQUE)
	{
		int drawListAndPassSize;
		fwDrawListAddPass *pDrawListAndPass = GetOpaqueDrawListAddPassAndSize(drawListAndPassSize);
		EXECUTE_PASS_LIST_WITH_SIZE("Opaque entities", pDrawListAndPass, drawListAndPassSize);


		// render PlantDecals before any decals (and alphas):
		if(bDrawPlantsMgr && drawmode == USE_DEFERRED_LIGHTING)
		{
			PF_START_TIMEBAR   ("Plants decals");
			DLC_GRCDBG_AUTOPUSH("Plants decals");
			DLC(dlCmdPlantMgrRenderDecal, ());
		}

		EXECUTE_PASS_LIST("Decal entities", s_DecalPass);

#if ENABLE_DISTANT_CARS
		if(drawmode == USE_DEFERRED_LIGHTING)
		{
			PF_START_TIMEBAR   ("DistantCars");
			DLC_GRCDBG_AUTOPUSH("DistantCars");
			DLC(dlCmdDistantCarsRender, ());
		}
#endif //ENABLE_DISTANT_CARS

		if(bDrawPlantsMgr && drawmode == USE_DEFERRED_LIGHTING)
		{
			PF_START_TIMEBAR   ("Plants");
			DLC_GRCDBG_AUTOPUSH("Plants");
			DLC(dlCmdPlantMgrRender, ());
		}
	}

	if(filter&RENDER_MIRROR_ALPHA)
	{
		EXECUTE_PASS_LIST("Mirror Alpha entities", s_MirrorAlphaPass)
	}
	else if(filter&RENDER_ALPHA)
	{
		EXECUTE_PASS_LIST("Alpha entities", s_AlphaPass)
	}

	if(filter&RENDER_DISPL_ALPHA)
	{
		EXECUTE_PASS_LIST("Displacement Alpha entities", s_DisplAlphaPass)
	}

	if(filter&RENDER_WATER)
	{
		EXECUTE_PASS_LIST("Water entities", s_WaterPass)
	}

	s_CustomSetupCallback = NULL;
	s_CustomCleanupCallback = NULL;
	PF_POP_TIMEBAR_DETAIL();
}


void CRenderListBuilder::AddToDrawListAlpha(s32 list, int fullFilter, eDrawMode drawmode, int subphasePass, 
									   fwRenderListBuilder::RenderStateCallback customSetupCallback, 
									   fwRenderListBuilder::RenderStateCallback customCleanupCallback,
									   fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback,
									   int RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY(entityStart), 
									   int RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY(numEntityLists),
									   int RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY(filterSubListStart),
									   int RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY(numFilterLists))
{
	const int filter			=  (fullFilter&RENDER_MASK);

	PF_PUSH_TIMEBAR_DETAIL("Render Scene");

	BANK_ONLY(if(drawmode != USE_DEBUGMODE))
	{
		PF_START_TIMEBAR_DETAIL("Lights");
		Lights::AddToDrawList();
	}

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	s_CustomSetupCallback = customSetupCallback;
	s_CustomCleanupCallback = customCleanupCallback;
	s_CustomFlagsChangedCallback = customFlagsChangedCallback;

	fullFilter |= s_GlobalRenderListFlags;

#define EXECUTE_ALPHA_PASS_LIST(name,passes) \
	{ \
	PF_START_TIMEBAR_DETAIL(name); \
	DLC_GRCDBG_AUTOPUSH(name); \
	RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
	fwRenderListBuilder::ExecutePassList(passes, NELEM(passes), list, fullFilter, drawmode, AddToDrawListEntityList, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(numEntityLists)); \
	}

#define EXECUTE_ALPHA_PASS_LIST_WITH_SIZE(name,passes,size) \
	{ \
	PF_START_TIMEBAR_DETAIL(name); \
	DLC_GRCDBG_AUTOPUSH(name); \
	RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
	fwRenderListBuilder::ExecutePassList(passes, size, list, fullFilter, drawmode, AddToDrawListEntityList, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(numEntityLists)); \
	}

	if(filter&RENDER_ALPHA)
	{
#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
		const s32 filterListCount =  NELEM(s_AlphaPass);
		s32 subListSize = filterListCount / numFilterLists;
		s32 startFilterIndex = subListSize*filterSubListStart;
		s32 remainder = filterListCount - subListSize*numFilterLists;		

		if(filterSubListStart == numFilterLists-1)
			subListSize += remainder;
		//Add offset of filter index to the s_AlphaPass pointer so it gets the right filter list
		EXECUTE_ALPHA_PASS_LIST_WITH_SIZE("Alpha entities", s_AlphaPass + startFilterIndex, subListSize)
#else
		EXECUTE_ALPHA_PASS_LIST("Alpha entities", s_AlphaPass)
#endif
	}

	s_CustomSetupCallback = NULL;
	s_CustomCleanupCallback = NULL;
	PF_POP_TIMEBAR_DETAIL();
}

void CRenderListBuilder::AddToDrawList_ForcedAlpha(s32 list, int fullFilter, eDrawMode drawMode, int subphasePass, 
									  fwRenderListBuilder::RenderStateCallback customSetupCallback, 
									  fwRenderListBuilder::RenderStateCallback customCleanupCallback,
									  fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback)
{
	const int filter			=  (fullFilter&RENDER_MASK);

	PF_PUSH_TIMEBAR_DETAIL("Render Forced Alpha");

	BANK_ONLY(if(drawMode != USE_DEBUGMODE))
	{
		PF_START_TIMEBAR_DETAIL("Lights");
		Lights::AddToDrawList();
	}

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	s_CustomSetupCallback = customSetupCallback;
	s_CustomCleanupCallback = customCleanupCallback;
	s_CustomFlagsChangedCallback = customFlagsChangedCallback;

	fullFilter |= s_GlobalRenderListFlags;

#define EXECUTE_PASS_LIST_FORCED_ALPHA(name,passes) \
	{ \
	PF_START_TIMEBAR_DETAIL(name); \
	DLC_GRCDBG_AUTOPUSH(name); \
	RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
	fwRenderListBuilder::ExecutePassList(passes, NELEM(passes), list, fullFilter, drawMode, AddToDrawListEntityList, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1)); \
	}


	if(filter&(RENDER_WATER | RENDER_FADING_ONLY))
	{
		EXECUTE_PASS_LIST_FORCED_ALPHA("Forced Alpha entities", s_ForcedAlphaPass)
	}

	s_CustomSetupCallback = NULL;
	s_CustomCleanupCallback = NULL;
	PF_POP_TIMEBAR_DETAIL();

}

#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

int CRenderListBuilder::AddToDrawList_Opaque(int drawAndPassListIdx, s32 list, int fullFilter, eDrawMode drawmode, int interleaveMask, 
									   int interleaveValue, int subphasePass,
									   fwRenderListBuilder::RenderStateCallback customSetupCallback, 
									   fwRenderListBuilder::RenderStateCallback customCleanupCallback,
									   fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback)
{
	int ret = 0;
	const int filter			=  (fullFilter&RENDER_MASK);

	PF_PUSH_TIMEBAR_DETAIL("Render Scene");

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	s_CustomSetupCallback = customSetupCallback;
	s_CustomCleanupCallback = customCleanupCallback;
	s_CustomFlagsChangedCallback = customFlagsChangedCallback;
	
	fullFilter |= s_GlobalRenderListFlags;

	#define RENDERLIST_BUILDER_EXECUTE_PASS_LIST_INTERLEAVED(name,passes,size) \
	{ \
		PF_START_TIMEBAR_DETAIL(name); \
		DLC_GRCDBG_AUTOPUSH(name); \
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
		ret = fwRenderListBuilder::ExecutePassList(passes, size, list, fullFilter, drawmode, AddToDrawListEntityList, subphasePass, interleaveMask, interleaveValue); \
	}

	if(filter&RENDER_OPAQUE)
	{
		fwDrawListAddPass *pDrawListAndPass = GetOpaqueDrawListAddPass();
		RENDERLIST_BUILDER_EXECUTE_PASS_LIST_INTERLEAVED("Opaque entities", &pDrawListAndPass[drawAndPassListIdx], 1);
	}

	s_CustomSetupCallback = NULL;
	s_CustomCleanupCallback = NULL;
	PF_POP_TIMEBAR_DETAIL();
	return ret;
}

void CRenderListBuilder::AddToDrawList_DecalsAndPlants(s32 list, int fullFilter, eDrawMode drawmode, int interleaveMask, 
									   int interleaveValue, int subphasePass,
									   fwRenderListBuilder::RenderStateCallback customSetupCallback, 
									   fwRenderListBuilder::RenderStateCallback customCleanupCallback,
									   fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback)
{
	const int filter			=  (fullFilter&RENDER_MASK);
	const bool bDrawPlantsMgr	= !(fullFilter&RENDER_DONT_RENDER_PLANTS) != 0;

	PF_PUSH_TIMEBAR_DETAIL("Render Scene");

	s_CustomSetupCallback = customSetupCallback;
	s_CustomCleanupCallback = customCleanupCallback;
	s_CustomFlagsChangedCallback = customFlagsChangedCallback;

	fullFilter |= s_GlobalRenderListFlags;

	#define EXECUTE_PASS_LIST_INTERLEAVED(name,passes) \
	{ \
		PF_START_TIMEBAR_DETAIL(name); \
		DLC_GRCDBG_AUTOPUSH(name); \
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  %s", name); }) \
		fwRenderListBuilder::ExecutePassList(passes, NELEM(passes), list, fullFilter, drawmode, AddToDrawListEntityList, subphasePass, interleaveMask, interleaveValue); \
	}

	if(filter&RENDER_OPAQUE)
	{
	#if ENABLE_DISTANT_CARS
		if(drawmode == USE_DEFERRED_LIGHTING)
		{
			PF_START_TIMEBAR   ("DistantCars");
			DLC_GRCDBG_AUTOPUSH("DistantCars");
			DLC(dlCmdDistantCarsRender, ());
		}
	#endif //ENABLE_DISTANT_CARS

		// render PlantDecals before any decals (and alphas):
		if(bDrawPlantsMgr && drawmode == USE_DEFERRED_LIGHTING)
		{
			PF_START_TIMEBAR   ("Plants decals");
			DLC_GRCDBG_AUTOPUSH("Plants decals");
			DLC(dlCmdPlantMgrRenderDecal, ());
		}

		EXECUTE_PASS_LIST_INTERLEAVED("Decal entities", s_DecalPass);
		
		if(bDrawPlantsMgr && drawmode == USE_DEFERRED_LIGHTING)
		{
			PF_START_TIMEBAR   ("Plants");
			DLC_GRCDBG_AUTOPUSH("Plants");
			DLC(dlCmdPlantMgrRender, ());
		}
	}

	s_CustomSetupCallback = NULL;
	s_CustomCleanupCallback = NULL;
	PF_POP_TIMEBAR_DETAIL();
}

#endif // RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

//
//
// counts number of entities with baseFlags in given drawlist selection:
//
u32	CRenderListBuilder::CountDrawListEntities(s32 list, int filter, u32 baseFlags)
{
u32 numEntities=0;

	if(filter&RENDER_OPAQUE)
	{
		if(PostFX::UseSubSampledAlpha())
		{
			numEntities += CountPassEntities(s_OpaquePassSSA, NELEM(s_OpaquePassSSA), list, baseFlags);
		}
		else
		{
			numEntities += CountPassEntities(s_OpaquePass, NELEM(s_OpaquePass), list, baseFlags);
		}
		numEntities += CountPassEntities(s_DecalPass, NELEM(s_DecalPass), list, baseFlags);
	}
	if(filter&RENDER_ALPHA)
	{
		numEntities += CountPassEntities(s_AlphaPass, NELEM(s_AlphaPass), list, baseFlags);
	}
	
	if(filter&RENDER_DISPL_ALPHA)
	{
		numEntities += CountPassEntities(s_DisplAlphaPass, NELEM(s_DisplAlphaPass), list, baseFlags);
	}

	if(filter&RENDER_WATER)
	{
		numEntities += CountPassEntities(s_WaterPass, NELEM(s_WaterPass), list, baseFlags);
	}

	return(numEntities);
}

//
//
// counts all entities with baseFlags in selected drawlistaddpass:
//
u32 CRenderListBuilder::CountPassEntities(fwDrawListAddPass *passList, int elementCount, s32 list, u32 baseFlags)
{
u32 numEntities=0;

	while (elementCount--)
	{
		fwRenderListDesc& renderList = gRenderListGroup.GetRenderListForPhase(list);
		fwEntityList &entityList = renderList.GetList(passList->pass);
		const int entityCount = entityList.GetCount();
		for(s32 i=0; i<entityCount; i++)
		{
			numEntities += ( (entityList.GetEntityBaseFlags(i) & baseFlags)?1:0 );
		}

		// Move on to the next list.
		passList++;
	}

	return(numEntities);
}

int CRenderListBuilder::GetAlphaFilterListCount()
{
	return NELEM(s_AlphaPass);
}

/** PURPOSE: This function is called internally for every render pass that is executed during a shadow pass.
 *  It will go through every entity, perform some filtering based on flags, and add draw list commands to render the
 *  entity if the tests succeed.
 *
 *  PARAMS:
 *  list - This is the index of the render list whose entities will be scanned and added if they are
 *         not rejected.
 *  id - The render pass that is being processed right now.
 *  bucket - The bucket that is being processed right now.
 *  renderMode - Render mode to use. In a shadow context, this always needs to be rmStandard on PS3, and rmStandard/rmShadow/rmShadowSkinned
 *               in other environments.
 *  subphasePass - The actual subphase pass. This is SUBPHASEPASS_NONE if this is a point (local) light, or
 *               a positive number to indicate the shadow pass.
 */
int CRenderListBuilder::AddToDrawListEntityListShadow(s32 list, fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStride))
{
	int ret = 0;
	Assert(((renderMode & rmSpecial) && bucket==CRenderer::RB_ALPHA) || ((renderMode & rmStandard) && bucket==CRenderer::RB_ALPHA) || (bucket==CRenderer::RB_OPAQUE || bucket==CRenderer::RB_CUTOUT));
#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
	(void)entityStart;
	(void)entityStride;
#endif // RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

	if (id == RPASS_FADING && !CRenderPhaseCascadeShadowsInterface::GetRenderFadingEnabled())
		return 0;

#if USE_EDGE

	Assert(id==RPASS_VISIBLE || id==RPASS_LOD || id==RPASS_CUTOUT || id==RPASS_TREE);
	Assert(IsRenderingModes((eRenderMode)renderMode, rmStandard HAS_RENDER_MODE_SHADOWS_ONLY(|rmShadows|rmShadowsSkinned)));

	//
	// ITERATION
	//

#if __BANK 
#if USE_TREE_IMPOSTERS
	bool isTreeCutout = (id==RPASS_TREE && bucket==CRenderer::RB_CUTOUT);
	if (isTreeCutout && CTreeImposters::ms_debugNoLeaves) {
		return 0;
	}
#endif // USE_TREE_IMPOSTERS

	if (s_DebugOnlyRenderCutoutShadow && bucket != CRenderer::RB_CUTOUT) {
		return 0;
	}
#endif // __BANK && USE_TREE_IMPOSTERS


	fwRenderListBuilder::AddEntityFlags addEntityFlags;
	CDrawDataAddParams addParams;

	addEntityFlags.requiredBaseFlags = 0;
	addEntityFlags.excludeBaseFlags = 0;
	addEntityFlags.requiredRenderFlags = 0;
	addEntityFlags.excludeRenderFlags = 0;
	addEntityFlags.useAlpha = false;
	addEntityFlags.drawDataAddParams = &addParams;
	addEntityFlags.renderStateSetup = RenderStateSetupShadowEdge;
	addEntityFlags.renderStateCleanup = RenderStateCleanupShadowEdge;

	if (subphasePass > SUBPHASE_NONE BANK_ONLY(&& !s_DebugAlwaysRenderShadow))
	{
		addEntityFlags.requiredRenderFlags |= BIT(subphasePass);
	}

#if ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK	// we only add them for dynamic pass of the local shadows.
	addEntityFlags.addEntityCallback = sm_AddEntityCallbackLocalShadow;
#endif 

	if (subphasePass < SUBPHASE_NONE)  // for local light shadows, we sometimes need to exclude static or dynamic objects
	{
		if( subphasePass==SUBPHASE_PARABOLOID_STATIC_ONLY)
			addEntityFlags.requiredRenderFlags |= CEntity::RenderFlag_IS_BUILDING;
		if( subphasePass==SUBPHASE_PARABOLOID_DYNAMIC_ONLY)
			addEntityFlags.excludeRenderFlags |= CEntity::RenderFlag_IS_BUILDING;
	}

	ret = fwRenderListBuilder::AddToDrawListEntityListCore(list, id, bucket, renderMode, subphasePass, addEntityFlags, generalFlags);
	Assertf(CDrawListPrototypeManager::GetPrototype() == NULL, "Did a custom cleanup function forget to flush the prototype manager?");

#if RENDERLIST_DUMP_PASS_INFO
	DumpPassInfo(id, bucket, renderMode, addEntityFlags, generalFlags, ret, gRenderListGroup.GetRenderListForPhase(list).GetList(id).GetCount());
#endif // RENDERLIST_DUMP_PASS_INFO

#else // not USE_EDGE

	Assert(id==RPASS_VISIBLE || id==RPASS_LOD || id==RPASS_CUTOUT || id==RPASS_TREE || id==RPASS_FADING);
	Assert(id != RPASS_FADING || (bucket == CRenderer::RB_OPAQUE || bucket == CRenderer::RB_CUTOUT));
	Assert(IsRenderingModes((eRenderMode)renderMode, rmStandard HAS_RENDER_MODE_SHADOWS_ONLY(|rmShadows|rmShadowsSkinned|rmSpecial|rmSpecialSkinned)));

	u32 requiredBaseFlags = 0;
	u32 excludeBaseFlags = 0;

#if __BANK
#if USE_TREE_IMPOSTERS
	if (CTreeImposters::ms_debugNoLeafShadows==true && id == RPASS_TREE && bucket == CRenderer::RB_CUTOUT) {
		return 0;
	}
#endif // USE_TREE_IMPOSTERS

	if (s_DebugOnlyRenderCutoutShadow && bucket != CRenderer::RB_CUTOUT) {
		return 0;
	}
#endif // __BANK

	fwRenderMode renderModeToSet = renderMode;

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	if(IsRenderingModes((eRenderMode)renderModeToSet, rmSpecial|rmSpecialSkinned))
	{
		// During shadow rendering these special modes mean render with regular shaders (it`s a separate case from entities with CEntity::RenderFlag_USE_CUSTOMSHADOWS set).
		renderModeToSet = rmStandard;
	}
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	DLC( dlCmdSetBucketsAndRenderMode, (bucket, CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW), renderModeToSet) ); // set bucket and render mode for all subsequent draw list calls

	if (renderMode==rmStandard && (id==RPASS_CUTOUT || id==RPASS_TREE))
	{
		fwRenderListBuilder::AddEntityFlags addEntityFlags;
		CDrawDataAddParams addParams;

		addEntityFlags.requiredBaseFlags = requiredBaseFlags;
		addEntityFlags.excludeBaseFlags = excludeBaseFlags;
		addEntityFlags.requiredRenderFlags = 0;
		addEntityFlags.excludeRenderFlags = 0;
		addEntityFlags.useAlpha = false;
		addEntityFlags.drawDataAddParams = &addParams;

		if (subphasePass > SUBPHASE_NONE BANK_ONLY(&& !s_DebugAlwaysRenderShadow))
		{
#if GS_INSTANCED_SHADOWS
			if (fwRenderListBuilder::GetInstancingMode()==false)
#endif
				addEntityFlags.requiredRenderFlags |= BIT(subphasePass);
		}

#if ENABLE_ADD_ENTITY_SHADOW_CALLBACK
		addEntityFlags.addEntityCallback = AddEntityCallbackShadow;
#endif
#if ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK	// we only add them for dynamic pass of the local shadows.
		if (sm_AddEntityCallbackLocalShadow)
			addEntityFlags.addEntityCallback = sm_AddEntityCallbackLocalShadow;
#endif 

		addEntityFlags.renderStateSetup = RenderStateSetupShadow;
		addEntityFlags.renderStateCleanup = RenderStateCleanupShadow;

		if (s_CustomSetupCallback || s_CustomCleanupCallback || s_CustomFlagsChangedCallback)
		{
			addEntityFlags.renderStateSetup = s_CustomSetupCallback;
			addEntityFlags.renderStateCleanup = s_CustomCleanupCallback;
			addEntityFlags.customFlagsChangedCallback = s_CustomFlagsChangedCallback;
		}


		if (subphasePass < SUBPHASE_NONE)  // for local light shadows, we sometimes need to exclude static or dynamic objects
		{
			if( subphasePass==SUBPHASE_PARABOLOID_STATIC_ONLY)
				addEntityFlags.requiredRenderFlags |= CEntity::RenderFlag_IS_BUILDING;
			if( subphasePass==SUBPHASE_PARABOLOID_DYNAMIC_ONLY)
				addEntityFlags.excludeRenderFlags |= CEntity::RenderFlag_IS_BUILDING;
		}

		ret = fwRenderListBuilder::AddToDrawListEntityListCore(list, id, bucket, renderMode, subphasePass, addEntityFlags,generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		Assertf(CDrawListPrototypeManager::GetPrototype() == NULL, "Did a custom cleanup function forget to flush the prototype manager?");

#if RENDERLIST_DUMP_PASS_INFO
		DumpPassInfo(id, bucket, renderMode, addEntityFlags, generalFlags, ret, gRenderListGroup.GetRenderListForPhase(list).GetList(id).GetCount());
#endif // RENDERLIST_DUMP_PASS_INFO
	}
	else //anything else
	{
		Assert(id != RPASS_FADING || renderMode == rmStandard);

		if (id == RPASS_FADING && bucket == CRenderer::RB_CUTOUT)
			requiredBaseFlags |= fwEntity::HAS_CUTOUT;

		if (bucket == CRenderer::RB_ALPHA)
			requiredBaseFlags |= fwEntity::HAS_ALPHA_SHADOW;

		u32 requiredRenderFlags = 0;
		u32 excludeRenderFlags = 0;

		CompileTimeAssert(HAS_RENDER_MODE_SHADOWS);

		switch (renderMode) {
			case rmShadowsSkinned:
			case rmSpecialSkinned:
				// only non-vehicles and dynamic objects
				requiredRenderFlags = CEntity::RenderFlag_IS_SKINNED;
				excludeRenderFlags = CEntity::RenderFlag_USE_CUSTOMSHADOWS;
				break;
			case rmShadows:
			case rmSpecial:
				// only non-vehicles
				// We need to draw peds here too as they render non-skinned props.
				// The actual ped is rejected by a check against the rendermode.
				excludeRenderFlags = CEntity::RenderFlag_USE_CUSTOMSHADOWS;
				break;
			default:	// rmStandard
				if (bucket != CRenderer::RB_ALPHA)
				{	// only vehicles, vehicle parts and character cloth entities.
					requiredRenderFlags = CEntity::RenderFlag_USE_CUSTOMSHADOWS;
				}
		}

		//
		// ITERATION
		//

		fwRenderListBuilder::AddEntityFlags addEntityFlags;
		CDrawDataAddParams addParams;

		addEntityFlags.requiredBaseFlags = requiredBaseFlags;
		addEntityFlags.excludeBaseFlags = excludeBaseFlags;
		addEntityFlags.requiredRenderFlags = requiredRenderFlags;
		addEntityFlags.excludeRenderFlags = excludeRenderFlags;
		addEntityFlags.useAlpha = (id == RPASS_FADING);
		addEntityFlags.drawDataAddParams = &addParams;
		addEntityFlags.renderStateSetup = RenderStateSetupShadow;
		addEntityFlags.renderStateCleanup = RenderStateCleanupShadow;

		if (s_CustomSetupCallback || s_CustomCleanupCallback || s_CustomFlagsChangedCallback)
		{
			addEntityFlags.renderStateSetup = s_CustomSetupCallback;
			addEntityFlags.renderStateCleanup = s_CustomCleanupCallback;
			addEntityFlags.customFlagsChangedCallback = s_CustomFlagsChangedCallback;
		}

		if (subphasePass > SUBPHASE_NONE BANK_ONLY(&& !s_DebugAlwaysRenderShadow))
		{
#if GS_INSTANCED_SHADOWS
			if (fwRenderListBuilder::GetInstancingMode()==false)
#endif
				addEntityFlags.requiredRenderFlags |= BIT(subphasePass);
		}

#if ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK	// we only add them for dynamic pass of the local shadows.
		addEntityFlags.addEntityCallback = sm_AddEntityCallbackLocalShadow;
#endif 	

		if (subphasePass < SUBPHASE_NONE)  // for local light shadows, we sometimes need to exclude static or dynamic objects
		{
			if( subphasePass==SUBPHASE_PARABOLOID_STATIC_ONLY)
				addEntityFlags.requiredRenderFlags |= CEntity::RenderFlag_IS_BUILDING;
			if( subphasePass==SUBPHASE_PARABOLOID_DYNAMIC_ONLY)
				addEntityFlags.excludeRenderFlags |= CEntity::RenderFlag_IS_BUILDING;
		}

		ret = fwRenderListBuilder::AddToDrawListEntityListCore(list, id, bucket, renderMode, subphasePass, addEntityFlags,generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		Assertf(CDrawListPrototypeManager::GetPrototype() == NULL, "Did a custom cleanup function forget to flush the prototype manager?");

#if RENDERLIST_DUMP_PASS_INFO
		DumpPassInfo(id, bucket, renderMode, addEntityFlags, generalFlags, ret, gRenderListGroup.GetRenderListForPhase(list).GetList(id).GetCount());
#endif // RENDERLIST_DUMP_PASS_INFO
	}

#endif // not USE_EDGE
	return ret;

}// end of CRenderListBuilder::AddToDrawListEntityListShadow()...

/** PURPOSE: This function is called internally for every render pass that is executed during a non-shadow pass.
 *  It will go through every entity, perform some filtering based on flags, and add draw list commands to render the
 *  entity if the tests succeed.
 *
 *  PARAMS:
 *  list - This is the index of the render list whose entities will be scanned and added if they are
 *         not rejected.
 *  id - The render pass that is being processed right now.
 *  bucket - The bucket that is being processed right now.
 *  renderMode - Render mode to use. In a shadow context, this always needs to be rmStandard on PS3, and rmStandard/rmShadow/rmShadowSkinned
 *               in other environments.
 *  subphasePass -
 */
int CRenderListBuilder::AddToDrawListEntityList(s32 list, fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStride))
{
	const bool bIsGBuffer = (renderMode == rmGBuffer);
	const bool isTreeCutout = (id==RPASS_TREE && bucket==CRenderer::RB_CUTOUT);
	const bool bIsMirrorReflection = gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_MIRROR_REFLECTION);
	const bool bIsDrawSceneAlphaInterior	= DRAWLISTMGR->IsBuildingDrawSceneDrawList() && (subphasePass == SUBPHASE_ALPHA_INTERIOR) ;
	const bool bIsDrawSceneAlphaHybrid		= DRAWLISTMGR->IsBuildingDrawSceneDrawList() && (subphasePass == SUBPHASE_ALPHA_HYBRID);

#if __ASSERT
	// since i've just recently added USE_SIMPLE_LIGHTING to the tree drawlists (for mirror/water reflections), let's check that they're not being used by someone else
	if (!(bIsGBuffer || id != RPASS_TREE BANK_ONLY(|| renderMode == rmDebugMode)))
	{
		Assert(gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_MIRROR_REFLECTION) || gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_WATER_REFLECTION));
	}
#endif // __ASSERT

	Assert(bIsGBuffer || IsRenderingModes((eRenderMode)renderMode, rmSimple BANK_ONLY(|rmDebugMode)) || id != RPASS_TREE);
	Assert(!bIsGBuffer || (id != RPASS_ALPHA && bucket != CRenderer::RB_ALPHA));
	Assert(bucket != CRenderer::RB_NOWATER && bucket != CRenderer::RB_NOSPLASH);

	Assert(subphasePass == SUBPHASE_NONE || (subphasePass >= SUBPHASE_REFLECT_FACET0 && subphasePass <= SUBPHASE_REFLECT_FACET5) || bIsDrawSceneAlphaInterior || bIsDrawSceneAlphaHybrid);

	//
	// FILTER/BITMASK SETUP
	//

	u32 requiredBaseFlags = 0;
	u32 excludeBaseFlags = 0;
	u32 excludeRenderFlags = 0;
//	float forceFade=1.0f;

#if __BANK
#if USE_TREE_IMPOSTERS
	if (bIsGBuffer && id==RPASS_TREE && bucket==CRenderer::RB_OPAQUE && CTreeImposters::ms_debugNoTrunks) {
		return 0;
	}
	if (bIsGBuffer && isTreeCutout && CTreeImposters::ms_debugNoLeaves) {
		return 0;
	}
#endif // USE_TREE_IMPOSTERS
	
#endif // __BANK

	bool setupLighting = false;
	
	if ((s_GlobalRenderListFlags & RENDER_FADE_PROPS) && bucket==CRenderer::RB_OPAQUE && (id==RPASS_FADING || (bIsGBuffer && isTreeCutout))) {
		if (bIsGBuffer) {
			requiredBaseFlags |= fwEntity::USE_SCREENDOOR;
		} else {
			if (renderMode == rmStandard) {
				excludeBaseFlags |= fwEntity::USE_SCREENDOOR;
				setupLighting = true;
			}
		}
	}
	
	if(generalFlags & RENDERFLAG_ONLY_FORCE_ALPHA)
	{
		requiredBaseFlags |= fwEntity::FORCE_ALPHA;
	}

	if(generalFlags & RENDERFLAG_SKIP_FORCE_ALPHA)
	{
		excludeBaseFlags |= fwEntity::FORCE_ALPHA;
	}

	if (bucket == CRenderer::RB_OPAQUE) {
		requiredBaseFlags |= fwEntity::HAS_OPAQUE;
	}
	else if (bucket == CRenderer::RB_CUTOUT) {
		requiredBaseFlags |= fwEntity::HAS_CUTOUT;
	}
	else if (bucket == CRenderer::RB_DECAL)	{
		requiredBaseFlags |= fwEntity::HAS_DECAL;
	}

	if (!bIsGBuffer)
	{
		switch (bucket)
		{
		case CRenderer::RB_ALPHA:
			requiredBaseFlags |= fwEntity::HAS_ALPHA;
			setupLighting = true;
			break;

		case CRenderer::RB_DISPL_ALPHA:
			requiredBaseFlags |= fwEntity::HAS_DISPL_ALPHA;
			setupLighting = true;
			break;

		case CRenderer::RB_WATER:
			requiredBaseFlags |= fwEntity::HAS_WATER;
			break;

		default:
			break;
		}
	}

	// draw only hybrid alpha objects (cables)
	if (bIsDrawSceneAlphaHybrid)
		requiredBaseFlags |= fwEntity::HAS_HYBRID_ALPHA;

	//u8 fadeVal = u8(255*forceFade);
	bool getAlphaFromEntity;
	
	if (bIsGBuffer) {
		getAlphaFromEntity = (id == RPASS_FADING && !isTreeCutout);
	} else {
		getAlphaFromEntity = (id == RPASS_FADING && IsNotRenderingModes((eRenderMode)renderMode, rmSimpleNoFade|rmSimpleDistFade BANK_ONLY(|rmDebugMode)));
	}

	//u8 AlphaFade=u8(255*forceFade);
	u32 requiredRenderFlags=0;
	
	// only allow decals from peds in mirror reflections
	if (bIsMirrorReflection && bucket == CRenderer::RB_DECAL)
		requiredRenderFlags |= CEntity::RenderFlag_IS_PED;

	//
	// ITERATION
	//

	fwRenderListBuilder::AddEntityFlags addEntityFlags;
	CDrawDataAddParams addParams;

	addEntityFlags.requiredBaseFlags = requiredBaseFlags;
	addEntityFlags.excludeBaseFlags = excludeBaseFlags;
	addEntityFlags.requiredRenderFlags = requiredRenderFlags;
	addEntityFlags.excludeRenderFlags = excludeRenderFlags;
	addEntityFlags.useAlpha = getAlphaFromEntity;
	addEntityFlags.drawDataAddParams = &addParams;

	if (subphasePass > SUBPHASE_NONE BANK_ONLY(&& !s_DebugAlwaysRenderReflection))
	{
#if GS_INSTANCED_SHADOWS
		if (fwRenderListBuilder::GetInstancingMode()==false)
#endif
		addEntityFlags.requiredRenderFlags |= BIT(subphasePass);
	}

	if (s_CustomSetupCallback || s_CustomCleanupCallback || s_CustomFlagsChangedCallback)
	{
		addEntityFlags.renderStateSetup = s_CustomSetupCallback;
		addEntityFlags.renderStateCleanup = s_CustomCleanupCallback;
		addEntityFlags.customFlagsChangedCallback = s_CustomFlagsChangedCallback;
	}
	else
	{
		addEntityFlags.renderStateSetup = (bIsGBuffer) ? RenderStateSetupDeferred : RenderStateSetupNormal;
		addEntityFlags.renderStateCleanup = (bIsGBuffer) ? RenderStateCleanupDeferred : RenderStateCleanupNormal;
		addEntityFlags.customFlagsChangedCallback = RenderStateCustomFlagsChangedNormal;
	}

#if SSA_USES_CONDITIONALRENDER
	if ( bIsGBuffer && (generalFlags&RENDERFLAG_BEGIN_RENDERPASS_QUERY) )
	{
		DLC_Add(DeferredLighting::BeginSSAConditionalQuery);
	}
#endif // SSA_USES_CONDITIONALRENDER
	
	if (bIsGBuffer) {
#if ENABLE_ADD_ENTITY_GBUFFER_CALLBACK
		addEntityFlags.addEntityCallback = AddEntityCallbackGBuffer;
#endif // ENABLE_ADD_ENTITY_GBUFFER_CALLBACK
	} else {
		if(IsRenderingModes((eRenderMode)renderMode, rmStandard|rmSpecial))
		{
			// Lights need to be setup always for the mirror reflection phase and for entitys that use alpha
			const bool reflectionPhase = gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_MIRROR_REFLECTION);
			addParams.m_SetupLights = reflectionPhase || setupLighting;
		}
	}

	if (bIsDrawSceneAlphaInterior)
	{
#if ENABLE_ADD_ENTITY_ALPHA_CALLBACK
		addEntityFlags.addEntityCallback = CRenderPhaseDrawSceneInterface::AlphaEntityInteriorCheck;
#endif
	}
	if (bIsDrawSceneAlphaHybrid)
	{
#if ENABLE_ADD_ENTITY_ALPHA_CALLBACK
		addEntityFlags.addEntityCallback = CRenderPhaseDrawSceneInterface::AlphaEntityHybridCheck;
#endif
	}

	int count = fwRenderListBuilder::AddToDrawListEntityListCore(list, id, bucket, renderMode, subphasePass, addEntityFlags, generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(entityStride));
	Assertf(CDrawListPrototypeManager::GetPrototype() == NULL, "Did a custom cleanup function forget to flush the prototype manager?");

#if RENDERLIST_DUMP_PASS_INFO
	DumpPassInfo(id, bucket, renderMode, addEntityFlags, generalFlags, count, gRenderListGroup.GetRenderListForPhase(list).GetList(id).GetCount());
#endif // RENDERLIST_DUMP_PASS_INFO

#if SSA_USES_CONDITIONALRENDER
	if ( bIsGBuffer && (generalFlags&RENDERFLAG_END_RENDERPASS_QUERY) )
	{
		DLC_Add(DeferredLighting::EndSSAConditionalQuery);
	}
#endif // SSA_USES_CONDITIONALRENDER

	return count;
}

#if (GS_INSTANCED_SHADOWS)
void CRenderListBuilder::AddToDrawListShadowEntitiesGrass()
{
#if PLANTS_CAST_SHADOWS
	DLC(dlCmdPlantMgrShadowRender, ());
#endif //PLANTS_CAST_SHADOWS
}

void CRenderListBuilder::AddToDrawListShadowEntitiesInstanced(s32 list, bool bDirLightShadow, int techniqueId,
													 int EDGE_ONLY(techniqueId_edge), int 
#if !USE_EDGE
													 textureTechniqueId
#endif
													 , int EDGE_ONLY(textureTechniqueId_edge)
													 , fwRenderListBuilder::AddEntityCallback localShadowCallBack)
{
	Assert(bDirLightShadow); // subphasePass is facet/cascade index, or it's a point light

	u32 filter = s_GlobalRenderListFlags;

	if (!bDirLightShadow) {
#if !__D3D11 && !RSG_ORBIS
		filter |= RENDER_FILTER_POINT_LIGHT_SHADOWS;
#endif
	}

	Assert(!bDirLightShadow || localShadowCallBack==NULL); // only set call back for local shadows
	sm_AddEntityCallbackLocalShadow = localShadowCallBack;

#if USE_EDGE

	DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

	const int drawModeNonEdge = USE_FORWARD_LIGHTING;
	const int drawModeEdge    = USE_FORWARD_LIGHTING;

	if ((filter & RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS) == 0)
	{
		// No Lods, No Emissive, No Cutout, those should go through non-edge path
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
		DLC_GRCDBG_PUSH("Non-EDGE shadows");
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Non-EDGE shadows"); })
			fwRenderListBuilder::ExecutePassList(s_ShadowNonEdgePPU, NELEM(s_ShadowNonEdgePPU), list, filter, drawModeNonEdge, AddToDrawListEntityListShadow, 0);
		DLC_GRCDBG_POP();
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	}

	if ((filter & RENDER_FILTER_DISABLE_EDGE_SHADOWS) == 0)
	{
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId_edge));
		DLC_GRCDBG_PUSH("Rope shadows");
		DLC(dlCmdRopeShadowRender, ());
		DLC_GRCDBG_POP();
		DLC_GRCDBG_PUSH("EDGE shadows");
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  EDGE shadows"); })
			fwRenderListBuilder::ExecutePassList(s_ShadowEdgePPU, NELEM(s_ShadowEdgePPU), list, filter, drawModeEdge, AddToDrawListEntityListShadow, 0);
		DLC_GRCDBG_POP();
		DLC(dlCmdShaderFxPopForcedTechnique, ());
		DLC(dlCmdShaderFxPushForcedTechnique, (textureTechniqueId_edge));
		DLC_GRCDBG_PUSH("Other shadows");
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Other shadows"); })
			fwRenderListBuilder::ExecutePassList(s_OtherShadowEdgePPU, NELEM(s_OtherShadowEdgePPU), list, filter, drawModeEdge, AddToDrawListEntityListShadow, 0);
		DLC_GRCDBG_POP();
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	}
#else // not USE_EDGE
	
	CompileTimeAssert(HAS_RENDER_MODE_SHADOWS); // need this

	int forcedShaderPass = 0;

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	// Render only the regular meshes.
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_ONLY);
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	DLC_Add(CRenderListBuilder::SetInstancedDrawMode, true);
	DLC_Add(CCustomShaderEffectTree::DontSetShaderVariables, true);

	//unskinned - not including vehicles
	DLC_GRCDBG_PUSH("Unskinned Shadows");
		DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAW/*_INSTANCED*/, forcedShaderPass);
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Unskinned shadows"); })
		fwRenderListBuilder::ExecutePassList(s_UnskinnedShadows, NELEM(s_UnskinnedShadows), list, filter, USE_SHADOWS, AddToDrawListEntityListShadow, 0 RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAW/*_INSTANCED*/, -1);
	DLC_GRCDBG_POP();

	//skinned - not including vehicles
	DLC_GRCDBG_PUSH("Skinned Shadows");
		DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAWSKINNED/*_INSTANCED*/, forcedShaderPass);
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Skinned shadows"); })
		fwRenderListBuilder::ExecutePassList(s_SkinnedShadows, NELEM(s_SkinnedShadows), list, filter, USE_SKINNED_SHADOWS, AddToDrawListEntityListShadow, 0 RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAWSKINNED/*_INSTANCED*/, -1);
	DLC_GRCDBG_POP();

	DLC_Add(CCustomShaderEffectTree::DontSetShaderVariables, false);
	DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	// Render only the tessellated meshes.
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::TESSELLATED_ONLY);

	//unskinned - not including vehicles
	DLC_GRCDBG_PUSH("Unskinned Shadows");
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Unskinned shadows - tessellated"); })
		fwRenderListBuilder::ExecutePassList(s_UnskinnedShadows, NELEM(s_UnskinnedShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, 0 RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	DLC_GRCDBG_POP();

	//skinned - not including vehicles
	DLC_GRCDBG_PUSH("Skinned Shadows");
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Skinned shadows - tessellated"); })
		fwRenderListBuilder::ExecutePassList(s_SkinnedShadows, NELEM(s_SkinnedShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, 0 RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	DLC_GRCDBG_POP();

	// Set back to default of rendering both mesh types.
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	const u32 earlyOutMask = RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS | RENDER_FILTER_DISABLE_VEHICLE_SHADOWS | RENDER_FILTER_DISABLE_CUTOUT_SHADOWS;

	// If at least one of those flags is not set, we got something to render.
	if ((s_GlobalRenderListFlags & earlyOutMask) != earlyOutMask)
	{
		DLC_GRCDBG_PUSH("Other shadows - opaque/fade/rope");
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Other opaque/fade shadows"); })
			fwRenderListBuilder::ExecutePassList(s_OtherOpaqueFadeShadows, NELEM(s_OtherOpaqueFadeShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, 0 RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		// And the rope too...
		DLC(dlCmdRopeShadowRender, ());
		DLC(dlCmdShaderFxPopForcedTechnique, ());
		DLC_GRCDBG_POP();

		DLC_GRCDBG_PUSH("Other shadows");
		DLC(dlCmdShaderFxPushForcedTechnique, (textureTechniqueId));
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Other shadows"); })
			fwRenderListBuilder::ExecutePassList(s_OtherShadows, NELEM(s_OtherShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, 0 RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		// DX11 TODO:- May need to add an equivalent of the two above to render fading and cut-out shadows from RenderFlag_USE_MIXEDSHADOWS entities.
		DLC(dlCmdShaderFxPopForcedTechnique, ());
		DLC_GRCDBG_POP();
	}

#endif // not USE_EDGE

	DLC_Add(CRenderListBuilder::SetInstancedDrawMode, false);

	sm_AddEntityCallbackLocalShadow = NULL; 

}// end of AddToDrawListShadowEntities()...
#endif

/** PURPOSE: This function will render the entire scene (using certain filters) in a shadow pass.
 *
 *  PARAMS:
 *  list - This is the index of the render list whose entities will be scanned and added if they are
 *         not rejected.
 *  subphasePass - The shadow pass index, or SUBPHASE_NONE (or less) if this is a point light shadow.
 */
void CRenderListBuilder::AddToDrawListShadowEntities(s32 list, bool bDirLightShadow, int subphasePass, int techniqueId,
															int EDGE_ONLY(techniqueId_edge), int 
#if !USE_EDGE
															textureTechniqueId
#endif
															, int EDGE_ONLY(textureTechniqueId_edge)
														    , fwRenderListBuilder::AddEntityCallback localShadowCallBack)
{
	Assert((bDirLightShadow && ((subphasePass >= SUBPHASE_CASCADE_0 && subphasePass <= SUBPHASE_CASCADE_3) || CRenderPhaseHeight::GetPtFxGPUCollisionMap()->GetIsRenderingHeightMap())) || (!bDirLightShadow && subphasePass <= SUBPHASE_NONE)); // subphasePass is facet/cascade index, or it's a point light

	u32 filter = s_GlobalRenderListFlags;

	if (!bDirLightShadow) {
#if !__D3D11 && !RSG_ORBIS
		filter |= RENDER_FILTER_POINT_LIGHT_SHADOWS;
#endif
	}

	Assert(!bDirLightShadow || localShadowCallBack==NULL); // only set call back for local shadows
	sm_AddEntityCallbackLocalShadow = localShadowCallBack;

#if USE_EDGE

	DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

	const int drawModeNonEdge = USE_FORWARD_LIGHTING;
	const int drawModeEdge    = USE_FORWARD_LIGHTING;

	if ((filter & RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS) == 0)
	{
		// No Lods, No Emissive, No Cutout, those should go through non-edge path
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
		DLC_GRCDBG_PUSH("Non-EDGE shadows");
			RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Non-EDGE shadows"); })
			fwRenderListBuilder::ExecutePassList(s_ShadowNonEdgePPU, NELEM(s_ShadowNonEdgePPU), list, filter, drawModeNonEdge, AddToDrawListEntityListShadow, subphasePass);
		DLC_GRCDBG_POP();
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	}

	if ((filter & RENDER_FILTER_DISABLE_EDGE_SHADOWS) == 0)
	{
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId_edge));
		DLC_GRCDBG_PUSH("Rope shadows");
			DLC(dlCmdRopeShadowRender, ());
		DLC_GRCDBG_POP();
		DLC_GRCDBG_PUSH("EDGE shadows");
			RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  EDGE shadows"); })
			fwRenderListBuilder::ExecutePassList(s_ShadowEdgePPU, NELEM(s_ShadowEdgePPU), list, filter, drawModeEdge, AddToDrawListEntityListShadow, subphasePass);
		DLC_GRCDBG_POP();
		DLC(dlCmdShaderFxPopForcedTechnique, ());
		DLC(dlCmdShaderFxPushForcedTechnique, (textureTechniqueId_edge));
		DLC_GRCDBG_PUSH("Other shadows");
			RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Other shadows"); })
			fwRenderListBuilder::ExecutePassList(s_OtherShadowEdgePPU, NELEM(s_OtherShadowEdgePPU), list, filter, drawModeEdge, AddToDrawListEntityListShadow, subphasePass);
		DLC_GRCDBG_POP();
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	}
#else // not USE_EDGE

	CompileTimeAssert(HAS_RENDER_MODE_SHADOWS); // need this

	int forcedShaderPass = 0;

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES && SUPPORT_TESSELLATION_IN_SHADOWS
	if(s_DebugEnableTessellationInShadows)
	{
		// Render only the regular meshes.
		DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_ONLY);
	}
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES && SUPPORT_TESSELLATION_IN_SHADOWS

	DLC_Add(CCustomShaderEffectTree::DontSetShaderVariables, true);

	//unskinned - not including vehicles
	DLC_GRCDBG_PUSH("Unskinned Shadows");
		DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAW, forcedShaderPass);
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Unskinned shadows"); })
		fwRenderListBuilder::ExecutePassList(s_UnskinnedShadows, NELEM(s_UnskinnedShadows), list, filter, USE_SHADOWS, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
		DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAW, -1);
	DLC_GRCDBG_POP();

#if __BANK
	if(!s_DisableDynamicObjects || (s_DisableDynamicObjects && subphasePass <= SUBPHASE_CASCADE_2))
#endif
	{
		//skinned - not including vehicles
		DLC_GRCDBG_PUSH("Skinned Shadows");
			DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAWSKINNED, forcedShaderPass);
			RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Skinned shadows"); })
			fwRenderListBuilder::ExecutePassList(s_SkinnedShadows, NELEM(s_SkinnedShadows), list, filter, USE_SKINNED_SHADOWS, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
			DLC_Add(CRenderListBuilder::SetForceShader, grmShader::RMC_DRAWSKINNED, -1);
		DLC_GRCDBG_POP();

		DLC_Add(CCustomShaderEffectTree::DontSetShaderVariables, false);
		DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

	#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES && SUPPORT_TESSELLATION_IN_SHADOWS
		if(s_DebugEnableTessellationInShadows)
		{
			// Render only the tessellated meshes.
			DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::TESSELLATED_ONLY);

			//unskinned - not including vehicles
			DLC_GRCDBG_PUSH("Unskinned Shadows");
				DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
				RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Unskinned shadows - tessellated"); })
				fwRenderListBuilder::ExecutePassList(s_UnskinnedShadows, NELEM(s_UnskinnedShadows), list, filter, USE_SHADOWS_SPECIAL, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
				DLC(dlCmdShaderFxPopForcedTechnique, ());
			DLC_GRCDBG_POP();

			//skinned - not including vehicles
			DLC_GRCDBG_PUSH("Skinned Shadows");
				DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
				RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Skinned shadows - tessellated"); })
				fwRenderListBuilder::ExecutePassList(s_SkinnedShadows, NELEM(s_SkinnedShadows), list, filter, USE_SKINNED_SHADOWS_SPECIAL, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0));
				DLC(dlCmdShaderFxPopForcedTechnique, ());
			DLC_GRCDBG_POP();

			// Set back to default of rendering both mesh types.
			DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);
		}
	#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES && SUPPORT_TESSELLATION_IN_SHADOWS

		const u32 earlyOutMask = RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS | RENDER_FILTER_DISABLE_VEHICLE_SHADOWS | RENDER_FILTER_DISABLE_CUTOUT_SHADOWS;

		// If at least one of those flags is not set, we got something to render.
		if ((s_GlobalRenderListFlags & earlyOutMask) != earlyOutMask)
		{
			DLC_GRCDBG_PUSH("Other shadows - opaque/fade/rope");
				DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
				RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Other opaque/fade shadows"); })
				fwRenderListBuilder::ExecutePassList(s_OtherOpaqueFadeShadows, NELEM(s_OtherOpaqueFadeShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
				// And the rope too...
				DLC(dlCmdRopeShadowRender, ());
				DLC(dlCmdShaderFxPopForcedTechnique, ());
			DLC_GRCDBG_POP();

			DLC_GRCDBG_PUSH("Other shadows");
				DLC(dlCmdShaderFxPushForcedTechnique, (textureTechniqueId));
				RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Other shadows"); })
				fwRenderListBuilder::ExecutePassList(s_OtherShadows, NELEM(s_OtherShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
				DLC(dlCmdShaderFxPopForcedTechnique, ());
			DLC_GRCDBG_POP();

#if RSG_PC
 			// Alpha shadows are mainly cables though could include anything if we set the shader up correctly.
			if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == CSettings::Ultra)
 			{
				CCustomShaderEffectCable::SwitchDepthWrite(true);
 				DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
 				RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Alpha shadows"); })
 					fwRenderListBuilder::ExecutePassList(s_AlphaShadows, NELEM(s_AlphaShadows), list, filter, USE_SHADOWS_SPECIAL, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
 				DLC(dlCmdShaderFxPopForcedTechnique, ());
				CCustomShaderEffectCable::SwitchDepthWrite(false);
 			}
#endif
		}

	#if PLANTS_CAST_SHADOWS
		if(subphasePass <= 2)
		{
			DLC(dlCmdPlantMgrShadowRender, ());
		}
	#endif //PLANTS_CAST_SHADOWS
	}
#endif // not USE_EDGE
	
	sm_AddEntityCallbackLocalShadow = NULL; 

}// end of AddToDrawListShadowEntities()...

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
/** PURPOSE: This function will render the alpha objects (using certain filters) to the shadow pass.
 *			 This is split into its own function to avoid needlessly switching render targets/modes for each cascade
 *
 *  PARAMS:
 *  list - This is the index of the render list whose entities will be scanned and added if they are
 *         not rejected.
 *  subphasePass - The shadow pass index, or SUBPHASE_NONE (or less) if this is a point light shadow.
 */
int CRenderListBuilder::AddToDrawListShadowEntitiesAlphaObjects(s32 list, bool bDirLightShadow, int subphasePass, int techniqueId)
{
	Assert((bDirLightShadow && ((subphasePass >= SUBPHASE_CASCADE_0 && subphasePass <= SUBPHASE_CASCADE_3) || CRenderPhaseHeight::GetPtFxGPUCollisionMap()->GetIsRenderingHeightMap())) || (!bDirLightShadow && subphasePass <= SUBPHASE_NONE)); // subphasePass is facet/cascade index, or it's a point light
	int count = 0;
	u32 filter = s_GlobalRenderListFlags;

	if (!bDirLightShadow) {
#if !__D3D11 && !RSG_ORBIS
		filter |= RENDER_FILTER_POINT_LIGHT_SHADOWS;
#endif
	}

	sm_AddEntityCallbackLocalShadow = NULL;
	s_CustomSetupCallback			= RenderStateSetupAlphaShadow;
	s_CustomCleanupCallback			= RenderStateCleanupShadow;
	s_CustomFlagsChangedCallback	= NULL;//RenderStateCustomFlagsChangedNormal;

	CompileTimeAssert(HAS_RENDER_MODE_SHADOWS); // need this

	DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	// Render only the tessellated meshes (cables).
	DLC_Add(CRenderListBuilder::SetTessellatedDrawMode, rmcLodGroup::UNTESSELLATED_AND_TESSELLATED);

	const u32 earlyOutMask = RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS | RENDER_FILTER_DISABLE_VEHICLE_SHADOWS | RENDER_FILTER_DISABLE_CUTOUT_SHADOWS;

	// If at least one of those flags is not set, we got something to render.
	if ((s_GlobalRenderListFlags & earlyOutMask) != earlyOutMask)
	{
		DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
		RENDERLIST_DUMP_PASS_INFO_ONLY(if (g_RenderListDumpPassFlag) { Displayf("  Alpha shadows"); })
		count = fwRenderListBuilder::ExecutePassList(s_AlphaShadows, NELEM(s_AlphaShadows), list, filter, USE_FORWARD_LIGHTING, AddToDrawListEntityListShadow, subphasePass RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(0) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(1));
	}
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES

	s_CustomSetupCallback 			= NULL;
	s_CustomCleanupCallback 		= NULL;
	s_CustomFlagsChangedCallback	= NULL;
	return count;
}

#endif

#if RMPTFX_USE_PARTICLE_SHADOWS
void CRenderListBuilder::AddToDrawListShadowParticles(s32 cascadeIndex)
{
	DLC(dlCmdParticleShadowRender, (cascadeIndex));

}// end of AddToDrawListShadowParticles()...

#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
void CRenderListBuilder::AddToDrawListShadowParticlesAllCascades(s32 noOfCascades, grcViewport *pViewports, Vec4V *pWindows, bool bUseInstancedShadow)
{
	DLC(dlCmdParticleShadowRenderAllCascades, (noOfCascades, pViewports, pWindows, bUseInstancedShadow));
} // end of AddToDrawListShadowParticlesAllCascades
#endif
#endif

void CRenderListBuilder::SetForceShader(rage::grmShader::eDrawType drawtype, int pass)
{
	// Can't just use GRC_ALLOC_SCOPE_AUTO_PUSH_POP here, since the allocation
	// lifetime from binding the shader needs to be until the next
	// SetForceShader call, not the end of this function.
	GRC_ALLOC_SCOPE_DECLARE_FUNCTION_STATIC(allocScope)

	Assert(grmShaderFx::GetForcedTechniqueGroupId()==-1);
	Assertf(pass < fwVisibilityFlags::MAX_SUBPHASE_VISIBILITY_BITS, "CRenderListBuilder::SetForceShader called with pass = %d, expected < fwVisibilityFlags::MAX_SUBPHASE_VISIBILITY_BITS = %d", pass, fwVisibilityFlags::MAX_SUBPHASE_VISIBILITY_BITS);

	grmShader *forceShader=grmModel::GetForceShader();
	AssertMsg(forceShader, "ForceShader is NULL, check for missing DLC_Add in higher-level platform-dependent code");

	if (pass<0)
	{
		forceShader->UnBind();
		forceShader->EndDraw();
		GRC_ALLOC_SCOPE_UNLOCK(allocScope)
		GRC_ALLOC_SCOPE_POP(allocScope)
	}
	else
	{
		GRC_ALLOC_SCOPE_PUSH(allocScope)
		AssertVerify(forceShader->BeginDraw(drawtype, true));
#if GS_INSTANCED_SHADOWS
		forceShader->Bind(pass);
#else
		if (drawtype == grmShader::RMC_DRAWSKINNED)
			forceShader->Bind(0);
		else
			forceShader->Bind(__PPU?pass:0);
#endif
		GRC_ALLOC_SCOPE_LOCK(allocScope)
	}
}


#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
void CRenderListBuilder::SetTessellatedDrawMode(int tessellatedDrawMode)
{
	switch (CRenderer::IsTessellationEnabled() ? 0 : tessellatedDrawMode)
	{
	case rmcLodGroup::TESSELLATED_ONLY:
		rmcLodGroup::SetTessellatedDrawMode(rmcLodGroup::TESSELLATED_ONLY_AS_UNTESSELLATED);
		break;
	case rmcLodGroup::UNTESSELLATED_AND_TESSELLATED:
		rmcLodGroup::SetTessellatedDrawMode(rmcLodGroup::UNTESSELLATED_AND_TESSELLATED_AS_UNTESSELLATED);
		break;
	default:	//leave as is
		rmcLodGroup::SetTessellatedDrawMode((rmcLodGroup::TessellatedDrawMode)tessellatedDrawMode);
	}
}
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES

#if GS_INSTANCED_SHADOWS
void CRenderListBuilder::SetInstancedDrawMode(bool bInstanced)
{
	grmShader::SetInstancedDrawMode(bInstanced);
}
#endif

#if __BANK
/** PURPOSE: Initialize RAG widgets.
 */
void CRenderListBuilder::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Render List Builder");
	{
		fwRenderListBuilder::AddWidgets(bank);

		bank.PushGroup("Debugging Toggles");
		{
			bank.PushGroup("Shadow/Reflection Debugging");
			{
#if __PPU
				bank.AddToggle("Disable EDGE shadows", &s_GlobalRenderListFlags, RENDER_FILTER_DISABLE_EDGE_SHADOWS);
				bank.AddToggle("Disable non-EDGE shadows", &s_GlobalRenderListFlags, RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS);
#endif // __PPU
				bank.AddToggle("Disable tree trunk shadows", &s_GlobalRenderListFlags, RENDER_FILTER_DISABLE_TREE_TRUNK_SHADOWS);
				bank.AddToggle("Disable tree leaf shadows", &s_GlobalRenderListFlags, RENDER_FILTER_DISABLE_TREE_LEAF_SHADOWS);
				bank.AddToggle("Enable tessellation in shadows", &s_DebugEnableTessellationInShadows);
				bank.AddToggle("Render shadows in all shadow passes", &s_DebugAlwaysRenderShadow);
				bank.AddToggle("Render reflections in both hemispheres", &s_DebugAlwaysRenderReflection);
				bank.AddSlider("Lower Lod reduce factor ", &s_DebugShadowsLowerLODFactor, 0.f, 1.f, 0.001f);
				bank.AddToggle("Only render cutout shadow", &s_DebugOnlyRenderCutoutShadow);
				bank.AddToggle("Disable Dynamics objects for cascade 3", &s_DisableDynamicObjects);
				bank.PopGroup();
			}

			bank.PushGroup("Alpha/Fading");
			{
				bank.AddToggle("Alpha fade props", &s_GlobalRenderListFlags, RENDER_FADE_PROPS);
				bank.AddToggle("Skip test passes", &s_DebugSkipTestPasses);
			}
			bank.PopGroup();
		}
		bank.PopGroup();
	}
	bank.PopGroup();
}
#endif // __BANK

//
//
//  RENDERSTATE SETUP/CLEANUP CALLBACKS
//
//

#if USE_EDGE

void CRenderListBuilder::RenderStateSetupShadowEdge(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, u32 /*generalFlags*/ )
{
	DLC( dlCmdSetBucketsAndRenderMode, (bucket, CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW), renderMode) ); // set bucket and render mode for all subsequent draw list calls

	grcBlendStateHandle BS = grcStateBlock::BS_Default;
	float BS_alpharef = 0.f;
	const bool bRenderingFarCascade = subphasePass>=SUBPHASE_CASCADE_3 && CRenderPhaseCascadeShadowsInterface::IsUsingStandardCascades();

	if( bRenderingFarCascade )
	{
		DLC_Add( LODDrawable::SetRenderingLowestCascade,true);
	}

	//if (subphasePass != SUBPHASE_NONE)
	{
		if (bucket == CRenderer::RB_CUTOUT)
		{
			BS = CRenderPhaseCascadeShadowsInterface::GetCutoutAlphaToCoverage() ? s_shadows_cutout_alpha_BS : s_shadows_cutout_BS;
			BS_alpharef = CRenderPhaseCascadeShadowsInterface::GetCutoutAlphaRef();

			if (id == RPASS_TREE)
			{
				BS_alpharef = 0.f; // TREES use the default for speed
			}
		}
	}

	if (subphasePass >= SUBPHASE_NONE)
	{
		DLC(dlCmdSetBlendState, (BS));
	}
	DLC_Add(CShaderLib::SetAlphaTestRef, (BS_alpharef));
}

void CRenderListBuilder::RenderStateCleanupShadowEdge(fwRenderPassId /*id*/, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, int /*subphasePass*/, u32 /*generalFlags*/)
{
	CDrawListPrototypeManager::Flush();
	g_InstancedBatcher.Flush();
	DLC_Add( LODDrawable::SetRenderingLowestCascade,false);
}

#else // not USE_EDGE

void CRenderListBuilder::RenderStateSetupShadow(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode /*renderMode*/, int subphasePass, u32 /*generalFlags*/)
{
	grcBlendStateHandle BS = __WIN32PC || RSG_DURANGO || RSG_ORBIS ? grcStateBlock::BS_Default_WriteMaskNone : grcStateBlock::BS_Default;
	float BS_alpharef = 0.f;
	const bool bRenderingFarCascade = subphasePass>=SUBPHASE_CASCADE_3 && CRenderPhaseCascadeShadowsInterface::IsUsingStandardCascades();

	if(DRAWLISTMGR->IsBuildingCascadeShadowDrawList())
	{
		CRenderPhaseCascadeShadows::SetCurrentBuildingCascade(subphasePass);
		DLC_Add(CRenderPhaseCascadeShadows::SetCurrentExecutingCascade, subphasePass);
	}

	if( bRenderingFarCascade )
	{
		DLC_Add( LODDrawable::SetRenderingLowestCascade,true);
	}

	//if (subphasePass != SUBPHASE_NONE)
	{
		if (bucket == CRenderer::RB_CUTOUT)
		{
			BS = CRenderPhaseCascadeShadowsInterface::GetCutoutAlphaToCoverage() ? s_shadows_cutout_alpha_BS : s_shadows_cutout_BS;
			BS_alpharef = CRenderPhaseCascadeShadowsInterface::GetCutoutAlphaRef();

			if (id == RPASS_TREE)
			{
				BS_alpharef = 0.f; // TREES use the default for speed
			}
		}
	}

	if (subphasePass >= SUBPHASE_NONE)
	{
		DLC(dlCmdSetBlendState, (BS));
	}
	DLC_Add(CShaderLib::SetAlphaTestRef, (BS_alpharef));
}

void CRenderListBuilder::RenderStateSetupAlphaShadow(fwRenderPassId /*id*/, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, int subphasePass, u32 /*generalFlags*/)
{
	const bool bRenderingFarCascade = subphasePass>=SUBPHASE_CASCADE_3 && CRenderPhaseCascadeShadowsInterface::IsUsingStandardCascades();

	if( bRenderingFarCascade )
	{
		DLC_Add( LODDrawable::SetRenderingLowestCascade,true);
	}
	DLC(dlCmdSetBlendState, (grcStateBlock::BS_Default));
	DLC_Add(CShaderLib::SetAlphaTestRef, (0.f));
}

void CRenderListBuilder::RenderStateCleanupShadow(fwRenderPassId id, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, int /*subphasePass*/, u32 /*generalFlags*/)
{
	CDrawListPrototypeManager::Flush();
	g_InstancedBatcher.Flush();
	DLC_Add( LODDrawable::SetRenderingLowestCascade,false);

	if (id == RPASS_FADING)
	{
		DLC_Add( CShaderLib::SetGlobalAlpha, 1.0f );
	}

	if(DRAWLISTMGR->IsBuildingCascadeShadowDrawList())
	{
		CRenderPhaseCascadeShadows::SetCurrentBuildingCascade(-1);
		DLC_Add(CRenderPhaseCascadeShadows::SetCurrentExecutingCascade, -1);
	}
}

#endif // not USE_EDGE

#if __BANK
static bool SetBucketsAndRenderModeForOverrideGBuf(fwRenderBucket bucket)
{
	if (Unlikely(CPostScanDebug::GetOverrideGBufBitsEnabled()))
	{
		if (DRAWLISTMGR->IsBuildingGBufDrawList() ||
			DRAWLISTMGR->IsBuildingDebugDrawList())
		{
			u32 subBucketIdx = 0;
			int LODFlagShift = 0;

			switch (CPostScanDebug::GetOverrideGBufVisibilityType())
			{
			case VIS_PHASE_CASCADE_SHADOWS:
			case VIS_PHASE_PARABOLOID_SHADOWS:
				subBucketIdx = CRenderer::RB_MODEL_SHADOW;
				LODFlagShift = LODDrawable::LODFLAG_SHIFT_SHADOW;
				break;
			case VIS_PHASE_PARABOLOID_REFLECTION:
				subBucketIdx = CRenderer::RB_MODEL_REFLECTION;
				LODFlagShift = LODDrawable::LODFLAG_SHIFT_REFLECTION;
				break;
			case VIS_PHASE_WATER_REFLECTION:
				subBucketIdx = CRenderer::RB_MODEL_WATER;
				LODFlagShift = LODDrawable::LODFLAG_SHIFT_WATER;
				break;
			case VIS_PHASE_MIRROR_REFLECTION:
				subBucketIdx = CRenderer::RB_MODEL_MIRROR;
				LODFlagShift = LODDrawable::LODFLAG_SHIFT_MIRROR;
				break;
			default:
				return false;
			}

			DLC( dlCmdSetBucketsAndRenderMode, (bucket, CRenderer::GenerateSubBucketMask(subBucketIdx), rmStandard) );
			DLC_Add( dlCmdNewDrawList::SetCurrDrawListLODFlagShift, LODFlagShift );
			return true;
		}
	}

	return false;
}
#endif // __BANK

void CRenderListBuilder::RenderStateSetupDeferred(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode /*renderMode*/, int /*subphasePass*/, fwRenderGeneralFlags generalFlags)
{
#if MULTIPLE_RENDER_THREADS
	// Probably needs to be done per render thread
	CViewportManager::DLCRenderPhaseDrawInit();
#endif

	bool treatAsCutout = (bucket == CRenderer::RB_CUTOUT);

	if (id==RPASS_FADING)
	{
		if (bucket==CRenderer::RB_OPAQUE)
			DLC_Add( DeferredLighting::PrepareFadePass);
		else if (treatAsCutout)
		{
			bool useSSA  = PostFX::UseSubSampledAlpha() && (generalFlags & RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA)!=0;	
			DLC_Add( DeferredLighting::PrepareSSAPass, useSSA, false );
			DLC_Add( DeferredLighting::PrepareCutoutPass, useSSA, false, false);
		}
		else if (bucket==CRenderer::RB_DECAL)
			DLC_Add( DeferredLighting::PrepareDecalPass, true);
	}
	else if (treatAsCutout)
	{
		bool forceClip = (generalFlags & RENDERFLAG_CUTOUT_FORCE_CLIP)!=0;
		bool useSSA  = (generalFlags & RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA)!=0;	
		bool disableEQAA = DEVICE_EQAA ? true : false;
		
		if ( PostFX::UseSinglePassSSA() && PostFX::UseSSAOnFoliage() && id == RPASS_TREE)
		{
			useSSA = true;
			forceClip = false;
		}

		DLC_Add( DeferredLighting::PrepareSSAPass, useSSA, forceClip );
		DLC_Add( DeferredLighting::PrepareCutoutPass, useSSA,  forceClip, disableEQAA);
	}
	else if (bucket==CRenderer::RB_DECAL)
	{
		DLC_Add( DeferredLighting::PrepareDecalPass, false);
	}
#if STENCIL_VEHICLE_INTERIOR
	else if(id == RPASS_TREE)
	{
		DLC_Add( DeferredLighting::PrepareTreePass );
	}
#endif // STENCIL_VEHICLE_INTERIOR
	else
	{
		DLC_Add( DeferredLighting::PrepareDefaultPass, id );
	}

#if __BANK
	if (SetBucketsAndRenderModeForOverrideGBuf(bucket))
	{
		return;
	}
#endif // __BANK

	DLC( dlCmdSetBucketsAndRenderMode, (bucket, rmStandard) );		// set bucket and render mode for all subsequent draw list calls
}

void CRenderListBuilder::RenderStateCleanupDeferred(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode /*renderMode*/, int /*subphasePass*/, u32 /*generalFlags*/)
{
	CDrawListPrototypeManager::Flush();
	g_InstancedBatcher.Flush();

	bool treatAsCutout = (bucket == CRenderer::RB_CUTOUT);
	DLC_Add( CShaderLib::SetGlobalAlpha, 1.0f );

	if (id==RPASS_FADING)
	{
		if (bucket==CRenderer::RB_OPAQUE)
		{
			DLC_Add( DeferredLighting::FinishFadePass);
		}
		else if (treatAsCutout)
		{
			DLC_Add( DeferredLighting::FinishCutoutPass);
			DLC_Add( DeferredLighting::FinishSSAPass);
		}
		else if (bucket==CRenderer::RB_DECAL)
		{
			DLC_Add( DeferredLighting::FinishDecalPass);
		}

	}
	else if (treatAsCutout)
	{
		DLC_Add( DeferredLighting::FinishCutoutPass);
		DLC_Add( DeferredLighting::FinishSSAPass);
	}
	else if (bucket==CRenderer::RB_DECAL)
	{
		DLC_Add( DeferredLighting::FinishDecalPass);
	}
#if STENCIL_VEHICLE_INTERIOR
	else if(id == RPASS_TREE)
	{
		DLC_Add( DeferredLighting::FinishTreePass );
	}
#endif // STENCIL_VEHICLE_INTERIOR
	else
	{
		DLC_Add( DeferredLighting::FinishDefaultPass );
	}
}

void CRenderListBuilder::RenderStateSetupNormal(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int /*shadowPass*/, fwRenderGeneralFlags /*generalFlags*/)
{
	BANK_ONLY(if (renderMode != rmDebugMode))
	{
		switch(bucket)
		{
		case CRenderer::RB_CUTOUT:
			// Why do we not want the cutout states set for rmStandard? Why only set it for the rmSimple* modes?
			if (renderMode != rmStandard)
				DLC( dlCmdSetBlendState,(s_normal_cutout_BS));
			break;
		case CRenderer::RB_ALPHA:
			DLC( dlCmdSetBlendState, (s_normal_alphatest_BS));
			break;
		case CRenderer::RB_DECAL:
		case CRenderer::RB_DISPL_ALPHA:
			DLC( dlCmdSetBlendState,(grcStateBlock::BS_Normal));
			break;
		default:
			if( id==RPASS_FADING )
			{
				DLC( dlCmdSetBlendState,(grcStateBlock::BS_Normal));
			}
			else
			{
				DLC( dlCmdSetBlendState,(grcStateBlock::BS_Default));
			}
			break;
		}

		switch (renderMode)
		{
		case rmNoLights: 
			DLC( dlCmdGrcLightStateSetEnabled, (false)); 
			break;
		default:
			DLC( dlCmdGrcLightStateSetEnabled, (true));
			break;
		}
	}
#if __BANK
	else if (SetBucketsAndRenderModeForOverrideGBuf(bucket))
	{
		return;
	}
#endif // __BANK

	DLC( dlCmdSetBucketsAndRenderMode, (bucket, rmStandard) );		// set bucket and render mode for all subsequent draw list calls
}

void CRenderListBuilder::RenderStateCustomFlagsChangedNormal(fwRenderPassId /*id*/, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, 
										 int /*subphasePass*/, fwRenderGeneralFlags /*generalFlags*/, u16 previousCustomFlags, 
										 u16 currentCustomFlags)
{
	u32 prevUseScreenQuad	 = (previousCustomFlags & fwEntityList::CUSTOM_FLAGS_USE_EXTERIOR_SCREEN_QUAD);
	u32 currentUseScreenQuad = (currentCustomFlags  & fwEntityList::CUSTOM_FLAGS_USE_EXTERIOR_SCREEN_QUAD);

	if (prevUseScreenQuad != currentUseScreenQuad)
	{
		CDrawListPrototypeManager::Flush();
		if (currentUseScreenQuad)
		{
			// Set scissor
			Vec4V extScQd = fwScan::GetGBuffExteriorScreenQuad().ToVec4V();

			extScQd = Clamp(extScQd, Vec4V(V_NEGONE), Vec4V(V_ONE));

			extScQd = (extScQd * Vec4V(0.5f, -0.5f, 0.5f, -0.5f)) + Vec4V(V_HALF);

			float x0 = extScQd[0];
			float y0 = extScQd[3];
			float x1 = extScQd[2];
			float y1 = extScQd[1];
			
			if (s_RenderListBuilderEnableScissorClamping)
			{
				const float acceptableX = 11.0f / 1280.0f;
				const float acceptableY = 6.0f / 720.0f;

				x0 = Selectf(x0 - acceptableX, x0, 0.0f);			// if (x0 <= acceptableX) x0 = 0.0f;
				y0 = Selectf(y0 - acceptableY, y0, 0.0f);			// if (y0 <= acceptableY) y0 = 0.0f;
				x1 = Selectf(x1 - (1.0f - acceptableX), 1.0f, x1);	// if (x1 >= (1.0f - acceptableX)) x1 = 1.0f;
				y1 = Selectf(y1 - (1.0f - acceptableY), 1.0f, y1);	// if (y1 >= (1.0f - acceptableY)) y1 = 1.0f;
			}

			float grcWidth  = (float)VideoResManager::GetSceneWidth();
			float grcHeight = (float)VideoResManager::GetSceneHeight();


			int extScQdX0 = int(x0 * grcWidth); // floorf
			int extScQdY0 = int(y0 * grcHeight);

			int extScQdX1 = int(x1 * grcWidth + 1.0f); // ceilf
			int extScQdY1 = int(y1 * grcHeight + 1.0f);

			extScQdX1 = Min<int>(int(grcWidth),  extScQdX1);
			extScQdY1 = Min<int>(int(grcHeight), extScQdY1);

			int extScQdWidth  = Max<int>(0, extScQdX1 - extScQdX0);
			int extScQdHeight = Max<int>(0, extScQdY1 - extScQdY0);

#if __XENON
			DLC(dlCmdXenonSetScissor, (extScQdX0, extScQdY0, extScQdWidth, extScQdHeight));
#else
			DLC(dlCmdGrcDeviceSetScissor, (extScQdX0, extScQdY0, extScQdWidth, extScQdHeight));
#endif
		}
		else
		{
			// Unset scissor
			DLC(dlCmdGrcDeviceDisableScissor, ());
		}
	}
}

void CRenderListBuilder::RenderStateCleanupNormal(fwRenderPassId /*id*/, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, int /*shadowPass*/, fwRenderGeneralFlags /*generalFlags*/)
{
	CDrawListPrototypeManager::Flush();
	g_InstancedBatcher.Flush();

	DLC_Add( CShaderLib::ResetStippleAlpha );
	DLC_Add( CShaderLib::SetGlobalAlpha, 1.0f );

}


void CRenderListBuilder::RenderStateCleanupSimpleFlush(fwRenderPassId /*id*/, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, int /*shadowPass*/, fwRenderGeneralFlags /*generalFlags*/)
{
	CDrawListPrototypeManager::Flush();
	g_InstancedBatcher.Flush();
}

#define ENABLE_TREE_IMPOSTERS (0 && ENABLE_ADD_ENTITY_GBUFFER_CALLBACK) // this doesn't work anymore

#if ENABLE_ADD_ENTITY_GBUFFER_CALLBACK
bool CRenderListBuilder::AddEntityCallbackGBuffer(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode /*renderMode*/, int /*subphasePass*/, float sortVal, fwEntity *entity)
{
#if ENABLE_TREE_IMPOSTERS
	BANK_ONLY(if (CTreeImposters::ms_Enable))
	{
		if (id==RPASS_TREE && bucket==CRenderer::RB_CUTOUT)
		{
			int cacheIndex=CTreeImposters::GetImposterCacheIndex(entity->GetModelIndex());
			if (cacheIndex!=-1)
			{
				float fade=CTreeImposters::GetImposterFade(cacheIndex, sortVal);
				float fade0=rage::Clamp(fade*2.0f, 0.0f, 1.0f);
				//float fade1=rage::Clamp(1.0f-(fade-0.5f)*2.0f, 0.0f, 1.0f);
				if (fade0>0.01f)
				{
					CEntity* pEntity = (CEntity*)entity;
					DLC_Add(CTreeImposters::RenderTreeImposter, cacheIndex, MAT34V_TO_MATRIX34(entity->GetMatrix()), u8(fade0*pEntity->GetAlpha()), u8(pEntity->GetNaturalAmbientScale()));
					return false;
				}
			}
		}
	}
#else
	(void)id;
	(void)bucket;
	(void)sortVal;
	(void)entity;
#endif
	return true;
}
#endif // ENABLE_ADD_ENTITY_GBUFFER_CALLBACK

#if ENABLE_ADD_ENTITY_SHADOW_CALLBACK
bool CRenderListBuilder::AddEntityCallbackShadow(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode /*renderMode*/, int /*subphasePass*/, float sortVal, fwEntity *entity)
{
#if ENABLE_TREE_IMPOSTERS
	BANK_ONLY(if (CTreeImposters::ms_Enable))
	{
		if (id==RPASS_TREE && bucket==CRenderer::RB_CUTOUT)
		{
			int cacheIndex=CTreeImposters::GetImposterCacheIndex(entity->GetModelIndex());
			if (cacheIndex!=-1)
			{
				float fade=CTreeImposters::GetImposterFadeShadow(cacheIndex, sortVal);
				float fade0=rage::Clamp(fade*2.0f, 0.0f, 1.0f);
				//float fade1=rage::Clamp(1.0f-(fade-0.5f)*2.0f, 0.0f, 1.0f);
				if (fade0>0.01f)
				{
					DLC_Add(CTreeImposters::RenderTreeImposterShadow, cacheIndex, MAT34V_TO_MATRIX34(entity->GetMatrix()), u8(fade0*255));
					return false;
				}
			}
		}
	}
#else
	(void)id;
	(void)bucket;
	(void)sortVal;
	(void)entity;
#endif
	return true;
}
#endif // ENABLE_ADD_ENTITY_SHADOW_CALLBACK
