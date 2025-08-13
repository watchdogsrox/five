//
//Inlined template function definitions for CInstancedRendererBatcher
//

#include "camera/CamInterface.h"
#include "profile/timebars.h"
#include "shaders/CustomShaderEffectBase.h"
#include "shaders/ShaderLib.h"

template <int EntityCacheSize>
bool CInstancedRendererBatcher<EntityCacheSize>::AddInstancedData(CEntity *entity, fwDrawData *drawHandler)
{
	bool success = false;

	//We check cached IsIsntanced value for the entire drawable, and early out if there is no instanced geometry.
	bool entityHasInstancedGeometry = false;
	if(Verifyf(entity && drawHandler, "WARNING: NULL entity (0x%p) or DrawHandler (0x%p) passed into CInstancedRendererBatcher::AddInstancedData()!", entity, drawHandler))
	{
		if(rmcDrawable *drawable = drawHandler->GetDrawable())
			entityHasInstancedGeometry = drawable->HasInstancedGeometry();
	}

	if(entityHasInstancedGeometry)
	{
		if(mEntityCache.IsFull())
			Flush();

		typename EntityCacheList::value_type cachedEntity(entity, drawHandler);
		if(cachedEntity.IsInstanced())
		{
			mEntityCache.Push(typename EntityCacheList::value_type(entity, drawHandler));

			//Check cases where non-instanced rendering of geometry is also needed. Should try to get rid of this if possible.
			success = cachedEntity.OnlyNeedsInstancedRenderer();
		}
	}

	return success;

}

//Certain STL algorithms are "adaptive" algorithms, which just means that they try to allocate temp buffer memory space to improve the complexity cost of the algorithm, but
//they still work in low-memory scenarios by operating in-place with a higher complexity. std::stable_sort is one of these algorithms. (O(N) complexity if allocation succeeds,
//O(N*log(N)) if it doesn't.)
//For our purposes, we'd rather not allocate memory but still want the savings on the algorithm. So we'd prefer to supply our own temp buffer, but STL does not offers any official
//way to do this unfortunately. So instead I added this little hack below. This specializes the get_temporary_buffer which normally does the allocation and I return a pre-allocated
//buffer. Then I also needed to specialize return_temporary_buffer which handles the de-allocation so it doesn't delete the memory.
_STD_BEGIN
template<> inline
	pair<InstancedEntityCache *, ptrdiff_t>
		get_temporary_buffer<InstancedEntityCache>(ptrdiff_t UNUSED_PARAM(_Count)) _NOEXCEPT
	{	// Provide a pre-allocated buffer.
		return (pair<InstancedEntityCache *, ptrdiff_t>(g_InstancedBatcher.GetStablePartitionBuffer().GetElements(), g_InstancedBatcher.GetStablePartitionBuffer().size()));
	}

template<> inline
	void return_temporary_buffer(InstancedEntityCache *UNUSED_PARAM(_Pbuf))
	{	// We don't own temp buffer, so best not to delete it!
	}
_STD_END

template <int EntityCacheSize>
void CInstancedRendererBatcher<EntityCacheSize>::Flush()
{
	PF_PUSH_TIMEBAR_DETAIL("CInstancedRendererBatcher<>::Flush");
	//Need to partition the array up into batches, then "instance render" the relevant batches.
	//Note, using stable_partition to try and keep sorted draw order as best as possible.
	typedef typename EntityCacheList::value_type EntityCache;
	typename EntityCacheList::iterator end = mEntityCache.end();
	typename EntityCacheList::iterator batchStart = mEntityCache.begin();
	typename EntityCacheList::iterator batchEnd;
	bool resetAlpha = static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingGBufDrawList();	//Helps minimize state changes & keeps from setting alpha state in shadow pass. Set true 1st time through gbuff pass.

	//Debug output
#if __BANK
	static s32 sCurrBuffer = -1;

	if(sCurrBuffer == -1)
	{
		if((InstancedRendererConfig::sOutputGbufferInstancedDrawList1Frame || InstancedRendererConfig::sOutputGbufferInstancedBatches1Frame))
			sCurrBuffer = gRenderThreadInterface.GetCurrentBuffer();
	}
	else if(sCurrBuffer != gRenderThreadInterface.GetCurrentBuffer())
	{
		InstancedRendererConfig::sOutputGbufferInstancedDrawList1Frame = false;
		InstancedRendererConfig::sOutputGbufferInstancedBatches1Frame = false;
		sCurrBuffer = -1;
	}

	if(InstancedRendererConfig::sOutputGbufferInstancedDrawList1Frame || InstancedRendererConfig::sOutputGbufferInstancedBatches1Frame)
	{
		const bool isShadowDrawList =			static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingShadowDrawList();
		const bool isGlobalReflectionDrawList =	static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingGlobalReflectionDrawList();
		const bool isMirrorReflectionDrawList =	static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingMirrorReflectionDrawList();
		const bool isWaterReflectionDrawList =	static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingWaterReflectionDrawList();
		const bool isGBufferDrawList =			static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingGBufDrawList();

		Displayf("");
		Displayf("================== Instanced Entity Render List - %d Cached Entites ==================", (int)(end - batchStart));
		if(isShadowDrawList)			Displayf("----==== Shadow Draw List");
		if(isGlobalReflectionDrawList)	Displayf("----==== Global Reflection Draw List");
		if(isMirrorReflectionDrawList)	Displayf("----==== Mirror Reflection Draw List");
		if(isWaterReflectionDrawList)	Displayf("----==== Water Reflection Draw List");
		if(isGBufferDrawList)			Displayf("----==== GBuffer Draw List");
		for(typename EntityCacheList::iterator iter = batchStart; iter != end; ++iter)
		{
			if(iter->IsValid())
			{
				Displayf("%4d:\tModel: %32s\t\tLOD = %d\t\tCrossfading = %5s\t\tDistFromCam = %10f\t\tSetStipple = %5s\t\tStippleMask = 0x%4x\t\tpEntity = (CEntity *)0x%p", (int)(iter - batchStart), iter->GetEntity()->GetBaseModelInfo()->GetModelName(), iter->mLODlevel, iter->IsCrossFading() ? "True" : "False", Dist(VECTOR3_TO_VEC3V(camInterface::GetPos()), iter->GetEntity()->GetTransform().GetPosition()).Getf(), iter->ShouldSetStipple() ? "True" : "False", iter->mStippleMask, iter->GetEntity());
			}
			else
			{
				Displayf("%4d:\t<Invalid>", (int)(iter - batchStart));
			}
		}
		Displayf("========================= End - Instanced Entity Render List =========================");
		Displayf("");
	}
#endif //__BANK

	bool hasBatchToInstance = (batchStart != end);
	if(hasBatchToInstance)
		DLC_Add(&grcInstanceBuffer::BeginInstancing);

	while(batchStart != end && InstancedRendererConfig::sRenderInstancedGeometry)
	{
		batchEnd = std::stable_partition(batchStart + 1, end, bind2nd(std::mem_fun_ref(&EntityCache::IsInSameBatch), *batchStart));

		bool isCrossFading = batchStart->IsCrossFading();
		if(isCrossFading)
		{
			bool setStipple = batchStart->ShouldSetStipple();
			u32 stippleAlpha = batchStart->GetStippleAlpha();
			if(batchStart->IsCurrLODInstanced() && InstancedRendererConfig::sRenderFadingOutGeometry)
			{
				if(setStipple)
				{
					DLC_Add(&CShaderLib::SetStippleAlpha, stippleAlpha, false);
					resetAlpha = true;
				}
				RenderBatch(batchStart, batchEnd, batchStart->mLODlevel);
			}

			if(batchStart->IsNextLODInstanced() && InstancedRendererConfig::sRenderFadingInGeometry)
			{
				if(setStipple)
				{
					DLC_Add(&CShaderLib::SetStippleAlpha, stippleAlpha, true);
					resetAlpha = true;
				}
				RenderBatch(batchStart, batchEnd, batchStart->mLODlevel + 1);
			}
		}
		else if(InstancedRendererConfig::sRenderNonCrossFadingGeometry)
		{
			if(resetAlpha)
			{
				DLC_Add(&CShaderLib::ResetAlpha, false, true);
				resetAlpha = false;
			}
			RenderBatch(batchStart, batchEnd, batchStart->mLODlevel);
		}

		//Don't forget to advance batchStart for next partition!
		batchStart = batchEnd;
	}

	if(hasBatchToInstance)
	{
		DLC_Add(&grcInstanceBuffer::EndInstancing);

		if(resetAlpha)
		{
			DLC_Add(&CShaderLib::ResetAlpha, false, true);
			resetAlpha = false;
		}
	}

	//Once all batches have been dispatched, need to clear the EntityCache array so new entities can be added.
	mEntityCache.Reset();
	PF_POP_TIMEBAR_DETAIL();
}

template <int EntityCacheSize>
void CInstancedRendererBatcher<EntityCacheSize>::RenderBatch(	typename CInstancedRendererBatcher<EntityCacheSize>::EntityCacheList::const_iterator start,
																typename CInstancedRendererBatcher<EntityCacheSize>::EntityCacheList::const_iterator end, u32 lod )
{
	typedef typename EntityCacheList::value_type EntityCache;

#if __BANK
	if(!InstancedRendererConfig::sRenderLodGeometry[lod])
		return; //Debug - Early out if we're not rendering this LOD level

	if(InstancedRendererConfig::sOutputGbufferInstancedBatches1Frame && static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingGBufDrawList())
	{
		Displayf("");
		Displayf("////************** Instanced Entity Batch - %d Cached Entites - LOD: %d **************////", (int)(end - start), lod);
		for(typename EntityCacheList::const_iterator iter = start; iter != end; ++iter)
		{
			if(iter->IsValid())
			{
				Displayf("%4d:\tModel: %32s\t\tLOD = %d\t\tCrossfading = %5s\t\tDistFromCam = %10f\t\tSetStipple = %5s\t\tStippleMask = 0x%4x\t\tpEntity = (CEntity *)0x%p", (int)(iter - start), iter->GetEntity()->GetBaseModelInfo()->GetModelName(), iter->mLODlevel, iter->IsCrossFading() ? "True" : "False", Dist(VECTOR3_TO_VEC3V(camInterface::GetPos()), iter->GetEntity()->GetTransform().GetPosition()).Getf(), iter->ShouldSetStipple() ? "True" : "False", iter->mStippleMask, iter->GetEntity());
			}
			else
			{
				Displayf("%4d:\t<Invalid>", (int)(iter - start));
			}
		}
		Displayf("////************************** End - Instanced Entity Batch *************************////");
		Displayf("");
	}
#endif //__BANK

	Assertf(start != end, "WARNING: CInstancedRendererBatcher<>::RenderBatch called with 0-sized batch.");

	if(Verifyf(start->IsValid(), "WARNING: CInstancedRendererBatcher<>::RenderBatch called on NULL cached entity batch!"))
	{
		//Need to get 1st element's CSE to setup our grcVecArrayInstanceBufferList
		CCustomShaderEffectBase *cse = static_cast<CCustomShaderEffectBase *>(start->GetDrawHandler()->GetShaderEffect());
		if(cse == NULL)
			return;	//Can't do this without the CSE!

		//Start PIX tagging.
		if(InstancedRendererConfig::sAddPixBatchAnnotation)
			DLC_Add(&EntityCache::PushPixTagsForBatch, start->GetModelIndex(), static_cast<u32>(end - start));

		//Add CSE to drawlist
		bool hasPerBatchShaderVars = cse->HasPerBatchShaderVars();
		if(hasPerBatchShaderVars && InstancedRendererConfig::sAddCSEToDrawlist)
			cse->AddToDrawList(start->GetModelIndex(), true);

		//Create grcVec4BufferInstanceBufferList
		grcVec4BufferInstanceBufferList ibList(cse->GetNumRegistersPerInstance());

		u32 count = (u32)(end - start);
		u32 maxBatchSize = ibList.GetMaxInstancesPerBuffer();
		typename EntityCacheList::const_iterator iter = start;

		while(count)
		{
			u32 batchSize = maxBatchSize;

			if(count < maxBatchSize)
				batchSize = count;

			Vector4 *pInstData = ibList.OpenBuffer(batchSize);

			if(pInstData)
			{
				for(u32 i=0; i<batchSize; i++)
				{
					CCustomShaderEffectBase *currCse = static_cast<CCustomShaderEffectBase *>(iter->GetDrawHandler()->GetShaderEffect());
					Assert(currCse != NULL);	//Should be from the same batch, so it should never equal NULL!
					u32 alpha = iter->mActualAlphaFade;
					if(iter->IsCrossFading())	//If cross fading, apply proper cross fade to our alpha
					{
						if(iter->mLODlevel == lod)
							alpha = (iter->mCrossFade * alpha) / 255;
						else
							alpha = ((255 - iter->mCrossFade) * alpha) / 255;
					}
					currCse->WriteInstanceData(ibList, pInstData, *iter, alpha);
					pInstData += cse->GetNumRegistersPerInstance();
					iter++;
				}
			}
			ibList.CloseBuffer();
			count -= batchSize;
		}

		if(InstancedRendererConfig::sCSESetBatchRenderStates)
			cse->AddBatchRenderStateToDrawList();

		DLC(CDrawEntityInstancedDC, (start->GetEntity(), ibList, lod));

		if(InstancedRendererConfig::sCSESetBatchRenderStates)
			cse->AddBatchRenderStateToDrawListAfterDraw();

		if(hasPerBatchShaderVars && InstancedRendererConfig::sAddCSEToDrawlist)
			cse->AddToDrawListAfterDraw();

		//End PIX tagging.
		if(InstancedRendererConfig::sAddPixBatchAnnotation)
			DLC_Add(&EntityCache::PopPixTagsForBatch);	//DLCPopTimebar();
	}
}
