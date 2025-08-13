//
// pm_main:
//
//
//

#if __SPU
#define DEBUG_SPU_JOB_NAME "spupmmgr_pm"

#define AM_SPU_DEBUG				(0)
#define SPU_DEBUGF(X)				X
//#define SPU_DEBUGF(x)
#define SPU_DEBUGF1(X)				X

#define SPUPM_ENABLE_TUNER_TRACE	(1)	// enable SN Tuner tracking packets

#undef	__STRICT_ALIGNED			// new.h compile warnings
#define ATL_INMAP_H					// inmap.h compile warnings

#define SYS_DMA_VALIDATION			(0)	// dma LS destination checking doesn't work for pm modules
#include "system/dma.h"
#include "system/spu_library.cpp"

#include <cell/spurs.h>
#include <cell/spurs/policy_module.h>
#include <cell/spurs/ready_count.h>
#include <spu_printf.h>

#include "SpuPmMgr.h"
#include "SpuPmMgrSPU.h"
#include "SpuPmMgrSPU.cpp"


//extern u8 lsBuffer[128];

extern char _end[];


// Global to store the handler entry. Is initialized to some invalid EA address (the job handler never should start at 0!)
void *cachedHandlerEA = (void *) ~0;



static inline void spuSleepNop(u32 sleepCount)
{
	do
	{
		si_nop();
	}
	while (sleepCount--);
}

static inline void spuSleep(u32 v)
{
	u32 v0 = spu_read_decrementer();
	while(1)
	{
		u32 v1 = spu_read_decrementer();
		// if decrementer overflows, then (v0-v1) will be really big number (so spuSleep() will exit immediately)
		// otherwise it works as expected:
		if(/*SPU_IABS*/(v0-v1) > v)
			break;
	}
}



// Description:
// SPURS policy module main function.
// Arguments:
// pKernelContext - The SPURS kernel context pointer, used when calling SPURS.
// ea - The effective address of the arguments. The arguments contain the effective address of the jobqueue pull and push pointers.
void pm_main(uintptr_t pKernelContext, uint64_t ea, CellSpursModulePollStatus pollStatus)
{
	if(pollStatus&CELL_SPURS_MODULE_POLL_STATUS_FLAG)	// workload flag set?
	{

#if SPUPM_ENABLE_TUNER_TRACE
	CellSpursTracePacket tracePacketStart, tracePacketStop;

	tracePacketStart.header.tag				= CELL_SPURS_TRACE_TAG_START;
	tracePacketStop.header.tag				= CELL_SPURS_TRACE_TAG_STOP;
	tracePacketStart.header.length			=
	tracePacketStop.header.length			= 2;	// always 2
	tracePacketStart.header.spu				=
	tracePacketStop.header.spu				= 0;	// SPU number
	tracePacketStart.header.workload		=
	tracePacketStop.header.workload			= cellSpursGetWorkloadId();	// workloadID
	tracePacketStart.header.time			=
	tracePacketStop.header.time				= 0;	// time base
	tracePacketStart.data.rawData			=
	tracePacketStop.data.rawData			= 0;

	tracePacketStart.data.start.module[0]	= 'J';
	tracePacketStart.data.start.module[1]	= 'P';
	tracePacketStart.data.start.module[2]	= 'M';
	tracePacketStart.data.start.module[3]	= '\0';
	tracePacketStart.data.start.level		= 1;	// module level
	tracePacketStart.data.start.ls			= (unsigned int) (0xa00) >> 2;

	spursTraceStart(&tracePacketStart, PC_SPURSTRACE_DMA_TAG);// Stop is done in pm_main because it does not return.
#endif //SPUPM_ENABLE_TUNER_TRACE...


	cellSpursModuleReadyCountAdd(0, 1);	// increase the readyCount

	// get the SPURS args:
	cellDmaGet((void*)lsBuffer128, ea, 128, 0, 0, 0);
	cellDmaWaitTagStatusAll(1 << 0);

	volatile u32 *arg	= (volatile u32*)lsBuffer128;
	u32		pushEa		= arg[0];
	u32		pullEa		= arg[1];
	u32		pushEa2		= arg[2];
	u32		pullEa2		= arg[3];

#if AM_SPU_DEBUG
	const u32 frameID	= arg[4];
//	const u32 taskID	= cellSpursGetTaskId();
//	const u32 workloadID= cellSpursGetWorkloadId();
//	spu_printf("\n Jimmy: taskID=%X, workloadID=%X", taskID, workloadID);

//	spu_printf("\n Jimmy: pm_main: entered: pushEa=%x, pushEa2=%x!", pushEa, pushEa2);
	SPU_DEBUGF(spu_printf("\n SPU%d: pm_main: entered: arg0=%08X, arg1=%08X, arg2=%08X, arg3=%08X", frameID, (u32)arg[0], (u32)arg[1], (u32)arg[2], (u32)arg[3]));
	//SPU_DEBUGF(spu_printf("\n SPU%d: pm_main: entered: frame=%d, arg5=%08X, arg6=%08X, arg7=%08X", frameID, (u32)arg[4], (u32)arg[5], (u32)arg[6], (u32)arg[7]));
#endif

//	spuSleep(10000000);


#if SPUPM_ENABLE_TUNER_TRACE
	tracePacketStart.data.start.module[0]	= 'G';
	tracePacketStart.data.start.module[1]	= 'T';
	tracePacketStart.data.start.module[2]	= 'A';
	tracePacketStart.data.start.module[3]	= '\0';
	tracePacketStart.data.start.level		= 2;	// unit level
	tracePacketStart.data.start.ls			= (unsigned int) (_end) >> 2;
#endif //SPUPM_ENABLE_TUNER_TRACE...

CSPUJobQueueEntry	job;
	
	// Get a job - prefer the second queue since it contains work the GPU is more tightly synchronized to.
	while(	(getJobFromJobQueue(pushEa2, pullEa2, &job, 0)		== PME_JOB_OBTAINED)	||
			(getJobFromJobQueue(pushEa,  pullEa,  &job, 1000)	== PME_JOB_OBTAINED)	)
	{
//		SPU_DEBUGF(spu_printf("\n SPU%d: executing job: count=%d frame=%d", frameID, (u32)job.m_infoPacketEA, (u32)job.m_infoPacketSize));
//		spu_printf("\n spu_pm: jobEA=0x%X, jobSize=%d, jobEntry=0x%X", (u32)job.m_jobHandlerEA, (u32)job.m_jobHandlerSize, (u32)job.m_jobHandlerEntry);
//		spu_printf("\n spu_pm: _end=0x%X, available=%d", (u32)_end, (256*1024)-((u32)_end));

		unsigned int entry = u32(_end) + job.m_jobHandlerEntry;
		if (cachedHandlerEA == job.m_jobHandlerEA)
		{
			// assume this is the most likely case.
		} else {
			sysDmaLargeGet(_end, (u64)job.m_jobHandlerEA, AlignSize16(job.m_jobHandlerSize), 0);
			sysDmaWaitTagStatusAll(1<<0);
			//spu_printf("\n memcpy_large_LS: _end=0x%X, job.m_jobHandlerEA=0x%X, job.m_jobHandlerSize=%d", (u32)_end, (u32)job.m_jobHandlerEA, (u32)AlignSize16(job.m_jobHandlerSize));
			cachedHandlerEA = job.m_jobHandlerEA;
			spu_sync();
		}

		ptrJobHandlerFn process = (ptrJobHandlerFn)entry;


#if SPUPM_ENABLE_TUNER_TRACE
		spursTraceStart(&tracePacketStart, PC_SPURSTRACE_DMA_TAG);
#endif
		{
			// start processing:
			process(job.m_infoPacketEA, job.m_infoPacketSize);
		}
#if SPUPM_ENABLE_TUNER_TRACE
		spursTraceStop(&tracePacketStop, _end, PC_SPURSTRACE_DMA_TAG);
		sysDmaWaitTagStatusAll(1<<PC_SPURSTRACE_DMA_TAG);
#endif

	}// end of while(getJobFromJobQueue())...



#if AM_SPU_DEBUG
	SPU_DEBUGF(spu_printf("\n SPU%d: pm_main: exited!", frameID));
#endif

		cellSpursModuleReadyCountAdd(0, -1);	// decrease the readyCount


#if SPUPM_ENABLE_TUNER_TRACE
	// Stop for level 1. Start is in spurs_pm_entry.cpp
	// Stop needs to be here because we dont return from pm_main().
	spursTraceStop(&tracePacketStop, (void*)0xa00, PC_SPURSTRACE_DMA_TAG);
	sysDmaWaitTagStatusAll(1<<PC_SPURSTRACE_DMA_TAG);
#endif //SPUPM_ENABLE_TUNER_TRACE...

	}// if(pollStatus&CELL_SPURS_MODULE_POLL_STATUS_FLAG)...

	cellSpursModuleExit(pKernelContext);
}


#endif//__SPU...

