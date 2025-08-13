//
// filename:	TaskScheduler.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

#include "TaskScheduler.h"

// C headers
// Rage headers
#include "system\task.h"
// Game headers

// --- Defines ------------------------------------------------------------------

#define USE_TASK_SCHEDULER	__XENON
#if USE_TASK_SCHEDULER
#define USE_TASK_SCHEDULER_ONLY(X)	X
#else
#define USE_TASK_SCHEDULER_ONLY(X)
#endif

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

#if USE_TASK_SCHEDULER

s32 CTaskScheduler::m_numTasks;
s32 CTaskScheduler::m_currentTask;
s32 CTaskScheduler::m_batchSize=1;
s32 CTaskScheduler::m_batchCount;
sysTaskHandle CTaskScheduler::m_taskHandles[NUM_TASKS_IN_MGR];
sysTaskParameters CTaskScheduler::m_params;

#endif

// --- Code ---------------------------------------------------------------------

void CTaskScheduler::AddTask(CTaskFunc func, const CTaskParams& taskParams)
{
#if USE_TASK_SCHEDULER
	s32 index = m_batchCount * (MAX_USER_DATA_IN_TASK+1);

	m_params.UserData[index].asPtr = (void*)func;
	// copy task params to sysTaskParameters structure
	for(s32 i=0; i<MAX_USER_DATA_IN_TASK; i++)
		m_params.UserData[index + i+1].asInt = taskParams.UserData[i].asInt;
	m_batchCount++;
	m_params.UserDataCount = m_batchCount * (MAX_USER_DATA_IN_TASK+1);

	if(m_batchCount == m_batchSize)
	{
		AddTask(m_params);
		m_batchCount = 0;
	}
#else
	func(taskParams);
#endif
}

#if USE_TASK_SCHEDULER
void CTaskScheduler::AddTask(sysTaskParameters& params)
{
	// if task handle array is full then wait on task to finish
	if(m_numTasks == NUM_TASKS_IN_MGR)
	{
		sysTaskManager::Wait(m_taskHandles[m_currentTask]);
		m_numTasks--;
	}

	m_taskHandles[m_currentTask] = rage::sysTaskManager::Create(TASK_INTERFACE(ProcessTask), params);
	m_numTasks++;
	m_currentTask++;
	// wrap current task
	if(m_currentTask == NUM_TASKS_IN_MGR)
		m_currentTask = 0;
}
#endif

void CTaskScheduler::Flush()
{
#if USE_TASK_SCHEDULER
	if(m_batchCount > 0)
		AddTask(m_params);
	m_batchCount = 0;
#endif
}

void CTaskScheduler::WaitOnResults()
{
#if USE_TASK_SCHEDULER
	Flush();

	if(m_numTasks > 0)
		sysTaskManager::WaitMultiple(m_numTasks, m_taskHandles);
	m_numTasks = 0;
	m_currentTask = 0;
#endif
}

void CTaskScheduler::StartBatchingTasks(s32 USE_TASK_SCHEDULER_ONLY(batchSize))
{
#if USE_TASK_SCHEDULER
	Assert(batchSize <= TASK_MAX_USER_DATA / (MAX_USER_DATA_IN_TASK+1) );
	m_batchSize = batchSize;
#endif
}

#if USE_TASK_SCHEDULER
void CTaskScheduler::ProcessTask(sysTaskParameters& params)
{
	for(u32 i=0; i<params.UserDataCount; i += (MAX_USER_DATA_IN_TASK+1))
	{
		CTaskParams taskParams;
		// copy task params to sysTaskParameters structure
		for(s32 j=0; j<MAX_USER_DATA_IN_TASK; j++)
			taskParams.UserData[j].asInt = params.UserData[j+1+i].asInt;

		((CTaskFunc)params.UserData[i].asPtr)(taskParams);
	}
}
#endif
