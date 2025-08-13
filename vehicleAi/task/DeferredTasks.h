#ifndef VEHICLE_AVOIDANCE_JOB_H
#define VEHICLE_AVOIDANCE_JOB_H

#include "physics/lockconfig.h"
#include "system/taskheader.h"
#include "vehicles/vehicle.h"

class CTask;

namespace aiDeferredTasks
{

#define MAX_DEFERRED_TASKS 512
#define MAX_DEFERRED_TASK_WORKERS 4

class TaskData
{
public:
	TaskData(CTask* task, CVehicle* vehicle, float timeStep, bool fullUpdate) : m_Task(task), m_Vehicle(vehicle), m_TimeStep(timeStep), m_FullUpdate(fullUpdate) {}
	TaskData() : m_Task(NULL), m_Vehicle(NULL), m_FullUpdate(false), m_TimeStep(0.0f) {}
	~TaskData() {}

	RegdTask m_Task;
	RegdVeh m_Vehicle;
	float m_TimeStep;
	bool m_FullUpdate;
};

////////////////////////////////////////////////////////////////////////////////

class DeferredTask
{
public:
	DeferredTask();
	virtual ~DeferredTask() {}

	void Reset();

	void Queue(const TaskData& data);

	virtual void Run() = 0;

protected:
	bool ValidateTaskData(const TaskData* data)
	{
		Assert(data);
		return (!m_RequiresValidTask || data->m_Task)
			&& (!m_RequiresValidVehicle || (data->m_Vehicle && !data->m_Vehicle->GetIsInReusePool()));
	}

	void CreateWorkers(void (*taskCb)(TaskData*));
	void Wait();
	void CreateWorkersAndWait(void (*taskCb)(TaskData*));
	static void WorkerCb(sysTaskParameters& params);

	void SetUseLockCommon(bool useLock);
	void SetOnlyMainThreadIsUpdatingPhysics(bool onlyMainThreadIsUpdatingPhysics);

	atFixedArray<TaskData, MAX_DEFERRED_TASKS> m_Tasks;
	sysLockFreeRing<TaskData, MAX_DEFERRED_TASKS> m_Ring;

private:
	sysTaskHandle m_Workers[MAX_DEFERRED_TASK_WORKERS];
	sysTaskParameters m_WorkerParameters;
	void (*m_TaskCb)(TaskData*);

protected:
	u32 m_NumWorkers;
	bool m_RequiresValidTask;
	bool m_RequiresValidVehicle;

private:
	bool m_OnlyMainThreadWasUpdatingPhysics;
#if ENABLE_PHYSICS_LOCK
	bool m_OnlyMainThreadWasUpdatingPhysicsLockConfig;
#endif // ENABLE_PHYSICS_LOCK
#if __ASSERT
	bool m_PhysicsLocksEnabled;
#endif // __ASSERT
};

////////////////////////////////////////////////////////////////////////////////

class VehicleAvoidanceTask : public DeferredTask
{
public:
	VehicleAvoidanceTask();
	virtual ~VehicleAvoidanceTask() {}

	virtual void Run();

	static void GoToPointWithAvoidanceCb(TaskData* data);
	static void GoToPointWithAvoidance_ProcessSuperDummyCb(TaskData* data);
};

////////////////////////////////////////////////////////////////////////////////

class VehicleGoToTask : public DeferredTask
{
public:
	VehicleGoToTask();
	virtual ~VehicleGoToTask() {}

	virtual void Run();

	static void GoToPointCb(TaskData* data);
};

////////////////////////////////////////////////////////////////////////////////

class VehicleIntelligenceProcessTask : public DeferredTask
{
public:
	VehicleIntelligenceProcessTask();
	virtual ~VehicleIntelligenceProcessTask() {}

	virtual void Run();

	static void Process_OnDeferredTaskCompletionCb(TaskData* data);
};

////////////////////////////////////////////////////////////////////////////////

class VehicleInterpolationTask : public DeferredTask
{
public:
	VehicleInterpolationTask();
	virtual ~VehicleInterpolationTask() {}

	virtual void Run();

	static void ProcessInterpolationCb(TaskData* data);
};

////////////////////////////////////////////////////////////////////////////////

void RunAll();

extern VehicleAvoidanceTask g_VehicleAvoidance;
extern VehicleGoToTask g_VehicleGoTo;
extern VehicleIntelligenceProcessTask g_VehicleIntelligenceProcess;
extern VehicleInterpolationTask g_VehicleInterpolation;
extern sysCriticalSectionToken g_BoxStreamerToken;
extern sysCriticalSectionToken g_EventGroupToken;
extern sysCriticalSectionToken g_AvoidancePoolToken;
extern bool g_Running;

}

#define AI_AVOIDANCE_POOL_LOCK sysCriticalSection cs(aiDeferredTasks::g_AvoidancePoolToken, aiDeferredTasks::g_Running);
#define AI_DEFERRED_TASKS_BOXSTREAMER_LOCK sysCriticalSection cs(aiDeferredTasks::g_BoxStreamerToken, aiDeferredTasks::g_Running);
#define AI_DEFERRED_TASKS_EVENTGROUPGLOBAL_LOCK sysCriticalSection cs(aiDeferredTasks::g_EventGroupGlobalToken, aiDeferredTasks::g_Running);

#endif // VEHICLE_AVOIDANCE_JOB_H