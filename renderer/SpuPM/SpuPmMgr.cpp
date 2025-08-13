//
//  Basic framework for SPU/RSX synchronization.
//	SPU postprocessing pipeline with custom Policy Module:
//	- blits RSX textures from VRam into temp buffers in MainRam;
//	- SPUs process temp buffers into dest textures in VRam;
//	- dest textures can be re-used by RSX;
//
//	2008/12/22	-	Andrzej:	- initial;
//	2009/01/20	-	Andrzej:	- first working version with fully synchronized lists of PPU-allocated buffers
//									freed by SPU and RSX;
//	2009/09/30	-	Andrzej:	- Deferred lighting on SPU;
//
//
//
//
#include "vector/vector4.h"
#include "SpuPmMgr.h"

#if __PPU && SPUPMMGR_ENABLED

#include <sys/timer.h>
#include <cell/spurs/workload.h>
#include <cell/spurs/ready_count.h>
#include <cell/spurs/control.h>
#include <cell/atomic.h>
#include <cell/gcm.h>
using namespace cell::Gcm;

#include "system/simpleallocator.h"
#include "grcore/wrapper_gcm.h"
#include "grcore/grcorespu.h"
#include "grcore/texturegcm.h"
#include "grcore/image.h"

#include "debug/rendering/DebugDeferred.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Lights/TiledLighting.h"

//#include "system/findsize.h"
//FindSize(CSPUPostProcessDriverInfo);	// 144
//FindSize(CSPUDefLight);				// 80
//FindSize(CSPUDeferredLightInfoBase);	// 128
//FindSize(CSPUDeferredLightInfo);		// 20608
CompileTimeAssert(CSPULightClassificationInfo::MAX_LIGHTS==MAX_STORED_LIGHTS);

//
// size of custom buffer used for temp work buffers for SPU:
//


#if SPUPMMGR_TEST
// simple processing test:
#include "renderer/sprite2d.h"

#if __DEV
CElfInfo* CSpuPmMgr::spupmReloadedElf = NULL;
u32 CSpuPmMgr::reloadSlot = 0;
bool CSpuPmMgr::bReloadElfBuf = true;
u8*  CSpuPmMgr::pReloadElf[2] = { NULL, NULL };
#endif

#define PSN_GENTEX_SPU_TASK_TEX_WIDTH		(128)
#define PSN_GENTEX_SPU_TASK_TEX_HEIGHT		(128)
static grcSpuDestTexture	*g_SpuGentexTexture	= NULL;

static dev_bool gbProcessSPU_Enable = FALSE;
static dev_bool gbProcessSPU_Display= FALSE;

namespace rage
{
	extern __THREAD CellGcmContextData* g_grcCurrentContext;
}
//
//
//
//
void SpuPM_ProcessTestTexture()
{
	// test dest texture:
	if((!g_SpuGentexTexture) && (gbProcessSPU_Enable || gbProcessSPU_Display))
	{
		grcImage *checkerImg = grcImage::MakeChecker(PSN_GENTEX_SPU_TASK_TEX_WIDTH,Color32(255,0,0,255), Color32(255,255,255,255));

		grcTextureFactory::TextureCreateParams createParams(
			grcTextureFactory::TextureCreateParams::VIDEO/*SYSTEM*/,
			grcTextureFactory::TextureCreateParams::LINEAR,
			0);

		// ARGB8888:
		g_SpuGentexTexture = rage_new grcSpuDestTexture();
		grcTextureGCM *genTex = (grcTextureGCM*)grcTextureFactory::GetInstance().Create(checkerImg, &createParams);
		Assert(genTex);
		g_SpuGentexTexture->SetTexture(genTex);

		checkerImg->Release();
		checkerImg = NULL;
	}

	// add processing job:
	if(g_SpuGentexTexture && gbProcessSPU_Enable)
	{
		// grab source:
		grcRenderTargetGCM *mrt0 = (grcRenderTargetGCM*)GBuffer::GetTarget(GBUFFER_RT_0);
		Assert(mrt0);
		CellGcmTexture *cellMrtTex0 = (CellGcmTexture*)mrt0->GetTexturePtr();
		Assert(cellMrtTex0);

		u32 testUserData[16]={101,202,303,404,4,5,6,7,8,9,10,11,12,13,14,15};

		CSpuPmMgr::PostProcessTexture(spupmProcessSPU, 1, NULL, testUserData, sizeof(testUserData),
			/*dst*/g_SpuGentexTexture, /*src*/cellMrtTex0, 
			/*x*/1216, /*y*/296, /*w*/128, /*h*/128, /*bpp*/4
			#if ENABLE_PIX_TAGGING
			,"ProcessSPU"
			#endif
			);
	}

}// end of PM_Update()...

//
//
// debug render of dest texture (called from CRenderPhaseDebug2d::StaticRender());
//
void SpuPM_RenderTestTexture()
{
	if(g_SpuGentexTexture && gbProcessSPU_Display)
	{
		// destTex: it should be called when destTex is about to be used:
		g_SpuGentexTexture->WaitForSPU();

		CSprite2d sprite;
		sprite.SetTexture(g_SpuGentexTexture->GetTexture());
		sprite.SetRenderState();

		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

static grcBlendStateHandle hSpuPmBlendState = grcStateBlock::BS_Invalid;
		if(hSpuPmBlendState == grcStateBlock::BS_Invalid)
		{
			grcBlendStateDesc spuPmBSDesc;
			spuPmBSDesc.AlphaTestEnable				= false;
			spuPmBSDesc.BlendRTDesc[0].BlendEnable	= false;
			hSpuPmBlendState = grcStateBlock::CreateBlendState(spuPmBSDesc);
			Assert(hSpuPmBlendState != grcStateBlock::BS_Invalid);
		}
		grcStateBlock::SetBlendState(hSpuPmBlendState);

		float x=800+256, y=100;
		float xMin = x/float(SCREEN_WIDTH);
		float yMin = y/float(SCREEN_HEIGHT);
		float xMax = (x+PSN_GENTEX_SPU_TASK_TEX_WIDTH*1.5f)/float(SCREEN_WIDTH);
		float yMax = (y+PSN_GENTEX_SPU_TASK_TEX_HEIGHT*1.5f)/float(SCREEN_HEIGHT);
		
		Vector2 p1(xMin, yMin);
		Vector2 p2(xMin, yMax);
		Vector2 p3(xMax, yMin);
		Vector2 p4(xMax, yMax);

		Vector2 uv1(0.0f, 0.0f);
		Vector2 uv2(0.0f, 1.0f);
		Vector2 uv3(1.0f, 0.0f);
		Vector2 uv4(1.0f, 1.0f);

		CSprite2d::DrawRect(p1,p2,p3,p4,0.0f,uv1,uv2,uv3,uv4,Color32(255,255,255,255));
	}
}
#endif //SPUPMMGR_TEST...

#if __BANK
//
//
// debug widgets:
//
bool CSpuPmMgr::InitWidgets(bkBank& bank)
{
	//	bkBank& bank = *BANKMGR.FindBank("Renderer");
	bank.PushGroup("SpuPmMgr", false);
#if SPUPMMGR_TEST
		bank.AddToggle("(test) Enable ProcessSPU on MRT0",			&gbProcessSPU_Enable);
		bank.AddToggle("(test) Display ProcessSPU final texture",	&gbProcessSPU_Display);
#endif //SPUPMMGR_TEST...
		DebugDeferred::AddWidgets_SpuPmElfReload(&bank);
//		DebugDeferred::AddWidgets_LightClassification(&bank);
		
	bank.PopGroup();

	return(TRUE);
}
#endif //__BANK...




////////////////////////////////////////////////////////////////////////////////////////////////

namespace rage
{
	extern CellSpurs *g_Spurs;
};


// Pointer to the Spurs policy module.
extern const char _spupmmgr_pm[];
// Size of the Spurs policy module.
extern char _spupmmgr_pm_size[];

#define PM_MALLOC_ALIGN(SIZE,ALIGN)		rage_aligned_new((ALIGN)) u8[(SIZE)]
#define PM_FREE_ALIGN(P)				delete [] ((u8*)(P))
#define PM_GCM_OFFSET					gcm::MainOffset

#define BWB_ALLOC(SIZE,ALIGN)				sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->Allocate((SIZE),(ALIGN),0)
#define BWB_FREE(P)							sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->Free((P))
#define BWB_GCM_OFFSET(P)					gcm::MainOffset(P)
#define BWB_GETSIZE(P)						sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetSize((P))
#define BWB_GETMEMUSED()					sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(-1)
#define BWB_GETMEMAVAILABLE()				sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryAvailable()
#define BWB_GETLARGESTAVAILABLEBLOCK()		sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetLargestAvailableBlock()


CLinkList<CWBAllocBlockHeader>				CSpuPmMgr::gbWBAllocList;		// list of allocated buffers in BigWorkBuffer:
CLinkList<CSPUPostProcessDriverInfo>		CSpuPmMgr::gbPostProcessDriverInfoList;
CLinkList<CJtsBlocker>						CSpuPmMgr::gbJtsBlockerList;


// job queues:
CSPUJobQueuePushPullPos	CSpuPmMgr::ms_jobQueueStandard;
CSPUJobQueuePushPullPos	CSpuPmMgr::ms_jobQueueGPU;

u32	CSpuPmMgr::PM_args[8] ALIGNED(128);

//
//
//
//
bool CSpuPmMgr::Init()
{
	// this must be called from RenderThread:
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	gbWBAllocList.Init(256);
	gbPostProcessDriverInfoList.Init(512);
	gbJtsBlockerList.Init(512);


	// initialize stuff for getRsxFence():
	rsxWaitTextureLabelIndex	= gcm::RsxSemaphoreRegistrar::Allocate();			// index of RSX label1
	rsxWaitTextureLabelPtr		= cellGcmGetLabelAddress(rsxWaitTextureLabelIndex);		// ptr of the label in RSX memory
	Assert(rsxWaitTextureLabelPtr);
	ASSERT16(rsxWaitTextureLabelPtr); // enough to be 16 byte aligned for cellDmaSmallGet/Put()


	sm_gpuFencePtr		= (u32*)PM_MALLOC_ALIGN(sizeof(u32), 16);
	sm_gpuFenceOffset	= gcm::MainOffset(sm_gpuFencePtr);
	Assert(sm_gpuFenceOffset);
	*sm_gpuFencePtr = 0;

// defined in system\task_psn.cpp
CellSpurs *pSpurs = g_Spurs;

	if(!createWorkload(pSpurs))
		return(FALSE);

	return(TRUE);
}// end of CSpuPmMgr::Init()...

//
//
//
//
bool CSpuPmMgr::Shutdown()
{
	// this should be called from RenderThread:
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	gbWBAllocList.Shutdown();
	gbPostProcessDriverInfoList.Shutdown();
	gbJtsBlockerList.Shutdown();

	if(sm_gpuFencePtr)
	{
		PM_FREE_ALIGN(sm_gpuFencePtr);
		sm_gpuFencePtr = NULL;
	}

#if SPUPMMGR_TEST
	// hmmm - should really find better place to do this:
	if(g_SpuGentexTexture)
	{
		delete g_SpuGentexTexture;
		g_SpuGentexTexture = NULL;
	}
#endif //SPUPMMGR_TEST...

	return(TRUE);
}// end of PM_Shutdown()...


#if __DEV
CElfInfo* CSpuPmMgr::GetReloadedElf(u32 slot)
{
	return (slot == reloadSlot && spupmReloadedElf) ? spupmReloadedElf : NULL;
}

void CSpuPmMgr::ReloadElf()
{
	// Reload on-the-fly:
	if(DebugDeferred::m_spupmEnableReloadingElf)
	{
		if(!pReloadElf[0])
		{
			pReloadElf[0] = PM_MALLOC_ALIGN(sizeof(u8)*RELOADED_ELF_MAX_SIZE, 16);
			pReloadElf[1] = PM_MALLOC_ALIGN(sizeof(u8)*RELOADED_ELF_MAX_SIZE, 16);

			if((!pReloadElf[0]) || (!pReloadElf[1]))
			{	
				Assertf(0, "Failed to allocate elf buffers.");

				// untick button if allocation failed:
				DebugDeferred::m_spupmEnableReloadingElf = false;
				DebugDeferred::m_spupmDoReloadElf = false;
			}
		}

		if(DebugDeferred::m_spupmDoReloadElf)
		{
			DebugDeferred::m_spupmDoReloadElf = false;

			bReloadElfBuf = !bReloadElfBuf;	// swap elf buffers
			const u32 b = bReloadElfBuf;
			Assert(pReloadElf[b]);

			fiStream* stream = fiStream::Open(DebugDeferred::m_spupmReloadElfName, true);
			if(stream)
			{
				const u32 fileSize = stream->Size();
				Assertf(fileSize <= RELOADED_ELF_MAX_SIZE, "%s: Elf too big to fit in memory!", DebugDeferred::m_spupmReloadElfName);

				if(stream->Read(pReloadElf[b], fileSize) != fileSize)
				{
					Assertf(0, "%s: Error reading file!", DebugDeferred::m_spupmReloadElfName);
					spupmReloadedElf = NULL;
				}
				stream->Close();

				reloadSlot = DebugDeferred::m_spupmReloadElfSlot;
				spupmReloadedElf = (CElfInfo*)pReloadElf[b];
			}
		}
	}//if(DebugDeferred::m_spupmEnableReloadingElf)...
	else
	{
		if(pReloadElf[0])
		{
			PM_FREE_ALIGN( pReloadElf[0] );
			pReloadElf[0] = NULL;
		}

		if(pReloadElf[1])
		{
			PM_FREE_ALIGN( pReloadElf[1] );
			pReloadElf[1] = NULL;
			
		}

		spupmReloadedElf = NULL;
		reloadSlot = 0;

		Assert(pReloadElf[0]==NULL);
		Assert(pReloadElf[1]==NULL);
		Assert(spupmReloadedElf==NULL);
	}
}
#endif

//
//
// periodically update internal lists, etc.
//
bool CSpuPmMgr::Update()
{
	// this must be called from RenderThread:
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	DEV_ONLY(ReloadElf());

	// periodically cleanup all used lists from unwanted objects:
	CleanList_WorkBufferForSPU();
	CleanList_SPUPostProcessDriverInfo();
	CleanList_JtsBlocker();

#if __DEV

#endif //_DEV...

	return(TRUE);
}// end of CSpuPmMgr::Update()...




static u32 jobCount=0;

//
//
//	add given src->SPU->dest texture with given postprocessing spuElf into queue workload;
//
//	note:
//	destSpuTex and scrCellTex must correspond to each other in depth, format, etc.
//
//	numSpuThreads=0: use max available SPU threads; 
//
bool CSpuPmMgr::PostProcessTexture(const CElfInfo *spuJobElf, u32 numSpuThreads,
									u32 *split,
									void *customInputDataPtr, u32 customInputDataSize,
									grcSpuDestTexture	*destSpuTex,
									CellGcmTexture		*srcCellTex,
									u16 opStartX, u16 opStartY, u16 opWidth, u16 opHeight,
									u16 opBytesPerPixel
									#if ENABLE_PIX_TAGGING
									,const char *tagMarkerName
									#endif
									)
{
#if ENABLE_PIX_TAGGING
	PF_PUSH_MARKER(tagMarkerName? tagMarkerName : "SpuPm::PostProcessTexture");
#endif

	Assert(srcCellTex);
	Assert(destSpuTex);

#if __ASSERT
	if (split)
	{
		u32 totalY = 0;
		for (int i = 0; i < numSpuThreads; i++) { totalY += split[i]; }		
		Assert(totalY <= opHeight);
	}
#endif

	numSpuThreads = numSpuThreads? numSpuThreads : PM_MAX_NUM_SPU;
	numSpuThreads = (numSpuThreads>PM_MAX_NUM_SPU)? PM_MAX_NUM_SPU : numSpuThreads;

	const u32 nNumSPUs = numSpuThreads;

	// debug info:
	const u32 frameCount = GRCDEVICE.GetFrameCounter();
	PM_args[4] = frameCount;
	PM_args[5] = jobCount++;



	const u32 opPitch = opWidth*opBytesPerPixel;

	// create simple workload:
	CSPUPostProcessDriverInfo* driverInfo[PM_MAX_NUM_SPU]={NULL};
	for(u32 i=0; i<nNumSPUs; i++)
	{
		driverInfo[i] = AllocateSPUPostProcessDriverInfo();
		Assert(driverInfo[i]);
	}

	// allocate instance of customInputData for spuElf (if required):
	CWBAllocBlockHeader* pCustomInputDataBuffer[PM_MAX_NUM_SPU] = {NULL};
	if(customInputDataPtr && customInputDataSize)
	{
		ASSERT16(customInputDataSize);
		for(u32 i=0; i<nNumSPUs; i++)
		{
			pCustomInputDataBuffer[i] = AllocateWorkBufferForSPU(customInputDataSize);
			sysMemCpy(pCustomInputDataBuffer[i]->m_allocatedPtr, customInputDataPtr, customInputDataSize);
			driverInfo[i]->m_userDataEA		= pCustomInputDataBuffer[i]->m_allocatedPtr;
			driverInfo[i]->m_userDataSize	= customInputDataSize;
		}
	}


	// split source buffer:

	// TODO: if height not dividible into equal parts, add extra remainder to first (or last) batch?
	u32 opJobHeight = opHeight / nNumSPUs;
	
	CWBAllocBlockHeader* pSpuBuffer[PM_MAX_NUM_SPU] = {NULL};

	for(u32 i=0; i<nNumSPUs; i++)
	{
		opJobHeight = (split != NULL) ? split[i] : opJobHeight;

		pSpuBuffer[i] = AllocateWorkBufferForSPU(opPitch*opJobHeight);
		Assert(pSpuBuffer[i]);
	}
	
	opJobHeight = opHeight / nNumSPUs;
	CellGcmContextData* thisContext = g_grcCurrentContext;
	const u8 transferMode = srcCellTex->location==CELL_GCM_LOCATION_MAIN? CELL_GCM_TRANSFER_MAIN_TO_MAIN : CELL_GCM_TRANSFER_LOCAL_TO_MAIN;

	u32 opY = 0;

	for(u32 i=0; i<nNumSPUs; i++)
	{
		opJobHeight = (split != NULL) ? split[i] : opJobHeight;

		CSPUPostProcessDriverInfo *pDriverInfo = driverInfo[i];
		pDriverInfo->m_sourceData		= (void*)(u32(pSpuBuffer[i]->m_allocatedPtr));
		// EAs for prev & next buffers in chain (if any):
		pDriverInfo->m_sourceDataPrev	= i>0?			  pSpuBuffer[i-1]->m_allocatedPtr : NULL;
		pDriverInfo->m_sourceDataNext	= i<(nNumSPUs-1)? pSpuBuffer[i+1]->m_allocatedPtr : NULL;
		pDriverInfo->m_sourceStride		= opPitch;
		pDriverInfo->m_sourceWidth		= opWidth;
		pDriverInfo->m_sourceHeight		= opJobHeight;
		pDriverInfo->m_sourceBpp		= opBytesPerPixel;
		pDriverInfo->m_sourceY			= opY;

		// let RSX tranfer piece of VRam texture into pSpuBuffer[i] in MainRam:
		if(1)
		{
			#if __ASSERT
				void* finalTexAddr = gcm::GetTextureAddress(srcCellTex);
				Assert(finalTexAddr);
			#endif

			// Add a data transfer to transfer the texture to main memory.
			const u32 srcPitch = srcCellTex->pitch;
			const u32 srcOffset= srcCellTex->offset + ((opStartY + opY) * srcPitch) + opStartX*opBytesPerPixel;
			gcm::SetTransferData(thisContext, transferMode,
				gcm::MainOffset(pSpuBuffer[i]->m_allocatedPtr), opPitch,// Destination: offset+pitch
				srcOffset, srcPitch,									// Source:		offset+pitch
				opPitch,												// bytesPerRow
				opJobHeight);											// lines
		}

		opY += opJobHeight;
	}

	// split dest buffer:
	if(1)
	{
		grcTexture *grcTex = destSpuTex->GetTexture();
		Assert(grcTex);
		CellGcmTexture *gcmTex = grcTex->GetTexturePtr();
		Assert(gcmTex);
		void *dstPtr = gcm::GetTextureAddress(gcmTex);
		ASSERT128(dstPtr);
		const u32 dstWidth = gcmTex->width;
		const u32 dstHeight= gcmTex->height;
		const u32 dstStride= gcmTex->pitch;	// Stride of the destination texture data.
		u32 dstJobHeight = dstHeight / nNumSPUs;
		u32 dstY = 0;

		for(u32 i=0; i<nNumSPUs; i++)
		{
			dstJobHeight = (split != NULL) ? split[i] : dstJobHeight;

			driverInfo[i]->m_destData	= (void*)(u32(dstPtr) + dstStride*dstY);
			driverInfo[i]->m_destWidth	= dstWidth;
			driverInfo[i]->m_destHeight	= dstJobHeight;
			driverInfo[i]->m_destBpp	= opBytesPerPixel;
			driverInfo[i]->m_destStride	= dstStride;
			//	driverInfo->m_destStride= destSpuTex->GetTextureGCM()->GetWidth()*4;	// Stride of the destination texture data.

			dstY += dstJobHeight;
		}
	}

	// set JTS gate:
	CJtsBlocker *jtsBlocker = AllocateJtsBlocker();
	u32* jtsTab = (u32*)jtsBlocker->SetupJTS(nNumSPUs);
	for(u32 i=0; i<nNumSPUs; i++)
	{
		driverInfo[i]->m_gateAddr		= &jtsTab[i];
		driverInfo[i]->m_gateValue		= 0x00000000;
	}


	// refCounters for SPU
	cellAtomicAdd32(&jtsBlocker->m_refCount,	nNumSPUs);
	for(u32 i=0; i<nNumSPUs; i++)
	{
		cellAtomicAdd32(&pSpuBuffer[i]->m_refCount,	1);
		cellAtomicAdd32(&driverInfo[i]->m_refCount,	1);	// 1 SPU will be processing this batch
		if(pCustomInputDataBuffer[i])
		{
			cellAtomicAdd32(&pCustomInputDataBuffer[i]->m_refCount, 1);
		}
	}


	// set decrementing gates (for PPU allocations):
	for(u32 i=0; i<nNumSPUs; i++)
	{
		CSPUPostProcessDriverInfo *pDriverInfo = driverInfo[i];
		
		pDriverInfo->m_gateDecrAddr[0]	= &driverInfo[i]->m_refCount;	// allocated driverInfo
		pDriverInfo->m_gateDecrAddr[1]	= &pSpuBuffer[i]->m_refCount;	// allocated WorkBuffer
		pDriverInfo->m_gateDecrAddr[2]	= &jtsBlocker->m_refCount;		// allocated jtsBlocker
		if(pCustomInputDataBuffer[i])
		{
			pDriverInfo->m_gateDecrAddr[3] = &pCustomInputDataBuffer[i]->m_refCount;	// allocated customInputData
		}
	}


	// destTex: call old jtsBlocker (if any):
	destSpuTex->CallJtsBlocker();
	// destTex: destText binding:
	destSpuTex->BindJtsBlocker(jtsBlocker);
	

	// signal for SPU PM to launch given job:
	volatile u32 *fenceAddr=NULL;
	u32 fenceValue=0;
	CSpuPmMgr::GetRsxFence(&fenceAddr, &fenceValue);

	for(u32 i=0; i<nNumSPUs; i++)
	{
		CSpuPmMgr::enqueueJob(spuJobElf, driverInfo[i], sizeof(CSPUPostProcessDriverInfo),
			/*conditionSwitch*/fenceAddr, /*conditionValue*/fenceValue);
		//Printf("\n PPU: PM_enqueueJob: fenceAddr=0x%X, val=%X", (u32)fenceAddr, fenceValue );

		//	Report to wakup our spurs PM.
		if(1 && sm_workloadFlagIndex)
		{
			cellGcmSetReport(thisContext, CELL_GCM_ZPASS_PIXEL_CNT, sm_workloadFlagIndex);
		}
	}

#if ENABLE_PIX_TAGGING
	PF_POP_MARKER();
#endif

	return(TRUE);
}// end of PM_PostProcessTexture()...


// Description:
// Enqueues an SPU job. If a condition switch is provided, the SPU job is not popped from the job list until the condition switch is at least the
// specified condition value.
// Arguments:
// jobElfInfo :       Pointer to <c>PElfInfo</c> structure of the SPU handler for the <i><c>jobPacket</c></i>.
// jobPacket :        Source address for the job description packet.
// jobPacketSize :    Size of the job description packet.
// conditionSwitch :  Optional condition switch that makes the job valid.
// conditionValue :   Optional condition value at which point the job becomes valid.
void CSpuPmMgr::enqueueJob(const CElfInfo *jobElfInfo, void* jobPacket, u32 jobPacketSize,
						volatile u32 *conditionSwitch, u32 conditionValue)
{
	// Conditional jobs go on a separate list.
	CSPUJobQueuePushPullPos &jobQueue = conditionSwitch ? ms_jobQueueGPU : ms_jobQueueStandard;


	// Get a lock on the list
	while(!compareAndSwap(&jobQueue.m_push.m_lockObtained, 0, 1))
	{
		sys_timer_usleep(1);			// PPU is blocked - thread switch opportunity to let someone else have a go.
	}

	// Work out next fill address
	void* nextPush = ((u8*)jobQueue.m_push.m_curr) + jobQueue.m_push.m_elementSize;
	if (nextPush == jobQueue.m_push.m_top)
		nextPush = jobQueue.m_push.m_base;

	// Wait for a slot to become free
	while(nextPush == jobQueue.m_pull.m_curr)
	{
		Printf("\n PM_enqueueJob: PPU waiting for an empty pull slot!");
		sys_timer_usleep(1);			// PPU is blocked - thread switch opportunity to let someone else have a go.
	}

	// Push the job into the queue
	CSPUJobQueueEntry *entry	= (CSPUJobQueueEntry*)jobQueue.m_push.m_curr;
	entry->m_jobHandlerEA		= (void*)(void*)(jobElfInfo+1);
	ASSERT16(entry->m_jobHandlerEA);	// hmmm: it's not always aligned to 128 bits, but dma works fine
	entry->m_jobHandlerSize		= jobElfInfo->m_imageSize;
	entry->m_jobHandlerEntry	= (u32)(jobElfInfo->m_entry - jobElfInfo->m_LSDestination);
	entry->m_infoPacketEA		= jobPacket;
	ASSERT16(entry->m_infoPacketEA);
	entry->m_infoPacketSize		= jobPacketSize;
	entry->m_jobValidSwitch		= (void*)conditionSwitch;
	entry->m_jobValidValue		= conditionValue;
	__lwsync();										// Make sure that the job is written before we bump the push pointer.
	jobQueue.m_push.m_curr		= nextPush;			// This causes an SPU to be able to see the new job.

	// Now an SPU will wake up and take this job.
	// Relinquish the lock.
	jobQueue.m_push.m_lockObtained = 0;

	//	Printf("\n PPU: Pushed SPU job: %p %d\n", jobPacket, jobPacketSize);
	//	Printf("\n PPU: Pushed SPU job: count=%d frame=%d\n", (u32)jobPacket, (u32)jobPacketSize);

	// Ensure job queue is updated before waking up SPU.
	__lwsync();
}// end of PM_enqueueJob()...



u32 CSpuPmMgr::sm_workloadFlagIndex	= 0;	// index 0 is invalid

//
//
// creates custom PM workload:
//
bool CSpuPmMgr::createWorkload(CellSpurs *pSpurs)
{
	Assert(pSpurs);
	Printf("\n Creating custom PM workload...");

	ms_jobQueueStandard.initializeJobQueue(PM_MAX_NUM_SPU);
	ms_jobQueueGPU.initializeJobQueue(PM_MAX_NUM_SPU);



	// I. AddWorkload:
	const char *driver	= _spupmmgr_pm;
	u32 driverSize	= u32(_spupmmgr_pm_size);

// run on SPU2 - this must be in sync with jobData[] in task_psn.cpp
// and EDGE spu allocation (pri=6 on SPU2&3) in grcEffect::InitClass();

// 1xSPU: run on SPU2
//static u8	spursPriority[8] = {0,0,6,0,0,		0,0,0};

// 2xSPU: run on SPU1 & SPU2
//static u8	spursPriority[8] = {0,6,6,0,0,		0,0,0};

// 3xSPU: run on SPU1, SPU2 & SPU4:
//static u8	spursPriority[8] = {10,6,6,0,0,		0,0,0};
//static u8	spursPriority[8] = {0,6,6,0,6,		0,0,0};
//static u8	spursPriority[8] = {0,15,15,0,15,	0,0,0};

// 4xSPU: run on SPU0, SPU1, SPU2 & SPU4:
//static u8	spursPriority[8] = {15,15,15,0,15,	15,15,15};
//static u8	spursPriority[8] = {12,12,12,0,12,	12,12,12};

// 5xSPU: run on SPU0, SPU1, SPU2, SPU3 & SPU4:
// static u8	spursPriority[8] = {12,12,12,12,12,	12,12,12};
// 4xSPU: run on SPU0, SPU1, SPU2 & SPU3 (leave SPU4 for drawablespu):
static u8	spursPriority[8] = {12,12,12,12,0,	12,12,12};

static CellSpursWorkloadId workloadID=0;

	PM_args[0] = (u32)&ms_jobQueueStandard.m_push;	// push
	PM_args[1] = (u32)&ms_jobQueueStandard.m_pull;	// pull
	ASSERT128(PM_args[0]);
	ASSERT128(PM_args[1]);

	PM_args[2] = (u32)&ms_jobQueueGPU.m_push;		// push2
	PM_args[3] = (u32)&ms_jobQueueGPU.m_pull;		// pull2
	ASSERT128(PM_args[2]);
	ASSERT128(PM_args[3]);

	PM_args[4] = 0x000000a1;
	PM_args[5] = 0x000000b2;
	PM_args[6] = 0x000000c3;
	PM_args[7] = 0x000000d4;


	const u32 alignedDriverSize = (u32)(driverSize + 15)&(~15);
	if (alignedDriverSize > (16*1024))
	{
		Assertf(0, "\n Policy module for spurs is bigger than 16k (currently: %d bytes) \n", alignedDriverSize);
		return(FALSE);
	}

	int ret = cellSpursAddWorkload(pSpurs, &workloadID,
		driver, alignedDriverSize,
		(uintptr_t)&PM_args, spursPriority, PM_MAX_NUM_SPU, PM_MAX_NUM_SPU);
	if (ret != CELL_OK)
	{
		Printf("\n cellSpursAddWorkload for spurs failed (returned %d).", ret);
		return FALSE;
	}
	Printf("\n workloadID=%x", workloadID);


	// II. Workload flag set by RSX reports:
	if(1)
	{
		// assuming Spurs instance allocated in RSX report area (0x0e000000) with strict ordering (SO) aperture (CELL_GCM_IOMAP_FLAG_STRICT_ORDERING):
#if !__NO_OUTPUT
		const void* report64addr	= pSpurs;
		const u32	report64Offset	= gcm::MainOffset(pSpurs);
		Printf("\n reportOffset64=0x%x, report64addr=0x%X, sizeof(CellGcmReportData)=%d", report64Offset, report64addr, sizeof(CellGcmReportData));
#endif

		// Get the workload flag index inside the RSX report area.
		CellSpursWorkloadFlag* flag=NULL;
		cellSpursGetWorkloadFlag(pSpurs, &flag);
		Assert(flag);

		u32 flagOffset=0;
		if(cellGcmAddressToOffset(flag, &flagOffset) != CELL_OK)
		{
			Printf("\n Spurs not mapped into RSX IO space!");
			return(FALSE);
		}

		sm_workloadFlagIndex  = (CellGcmReportData*)flag - cellGcmGetReportDataAddressLocation(0, CELL_GCM_LOCATION_MAIN);
		Assert(sm_workloadFlagIndex != 0);
		Printf("\n sm_workloadFlagIndex=%d", sm_workloadFlagIndex);

		if(sm_workloadFlagIndex >= 0x100000)
		{
			Printf("\n Spurs is not in report memory!");
			return(FALSE);
		}

		// setup workload flag receiver:
		ret  = cellSpursSetWorkloadFlagReceiver(pSpurs, workloadID);
		if(ret != CELL_OK)
		{
			Printf("\n cellSpursSetWorkloadFlagReceiver for spurs failed (returned=%d)!", ret);
			return FALSE;
		}

	}// workload flag...


	// III. WakeupWorkload:
	if( !wakeupWorkload(pSpurs, workloadID) )
	{
		Assertf(0, "Error wakingup workload!");
		return(FALSE);
	}

	if(0 && !suspendCurrentWorkload(pSpurs) )
	{
		Assertf(0, "Error suspending workload!");
		return(FALSE);
	}


	return(TRUE);
}// end of CreateWorklad()...


#define INVALID_WORKLOADID		(u32(-1))
u32 CSpuPmMgr::sm_currentWorkloadID = INVALID_WORKLOADID;

//
//
//
//
bool CSpuPmMgr::wakeupWorkload(CellSpurs *pSpurs, u32 workloadID)
{

#if 0
	// If we are about to switch a workload off, make sure that its job queue has been exhausted.
	if (m_currentWorkload != PE_SPURS_WORKLOAD_NOTRUNNING && m_currentWorkload != workload)
	{
		// Wait for the job queue to empty.
		const Spu::PSPUJobQueuePushPullPos *jobQueuePos = m_spursWorkloadJobQueuePos[m_currentWorkload];
		if (jobQueuePos)
		{
			while (!jobQueuePos->isEmpty())
				sys_timer_usleep(1);	// PPU is blocked - thread switch opportunity to let someone else have a go.
		}
	}
#endif 

	unsigned int readyCount = PM_MAX_NUM_SPU;	//m_spursWorkloadJobQueuePos[workload]->m_maxSPUs;

	u32 ret;
	if(sm_currentWorkloadID != INVALID_WORKLOADID)
	{
		ret = cellSpursReadyCountStore(pSpurs, sm_currentWorkloadID, 0);
		if(ret != CELL_OK)
			return false;
	}

	if(workloadID != INVALID_WORKLOADID)
	{
		ret = cellSpursReadyCountStore(pSpurs, workloadID, readyCount);
		if(ret != CELL_OK)
			return(FALSE);
	}

	if(sm_currentWorkloadID != workloadID)
	{
		sm_currentWorkloadID = workloadID;
		ret = cellSpursWakeUp(pSpurs);
	}

	return (ret == CELL_OK);
}// end of wakeupWorkload()...

//
//
//
//
bool CSpuPmMgr::suspendCurrentWorkload(CellSpurs *pSpurs)
{
	if(sm_currentWorkloadID != INVALID_WORKLOADID)
	{
		u32 ret = cellSpursReadyCountStore(pSpurs, sm_currentWorkloadID, 0);
		if(ret != CELL_OK)
			return false;
	}

	sm_currentWorkloadID = INVALID_WORKLOADID;
	return(TRUE);
}



////////////////////////////////////////////////////////////////////////////////////////////

// Description:
// Initializes this SPU job queue to empty.
// Arguments:
// maxSPUs :  The maximum number of SPUs to be used by this workload.
void CSPUJobQueuePushPullPos::initializeJobQueue(unsigned int maxSPUs)
{
	const u32 numEntries = PM_MAX_WORK_QUEUE_JOBS;
	sysMemSet(m_entries, 0, sizeof(m_entries[0]) * numEntries);

	m_push.m_lockObtained	= 0;
	m_push.m_elementSize	= sizeof(m_entries[0]);
	m_push.m_base			= &m_entries[0];
	m_push.m_top			= &m_entries[numEntries];
	m_push.m_curr			= &m_entries[0];

	m_pull.m_lockObtained	= 0;
	m_pull.m_elementSize	= sizeof(m_entries[0]);
	m_pull.m_base			= &m_entries[0];
	m_pull.m_top			= &m_entries[numEntries];
	m_pull.m_curr			= &m_entries[0];

	m_maxSPUs				= maxSPUs;
}


/////////////////////////////////////////////////////////////////////////////////////////////
//
//
// destTex: destText binding:
//
void grcSpuDestTexture::BindJtsBlocker(CJtsBlocker *jtsBlocker, bool bCallOldBlocker)
{
	if(bCallOldBlocker)	// call previously assigned blocker?
		this->CallJtsBlocker();

	Assert(this->m_jtsBlocker==NULL);	// make sure previous blocker (if any) has been called

	this->m_jtsBlocker = jtsBlocker;
	if(this->m_jtsBlocker)
	{
		cellAtomicIncr32(&this->m_jtsBlocker->m_refCount);
	}
}

//
//
// insert JTS, so RSX waits for pending SPU operations (if any) to finish on given destTex
//
void grcSpuDestTexture::CallJtsBlocker()
{
	if(this->m_jtsBlocker)
	{
		this->m_jtsBlocker->CallJTS();	// call JTS
		cellAtomicDecr32(&this->m_jtsBlocker->m_refCount);
		this->m_jtsBlocker = NULL;		// we don't have this jtsBlocker anymore!
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////

static
int32_t CustomGcmReserveFailedJtsBlocker(CellGcmContextData* ASSERT_ONLY(context), uint32_t ASSERT_ONLY(count))
{
#if __ASSERT
	Errorf("\nCustomGcmReserveFailedJtsBlocker: Custom context 0x%p overfilled! CurrentPos=%d. MaxSize=%d. CountRequired=%d.",
		context,
		context->current - context->begin, context->end - context->begin, count);
	Assert(FALSE);
#endif
	return(CELL_OK);
}

//
//
//
//
static
void SetupContextDataJtsBlocker(CellGcmContextData *context, const uint32_t *addr, const uint32_t size, CellGcmContextCallback callback)
{
	context->begin		= (uint32_t*)addr;
	context->current	= (uint32_t*)addr;
	context->end		= (uint32_t*)addr + size/sizeof(uint32_t) - 1;
	context->callback	= callback;
}

//
// sets given # of JTSes in current command buffer;
// every SPU batch has its own JTS to mark processing complete;
// returns pointer to first JTS;
//
void* CJtsBlocker::SetupJTS(u32 numJTS)
{
ASSERT_ONLY(const u32 maxCommandSize=20;)

	sysMemSet(&this->m_JTS[0], 0x00, sizeof(this->m_JTS));

CellGcmContext		con;
	SetupContextDataJtsBlocker((CellGcmContextData*)&con, &this->m_JTS[0], sizeof(this->m_JTS), &CustomGcmReserveFailedJtsBlocker);

	Assert(numJTS <= maxNumJTS);
	for(u32 i=0; i<numJTS; i++)
	{
		u32 jmpDst = gcm::MainOffset(&m_JTS[i]);
		Assert(jmpDst);
		con.SetJumpCommand(jmpDst);
	}

	const u32 numNOPs = maxNumJTS-numJTS;
	if(numNOPs > 0)
	{
		con.SetNopCommand(numNOPs);
	}

	// clear RSX refCounter:
	const u32 NOPcommand=0;
	con.InlineTransfer(gcm::MainOffset(&this->m_refCountRsx), &NOPcommand, 1, CELL_GCM_LOCATION_MAIN);
	con.SetTransferLocation(CELL_GCM_LOCATION_LOCAL);

#if __ASSERT
	const u32 commandSize = con.current - con.begin;
	//	Printf("\n CJtsBlocker::SetupJTS: cbsize=%d words", commandSize);
	AssertMsg(commandSize==maxCommandSize,"CJtsBlocker::SetupJTS: command size is not 20!");
#endif //__ASSERT...

	this->m_ReturnFromJTSblock = con.current;
	ASSERT16(this->m_ReturnFromJTSblock);

	return(&this->m_JTS[0]);
}

//
//
//
//
void* CJtsBlocker::CallJTS()
{
	// Optimization: was this jtsBlocker already processed by all SPU jobs?
	// if yes, then no need to add JTS (as it's already patched by SPU)
	if(cellAtomicNop32(&this->m_refCount)==0)
		return NULL;

	// increase RSX refCount:
	cellAtomicStore32(&this->m_refCountRsx, 1);

	// jump from Rage's GCM FIFO into this JTS code block:
	SPU_COMMAND(grcDevice__Jump, 0);
	// Local address of the jump target
	cmd->jumpLocalOffset	= PM_GCM_OFFSET(&this->m_JTS[0]);
	// PPU address of the return jump
	cmd->jumpBack			= m_ReturnFromJTSblock;
	ASSERT16(cmd->jumpBack);

	//	Printf("\n CJtsBlocker::CallJTS on 0x%X",cmd->jumpLocalAddr);

	return(&this->m_JTS[0]);
}

//
//
// - allocates workbuffer for SPU, with given size and align
// - adds relevant buffers to linked lists, etc.
//
CWBAllocBlockHeader* CSpuPmMgr::AllocateWorkBufferForSPU(u32 size, bool bCleanAllocList)
{
	// try first to free list of allocated buffers if caller wants it:
	if(bCleanAllocList)
		CleanList_WorkBufferForSPU();

	CWBAllocBlockHeaderLink *newWBlink = gbWBAllocList.Insert();
	AssertMsg(newWBlink, "gbWBAllocList: No more room!");

	CWBAllocBlockHeader *newWB = &newWBlink->item;

	newWB->m_allocatedPtr = BWB_ALLOC(size, 128);	// 128 bits alignment for SPU

	Assertf(newWB->m_allocatedPtr, "gbWorkBufferAllocator: No more room for %dKB! (Used: %dKB, available: %dKB, largest block available: %dKB).", size/1024, BWB_GETMEMUSED()/1024, BWB_GETMEMAVAILABLE()/1024, BWB_GETLARGESTAVAILABLEBLOCK()/1024);
	newWB->m_refCount = 0;

	return(newWB);
}

// allocated lists cleanup:
// go over allocated lists of driverInfos and workbuffers and free unwanted blocks:
void CSpuPmMgr::CleanList_WorkBufferForSPU()
{
	const CWBAllocBlockHeaderLink *first= gbWBAllocList.GetFirst();
	const CWBAllocBlockHeaderLink *last	= gbWBAllocList.GetLast();

	CWBAllocBlockHeaderLink *curr	= last->GetPrevious();	
	while(curr != first)
	{
		CWBAllocBlockHeaderLink *prev = curr->GetPrevious();
		if(cellAtomicNop32(&curr->item.m_refCount) == 0)
		{
			if(curr->item.m_allocatedPtr)
			{
//				Printf("\n PM_Update: freed CWBAllocBlockHeaderLink 0x%X: chunksize=%d", (u32)curr, (u32)BWB_GETSIZE(curr->item.m_allocatedPtr));
//				Printf("\n PM_Update: BWB mem used: %dKB, available: %dKB", (u32)BWB_GETMEMUSED()/1024, BWB_GETMEMAVAILABLE()/1024);
				BWB_FREE(curr->item.m_allocatedPtr);
				curr->item.m_allocatedPtr = NULL;
			}
			gbWBAllocList.Remove(curr);
		}
		curr = prev;
	}
}


//
//
//
//
CSPUPostProcessDriverInfo*	CSpuPmMgr::AllocateSPUPostProcessDriverInfo(bool bCleanAllocList)
{
	// try first to free list of allocated buffers if caller wants it:
	if(bCleanAllocList)
		CleanList_SPUPostProcessDriverInfo();


	CSPUPostProcessDriverInfoLink *newDIlink = gbPostProcessDriverInfoList.Insert();
	AssertMsg(newDIlink, "gbPostProcessDriverInfoList: No more room!");

	CSPUPostProcessDriverInfo *newDI = &newDIlink->item;
	newDI->ZeroAll();		// zero all fields
	newDI->m_refCount = 0;

	return(newDI);
}

void CSpuPmMgr::CleanList_SPUPostProcessDriverInfo()
{
const CSPUPostProcessDriverInfoLink *first	= gbPostProcessDriverInfoList.GetFirst();
const CSPUPostProcessDriverInfoLink *last	= gbPostProcessDriverInfoList.GetLast();

	CSPUPostProcessDriverInfoLink *curr = last->GetPrevious();	
	while(curr != first)
	{
		CSPUPostProcessDriverInfoLink *prev = curr->GetPrevious();
		if(cellAtomicNop32(&curr->item.m_refCount) == 0)
		{
			gbPostProcessDriverInfoList.Remove(curr);
			//Printf("\n PM_Update: freed CSPUPostProcessDriverInfoLink: 0x%X", (u32)curr);
		}
		curr = prev;
	}
}


//
//
//
//
CJtsBlocker* CSpuPmMgr::AllocateJtsBlocker(bool bCleanAllocList)
{
	// try first to free list of allocated buffers if caller wants it:
	if(bCleanAllocList)
		CleanList_JtsBlocker();

	CJtsBlockerLink *newJtsBlockerLink = gbJtsBlockerList.Insert();
	AssertMsg(newJtsBlockerLink, "gbJtsBlockerList: No more room!");

	CJtsBlocker *newJtsBlocker = &newJtsBlockerLink->item;
	newJtsBlocker->ZeroAll();		// zero all fields
	newJtsBlocker->m_refCount		= 0;
	newJtsBlocker->m_refCountRsx	= 0;

	return(newJtsBlocker);
}

void CSpuPmMgr::CleanList_JtsBlocker()
{
const CJtsBlockerLink *first	= gbJtsBlockerList.GetFirst();
const CJtsBlockerLink *last		= gbJtsBlockerList.GetLast();

	CJtsBlockerLink *curr = last->GetPrevious();	
	while(curr != first)
	{
		CJtsBlockerLink *prev = curr->GetPrevious();
		if(	(cellAtomicNop32(&curr->item.m_refCount)==0)		&&
			(cellAtomicNop32(&curr->item.m_refCountRsx)==0)		)
		{
			gbJtsBlockerList.Remove(curr);
			//Printf("\n PM_Update: freed CJtsBlockerLink 0x%X", (u32)curr);
		}
		curr = prev;
	}
}





u32				CSpuPmMgr::rsxWaitTextureLabelIndex= u32(-1);
volatile u32*	CSpuPmMgr::rsxWaitTextureLabelPtr	= NULL;
u32				CSpuPmMgr::sm_waitTextureValue		=0;

u32				CSpuPmMgr::sm_gpuFenceValue		=0;
u32*			CSpuPmMgr::sm_gpuFencePtr			=NULL;
u32				CSpuPmMgr::sm_gpuFenceOffset		=0;

// Description:
// Get a fence value for the current construction position of the graphics command buffer.
// Arguments:
// fenceAddr :   Address of the fence. The memory at this location is written as the RSX(TM) processes the command buffer.
// fenceValue :  The value that the RSX(TM) will write to the fence.
// Return Value List:
// PE_RESULT_NO_ERROR :  The fence was obtained successfully.
bool CSpuPmMgr::GetRsxFence(volatile u32 **fenceAddr, u32 *fenceValue)
{
	CellGcmContextData* thisContext = g_grcCurrentContext;

	sm_gpuFenceValue++;
	(*fenceValue)	= sm_gpuFenceValue;
	(*fenceAddr)	= sm_gpuFencePtr;

	// Make the RSX(tm) wait for command read completion:
	sm_waitTextureValue++;
	cellGcmSetWriteTextureLabelInline(thisContext, rsxWaitTextureLabelIndex, sm_waitTextureValue);
	cellGcmSetWaitLabel(thisContext, rsxWaitTextureLabelIndex, sm_waitTextureValue);

	// Add an inline transfer to write this fence value (and dont forget to set transfer location back to local afterwards).
	cellGcmInlineTransfer(thisContext, sm_gpuFenceOffset, &sm_gpuFenceValue, 1, CELL_GCM_LOCATION_MAIN);
	cellGcmSetTransferLocation(thisContext, CELL_GCM_LOCATION_LOCAL);

	return(TRUE);
}

#endif //___PPU...



