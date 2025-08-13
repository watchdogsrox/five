//
//
//
//
//
#ifndef __SPUPMMGR_SPU_H__
#define __SPUPMMGR_SPU_H__
#if __SPU

#include "system/dma.h"
#include <cell/atomic.h>

#include "SpuPmMgr_pm.h"

typedef void (*ptrJobHandlerFn)(void *infoPacketSource, unsigned int infoPacketSize);


// Enumerated value representing the results of an attempt to get a job from the job queue.
enum eGetSpuJobFromQueueResult
{
	PME_JOB_OBTAINED			= 0,	// A job was successfully obtained.
	PME_JOB_UNAVAILABLE			= 1,	// There were no jobs available.
	PME_JOB_AVAILABLEBUTINVALID	= 2,	// There are jobs available, but the next job is not yet valid. It is blocked on a condition switch.
	PME_JOB_LOCKFAILED			= 3		// The job queue failed to lock in a reasonable amount of time.
};

extern u8 lsBuffer128[128];
extern eGetSpuJobFromQueueResult getJobFromJobQueue(u32 pushEa, u32 pullEa, CSPUJobQueueEntry *job, u32 sleepCountOnFail);


// Description:
// Align a given size with the next multiple of 16.
// Arguments:
// size - Size value to align.
// Returns:
// Aligned size.
static inline u32 AlignSize16(u32 size)
{
	return (size + 0xF) & ~0xF;
}

static inline bool IsAligned(u32 lsAddr, u32 align)
{
	return !((lsAddr) & (align-1));
}


static inline u32 Align(u32 lsAddr, u32 align)
{
	return IsAligned(lsAddr, align)? (lsAddr) : ((lsAddr + align) & (~(align-1)));
}


static inline u32 stepLSAddr(u32 lsAddr, u32 step)
{
	lsAddr = (u32)((unsigned char*)lsAddr + step);
//	Assert(lsAddr <= (256*1024));
	Assert(lsAddr <= (PM_SPU_MEMORY_MAP_STACK_BASE-PM_SPU_MEMORY_MAP_STACK_SIZE));
	return lsAddr;
}


//
// Matches LS address alignment with an effective address alignment, in order to get the best DMA performance.
// The alignment is matched to a granularity of 128 bytes.
//
// Arguments:
// lsAddr : The local store address to align.
// eaAddr : The effective address to align.
// Returns:
// The aligned local store address greater or equal to the input lsAddr.
//
static inline u32 matchAlignment128(u32 lsAddr, void* eaAddr)
{
	u32 newLsAddr = (u32)si_to_ptr((qword)spu_sel((vec_uint4)si_from_ptr((void*)lsAddr), (vec_uint4)si_from_ptr(eaAddr), spu_splats(127u)));

	if (newLsAddr < lsAddr)
		newLsAddr = stepLSAddr(newLsAddr, 128);

	return newLsAddr;
}

static inline u32 matchAlignment16(u32 lsAddr, void* eaAddr)
{
	u32 newLsAddr = (u32)si_to_ptr((qword)spu_sel((vec_uint4)si_from_ptr((void*)lsAddr), (vec_uint4)si_from_ptr(eaAddr), spu_splats(15u)));

	if (newLsAddr < lsAddr)
		newLsAddr = stepLSAddr(newLsAddr, 16);

	return newLsAddr;
}

//
// Write the specified 4 byte word to the specified effective address.
//
// Arguments:
// eaAddr	- The effective address to write the word to.
// value	- The word to write.
// tag		- The DMA tag to write the word with.
// storage	- Reference to an aligned quadword of storage used for aligning the transfer.
//			This must remain valid and undisturbed until the dma transfer is synced.
static inline void writeEAWord(void* eaAddr, u32 value, u32 tag, vec_uint4 &storage)
{
	if(eaAddr)
	{
		storage = spu_splats(value);
		u32 *alignedLSAddr = (u32*)matchAlignment16((u32)&storage, eaAddr);
		sysDmaSmallPutf(alignedLSAddr, (u64)eaAddr, sizeof(u32), tag);
	}
}


//
// Finalize the post process operation by writing the gates.
// This frees the bounce buffer and causes the GPU to continue if
// it is waiting for the post processing results.
//
// Arguments:
// postProcessPacketInfo - The packet info structure for the post process operation.
// buffer - 128 byte buffer aligned on 128 byte boundary.
// tag - The DMA tag to use for the DMA transfers.
//
static inline void finalizeSpuGtaProcess(const CSPUBaseDriverInfo *packetInfo, u8 *buffer128, u32 tag)
{

	if(packetInfo->m_gateAddr)
	{
		vec_uint4 *storage = (vec_uint4*)buffer128;
		writeEAWord(packetInfo->m_gateAddr, packetInfo->m_gateValue, tag, *storage);
		sysDmaWaitTagStatusAll(1<<tag);
	}

	// atomically decrement gate#0:
	if(packetInfo->m_gateDecrAddr[0])
	{
		cellAtomicDecr32((u32*)buffer128, (u64)packetInfo->m_gateDecrAddr[0]);
	}

	// atomically decrement gate#1:
	if(packetInfo->m_gateDecrAddr[1])
	{
		cellAtomicDecr32((u32*)buffer128, (u64)packetInfo->m_gateDecrAddr[1]);
	}

	// atomically decrement gate#2:
	if(packetInfo->m_gateDecrAddr[2])
	{
		cellAtomicDecr32((u32*)buffer128, (u64)packetInfo->m_gateDecrAddr[2]);
	}

	// atomically decrement gate#3:
	if(packetInfo->m_gateDecrAddr[3])
	{
		cellAtomicDecr32((u32*)buffer128, (u64)packetInfo->m_gateDecrAddr[3]);
	}

}// end of finalizeProcess()...



// SN Tuner tracing stuff:
#if SPUPM_ENABLE_TUNER_TRACE

#include <cell/spurs/trace.h>
#define PC_SPURSTRACE_DMA_TAG			(31)	// dma tag for spurs trace dma transfers.

// Description:
// Sends a spurs trace start packet. This function is adding the ls address information into the packet.
// Arguments:
// packet - Pointer to the CellSpursTracePacket to be sent.
// code   - Pointer to the code to be executed.
// tag    - Dma tag for the trace packet.
__attribute__((always_inline)) inline void spursTraceStart(CellSpursTracePacket *packet, const void *code, unsigned int tag)
{
	packet->data.start.ls	= ((uint32_t) code) >>2;
	cellSpursModulePutTrace(packet, tag);
}
// Description:
// Sends a spurs trace start packet.
// Arguments:
// packet - Pointer to the valid/completely filled out CellSpursTracePacket to be sent.
// tag    - Dma tag for the trace packet.
__attribute__((always_inline)) inline void spursTraceStart(CellSpursTracePacket *packet, u32 tag)
{
    cellSpursModulePutTrace(packet, tag);
}

// Description:
// Shuffle word to extract the guid of an spu elf.
static const vec_uchar16 guidShuffle = (vec_uchar16) {
    0,1,4,5,
	8,9,12,13,
	0,1,4,5,
	8,9,12,13};

// Description:
// Sends a spurs trace stop packet. This function is caclulating the guid id required for the stop packet.
// Arguments:
// packet - Pointer to the CellSpursTracePacket to be sent.
// code   - Pointer to the code that was executed.
// tag    - Dma tag for the trace packet.
__attribute__((always_inline)) inline void spursTraceStop(CellSpursTracePacket *packet, const void *code, u32 tag)
{
	qword insn			= si_roti(*((const qword *)code),7);
	uint64_t guid		= si_to_ullong(si_shufb(insn, insn, (const qword) guidShuffle));
	packet->data.stop	= guid;
	cellSpursModulePutTrace(packet, tag);
}
#endif //SPUPM_ENABLE_TUNER_TRACKING...


#endif //__SPU...
#endif //__SPUPMMGR_SPU_H__...

