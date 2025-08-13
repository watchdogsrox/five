//
//
//
//
#ifndef __RENDERER_RENDERLISTBUILDER_H__
#define __RENDERER_RENDERLISTBUILDER_H__

#include "grcore/effect_config.h"
#include "fwrenderer/renderlistbuilder.h"
#include "renderer/Renderer.h"
#include "rmptfx/ptxconfig.h"
#include "rmptfx/ptxdraw.h"

namespace rage {
	class bkBank;
}

namespace rage {
	class fwEntity;
}


#define RENDERLIST_DUMP_PASS_INFO (1 && __DEV)

#if RENDERLIST_DUMP_PASS_INFO

extern int  g_RenderListDumpPassInfo;
extern bool g_RenderListDumpPassFlag;

#define RENDERLIST_DUMP_PASS_INFO_ONLY(x) x
#define RENDERLIST_DUMP_PASS_INFO_BEGIN(name) \
{ \
	static int s_RenderListDumpPassInfo = 0; \
	if (s_RenderListDumpPassInfo < g_RenderListDumpPassInfo) \
	{ \
		s_RenderListDumpPassInfo = g_RenderListDumpPassInfo; \
		g_RenderListDumpPassFlag = true; \
		Displayf("> RENDERPHASE: %s", name); \
	} \
}
#define RENDERLIST_DUMP_PASS_INFO_END() \
{ \
	g_RenderListDumpPassFlag = false; \
}

#else
#define RENDERLIST_DUMP_PASS_INFO_ONLY(x)
#define RENDERLIST_DUMP_PASS_INFO_BEGIN(name)
#define RENDERLIST_DUMP_PASS_INFO_END()
#endif

struct DrawListAddPass;

//
//
//
//
class CRenderListBuilder
{
public:
	enum eDrawFilter
	{
		RENDER_OPAQUE						=0x0001,
		RENDER_ALPHA						=0x0002,
		RENDER_MIRROR_ALPHA					=0x0004,
		RENDER_FADING						=0x0008,
		RENDER_FADING_ONLY					=0x0010,
		RENDER_WATER						=0x0020,
		RENDER_DISPL_ALPHA					=0x0040,
		RENDER_PUDDLES						=0x0080,
		RENDER_MASK							=0x00FF,

		// don't render flags:
		RENDER_DONT_RENDER_PLANTS			=0x0100,
		RENDER_DONT_RENDER_TREES			=0x0200,
		RENDER_DONT_RENDER_DECALS_CUTOUTS	=0x0400,
		RENDER_DONT_RENDER_MASK				=0x0700
	};

	enum eDrawMode // not to be confused with grcDrawMode
	{
		USE_FORWARD_LIGHTING				=0x0001,
		USE_SIMPLE_LIGHTING					=0x0002,
		USE_DEFERRED_LIGHTING				=0x0004,
		USE_SIMPLE_LIGHTING_DISTANCE_FADE	=0x0008,
		USE_NO_LIGHTING						=0x0010,
#if HAS_RENDER_MODE_SHADOWS
		USE_SHADOWS							=0x0020,
		USE_SKINNED_SHADOWS					=0x0040,
#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
		USE_SHADOWS_SPECIAL					=0x0080,
		USE_SKINNED_SHADOWS_SPECIAL			=0x0100,
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES
#endif // HAS_RENDER_MODE_SHADOWS
#if __BANK
		USE_DEBUGMODE						=0x0200,
#endif // __BANK
		eDrawMode_MAX						=0x0200, // must be >= highest drawmode
	};

	static int GetOpaqueDrawListAddPassRowCount();
	static int GetAlphaFilterListCount();
	static fwDrawListAddPass *GetOpaqueDrawListAddPass();
	static fwDrawListAddPass *GetOpaqueDrawListAddPassAndSize(int &nelements);
	static int GetEntityListSize(s32 list);

	static void AddToDrawList(s32 list, int fullFilter, eDrawMode drawMode, int subphasePass = SUBPHASE_NONE, 
							fwRenderListBuilder::RenderStateCallback customSetupCallback = NULL, 
							fwRenderListBuilder::RenderStateCallback customCleanupCallback = NULL,
							fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback = NULL,
							int entityStart=0, int entityStride=1);

	static void AddToDrawListAlpha(s32 list, int fullFilter, eDrawMode drawMode, int subphasePass = SUBPHASE_NONE, 
		fwRenderListBuilder::RenderStateCallback customSetupCallback = NULL, 
		fwRenderListBuilder::RenderStateCallback customCleanupCallback = NULL,
		fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback = NULL,
		int entityStart=0, int numEntityLists=1, int filterSubListStart = 0, int numFilterLists = 1);

	static void AddToDrawList_ForcedAlpha(s32 list, int fullFilter, eDrawMode drawMode, int subphasePass = SUBPHASE_NONE, 
		fwRenderListBuilder::RenderStateCallback customSetupCallback = NULL, 
		fwRenderListBuilder::RenderStateCallback customCleanupCallback = NULL,
		fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback = NULL);

#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
	static void AddToDrawList_Partial(s32 list, int fullFilter, eDrawMode drawMode,
									int entityStart, int entityStride,
									int subphasePass = SUBPHASE_NONE, 
									fwRenderListBuilder::RenderStateCallback customSetupCallback = NULL, 
									fwRenderListBuilder::RenderStateCallback customCleanupCallback = NULL,
									fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback = NULL);
	static int AddToDrawList_Opaque(int drawAndPassListIdx, s32 list, int fullFilter, eDrawMode drawMode, 
							int entityStart, int entityStride,
							int subphasePass = SUBPHASE_NONE, 
							fwRenderListBuilder::RenderStateCallback customSetupCallback = NULL, 
							fwRenderListBuilder::RenderStateCallback customCleanupCallback = NULL,
							fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback = NULL);
	static void AddToDrawList_DecalsAndPlants(s32 list, int fullFilter, eDrawMode drawMode, 
 							int entityStart, int entityStride,
							int subphasePass = SUBPHASE_NONE, 
							fwRenderListBuilder::RenderStateCallback customSetupCallback = NULL, 
							fwRenderListBuilder::RenderStateCallback customCleanupCallback = NULL,
							fwRenderListBuilder::CustomFlagsChangedCallback customFlagsChangedCallback = NULL);
#endif //RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

#if GS_INSTANCED_SHADOWS
	static void AddToDrawListShadowEntitiesGrass();
	static void AddToDrawListShadowEntitiesInstanced(s32 list, bool bDirLightShadow, int techniqueId, int techniqueId_edge = -1,
		int textureTechniqueId = -1, int textureTechniqueId_edge = -1 , fwRenderListBuilder::AddEntityCallback localShadowCallBack = NULL);
#endif
	static void AddToDrawListShadowEntities(s32 list, bool bDirLightShadow, int subphasePass, int techniqueId, int techniqueId_edge = -1,
																							  int textureTechniqueId = -1, int textureTechniqueId_edge = -1 , fwRenderListBuilder::AddEntityCallback localShadowCallBack = NULL);
#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	static int AddToDrawListShadowEntitiesAlphaObjects(s32 list, bool bDirLightShadow, int subphasePass, int techniqueId);
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

#if RMPTFX_USE_PARTICLE_SHADOWS
	static void AddToDrawListShadowParticles(s32 cascadeIndex);
#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
	static void AddToDrawListShadowParticlesAllCascades(s32 noOfCascades, grcViewport *pViewports, Vec4V *pWindows, bool bUseInstancedShadow = false);
#endif
#endif

	static u32	CountDrawListEntities(s32 list, int filter, u32 baseFlags);

	static void InitDLCCommands();

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

	static void RenderStateSetupDeferred           (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	static void RenderStateCleanupDeferred         (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	static void RenderStateSetupNormal             (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	static void RenderStateCustomFlagsChangedNormal(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags, u16 previousCustomFlags, u16 currentCustomFlags);
	static void RenderStateCleanupNormal           (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	
	static void RenderStateCleanupSimpleFlush(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);

#if __BANK
	static bool s_RenderListBuilderEnableScissorClamping;
#else
	static const bool s_RenderListBuilderEnableScissorClamping = true;
#endif // __BANK

private:
	static int  AddToDrawListEntityList      (s32 list, fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStride));
	static int AddToDrawListEntityListShadow(s32 list, fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStart) RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS_ONLY_PARAM(int entityStride));
	static void AddToDrawListTrees           (s32 list,                                            eRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);

	static void ExecutePassList(fwDrawListAddPass *passList, int elementCount, s32 list, int filter, eDrawMode drawMode, int subphasePass);
#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
	static void ExecutePassList_Interleaved(fwDrawListAddPass *passList, int elementCount, s32 list, int filter, eDrawMode drawMode, int subphasePass, int interleaveMask, int interleaveValue);
#endif //RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

#if USE_EDGE
	static void RenderStateSetupShadowEdge  (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	static void RenderStateCleanupShadowEdge(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
#else // not USE_EDGE
	static void RenderStateSetupShadow      (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	static void RenderStateSetupAlphaShadow	(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
	static void RenderStateCleanupShadow    (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags);
#endif // not USE_EDGE

#if ENABLE_ADD_ENTITY_GBUFFER_CALLBACK
	static bool AddEntityCallbackGBuffer    (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, float sortVal, fwEntity *entity);
#endif // ENABLE_ADD_ENTITY_GBUFFER_CALLBACK  

#if ENABLE_ADD_ENTITY_SHADOW_CALLBACK
	static bool AddEntityCallbackShadow     (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, float sortVal, fwEntity *entity);
#endif // ENABLE_ADD_ENTITY_SHADOW_CALLBACK	  

#if ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK
	static DECLARE_MTR_THREAD fwRenderListBuilder::AddEntityCallback sm_AddEntityCallbackLocalShadow;
#endif // ENABLE_ADD_ENTITY_LOCALSHADOW_CALLBACK

	static u32 CountPassEntities(fwDrawListAddPass *passList, int elementCount, s32 list, u32 baseFlags);

	static void SetForceShader(rage::grmShader::eDrawType drawtype, int pass);
#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	static void SetTessellatedDrawMode(int tessellatedDrawMode);
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES
#if GS_INSTANCED_SHADOWS
	static void SetInstancedDrawMode(bool bInstanced);
#endif

#if RENDERLIST_DUMP_PASS_INFO
	static void DumpPassInfo(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, const fwRenderListBuilder::AddEntityFlags& addEntityFlags, fwRenderGeneralFlags generalFlags, int count, int total);
#endif // RENDERLIST_DUMP_PASS_INFO
};

#endif // __RENDERER_RENDERLISTBUILDER_H__...

