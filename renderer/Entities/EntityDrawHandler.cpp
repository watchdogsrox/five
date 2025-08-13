/////////////////////////////////////////////////////////////////////////////////
// Title	:	EntityDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	18/11/2010
//
/////////////////////////////////////////////////////////////////////////////////


#include "renderer/Entities/EntityDrawHandler.h"

// Rage Headers
#include "crskeleton/skeletondata.h"
#include "grcore/grcorespu.h"

// Framework Headers
#if __BANK
#include "fwdebug/picker.h"
#endif // __BANK

// Game Headers
#include "cloth/charactercloth.h"
#include "cloth/ClothMgr.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/SceneGeometryCapture.h"
#include "physics/physics.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/Lights/lights.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/Renderer.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "scene/Entity.h"
#include "scene/Lod/LodDrawable.h"
#include "scene/Lod/LodMgr.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "shaders/CustomShaderEffectBase.h"
#include "shaders/CustomShaderEffectGrass.h"
#include "shaders/CustomShaderEffectTint.h"
#include "shaders/CustomShaderEffectTree.h"
#include "shaders/CustomShaderEffectCable.h"
#include "streaming/streaming.h"
#include "renderer/Entities/InstancedEntityRenderer.h"

RENDER_OPTIMISATIONS()
//
//
//
//
CEntityDrawHandler::CEntityDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable)
{
	m_pDrawable			= pDrawable;
	m_pShaderEffect		= NULL;
	m_pActiveListLink	= NULL;

	// only add buildings and dummies to entity rendered list. Don't want peds, vehicles
	// or objects in there
	if(pEntity && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeDummyObject()))
		AddEntityToRenderedList(pEntity);

	if(pDrawable)
	{
		// Setup lod flags
		const rmcLodGroup &lodGroup = pDrawable->GetLodGroup();

		m_phaseLODs =
			(LODDrawable::CalculateDrawableLODFlags(lodGroup, CRenderer::RB_MODEL_SHADOW)		<< LODDrawable::LODFLAG_SHIFT_SHADOW	) |
			(LODDrawable::CalculateDrawableLODFlags(lodGroup, CRenderer::RB_MODEL_REFLECTION)	<< LODDrawable::LODFLAG_SHIFT_REFLECTION) |
			(LODDrawable::CalculateDrawableLODFlags(lodGroup, CRenderer::RB_MODEL_WATER)		<< LODDrawable::LODFLAG_SHIFT_WATER		) |
			(LODDrawable::CalculateDrawableLODFlags(lodGroup, CRenderer::RB_MODEL_MIRROR)		<< LODDrawable::LODFLAG_SHIFT_MIRROR	) |
			(LODDrawable::LODFLAG_NONE															<< LODDrawable::LODFLAG_SHIFT_DEFAULT	) ;

		m_lastLODIdx = 0;

		for (int i = LOD_COUNT - 1; i > 0; i--)
		{
			if (lodGroup.ContainsLod(i))
			{
				m_lastLODIdx = i;
				break;
			}
		}

		m_hasBones = pDrawable->GetSkeletonData() && (pDrawable->GetSkeletonData()->GetNumBones() > 1) ? 1 : 0;

		if(Verifyf(pEntity, "CEntityDrawHandler called with NULL entity"))
		{
			CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
			Assert(pModelInfo);
			
#if __ASSERT
			// cloth and glass panes can have no LODs ..
			const fragType* pFragType = pModelInfo->GetFragType();
			const bool bHasEnvCloths = pFragType ? (pFragType->GetNumEnvCloths() > 0) : false;
			const bool bHasGlassPane = pFragType ? (pFragType->GetNumGlassPaneModelInfos() > 0) : false;

			for (int i = m_lastLODIdx; i >= 0; i--) // make sure all the higher LODs exist
			{
				Assertf(lodGroup.ContainsLod(i) || ((bHasEnvCloths || bHasGlassPane) && i == 0), "%s: last LOD index is %d, but LOD index %d does not exist (model type = %d, entity type = %d)", pEntity->GetModelName(), m_lastLODIdx, i, pModelInfo->GetModelType(), pEntity->GetType());
			}
#endif // __ASSERT

			// [HACK GTAV]
			const u32 uModelNameHash = pModelInfo->GetModelNameHash();
			if(	(uModelNameHash==MID_P_PARACHUTE1_S)				||
				(uModelNameHash==MID_P_PARACHUTE1_SP_S)				||
				(uModelNameHash==MID_P_PARACHUTE1_MP_S)				|| 
				(uModelNameHash==MID_PIL_P_PARA_PILOT_SP_S)			||
				(uModelNameHash==MID_LTS_P_PARA_PILOT2_SP_S)		||
				(uModelNameHash==MID_SR_PROP_SPECRACES_PARA_S_01)	||
				(uModelNameHash==MID_GR_PROP_GR_PARA_S_01)			||
				(uModelNameHash==MID_XM_PROP_X17_PARA_SP_S)			||
				(uModelNameHash==MID_TR_PROP_TR_PARA_SP_S_01A)		||
				(uModelNameHash==MID_REH_PROP_REH_PARA_SP_S_01A)	)
			{
				// BS#842523: special case to force lod=1 for parachute shadow:
				u32 shadowLODFlag = (m_phaseLODs >> LODDrawable::LODFLAG_SHIFT_SHADOW) & LODDrawable::LODFLAG_MASK;

				if (shadowLODFlag == LODDrawable::LODFLAG_NONE)
				{
					shadowLODFlag = LODDrawable::LODFLAG_FORCE_LOD_LEVEL | LOD_MED;
					m_phaseLODs &= ~(LODDrawable::LODFLAG_MASK	<< LODDrawable::LODFLAG_SHIFT_SHADOW);
					m_phaseLODs |= (shadowLODFlag				<< LODDrawable::LODFLAG_SHIFT_SHADOW);
				}
			}
		}
	}
}

CEntityDrawHandler::~CEntityDrawHandler()
{
	u32 query = GetOcclusionQueryId();
	if(query)
	{
		OcclusionQueries::OQFree(query);
		SetOcclusionQueryId(0);
	}

	RemoveEntityFromRenderedList();
	RemoveDrawable();
}

//
// name:		CEntityDrawComponent::AddEntityToRenderedList
// description:	Add the entity to the rendered list
void CEntityDrawHandler::AddEntityToRenderedList(CEntity* pEntity)
{
	m_pActiveListLink = g_SceneStreamerMgr.GetStreamingLists().AddActive(pEntity);
#if STREAMING_VISUALIZE
//	g_SceneStreamerMgr.GetStreamingLists().AddActiveForStreamingVisualize(pEntity); 
#endif // STREAMING_VISUALIZE
}

//
// name:		CEntityDrawComponent::RemoveEntityFromRenderedList
// description:	Remove the entity from the rendered list
void CEntityDrawHandler::RemoveEntityFromRenderedList()
{
	if (m_pActiveListLink)
	{
#if __ASSERT
		fwEntity *entity = m_pActiveListLink->item.GetEntity();
#endif // __ASSERT

#if STREAMING_VISUALIZE
//		g_SceneStreamerMgr.GetStreamingLists().RemoveActiveForStreamingVisualize(entity); 
#endif // STREAMING_VISUALIZE

		// Make sure the strIndex hasn't changed throughout the lifetime of this object.
#if __ASSERT
		Assert(entity);
		Assert(entity->GetDrawHandlerPtr() == this);

		u32 objIndex = entity->GetModelIndex();
		strIndex archetypeIndex = fwArchetypeManager::GetStreamingModule()->GetStreamingIndex(strLocalIndex(objIndex));
		Assertf(archetypeIndex == m_pActiveListLink->item.GetArchetypeIndex(), "The archetype index for %s has changed during its lifetime from %d to %d. Krehan and Landry have brought shame over their names.", entity->GetModelName(), archetypeIndex.Get(), m_pActiveListLink->item.GetArchetypeIndex().Get());
#endif // __ASSERT
		g_SceneStreamerMgr.GetStreamingLists().GetActiveEntityList().RemoveEntity(m_pActiveListLink);
		m_pActiveListLink = NULL;
	}
}

//
//
//
//


fwCustomShaderEffect* CEntityDrawHandler::ShaderEffectCreateInstance(CBaseModelInfo *pModelInfo, CEntity *pEntity)
{
	static	sysSpinLockToken	sSpinlock; //has to be static or global to be shared by threads
	fwCustomShaderEffect *ret=NULL;
	SYS_SPINLOCK_ENTER(sSpinlock);

	CCustomShaderEffectBaseType *pMasterShaderEffectType = pModelInfo->GetCustomShaderEffectType();
	if(pMasterShaderEffectType)
	{
		this->m_pShaderEffect = pMasterShaderEffectType->CreateInstance(pEntity);
		Assert(this->m_pShaderEffect);

		ret=this->m_pShaderEffect;
	}
	SYS_SPINLOCK_EXIT(sSpinlock);

	return(ret);
}

//
// called just before Draw():
//
void CEntityDrawHandler::BeforeAddToDrawList(fwEntity *pEntity, u32 modelIndex, u32 renderMode, u32 bucket, u32 bucketMask, fwDrawDataAddParams* pBaseParams, bool bAlwaysSetCSE)
{
	DL_PF_FUNC( TotalBeforeAddToDrawList );

	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	CDrawDataAddParams *pGTAParams = (CDrawDataAddParams *) pBaseParams;
#if __DEV
	Assert(pGTAParams->m_Magic == CDrawDataAddParams::MAGIC);	// Someone didn't create the subclassed version of fwDrawDataAddParams!
#endif // __DEV

	CEntity *pGTAEntity = (CEntity *) pEntity;

#if DRAWABLESPU_STATS || DRAWABLE_STATS
	DLC(CSetDrawableStatContext, (pGTAEntity->GetDrawableStatsContext()));
#endif

	bool runCSE = IsRenderingModes((eRenderMode)renderMode, rmStandard|rmSimpleDistFade); 
	runCSE |= (renderMode == rmSpecial && pEntity->IsBaseFlagSet(fwEntity::SUPPRESS_ALPHA) && bucket == CRenderer::RB_ALPHA );
	// Don't run CSE during the water pass.
	runCSE &= (bucket != CRenderer::RB_WATER);

	if(m_pShaderEffect && runCSE)
	{
		// no standard CBs when local shaders variables may be present
		CCustomShaderEffectBase* pCse = static_cast<CCustomShaderEffectBase*>(this->m_pShaderEffect);
		pCse->AddToDrawList(modelIndex, true);
	}
	else if (bAlwaysSetCSE)
	{
		DLC(CCustomShaderEffectDC, (NULL));			// reset latest shader effect command
	}

	if (!DRAWLISTMGR->IsBuildingShadowDrawList())
	{
		// Only use local lights if this is not a cable (cables are not affected by local lights)
		bool bUseLightsInArea = pGTAParams->m_SetupLights;
		
		if(m_pShaderEffect && (m_pShaderEffect->GetType() == CSE_CABLE))
		{
			bool bLitCable = ((CCustomShaderEffectCable*)m_pShaderEffect)->IsLitCable() BANK_ONLY(&& DebugLights::m_cablesCanUseLocalLights);
			if(!bLitCable)
			{
				bUseLightsInArea = false;
			}
		}

		// If we're supposed to set up the closest lights for the shader, do so.
		SetupLightsAndGlobalInInteriorFlag(pGTAEntity, renderMode, bucketMask, bUseLightsInArea);
	}
}

//
//
//
//
void CEntityDrawHandler::AfterAddToDrawList(fwEntity *pEntity, u32 renderMode, fwDrawDataAddParams* pBaseParams)
{
	DL_PF_FUNC( TotalAfterAddToDrawList );

	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if (!DRAWLISTMGR->IsBuildingShadowDrawList())
	{
		CEntity *pGTAEntity = (CEntity *) pEntity;
		CDrawDataAddParams *pGTAParams = (CDrawDataAddParams *) pBaseParams;

		// Only use local lights if this is not a cable (cables are not affected by local lights)
		bool bUseLightsInArea = pGTAParams->m_SetupLights;

		if(m_pShaderEffect && (m_pShaderEffect->GetType() == CSE_CABLE))
		{
			bool bLitCable = ((CCustomShaderEffectCable*)m_pShaderEffect)->IsLitCable() BANK_ONLY(&& DebugLights::m_cablesCanUseLocalLights);
			if(!bLitCable)
			{
				bUseLightsInArea = false;
			}
		}

		ResetLightOverride(pGTAEntity, renderMode, bUseLightsInArea);
	}


	if(this->m_pShaderEffect)
	{
		CCustomShaderEffectBase* pCse = static_cast<CCustomShaderEffectBase*>(this->m_pShaderEffect);
		pCse->AddToDrawListAfterDraw();
		DLC(CCustomShaderEffectDC, (NULL));			// reset latest shader effect command
	}
}

// CDrawEntityDC with no custom shaders
class CBasicEntityPrototype : public IDrawListPrototype
{
public:
	virtual void* AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data);
	virtual void  Draw(u32 batchSize, void * data);
	virtual u32   SizeOfElement();
private:
	struct Data
	{
		u32 fullMatrix : 1;
		u32 padding    : 31;
		DrawListAddress entityDraw;
	};
private:
	static const u32 sm_ElementSize;
};

const u32 CBasicEntityPrototype::sm_ElementSize = sizeof(CBasicEntityPrototype::Data);

void* CBasicEntityPrototype::AddDataForEntity(CEntity * entity, const CEntityDrawHandler * UNUSED_PARAM(drawHandler), void * data)
{
	Data * dataStruct = reinterpret_cast<Data*>(data);

	if(Unlikely(entity->GetTransform().GetTypeIdentifier() & fwTransform::TYPE_FULL_TRANSFORM_MASK))
	{
		dataStruct->fullMatrix = true;
		dataStruct->entityDraw = CDrawEntityFmDC::SharedData(entity);
	}
	else
	{
		dataStruct->fullMatrix = false;
		dataStruct->entityDraw = CDrawEntityDC::SharedData(entity);
	}
	return (char*)data + sm_ElementSize;
}

void CBasicEntityPrototype::Draw(u32 batchSize, void * data)
{
	for(u32 batchIndex = 0; batchIndex < batchSize; ++batchIndex)
	{
		Data * dataStruct = reinterpret_cast<Data*>(data);

		if(dataStruct->fullMatrix)
		{
			void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFm), dataStruct->entityDraw);
			CEntityDrawDataFm * entityDraw = static_cast<CEntityDrawDataFm*>(entityDrawData);
			entityDraw->Draw();
		}
		else
		{
			void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawData), dataStruct->entityDraw);
			CEntityDrawData * entityDraw = static_cast<CEntityDrawData*>(entityDrawData);
			entityDraw->Draw();
		}

		data = (char*)data + sm_ElementSize;
	}
}

u32 CBasicEntityPrototype::SizeOfElement()
{
	return sm_ElementSize;
}

// CDrawEntityDC with call to set interior flag and alternate alpha update
class CBasicEntityReflectionPrototype : public IDrawListPrototype
{
public:
	virtual void* AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data);
	virtual void  Draw(u32 batchSize, void * data);
	virtual u32   SizeOfElement();
private:
	struct Data
	{
		u32 fullMatrix : 1;
		u32 altAlpha   : 8;
		u32 padding    : 23;
		DrawListAddress entityDraw;
	};
private:
	static const u32 sm_ElementSize;
};

const u32 CBasicEntityReflectionPrototype::sm_ElementSize = sizeof(CBasicEntityReflectionPrototype::Data);

void* CBasicEntityReflectionPrototype::AddDataForEntity(CEntity * entity, const CEntityDrawHandler * UNUSED_PARAM(drawHandler), void * data)
{
	Data * dataStruct = reinterpret_cast<Data*>(data);

	// Alternate alpha
	if (entity->GetLodData().IsSlod3())
	{
		Vector3 entityPos;
		const float objectRadius = entity->GetBoundCentreAndRadius(entityPos);
		const float sphereRadius = CRenderPhaseReflection::ms_lodRanges[LODTYPES_DEPTH_SLOD3][1];
		const float fadeDist = objectRadius * 0.15f;

		const float distToCamera = (entityPos - RCC_VECTOR3(CRenderPhaseReflection::ms_cameraPos)).Mag();
		const float alphaValue = Saturate(((objectRadius + sphereRadius) - distToCamera) / fadeDist);
		dataStruct->altAlpha = u32(alphaValue * 255.0f);
	}
	else
	{
		dataStruct->altAlpha = 255;
	}

	if(entity->GetTransform().GetTypeIdentifier() & fwTransform::TYPE_FULL_TRANSFORM_MASK)
	{
		dataStruct->fullMatrix = true;
		dataStruct->entityDraw = CDrawEntityFmDC::SharedData(entity);
	}
	else
	{
		dataStruct->fullMatrix = false;
		dataStruct->entityDraw = CDrawEntityDC::SharedData(entity);
	}
	return (char*)data + sm_ElementSize;
}

void CBasicEntityReflectionPrototype::Draw(u32 batchSize, void * data)
{
	for(u32 batchIndex = 0; batchIndex < batchSize; ++batchIndex)
	{
		Data * dataStruct = reinterpret_cast<Data*>(data);

		const float a = CShaderLib::DivideBy255(dataStruct->altAlpha);
		CShaderLib::SetGlobalAlpha(a);

		if(dataStruct->fullMatrix)
		{
			void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFm), dataStruct->entityDraw);
			CEntityDrawDataFm * entityDraw = static_cast<CEntityDrawDataFm*>(entityDrawData);
			entityDraw->Draw();
		}
		else
		{
			void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawData), dataStruct->entityDraw);
			CEntityDrawData * entityDraw = static_cast<CEntityDrawData*>(entityDrawData);
			entityDraw->Draw();
		}

		data = (char*)data + sm_ElementSize;
	}
}

u32 CBasicEntityReflectionPrototype::SizeOfElement()
{
	return sm_ElementSize;
}

// CDrawEntityDC with call to set interior flag
class CBasicEntityInteriorPrototype : public IDrawListPrototype
{
public:
	virtual void* AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data);
	virtual void  Draw(u32 batchSize, void * data);
	virtual u32   SizeOfElement();
private:
	struct Data
	{
		u32 fullMatrix : 1;
		u32 padding    : 31;
		DrawListAddress entityDraw;
	};
private:
	static const u32 sm_ElementSize;
};

const u32 CBasicEntityInteriorPrototype::sm_ElementSize = sizeof(CBasicEntityInteriorPrototype::Data);

void* CBasicEntityInteriorPrototype::AddDataForEntity(CEntity * entity, const CEntityDrawHandler * UNUSED_PARAM(drawHandler), void * data)
{
	Data * dataStruct = reinterpret_cast<Data*>(data);

	if(entity->GetTransform().GetTypeIdentifier() & fwTransform::TYPE_FULL_TRANSFORM_MASK)
	{
		dataStruct->fullMatrix = true;
		dataStruct->entityDraw = CDrawEntityFmDC::SharedData(entity);
	}
	else
	{
		dataStruct->fullMatrix = false;
		dataStruct->entityDraw = CDrawEntityDC::SharedData(entity);
	}
	return (char*)data + sm_ElementSize;
}

void CBasicEntityInteriorPrototype::Draw(u32 batchSize, void * data)
{
	for(u32 batchIndex = 0; batchIndex < batchSize; ++batchIndex)
	{
		Data * dataStruct = reinterpret_cast<Data*>(data);

		if(dataStruct->fullMatrix)
		{
			void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFm), dataStruct->entityDraw);
			CEntityDrawDataFm * entityDraw = static_cast<CEntityDrawDataFm*>(entityDrawData);
			entityDraw->Draw();
		}
		else
		{
			void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawData), dataStruct->entityDraw);
			CEntityDrawData * entityDraw = static_cast<CEntityDrawData*>(entityDrawData);
			entityDraw->Draw();
		}

		data = (char*)data + sm_ElementSize;
	}
}

u32 CBasicEntityInteriorPrototype::SizeOfElement()
{
	return sm_ElementSize;
}

// shader effect tree with optional tint and draw entity full matrix
class CTreePrototype : public IDrawListPrototype
{
public:
	virtual void* AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data);
	virtual void  Draw(u32 batchSize, void * data);
	virtual u32   SizeOfElement();

private:
	struct Data
	{
		u32 modelIndex : 16;
		u32 fullMatrix : 1;
		u32 padding    : 15;
		DrawListAddress treeShader;
		DrawListAddress tintShader;
		DrawListAddress entityDraw;
	};
private:
	static const u32 sm_ElementSize;
};

const u32 CTreePrototype::sm_ElementSize = sizeof(CTreePrototype::Data);

void* CTreePrototype::AddDataForEntity(CEntity * entity, const CEntityDrawHandler * drawHandler, void * data)
{
	Data * dataStruct = reinterpret_cast<Data*>(data);

	dataStruct->modelIndex = entity->GetModelIndex();
	CCustomShaderEffectBase * shaderEffect = static_cast<CCustomShaderEffectBase*>(drawHandler->GetShaderEffect());
	if(shaderEffect)
	{
		shaderEffect->AddDataForPrototype(&dataStruct->treeShader);
	}
	if(entity->GetTransform().GetTypeIdentifier() & fwTransform::TYPE_FULL_TRANSFORM_MASK)
	{
		dataStruct->fullMatrix = 1;
		dataStruct->entityDraw = CDrawEntityFmDC::SharedData(entity);
	}
	else
	{
		dataStruct->fullMatrix = 0;
		dataStruct->entityDraw = CDrawEntityDC::SharedData(entity);
	}

	return (char*)data + sm_ElementSize;
}

BankBool sDontDrawNonInstancedTrees = false;
void CTreePrototype::Draw(u32 batchSize, void * data)
{
	if(sDontDrawNonInstancedTrees)
		return;

	const bool isShadowDrawList = static_cast<CDrawListMgr*>(gDrawListMgr)->IsExecutingShadowDrawList();

	if(isShadowDrawList)
	{
		CRenderPhaseCascadeShadowsInterface::SetRasterizerState(CSM_RS_TWO_SIDED);
	}

	CCustomShaderEffectTree::EnableShaderVarCaching(false);
	bool bSetShaderVarCaching = false;
	for(u32 batchIndex = 0; batchIndex < batchSize; ++batchIndex)
	{
		Data * dataStruct = reinterpret_cast<Data*>(data);

		void * treeShaderData  = gDCBuffer->GetDataBlock(sizeof(CCustomShaderEffectTree), dataStruct->treeShader);
#if __PS3
		void * tintShaderData = dataStruct->tintShader.IsNULL() ? NULL : gDCBuffer->ResolveDrawListAddress(dataStruct->tintShader);
#else
		void * tintShaderData = dataStruct->tintShader.IsNULL() ? NULL : gDCBuffer->GetDataBlock(sizeof(CCustomShaderEffectTint), dataStruct->tintShader);
#endif

		if(treeShaderData)
		{
			CCustomShaderEffectTree * treeShader = static_cast<CCustomShaderEffectTree*>(treeShaderData);

			rmcDrawable* pDrawable = NULL;
			CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(dataStruct->modelIndex)));
			Assert(pMI);
			if (pMI)
			{
				pDrawable = pMI->GetDrawable();
				if (pDrawable)
				{
					treeShader->SetShaderVariables(pDrawable);
					if(Unlikely(!bSetShaderVarCaching))
					{
						CCustomShaderEffectTree::EnableShaderVarCaching(true);
						bSetShaderVarCaching = true;
					}
					if(tintShaderData)
					{
						CCustomShaderEffectTint * tintShader = static_cast<CCustomShaderEffectTint*>(tintShaderData);
						tintShader->SetShaderVariables(pDrawable);
					}
				}
			}

			if(dataStruct->fullMatrix)
			{
				void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawDataFm), dataStruct->entityDraw);
				CEntityDrawDataFm * entityDraw = static_cast<CEntityDrawDataFm*>(entityDrawData);
				entityDraw->Draw();
			}
			else
			{
				void * entityDrawData = gDCBuffer->GetDataBlock(sizeof(CEntityDrawData), dataStruct->entityDraw);
				CEntityDrawData * entityDraw = static_cast<CEntityDrawData*>(entityDrawData);
				entityDraw->Draw();
			}

			data = (char*)data + sm_ElementSize;
		}

		// cleanup
#if TINT_PALETTE_SET_IN_EDGE
		if(tintShaderData)
		{
			SPU_COMMAND(GTA4__SetTintDescriptor,0);
			cmd->tintDescriptorPtr		= NULL;
			cmd->tintDescriptorCount	= 0;
			cmd->tintShaderIdx			= 0x00;
		}
#endif
	}

	CCustomShaderEffectTree::EnableShaderVarCaching(false);

	if(isShadowDrawList)
	{
		CRenderPhaseCascadeShadowsInterface::RestoreRasterizerState();
	}
}

u32 CTreePrototype::SizeOfElement()
{
	return sm_ElementSize;
}

CBasicEntityPrototype			g_BasicEntityPrototype;
CBasicEntityInteriorPrototype	g_BasicEntityInteriorPrototype;
CBasicEntityReflectionPrototype	g_BasicEntityReflectionPrototype;
CTreePrototype					g_TreePrototype;

bool g_prototype_batch = true;

IDrawListPrototype * CDrawListPrototypeManager::ms_Prototype = NULL;

u32 CDrawListPrototypeManager::ms_TimeStamp = 0xFFFFFFFF;

// This is the "cursor" within our current batch. Our next instance will be written here.
u8*	CDrawListPrototypeManager::ms_PageBuffer;
u8* CDrawListPrototypeManager::ms_BasePageBuffer;

// This is the beginning of the current batch.
u8* CDrawListPrototypeManager::ms_PageBufferCurrentStart; 

u32	CDrawListPrototypeManager::ms_PageBufferSize = 2048;

// This is the beginning of the current batch.
DrawListAddress	CDrawListPrototypeManager::ms_Address;

// This is the beginning of the current batch.
DrawListAddress	CDrawListPrototypeManager::ms_BaseAddress;

#if __ASSERT
u32 CDrawListPrototypeManager::ms_PageCreationTimestamp;
#endif // __ASSERT


void CDrawListPrototypeManager::Flush(bool forceAllocateNewBuffer)
{
	Assert(sysThreadType::IsUpdateThread());

	// Is there anything in the current batch?
	if(ms_PageBuffer > ms_PageBufferCurrentStart)
	{
		Assertf((u32)(size_t)(ms_PageBuffer - ms_PageBufferCurrentStart) <= ms_PageBufferSize, "Prototype wrote outside page boundaries! Wrote %d bytes, buffer cursor %p, start %p", (u32)(size_t)(ms_PageBuffer - ms_PageBufferCurrentStart), ms_PageBuffer, ms_PageBufferCurrentStart);
		Assertf(ms_PageCreationTimestamp == gDCBuffer->GetTimeStamp(), "Got stale prototype buffer - was created at %d, flushed at %d", ms_PageCreationTimestamp, gDCBuffer->GetTimeStamp());

		// Send it off to the draw list. Let's find out how many elements there are.
		u32 batchSize;
		u32 dataSize = u32(ms_PageBuffer - ms_PageBufferCurrentStart);
		batchSize = dataSize / ms_Prototype->SizeOfElement();
		Assertf((dataSize % ms_Prototype->SizeOfElement()) == 0, "Error wrong batch size");

		// Send off what we have so far.
		DLC(CDrawPrototypeBatchDC, (ms_Prototype, batchSize, ms_Address));

		// If we've exceeded the current page or if we're supposed to start a new page altogether,
		// ditch the current one and create a new one.
		if (ms_PageBuffer >= (ms_BasePageBuffer + ms_PageBufferSize) || forceAllocateNewBuffer)
		{
			AllocNewBuffer();
		}
		else
		{
			// Start a new batch within the page.
			ms_Address.SetOffset(ms_Address.GetOffset() + dataSize);

			ms_PageBufferCurrentStart = ms_PageBuffer;

			Assertf(ms_PageBuffer == (u8*)gDCBuffer->ResolveDrawListAddress(ms_Address), "Addresses don't match up - expected %p, is %p", ms_PageBuffer, (u8*)gDCBuffer->ResolveDrawListAddress(ms_Address));
		}
	} 

	ms_Prototype = NULL;
}

void CEntityDrawHandler::SetupLightsAndGlobalInInteriorFlag(const CEntity* pEntity, u32 ASSERT_ONLY(renderMode), u32 bucketMask, bool bSetupLights)
{
	bool bValidInterior = pEntity->GetInteriorLocation().IsValid();

	const bool reflectionPhase = 
		gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_REFLECTION_MAP) ||
		gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_WATER_REFLECTION) ||
		gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_MIRROR_REFLECTION);

	if (bSetupLights)
	{
		Assert(renderMode != rmDebugMode);
		spdAABB box = Lights::CalculatePartialDrawableBound( *pEntity, bucketMask, 0 );
		DLC_Add( Lights::UseLightsInArea, box, bValidInterior, BANK_SWITCH(DebugDeferred::m_forceSetAll8Lights, false), reflectionPhase, false);
		DLC_Add( CShaderLib::SetGlobalInInterior, bValidInterior );

		if(pEntity->GetIsTypeObject())
			if(static_cast< const CObject * >(pEntity)->GetUseLightOverride())
				Lights::ApplyLightOveride(pEntity);
	}
	else if (DRAWLISTMGR->IsBuildingDrawSceneDrawList() || DRAWLISTMGR->IsBuildingGBufDrawList() || reflectionPhase)
	{
		DLC_Add( CShaderLib::SetGlobalInInterior, bValidInterior );
	}
}


void CEntityDrawHandler::ResetLightOverride(const CEntity* pEntity, u32 ASSERT_ONLY(renderMode), bool bSetupLights)
{
	if (bSetupLights)
	{
		Assert(renderMode != rmDebugMode);
		if(pEntity->GetIsTypeObject())
			if(static_cast< const CObject * >(pEntity)->GetUseLightOverride())
				Lights::UnApplyLightOveride(pEntity);
	}
}


#if __BANK
bool CEntityDrawHandler::ShouldSkipEntity(const CEntity* pEntity)
{
	/*if (TiledScreenCapture::ShouldSkipEntity(pEntity))
		return true;
	else*/ if (SceneGeometryCapture::ShouldSkipEntity(pEntity))
		return true;
	else if (Unlikely(CEntity::ms_cullProps || CEntity::ms_renderOnlyProps))
	{
		if (pEntity && pEntity->GetBaseModelInfo() && pEntity->GetBaseModelInfo()->GetIsProp() && CEntity::ms_cullProps)
			return true;
		if (pEntity && pEntity->GetBaseModelInfo() && !pEntity->GetBaseModelInfo()->GetIsProp() && CEntity::ms_renderOnlyProps)
			return true;
	}


	if(g_EnableSkipRenderingShaders)
	{
		rmcDrawable *pDrawable = pEntity->GetDrawable();
		if(pDrawable)
		{
			grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
			if(pShaderGroup)
			{
				if(g_SkipRenderingGrassFur)
				{
					if(	pShaderGroup->LookupShader("grass_fur")			||
						pShaderGroup->LookupShader("grass_fur_mask")	||
						pShaderGroup->LookupShader("grass_fur_tnt")		)
					{
						return(true);	// skip rendering
					}
				}

				if(g_SkipRenderingTrees)
				{
					if(	pShaderGroup->LookupShader("trees")									||
						pShaderGroup->LookupShader("trees_camera_aligned")					||
						pShaderGroup->LookupShader("trees_camera_facing")					||
						pShaderGroup->LookupShader("trees_lod")								||
						pShaderGroup->LookupShader("trees_lod_tnt")							||
						pShaderGroup->LookupShader("trees_lod2")							||
						pShaderGroup->LookupShader("trees_lod2d")							||
						pShaderGroup->LookupShader("trees_normal")							||
						pShaderGroup->LookupShader("trees_normal_diffspec")					||
						pShaderGroup->LookupShader("trees_normal_diffspec_tnt")				||
						pShaderGroup->LookupShader("trees_normal_spec")						||
						pShaderGroup->LookupShader("trees_normal_spec_camera_aligned")		||
						pShaderGroup->LookupShader("trees_normal_spec_camera_aligned_tnt")	||
						pShaderGroup->LookupShader("trees_normal_spec_camera_facing")		||
						pShaderGroup->LookupShader("trees_normal_spec_camera_facing_tnt")	||
						pShaderGroup->LookupShader("trees_normal_spec_tnt")					||
						pShaderGroup->LookupShader("trees_normal_spec_wind")				||
						pShaderGroup->LookupShader("trees_shadow_proxy")					||
						pShaderGroup->LookupShader("trees_tnt")								)
					{
						return(true);	// skip rendering
					}
				}
			}
		}
	}// g_EnableSkipRenderingShaders...

	return false;
}
#endif // __BANK

dlCmdBase* CEntityDrawHandler::AddBasicToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams ASSERT_ONLY(, bool bDoInstancedDataCheck))
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( EntityAddToDrawList );

	CEntity* pEntity = static_cast<CEntity*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pEntity))
		return NULL;
#endif // __BANK

	Assert(pEntity->GetDrawHandlerPtr());
	Assertf(!pEntity->m_nFlags.bIsFrag, "Someone changed the bIsFrag flag late!");
	Assert(pEntity->GetBaseModelInfo());
	Assertf(pEntity->GetBaseModelInfo()->GetNumRefs() > 0,"AddToDrawList %s but it has no refs. _Very_ dangerous", pEntity->GetBaseModelInfo()->GetModelName());

	Assert(pDrawHandler->GetHasBones() == (pDrawHandler->GetDrawable() && pDrawHandler->GetDrawable()->GetSkeletonData() && (pDrawHandler->GetDrawable()->GetSkeletonData()->GetNumBones() > 1)));
	if(pDrawHandler->GetHasBones())
	{
		return NULL;
	}

#if __ASSERT
	if(bDoInstancedDataCheck)
	{
		bool entityHasInstancedGeometry = false;
		if(pEntity && pDrawHandler)
		{
			if(rmcDrawable *drawable = pDrawHandler->GetDrawable())
				entityHasInstancedGeometry = drawable->HasInstancedGeometry();
		}
		Assert(!entityHasInstancedGeometry);
	}	
#endif

	// We'll need the transform in CBasicEntityPrototype::AddDataForEntity(), 
	// and the archetype to get the model index in CEntityDrawData::Init().
	// Prefetch these now to avoid hundreds, possibly thousands, of cache misses.
	PrefetchDC(pEntity->GetTransformPtr());
	PrefetchDC(pEntity->GetArchetype());

#if NORTH_CLOTHS && __ASSERT
	bool bIsBendable = FALSE;
	if(pEntity->GetIsTypeDummyObject())
	{
		CBaseModelInfo* pModel = pEntity->GetBaseModelInfo();
		bIsBendable = (pModel->GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT);
	}
	Assert(!bIsBendable);
#endif // NORTH_CLOTHS

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	IDrawListPrototype * prototype = NULL;
	if(g_prototype_batch)
	{
		CDrawDataAddParams *pGTAParams = (CDrawDataAddParams *) pBaseParams;
#if __DEV
		// Someone didn't create the subclassed version of fwDrawDataAddParams!
		Assert(pGTAParams->m_Magic == CDrawDataAddParams::MAGIC);
#endif // __DEV

		if (pGTAParams->m_SetupLights)
		{
			// no suitable prototypes
		}
		else if (gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_REFLECTION_MAP))
		{
			if(pDrawHandler->GetShaderEffect() == NULL)
			{
				prototype = &g_BasicEntityReflectionPrototype;
			}
		}
		else if (DRAWLISTMGR->IsBuildingDrawSceneDrawList() ||
				  gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_MIRROR_REFLECTION))
		{
			if(pDrawHandler->GetShaderEffect() == NULL)
			{
				prototype = &g_BasicEntityInteriorPrototype;
			}
		}
		else
		{
			if(pDrawHandler->GetShaderEffect() && renderMode == rmStandard)
			{
				if(pDrawHandler->GetShaderEffect()->GetType() == CSE_TREE)
				{
					//Cull trees intersecting the script vehicle hack...
					if(CCustomShaderEffectGrass::IntersectsScriptFlattenAABB(pEntity))
						return NULL;

					prototype = &g_TreePrototype;
				}
			}
			else
			{
				prototype = &g_BasicEntityPrototype;
			}
		}
	}

	CDrawListPrototypeManager::SetPrototype(prototype);

	if(prototype)
	{
		CDrawListPrototypeManager::AddData(pEntity, pDrawHandler);
	}
	else
	{
		u32 bucket = DRAWLISTMGR->GetUpdateBucket();
		u32 bucketMask = DRAWLISTMGR->GetUpdateBucketMask();

		pDrawHandler->BeforeAddToDrawList(pEntity, pEntity->GetModelIndex(), renderMode, bucket, bucketMask, pBaseParams);

		if(pEntity->GetTransform().GetTypeIdentifier() & fwTransform::TYPE_FULL_TRANSFORM_MASK)
		{
			DLC(CDrawEntityFmDC, (pEntity));
		}
		else
		{
			DLC(CDrawEntityDC, (pEntity));
		}

		pDrawHandler->AfterAddToDrawList(pEntity, renderMode, pBaseParams);
	}

	// nothing needs the return value for this type atm
	return NULL;
}


dlCmdBase* CEntityDrawHandler::AddFragmentToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams ASSERT_ONLY(, bool bDoInstancedDataCheck))
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( FragAddToDrawList );

	CEntity* pEntity = static_cast<CEntity*>(pBaseEntity);
	Assertf(pEntity->m_nFlags.bIsFrag, "Someone changed the bIsFrag flag late!");

#if __BANK
	if (ShouldSkipEntity(pEntity))
		return NULL;
#endif // __BANK

#if __ASSERT
	if(bDoInstancedDataCheck)
	{
		bool entityHasInstancedGeometry = false;
		if(pEntity && pDrawHandler)
		{
			if(rmcDrawable *drawable = pDrawHandler->GetDrawable())
				entityHasInstancedGeometry = drawable->HasInstancedGeometry();
		}
		Assert(!entityHasInstancedGeometry);
	}	
#endif

	u32 modelIndex = pEntity->GetModelIndex();

#if __DEV
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"AddToDrawList %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
#endif //__DEV


#if NORTH_CLOTHS && __ASSERT
	bool bIsBendable = FALSE;
	if(pEntity->GetIsTypeDummyObject())
	{
		CBaseModelInfo* pModel = pEntity->GetBaseModelInfo();
		bIsBendable = (pModel->GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT);
	}
	Assert(!bIsBendable);
#endif // NORTH_CLOTHS

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();		// get the render mode for the list we are building
	u32 bucket = DRAWLISTMGR->GetUpdateBucket();
	u32 bucketMask = DRAWLISTMGR->GetUpdateBucketMask();

	pDrawHandler->BeforeAddToDrawList(pEntity, modelIndex, renderMode, bucket, bucketMask, pBaseParams, true);

	CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
	Assert(pMI);
	// don't allow peds onto the drawlist as frag objects!
	fragInst* pFragInst = pEntity->GetFragInst();
	rmcDrawable *pClothDrawable = NULL;
	environmentCloth* pEnvCloth = NULL;

	if( pFragInst )
	{
		bool captureStats = false;
		BANK_ONLY(captureStats = (g_PickerManager.GetSelectedEntity() == pBaseEntity);)
		// Search for a cloth drawable
		fragCacheEntry *cacheEntry = pFragInst->GetCacheEntry();
		if( cacheEntry )
		{
			fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
			Assertf(hierInst,"No HierInst on a cached frag");
			pEnvCloth = hierInst->envCloth;
			if( pEnvCloth )
				pClothDrawable = pEnvCloth->GetDrawable();
		}		

		// Draw a cloth ?
		if( pClothDrawable )
		{
#if __BANK
			if( g_DrawCloth )
#endif // __BANK			
			{		
				Assert( pEnvCloth );
				gDrawListMgr->AddClothReference(pEnvCloth);
				// Set material to trees to get back lighting to work
				u8 matid = pEntity->GetDeferredMaterial();
				if ((matid&DEFERRED_MATERIAL_CLEAR)==0){
					matid = (matid&~DEFERRED_MATERIAL_CLEAR) |DEFERRED_MATERIAL_TREE;
				}
				Vector3 v = VEC3V_TO_VECTOR3(pEnvCloth->GetOffset());
				grcDepthStencilStateHandle DSS = static_cast<CDrawListMgr*>(gDrawListMgr)->IsBuildingMirrorReflectionDrawList() ? CPhysics::GetClothManager()->GetInvertPassDSS() : CPhysics::GetClothManager()->GetPassDSS();
				DLC( dlCmdDrawTwoSidedDrawable, (pClothDrawable, v, modelIndex, 255, pEntity->GetNaturalAmbientScale(), pEntity->GetArtificialAmbientScale(), matid, pEntity->GetInteriorLocation().IsValid(), CPhysics::GetClothManager()->GetPassRS(), DSS, captureStats, false, false, pEntity));
			}
		}
		else if (pFragInst->GetType()->GetNumEnvCloths() == 0 && pMI->GetModelType() != MI_TYPE_PED)
		{
			// No cloth, no peds.
#if ENABLE_FRAG_OPTIMIZATION
			if(pMI->GetModelType() == MI_TYPE_VEHICLE)
			{
				DLC(CDrawFragTypeVehicleDC, (pEntity));
			}
			else 
#endif // ENABLE_FRAG_OPTIMIZATION
			{
				DLC(CDrawFragTypeDC, (pEntity));
			}
		}
	}
	else
	{
		DLC(CDrawFragTypeDC, (pEntity));
	}

	pDrawHandler->AfterAddToDrawList(pEntity, renderMode, pBaseParams);

	// nothing needs the return value for this type atm
	return NULL;
}


dlCmdBase* CEntityDrawHandler::AddBendableToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams ASSERT_ONLY(, bool bDoInstancedDataCheck))
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( BendableAddToDrawList );

	CEntity* pEntity = static_cast<CEntity*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pEntity))
		return NULL;
#endif // __BANK

#if __ASSERT
	if(bDoInstancedDataCheck)
	{
		bool entityHasInstancedGeometry = false;
		if(pEntity && pDrawHandler)
		{
			if(rmcDrawable *drawable = pDrawHandler->GetDrawable())
				entityHasInstancedGeometry = drawable->HasInstancedGeometry();
		}
		Assert(!entityHasInstancedGeometry);
	}	
#endif

	u32 modelIndex = pEntity->GetModelIndex();

#if __DEV
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	Assert(pModelInfo);
	Assertf(pModelInfo->GetNumRefs() > 0,"AddToDrawList %s but it has no refs. _Very_ dangerous", pModelInfo->GetModelName());
#endif //__DEV


#if NORTH_CLOTHS && __ASSERT
	bool bIsBendable = FALSE;
	if(pEntity->GetIsTypeDummyObject())
	{
		CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(fwModelId(modelIndex));
		bIsBendable = (pModel->GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT);
	}
	Assert(bIsBendable);
#endif // NORTH_CLOTHS

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();		// get the render mode for the list we are building
	u32 bucket = DRAWLISTMGR->GetUpdateBucket();
	u32 bucketMask = DRAWLISTMGR->GetUpdateBucketMask();

	pDrawHandler->BeforeAddToDrawList(pEntity, modelIndex, renderMode, bucket, bucketMask, pBaseParams);

#if NORTH_CLOTHS
	rmcLodGroup &lodGroup = pDrawHandler->GetDrawable()->GetLodGroup();
	if(lodGroup.ContainsLod(LOD_MED))
	{
		DLC(CDrawEntityDC, (pEntity, 1));	// Bendables: draw entity as non-skinned LOD1
	}
#endif //NORTH_CLOTHS...

	pDrawHandler->AfterAddToDrawList(pEntity, renderMode, pBaseParams);

	// nothing needs the return value for this type atm
	return NULL;
}

dlCmdBase* CEntityInstancedBasicDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* pParams)
{
	CEntity* pEntity = static_cast<CEntity*>(pBaseEntity);

	if(g_InstancedBatcher.AddInstancedData(pEntity, this))
		return NULL;

	return CEntityDrawHandler::AddBasicToDrawList(this, pEntity, pParams ASSERT_ONLY(, false));
}

dlCmdBase* CEntityInstancedFragDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* pParams)
{
	CEntity* pEntity = static_cast<CEntity*>(pBaseEntity);

	if(g_InstancedBatcher.AddInstancedData(pEntity, this))
		return NULL;

	CDrawListPrototypeManager::Flush(); 
	return CEntityDrawHandler::AddFragmentToDrawList(this, pEntity, pParams ASSERT_ONLY(, false));
}

dlCmdBase* CEntityInstancedBendableDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* pParams)
{
	CEntity* pEntity = static_cast<CEntity*>(pBaseEntity);

	if(g_InstancedBatcher.AddInstancedData(pEntity, this))
		return NULL;

	CDrawListPrototypeManager::Flush(); 

	return CEntityDrawHandler::AddBendableToDrawList(this, pEntity, pParams ASSERT_ONLY(, false));
}

