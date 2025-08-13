//
//	GPU particles update job:
//   Handles GPU particles simulation on the SPU
//   All the calculations must match the shader version used on other platforms
//

#if __SPU

#define SPUPMMGR_RAINUPDATE (1)

#define DEBUG_SPU_JOB_NAME "spupmRainUpdateSPU"

#define AM_SPU_DEBUG				(1)

#define RAIN_DEBUG_ENABLE (0)
#if RAIN_DEBUG_ENABLE
#define SPU_DEBUG0F(x)			x
#define SPU_DEBUG1F(x)			x
#define SPU_DEBUG2F(x)			x
#else
#define SPU_DEBUG0F(x)
#define SPU_DEBUG1F(x)
#define SPU_DEBUG2F(x)
#endif // RAIN_DEBUG_ENABLE

#undef	__STRICT_ALIGNED			// new.h compile warnings
#define ATL_INMAP_H					// inmap.h compile warnings   

#define SYS_DMA_VALIDATION			(1)	// 1 -	General checks on valid alignment, size and DMA source and  destination addresses for all DMA commands. 
#include "system/dma.h"
#include <cell/atomic.h>
#include <spu_printf.h>
#include <cell/gcm_spu.h>

#include "renderer\SpuPM\SpuPmMgr.h"
#include "renderer\SpuPM\SpuPmMgrSPU.h"

#include "math/intrinsics.h"
#include "math/vecmath.h"
#include "math/spu_random.h"
#include "math/float16.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "vector/matrix34.h"
#include "vector/matrix44.h"

#define _rainUpdateDefAlignment (128)
static u8* _localUpdateBuffer0  = NULL;
static u8* _localUpdateBuffer1  = NULL;
#define localRainBuffer0 ((Float16Vec4*)&_localUpdateBuffer0[0])
#define localRainBuffer1 ((Float16Vec4*)&_localUpdateBuffer1[0])

static u8* _localHeightMap  = NULL;
#define localHeightMap	((float*)&_localHeightMap[0])

static u8*	_gFrustumClipPlanes = NULL;			// used for sphere tests
#define		gFrustumClipPlanes	((Vector4*)_gFrustumClipPlanes)

// Transposed clip plane equations (for better vectorization):
static u8*	_gFrustumTPlane0 = NULL;
#define		gFrustumTPlane0	((Vector4*)_gFrustumTPlane0)

static u8*	_gFrustumTPlane1 = NULL;
#define		gFrustumTPlane1	((Vector4*)_gFrustumTPlane1)

static const u32 kMaxHeightMapSize = 128*128;
static u32	nHeightMapWidth	= 0;
static u32	nHeightMapHeight = 0;
//
#define SPU_DONT_INCLUDE_GLOBAL_DISTURBANCES

#include "../../../rage/suite/src/rmptfx/ptxrandomtable.h"
#include "../../../rage/suite/src/rmptfx/ptxrandomtable.cpp"
#include "../../../rage/suite/src/gpuptfx/ptxrainupdatespu.h"
#include "../../../rage/suite/src/gpuptfx/ptxrainupdatespu.cpp"

#undef SPU_DONT_INCLUDE_GLOBAL_DISTURBANCES

int32_t rainUpdateCtxCB(CellGcmContextData *, uint32_t)
{
	FastAssert(0);	// local context overflowed!
	return 0;
};


//
//
//
//
CSPUPostProcessDriverInfo* RainUpdateSPU_cr0(char* _end, void *infoPacketSource, unsigned int infoPacketSize)
{	
	SPU_DEBUG0F(spu_printf("\n [RainUpdateSPU_cr0] START infoPacketSource: 0x%X, size: %u\n", (u32)infoPacketSource, infoPacketSize));

	// Start SPU memory allocator.  Align to info packet alignment to get best DMA performance.
	u32 spuAddr = matchAlignment128((u32)_end, infoPacketSource);
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_INIT %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	// Get the job packet
	u32 alignedSize = AlignSize16(infoPacketSize);
	CSPUPostProcessDriverInfo *infoPacket = (CSPUPostProcessDriverInfo*)spuAddr;
	sysDmaGet(infoPacket, (u64)infoPacketSource, alignedSize, 0);
	spuAddr = stepLSAddr(spuAddr, alignedSize);
	sysDmaWaitTagStatusAll(1<<0);
	SPU_DEBUG0F(spu_printf("\n [RainUpdateSPU_cr0] GET info packet dma'ed in\n"));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_5 %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	alignedSize = AlignSize16(infoPacket->m_userDataSize);
	u32 *userData = (u32*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, alignedSize);
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_6 %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	SPU_DEBUG0F(spu_printf("\n [RainUpdateSPU_cr0] GET userData: 0x%X, size: %u\n", (u32)infoPacket->m_userDataEA, infoPacket->m_userDataSize));

	CGpuParticleUpdateSpuInfo* spuInfo = NULL;
	sysDmaGet(userData, (u64)infoPacket->m_userDataEA, alignedSize, 0);
	sysDmaWaitTagStatusAll(1<<0);
	spuInfo = (CGpuParticleUpdateSpuInfo*)userData;
	SPU_DEBUG0F(spu_printf("\n [RainUpdateSPU_cr0] GET userData dma'ed in\n"));

	nHeightMapWidth	 = spuInfo->m_nHeightMapWidth;
	nHeightMapHeight = spuInfo->m_nHeightMapHeight;

	// allocate local update buffers
	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	_localUpdateBuffer0 = (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, gPtxGpuUpdateNumParticlesPerSpu[spuInfo->m_instanceId]*sizeof(Float16Vec4));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_7 %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	
	 spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	 _localUpdateBuffer1 = (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, gPtxGpuUpdateNumParticlesPerSpu[spuInfo->m_instanceId]*sizeof(Float16Vec4));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_8 %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	// allocate memory for height map
	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	_localHeightMap	= (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, kMaxHeightMapSize*sizeof(float));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_9 %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	// misc allocations
	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	_gFrustumClipPlanes = (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, 6*sizeof(Vector4));

	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	_gFrustumTPlane0 = (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, 4*sizeof(Vector4));
	
	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	_gFrustumTPlane1 = (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, 4*sizeof(Vector4));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_10 %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	// dma height map
	char* pHeightMapEa				= spuInfo->m_pHeightMapEa;
	const u32	nHeightMapWidth		= spuInfo->m_nHeightMapWidth;
	const u32	nHeightMapHeight	= spuInfo->m_nHeightMapHeight;
	const u32	nHeightMapSize		= nHeightMapWidth * nHeightMapHeight * 4;

	if(pHeightMapEa)
	{
		SPU_DEBUG0F(spu_printf("\n [RainUpdateSPU_cr0] GET pHeightMapEa: 0x%X, size: %u\n", (u32)spuInfo->m_pHeightMapEa, nHeightMapSize));
		sysDmaLargeGet(localHeightMap, (u32)pHeightMapEa, nHeightMapSize, 0);
		sysDmaWaitTagStatusAll(1<<0);
		SPU_DEBUG0F(spu_printf("\n [RainUpdateSPU_cr0] GET pHeightMapEa dma'ed in\n"));

		ConvertHeightMap(nHeightMapWidth, nHeightMapHeight);
	}

#if PTXGPU_USE_WIND_ON_SPU
	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	phSpuWindEvalData* pWindEvalData = (phSpuWindEvalData*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, sizeof(phSpuWindEvalData));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_A %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
	phSpuWindEval* pWindEval = (phSpuWindEval*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, sizeof(phSpuWindEval));
	SPU_DEBUG1F(spu_printf("\n [MEMALLOC_B %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

	sysDmaGet(pWindEvalData, (u32)spuInfo->m_pWindEvalData, sizeof(phSpuWindEvalData),0);
	sysDmaGetf(ptxRandomTable::GetDataPtr(), uptr(spuInfo->m_pRandomTableAddr), PTXRANDTABLE_SIZE * sizeof(float), 0);
	sysDmaWaitTagStatusAll(1);
	pWindEval->Init(*pWindEvalData);

	spuInfo->m_pWindEvalData = pWindEvalData;
	spuInfo->m_pWindEval = pWindEval;
#endif

	u32 memCheckPoint = spuAddr;
	for (int i = 0; i < gPtxGpuUpdateNumSpuTasks[spuInfo->m_instanceId]; i++)
	{
		SPU_DEBUG0F(spu_printf("\n\t[RainUpdateSPU_cr0 LOOP: %d]\n", i ));


		// allocate local memory for gcm context
		const u32 contextSize = spuInfo->m_gcmContextSize;
		spuAddr = Align(spuAddr, _rainUpdateDefAlignment);
		u32* pContextMem = (u32*)spuAddr;
		spuAddr = stepLSAddr(spuAddr, contextSize);
		memset(pContextMem, 0x00, contextSize);
		SPU_DEBUG1F(spu_printf("\n [MEMALLOC_C %u out of %u\n", spuAddr, (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE)));

		// setup context
		CellGcmContextData ptxContext;
		ptxContext.begin	= (uint32_t*)pContextMem;
		ptxContext.current	= (uint32_t*)pContextMem;
		ptxContext.end		= (uint32_t*)pContextMem + contextSize/sizeof(uint32_t);
		ptxContext.callback = &rainUpdateCtxCB;


		char **pRainUpdateBufEa			= spuInfo->m_rainUpdateBufEa;
		char **pRainUpdateBufVRamEa		= &spuInfo->m_rainUpdateBufVRamEa[i][0];
		const u32 nRainUpdateBufSize	= spuInfo->m_rainUpdateBufSize;
		const u32 updateOffset			= i*nRainUpdateBufSize;
		const u32 gpRainUpdateBufEa0	= (u32)(pRainUpdateBufEa[0]+updateOffset);
		const u32 gpRainUpdateBufEa1	= (u32)(pRainUpdateBufEa[1]+updateOffset);

		SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] GET gpRainUpdateBufEa0: 0x%X, size: %u offset: %u\n", (u32)gpRainUpdateBufEa0, nRainUpdateBufSize, updateOffset));
		SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] GET gpRainUpdateBufEa1: 0x%X, size: %u offset: %u\n", (u32)gpRainUpdateBufEa1, nRainUpdateBufSize, updateOffset));
		sysDmaLargeGet(localRainBuffer0, gpRainUpdateBufEa0, nRainUpdateBufSize, 1);
		sysDmaLargeGet(localRainBuffer1, gpRainUpdateBufEa1, nRainUpdateBufSize, 1);

		// finish loading update maps:
		sysDmaWaitTagStatusAll(1<<1);
		SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] GET gpRainUpdateBufEa0/1 dma'ed in\n"));

		SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] BEFORE spuRainUpdateMain\n"));
		u32 numDrops = spuRainUpdateMain(i, spuInfo);
		numDrops = (u32)(numDrops*spuInfo->m_ratioToRender);
		SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] AFTER spuRainUpdateMain (numDrops = %d)\n", numDrops));

		if(numDrops)
		{
			const u32 storedSize = numDrops * sizeof(Float16Vec4);
			SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] PUT gpRainUpdateBufEa0: 0x%X, size: %u\n", (u32)pRainUpdateBufVRamEa[0], storedSize));
			SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] PUT gpRainUpdateBufEa1: 0x%X, size: %u\n", (u32)pRainUpdateBufVRamEa[1], storedSize));

			sysDmaLargePut(localRainBuffer0, (u32)pRainUpdateBufVRamEa[0], storedSize, 0);
			sysDmaLargePut(localRainBuffer1, (u32)pRainUpdateBufVRamEa[1], storedSize, 0);

			::cell::Gcm::Inline::cellGcmSetNopCommand(&ptxContext, 4);
//			::cell::Gcm::Inline::cellGcmSetDrawArrays(&ptxContext, CELL_GCM_PRIMITIVE_QUADS, 0, gPtxGpuUpdateNumParticlesPerSpu[spuInfo->m_instanceId]*4);
			::cell::Gcm::Inline::cellGcmSetDrawArrays(&ptxContext, CELL_GCM_PRIMITIVE_QUADS, 0, numDrops*4);

			SPU_DEBUG2F(spu_printf("\n>>>>>>>spuInfo->m_gcmContextEa[%u] = 0x%x<<<<<<<\n", i, (u32)(spuInfo->m_gcmContextEa[i])));
		}

		const u32 cbSize = contextSize;
		sysDmaPut(pContextMem, (u32)(spuInfo->m_gcmContextEa[i]), cbSize, 0);
//		spu_printf("\n RainUpdateSpu: %d: contextEa=0x%p, defNumParticles=%d, numParticles=%d, cbSize=%d.", i, spuInfo->m_gcmContextEa[i], gPtxGpuUpdateNumParticlesPerSpu[spuInfo->m_instanceId]*4, numDrops, cbSize);

		sysDmaWaitTagStatusAll(1<<0);
		SPU_DEBUG0F(spu_printf("\n\t\t[RainUpdateSPU_cr0] PUT gpRainUpdateBufEa0/1 dma'ed\n"));

		// reset in-loop allocations
		spuAddr = memCheckPoint;
	}

	return(infoPacket);

}// end of RainUpdateSPU_cr0()...

#endif //__SPU....


