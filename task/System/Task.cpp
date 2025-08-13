//
// task/task.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/System/Task.h"

#include "fwanimation/clipsets.h"
#include "ai/debugstatehistory.h"
#include "system/threadtype.h"

// Game headers
#include "Event/Events.h"
#include "Objects/ObjectIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedTaskRecord.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task/Combat/Subtasks/TaskReactToBuddyShot.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskWander.h"
#include "Task/Helpers/TaskConversationHelper.h"
#include "Task/Motion/Locomotion/TaskBirdLocomotion.h"
#include "Task/Motion/TaskMotionBase.h"
#include "task/Movement/TaskGetUp.h"
#include "Task/Physics/GrabHelper.h"
#include "task/Response/TaskReactAndFlee.h"
#include "task/Response/TaskReactToImminentExplosion.h"
#include "Task/Response/TaskShockingEvents.h"
#include "task/Scenario/Types/TaskCowerScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/TaskClassInfo.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "VehicleAi/task/TaskVehicleCruise.h"

#if __BANK
#include "Debug/DebugScene.h"
#endif 

AI_OPTIMISATIONS()

// Static initialization
INSTANTIATE_POOL_ALLOCATOR(CTask);

#if DEBUG_DRAW
#define MAX_TASK_DRAWABLES 100
CDebugDrawStore CTask::ms_debugDraw(MAX_TASK_DRAWABLES);
#endif // DEBUG_DRAW

u8 CTask::sm_ProcessPhysicsNumSteps = 0;

CTask::CTask()
{
#if !TASK_USE_THREADSAFE_POOL_ALLOCATOR
	// This must be called from MainThread
	Assert(sysThreadType::GetCurrentThreadType() == sysThreadType::THREAD_TYPE_UPDATE);
#endif // !TASK_USE_THREADSAFE_POOL_ALLOCATOR

	m_clipHelper.SetAssociatedTask(this);
	SetInternalTaskType(CTaskTypes::TASK_NONE);
}

CTask::~CTask()
{
#if !TASK_USE_THREADSAFE_POOL_ALLOCATOR
	// This must be called from MainThread
	Assert(sysThreadType::GetCurrentThreadType() == sysThreadType::THREAD_TYPE_UPDATE);
#endif // !TASK_USE_THREADSAFE_POOL_ALLOCATOR

	StopClip();
}

#if !__NO_OUTPUT
void CTask::Debug() const
{
	aiTask::Debug();
}

atString CTask::GetName() const
{
	const char* pStr = TASKCLASSINFOMGR.GetTaskName(GetTaskType());
	if(pStr)
	{
		atString name(pStr);
		name+=":";
		name+=(GetStateName(GetState()));

		if (GetMoveNetworkHelper() && GetMoveNetworkHelper()->IsNetworkActive())
		{
			fwMvClipSetId clipSetId = GetMoveNetworkHelper()->GetClipSetId();
			if (clipSetId != CLIP_SET_ID_INVALID)
			{
				name+="::Clipset:";
				name+=clipSetId.TryGetCStr();
			}
		}
		return name;
	}
	else
	{
		return atString("Unnamed_task");
	}
}
#endif // !__NO_OUTPUT

void CTask::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		ResetStatics();
	}
	else
	{
		ResetStatics();
		CAmbientClipStreamingManager::GetInstance()->Init();
		CClimbHandHoldDetected::InitPool(MEMBUCKET_GAMEPLAY);
		CGrabHelper::InitPool(MEMBUCKET_GAMEPLAY);
		CPathNodeRouteSearchHelper::InitPool(MEMBUCKET_GAMEPLAY);
		CVehicleClipRequestHelper::InitPool(MEMBUCKET_GAMEPLAY);
		CAmbientLookAt::InitPool(MEMBUCKET_GAMEPLAY);
		CTaskConversationHelper::InitPool(MEMBUCKET_GAMEPLAY);
		CPropManagementHelper::InitPool(MEMBUCKET_GAMEPLAY);
	}
}

void CTask::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	CTaskConversationHelper::ShutdownPool();
	CAmbientLookAt::ShutdownPool();
	CVehicleClipRequestHelper::ShutdownPool();
	CPathNodeRouteSearchHelper::ShutdownPool();
	CGrabHelper::ShutdownPool();
	CClimbHandHoldDetected::ShutdownPool();
	CTaskMeleeActionResult::Shutdown();
	CPropManagementHelper::ShutdownPool();
	ResetStatics();
}

void CTask::StaticUpdate()
{
	CAmbientClipStreamingManager::GetInstance()->Update();
}

void CTask::ResetStatics()
{
	CTaskBirdLocomotion::sm_TimeForNextTakeoff = 0;
	CTaskCombatAdditionalTask::ms_iTimeOfLastOrderClip = 0;
	CTaskUseScenario::ResetStatics();
	CTaskVehicleFlee::ResetStatics();
	CTaskVehicleFSM::TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED = 0;
	CTaskWander::ResetStatics();
#if 0	// SHOCKING_EVENTS_IN_GLOBAL_LIST TODO: Reimplement if we end up using CTaskShockingEventFlee.
	CTaskShockingEventFlee::ResetStatics();
#endif
	CTaskReactAndFlee::ResetLastTimeReactedInDirection();
	CTaskReactToImminentExplosion::ResetLastTimeEscapeUsedOnScreen();
	CTaskCowerScenario::ResetLastTimeAnyoneFlinched();
	CTaskReactToBuddyShot::ResetLastTimeUsedOnScreen();
	CTaskVehicleCombat::ResetNextShootOutTireTimeGlobal();
	CTaskCombat::ResetTimeLastDragWasActive();
	CTaskCombat::ResetTimeLastAsyncProbeSubmitted();
	CTaskGetUp::ResetLastAimFromGroundStartTime();
	CTaskGun::ResetLastTimeOverheadUsed();
	CTaskShockingEventWatch::ResetStatics();
}

bool CTask::IsTaskPtr(void* pData)
{
	if(pData == NULL)
		return true;
	return GetPool()->IsValidPtr((CTask*)pData);
}

void CTask::RequestProcessMoveSignalCalls()
{
	// We use a reset flag to keep track of this stuff now. We could potentially use different flags
	// for different task trees, or set some flag in the task trees themselves, but not sure if it's worth it.
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_TasksNeedProcessMoveSignalCalls, true);
}

void CTask::PropagateNewMoveSpeed(const float fMBR)
{
	CTask* pTask = this;
	do
	{
		taskAssert(pTask->IsMoveTask());

		CTaskMoveInterface* pInterface = pTask->GetMoveInterface();

		pInterface->SetMoveBlendRatio(fMBR, false);
		pInterface->SetHasNewMoveSpeed(false);

		// Continue down to the subtasks, without recursing.
		pTask = pTask->GetSubTask();
	} while(pTask);
}

bool CTask::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid;

	if (task.IsOnFoot())
	{
		isValid = true;
	}
	else if( task.IsInWater() )
	{
		const CPed* pPed = GetPed();
		isValid = pPed && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater );
	}
	else
	{
		isValid = false;
	}

	return isValid;
}

bool CTask::RecordDamage(CDamageInfo& damageInfo)
{
	if(GetSubTask())
	{
		return GetSubTask()->RecordDamage(damageInfo);
	}
	return false;
}

CTaskInfo* CTask::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

void CTask::CleanUp()
{
	StopClip();
	aiTask::CleanUp();
}

void CTask::FSM_SetAnimationFlags()
{
	// If the clip has terminated, set the clip finished flag
	if( GetClipHelperStatus() == CClipHelper::Status_Terminated )
	{
		m_Flags.SetFlag(aiTaskFlags::AnimFinished);
		ResetClipHelperStatus();
	}
}

void CTask::FSM_AbortPreviousTask()
{
	if( GetSubTask() )
	{
		//Check if the new task is a ragdoll task.
		bool bNewTaskIsRagdollTask = CTaskTypes::IsRagdollTask(GetNewTask()->GetTaskType());
		if(!bNewTaskIsRagdollTask)
		{
			//Check if the entity is a ped.
			if(GetEntity() && (GetEntity()->GetType() == ENTITY_TYPE_PED))
			{
				//The static_cast here is somewhat questionable, but I believe all the tasks in use are currently CTask.
				//Also considered adding an empty HandlesRagdoll definition to aiTask, but CPed is not available that low.
				bNewTaskIsRagdollTask = static_cast<CTask *>(GetNewTask())->HandlesRagdoll(GetPed());
			}
		}

		CEventNewTask eventNewTask(GetNewTask(), E_PRIORITY_NEW_TASK, bNewTaskIsRagdollTask);
		if( !GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, &eventNewTask) )
		{
			GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, &eventNewTask);
		}
	}
}



#if !__FINAL

bool CTask::VerifyTaskCountCallback( void* pItem, void * )
{
	// Are we allocated out?
	if ( pItem != NULL )
	{
 		// No entity and no parent?
// 		if (!pTask->GetEntity() && !pTask->GetParent() && pTask->IsDeadForNFrames(7200) )
// 		{
// 			pTask->FailIntegrity( "Task has no entity owner or parent and hasn't been ticked for a long time.  "
// 				"Is it allocated outside the task tree.  See TTY." );
// 		}
// 		else
		{
#if __ASSERT
			CTask* pTask = static_cast<CTask*>(pItem);
			Assertf( pTask->IsReferenced(), "(0x%p) Task %s \"%s\" has been orphaned.\n", pTask, pTask->GetTaskName(), pTask->GetName().c_str() );
#endif
		}
	}

	// Go through them all
	return true;
}

void CTask::VerifyTaskCounts()
{
	Pool *pool = GetPool();
	if ( pool )
	{
		pool->ForAll( &VerifyTaskCountCallback, NULL );
	}

}

#endif // !__FINAL


#if TASK_DEBUG_ENABLED
void CTask::PoolFullCallback(void* item)
{
	CTask* pTask = static_cast<CTask*>(item);

	static const s32 MAX_STRING_SIZE = 64;
	char buff[MAX_STRING_SIZE];
	s32 iTreeIndex = -1;
	s32 iPriorityIndex = -1;
	s32 iDepthIndex = 0;

	if(pTask && pTask->GetEntity())
	{
		CTaskManager* pTaskManager = NULL;

		switch(pTask->GetEntity()->GetType())
		{
		case ENTITY_TYPE_PED:
			formatf(buff, MAX_STRING_SIZE, "Ped: 0x%X", pTask->GetEntity());
			pTaskManager = static_cast<CPed*>(pTask->GetEntity())->GetPedIntelligence() ? static_cast<CPed*>(pTask->GetEntity())->GetPedIntelligence()->GetTaskManager() : NULL;
			break;
		case ENTITY_TYPE_VEHICLE:
			formatf(buff, MAX_STRING_SIZE, "Vehicle: 0x%X", pTask->GetEntity());
			pTaskManager = static_cast<CVehicle*>(pTask->GetEntity())->GetIntelligence() ? static_cast<CVehicle*>(pTask->GetEntity())->GetIntelligence()->GetTaskManager() : NULL;
			break;
		case ENTITY_TYPE_OBJECT:
			formatf(buff, MAX_STRING_SIZE, "Object: 0x%X", pTask->GetEntity());
			pTaskManager = static_cast<CObject*>(pTask->GetEntity())->GetObjectIntelligence() ? static_cast<CObject*>(pTask->GetEntity())->GetObjectIntelligence()->GetTaskManager() : NULL;
			break;
		default:
			formatf(buff, MAX_STRING_SIZE, "Unknown: 0x%X, Type: %d", pTask->GetEntity(), pTask->GetEntity()->GetType());
			break;
		}

		if(pTaskManager)
		{
			for(s32 i = 0; i < pTaskManager->GetTreeCount() && iTreeIndex == -1; i++)
			{
				aiTaskTree* pTree = pTaskManager->GetTree(i);
				if(pTree)
				{
					iDepthIndex = 0;

					for(s32 j = 0; j < pTree->GetPriorityCount() && iPriorityIndex == -1; j++)
					{
						aiTask* pSubTask = pTree->GetTask(j);
						while(pSubTask)
						{
							if(pSubTask == pTask)
							{
								iTreeIndex = i;
								iPriorityIndex = j;
								break;
							}
							++iDepthIndex;
							pSubTask = pSubTask->GetSubTask();
						}
					}
				}
			}
			taskAssert(iTreeIndex != -1 && iPriorityIndex != -1);
		}
	}
	else
	{
		formatf(buff, MAX_STRING_SIZE, "Entity: NULL");
	}

	if(pTask)
		taskDisplayf("0x%p, [%s], Tree: %d, Priority: %d, Depth: %d, RunTime: %.2f, %s, PopType: %d", pTask, pTask->GetName().c_str(), iTreeIndex, iPriorityIndex, iDepthIndex, pTask->GetTimeRunning(), buff, pTask->GetEntity() ? static_cast<CDynamicEntity*>(pTask->GetEntity())->PopTypeGet() : 0);
	else
		taskDisplayf("0x%p, [%s], Tree: %d, Priority: %d, Depth: %d, RunTime: %.2f, %s, PopType: %d", (void*)NULL, "NULL", iTreeIndex, iPriorityIndex, iDepthIndex, 0.0f, buff, 0);
}
#endif // TASK_DEBUG_ENABLED
