//Game headers.
#include "Task\System\TaskTreeClone.h"

#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\QueriableInterface.h"
#include "Task\Combat\TaskDamageDeath.h"
#include "Task\General\TaskBasic.h"
#include "Task\System\TaskFSMClone.h"
#include "Network/NetworkInterface.h"
#include "fwnet/neteventmgr.h"

#if !__FINAL
#include "Network/Objects/Entities/NetObjPlayer.h"
#endif

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

CTaskTreeClone::CTaskTreeClone(fwEntity* pEntity, const s32 iNumberOfPriorities) :
CTaskTree(pEntity, iNumberOfPriorities)
, m_UpdatingTasks(false)
, m_OverridingNetworkAttachment(false)
, m_OverridingNetworkBlender(false)
, m_OverridingNetworkHeadingBlender(false)
, m_OverridingNetworkCollision(false)
, m_LocalHitReactionsAllowed(true)
, m_UseInAirBlender(false)
, m_PendingLocalCloneTask(0)
{
}

bool CTaskTreeClone::ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly)
{
	bool bProcessedPhysics = false;

	fwEntity* pEnt = m_pEntity;
	Assert(dynamic_cast<CEntity*>(pEnt));
	CPed* pPed = NULL;
	CPed* pClonePed = NULL;
	if(static_cast<CEntity*>(pEnt)->GetIsTypePed())
	{
		pPed = static_cast<CPed*>(pEnt);
		if(pPed->IsNetworkClone())
		{
			pClonePed = pPed;
		}
	}

	CTask* pTask = static_cast<CTask*>(GetActiveTask());
	while(pTask)
	{
        bool bTaskInScope = true;
		Assert(pEnt == pTask->GetEntity());
        if(pClonePed && pTask->IsClonedFSMTask())
        {
			CTaskFSMClone *pTaskFSMClone = static_cast<CTaskFSMClone *>(pTask);
			bTaskInScope = pTaskFSMClone->GetCachedIsInScope();
        }

		if( bTaskInScope && (!nmTasksOnly || pTask->IsNMBehaviourTask()))
		{
		    bProcessedPhysics = pTask->ProcessPhysics(fTimeStep, nTimeSlice);
		}

		pTask = pTask->GetSubTask();
	}

	return bProcessedPhysics;
}

void CTaskTreeClone::RecalculateTasks()
{
    CPed *pPed = SafeCast(CPed, m_pEntity);

    if(pPed)
    {
        CQueriableInterface *pQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();

        if(pQueriableInterface)
		{
            CTask *task = static_cast<CTask*>(m_Tasks[PED_TASK_PRIORITY_PRIMARY].Get());

            if(gnetVerifyf(task == 0 || task->IsClonedFSMTask(), "A non-cloned %s if the root task on the clone task tree!", task->GetTaskName()))
            {
				CTaskFSMClone *activeTask			= SafeCast(CTaskFSMClone, task);
				u32 activeTaskType					= activeTask ? static_cast<u32>(activeTask->GetTaskType()) : CTaskTypes::MAX_NUM_TASK_TYPES;
				bool activeTaskLocallyStarted		= activeTask ? activeTask->IsRunningLocally() : false;
				
				CTaskFSMClone *highestPriorityTask = 0;

                CTaskInfo *taskInfo = pQueriableInterface->GetFirstTaskInfo();

				bool bFoundDyingDeadTask = false;

                while(taskInfo)
                {
                    u32 taskType      = taskInfo->GetTaskType();
                    u32 netSequenceID = taskInfo->GetNetSequenceID();

					CClonedFSMTaskInfo *clonedTaskInfo = NULL;
					if(taskInfo->IsNetworked())
					{
						clonedTaskInfo = SafeCast(CClonedFSMTaskInfo, taskInfo);
					}

					bFoundDyingDeadTask = (taskType == CTaskTypes::TASK_DYING_DEAD);

                    if(!highestPriorityTask || bFoundDyingDeadTask)
                    {
						bool bKeepActiveTask = (activeTask && activeTaskType == taskType && (!bFoundDyingDeadTask || activeTaskType == CTaskTypes::TASK_NM_CONTROL));

						if (bKeepActiveTask)
						{
							if (activeTask->IsRunningLocally())
							{
								// set up the locally given clone task to be remotely controlled
								if (activeTask->HandleLocalToRemoteSwitch(pPed, clonedTaskInfo))
								{
									Assert(netSequenceID != INVALID_TASK_SEQUENCE_ID);

									activeTask->SetNetSequenceID(netSequenceID);
									activeTask->SetRunningLocally(false);

									// update the sequence info for this task type
									TaskSequenceInfo *sequenceInfo = pQueriableInterface->GetTaskSequenceInfo(activeTask->GetTaskType());

									if (!sequenceInfo)
									{
										sequenceInfo = pQueriableInterface->AddTaskSequenceInfo(activeTask->GetTaskType());
									}
									
									if (sequenceInfo)
									{
										sequenceInfo->m_sequenceID = netSequenceID;
									}
								}
								else if(!activeTask->CanRetainLocalTaskOnFailedSwitch())
								{
									bKeepActiveTask = false;
								}
							}
							else if (activeTask->GetNetSequenceID() != netSequenceID)
							{
								activeTask->SetFlag(aiTaskFlags::HandleCloneSwapToSameTaskType);
								bKeepActiveTask = false;
							}
						}

						// if we find a dying dead task we must apply it to avoid zombie peds
						if (bFoundDyingDeadTask)
						{
							if (activeTaskType == CTaskTypes::TASK_DYING_DEAD)
							{
								highestPriorityTask = activeTask;
							}
							else if (!activeTask || activeTask->CanBeReplacedByCloneTask(taskType))
                            {
								CTaskNMControl* pNMControlTask = NULL;

								if (activeTaskType == CTaskTypes::TASK_NM_CONTROL && activeTask && activeTask->GetSubTask() && activeTask->GetSubTask()->IsNMBehaviourTask())
								{
									CTaskNMBehaviour *pBehaviourTask = static_cast<CTaskNMBehaviour*>(activeTask->GetSubTask());

									pNMControlTask = rage_new CTaskNMControl(pBehaviourTask->GetMinTime(), pBehaviourTask->GetMaxTime(), pBehaviourTask->Copy(), CTaskNMControl::ALREADY_RUNNING);
								}

								if (highestPriorityTask)
							    {
								    if (activeTask == highestPriorityTask)
								    {
									    activeTask = NULL;
								    }

								    delete highestPriorityTask;

									highestPriorityTask = NULL;
							    }

							    highestPriorityTask = CreateCloneTaskFromTaskInfo(taskInfo, bFoundDyingDeadTask);
								Assert(highestPriorityTask);

								if (pNMControlTask)
								{
									if (highestPriorityTask && AssertVerify(highestPriorityTask->GetTaskType() == CTaskTypes::TASK_DYING_DEAD))
									{
										static_cast<CTaskDyingDead*>(highestPriorityTask)->SetForcedNaturalMotionTask(pNMControlTask);
									}
									else
									{
										delete pNMControlTask;
									}
								}

							    break;
                            }
						}
                        else if (bKeepActiveTask || (activeTask && (activeTask->GetPriority() <= taskInfo->GetTaskPriority())))
                        {
                            highestPriorityTask = activeTask;
                        }
                        else
                        {
                           if(!activeTask || activeTask->CanBeReplacedByCloneTask(taskType))
                           {
                               highestPriorityTask = CreateCloneTaskFromTaskInfo(taskInfo, activeTaskLocallyStarted);
                           }
                        }
                    }

                    CTask *currentTask = activeTask;

                    while(currentTask)
                    {
                        if(currentTask->IsClonedFSMTask())
                        {
						    CTaskFSMClone *taskFSMClone = SafeCast(CTaskFSMClone, currentTask);

							// the info is the same type, at the same priority level and is active.....
							if (static_cast<u32>(taskFSMClone->GetTaskType()) == taskType &&
								Verifyf(clonedTaskInfo, "Task info for %s is not networked. Have you forgotten to add this to CQueriableInterface::CreateEmptyTaskInfo?", TASKCLASSINFOMGR.GetTaskName(currentTask->GetTaskType())))
							{
								if (taskFSMClone->IsRunningLocally())
								{
									//! Always give a valid seq number anyway. This prevents us re-creating task if we decide we
									//! don't want to override.
									taskFSMClone->SetNetSequenceID(netSequenceID);

									TaskSequenceInfo *sequenceInfo = pQueriableInterface->GetTaskSequenceInfo(taskType);
									if (!sequenceInfo)
									{
										sequenceInfo = pQueriableInterface->AddTaskSequenceInfo(taskType);
									}

									if (sequenceInfo)
									{
										sequenceInfo->m_sequenceID = netSequenceID;
									}

									if(taskFSMClone->HandleLocalToRemoteSwitch(pPed, clonedTaskInfo))
									{
										taskFSMClone->SetRunningLocally(false);
									}
								}

								if(!taskFSMClone->IsRunningLocally())
								{
									// if we have a subtask running without a sequence (because it was started locally before it had an entry in the
									// queriable state), then assign its sequence here.
									if (taskFSMClone->GetNetSequenceID() == INVALID_TASK_SEQUENCE_ID)
									{
										taskFSMClone->SetNetSequenceID(netSequenceID);
									}

									if (taskFSMClone->GetNetSequenceID() == netSequenceID)
									{
										taskFSMClone->ReadQueriableState(clonedTaskInfo);
										break;
									}
								}
							}
                        }

                        currentTask = currentTask->GetSubTask();
                    }

                    taskInfo = taskInfo->GetNextTaskInfo();
                }

                if(highestPriorityTask && (highestPriorityTask != activeTask))
                {
                    bool replaceTask = true;

                    if(activeTask)
                    {
						// the dying dead task must always be applied, to avoid zombie peds
						if(activeTask->IgnoresCloneTaskPriorities() && highestPriorityTask->GetTaskType() != CTaskTypes::TASK_DYING_DEAD)
                        {
							if(highestPriorityTask->GetTaskType()==activeTask->GetTaskType() ) 
							{ 
								activeTask->SetNetSequenceID(highestPriorityTask->GetNetSequenceID());
							}

                            replaceTask = false;
						}
                        else if (!activeTaskLocallyStarted)
                        {
                            u32 taskType      = activeTask->GetTaskType();
#if __ASSERT
                            u32 netSequenceID = activeTask->GetNetSequenceID();
#endif // __ASSERT

                            if(taskType != static_cast<u32>(highestPriorityTask->GetTaskType()))
                            {
                                // decrement the sequence for the task type so the previously active task can run again
                                // when the higher priority task finishes
                                TaskSequenceInfo *sequenceInfo = pQueriableInterface->GetTaskSequenceInfo(taskType);

                                if(sequenceInfo)
                                {
									ASSERT_ONLY(gnetAssertf(sequenceInfo->m_sequenceID == netSequenceID, "Unexpected task sequence ID %d! (Expected: %d)", netSequenceID, sequenceInfo->m_sequenceID);)

									// handle wrapping of the task sequence id
                                    if(sequenceInfo->m_sequenceID == 0)
                                    {
										sequenceInfo->m_sequenceID = MAX_TASK_SEQUENCE_ID;
									}
									else
									{
                                        sequenceInfo->m_sequenceID--;
                                    }
                                 }
                            }
                        }
                    }

                    if(replaceTask)
                    {
						Assert(highestPriorityTask->GetNetSequenceID() != INVALID_TASK_SEQUENCE_ID);

						// we must force the ped out of ragdoll if the new task does not support it
						if (!highestPriorityTask->HandlesRagdoll(pPed) && pPed->GetUsingRagdoll())
						{
							nmEntityDebugf(pPed, "CTaskTreeClone::RecalculateTasks switching to animated");
							pPed->SwitchToAnimated();
						}

						SetTask(highestPriorityTask, PED_TASK_PRIORITY_PRIMARY, true);
                    }
					else
					{
						delete highestPriorityTask;
						highestPriorityTask = 0;
					}
                }
                else if (!activeTaskLocallyStarted)
                {
                    // now update any existing clone tasks that have finished (are no longer in the queriable interface)
                    CTask *currentTask = activeTask;

                    while(currentTask)
                    {
                        if(currentTask->IsClonedFSMTask())
                        {
                            CTaskFSMClone *taskFSMClone = SafeCast(CTaskFSMClone, currentTask);

                            if(!taskFSMClone->IsRunningLocally() && !pQueriableInterface->IsTaskCurrentlyRunning(taskFSMClone->GetTaskType(), false, taskFSMClone->GetNetSequenceID()))
                            {
                                taskFSMClone->OnCloneTaskNoLongerRunningOnOwner();
                            }
                        }

                        currentTask = currentTask->GetSubTask();
                    }
                }
			}
        }
    }
}

void CTaskTreeClone::UpdateTaskSpecificData()
{
	CPed *pPed = SafeCast(CPed, m_pEntity);

	if(pPed)
	{
		CQueriableInterface *pQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();

		if (pQueriableInterface)
		{
			CTask *task = static_cast<CTask*>(m_Tasks[PED_TASK_PRIORITY_PRIMARY].Get());

			if (gnetVerifyf(task == 0 || task->IsClonedFSMTask(), "A non-cloned %s if the root task on the clone task tree!", task->GetTaskName()))
			{
				while(task)
				{
					if(task->IsClonedFSMTask())
					{
						CTaskFSMClone *taskFSMClone = SafeCast(CTaskFSMClone, task);

						CTaskInfo *taskInfo = pQueriableInterface->GetFirstTaskInfo();

						while(taskInfo)
						{
							if(!taskFSMClone->IsRunningLocally() && 
								static_cast<s32>(taskFSMClone->GetTaskType()) == taskInfo->GetTaskType() && 
								taskFSMClone->GetNetSequenceID() == taskInfo->GetNetSequenceID())
							{
								CClonedFSMTaskInfo *clonedTaskInfo = SafeCast(CClonedFSMTaskInfo, taskInfo);
								taskFSMClone->ReadQueriableState(clonedTaskInfo);
								break;
							}

							taskInfo = taskInfo->GetNextTaskInfo();
						}
					}

					task = task->GetSubTask();
				}
			}
		}
	}
}

bool CTaskTreeClone::ControlPassingAllowed(const netPlayer& player, eMigrationType migrationType)
{
    bool allowControlPassing = true;

    for(s32 i = 0; i < m_Tasks.GetCount(); i++)
	{
        aiTask* pTask = m_Tasks[i];

        while(pTask)
        {
            if(((CTask*)pTask)->IsClonedFSMTask())
            {
                CPed *pPed = SafeCast(CPed, m_pEntity);
		        allowControlPassing &= static_cast<CTaskFSMClone *>(pTask)->ControlPassingAllowed(pPed, player, migrationType);

#if !__FINAL
				//Add more debug output in the network logs
				if (!allowControlPassing)
				{
					netLogSplitter log(NetworkInterface::GetEventManager().GetLog(), NetworkInterface::GetMessageLog());
					log.WriteDataValue("AllowControlPassing", "Rejected by CloneFsmTask name=\"%s\" in state=\"%s\""
																,static_cast<CTaskFSMClone *>(pTask)->GetTaskName()
																,static_cast<CTaskFSMClone *>(pTask)->GetStateName(static_cast<CTaskFSMClone *>(pTask)->GetState()));
				}
#endif // !__FINAL
            }

		    pTask = pTask->GetSubTask();
        }
    }

    return allowControlPassing;
}

CTaskFSMClone *CTaskTreeClone::CreateCloneTaskForTaskType(u32 taskType)
{
    CTaskFSMClone *cloneTask = 0;

    CPed *pPed = SafeCast(CPed, m_pEntity);

    if(AssertVerify(pPed))
    {
        CTaskInfo *taskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(taskType, PED_TASK_PRIORITY_MAX, false);

        if(taskInfo)
        {
            cloneTask = CreateCloneTaskFromTaskInfo(taskInfo);
        }
    }

    return cloneTask;
}

CTaskFSMClone *CTaskTreeClone::CreateCloneTaskFromInfo(CTaskInfo *taskInfo)
{
	CTaskFSMClone *cloneTask = taskInfo->CreateCloneFSMTask();

	if(cloneTask)
	{
		cloneTask->SetRunAsAClone(true);
		Assert(dynamic_cast<CClonedFSMTaskInfo *>(taskInfo));
		cloneTask->ReadQueriableState(static_cast<CClonedFSMTaskInfo *>(taskInfo));
		Assert(cloneTask->GetNetSequenceID() != INVALID_TASK_SEQUENCE_ID);
	}

	return cloneTask;
}

void CTaskTreeClone::StartLocalCloneTask(CTaskFSMClone* pTask, u32 const iPriority)
{
	if (aiVerify(pTask))
	{
        if(m_UpdatingTasks)
        {
            if(m_PendingLocalCloneTask)
            {
                delete m_PendingLocalCloneTask;
            }

            m_PendingLocalCloneTask = pTask;
        }
        else
        {
		    CTask *activeTask = static_cast<CTask*>(m_Tasks[PED_TASK_PRIORITY_PRIMARY].Get());
			CPed *pPed = SafeCast(CPed, m_pEntity);

		    if (activeTask)
			{
				if (activeTask->GetTaskType() == pTask->GetTaskType())
				{
					activeTask->SetFlag(aiTaskFlags::HandleCloneSwapToSameTaskType);
				}
				else
				{
					CQueriableInterface *pQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();

					CTaskInfo *taskInfo = pQueriableInterface->GetFirstTaskInfo();

					// decrement the sequence for the all task types currently in the queriable interface so that the corresponding tasks can run again
					// when the local clone task finishes
					while(taskInfo)
					{
						TaskSequenceInfo *sequenceInfo = pQueriableInterface->GetTaskSequenceInfo(taskInfo->GetTaskType());

						if(sequenceInfo)
						{
							// handle wrapping of the task sequence id
							if(sequenceInfo->m_sequenceID == 0)
							{
								sequenceInfo->m_sequenceID = MAX_TASK_SEQUENCE_ID;
							}
							else
							{
								sequenceInfo->m_sequenceID--;
							}
						}	

						taskInfo = taskInfo->GetNextTaskInfo();
					}
				}
		    }
    	
		    pTask->SetRunningLocally(true);
		    pTask->SetRunAsAClone(true);
			
			pTask->SetPriority(iPriority);

#if ENABLE_NETWORK_LOGGING
			if (pPed->GetNetworkObject())
			{
				netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
				NetworkLogUtils::WriteLogEvent(log, "START_LOCAL_CLONE_TASK", pPed->GetNetworkObject()->GetLogName());
				log.WriteDataValue("Task", pTask->GetTaskName());
			}
#endif // ENABLE_NETWORK_LOGGING

		    SetTask(pTask, PED_TASK_PRIORITY_PRIMARY, true);
        }
	}
}

CTask* CTaskTreeClone::GetActiveCloneTask()
{
	return static_cast<CTask*>(m_Tasks[PED_TASK_PRIORITY_PRIMARY].Get());
}

void CTaskTreeClone::UpdateTasks(const bool bDontDeleteHeadTask, const s32 iDefaultTaskPriority, float fTimeStep)
{
    m_OverridingNetworkAttachment		= false;
	m_OverridingNetworkBlender			= false;
	m_OverridingNetworkHeadingBlender   = false;
    m_OverridingNetworkCollision		= false;
    m_LocalHitReactionsAllowed          = true;
    m_UseInAirBlender                   = false;

	// we have to do this before setting m_UpdatingTasks to true
	if(m_PendingLocalCloneTask)
	{
		StartLocalCloneTask(m_PendingLocalCloneTask);
		m_PendingLocalCloneTask = 0;
	}

	m_UpdatingTasks = true;

    bool taskExistedBeforeUpdate = m_Tasks[PED_TASK_PRIORITY_PRIMARY] != 0;

    CTaskTree::UpdateTasks(bDontDeleteHeadTask, iDefaultTaskPriority, fTimeStep);

    // if we have finished the main task we need to recalculate the tasks now to
    // run any new clone tasks that were waiting for this task to finish
    if(taskExistedBeforeUpdate && (m_Tasks[PED_TASK_PRIORITY_PRIMARY] == 0))
    {
        RecalculateTasks();

		// update this task immediately so that the new task takes control of the ped immediately, otherwise the frame delay can cause problems
		aiTask* pNewTask = GetTask(PED_TASK_PRIORITY_PRIMARY);

		if (pNewTask)
		{
			aiTask::BeginTaskUpdates(fTimeStep);
			UpdateTask(pNewTask, fTimeStep);
			aiTask::EndTaskUpdates();
		}
    }

    m_UpdatingTasks = false;

  // ensure the ped has a default task
    CPed *pPed = SafeCast(CPed, m_pEntity);
    if(pPed && !pPed->GetPedIntelligence()->GetTaskDefault())
    {
        pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskNetworkClone());
    }
}

aiTaskTree::UpdateResult CTaskTreeClone::UpdateTask(aiTask* pTask, float fTimeStep)
{
    aiTaskTree::UpdateResult result = aiTaskTree::UR_QUIT;

    if( ((CTask*)pTask)->IsClonedFSMTask() )
    {
        CTaskFSMClone *taskFSMClone = static_cast<CTaskFSMClone*>(pTask);

        CPed *pPed = SafeCast(CPed, m_pEntity);

		Assert(pPed == taskFSMClone->GetPed());
        if(taskFSMClone->UpdateCachedIsInScope())
        {
	        result = CTaskTree::UpdateTask(pTask, fTimeStep);
        }
        else
        {
            if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(taskFSMClone->GetTaskType()))
            {
                result = aiTaskTree::UR_CONTINUED;
            }
            else
            {
#if ENABLE_NETWORK_LOGGING
                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "QUIT_OUT_OF_SCOPE_CLONE_TASK", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "Unknown");

                if(taskVerifyf(static_cast<const char *>(taskFSMClone->GetName()), "A task is returning NULL from GetName()!"))
                {
                    NetworkInterface::GetObjectManagerLog().WriteDataValue("Task Type", taskFSMClone->GetName());
                }
#endif // ENABLE_NETWORK_LOGGING

                result = aiTaskTree::UR_QUIT;
            }
        }

        m_OverridingNetworkAttachment		|= taskFSMClone->OverridesNetworkAttachment(pPed);
        m_OverridingNetworkCollision		|= taskFSMClone->OverridesCollision();
        m_LocalHitReactionsAllowed          &= taskFSMClone->AllowsLocalHitReactions();
        m_UseInAirBlender                   |= taskFSMClone->UseInAirBlender();

        if(taskFSMClone->IsInScope(pPed))
        {
            m_OverridingNetworkBlender        |= taskFSMClone->OverridesNetworkBlender(pPed);
		    m_OverridingNetworkHeadingBlender |= taskFSMClone->OverridesNetworkHeadingBlender(pPed);
        }
    }
    else
    {
        result = CTaskTree::UpdateTask(pTask, fTimeStep);
    }

    return result;
}

void CTaskTreeClone::OnNewSubTaskCreated(aiTask *pTask, aiTask* pSubTask)
{
    CPed *pPed = SafeCast(CPed, m_pEntity);

    // we need to update the new subtask with the latest queriable state data
    if(pSubTask && ((CTask*)pSubTask)->IsClonedFSMTask())
    {
        // not sure if the call to IsTaskCurrentlyRunning is necessary...
        if (pPed && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(pSubTask->GetTaskType()))
        {
            CTaskInfo *taskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetFirstTaskInfo();

            while(taskInfo)
            {
                if(taskInfo->IsNetworked() && pSubTask->GetTaskType() == taskInfo->GetTaskType() && 
                    static_cast<CTaskFSMClone*>(pTask)->GetPriority() == taskInfo->GetTaskPriority())
                {
                    CClonedFSMTaskInfo *clonedTaskInfo = SafeCast(CClonedFSMTaskInfo, taskInfo);
                    static_cast<CTaskFSMClone*>(pSubTask)->ReadQueriableState(clonedTaskInfo);
                    break;
                }

                taskInfo = taskInfo->GetNextTaskInfo();
            }
        }
    }
}

CTaskFSMClone *CTaskTreeClone::CreateCloneTaskFromTaskInfo(CTaskInfo* pTaskInfo, bool bIgnoreSequences)
{
    CTaskFSMClone* pCloneTask = 0;

    CPed *pPed = SafeCast(CPed, m_pEntity);

    if(AssertVerify(pPed) && AssertVerify(pTaskInfo))
    {
        u32 taskType = pTaskInfo->GetTaskType();

        CQueriableInterface *pQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();

        if(AssertVerify(pQueriableInterface))
		{
            TaskSequenceInfo *sequenceInfo = pQueriableInterface->GetTaskSequenceInfo(taskType);

            if((sequenceInfo == 0) || (sequenceInfo->m_sequenceID != pTaskInfo->GetNetSequenceID() || bIgnoreSequences))
            {
		        if (pTaskInfo->AutoCreateCloneTask(pPed))
		        {
			        pCloneTask = CreateCloneTaskFromInfo(pTaskInfo);
		        }

                if(pCloneTask)
                {
                    if(sequenceInfo == 0)
                    {
                        // add a new sequence info structure for this task type
                        sequenceInfo = pQueriableInterface->AddTaskSequenceInfo(taskType);
                    }

                    if(sequenceInfo)
                    {
                        sequenceInfo->m_sequenceID = pTaskInfo->GetNetSequenceID();
                    }
                }
            }
        }
    }

    return pCloneTask;
}
