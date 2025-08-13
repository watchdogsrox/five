#include "InstancedEntityRenderer.h"

//Game Includes
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "shaders/CustomShaderEffectGrass.h"
#include "scene/lod/LodDrawable.h"
#include "camera/CamInterface.h"
#include "bank/bank.h"

RENDER_OPTIMISATIONS()

PARAM(instancebatchincapture,"Show useful batch info in GPAD/PIX");

//
// Statics
//

BankBool InstancedRendererConfig::sRenderInstancedGeometry = true;
BankBool InstancedRendererConfig::sAddCSEToDrawlist = true;
BankBool InstancedRendererConfig::sCSESetBatchRenderStates = true;
BankBool InstancedRendererConfig::sAddPixBatchAnnotation = false; //Should be false by default! Otherwise, we'll do unnecessary work for instanced geometry. There's a cmdline option to enable by default.

//Cross Fade debugging
BankBool InstancedRendererConfig::sRenderNonCrossFadingGeometry = true;
BankBool InstancedRendererConfig::sRenderFadingOutGeometry = true;
BankBool InstancedRendererConfig::sRenderFadingInGeometry = true;

//LOD debugging
BANK_ONLY(atRangeArray<bool, LOD_COUNT> InstancedRendererConfig::sRenderLodGeometry(true));

BankBool InstancedRendererConfig::sOutputGbufferInstancedDrawList1Frame = false;
BankBool InstancedRendererConfig::sOutputGbufferInstancedBatches1Frame = false;

BankBool sClearCrossFadeOnMinMaxStippleMask = true;

#define INSTANCE_BUFFER_PREALLOCATE_AMT 1500



namespace InstancedDrawHelpers
{
	//TODO: This is a slightly modified copy of the same func from DrawableDataStructs.cpp. Should probably merge them at some point.
	static u32 AdjustAlpha(u32 alpha, bool shouldForceAlpha)
	{
		bool isWaterReflectionDrawList;
		if(sysThreadType::IsRenderThread())
		{
			isWaterReflectionDrawList = gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_WATER_REFLECTION);
		}
		else
		{
			isWaterReflectionDrawList = gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_WATER_REFLECTION);
		}

		// Usual case is not shouldForceAlpha == false, so check that first so we 
		// can avoid both the call to IsExecutingDrawList() and the branch midprediction.
		if (!shouldForceAlpha || !isWaterReflectionDrawList)
		{
			return alpha;
		}

		// this causes SLOD3's etc. to cover up the LODs unfortunately, but
		// without it we can't render lower LODs in place of higher LODs ..
		return LODTYPES_ALPHA_VISIBLE;
	}
}

//
// InstancedEntityCache
//
InstancedEntityCache::InstancedEntityCache()
: mEntity(NULL)
, mDrawHandler(NULL)
, mLODlevel(0)
, mCrossFade(-1)
, mStippleMask(0xFFFF)
, mIsLODModelInstanced(false)
, mIsNextLODModelInstanced(false)
, mLODModelHasNonInstancedGeom(false)
, mNextLODModelHasNonInstancedGeom(false)
, mSetStipple(false)
{ }

InstancedEntityCache::InstancedEntityCache(CEntity *entity, fwDrawData *drawHandler)
: mEntity(entity)
, mDrawHandler(drawHandler)
, mLODlevel(0)
, mCrossFade(-1)
, mStippleMask(0xFFFF)
, mIsLODModelInstanced(false)
, mIsNextLODModelInstanced(false)
, mLODModelHasNonInstancedGeom(false)
, mNextLODModelHasNonInstancedGeom(false)
, mSetStipple(false)
{ RecomputeLODLevel(); }

void InstancedEntityCache::SetEntity(CEntity *entity, fwDrawData *drawHandler)
{
	mEntity = entity; mDrawHandler = drawHandler;
	mIsLODModelInstanced = mIsNextLODModelInstanced = mLODModelHasNonInstancedGeom = mNextLODModelHasNonInstancedGeom = false;
	mSetStipple = false;
	mStippleMask = 0xFFFF;
	RecomputeLODLevel();
}

void InstancedEntityCache::RecomputeLODLevel()
{
	//Cull trees intersecting the script vehicle hack...
	if(mDrawHandler && mDrawHandler->GetShaderEffect() && mDrawHandler->GetShaderEffect()->GetType() == CSE_TREE && CCustomShaderEffectGrass::IntersectsScriptFlattenAABB(mEntity))
		mEntity = NULL; //Culled entity, make cache entry invalid so this won't be added to instanced draw cache.

	if(mDrawHandler == NULL)
		mEntity = NULL;	//Make this invalid if either entity or drawHandler is NULL. This way, IsValid just has to check the entity ptr.

	if(IsValid())
	{
		if(CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(GetModelIndex()))))
		{
			if(rmcDrawable *pDrawable = pModelInfo->GetDrawable())
			{
				//Vars from entity
				u32 fadeAlpha = mEntity->GetAlpha();
				bool forceAlpha = CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(mEntity);
				bool useAltfadeDist = mEntity->GetUseAltFadeDistance();
				LODDrawable::crossFadeDistanceIdx fadeDist = useAltfadeDist ? LODDrawable::CFDIST_ALTERNATE : LODDrawable::CFDIST_MAIN;
				u32 phaseLODs = mEntity->GetPhaseLODs();
				u32 lastLODIdx = mEntity->GetLastLODIdx();

				mActualAlphaFade = InstancedDrawHelpers::AdjustAlpha(fadeAlpha, forceAlpha);
				//const eRenderMode renderMode = DRAWLISTMGR->GetRtRenderMode();
				//const u32 bucketMask = DRAWLISTMGR->GetRtBucketMask();

				const bool isShadowDrawList =			static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingShadowDrawList();
				const bool isGBufferDrawList =			static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingGBufDrawList();

				mLODlevel = 0;
				mCrossFade = -1;
				u32 LODFlag = (phaseLODs >> dlCmdNewDrawList::GetCurrDrawListLODFlagShift_UpdateThread()) & LODDrawable::LODFLAG_MASK;

				if (LODFlag != LODDrawable::LODFLAG_NONE)
				{
					if (LODFlag & LODDrawable::LODFLAG_FORCE_LOD_LEVEL)
					{
						mLODlevel = LODFlag & ~LODDrawable::LODFLAG_FORCE_LOD_LEVEL;
					}
					else
					{
						//float dist = LODDrawable::CalcLODDistance(VEC3V_TO_VECTOR3(mEntity->GetTransform().GetPosition()));
						float dist = Dist(VECTOR3_TO_VEC3V(camInterface::GetPos()), mEntity->GetTransform().GetPosition()).Getf();
						LODDrawable::CalcLODLevelAndCrossfadeForProxyLod(pDrawable, dist, LODFlag, mLODlevel, mCrossFade, fadeDist, lastLODIdx);
					}
				}
				else
				{
					//float dist = LODDrawable::CalcLODDistance(VEC3V_TO_VECTOR3(mEntity->GetTransform().GetPosition()));
					float dist = Dist(VECTOR3_TO_VEC3V(camInterface::GetPos()), mEntity->GetTransform().GetPosition()).Getf();
					LODDrawable::CalcLODLevelAndCrossfade(pDrawable, dist, mLODlevel, mCrossFade, fadeDist, lastLODIdx ASSERT_ONLY(, pModelInfo));
				}

				if((LODFlag != LODDrawable::LODFLAG_NONE || isShadowDrawList) && mCrossFade > -1)
				{
					if(mCrossFade < 128 && mLODlevel + 1 < LOD_COUNT && pDrawable->GetLodGroup().ContainsLod(mLODlevel + 1))
						mLODlevel = mLODlevel + 1;

					mCrossFade = -1;
				}
				else if( mActualAlphaFade < 255 ) // Entity + cross fade fade: we can't deal with this, nuke crossFade
				{
					mCrossFade = -1;
				}

				//These checks happens in rendering code... so simplify it and do it here.
				if(mLODlevel >= 3 || !pDrawable->GetLodGroup().ContainsLod(mLODlevel + 1))
				{
					mCrossFade = -1;
				}

				//Check current lod if it has instanced geometry.
				bool hasNonInstShaders;
				mIsLODModelInstanced = pDrawable->GetLodGroup().IsLodInstanced(mLODlevel, pDrawable->GetShaderGroup(), hasNonInstShaders);
				mLODModelHasNonInstancedGeom = hasNonInstShaders;

				//If crossfading, check if the next lod has instanced geometry.
				if(mCrossFade != -1)
				{
					mIsNextLODModelInstanced = pDrawable->GetLodGroup().IsLodInstanced(mLODlevel + 1, pDrawable->GetShaderGroup(), hasNonInstShaders);
					mNextLODModelHasNonInstancedGeom = hasNonInstShaders;
				}

				//const bool allowAlphaBlend = !mEntity->IsBaseFlagSet(fwEntity::HAS_OPAQUE) && mEntity->IsBaseFlagSet(fwEntity::HAS_DECAL);
				u32 stippleAlpha = GetStippleAlpha();
				mSetStipple = IsCrossFading() ? isGBufferDrawList : false; //isGBufferDrawList && !allowAlphaBlend;
				mStippleMask = mSetStipple ? CShaderLib::GetStippleMask(stippleAlpha, false) : 0xFFFF;

				//Slight optimization for stippled alpha with stipple mask = 0xFFFF or  0x0000
				if(mSetStipple && sClearCrossFadeOnMinMaxStippleMask)
				{
					if(mStippleMask == 0xFFFF && CShaderLib::GetStippleMask(stippleAlpha, true) == 0x0000)
					{
						mSetStipple = false;
						mCrossFade = -1;
					}
					else if(mStippleMask == 0x000 && CShaderLib::GetStippleMask(stippleAlpha, true) == 0xFFFF)
					{
						mSetStipple = false;
						mCrossFade = -1;
						mLODlevel = mLODlevel + 1;
					}
				}
			} // if(rmcDrawable *pDrawable = pModelInfo->GetDrawable())
		} //if(CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(static_cast<u32>(GetModelIndex()))))
	} //IsValid()
}

bool InstancedEntityCache::OnlyNeedsInstancedRenderer() const
{
	//How should we deal with cross-fading entities where 1 lod is instanced and 1 is not? What about geometry with both 
	//instanced and non-instanced geometry?
	//While not quite optimal, we can prompt the calling system to submit the entity for normal drawing as well.
	bool onlyNeedsInstRender =	IsCurrLODInstanced() && !mLODModelHasNonInstancedGeom &&
								(!IsCrossFading() || (IsCurrLODInstanced() && !mNextLODModelHasNonInstancedGeom));

// 	if(IsCrossFading())
// 		onlyNeedsInstRender = onlyNeedsInstRender && cachedEntity.IsCurrLODInstanced() && !mNextLODModelHasNonInstancedGeom;

	return onlyNeedsInstRender;
}

//Instanced batch pix tagging
void InstancedEntityCache::PushPixTagsForBatch(u32 PIX_TAGGING_ONLY(modelIndex), u32 PIX_TAGGING_ONLY(batchSize))
{
	//Don't do extra tagging work if not necessary!!
#if ENABLE_PIX_TAGGING
	char batchName[255];
	fwModelId modelId((strLocalIndex(modelIndex)));
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	sprintf(batchName, "%s Batch - %d Instances", pModelInfo ? pModelInfo->GetModelName() : "<InvalidModel>", batchSize);

	PF_PUSH_MARKER(batchName);
#else
	PF_PUSH_MARKER("Instanced Batch");
#endif
}

void InstancedEntityCache::PopPixTagsForBatch()
{
	PF_POP_MARKER();
}


//
// InstancedRendererConfig
//
void InstancedRendererConfig::Init()
{
#if RSG_PC
	// increase size of pool
	static bool g_firstFrame = true;
	if ( g_firstFrame ){
		const int PreAllocateSize = INSTANCE_BUFFER_PREALLOCATE_AMT;
		for(int i=0;i<PreAllocateSize;i++ ){
			grcInstanceBuffer::Create();
		}
		g_firstFrame  = false;
	}
#endif
#if __BANK
	sAddPixBatchAnnotation = PARAM_instancebatchincapture.Get();
#endif
}

#if __BANK
extern BankBool sDontDrawNonInstancedTrees;
void InstancedRendererConfig::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Instancing", false);
	{
		bank.AddToggle("Render Instanced Geometry - Toggle to visualize what is instanced", &sRenderInstancedGeometry);
		bank.AddToggle("Don't Draw Non-Instanced Trees", &sDontDrawNonInstancedTrees);

		bank.AddSeparator();
		bank.AddToggle("Add CSE to drawlist before instanced batch", &sAddCSEToDrawlist);
		bank.AddToggle("Let CSE set per batch render states", &sCSESetBatchRenderStates);
		bank.AddToggle("Clear Stipple & Crossfade when Stipple Mask = 0xFFFF or 0x000", &sClearCrossFadeOnMinMaxStippleMask);

		bank.AddTitle("");
		bank.AddToggle("Add Pix Annotations for Instanced Batches", &sAddPixBatchAnnotation);

		bank.PushGroup("Debugging", false);
		{
			bank.AddToggle("Render Non-CrossFading Instanced Geometry", &sRenderNonCrossFadingGeometry);
			bank.AddToggle("Render Fading Out Instanced Geometry", &sRenderFadingOutGeometry);
			bank.AddToggle("Render Fading In Instanced Geometry", &sRenderFadingInGeometry);

			bank.PushGroup("LODs");
			{
				LODRenderArray::iterator begin = sRenderLodGeometry.begin();
				LODRenderArray::iterator end = sRenderLodGeometry.end();
				for(LODRenderArray::iterator iter = begin; iter != end; ++iter)
				{
					char buff[64];
					formatf(buff, "Render LOD %d Instanced Geometry", iter - begin);
					bank.AddToggle(buff, &(*iter));
				}
			}
			bank.PopGroup();

			bank.AddSeparator();
			bank.AddToggle("Output Instanced Draw List (1 Frame)", &sOutputGbufferInstancedDrawList1Frame);
			bank.AddToggle("Output Instanced Draw List & Batches (1 Frame)", &sOutputGbufferInstancedBatches1Frame);
		}
		bank.PopGroup();
	}
	bank.PopGroup();
}
#endif
