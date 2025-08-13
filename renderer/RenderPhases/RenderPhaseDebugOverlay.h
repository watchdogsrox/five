// ===============================================
// renderer/renderphases/renderphasedebugoverlay.h
// (c) 2011 RockstarNorth
// ===============================================

#ifndef _RENDERPHASE_DEBUG_OVERLAY_H_
#define _RENDERPHASE_DEBUG_OVERLAY_H_

#if __BANK

#include "renderer/renderphases/renderphase.h"
#include "../../shader_source/Debug/debug_overlay.fxh"

namespace rage { class Color32; }
namespace rage { class bkBank; }

class CViewport;

enum eDebugOverlayChannel
{
	DEBUG_OVERLAY_CHANNEL_NONE,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_XYZ,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_X,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_Y,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_Z,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV0_W,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_XYZ,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_X,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_Y,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_Z,
	DEBUG_OVERLAY_VERTEX_CHANNEL_CPV1_W,
	DEBUG_OVERLAY_WORLD_NORMAL,
	DEBUG_OVERLAY_WORLD_TANGENT,
#if DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
	DEBUG_OVERLAY_DIFFUSE_TEXTURE,
	DEBUG_OVERLAY_BUMP_TEXTURE,
	DEBUG_OVERLAY_SPEC_TEXTURE,
#endif // DEBUG_OVERLAY_SUPPORT_TEXTURE_MODES
	DEBUG_OVERLAY_TERRAIN_WETNESS,
	DEBUG_OVERLAY_PED_DAMAGE,
	DEBUG_OVERLAY_VEH_GLASS,
	DEBUG_OVERLAY_CHANNEL_COUNT
};

enum eDebugOverlayColourMode
{
	DOCM_NONE,
	DOCM_RENDER_BUCKETS,
	DOCM_TXD_COST,
	DOCM_TXD_COST_OVER_AREA,
	DOCM_MODEL_PATH_CONTAINS,
	DOCM_MODEL_NAME_IS,
	DOCM_MODEL_USES_TEXTURE,
	DOCM_MODEL_USES_PARENT_TXD,
	DOCM_MODEL_DOES_NOT_USE_PARENT_TXD,
	DOCM_SHADER_NAME_IS,
	DOCM_SHADER_USES_TEXTURE,
	DOCM_SHADER_USES_TXD,
	DOCM_RANDOM_COLOUR_PER_ENTITY,
	DOCM_RANDOM_COLOUR_PER_TEXTURE_DICTIONARY,
	DOCM_RANDOM_COLOUR_PER_SHADER,
	DOCM_RANDOM_COLOUR_PER_DRAW_CALL,
	DOCM_PICKER,
	DOCM_ENTITYSELECT,
	DOCM_SHADER_EDIT,
	DOCM_TEXTURE_VIEWER,
	DOCM_INTERIOR_LOCATION,
	DOCM_INTERIOR_LOCATION_LIMBO_ONLY,
	DOCM_INTERIOR_LOCATION_PORTAL_ONLY,
	DOCM_INTERIOR_LOCATION_INVALID,
	DOCM_GEOMETRY_COUNTS,
	DOCM_ENTITY_FLAGS,
	DOCM_ENTITY_TYPE,
	DOCM_IS_ENTITY_PHYSICAL,
	DOCM_IS_ENTITY_DYNAMIC,
	DOCM_IS_ENTITY_STATIC,
	DOCM_IS_ENTITY_NOT_STATIC,
	DOCM_IS_ENTITY_NOT_HD_TXD_CAPABLE,
	DOCM_IS_ENTITY_HD_TXD_CAPABLE,
	DOCM_IS_ENTITY_CURRENTLY_HD,
	DOCM_IS_ENTITY_RENDER_IN_SHADOWS,
	DOCM_IS_ENTITY_DONT_RENDER_IN_SHADOWS,
	DOCM_IS_ENTITY_ONLY_RENDER_IN_SHADOWS,
	DOCM_IS_ENTITY_RENDER_IN_MIRROR_REFLECTIONS,
	DOCM_IS_ENTITY_RENDER_IN_WATER_REFLECTIONS,
	DOCM_IS_ENTITY_RENDER_IN_PARABOLOID_REFLECTIONS,
	DOCM_IS_ENTITY_DONT_RENDER_IN_ALL_REFLECTIONS,
	DOCM_IS_ENTITY_DONT_RENDER_IN_ANY_REFLECTIONS,
	DOCM_IS_ENTITY_ONLY_RENDER_IN_REFLECTIONS,
	DOCM_IS_ENTITY_DONT_RENDER_IN_MIRROR_REFLECTIONS,
	DOCM_IS_ENTITY_DONT_RENDER_IN_WATER_REFLECTIONS,
	DOCM_IS_ENTITY_DONT_RENDER_IN_PARABOLOID_REFLECTIONS,
	DOCM_IS_ENTITY_ALWAYS_PRERENDER,
	DOCM_IS_ENTITY_ALPHA_LESS_THAN_255,
	DOCM_IS_ENTITY_SMASHABLE,
	DOCM_IS_ENTITY_LOW_PRIORITY_STREAM,
	DOCM_IS_ENTITY_LIGHT_INSTANCED,
	DOCM_PROP_PRIORITY_LEVEL,
	DOCM_IS_PED_HD,
	DOCM_MODEL_TYPE,
	DOCM_MODEL_HD_STATE,
	DOCM_IS_MODEL_NOT_HD_TXD_CAPABLE,
	DOCM_IS_MODEL_HD_TXD_CAPABLE,
	DOCM_IS_MODEL_BASE_HD_TXD_CAPABLE,
	DOCM_IS_MODEL_ANY_HD_TXD_CAPABLE,
	DOCM_IS_MODEL_CURRENTLY_USING_BASE_HD_TXD,
	DOCM_IS_MODEL_CURRENTLY_USING_ANY_HD_TXD,
	DOCM_LOD_TYPE,
//	DOCM_DRAWABLE_LOD_INDEX,
	DOCM_DRAWABLE_TYPE,
	DOCM_DRAWABLE_HAS_SKELETON,
	DOCM_DRAWABLE_HAS_NO_SKELETON,
	DOCM_CONTAINS_DRAWABLE_LOD_HIGH,
	DOCM_CONTAINS_DRAWABLE_LOD_MED ,
	DOCM_CONTAINS_DRAWABLE_LOD_LOW ,
	DOCM_CONTAINS_DRAWABLE_LOD_VLOW,
	DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_HIGH,
	DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_MED ,
	DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_LOW ,
	DOCM_DOESNT_CONTAIN_DRAWABLE_LOD_VLOW,
	DOCM_IS_OBJECT_PICKUP           ,
	DOCM_IS_OBJECT_DOOR             ,
	DOCM_IS_OBJECT_UPROOTED         ,
	DOCM_IS_OBJECT_VEHICLE_PART     ,
	DOCM_IS_OBJECT_AMBIENT_PROP     ,
	DOCM_IS_OBJECT_DETACHED_PED_PROP,
	DOCM_IS_OBJECT_CAN_BE_PICKED_UP ,
	DOCM_IS_DONT_WRITE_DEPTH,
	DOCM_IS_PROP,
	DOCM_IS_FRAG,
	DOCM_IS_TREE,
	DOCM_IS_TINTED,
//	DOCM_OBJECT_WITH_BUOYANCY,
	DOCM_OBJECT_USES_MAX_DISTANCE_FOR_WATER_REFLECTION,
	DOCM_HAS_VEHICLE_DAMAGE_TEXTURE,
	DOCM_HAS_2DEFFECTS,
	DOCM_HAS_2DEFFECT_LIGHTS,
	DOCM_VERTEX_VOLUME_RATIO,
//	DOCM_SHADER_HAS_INEFFECTIVE_DIFFTEX,
	DOCM_SHADER_HAS_INEFFECTIVE_BUMPTEX,
	DOCM_SHADER_HAS_INEFFECTIVE_SPECTEX,
	DOCM_SHADER_HAS_ALPHA_DIFFTEX,
	DOCM_SHADER_HAS_ALPHA_BUMPTEX,
	DOCM_SHADER_HAS_ALPHA_SPECTEX,
	DOCM_SHADER_USES_DIFFTEX_NAME,
	DOCM_SHADER_USES_BUMPTEX_NAME,
	DOCM_SHADER_USES_SPECTEX_NAME,
	DOCM_SHADER_FRESNEL_TOO_LOW,
	DOCM_SKINNED_SELECT,
	DOCM_SHADER_NOT_IN_MODEL_DRAWABLE___TEMP,
	DOCM_OLD_WIRES___TEMP, // for BS#225656
	DOCM_COUNT
};

class CRenderPhaseDebugOverlayInterface
{
public:
	static void InitShaders();
	static void AddWidgets();

	static bool IsEntitySelectRequired();

	static void Update();

	static bool& DrawEnabledRef(); // for external toggles
	static float& DrawOpacityRef();
	static Color32& DrawColourRef();
	static int& ColourModeRef(); // eDebugOverlayColourMode

	static void SetChannel(eDebugOverlayChannel channel);
	static void SetColourMode(eDebugOverlayColourMode mode);
	static void SetFilterString(const char* filterStr);

	static bool BindShaderParams(entitySelectID entityId, int renderBucket);

	static bool InternalPassBegin(bool bWireframe, bool bOverdraw, int cull, bool bDrawRope);
	static void InternalPassEnd(bool bWireframe);

	static void SetRasterizerCullState(int cull);

	static int GetWaterQuadWireframeMode(bool bForceWireframeMode1); // special wireframe modes for water quads
	static u32 GetWaterQuadWireframeFlags();

#if __PS3
	static void ForceGrmModelPPUCodepathBegin();
	static void ForceGrmModelPPUCodepathEnd();
#endif // __PS3

	static void ToggleGlobalTxdMode();
	static void ToggleLowPriorityStreamMode();
	static void TogglePropPriorityLevelMode();
	static void ToggleModelHDStateMode();
	static void TogglePedSilhouetteMode();
	static void TogglePedHDMode();
	static void TogglePedHDModeSetState(int state);
	static void ToggleVehicleGlassMode();
	static void ToggleInteriorLocationMode();
	static void ToggleTerrainWetnessMode();
	static void TogglePedDamageMode();
	static void TogglePedDamageModeSetEnabled(bool bEnabled);
	static void ToggleVehicleGlassDebugMode();
	static void ToggleShadowProxyMode();
	static void ToggleNoShadowCastingMode();
	static void ToggleReflectionProxyMode();
	static void ToggleNoReflectionsMode();

	static void StencilMaskOverlayAddWidgets(bkBank& bk);
	static void StencilMaskOverlayRender(int phase);
	static bool StencilMaskOverlayIsSelectModeEnabled();
	static bool StencilMaskOverlayIsSelectAll();
	static void StencilMaskOverlaySetWaterMode(bool bWaterOverlay, bool bWaterOnly, const Color32& waterColour);
};

class CRenderPhaseDebugOverlay : public CRenderPhaseScanned
{
public:
	CRenderPhaseDebugOverlay(CViewport* pGameViewport);
	virtual ~CRenderPhaseDebugOverlay() {}

	virtual void UpdateViewport();
	virtual void BuildDrawList();
};

#endif // __BANK
#endif // _RENDERPHASE_DEBUG_OVERLAY_H_
