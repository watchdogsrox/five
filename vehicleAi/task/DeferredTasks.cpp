#include "DeferredTasks.h"
#include "ai/spatialarray.h"
#include "audio/environment/environmentgroup.h"
#include "event/Event.h"
#include "fwtl/regdrefs.h"
#include "scene/2dEffect.h"
#include "system/ipc.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/VehicleIntelligence.h"

#if __STATS
PF_PAGE(deferredAiPage, "AI Deferred Tasks");
PF_GROUP(deferredAiGroup);
PF_LINK(deferredAiPage, deferredAiGroup);

PF_TIMER(DeferredAi_PedsVehiclesObjects, deferredAiGroup);

PF_TIMER(DeferredAiTask_Queue, deferredAiGroup);
PF_TIMER(DeferredAiTask_Create, deferredAiGroup);
PF_TIMER(DeferredAiTask_Wait, deferredAiGroup);
PF_TIMER(DeferredAiTask_MainThreadIdle, deferredAiGroup);

PF_TIMER(VehicleAvoidanceTask_Run, deferredAiGroup);
PF_TIMER(VehicleAvoidanceTask_Finalize, deferredAiGroup);

PF_TIMER(VehicleGoToTask_Run, deferredAiGroup);

PF_TIMER(VehicleIntelligenceProcessTask_Run, deferredAiGroup);

PF_TIMER(VehicleInterpolationTask_Run, deferredAiGroup);
PF_TIMER(VehicleInterpolationTask_Finalize, deferredAiGroup);
#endif // __STATS

namespace aiDeferredTasks
{

VehicleAvoidanceTask g_VehicleAvoidance;
VehicleGoToTask g_VehicleGoTo;
VehicleIntelligenceProcessTask g_VehicleIntelligenceProcess;
VehicleInterpolationTask g_VehicleInterpolation;
sysCriticalSectionToken g_BoxStreamerToken;
sysCriticalSectionToken g_EventGroupToken;
sysCriticalSectionToken g_AvoidancePoolToken;
bool g_Running = false;

////////////////////////////////////////////////////////////////////////////////

DeferredTask::DeferredTask() 
: m_OnlyMainThreadWasUpdatingPhysics(false)
#if ENABLE_PHYSICS_LOCK
, m_OnlyMainThreadWasUpdatingPhysicsLockConfig(false)
#endif // ENABLE_PHYSICS_LOCK
#if __ASSERT
, m_PhysicsLocksEnabled(false)
#endif // __ASSERT
{
	memset(m_Workers, 0, sizeof(sysTaskHandle) * MAX_DEFERRED_TASK_WORKERS);
	m_WorkerParameters.UserData[0].asPtr = this;
	m_WorkerParameters.UserDataCount = 1;

	Reset();
}

void DeferredTask::Reset()
{
	m_Tasks.Reset();
}

void DeferredTask::Queue(const TaskData& data)
{
	PF_FUNC(DeferredAiTask_Queue);
	Assert(data.m_Vehicle);

	//for (u32 i = 0; i < m_DeferredTasks.GetCount(); ++i)
	//{
	//	Assert(m_DeferredTasks[i].m_Task != data.m_Task);
	//}

	m_Tasks.Append() = data;
}

void DeferredTask::CreateWorkers(void (*taskCb)(TaskData*))
{
	PF_FUNC(DeferredAiTask_Create);

	m_TaskCb = taskCb;

	for (u32 i = 0; i < m_Tasks.GetCount(); ++i)
	{
		if (ValidateTaskData(&m_Tasks[i]))
		{
			m_Ring.Push(&m_Tasks[i]);
		}
	}

	// If we only have one task, skip worker creation -- the task will run on the main thread
	for (u32 i = 0; i < m_NumWorkers && i < m_Tasks.GetCount() - 1; ++i)
	{
		Assert(!m_Workers[i]);

		m_Workers[i] = sysTaskManager::Create(TASK_INTERFACE(WorkerCb), m_WorkerParameters);
	}
}

void DeferredTask::Wait()
{
	PF_FUNC(DeferredAiTask_Wait);

	WorkerCb(m_WorkerParameters);

	PF_START(DeferredAiTask_MainThreadIdle);
	sysTaskManager::WaitMultiple(m_NumWorkers, m_Workers);
	PF_STOP(DeferredAiTask_MainThreadIdle);

	memset(&m_Workers, 0, sizeof(sysTaskHandle) * m_NumWorkers);
}

void DeferredTask::CreateWorkersAndWait(void (*taskCb)(TaskData*))
{
	CreateWorkers(taskCb);
	Wait();
}

void DeferredTask::WorkerCb(sysTaskParameters& params)
{
	DeferredTask *creator = static_cast<DeferredTask*>(params.UserData[0].asPtr);

	while (!creator->m_Ring.IsEmpty())
	{
		TaskData* data = creator->m_Ring.Pop();

		if (data)
		{
			Assert(creator->ValidateTaskData(data));
			creator->m_TaskCb(data);
		}
	}
}

void DeferredTask::SetUseLockCommon(bool useLock)
{
	if (m_NumWorkers > 0 && m_Tasks.GetCount() > 1)
	{
		fwRegdRefCriticalSection::SetUseLock(useLock);
		CEventCriticalSection::SetUseLock(useLock);
		naEnvironmentGroupCriticalSection::SetUseLock(useLock);
		CSpatialArray::SetUseLock(useLock);
		if(NetworkInterface::IsNetworkOpen())
		{
			NetworkInterface::AllowOtherThreadsAccessToNetworkManager(useLock);
		}
		aiTaskTree::SetAllowDelete(useLock ? false : true); //if locking, don't allow task deletion.
	}
}

void DeferredTask::SetOnlyMainThreadIsUpdatingPhysics(bool onlyMainThreadIsUpdatingPhysics)
{
	if (m_NumWorkers > 0 && m_Tasks.GetCount() > 1)
	{
		if (onlyMainThreadIsUpdatingPhysics)
		{
			Assert(m_PhysicsLocksEnabled);

			if (m_OnlyMainThreadWasUpdatingPhysics)
			{
				PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
			}

#if ENABLE_PHYSICS_LOCK
			if (m_OnlyMainThreadWasUpdatingPhysicsLockConfig)
			{
				g_OnlyMainThreadIsUpdatingPhysicsLockConfig = true;
			}
#endif // ENABLE_PHYSICS_LOCK

			ASSERT_ONLY(m_PhysicsLocksEnabled = false;)
		}
		else
		{
			Assert(!m_PhysicsLocksEnabled);

			m_OnlyMainThreadWasUpdatingPhysics = PHLEVEL->GetOnlyMainThreadIsUpdatingPhysics();

			if (m_OnlyMainThreadWasUpdatingPhysics)
			{
				PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
			}

#if ENABLE_PHYSICS_LOCK
			m_OnlyMainThreadWasUpdatingPhysicsLockConfig = g_OnlyMainThreadIsUpdatingPhysicsLockConfig;
			g_OnlyMainThreadIsUpdatingPhysicsLockConfig = false;
#endif // ENABLE_PHYSICS_LOCK

			ASSERT_ONLY(m_PhysicsLocksEnabled = true;)
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

VehicleAvoidanceTask::VehicleAvoidanceTask() : DeferredTask()
{
	m_NumWorkers = MAX_DEFERRED_TASK_WORKERS;
	m_RequiresValidTask = true;
	m_RequiresValidVehicle = true;
}

void VehicleAvoidanceTask::Run()
{
	PF_FUNC(VehicleAvoidanceTask_Run);

	if (!m_Tasks.IsEmpty())
	{
		SetUseLockCommon(true);

		if(NetworkInterface::IsNetworkOpen())
		{
			NetworkInterface::ForceSortNetworkObjectMap();
		}

		CreateWorkersAndWait(GoToPointWithAvoidanceCb);

		SetOnlyMainThreadIsUpdatingPhysics(false);

		CreateWorkersAndWait(GoToPointWithAvoidance_ProcessSuperDummyCb);

		SetUseLockCommon(false);
		SetOnlyMainThreadIsUpdatingPhysics(true);

		PF_START(VehicleAvoidanceTask_Finalize);
		for (u32 i = 0; i < m_Tasks.GetCount(); ++i)
		{
			if (ValidateTaskData(&m_Tasks[i]))
			{
				static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(m_Tasks[i].m_Task.Get())->GoToPointWithAvoidance_OnDeferredTaskCompletion(m_Tasks[i]);
			}
		}
		PF_STOP(VehicleAvoidanceTask_Finalize);

		Reset();
	}
}

void VehicleAvoidanceTask::GoToPointWithAvoidanceCb(TaskData* data)
{
	static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(data->m_Task.Get())->GoToPointWithAvoidance_OnDeferredTask(*data);
}

void VehicleAvoidanceTask::GoToPointWithAvoidance_ProcessSuperDummyCb(TaskData* data)
{
	static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(data->m_Task.Get())->GoToPointWithAvoidance_OnDeferredTask_ProcessSuperDummy(*data);
}

////////////////////////////////////////////////////////////////////////////////

VehicleGoToTask::VehicleGoToTask() : DeferredTask()
{
	m_NumWorkers = MAX_DEFERRED_TASK_WORKERS;
	m_RequiresValidTask = true;
	m_RequiresValidVehicle = true;
}

void VehicleGoToTask::Run()
{
	PF_FUNC(VehicleGoToTask_Run);

	if (!m_Tasks.IsEmpty())
	{
		SetUseLockCommon(true);

		if(NetworkInterface::IsNetworkOpen())
		{
			NetworkInterface::ForceSortNetworkObjectMap();
		}

		CreateWorkersAndWait(GoToPointCb);

		SetUseLockCommon(false);

		Reset();
	}
}

void VehicleGoToTask::GoToPointCb(TaskData* data)
{
	static_cast<CTaskVehicleGoToPointAutomobile*>(data->m_Task.Get())->GoToPoint_OnDeferredTask(*data);
}

////////////////////////////////////////////////////////////////////////////////

VehicleIntelligenceProcessTask::VehicleIntelligenceProcessTask() : DeferredTask()
{
	m_NumWorkers = 0;
	m_RequiresValidTask = false;
	m_RequiresValidVehicle = true;
}

void VehicleIntelligenceProcessTask::Run()
{
	PF_FUNC(VehicleIntelligenceProcessTask_Run);

	if (!m_Tasks.IsEmpty())
	{
		SetUseLockCommon(true);
		SetOnlyMainThreadIsUpdatingPhysics(false);

		if(NetworkInterface::IsNetworkOpen())
		{
			NetworkInterface::ForceSortNetworkObjectMap();
		}

		CreateWorkersAndWait(Process_OnDeferredTaskCompletionCb);

		SetUseLockCommon(false);
		SetOnlyMainThreadIsUpdatingPhysics(true);

		Reset();
	}
}

void VehicleIntelligenceProcessTask::Process_OnDeferredTaskCompletionCb(TaskData* data)
{
	data->m_Vehicle->GetIntelligence()->Process_OnDeferredTaskCompletion();
}

////////////////////////////////////////////////////////////////////////////////

VehicleInterpolationTask::VehicleInterpolationTask() : DeferredTask()
{
	m_NumWorkers = MAX_DEFERRED_TASK_WORKERS;
	m_RequiresValidTask = false;
	m_RequiresValidVehicle = true;
}

void VehicleInterpolationTask::Run()
{
	PF_FUNC(VehicleInterpolationTask_Run);

	if (!m_Tasks.IsEmpty())
	{
		SetUseLockCommon(true);
		SetOnlyMainThreadIsUpdatingPhysics(false);

		if(NetworkInterface::IsNetworkOpen())
		{
			NetworkInterface::ForceSortNetworkObjectMap();
		}

		CreateWorkersAndWait(ProcessInterpolationCb);

		SetUseLockCommon(false);
		SetOnlyMainThreadIsUpdatingPhysics(true);

		PF_START(VehicleInterpolationTask_Finalize);
		for (u32 i = 0; i < m_Tasks.GetCount(); ++i)
		{
			if (ValidateTaskData(&m_Tasks[i]))
			{
				m_Tasks[i].m_Vehicle->DoProcessControl_OnDeferredTaskCompletion(m_Tasks[i].m_FullUpdate, m_Tasks[i].m_TimeStep);
			}
		}
		PF_STOP(VehicleInterpolationTask_Finalize);

		Reset();
	}
}

void VehicleInterpolationTask::ProcessInterpolationCb(TaskData* data)
{
	data->m_Vehicle->ProcessInterpolation(data->m_FullUpdate, data->m_TimeStep);
}

////////////////////////////////////////////////////////////////////////////////

void RunAll()
{
	g_Running = true;
	{ PF_AUTO_PUSH_TIMEBAR("g_VehicleAvoidance.Run");
		g_VehicleAvoidance.Run();
	}
	{ PF_AUTO_PUSH_TIMEBAR("g_VehicleGoTo.Run");
		g_VehicleGoTo.Run();
	}
	{ PF_AUTO_PUSH_TIMEBAR("g_VehicleIntelligenceProcess.Run");
		g_VehicleIntelligenceProcess.Run();
	}
	{ PF_AUTO_PUSH_TIMEBAR("g_VehicleInterpolation.Run");
		g_VehicleInterpolation.Run();
	}

	g_Running = false;
}

}