//
//
//
//
//
#if __SPU
#include <spu_intrinsics.h>
#include <spu_printf.h>

#include "SpuPmMgr.h"
#include "SpuPmMgrSPU.h"

u8 lsBuffer128[128] ALIGNED(128);	// handy general purpose scratch buffer


// Define to enable the use of SPU events to reduce bus badnwidth usage during polling for jobs.
#define SPU_LOCK_USES_LLAR_EVENT		(1)

// Event mask to enable inter-spu communication of lost LLAR.
#define SPU_MFC_LLAR_LOST_EVENT			(1<<10)


static bool lock(u32 ea, volatile CSPUJobQueuePos *ls, u32 retryCount);
static void unlockAndIncrement(u32 ea, volatile CSPUJobQueuePos *ls);
static void unlock(u32 ea, volatile CSPUJobQueuePos *ls);





// Description:
// Attempt to get a job from the jobqueue.
// Arguments:
// pushEa - The effective address of the job queue push pointer in main memory.
// pullEa - The effective address of the job queue pull pointer in main memory.
// job - The job description structure to be filled with the retrieved job.
// sleepCountOnFail - Amount of sleep time to execute on failure to obtain a job.
// Return Value List:
// PE_JOB_UNAVAILABLE :			There were no jobs available.
// PE_JOB_AVAILABLEBUTINVALID :	There are jobs available, but the next job is not yet valid. It is blocked on a condition switch.
// PE_JOB_OBTAINED :			A job was successfully obtained.
eGetSpuJobFromQueueResult getJobFromJobQueue(u32 pushEa, u32 pullEa, CSPUJobQueueEntry *job, u32 sleepCountOnFail)
{
	volatile CSPUJobQueuePos *ptrMem = (volatile CSPUJobQueuePos*)lsBuffer128;

	// grab pull entry:
	// Lock the pull pointer.
	if(!lock(pullEa, ptrMem, 16))
		return PME_JOB_LOCKFAILED;
	void *pullEntry = (void*)ptrMem->m_curr;

	// grab pushEntry:
	sysDmaGet((void*)ptrMem, (u64)pushEa, AlignSize16(sizeof(CSPUJobQueuePos)), 0);
	sysDmaWaitTagStatusAll(1<<0);
	void *pushEntry = (void*)ptrMem->m_curr;


eGetSpuJobFromQueueResult result = PME_JOB_UNAVAILABLE;

	// Try to get a job.  Return false if we can't.
	if(pushEntry != pullEntry)
	{
		// Grab the job entry.
		sysDmaGet(lsBuffer128, (u64)pullEntry, AlignSize16(sizeof(CSPUJobQueueEntry)), 0);
		sysDmaWaitTagStatusAll(1<<0);

		//memcpy(job, lsBuffer, sizeof(CSPUJobQueueEntry));
		*job = *(CSPUJobQueueEntry*)lsBuffer128;

		result = PME_JOB_OBTAINED;

		// Now we need to test the job condition if one exists.
		if (job->m_jobValidSwitch)
		{
			// Transfer condition word from EA.
			volatile u32 *conditionValue = (volatile u32*)lsBuffer128;
			sysDmaGet(lsBuffer128, (u64)job->m_jobValidSwitch, 16, 0);
			sysDmaWaitTagStatusAll(1<<0);

			// TODO1: when conditionalValue overflows, then this immediately returns true (!);
			// problem when SPU is expecting job's ValidValue in range before overflow (=very big number),
			// but RSX is writing labels with labels after overflow (very small number)
			// TODO2: condition should be <=0, not <0 (otherwise job will be triggered after fence+1
			//        has been passed by RSX (see getGpuFence()):
			if ((int)(*conditionValue - job->m_jobValidValue) < 0)
			{
				// The job valid value exceeds the condition value, the job is not valid yet.
				result = PME_JOB_AVAILABLEBUTINVALID;
				//spu_printf("\n SPU: getJobFromJobQueue: fenceAddr=0x%X, val=%X, cur=%X", (u32)job->m_jobValidSwitch, job->m_jobValidValue, *conditionValue);
			}
		}
	}

	if (result == PME_JOB_OBTAINED)
		unlockAndIncrement(pullEa, ptrMem);
	else
	{
		// Sleep a bit - this stops us hitting the bus so hard.
		do
		{
			si_nop();
		}
		while (sleepCountOnFail--);
		unlock(pullEa, ptrMem);
	}

	return result;
}



// Description:
// Lock the job queue and get the job queue pointer.
// Arguments:
// ea - Effective address of the job queue pointer to lock and get.
// ls - 128 byte aligned local store buffer to local and fill with the job queue pointer
// retryCount - The number of times to retry the lock attempt.
// Return Value List:
// true - The lock succeeded.
// false - The lock failed within the retry count specified.
static bool lock(u32 ea, volatile CSPUJobQueuePos *ls, u32 retryCount)
{
int status=1;

#if SPU_LOCK_USES_LLAR_EVENT
	unsigned int event;

	// Discard previous (or phantom) events, as needed
	spu_write_event_mask(0);

	if (spu_stat_event_status() != 0)
	{
		event = spu_read_event_status();
		spu_write_event_ack(event);
	}

	// Enable SPU_MFC_LLAR_LOST_EVENT
	spu_write_event_mask(SPU_MFC_LLAR_LOST_EVENT);
#endif //SPU_LOCK_USES_LLAR_EVENT

	do
	{
		status = 1;
		mfc_getllar(ls, ea, 0, 0);
		mfc_read_atomic_status();
		if (ls->m_lockObtained == 0)
		{
			ls->m_lockObtained = 1;
			mfc_putllc(ls, ea, 0, 0);
			status = mfc_read_atomic_status();
		}

#if SPU_LOCK_USES_LLAR_EVENT
		if (status != 0)
		{
			event = spu_read_event_status();
			spu_write_event_ack(SPU_MFC_LLAR_LOST_EVENT);
		}
#endif //SPU_LOCK_USES_LLAR_EVENT
	}
	while ((retryCount--) && (status != 0));

#if SPU_LOCK_USES_LLAR_EVENT
	spu_write_event_mask(0);
	if (spu_stat_event_status() != 0)
	{
		event = spu_read_event_status();
		spu_write_event_ack(event);
	}
#endif //SPU_LOCK_USES_LLAR_EVENT

	// Status is 0 if we got the lock.
	return (status == 0);
}


// Description:
// Unlock and increment the job queue pointer.
// Arguments:
// ea - Effective address of the job queue pointer to increment.
// ls - 128 byte aligned local store buffer for the lock.
static void unlockAndIncrement(u32 ea, volatile CSPUJobQueuePos *ls)
{
int status=1;

	do
	{
		mfc_getllar(ls, ea, 0, 0);
		mfc_read_atomic_status();
		
		void* addr = (void*)(((u32)ls->m_curr) + ls->m_elementSize);
		if (addr >= ls->m_top)
			addr = ls->m_base;

		ls->m_curr = addr;
		ls->m_lockObtained = 0;
		mfc_putllc(ls, ea, 0, 0);
		status = mfc_read_atomic_status();
	}
	while (status != 0);
}


// Description:
// Unlock the job queue pointer.
// Arguments:
// ea - Effective address of the job queue pointer to increment.
// ls - 128 byte aligned local store buffer for the lock.
static void unlock(u32 ea, volatile CSPUJobQueuePos *ls)
{
int status=1;

	do
	{
		mfc_getllar(ls, ea, 0, 0);
		mfc_read_atomic_status();
		
		ls->m_lockObtained = 0;
		
		mfc_putllc(ls, ea, 0, 0);
		status = mfc_read_atomic_status();
	}
	while (status != 0);
}


#endif //__SPU...

