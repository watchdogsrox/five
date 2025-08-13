// =================================================
// renderer/renderphases/renderphasedebugoverlay.cpp
// (c) 2011 RockstarNorth
// =================================================

#if __BANK

#include "profile/profiler.h"
#include "profile/timebars.h"
#if __D3D
#include "system/xtl.h"
#include "system/d3d9.h"
#endif // __D3D
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "file/remote.h"
#include "grmodel/model.h"
#include "grmodel/shaderfx.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/image.h"
#include "grcore/stateblock.h"
#include "grcore/stateblock_internal.h"
#include "grcore/texture.h"
#include "math/vecrand.h"
#include "rmcore/lodgroup.h"
#include "string/stringutil.h"
#include "system/memory.h"

#include "fwdrawlist/drawlistmgr.h"
#include "fwdebug/picker.h"
#include "fwmaths/vectorutil.h"
#include "fwpheffects/ropemanager.h"
#include "fwrenderer/renderlistbuilder.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/world/InteriorLocation.h"
#include "fwsys/glue.h" // [VEHICLE DAMAGE]
#include "fwutil/xmacro.h"

#include "camera/CamInterface.h"
#include "camera/viewports/viewport.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/colorizer.h"
#include "debug/DebugArchetypeProxy.h"
#include "debug/GtaPicker.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugRendering.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerUtil.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "Peds/rendering/PedDamage.h"
#include "Peds/rendering/PedVariationDebug.h"
#include "physics/physics.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/drawlists/drawlistmgr.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/renderlistbuilder.h"
#include "renderer/renderlistgroup.h"
#include "renderer/renderphases/renderphasecascadeshadows.h"
#include "renderer/renderphases/renderphasedebugoverlay.h"
#include "renderer/rendertargets.h"
#include "renderer/occlusion.h"
#include "renderer/util/shaderutil.h"
#include "renderer/Water.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/loader/MapData.h"
#include "scene/scene.h"
#include "scene/texLod.h"
#include "shaders/shaderlib.h"
#include "system/controlMgr.h"
#include "system/keyboard.h"
#include "Vehicles/vehicleDamage.h" // [VEHICLE DAMAGE]
#include "vfx/decals/DecalCallbacks.h" // [VEHICLE DAMAGE]
#include "vfx/vehicleglass/VehicleGlass.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

#define DEBUG_OVERLAY_CLIP_TEST (1 && __DEV && __PS3)
#define DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC (1)

#if DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
	#define grmModel__CustomShaderParamsFuncType grmModel::CustomShaderParamsFuncType
#else
	typedef bool (*grmModel__CustomShaderParamsFuncType)(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned);
#endif

EXT_PF_GROUP(BuildRenderList);
EXT_PF_GROUP(RenderPhase);

#if DEBUG_OVERLAY_CLIP_TEST
template <int i> static void DebugOverlayShowRenderingStats_button()
{
	if (CPostScanDebug::GetCountModelsRendered())
	{
		CPostScanDebug::SetCountModelsRendered(false);
		gDrawListMgr->SetStatsHighlight(false);
	}
	else
	{
		CPostScanDebug::SetCountModelsRendered(true, DL_RENDERPHASE_DEBUG_OVERLAY);
		gDrawListMgr->SetStatsHighlight(true, "DL_DEBUG_OVERLAY", i == 0, i == 1, false);
		grcDebugDraw::SetDisplayDebugText(true);
	}
}
#endif // DEBUG_OVERLAY_CLIP_TEST

enum eDebugOverlaySkinnedSelect
{
	DEBUG_OVERLAY_SKINNED_SELECT_ALL,
	DEBUG_OVERLAY_SKINNED_SELECT_SKINNED,
	DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED,
	DEBUG_OVERLAY_SKINNED_SELECT_COUNT
};

enum eDebugOverlayRenderPass
{
	DEBUG_OVERLAY_RENDER_PASS_NONE,
	DEBUG_OVERLAY_RENDER_PASS_FILL,
	DEBUG_OVERLAY_RENDER_PASS_OUTLINE,
	DEBUG_OVERLAY_RENDER_PASS_WIREFRAME,
};

enum eDebugOverlayDrawType
{
	DEBUG_OVERLAY_DRAW_TYPE_PEDS    ,
	DEBUG_OVERLAY_DRAW_TYPE_VEHICLES,
	DEBUG_OVERLAY_DRAW_TYPE_MAP     ,
	DEBUG_OVERLAY_DRAW_TYPE_PROPS   ,
	DEBUG_OVERLAY_DRAW_TYPE_TREES   ,
	DEBUG_OVERLAY_DRAW_TYPE_TERRAIN ,
	DEBUG_OVERLAY_DRAW_TYPE_WATER   ,
	DEBUG_OVERLAY_DRAW_TYPE_COUNT   ,

	DEBUG_OVERLAY_DRAW_TYPE_FLAGS_ALL = BIT(DEBUG_OVERLAY_DRAW_TYPE_COUNT) - 1,
};

// TODO -- hook these up to CEntityDrawDataCommon::SetShaderParams
enum eEntityDrawDataType
{
	EDDT_CEntityDrawData               ,
	EDDT_CEntityDrawDataFm             ,
	EDDT_CEntityDrawDataSkinned        ,
	EDDT_CEntityDrawDataStreamPed      ,
	EDDT_CEntityDrawDataPedBIG         ,
	EDDT_CEntityDrawDataDetachedPedProp,
	EDDT_CEntityDrawDataFragType       ,
	EDDT_CEntityDrawDataFrag           ,
	EDDT_CEntityDrawDataVehicleVar     ,
	EDDT_COUNT
};

enum eDebugOverlayUsesParentTxdTextures
{
	DOUPTT_ALWAYS,
	DOUPTT_ONLY_IF_USING_TEXTURES,
	DOUPTT_ONLY_IF_NOT_USING_TEXTURES,
	DOUPTT_COUNT
};

enum eDebugOverlayModelHDState
{
	MODEL_HD_STATE_HD_ENTITY, // ped/vehicle/weapon
	MODEL_HD_STATE_CURRENTLY_USING_HD_TXD,
	MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED, // currently using HD txd but only because something else close has it bound
	MODEL_HD_STATE_HD_TXD_CAPABLE,
	MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING, // close enough for switch, but not yet bound
	MODEL_HD_STATE_NOT_HD_TXD_CAPABLE,
	MODEL_HD_STATE_COUNT
};

class CDebugOverlay
{
public:
	CDebugOverlay()
	{
		sysMemSet(this, 0, sizeof(*this));
	}

	void Init()
	{
		// init shaders
		{
			m_depthTextureVarId = grcEffect::LookupGlobalVar("ReflectionTex");
			m_depthBiasParamsId = grcEffect::LookupGlobalVar("gDirectionalLight");					// shared var
			m_colourNearId      = grcEffect::LookupGlobalVar("gDirectionalColour");					// shared var
			m_colourFarId       = grcEffect::LookupGlobalVar("gLightNaturalAmbient0");		 	    // shared var
			m_zNearFarVisId     = grcEffect::LookupGlobalVar("gLightNaturalAmbient1");				// shared var
			m_depthProjParamsId = grcEffect::LookupGlobalVar("gLightArtificialIntAmbient0");		// shared var
			m_techniqueGroupId  = grmShaderFx::FindTechniqueGroupId("debugoverlay");
		}

		// setup stateblocks
		{
			grcBlendStateDesc bs_additive;

			bs_additive.BlendRTDesc[0].BlendEnable = true;
			bs_additive.BlendRTDesc[0].BlendOp     = grcRSV::BLENDOP_ADD;
			bs_additive.BlendRTDesc[0].SrcBlend    = grcRSV::BLEND_SRCALPHA;
			bs_additive.BlendRTDesc[0].DestBlend   = grcRSV::BLEND_ONE;

#if !__D3D11
			grcRasterizerStateDesc rs_point;
#endif // !__D3D11
			grcRasterizerStateDesc rs_line;
			grcRasterizerStateDesc rs_lineSmooth;

#if !__D3D11
			rs_point.CullMode              = grcRSV::CULL_NONE;
			rs_point.FillMode              = grcRSV::FILL_POINT;
			rs_point.AntialiasedLineEnable = false;
#endif // !__D3D11

			rs_line.CullMode              = grcRSV::CULL_NONE;
			rs_line.FillMode              = grcRSV::FILL_WIREFRAME;
			rs_line.AntialiasedLineEnable = false;

			rs_lineSmooth.CullMode              = grcRSV::CULL_NONE;
			rs_lineSmooth.FillMode              = grcRSV::FILL_WIREFRAME;
			rs_lineSmooth.AntialiasedLineEnable = true;

			m_DS            = grcStateBlock::DSS_IgnoreDepth;
			m_BS            = grcStateBlock::BS_Normal;
			m_BS_additive   = grcStateBlock::CreateBlendState(bs_additive);
			m_RS_fill       = grcStateBlock::RS_NoBackfaceCull;
#if !__D3D11
			m_RS_point      = grcStateBlock::CreateRasterizerState(rs_point);
#endif // !__D3D11
			m_RS_line       = grcStateBlock::CreateRasterizerState(rs_line);
			m_RS_lineSmooth = grcStateBlock::CreateRasterizerState(rs_lineSmooth);
		}

		// setup special cw/ccw rasterizer stateblocks (for water)
		{
			grcRasterizerStateDesc rs_fill;
			grcRasterizerStateDesc rs_line;

			rs_fill.CullMode = grcRSV::CULL_CW;
			rs_fill.FillMode = grcRSV::FILL_SOLID;
			rs_fill.AntialiasedLineEnable = false;

			rs_line.CullMode = grcRSV::CULL_CW;
			rs_line.FillMode = grcRSV::FILL_WIREFRAME;
			rs_line.AntialiasedLineEnable = false;

			m_RS_cw_fill = grcStateBlock::CreateRasterizerState(rs_fill);
			m_RS_cw_line = grcStateBlock::CreateRasterizerState(rs_line);

			rs_line.AntialiasedLineEnable = true;

			m_RS_cw_lineSmooth = grcStateBlock::CreateRasterizerState(rs_line);

			rs_fill.CullMode = grcRSV::CULL_CCW;
			rs_line.CullMode = grcRSV::CULL_CCW;
			rs_line.AntialiasedLineEnable = false;

			m_RS_ccw_fill = grcStateBlock::CreateRasterizerState(rs_fill);
			m_RS_ccw_line = grcStateBlock::CreateRasterizerState(rs_line);

			rs_line.AntialiasedLineEnable = true;

			m_RS_ccw_lineSmooth = grcStateBlock::CreateRasterizerState(rs_line);

			m_RS_current = grcStateBlock::RS_Invalid;
			m_wireframe_current = false;
		}

		Reset();
	}

	void Reset()
	{
#if 0 && __CONSOLE
		u32 multisample_count = 0;
		u32 multisample_quality = 0;
		GRCDEVICE.GetMultiSample(multisample_count, multisample_quality);
#endif // __CONSOLE
		m_pass                = DEBUG_OVERLAY_RENDER_PASS_NONE;
		m_skipAddToDrawList   = false;
		m_skipDepthBind       = false;//CONSOLE_ONLY(multisample_count > 1 ||) false;
		m_draw                = false;
		m_drawSpecialMode     = false;
		m_drawTypeFlags       = DEBUG_OVERLAY_DRAW_TYPE_FLAGS_ALL;
		m_drawWaterQuads      = 0;
		m_renderOpaque        = true;
		m_renderAlpha         = true;
		m_renderFading        = true;
		m_renderPlants        = true;
		m_renderTrees         = true;
		m_renderDecalsCutouts = true;
		m_channel             = DEBUG_OVERLAY_CHANNEL_NONE;
		m_colourMode          = DOCM_NONE;
		m_filter[0]           = '\0';
		m_filterValue         = 0.5f;
		m_skinnedSelect       = DEBUG_OVERLAY_SKINNED_SELECT_ALL;

#if 1//__PS3
		m_drawWaterQuads |= 16+32+64+128; // TODO -- temp hack to stop GPU deadlock (BS#769034), and also graphical corruption on 360
#endif // 1//__PS3

		m_overdrawRender     = false;
		m_overdrawRenderCost = true;
#if __XENON
		m_overdrawUseHiZ     = false;
#endif // __XENON
		m_overdrawColour     = Color32(255,255,255,255); // white
		m_overdrawOpacity    = 1.0f;
		m_overdrawMin        = 0;
		m_overdrawMax        = 20;

		m_txdCostSizeMin  = 0.0f;
		m_txdCostSizeMax  = 1024.0f;
		m_txdCostAreaMin  = 0.0f;
		m_txdCostAreaMax  = 1024.0f;
		m_txdCostRatioMin = 0.0f;
		m_txdCostRatioMax = 10.0f;
		m_txdCostExpBias  = 0.0f;

		m_geometryCountLessThanMinColour    = Color32(  0,255,  0,255); // green
		m_geometryCountMinColour            = Color32(  0,255,  0,255); // green
		m_geometryCountMin                  = 1;
		m_geometryCountMax                  = 5;
		m_geometryCountMaxColour            = Color32(255,255,  0,255); // yellow
		m_geometryCountGreaterThanMaxColour = Color32(255,  0,  0,255); // red

		m_entityFlags     = 0;
		m_entityFlagsFlip = false;

		m_entityType                                       = 0;
		m_entityTypeColour_ENTITY_TYPE_NOTHING             = Color32(  0,  0,  0,255); // black
		m_entityTypeColour_ENTITY_TYPE_BUILDING            = Color32(  0,255,  0,255); // green
		m_entityTypeColour_ENTITY_TYPE_ANIMATED_BUILDING   = Color32(  0,255,255,255); // cyan
		m_entityTypeColour_ENTITY_TYPE_VEHICLE             = Color32(  0,  0,255,255); // blue
		m_entityTypeColour_ENTITY_TYPE_PED                 = Color32(255,  0,  0,255); // red
		m_entityTypeColour_ENTITY_TYPE_OBJECT              = Color32(255,255,  0,255); // yellow
		m_entityTypeColour_ENTITY_TYPE_DUMMY_OBJECT        = Color32(255,255,255,255); // white
		m_entityTypeColour_ENTITY_TYPE_PORTAL              = Color32(255,255,255,255); // white
		m_entityTypeColour_ENTITY_TYPE_MLO                 = Color32(  0,  0,  0,255); // black
		m_entityTypeColour_ENTITY_TYPE_NOTINPOOLS          = Color32(  0,  0,  0,255); // black
		m_entityTypeColour_ENTITY_TYPE_PARTICLESYSTEM      = Color32(  0,  0,  0,255); // black
		m_entityTypeColour_ENTITY_TYPE_LIGHT               = Color32(  0,  0,  0,255); // black
		m_entityTypeColour_ENTITY_TYPE_COMPOSITE           = Color32(255,  0,255,255); // magenta
		m_entityTypeColour_ENTITY_TYPE_INSTANCE_LIST       = Color32(127,127,127,255); // gray
		m_entityTypeColour_ENTITY_TYPE_GRASS_INSTANCE_LIST = Color32(127,127,127,255); // gray

		m_modelType                         = 0;
		m_modelTypeColour_MI_TYPE_NONE      = Color32(255,255,255,255); // white
		m_modelTypeColour_MI_TYPE_BASE      = Color32(  0,255,  0,255); // green
		m_modelTypeColour_MI_TYPE_MLO       = Color32(255,255,255,255); // white
		m_modelTypeColour_MI_TYPE_TIME      = Color32(255,255,255,255); // white
		m_modelTypeColour_MI_TYPE_WEAPON    = Color32(  0,255,255,255); // cyan
		m_modelTypeColour_MI_TYPE_VEHICLE   = Color32(  0,  0,255,255); // blue
		m_modelTypeColour_MI_TYPE_PED       = Color32(255,  0,  0,255); // red
		m_modelTypeColour_MI_TYPE_COMPOSITE = Color32(255,255,255,255); // white

		m_modelHDState                                                    = 0;
		m_modelHDStateIgnoreEntityHDState                                 = false;
		m_modelHDStateSimple                                              = false;
		m_modelHDStateSimpleDistanceOnly                                  = false;
		m_modelHDStateColour_MODEL_HD_STATE_HD_ENTITY                     = Color32(  0,  0,255,255); // blue
		m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD        = Color32(  0,255,  0,255); // green
		m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED = Color32(  0,255,255,255); // cyan
		m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE                = Color32(255,255,  0,255); // yellow
		m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING      = Color32(255,  0,255,255); // magenta
		m_modelHDStateColour_MODEL_HD_STATE_NOT_HD_TXD_CAPABLE            = Color32(255,  0,  0,255); // red

		m_LODType                               = 0;
		m_LODTypeColour_LODTYPES_DEPTH_HD       = Color32(255,  0,  0,255); // red
		m_LODTypeColour_LODTYPES_DEPTH_LOD      = Color32(  0,255,  0,255); // green
		m_LODTypeColour_LODTYPES_DEPTH_SLOD1    = Color32(  0,  0,255,255); // blue
		m_LODTypeColour_LODTYPES_DEPTH_SLOD2    = Color32(  0,255,255,255); // cyan
		m_LODTypeColour_LODTYPES_DEPTH_SLOD3    = Color32(255,255,  0,255); // yellow
		m_LODTypeColour_LODTYPES_DEPTH_SLOD4    = Color32(128,128,128,255); // grey
		m_LODTypeColour_LODTYPES_DEPTH_ORPHANHD = Color32(255,255,255,255); // white

	//	m_drawableLODIndex                = 0;
	//	m_drawableLODIndexColour_LOD_HIGH = Color32(  0,255,  0,255); // green
	//	m_drawableLODIndexColour_LOD_MED  = Color32(255,  0,  0,255); // red
	//	m_drawableLODIndexColour_LOD_LOW  = Color32(  0,  0,255,255); // blue
	//	m_drawableLODIndexColour_LOD_VLOW = Color32(255,255,  0,255); // yellow

		m_drawableType                             = 0;
		m_drawableTypeColour_DT_UNINITIALIZED      = Color32(  0,  0,  0,255); // black
		m_drawableTypeColour_DT_FRAGMENT           = Color32(255,  0,  0,255); // red
		m_drawableTypeColour_DT_DRAWABLE           = Color32(  0,255,  0,255); // green
		m_drawableTypeColour_DT_DRAWABLEDICTIONARY = Color32(  0,  0,255,255); // blue
		m_drawableTypeColour_DT_ASSETLESS          = Color32(255,255,  0,255); // yellow

		m_renderBucket                      = 0;
		m_renderBucketColour_RB_OPAQUE      = Color32(  0,255,  0,255); // green
		m_renderBucketColour_RB_ALPHA       = Color32(255,  0,  0,255); // red
		m_renderBucketColour_RB_DECAL       = Color32(  0,  0,255,255); // blue
		m_renderBucketColour_RB_CUTOUT      = Color32(255,255,  0,255); // yellow
		m_renderBucketColour_RB_WATER       = Color32(255,255,255,255); // white
		m_renderBucketColour_RB_DISPL_ALPHA = Color32(255,255,255,255); // white

		m_propPriorityLevel                           = 0;
		m_propPriorityLevelIncludeStatic              = true;
		m_propPriorityLevelIncludeNonProp             = false;
		m_propPriorityLevelColour_PRI_REQUIRED        = Color32(255,255,255, 64); // white (low opacity)
		m_propPriorityLevelColour_PRI_OPTIONAL_HIGH   = Color32(255,255,  0,255); // yellow
		m_propPriorityLevelColour_PRI_OPTIONAL_MEDIUM = Color32(255,128,  0,255); // orange
		m_propPriorityLevelColour_PRI_OPTIONAL_LOW    = Color32(255,  0,  0,255); // red
		m_propPriorityLevelColour_STATIC              = Color32(  0,  0,  0,255); // black
		m_propPriorityLevelColour_NOT_A_PROP          = Color32(  0,  0,  0,  0); // black (zero opacity)
		m_propPriorityLevelColour_PRI_INVALID         = Color32(255,  0,255,255); // magenta

		m_usesParentTxdTextures = DOUPTT_ALWAYS;

		for (int i = 0; i < NELEM(m_usesParentTxdColours); i++)
		{
			strcpy(m_usesParentTxdNames[i], "");
			m_usesParentTxdColours[i] = Color32(0,255,255,255);
		}

		m_volumeDivideByCameraDistance = false;
		m_volumeColourMinBelow         = Color32(  0,255,  0,255); // green
		m_volumeColourMin              = Color32(  0,255,  0,255); // green
		m_volumeColourMid              = Color32(255,255,  0,255); // yellow
		m_volumeColourMax              = Color32(255,  0,  0,255); // red
		m_volumeColourMaxAbove         = Color32(255,  0,  0,255); // red
		m_volumeRatioMin               = 0.0f;
		m_volumeRatioMax               = 10.0f;
		m_volumeRatioExpBias           = 0.0f;

		m_lineWidth            = __PS3 ? 0.25f : 1.0f;
		m_lineWidthShaderEdit  = 1.0f;
		m_lineSmooth           = true;
#if !__D3D11
		m_renderPoints         = false;
#endif // !__D3D11
		m_culling              = 0; // default
		m_opacity              = 1.0f;
		m_visibleOpacity       = RSG_PC ? 0.5f : 1.0f; // hack to compensate for not having control over line width on pc
		m_invisibleOpacity     = 0.0f;
		m_colourNear           = Color32(  0,255,  0,255); // green
		m_colourFar            = Color32(255,255,255,255); // white
		m_fillOpacity          = 0.0f;
		m_fillOnly             = false;
		m_fillColourSeparate   = false;
		m_fillColourNear       = Color32(  0,255,  0,255); // green
		m_fillColourFar        = Color32(255,255,255,255); // white
		m_zNear                = 4.0f;
		m_zFar                 = 128.0f;
		m_useWireframeOverride = RSG_PC ? false : true;
		m_forceStates          = true;
		m_disableWireframe     = false;

#if DEBUG_OVERLAY_CLIP_TEST
		m_clipXEnable         = false;
		m_clipX               = 0.0f;
		m_clipYEnable         = false;
		m_clipY               = 0.0f;
#endif // DEBUG_OVERLAY_CLIP_TEST

		m_occlusion           = false;
#if USE_EDGE
		m_edgeViewportCull    = false; // don't cull wireframe, it causes some geometry to drop out
#endif // USE_EDGE
		m_depthBias_z0        = 0.0f;
		m_depthBias_z1        = 200.0f;
		m_depthBias_d0        = 0.05f;
		m_depthBias_d1        = 0.10f;
		m_depthBias_scale     = 0.001f;
		m_drawForOneFrameOnly = false;

#if __DEV
		m_HighestDrawableLOD_debug = dlDrawListMgr::DLOD_DEFAULT;
		m_HighestFragmentLOD_debug = dlDrawListMgr::DLOD_DEFAULT;
		m_HighestVehicleLOD_debug  = dlDrawListMgr::VLOD_DEFAULT;
		m_HighestPedLOD_debug      = dlDrawListMgr::PLOD_DEFAULT;
#endif // __DEV
	}

	void AddOverdrawWidgets(bkBank& bk)
	{
		bk.AddToggle("Render Overdraw"     , &m_overdrawRender);
		bk.AddToggle("Render Overdraw Cost", &m_overdrawRenderCost);
#if __XENON
		bk.AddToggle("Use HiZ"             , &m_overdrawUseHiZ);
#endif // __XENON
		bk.AddColor ("Overdraw Color"      , &m_overdrawColour);
		bk.AddSlider("Overdraw Opacity"    , &m_overdrawOpacity, 0.0f, 1.0f, 1.0f/64.0f);
		bk.AddSlider("Overdraw Min"        , &m_overdrawMin, 0, 100, 1);
		bk.AddSlider("Overdraw Max"        , &m_overdrawMax, 1, 100, 1);
	}

	void AddWidgets(bkBank& bk)
	{
		const char* _UIStrings_ColourMode[] =
		{
			"", // None
			"Render Buckets",
			"Texture Dictionary Cost",
			"Texture Dictionary Cost Over Area",
			"Model Path Contains ...",
			"Model Name Is ...",
			"Model Uses Texture ...",
			"Model Uses Parent Txd ...",
			"Model Does Not Use Parent Txd ...",
			"Shader Name Is ...",
			"Shader Uses Texture ...",
			"Shader Uses Txd ...",
			"Random Colour Per Entity",
			"Random Colour Per Texture Dict",
			"Random Colour Per Shader",
			"Random Colour Per Draw Call",
			"Picker",
			"Entity Select",
			"Shader Edit",
			"Texture Viewer",
			"Interior Location",
			"Interior Location Limbo Only",
			"Interior Location Portal Only",
			"Interior Location Invalid",
			"Geometry Counts",
			"Entity Flags",
			"Entity Type",
			"Is Entity Physical",
			"Is Entity Dynamic",
			"Is Entity Static",
			"Is Entity Not Static",
			"Is Entity Not HD Txd Capable",
			"Is Entity HD Txd Capable",
			"Is Entity Currently HD",
			"Is Entity Render in Shadows",
			"Is Entity Don't Render in Shadows",
			"Is Entity Only Render in Shadows (Shadow Proxy)",
			"Is Entity Render in Mirror Reflections",
			"Is Entity Render in Water Reflections",
			"Is Entity Render in Paraboloid Reflections",
			"Is Entity Don't Render in All Reflections",
			"Is Entity Don't Render in Any Reflections",
			"Is Entity Only Render in Reflections (Reflection Proxy)",
			"Is Entity Don't Render in Mirror Reflections",
			"Is Entity Don't Render in Water Reflections",
			"Is Entity Don't Render in Paraboloid Reflections",
			"Is Entity Always Prerender",
			"Is Entity Alpha < 255",
			"Is Entity Smashable",
			"Is Entity Low Priority Stream",
			"Is Entity Light Instanced",
			"Prop Priority Level",
			"Is Ped HD",
			"Model Type",
			"Model HD State",
			"Is Model Not HD Txd Capable",
			"Is Model HD Txd Capable",
			"Is Model Base HD Txd Capable",
			"Is Model Any HD Txd Capable",
			"Is Model Currently Using Base HD Txd",
			"Is Model Currently Using Any HD Txd",
			"LOD Type",
		//	"Drawable LOD Index",
			"Drawable Type",
			"Drawable Has Skeleton",
			"Drawable Has No Skeleton",
			"Contains Drawable LOD_HIGH",
			"Contains Drawable LOD_MED",
			"Contains Drawable LOD_LOW",
			"Contains Drawable LOD_VLOW",
			"Doesn't Contain Drawable LOD_HIGH",
			"Doesn't Contain Drawable LOD_MED",
			"Doesn't Contain Drawable LOD_LOW",
			"Doesn't Contain Drawable LOD_VLOW",
			"Is Object Pickup",
			"Is Object Door",
			"Is Object Uprooted",
			"Is Object Vehicle Part",
			"Is Object Ambient Prop",
			"Is Object Detached Ped Prop",
			"Is Object Can Be Picked Up",
			"Is Object Don't Write Depth",
			"Is Prop",
			"Is Frag",
			"Is Tree",
			"Is Tinted",
		//	"Object With Buoyancy",
			"Object Uses Max Distance For Water Reflection",
			"Has Vehicle Damage Texture",
			"Has 2DEffects",
			"Has 2DEffect Lights",
			"Vertex Volume Ratio",
		//	"Shader Has Ineffective DiffuseTex",
			"Shader Has Ineffective BumpTex",
			"Shader Has Ineffective SpecularTex",
			"Shader Has Alpha DiffuseTex",
			"Shader Has Alpha BumpTex",
			"Shader Has Alpha SpecularTex",
			"Shader Uses Diffuse Tex ...",
			"Shader Uses Bump Tex ...",
			"Shader Uses Specular Tex ...",
			"Shader Fresnel Too Low",
			"Shader Skinned Select",
			"Shader Not In Model Drawable (TEMP)",
			"Old Wires (TEMP)", // for BS#225656
		};
		CompileTimeAssert(NELEM(_UIStrings_ColourMode) == DOCM_COUNT);

		const char* channelStrings[] =
		{
			"",
			"CPV0.xyz: Tree Micromovements",
			"CPV0.x: Ambient Occlusion (natural)",
			"CPV0.y: Ambient Occlusion (artificial) / Ped Rim",
			"CPV0.z: Emissive Intensity / Ped Environmental Effects",
			"CPV0.w: Alpha / Tree Mesh Tint",
			"CPV1.xyz: Terrain / Micromovements",
			"CPV1.x: Terrain / Micromovements (x)",
			"CPV1.y: Terrain / Micromovements (y)",
			"CPV1.z: Terrain / Micromovements (z) / Mesh Tint",
			"CPV1.w: Ped Fatness/Sweat",
			"World Normal",
			"World Tangent",
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
			"Diffuse Texture",
			"Bump Texture",
			"Spec Texture",
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
			"Terrain Wetness",
			"Ped Damage",
			"Vehicle Glass Debug Mode",
		};
		CompileTimeAssert(NELEM(channelStrings) == DEBUG_OVERLAY_CHANNEL_COUNT);

		bk.AddToggle("Skip AddToDrawList ..", &m_skipAddToDrawList);
		bk.AddToggle("Skip Depth Bind ..", &m_skipDepthBind);

		bk.AddToggle("Draw"         , &m_draw);
		bk.AddToggle("Draw Peds"    , &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_PEDS    ), ColourModeOrDrawTypeFlags_changed);
		bk.AddToggle("Draw Vehicles", &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_VEHICLES), ColourModeOrDrawTypeFlags_changed);
		bk.AddToggle("Draw Map"     , &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_MAP     ), ColourModeOrDrawTypeFlags_changed);
		bk.AddToggle("Draw Props"   , &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_PROPS   ), ColourModeOrDrawTypeFlags_changed);
		bk.AddToggle("Draw Trees"   , &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_TREES   ), ColourModeOrDrawTypeFlags_changed);
		bk.AddToggle("Draw Terrain" , &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_TERRAIN ), ColourModeOrDrawTypeFlags_changed);
		bk.AddToggle("Draw Water"   , &m_drawTypeFlags, BIT(DEBUG_OVERLAY_DRAW_TYPE_WATER   ), ColourModeOrDrawTypeFlags_changed);

		bk.PushGroup("Water Flags", false);
		{
#if __DEV
			bk.AddToggle("Draw Water Quads - water"  , &m_drawWaterQuads, BIT(1));
			bk.AddToggle("Draw Water Quads - calming", &m_drawWaterQuads, BIT(2));
			bk.AddToggle("Draw Water Quads - wave"   , &m_drawWaterQuads, BIT(3));
#endif // __DEV
			bk.AddToggle("Draw Water Quads - mode 1"           , &m_drawWaterQuads, BIT(0));
			bk.AddToggle("Draw Water Quads - mode 2: no border", &m_drawWaterQuads, BIT(4));
			bk.AddToggle("Draw Water Quads - mode 2: no far"   , &m_drawWaterQuads, BIT(5));
			bk.AddToggle("Draw Water Quads - mode 2: no near"  , &m_drawWaterQuads, BIT(6));
			bk.AddToggle("Draw Water Quads - mode 2: no tess"  , &m_drawWaterQuads, BIT(7));
		}
		bk.PopGroup();

		bk.AddToggle("Render Opaque"        , &m_renderOpaque);
		bk.AddToggle("Render Alpha"         , &m_renderAlpha);
		bk.AddToggle("Render Fading"        , &m_renderFading);
	//	bk.AddToggle("Render Plants"        , &m_renderPlants);
		bk.AddToggle("Render Trees"         , &m_renderTrees);
		bk.AddToggle("Render Decals/Cutouts", &m_renderDecalsCutouts);

		bk.AddCombo("Channel", &m_channel, NELEM(channelStrings), channelStrings);
		bk.AddCombo("Mode", &m_colourMode, NELEM(_UIStrings_ColourMode), _UIStrings_ColourMode, ColourModeOrDrawTypeFlags_changed);

		bk.PushGroup("Mode Params", false);
		{
			const char* skinnedSelectStrings[] =
			{
				"",
				"Skinned Only (*_drawskinned technique)",
				"Not Skinned Only (*_draw techniques)",
			};
			CompileTimeAssert(NELEM(skinnedSelectStrings) == DEBUG_OVERLAY_SKINNED_SELECT_COUNT);

			bk.AddText("Filter", &m_filter[0], sizeof(m_filter), false);
			bk.AddSlider("Filter Value", &m_filterValue, 0.0f, 1.0f, 0.01f);
			bk.AddCombo("Skinned Select", &m_skinnedSelect, NELEM(skinnedSelectStrings), skinnedSelectStrings);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.PushGroup("Texture Dictionary Cost", false);
			{
				bk.AddSlider("Size Min (KB)"           , &m_txdCostSizeMin, 0.0f, 4096.0f, 1.0f);
				bk.AddSlider("Size Max (KB)"           , &m_txdCostSizeMax, 0.0f, 4096.0f, 1.0f);
				bk.AddSlider("Area Min (square metres)", &m_txdCostAreaMin, 0.0f, 4096.0f, 1.0f);
				bk.AddSlider("Area Max (square metres)", &m_txdCostAreaMax, 0.0f, 4096.0f, 1.0f);
				bk.AddSlider("Ratio Min"               , &m_txdCostRatioMin, 0.0f, 100.0f, 1.0f/64.0f);
				bk.AddSlider("Ratio Max"               , &m_txdCostRatioMax, 0.0f, 100.0f, 1.0f/64.0f);
				bk.AddSlider("Exponent Bias"           , &m_txdCostExpBias, -4.0f, 4.0f, 1.0f/64.0f);
			}
			bk.PopGroup();

			bk.AddTitle("-----------------------------------------------------------------");
			bk.PushGroup("Geometry Counts", false);
			{
				bk.AddColor ("Geometry Count Min Colour <", &m_geometryCountLessThanMinColour);
				bk.AddColor ("Geometry Count Min Colour"  , &m_geometryCountMinColour);
				bk.AddSlider("Geometry Count Min"         , &m_geometryCountMin, 1, 20, 1);
				bk.AddSlider("Geometry Count Max"         , &m_geometryCountMax, 1, 20, 1);
				bk.AddColor ("Geometry Count Max Colour"  , &m_geometryCountMaxColour);
				bk.AddColor ("Geometry Count Max Colour >", &m_geometryCountGreaterThanMaxColour);
			}
			bk.PopGroup();

			bk.AddTitle("-----------------------------------------------------------------");
			bk.PushGroup("Entity Flags", false);
			{
				bk.AddToggle(STRING(IS_VISIBLE                          ), &m_entityFlags, fwEntity::IS_VISIBLE                          );
				bk.AddToggle(STRING(IS_SEARCHABLE                       ), &m_entityFlags, fwEntity::IS_SEARCHABLE                       );
				bk.AddToggle(STRING(HAS_ALPHA                           ), &m_entityFlags, fwEntity::HAS_ALPHA                           );
				bk.AddToggle(STRING(HAS_DECAL                           ), &m_entityFlags, fwEntity::HAS_DECAL                           );
				bk.AddToggle(STRING(HAS_CUTOUT                          ), &m_entityFlags, fwEntity::HAS_CUTOUT                          );
				bk.AddToggle(STRING(HAS_WATER                           ), &m_entityFlags, fwEntity::HAS_WATER                           );
				bk.AddToggle(STRING(HAS_DISPL_ALPHA                     ), &m_entityFlags, fwEntity::HAS_DISPL_ALPHA                     );
				bk.AddToggle(STRING(HAS_PRERENDER                       ), &m_entityFlags, fwEntity::HAS_PRERENDER                       );
				bk.AddToggle(STRING(IS_LIGHT                            ), &m_entityFlags, fwEntity::IS_LIGHT                            );
				bk.AddToggle(STRING(USE_SCREENDOOR                      ), &m_entityFlags, fwEntity::USE_SCREENDOOR                      );
				bk.AddToggle(STRING(SUPPRESS_ALPHA                      ), &m_entityFlags, fwEntity::SUPPRESS_ALPHA                      );
				bk.AddToggle(STRING(RENDER_SMALL_SHADOW                 ), &m_entityFlags, fwEntity::RENDER_SMALL_SHADOW                 );
				bk.AddToggle(STRING(OK_TO_PRERENDER                     ), &m_entityFlags, fwEntity::OK_TO_PRERENDER                     );
				bk.AddToggle(STRING(HAS_NON_WATER_REFLECTION_PROXY_CHILD), &m_entityFlags, fwEntity::HAS_NON_WATER_REFLECTION_PROXY_CHILD);
				bk.AddToggle(STRING(IS_DYNAMIC                          ), &m_entityFlags, fwEntity::IS_DYNAMIC                          );
				bk.AddToggle(STRING(IS_FIXED                            ), &m_entityFlags, fwEntity::IS_FIXED                            );
				bk.AddToggle(STRING(IS_FIXED_BY_NETWORK                 ), &m_entityFlags, fwEntity::IS_FIXED_BY_NETWORK                 );
				bk.AddToggle(STRING(SPAWN_PHYS_ACTIVE                   ), &m_entityFlags, fwEntity::SPAWN_PHYS_ACTIVE                   );
				bk.AddToggle(STRING(DONT_STREAM                         ), &m_entityFlags, fwEntity::DONT_STREAM                         );
				bk.AddToggle(STRING(LOW_PRIORITY_STREAM                 ), &m_entityFlags, fwEntity::LOW_PRIORITY_STREAM                 );
				bk.AddToggle(STRING(HAS_PHYSICS_DICT                    ), &m_entityFlags, fwEntity::HAS_PHYSICS_DICT                    );
				bk.AddToggle(STRING(REMOVE_FROM_WORLD                   ), &m_entityFlags, fwEntity::REMOVE_FROM_WORLD                   );
				bk.AddToggle(STRING(FORCED_ZERO_ALPHA                   ), &m_entityFlags, fwEntity::FORCED_ZERO_ALPHA                   );
				bk.AddToggle(STRING(DRAW_LAST                           ), &m_entityFlags, fwEntity::DRAW_LAST                           );
				bk.AddToggle(STRING(DRAW_FIRST_SORTED                   ), &m_entityFlags, fwEntity::DRAW_FIRST_SORTED                   );
				bk.AddToggle(STRING(NO_INSTANCED_PHYS                   ), &m_entityFlags, fwEntity::NO_INSTANCED_PHYS                   );
				bk.AddToggle(STRING(HAS_HD_TEX_DIST                     ), &m_entityFlags, fwEntity::HAS_HD_TEX_DIST                     );
				bk.AddSeparator();
				bk.AddToggle("Flip", &m_entityFlagsFlip);
			}
			bk.PopGroup();

			const char* _UIStrings_EntityType[] =
			{
				"All",
				STRING(ENTITY_TYPE_NOTHING            ),
				STRING(ENTITY_TYPE_BUILDING           ),
				STRING(ENTITY_TYPE_ANIMATED_BUILDING  ),
				STRING(ENTITY_TYPE_VEHICLE            ),
				STRING(ENTITY_TYPE_PED                ),
				STRING(ENTITY_TYPE_OBJECT             ),
				STRING(ENTITY_TYPE_DUMMY_OBJECT       ),
				STRING(ENTITY_TYPE_PORTAL             ),
				STRING(ENTITY_TYPE_MLO                ),
				STRING(ENTITY_TYPE_NOTINPOOLS         ),
				STRING(ENTITY_TYPE_PARTICLESYSTEM     ),
				STRING(ENTITY_TYPE_LIGHT              ),
				STRING(ENTITY_TYPE_COMPOSITE          ),
				STRING(ENTITY_TYPE_INSTANCE_LIST      ),
				STRING(ENTITY_TYPE_GRASS_INSTANCE_LIST),
				STRING(ENTITY_TYPE_VEHICLEGLASSCOMPONENT),
			};
			CompileTimeAssert(ENTITY_TYPE_NOTHING == 0);
			CompileTimeAssert(NELEM(_UIStrings_EntityType) == 1 + ENTITY_TYPE_TOTAL);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("Entity Type", &m_entityType, NELEM(_UIStrings_EntityType), _UIStrings_EntityType);
			bk.PushGroup("Entity Type Colours", false);
			{
				bk.AddColor(STRING(ENTITY_TYPE_NOTHING            ), &m_entityTypeColour_ENTITY_TYPE_NOTHING            );
				bk.AddColor(STRING(ENTITY_TYPE_BUILDING           ), &m_entityTypeColour_ENTITY_TYPE_BUILDING           );
				bk.AddColor(STRING(ENTITY_TYPE_ANIMATED_BUILDING  ), &m_entityTypeColour_ENTITY_TYPE_ANIMATED_BUILDING  );
				bk.AddColor(STRING(ENTITY_TYPE_VEHICLE            ), &m_entityTypeColour_ENTITY_TYPE_VEHICLE            );
				bk.AddColor(STRING(ENTITY_TYPE_PED                ), &m_entityTypeColour_ENTITY_TYPE_PED                );
				bk.AddColor(STRING(ENTITY_TYPE_OBJECT             ), &m_entityTypeColour_ENTITY_TYPE_OBJECT             );
				bk.AddColor(STRING(ENTITY_TYPE_DUMMY_OBJECT       ), &m_entityTypeColour_ENTITY_TYPE_DUMMY_OBJECT       );
				bk.AddColor(STRING(ENTITY_TYPE_PORTAL             ), &m_entityTypeColour_ENTITY_TYPE_PORTAL             );
				bk.AddColor(STRING(ENTITY_TYPE_MLO                ), &m_entityTypeColour_ENTITY_TYPE_MLO                );
				bk.AddColor(STRING(ENTITY_TYPE_NOTINPOOLS         ), &m_entityTypeColour_ENTITY_TYPE_NOTINPOOLS         );
				bk.AddColor(STRING(ENTITY_TYPE_PARTICLESYSTEM     ), &m_entityTypeColour_ENTITY_TYPE_PARTICLESYSTEM     );
				bk.AddColor(STRING(ENTITY_TYPE_LIGHT              ), &m_entityTypeColour_ENTITY_TYPE_LIGHT              );
				bk.AddColor(STRING(ENTITY_TYPE_COMPOSITE          ), &m_entityTypeColour_ENTITY_TYPE_COMPOSITE          );
				bk.AddColor(STRING(ENTITY_TYPE_INSTANCE_LIST      ), &m_entityTypeColour_ENTITY_TYPE_INSTANCE_LIST      );
				bk.AddColor(STRING(ENTITY_TYPE_GRASS_INSTANCE_LIST), &m_entityTypeColour_ENTITY_TYPE_GRASS_INSTANCE_LIST);
			}
			bk.PopGroup();

			const char* _UIStrings_ModelType[] =
			{
				"All",
				STRING(MI_TYPE_NONE     ),
				STRING(MI_TYPE_BASE     ),
				STRING(MI_TYPE_MLO      ),
				STRING(MI_TYPE_TIME     ),
				STRING(MI_TYPE_WEAPON   ),
				STRING(MI_TYPE_VEHICLE  ),
				STRING(MI_TYPE_PED      ),
				STRING(MI_TYPE_COMPOSITE),
			};
			CompileTimeAssert(MI_TYPE_NONE == 0);
			CompileTimeAssert(NELEM(_UIStrings_ModelType) == 1 + NUM_MI_TYPES);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("Model Type", &m_modelType, NELEM(_UIStrings_ModelType), _UIStrings_ModelType);
			bk.PushGroup("Model Type Colours", false);
			{
				bk.AddColor(STRING(MI_TYPE_NONE     ), &m_modelTypeColour_MI_TYPE_NONE     );
				bk.AddColor(STRING(MI_TYPE_BASE     ), &m_modelTypeColour_MI_TYPE_BASE     );
				bk.AddColor(STRING(MI_TYPE_MLO      ), &m_modelTypeColour_MI_TYPE_MLO      );
				bk.AddColor(STRING(MI_TYPE_TIME     ), &m_modelTypeColour_MI_TYPE_TIME     );
				bk.AddColor(STRING(MI_TYPE_WEAPON   ), &m_modelTypeColour_MI_TYPE_WEAPON   );
				bk.AddColor(STRING(MI_TYPE_VEHICLE  ), &m_modelTypeColour_MI_TYPE_VEHICLE  );
				bk.AddColor(STRING(MI_TYPE_PED      ), &m_modelTypeColour_MI_TYPE_PED      );
				bk.AddColor(STRING(MI_TYPE_COMPOSITE), &m_modelTypeColour_MI_TYPE_COMPOSITE);
			}
			bk.PopGroup();

			const char* _UIStrings_modelHDState[] =
			{
				"All",
				"HD Entity (ped/vehicle/weapon)",
				"Currently Using HD Txd",
				"Currently Using HD Txd (shared)",
				"HD Txd Capable",
				"HD Txd Capable (switching ..)",
				"Not HD Txd Capable",
			};
			CompileTimeAssert(NELEM(_UIStrings_modelHDState) == 1 + MODEL_HD_STATE_COUNT);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("Model HD State", &m_modelHDState, NELEM(_UIStrings_modelHDState), _UIStrings_modelHDState);
			bk.AddToggle("Model HD State - Ignore Entity HD State", &m_modelHDStateIgnoreEntityHDState);
			bk.AddToggle("Model HD State - Simple", &m_modelHDStateSimple);
			bk.AddToggle("Model HD State - Simple Distance Only", &m_modelHDStateSimpleDistanceOnly);
			bk.PushGroup("Model HD State Colours", false);
			{
				bk.AddColor("HD Entity"                      , &m_modelHDStateColour_MODEL_HD_STATE_HD_ENTITY                    );
				bk.AddColor("Currently Using HD Txd"         , &m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD       );
				bk.AddColor("Currently Using HD Txd (shared)", &m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED);
				bk.AddColor("HD Txd Capable"                 , &m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE               );
				bk.AddColor("HD Txd Capable (switching ..)"  , &m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING     );
				bk.AddColor("Not HD Txd Capable"             , &m_modelHDStateColour_MODEL_HD_STATE_NOT_HD_TXD_CAPABLE           );
			}
			bk.PopGroup();

			const char* _UIStrings_LODType[] =
			{
				"All",
				STRING(LODTYPES_DEPTH_HD      ),
				STRING(LODTYPES_DEPTH_LOD     ),
				STRING(LODTYPES_DEPTH_SLOD1   ),
				STRING(LODTYPES_DEPTH_SLOD2   ),
				STRING(LODTYPES_DEPTH_SLOD3   ),
				STRING(LODTYPES_DEPTH_ORPHANHD),
				STRING(LODTYPES_DEPTH_SLOD4   ),
			};
			CompileTimeAssert(LODTYPES_DEPTH_HD == 0);
			CompileTimeAssert(NELEM(_UIStrings_LODType) == 1 + LODTYPES_DEPTH_TOTAL);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("LOD Type", &m_LODType, NELEM(_UIStrings_LODType), _UIStrings_LODType);
			bk.PushGroup("LOD Type Colours", false);
			{
				bk.AddColor(STRING(LODTYPES_DEPTH_HD      ), &m_LODTypeColour_LODTYPES_DEPTH_HD      );
				bk.AddColor(STRING(LODTYPES_DEPTH_LOD     ), &m_LODTypeColour_LODTYPES_DEPTH_LOD     );
				bk.AddColor(STRING(LODTYPES_DEPTH_SLOD1   ), &m_LODTypeColour_LODTYPES_DEPTH_SLOD1   );
				bk.AddColor(STRING(LODTYPES_DEPTH_SLOD2   ), &m_LODTypeColour_LODTYPES_DEPTH_SLOD2   );
				bk.AddColor(STRING(LODTYPES_DEPTH_SLOD3   ), &m_LODTypeColour_LODTYPES_DEPTH_SLOD3   );
				bk.AddColor(STRING(LODTYPES_DEPTH_SLOD4   ), &m_LODTypeColour_LODTYPES_DEPTH_SLOD4   );
				bk.AddColor(STRING(LODTYPES_DEPTH_ORPHANHD), &m_LODTypeColour_LODTYPES_DEPTH_ORPHANHD);
			}
			bk.PopGroup();

		//	const char* _UIStrings_DrawableLODIndex[] =
		//	{
		//		"All",
		//		STRING(LOD_HIGH),
		//		STRING(LOD_MED ),
		//		STRING(LOD_LOW ),
		//		STRING(LOD_VLOW),
		//	};
		//	CompileTimeAssert(LOD_HIGH == 0);
		//	CompileTimeAssert(NELEM(_UIStrings_DrawableLODIndex) == 1 + LOD_COUNT);
		//
		//	bk.AddTitle("-----------------------------------------------------------------");
		//	bk.AddCombo ("Drawable LOD Index", &m_drawableLODIndex, NELEM(_UIStrings_DrawableLODIndex), _UIStrings_DrawableLODIndex);
		//	bk.PushGroup("Drawable LOD Index Colours", false);
		//	{
		//		bk.AddColor(STRING(LOD_HIGH), &m_drawableLODIndexColour_LOD_HIGH);
		//		bk.AddColor(STRING(LOD_MED ), &m_drawableLODIndexColour_LOD_MED );
		//		bk.AddColor(STRING(LOD_LOW ), &m_drawableLODIndexColour_LOD_LOW );
		//		bk.AddColor(STRING(LOD_VLOW), &m_drawableLODIndexColour_LOD_VLOW);
		//	}
		//	bk.PopGroup();

			const char* _UIStrings_DrawableType[] =
			{
				"All",
				STRING(DT_UNINITIALIZED     ),
				STRING(DT_FRAGMENT          ),
				STRING(DT_DRAWABLE          ),
				STRING(DT_DRAWABLEDICTIONARY),
				STRING(DT_ASSETLESS         ),
			};
			CompileTimeAssert(fwArchetype::DT_UNINITIALIZED == 0);
			CompileTimeAssert(NELEM(_UIStrings_DrawableType) == 1 + fwArchetype::DT_COUNT);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("Drawable Type", &m_drawableType, NELEM(_UIStrings_DrawableType), _UIStrings_DrawableType);
			bk.PushGroup("Drawable Type Colours", false);
			{
				bk.AddColor(STRING(DT_UNINITIALIZED     ), &m_drawableTypeColour_DT_UNINITIALIZED     );
				bk.AddColor(STRING(DT_FRAGMENT          ), &m_drawableTypeColour_DT_FRAGMENT          );
				bk.AddColor(STRING(DT_DRAWABLE          ), &m_drawableTypeColour_DT_DRAWABLE          );
				bk.AddColor(STRING(DT_DRAWABLEDICTIONARY), &m_drawableTypeColour_DT_DRAWABLEDICTIONARY);
				bk.AddColor(STRING(DT_ASSETLESS         ), &m_drawableTypeColour_DT_ASSETLESS         );
			}
			bk.PopGroup();

			const char* _UIStrings_RenderBucket[] =
			{
				"All",
				"GBuffer",                 // RB_OPAQUE, RB_DECAL, RB_CUTOUT
				"Forward",                 // RB_ALPHA, RB_WATER, RB_DISPL_ALPHa
				"RB - Opaque",             // RB_OPAQUE
				"RB - Alpha",              // RB_ALPHA
				"RB - Decal",              // RB_DECAL
				"RB - Cutout",             // RB_CUTOUT
				"RB - No Splash",          // RB_NOSPLASH,
				"RB - No Water",           // RB_NOWATER,
				"RB - Water",              // RB_WATER
				"RB - Displacement Alpha", // RB_DISPL_ALPHA
			};
			CompileTimeAssert(CRenderer::RB_OPAQUE == 0);
			CompileTimeAssert(NELEM(_UIStrings_RenderBucket) == 3 + CRenderer::RB_NUM_BASE_BUCKETS);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("Render Bucket", &m_renderBucket, NELEM(_UIStrings_RenderBucket), _UIStrings_RenderBucket);
			bk.PushGroup("Render Bucket Colours", false);
			{
				bk.AddColor(STRING(RB_OPAQUE     ), &m_renderBucketColour_RB_OPAQUE     );
				bk.AddColor(STRING(RB_ALPHA      ), &m_renderBucketColour_RB_ALPHA      );
				bk.AddColor(STRING(RB_DECAL      ), &m_renderBucketColour_RB_DECAL      );
				bk.AddColor(STRING(RB_CUTOUT     ), &m_renderBucketColour_RB_CUTOUT     );
				bk.AddColor(STRING(RB_WATER      ), &m_renderBucketColour_RB_WATER      );
				bk.AddColor(STRING(RB_DISPL_ALPHA), &m_renderBucketColour_RB_DISPL_ALPHA);
			}
			bk.PopGroup();

			const char* _UIStrings_PropPriorityLevel[] =
			{
				"All",
				STRING(PRI_REQUIRED       ),
				STRING(PRI_OPTIONAL_HIGH  ),
				STRING(PRI_OPTIONAL_MEDIUM),
				STRING(PRI_OPTIONAL_LOW   ),
				STRING(STATIC             ),
				STRING(NOT_A_PROP         ),
				STRING(PRI_INVALID        ),
			};
			CompileTimeAssert(NELEM(_UIStrings_PropPriorityLevel) == 1 + fwEntityDef::PRI_COUNT + 3);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo ("Prop Priority Level"                 , &m_propPriorityLevel, NELEM(_UIStrings_PropPriorityLevel), _UIStrings_PropPriorityLevel);
			bk.AddToggle("Prop Priority Level Include Static"  , &m_propPriorityLevelIncludeStatic);
			bk.AddToggle("Prop Priority Level Include Non Prop", &m_propPriorityLevelIncludeNonProp);
			bk.PushGroup("Prop Priority Level Colours", false);
			{
				bk.AddColor(STRING(PRI_REQUIRED       ), &m_propPriorityLevelColour_PRI_REQUIRED       );
				bk.AddColor(STRING(PRI_OPTIONAL_HIGH  ), &m_propPriorityLevelColour_PRI_OPTIONAL_HIGH  );
				bk.AddColor(STRING(PRI_OPTIONAL_MEDIUM), &m_propPriorityLevelColour_PRI_OPTIONAL_MEDIUM);
				bk.AddColor(STRING(PRI_OPTIONAL_LOW   ), &m_propPriorityLevelColour_PRI_OPTIONAL_LOW   );
				bk.AddColor(STRING(STATIC             ), &m_propPriorityLevelColour_STATIC             );
				bk.AddColor(STRING(NOT_A_PROP         ), &m_propPriorityLevelColour_NOT_A_PROP         );
				bk.AddColor(STRING(PRI_INVALID        ), &m_propPriorityLevelColour_PRI_INVALID        );
			}
			bk.PopGroup();

			const char* _UIStrings_UsesParentTxdTextures[] =
			{
				"Always",
				"Only if using textures",
				"Only if not using textures",
			};
			CompileTimeAssert(NELEM(_UIStrings_UsesParentTxdTextures) == DOUPTT_COUNT);

			bk.AddTitle("-----------------------------------------------------------------");
			bk.AddCombo("Uses Parent Txd Textures", &m_usesParentTxdTextures, NELEM(_UIStrings_UsesParentTxdTextures), _UIStrings_UsesParentTxdTextures);
			bk.PushGroup("Uses Parent Txd Colours", false);
			{
				for (int i = 0; i < NELEM(m_usesParentTxdColours); i++)
				{
					bk.AddText(atVarString("Uses Parent Txd %d", i + 1).c_str(), &m_usesParentTxdNames[i][0], sizeof(m_usesParentTxdNames[i]), false);
					bk.AddColor(atVarString("Uses Parent Txd Colour %d", i + 1).c_str(), &m_usesParentTxdColours[i]);
				}
			}
			bk.PopGroup();

			bk.AddTitle("-----------------------------------------------------------------");
			bk.PushGroup("Volume Vertex Ratio", false);
			{
				bk.AddToggle("Volume Divide By Camera Distance", &m_volumeDivideByCameraDistance);
				bk.AddColor ("Volume Colour Min <", &m_volumeColourMinBelow);
				bk.AddColor ("Volume Colour Min", &m_volumeColourMin);
				bk.AddColor ("Volume Colour Mid", &m_volumeColourMid);
				bk.AddColor ("Volume Colour Max", &m_volumeColourMax);
				bk.AddColor ("Volume Colour Max >", &m_volumeColourMaxAbove);
				bk.AddSlider("Volume Ratio Min", &m_volumeRatioMin, 0.0f, 50.0f, 1.0f/32.0f);
				bk.AddSlider("Volume Ratio Max", &m_volumeRatioMax, 0.0f, 50.0f, 1.0f/32.0f);
				bk.AddSlider("Volume Ratio Exp Bias", &m_volumeRatioExpBias, -4.0f, 4.0f, 1.0f/32.0f);
			}
			bk.PopGroup();
		}
		bk.PopGroup();

		bk.PushGroup("Rendering", false);
		{
			const char* cullingStrings[] =
			{
				"Default",
				"None",
				"CW",
				"CCW",
			};

#if !__WIN32PC
			bk.AddSlider("Line Width / Point Size" , &m_lineWidth, 0.25f, 4.0f, 1.0f/8.0f);
			bk.AddSlider("Line Width (Shader Edit)", &m_lineWidthShaderEdit, 1.0f, 8.0f, 1.0f/32.0f);
			bk.AddToggle("Line Smooth"             , &m_lineSmooth);
#endif // !__WIN32PC
#if !__D3D11
			bk.AddToggle("Render Points"           , &m_renderPoints);
#endif // !__D3D11
			bk.AddCombo ("Culling"                 , &m_culling, NELEM(cullingStrings), cullingStrings);
			bk.AddSlider("Opacity"                 , &m_opacity, 0.0f, 1.0f, 1.0f/32.0f);
			bk.AddSlider("Visible Opacity"         , &m_visibleOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			bk.AddSlider("Invisible Opacity"       , &m_invisibleOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			bk.AddColor ("Colour Near"             , &m_colourNear);
			bk.AddColor ("Colour Far"              , &m_colourFar);
			bk.AddSlider("Fill Opacity"            , &m_fillOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			bk.AddToggle("Fill Only"               , &m_fillOnly);
			bk.AddToggle("Fill Colour Separate"    , &m_fillColourSeparate);
			bk.AddColor ("Fill Colour Near"        , &m_fillColourNear);
			bk.AddColor ("Fill Colour Far"         , &m_fillColourFar);
			bk.AddSlider("Z Near"                  , &m_zNear, 0.0f, 2048.0f, 1.0f/32.0f);
			bk.AddSlider("Z Far"                   , &m_zFar , 0.0f, 2048.0f, 1.0f/32.0f);
			bk.AddToggle("Use Wireframe Override"  , &m_useWireframeOverride);
			bk.AddToggle("Force States"            , &m_forceStates);
		//	bk.AddToggle("Disable Wireframe"       , &m_disableWireframe);

			if (fwScan::ms_shadowProxyGroup)
			{
				fwScan::ms_shadowProxyGroup->AddButton("Toggle shadow proxy wireframe", CRenderPhaseDebugOverlayInterface::ToggleShadowProxyMode);
				fwScan::ms_shadowProxyGroup->AddButton("Toggle no shadow casting wireframe", CRenderPhaseDebugOverlayInterface::ToggleNoShadowCastingMode);
			}

			if (fwScan::ms_reflectionProxyGroup)
			{
				fwScan::ms_reflectionProxyGroup->AddButton("Toggle reflection proxy wireframe", CRenderPhaseDebugOverlayInterface::ToggleReflectionProxyMode);
				fwScan::ms_reflectionProxyGroup->AddButton("Toggle no reflections wireframe", CRenderPhaseDebugOverlayInterface::ToggleNoReflectionsMode);
			}
		}
		bk.PopGroup();

#if DEBUG_OVERLAY_CLIP_TEST
		bk.PushGroup("Clip Test", false);
		{
			bk.AddButton("Show Rendering Stats", DebugOverlayShowRenderingStats_button<0>);
			bk.AddButton("Show Context Stats"  , DebugOverlayShowRenderingStats_button<1>);

			bk.AddToggle("Clip X Enable", &m_clipXEnable);
			bk.AddSlider("Clip X"       , &m_clipX, -1.0f, 1.0f, 1.0f/256.0f);
			bk.AddToggle("Clip Y Enable", &m_clipYEnable);
			bk.AddSlider("Clip Y"       , &m_clipY, -1.0f, 1.0f, 1.0f/256.0f);
		}
		bk.PopGroup();
#endif // DEBUG_OVERLAY_CLIP_TEST

		bk.PushGroup("Mode Toggles", false);
		{
			bk.AddButton("Global Txd Mode"         , CRenderPhaseDebugOverlayInterface::ToggleGlobalTxdMode);
			bk.AddButton("Low Priority Stream Mode", CRenderPhaseDebugOverlayInterface::ToggleLowPriorityStreamMode);
			bk.AddButton("Prop Priority Level Mode", CRenderPhaseDebugOverlayInterface::TogglePropPriorityLevelMode);
			bk.AddButton("Model HD State Mode"     , CRenderPhaseDebugOverlayInterface::ToggleModelHDStateMode);
			bk.AddButton("Ped Silhouette Mode"     , CRenderPhaseDebugOverlayInterface::TogglePedSilhouetteMode);
			bk.AddButton("Ped HD Mode"             , CRenderPhaseDebugOverlayInterface::TogglePedHDMode);
			bk.AddButton("Vehicle Glass Mode"      , CRenderPhaseDebugOverlayInterface::ToggleVehicleGlassMode);
			bk.AddButton("Interior Location Mode"  , CRenderPhaseDebugOverlayInterface::ToggleInteriorLocationMode);
			bk.AddButton("Terrain Wetness Mode"    , CRenderPhaseDebugOverlayInterface::ToggleTerrainWetnessMode);
			bk.AddButton("Ped Damage Mode"         , CRenderPhaseDebugOverlayInterface::TogglePedDamageMode);
			bk.AddButton("Vehicle Glass Debug Mode", CRenderPhaseDebugOverlayInterface::ToggleVehicleGlassDebugMode);
			bk.AddButton("Shadow Proxy Mode"       , CRenderPhaseDebugOverlayInterface::ToggleShadowProxyMode);
			bk.AddButton("No Shadow Casting Mode"  , CRenderPhaseDebugOverlayInterface::ToggleNoShadowCastingMode);
			bk.AddButton("Reflection Proxy Mode"   , CRenderPhaseDebugOverlayInterface::ToggleReflectionProxyMode);
			bk.AddButton("No Reflections Mode"     , CRenderPhaseDebugOverlayInterface::ToggleNoReflectionsMode);
		}
		bk.PopGroup();

		bk.PushGroup("Advanced", false);
		{
			bk.AddToggle("Occlusion", &m_occlusion);
#if USE_EDGE
			bk.AddToggle("EDGE Viewport Cull", &m_edgeViewportCull);
#endif // USE_EDGE

			bk.AddSeparator();

			bk.AddSlider("Depth Bias z0"   , &m_depthBias_z0   , 0.0f, 512.00f, 1.0f/64.0f);
			bk.AddSlider("Depth Bias z1"   , &m_depthBias_z1   , 0.0f, 512.00f, 1.0f/64.0f);
			bk.AddSlider("Depth Bias d0"   , &m_depthBias_d0   , 0.0f,   1.00f, 1.0f/512.0f);
			bk.AddSlider("Depth Bias d1"   , &m_depthBias_d1   , 0.0f,   1.00f, 1.0f/512.0f);
			bk.AddSlider("Depth Bias scale", &m_depthBias_scale, 0.0f,   0.01f, 1.0f/2048.0f);

			bk.AddToggle("Draw For One Frame Only", &m_drawForOneFrameOnly);

			bk.AddSeparator();
#if __DEV
			gDrawListMgr->SetupLODWidgets(bk, &m_HighestDrawableLOD_debug, &m_HighestFragmentLOD_debug,  &m_HighestVehicleLOD_debug, &m_HighestPedLOD_debug);
#endif // __DEV
		}
		bk.PopGroup();
	}

	static void ColourModeOrDrawTypeFlags_changed();
	static void SetCurrentPass(eDebugOverlayRenderPass pass, grmModel__CustomShaderParamsFuncType func);

	grcEffectGlobalVar m_depthTextureVarId;
	grcEffectGlobalVar m_depthProjParamsId;
	grcEffectGlobalVar m_depthBiasParamsId;
	grcEffectGlobalVar m_colourNearId;
	grcEffectGlobalVar m_colourFarId;
	grcEffectGlobalVar m_zNearFarVisId; // {zNear,zFar,visibleOpacity,invisibleOpacity}
	int                m_techniqueGroupId;

	grcDepthStencilStateHandle m_DS;
	grcBlendStateHandle        m_BS;
	grcBlendStateHandle        m_BS_additive;
	grcRasterizerStateHandle   m_RS_fill;
#if !__D3D11
	grcRasterizerStateHandle   m_RS_point;
#endif // !__D3D11
	grcRasterizerStateHandle   m_RS_line;
	grcRasterizerStateHandle   m_RS_lineSmooth;
	grcRasterizerStateHandle   m_RS_cw_fill;
	grcRasterizerStateHandle   m_RS_cw_line;
	grcRasterizerStateHandle   m_RS_cw_lineSmooth;
	grcRasterizerStateHandle   m_RS_ccw_fill;
	grcRasterizerStateHandle   m_RS_ccw_line;
	grcRasterizerStateHandle   m_RS_ccw_lineSmooth;
	grcRasterizerStateHandle   m_RS_current;
	bool                       m_wireframe_current;

	eDebugOverlayRenderPass m_pass; // updated in render thread

	bool    m_skipAddToDrawList; // temporary hack to debug crashing (see BS#1735980)
	bool    m_skipDepthBind;
	bool    m_draw;
	bool    m_drawSpecialMode; // enabled when we're using a predefined draw mode (i.e. invoked through a button), this causes point rendering to be skipped when cycling ALT+W
	u32     m_drawTypeFlags;
	u32     m_drawWaterQuads;
	bool    m_renderOpaque;
	bool    m_renderAlpha;
	bool    m_renderFading;
	bool    m_renderPlants; // doesn't work, that's annoying
	bool    m_renderTrees;
	bool    m_renderDecalsCutouts;
	int     m_channel; // eDebugOverlayChannel
	int     m_colourMode; // eDebugOverlayColourMode
	char    m_filter[256];
	float   m_filterValue; // used with DOCM_SHADER_FRESNEL_TOO_LOW etc.
	int     m_skinnedSelect; // eDebugOverlaySkinnedSelect

	bool    m_overdrawRender;
	bool    m_overdrawRenderCost;
#if __XENON
	bool    m_overdrawUseHiZ;
#endif // __XENON
	Color32 m_overdrawColour;
	float   m_overdrawOpacity; // only affects cost mode
	int     m_overdrawMin;
	int     m_overdrawMax;

	// used with m_colourMode == DOCM_TXD_COST
	float   m_txdCostSizeMin;
	float   m_txdCostSizeMax;
	float   m_txdCostAreaMin;
	float   m_txdCostAreaMax;
	float   m_txdCostRatioMin;
	float   m_txdCostRatioMax;
	float   m_txdCostExpBias;

	// used with m_colourMode == DOCM_GEOMETRY_COUNTS
	Color32 m_geometryCountLessThanMinColour;
	Color32 m_geometryCountMinColour;
	int     m_geometryCountMin;
	int     m_geometryCountMax;
	Color32 m_geometryCountMaxColour;
	Color32 m_geometryCountGreaterThanMaxColour;

	// used with m_colourMode == DOCM_ENTITY_FLAGS
	u32     m_entityFlags;
	bool    m_entityFlagsFlip;

	// used with m_colourMode == DOCM_ENTITY_TYPE
	int     m_entityType;
	Color32 m_entityTypeColour_ENTITY_TYPE_NOTHING            ;
	Color32 m_entityTypeColour_ENTITY_TYPE_BUILDING           ;
	Color32 m_entityTypeColour_ENTITY_TYPE_ANIMATED_BUILDING  ;
	Color32 m_entityTypeColour_ENTITY_TYPE_VEHICLE            ;
	Color32 m_entityTypeColour_ENTITY_TYPE_PED                ;
	Color32 m_entityTypeColour_ENTITY_TYPE_OBJECT             ;
	Color32 m_entityTypeColour_ENTITY_TYPE_DUMMY_OBJECT       ;
	Color32 m_entityTypeColour_ENTITY_TYPE_PORTAL             ;
	Color32 m_entityTypeColour_ENTITY_TYPE_MLO                ;
	Color32 m_entityTypeColour_ENTITY_TYPE_NOTINPOOLS         ;
	Color32 m_entityTypeColour_ENTITY_TYPE_PARTICLESYSTEM     ;
	Color32 m_entityTypeColour_ENTITY_TYPE_LIGHT              ;
	Color32 m_entityTypeColour_ENTITY_TYPE_COMPOSITE          ;
	Color32 m_entityTypeColour_ENTITY_TYPE_INSTANCE_LIST      ;
	Color32 m_entityTypeColour_ENTITY_TYPE_GRASS_INSTANCE_LIST;

	// used with m_colourMode == DOCM_MODEL_TYPE
	int     m_modelType;
	Color32 m_modelTypeColour_MI_TYPE_NONE     ;
	Color32 m_modelTypeColour_MI_TYPE_BASE     ;
	Color32 m_modelTypeColour_MI_TYPE_MLO      ;
	Color32 m_modelTypeColour_MI_TYPE_TIME     ;
	Color32 m_modelTypeColour_MI_TYPE_WEAPON   ;
	Color32 m_modelTypeColour_MI_TYPE_VEHICLE  ;
	Color32 m_modelTypeColour_MI_TYPE_PED      ;
	Color32 m_modelTypeColour_MI_TYPE_COMPOSITE;

	// used with m_colourMode == DOCM_MODEL_HD_STATE
	int     m_modelHDState;
	bool    m_modelHDStateIgnoreEntityHDState;
	bool    m_modelHDStateSimple;
	bool    m_modelHDStateSimpleDistanceOnly;
	Color32 m_modelHDStateColour_MODEL_HD_STATE_HD_ENTITY;
	Color32 m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD;
	Color32 m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED;
	Color32 m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE;
	Color32 m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING;
	Color32 m_modelHDStateColour_MODEL_HD_STATE_NOT_HD_TXD_CAPABLE;

	// used with m_colourMode == DOCM_LOD_TYPE
	int     m_LODType;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_HD      ;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_LOD     ;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_SLOD1   ;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_SLOD2   ;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_SLOD3   ;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_SLOD4   ;
	Color32 m_LODTypeColour_LODTYPES_DEPTH_ORPHANHD;

//	// used with m_colourMode == DOCM_DRAWABLE_LOD_INDEX
//	int     m_drawableLODIndex;
//	Color32 m_drawableLODIndexColour_LOD_HIGH;
//	Color32 m_drawableLODIndexColour_LOD_MED ;
//	Color32 m_drawableLODIndexColour_LOD_LOW ;
//	Color32 m_drawableLODIndexColour_LOD_VLOW;

	// used with m_colourMode == DOCM_DRAWABLE_TYPE
	int     m_drawableType;
	Color32 m_drawableTypeColour_DT_UNINITIALIZED     ;
	Color32 m_drawableTypeColour_DT_FRAGMENT          ;
	Color32 m_drawableTypeColour_DT_DRAWABLE          ;
	Color32 m_drawableTypeColour_DT_DRAWABLEDICTIONARY;
	Color32 m_drawableTypeColour_DT_ASSETLESS         ;

	// used with m_colourMode == DOCM_RENDER_BUCKETS
	int     m_renderBucket;
	Color32 m_renderBucketColour_RB_OPAQUE     ;
	Color32 m_renderBucketColour_RB_ALPHA      ;
	Color32 m_renderBucketColour_RB_DECAL      ;
	Color32 m_renderBucketColour_RB_CUTOUT     ;
	Color32 m_renderBucketColour_RB_WATER      ;
	Color32 m_renderBucketColour_RB_DISPL_ALPHA;

	// used with m_colourMode == DOCM_PROP_PRIORITY_LEVEL
	int     m_propPriorityLevel;
	bool    m_propPriorityLevelIncludeStatic;
	bool    m_propPriorityLevelIncludeNonProp;
	Color32 m_propPriorityLevelColour_PRI_REQUIRED       ;
	Color32 m_propPriorityLevelColour_PRI_OPTIONAL_HIGH  ;
	Color32 m_propPriorityLevelColour_PRI_OPTIONAL_MEDIUM;
	Color32 m_propPriorityLevelColour_PRI_OPTIONAL_LOW   ;
	Color32 m_propPriorityLevelColour_STATIC             ;
	Color32 m_propPriorityLevelColour_NOT_A_PROP         ;
	Color32 m_propPriorityLevelColour_PRI_INVALID        ;

	// used with m_colourMode == DOCM_USES_PARENT_TXD
	int     m_usesParentTxdTextures; // eDebugOverlayUsesParentTxdTextures
	char    m_usesParentTxdNames[4][64];
	Color32 m_usesParentTxdColours[4];

	// used with m_colourMode == DOCM_VERTEX_VOLUME_RATIO
	bool    m_volumeDivideByCameraDistance;
	Color32 m_volumeColourMinBelow;
	Color32 m_volumeColourMin;
	Color32 m_volumeColourMid;
	Color32 m_volumeColourMax;
	Color32 m_volumeColourMaxAbove;
	float   m_volumeRatioMin;
	float   m_volumeRatioMax;
	float   m_volumeRatioExpBias;

	float   m_lineWidth; // not supported on pc
	float   m_lineWidthShaderEdit; // scales line width when in DOCM_SHADER_EDIT mode
	bool    m_lineSmooth;
#if !__D3D11
	bool    m_renderPoints;
#endif // !__D3D11
	int     m_culling;
	float   m_opacity;
	float   m_visibleOpacity;
	float   m_invisibleOpacity;
	Color32 m_colourNear;
	Color32 m_colourFar;
	float   m_fillOpacity;
	bool    m_fillOnly;
	bool    m_fillColourSeparate;
	Color32 m_fillColourNear;
	Color32 m_fillColourFar;
	float   m_zNear;
	float   m_zFar;
	bool    m_useWireframeOverride;
	bool    m_forceStates; // TODO -- i'm not sure why this is required, but it seems to fix alpha/decal/water geometry showing through the depth buffer
	bool    m_disableWireframe;

#if DEBUG_OVERLAY_CLIP_TEST
	bool    m_clipXEnable;
	float   m_clipX;
	bool    m_clipYEnable;
	float   m_clipY;
#endif // DEBUG_OVERLAY_CLIP_TEST

	bool    m_occlusion;
#if USE_EDGE
	bool    m_edgeViewportCull; // see grcDevice::SetWindow .. 'scissorArea'
#endif // USE_EDGE
	float   m_depthBias_z0;
	float   m_depthBias_z1;
	float   m_depthBias_d0;
	float   m_depthBias_d1;
	float   m_depthBias_scale;
	bool    m_drawForOneFrameOnly;

#if __DEV
	// TESTING RENDERPHASE-SPECIFIC LOD CONTROLS
	int     m_HighestDrawableLOD_debug;
	int     m_HighestFragmentLOD_debug;
	int     m_HighestVehicleLOD_debug;
	int     m_HighestPedLOD_debug;
#endif // __DEV
};

CDebugOverlay gDebugOverlay;

__COMMENT(static) void CDebugOverlay::ColourModeOrDrawTypeFlags_changed()
{
#if ENTITYSELECT_ENABLED_BUILD
	if (gDebugOverlay.m_colourMode != DOCM_NONE ||
		gDebugOverlay.m_drawTypeFlags != DEBUG_OVERLAY_DRAW_TYPE_FLAGS_ALL)
	{
		CEntitySelect::SetEntitySelectPassEnabled();
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	if (gDebugOverlay.m_colourMode == DOCM_MODEL_PATH_CONTAINS)
	{
		CDebugArchetype::CreateDebugArchetypeProxies();
	}
}

__COMMENT(static) void CDebugOverlay::SetCurrentPass(eDebugOverlayRenderPass pass, grmModel__CustomShaderParamsFuncType func)
{
	if (gDebugOverlay.m_colourMode != DOCM_NONE ||
		gDebugOverlay.m_drawTypeFlags != DEBUG_OVERLAY_DRAW_TYPE_FLAGS_ALL)
	{
		gDebugOverlay.m_pass = pass;
#if DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
		grmModel::SetCustomShaderParamsFunc(func);
#else
		(void)func;
#endif
	}
	else
	{
		gDebugOverlay.m_pass = DEBUG_OVERLAY_RENDER_PASS_NONE; // set pass to NONE so we don't do any fancy computation in BindShaderParams
#if DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
		grmModel::SetCustomShaderParamsFunc(NULL);
#endif // DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
	}
}

// ================================================================================================

#if ENTITYSELECT_ENABLED_BUILD
static u32 s_currentEntityId = 0;
static u32 s_currentEntityIdExtraBits = 0xdeadc0de; // hit CNTRL-ALT-W to rescramble random colours
static const CEntity* s_currentEntity = NULL;
#endif // ENTITYSELECT_ENABLED_BUILD

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::InitShaders()
{
	gDebugOverlay.Init();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::AddWidgets()
{
	bkBank* pRendererBank = BANKMGR.FindBank("Renderer");
	Assert(pRendererBank);

	pRendererBank->PushGroup("Overdraw", false);
	{
		gDebugOverlay.AddOverdrawWidgets(*pRendererBank);
	}
	pRendererBank->PopGroup();

	pRendererBank->PushGroup("Wireframe", false);
	{
		gDebugOverlay.AddWidgets(*pRendererBank);
	}
	pRendererBank->PopGroup();
}

__COMMENT(static) bool CRenderPhaseDebugOverlayInterface::IsEntitySelectRequired()
{
#if ENTITYSELECT_ENABLED_BUILD
	if (gDebugOverlay.m_draw)
	{
		return true; // technically not all modes require entity select, but let's be safe
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	return false;
}

static void CycleWireframeMode()
{
#if !__D3D11
	if (!gDebugOverlay.m_draw) // off -> wireframe
	{
		gDebugOverlay.m_draw = true;
		gDebugOverlay.m_renderPoints = false;
	}
	else if (!gDebugOverlay.m_renderPoints && !gDebugOverlay.m_drawSpecialMode) // wireframe -> points
	{
		gDebugOverlay.m_renderPoints = true;
	}
	else // points -> off
	{
		gDebugOverlay.m_draw = false;
		gDebugOverlay.m_renderPoints = false;
	}
#else
	gDebugOverlay.m_draw = !gDebugOverlay.m_draw;
#endif
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::Update()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG_ALT))
	{
		CycleWireframeMode();

		if (!gDebugOverlay.m_draw)
		{
			gDebugOverlay.m_overdrawRender = false;
		}
	}
	else if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_O, KEYBOARD_MODE_DEBUG_ALT))
	{
		gDebugOverlay.m_overdrawRender = !gDebugOverlay.m_overdrawRender;

		while (gDebugOverlay.m_overdrawRender && gDebugOverlay.m_draw)
		{
			CycleWireframeMode();
		}
	}
#if ENTITYSELECT_ENABLED_BUILD
	else if (CControlMgr::GetKeyboard().GetKeyDown(KEY_W, KEYBOARD_MODE_DEBUG_CNTRL_ALT))
	{
		s_currentEntityIdExtraBits = XorShiftLRL<XORSHIFT_LRL_DEFAULT>(s_currentEntityIdExtraBits);
	}
#endif // ENTITYSELECT_ENABLED_BUILD

#if 0 && __CONSOLE
	static bool wasDrawing = false;
	static int numWarnings = 0;

	if (gDebugOverlay.m_draw && !wasDrawing && numWarnings < 5)
	{
		u32 multisample_count = 0;
		u32 multisample_quality = 0;
		GRCDEVICE.GetMultiSample(multisample_count, multisample_quality);

		if (multisample_count > 1)
		{
			const bool nopopups = ForceNoPopupsBegin(false);
			fiRemoteShowMessageBox("Can't enable wireframe on consoles while multisample is enabled, run with -multisample=0 (see BS#1735980)", sysParam::GetProgramName(), MB_ICONASTERISK | MB_OK | MB_TOPMOST, IDOK);
			ForceNoPopupsEnd(nopopups);
		}

		numWarnings++;
	}

	wasDrawing = true;
#endif // __CONSOLE
}

__COMMENT(static) bool& CRenderPhaseDebugOverlayInterface::DrawEnabledRef()
{
	return gDebugOverlay.m_draw;
}

__COMMENT(static) float& CRenderPhaseDebugOverlayInterface::DrawOpacityRef()
{
	return gDebugOverlay.m_opacity;
}

__COMMENT(static) Color32& CRenderPhaseDebugOverlayInterface::DrawColourRef()
{
	return gDebugOverlay.m_colourNear;
}

__COMMENT(static) int& CRenderPhaseDebugOverlayInterface::ColourModeRef()
{
	return gDebugOverlay.m_colourMode;
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::SetChannel(eDebugOverlayChannel channel)
{
	gDebugOverlay.m_channel = channel;
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::SetColourMode(eDebugOverlayColourMode mode)
{
	gDebugOverlay.m_colourMode = (int)mode;
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::SetFilterString(const char* filterStr)
{
	strcpy(gDebugOverlay.m_filter, filterStr);
}

static fwEntity* gCurrentRenderingEntity = NULL; // TODO -- this is a hack

#if ENTITYSELECT_ENABLED_BUILD
static bool EntityContainsShaderName(const CEntity* entity, const char* substr)
{
	const Drawable* drawable = entity->GetDrawable();

	if (drawable)
	{
		const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup.GetCount(); i++)
		{
			if (strstr(shaderGroup.GetShader(0).GetName(), substr))
			{
				return true;
			}
		}
	}

	return false;
}
#endif

__COMMENT(static) bool CRenderPhaseDebugOverlayInterface::BindShaderParams(entitySelectID ENTITYSELECT_ONLY(entityId), int ENTITYSELECT_ONLY(renderBucket))
{
	gCurrentRenderingEntity = NULL;

	if (gDebugOverlay.m_pass == DEBUG_OVERLAY_RENDER_PASS_NONE)
	{
#if ENTITYSELECT_ENABLED_BUILD
		if (g_scanDebugFlagsPortals & (SCAN_DEBUG_FORCE_SHADOW_PROXIES_IN_PRIMARY|SCAN_DEBUG_FORCE_REFLECTION_PROXIES_IN_PRIMARY))
		{
			if (DRAWLISTMGR->IsExecutingGBufDrawList())
			{
				const CEntity* entity = (const CEntity*)CEntityIDHelper::GetEntityFromID(entityId);

				if (entity)
				{
					const fwFlags16 mask = entity->GetRenderPhaseVisibilityMask();

					const bool bIsShadowProxy     = mask.IsFlagSet(VIS_PHASE_MASK_SHADOWS)                                     && !mask.IsFlagSet(VIS_PHASE_MASK_GBUF);
					const bool bIsReflectionProxy = mask.IsFlagSet(VIS_PHASE_MASK_REFLECTIONS|VIS_PHASE_MASK_WATER_REFLECTION) && !mask.IsFlagSet(VIS_PHASE_MASK_GBUF);

					if (bIsShadowProxy || bIsReflectionProxy)
					{
						return false; // we're forcing proxies in primary visibility, but let's not draw the proxies in the gbuffer
					}
				}
			}
		}
#endif // ENTITYSELECT_ENABLED_BUILD

		return true;
	}

	bool bDraw = true;

#if ENTITYSELECT_ENABLED_BUILD
	const CEntity* entity = (const CEntity*)CEntityIDHelper::GetEntityFromID(entityId);

	if (gDebugOverlay.m_channel == DEBUG_OVERLAY_PED_DAMAGE && entity && !entity->GetIsTypePed())
	{
		return false;
	}

	if( gDebugOverlay.m_channel == DEBUG_OVERLAY_VEH_GLASS && entity && !entity->GetIsTypeVehicle())
	{
		return false;
	}

	if (entity && gDebugOverlay.m_drawTypeFlags != DEBUG_OVERLAY_DRAW_TYPE_FLAGS_ALL)
	{
		// TODO -- to make this much faster, we could store a static array of bitvectors for all the archetypes (~50k or so) that gets created on demand
		const bool bIsTypePed      = entity->GetIsTypePed();
		const bool bIsTypeVehicle  = entity->GetIsTypeVehicle();
		const bool bIsTypeTree     = (entity->GetBaseModelInfo() ? entity->GetBaseModelInfo()->GetIsTree() : false);
		const bool bIsTypeProp     = (entity->GetBaseModelInfo() ? entity->GetBaseModelInfo()->GetIsProp() : false) && !bIsTypeTree;
		const bool bIsTypeTerrain  = EntityContainsShaderName(entity, "terrain_cb_4lyr");
		const bool bIsTypeWater    = EntityContainsShaderName(entity, "water");
		const bool bIsTypeMap      = !bIsTypePed && !bIsTypeVehicle && !bIsTypeTree && !bIsTypeProp && !bIsTypeTerrain && !bIsTypeWater; // everything else

		if (((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_PEDS    )) != 0 && bIsTypePed     ) ||
			((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_VEHICLES)) != 0 && bIsTypeVehicle ) ||
			((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_MAP     )) != 0 && bIsTypeMap     ) ||
			((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_PROPS   )) != 0 && bIsTypeProp    ) ||
			((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_TREES   )) != 0 && bIsTypeTree    ) ||
			((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_TERRAIN )) != 0 && bIsTypeTerrain ) ||
			((gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_WATER   )) != 0 && bIsTypeWater   ) ||
			false)
		{
			// ok
		}
		else
		{
			return false;
		}
	}

	//if (gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE)
	{
		Color32 c = Color32(0);

		bDraw = false;
		s_currentEntityId = (u32)entityId + s_currentEntityIdExtraBits;
		s_currentEntity = entity;

		switch ((int)gDebugOverlay.m_pass)
		{
		case DEBUG_OVERLAY_RENDER_PASS_FILL: c = gDebugOverlay.m_fillColourSeparate ? gDebugOverlay.m_fillColourNear : gDebugOverlay.m_colourNear; break;
		case DEBUG_OVERLAY_RENDER_PASS_WIREFRAME: c = gDebugOverlay.m_colourNear; break;
		}

		Vec4V colour = Vec4V(V_ZERO);

		switch ((eDebugOverlayColourMode)gDebugOverlay.m_colourMode)
		{
		case DOCM_NONE:
		{
			colour = c.GetRGBA();
			bDraw = true;
			break;
		}
		case DOCM_RENDER_BUCKETS:
		{
			const bool bIsGBufferBucket =
			(
				renderBucket == CRenderer::RB_OPAQUE   ||
				renderBucket == CRenderer::RB_DECAL    ||
				renderBucket == CRenderer::RB_CUTOUT
			);

			if ((gDebugOverlay.m_renderBucket == 0) || // all
				(gDebugOverlay.m_renderBucket == 1 &&  bIsGBufferBucket) ||
				(gDebugOverlay.m_renderBucket == 2 && !bIsGBufferBucket) ||
				(gDebugOverlay.m_renderBucket == renderBucket + 3))
			{
				switch (renderBucket)
				{
				case CRenderer::RB_OPAQUE      : colour = gDebugOverlay.m_renderBucketColour_RB_OPAQUE     .GetRGBA(); break;
				case CRenderer::RB_ALPHA       : colour = gDebugOverlay.m_renderBucketColour_RB_ALPHA      .GetRGBA(); break;
				case CRenderer::RB_DECAL       : colour = gDebugOverlay.m_renderBucketColour_RB_DECAL      .GetRGBA(); break;
				case CRenderer::RB_CUTOUT      : colour = gDebugOverlay.m_renderBucketColour_RB_CUTOUT     .GetRGBA(); break;
				case CRenderer::RB_WATER       : colour = gDebugOverlay.m_renderBucketColour_RB_WATER      .GetRGBA(); break;
				case CRenderer::RB_DISPL_ALPHA : colour = gDebugOverlay.m_renderBucketColour_RB_DISPL_ALPHA.GetRGBA(); break;
				default                        : colour = Vec4V(V_ZERO_WONE); break;
				}

				bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
			}
			break;
		}
		case DOCM_TXD_COST:
		case DOCM_TXD_COST_OVER_AREA:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
#if GTA_VERSION
					const strLocalIndex txdSlot = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
					if (txdSlot.Get() != -1 && g_TxdStore.IsObjectInImage(txdSlot))
#elif RDR_VERSION
					const int txdSlot = modelInfo->GetAssetParentTxdIndex();
					if (txdSlot != -1 && g_TxdStore.IsObjectInImage(txdSlot))
#endif
					{
						strIndex txdStreamIndex = g_TxdStore.GetStreamingIndex(txdSlot);
						const strStreamingInfo* streamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(txdStreamIndex);
						const float size = (float)streamingInfo->ComputePhysicalSize(txdStreamIndex, true)/1024.0f;
						const float p = Clamp<float>((size - gDebugOverlay.m_txdCostSizeMin)/(gDebugOverlay.m_txdCostSizeMax - gDebugOverlay.m_txdCostSizeMin), 0.0f, 1.0f);

						float cost = p;

						if (gDebugOverlay.m_colourMode == DOCM_TXD_COST_OVER_AREA)
						{
							spdAABB bounds;
							entity->GetAABB(bounds);

							const float area = bounds.GetExtent().GetXf()*bounds.GetExtent().GetYf()*4.0f;
							const float q = Clamp<float>((area - gDebugOverlay.m_txdCostAreaMin)/(gDebugOverlay.m_txdCostAreaMax - gDebugOverlay.m_txdCostAreaMin), 0.0f, 1.0f);

							if (q > 0.0f)
							{
								cost = Clamp<float>((p/q - gDebugOverlay.m_txdCostRatioMin)/(gDebugOverlay.m_txdCostRatioMax - gDebugOverlay.m_txdCostRatioMin), 0.0f, 1.0f);
							}
							else
							{
								cost = 1.0f;
							}
						}

						if (gDebugOverlay.m_txdCostExpBias != 0.0f)
						{
							cost = powf(cost, powf(2.0f, -gDebugOverlay.m_txdCostExpBias));
						}

						const Vec4V colour0 = Vec4V(V_Z_AXIS_WONE); // blue
						const Vec4V colour1 = Vec4V(V_Y_AXIS_WONE); // green
						const Vec4V colour2 = Vec4V(1.0f, 1.0f, 0.0f, 1.0f); // yellow
						const Vec4V colour3 = Vec4V(V_X_AXIS_WONE); // red

						if      (cost*3.0f < 1.0f) { colour = Lerp(ScalarV(cost*3.0f - 0.0f), colour0, colour1); }
						else if (cost*3.0f < 2.0f) { colour = Lerp(ScalarV(cost*3.0f - 1.0f), colour1, colour2); }
						else                       { colour = Lerp(ScalarV(cost*3.0f - 2.0f), colour2, colour3); }

						bDraw = true;
					}
				}
			}
			break;
		}
		case DOCM_MODEL_PATH_CONTAINS:
		{
			if (entity)
			{
				if (StringMatch(AssetAnalysis::GetArchetypePath(CDebugArchetype::GetDebugArchetypeProxyForModelInfo(entity->GetBaseModelInfo())).c_str(), gDebugOverlay.m_filter))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_MODEL_NAME_IS:
		{
			if (entity)
			{
				if (StringMatch(entity->GetModelName(), gDebugOverlay.m_filter))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_MODEL_USES_TEXTURE:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && IsTextureUsedByModel(NULL, gDebugOverlay.m_filter, modelInfo, entity))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_MODEL_USES_PARENT_TXD:
		case DOCM_MODEL_DOES_NOT_USE_PARENT_TXD:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					for (int i = 0; i <= NELEM(gDebugOverlay.m_usesParentTxdColours); i++)
					{
						strLocalIndex parentTxdSlot = strLocalIndex(modelInfo->GetAssetParentTxdIndex());
						bool bUsesParentTxd = false;

						const char*   parentTxdName   = (i == 0) ? gDebugOverlay.m_filter : gDebugOverlay.m_usesParentTxdNames  [i - 1];
						const Color32 parentTxdColour = (i == 0) ? c.GetRGBA()            : gDebugOverlay.m_usesParentTxdColours[i - 1];

						if (parentTxdName[0] == '\0' || parentTxdColour.GetAlpha() == 0)
						{
							continue;
						}

						while (parentTxdSlot != -1)
						{
							if (StringMatch(g_TxdStore.GetName(parentTxdSlot), parentTxdName))
							{
								bUsesParentTxd = true;

								if (gDebugOverlay.m_colourMode == DOCM_MODEL_USES_PARENT_TXD &&
									gDebugOverlay.m_usesParentTxdTextures != DOUPTT_ALWAYS)
								{
									const fwTxd* txd = g_TxdStore.Get(parentTxdSlot);
									bool bUsesParentTxdTextures = false;

									if (txd)
									{
										for (int k = 0; k < txd->GetCount(); k++)
										{
											if (IsTextureUsedByModel(txd->GetEntry(k), NULL, modelInfo, entity))
											{
												bUsesParentTxdTextures = true;
												break;
											}
										}
									}

									if (gDebugOverlay.m_usesParentTxdTextures == DOUPTT_ONLY_IF_USING_TEXTURES     && !bUsesParentTxdTextures) { bUsesParentTxd = false; }
									if (gDebugOverlay.m_usesParentTxdTextures == DOUPTT_ONLY_IF_NOT_USING_TEXTURES &&  bUsesParentTxdTextures) { bUsesParentTxd = false; }
								}

								break;
							}

							parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
						}

						if (gDebugOverlay.m_colourMode == DOCM_MODEL_USES_PARENT_TXD)
						{
							if (bUsesParentTxd)
							{
								colour = parentTxdColour.GetRGBA();
								bDraw = true;
								break;
							}
						}
						else
						{
							if (!bUsesParentTxd)
							{
								colour = parentTxdColour.GetRGBA();
								bDraw = true;
							}

							break;
						}
					}
				}
			}
			break;
		}
		case DOCM_SHADER_NAME_IS:
		case DOCM_SHADER_USES_TEXTURE:
		case DOCM_SHADER_USES_TXD:
		{
			colour = c.GetRGBA();
			bDraw = true;
			break;
		}
		case DOCM_RANDOM_COLOUR_PER_ENTITY:
		{
			c = Color32(ScrambleBits32(ScrambleBits32((u32)s_currentEntityId))); // TODO -- why do we need double-scramble?
			c.SetAlpha(255);
			colour = c.GetRGBA();
			bDraw = true;
			break;
		}
		case DOCM_RANDOM_COLOUR_PER_TEXTURE_DICTIONARY:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					for (int i = 0; i <= NELEM(gDebugOverlay.m_usesParentTxdColours); i++)
					{
						const int parentTxdSlot = modelInfo->GetAssetParentTxdIndex();

						c = Color32(ScrambleBits32(ScrambleBits32(parentTxdSlot))); // TODO -- why do we need double-scramble?
						c.SetAlpha(255);
						colour = c.GetRGBA();
						bDraw = true;
					}
				}
			}
			break;
		}
		case DOCM_RANDOM_COLOUR_PER_SHADER:
		{
			colour = Vec4V(V_ONE);
			bDraw = true;
			break;
		}
		case DOCM_RANDOM_COLOUR_PER_DRAW_CALL:
		{
			colour = Vec4V(V_ONE);
			bDraw = true;
			break;
		}
		case DOCM_PICKER:
		{
			// TODO -- cache these? and also cache the colours, and only update the var when it's changed (or maybe just pass a separate shader var)

			const CEntity*			selectedEntity   = (CEntity*)g_PickerManager.GetSelectedEntity();
			const entitySelectID	selectedEntityId = selectedEntity ? CEntityIDHelper::ComputeEntityID(selectedEntity) : 0;
			const CEntity*			hoverEntity      = (CEntity*)g_PickerManager.GetHoverEntity();
			const entitySelectID    hoverEntityId    = hoverEntity ? CEntityIDHelper::ComputeEntityID(hoverEntity) : 0;

			if (entityId == selectedEntityId)
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			else if (g_PickerManager.GetNumberOfEntities() > 1)
			{
				for (int i = 0; i < g_PickerManager.GetNumberOfEntities(); i++)
				{
					const CEntity* listedEntity   = (CEntity*)g_PickerManager.GetEntity(i);
					const entitySelectID listedEntityId = listedEntity ? CEntityIDHelper::ComputeEntityID(listedEntity) : 0;

					if (entityId == listedEntityId)
					{
						colour = c.GetRGBA();
						bDraw = true;
						break;
					}
				}
			}
			else if (g_PickerManager.GetNumberOfEntities() == 0 && entityId == hoverEntityId)
			{
				//colour = c.GetRGBA();
				//colour *= Vec4V(Vec3V(V_ONE), ScalarV(V_QUARTER)); // hover entity has 1/4 opacity
				//bDraw = true;
			}
			break;
		}
		case DOCM_ENTITYSELECT:
		{
			if (CEntityIDHelper::GetEntityFromID(entityId) == CEntitySelect::GetSelectedEntity())
			{
				colour = c.GetRGBA();
				bDraw = true; // this will get filtered per-shader later
			}
			else
			{
				bDraw = false;
			}
			break;
		}
		case DOCM_SHADER_EDIT:
		{
			colour = c.GetRGBA();
			bDraw = true; // this will get filtered per-shader later
			break;
		}
		case DOCM_TEXTURE_VIEWER:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && modelInfo == CDebugTextureViewerInterface::GetSelectedModelInfo_renderthread())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_INTERIOR_LOCATION:
		case DOCM_INTERIOR_LOCATION_LIMBO_ONLY:
		case DOCM_INTERIOR_LOCATION_PORTAL_ONLY:
		{
			if (entity)
			{
				const fwInteriorLocation location = entity->GetInteriorLocation();

				if (location.IsValid())
				{
					if (location.IsAttachedToRoom())
					{
						if (location.GetRoomIndex() == 0) // limbo = white
						{
							if (gDebugOverlay.m_colourMode == DOCM_INTERIOR_LOCATION ||
								gDebugOverlay.m_colourMode == DOCM_INTERIOR_LOCATION_LIMBO_ONLY)
							{
								colour = Vec4V(V_ONE);
								bDraw = true;
							}
						}
						else if (gDebugOverlay.m_colourMode == DOCM_INTERIOR_LOCATION)
						{
							const u32 bits = ScrambleBits32(location.GetInteriorProxyIndex() + location.GetRoomIndex()*256);

							colour = Vec4V(
								(float)((bits>>(0*8))&0xff)/255.0f, 
								(float)((bits>>(1*8))&0xff)/255.0f, 
								(float)((bits>>(2*8))&0xff)/255.0f, 
								1.0f
							);
							bDraw = true;
						}
					}
					else if (location.IsAttachedToPortal()) // portal = red
					{
						if (gDebugOverlay.m_colourMode == DOCM_INTERIOR_LOCATION ||
							gDebugOverlay.m_colourMode == DOCM_INTERIOR_LOCATION_PORTAL_ONLY)
						{
							colour = Vec4V(V_X_AXIS_WONE);
							bDraw = true;
						}
					}
				}
			}
			break;
		}
		case DOCM_INTERIOR_LOCATION_INVALID:
		{
			if (entity)
			{
				const fwInteriorLocation location = entity->GetInteriorLocation();

				if (!location.IsValid())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_GEOMETRY_COUNTS:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				const rmcDrawable* drawable = modelInfo->GetDrawable();

				if (drawable)
				{
					const rmcLodGroup* lodGroup = &drawable->GetLodGroup();
					const rmcLod* lod = NULL;

					for (int i = LOD_HIGH; i < LOD_COUNT; i++) // find highest available lod
					{
						if (lodGroup->ContainsLod(i))
						{
							lod = &lodGroup->GetLod(i);
							break;
						}
					}

					if (lod)
					{
						const int modelCount = lod->GetCount();
						int geomCount = 0;

						for (int i = 0; i < modelCount; i++)
						{
							geomCount += lod->GetModel(i)->GetGeometryCount();
						}

						if      (geomCount < gDebugOverlay.m_geometryCountMin) { colour = gDebugOverlay.m_geometryCountLessThanMinColour   .GetRGBA(); }
						else if (geomCount > gDebugOverlay.m_geometryCountMax) { colour = gDebugOverlay.m_geometryCountGreaterThanMaxColour.GetRGBA(); }
						else
						{
							const Vec4V colour0 = gDebugOverlay.m_geometryCountMinColour.GetRGBA();
							const Vec4V colour1 = gDebugOverlay.m_geometryCountMaxColour.GetRGBA();

							colour = colour0 + (colour1 - colour0)*ScalarV((float)(geomCount - gDebugOverlay.m_geometryCountMin)/(float)(gDebugOverlay.m_geometryCountMax - gDebugOverlay.m_geometryCountMin));
						}

						bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
					}
				}
			}
			break;
		}
		case DOCM_ENTITY_FLAGS:
		{
			if (entity)
			{
				const bool bFlagged = (entity->GetBaseFlags() & gDebugOverlay.m_entityFlags) != 0;

				if (bFlagged != gDebugOverlay.m_entityFlagsFlip)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_ENTITY_TYPE:
		{
			if (entity)
			{
				const int entityType = entity->GetType();

				if ((gDebugOverlay.m_entityType == 0) || // all
					(gDebugOverlay.m_entityType == entityType + 1))
				{
					switch (entityType)
					{
					case ENTITY_TYPE_NOTHING             : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_NOTHING            .GetRGBA(); break;
					case ENTITY_TYPE_BUILDING            : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_BUILDING           .GetRGBA(); break;
					case ENTITY_TYPE_ANIMATED_BUILDING   : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_ANIMATED_BUILDING  .GetRGBA(); break;
					case ENTITY_TYPE_VEHICLE             : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_VEHICLE            .GetRGBA(); break;
					case ENTITY_TYPE_PED                 : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_PED                .GetRGBA(); break;
					case ENTITY_TYPE_OBJECT              : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_OBJECT             .GetRGBA(); break;
					case ENTITY_TYPE_DUMMY_OBJECT        : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_DUMMY_OBJECT       .GetRGBA(); break;
					case ENTITY_TYPE_PORTAL              : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_PORTAL             .GetRGBA(); break;
					case ENTITY_TYPE_MLO                 : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_MLO                .GetRGBA(); break;
					case ENTITY_TYPE_NOTINPOOLS          : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_NOTINPOOLS         .GetRGBA(); break;
					case ENTITY_TYPE_PARTICLESYSTEM      : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_PARTICLESYSTEM     .GetRGBA(); break;
					case ENTITY_TYPE_LIGHT               : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_LIGHT              .GetRGBA(); break;
					case ENTITY_TYPE_COMPOSITE           : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_COMPOSITE          .GetRGBA(); break;
					case ENTITY_TYPE_INSTANCE_LIST       : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_INSTANCE_LIST      .GetRGBA(); break;
					case ENTITY_TYPE_GRASS_INSTANCE_LIST : colour = gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_GRASS_INSTANCE_LIST.GetRGBA(); break;
					default                              : colour = Vec4V(V_ZERO_WONE); break;
					}

					bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_PHYSICAL:
		{
			if (entity && entity->GetIsPhysical())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_DYNAMIC:
		{
			if (entity && entity->GetIsDynamic())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_STATIC:
		{
			if (entity && entity->GetIsStatic())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_NOT_STATIC:
		{
			if (entity && !entity->GetIsStatic())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_NOT_HD_TXD_CAPABLE:
		{
			if (entity && !entity->GetIsHDTxdCapable())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_HD_TXD_CAPABLE:
		{
			if (entity && entity->GetIsHDTxdCapable())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_CURRENTLY_HD:
		{
			if (entity && entity->GetIsCurrentlyHD())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_RENDER_IN_SHADOWS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (flags.IsFlagSet(VIS_PHASE_MASK_SHADOWS))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_DONT_RENDER_IN_SHADOWS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (!flags.AreAllFlagsSet(VIS_PHASE_MASK_SHADOWS))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_ONLY_RENDER_IN_SHADOWS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (flags.IsFlagSet(VIS_PHASE_MASK_SHADOWS) && !flags.IsFlagSet(VIS_PHASE_MASK_GBUF))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_RENDER_IN_MIRROR_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (flags.IsFlagSet(VIS_PHASE_MASK_MIRROR_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_RENDER_IN_WATER_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (flags.IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_RENDER_IN_PARABOLOID_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (flags.IsFlagSet(VIS_PHASE_MASK_PARABOLOID_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_DONT_RENDER_IN_ALL_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (!flags.AreAllFlagsSet(VIS_PHASE_MASK_REFLECTIONS | VIS_PHASE_MASK_WATER_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;

					if (EntityContainsShaderName(entity, "mirror"))
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
					}
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_DONT_RENDER_IN_ANY_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (!flags.IsFlagSet(VIS_PHASE_MASK_REFLECTIONS | VIS_PHASE_MASK_WATER_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;

					if (EntityContainsShaderName(entity, "mirror"))
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
					}
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_ONLY_RENDER_IN_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (flags.IsFlagSet(VIS_PHASE_MASK_REFLECTIONS | VIS_PHASE_MASK_WATER_REFLECTION) && !flags.IsFlagSet(VIS_PHASE_MASK_GBUF))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_DONT_RENDER_IN_MIRROR_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (!flags.IsFlagSet(VIS_PHASE_MASK_MIRROR_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;

					if (EntityContainsShaderName(entity, "mirror"))
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
					}
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_DONT_RENDER_IN_WATER_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (!flags.IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;

					if (EntityContainsShaderName(entity, "mirror"))
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
					}
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_DONT_RENDER_IN_PARABOLOID_REFLECTIONS:
		{
			if (entity)
			{
				const fwFlags16& flags = entity->GetRenderPhaseVisibilityMask();

				if (!flags.IsFlagSet(VIS_PHASE_MASK_PARABOLOID_REFLECTION))
				{
					colour = c.GetRGBA();
					bDraw = true;

					if (EntityContainsShaderName(entity, "mirror"))
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
					}
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_ALWAYS_PRERENDER:
		{
			if (entity && entity->GetAlwaysPreRender())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_ALPHA_LESS_THAN_255:
		{
			if (entity && entity->GetAlpha() < 255)
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_SMASHABLE:
		{
			if (entity && IsEntitySmashable(entity))
			{
				colour = c.GetRGBA();
				bDraw = true;

				if (entity->GetIsTypeObject() && static_cast<const CPhysical*>(entity)->m_nPhysicalFlags.bNotDamagedByBullets)
				{
					colour = Vec4V(V_X_AXIS_WONE); // red
				}
			}
			break;
		}
		case DOCM_IS_ENTITY_LOW_PRIORITY_STREAM:
		{
			if (entity && entity->IsBaseFlagSet(fwEntity::LOW_PRIORITY_STREAM))
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_ENTITY_LIGHT_INSTANCED:
		{
			if (entity && entity->GetExtension<CLightExtension>())
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_PROP_PRIORITY_LEVEL:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				int priority = 0;

				const u32 fwEntityDef__STATIC      = fwEntityDef::PRI_COUNT + 0;
				const u32 fwEntityDef__NOT_A_PROP  = fwEntityDef::PRI_COUNT + 1;
				const u32 fwEntityDef__PRI_INVALID = fwEntityDef::PRI_COUNT + 2;

				if ((modelInfo && modelInfo->GetIsProp()) || gDebugOverlay.m_propPriorityLevelIncludeNonProp)
				{
					priority = entity->GetDebugPriority();

					if (!entity->IsBaseFlagSet(fwEntity::IS_DYNAMIC) && !gDebugOverlay.m_propPriorityLevelIncludeStatic)
					{
						//Assertf(priority == 0, "%s is not dynamic but has priority level %d", entity->GetModelName(), priority);
						priority = fwEntityDef__STATIC;
					}
					else if (priority < 0 || priority >= fwEntityDef::PRI_COUNT)
					{
						Assertf(0, "%s unknown priority level %d", entity->GetModelName(), priority);
						priority = fwEntityDef__PRI_INVALID;
					}
				}
				else
				{
					priority = fwEntityDef__NOT_A_PROP;
				}

				if (gDebugOverlay.m_propPriorityLevel == 0 ||
					gDebugOverlay.m_propPriorityLevel == priority + 1)
				{
					Color32 c(0);

					switch (priority)
					{
					case fwEntityDef::PRI_REQUIRED        : c = gDebugOverlay.m_propPriorityLevelColour_PRI_REQUIRED       ; break;
					case fwEntityDef::PRI_OPTIONAL_HIGH   : c = gDebugOverlay.m_propPriorityLevelColour_PRI_OPTIONAL_HIGH  ; break;
					case fwEntityDef::PRI_OPTIONAL_MEDIUM : c = gDebugOverlay.m_propPriorityLevelColour_PRI_OPTIONAL_MEDIUM; break;
					case fwEntityDef::PRI_OPTIONAL_LOW    : c = gDebugOverlay.m_propPriorityLevelColour_PRI_OPTIONAL_LOW   ; break;
					case fwEntityDef__STATIC              : c = gDebugOverlay.m_propPriorityLevelColour_STATIC             ; break;
					case fwEntityDef__NOT_A_PROP          : c = gDebugOverlay.m_propPriorityLevelColour_NOT_A_PROP         ; break;
					case fwEntityDef__PRI_INVALID         : c = gDebugOverlay.m_propPriorityLevelColour_PRI_INVALID        ; break;
					}

					if (c.GetAlpha() > 0)
					{
						colour = c.GetRGBA();
						bDraw = true;
					}
				}
			}
			break;
		}
		case DOCM_IS_PED_HD:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && modelInfo->GetModelType() == MI_TYPE_PED)
				{
					const CPedModelInfo* pedModelInfo = static_cast<const CPedModelInfo*>(modelInfo);

					if (!pedModelInfo->GetHasHDFiles()) // not HD capable
					{
						const bool bIsPlayerPed = entity->GetIsTypePed() ? static_cast<const CPed*>(entity)->IsPlayer() : false;

						if (bIsPlayerPed)
						{
							colour = Vec4V(ScalarV(V_ZERO), Vec3V(V_ONE)); // cyan
						}
						else
						{
							colour = Vec4V(V_Z_AXIS_WONE); // blue
						}
					}
					else if (!pedModelInfo->GetAreHDFilesLoaded()) // not currently HD
					{
						colour = Vec4V(V_Y_AXIS_WONE); // green
					}
					else // currently HD
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
					}

					bDraw = true;
				}
			}
			break;
		}
		case DOCM_MODEL_TYPE:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					const int modelType = modelInfo->GetModelType();

					if ((gDebugOverlay.m_modelType == 0) || // all
						(gDebugOverlay.m_modelType == modelType + 1))
					{
						switch (modelType)
						{
						case MI_TYPE_NONE      : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_NONE     .GetRGBA(); break;
						case MI_TYPE_BASE      : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_BASE     .GetRGBA(); break;
						case MI_TYPE_MLO       : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_MLO      .GetRGBA(); break;
						case MI_TYPE_TIME      : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_TIME     .GetRGBA(); break;
						case MI_TYPE_WEAPON    : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_WEAPON   .GetRGBA(); break;
						case MI_TYPE_VEHICLE   : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_VEHICLE  .GetRGBA(); break;
						case MI_TYPE_PED       : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_PED      .GetRGBA(); break;
						case MI_TYPE_COMPOSITE : colour = gDebugOverlay.m_modelTypeColour_MI_TYPE_COMPOSITE.GetRGBA(); break;
						default                : colour = Vec4V(V_ZERO_WONE); break;
						}

						bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
					}
				}
			}
			break;
		}
		case DOCM_MODEL_HD_STATE:
		{
			if (entity)
			{
				if (entity->GetLodData().GetLodType() == LODTYPES_DEPTH_HD ||
					entity->GetLodData().GetLodType() == LODTYPES_DEPTH_ORPHANHD)
				{
					int state = MODEL_HD_STATE_NOT_HD_TXD_CAPABLE;

					if (entity->GetIsCurrentlyHD() && !gDebugOverlay.m_modelHDStateIgnoreEntityHDState)
					{
						state = MODEL_HD_STATE_HD_ENTITY;
					}
					else if (gDebugOverlay.m_modelHDStateSimpleDistanceOnly)
					{
						if (entity->GetIsHDTxdCapable())
						{
							if (CTexLod::IsEntityCloseEnoughForHDSwitch(entity))
							{
								state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD;
							}
							else
							{
								state = MODEL_HD_STATE_HD_TXD_CAPABLE;
							}
						}
					}
					else if (gDebugOverlay.m_modelHDStateSimple)
					{
						const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

						if (modelInfo)
						{
							switch (modelInfo->GetDrawableType())
							{
							case fwArchetype::DT_DRAWABLE:
								if      (g_DrawableStore.GetIsBoundHD  (GTA_ONLY(strLocalIndex)(modelInfo->GetDrawableIndex()))) { state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD; }
								else if (g_DrawableStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetDrawableIndex()))) { state = MODEL_HD_STATE_HD_TXD_CAPABLE; }
								break;
							case fwArchetype::DT_FRAGMENT:
								if      (g_FragmentStore.GetIsBoundHD  (GTA_ONLY(strLocalIndex)(modelInfo->GetFragmentIndex()))) { state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD; }
								else if (g_FragmentStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetFragmentIndex()))) { state = MODEL_HD_STATE_HD_TXD_CAPABLE; }
								break;
							case fwArchetype::DT_DRAWABLEDICTIONARY:
								if      (g_DwdStore.GetIsBoundHD  (GTA_ONLY(strLocalIndex)(modelInfo->GetDrawDictIndex()))) { state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD; }
								else if (g_DwdStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetDrawDictIndex()))) { state = MODEL_HD_STATE_HD_TXD_CAPABLE; }
								break;
							default:
								break;
							}

							if (CTexLod::IsEntityCloseEnoughForHDSwitch(entity))
							{
								if (state == MODEL_HD_STATE_HD_TXD_CAPABLE)
								{
									state = MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING;
								}
							}
							else
							{
								if (state == MODEL_HD_STATE_CURRENTLY_USING_HD_TXD)
								{
									state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED;
								}
							}

						}
					}
					else if (CTexLod::IsModelUpgradedToHD(entity->GetModelIndex())) // entity->GetIsCurrentlyHD()
					{
						if (CTexLod::IsEntityCloseEnoughForHDSwitch(entity))
						{
							state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD;
						}
						else
						{
							state = MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED;
						}
					}
					else if (CTexLod::IsModelHDTxdCapable(entity->GetModelIndex())) // entity->GetIsHDTxdCapable()
					{
						if (CTexLod::IsEntityCloseEnoughForHDSwitch(entity))
						{
							state = MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING;
						}
						else
						{
							state = MODEL_HD_STATE_HD_TXD_CAPABLE;
						}
					}

					if ((gDebugOverlay.m_modelHDState == 0) || // all
						(gDebugOverlay.m_modelHDState == state + 1))
					{
						switch (state)
						{
						case MODEL_HD_STATE_HD_ENTITY                     : colour = gDebugOverlay.m_modelHDStateColour_MODEL_HD_STATE_HD_ENTITY                    .GetRGBA(); break;
						case MODEL_HD_STATE_CURRENTLY_USING_HD_TXD        : colour = gDebugOverlay.m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD       .GetRGBA(); break;
						case MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED : colour = gDebugOverlay.m_modelHDStateColour_MODEL_HD_STATE_CURRENTLY_USING_HD_TXD_SHARED.GetRGBA(); break;
						case MODEL_HD_STATE_HD_TXD_CAPABLE                : colour = gDebugOverlay.m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE               .GetRGBA(); break;
						case MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING      : colour = gDebugOverlay.m_modelHDStateColour_MODEL_HD_STATE_HD_TXD_CAPABLE_SWITCHING     .GetRGBA(); break;
						case MODEL_HD_STATE_NOT_HD_TXD_CAPABLE            : colour = gDebugOverlay.m_modelHDStateColour_MODEL_HD_STATE_NOT_HD_TXD_CAPABLE           .GetRGBA(); break;
						default                                           : colour = Vec4V(V_ZERO_WONE); break;
						}

						bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
					}
				}
			}
			break;
		}
		case DOCM_IS_MODEL_NOT_HD_TXD_CAPABLE:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && !modelInfo->GetIsHDTxdCapable())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_MODEL_HD_TXD_CAPABLE:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && modelInfo->GetIsHDTxdCapable())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_MODEL_BASE_HD_TXD_CAPABLE:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					switch (modelInfo->GetDrawableType())
					{
					case fwArchetype::DT_DRAWABLE:           bDraw = g_DrawableStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetDrawableIndex())); break;
					case fwArchetype::DT_FRAGMENT:           bDraw = g_FragmentStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetFragmentIndex())); break;
					case fwArchetype::DT_DRAWABLEDICTIONARY: bDraw = g_DwdStore     .GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetDrawDictIndex())); break;
					default: break;
					}

					if (bDraw)
					{
						colour = c.GetRGBA();
					}
				}
			}
			break;
		}
		case DOCM_IS_MODEL_ANY_HD_TXD_CAPABLE:
		{
			if (entity && CTexLod::IsModelHDTxdCapable(entity->GetModelIndex()))
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_IS_MODEL_CURRENTLY_USING_BASE_HD_TXD:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					switch (modelInfo->GetDrawableType())
					{
					case fwArchetype::DT_DRAWABLE:           bDraw = g_DrawableStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetDrawableIndex())); break;
					case fwArchetype::DT_FRAGMENT:           bDraw = g_FragmentStore.GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetFragmentIndex())); break;
					case fwArchetype::DT_DRAWABLEDICTIONARY: bDraw = g_DwdStore     .GetIsHDCapable(GTA_ONLY(strLocalIndex)(modelInfo->GetDrawDictIndex())); break;
					default: break;
					}

					if (bDraw)
					{
						colour = c.GetRGBA();
					}
				}
			}
			break;
		}
		case DOCM_IS_MODEL_CURRENTLY_USING_ANY_HD_TXD:
		{
			if (entity && CTexLod::IsModelUpgradedToHD(entity->GetModelIndex()))
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
			break;
		}
		case DOCM_LOD_TYPE:
		{
			if (entity)
			{
				const int LODType = entity->GetLodData().GetLodType();

				if ((gDebugOverlay.m_LODType == 0) || // all
					(gDebugOverlay.m_LODType == LODType + 1))
				{
					switch (LODType)
					{
					case LODTYPES_DEPTH_HD       : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_HD      .GetRGBA(); break;
					case LODTYPES_DEPTH_LOD      : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_LOD     .GetRGBA(); break;
					case LODTYPES_DEPTH_SLOD1    : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_SLOD1   .GetRGBA(); break;
					case LODTYPES_DEPTH_SLOD2    : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_SLOD2   .GetRGBA(); break;
					case LODTYPES_DEPTH_SLOD3    : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_SLOD3   .GetRGBA(); break;
					case LODTYPES_DEPTH_SLOD4    : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_SLOD4   .GetRGBA(); break;
					case LODTYPES_DEPTH_ORPHANHD : colour = gDebugOverlay.m_LODTypeColour_LODTYPES_DEPTH_ORPHANHD.GetRGBA(); break;
					default                      : colour = Vec4V(V_ZERO_WONE); break;
					}

					bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
				}
			}
			break;
		}
		/*case DOCM_DRAWABLE_LOD_INDEX:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				const rmcDrawable* drawable = modelInfo->GetDrawable();

				if (drawable)
				{
					const rmcLodGroup* drawableLODGroup = &drawable->GetLodGroup();
					const int          drawableLODIndex = drawableLODGroup->GetCurrentLOD();

					if ((gDebugOverlay.m_drawableLODIndex == 0) || // all
						(gDebugOverlay.m_drawableLODIndex == drawableLODIndex + 1))
					{
						switch (drawableLODIndex)
						{
						case LOD_HIGH : colour = gDebugOverlay.m_drawableLODIndexColour_LOD_HIGH.GetRGBA(); break;
						case LOD_MED  : colour = gDebugOverlay.m_drawableLODIndexColour_LOD_MED .GetRGBA(); break;
						case LOD_LOW  : colour = gDebugOverlay.m_drawableLODIndexColour_LOD_LOW .GetRGBA(); break;
						case LOD_VLOW : colour = gDebugOverlay.m_drawableLODIndexColour_LOD_VLOW.GetRGBA(); break;
						default       : colour = Vec4V(V_ZERO_WONE); break;
						}

						bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
					}
				}
			}
			break;
		}*/
		case DOCM_DRAWABLE_TYPE:
		{
			if (entity)
			{
				const int drawableType = (int)entity->GetBaseModelInfo()->GetDrawableType(); // DrawableType

				if ((gDebugOverlay.m_drawableType == 0) || // all
					(gDebugOverlay.m_drawableType == drawableType + 1))
				{
					switch (drawableType)
					{
					case fwArchetype::DT_UNINITIALIZED      : colour = gDebugOverlay.m_drawableTypeColour_DT_UNINITIALIZED     .GetRGBA(); break;
					case fwArchetype::DT_FRAGMENT           : colour = gDebugOverlay.m_drawableTypeColour_DT_FRAGMENT          .GetRGBA(); break;
					case fwArchetype::DT_DRAWABLE           : colour = gDebugOverlay.m_drawableTypeColour_DT_DRAWABLE          .GetRGBA(); break;
					case fwArchetype::DT_DRAWABLEDICTIONARY : colour = gDebugOverlay.m_drawableTypeColour_DT_DRAWABLEDICTIONARY.GetRGBA(); break;
					case fwArchetype::DT_ASSETLESS          : colour = gDebugOverlay.m_drawableTypeColour_DT_ASSETLESS         .GetRGBA(); break;
					default                                 : colour = Vec4V(V_ZERO_WONE); break;
					}

					bDraw = (IsGreaterThanAll(colour.GetW(), ScalarV(V_ZERO))) != 0;
				}
			}
			break;
		}
		case DOCM_DRAWABLE_HAS_SKELETON:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				const rmcDrawable* drawable = modelInfo->GetDrawable();

				if (drawable && drawable->GetSkeletonData() != NULL)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_DRAWABLE_HAS_NO_SKELETON:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				const rmcDrawable* drawable = modelInfo->GetDrawable();

				if (drawable && drawable->GetSkeletonData() == NULL)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_CONTAINS_DRAWABLE_LOD_HIGH:
		case DOCM_CONTAINS_DRAWABLE_LOD_MED:
		case DOCM_CONTAINS_DRAWABLE_LOD_LOW:
		case DOCM_CONTAINS_DRAWABLE_LOD_VLOW:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				const rmcDrawable* drawable = modelInfo->GetDrawable();

				if (drawable && drawable->GetLodGroup().ContainsLod(LOD_HIGH + (gDebugOverlay.m_colourMode - DOCM_CONTAINS_DRAWABLE_LOD_HIGH)))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_HIGH:
		case DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_MED:
		case DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_LOW:
		case DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_VLOW:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();
				const rmcDrawable* drawable = modelInfo->GetDrawable();

				if (drawable && !drawable->GetLodGroup().ContainsLod(LOD_HIGH + (gDebugOverlay.m_colourMode - DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_HIGH)))
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
#define DEF_DOCM_IS_OBJECT_(what,flag) \
		case DOCM_IS_OBJECT_##what: \
		{ \
			if (entity) \
			{ \
				if (entity->GetIsTypeObject() && static_cast<const CObject*>(entity)->m_nObjectFlags.flag) \
				{ \
					colour = c.GetRGBA(); \
					bDraw = true; \
				} \
			} \
			break; \
		}
		DEF_DOCM_IS_OBJECT_(PICKUP           ,bIsPickUp       )
		DEF_DOCM_IS_OBJECT_(DOOR             ,bIsDoor         )
		DEF_DOCM_IS_OBJECT_(UPROOTED         ,bHasBeenUprooted)
		DEF_DOCM_IS_OBJECT_(VEHICLE_PART     ,bVehiclePart    )
		DEF_DOCM_IS_OBJECT_(AMBIENT_PROP     ,bAmbientProp    )
		DEF_DOCM_IS_OBJECT_(DETACHED_PED_PROP,bDetachedPedProp)
		DEF_DOCM_IS_OBJECT_(CAN_BE_PICKED_UP ,bCanBePickedUp  )
#undef  DEF_DOCM_IS_OBJECT_

		case DOCM_IS_DONT_WRITE_DEPTH:
		{
			if (entity)
			{
				if (entity->GetBaseModelInfo()->GetDontWriteZBuffer())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_PROP:
		{
			if (entity)
			{
				if (entity->GetBaseModelInfo()->GetIsProp())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_FRAG:
		{
			if (entity)
			{
				if (entity->m_nFlags.bIsFrag)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_TREE:
		{
			if (entity)
			{
				if (entity->GetLodData().IsTree())
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_IS_TINTED:
		{
			if (entity)
			{
				if (entity->GetTintIndex() != 0)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		/*case DOCM_OBJECT_WITH_BUOYANCY:
		{
			if (entity && (entity->GetIsTypeObject() || entity->GetIsTypeDummyObject()) && entity->GetBaseModelInfo())
			{
				const CBuoyancyInfo* pBuoyancyInfo = entity->GetBaseModelInfo()->GetBuoyancyInfo();

				if (pBuoyancyInfo)
				{
					if (pBuoyancyInfo->m_bIsFloater)
					{
						colour = Vec4V(V_X_AXIS_WONE); // red
						bDraw = true;
					}
					else
					{
						colour = Vec4V(V_Z_AXIS_WONE); // blue
						bDraw = true;
					}
				}
				else
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}*/
		case DOCM_OBJECT_USES_MAX_DISTANCE_FOR_WATER_REFLECTION:
		{
			if (entity && entity->m_nFlags.bUseMaxDistanceForWaterReflection)
			{
				colour = c.GetRGBA();
				bDraw = true;
			}
		}
		case DOCM_HAS_VEHICLE_DAMAGE_TEXTURE:
		{
			if (entity && entity->GetIsTypeVehicle())
			{
				fwTexturePayload* pDamageTex = NULL;
				float damageRadius = 0.0f;
				GTA_ONLY(float damageMult = 1.0f;)
				g_pDecalCallbacks->GetDamageData(const_cast<CEntity*>(entity), &pDamageTex, damageRadius GTA_ONLY(, damageMult));

				if (pDamageTex)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_HAS_2DEFFECTS:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && modelInfo->GetNum2dEffectLights() > 0)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_HAS_2DEFFECT_LIGHTS:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo && modelInfo->GetNum2dEffectLights() > 0)
				{
					colour = c.GetRGBA();
					bDraw = true;
				}
			}
			break;
		}
		case DOCM_VERTEX_VOLUME_RATIO:
		{
			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					float vtxCount = 0.0f;
					float volume = 0.0f;

					if (entity->GetType() == ENTITY_TYPE_PED)
					{
						const CPedModelInfo* pedModelInfo = static_cast<const CPedModelInfo*>(modelInfo);
						const CPed* ped = static_cast<const CPed*>(entity);

						if (pedModelInfo->GetIsStreamedGfx() && ped->IsPlayer())
						{
							const CPedStreamRenderGfx* pGfxData = ped->GetPedDrawHandler().GetConstPedRenderGfx();

							if (pGfxData)
							{
								CPedVariationDebug::GetVtxVolumeCompPlayerInternal(pGfxData, vtxCount, volume);
							}
						} 
						else
						{
							CPedVariationDebug::GetVtxVolumeCompPedInternal(pedModelInfo, ped->GetPedDrawHandler().GetVarData(), vtxCount, volume);
						}
					}
					else if (modelInfo->GetFragType())
					{
						ColorizerUtils::FragGetVVRData(entity, modelInfo, vtxCount, volume);
					}
					else
					{
						const rmcDrawable* drawable = modelInfo->GetDrawable();

						if (drawable)
						{
							ColorizerUtils::GetVVRData(drawable, vtxCount, volume);
						}
					}

					if (volume > 0.0f)
					{
						if (gDebugOverlay.m_volumeDivideByCameraDistance)
						{
							volume *= InvMag(GTA_ONLY(RCC_VEC3V)(camInterface::GetPos()) - entity->GetTransform().GetPosition()).Getf();
						}

						const float ratio = vtxCount/volume;

						if (ratio < gDebugOverlay.m_volumeRatioMin)
						{
							colour = gDebugOverlay.m_volumeColourMinBelow.GetRGBA();
						}
						else if (ratio > gDebugOverlay.m_volumeRatioMax)
						{
							colour = gDebugOverlay.m_volumeColourMaxAbove.GetRGBA();
						}
						else
						{
							float f = Clamp<float>((ratio - gDebugOverlay.m_volumeRatioMin)/(gDebugOverlay.m_volumeRatioMax - gDebugOverlay.m_volumeRatioMin), 0.0f, 1.0f);

							if (gDebugOverlay.m_volumeRatioExpBias != 0.0f)
							{
								f = powf(f, powf(2.0f, gDebugOverlay.m_volumeRatioExpBias));
							}

							if (f < 0.5f)
							{
								const Vec4V c0 = gDebugOverlay.m_volumeColourMin.GetRGBA();
								const Vec4V c1 = gDebugOverlay.m_volumeColourMid.GetRGBA();

								colour = c0 + (c1 - c0)*ScalarV(f*2.0f);
							}
							else
							{
								const Vec4V c0 = gDebugOverlay.m_volumeColourMid.GetRGBA();
								const Vec4V c1 = gDebugOverlay.m_volumeColourMax.GetRGBA();

								colour = c0 + (c1 - c0)*ScalarV(f*2.0f - 1.0f);
							}
						}

						bDraw = true;
					}
				}
			}
			break;
		}
	//	case DOCM_SHADER_HAS_INEFFECTIVE_DIFFTEX:
		case DOCM_SHADER_HAS_INEFFECTIVE_BUMPTEX:
		case DOCM_SHADER_HAS_INEFFECTIVE_SPECTEX:
		case DOCM_SHADER_HAS_ALPHA_DIFFTEX:
		case DOCM_SHADER_HAS_ALPHA_BUMPTEX:
		case DOCM_SHADER_HAS_ALPHA_SPECTEX:
		case DOCM_SHADER_USES_DIFFTEX_NAME:
		case DOCM_SHADER_USES_BUMPTEX_NAME:
		case DOCM_SHADER_USES_SPECTEX_NAME:
		case DOCM_SHADER_FRESNEL_TOO_LOW:
		case DOCM_SKINNED_SELECT:
		{
			colour = c.GetRGBA();
			bDraw = true; // this will get filtered per-shader later 
			break;
		}
		case DOCM_SHADER_NOT_IN_MODEL_DRAWABLE___TEMP:
		{
			gCurrentRenderingEntity = CEntityIDHelper::GetEntityFromID(entityId);
			colour = c.GetRGBA();
			bDraw = true; // this will get filtered per-shader later 
			break;
		}
		case DOCM_OLD_WIRES___TEMP:
		{
			if (entity)
			{
				if (stristr(entity->GetModelName(), "wire"  ) ||
					stristr(entity->GetModelName(), "tram"  ) ||
					stristr(entity->GetModelName(), "tel_"  ) ||
					stristr(entity->GetModelName(), "elec_" ) ||
					stristr(entity->GetModelName(), "spline"))
				{
					const Drawable* drawable = entity->GetDrawable();

					if (drawable)
					{
						bool bContainsCableShader = false;
						bool bContainsWireShader  = false;

						const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

						for (int i = 0; i < shaderGroup.GetCount(); i++)
						{
							const grmShader& shader = shaderGroup.GetShader(i);

							if (stristr(shader.GetName(), "cable")) { bContainsCableShader = true; }
							if (stristr(shader.GetName(), "wire" )) { bContainsWireShader  = true; }
						}

						if (bContainsWireShader)
						{
							colour = Vec4V(V_X_AXIS_WONE);
							bDraw = true;
						}
						else if (!bContainsCableShader)
						{
							colour = c.GetRGBA();
							bDraw = true;
						}
					}
				}
			}
			break;
		}
		case DOCM_COUNT:
		{
			break;
		}
		} // end case

		grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), colour);
	}
#endif // ENTITYSELECT_ENABLED_BUILD
	return bDraw;
}

static void DebugOverlaySetLineWidth_renderthread(float lineWidth, bool PS3_ONLY(lineSmoothEnable))
{
#if __XENON
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_LINEWIDTH, *(const u32*)&lineWidth);
#elif __PS3
	GCM_STATE(SetPointSize, lineWidth);
	lineWidth *= 8.0f; // 6.3 fixed point
	lineWidth *= lineSmoothEnable ? 1.0f : 4.0f; // non-smoothed lines need to be thicker
	GCM_STATE(SetLineSmoothEnable, lineSmoothEnable ? CELL_GCM_TRUE : CELL_GCM_FALSE); // temp hack - until we're all using stateblocks, we need to set the line smooth here too
	GCM_STATE(SetLineWidth, (u32)(lineWidth));
#else
	(void)lineWidth; // not supported on pc
#endif
}

static bool g_debugOverlayLineSmooth = false;
static void DebugOverlaySetLineSmooth_renderthread(bool bLineSmooth) { g_debugOverlayLineSmooth = bLineSmooth; }

#if !__D3D11
static bool g_debugOverlayRenderPoints = false; // this gets synced on render thread to avoid flashing when cycling the mode
static void DebugOverlaySetRenderPoints_renderthread(bool bRenderPoints) { g_debugOverlayRenderPoints = bRenderPoints; }
#endif // !__D3D11

__COMMENT(static) bool CRenderPhaseDebugOverlayInterface::InternalPassBegin(bool bWireframe, bool bOverdraw, int cull, bool bDrawRope)
{
	float wireOpacity = gDebugOverlay.m_opacity;
	float fillOpacity = gDebugOverlay.m_opacity*gDebugOverlay.m_fillOpacity;

	if (gDebugOverlay.m_channel == DEBUG_OVERLAY_PED_DAMAGE)
	{
		wireOpacity = 0.0f;
		fillOpacity = 0.5f;
	}
	else if (gDebugOverlay.m_channel == DEBUG_OVERLAY_VEH_GLASS)
	{
		wireOpacity = 0.0f;
		fillOpacity = 1.0f;
	}
	else if (gDebugOverlay.m_channel != DEBUG_OVERLAY_CHANNEL_NONE &&
		gDebugOverlay.m_channel != DEBUG_OVERLAY_TERRAIN_WETNESS)
	{
		wireOpacity = 0.0f;
		fillOpacity = gDebugOverlay.m_opacity;
	}

	if (bOverdraw)
	{
		Assert(!bWireframe);

		if (gDebugOverlay.m_overdrawRenderCost)
		{
			// 360 and PC needs 1.5001 because somehow it gets rounded off to zero when finally getting rendered
			fillOpacity = PS3_SWITCH(1.0f, 1.5001f)/255.0f;
		}
		else
		{
			fillOpacity = 1.0f/(float)gDebugOverlay.m_overdrawMax;
		}
	}

	const float opacity = bWireframe ? wireOpacity : fillOpacity;

	if (opacity > 0.0f)
	{
		grcViewport* viewport = grcViewport::GetCurrent();

		grcEffect::SetGlobalVar(gDebugOverlay.m_depthProjParamsId, VECTOR4_TO_VEC4V(ShaderUtil::CalculateProjectionParams(viewport)));
		grcEffect::SetGlobalVar(gDebugOverlay.m_depthBiasParamsId, Vec4f(
			gDebugOverlay.m_depthBias_z0,
			gDebugOverlay.m_depthBias_z1,
			gDebugOverlay.m_depthBias_d0*gDebugOverlay.m_depthBias_scale,
			gDebugOverlay.m_depthBias_d1*gDebugOverlay.m_depthBias_scale
		));
#if SUPPORT_INVERTED_PROJECTION
		viewport->SetInvertZInProjectionMatrix(true);
#endif

		Vec4V colourNear;
		Vec4V colourFar;
		float visibleOpacity;
		float invisibleOpacity;

		if (bOverdraw)
		{
			colourNear       = gDebugOverlay.m_overdrawColour.GetRGBA();
			colourFar        = gDebugOverlay.m_overdrawColour.GetRGBA();
			visibleOpacity   = fillOpacity;
			invisibleOpacity = fillOpacity;

			if (gDebugOverlay.m_overdrawRenderCost)
			{
				colourNear = Vec4V(V_ONE);
				colourFar  = Vec4V(V_ONE);
			}
			else
			{
				colourNear.SetW(ScalarV(V_ONE));
				colourFar .SetW(ScalarV(V_ONE));
			}
		}
		else
		{
			colourNear       = (gDebugOverlay.m_fillColourSeparate ? gDebugOverlay.m_fillColourNear : gDebugOverlay.m_colourNear).GetRGBA();
			colourFar        = (gDebugOverlay.m_fillColourSeparate ? gDebugOverlay.m_fillColourFar  : gDebugOverlay.m_colourFar ).GetRGBA();
			visibleOpacity   = opacity*gDebugOverlay.m_visibleOpacity;
			invisibleOpacity = opacity*gDebugOverlay.m_invisibleOpacity;
		}

		if (gDebugOverlay.m_channel != DEBUG_OVERLAY_CHANNEL_NONE)
		{
			switch (gDebugOverlay.m_channel)
			{
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_XYZ : colourFar  = -Vec4V(V_TWO         ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_X   : colourNear = -Vec4V(V_X_AXIS_WZERO); colourFar  = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_Y   : colourNear = -Vec4V(V_Y_AXIS_WZERO); colourFar  = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_Z   : colourNear = -Vec4V(V_Z_AXIS_WZERO); colourFar  = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_W   : colourNear = -Vec4V(V_ZERO_WONE   ); colourFar  = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_XYZ : colourFar  = -Vec4V(V_THREE       ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_X   : colourFar  = -Vec4V(V_X_AXIS_WZERO); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_Y   : colourFar  = -Vec4V(V_Y_AXIS_WZERO); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_Z   : colourFar  = -Vec4V(V_Z_AXIS_WZERO); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_W   : colourFar  = -Vec4V(V_ZERO_WONE   ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_WORLD_NORMAL            : colourFar  = -Vec4V(V_FOUR        ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_WORLD_TANGENT           : colourFar  = -Vec4V(V_FIVE        ); colourNear = Vec4V(V_ZERO); break;
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
			case DEBUG_OVERLAY_DIFFUSE_TEXTURE         : colourFar  = -Vec4V(V_SIX         ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_BUMP_TEXTURE            : colourFar  = -Vec4V(V_SEVEN       ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_SPEC_TEXTURE            : colourFar  = -Vec4V(V_EIGHT       ); colourNear = Vec4V(V_ZERO); break;
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
			case DEBUG_OVERLAY_TERRAIN_WETNESS         : colourFar  = -Vec4V(V_NINE        ); colourNear = Vec4V(V_ZERO); break;
			case DEBUG_OVERLAY_PED_DAMAGE              : colourFar  = -Vec4V(V_TEN         ); colourNear = Vec4V((float)CPedDamageManager::GetDebugRenderMode(), 0.0f, 0.0f, 0.0f); break;
			case DEBUG_OVERLAY_VEH_GLASS			   : colourFar  = -Vec4V(V_ELEVEN      ); colourNear = Vec4V(V_ZERO); break;
			}
		}

		const Vec4V nearFarVis = Vec4V(
			gDebugOverlay.m_zNear,
			gDebugOverlay.m_colourMode ? -1.0f : gDebugOverlay.m_zFar, // set y=-1 if picker is enabled (colour will be picked up from GetEntitySelectID in the shader)
			visibleOpacity,
			invisibleOpacity
		);

		grcEffect::SetGlobalVar(gDebugOverlay.m_colourNearId, colourNear);
		grcEffect::SetGlobalVar(gDebugOverlay.m_colourFarId, colourFar);
		grcEffect::SetGlobalVar(gDebugOverlay.m_zNearFarVisId, nearFarVis);

		gDebugOverlay.m_wireframe_current = bWireframe;

		SetRasterizerCullState(cull);

		grcStateBlock::SetDepthStencilState(gDebugOverlay.m_DS);
		grcStateBlock::SetBlendState((gDebugOverlay.m_fillOnly || gDebugOverlay.m_overdrawRender) ? gDebugOverlay.m_BS_additive : gDebugOverlay.m_BS);

		if (bDrawRope)
		{
			CPhysics::GetRopeManager()->Draw(RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx()), g_LodScale.GetGlobalScaleForRenderThread() RDR_ONLY(, true), NULL);
			CPhysics::GetRopeManager()->Draw(RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx()), g_LodScale.GetGlobalScaleForRenderThread() RDR_ONLY(, false), NULL);
		}

		return true;
	}

	return false;
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::InternalPassEnd(bool bWireframe)
{
	if (bWireframe)
	{
		DebugOverlaySetLineWidth_renderthread(1.0f, false);

		grcStateBlock::SetWireframeOverride(false);
		grcStateBlock::SetStates(grcStateBlock::RS_Default, CShaderLib::DSS_Default_Invert, grcStateBlock::BS_Default);

#if SUPPORT_INVERTED_PROJECTION
		grcViewport::GetCurrent()->SetInvertZInProjectionMatrix(false);
#endif
	}
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::SetRasterizerCullState(int cull)
{
	grcRasterizerStateHandle rs = grcStateBlock::RS_Invalid;

	if (gDebugOverlay.m_wireframe_current)
	{
		const float lineWidth = gDebugOverlay.m_lineWidth*(gDebugOverlay.m_colourMode == DOCM_SHADER_EDIT ? gDebugOverlay.m_lineWidthShaderEdit : 1.0f);

		DebugOverlaySetLineWidth_renderthread(lineWidth, g_debugOverlayLineSmooth);

#if !__D3D11
		if (g_debugOverlayRenderPoints)
		{
			rs = gDebugOverlay.m_RS_point;
		}
		else
#endif // !__D3D11
		{
			grcStateBlock::SetWireframeOverride(gDebugOverlay.m_useWireframeOverride);

			switch (cull)
			{
			case grcRSV::CULL_NONE: rs = g_debugOverlayLineSmooth ? gDebugOverlay.m_RS_lineSmooth     : gDebugOverlay.m_RS_line; break;
			case grcRSV::CULL_CW  : rs = g_debugOverlayLineSmooth ? gDebugOverlay.m_RS_cw_lineSmooth  : gDebugOverlay.m_RS_cw_line; break;
			case grcRSV::CULL_CCW : rs = g_debugOverlayLineSmooth ? gDebugOverlay.m_RS_ccw_lineSmooth : gDebugOverlay.m_RS_ccw_line; break;
			}
		}
	}
	else
	{
		grcStateBlock::SetWireframeOverride(false);

		switch (cull)
		{
		case grcRSV::CULL_NONE: rs = gDebugOverlay.m_RS_fill; break;
		case grcRSV::CULL_CW  : rs = gDebugOverlay.m_RS_cw_fill; break;
		case grcRSV::CULL_CCW : rs = gDebugOverlay.m_RS_ccw_fill; break;
		}
	}

	grcStateBlock::SetRasterizerState(rs);
	gDebugOverlay.m_RS_current = rs;
}

__COMMENT(static) int CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeMode(bool bForceWireframeMode1)
{
	if (bForceWireframeMode1)
	{
		return 1;
	}
	else if (gDebugOverlay.m_draw && (gDebugOverlay.m_drawTypeFlags & BIT(DEBUG_OVERLAY_DRAW_TYPE_WATER)) != 0)
	{
		const u32 DRAW_WATER_QUADS_MODE2_MASK = (16+32+64+128); // TODO -- ugh, need some enums for this

		if (gDebugOverlay.m_drawWaterQuads & BIT(0))
		{
			return 1; // this mode forces wireframe renderstate within the water system
		}
		else if (gDebugOverlay.m_colourMode == DOCM_NONE && gDebugOverlay.m_opacity > 0.0f && (~gDebugOverlay.m_drawWaterQuads & DRAW_WATER_QUADS_MODE2_MASK) != 0)
		{
			return 2; // this mode actually renders a wireframe overlay pass over the existing water
		}
	}

	return 0;
}

__COMMENT(static) u32 CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags()
{
	return gDebugOverlay.m_drawWaterQuads;
}

#if __PS3

#if DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
static bool CustomShaderParamsFunc_dummy(const grmShaderGroup&, int, bool) // this dummy function forces grmModel to use PPU codepath
{
	return true;
}
#endif // DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ForceGrmModelPPUCodepathBegin()
{
#if DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
	grmModel::SetCustomShaderParamsFunc(CustomShaderParamsFunc_dummy);
#endif // DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ForceGrmModelPPUCodepathEnd()
{
#if DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
	grmModel::SetCustomShaderParamsFunc(NULL);
#endif // DEBUG_OVERLAY_CUSTOM_SHADER_PARAMS_FUNC
}

#endif // __PS3

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleGlobalTxdMode()
{
	if (gDebugOverlay.m_draw == false)
	{
		gDebugOverlay.m_draw        = true;
		gDebugOverlay.m_colourMode  = DOCM_SHADER_USES_TXD;
		gDebugOverlay.m_fillOpacity = 0.75f;

		strcpy(gDebugOverlay.m_filter, "gtxd");
	}
	else // default settings
	{
		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleLowPriorityStreamMode()
{
	if (gDebugOverlay.m_draw == false)
	{
		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_IS_ENTITY_LOW_PRIORITY_STREAM;
		gDebugOverlay.m_colourNear      = Color32(0,0,255,255); // blue
		gDebugOverlay.m_fillOpacity     = 0.5f;
	}
	else // default settings
	{
		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::TogglePropPriorityLevelMode()
{
	if (gDebugOverlay.m_draw == false)
	{
		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_PROP_PRIORITY_LEVEL;
		gDebugOverlay.m_fillOpacity     = 0.8f;
	}
	else // default settings
	{
		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleModelHDStateMode()
{
	if (gDebugOverlay.m_draw == false)
	{
		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_MODEL_HD_STATE;
		gDebugOverlay.m_fillOpacity     = 0.4f;
	}
	else // default settings
	{
		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::TogglePedSilhouetteMode()
{
	float& DebugLighting__m_deferredDiffuseLightAmount = DebugLighting::GetDeferredDiffuseLightAmountRef(); // make the code look like i'm not using an accessor .. heh

	if (gDebugOverlay.m_draw == false)
	{
		gDebugOverlay.m_draw                             = true;
		gDebugOverlay.m_drawSpecialMode                  = true;
		gDebugOverlay.m_colourMode                       = DOCM_ENTITY_TYPE;
		gDebugOverlay.m_entityType                       = ENTITY_TYPE_PED + 1;
		gDebugOverlay.m_entityTypeColour_ENTITY_TYPE_PED = Color32(0,0,0,255);
		gDebugOverlay.m_fillOpacity                      = 1.0f;
		DebugDeferred::m_overrideDiffuse                 = true;
		DebugDeferred::m_overrideShadow			         = true;
		DebugLighting__m_deferredDiffuseLightAmount      = 0.0f;
	}
	else // default settings
	{
		gDebugOverlay.Reset();
		DebugDeferred::m_overrideDiffuse                 = false;
		DebugDeferred::m_overrideShadow			         = false;
		DebugLighting__m_deferredDiffuseLightAmount      = 1.0f;
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::TogglePedHDMode()
{
	if (gDebugOverlay.m_draw == false)
	{
		TogglePedHDModeSetState(1);
	}
	else if (gDebugOverlay.m_fillOpacity == 0.0f)
	{
		TogglePedHDModeSetState(2);
	}
	else // default settings
	{
		TogglePedHDModeSetState(0);
	}
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::TogglePedHDModeSetState(int state)
{
	if (state == 1) // wireframe
	{
		gDebugOverlay.m_draw             = true;
		gDebugOverlay.m_drawSpecialMode  = true;
		gDebugOverlay.m_colourMode       = DOCM_IS_PED_HD;
		gDebugOverlay.m_fillOpacity      = 0.0f;
		gDebugOverlay.m_disableWireframe = false;
	}
	else if (state == 2) // solid
	{
		gDebugOverlay.m_draw             = true;
		gDebugOverlay.m_drawSpecialMode  = true;
		gDebugOverlay.m_colourMode       = DOCM_IS_PED_HD;
		gDebugOverlay.m_fillOpacity      = 0.5f;
		gDebugOverlay.m_disableWireframe = true;
	}
	else // default settings
	{
		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleVehicleGlassMode()
{
	if (gDebugOverlay.m_draw == false)
	{
		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_SHADER_NAME_IS;

		strcpy(gDebugOverlay.m_filter, "\"vehicle_vehglass");
	}
	else // default settings
	{
		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleInteriorLocationMode()
{
	static bool s_bEnabled = false;

	if (s_bEnabled == false)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_INTERIOR_LOCATION;
		gDebugOverlay.m_fillOpacity     = 0.5f;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleTerrainWetnessMode()
{
	static bool s_bEnabled = false;

	if (s_bEnabled == false)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_NONE;
		gDebugOverlay.m_channel         = DEBUG_OVERLAY_TERRAIN_WETNESS;
		gDebugOverlay.m_fillOpacity     = 0.5f;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::TogglePedDamageMode()
{
	int mode = CPedDamageManager::GetDebugRenderMode();
	mode++;

	if (mode > 4)
	{
		mode = 0;
	}
	
	CPedDamageManager::SetDebugRenderMode(mode);

	TogglePedDamageModeSetEnabled(mode != 0);

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::TogglePedDamageModeSetEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_colourMode      = DOCM_NONE;
		gDebugOverlay.m_channel         = DEBUG_OVERLAY_PED_DAMAGE;
		gDebugOverlay.m_fillOpacity     = 0.5f;
		gDebugOverlay.m_depthBias_z1    = 512.0f;
		gDebugOverlay.m_depthBias_d1    = 0.2f;
	}
	else
	{
		gDebugOverlay.Reset();
	}
}


__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleVehicleGlassDebugMode()
{
	static bool s_bEnabled = false;

	if (!s_bEnabled)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw            = true;
		gDebugOverlay.m_drawSpecialMode = true;
		gDebugOverlay.m_renderOpaque	= false;
		gDebugOverlay.m_renderAlpha		= true;
		gDebugOverlay.m_renderFading	= false;
		gDebugOverlay.m_renderPlants	= false;
		gDebugOverlay.m_renderTrees		= false;
		gDebugOverlay.m_renderDecalsCutouts = false;
		gDebugOverlay.m_colourMode      = DOCM_NONE;
		gDebugOverlay.m_channel         = DEBUG_OVERLAY_VEH_GLASS;
		gDebugOverlay.m_fillOpacity     = 1.0f;
		gDebugOverlay.m_visibleOpacity	= 1.0f;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();
	}

}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleShadowProxyMode()
{
	static bool s_bEnabled = false;

	if (s_bEnabled == false)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw             = true;
		gDebugOverlay.m_drawSpecialMode  = true;
		gDebugOverlay.m_colourMode       = DOCM_IS_ENTITY_ONLY_RENDER_IN_SHADOWS;
		gDebugOverlay.m_fillOpacity      = 0.1f;
		gDebugOverlay.m_invisibleOpacity = 1.0f;

		g_scanDebugFlagsPortals |= SCAN_DEBUG_FORCE_SHADOW_PROXIES_IN_PRIMARY;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();

		g_scanDebugFlagsPortals &= ~SCAN_DEBUG_FORCE_SHADOW_PROXIES_IN_PRIMARY;
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleNoShadowCastingMode()
{
	static bool s_bEnabled = false;

	if (s_bEnabled == false)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw             = true;
		gDebugOverlay.m_drawSpecialMode  = true;
		gDebugOverlay.m_colourMode       = DOCM_IS_ENTITY_DONT_RENDER_IN_SHADOWS;
		gDebugOverlay.m_fillOpacity      = 0.1f;
		gDebugOverlay.m_invisibleOpacity = 1.0f;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleReflectionProxyMode()
{
	static bool s_bEnabled = false;

	if (s_bEnabled == false)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw             = true;
		gDebugOverlay.m_drawSpecialMode  = true;
		gDebugOverlay.m_colourMode       = DOCM_IS_ENTITY_ONLY_RENDER_IN_REFLECTIONS;
		gDebugOverlay.m_fillOpacity      = 0.1f;
		gDebugOverlay.m_invisibleOpacity = 1.0f;

		g_scanDebugFlagsPortals |= SCAN_DEBUG_FORCE_REFLECTION_PROXIES_IN_PRIMARY;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();

		g_scanDebugFlagsPortals &= ~SCAN_DEBUG_FORCE_REFLECTION_PROXIES_IN_PRIMARY;
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::ToggleNoReflectionsMode()
{
	static bool s_bEnabled = false;

	if (s_bEnabled == false)
	{
		s_bEnabled = true;

		gDebugOverlay.m_draw             = true;
		gDebugOverlay.m_drawSpecialMode  = true;
		gDebugOverlay.m_colourMode       = DOCM_IS_ENTITY_DONT_RENDER_IN_ALL_REFLECTIONS;
		gDebugOverlay.m_fillOpacity      = 0.1f;
		gDebugOverlay.m_invisibleOpacity = 1.0f;
	}
	else
	{
		s_bEnabled = false;

		gDebugOverlay.Reset();
	}

	gDebugOverlay.ColourModeOrDrawTypeFlags_changed();
}

// ================================================================================================

CRenderPhaseDebugOverlay::CRenderPhaseDebugOverlay(CViewport* pGameViewport) : CRenderPhaseScanned(pGameViewport, "Debug Overlay", DL_RENDERPHASE_DEBUG_OVERLAY, 0.0f)
{
	//m_RenderFlags = 0;
}

static bool DebugOverlayBindCustomShaderParams_ShaderNameIs(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	return StringMatch(group.GetShader(shaderIndex).GetName(), gDebugOverlay.m_filter);
}

static bool DebugOverlayBindCustomShaderParams_ShaderUsesTexture(const grmShaderGroup& group, int shaderIndex, bool)
{
	const grmShader* shader = group.GetShaderPtr(shaderIndex);

	for (int j = 0; j < shader->GetVarCount(); j++)
	{
		const grcEffectVar var = shader->GetVarByIndex(j);
		const char* name = NULL;
		grcEffect::VarType type;
		int annotationCount = 0;
		bool isGlobal = false;

		shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

		if (type == grcEffect::VT_TEXTURE)
		{
			grcTexture* texture = NULL;
			shader->GetVar(var, texture);

			if (texture && StringMatch(GetFriendlyTextureName(texture).c_str(), gDebugOverlay.m_filter))
			{
				return true;
			}
		}
	}

	return false;
}

static bool DebugOverlayBindCustomShaderParams_ShaderUsesTxd(const grmShaderGroup& group, int shaderIndex, bool)
{
	if (s_currentEntity)
	{
		const grmShader* shader = group.GetShaderPtr(shaderIndex);
		int usesThisTxd = 0;
		int usesOtherTxd = 0;

		for (int j = 0; j < shader->GetVarCount(); j++)
		{
			const grcEffectVar var = shader->GetVarByIndex(j);
			const char* name = NULL;
			grcEffect::VarType type;
			int annotationCount = 0;
			bool isGlobal = false;

			shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

			if (type == grcEffect::VT_TEXTURE)
			{
				grcTexture* texture = NULL;
				shader->GetVar(var, texture);

				if (texture)
				{
					if (StringMatch(AssetAnalysis::GetTexturePath(s_currentEntity->GetBaseModelInfo(), texture).c_str(), gDebugOverlay.m_filter))
					{
						usesThisTxd++;
					}
					else
					{
						usesOtherTxd++;
					}
				}
			}
		}

		if (!usesOtherTxd && usesThisTxd)
		{
			grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), Vec4V(V_Y_AXIS_WONE));
			return true;
		}
		else if (usesOtherTxd && !usesThisTxd)
		{
			grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), Vec4V(V_X_AXIS_WONE));
			return true;
		}
		else if (usesOtherTxd && usesThisTxd)
		{
			grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), Vec4V(1.0f, 1.0f, 0.0f, 1.0f));
			return true;
		}
	}

	return false;
}

static bool DebugOverlayBindCustomShaderParams_RandomColourPerShader(const grmShaderGroup& ENTITYSELECT_ONLY(group), int ENTITYSELECT_ONLY(shaderIndex), bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	bool bDraw = true;

#if ENTITYSELECT_ENABLED_BUILD
	const grmShader* shader = group.GetShaderPtr(shaderIndex);

	Color32 c = Color32(ScrambleBits32(ScrambleBits32((u32)(intptr_t)shader) ^ s_currentEntityId));
	c.SetAlpha(255);

	grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), c.GetRGBA());
#endif // ENTITYSELECT_ENABLED_BUILD

	return bDraw;
}

static bool DebugOverlayBindCustomShaderParams_RandomColourPerDrawCall(const grmShaderGroup& ENTITYSELECT_ONLY(group), int ENTITYSELECT_ONLY(shaderIndex), bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	bool bDraw = true;

#if ENTITYSELECT_ENABLED_BUILD
	const grmShader* shader = group.GetShaderPtr(shaderIndex);

	Color32 c = Color32(ScrambleBits32(ScrambleBits32((u32)(intptr_t)shader) ^ s_currentEntityId));
	c.SetAlpha(255);

	// rescramble each draw call
	s_currentEntityId = XorShiftLRL<XORSHIFT_LRL_DEFAULT>(s_currentEntityId);

	grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), c.GetRGBA());
#endif // ENTITYSELECT_ENABLED_BUILD

	return bDraw;
}

static bool DebugOverlayBindCustomShaderParams_EntitySelect(const grmShaderGroup& ENTITYSELECT_ONLY(group), int ENTITYSELECT_ONLY(shaderIndex), bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	// TODO -- this doesn't work for peds or vehicles yet!
	if (gCurrentRenderingEntity && (static_cast<CEntity*>(gCurrentRenderingEntity)->GetIsTypePed() || static_cast<CEntity*>(gCurrentRenderingEntity)->GetIsTypeVehicle()))
	{
		return false;
	}

#if ENTITYSELECT_ENABLED_BUILD
	const grmShader* selectedShader = CEntitySelect::GetSelectedEntityShader(); // do we need to buffer this?

	if (selectedShader)
	{
		return selectedShader == group.GetShaderPtr(shaderIndex);
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	return true; // no selected shader, draw the whole entity
}

static bool DebugOverlayBindCustomShaderParams_ShaderEdit(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	const grmShader* shader = group.GetShaderPtr(shaderIndex);
	return shader->IsDebugWireframeOverride();
}

static bool DebugOverlayBindCustomShaderParams_TextureViewer(const grmShaderGroup& ENTITYSELECT_ONLY(group), int ENTITYSELECT_ONLY(shaderIndex), bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	bool bDraw = true;

#if ENTITYSELECT_ENABLED_BUILD
	const u32   selectedShaderPtr  = CDebugTextureViewerInterface::GetSelectedShaderPtr_renderthread();
	const char* selectedShaderName = CDebugTextureViewerInterface::GetSelectedShaderName_renderthread();

	if (selectedShaderName[0] != '\0')
	{
		Color32 c = Color32(0);

		bDraw = false;

		switch ((int)gDebugOverlay.m_pass)
		{
		case DEBUG_OVERLAY_RENDER_PASS_FILL: c = gDebugOverlay.m_fillColourSeparate ? gDebugOverlay.m_fillColourNear : gDebugOverlay.m_colourNear; break;
		case DEBUG_OVERLAY_RENDER_PASS_WIREFRAME: c = gDebugOverlay.m_colourNear; break;
		}

		const grmShader* shader = group.GetShaderPtr(shaderIndex);
		Vec4V colour = Vec4V(V_ZERO);

		if (selectedShaderPtr != 0)
		{
			if (selectedShaderPtr == (u32)(intptr_t)shader)
			{
				colour = c.GetRGBA();
				bDraw  = true;
			}
		}
		else
		{
			if (selectedShaderName[0] == '\0' || strcmp(shader->GetName(), selectedShaderName) == 0)
			{
				colour = c.GetRGBA();
				bDraw  = true;
			}
		}

		grcEffect::SetGlobalVar(CEntitySelect::GetEntityIDShaderVarID(), colour);
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	return bDraw;
}

// copied from streaming iterators ..
static bool __IsDiffuseTex(const char* name)
{
	if (stristr(name, "DiffuseTex") ||
		stristr(name, "DirtTex") ||
		stristr(name, "SnowTex") ||
		stristr(name, "WrinkleMaskTex") ||
		stristr(name, "PlateBgTex") ||
		stristr(name, "StubbleTex"))
	{
		return true;
	}

	return false;
}

// copied from streaming iterators ..
static bool __IsBumpTex(const char* name)
{
	if (stristr(name, "BumpTex") ||
		stristr(name, "DetailTex") ||
		stristr(name, "NormalTex") ||
		stristr(name, "WrinkleTex") ||
		stristr(name, "MirrorCrackTex"))
	{
		return true;
	}

	return false;
}

// copied from streaming iterators ..
static bool __IsSpecularTex(const char* name)
{
	if (stristr(name, "SpecularTex"))
	{
		return true;
	}

	return false;
}

#if EFFECT_PRESERVE_STRINGS
template <int mode> static bool DebugOverlayBindCustomShaderParams_ShaderHasIneffectiveTextures(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	const grmShader* shader = group.GetShaderPtr(shaderIndex);
	bool bDraw = false;

	const float threshold = 0.02f;

	float bumpiness = 1.0f;
	float specIntensity = 1.0f;
	Vec3V specIntensityMask = Vec3V(V_X_AXIS_WZERO);

	bool bHasDiffTex = false;
	bool bHasBumpTex = false;
	bool bHasSpecTex = false;

	for (int j = 0; j < shader->GetVarCount(); j++)
	{
		const grcEffectVar var = shader->GetVarByIndex(j);
		const char* name = NULL;
		grcEffect::VarType type;
		int annotationCount = 0;
		bool isGlobal = false;

		shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

		if (!isGlobal)
		{
			if (mode == 0)
			{
				if (type == grcEffect::VT_TEXTURE && __IsDiffuseTex(name))
				{
					grcTexture* texture;
					shader->GetVar(var, texture);

					if (texture && Max<int>(texture->GetWidth(), texture->GetHeight()) > 4)
					{
						bHasDiffTex = true;
					}
				}
			}
			else if (mode == 1)
			{
				if (type == grcEffect::VT_TEXTURE && __IsBumpTex(name))
				{
					grcTexture* texture;
					shader->GetVar(var, texture);

					if (texture && Max<int>(texture->GetWidth(), texture->GetHeight()) > 4)
					{
						bHasBumpTex = true;
					}
				}
				else if (type == grcEffect::VT_FLOAT && stricmp(name, "Bumpiness") == 0)
				{
					shader->GetVar(var, bumpiness);
				}
			}
			else if (mode == 2)
			{
				if (type == grcEffect::VT_TEXTURE && __IsSpecularTex(name))
				{
					grcTexture* texture;
					shader->GetVar(var, texture);

					if (texture && Max<int>(texture->GetWidth(), texture->GetHeight()) > 4)
					{
						bHasSpecTex = true;
					}
				}
				else if (type == grcEffect::VT_FLOAT && stricmp(name, "SpecularColor") == 0)
				{
					shader->GetVar(var, specIntensity);
				}
				else if (type == grcEffect::VT_VECTOR3 && stricmp(name, "SpecularMapIntensityMask") == 0)
				{
					shader->GetVar(var, RC_VECTOR3(specIntensityMask));
				}
			}
		}
	}

	if ((mode == 1 && bHasBumpTex && bumpiness <= threshold) ||
		(mode == 2 && bHasSpecTex && specIntensity*MaxElement(specIntensityMask).Getf() <= threshold))
	{
		bDraw = true;
	}

	return bDraw;
}

template <int mode> static bool DebugOverlayBindCustomShaderParams_ShaderHasAlphaTextures(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	if (mode == 2 && gCurrentRenderingEntity && (static_cast<CEntity*>(gCurrentRenderingEntity)->GetIsTypePed()))
	{
		return false; // peds can have alpha in their specular textures
	}

	const grmShader* shader = group.GetShaderPtr(shaderIndex);
	bool bDraw = false;

	for (int j = 0; j < shader->GetVarCount(); j++)
	{
		const grcEffectVar var = shader->GetVarByIndex(j);
		const char* name = NULL;
		grcEffect::VarType type;
		int annotationCount = 0;
		bool isGlobal = false;

		shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

		if (!isGlobal)
		{
			if ((mode == 0 && type == grcEffect::VT_TEXTURE && __IsDiffuseTex (name)) ||
				(mode == 1 && type == grcEffect::VT_TEXTURE && __IsBumpTex    (name)) ||
				(mode == 2 && type == grcEffect::VT_TEXTURE && __IsSpecularTex(name)))
			{
				grcTexture* texture = NULL;
				shader->GetVar(var, texture);

				if (texture)
				{
					if (texture->GetImageFormat() == grcImage::DXT3 ||
						texture->GetImageFormat() == grcImage::DXT5)
					{
						grcTexture::eTextureSwizzle swizzle[4];
						texture->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

						if (swizzle[0] != grcTexture::TEXTURE_SWIZZLE_A && // maybe it's a "high quality" texture
							swizzle[1] != grcTexture::TEXTURE_SWIZZLE_A &&
							swizzle[2] != grcTexture::TEXTURE_SWIZZLE_A)
						{
							if (mode != 0 || stristr(shader->GetName(), "emissive") == NULL) // it's ok for emissive shaders to have diffuse alpha
							{
								bDraw = true;
								break;
							}
						}
					}
				}
			}
		}
	}

	return bDraw;
}

template <int mode> static bool DebugOverlayBindCustomShaderParams_ShaderUsesTextureName(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	const grmShader* shader = group.GetShaderPtr(shaderIndex);
	bool bDraw = false;

	for (int j = 0; j < shader->GetVarCount(); j++)
	{
		const grcEffectVar var = shader->GetVarByIndex(j);
		const char* name = NULL;
		grcEffect::VarType type;
		int annotationCount = 0;
		bool isGlobal = false;

		shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

		if (!isGlobal)
		{
			if ((mode == 0 && type == grcEffect::VT_TEXTURE && __IsDiffuseTex (name)) ||
				(mode == 1 && type == grcEffect::VT_TEXTURE && __IsBumpTex    (name)) ||
				(mode == 2 && type == grcEffect::VT_TEXTURE && __IsSpecularTex(name)))
			{
				grcTexture* texture = NULL;
				shader->GetVar(var, texture);

				if (texture)
				{
					extern atString GetFriendlyTextureName(const grcTexture* texture);

					if (stricmp(GetFriendlyTextureName(texture).c_str(), gDebugOverlay.m_filter) == 0)
					{
						bDraw = true;
						break;
					}
				}
			}
		}
	}

	return bDraw;
}
#endif

static bool DebugOverlayBindCustomShaderParams_ShaderFresnelTooLow(const grmShaderGroup& group, int shaderIndex, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	const grmShader* shader = group.GetShaderPtr(shaderIndex);
	grcEffectVar idVarFresnel = shader->LookupVar("Fresnel", false);

	if (idVarFresnel)
	{
		float fresnel = 0.0f;
		shader->GetVar(idVarFresnel, fresnel);

		if (fresnel < gDebugOverlay.m_filterValue)
		{
			return true;
		}
	}

	return false;
}

static bool DebugOverlayBindCustomShaderParams_SkinnedSelect(const grmShaderGroup&, int, bool bDrawSkinned)
{
	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	return true;
}

static bool DebugOverlayBindCustomShaderParams_ShaderNotInModelDrawable___TEMP(const grmShaderGroup& group, int, bool bDrawSkinned)
{
	Assert(gDebugOverlay.m_pass != DEBUG_OVERLAY_RENDER_PASS_NONE);

	if (( bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_NOT_SKINNED) ||
		(!bDrawSkinned && gDebugOverlay.m_skinnedSelect == DEBUG_OVERLAY_SKINNED_SELECT_SKINNED))
	{
		return false;
	}

	if (gCurrentRenderingEntity)
	{
		const u32 modelIndex = gCurrentRenderingEntity->GetModelIndex();

		if (CModelInfo::IsValidModelInfo(modelIndex))
		{
			const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(GTA_ONLY(strLocalIndex)(modelIndex)));

			if (modelInfo)
			{
				const Drawable* drawable = modelInfo->GetDrawable();

				if (drawable)
				{
					if (&group == &drawable->GetShaderGroup())
					{
						return false; // don't draw
					}
				}
			}
		}
	}

	return true;
}

__COMMENT(virtual) void CRenderPhaseDebugOverlay::UpdateViewport()
{
	SetActive(true);
	CRenderPhaseScanned::UpdateViewport();
}

#if __XENON
static DWORD HiZEnabled;
static DWORD HiZFunc;
static DWORD HiZWriteEnabled;

static void SetupHiZ()
{
	GRCDEVICE.GetCurrent()->GetRenderState(D3DRS_HIZENABLE, &HiZEnabled);
	GRCDEVICE.GetCurrent()->GetRenderState(D3DRS_ZFUNC, &HiZFunc);
	GRCDEVICE.GetCurrent()->GetRenderState(D3DRS_HIZWRITEENABLE, &HiZWriteEnabled);

	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_ENABLE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC);

	GRCDEVICE.GetCurrent()->FlushHiZStencil(D3DFHZS_SYNCHRONOUS);
}

static void RestoreHiZ()
{
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE, HiZEnabled);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_ZFUNC, HiZFunc);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, HiZWriteEnabled);
}
#endif // __XENON

static void RenderStateSetup(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int shadowPass, fwRenderGeneralFlags generalFlags)
{
	if (gDebugOverlay.m_forceStates)
	{
		class SetStates { public: static void func()
		{
			// TODO -- this won't work codepath requiring SetBucketsAndRenderModeForOverrideGBuf .. need to move that functionality into this function here
			grcStateBlock::SetStates(gDebugOverlay.m_RS_current, gDebugOverlay.m_DS, (gDebugOverlay.m_fillOnly || gDebugOverlay.m_overdrawRender) ? gDebugOverlay.m_BS_additive : gDebugOverlay.m_BS);
		}};
		DLC_Add(SetStates::func);
		DLC(dlCmdGrcLightStateSetEnabled, (false));
		DLC(dlCmdSetBucketsAndRenderMode, (bucket, rmStandard));
	}
	else
	{
		CRenderListBuilder::RenderStateSetupNormal(id, bucket, renderMode, shadowPass, generalFlags);
	}

	/*if (gDebugOverlay.m_forceVars)
	{
		class SetVars { public: static void func()
		{
			const grcViewport* viewport = grcViewport::GetCurrent();

			grcEffect::SetGlobalVar(gDebugOverlay.m_depthProjParamsId, VECTOR4_TO_VEC4V(ShaderUtil::CalculateProjectionParams(viewport)));
			grcEffect::SetGlobalVar(gDebugOverlay.m_depthBiasParamsId, Vec4f(
				gDebugOverlay.m_depthBias_z0,
				gDebugOverlay.m_depthBias_z1,
				gDebugOverlay.m_depthBias_d0*gDebugOverlay.m_depthBias_scale,
				gDebugOverlay.m_depthBias_d1*gDebugOverlay.m_depthBias_scale
				));
			grcEffect::SetGlobalVar(gDebugOverlay.m_colourNearId, gDebugOverlay.m_colourNear_current);
			grcEffect::SetGlobalVar(gDebugOverlay.m_colourFarId, gDebugOverlay.m_colourFar_current);
			grcEffect::SetGlobalVar(gDebugOverlay.m_zNearFarVisId, gDebugOverlay.m_nearFarVis_current);
		}};
		DLC_Add(SetVars::func);
	}*/
}

__COMMENT(virtual) void CRenderPhaseDebugOverlay::BuildDrawList()
{
	if (!HasEntityListIndex())
	{
		return;
	}

	DrawListPrologue();

	if ((gDebugOverlay.m_draw && gDebugOverlay.m_opacity > 0.0f) || gDebugOverlay.m_overdrawRender || CRenderPhaseCascadeShadowsInterface::IsDebugDrawEnabled())
	{
		grcViewport viewport = GetViewport()->GetGrcViewport();
#if __DEV
		bool bUseLODControls = false;

		if (gDebugOverlay.m_HighestDrawableLOD_debug != dlDrawListMgr::DLOD_DEFAULT ||
			gDebugOverlay.m_HighestFragmentLOD_debug != dlDrawListMgr::DLOD_DEFAULT ||
			gDebugOverlay.m_HighestVehicleLOD_debug  != dlDrawListMgr::VLOD_DEFAULT ||
			gDebugOverlay.m_HighestPedLOD_debug      != dlDrawListMgr::PLOD_DEFAULT)
		{
			dlDrawListMgr::SetHighestLOD(
				false,
				(dlDrawListMgr::eHighestDrawableLOD)gDebugOverlay.m_HighestDrawableLOD_debug,
				(dlDrawListMgr::eHighestDrawableLOD)gDebugOverlay.m_HighestFragmentLOD_debug,
				(dlDrawListMgr::eHighestVehicleLOD)gDebugOverlay.m_HighestVehicleLOD_debug,
				(dlDrawListMgr::eHighestPedLOD)gDebugOverlay.m_HighestPedLOD_debug
			);
			bUseLODControls = true;
		}
#endif // __DEV

		viewport.SetWorldIdentity();

		PF_PUSH_TIMEBAR_DETAIL("Render Debug Overlay");
		PF_START_TIMEBAR_DETAIL("Render Entities");

		const int fullFilter =
		(
			(gDebugOverlay.m_renderOpaque ? CRenderListBuilder::RENDER_OPAQUE : 0) |
			(gDebugOverlay.m_renderAlpha  ? CRenderListBuilder::RENDER_ALPHA | CRenderListBuilder::RENDER_DISPL_ALPHA : 0) |
			(gDebugOverlay.m_renderFading ? CRenderListBuilder::RENDER_FADING : 0) |
			CRenderListBuilder::RENDER_WATER |
			(gDebugOverlay.m_renderPlants        ? 0 : CRenderListBuilder::RENDER_DONT_RENDER_PLANTS        ) |
			(gDebugOverlay.m_renderTrees         ? 0 : CRenderListBuilder::RENDER_DONT_RENDER_TREES         ) |
			(gDebugOverlay.m_renderDecalsCutouts ? 0 : CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS)
		);

		DLC(dlCmdSetCurrentViewport, (&viewport));
		DLC(dlCmdShaderFxPushForcedTechnique, (gDebugOverlay.m_techniqueGroupId)); // required if we're not calling CRenderListBuilder::AddToDrawListShadowEntities .. do we even need the edge technique id?
		EDGE_ONLY(DLC_Add(grcEffect::SetEdgeViewportCullEnable, gDebugOverlay.m_edgeViewportCull));

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES && GTA_VERSION
		CRenderer::SetTessellationVars(viewport, (float)VideoResManager::GetSceneWidth()/(float)VideoResManager::GetSceneHeight());
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES

		// NOTE -- i've tried the following, they crash the gpu on xb1/ps4 when run with multisampling
		// CRenderTargets::GetDepthBuffer (same as CRenderTargets::GetUIDepthBuffer or GBuffer::GetDepthTarget)
		// CRenderTargets::GetDepthBuffer_ReadOnly
		// CRenderTargets::GetDepthBufferCopy
		CShaderLib::SetGlobalVarInContext(gDebugOverlay.m_depthTextureVarId, gDebugOverlay.m_skipDepthBind ? NULL : CRenderTargets::GetDepthResolved());

#if DEBUG_OVERLAY_CLIP_TEST
		if (gDebugOverlay.m_clipXEnable || gDebugOverlay.m_clipYEnable)
		{
			const Mat34V viewToWorld = viewport.GetCameraMtx();
			const ScalarV zNearClip = ScalarV(viewport.GetNearClip()*1.1f); // slightly in front of near clip
			u32 clipEnable = 0;

			Vec4V clipPlanes[2] = {Vec4V(V_ZERO), Vec4V(V_ZERO)};

			if (gDebugOverlay.m_clipXEnable)
			{
				const ScalarV clipX = ScalarV(gDebugOverlay.m_clipX*viewport.GetTanHFOV());

				grcDebugDraw::Line(
					Transform(viewToWorld, Vec3V(clipX, -ScalarV(V_TEN), ScalarV(V_NEGONE))*zNearClip),
					Transform(viewToWorld, Vec3V(clipX, +ScalarV(V_TEN), ScalarV(V_NEGONE))*zNearClip),
					Color32(255, 0, 0, 255)
				);

				const Vec3V n = Normalize(Vec3V(Vec2V(V_X_AXIS_WZERO), clipX));
				clipPlanes[0] = BuildPlane(viewToWorld.GetCol3(), Transform3x3(viewToWorld, n));
				clipEnable |= (1<<0);
			}

			if (gDebugOverlay.m_clipYEnable)
			{
				const ScalarV clipY = ScalarV(gDebugOverlay.m_clipY*viewport.GetTanVFOV());

				grcDebugDraw::Line(
					Transform(viewToWorld, Vec3V(-ScalarV(V_TEN), clipY, ScalarV(V_NEGONE))*zNearClip),
					Transform(viewToWorld, Vec3V(+ScalarV(V_TEN), clipY, ScalarV(V_NEGONE))*zNearClip),
					Color32(255, 0, 0, 255)
				);

				const Vec3V n = Normalize(Vec3V(Vec2V(V_Y_AXIS_WZERO), clipY));
				clipPlanes[1] = BuildPlane(viewToWorld.GetCol3(), Transform3x3(viewToWorld, n));
				clipEnable |= (1<<1);
			}

			DLC(dlCmdSetClipPlane, (0, clipPlanes[0]));
			DLC(dlCmdSetClipPlane, (1, clipPlanes[1]));
			DLC(dlCmdSetClipPlane, (2, Vec4V(V_ZERO)));
			DLC(dlCmdSetClipPlane, (3, Vec4V(V_ZERO)));
			DLC(dlCmdSetClipPlaneEnable, (clipEnable));
		}
#endif // DEBUG_OVERLAY_CLIP_TEST

		grmModel__CustomShaderParamsFuncType func = NULL;

		switch (gDebugOverlay.m_colourMode)
		{
		case DOCM_SHADER_NAME_IS                      : func = DebugOverlayBindCustomShaderParams_ShaderNameIs; break;
		case DOCM_SHADER_USES_TEXTURE                 : func = DebugOverlayBindCustomShaderParams_ShaderUsesTexture; break;
		case DOCM_SHADER_USES_TXD                     : func = DebugOverlayBindCustomShaderParams_ShaderUsesTxd; break;
		case DOCM_RANDOM_COLOUR_PER_SHADER            : func = DebugOverlayBindCustomShaderParams_RandomColourPerShader; break;
		case DOCM_RANDOM_COLOUR_PER_DRAW_CALL         : func = DebugOverlayBindCustomShaderParams_RandomColourPerDrawCall; break;
		case DOCM_ENTITYSELECT                        : func = DebugOverlayBindCustomShaderParams_EntitySelect; break;
		case DOCM_SHADER_EDIT                         : func = DebugOverlayBindCustomShaderParams_ShaderEdit; break;
		case DOCM_TEXTURE_VIEWER                      : func = DebugOverlayBindCustomShaderParams_TextureViewer; break;
	//	case DOCM_SHADER_HAS_INEFFECTIVE_DIFFTEX      : func = DebugOverlayBindCustomShaderParams_ShaderHasIneffectiveTextures<0>; break;
#if EFFECT_PRESERVE_STRINGS
		case DOCM_SHADER_HAS_INEFFECTIVE_BUMPTEX      : func = DebugOverlayBindCustomShaderParams_ShaderHasIneffectiveTextures<1>; break;
		case DOCM_SHADER_HAS_INEFFECTIVE_SPECTEX      : func = DebugOverlayBindCustomShaderParams_ShaderHasIneffectiveTextures<2>; break;
		case DOCM_SHADER_HAS_ALPHA_DIFFTEX            : func = DebugOverlayBindCustomShaderParams_ShaderHasAlphaTextures<0>; break;
		case DOCM_SHADER_HAS_ALPHA_BUMPTEX            : func = DebugOverlayBindCustomShaderParams_ShaderHasAlphaTextures<1>; break;
		case DOCM_SHADER_HAS_ALPHA_SPECTEX            : func = DebugOverlayBindCustomShaderParams_ShaderHasAlphaTextures<2>; break;
		case DOCM_SHADER_USES_DIFFTEX_NAME            : func = DebugOverlayBindCustomShaderParams_ShaderUsesTextureName<0>; break;
		case DOCM_SHADER_USES_BUMPTEX_NAME            : func = DebugOverlayBindCustomShaderParams_ShaderUsesTextureName<1>; break;
		case DOCM_SHADER_USES_SPECTEX_NAME            : func = DebugOverlayBindCustomShaderParams_ShaderUsesTextureName<2>; break;
#endif
		case DOCM_SHADER_FRESNEL_TOO_LOW              : func = DebugOverlayBindCustomShaderParams_ShaderFresnelTooLow; break;
		case DOCM_SKINNED_SELECT                      : func = DebugOverlayBindCustomShaderParams_SkinnedSelect; break;
		case DOCM_SHADER_NOT_IN_MODEL_DRAWABLE___TEMP : func = DebugOverlayBindCustomShaderParams_ShaderNotInModelDrawable___TEMP; break;
		default:
			if (gDebugOverlay.m_skinnedSelect != DEBUG_OVERLAY_SKINNED_SELECT_ALL)
			{
				// this should let us use the skinned select feature in combination with any other mode
				func = DebugOverlayBindCustomShaderParams_SkinnedSelect;
			}
		}

		float wireOpacity = gDebugOverlay.m_opacity;
		float fillOpacity = gDebugOverlay.m_opacity*gDebugOverlay.m_fillOpacity;
		int cull = (int)grcRSV::CULL_NONE;

		if (gDebugOverlay.m_channel == DEBUG_OVERLAY_PED_DAMAGE)
		{
			wireOpacity = 0.0f;
			fillOpacity = 0.5f;
		}
		else
		if (gDebugOverlay.m_channel != DEBUG_OVERLAY_CHANNEL_NONE &&
			gDebugOverlay.m_channel != DEBUG_OVERLAY_TERRAIN_WETNESS)
		{
			wireOpacity = 0.0f;
			fillOpacity = gDebugOverlay.m_opacity;
			cull = (int)grcRSV::CULL_CW;
		}

		if (gDebugOverlay.m_disableWireframe || !gDebugOverlay.m_draw) // hacked to disable wireframe when using the renderphase for cascade shadow debug draw .. nasty
		{
			wireOpacity = 0.0f;
		}

		switch (gDebugOverlay.m_culling)
		{
		case 1: cull = (int)grcRSV::CULL_NONE; break;
		case 2: cull = (int)grcRSV::CULL_CW; break;
		case 3: cull = (int)grcRSV::CULL_CCW; break;
		}

		grcRenderTarget* overdrawTarget = NULL;

		DLC_Add(DebugOverlaySetLineSmooth_renderthread, gDebugOverlay.m_lineSmooth);
#if !__D3D11
		DLC_Add(DebugOverlaySetRenderPoints_renderthread, gDebugOverlay.m_renderPoints);
#endif // !__D3D11

		if (gDebugOverlay.m_fillOnly || gDebugOverlay.m_overdrawRender)
		{
			if (gDebugOverlay.m_overdrawRender &&
				gDebugOverlay.m_overdrawRenderCost)
			{
				overdrawTarget = CRenderTargets::GetHudBuffer();
				DLC(dlCmdLockRenderTarget, (0, overdrawTarget, NULL));
			}

			DLC(dlCmdClearRenderTarget, (true, Color32(0x00000000), false, 0.0f, false, 0));

#if !__D3D11
			if ((gDebugOverlay.m_fillOnly && !gDebugOverlay.m_renderPoints) || gDebugOverlay.m_overdrawRender)
			{
				wireOpacity = 0.0f;
			}
#endif // !__D3D11
		}

		if (overdrawTarget == NULL)
		{
#if __D3D11 && GTA_VERSION // TODO -- gta only?
			DLC_Add(CRenderTargets::LockUIRenderTargetsReadOnlyDepth);
#elif RDR_VERSION
			DLC_Add(CRenderTargets::LockUIRenderTargetsNoDepth);
#else
			DLC_Add(CRenderTargets::LockUIRenderTargets);
#endif
		}

		if (fillOpacity > 0.0f)
		{
			DLC_Add(CDebugOverlay::SetCurrentPass, DEBUG_OVERLAY_RENDER_PASS_FILL, func);

			if (!gDebugOverlay.m_skipAddToDrawList)
			{
				DLC_Add(CRenderPhaseDebugOverlayInterface::InternalPassBegin, false, false, cull, true);
				CRenderListBuilder::AddToDrawList(GetEntityListIndex(), fullFilter, CRenderListBuilder::USE_DEBUGMODE, SUBPHASE_NONE, RenderStateSetup, CRenderListBuilder::RenderStateCleanupNormal);
				DLC_Add(CRenderPhaseDebugOverlayInterface::InternalPassEnd, false);
			}
#if __DEV
			if (gDebugOverlay.m_drawWaterQuads)
			{
				const int waterOpacity = (int)(fillOpacity*255.0f + 0.5f);

				Water::RenderDebug(
					Color32(100,100,255, (gDebugOverlay.m_drawWaterQuads & BIT(1)) ? waterOpacity : 0), // water quads
					Color32(  0,  0,255, (gDebugOverlay.m_drawWaterQuads & BIT(2)) ? waterOpacity : 0), // calming quads
					Color32(255,255,  0, (gDebugOverlay.m_drawWaterQuads & BIT(3)) ? waterOpacity : 0), // wave quads
					true, // bRenderSolid
					true,
					false
				);
			}
#endif // __DEV
		}

		if (gDebugOverlay.m_overdrawRender)
		{
			XENON_ONLY(if (gDebugOverlay.m_overdrawUseHiZ) { DLC_Add(SetupHiZ); })
			DLC_Add(CDebugOverlay::SetCurrentPass, DEBUG_OVERLAY_RENDER_PASS_FILL, func);

			if (!gDebugOverlay.m_skipAddToDrawList)
			{
				DLC_Add(CRenderPhaseDebugOverlayInterface::InternalPassBegin, false, true, (int)grcRSV::CULL_BACK, true);
				CRenderListBuilder::AddToDrawList(GetEntityListIndex(), fullFilter, CRenderListBuilder::USE_DEBUGMODE, SUBPHASE_NONE, RenderStateSetup, CRenderListBuilder::RenderStateCleanupNormal);
				DLC_Add(CRenderPhaseDebugOverlayInterface::InternalPassEnd, false);
			}

			XENON_ONLY(if (gDebugOverlay.m_overdrawUseHiZ) { DLC_Add(RestoreHiZ); })

			const char* overdrawLegendTitle = "Overdraw Legend:";

			if (gDebugOverlay.m_overdrawRenderCost)
			{
				DLC(dlCmdUnLockRenderTarget, (0));
				Assert(overdrawTarget);
#if __D3D11 && GTA_VERSION // TODO -- gta only?
				DLC_Add(CRenderTargets::LockUIRenderTargetsReadOnlyDepth);
#elif RDR_VERSION
				DLC_Add(CRenderTargets::LockUIRenderTargetsNoDepth);
#else
				DLC_Add(CRenderTargets::LockUIRenderTargets);
#endif
				DLC_Add(DebugRendering::RenderCost, overdrawTarget, DR_COST_PASS_OVERDRAW, gDebugOverlay.m_overdrawOpacity, gDebugOverlay.m_overdrawMin, gDebugOverlay.m_overdrawMax, overdrawLegendTitle);
			}
			else
			{
				DLC_Add(DebugRendering::RenderCostLegend, false, 0, gDebugOverlay.m_overdrawMax, overdrawLegendTitle);
			}
		}
		else if (wireOpacity > 0.0f)
		{
			DLC_Add(CDebugOverlay::SetCurrentPass, DEBUG_OVERLAY_RENDER_PASS_WIREFRAME, func);
			DLC_Add(CRenderPhaseDebugOverlayInterface::InternalPassBegin, true, false, cull, true);

			if (!gDebugOverlay.m_skipAddToDrawList)
			{
				RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDebugOverlay - wireframe");
				CRenderListBuilder::AddToDrawList(GetEntityListIndex(), fullFilter, CRenderListBuilder::USE_DEBUGMODE, SUBPHASE_NONE, RenderStateSetup, CRenderListBuilder::RenderStateCleanupNormal);
				RENDERLIST_DUMP_PASS_INFO_END();
			}

			DLC_Add(CRenderPhaseDebugOverlayInterface::InternalPassEnd, true);
#if __DEV
			if (gDebugOverlay.m_drawWaterQuads)
			{
				const int waterOpacity = (int)(wireOpacity*255.0f + 0.5f);

				Water::RenderDebug(
					Color32(100,100,255, (gDebugOverlay.m_drawWaterQuads & BIT(1)) ? waterOpacity : 0), // water quads
					Color32(  0,  0,255, (gDebugOverlay.m_drawWaterQuads & BIT(2)) ? waterOpacity : 0), // calming quads
					Color32(255,255,  0, (gDebugOverlay.m_drawWaterQuads & BIT(3)) ? waterOpacity : 0), // wave quads
					false, // bRenderSolid
					true,
					false
				);
			}
#endif // __DEV
		}

		if (CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeMode(false) == 2)
		{
			DLC_Add(Water::RenderDebugOverlay);
		}

		DLC_Add(CDebugOverlay::SetCurrentPass, DEBUG_OVERLAY_RENDER_PASS_NONE, (grmModel__CustomShaderParamsFuncType)NULL);

#if DEBUG_OVERLAY_CLIP_TEST
		if (gDebugOverlay.m_clipXEnable || gDebugOverlay.m_clipYEnable)
		{
			DLC(dlCmdSetClipPlaneEnable, (0x00));
		}
#endif // DEBUG_OVERLAY_CLIP_TEST

		// TODO -- this is fucking lame
		// can't call this from CRenderPhaseDebug2d::StaticRender anymore, since the depth buffer does not seem to be available ..
		// so i've hacked it to render from within debug overlay renderphase
		DLC_Add(CRenderPhaseCascadeShadowsInterface::DebugDraw_render);

		EDGE_ONLY(DLC_Add(grcEffect::SetEdgeViewportCullEnable, true)); // restore
		DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);
		DLC(dlCmdSetCurrentViewportToNULL, ());
		DLC(dlCmdShaderFxPopForcedTechnique, ());

#if __DEV
		if (bUseLODControls)
		{
			dlDrawListMgr::ResetHighestLOD(false);
		}
#endif // __DEV

		PF_POP_TIMEBAR_DETAIL();

		if (gDebugOverlay.m_drawForOneFrameOnly)
		{
			gDebugOverlay.m_draw = false;
		}
	}

	DrawListEpilogue();
}

// ================================================================================================

#define STENCIL_MASK_SUPPORT_DEPTH (0 && __PS3) // not working yet ..
#define STENCIL_MASK_SUPPORT_PHASE (1 && !__XENON)

static bool  g_stencilMaskOverlayEnable = false;
static float g_stencilMaskOverlayOpacity = 1.0f;
static float g_stencilMaskOverlayIntensity = 1.0f;
//#if STENCIL_MASK_SUPPORT_PHASE
static int   g_stencilMaskOverlayPhase = 1;
//#endif // STENCIL_MASK_SUPPORT_PHASE
static bool  g_stencilMaskOverlayDisableShadows = false;
static bool  g_stencilMaskOverlaySelectAll = false;
static Vec4V g_stencilMaskOverlayColours[8] =
{
	Vec4V(V_ZERO),        // DEFERRED_MATERIAL_DEFAULT
	Vec4V(V_Z_AXIS_WONE), // DEFERRED_MATERIAL_PED
	Vec4V(V_ZERO),        // DEFERRED_MATERIAL_VEHICLE
	Vec4V(V_ZERO),        // DEFERRED_MATERIAL_TREE
	Vec4V(V_ZERO),        // DEFERRED_MATERIAL_TERRAIN
	Vec4V(V_Y_AXIS_WONE), // DEFERRED_MATERIAL_SELECTED
	Vec4V(V_ZERO),        // DEFERRED_MATERIAL_WATER_DEBUG
	Vec4V(V_ZERO),        // DEFERRED_MATERIAL_CLEAR
};
#if STENCIL_MASK_SUPPORT_DEPTH
static int   g_stencilMaskOverlayDepthFunc = 0;
static float g_stencilMaskOverlayDepth = 0.0f;
#endif // STENCIL_MASK_SUPPORT_DEPTH

// picker options
static bool g_stencilMaskOverlayDisplayUIWindow               = true;
static bool g_stencilMaskOverlayDisplayBoundsOfSelectedEntity = true;
static bool g_stencilMaskOverlayDisplayBoundsOfEntitiesInList = false;

static void StencilMaskOverlayDisableShadows_changed()
{
	if (g_stencilMaskOverlayDisableShadows)
	{
		DebugDeferred::m_overrideShadow = true;
		DebugDeferred::m_overrideShadow_intensity = 1.0f;
	}
	else
	{
		DebugDeferred::m_overrideShadow = false;
		DebugDeferred::m_overrideShadow_intensity = 0.0f;
	}
}

static void StencilMaskOverlayPickerOptions_changed()
{
	CGtaPickerInterface* pPickerInterface = static_cast<CGtaPickerInterface*>(g_PickerManager.GetInterface());

	if (pPickerInterface)
	{
		if (g_PickerManager.IsUiEnabled() != g_stencilMaskOverlayDisplayUIWindow)
		{
			pPickerInterface->ToggleWindowVisibility();
		}

		pPickerInterface->GetSettings().m_bDisplayBoundsOfSelectedEntity = g_stencilMaskOverlayDisplayBoundsOfSelectedEntity;
		pPickerInterface->GetSettings().m_bDisplayBoundsOfEntitiesInList = g_stencilMaskOverlayDisplayBoundsOfEntitiesInList;
	}
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::StencilMaskOverlayAddWidgets(bkBank& bk)
{
#if STENCIL_MASK_SUPPORT_DEPTH
	const char* depthFuncStrings[] =
	{
		"None",
		"Near Only",
		"Far Only",
	};
#endif // STENCIL_MASK_SUPPORT_DEPTH

	bk.PushGroup("Stencil Mask Overlay", false);
	{
		bk.AddToggle("Enable"            , &g_stencilMaskOverlayEnable);
		bk.AddSlider("Opacity"           , &g_stencilMaskOverlayOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Intensity"         , &g_stencilMaskOverlayIntensity, 1.0f, 64.0f, 1.0f/8.0f);
#if STENCIL_MASK_SUPPORT_PHASE
		const char* phaseStrings[] = {"Diffuse", "Overlay", "Water Mask"};
		bk.AddCombo ("Phase"             , &g_stencilMaskOverlayPhase, NELEM(phaseStrings), phaseStrings);
#endif // STENCIL_MASK_SUPPORT_PHASE
		bk.AddToggle("Disable Alpha"     , &fwRenderListBuilder::GetRenderPassDisableBits(), BIT(RPASS_ALPHA));
		bk.AddToggle("Disable Shadows"   , &g_stencilMaskOverlayDisableShadows, StencilMaskOverlayDisableShadows_changed);
		bk.AddToggle("Simple PostFX"     , &PostFX::GetSimplePfxRef());
		bk.AddToggle("Select All"        , &g_stencilMaskOverlaySelectAll);

		bk.AddSeparator();

		bk.AddColor ("Layer 0 - Default" , &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_DEFAULT]);
		bk.AddColor ("Layer 1 - Peds"    , &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_PED]);
		bk.AddColor ("Layer 2 - Vehicles", &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_VEHICLE]);
		bk.AddColor ("Layer 3 - Trees"   , &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_TREE]);
		bk.AddColor ("Layer 4 - Terrain" , &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_TERRAIN]);
		bk.AddColor ("Layer 5 - Selected", &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_SELECTED]);
		bk.AddColor ("Layer 6 - Water"   , &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_WATER_DEBUG]);
		bk.AddColor ("Layer 7 - Sky"     , &g_stencilMaskOverlayColours[DEFERRED_MATERIAL_CLEAR]);
#if STENCIL_MASK_SUPPORT_DEPTH
		bk.AddCombo ("Depth Func"        , &g_stencilMaskOverlayDepthFunc, NELEM(depthFuncStrings), depthFuncStrings);
		bk.AddSlider("Depth"             , &g_stencilMaskOverlayDepth, 0.0f, 9999.0f, 1.0f/32.0f);
#endif // STENCIL_MASK_SUPPORT_DEPTH

		bk.AddSeparator();

		bk.AddToggle("Picker - Display UI Window"                 , &g_stencilMaskOverlayDisplayUIWindow              , StencilMaskOverlayPickerOptions_changed);
		bk.AddToggle("Picker - Display Bounds Of Selected Entity" , &g_stencilMaskOverlayDisplayBoundsOfSelectedEntity, StencilMaskOverlayPickerOptions_changed);
		bk.AddToggle("Picker - Display Bounds Of Entities In List", &g_stencilMaskOverlayDisplayBoundsOfEntitiesInList, StencilMaskOverlayPickerOptions_changed);
	}
	bk.PopGroup();
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::StencilMaskOverlayRender(int phase)
{
	if (g_stencilMaskOverlayEnable &&
		g_stencilMaskOverlayOpacity > 0.0f &&
		g_stencilMaskOverlayPhase == phase)
	{
#if STENCIL_MASK_SUPPORT_PHASE
		if (phase == 0)
		{
#if GTA_VERSION
			GBuffer::UnlockTargets();
#elif RDR_VERSION
			GBuffer::UnlockAllTargets();
#endif
			GBuffer::LockSingleTarget(GBUFFER_RT_0, true);
		}
#endif // STENCIL_MASK_SUPPORT_PHASE

#if STENCIL_MASK_SUPPORT_DEPTH // TODO -- fix this or remove (note -- needs to be synced with changes to getSampleDepth)
		const float sampleDepth = CShaderLib::GetSampleDepth(g_stencilMaskOverlayDepth);

		int depthFunc = -1;

		switch (g_stencilMaskOverlayDepthFunc)
		{
		case 0: depthFunc = grcRSV::CMP_ALWAYS; break;
		case 1: depthFunc = grcRSV::CMP_LESSEQUAL; break;
		case 2: depthFunc = grcRSV::CMP_GREATER; break;
		}
#else
		const float sampleDepth = 0;
		const int depthFunc = grcRSV::CMP_ALWAYS;
#endif
		for (int i = 0; i < NELEM(g_stencilMaskOverlayColours); i++)
		{
			if (g_stencilMaskOverlayColours[i].GetWf() > 0.002f) // RAG colours don't go to zero .. lame
			{
				Vec4V colour = g_stencilMaskOverlayColours[i];

				colour.SetXYZ(colour.GetXYZ()*ScalarV(g_stencilMaskOverlayIntensity));
				colour.SetW(colour.GetW()*ScalarV(g_stencilMaskOverlayOpacity));

				DebugDeferred::StencilMaskOverlay((u8)i, DEFERRED_MATERIAL_TYPE_MASK, true, colour, sampleDepth, depthFunc);
			}
		}

#if STENCIL_MASK_SUPPORT_PHASE
		if (phase == 0)
		{
			GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
#if GTA_VERSION
			GBuffer::LockTargets();
#elif RDR_VERSION
			GBuffer::LockAllTargets();
#endif
		}
#endif // STENCIL_MASK_SUPPORT_PHASE
	}
}

__COMMENT(static) bool CRenderPhaseDebugOverlayInterface::StencilMaskOverlayIsSelectModeEnabled()
{
	return g_stencilMaskOverlayEnable && g_stencilMaskOverlayOpacity > 0.0f && g_stencilMaskOverlayColours[DEFERRED_MATERIAL_SELECTED].GetWf() > 0.002f;
}

__COMMENT(static) bool CRenderPhaseDebugOverlayInterface::StencilMaskOverlayIsSelectAll()
{
	return g_stencilMaskOverlaySelectAll;
}

__COMMENT(static) void CRenderPhaseDebugOverlayInterface::StencilMaskOverlaySetWaterMode(bool bWaterOverlay, bool bWaterOnly, const Color32& waterColour)
{
	g_stencilMaskOverlayEnable = bWaterOverlay;

	if (bWaterOverlay)
	{
		g_stencilMaskOverlayIntensity = 64.0f;

		for (int i = 0; i < 8; i++)
		{
			g_stencilMaskOverlayColours[i] = bWaterOnly ? Vec4V(V_ZERO_WONE) : Vec4V(V_ZERO);
		}

		g_stencilMaskOverlayColours[DEFERRED_MATERIAL_WATER_DEBUG] = waterColour.GetRGBA();
	}

#if STENCIL_MASK_SUPPORT_PHASE
	g_stencilMaskOverlayPhase = bWaterOverlay ? 2 : 1;
#endif // STENCIL_MASK_SUPPORT_PHASE
}

#endif // __BANK
