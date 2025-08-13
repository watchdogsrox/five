//
//	Custom test of SPU processing job:
//
//
//

#if __SPU
#define DEBUG_SPU_JOB_NAME "spupmProcessSPU"

#define AM_SPU_DEBUG				(1)
#define SPU_DEBUGF(X)				X
//#define SPU_DEBUGF(x)
#define SPU_DEBUGF1(X)				X


#undef	__STRICT_ALIGNED			// new.h compile warnings
#define ATL_INMAP_H					// inmap.h compile warnings

#define SYS_DMA_VALIDATION			(1)	// 1 -	General checks on valid alignment, size and DMA source and  destination addresses for all DMA commands. 
#include "system/dma.h"
#include <cell/atomic.h>
#include <spu_printf.h>


#include "renderer\SpuPM\SpuPmMgr.h"
#include "renderer\SpuPM\SpuPmMgrSPU.h"



//static u8 bigWorkBuffer[128*128*4] ALIGNED(128) = {0};
#define bigWorkBuffer_size	(128*128*4)
#define bigWorkBuffer_align	(128)
static u8* bigWorkBuffer=0;

//
//
//
//
CSPUPostProcessDriverInfo* ProcessSPU_cr0(char* _end, void *infoPacketSource, unsigned int infoPacketSize)
{	
	// Start SPU memory allocator.  Align to info packet alignment to get best DMA performance.
	u32 spuAddr = matchAlignment128((u32)_end, infoPacketSource);
	// Get the job packet
	u32 alignedSize = AlignSize16(infoPacketSize);
	CSPUPostProcessDriverInfo *infoPacket = (CSPUPostProcessDriverInfo*)spuAddr;
	sysDmaGet(infoPacket, (u64)infoPacketSource, alignedSize, 0);
	spuAddr = stepLSAddr(spuAddr, alignedSize);
	sysDmaWaitTagStatusAll(1<<0);

	// Grab userdata packet (if any):
	if(infoPacket->m_userDataEA)
	{
		alignedSize = AlignSize16(infoPacket->m_userDataSize);
		u32 *userData = (u32*)spuAddr;
		sysDmaGet(userData, (u64)infoPacket->m_userDataEA, alignedSize, 0);
		spuAddr = stepLSAddr(spuAddr, alignedSize);
		sysDmaWaitTagStatusAll(1<<0);

		//spu_printf("\n CustomProcessSPU: userData: 0=%d 1=%d 2=%d 3=%d", userData[0], userData[1], userData[2], userData[3]);
	}

	// allcate big work buffers (to minimize elf image size):
	spuAddr = Align(spuAddr, bigWorkBuffer_align);
	bigWorkBuffer = (u8*)spuAddr;
	spuAddr = stepLSAddr(spuAddr, bigWorkBuffer_size);


//	SPU_DEBUGF(spu_printf("\n CustomProcessSPU: process: infoPacketSource=0x%X, infoPacketSize=%d", (u32)infoPacketSource, infoPacketSize));
//	SPU_DEBUGF(spu_printf("\n CustomProcessSPU: _end=0x%X", u32(_end)));



	// simple processing: copy src over to destination:
	Assert(bigWorkBuffer);

	u8 *srcData = (u8*)infoPacket->m_sourceData;
	sysDmaGet(bigWorkBuffer+0*128*32*4, (u64)(srcData+0*128*32*4), 128*32*4, 0);
	sysDmaGet(bigWorkBuffer+1*128*32*4, (u64)(srcData+1*128*32*4), 128*32*4, 0);
	sysDmaGet(bigWorkBuffer+2*128*32*4, (u64)(srcData+2*128*32*4), 128*32*4, 0);
	sysDmaGet(bigWorkBuffer+3*128*32*4, (u64)(srcData+3*128*32*4), 128*32*4, 0);
	sysDmaWaitTagStatusAll(1<<0);

#if 0
	// ABGR -> ARGB:
	u32 *bigWorkBuffer32 = (u32*)bigWorkBuffer;
	for(u32 i=0; i<128*128; i++)
	{
		u32 src = bigWorkBuffer32[i];
			u32 colorA	= (src>>24)&0xFF;
			u32 colorB	= (src>>16)&0xFF;
			u32 colorG	= (src>> 8)&0xFF;
			u32 colorR	= (src    )&0xFF;
			u32 dst		= (colorA<<24) | (colorR<<16) | (colorG<<8) | (colorB);
		bigWorkBuffer32[i] = dst;
	}
#else
	const vec_uchar16 shufADCB_EHGF_ILKJ_MPON = {	0x00,0x03,0x02,0x01,
													0x04,0x07,0x06,0x05,
													0x08,0x0b,0x0a,0x09,
													0x0c,0x0f,0x0e,0x0d};

	vec_uint4 *bigWorkBuffer128 = (vec_uint4*)bigWorkBuffer;
	for(u32 i=0; i<128*(128/4); i+=4)
	{
		vec_uint4 src0 = bigWorkBuffer128[i+0];
		vec_uint4 src1 = bigWorkBuffer128[i+1];
		vec_uint4 src2 = bigWorkBuffer128[i+2];
		vec_uint4 src3 = bigWorkBuffer128[i+3];

		// ABGR -> ARGB:
		vec_uint4 dst0 = spu_shuffle(src0, src0, shufADCB_EHGF_ILKJ_MPON);
		vec_uint4 dst1 = spu_shuffle(src1, src1, shufADCB_EHGF_ILKJ_MPON);
		vec_uint4 dst2 = spu_shuffle(src2, src2, shufADCB_EHGF_ILKJ_MPON);
		vec_uint4 dst3 = spu_shuffle(src3, src3, shufADCB_EHGF_ILKJ_MPON);

		dst1 = spu_add(dst1, dst1);
//		dst2 = spu_add(dst2, dst0);
		dst3 = spu_add(dst3, dst3);

		bigWorkBuffer128[i+0] = dst0;
		bigWorkBuffer128[i+1] = dst1;
		bigWorkBuffer128[i+2] = dst2;
		bigWorkBuffer128[i+3] = dst3;
	}
#endif

	u8 *dstData = (u8*)infoPacket->m_destData;
	sysDmaPut((void*)((u32)bigWorkBuffer+0*128*32*4), (u64)(dstData+0*128*32*4), 128*32*4, 0);
	sysDmaPut((void*)((u32)bigWorkBuffer+1*128*32*4), (u64)(dstData+1*128*32*4), 128*32*4, 0);
	sysDmaPut((void*)((u32)bigWorkBuffer+2*128*32*4), (u64)(dstData+2*128*32*4), 128*32*4, 0);
	sysDmaPut((void*)((u32)bigWorkBuffer+3*128*32*4), (u64)(dstData+3*128*32*4), 128*32*4, 0);
	sysDmaWaitTagStatusAll(1<<0);

	return(infoPacket);

}// end of ProcessSPU_cr0()...

#endif //__SPU....


