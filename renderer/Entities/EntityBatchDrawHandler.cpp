//
// renderer/Entities/EntityBatchDrawHandler.cpp : batched entity list draw handler
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "EntityBatchDrawHandler.h"
#include "InstancedEntityRenderer.h"
#include "scene/EntityBatch.h"
#include "scene/lod/LodDrawable.h"
#include "scene/lod/LodScale.h"
#include "camera/viewports/ViewportManager.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/Renderer.h"

#include "grcore/device.h"
#include "grcore/viewport.h"
#include "fwscene/mapdata/mapinstancedata.h"
#include "fwscene/scan/ScanCascadeShadows.h"
#include "fwscene/stores/boxstreamer.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwsys/timer.h"
#include "bank/bank.h"

#if RSG_ORBIS
#	include "grcore/gfxcontext_gnm.h"
#endif //RSG_ORBIS

RENDER_OPTIMISATIONS()

namespace EBStatic
{
	extern CSettings::eSettingsLevel sQuality;

#if RSG_DURANGO || RSG_PC
	extern u32 sPerfSkipInstance;
#endif // RSG_DURANGO || RSG_PC...

#if __BANK
	extern BankBool sPerfOnlyDraw1stInstanceGBuf;
	extern BankBool sPerfOnlyDraw1stInstanceShadows;

#if GRASS_BATCH_CS_CULLING
	extern BankBool sGrassRenderGeometry;
#endif // GRASS_BATCH_CS_CULLING

	extern BankBool sPerfDisplayUsageInfo;
	extern BankBool sPerfDisplayRuntimeInfo;
	extern u32 sPerfBatchesRenderedGbuf;
	extern u32 sPerfInstancesRenderedGbuf;
	extern u32 sPerfInstancesRenderedGbuf_LODFade;

	extern u32 sPerfBatchesRenderedShadow;
	extern u32 sPerfInstancesRenderedShadow;
	extern u32 sPerfInstancesRenderedShadow_LODFade;

	extern u32 sPerfBatchesRenderedOther;
	extern u32 sPerfInstancesRenderedOther;
	extern u32 sPerfInstancesRenderedOther_LODFade;

#if GRASS_BATCH_CS_CULLING && __BANK && (RSG_DURANGO || RSG_ORBIS)
	extern BankBool sPerfDisplayGrassCSInfo;	//Preliminary... and potentially very slow!
# if DEVICE_GPU_WAIT
	extern BankBool sPerfDisplayGrassCS_UseGPUFence;
# endif
	extern u32 sPerfCSInstancesRenderedGbuf;
	extern u32 sPerfCSZeroCountBatchesGbuf;
	extern u32 sPerfCSInstancesRenderedShadow;
	extern u32 sPerfCSZeroCountBatchesShadow;
	extern u32 sPerfCSInstancesRenderedOther;
	extern u32 sPerfCSZeroCountBatchesOther;
#endif //GRASS_BATCH_CS_CULLING && __BANK && (RSG_DURANGO || RSG_ORBIS)

	u32 ApplyDebugInstanceCulling(u32 instanceCount, u32 startInstance)
	{
		bool isGBufferDrawList, isShadowDrawList;
		if(CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
			isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();
		}
		else
		{
			isGBufferDrawList = DRAWLISTMGR->IsBuildingGBufDrawList();
			isShadowDrawList = DRAWLISTMGR->IsBuildingShadowDrawList();
		}
		if(isGBufferDrawList && sPerfOnlyDraw1stInstanceGBuf) startInstance = instanceCount-1;
		else if(isShadowDrawList && sPerfOnlyDraw1stInstanceShadows) startInstance = instanceCount-1;

		return startInstance;
	}

	void ResetFrameCounters()
	{
		if(sPerfDisplayUsageInfo && sPerfDisplayRuntimeInfo)
		{
			static u32 sCurrFrame = static_cast<u32>(-1);
			if(sCurrFrame != fwTimer::GetSystemFrameCount())
			{
				sPerfBatchesRenderedGbuf = sPerfBatchesRenderedShadow = sPerfBatchesRenderedOther = 0;
				sPerfInstancesRenderedGbuf = sPerfInstancesRenderedShadow = sPerfInstancesRenderedOther = 0;
				sPerfInstancesRenderedGbuf_LODFade = sPerfInstancesRenderedShadow_LODFade = sPerfInstancesRenderedOther_LODFade = 0;
				sCurrFrame = fwTimer::GetSystemFrameCount();
			}
		}
	}

	void UpdateGrassCounters(u32 instanceCount, u32 startInstance)
	{
		if(sPerfDisplayUsageInfo && sPerfDisplayRuntimeInfo)
		{
			ResetFrameCounters();

			const bool isGBufferDrawList = DRAWLISTMGR->IsBuildingGBufDrawList();
			const bool isShadowDrawList = DRAWLISTMGR->IsBuildingShadowDrawList();
			if(isGBufferDrawList)
			{
				++sPerfBatchesRenderedGbuf;
				sPerfInstancesRenderedGbuf += instanceCount;
				sPerfInstancesRenderedGbuf_LODFade += instanceCount - startInstance;
			}
			else if(isShadowDrawList)
			{
				++sPerfBatchesRenderedShadow;
				sPerfInstancesRenderedShadow += instanceCount;
				sPerfInstancesRenderedShadow_LODFade += instanceCount - startInstance;
			}
			else
			{
				++sPerfBatchesRenderedOther;
				sPerfInstancesRenderedOther += instanceCount;
				sPerfInstancesRenderedOther_LODFade += instanceCount - startInstance;
			}
		}
	}

	void UpdateGrassCounters(u32 instanceCount, CDrawGrassBatchDC *dc)
	{
		u32 startInstance = 0;
		if(Verifyf(dc != NULL, "Invalid GrassBatchDC pointer! Actual instance counters will not be accurate."))
			startInstance = dc->GetDrawData().GetStartInstance();

		//From: void DrawInstancedGrass(grmGeometry &geometry, const grcVertexBuffer *vb, u32 startInstance)
		if(startInstance >= instanceCount)
			return;

		startInstance = ApplyDebugInstanceCulling(instanceCount, startInstance);
		UpdateGrassCounters(instanceCount, startInstance);
	}
#endif //__BANK

	static BankBool sPropInstanceList_EnableDraw = true;
	static BankInt32 sPropInstanceList_ForceLod = -1;

	static BankBool sGrassInstanceList_EnableDraw = true;
	BANK_ONLY(atRangeArray<bool, LOD_COUNT> sGrassRenderLodGeometry(true));

#if GRASS_BATCH_CS_CULLING
	BankInt32 sGrassComputeShader_NumClipPlanesForNonShadowVp = 6;

	static BankBool sPerfRunCSOnlyDuringCutout = true;
	BankBool sGrassComputeShader_AllowBatching = true;
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(BankBool sGrassComputeShader_AllowCopyStructureCountBatching = true);
	GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(BankBool sGrassComputeShader_AvoidInterleaving = true);
	BankBool sFlushCSAfterEachDispatch = false;
	BankBool sCSDisableCrossfade = false;
	ORBIS_ONLY(BankBool sGrassComputeShader_DebugDrawIndirect = false);
	BankBool sGrassComputeShader_IgnoreLodScale_DrawableLOD = false;
	BankBool sGrassComputeShader_IgnoreLodScale_LODFade = false;
	BankBool sGrassComputeShader_UAV_Sync = false;

	struct CSParamsDH
	{
		CSParamsDH(EBStatic::GrassCSParams &params , u32 lodIdx)
		: m_CSParams(params)
		, m_LodIdx(lodIdx)
		GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(, m_CurrIndirectCount(0))
		{
		}

#	if GRASS_BATCH_CS_DYNAMIC_BUFFERS
		void IncrementArgsCount()
		{
			++m_CurrIndirectCount;
		}
#	endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS

		EBStatic::GrassCSParams &m_CSParams;
		u32 m_LodIdx;
		GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(u32 m_CurrIndirectCount);
	};

#if __BANK && (RSG_DURANGO || RSG_ORBIS)
	void ResetCSFrameCounters(EBStatic::CSParamsDH & UNUSED_PARAM(params))	//No accurate b/c this happens on render thread.
	{
		static u32 sCurrFrame = static_cast<u32>(-1);
		if(sCurrFrame != grcDevice::GetFrameCounter())
		{
			sPerfCSInstancesRenderedGbuf = sPerfCSZeroCountBatchesGbuf = 0;
			sPerfCSInstancesRenderedShadow = sPerfCSZeroCountBatchesShadow = 0;
			sPerfCSInstancesRenderedOther = sPerfCSZeroCountBatchesOther = 0;
			sCurrFrame = grcDevice::GetFrameCounter();
		}
	}

	void UpdateCSCounters(GRASS_BATCH_CS_DYNAMIC_BUFFERS_SWITCH(void *argBuf, grcBufferBasic *argBuf), EBStatic::CSParamsDH &GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(params))
	{
		//DBG Only - Possibly very slow!
		if(sPerfDisplayGrassCSInfo && sPerfDisplayRuntimeInfo && argBuf)
		{
			ResetCSFrameCounters(params);

			GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(if(params.m_CurrIndirectCount != 0) return);	//Count each model only once.

		#if DEVICE_GPU_WAIT
			if(sPerfDisplayGrassCS_UseGPUFence)
			{
				GRCDEVICE.GpuWaitOnPreviousWrites();
			}
		#endif
		#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
			IndirectArgParams *args = reinterpret_cast<IndirectArgParams *>(argBuf);
		#else
			#if RSG_DURANGO
				static u32 flags = (u32)grcsWrite | (u32)grcsRead;
				IndirectArgParams *args;
				argBuf->Lock(flags, 0, sizeof(IndirectArgParams), reinterpret_cast<void **>(&args));
			#else
				IndirectArgParams *args = reinterpret_cast<IndirectArgParams *>(argBuf->GetAddress());
			#endif
		#endif

			u32 instCount = args->m_instanceCount;

		#if RSG_DURANGO && !GRASS_BATCH_CS_DYNAMIC_BUFFERS
			argBuf->Unlock(flags);
		#endif

			const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
			const bool isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();
			if(isGBufferDrawList)
			{
				sPerfCSInstancesRenderedGbuf += instCount;
				if(instCount == 0) ++sPerfCSZeroCountBatchesGbuf;
			}
			else if(isShadowDrawList)
			{
				sPerfCSInstancesRenderedShadow += instCount;
				if(instCount == 0) ++sPerfCSZeroCountBatchesShadow;
			}
			else
			{
				sPerfCSInstancesRenderedOther += instCount;
				if(instCount == 0) ++sPerfCSZeroCountBatchesOther;
			}
		}
	}
#endif //__BANK && (RSG_DURANGO || RSG_ORBIS)
#endif //GRASS_BATCH_CS_CULLING

#if __BANK
	void AddDrawHandlerWidgets(bkBank &bank)
	{
		bank.PushGroup("Render Settings", false);
		{
			bank.PushGroup("Prop Instance Lists", false);
			{
				bank.AddToggle("Render Instance Lists", &sPropInstanceList_EnableDraw);
				bank.AddSlider("Force LOD:", &sPropInstanceList_ForceLod, -1, LOD_COUNT - 1, 1);
			}
			bank.PopGroup();

			bank.PushGroup("Grass Instance Lists", false);
			{
				bank.AddToggle("Render Instance Lists", &sGrassInstanceList_EnableDraw);
				GRASS_BATCH_CS_CULLING_ONLY(bank.AddToggle("CS: Only Run During Cutout Pass", &sPerfRunCSOnlyDuringCutout));
				GRASS_BATCH_CS_CULLING_ONLY(bank.AddToggle("Enable geometry render (CS still runs)", &sGrassRenderGeometry));
				bank.PushGroup("LODs");
				{
					auto begin = sGrassRenderLodGeometry.begin();
					auto end = sGrassRenderLodGeometry.end();
					for(auto iter = begin; iter != end; ++iter)
					{
						char buff[64];
						formatf(buff, "Render LOD %d Grass Geometry", iter - begin);
						bank.AddToggle(buff, &(*iter));
					}
				}
				bank.PopGroup();
			}
			bank.PopGroup();
		}
		bank.PopGroup();
	}
#endif //__BANK
}

//////////////////////////////////////////////////////////////////////////////
//
// Entity Batch Draw Handler Implementation
//

//CEntityBatchDrawHandler
dlCmdBase *CEntityBatchDrawHandler::AddToDrawList(fwEntity *pEntity, fwDrawDataAddParams *UNUSED_PARAM(pParams))
{
	dlCmdBase *pReturnDC = NULL;

	if(pEntity && EBStatic::sPropInstanceList_EnableDraw)
	{
		CEntityBatch *eb = static_cast<CEntityBatch *>(pEntity);
		if(grcInstanceBuffer *ib = eb->CreateInstanceBuffer())
		{
			DLC_Add(&grcInstanceBuffer::BeginInstancing);

			const int lod = (	EBStatic::sPropInstanceList_ForceLod < 0 ? eb->GetLod() :
								(GetDrawable() && GetDrawable()->GetLodGroup().ContainsLod(EBStatic::sPropInstanceList_ForceLod) ? EBStatic::sPropInstanceList_ForceLod : eb->GetLod())	);
			CCustomShaderEffectBase *cse = static_cast<CCustomShaderEffectBase *>(GetShaderEffect());

			//Start PIX tagging.
			if(InstancedRendererConfig::sAddPixBatchAnnotation && eb->GetInstanceList())
				DLC_Add(&InstancedEntityCache::PushPixTagsForBatch, eb->GetModelIndex(), eb->GetInstanceList()->m_InstanceList.size());

			//Add CSE to drawlist
			bool hasPerBatchShaderVars = cse && cse->HasPerBatchShaderVars();
			if(hasPerBatchShaderVars && InstancedRendererConfig::sAddCSEToDrawlist)
				cse->AddToDrawList(eb->GetModelIndex(), true);

			if(cse && InstancedRendererConfig::sCSESetBatchRenderStates)
				cse->AddBatchRenderStateToDrawList();

			GET_DLC(pReturnDC, CDrawEntityInstancedDC, (eb, ib, lod));

			if(cse && InstancedRendererConfig::sCSESetBatchRenderStates)
				cse->AddBatchRenderStateToDrawListAfterDraw();

			if(hasPerBatchShaderVars && InstancedRendererConfig::sAddCSEToDrawlist)
				cse->AddToDrawListAfterDraw();

			DLC(CCustomShaderEffectDC, (NULL));

			//End PIX tagging.
			if(InstancedRendererConfig::sAddPixBatchAnnotation && eb->GetInstanceList())
				DLC_Add(&InstancedEntityCache::PopPixTagsForBatch);	//DLCPopTimebar();

			DLC_Add(&grcInstanceBuffer::EndInstancing);
		}
	}

	return pReturnDC;
}

//////////////////////////////////////////////////////////////////////////////
//
// Grass Batch Draw Handler Implementation
//

dlCmdBase *CGrassBatchDrawHandler::AddToDrawList(fwEntity *pEntity, fwDrawDataAddParams *UNUSED_PARAM(pParams))
{
	dlCmdBase *pReturnDC = NULL;

	//AJG-Hack:	Fix for running grass CS during opaque pass.
	//			Code tries rendering grass for cutout & opaque, but currently, there's no opaque geometry. However, this still causes execution of the CS.
	//			This also causes asserts to trigger during the shadow opaque pass because we try setting shader vars while the shader shader is forced.
	GRASS_BATCH_CS_CULLING_ONLY(if(gDrawListMgr->GetUpdateBucket() != CRenderer::RB_CUTOUT && EBStatic::sPerfRunCSOnlyDuringCutout) return pReturnDC);

	if(pEntity && CGrassBatch::IsRenderingEnabled() && EBStatic::sGrassInstanceList_EnableDraw)
	{
		CGrassBatch *eb = static_cast<CGrassBatch *>(pEntity);

		// first check to see if we have been culled by an IPL, and if so, don't render.  This frees up
		// our ref's so that the box stream can unload us
		s32 mapIndex = eb->GetMapDataDefIndex();
		fwBoxStreamerVariable& boxStreamer = g_MapDataStore.GetBoxStreamer();
		if( boxStreamer.GetIsCulled( mapIndex ) )
		{
			return pReturnDC;
		}

		DLC_Add(&grcInstanceBuffer::BeginInstancing);

		//Start PIX tagging.
		if(InstancedRendererConfig::sAddPixBatchAnnotation && eb->GetInstanceList())
			DLC_Add(&InstancedEntityCache::PushPixTagsForBatch, eb->GetModelIndex(), eb->GetInstanceList()->m_InstanceList.size());
		
		//Add CSE to drawlist
		CCustomShaderEffectBase *cse = static_cast<CCustomShaderEffectBase *>(GetShaderEffect());
		if(cse)
		{
			cse->AddToDrawList(eb->GetModelIndex(), true);
			cse->AddBatchRenderStateToDrawList();
		}

		//Draw here!
		GET_DLC(pReturnDC, CDrawGrassBatchDC, (eb));
		BANK_ONLY(EBStatic::UpdateGrassCounters(eb->GetInstanceList()->m_InstanceList.size(), static_cast<CDrawGrassBatchDC *>(pReturnDC)));

		if(cse)
		{
			cse->AddBatchRenderStateToDrawListAfterDraw();
			cse->AddToDrawListAfterDraw();
			DLC(CCustomShaderEffectDC, (NULL));
		}

		//End PIX tagging.
		if(InstancedRendererConfig::sAddPixBatchAnnotation && eb->GetInstanceList())
			DLC_Add(&InstancedEntityCache::PopPixTagsForBatch);	//DLCPopTimebar();

		DLC_Add(&grcInstanceBuffer::EndInstancing);
	}
#if __BANK
	else
		EBStatic::ResetFrameCounters();
#endif

	return pReturnDC;
}

#if __PS3 && SPU_GCM_FIFO >= 2
namespace rage
{
	extern u32 grcCurrentVertexOffsets[grcVertexDeclaration::c_MaxStreams];
}
#endif // __PS3 && SPU_GCM_FIFO >= 2

#if __XENON || __D3D11 || RSG_ORBIS || __PS3
extern float g_crossFadeDistance[2];
namespace rage
{
#if RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
	extern D3D11X_DESCRIPTOR_UNORDERED_ACCESS_VIEW s_CommonStructuredBufferUAV;
	extern D3D11X_DESCRIPTOR_SHADER_RESOURCE_VIEW  s_CommonStructuredBufferSRV;
#endif
}

namespace EBStatic
{
	void DrawInstancedGrass(grmGeometry &geometry, GRASS_BATCH_CS_CULLING_SWITCH(EBStatic::CSParamsDH &params, const grcVertexBuffer *vb), u32 startInstance GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, u32 argsOffset))
	{
		const grcVertexBuffer *modelVerts = geometry.GetVertexBufferByIndex(0);
		XENON_ONLY(const grcVertexBuffer *indexVerts = geometry.GetVertexBufferByIndex(3));
		grcVertexDeclaration *decl = geometry.GetCustomDecl();
		if(modelVerts && decl XENON_ONLY(&& indexVerts))
		{
			u32 instanceCount = GRASS_BATCH_CS_CULLING_SWITCH(params.m_CSParams.m_Base.m_InstanceCount, vb->GetVertexCount());
			if(startInstance >= instanceCount GRASS_BATCH_CS_CULLING_ONLY(&& !params.m_CSParams.IsActive()))
				return;

			BANK_ONLY(startInstance = ApplyDebugInstanceCulling(instanceCount, startInstance));

#if __XENON
			int indexCount = indexVerts->GetVertexCount();
			int sharedCount = CGrassBatch::GetSharedInstanceCount();
			FLOAT instData[4] = { static_cast<float>(indexCount), static_cast<float>(sharedCount), static_cast<float>(instanceCount), 0 };
			GRCDEVICE.GetCurrent()->SetVertexShaderConstantF(INSTANCE_SHADER_CONTROL_SLOT, instData, 1);
#endif //__XENON

			GRCDEVICE.SetStreamSource(0, *modelVerts, 0, modelVerts->GetVertexStride());
#if !GRASS_BATCH_CS_CULLING
			GRCDEVICE.SetStreamSource(1, *vb, 0, vb->GetVertexStride());
#endif
			XENON_ONLY(GRCDEVICE.SetStreamSource(3, *indexVerts, 0, indexVerts->GetVertexStride()));

			GRCDEVICE.SetVertexDeclaration(decl);

#if __XENON
			u32 startVertex = indexCount * startInstance;
			CGrassBatch::BindSharedBuffer();
			GRCDEVICE.DrawPrimitive(static_cast<grcDrawMode>(geometry.GetPrimitiveType()), startVertex, (indexCount * instanceCount) - startVertex);
#elif __PS3
			CGrassBatch::BindSharedBuffer();
			if(const grcIndexBuffer *indexBuffer = geometry.GetIndexBufferByIndex(0))
			{
				const u32 indexCount = geometry.GetIndexCount();
				const u32 instancedIndexCount = static_cast<u32>(indexBuffer->GetIndexCount());
				const u32 numInstPerBatch = instancedIndexCount / indexCount;
				Assertf(numInstPerBatch == CGrassBatch::GetSharedInstanceCount(), "WARNING: Grass instance batch rendering on PS3 requires instanced batch count to be the same as the shared vert count!\t[numInstPerBatch = %d | sharedCount = %d]", numInstPerBatch, CGrassBatch::GetSharedInstanceCount());

				GRCDEVICE.SetIndices(*indexBuffer);

				//Clamp start instance to the beginning of a batch
				u32 startBatch = startInstance / numInstPerBatch;
				startInstance = startBatch * numInstPerBatch;

				for(u32 offset = startInstance; offset < instanceCount; offset += numInstPerBatch)
				{
					u32 numInstToDraw = Min(instanceCount - offset, numInstPerBatch);

					//Set instancing constant
					float instData[4] = { static_cast<float>(offset), static_cast<float>(numInstPerBatch), static_cast<float>(instanceCount), 0 };
					GRCDEVICE.SetVertexShaderConstant(INSTANCE_SHADER_CONTROL_SLOT, instData, 1);

					//Set stream offset
#if SPU_GCM_FIFO >= 2
					grcCurrentVertexOffsets[1] = reinterpret_cast<u32>(const_cast<grcVertexBuffer *>(vb)->GetVertexData()) + (offset * vb->GetVertexStride());
#else
					for(u32 attrib = 0; attrib < grcVertexDeclaration::c_MaxAttributes; ++attrib)
					{
						if(decl->Stream[attrib] == 1)	//Main Instance Streams
							decl->Offset[attrib] = static_cast<u8>(offset * vb->GetVertexStride());
					}
					GRCDEVICE.SetVertexDeclaration(decl);
#endif
					GRCDEVICE.DrawIndexedPrimitive(static_cast<grcDrawMode>(geometry.GetPrimitiveType()), 0, indexCount * numInstToDraw);
				}
			}
#else
			if(const grcIndexBuffer *indexBuffer = geometry.GetIndexBufferByIndex(0))
			{
				static BankBool sAlreadySetupPriorToDraw = true;
				int indexCount = geometry.GetIndexCount();
				grcDrawMode dm = static_cast<grcDrawMode>(geometry.GetPrimitiveType());
				GRCDEVICE.SetIndices(*indexBuffer);
				GRCDEVICE.SetUpPriorToDraw(dm);
				CGrassBatch::BindSharedBuffer();
#if GRASS_BATCH_CS_CULLING
				if(params.m_CSParams.IsActive())
				{
	#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
					FatalAssertf(params.m_CSParams.m_Base.m_IndirectArgLodOffsets[params.m_LodIdx] + params.m_CurrIndirectCount < params.m_CSParams.m_Base.m_IndirectArgCount, "Geometry count is higher than num allocated indirect buffers! This should never happen!");
					EBStatic::IndirectArgParams *indirectArgs = params.m_CSParams.m_IndirectBufferMem.getPtrSingleThreaded() + params.m_CSParams.m_Base.m_IndirectArgLodOffsets[params.m_LodIdx] + params.m_CurrIndirectCount;
					void *argBuf = reinterpret_cast<void *>(indirectArgs);

			#if RSG_ORBIS
					void *gpuAddr = reinterpret_cast<char *>(indirectArgs) + EBStatic::IndirectArgParams::sInstanceCountOffset;
					gfxc.waitOnAddress(gpuAddr, ~0U, sce::Gnm::kWaitCompareFuncNotEqual, EBStatic::IndirectArgParams::sInvalidInstanceCount);
			#endif
	#else //GRASS_BATCH_CS_DYNAMIC_BUFFERS
					grcBufferBasic *argBuf = params.m_CSParams.m_LocalIndirectArgBuffer;
					GRCDEVICE.SynchronizeComputeToGraphics();
					int index = params.m_LodIdx - static_cast<int>(params.m_CSParams.m_LODIdx);
					GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(if(!EBStatic::sGrassComputeShader_AllowCopyStructureCountBatching))
						GRCDEVICE.CopyStructureCount(argBuf, argsOffset + EBStatic::IndirectArgParams::sInstanceCountOffset, params.m_CSParams.m_AppendInstBuffer[index]);
	#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS

					//DBG Only!!
				#if __BANK && (RSG_DURANGO || RSG_ORBIS)
					EBStatic::UpdateCSCounters(argBuf, params);
				#endif //__BANK && (RSG_DURANGO || RSG_ORBIS)

					//Draw
					WIN32PC_ONLY(if (!sGrassComputeShader_UAV_Sync) GRCDEVICE.SetUAVSync(false);)
					GRCDEVICE.DrawIndexedInstancedIndirect(argBuf, dm, sAlreadySetupPriorToDraw GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, argsOffset));
					WIN32PC_ONLY(if (!sGrassComputeShader_UAV_Sync) GRCDEVICE.SetUAVSync(true);)

					//Increment current indirect arg count.
					GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(params.IncrementArgsCount());
				}
				else
#endif //GRASS_BATCH_CS_CULLING
				{
#if GRASS_BATCH_CS_CULLING && RSG_ORBIS
					if(EBStatic::sGrassComputeShader_DebugDrawIndirect)
					{
						sce::Gnm::DrawIndexIndirectArgs *indirectArgs = static_cast<sce::Gnm::DrawIndexIndirectArgs *>(gfxc.allocateFromCommandBuffer(4*5, sce::Gnm::kEmbeddedDataAlignment8));
						indirectArgs->m_indexCountPerInstance = indexCount;
						indirectArgs->m_instanceCount = instanceCount - startInstance;
						indirectArgs->m_startIndexLocation = 0;
						indirectArgs->m_baseVertexLocation = 0;
						indirectArgs->m_startInstanceLocation = 0;

						grcBufferBasic indirectArgsBuf(grcBuffer_Raw, true);
						indirectArgsBuf.Initialize(indirectArgs, sizeof(sce::Gnm::DrawIndexIndirectArgs), false);

						GRCDEVICE.DrawIndexedInstancedIndirect(&indirectArgsBuf, dm, sAlreadySetupPriorToDraw);
					}
					else
#endif //GRASS_BATCH_CS_CULLING && RSG_ORBIS
					GRCDEVICE.DrawInstancedIndexedPrimitive(dm, indexCount, instanceCount - startInstance, 0, 0, 0, sAlreadySetupPriorToDraw);
				}
			}
#endif

			PS3_ONLY(GRCDEVICE.ClearStreamSource(0));
			GRCDEVICE.ClearStreamSource(1);
#if !GRASS_BATCH_CS_CULLING
			GRCDEVICE.ClearStreamSource(2);
#endif
			XENON_ONLY(GRCDEVICE.ClearStreamSource(3));
		}
	}

	void DrawInstancedGrass(grmModel &model, const grmShaderGroup &group, GRASS_BATCH_CS_CULLING_SWITCH(EBStatic::CSParamsDH &params, const grcVertexBuffer *vb), u32 bucketMask, u32 startInstance GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, atArray<u32> &args))
	{
		const bool isUltraQuality = EBStatic::sQuality == CSettings::Ultra;
		const u32 ultraQualityBucketMask = isUltraQuality ? CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW) : 0;
		if(BUCKETMASK_MODELMATCH(model.GetMask() | ultraQualityBucketMask, bucketMask))
			return;

		u32 forceBucketMask = ~0U;
		if (grmModel::GetForceShader() && grmModel::GetForceShader()->InheritsBucketId() == false) 
			forceBucketMask = grmModel::GetForceShader()->GetDrawBucketMask();

		int count = model.GetGeometryCount();
		for(int i = 0; i < count; ++i)
		{
			int shaderIndex = model.GetShaderIndex(i);
			const grmShader *shader = (grmModel::GetForceShader()) ? grmModel::GetForceShader() : &group[shaderIndex];
			u32 shaderBucketMask = (forceBucketMask==~0U) ? group[shaderIndex].GetDrawBucketMask() : forceBucketMask;

			if(BUCKETMASK_MATCH(shaderBucketMask, bucketMask))
			{
				int numPasses = shader->BeginDraw(grmShader::RMC_DRAW, true);
				for(int p = 0; p < numPasses; ++p)
				{
					shader->Bind(p);
					grmGeometry &geometry = model.GetGeometry(i);
#if RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
					if(params.m_CSParams.IsActive())
					{
						if(params.m_CSParams.m_Base.m_Vars->m_idVarInstanceBuffer)
						{
							const grcEffectVarEntry &entry = group.GetShaderGroupVars()[static_cast<int>(params.m_CSParams.m_Base.m_Vars->m_idVarInstanceBuffer) - 1];
							auto iter = std::find_if(entry.Pairs.begin(), entry.Pairs.end(), [=](const grcEffectVarPair &p){ return p.index == shaderIndex; });
							if(iter != entry.Pairs.end())
							{
								const int paramIndex = static_cast<int>(iter->var) - 1;
								u32 slot = shader->GetInstanceData().GetBasis().GetLocalVar()[paramIndex].GetCBufOffset();
								int index = params.m_LodIdx - static_cast<int>(params.m_CSParams.m_LODIdx);

								D3D11X_DESCRIPTOR_SHADER_RESOURCE_VIEW descUAV = s_CommonStructuredBufferSRV;
								D3D11XUpdateDescriptorView(static_cast<D3D11X_UPDATE_VIEW>(D3D11X_UPDATE_VIEW_SET_ELEMENTS | D3D11X_UPDATE_VIEW_TYPE_BUFFER), &descUAV, &s_CommonStructuredBufferSRV, 0, params.m_CSParams.m_Base.m_InstanceCount, 0);
								g_grcCurrentContext->SetFastResources(static_cast<D3D11X_SET_FAST>(D3D11X_SET_FAST_VS_WITH_OFFSET | D3D11X_SET_FAST_TYPE_BUFFER_VIEW), slot, &descUAV, reinterpret_cast<uptr>(params.m_CSParams.m_AppendBufferMem[index]));
							}
						}
					}
#endif //RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
					DrawInstancedGrass(geometry, GRASS_BATCH_CS_CULLING_SWITCH(params, vb), startInstance GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, args[i]));
					shader->UnBind();
				}
				shader->EndDraw();
			}
		}
	}

#if GRASS_BATCH_CS_CULLING
	void SetInstanceBufferVar(rmcDrawable &drawable, const EBStatic::GrassCSParams &params, int lodIdx)
	{
		if(params.m_Base.m_Vars->m_idVarRawInstBuffer)
			drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarRawInstBuffer, params.m_Base.m_RawInstBuffer);

		if(params.IsActive())
		{
			if(params.m_Base.m_Vars->m_idVarInstanceBuffer)
			{
#if RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
				(void)lodIdx;
				drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarInstanceBuffer, static_cast<grcBufferUAV *>(NULL));
#else //RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
				int index = lodIdx - static_cast<int>(params.m_LODIdx);
				GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(ORBIS_ONLY(drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarInstanceBuffer, &(params.m_AppendDeviceBuffer[index]))));
				GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarInstanceBuffer, params.m_AppendInstBuffer[index]));
#endif //RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
			}

			if(params.m_Base.m_Vars->m_idVarUseCSOutputBuffer)
				drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarUseCSOutputBuffer, 1);
		}
		else
		{
			if(params.m_Base.m_Vars->m_idVarInstanceBuffer)
				drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarInstanceBuffer, static_cast<grcBufferUAV *>(NULL));

			static int sZero = 0;
			if(params.m_Base.m_Vars->m_idVarUseCSOutputBuffer)
				drawable.GetShaderGroup().SetVar(params.m_Base.m_Vars->m_idVarUseCSOutputBuffer, sZero);
		}
	}

#if RSG_ORBIS
	void CopyStructureCountAtEndOfShader(EBStatic::GrassCSParams &params GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, EBStatic::IndirectArgsDrawableOffsetMap &offsets_0))
	{
		const u32 gdsSizeInDwords = 1;
#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
		for(u32 i = params.m_LODIdx; i < params.m_AppendBufferMem.size(); ++i)
		{
			u32 bufferIdx = i - params.m_LODIdx;
			if(params.m_AppendBufferMem[bufferIdx] == NULL) continue;
			const u32 gdsOffsetInDwords = params.m_AppendDeviceBuffer[bufferIdx].m_GdsCounterOffset / 4;

			EBStatic::IndirectArgParams *indirectArgs = params.m_IndirectBufferMem.getPtrSingleThreaded();
			u32 argEnd = i + 1 < params.m_Base.m_IndirectArgLodOffsets.size() ? params.m_Base.m_IndirectArgLodOffsets[i + 1] : params.m_Base.m_IndirectArgCount;
			for(u32 argIdx = params.m_Base.m_IndirectArgLodOffsets[i]; argIdx < argEnd; ++argIdx)
			{
				void *destGpuAddr = reinterpret_cast<char *>(indirectArgs + argIdx) + EBStatic::IndirectArgParams::sInstanceCountOffset;
				gfxc.readDataFromGds(sce::Gnm::kEosCsDone, destGpuAddr, gdsOffsetInDwords, gdsSizeInDwords);
			}
		}
#elif GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
		for(int i = params.m_LODIdx; i < params.m_AppendInstBuffer.size(); ++i)
		{
			u32 appendIdx = i - params.m_LODIdx;
			if(params.m_AppendInstBuffer[appendIdx] == NULL) continue;
			const u32 gdsOffsetInDwords = params.m_AppendInstBuffer[appendIdx]->m_GdsCounterOffset / 4;

			const atArray<atArray<u32> > &offsets_1 = offsets_0[i];
			std::for_each(offsets_1.begin(), offsets_1.end(), [&, gdsOffsetInDwords, gdsSizeInDwords](const atArray<u32> &offsets_2){
				std::for_each(offsets_2.begin(), offsets_2.end(), [&, gdsOffsetInDwords, gdsSizeInDwords](u32 offset){
					void *destGpuAddr = reinterpret_cast<char *>(params.m_Base.m_IndirectArgsBuffer->GetAddress()) + offset + EBStatic::IndirectArgParams::sInstanceCountOffset;
					gfxc.readDataFromGds(sce::Gnm::kEosCsDone, destGpuAddr, gdsOffsetInDwords, gdsSizeInDwords);
				});
			});
		}
#endif
	}
#endif //RSG_ORBIS

#if GRASS_BATCH_CS_DYNAMIC_BUFFERS
	u32 FillIndirectArgs(const rmcDrawable &drawable, EBStatic::IndirectArgParams *args, u32 argCount)
	{
		u32 count = 0;
		const rmcLodGroup &lodGrp = drawable.GetLodGroup();
		for(int lodIdx = 0; lodIdx < LOD_COUNT; ++lodIdx)
		{
			if(lodGrp.ContainsLod(lodIdx))
			{
				const rmcLod &lod = lodGrp.GetLod(lodIdx);
				for(int modelIdx = 0; modelIdx < lod.GetCount(); ++modelIdx)
				{
					if(const grmModel *model = lod.GetModel(modelIdx))
					{
						for(int geoIdx = 0; geoIdx < model->GetGeometryCount(); ++geoIdx)
						{
							if(Verifyf(count < argCount, "Not enough indirect args for drawable's geometry count."))
							{
								grmGeometry &geometry = model->GetGeometry(geoIdx);
								rage_placement_new(args + count) EBStatic::IndirectArgParams();
								args[count++].m_indexCountPerInstance = geometry.GetIndexCount();
							}
						}
					}
				}
			}
		}

		//Flush was only necessary before when indirect args were allocated from drawlist mem. Indirect args are now allocated from temp memory.
		//Flush CPU cache for indirect args to make sure initial values are updated.
		//DURANGO_ONLY(D3DFlushCpuCache(reinterpret_cast<void *>(args), argCount * sizeof(EBStatic::IndirectArgParams)));

		return count;
	}
#endif //GRASS_BATCH_CS_DYNAMIC_BUFFERS

#if ENABLE_PIX_TAGGING
	const char *GetPixTagForLod(u32 lod)
	{
		static const char *sLodPixTag[LOD_COUNT] = {
			"LOD 0",
			"LOD 1",
			"LOD 2",
			"LOD 3",
		};

		if(lod < LOD_COUNT)
			return sLodPixTag[lod];

		return "<Invalid LOD>";
	}
#endif //ENABLE_PIX_TAGGING

	void PushPixTagsForDispatch(u32 PIX_TAGGING_ONLY(modelIndex), u32 PIX_TAGGING_ONLY(batchSize))
	{
		//Don't do extra tagging work if not necessary!!
#if ENABLE_PIX_TAGGING
		char batchName[128];
		fwModelId modelId((strLocalIndex(modelIndex)));
		CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
		sprintf(batchName, "%s Dispatch - %d Instances", pModelInfo ? pModelInfo->GetModelName() : "<InvalidModel>", batchSize);

		PF_PUSH_MARKER(batchName);
#else
		PF_PUSH_MARKER("GrassBatch Instance Culling - CS");
#endif
	}

	void PopPixTagsForDispatch()
	{
		PF_POP_MARKER();
	}

	inline u32 GetModelIndexForCS(EBStatic::GrassCSParams &PIX_TAGGING_ONLY(params))
	{
#if ENABLE_PIX_TAGGING
		return params.m_Base.m_ModelIndex;
#else
		return static_cast<u32>(-1);
#endif
	}

	void DispatchComputeShader(rmcDrawable &drawable, EBStatic::GrassCSParams &params)
	{
		if(params.IsActive())
		{
			PushPixTagsForDispatch(GetModelIndexForCS(params), params.m_Base.m_InstanceCount);

			grmShaderGroup &shaderGroup = drawable.GetShaderGroup();
			grmShader &shader = shaderGroup.GetShader(params.m_Base.m_Vars->m_ShaderIndex);

			//Bind Vars
			Vec3V camPos(V_ZERO_WONE);
			Vec2V instCullParams(params.m_Base.m_InstAabbRadius, 1.0f);
			if(params.m_CurrVp)
			{
#if NV_SUPPORT
				if(GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled())
				{
					Vec4V cullClipPlanes[6];
					cullClipPlanes[0] = params.m_CurrVp->GetCullFrustumClipPlane(grcViewport::CLIP_PLANE_LEFT);
					cullClipPlanes[1] = params.m_CurrVp->GetCullFrustumClipPlane(grcViewport::CLIP_PLANE_RIGHT);
					cullClipPlanes[2] = params.m_CurrVp->GetFrustumClipPlane(grcViewport::CLIP_PLANE_TOP);
					cullClipPlanes[3] = params.m_CurrVp->GetFrustumClipPlane(grcViewport::CLIP_PLANE_BOTTOM);
					cullClipPlanes[4] = params.m_CurrVp->GetFrustumClipPlane(grcViewport::CLIP_PLANE_NEAR);
					cullClipPlanes[5] = params.m_CurrVp->GetFrustumClipPlane(grcViewport::CLIP_PLANE_FAR);				

					shader.SetVar(params.m_Base.m_Vars->m_idClipPlanes, cullClipPlanes, EBStatic::sGrassComputeShader_NumClipPlanesForNonShadowVp);
				}
				else
#endif
				shader.SetVar(params.m_Base.m_Vars->m_idClipPlanes, params.m_CurrVp->GetFrustumClipPlaneArray(), EBStatic::sGrassComputeShader_NumClipPlanesForNonShadowVp);

				shader.SetVar(params.m_Base.m_Vars->m_idNumClipPlanes, EBStatic::sGrassComputeShader_NumClipPlanesForNonShadowVp);
				camPos = params.m_CurrVp->GetCameraPosition();
			}
			else
			{
				shader.SetVar(params.m_Base.m_Vars->m_idNumClipPlanes, static_cast<int>(CRenderPhaseCascadeShadowsInterface::GetClipPlaneCount()));
				shader.SetVar(params.m_Base.m_Vars->m_idClipPlanes, CRenderPhaseCascadeShadowsInterface::GetClipPlaneArray(), CASCADE_SHADOWS_SCAN_NUM_PLANES);
				instCullParams.SetY(ScalarV(V_NEGONE));
				camPos = gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition();
			}

			shaderGroup.SetVar(params.m_Base.m_Vars->m_idVarRawInstBuffer, params.m_Base.m_RawInstBuffer);
			for(int i = 0; i < params.m_Base.m_Vars->m_idVarAppendInstBuffer.size(); ++i)
			{
#if RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
				shader.SetVarUAV(params.m_Base.m_Vars->m_idVarAppendInstBuffer[i], NULL, 0);
#else //RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
				if(params. GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(m_AppendBufferMem) GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(m_AppendInstBuffer) [i] == NULL)
					shader.SetVarUAV(params.m_Base.m_Vars->m_idVarAppendInstBuffer[i], NULL, 0);
				else
				{
					GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(ORBIS_ONLY(shader.SetVarUAV(params.m_Base.m_Vars->m_idVarAppendInstBuffer[i], &(params.m_AppendDeviceBuffer[i]), 0)));
					GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(shader.SetVarUAV(params.m_Base.m_Vars->m_idVarAppendInstBuffer[i], params.m_AppendInstBuffer[i], 0));
				}
#endif //RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
			}
			DURANGO_ONLY(GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(shader.SetVarUAV(params.m_Base.m_Vars->m_idIndirectArgs, NULL, 0)));
			shader.SetVar(params.m_Base.m_Vars->m_idInstCullParams, instCullParams);

#if RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
			//LOD Offsets
			u32 indirectArgLodOffset = params.m_Base.m_IndirectArgLodOffsets[params.m_LODIdx];
			IntVector lodOffsets = {
					(params.m_LODIdx + 1 < params.m_Base.m_IndirectArgLodOffsets.GetMaxCount() ? params.m_Base.m_IndirectArgLodOffsets[params.m_LODIdx + 1] : params.m_Base.m_IndirectArgCount) - indirectArgLodOffset,
					(params.m_LODIdx + 2 < params.m_Base.m_IndirectArgLodOffsets.GetMaxCount() ? params.m_Base.m_IndirectArgLodOffsets[params.m_LODIdx + 2] : params.m_Base.m_IndirectArgCount) - indirectArgLodOffset,
					(params.m_LODIdx + 3 < params.m_Base.m_IndirectArgLodOffsets.GetMaxCount() ? params.m_Base.m_IndirectArgLodOffsets[params.m_LODIdx + 3] : params.m_Base.m_IndirectArgCount) - indirectArgLodOffset,
					(params.m_LODIdx + 4 < params.m_Base.m_IndirectArgLodOffsets.GetMaxCount() ? params.m_Base.m_IndirectArgLodOffsets[params.m_LODIdx + 4] : params.m_Base.m_IndirectArgCount) - indirectArgLodOffset	};
			shader.SetVar(params.m_Base.m_Vars->m_idIndirectCountPerLod, lodOffsets);
#endif //RSG_DURANGO

			const spdAABB &aabb = params.m_Base.m_InstanceList->m_BatchAABB;
			Vec3V aabbMin = aabb.GetMin();																				aabbMin.SetWZero();
			Vec3V aabbDelta = aabb.GetMax() - aabb.GetMin();															aabbDelta.SetWZero();
			Vec3V scaleRange = params.m_Base.m_InstanceList->m_ScaleRange;												scaleRange.SetWZero();
			shaderGroup.SetVar(params.m_Base.m_Vars->m_idAabbMin, aabbMin);
			shaderGroup.SetVar(params.m_Base.m_Vars->m_idAabbDelta, aabbDelta);
			shaderGroup.SetVar(params.m_Base.m_Vars->m_idScaleRange, scaleRange);

			const u32 phaseLODs = params.m_phaseLODs;
			const LODDrawable::crossFadeDistanceIdx fadeDist = params.m_UseAltfadeDist ? LODDrawable::CFDIST_ALTERNATE : LODDrawable::CFDIST_MAIN;

			const u32 LODFlag = (phaseLODs >> dlCmdNewDrawList::GetCurrDrawListLODFlagShift()) & LODDrawable::LODFLAG_MASK;
			const bool isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();
			const bool lodTransitionInstantly = (LODFlag != LODDrawable::LODFLAG_NONE || isShadowDrawList) || EBStatic::sCSDisableCrossfade;
#if RSG_DURANGO|| RSG_PC
			const s32 grassSkipInstance = (s32)sPerfSkipInstance;
#endif // RSG_DURANGO || RSG_PC...
			atRangeArray<float, LOD_COUNT> thresholds(-1.0f);
			float fScale = sGrassComputeShader_IgnoreLodScale_DrawableLOD ? 1.0f : g_LodScale.GetGrassBatchScaleForRenderThread();
			u32 iterationEnd = 1 + (params.m_lastLODIdx - params.m_LODIdx);
			for(u32 i = 0; i < iterationEnd; ++i)
			{
				u32 lod = params.m_LODIdx + i;
				if(drawable.GetLodGroup().ContainsLod(lod))
					thresholds[i] = drawable.GetLodGroup().GetLodThresh(lod) * fScale;
			}

			Vec2V crossFadeDistance(g_crossFadeDistance[0], g_crossFadeDistance[fadeDist]);
			Vec4V lodThresholds(thresholds[0], thresholds[1], thresholds[2], thresholds[3]);
			shader.SetVar(params.m_Base.m_Vars->m_idCameraPosition, camPos);
			shader.SetVar(params.m_Base.m_Vars->m_idLodInstantTransition, lodTransitionInstantly ? 1 : 0);
#if RSG_DURANGO || RSG_PC
			shader.SetVar(params.m_Base.m_Vars->m_idGrassSkipInstance, grassSkipInstance);
#endif // RSG_DURANGO || RSG_PC...
			shader.SetVar(params.m_Base.m_Vars->m_idLodThresholds, lodThresholds);
			shader.SetVar(params.m_Base.m_Vars->m_idCrossFadeDistance, crossFadeDistance);

			const bool isUltraQuality = EBStatic::sQuality == CSettings::Ultra;
			ScalarV lodScale(sGrassComputeShader_IgnoreLodScale_LODFade ? 1.0f : g_LodScale.GetGrassBatchScaleForRenderThread());
			Vec2V lodFadeControlRange(params.m_Base.m_InstanceList->m_LodFadeStartDist, static_cast<float>(params.m_Base.m_InstanceList->m_lodDist));
			Vec3V lodFadeControlRangeScale(lodFadeControlRange * lodScale, lodScale);
			shader.SetVar(params.m_Base.m_Vars->m_idIsShadowPass, isShadowDrawList && !isUltraQuality ? 1 : 0);
			shader.SetVar(params.m_Base.m_Vars->m_idLodFadeControlRange, lodFadeControlRangeScale);

			//Bind compute shader
			static const u32 pass = 0;
			grcComputeProgram *const pComputeProgram = shader.GetComputeProgram(pass);
			pComputeProgram->Bind(shader.GetInstanceData(), shader.GetInstanceData().GetBasis().GetLocalVar());

			//Bind Additional Buffers
			CGrassBatch::BindSharedBuffer();

			//Allocate & fill indirect arg buffer
			GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(EBStatic::FillIndirectArgs(drawable, params.m_IndirectBufferMem.getPtrSingleThreaded(), params.m_Base.m_IndirectArgCount));

#if RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS
			//Durango Placement UAVs
			for(int i = 0; i < params.m_Base.m_Vars->m_idVarAppendInstBuffer.size(); ++i)
			{
				if(params.m_AppendBufferMem[i] != NULL)
				{
					//Update descriptor for buffers.
					D3D11X_DESCRIPTOR_UNORDERED_ACCESS_VIEW descUAV = s_CommonStructuredBufferUAV;
					D3D11XUpdateDescriptorView(static_cast<D3D11X_UPDATE_VIEW>(D3D11X_UPDATE_VIEW_SET_STRIDE | D3D11X_UPDATE_VIEW_SET_ELEMENTS | D3D11X_UPDATE_VIEW_TYPE_BUFFER), &descUAV, &s_CommonStructuredBufferUAV,  D3D11X_MAKE_STRIDE(GrassBatchCSInstanceData_Size), params.m_Base.m_InstanceCount, 0);
					g_grcCurrentContext->SetFastResources(static_cast<D3D11X_SET_FAST>(D3D11X_SET_FAST_CS_WITH_OFFSET | D3D11X_SET_FAST_TYPE_BUFFER_VIEW), i, &descUAV, reinterpret_cast<uptr>(params.m_AppendBufferMem[i]));
				}
			}

			//Update descriptor for buffers.
			D3D11X_DESCRIPTOR_UNORDERED_ACCESS_VIEW descUAV = s_CommonStructuredBufferUAV;
			D3D11XUpdateDescriptorView(static_cast<D3D11X_UPDATE_VIEW>(D3D11X_UPDATE_VIEW_SET_STRIDE | D3D11X_UPDATE_VIEW_SET_ELEMENTS | D3D11X_UPDATE_VIEW_TYPE_BUFFER), &descUAV, &s_CommonStructuredBufferUAV, D3D11X_MAKE_STRIDE(sizeof(EBStatic::IndirectArgParams)), params.m_Base.m_IndirectArgCount, 0);
			g_grcCurrentContext->SetFastResources(static_cast<D3D11X_SET_FAST>(D3D11X_SET_FAST_CS_WITH_OFFSET | D3D11X_SET_FAST_TYPE_BUFFER_VIEW), params.m_AppendBufferMem.GetMaxCount(), &descUAV, reinterpret_cast<uptr>(params.m_IndirectBufferMem.getPtrSingleThreaded() + indirectArgLodOffset));
#endif //RSG_DURANGO && GRASS_BATCH_CS_DYNAMIC_BUFFERS

			//Dispatch
			const u32 numThreadGroupX = static_cast<u32>(ceil(static_cast<float>(params.m_Base.m_InstanceCount) / static_cast<float>(GRASS_BATCH_CS_THREADS_PER_WAVEFRONT)));
			GRCDEVICE.Dispatch(numThreadGroupX, 1, 1);

			//Special sync for PS4.
			ORBIS_ONLY(CopyStructureCountAtEndOfShader(params GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, *params.m_Base.m_OffsetMap)));

#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
			if (!EBStatic::sGrassComputeShader_AvoidInterleaving)
				g_grcCurrentContext->CopySubresourceRegion(params.m_LocalIndirectArgBuffer->GetD3DBuffer(), 0, 0, 0, 0, params.m_Base.m_IndirectArgsBuffer->GetD3DBuffer(), 0, NULL);
#endif // GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS

			if(sFlushCSAfterEachDispatch)
			{
				GRCDEVICE.SynchronizeComputeToGraphics();
				DURANGO_ONLY(g_grcCurrentContext->InsertWaitUntilIdle(0));
			}

			PopPixTagsForDispatch();
		}
	}
#endif //GRASS_BATCH_CS_CULLING
}

void CGrassBatchDrawHandler::DrawInstancedGrass(rmcDrawable &drawable, GRASS_BATCH_CS_CULLING_SWITCH(EBStatic::GrassCSParams &params, const grcVertexBuffer *vb), u32 bucketMask, int lodIdx, u32 startInstance)
{
	FastAssert(sysThreadType::IsRenderThread());

	BANK_ONLY(if(GRASS_BATCH_CS_CULLING_ONLY(!EBStatic::sGrassRenderGeometry ||) !EBStatic::sGrassRenderLodGeometry[lodIdx]) return);

#if GRASS_BATCH_CS_CULLING
	if(!EBStatic::sGrassComputeShader_AllowBatching && params.m_LODIdx == static_cast<u32>(lodIdx))
	{
		ORBIS_ONLY(gfxc.setShaderType(sce::Gnm::kShaderTypeCompute));
		EBStatic::DispatchComputeShader(drawable, params);
		ORBIS_ONLY(gfxc.setShaderType(sce::Gnm::kShaderTypeGraphics));
	}
#endif //GRASS_BATCH_CS_CULLING

	if(GRASS_BATCH_CS_CULLING_SWITCH(params.m_Base.IsValid(), vb))	//Only check if params base is valid because we need the buffers & shader var ids to set the raw buffer. But Don't care if not active b/c we can draw w/o compute shader.
	{
		GRASS_BATCH_CS_CULLING_ONLY(PF_PUSH_MARKER(EBStatic::GetPixTagForLod(lodIdx));)
		GRASS_BATCH_CS_CULLING_ONLY(EBStatic::SetInstanceBufferVar(drawable, params, lodIdx));
		GRASS_BATCH_CS_CULLING_ONLY(WIN32PC_ONLY(Assertf(GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50), "Hardware doesn't support compute shader")));
		rmcLodGroup &lodGrp = drawable.GetLodGroup();
		const grmShaderGroup &shaderGrp = drawable.GetShaderGroup();
		const bool isUltraQuality = EBStatic::sQuality == CSettings::Ultra;
		const u32 ultraQualityBucketMask = isUltraQuality ? rage::dlDrawListMgr::PackBucketMask(0, CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW)) : 0;
		if(lodGrp.ContainsLod(lodIdx) && BUCKETMASK_MATCH(lodGrp.GetBucketMask(lodIdx) | ultraQualityBucketMask, bucketMask))
		{
			rmcLod &lod = lodGrp.GetLod(lodIdx);
			int count = lod.GetCount();
			for (int i = 0; i < count; ++i)
			{
				if(lod.GetModel(i))
				{
					grmModel &model = *lod.GetModel(i);
					GRASS_BATCH_CS_CULLING_ONLY(EBStatic::CSParamsDH dhParams(params, lodIdx));
					EBStatic::DrawInstancedGrass(model, shaderGrp, GRASS_BATCH_CS_CULLING_SWITCH(dhParams, vb), bucketMask, startInstance GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS_ONLY(, (*params.m_Base.m_OffsetMap)[lodIdx][i]));
				}
			}
		}

		GRASS_BATCH_CS_CULLING_ONLY(PF_POP_MARKER();)
	}
}
#else //__XENON || __D3D11 || RSG_ORBIS || __PS3
void CGrassBatchDrawHandler::DrawInstancedGrass(rmcDrawable &UNUSED_PARAM(drawable), GRASS_BATCH_CS_CULLING_SWITCH(EBStatic::GrassCSParams &UNUSED_PARAM(params), const grcVertexBuffer *UNUSED_PARAM(vb)), u32 UNUSED_PARAM(bucketMask), int UNUSED_PARAM(lod), u32 UNUSED_PARAM(startInstance))
{
	//Assertf(false, "CEntityBatchDrawHandler::DrawInstancedGrass() - Not Implemented!");
}
#endif //__XENON || __D3D11 || RSG_ORBIS || __PS3

#if GRASS_BATCH_CS_CULLING
void CGrassBatchDrawHandler::DispatchComputeShader(rmcDrawable &drawable, EBStatic::GrassCSParams &params)
{
	FastAssert(sysThreadType::IsRenderThread());

	if(EBStatic::sGrassComputeShader_AllowBatching)
		EBStatic::DispatchComputeShader(drawable, params);
}

#if __WIN32PC
void CGrassBatchDrawHandler::CopyStructureCount(EBStatic::GrassCSParams &params)
{
#if GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
	if (!params.IsActive())
		return;

#if NV_SUPPORT
	if( params.m_LocalIndirectArgBuffer->bNeedsNVFlaging )
	{
		params.m_LocalIndirectArgBuffer->FlagForNoSLITransfer();
	}
#endif // NV_SUPPORT

	if (EBStatic::sGrassComputeShader_AvoidInterleaving)
	{
		g_grcCurrentContext->CopySubresourceRegion(params.m_LocalIndirectArgBuffer->GetD3DBuffer(), 0, 0, 0, 0, params.m_Base.m_IndirectArgsBuffer->GetD3DBuffer(), 0, NULL);
	}

	if(EBStatic::sGrassComputeShader_AllowCopyStructureCountBatching)
	{
		for(u32 i = params.m_LODIdx; i < params.m_AppendInstBuffer.size(); ++i)
		{
			u32 bufferIdx = i - params.m_LODIdx;
			if(params.m_AppendInstBuffer[bufferIdx] == NULL) continue;
			
			const atArray<atArray<u32> > &offsets_1 = (*params.m_Base.m_OffsetMap)[i];
			std::for_each(offsets_1.begin(), offsets_1.end(), [&, bufferIdx](const atArray<u32> &offsets_2)
			{
				const u32 _bufferIdx = bufferIdx;
				std::for_each(offsets_2.begin(), offsets_2.end(), [&, _bufferIdx](u32 offset)
				{
					GRCDEVICE.CopyStructureCount(params.m_LocalIndirectArgBuffer, offset + EBStatic::IndirectArgParams::sInstanceCountOffset, params.m_AppendInstBuffer[_bufferIdx]);
				});
			});
		}
	}
#endif //GRASS_BATCH_CS_PRE_ALLOCATE_BUFFERS
}
#endif //__WIN32PC
#endif //GRASS_BATCH_CS_CULLING

#if __BANK
void CEntityBatchDrawHandler::RenderDebug()
{
	//EBStatic::RenderDrawHandlerDebug();
}

void CEntityBatchDrawHandler::AddWidgets(bkBank &bank)
{
	EBStatic::AddDrawHandlerWidgets(bank);
}
void CEntityBatchDrawHandler::SetGrassBatchEnabled(bool bEnabled)
{
	EBStatic::sGrassInstanceList_EnableDraw=bEnabled;
}
#endif //__BANK
