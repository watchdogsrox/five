//
// filename:	TaskScheduler.h
// description:	
//

#ifndef INC_TASKSCHEDULER_H_
#define INC_TASKSCHEDULER_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "system/taskheader.h"
// Game headers

// --- Defines ------------------------------------------------------------------

#define NUM_TASKS_IN_MGR	32
#define MAX_USER_DATA_IN_TASK	3

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

struct CTaskParams
{
	union {
		float asFloat;
		int asInt;
		void *asPtr;
		unsigned int asUInt;
		bool asBool;
	} UserData[MAX_USER_DATA_IN_TASK];
};

class CTaskScheduler
{
public:
	typedef void (*CTaskFunc)(const CTaskParams& params);

	CTaskScheduler() {}

	static void AddTask(CTaskFunc func, const CTaskParams& taskParams);
	static void WaitOnResults();
	static void Flush();
	static void StartBatchingTasks(s32 batchSize);

private:
	static void AddTask(sysTaskParameters& params);
	static void ProcessTask(sysTaskParameters& params);

	static s32 m_batchSize;
	static s32 m_batchCount;
	static s32 m_numTasks;
	static s32 m_currentTask;
	static sysTaskHandle m_taskHandles[NUM_TASKS_IN_MGR];
	static sysTaskParameters m_params;
};


// --- Globals ------------------------------------------------------------------

#endif // !INC_TASKSCHEDULER_H_
