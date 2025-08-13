/////////////////////////////////////////////////////////////////////////////////
// Title	:	PedDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	18/11/2010
//
/////////////////////////////////////////////////////////////////////////////////

#include "renderer/Entities/PedDrawHandler.h"

// Rage Headers

// Framework Headers
#include "fwscene/Scan/Scandebug.h"

// Game Headers
#include "camera/CamInterface.h"
#include "cloth/charactercloth.h"
#include "debug/DebugScene.h"
#include "game/ModelIndices.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/ped.h"
#include "Peds/ped_channel.h"
#include "Peds/PlayerPed.h"
#include "Peds/rendering/PedVariationDS.h"
#include "Peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedVariationPack.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/renderer.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/populationstreaming.h"

#include "renderer/DrawLists/drawList_CopyOffEntityVirtual.h"

RENDER_OPTIMISATIONS()

CPedDrawHandler::~CPedDrawHandler()
{
	if (m_pPedRenderGfx != NULL)
	{
		gPopStreaming.RemoveStreamedPedVariation(m_pPedRenderGfx);
		m_pPedRenderGfx->SetUsed(false);
	}
	m_pPedRenderGfx = NULL;

	if (m_pPedPropRenderGfx)
		m_pPedPropRenderGfx->SetUsed(false);
	m_pPedPropRenderGfx = NULL;
}

const crSkeleton *GetSkeletonForDraw(const CEntity *entity);
u16 CopyOffMatrixSet(const crSkeleton &skel, s32& drawListOffset, eSkelMatrixMode mode, CBaseModelInfo* modelInfo, bool isLodded, u16* skelMap, u8 numSkelMapBones, u8 skelMapComponentId, u16 firstBoneToScale, u16 lastBoneToScale, float fScaleFactor);

// prototype for drawing PedBIG
class CPedBigPrototype : public IDrawListPrototype
{
public:
	virtual void* AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data);
	virtual void  Draw(u32 batchSize, void * data);
	virtual u32   SizeOfElement();

#if CREATE_CLOTH_ONLY_IF_RENDERED
	static bool CreateCharacterCloth(CPedVariationData& varData, CPedVariationInfoCollection* pPedVarInfo, CPed* pPed);
	static void RemoveCharacterCloth(CPedVariationData& varData, CPed* pPed);
#endif

private:
	struct Data
	{
		u32 modelIndex   : 16;
		u32 skeletonSize : 16;
		s32 skeletonOffset;
		DrawListAddress pedShader;
		DrawListAddress entityDraw;
		u8 lodIdx;
		u8 numProps;
	};
private:
	static const u32 sm_ElementSize;
};

#if CREATE_CLOTH_ONLY_IF_RENDERED

bool CPedBigPrototype::CreateCharacterCloth(CPedVariationData& varData, CPedVariationInfoCollection* pPedVarInfo, CPed* pPed)
{
	Assert( pPedVarInfo );
	Assert( pPed );
	bool clothCreated = false;
	for( u32 j = 0; j < PV_MAX_COMP; ++j )
	{
		const ePedVarComp varComp = static_cast<ePedVarComp>(j);
		if( varData.ShouldCreateCloth( varComp ) )
		{					
			s32 drawblId = varData.GetPedCompIdx(varComp);
			Assert( drawblId < pPedVarInfo->GetMaxNumDrawbls(varComp)); 			
			CPVDrawblData* pDrawable = pPedVarInfo->GetCollectionDrawable(j, drawblId);
			Assert(pDrawable);
			clothVariationData* clothVarData = CPedVariationPack::CreateCloth( pDrawable->m_clothData.m_charCloth, varData.GetPedSkeleton(), pDrawable->m_clothData.m_dictSlotId );
			Assert( clothVarData );
			varData.SetClothData( varComp, clothVarData );
			varData.SetFlagClothCreated( varComp );
			clothCreated = true;

			pPed->m_CClothController[j] = clothVarData->GetCloth()->GetClothController();
			Assert( pPed->m_CClothController[j] );

			pPed->CheckComponentCloth();

			if( pPed->m_CClothController[j] && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth ))
			{
				pPed->m_CClothController[j]->SetFlag( characterClothController::enIsProneFlipped, true );
			}
		}
	}

	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth ))
	{
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ForceProneCharacterCloth, false );
	}

	return clothCreated;
}

void CPedBigPrototype::RemoveCharacterCloth(CPedVariationData& varData, CPed* pPed)
{
	varData.RemoveAllCloth(true);
	Assert( pPed );
	pPed->ClearClothController();
}


#endif // CREATE_CLOTH_ONLY_IF_RENDERED


const u32 CPedBigPrototype::sm_ElementSize = sizeof(CPedBigPrototype::Data);

void* CPedBigPrototype::AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data)
{	
	// Prefetch the base model info and the skeleton in the following order, to allow 
	// for the most time for them to get into the cache, as they be used down the line.
	PrefetchDC(entity->GetBaseModelInfo());
	const crSkeleton *skeleton = GetSkeletonForDraw(entity);
	PrefetchDC(skeleton);
	
	Data * dataStruct = reinterpret_cast<Data*>(data);

	dataStruct->modelIndex = (u16)entity->GetModelIndex();

	float fScaleFactor = 1.0f;
	u16 firstBoneToScale = 0;
	u16 lastBoneToScale = 0;

	bool isLodded = false;
	if (entity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(entity);
		CPedModelInfo* pedMi = pPed->GetPedModelInfo();

		dataStruct->lodIdx = pPed->GetModelLodIndex();		// get the model LOD we should be using for this type

		//if (!pedMi->GetVarInfo()->GetIsStreamed())
		{
			//isLodded = dataStruct->lodIdx >= 3;
			isLodded = dataStruct->lodIdx >= pedMi->GetNumAvailableLODs();

#if CREATE_CLOTH_ONLY_IF_RENDERED
			if( DRAWLISTMGR->IsBuildingGBufDrawList() )
			{
				if( dataStruct->lodIdx == 0 )
				{
					CPedDrawHandler* pPedDrawHandler = (CPedDrawHandler*)(const_cast<CEntityDrawHandler*>(drawHandler));
					CreateCharacterCloth( pPedDrawHandler->GetVarData(), pedMi->GetVarInfo(), (CPed*)entity);
				}
				else
				{
					CPedDrawHandler* pPedDrawHandler = (CPedDrawHandler*)(const_cast<CEntityDrawHandler*>(drawHandler));
					RemoveCharacterCloth( pPedDrawHandler->GetVarData(), (CPed*)entity );
				}
			}
#endif
		}

		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseAmbientModelScaling))
		{
			fScaleFactor = pPed->GetRandomNumberInRangeFromSeed(CPed::ms_minAmbientDrawingScaleFactor, CPed::ms_maxAmbientDrawingScaleFactor);
			firstBoneToScale = 0;
			lastBoneToScale = (u16)skeleton->GetBoneCount();
		}
		else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_MakeHeadInvisible) || camInterface::ComputeShouldMakePedHeadInvisible(*pPed))
		{
			s32 headBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
			if (headBoneIndex != BONETAG_INVALID)
			{
				u32 endBoneIndex = pPed->GetSkeleton()->GetTerminatingPartialBone((u32)headBoneIndex);
				if (endBoneIndex != BONETAG_INVALID)
				{
                    fScaleFactor = 0.f;
                    firstBoneToScale = (u16)headBoneIndex;
                    lastBoneToScale = (u16)(endBoneIndex - 1);
				}
			}
		}
	}

	dataStruct->skeletonSize = CopyOffMatrixSet(*skeleton, dataStruct->skeletonOffset, SKEL_MODEL_RELATIVE, entity->GetBaseModelInfo(), isLodded, NULL, 0, 0, firstBoneToScale, lastBoneToScale, fScaleFactor);	
	
// 	const void* id = skeleton;
// 	dlSharedDataInfo *sharedDataInfo = gDCBuffer->LookupSharedDataById(DL_MEMTYPE_MATRIXSET, id);
// 	dataStruct->skeletonOffset = gDCBuffer->LookupSharedData(DL_MEMTYPE_MATRIXSET, *sharedDataInfo);
	
	CCustomShaderEffectBase * shaderEffect = static_cast<CCustomShaderEffectBase*>(drawHandler->GetShaderEffect());
	Assertf(shaderEffect, "Entity %s has an null shader effect", entity->GetModelName());
	if(shaderEffect)
	{
		shaderEffect->AddDataForPrototype(&dataStruct->pedShader);
	}

	// EJ: Memory Optimization
	dataStruct->numProps = entity->GetNumProps();

	switch (dataStruct->numProps)
	{
	case 0:		dataStruct->entityDraw = CDrawPedBIGDC::SharedData(entity);
				break;
	case 1:		dataStruct->entityDraw = CDrawPedBIGDC::SharedDataWithProps<1>(entity);
				break;
	case 2:		dataStruct->entityDraw = CDrawPedBIGDC::SharedDataWithProps<2>(entity);
				break;
	case 3:		dataStruct->entityDraw = CDrawPedBIGDC::SharedDataWithProps<3>(entity);
				break;
	case 4:		dataStruct->entityDraw = CDrawPedBIGDC::SharedDataWithProps<4>(entity);
				break;
	case 5:		dataStruct->entityDraw = CDrawPedBIGDC::SharedDataWithProps<5>(entity);
				break;
	case 6:		dataStruct->entityDraw = CDrawPedBIGDC::SharedDataWithProps<6>(entity);
				break;
	default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
	}

	return (char*)data + sm_ElementSize;
}

void CPedBigPrototype::Draw(u32 batchSize, void * data)
{
	for(u32 batchIndex = 0; batchIndex < batchSize; ++batchIndex)
	{
		Data * dataStruct = reinterpret_cast<Data*>(data);

		dlCmdAddSkeleton::ExecuteCore(dataStruct->skeletonSize, dataStruct->skeletonOffset);

		void * pedShaderData = gDCBuffer->GetDataBlock(sizeof(CCustomShaderEffectPed), dataStruct->pedShader);

		if(pedShaderData)
		{
			CCustomShaderEffectPed * pedShader = static_cast<CCustomShaderEffectPed*>(pedShaderData);
			CCustomShaderEffectDC::SetLatestShader(pedShader);

			// EJ: Memory Optimization
			switch (dataStruct->numProps)
			{
				case 0:		
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIG), dataStruct->entityDraw);
					CEntityDrawDataPedBIG* entityDraw = static_cast<CEntityDrawDataPedBIG*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				case 1:
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<1>), dataStruct->entityDraw);
					CEntityDrawDataPedBIGWithProps<1>* entityDraw = static_cast<CEntityDrawDataPedBIGWithProps<1>*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				case 2:
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<2>), dataStruct->entityDraw);
					CEntityDrawDataPedBIGWithProps<2>* entityDraw = static_cast<CEntityDrawDataPedBIGWithProps<2>*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				case 3:
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<3>), dataStruct->entityDraw);
					CEntityDrawDataPedBIGWithProps<3>* entityDraw = static_cast<CEntityDrawDataPedBIGWithProps<3>*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				case 4:
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<4>), dataStruct->entityDraw);
					CEntityDrawDataPedBIGWithProps<4>* entityDraw = static_cast<CEntityDrawDataPedBIGWithProps<4>*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				case 5:
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<5>), dataStruct->entityDraw);
					CEntityDrawDataPedBIGWithProps<5>* entityDraw = static_cast<CEntityDrawDataPedBIGWithProps<5>*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				case 6:
				{
					void* entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataPedBIGWithProps<6>), dataStruct->entityDraw);
					CEntityDrawDataPedBIGWithProps<6>* entityDraw = static_cast<CEntityDrawDataPedBIGWithProps<6>*>(entityDrawData);
					entityDraw->Draw(dataStruct->lodIdx);
					break;
				}
				default:	Assertf(false, "MAX_PROPS_PER_PED has been increased!");
			}

			data = (char*)data + sm_ElementSize;
		}
		else
		{
			CCustomShaderEffectDC::SetLatestShader(NULL);
		}
	}
	CCustomShaderEffectDC::SetLatestShader(NULL);
}

u32 CPedBigPrototype::SizeOfElement()
{
	return sm_ElementSize;
}

CPedBigPrototype g_PedBigPrototype;

#if __BANK

static bool g_PedDrawHandlerEnableRendering = true;

void CPedDrawHandler::SetEnableRendering(bool bEnable)
{
	g_PedDrawHandlerEnableRendering = bEnable;
}

#endif // __BANK

dlCmdBase* CPedDrawHandler::AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( PedAddToDrawList );

	//	AlphaFade=GetAlphaFade();
	CPed* pPed = static_cast<CPed*>(pEntity);

#if __BANK
	if (ShouldSkipEntity(pPed))
		return NULL;
	else if (!g_PedDrawHandlerEnableRendering && !DRAWLISTMGR->IsBuildingDebugOverlayDrawList())
		return NULL;
#endif // __BANK

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnAddToDrawListOfFocusEntity(), pPed );	

	pedAssert(!CPed::IsSuperLodFromIndex(pPed->GetModelIndex()));

	CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
	Assert(pModelInfo);
	if (!Verifyf(pModelInfo->GetVarInfo(), "No variation info on ped '%s', can't render!", pModelInfo->GetModelName()))
		return NULL;

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	u32 bucket = gDrawListMgr->GetUpdateBucket();
	if( pPed->IsBaseFlagSet(fwEntity::SUPPRESS_ALPHA) == true && bucket == CRenderer::RB_ALPHA && renderMode != rmSpecial )
	{
		return NULL;
	}

	// hi LOD if close enough, or if no low LODs available
	//bool bHiLOD = (GetPedBase()->GetIsHighLOD() || !pModelInfo->GetVarInfo()->HasLowLODs());

	// If there is a reflection being rendered the playerped is put on the renderlist
	// even if his GetIsVisible() is false. For the main render (not the reflection) we
	// jump out here.
	// if we're building the hud drawlist let the ped render regardless of these flags (this will be a mugshot)
	const bool bNotBuildingHUDDrawList = !DRAWLISTMGR->IsBuildingHudDrawList();
	bool bDontRenderPed = (!pPed->GetIsVisible() || pPed->GetPedResetFlag( CPED_RESET_FLAG_DontRenderThisFrame )) && bNotBuildingHUDDrawList;
#if __BANK
	bDontRenderPed |= !g_DrawPeds;
#endif // __BANK

	if (bDontRenderPed)
		return NULL;

	bool isStreamedGfx = pModelInfo->GetIsStreamedGfx();

	IDrawListPrototype * prototype = NULL;

	if(g_prototype_batch)
	{
		if(!isStreamedGfx)
		{
			prototype = &g_PedBigPrototype;
		}
	}

	if(pPed->GetUseOcclusionQuery())
	{
		unsigned int query = GetOcclusionQueryId();
			
		if(query == 0)
		{
			query = OcclusionQueries::OQAllocate();
			SetOcclusionQueryId(query);
		}
			
		if( query )
		{
			Vec3V min,max;
			CEntity *boundSource = pPed->GetOQBoundingBox(min,max);
			Mat34V matrix = boundSource->GetMatrix();
			OcclusionQueries::OQSetBoundingBox(query, min, max, matrix);
		}
	}

	if (!DRAWLISTMGR->IsBuildingShadowDrawList())
	{
		bool setupLight = pParams ? static_cast<const CDrawDataAddParams*>(pParams)->m_SetupLights : false;
		CEntityDrawHandler::SetupLightsAndGlobalInInteriorFlag(pPed, rmStandard, ~0U, setupLight);
	}

	CDrawListPrototypeManager::SetPrototype(prototype);

	// if there's a prototype for this ped and we're building the hud drawlist (ped headshot, pause menu ped, etc.), 
	// don't go through the prototype manager and issue a full draw command instead.
	// tbr: the prototype manager cannot correctly handle a ped being rendered in the hud renderphase (data seems to end up in the gbuffer phase)
	if(prototype && bNotBuildingHUDDrawList)
	{
		CDrawListPrototypeManager::AddData(pPed, this);
	}
	else
	{
#if __PS3
		BANK_ONLY(extern bool IsEdgeNoPixelTestDisabledForPeds());
		extern bool SetEdgeNoPixelTestEnabled(bool bEnabled);
		bool bWasEdgeNoPixelTestEnabled = false;

		// we only want to disable the no-pixel test if we're building a drawlist with the test potentially disabled
		const bool bIsBuildingDrawListWithNoPixelTestPotentiallyDisabled =
			DRAWLISTMGR->IsBuildingGBufDrawList() ||
			DRAWLISTMGR->IsBuildingShadowDrawList() || // disabling no-pixel test for ped shadows (see BS#571750)
			DRAWLISTMGR->IsBuildingMirrorReflectionDrawList();

		if (bIsBuildingDrawListWithNoPixelTestPotentiallyDisabled BANK_ONLY(&& IsEdgeNoPixelTestDisabledForPeds()))
		{
			bWasEdgeNoPixelTestEnabled = SetEdgeNoPixelTestEnabled(false);
		}
#endif // __PS3

		if (isStreamedGfx && pPed->IsPlayer())
		{
			CPedStreamRenderGfx* pGfxData = GetPedRenderGfx();

			if (pGfxData){
				if (pPed->GetVehiclePedInside() && static_cast<CVehicle*>(pPed->GetVehiclePedInside())->GetVehicleType() == VEHICLE_TYPE_BOAT){
					pGfxData->m_bDisableBulkyItems = true;
				} else {
					pGfxData->m_bDisableBulkyItems = false;
				}

				if (pGfxData->m_pShaderEffect){
					pGfxData->m_pShaderEffect->AddToDrawList(pPed->GetModelIndex(), false);
				} 
#if DRAWABLE_STATS
				DLC(CSetDrawableStatContext, (pPed->GetDrawableStatsContext()));
#endif

				if (pPed->HasHeadBlend() && GetPedPropRenderGfx())
				{
					CPedHeadBlendData* blendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
                    GetPedPropRenderGfx()->m_blendIndex = MESHBLENDMANAGER.GetIndexFromHandle(blendData->m_blendHandle);
				}

				DLC(CDrawStreamPedDC, (pPed));
			}
#if __BANK
			else
			{
				spdAABB aabb;
				pEntity->GetAABB(aabb);
				ScanDebugAddPedMissingGfx(pEntity->GetArchetype()->GetModelNameHash(), pEntity, aabb, false);
			}
#endif
		} 
		else 
		{
			if (isStreamedGfx){
				if (GetPedRenderGfx() && GetPedRenderGfx()->m_pShaderEffect){
					GetPedRenderGfx()->m_pShaderEffect->AddToDrawList(pPed->GetModelIndex(), false);
#if DRAWABLE_STATS
					DLC(CSetDrawableStatContext, (pPed->GetDrawableStatsContext()));
#endif

					if (pPed->HasHeadBlend() && GetPedPropRenderGfx())
					{
						CPedHeadBlendData* blendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
						GetPedPropRenderGfx()->m_blendIndex = MESHBLENDMANAGER.GetIndexFromHandle(blendData->m_blendHandle);
					}

					DLC(CDrawStreamPedDC, (pPed));
				}
#if __BANK
				else
				{
					spdAABB aabb;
					pEntity->GetAABB(aabb);
					ScanDebugAddPedMissingGfx(pEntity->GetArchetype()->GetModelNameHash(), pEntity, aabb, GetPedRenderGfx() != 0);
				}
#endif

			} else {

#if CREATE_CLOTH_ONLY_IF_RENDERED
				CPedBigPrototype::CreateCharacterCloth(m_varData, pModelInfo->GetVarInfo(), pPed);
#endif
				if (GetShaderEffect()){
					GetShaderEffect()->AddToDrawList(pPed->GetModelIndex(), false);
#if DRAWABLE_STATS
					DLC(CSetDrawableStatContext, (pPed->GetDrawableStatsContext()));
#endif
					DLC(CDrawPedBIGDC, (pPed));
				}
			}
		}

#if __PS3
		if (bWasEdgeNoPixelTestEnabled)
		{
			SetEdgeNoPixelTestEnabled(true);
		}
#endif // __PS3
	}

	// nothing needs the return value for this type atm
	return NULL;
}

void CPedDrawHandler::SetPedPropRenderGfx(CPedPropRenderGfx* pGfxData)
{
	if (m_pPedPropRenderGfx != NULL)
		m_pPedPropRenderGfx->SetUsed(false);

	m_pPedPropRenderGfx = pGfxData;
}

void CPedDrawHandler::SetPedRenderGfx(CPedStreamRenderGfx* pGfxData)
{
	if (m_pPedRenderGfx != NULL)
	{
		gPopStreaming.RemoveStreamedPedVariation(m_pPedRenderGfx);
		m_pPedRenderGfx->SetUsed(false);
	}

	m_pPedRenderGfx = pGfxData;
	
	if (m_pPedRenderGfx)
		gPopStreaming.AddStreamedPedVariation(m_pPedRenderGfx);
}

void CPedDrawHandler::UpdateCachedWetnessFlags(CPed* pPed)
{
	// Cache the ped's per-component wetness flags for use at render time
	CompileTimeAssert(PV_MAX_COMP <= 12); // This code assumes a max of 12 components to pack into 24 bits
	u32 wetness = 0;
	CPedVariationInfoCollection& varInfo = *pPed->GetPedModelInfo()->GetVarInfo();
	for(int i=0; i<PV_MAX_COMP; ++i)
	{
		u32 drawable = m_varData.GetPedCompIdx((ePedVarComp)i);
		u32 flags = varInfo.LookupCompInfoFlags(i, drawable);
		if(flags & PV_FLAG_WET_MORE_WET)
			wetness |= 1 << (i*2);
		else if(flags & PV_FLAG_WET_LESS_WET)
			wetness |= 2 << (i*2);
	}
	m_perComponentWetnessFlags = wetness;
}
