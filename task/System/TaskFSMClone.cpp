//
// filename:	TaskClonedFSM.h
// description:	Base state machine class, provides support for writing/reading network sync data.
//				Also provides an interface to be run on a clone ped on another machine as a clone task
//				visually simulating the peds AI to make it look as if it were running locally.
//

#include "ai\debugstatehistory.h"

#include "Task\System\TaskFSMClone.h"

#include "Camera\CamInterface.h"
#include "Event\Events.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Renderer\Renderer.h"
#include "scene/lod/LodScale.h"

AI_OPTIMISATIONS()


CTaskFSMClone::CTaskFSMClone()
: m_netSequenceID(INVALID_TASK_SEQUENCE_ID)
, m_iTaskPriority(PED_TASK_PRIORITY_DEFAULT)
, m_iStateFromNetwork(-1)
, m_bRunAsAClone(false)
, m_cloneTaskInScope(true)
, m_bCachedIsInScope(false)
, m_bRunningLocally(false)
, m_bWasLocallyStarted(false)
, m_bIsMigrating(false)
, m_bCloneDictRefAdded(false)
, m_scopeModifier(1.0f)
{
}

CTaskFSMClone::~CTaskFSMClone()
{
}

void CTaskFSMClone::SetNetSequenceID(u32 netSequenceID) 
{ 
    Assertf(!NetworkInterface::IsGameInProgress() || netSequenceID != INVALID_TASK_SEQUENCE_ID, "%s is being given an invalid sequence id. IsRunningLocally %s, WasLocallyStarted %s ", GetTaskName(),
		IsRunningLocally()?"yes":"no",WasLocallyStarted()?"yes":"no"); 
	m_netSequenceID = netSequenceID; 
}

void CTaskFSMClone::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
    m_netSequenceID     = pTaskInfo->GetNetSequenceID();
    m_iTaskPriority     = pTaskInfo->GetTaskPriority();
	m_iStateFromNetwork = pTaskInfo->GetState();

	Assert(m_netSequenceID != INVALID_TASK_SEQUENCE_ID);
}

void CTaskFSMClone::OnCloneTaskNoLongerRunningOnOwner() 
{ 
#if __ASSERT
	if (!m_bRunAsAClone)
	{
		CTask* pTask = GetParent();
		CTask* pRootParent = pTask;

		while (pTask)
		{
			if(pTask->IsClonedFSMTask())
			{
				CTaskFSMClone *pTaskFSMClone = static_cast<CTaskFSMClone *>(pTask);

				if (pTaskFSMClone->GetRunAsAClone())
				{
					return;
				}
			}

			pRootParent = pTask;

			pTask = pTask->GetParent();
		}

		if (!pRootParent)
		{
			Assertf(0, "%s (%s) : OnCloneTaskNoLongerRunningOnOwner() is being called on this task, which is not running as a clone task. Ped: %s, Parent task: -none-", GetTaskName(), m_bRunningLocally ? "LOCAL" : "NOT LOCAL", GetPed() ? GetPed()->GetNetworkObject()->GetLogName() : "??"); 
		}
		else
		{
			Assertf(0, "%s (%s) : OnCloneTaskNoLongerRunningOnOwner() is being called on this task, which is not running as a clone task. Ped: %s, Parent task: %s (%s)", GetTaskName(), m_bRunningLocally ? "LOCAL" : "NOT LOCAL", GetPed() ? GetPed()->GetNetworkObject()->GetLogName() : "??", pRootParent->GetTaskName(), static_cast<CTaskFSMClone*>(pRootParent)->IsRunningLocally() ? "LOCAL" : "NOT LOCAL"); 
		}
	}
#endif
}

bool CTaskFSMClone::IsInScope(const CPed *ped)
{
    Assert(ped);

    if(ped)
    {
#if __BANK
        bool forceOutOfScope = NetworkInterface::ShouldForceCloneTasksOutOfScope(ped);
#endif // __BANK

        Vector3 delta = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()) - camInterface::GetPos();

		float enterScopeDist = GetScopeDistance();
		float leaveScopeDist = enterScopeDist * 1.2f;

		bool inDifferentTutorials = false;
		
		if (AssertVerify(ped->GetNetworkObject()) && ped->GetNetworkObject()->GetPlayerOwner())
		{
			inDifferentTutorials  = NetworkInterface::ArePlayersInDifferentTutorialSessions(*NetworkInterface::GetLocalPlayer(), *ped->GetNetworkObject()->GetPlayerOwner());
            inDifferentTutorials |= SafeCast(CNetObjPed, ped->GetNetworkObject())->IsConcealed();
		}

        if(m_cloneTaskInScope)
        {
            const float adjustedScopeDistance = leaveScopeDist * g_LodScale.GetGlobalScale() * m_scopeModifier * GetScopeDistanceMultiplierOverride();

            if(inDifferentTutorials || delta.XYMag2() > rage::square(adjustedScopeDistance) BANK_ONLY(|| forceOutOfScope))
            {
                LeaveScope(ped);
                m_cloneTaskInScope = false;
            }
        }
        else
        {
            const float adjustedScopeDistance = enterScopeDist * g_LodScale.GetGlobalScale()* m_scopeModifier;

            if(!inDifferentTutorials && delta.XYMag2() <= rage::square(adjustedScopeDistance) BANK_ONLY(&& !forceOutOfScope))
            {
                EnterScope(ped);
                m_cloneTaskInScope = true;
            }
        }
    }   

    return m_cloneTaskInScope;
}

void CTaskFSMClone::EnterScope(const CPed *BANK_ONLY(pPed))
{
#if __BANK
    if(NetworkInterface::IsNetworkOpen() && pPed && pPed->IsNetworkClone())
    {
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "CLONE_TASK_INTO_SCOPE", pPed->GetNetworkObject()->GetLogName());

        if(taskVerifyf(static_cast<const char *>(GetName()), "A task is returning NULL from GetName()!"))
        {
            NetworkInterface::GetObjectManagerLog().WriteDataValue("Task Type", GetName());
        }
    }
#endif // __BANK
}

void CTaskFSMClone::LeaveScope(const CPed *BANK_ONLY(pPed))
{
#if __BANK
    if(NetworkInterface::IsNetworkOpen() && pPed && pPed->IsNetworkClone())
    {
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "CLONE_TASK_OUT_OF_SCOPE", pPed->GetNetworkObject()->GetLogName());

        if(taskVerifyf(static_cast<const char *>(GetName()), "A task is returning NULL from GetName()!"))
        {
            NetworkInterface::GetObjectManagerLog().WriteDataValue("Task Type", GetName());
        }
    }
#endif // __BANK
}

float CTaskFSMClone::GetScopeDistance() const
{
	static const float CLONE_TASK_ENTER_SCOPE_DISTANCE = 100.0f;

	return CLONE_TASK_ENTER_SCOPE_DISTANCE;
}

bool CTaskFSMClone::MakeAbortable(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if( m_bRunAsAClone )
	{
		if( GetSubTask() == NULL || GetSubTask()->MakeAbortable(iPriority, pEvent) )
		{
			m_Flags.SetFlag(aiTaskFlags::InMakeAbortable);
			UpdateClonedFSM( GetState(), OnExit );
			SetFlag(aiTaskFlags::QuitOnResume);
			DoAbort(iPriority, pEvent);
			CleanUp();
			StopClip();
			m_Flags.SetFlag(aiTaskFlags::IsAborted);
			return true;
		}
	}
	else
	{
		return CTask::MakeAbortable(iPriority, pEvent);
	}
	return false;
}

aiTask::FSM_Return CTaskFSMClone::ProcessFSM( const s32 iState, const FSM_Event iEvent )
{
	taskFatalAssertf(GetEntity(), "Task not associated with an entity");
	taskAssertf(dynamic_cast<const CDynamicEntity*>(GetEntity()), "Only dynamic entities can run tasks");
	const CDynamicEntity* pEntity = static_cast<const CDynamicEntity*>(GetEntity());
	if( pEntity->IsNetworkClone() && IsClonedFSMTask() )
	{
		return UpdateClonedFSM(iState, iEvent);
	}
	else
	{
		return UpdateFSM(iState, iEvent);
	}
}

void CTaskFSMClone::UpdateClonedSubTasks(CPed* UNUSED_PARAM(pPed), int const UNUSED_PARAM(iState))
{
	// Do nothing - just allow us to be *optionally* overridden
}

bool CTaskFSMClone::CreateSubTaskFromClone(CPed* pPed, CTaskTypes::eTaskType const expectedTaskType, bool bAssertIfTaskInfoNotFound)
{
	Assert(pPed);

	if(pPed)
	{
		if( expectedTaskType != CTaskTypes::TASK_INVALID_ID)
		{
			// is the clone already using the task itself - don't have to bother with this....
			CTask* cloneTask = pPed->GetPedIntelligence()->FindTaskByType(expectedTaskType);

			if(!cloneTask)
			{
				CQueriableInterface* queriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
				Assert(queriableInterface);

				if(queriableInterface)
				{
					Assert(queriableInterface->IsTaskSentOverNetwork(expectedTaskType));
					Assert(GetTaskType() != CTaskTypes::TASK_INVALID_ID);

					CTaskInfo* parentInfo	= queriableInterface->GetTaskInfoForTaskType(GetTaskType(), PED_TASK_PRIORITY_MAX, bAssertIfTaskInfoNotFound);
					Assert(parentInfo);

					// Don't assert here as there is often a frame delay 
					// between task state changing and task adding subtask (with the QI
					// being rebuilt between the two) and it's harmless to skip it.
					// Everything gets resolved correctly next frame / update...
					CTaskInfo* expectedInfo = queriableInterface->GetTaskInfoForTaskType(expectedTaskType, PED_TASK_PRIORITY_MAX, false);

					if(parentInfo && expectedInfo)
					{
						if(expectedInfo->GetTaskPriority() == parentInfo->GetTaskPriority())
						{
						//	if(expectedInfo->GetTaskTreeDepth() == parentInfo->GetTaskTreeDepth())
							{
								TaskSequenceInfo const* sequenceInfo = queriableInterface->GetTaskSequenceInfo(expectedTaskType);

								// we've launched the task the required amount of times so shouldn't be launching it again....
								// even at this late stage it's acceptable to fail - The task heirarchy and task sequence info data are not
								// updated in sync so the clone can be trying to copy the updated task heirarchy but the stale TaskInfo sequence ID
								// and local sequence info ID's can be equal meaning the task can't be created - this is resolved in 
								// subsequent frames when the owner ped updates it's TaskInfo sequence values and sends them across....
								if(sequenceInfo && (sequenceInfo->m_sequenceID == expectedInfo->GetNetSequenceID()))
								{
									return false;
								}

								// the subtask is part of the ped's QI but not in the task tree so we create it....
								CTask *cloneSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(expectedTaskType);
								Assertf(cloneSubTask,"%s has Net Object %s. Failed to create task type name %s, type num %d. This task type %d",
									pPed->GetDebugName(),
									pPed->GetNetworkObject()?"Valid Net Object":"NUll Net Object",
									TASKCLASSINFOMGR.GetTaskName(expectedTaskType),
									expectedTaskType,
									GetTaskType());

								if(cloneSubTask)
								{
									SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
									SetNewTask(cloneSubTask);

									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}

//A helper function that will check if the passed dict hash is load and request it if not and manage the m_bCloneDictRefAdded accordingly
bool CTaskFSMClone::CheckAndRequestSyncDictionary(const u32 *arrayOfHashes, u32 numHashes)
{
	const int MAX_CLIP_DICT_HASHES = 3; // set the same as CTaskScriptedAnimation::MAX_CLIPS_PER_PRIORITY

	if (!m_bCloneDictRefAdded && 
		taskVerifyf(arrayOfHashes,"null arrayOfHashes") && 
		taskVerifyf(numHashes <=MAX_CLIP_DICT_HASHES,"Too many dictionary hashes %d, Don't expect more than CTaskScriptedAnimation::MAX_CLIPS_PER_PRIORITY",numHashes) )
	{
		int clipDictIndex[MAX_CLIP_DICT_HASHES];

		bool bAllStreamed = true;

		for(int i=0; i<numHashes; i++)
		{
			if(IsDictStreamedReady(arrayOfHashes[i], clipDictIndex[i])==false)
			{
				bAllStreamed = false;
			}
		}

		if(!bAllStreamed)
		{
			return false; //not all ready yet
		}

		//All valid hashes in array must be all ready and streamed so ref the valid ones
		for(int i=0; i<numHashes; i++)
		{
			if(clipDictIndex[i]!=-1)
			{
				fwAnimManager::AddRef(strLocalIndex(clipDictIndex[i]));
			}
		}
		m_bCloneDictRefAdded = true;
	}

	return true;
}

bool CTaskFSMClone::IsDictStreamedReady(u32 dictHash, int& outClipDictIndex)
{
	outClipDictIndex = -1;
	if (dictHash)
	{
		outClipDictIndex = fwAnimManager::FindSlotFromHashKey(dictHash).Get();
		if(taskVerifyf(outClipDictIndex!=-1,"Unrecognized anim dictionary %u",dictHash))
		{
			fwClipDictionaryLoader loader(outClipDictIndex, false);

			// Get clip dictionary
			crClipDictionary *clipDictionary = loader.GetDictionary();

			if(clipDictionary==NULL)			
			{	//if not loaded put in a request for it
				g_ClipDictionaryStore.StreamingRequest(strLocalIndex(outClipDictIndex), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				return false;
			}
		}
	}

	return true; //either dictHash is zero or it's valid and has been found streamed in
}

