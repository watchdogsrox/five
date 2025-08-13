//
// FILE :    DrawableDataStructs.cpp
// PURPOSE : structs to establish data required to draw entities. effectively handles the extraction of necessary data from
// entity type objects to create drawable data type objects which are wholly independent of the entity.
// AUTHOR :  john.
// CREATED : 1/06/2009
//
/////////////////////////////////////////////////////////////////////////////////
 



// rage headers
#include "breakableglass/bgdrawable.h"
#include "fragment/tune.h"
#if __BANK
#include "fwdebug/picker.h"
#endif // __BANK
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwtl/pool.h"
#include "vector/quaternion.h"
#include "fwrenderer/instancing.h"
#include "entity/camerarelativeextension.h"
#if __DEV
#include "rmcore/lodgroup.h"
#endif // __DEV

//game headers
#include "camera/CamInterface.h"
#include "debug/Rendering/DebugDeferred.h"
#include "grmodel/matrixset.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/Ped.h"
#include "peds/PedTuning.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedVariationPack.h"
#include "physics/breakable.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/DrawLists/DrawableDataStructs.h"
#include "renderer/DrawLists/drawList.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "renderer/Entities/EntityBatchDrawHandler.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/Entity.h"
#include "scene/EntityBatch.h"
#include "scene/lod/LodScale.h"
#include "scene/lod/LodDrawable.h"
#include "scene/world/GameWorld.h"
#include "shaders/CustomShaderEffectGrass.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "shaders/CustomShaderEffectPed.h"
#include "Vehicles/VehicleFactory.h"
#include "vfx/decal/DecalManager.h"

#if __PPU
#include "rmcore/drawablespu.h"
#endif

#define PROFILE_DATASTRUCTS 0


#if PROFILE_DATASTRUCTS
#include "profile/element.h"

PF_PAGE(DrawableDataStructs_Init_Page, "Drawable Data Structures Init()");
PF_GROUP(EntityDrawData_Init_Group);
PF_LINK(DrawableDataStructs_Init_Page, EntityDrawData_Init_Group);


PF_TIMER(EntityDrawData__Init, EntityDrawData_Init_Group);

PF_PAGE(DrawableDataStructs_Draw_Page, "Drawable Data Structures Draw()");
PF_GROUP(EntityDrawData_Draw_Group);
PF_LINK(DrawableDataStructs_Draw_Page, EntityDrawData_Draw_Group);

PF_TIMER(EntityDrawDataSimple__Draw, EntityDrawData_Draw_Group);

PF_TIMER(EntityDrawDataFM__Draw, EntityDrawData_Draw_Group);


PF_PAGE(DrawableDataStructs_Count_Init_Page, "Drawable Data Structures Init Count");
PF_GROUP(EntityDrawData_Init_Count_Group);
PF_LINK(DrawableDataStructs_Count_Init_Page, EntityDrawData_Init_Count_Group);


PF_COUNTER(EntityDrawDataSimpleCount__Init, EntityDrawData_Init_Count_Group);
PF_COUNTER(EntityDrawDataFMCount__Init, EntityDrawData_Init_Count_Group);
#endif

RENDER_OPTIMISATIONS();

PF_PAGE(DrawableDataStructs_Page, "Validate Str Indices Cache");
PF_GROUP(DrawableDataStructs_Group);
PF_LINK(DrawableDataStructs_Page, DrawableDataStructs_Group);

#if __BANK
// Set this to true to add GCM/PIX markers for every model that is being rendered
bool g_AddModelGcmMarkers = false;

inline void BeginModelGcmMarker(const CBaseModelInfo *pModelInfo)
{
	if (g_AddModelGcmMarkers)
	{
		const char *name = pModelInfo->GetModelName();
		PIXBegin(0, name);
	}

	DEV_ONLY(rmcLodGroup::debugRenderModel = pModelInfo->GetModelName());
}

inline void EndModelGcmMarker()
{
	if (g_AddModelGcmMarkers)
	{
		PIXEnd();
	}
}

#else // __BANK

__forceinline void BeginModelGcmMarker(const CBaseModelInfo * /*pModelInfo*/) {}
__forceinline void EndModelGcmMarker() {}

#endif // __BANK


#if __DEV
inline void ValidateStreamingIndexCache(fwModelId modelId, CBaseModelInfo* ASSERT_ONLY(pModelInfo))
{
	if (dlDrawListMgr::StrIndicesValidationEnabled())
	{
		strStreamingModule* streamingModule = fwArchetypeManager::GetStreamingModule();
		s32 objIndex = modelId.ConvertToStreamingIndex().Get();
		Assertf(gDrawListMgr->ValidateModuleIndexHasBeenCached(objIndex, streamingModule), "Failed to find cached streaming index for %s", pModelInfo->GetModelName());
	}
}

#else // __DEV

__forceinline void ValidateStreamingIndexCache(fwModelId, CBaseModelInfo*) {}

#endif // __DEV

__forceinline u32 AdjustAlpha(u32 alpha, bool shouldForceAlphaForWaterReflection)
{
#if __BANK
	if (Unlikely(CPostScanDebug::GetOverrideGBufBitsEnabled()))
	{
		if (CPostScanDebug::GetOverrideGBufVisibilityType() == VIS_PHASE_WATER_REFLECTION)
		{
			if (gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_GBUF) ||
				gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_DRAWSCENE) ||
				gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_DEBUG) ||
				gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_DEBUG_OVERLAY))
			{
				if (shouldForceAlphaForWaterReflection)
				{
					return LODTYPES_ALPHA_VISIBLE;
				}
			}
		}
	}
#endif // __BANK

	// Usual case is not shouldForceAlpha == false, so check that first so we 
	// can avoid both the call to IsExecutingDrawList() and the branch misprediction.
	if (gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_REFLECTION_MAP))
	{
		return LODTYPES_ALPHA_VISIBLE;
	}
	
	if (!shouldForceAlphaForWaterReflection || !gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_WATER_REFLECTION))
	{
		return alpha;
	}

	// this causes SLOD3's etc. to cover up the LODs unfortunately, but
	// without it we can't render lower LODs in place of higher LODs ..
	return LODTYPES_ALPHA_VISIBLE;
}


/***************************** CEntityDrawDataCommon definitions *****************************/
#if __BANK
void CEntityDrawDataCommon::InitFromEntity(fwEntity* pEntity)
{
#if ENTITYSELECT_ENABLED_BUILD
	m_entityId = CEntityIDHelper::ComputeEntityID(pEntity);
#endif // ENTITYSELECT_ENABLED_BUILD
#if __PS3
	m_contextStats = static_cast<CEntity*>(pEntity)->GetDrawableStatsContext();
#endif // __PS3
	m_captureStats = g_PickerManager.GetIndexOfEntityWithinResultsArray(pEntity) > -1;
}

bool CEntityDrawDataCommon::SetShaderParams(int renderBucket)
{
	return SetShaderParamsStatic(m_entityId, renderBucket);
}

bool CEntityDrawDataCommon::SetShaderParamsStatic(entitySelectID ENTITYSELECT_ONLY(entityId), int ENTITYSELECT_ONLY(renderBucket))
{
#if ENTITYSELECT_ENABLED_BUILD
	CEntitySelect::BindEntityIDShaderParam(entityId);
	if (!CRenderPhaseDebugOverlayInterface::BindShaderParams(entityId, renderBucket))
	{
		return false; // don't draw
	}
#endif // ENTITYSELECT_ENABLED_BUILD
	return true;
}

u8 CEntityDrawDataCommon::GetContextStats() const
{
#if __PS3
	return m_contextStats;
#else
	return (u8)DRAWLISTMGR->GetDrawableStatContext();
#endif
}
#endif // __BANK

// CEntityDrawData relies on the following transforms being 32 bytes
CompileTimeAssert(sizeof(fwSimpleTransform) == 32);
CompileTimeAssert(sizeof(fwSimpleScaledTransform) == 32);
CompileTimeAssert(sizeof(fwIdentityTransform) == 16);

// initialise this type of drawable data from the entity
void CEntityDrawData::Init(CEntity *pEntity)
{
#if PROFILE_DATASTRUCTS
	PF_FUNC(EntityDrawData__Init);
	PF_INCREMENT(EntityDrawDataSimpleCount__Init);
#endif

#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif
	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_fadeAlpha = pEntity->GetAlpha();
	m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_allowAlphaBlend = !pEntity->IsBaseFlagSet(fwEntity::HAS_OPAQUE) && pEntity->IsBaseFlagSet(fwEntity::HAS_DECAL);
	m_matID = pEntity->GetDeferredMaterial();

	m_phaseLODs = pEntity->GetPhaseLODs();
	m_lastLODIdx = pEntity->GetLastLODIdx();

	m_interior = pEntity->GetInteriorLocation().IsValid();

#if __ASSERT
	fwTransform::Type type = pEntity->GetTransform().GetTypeIdentifier();
	FatalAssertf(type == fwTransform::TYPE_SIMPLE || type == fwTransform::TYPE_SIMPLE_SCALED || type == fwTransform::TYPE_IDENTITY, "Transform error - entity '%s' (%p) expected to have simple/identity transform, but has type %d", pEntity->GetModelName(), pEntity, type);
#endif
	const __vector4 *pPackVector = reinterpret_cast<const __vector4*>(&pEntity->GetTransform());
	m_vector0 = pPackVector[0];
	m_vector1 = pPackVector[1];

	m_fadeDist = pEntity->GetUseAltFadeDistance();

	// Get the model index last - it needs to deref the archetype, and we've 
	// prefetched that some time ago - doing this last gives us some more time 
	// for it to come in.
	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());

	Assert(pEntity->GetDrawHandlerPtr());
	m_pDrawable = static_cast<CEntityDrawHandler&>(pEntity->GetDrawHandler()).GetDrawable();
}

void SubmitDrawTree( const Drawable* pDrawable, const Matrix34& mat, u32 bucketMask, int LODLevel, u16 drawableStat, bool isShadowPass )
{
	// Cache previous state
	const grcRasterizerStateHandle rs_prev = grcStateBlock::RS_Active;

	// Either draw 2-Pass front & back or 1-Pass double-sided
	const bool twoPassTrees = BANK_SWITCH(CRenderer::TwoPassTreeRenderingEnabled(), false);
	if ( !twoPassTrees )
	{
		// Set front face culling
		if (isShadowPass)
		{
			CRenderPhaseCascadeShadowsInterface::SetRasterizerState(CSM_RS_TWO_SIDED);
		}
		else
		{
			CShaderLib::SetFacingBackwards(false);	//When face culling is disabled, trees will use VFACE register for proper normal flipping, so don't flip twice!
			CShaderLib::SetDisableFaceCulling();
		}
	}

	pDrawable->Draw(mat, bucketMask, LODLevel, drawableStat);

	if(twoPassTrees)
	{
		if (isShadowPass){
			CRenderPhaseCascadeShadowsInterface::SetRasterizerState(CSM_RS_CULL_FRONT);
		}
		else{
			CShaderLib::SetFrontFaceCulling();
			CShaderLib::SetFacingBackwards(CRenderer::TwoPassTreeNormalFlipping());
		}

		pDrawable->Draw(mat, bucketMask, LODLevel, drawableStat);
	}

	// Restore previous state
	if (isShadowPass)
		CRenderPhaseCascadeShadowsInterface::RestoreRasterizerState();
	else{
		grcStateBlock::SetRasterizerState(rs_prev);
		CShaderLib::SetFacingBackwards(false);
	}
}

static bank_float g_NearClipOverrideValue = 0.046f;
static bank_float g_FovOverrideValue = 50.0f;

#if __DEV

static bool g_bypass_CEntityDrawData                = false;
static bool g_bypass_CEntityDrawDataFm              = false;
static bool g_bypass_CEntityDrawDataStreamPed       = false;
static bool g_bypass_CEntityDrawDataPedBIG          = false;
static bool g_bypass_CEntityDrawDataBreakableGlass  = false;
static bool g_bypass_CEntityDrawDataFragType        = false;
static bool g_bypass_CEntityDrawDataSkinned         = false;
static bool g_bypass_CEntityDrawDataFrag            = false;
static bool g_bypass_CEntityDrawDataDetachedPedProp = false;
static bool g_bypass_CEntityDrawDataVehicleVar      = false;

void InitBypassWidgets(bkBank& bk)
{
	bk.PushGroup("DrawableDataStruct Bypass", false);
	{
		bk.AddToggle("g_bypass_CEntityDrawData               ", &g_bypass_CEntityDrawData               );
		bk.AddToggle("g_bypass_CEntityDrawDataFm             ", &g_bypass_CEntityDrawDataFm             );
		bk.AddToggle("g_bypass_CEntityDrawDataStreamPed      ", &g_bypass_CEntityDrawDataStreamPed      );
		bk.AddToggle("g_bypass_CEntityDrawDataPedBIG         ", &g_bypass_CEntityDrawDataPedBIG         );
		bk.AddToggle("g_bypass_CEntityDrawDataBreakableGlass ", &g_bypass_CEntityDrawDataBreakableGlass );
		bk.AddToggle("g_bypass_CEntityDrawDataFragType       ", &g_bypass_CEntityDrawDataFragType       );
		bk.AddToggle("g_bypass_CEntityDrawDataSkinned        ", &g_bypass_CEntityDrawDataSkinned        );
		bk.AddToggle("g_bypass_CEntityDrawDataFrag           ", &g_bypass_CEntityDrawDataFrag           );
		bk.AddToggle("g_bypass_CEntityDrawDataDetachedPedProp", &g_bypass_CEntityDrawDataDetachedPedProp);
		bk.AddToggle("g_bypass_CEntityDrawDataVehicleVar     ", &g_bypass_CEntityDrawDataVehicleVar     );
	}
	bk.PopGroup();
}
#endif // __DEV

static bank_bool s_zTestFPV = false;
#if __BANK
void InitRenderWidgets(bkBank& bk)
{
	bk.PushGroup("First Person");
	{
		bk.AddSlider("Near clip override value", &g_NearClipOverrideValue, 0.0f, 5.0f, 0.0001f);
		bk.AddSlider("FOV override value", &g_FovOverrideValue, 1.0f, 180.0f, 0.1f);
		bk.AddToggle("Z-test in FPV?", &s_zTestFPV);
	}
	bk.PopGroup();
}
#endif // __BANK

void SubmitDraw( const Drawable* pDrawable, const Matrix34& mat, u32 bucketMask, int LODLevel, u16 drawableStat, bool useTreeRendering, bool isShadowPass  )
{
	if ( useTreeRendering )
		SubmitDrawTree(pDrawable, mat, bucketMask, LODLevel, drawableStat, isShadowPass );
	else
		pDrawable->Draw(mat, bucketMask, LODLevel, drawableStat);
}

#if __BANK
bool s_bShowLODAndCrossfades = false;

void DrawLODLevel(const Matrix34& matrix, s32 lodLevel1, u32 fade1, s32 lodLevel2, u32 fade2)
{
	if (s_bShowLODAndCrossfades)
	{
		char LODLevel[64];

		sprintf(LODLevel, "[(%d,%03d) : (%d,%03d)", lodLevel1, fade1, lodLevel2, fade2);

		grcDebugDraw::Text(matrix.d + Vector3(0.f,0.f,1.3f), CRGBA(255, 255, 128, 255), LODLevel);
	}
}
#endif // __BANK

__forceinline void EntityDraw(	u32 modelInfoIdx,
								const CBaseModelInfo* pModelInfo, 
								rmcDrawable* pDrawable,
#if RAGE_INSTANCED_TECH
								u32 viewportInstancedRenderBit,
#endif
								u32 fadeAlpha, 
								bool forceAlphaForWaterReflection, 
								bool allowAlphaBlend,
								u32 naturalAmb, 
								u32 artificialAmb,
								u32 matID,
								bool forceMaterial,
								const Matrix34 &matrix,
								LODDrawable::crossFadeDistanceIdx fadeDist,
								u32 phaseLODs,
								u32 lastLODIdx,
								bool isInterior,
								bool noZTest,
								bool noColourWrite
#if __BANK
								,entitySelectID entityId
								,u32 contextStats
								,u32 captureStats
#endif // __BANK
)
{
	{
		BeginModelGcmMarker(pModelInfo);

		u32 bucket = DRAWLISTMGR->GetRtBucket();

		Assert(pDrawable == pModelInfo->GetDrawable());
		if (pDrawable BANK_ONLY(&& CEntityDrawDataCommon::SetShaderParamsStatic(entityId, bucket) ))
		{

#if DEBUG_ENTITY_GPU_COST
			START_SPU_STAT_RECORD(captureStats);
			if(captureStats)
			{
				gDrawListMgr->StartDebugEntityTimer();
			}
#endif

#if __PPU
			// Estimate the amount of GCM buffer space we might need, and reserve it once now.
			const unsigned byteCount = 
					sizeof(spuCmd_grcEffect__SetGlobalFloatCommon) + sizeof(Vector4) // SetGlobal_Alpha_EmissiveScale_AmbientScale
				+ 3*sizeof(spuCmd_grcEffect__SetDefaultRenderState)					// SetGlobalDeferredMaterial, twice, and SetStippleAlpha
				+ sizeof(spuCmd_rmcDrawable__Draw)									// drawable draw
				+ 64;																// some slop
			if (GCM_CONTEXT->current + byteCount/4 > GCM_CONTEXT->end)
				GCM_CONTEXT->callback(GCM_CONTEXT, 0);
#endif

			const u32 actualFadeAlpha = AdjustAlpha(fadeAlpha, forceAlphaForWaterReflection);

			const eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
			const u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();

			const bool isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();
			const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
			const bool isHeightmapDrawList = DRAWLISTMGR->IsExecutingRainCollisionMapDrawList();

			bool bDontWriteDepth = pModelInfo->GetDontWriteZBuffer() && (bucket == CRenderer::RB_ALPHA) && !isShadowDrawList;
			const bool setAlpha = (	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
									!isGBufferDrawList ) || 
									(isGBufferDrawList && allowAlphaBlend);
			const bool setStipple = isGBufferDrawList && !allowAlphaBlend;
			const bool crossFadeSetAlpha = IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple) && !isGBufferDrawList;
			const bool crossFadeSetStipple = isGBufferDrawList ;
			const bool isTree = pModelInfo->GetIsTree();
			const bool useTreeRendering = (isTree && (bucket == CRenderer::RB_CUTOUT)) && (CRenderer::TwoPassTreeShadowRenderingEnabled() || isGBufferDrawList);
			const bool forceMaterialID = forceMaterial;

			CShaderLib::SetGlobals((u8)naturalAmb, (u8)artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, noZTest, noColourWrite);
			CShaderLib::SetGlobalDeferredMaterial(matID, forceMaterialID);
			CShaderLib::SetGlobalInInterior(isInterior);

			// detect models without LOD0, as it's fatal further down the pipeline:
			Assertf(pDrawable->GetLodGroup().ContainsLod(LOD_HIGH), "Fatal: Object '%s' has no LOD0, which may crash the game!", pModelInfo->GetModelName());

			u32 LODFlag = (phaseLODs >> DRAWLISTMGR->GetDrawListLODFlagShift()) & LODDrawable::LODFLAG_MASK;
			u32 LODLevel = 0;
			s32 crossFade = -1;
			float dist = LODDrawable::CalcLODDistance(matrix.d);

			if (LODFlag != LODDrawable::LODFLAG_NONE)
			{
				if (LODFlag & LODDrawable::LODFLAG_FORCE_LOD_LEVEL)
				{
					LODLevel = LODFlag & ~LODDrawable::LODFLAG_FORCE_LOD_LEVEL;
				}
				else
				{
					LODDrawable::CalcLODLevelAndCrossfadeForProxyLod(pDrawable, dist, LODFlag, LODLevel, crossFade, fadeDist, lastLODIdx);
				}
			}
			else
			{
				LODDrawable::CalcLODLevelAndCrossfade(pDrawable, dist, LODLevel, crossFade, fadeDist, lastLODIdx ASSERT_ONLY(, pModelInfo) );
			}

			if ((LODFlag != LODDrawable::LODFLAG_NONE || isShadowDrawList || isHeightmapDrawList) && crossFade > -1)
			{
				if (crossFade < 128 && LODLevel+1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel+1))
				{
					LODLevel = LODLevel+1;
				}

				crossFade = -1;
			}
			else if (actualFadeAlpha < 255) // Entity + cross fade fade: we can't deal with this, nuke crossFade
			{
				crossFade = -1;
			}

			DbgSetDrawableModelIdxForSpu((u16)modelInfoIdx);

#if RAGE_INSTANCED_TECH
			grcViewport::SetViewportInstancedRenderBit(viewportInstancedRenderBit);
#endif
			if (HAS_RENDER_MODE_SHADOWS_ONLY(IsRenderingShadows(renderMode) ||) grmModel::GetForceShader())
			{
			#if !RAGE_SUPPORT_TESSELLATION_TECHNIQUES
				pDrawable->DrawNoShaders(matrix, bucketMask, LODLevel);
			#else
				pDrawable->DrawNoShadersTessellationControlled(matrix, bucketMask, LODLevel);
			#endif
			} 
			else 
			{
#if __ASSERT
				// trying to catch this assert in lodgroup.cpp earlier when we know which model it is
				// "Not enough matrices in model.  You probably tried to draw an articulated drawable without a skeleton."
				if(pDrawable)
				{
					rmcLod &lod = pDrawable->GetLodGroup().GetLod(LOD_HIGH);
					for(int i=0; i<lod.GetCount(); i++)
					{
						if(lod.GetModel(i))
						{
							grmModel &model = *lod.GetModel(i);
							int idx = model.GetMatrixIndex();
							Assertf(idx == 0, "%s:Entity(Building) is skinned or parented to more matricies that the 1 it's allowed", pModelInfo->GetModelName());
						}
					}
				}
#endif // __ASSERT

#if __BANK
				const bool bNeedsDepthStateRestore = DebugDeferred::StencilMaskOverlaySelectedBegin(captureStats != 0);
#endif // __BANK

				// check if crossfading - need to render twice if so.
				if ((renderMode==rmStandard || isGBufferDrawList) && crossFade > -1)
				{
					Assert(LODLevel+1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(LODLevel+1));
					Assert(actualFadeAlpha == 255);

					u32 fadeAlphaN1 = crossFade;
					u32 fadeAlphaN2 = 255 - crossFade;

					const bool dontWritedepthN1 = false;
					const bool dontWritedepthN2 = false;

					// cross fade ignore allowAlphaBlend flag...
#if !RSG_ORBIS && !RSG_DURANGO
					s32 previousTechnique = -1;
					bool techOverride = CShaderLib::UsesStippleFades() && isGBufferDrawList;
					if (techOverride)
					{
						// On DX11 we can't use the D3DRS_MULTISAMPLEMASK for fade withough MSAA (as on 360) so for the fade pass.
						// It needs to be manually inserted into the shader itself. For DX11, as we don't want to add it to 
						// all shaders, it's been added to the alphaclip passes and we force the technique here.
						previousTechnique = grmShaderFx::GetForcedTechniqueGroupId();
						if (previousTechnique == DeferredLighting::GetSSATechniqueGroup())
						{
							grmShaderFx::SetForcedTechniqueGroupId(DeferredLighting::GetSSAAlphaClipTechniqueGroup());
						}
						else
						{
							grmShaderFx::SetForcedTechniqueGroupId(DeferredLighting::GetAlphaClipTechniqueGroup());
						}
					}
#endif // !RSG_ORBIS

					if (fadeAlphaN1 > 0)
					{
						CShaderLib::SetGlobals((u8)naturalAmb, (u8)artificialAmb, crossFadeSetAlpha, fadeAlphaN1, crossFadeSetStipple, false, fadeAlphaN1, dontWritedepthN1, false, false);
						SubmitDraw( pDrawable, matrix, bucketMask, LODLevel, (u16)BANK_SWITCH_NT(contextStats,0), useTreeRendering, isShadowDrawList || isHeightmapDrawList);
					}

					if (fadeAlphaN2 > 0)
					{
						CShaderLib::SetGlobals((u8)naturalAmb, (u8)artificialAmb, crossFadeSetAlpha, fadeAlphaN2, crossFadeSetStipple, true, fadeAlphaN1, dontWritedepthN2, false, false);
						SubmitDraw( pDrawable, matrix, bucketMask, LODLevel+1, (u16)BANK_SWITCH_NT(contextStats,0), useTreeRendering, isShadowDrawList || isHeightmapDrawList);
					}

#if !RSG_ORBIS && !RSG_DURANGO
					if (techOverride)
					{
						grmShaderFx::SetForcedTechniqueGroupId(previousTechnique);
					}
#endif // !RSG_ORBIS

#if __BANK
					if (isGBufferDrawList)
					{
						DrawLODLevel(matrix, LODLevel, fadeAlphaN1, LODLevel+1, fadeAlphaN2);
					}
#endif // __BANK
				}
				else
				{
#if !RSG_ORBIS && !RSG_DURANGO
					s32 previousTechnique = -1;
					bool techOverride = isTree && (actualFadeAlpha<255) && (bucket != CRenderer::RB_CUTOUT) && isGBufferDrawList && CShaderLib::UsesStippleFades() ;
					if (techOverride)
					{
						// On DX11 we can't use the D3DRS_MULTISAMPLEMASK for fade withough MSAA (as on 360) so for the fade pass.
						// It needs to be manually inserted into the shader itself. For DX11, as we don't want to add it to 
						// all shaders, it's been added to the alphaclip passes and we force the technique here.
						previousTechnique = grmShaderFx::GetForcedTechniqueGroupId();
						grmShaderFx::SetForcedTechniqueGroupId(DeferredLighting::GetAlphaClipTechniqueGroup());
					}
#endif // !RSG_ORBIS && !RSG_DURANGO

					if( noColourWrite )
					{
						SubmitDraw( pDrawable, matrix, CRenderer::GenerateBucketMask(CRenderer::RB_ALPHA), LODLevel, (u16)BANK_SWITCH_NT(contextStats,0), useTreeRendering, isShadowDrawList || isHeightmapDrawList);
					}
					else
					{
						SubmitDraw( pDrawable, matrix, bucketMask, LODLevel, (u16)BANK_SWITCH_NT(contextStats,0), useTreeRendering, isShadowDrawList || isHeightmapDrawList);
					}

#if !RSG_ORBIS && !RSG_DURANGO
					if (techOverride)
					{
						grmShaderFx::SetForcedTechniqueGroupId(previousTechnique);
					}
#endif // !RSG_ORBIS && !RSG_DURANGO

#if __BANK
					if (isGBufferDrawList)
					{
						DrawLODLevel(matrix, LODLevel, 255, 0, 999);
					}
#endif // __BANK
				}
#if __BANK
				DebugDeferred::StencilMaskOverlaySelectedEnd(bNeedsDepthStateRestore);
#endif // __BANK
			}
			DbgCleanDrawableModelIdxForSpu();

			// restore depth/stencil state
			if (bDontWriteDepth)
			{
				CShaderLib::SetDefaultDepthStencilBlock();
			}

			if (noColourWrite) 
			{
				CShaderLib::ResetBlendState();
			}

			CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
			CShaderLib::ResetAlpha(setAlpha || crossFadeSetAlpha, setStipple || crossFadeSetStipple);

#if DEBUG_ENTITY_GPU_COST
			if(captureStats)
			{
				gDrawListMgr->StopDebugEntityTimer();
			}
			STOP_SPU_STAT_RECORD(captureStats);
#endif
		}//if(pDrawable)...

		EndModelGcmMarker();
	}
}

void CEntityDrawData::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawData) { return; })

#if PROFILE_DATASTRUCTS
	PF_FUNC(EntityDrawDataSimple__Draw);
#endif

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_modelInfoIdx)));
	PrefetchDC(pModelInfo);
	Assert(pModelInfo);
	if( pModelInfo )
	{
		Matrix34	matrix;
		fwTransform *pTransform = reinterpret_cast<fwTransform*>(&m_vector0);
		pTransform->GetNonOrthoMatrixCopy(reinterpret_cast<Mat34V&>(matrix));

#if WATER_REFLECTION_OFFSET_TRICK
		if (DRAWLISTMGR && DRAWLISTMGR->IsExecutingWaterReflectionDrawList())
		{
			CRenderPhaseWaterReflectionInterface::ApplyWaterReflectionOffset(RC_VEC4V(matrix.d), m_modelInfoIdx);
		}
#endif // WATER_REFLECTION_OFFSET_TRICK

		LODDrawable::crossFadeDistanceIdx fadeDist = m_fadeDist ? LODDrawable::CFDIST_ALTERNATE : LODDrawable::CFDIST_MAIN;

		EntityDraw(m_modelInfoIdx,
						pModelInfo,
						m_pDrawable,
#if RAGE_INSTANCED_TECH
				m_viewportInstancedRenderBit,
#endif
						m_fadeAlpha, 
						m_forceAlphaForWaterReflection,
						m_allowAlphaBlend,
						m_naturalAmb, 
						m_artificialAmb,
						m_matID,
						false,
						matrix,
						fadeDist,
						m_phaseLODs,
						m_lastLODIdx
						,m_interior
						,false
						,false
#if __BANK
						,m_commonData.GetEntityID()
						,m_commonData.GetContextStats()
						,m_commonData.GetCaptureStats()
#endif // __BANK	
		);
	}
}

void CEntityDrawDataFm::Init(CEntity *pEntity)
{
#if PROFILE_DATASTRUCTS
	PF_FUNC(EntityDrawData__Init);
	PF_INCREMENT(EntityDrawDataFMCount__Init);
#endif

#if __ASSERT
	fwTransform::Type type = pEntity->GetTransform().GetTypeIdentifier();
	Assertf(type & fwTransform::TYPE_FULL_TRANSFORM_MASK, "Transform error - %s was expected to have a full transform, but has transform type %d",
		pEntity->GetModelName(), type);
#endif

	// Copy the matrix over first
	pEntity->GetTransform().GetNonOrthoMatrixCopy(*reinterpret_cast<Mat34V*>(this));

	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
#if RAGE_INSTANCED_TECH
	m_pack0.m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif
	m_pack1.m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());
	m_pack1.m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_pack1.m_fadeAlpha	= pEntity->GetAlpha();
	m_pack2.m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_pack2.m_allowAlphaBlend = !pEntity->IsBaseFlagSet(fwEntity::HAS_OPAQUE) && pEntity->IsBaseFlagSet(fwEntity::HAS_DECAL);
	m_pack2.m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_pack2.m_matID = pEntity->GetDeferredMaterial();

#if __BANK
#if ENTITYSELECT_ENABLED_BUILD
	m_entityId = CEntityIDHelper::ComputeEntityID(pEntity);
#endif // ENTITYSELECT_ENABLED_BUILD
#if __PS3
	m_pack2.m_contextStats = pEntity->GetDrawableStatsContext();
#endif // __PS3
	m_pack2.m_captureStats = g_PickerManager.GetIndexOfEntityWithinResultsArray(pEntity) > -1;
#endif // __BANK

	m_pack3.m_phaseLODs = pEntity->GetPhaseLODs();
	m_pack3.m_fadeDist = pEntity->GetUseAltFadeDistance();
	m_pack3.m_lastLODIdx = pEntity->GetLastLODIdx();
	m_pack3.m_interior = pEntity->GetInteriorLocation().IsValid();
	m_pack3.m_closeToCam = pEntity->IsExtremelyCloseToCamera();
	m_pack3.m_fpv = pEntity->IsProtectedBaseFlagSet(fwEntity::HAS_FPV);
	m_pack3.m_forceAlpha = pEntity->IsBaseFlagSet(fwEntity::FORCE_ALPHA);
}


// draw this data
void CEntityDrawDataFm::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataFm) { return; })

#if PROFILE_DATASTRUCTS
	PF_FUNC(EntityDrawDataFM__Draw);
#endif

	// unpack fields - grab model info index first, so we cna check if its non-null & prefetch it.
	const u32 modelInfoIdx = m_pack1.m_modelInfoIdx;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIdx)));
	PrefetchDC(pModelInfo);
	Assert(pModelInfo);
	if( pModelInfo )
	{
		// unpack fields
#if RAGE_INSTANCED_TECH
	u32 viewportInstanceRenderBit = m_pack0.m_viewportInstancedRenderBit;
#endif
		u32	modelInfoIdx = m_pack1.m_modelInfoIdx;
		u32 fadeAlpha = m_pack1.m_fadeAlpha;
		u8  naturalAmb = m_pack1.m_naturalAmb;

		u8  artificialAmb = m_pack2.m_artificialAmb;
		u8  matID = m_pack2.m_matID; 
		bool forceMaterial = m_pack3.m_forceAlpha && DRAWLISTMGR->IsExecutingDrawSceneDrawList();
		bool forceAlpha = m_pack2.m_forceAlphaForWaterReflection;
		bool allowAlphaBlend = m_pack2.m_allowAlphaBlend;

		u32 phaseLODs = m_pack3.m_phaseLODs;
		u32 lastLODIdx = m_pack3.m_lastLODIdx;
		bool isInterior = m_pack3.m_interior;
	
		LODDrawable::crossFadeDistanceIdx  fadeDist = m_pack3.m_fadeDist ? LODDrawable::CFDIST_ALTERNATE : LODDrawable::CFDIST_MAIN;

#if WATER_REFLECTION_OFFSET_TRICK
		Matrix34 matrix = *reinterpret_cast<Matrix34*>(this);

		if (DRAWLISTMGR && DRAWLISTMGR->IsExecutingWaterReflectionDrawList())
		{
			CRenderPhaseWaterReflectionInterface::ApplyWaterReflectionOffset(RC_VEC4V(matrix.d), modelInfoIdx);
		}
#else
		const Matrix34& matrix = *reinterpret_cast<Matrix34*>(this);
#endif

		float prevNearClip = 0.0f;
		if(m_pack3.m_closeToCam)
		{
			prevNearClip = grcViewport::GetCurrent()->GetNearClip();
			grcViewport::GetCurrent()->SetNearClip(g_NearClipOverrideValue);
		}
		bool noTest = m_pack3.m_closeToCam;
		if(m_pack3.m_closeToCam)
		{
			noTest = !s_zTestFPV;
		}

		// Use camera at the origin for FPV objects
		grcViewport* pPreFPVViewport = NULL;
		grcViewport fpsVp;
		if(m_pack3.m_fpv && m_pack3.m_closeToCam && (DRAWLISTMGR->IsExecutingGBufDrawList() || DRAWLISTMGR->IsExecutingDrawSceneDrawList()))
		{
			fpsVp = *grcViewport::GetCurrent();
			fpsVp.SetWorldIdentity();
			fpsVp.Perspective(g_FovOverrideValue, fpsVp.GetAspectRatio(), fpsVp.GetNearClip(), fpsVp.GetFarClip());
			Mat34V mat(fpsVp.GetCameraMtx());
			mat.SetCol3(Vec3V(0.0f, 0.0f, 0.0f));
			fpsVp.SetCameraMtx(mat);
			pPreFPVViewport = grcViewport::SetCurrent(&fpsVp);
		}

		bool noColorWrite = false;
		if(m_pack3.m_fpv && m_pack3.m_closeToCam && (DRAWLISTMGR->IsExecutingGBufDrawList()))
		{
			noColorWrite = true;
		}

		EntityDraw(	modelInfoIdx, 
					pModelInfo,
					pModelInfo->GetDrawable(),
#if RAGE_INSTANCED_TECH
					viewportInstanceRenderBit,
#endif
					fadeAlpha, 
					forceAlpha,
					allowAlphaBlend, 
					naturalAmb, 
					artificialAmb,
					matID,
					forceMaterial,
					matrix,
					fadeDist,
					phaseLODs,
					lastLODIdx,
					isInterior,
					noTest,
					noColorWrite
#if __BANK
					,m_entityId
					,PS3_SWITCH(m_pack2.m_contextStats, DRAWLISTMGR->GetDrawableStatContext())
					,m_pack2.m_captureStats
#endif // __BANK
					);

		// Restore FPV viewport if needed
		if(pPreFPVViewport)
		{
			grcViewport::SetCurrent(pPreFPVViewport);
		}

		if(m_pack3.m_closeToCam)
		{
			grcViewport::GetCurrent()->SetNearClip(prevNearClip);
		}
	}
}

void CEntityInstancedDrawData::Init(CEntity* pEntity, grcInstanceBuffer *ib, int lod)
{
#if __BANK
	BANK_ONLY(m_commonData.InitFromEntity(pEntity));
	//NOTE: This currently only works for entity batches. So if we're drawing normal instanced entities, reset common data so entity ID is invalid. (Rather then always selecting 1st entity in batch.)
//	ENTITYSELECT_ONLY(m_commonData = pEntity->GetIsTypeInstanceList() ? m_commonData : CEntityDrawDataCommon());
#endif

	//Per-model data
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());
	Assertf((lod & ~0xFF) == 0, "WARNING: LOD storage is 8 bits. Can not store supplied LOD of %d!", lod);
	m_LODIndex = static_cast<u8>(lod);
	m_matID = pEntity->GetDeferredMaterial();

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif

	//Per-instance data. (Already computed & packed for us.)
#if IB_ALLOW_DL_ALLOCATION
	if(ib && ib->GetAllocationType() == grcInstanceBuffer::ALLOC_DRAWLIST)
	{
		m_HasDLAddress = true;
		reinterpret_cast<DrawListAddress&>(m_IBAddr.InstanceBufferOffset) = static_cast<fwInstanceBuffer *>(ib)->GetDrawListAddress();
	}
	else
#endif // IB_ALLOW_DL_ALLOCATION
	{
		m_HasDLAddress = false;
		m_IBAddr.InstanceBufferPtr = ib;
	}
}

void CEntityInstancedDrawData::Draw()
{
	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(Verifyf(pModelInfo, "NULL model returned from model info idx: %d", m_modelInfoIdx))
	{
		const u32 bucketMask = gDrawListMgr->GetRtBucketMask();
		rmcDrawable *pDrawable = pModelInfo->GetDrawable();
		if(pDrawable && Verifyf(pDrawable->GetLodGroup().ContainsLod(m_LODIndex), "WARNING: Instanced model '%s' doesn't seem to have LOD Level %d.", pModelInfo->GetModelName(), m_LODIndex))
		{
			// detect models without LOD0, as it's fatal further down the pipeline:
			Assertf(pDrawable->GetLodGroup().ContainsLod(0), "Fatal: Object '%s' has no LOD0, which may crash the game!", pModelInfo->GetModelName()); //Necessary??

			grcInstanceBuffer *ib = NULL;
#if IB_ALLOW_DL_ALLOCATION
			if(m_HasDLAddress && !reinterpret_cast<DrawListAddress&>(m_IBAddr.InstanceBufferOffset).IsNULL())
			{
				static const u32 instBuffSize = static_cast<u32>(sizeof(fwInstanceBuffer));
				ib = reinterpret_cast<grcInstanceBuffer *>(gDCBuffer->GetDataBlock(instBuffSize, reinterpret_cast<DrawListAddress&>(m_IBAddr.InstanceBufferOffset)));
			}
			else
#endif // IB_ALLOW_DL_ALLOCATION
			{
				ib = m_IBAddr.InstanceBufferPtr;
			}

			if(ib)
			{
				BANK_ONLY(m_commonData.SetShaderParams(DRAWLISTMGR->GetRtBucket()));
				CShaderLib::SetGlobalDeferredMaterial(m_matID);
#if RAGE_INSTANCED_TECH
				if (grcViewport::GetInstancing())
				{
					grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
					grcViewport::RegenerateInstVPMatrices(Mat44V(V_IDENTITY),(int)ib->GetCount());
				}
#endif
				pDrawable->DrawInstanced(ib, bucketMask, m_LODIndex);
			}
		}
	}
}

void CGrassBatchDrawData::Init(CGrassBatch* pEntity)
{
	BANK_ONLY(m_commonData.InitFromEntity(pEntity));

	//Grass Batch Entity Data
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());
	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_fadeAlpha = pEntity->GetAlpha();
#if !GRASS_BATCH_CS_CULLING
	m_LODIndex = pEntity->ComputeLod();
#endif
	m_matID = pEntity->GetDeferredMaterial();

	m_interior = pEntity->GetInteriorLocation().IsValid();
	
	GRASS_BATCH_CS_CULLING_SWITCH(pEntity->GetCurrentCSParams(m_CSParams), m_vb = pEntity->GetInstanceVertexBuffer());

	m_startInst = 0;
	if(fwDrawData *dh = pEntity->GetDrawHandlerPtr())
	{
		if(CCustomShaderEffectGrass *cse = static_cast<CCustomShaderEffectGrass *>(dh->GetShaderEffect()))
		{
			float rv = cse->GetLodFadeInstRange().GetXf();
	
			if (!DRAWLISTMGR->IsBuildingGBufDrawList())
			{
				float numInst = cse->GetLodFadeInstRange().GetZf();
				rv = Min((numInst - rv) * CGrassBatch::GetShadowLODFactor() + rv, numInst-1.f);
			}
			m_startInst = static_cast<u32>(rv);
		}
	}
}

void CGrassBatchDrawData::Draw()
{
	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(Verifyf(pModelInfo, "NULL model returned from model info idx: %d", m_modelInfoIdx))
	{
		rmcDrawable *pDrawable = pModelInfo->GetDrawable();
		if(pDrawable)
		{
			BANK_ONLY(m_commonData.SetShaderParams(DRAWLISTMGR->GetRtBucket()));
			CShaderLib::SetGlobals(static_cast<u8>(m_naturalAmb), static_cast<u8>(m_artificialAmb), true, m_fadeAlpha, false, false, m_fadeAlpha, false, false, false);
			CShaderLib::SetGlobalDeferredMaterial(m_matID);
			CShaderLib::SetGlobalInInterior(m_interior);
			const u32 bucketMask = gDrawListMgr->GetRtBucketMask();

			GRASS_BATCH_CS_CULLING_ONLY(bool isCSActive = m_CSParams.IsActive());
			GRASS_BATCH_CS_CULLING_ONLY(int lodCount = isCSActive ? LOD_COUNT : 1);
			GRASS_BATCH_CS_CULLING_ONLY(for(int i = 0; i < lodCount; ++i))
			{
				GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(if(m_CSParams.m_AppendBufferMem[i] || !isCSActive))
				GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(if(m_CSParams.m_AppendInstBuffer[i] || !isCSActive))
				{
					int lod = static_cast<int>(GRASS_BATCH_CS_CULLING_SWITCH(m_CSParams.m_LODIdx + i, m_LODIndex));
					CGrassBatchDrawHandler::DrawInstancedGrass(*pDrawable, GRASS_BATCH_CS_CULLING_SWITCH(m_CSParams, m_vb), bucketMask, lod, m_startInst);
				}
			}

			//Free temp buffer
			GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(if(isCSActive) m_CSParams.m_IndirectBufferMem.free());
			
			CShaderLib::SetGlobalAlpha(1.0f);
		}
	}
}

void CGrassBatchDrawData::DispatchComputeShader()
{
#if GRASS_BATCH_CS_CULLING
	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(Verifyf(pModelInfo, "NULL model returned from model info idx: %d", m_modelInfoIdx))
	{
		rmcDrawable *pDrawable = pModelInfo->GetDrawable();
		if(pDrawable)
		{
			//Call to dispatch compute job.
			CGrassBatchDrawHandler::DispatchComputeShader(*pDrawable, m_CSParams);
		}
	}
#endif //GRASS_BATCH_CS_CULLING
}

#if __WIN32PC
void CGrassBatchDrawData::CopyStructureCount()
{
	CGrassBatchDrawHandler::CopyStructureCount(m_CSParams);
}
#endif

#if RAGE_INSTANCED_TECH
#if __BANK // Entity select Id alter sizes between __BANK and not.
CompileTimeAssert(sizeof(CEntityDrawDataFm) == 80);
CompileTimeAssert(sizeof(CEntityDrawData) == 80);
#else
CompileTimeAssert(sizeof(CEntityDrawDataFm) == 64);
CompileTimeAssert(sizeof(CEntityDrawData) == 64);
#endif

#else
CompileTimeAssert(sizeof(CEntityDrawDataFm) == 64);
// CompileTimeAssertSize(CEntityDrawData,0,0);
CompileTimeAssert(sizeof(CEntityDrawData) == (__BANK || __64BIT ? 64 : 48));
#endif

// EJ: Memory Optimization
const crSkeleton *GetSkeletonForDrawIgnoringDrawlist(const CEntity *entity);
void CEntityDrawDataPedPropsBase::Init(CEntity* pEntity, Matrix34* pMatrix, u32 boneCount, u32 matrixSetCount, s32* propDataIndices)
{
	m_pPropGfxData = NULL;

	if (pEntity->GetType() == ENTITY_TYPE_PED)
	{
		Assert(pEntity->GetIsTypePed());		

		// get the mtxs that the props are attached to
		CPed* pPed = reinterpret_cast<CPed*>(pEntity);
		m_pPropGfxData = pPed->GetPedDrawHandler().GetPedPropRenderGfx();
		m_propData = pPed->GetPedDrawHandler().GetPropData();

		CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
		Assert(pModelInfo /*&& pModelInfo->GetIsStreamedGfx()*/);

		s32 boneIdxs[MAX_PROPS_PER_PED];
		ASSERT_ONLY(const u32 numProps =) CPedPropsMgr::GetPropBoneIdxs(boneIdxs, boneCount, pModelInfo->GetVarInfo(), pPed->GetPedDrawHandler().GetPropData(), propDataIndices);
		Assert(numProps == boneCount);

		// Matrix set idx 0: Default skeleton matrices (I.e. first person skeleton if in first person, else third person skeleton), world space
		CPedPropsMgr::GetPropMatrices(pMatrix, pPed->GetSkeleton(), boneIdxs, boneCount, pPed->GetPedDrawHandler().GetPropData(), propDataIndices, false);

		if(matrixSetCount > 1)
		{
			Assert(matrixSetCount == 3);
			Assert(pEntity->IsProtectedBaseFlagSet(fwEntity::HAS_FPV));

			const crSkeleton* pSkeleton = GetSkeletonForDrawIgnoringDrawlist(pPed);

			Matrix34 camToPed;
			fwCameraRelativeExtension* extension = pEntity->GetExtension<fwCameraRelativeExtension>();
			if(extension)
			{
				camToPed.Inverse(RC_MATRIX34(extension->m_cameraOffset));
			}
			else
			{
				camToPed.Identity();
			}

			// Matrix set idx 1: Third person skeleton matrices, world space (For reflections, etc)
			pMatrix += boneCount;
			CPedPropsMgr::GetPropMatrices(pMatrix, pSkeleton, boneIdxs, boneCount, pPed->GetPedDrawHandler().GetPropData(), propDataIndices, false);

			// Matrix set idx 2: First person skeleton matrices, local space (For props in first person)
			pMatrix += boneCount;
			CPedPropsMgr::GetPropMatrices(pMatrix, pPed->GetSkeleton(), boneIdxs, boneCount, pPed->GetPedDrawHandler().GetPropData(), propDataIndices, true);

			// Transform bones into correct view space for first person
			Mat34V camMtxV = RageCameraMtxToGameCameraMtx(dlCmdSetCurrentViewport::GetCurrentViewport_UpdateThread()->GetCameraMtx());
			camMtxV.SetCol3(Vec3V(0.0f, 0.0f, 0.0f));
			Matrix34 propsOffsetMtx;
			propsOffsetMtx.Dot(camToPed, RC_MATRIX34(camMtxV));
			for(u32 i=0; i<boneCount; ++i)
			{
				Matrix34 m;
				m.Dot(pMatrix[i], propsOffsetMtx);
				pMatrix[i] = m;
			}
		}
	} 
	else if (pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		m_pPropGfxData = CGameWorld::FindLocalPlayer()->GetPedDrawHandler().GetPedPropRenderGfx();
	} 
	else 
	{
		Assertf(false, "Unhandled type\n");
	}

	if (m_pPropGfxData)
		m_pPropGfxData->AddRefsToGfx();
}

// initialise this type of drawable data from the entity
void CEntityDrawDataStreamPed::Init(CEntity *pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

#if __ASSERT
	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	// check that it is _really_ a streamed ped
	if (!pModelInfo->GetIsStreamedGfx())
	{
		Assert(false);
	}
#endif // __ASSERT

	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());

	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_bIsInInterior  = pEntity->GetInteriorLocation().IsValid();
	m_fadeAlpha = pEntity->GetAlpha();
	//m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_matID = pEntity->GetDeferredMaterial();
	m_rootMatrix = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
	m_bIsFPV = false;
	if(pEntity->IsProtectedBaseFlagSet(fwEntity::HAS_FPV))
	{
		fwCameraRelativeExtension* extension = pEntity->GetExtension<fwCameraRelativeExtension>();
		if(extension)
		{
			m_rootMatrixFPV = RC_MATRIX34(extension->m_cameraOffset);
			m_bIsFPV = true;
		}
	}

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif

	m_bAddToMotionBlurMask = pEntity->m_nFlags.bAddtoMotionBlurMask;

	if (pEntity->GetType() == ENTITY_TYPE_PED)
	{
		Assert(pEntity->GetIsTypePed());

		CPed* pPed = reinterpret_cast<CPed*>(pEntity);
		m_pGfxData = pPed->GetPedDrawHandler().GetPedRenderGfx();
		Assert(m_pGfxData);
		m_pGfxData->AddRefsToGfx();
		m_bIsFPSSwimming = pPed->GetIsFPSSwimming();
		m_bBlockHideInFirstPersonFlag = pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS);
		m_bSupportsHairTint = pPed->HasHeadBlend();
	} 
	else if (pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		m_pGfxData = CGameWorld::FindLocalPlayer()->GetPedDrawHandler().GetPedRenderGfx();
		Assert(m_pGfxData);
		m_pGfxData->AddRefsToGfx();
		m_bIsFPSSwimming = false;
		m_bBlockHideInFirstPersonFlag = false;
	} 
	else 
	{
		Assertf(false, "Unhandled type\n");
	}
}

void CEntityDrawDataStreamPed::Draw(CAddCompositeSkeletonCommand& skeletonCmd, CEntityDrawDataPedPropsBase* pProps, u8 lodIdx, u32 perComponentWetness)
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataStreamPed) { return; })

	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();
#if __BANK
	u32 bucket = DRAWLISTMGR->GetRtBucket();
#endif
	u16 contextStats = BANK_SWITCH_NT(m_commonData.GetContextStats(), 0);

	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CPedModelInfo* pMI = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(modelId));
	Assert(pMI);
	Assertf(pMI->GetNumRefs() > 0,"Rendering %s but it has no refs. _Very_ dangerous", pMI->GetModelName());
	Assertf(m_pGfxData, "m_pGfxData is NULL for Streamed Ped %s", pMI->GetModelName());

#if __DEV
	if (dlDrawListMgr::StrIndicesValidationEnabled())
	{
		Assertf(m_pGfxData->ValidateStreamingIndexHasBeenCached(), "Stream Index not cached for Streamed Ped %s", pMI->GetModelName());
	}
#endif
	ValidateStreamingIndexCache(modelId, pMI);

	BeginModelGcmMarker(pMI);

	// extract the skeleton data from the drawable for this modelInfo
	rmcDrawable* pDrawable = pMI->GetDrawable();
	Assert(pDrawable);
	if (pDrawable BANK_ONLY(&& m_commonData.SetShaderParams(bucket)))
	{
#if DEBUG_ENTITY_GPU_COST
		START_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StartDebugEntityTimer();
		}
#endif

#if RAGE_INSTANCED_TECH
		grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif

		u32 fadeAlpha = m_fadeAlpha;//AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);
		const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
		const bool isGBufferOrSceneDrawList = isGBufferDrawList || DRAWLISTMGR->IsExecutingDrawSceneDrawList();
		const bool bDontWriteDepth = false;
		const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
								!isGBufferDrawList;
		const bool setStipple = isGBufferDrawList;
		CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, fadeAlpha, setStipple, false, fadeAlpha, bDontWriteDepth, false, false);
		CShaderLib::SetGlobalDeferredMaterial(m_matID);
		CShaderLib::SetGlobalInInterior(m_bIsInInterior);

		// Setup FPV viewport if required
		grcViewport* pPreFPVViewport = NULL;
		Matrix34 rootMatrix;
		grcViewport fpsVp;
		if(m_bIsFPV && isGBufferOrSceneDrawList)
		{
			fpsVp = *grcViewport::GetCurrent();
			fpsVp.SetWorldIdentity();
			Mat34V mat(fpsVp.GetCameraMtx());
			mat.SetCol3(Vec3V(0.0f, 0.0f, 0.0f));
			fpsVp.SetCameraMtx(mat);
			pPreFPVViewport = grcViewport::SetCurrent(&fpsVp);

			Mat34V cam = RageCameraMtxToGameCameraMtx(mat);
			Matrix34 fpvInverse;
			fpvInverse.Inverse(m_rootMatrixFPV);
			rootMatrix.Dot(fpvInverse, RC_MATRIX34(cam));
		}
		else
			rootMatrix = m_rootMatrix;

		const bool isSeeThroughDrawList = DRAWLISTMGR->IsExecutingSeeThroughDrawList();
		if (isSeeThroughDrawList)
		{
			RenderPhaseSeeThrough::SetGlobals();
		}
			
		CCustomShaderEffectBase* pShaderEffect = CCustomShaderEffectDC::GetLatestShader();
		Assert(pShaderEffect);
		Assert(dynamic_cast<CCustomShaderEffectPed*>(pShaderEffect) != NULL);

		DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);
		CCustomShaderEffectPed* pPedShaderEffect = NULL;
		if (pShaderEffect){
			pPedShaderEffect = static_cast<CCustomShaderEffectPed*>(pShaderEffect);
			Assert(pDrawable->GetSkeletonData());

			if (lodIdx >= pMI->GetNumAvailableLODs() && pMI->GetSuperlodType() == SLOD_KEEP_LOWEST)
				lodIdx = pMI->GetNumAvailableLODs() - 1;

			if (lodIdx < pMI->GetNumAvailableLODs())
			{
                CPedVariationStream::RenderPed(pMI, rootMatrix, skeletonCmd, m_pGfxData, pPedShaderEffect, bucketMask, renderMode, contextStats, lodIdx, perComponentWetness, m_bIsFPSSwimming, m_bIsFPV && isGBufferOrSceneDrawList, m_bAddToMotionBlurMask, m_bSupportsHairTint);
			}
			else
			{
				skeletonCmd.Execute(0); // superlod matrix set is copied into component 0, we need to set it as the current one before we render
				CPedVariationPack::RenderSLODPed(rootMatrix, dlCmdAddCompositeSkeleton::GetCurrentMatrixSet(), bucketMask, renderMode, CCustomShaderEffectDC::GetLatestShader(), pMI->GetSuperlodType());
			}
		}

		// draw props for player
		if (pProps)
		{
			// If there's only one matrix set, use the first matrix set (Default skeleton)
			if(pProps->GetNumMatrixSets() == 1)
			{
				CPedPropsMgr::RenderPropsInternal(pMI, rootMatrix, pProps->GetGfxData(), pProps->GetMatrices(0), pProps->GetMatrixCount(), pProps->GetPropData(), pProps->GetPropDataIndices(), bucketMask, renderMode, m_bAddToMotionBlurMask, lodIdx, pPedShaderEffect, m_bBlockHideInFirstPersonFlag);
			}
			// If we have >1 sets, and this is not the g-buffer or scene draw list, use the second matrix set (Third person skeleton, world space)
			else if(!isGBufferOrSceneDrawList)
			{
				CPedPropsMgr::RenderPropsInternal(pMI, rootMatrix, pProps->GetGfxData(), pProps->GetMatrices(1), pProps->GetMatrixCount(), pProps->GetPropData(), pProps->GetPropDataIndices(), bucketMask, renderMode, m_bAddToMotionBlurMask, lodIdx, pPedShaderEffect, m_bBlockHideInFirstPersonFlag);
			}
			// If we have >1 sets, and this is the g-buffer or scene draw list, use the third matrix set (Third person skeleton, adjusted matrices)
			else
			{
				CPedPropsMgr::RenderPropsInternal(pMI, rootMatrix, pProps->GetGfxData(), pProps->GetMatrices(2), pProps->GetMatrixCount(), pProps->GetPropData(), pProps->GetPropDataIndices(), bucketMask, renderMode, m_bAddToMotionBlurMask, lodIdx, pPedShaderEffect, m_bBlockHideInFirstPersonFlag);
			}
		}
		DbgCleanDrawableModelIdxForSpu();
		
		// Restore FPV viewport if needed
		if(m_bIsFPV && isGBufferOrSceneDrawList)
		{
			grcViewport::SetCurrent(pPreFPVViewport);
		}

		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
		CShaderLib::ResetAlpha(setAlpha,setStipple);

#if DEBUG_ENTITY_GPU_COST
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StopDebugEntityTimer();
		}
		STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
#endif
	}//if(pDrawable)...

	EndModelGcmMarker();
}

void CEntityDrawDataPedBIG::Init(CEntity* pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());
	m_fadeAlpha = pEntity->GetAlpha();
	//m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);

	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_matID = pEntity->GetDeferredMaterial();
	m_bIsInInterior = pEntity->GetInteriorLocation().IsValid();

	m_rootMatrix = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif

	Assert(pEntity->GetIsDynamic());

	// we can possible be dealing with a CPed or CDummyPed here

	if (pEntity->GetType() == ENTITY_TYPE_PED)
	{
		CPed* pPed = static_cast<CPed*>(pEntity);			// incorrect assumption (can come from dummyPed and cutsceneObject too)

		// shader hasn't been setup for this vardata 

		CPedModelInfo* pMI = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_modelInfoIdx))));
#if __DEV
		Assert(pMI);
		Assertf(pPed->GetPedDrawHandler().GetVarData().IsShaderSetup(), "adding %s to drawList but doesn't have initialised shader. Error.\n",pMI->GetModelName());
#endif //__DEV

		m_varData = pPed->GetPedDrawHandler().GetVarData();
		m_varData.ResetHasComponentHiLOD();
		u32 drawblIdx = 0;
// TODO: test well .. .find better solution - svetli
		for(int i = 0; i < PV_MAX_COMP; ++i)
		{
			//Cache the hi Detail flag into ped variation data to save a few cycles in the look up.
			ePedVarComp componentId = static_cast<ePedVarComp>(i);
			drawblIdx = m_varData.GetPedCompIdx(componentId);
			if (drawblIdx != PV_NULL_DRAWBL && pMI->GetVarInfo())
			{
				m_varData.SetHasComponentHiLOD((u8)i, pMI->GetVarInfo()->HasComponentFlagsSet(componentId, drawblIdx, PV_FLAG_HIGH_DETAIL));				
			}
			clothVariationData* clothVarData = m_varData.GetClothData(componentId);
			if( clothVarData )
				gDrawListMgr->AddCharClothReference( clothVarData );
		}
		

		// ToDo JW : these are coming in as arguments in the original version...

		bool bHiLOD = (pPed->GetPedDrawHandler().GetVarData().GetIsHighLOD() || !pMI->GetVarInfo()->HasLowLODs());


		grbTargetManager* pBlendShapeManager = pPed->GetTargetManager();
		if(pBlendShapeManager)
		{
			m_bUseBlendShapes = true;
		}

		m_hiLOD = bHiLOD;
		m_bAddToMotionBlurMask = pPed->m_nFlags.bAddtoMotionBlurMask;
		m_bIsFPSSwimming = pPed->GetIsFPSSwimming();

	} 
	else if (pEntity->GetType() == ENTITY_TYPE_OBJECT)
	{
		m_bUseBlendShapes = true;
		m_hiLOD = true;
		m_bAddToMotionBlurMask = false;
		m_bIsFPSSwimming = false;

	}
	else
	{
		Assertf(false, "Unhandled object\n");
	}
}


void CEntityDrawDataPedBIG::Draw(u8 lodIdx, CEntityDrawDataPedPropsBase* pProps)
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataPedBIG) { return; })

	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();
#if __BANK
	u32 bucket = DRAWLISTMGR->GetRtBucket();
#endif

	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"Rendering %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
	ValidateStreamingIndexCache(modelId, pModelInfo);

	BeginModelGcmMarker(pModelInfo);

	grmMatrixSet* ms = dlCmdAddSkeleton::GetCurrentMatrixSet();

	rmcDrawable* pDrawable = pModelInfo->GetDrawable();
	Assert(pDrawable);

	if (!pDrawable BANK_ONLY(|| !m_commonData.SetShaderParams(bucket))){
		return;
	}
#if DEBUG_ENTITY_GPU_COST
	START_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
	if(m_commonData.GetCaptureStats())
	{
		gDrawListMgr->StartDebugEntityTimer();
	}
#endif

	// --- draw the high detail model ---
	DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);

	if (lodIdx >= pModelInfo->GetNumAvailableLODs() && pModelInfo->GetSuperlodType() == SLOD_KEEP_LOWEST)
		lodIdx = pModelInfo->GetNumAvailableLODs() - 1;

	CCustomShaderEffectPed* pPedShaderEffect = NULL;

	const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
	const bool bDontWriteDepth = false;
	const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
							!isGBufferDrawList;
	const bool setStipple = isGBufferDrawList;
	const u32 actualFadeAlpha = m_fadeAlpha;//AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);
	CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false);
	CShaderLib::SetGlobalInInterior(m_bIsInInterior);

	const bool isSeeThroughDrawList = DRAWLISTMGR->IsExecutingSeeThroughDrawList();
	if (isSeeThroughDrawList)
	{
		RenderPhaseSeeThrough::SetGlobals();
	}

#if RAGE_INSTANCED_TECH
	grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif

	if (lodIdx < pModelInfo->GetNumAvailableLODs())
	{
		CShaderLib::SetGlobalDeferredMaterial(m_matID);
		
		CCustomShaderEffectBase* pShaderEffect = CCustomShaderEffectDC::GetLatestShader();
		Assert(pShaderEffect);
		Assert(dynamic_cast<CCustomShaderEffectPed*>(pShaderEffect) != NULL);

		if (pShaderEffect){
			pPedShaderEffect = static_cast<CCustomShaderEffectPed*>(pShaderEffect);
			CPedVariationPack::RenderPed(pModelInfo, m_rootMatrix, ms, m_varData, pPedShaderEffect, bucketMask, lodIdx, renderMode,
				m_bUseBlendShapes, m_bAddToMotionBlurMask, m_bIsFPSSwimming);
		}

		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
	} 
	else 
	{
		CShaderLib::SetGlobalDeferredMaterial(m_matID);
		CPedVariationPack::RenderSLODPed(m_rootMatrix, ms, bucketMask, renderMode, CCustomShaderEffectDC::GetLatestShader(), pModelInfo->GetSuperlodType());
		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
	}
	DbgCleanDrawableModelIdxForSpu();

	CShaderLib::ResetAlpha(setAlpha,setStipple);

	// --- draw the props for this ped ---
	if (pProps && pProps->GetPropData().GetNumActiveProps() == 0)
	{
#if DEBUG_ENTITY_GPU_COST
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StopDebugEntityTimer();
		}
		STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
#endif
		return;
	}

	// extract the skeleton data from the drawable for this modelInfo
	if (lodIdx < pModelInfo->GetNumAvailableLODs())
	{
		DbgSetDrawableModelIdxForSpu(m_modelInfoIdx); // not accurate - it's not prop modelIdx

		CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false); 
		CShaderLib::SetGlobalDeferredMaterial(m_matID);
		CShaderLib::SetGlobalInInterior(m_bIsInInterior);

		// draw props for player
		if (pProps)
		{
			// EJ: Memory Optimization
			CPedPropsMgr::RenderPropsInternal(pModelInfo, m_rootMatrix, pProps->GetGfxData(), pProps->GetMatrices(0), pProps->GetMatrixCount(), pProps->GetPropData(), pProps->GetPropDataIndices(), bucketMask, renderMode, m_bAddToMotionBlurMask, lodIdx, pPedShaderEffect);
		}			

		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
		CShaderLib::ResetAlpha(setAlpha,setStipple);

		DbgCleanDrawableModelIdxForSpu();

#if DEBUG_ENTITY_GPU_COST
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StopDebugEntityTimer();
		}
		STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());


#endif
	}//if(pDrawable)...

	EndModelGcmMarker();
}


CEntityDrawDataBreakableGlass::CEntityDrawDataBreakableGlass(bgDrawable& drawable, const bgDrawableDrawData &bgDrawData, const CEntity *entity) 
: 	m_drawable(&drawable), 
	m_transforms(bgDrawData.GetTransforms()), 
	m_numTransforms(bgDrawData.GetNumTransforms()),
	m_matrix(bgDrawData.GetMatrix()), 
	m_crackTexMatrix(bgDrawData.GetCrackTexMatrix()), 
	m_crackTexOffset(bgDrawData.GetCrackTexOffset()),
	m_pBuffers(bgDrawData.GetBuffers()),
	m_lod(bgDrawData.GetLOD()),
	m_pCrackStarMap(bgDrawData.GetCrackStarMap()),
	m_naturalAmb(entity->GetNaturalAmbientScale()),
	m_artificialAmb(entity->GetArtificialAmbientScale()),
	m_bIsInInterior(entity->GetInteriorLocation().IsValid())
{
	sysMemCpy(m_arrPieceIndexCount, bgDrawData.GetPieceIndexCount(), sizeof(m_arrPieceIndexCount));
	sysMemCpy(m_arrCrackIndexCount, bgDrawData.GetCrackIndexCount(), sizeof(m_arrCrackIndexCount));

	drawable.MarkRenderedThisFrame();
}

void CEntityDrawDataBreakableGlass::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataBreakableGlass) { return; })

	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();

	CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, false, 0, false, false, 0, false, false, false);
	CShaderLib::SetGlobalInInterior(m_bIsInInterior);
	
	m_drawable->Draw(m_transforms, m_numTransforms, m_matrix, m_crackTexMatrix, m_crackTexOffset, m_pBuffers, m_pCrackStarMap, m_lod, m_arrPieceIndexCount, m_arrCrackIndexCount, bucketMask);
}

// initialize this type of drawable data from the entity
void CEntityDrawDataFragType::Init(CEntity *pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());
	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_bIsInterior = pEntity->GetInteriorLocation().IsValid();
	m_fadeAlpha = pEntity->GetAlpha();	
	m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_matID = pEntity->GetDeferredMaterial();
#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif
	m_matrix = MAT34V_TO_MATRIX34(pEntity->GetNonOrthoMatrix());

	m_phaseLODs = pEntity->GetPhaseLODs();
	m_lastLODIdx = pEntity->GetLastLODIdx();

#if ENABLE_FRAG_OPTIMIZATION
	InitExtras(pEntity);
#endif // ENABLE_FRAG_OPTIMIZATION
}

void CEntityDrawDataFragType::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataFragType) { return; })

	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();
#if __BANK
	u32 bucket = DRAWLISTMGR->GetRtBucket();
#endif

	// add a ref to this object 
	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"Rendering %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
	ValidateStreamingIndexCache(modelId, pModelInfo);

	// extract the skeleton data from the drawable for this modelInfo
	if (pModelInfo)
	{
		BeginModelGcmMarker(pModelInfo);

#if DEBUG_ENTITY_GPU_COST
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StartDebugEntityTimer();
		}
#endif
		const fragType* pFragType = pModelInfo->GetFragType();
		Assert(pFragType);

		rmcDrawable* pFragDrawable = pFragType->GetCommonDrawable();
		Assert(pFragDrawable);

		DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);
		if (pFragDrawable BANK_ONLY(&& m_commonData.SetShaderParams(bucket)))
		{
			const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
			const bool bDontWriteDepth = false;
			const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
									!isGBufferDrawList;
			const bool setStipple = isGBufferDrawList;
			const u32 actualFadeAlpha = AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);

			CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false);
			CShaderLib::SetGlobalDeferredMaterial(m_matID);
			CShaderLib::SetGlobalInInterior(m_bIsInterior);

			CBreakable localBreakable;
			g_pBreakable = &localBreakable;
			localBreakable.SetRenderMode(renderMode);
			localBreakable.SetDrawBucketMask(bucketMask);
			localBreakable.SetModelType((int)pModelInfo->GetModelType());
			localBreakable.SetEntityAlpha(m_fadeAlpha);
			localBreakable.SetLODFlags(m_phaseLODs, m_lastLODIdx); 
		#if RSG_PC || __ASSERT
			localBreakable.SetModelInfo(pModelInfo);
		#endif // RSG_PC || __ASSERT
			
#if RAGE_INSTANCED_TECH
			grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif
#if __BANK
			const bool bNeedsDepthStateRestore = DebugDeferred::StencilMaskOverlaySelectedBegin(m_commonData.GetCaptureStats() != 0);
#endif // __BANK

#if ENABLE_FRAG_OPTIMIZATION
			pFragType->Draw(0, m_matrix, gDrawListMgr->GetCalcPosDrawableLOD(), NULL, GetSharedMatrixSetOverride(), NULL );
			DrawExtras();
#else
			pFragType->Draw(0, m_matrix, gDrawListMgr->GetCalcPosDrawableLOD(), NULL, NULL );
#endif

#if __BANK
			DebugDeferred::StencilMaskOverlaySelectedEnd(bNeedsDepthStateRestore);
#endif // __BANK

			g_pBreakable = NULL;
			/* localBreakable.SetEntityAlpha(255);
			localBreakable.SetModelType(-1);
			localBreakable.SetRenderMode(rmNIL);
			localBreakable.SetDrawBucketMask(0);
			localBreakable.SetLODFlags(LODDrawable::LODFLAG_NONE_ALL, 0); */

			CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);
			CShaderLib::ResetAlpha(setAlpha,setStipple);
		}//if(pFragDrawable)...
		DbgCleanDrawableModelIdxForSpu();

#if DEBUG_ENTITY_GPU_COST
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StopDebugEntityTimer();
		}
#endif

		EndModelGcmMarker();
	}//if(pModelInfo)...
}

#if ENABLE_FRAG_OPTIMIZATION
void CEntityDrawDataFragTypeVehicle::InitExtras(CEntity* pEntity)
{
	const CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);

	const fragInst* pFragInst = pVehicle->GetFragInst();
	const grmMatrixSet* pSharedMatrixSet = pFragInst->GetType()->GetSharedMatrixSet();

	const void* id = pFragInst;

	dlSharedDataInfo *pSharedDataInfo = gDCBuffer->LookupSharedDataById(DL_MEMTYPE_MATRIXSET, id);

	m_dataSize = grmMatrixSet::ComputeSize(pSharedMatrixSet->GetMatrixCount());

	if (pSharedDataInfo)
	{
		m_dataAddress = gDCBuffer->LookupSharedData(DL_MEMTYPE_MATRIXSET, *pSharedDataInfo);
	}
	else
	{
		pSharedDataInfo = &gDCBuffer->GetSharedMemData(DL_MEMTYPE_MATRIXSET).AllocateSharedData(id);

#if __PS3
		m_dataSize += 3;
		m_dataSize &= 0xfffffffc;
		grmMatrixSet* pDest = gDCBuffer->AllocateObjectFromPagedMemory<grmMatrixSet>(DPT_TRIPLE_BUFFERED, m_dataSize, false, m_dataAddress);
#else
		grmMatrixSet* pDest = static_cast<grmMatrixSet*>(gDCBuffer->AddDataBlock(NULL, m_dataSize, m_dataAddress));
#endif

		gDCBuffer->AllocateSharedData(DL_MEMTYPE_MATRIXSET, *pSharedDataInfo, m_dataSize, m_dataAddress);

		grmMatrixSet::Create(pDest, pSharedMatrixSet->GetMatrixCount());
	}

	grmMatrixSet* pMatrixSet = GetSharedMatrixSetOverride();

	pMatrixSet->CopySet(*pSharedMatrixSet);

	grmMatrixSet::MatrixType* pMatrices = pMatrixSet->GetMatrices();

	s32 iFirstExtra = VEH_EXTRA_1;

	if(pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
	{
		iFirstExtra = VEH_EXTRA_12;
	}

	for(s32 i = iFirstExtra; i <= VEH_LAST_EXTRA; ++i)
	{
		const eHierarchyId hierarchyId = static_cast<eHierarchyId>(i);

		if(!pVehicle->GetIsExtraOn(hierarchyId))
		{
			const s32 iBoneIndex = pVehicle->GetBoneIndex(hierarchyId);

			if(iBoneIndex != -1)
			{
				pMatrices[iBoneIndex].Zero();

				if(pFragInst)
				{
					const crBoneData* boneData = pFragInst->GetType()->GetSkeletonData().GetBoneData(iBoneIndex);

					const crBoneData* pNext = boneData->GetChild();

					while(pNext)
					{
						pMatrices[pNext->GetIndex()].Zero();
						pNext = pNext->GetNext();
					}
				}
			}
		}
	}
}

void CEntityDrawDataFragTypeVehicle::DrawExtras()
{
	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);

	if(pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
		float dist = -10.0f;
		float distSq = m_matrix.d.Dist2(FRAGMGR->GetInterestMatrix().d);

		if(distSq < square(FRAGTUNE->GetGlobalMaxDrawingDistance()))
		{
			dist = sqrtf(distSq);
		}

		if(dist >= 0.0f)
		{
			u32 bucket = DRAWLISTMGR->GetRtBucket();

			u8 wheelBurstRatios[NUM_VEH_CWHEELS_MAX][2];
			memset(wheelBurstRatios, 0, sizeof(wheelBurstRatios));

			FragDrawCarWheels((CVehicleModelInfo*)pModelInfo, m_matrix, NULL, 0, bucket, false, wheelBurstRatios/*m_wheelBurstRatios*/, dist, 0/*m_flags*/, NULL, false, 1.0f/*m_lodMult*/);
		}
	}
}
#endif // ENABLE_FRAG_OPTIMIZATION

// initialise this type of drawable data from the entity
void CEntityDrawDataSkinned::Init(CEntity *pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

	m_isWeaponModelInfo = 0;
	m_isHD = 0;

	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());
	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_bIsInterior = pEntity->GetInteriorLocation().IsValid();
	m_fadeAlpha = pEntity->GetAlpha();
	m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_matID = pEntity->GetDeferredMaterial();
	m_rootMatrix = MAT34V_TO_MATRIX34(pEntity->GetNonOrthoMatrix());
	m_phaseLODs = pEntity->GetPhaseLODs();
	m_lastLODIdx = pEntity->GetLastLODIdx();
	m_bIsFPV = false;

	if(Unlikely(pEntity->IsProtectedBaseFlagSet(fwEntity::HAS_FPV)))
	{
		fwAttachmentEntityExtension* pExtension = pEntity->GetAttachmentExtension();
		fwEntity* pParent = NULL;

		// Get root matrix for first person skeleton
		Mat34V mat(V_IDENTITY);
		if(pExtension && pExtension->FindParentAndOffsetWithCameraRelativeExtension(pParent, mat))
		{
			Matrix34 rootRelParent;
			rootRelParent.Inverse(RC_MATRIX34(mat));

			fwCameraRelativeExtension* pParentExt = pParent->GetExtension<fwCameraRelativeExtension>();
			FastAssert(pParentExt);

			Matrix34 parentMtx = RC_MATRIX34(pParentExt->m_cameraOffset);
			m_rootMatrixFPV.Dot(parentMtx, rootRelParent);

			// And get root matrix based on the third person skeleton
			pParent = pExtension->GetAttachParent();
			m_rootMatrix.Identity();
			while(pParent)
			{
				// Grab parent skeleton
				const crSkeleton* pSkel = GetSkeletonForDrawIgnoringDrawlist((CEntity*)pParent);

				// Get attach offset matrix
				Matrix34 matOffset;
				matOffset.FromQuaternion(pExtension->GetAttachQuat());
				matOffset.d = pExtension->GetAttachOffset();

				// Get offset relative to parent and multiply
				if(pExtension->GetOtherAttachBone() > -1)
				{
					parentMtx.Dot(matOffset, RCC_MATRIX34(pSkel->GetObjectMtx(pExtension->GetOtherAttachBone())));
					m_rootMatrix.Dot(parentMtx);
				}

				// Next parent...
				pExtension = pParent->GetAttachmentExtension();
				if(!pExtension || !pExtension->GetAttachParent())
				{
					// Multiply by entity matrix on last attachment
					Mat34V parentMatrix = pParent->GetMatrix();
					m_rootMatrix.Dot(RCC_MATRIX34(parentMatrix));
					pParent = NULL;
				}
				else
				{
					pParent = pExtension->GetAttachParent();
				}
			}
			m_bIsFPV = true;
		}
	}

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif

	const CObject* pObject = static_cast<CObject*>(pEntity);
	m_bAddToMotionBlurMask = pObject->m_nFlags.bAddtoMotionBlurMask;
}

void CEntityDrawDataSkinned::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataSkinned) { return; })


	u32 bucket = DRAWLISTMGR->GetRtBucket();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();
	u16 contextStats = BANK_SWITCH_NT(m_commonData.GetContextStats(), 0);

	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);
	ValidateStreamingIndexCache(modelId, pModelInfo);

	BeginModelGcmMarker(pModelInfo);

	// extract the skeleton data from the drawable for this modelInfo
	rmcDrawable* pDrawable;
	if (m_isWeaponModelInfo && m_isHD)
	{
		CWeaponModelInfo *pWInfo = static_cast<CWeaponModelInfo*>(pModelInfo);
		strLocalIndex drawableIndex = strLocalIndex(pWInfo->GetHDDrawableIndex());

		if (drawableIndex != -1)
		{
			pDrawable = g_DrawableStore.Get(drawableIndex);
		}
		else
		{
			pDrawable = pModelInfo->GetDrawable();
		}
	}
	else
	{
		pDrawable = pModelInfo->GetDrawable();
	}

	Assert(pDrawable);
	if (pDrawable BANK_ONLY(&& m_commonData.SetShaderParams(bucket)))
	{
#if DEBUG_ENTITY_GPU_COST
		START_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StartDebugEntityTimer();
		}
#endif

#if RAGE_INSTANCED_TECH
		grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif

		const eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
		const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
		const bool isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();

		const bool isGBufferOrSceneDrawList = isGBufferDrawList || DRAWLISTMGR->IsExecutingDrawSceneDrawList();

		const bool bDontWriteDepth = pModelInfo->GetDontWriteZBuffer() && (bucket == CRenderer::RB_ALPHA) && !isShadowDrawList;

		const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
								!isGBufferDrawList;
		const bool setStipple = isGBufferDrawList;
		const u32 actualFadeAlpha = AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);

		CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false);
		CShaderLib::SetGlobalDeferredMaterial(m_matID, m_isWeaponModelInfo && isGBufferOrSceneDrawList);
		CShaderLib::SetGlobalInInterior(m_bIsInterior);

		// Setup FPV viewport if required
		grcViewport* pPreFPVViewport = NULL;
		Matrix34 rootMatrix;
		grcViewport fpsVp;
		if(m_bIsFPV && isGBufferOrSceneDrawList)
		{
			fpsVp = *grcViewport::GetCurrent();
			fpsVp.SetWorldIdentity();
			Mat34V mat(fpsVp.GetCameraMtx());
			mat.SetCol3(Vec3V(0.0f, 0.0f, 0.0f));
			fpsVp.SetCameraMtx(mat);
			pPreFPVViewport = grcViewport::SetCurrent(&fpsVp);

			Mat34V cam = RageCameraMtxToGameCameraMtx(mat);
			Matrix34 fpvInverse;
			fpvInverse.Inverse(m_rootMatrixFPV);
			rootMatrix.Dot(fpvInverse, RC_MATRIX34(cam));

			// We want to keep the emissive bits on weapon in firts person
			// When the emp as gone off.
			CShaderLib::SetGlobalEmissiveScale(1.0f,true);
		}
		else
			rootMatrix = m_rootMatrix;
		
		grmMatrixSet *ms = dlCmdAddSkeleton::GetCurrentMatrixSet();

		u32 LODFlag = (m_phaseLODs >> DRAWLISTMGR->GetDrawListLODFlagShift()) & LODDrawable::LODFLAG_MASK;
		u32 LODLevel = 0;
		s32 crossFade = -1;
		float dist = LODDrawable::CalcLODDistance(rootMatrix.d);

		if (LODFlag != LODDrawable::LODFLAG_NONE)
		{
			if (LODFlag & LODDrawable::LODFLAG_FORCE_LOD_LEVEL)
			{
				LODLevel = LODFlag & ~LODDrawable::LODFLAG_FORCE_LOD_LEVEL;
			}
			else
			{
				LODDrawable::CalcLODLevelAndCrossfadeForProxyLod(pDrawable, dist, LODFlag, LODLevel, crossFade, LODDrawable::CFDIST_MAIN, m_lastLODIdx);
			}
		}
		else
		{
			LODDrawable::CalcLODLevelAndCrossfade(pDrawable, dist, LODLevel, crossFade, LODDrawable::CFDIST_MAIN, m_lastLODIdx ASSERT_ONLY(, pModelInfo));
		}

		DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);
#if HAS_RENDER_MODE_SHADOWS
		if( IsRenderingShadowsSkinned(renderMode) )
		{
		#if !RAGE_SUPPORT_TESSELLATION_TECHNIQUES
			pDrawable->DrawSkinnedNoShaders(rootMatrix, *ms, bucketMask, LODLevel, rmcDrawableBase::RENDER_SKINNED);
		#else
			pDrawable->DrawSkinnedNoShadersTessellationControlled(rootMatrix, *ms, bucketMask, LODLevel, rmcDrawableBase::RENDER_SKINNED);
		#endif
		}
		else if( IsRenderingShadowsNotSkinned(renderMode) )
		{
			// ToDo JW : why do we need to do this here? (ie. drawing a skinned entity non-skinned?)
			pDrawable->DrawSkinnedNoShaders(rootMatrix, *ms, bucketMask, LODLevel, rmcDrawableBase::RENDER_NONSKINNED);
		}
		else
#endif // HAS_RENDER_MODE_SHADOWS
		{
#if __BANK
			const bool bNeedsDepthStateRestore = DebugDeferred::StencilMaskOverlaySelectedBegin(m_commonData.GetCaptureStats() != 0);
#endif // __BANK

			if( m_bAddToMotionBlurMask )
			{
				CShaderLib::BeginMotionBlurMask();
			}

			pDrawable->DrawSkinned(rootMatrix, *ms, bucketMask, LODLevel, contextStats);

			if( m_bAddToMotionBlurMask )
			{
				CShaderLib::EndMotionBlurMask();
			}

#if __BANK
			DebugDeferred::StencilMaskOverlaySelectedEnd(bNeedsDepthStateRestore);
#endif // __BANK
		}
		DbgCleanDrawableModelIdxForSpu();

		// Restore FPV viewport and emissive if needed
		if(m_bIsFPV && isGBufferOrSceneDrawList)
		{
			grcViewport::SetCurrent(pPreFPVViewport);
			CShaderLib::SetGlobalEmissiveScale(1.0f,false);
		}

		if (bDontWriteDepth)
		{
			CShaderLib::SetDefaultDepthStencilBlock();
		}

		CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT, m_isWeaponModelInfo && isGBufferOrSceneDrawList);
		CShaderLib::ResetAlpha(setAlpha,setStipple);

#if DEBUG_ENTITY_GPU_COST
		if(m_commonData.GetCaptureStats())
		{
			gDrawListMgr->StopDebugEntityTimer();
		}
		STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
#endif
	}//if(pDrawable)...
	EndModelGcmMarker();
}

// initialise this type of drawable data from the entity
void CEntityDrawDataFrag::Init(CEntity *pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

	memset( m_wheelBurstRatios, 0, sizeof(m_wheelBurstRatios) );
	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());

	Assert(pEntity->GetIsDynamic());
	CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pEntity);
	fragInst* pFragInst = pDynamicEntity->GetFragInst();
	Assert(pFragInst);

	m_currentLOD = (s8)pFragInst->GetCurrentPhysicsLOD();

	m_rootMatrix = RCC_MATRIX34(pFragInst->GetMatrix());

	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_bIsInterior = pEntity->GetInteriorLocation().IsValid();

	m_fadeAlpha = pEntity->GetAlpha();
	m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_matID = pEntity->GetDeferredMaterial();
	m_matLocalPlayerEmblem = false;
	m_forceSetMaterial = pEntity->GetIsTypeVehicle() && pEntity->IsBaseFlagSet(fwEntity::FORCE_ALPHA);

	if(pDynamicEntity->GetIsTypeVehicle())
	{
		if(((CVehicle*)pDynamicEntity)->IsLocalPlayerEmblemMaterialGroup())
		{
			m_matLocalPlayerEmblem = true;
		}
	}

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif

#if __DEV
	const fragType* type = pFragInst->GetType();
	Assert(type);
	// make a copy of the child damage array
	Assertf( pFragInst->GetTypePhysics()->GetNumChildren() < MAX_FRAG_CHILDREN, "[ART] : too many frag children (more than 64) in %s\n",CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(m_modelInfoIdx))));
#endif //__DEV

	PopulateBitFields(pFragInst, m_damagedBits, m_destroyedBits);

	m_flags = 0;	// always seems to be init'ed to 0

	m_phaseLODs = pEntity->GetPhaseLODs();
	m_lastLODIdx = pEntity->GetLastLODIdx();

    m_lodMult = 1.f;
	m_lowLodMult = 1.f;
	m_isWrecked = false;
	m_clampedLod = -1;
	if (pDynamicEntity->GetIsTypeVehicle())
	{
		CVehicle* veh = (CVehicle*)pDynamicEntity;
		m_lodMult = veh->GetLodMultiplier();
		m_isWrecked = veh->GetStatus() == STATUS_WRECKED;
		m_clampedLod = veh->GetClampedRenderLod();

		// bump lod distance (but only on for the low lod!) for broken vehicles so we don't see doors/bumpers pop and things like that
		if (!veh->InheritsFromBike())
		{
            bool bumpLod = veh->GetHavePartsBrokenOff();
            if (!bumpLod)
            {
                s32 numDoors = veh->GetNumDoors();
                for (s32 i = 0; i < numDoors; ++i)
                {
                    CCarDoor* door = veh->GetDoor(i);
                    if (door && door->GetDoorRatio() > 0.01f)
                    {
                        bumpLod = true;
                        break;
                    }
                }
            }

            if (bumpLod)
                m_lowLodMult = 2.f;
		}
	}

	// fallen off vehicle mod, need to use the right mod drawable
	m_modAssetIdx = -1;
	m_drawModOnTop = true;
	m_fragMod = true;
	if (pDynamicEntity->GetIsTypeObject())
	{
		CObject* obj = (CObject*)pDynamicEntity;
		if (obj->m_nObjectFlags.bVehiclePart && obj->GetVehicleModSlot() > -1)
		{
			CVehicle* veh = (CVehicle*)obj->GetFragParent();
			if (veh)
			{
				CVehicleStreamRenderGfx* gfx = veh->GetVariationInstance().GetVehicleRenderGfx();
				if (gfx)
				{
					u8 modSlot = (u8)obj->GetVehicleModSlot();
					m_drawModOnTop = false;
					m_modMatrix = m_rootMatrix;

					if (modSlot >= VMT_RENDERABLE + MAX_LINKED_MODS)
					{
						// broken wheel
						m_fragMod = false;
						m_renderGfx = gfx;
						m_renderGfx->AddRefsToGfx();
					}
					else
					{
						// broken frag part
						m_modAssetIdx = gfx->GetFragIndex(modSlot).Get();

						if (m_modAssetIdx != -1 && g_FragmentStore.Get(strLocalIndex(m_modAssetIdx)))
						{
							g_FragmentStore.AddRef(strLocalIndex(m_modAssetIdx), REF_RENDER);
							gDrawListMgr->AddRefCountedModuleIndex(m_modAssetIdx, &g_FragmentStore);
						}

						// render original drawable too if the mod doesn't turn it off
						m_drawModOnTop = veh->GetVariationInstance().ShouldDrawOriginalForSlot(veh, modSlot);

						// the original geometry (e.g a bumper) is part of the main vehicle and offset from the center
						// the mod is a standalone drawable so we need to offset it manually
						// this is pretty crap, we could maybe decouple the vehicle entirely from fallen off mods
						s32 modBoneIdx = veh->GetVariationInstance().GetModBoneIndex(modSlot);
						if (modBoneIdx != -1)
						{
							int boneIdx = veh->GetBoneIndex((eHierarchyId)modBoneIdx);
							if (boneIdx != -1)
							{
								crSkeleton* skel = GetSkeletonFromFragInst(pFragInst);
								if (skel)
								{
									Matrix34 boneMat;
									skel->GetGlobalMtx(boneIdx, RC_MAT34V(boneMat));
									m_modMatrix = boneMat;
								}
							}
						}

						int iTurnOffCount = veh->GetVariationInstance().GetNumBonesToTurnOffForSlot(modSlot);
						if(iTurnOffCount > 0)
						{
							Matrix34 boneMat;
							for (s32 iTurnOff = 0; iTurnOff < iTurnOffCount; ++iTurnOff)
							{
								s32 bone = veh->GetVariationInstance().GetBoneToTurnOffForSlot(modSlot, iTurnOff);
								if (bone == -1)
									continue;

								int boneIdx = veh->GetBoneIndex((eHierarchyId)bone);

								crSkeleton* pSkeleton = GetSkeletonFromFragInst(pFragInst);
								if (boneIdx != -1 && pSkeleton)
								{
									if(pFragInst)
									{
										const crBoneData* boneData = pFragInst->GetType()->GetSkeletonData().GetBoneData(boneIdx);
										Assert(boneData->GetIndex() == boneIdx);

										//only zero out the non-root matrices because we need the matrix cannot be restored.
										if (boneData->GetParent() && boneData->GetParent()->GetIndex() != 0)
										{
											pSkeleton->GetGlobalMtx(boneIdx, RC_MAT34V(boneMat));
											boneMat.Zero3x3();
											pSkeleton->SetGlobalMtx(boneIdx, RCC_MAT34V(boneMat));

											const crBoneData* next = boneData->GetChild();
											while (next)
											{
												pSkeleton->SetGlobalMtx(next->GetIndex(), RCC_MAT34V(boneMat));
												next = next->GetNext();
											}
										}
									}

								}

							}
						}
					}
				}
			}
		}
	}
}

void CEntityDrawDataFrag::Draw(bool damaged)
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataFrag) { return; })

	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucket = DRAWLISTMGR->GetRtBucket();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();

	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"Rendering %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
	ValidateStreamingIndexCache(modelId, pModelInfo);

	// extract the skeleton data from the drawable for this modelInfo
	if (pModelInfo)
	{
		BeginModelGcmMarker(pModelInfo);

		const fragType* overrideFragType = NULL;
		if (m_fragMod && m_modAssetIdx != -1)
		{
			overrideFragType = g_FragmentStore.Get(strLocalIndex(m_modAssetIdx));
		}

		const fragType* type = pModelInfo->GetFragType();
		if (!type)
		{
#if __ASSERT
			Displayf("Error in CEntityDrawDataFrag::Draw() with model %s", pModelInfo->GetModelName());
			Assertf(!pModelInfo->GetHasLoaded(), "Model %s has had its resources unloaded even though its part of a draw list", pModelInfo->GetModelName());
			Assertf(pModelInfo->GetDrawableType() == fwArchetype::DT_FRAGMENT, "The Drawble type must be DT_FRAGMENT, its %d", pModelInfo->GetDrawableType());

			Assertf(type, "Failed to get fragType for Model %s", pModelInfo->GetModelName());
#endif //__ASSERT

			Displayf("frag index %d : model index : %d : model hash : %x",pModelInfo->GetFragmentIndex(), m_modelInfoIdx, pModelInfo->GetModelNameHash());
			Quitf(ERR_GFX_DRAW_DATA,"NULL fragType returned from pModelInfo->GetFragType() - Please save the core dump from this run!!!");
		}

		rmcDrawable* pFragDrawable = type->GetCommonDrawable();
		Assert(pFragDrawable);

		if (pFragDrawable BANK_ONLY(&& m_commonData.SetShaderParams(bucket)))
		{
#if DEBUG_ENTITY_GPU_COST
			START_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
			if(m_commonData.GetCaptureStats())
			{
				gDrawListMgr->StartDebugEntityTimer();
			}
#endif
			DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);

			const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
			const bool isDrawSceneDrawList = DRAWLISTMGR->IsExecutingDrawSceneDrawList();
			const bool bDontWriteDepth = false;
			const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
									!isGBufferDrawList;
			const bool setStipple = isGBufferDrawList;
			const u32 actualFadeAlpha = AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);
			const u32 actualmatid = m_matID | (	m_matLocalPlayerEmblem ? DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER : 0);

			CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false);
			CShaderLib::SetGlobalDeferredMaterial(actualmatid, m_forceSetMaterial && isDrawSceneDrawList); 
			CShaderLib::SetGlobalInInterior(m_bIsInterior);

			CBreakable localBreakable;
			g_pBreakable = &localBreakable;
			localBreakable.SetDrawBucketMask(bucketMask);
			localBreakable.SetRenderMode((eRenderMode)renderMode);
			localBreakable.SetModelType((int)pModelInfo->GetModelType());
			localBreakable.SetEntityAlpha(m_fadeAlpha);
			localBreakable.SetLODFlags(m_phaseLODs, m_lastLODIdx); 
			localBreakable.SetIsWreckedVehicle(m_isWrecked);
			localBreakable.SetClampedLod(m_clampedLod);
			localBreakable.SetLowLodMultiplier(m_lowLodMult);
		#if RSG_PC || __ASSERT
			localBreakable.SetModelInfo(pModelInfo); 
		#endif // RSG_PC || __ASSERT
			Assert(pFragDrawable->GetSkeletonData());
			grmMatrixSet *ms = dlCmdAddSkeleton::GetCurrentMatrixSet();

#if RAGE_INSTANCED_TECH
			grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif

            // calculate dist here only if we'll draw the wheels but not any vehicle part, otherwise don't bother
            // as we'll get back a dist value from FragDraw
			float dist = -10.0f;
			if ((m_flags & CDrawFragDC::CAR_WHEEL) || ((m_flags & CDrawFragDC::CAR_PLUS_WHEELS) && !m_drawModOnTop && !overrideFragType))
			{
				float distSq = m_rootMatrix.d.Dist2(FRAGMGR->GetInterestMatrix().d);
				if(distSq < square(FRAGTUNE->GetGlobalMaxDrawingDistance()))
					dist = sqrtf(distSq);
			}
			else
			{
				// render original drawable from the modelinfo if the mod didn't turn off the bone
				if (m_drawModOnTop)
					FragDraw(m_currentLOD, pModelInfo, ms, m_rootMatrix, m_damagedBits, m_destroyedBits, dist, m_flags, m_lodMult, damaged, type);

				// render the mod drawable
				if (overrideFragType)
					FragDraw(m_currentLOD, pModelInfo, NULL, m_modMatrix, m_damagedBits, m_destroyedBits, dist, m_flags, m_lodMult, damaged, overrideFragType);
			}

			if(dist >= 0.0f && ((m_flags & CDrawFragDC::CAR_PLUS_WHEELS) || (m_flags & CDrawFragDC::CAR_WHEEL)))
			{
#if __BANK
				if (!DRAWLISTMGR->IsExecutingDebugOverlayDrawList() || !grmModel::HasCustomShaderParamsFunc()) // TODO -- support wheels properly in wireframe with custom shader params func
#endif // __BANK
				{
					FragDrawCarWheels((CVehicleModelInfo*)pModelInfo, m_rootMatrix, ms, m_destroyedBits, bucket, (m_flags & (CDrawFragDC::WHEEL_BURST_RATIOS|CDrawFragDC::IS_LOCAL_PLAYER)) != 0, m_wheelBurstRatios, dist, m_flags, m_fragMod ? NULL : m_renderGfx, false, m_lodMult);
				}
			}

			g_pBreakable = NULL;
			/* localBreakable.SetLowLodMultiplier(m_lowLodMult);
			localBreakable.SetClampedLod(-1);
			localBreakable.SetIsWreckedVehicle(false);
			localBreakable.SetEntityAlpha(255);
			localBreakable.SetModelType(-1);
			localBreakable.SetRenderMode(rmNIL);
			localBreakable.SetDrawBucketMask(0);
			localBreakable.SetLODFlags(LODDrawable::LODFLAG_NONE_ALL, 0); */

			CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT, m_forceSetMaterial && isDrawSceneDrawList MSAA_EDGE_PROCESS_FADING_ONLY(, m_matLocalPlayerEmblem));
			CShaderLib::ResetAlpha(setAlpha,setStipple);

			DbgCleanDrawableModelIdxForSpu();

#if DEBUG_ENTITY_GPU_COST
			if(m_commonData.GetCaptureStats())
			{
				gDrawListMgr->StopDebugEntityTimer();
			}
			STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
#endif
		}//if(pFragDrawable)...

		EndModelGcmMarker();
	}//if(pModelInfo)...
}

// initialise the single ped prop draw data from the source entity
void CEntityDrawDataDetachedPedProp::Init(CEntity* pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

	m_mat = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

	m_modelInfoIdx = fwModelId::MI_INVALID;

	m_naturalAmb = pEntity->GetNaturalAmbientScale();
	m_artificialAmb = pEntity->GetArtificialAmbientScale();
	m_bIsInterior = pEntity->GetInteriorLocation().IsValid();

	m_fadeAlpha = pEntity->GetAlpha();	
#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif
	//m_forceAlphaForWaterReflection = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	//m_matID = pEntity->GetDeferredMaterial();

	Assert(pEntity->GetIsTypeObject());
	CObject* pObject = static_cast<CObject*>(pEntity);

	m_modelHash = pObject->uPropData.m_propIdxHash;
	m_TexHash = pObject->uPropData.m_texIdxHash;

    // add refs either to pack dictionaries or streamed assets
	CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(pObject->uPropData.m_parentModelIdx)));
	if (bmi && bmi->GetModelType() == MI_TYPE_PED)
	{
		CPedModelInfo* pmi = (CPedModelInfo*)bmi;
		strLocalIndex DwdIdx = strLocalIndex(pmi->GetPropsFileIndex());
		if (DwdIdx.Get() != -1) 
		{
			// pack ped
            if (Verifyf(g_DwdStore.Get(DwdIdx), "Dwd '%s' for detached object is streamed out!", g_DwdStore.GetName(DwdIdx)))
            {
                g_DwdStore.AddRef(DwdIdx, REF_RENDER);
				gDrawListMgr->AddRefCountedModuleIndex(DwdIdx.Get(), &g_DwdStore);
            }

            strLocalIndex txdIdx = strLocalIndex(g_DwdStore.GetParentTxdForSlot(DwdIdx));
            if (txdIdx != -1)
            {
                if (Verifyf(g_TxdStore.Get(txdIdx), "Txd '%s' for detached object is streamed out!", g_TxdStore.GetName(txdIdx)))
                {
                    g_TxdStore.AddRef(txdIdx, REF_RENDER);
					gDrawListMgr->AddRefCountedModuleIndex(txdIdx.Get(), &g_TxdStore);
                }
            }
		}
		else 
		{
			// streamed ped
            strLocalIndex	dwdIdx = strLocalIndex(g_DwdStore.FindSlot(m_modelHash));
            if (dwdIdx != -1)
            {
                if (Verifyf(g_DwdStore.Get(dwdIdx), "Dwd '%s' for detached object is streamed out!", g_DwdStore.GetName(dwdIdx)))
                {
                    g_DwdStore.AddRef(dwdIdx, REF_RENDER);
					gDrawListMgr->AddRefCountedModuleIndex(dwdIdx.Get(), &g_DwdStore);
                }
            }

            strLocalIndex	txdIdx = strLocalIndex(g_TxdStore.FindSlot(m_TexHash));
            if (txdIdx != -1)
            {
                if (Verifyf(g_TxdStore.Get(txdIdx), "Txd '%s' for detached object is streamed out!", g_TxdStore.GetName(txdIdx)))
                {
                    g_TxdStore.AddRef(txdIdx, REF_RENDER);
					gDrawListMgr->AddRefCountedModuleIndex(txdIdx.Get(), &g_TxdStore);
                }
            }
        }
    }

	m_bAddToMotionBlurMask = pObject->m_nFlags.bAddtoMotionBlurMask;

	CPed* parentPed = static_cast<CPed*>(pObject->GetFragParent());
	if (parentPed && parentPed->GetPedDrawHandler().GetPedPropRenderGfx())
	{
		parentPed->GetPedDrawHandler().GetPedPropRenderGfx()->AddRefsToGfx();
	}

	if (Verifyf(parentPed, "Trying to render a detached ped prop with no parent ped!"))
	{
		m_modelInfoIdx = static_cast<u16>(parentPed->GetModelIndex());
		gDrawListMgr->AddArchetypeReference(parentPed->GetModelIndex());
	}
}

void CEntityDrawDataDetachedPedProp::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataDetachedPedProp) { return; })

	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();
#if __BANK
	u32 bucket = DRAWLISTMGR->GetRtBucket();
	if (!m_commonData.SetShaderParams(bucket)) { return; };
#endif

#if DEBUG_ENTITY_GPU_COST
	START_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
	if(m_commonData.GetCaptureStats())
	{
		gDrawListMgr->StartDebugEntityTimer();
	}
#endif
	const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
	const bool bDontWriteDepth = false;
	const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
							!isGBufferDrawList;
	const bool setStipple = isGBufferDrawList;
	const u32 actualFadeAlpha = m_fadeAlpha;//AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);

	u32 matId = DEFERRED_MATERIAL_PED;
	if (m_bIsInterior) matId |= DEFERRED_MATERIAL_INTERIOR;

	CShaderLib::SetGlobalDeferredMaterial(matId);
	CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false);
	CShaderLib::SetGlobalInInterior(m_bIsInterior);
#if RAGE_INSTANCED_TECH
	grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif

	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"Rendering %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
	ValidateStreamingIndexCache(modelId, pModelInfo);

	BeginModelGcmMarker(pModelInfo);
	DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);
	CPedPropsMgr::RenderPropAtPosn(pModelInfo, m_mat, m_modelHash, m_TexHash, bucketMask, renderMode, m_bAddToMotionBlurMask, NULL, ANCHOR_NONE);
	DbgCleanDrawableModelIdxForSpu();
	EndModelGcmMarker();

#if DEBUG_ENTITY_GPU_COST
	if(m_commonData.GetCaptureStats())
	{
		gDrawListMgr->StopDebugEntityTimer();
	}
	STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
#endif
}

// initialize the vehicle variation draw data from the source entity
void CEntityDrawDataVehicleVar::Init(CEntity* pEntity)
{
#if __BANK
	m_commonData.InitFromEntity(pEntity);
#endif // __BANK

	Assertf(pEntity->GetIsTypeVehicle(), "Entity %s isn't of type vehicle!", pEntity->GetModelName());
	CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
	Assert(pVeh);

	fragInst* pFragInst = pVeh->GetFragInst();
	Assert(pFragInst);
	m_rootMatrix = RCC_MATRIX34(pFragInst->GetMatrix());

	pVeh->GetVehicleDrawHandler().CopyVariationInstance(m_variation);
	m_variation.GetVehicleRenderGfx()->AddRefsToGfx();
	
	Matrix34 boneMat;

	for (s32 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		s32 modBoneIdx = m_variation.GetModBoneIndex((u8)i);
		if (modBoneIdx == -1)
			continue;

		int boneIdx = pVeh->GetBoneIndex((eHierarchyId)modBoneIdx);
		if (boneIdx > -1)
		{
			pVeh->GetGlobalMtx(boneIdx, boneMat);
			m_mat[i] = m_variation.GetVehicleRenderGfx()->GetMatrix((u8)i);
			m_mat[i].d = boneMat.d;
		}
	}

	FastAssert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
	m_modelInfoIdx = static_cast<u16>(pEntity->GetModelIndex());

	m_naturalAmb					= pEntity->GetNaturalAmbientScale();
	m_artificialAmb					= pEntity->GetArtificialAmbientScale();
	m_fadeAlpha						= pEntity->GetAlpha();
	m_bIsInterior					= pEntity->GetInteriorLocation().IsValid();
//	m_forceAlphaForWaterReflection	= CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity);
	m_matID							= pEntity->GetDeferredMaterial();
	m_matLocalPlayerEmblem			= false;
	m_forceSetMaterial				= pEntity->GetIsTypeVehicle() && pEntity->IsBaseFlagSet(fwEntity::FORCE_ALPHA);

	if(pEntity->GetIsTypeVehicle())
	{
		if(((CVehicle*)pEntity)->IsLocalPlayerEmblemMaterialGroup())
		{
			m_matLocalPlayerEmblem = true;
		}
	}

#if RAGE_INSTANCED_TECH
	m_viewportInstancedRenderBit = pEntity->GetViewportInstancedRenderBit();
#endif

	m_bAddToMotionBlurMask = pVeh->m_nFlags.bAddtoMotionBlurMask;

#if __DEV
	const fragType* type = pFragInst->GetType();
	Assert(type);
	// make a copy of the child damage array
	Assertf( pFragInst->GetTypePhysics()->GetNumChildren() < MAX_FRAG_CHILDREN, "[ART] : too many frag children (more than 64) in %s\n",CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(m_modelInfoIdx))));
#endif //__DEV

	PopulateBitFields(pFragInst, m_damagedBits, m_destroyedBits);

	m_hasBurstWheels = false;

    m_lodMult = pVeh->GetLodMultiplier();
	m_isHd = pVeh->GetIsCurrentlyHD();
	m_isWrecked = pVeh->GetStatus() == STATUS_WRECKED;
	m_clampedLod = pVeh->GetClampedRenderLod();

	m_lowLodMult = 1.f;

	// bump lod distance (but only on for the low lod!) for broken vehicles so we don't see doors/bumpers pop and things like that
	if (!pVeh->InheritsFromBike())
	{
		bool bumpLod = pVeh->GetHavePartsBrokenOff();
		if (!bumpLod)
		{
			s32 numDoors = pVeh->GetNumDoors();
			for (s32 i = 0; i < numDoors; ++i)
			{
				CCarDoor* door = pVeh->GetDoor(i);
				if (door && door->GetDoorRatio() > 0.01f)
				{
					bumpLod = true;
					break;
				}
			}
		}

		if (bumpLod)
			m_lowLodMult = 2.f;
	}
}

void CEntityDrawDataVehicleVar::Draw()
{
	DEV_ONLY(if (g_bypass_CEntityDrawDataVehicleVar) { return; })

	eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
	u32 bucket = DRAWLISTMGR->GetRtBucket();
	u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();
	u16 contextStats = BANK_SWITCH_NT(m_commonData.GetContextStats(), 0);

	fwModelId modelId((strLocalIndex(m_modelInfoIdx)));
	CVehicleModelInfo* pModelInfo = static_cast<CVehicleModelInfo*>(CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"Rendering %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
	Assertf(m_variation.GetNumMods(), "Rendering vehicle variation with no active mods!");
	Assertf(m_variation.GetVehicleRenderGfx(), "Rendering vehicle with no render data");
#if __DEV
	if (dlDrawListMgr::StrIndicesValidationEnabled())
	{
		Assertf(m_variation.GetVehicleRenderGfx()->ValidateStreamingIndexHasBeenCached(), "Failed to find cached streaming indices for %s", pModelInfo->GetModelName());
	}
#endif
	ValidateStreamingIndexCache(modelId, pModelInfo);

	BeginModelGcmMarker(pModelInfo);
	DbgSetDrawableModelIdxForSpu(m_modelInfoIdx);

#if DEBUG_ENTITY_GPU_COST
	START_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
	if(m_commonData.GetCaptureStats())
	{
		gDrawListMgr->StartDebugEntityTimer();
	}
#endif
	
	const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
	const bool isDrawSceneDrawList = DRAWLISTMGR->IsExecutingDrawSceneDrawList();
	const bool bDontWriteDepth = false;
	const bool setAlpha =	IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple|rmSimpleDistFade) && 
							!isGBufferDrawList;
	const bool setStipple = isGBufferDrawList;
	const u32 actualFadeAlpha = m_fadeAlpha;//AdjustAlpha(m_fadeAlpha, m_forceAlphaForWaterReflection);
	const u32 actualmatid = m_matID | (	m_matLocalPlayerEmblem ? DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER : 0);

	CShaderLib::SetGlobals(m_naturalAmb, m_artificialAmb, setAlpha, actualFadeAlpha, setStipple, false, actualFadeAlpha, bDontWriteDepth, false, false);
	CShaderLib::SetGlobalDeferredMaterial(actualmatid,m_forceSetMaterial && isDrawSceneDrawList);
	CShaderLib::SetGlobalInInterior(m_bIsInterior);

	CBreakable localBreakable;
	g_pBreakable = &localBreakable;
	localBreakable.SetDrawBucketMask(bucketMask);
	localBreakable.SetRenderMode((eRenderMode)renderMode);
	localBreakable.SetModelType((int)pModelInfo->GetModelType());
	localBreakable.SetEntityAlpha(m_fadeAlpha);
	localBreakable.SetIsVehicleMod(true);
	localBreakable.SetIsHd(m_isHd);
	localBreakable.SetIsWreckedVehicle(m_isWrecked);
	localBreakable.SetClampedLod((s8)m_clampedLod);
	localBreakable.SetLowLodMultiplier(m_lowLodMult);

	CVehicleStreamRenderGfx* renderGfx = m_variation.GetVehicleRenderGfx();
	grmMatrixSet* ms = dlCmdAddSkeleton::GetCurrentMatrixSet();

	CCustomShaderEffectBase* pCSE = CCustomShaderEffectDC::GetLatestShader();
	Assert(pCSE);
	CCustomShaderEffectVehicle* pCSEVehicle = (CCustomShaderEffectVehicle*)pCSE;
	Assert(pCSEVehicle);

#if __PPU && USE_EDGE
	const grcTexture *pVehicleDamageTex = NULL;
	void *pEdgeVehicleDamageTex = NULL;
	bool bIsEdgeVehicle = false;
	
	if(pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
	{
		CCustomShaderEffectVehicle *pShader = pCSEVehicle;
		if(pShader)
		{
			bIsEdgeVehicle = true;
			pVehicleDamageTex = pCSEVehicle->GetDamageTex();
			if(pVehicleDamageTex && pShader->GetEnableDamage())
			{
				const CellGcmTexture* gcmTex = reinterpret_cast<const CellGcmTexture*>(pVehicleDamageTex->GetTexturePtr()); 
				// damage texture format: is it X32_FLOAT linear?
				Assertf(gcmTex->format==(CELL_GCM_TEXTURE_X32_FLOAT|CELL_GCM_TEXTURE_LN), "Vehicle Damage: Unrecognized texture format (not FLOAT32 linear)!");
				// damage texture in main memory?
				Assertf(gcm::IsValidMainOffset(gcmTex->offset)==TRUE, "Vehicle Damage: Texture not in MAIN memory!");

				pEdgeVehicleDamageTex = gcm::MainPtr(gcmTex->offset);
				AlignedAssertf(u32(pEdgeVehicleDamageTex), 16, "pEdgeVehicleDamageTex=0x%X not 16-aligned!", (u32)pEdgeVehicleDamageTex);

				#if SPU_GCM_FIFO
					SPU_COMMAND(GTA4__SetDamageTexture,0xff);
					cmd->damageTexture	= pEdgeVehicleDamageTex;
					cmd->boundRadius	= pShader->GetBoundRadius();
				#endif //SPU_GCM_FIFO...
			}
			else
			{
				#if SPU_GCM_FIFO
					SPU_COMMAND(GTA4__SetDamageTexture,0xff);
					cmd->damageTexture	= NULL;
					cmd->boundRadius	= 0.0f;
				#endif //SPU_GCM_FIFO...
			}
		}// if(pShader)...
	}//if(MI_TYPE_VEHICLE)...
#endif //__PPU && USE_EDGE...

	float dist = m_rootMatrix.d.Dist(camInterface::GetPos());

#if __BANK
	dist = dist / CVehicleFactory::ms_vehicleLODScaling;
#endif //__BANK

#if RAGE_INSTANCED_TECH
	grcViewport::SetViewportInstancedRenderBit(m_viewportInstancedRenderBit);
#endif

	for (s32 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; i++)
	{
		if (!m_variation.IsModVisible((u8)i))
		{
			continue;
		}

		const u8* mods = m_variation.GetMods();
		const u8* linkedMods = m_variation.GetLinkedMods();
		s32 linkedModIndex = (i - VMT_RENDERABLE);
		bool renderableMod = (i < VMT_RENDERABLE);
		s32 modIndex = renderableMod ? i : linkedModIndex;
		u16 modEntry = renderableMod ? mods[modIndex] : linkedMods[modIndex];

		if (modEntry == INVALID_MOD)
		{
			continue;
		}

		fragType* type = renderGfx->GetFrag((u8)i);

		if (!type)
		{
			continue;
		}

		rmcDrawable* damagedDrawable = type->GetDamagedDrawable();
		rmcDrawable* drawable = (damagedDrawable != NULL) ? damagedDrawable : type->GetCommonDrawable();

		if (!drawable)
		{
			continue;
		}

		if (type)
		{
			grmMatrixSet* sharedSet = type->GetSharedMatrixSet();
			fragDrawable* toDraw = type->GetCommonDrawable();

			Assert(toDraw);

#if (__XENON || __PS3)

			rmcLodGroup& LodGroup = toDraw->GetLodGroup();
			if (!LodGroup.ContainsLod(1) && (dist > (LodGroup.GetLodThresh(0) * g_LodScale.GetGlobalScaleForRenderThread() * m_lodMult)))
			{
				continue;
			}

#endif // (__XENON || __PS3)

			if (LODDrawable::IsRenderingLowestCascade())
			{  // don't need these in the lowest cascade
				continue;
			}

			CCustomShaderEffectVehicleType* pModType = renderGfx->GetFragCSEType((u8)i);

			if (type->GetSkinned())
			{
				// set shader variables for vehicle mod:
				static dev_bool bEnableCSE1 = true;
				if(bEnableCSE1)
				{
					// replace main vehicle Type with mod Type:
					CCustomShaderEffectVehicleType* oldType = pModType->SwapType(pCSEVehicle);
					Assert(oldType);

				#if !USE_GPU_VEHICLEDEFORMATION
					#if __PPU && USE_EDGE && SPU_GCM_FIFO
						SPU_COMMAND(GTA4__SetDamageTextureOffset,0);
						cmd->damageTexOffset = m_variation.GetBonePosition((u8)i);
					#endif //SPU_GCM_FIFO...
				#endif
					pCSEVehicle->SetShaderVariables(toDraw, m_variation.GetBonePosition((u8)i));

					oldType->SwapType(pCSEVehicle);
				}

				localBreakable.AddToDrawBucket(*toDraw, m_mat[i], NULL, sharedSet, dist, contextStats, m_lodMult, false);

				#if !USE_GPU_VEHICLEDEFORMATION
					#if __PPU && USE_EDGE && SPU_GCM_FIFO
						SPU_COMMAND(GTA4__SetDamageTextureOffset,0);
						cmd->damageTexOffset = VEC3_ZERO;
					#endif //SPU_GCM_FIFO...
				#endif
			} 
			else 
			{
				if (fragTune::IsInstantiated())
				{
					dist += FRAGTUNE->GetLodDistanceBias();
				}

				//in this case, we have a list of entities to draw...
				for (u64 child = 0; child < type->GetPhysics(0)->GetNumChildren(); ++child)
				{
					fragTypeChild* childType = type->GetPhysics(0)->GetAllChildren()[child];

					// TODO: the destroy/damage bits are wrong, update
					toDraw = NULL;
					bool destroyed = ((m_destroyedBits & ((u64)1<<child)) != 0);
					
					if (!destroyed)
					{
						bool broken = ((m_damagedBits & ((u64)1<<child)) != 0);
						broken &= (childType->GetDamagedEntity() != NULL);
						toDraw = broken ? childType->GetDamagedEntity() : childType->GetUndamagedEntity();
					}

					if (toDraw)
					{
						const rmcLodGroup& lod = toDraw->GetLodGroup();
						if (!fragTune::IsInstantiated() || dist < FRAGTUNE->GetGlobalMaxDrawingDistance() + lod.GetCullRadius())
						{
							// set shader variables for vehicle mod:
							static dev_bool bEnableCSE2 = true;
							if(bEnableCSE2)
							{
								// replace main vehicle Type with mod Type:
								CCustomShaderEffectVehicleType *oldType = pModType->SwapType(pCSEVehicle);
								Assert(oldType);
								pCSEVehicle->SetShaderVariables(toDraw);
								// swap back to old type!
								oldType->SwapType(pCSEVehicle);
							}

							localBreakable.AddToDrawBucket(*toDraw, m_mat[i], ms, sharedSet, dist, contextStats, m_lodMult, false);
#if __PFDRAW
							toDraw->ProfileDraw(NULL, &m_mat[i], lod.ComputeLod(0.f)); // should we use dist here instead of 0.f?
#endif
						}
					}
				}
			}

		} // if(type)...
	}//	for (s32 i = 0; i < VMT_RENDERABLE; i++)...

	// render wheels
	if (m_variation.HasCustomWheels() || m_variation.HasCustomRearWheels())
	{
#if __BANK
		if (!DRAWLISTMGR->IsExecutingDebugOverlayDrawList() || !grmModel::HasCustomShaderParamsFunc()) // TODO -- support wheels properly in wireframe with custom shader params func
#endif // __BANK
		{
			FragDrawCarWheels((CVehicleModelInfo*)pModelInfo, m_rootMatrix, ms, m_destroyedBits, bucket, m_hasBurstWheels, m_wheelBurstRatios, dist, 0, m_variation.GetVehicleRenderGfx(), m_variation.CanHaveRearWheels(), m_lodMult);
		}
	}

#if __PPU && USE_EDGE
	if(bIsEdgeVehicle)
	{
		pEdgeVehicleDamageTex = NULL;
		#if SPU_GCM_FIFO
			SPU_COMMAND(GTA4__SetDamageTexture,0x00);
			cmd->damageTexture	= NULL;
			cmd->boundRadius	= 0.0f;
		#endif //SPU_GCM_FIFO...
	}
#endif //__PPU && USE_EDGE...

	g_pBreakable = NULL;
	/* localBreakable.SetLowLodMultiplier(m_lowLodMult);
	localBreakable.SetClampedLod(-1);
	localBreakable.SetIsWreckedVehicle(false);
	localBreakable.SetIsHd(false);
	localBreakable.SetIsVehicleMod(false);
	localBreakable.SetEntityAlpha(255);
	localBreakable.SetModelType(-1);
	localBreakable.SetRenderMode(rmNIL);
	localBreakable.SetDrawBucketMask(0); */

	CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT, m_forceSetMaterial && isDrawSceneDrawList MSAA_EDGE_PROCESS_FADING_ONLY(, m_matLocalPlayerEmblem));
	CShaderLib::ResetAlpha(setAlpha,setStipple);

	DbgCleanDrawableModelIdxForSpu();

#if DEBUG_ENTITY_GPU_COST
	if(m_commonData.GetCaptureStats())
	{
		gDrawListMgr->StopDebugEntityTimer();
	}
	STOP_SPU_STAT_RECORD(m_commonData.GetCaptureStats());
#endif

	EndModelGcmMarker();
}

