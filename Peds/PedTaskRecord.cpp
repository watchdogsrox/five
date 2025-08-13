#include "Peds\PedTaskRecord.h"
#include "Peds\PedIntelligence.h"
#include "Peds\Ped.h"
#include "PedGroup\PedGroup.h"
#include "Event\Events.h"
#include "Script\gta_thread.h"
#include "Script\ScriptTaskTypes.h"
#include "Task\System\Task.h"
#include "Task\General\TaskBasic.h"

AI_OPTIMISATIONS()

CPedScriptedTaskRecordData::CPedScriptedTaskRecordData()
: m_iCommandType(SCRIPT_TASK_INVALID),
  m_pEvent(0),
  m_pTask(0),
  m_pPed(0),
  m_iStage(VACANT_STAGE)
{
}

CPedScriptedTaskRecordData CPedScriptedTaskRecord::ms_scriptedTasks[MAX_NUM_SCRIPTED_TASKS];

void CPedScriptedTaskRecordData::Flush()
{
	m_pPed=0;
	m_pEvent=0;
	m_pTask=0;
	m_iStage=VACANT_STAGE;
	m_iCommandType=SCRIPT_TASK_INVALID;
}

#if __DEV
void CPedScriptedTaskRecordData::Set(CPed* pPed, const s32 iCommandType, const CEventScriptCommand* pEvent, const char* pString)
{
	CPedScriptedTaskRecord::CheckUniqueScriptCommand(pPed,iCommandType,pEvent,pString);
#else
void CPedScriptedTaskRecordData::Set(CPed* pPed, const s32 iCommandType, const CEventScriptCommand* pEvent)
{
#endif
	Flush();
	m_pPed=pPed;
	m_iCommandType=iCommandType;
	Assert(pEvent);
	m_pEvent=pEvent;
	m_pTask=0;
	m_iStage=EVENT_STAGE;			
	// Store the current status with the ped
	m_pPed->GetPedIntelligence()->GetQueriableInterface()->SetScriptedTaskStatus( m_iCommandType, (s8)m_iStage );
}

void CPedScriptedTaskRecordData::Set(CPed* pPed, const s32 iCommandType, const aiTask* pTask)
{
	Flush();
	m_pPed=pPed;
	m_iCommandType=iCommandType;
	m_pEvent=0;
	m_pTask=pTask;
	m_iStage=ACTIVE_TASK_STAGE;
	// Store the current status with the ped
	m_pPed->GetPedIntelligence()->GetQueriableInterface()->SetScriptedTaskStatus( m_iCommandType, (s8)m_iStage );
}

/*	GW - 30/08/06 - once the script grabs the head of the queue, he is no longer considered part of the attractor
void CPedScriptedTaskRecordData::SetAsAttractorScriptTask(CPed* pPed, const int iCommandType, const CTask* pTask)
{
	Flush();
	m_pPed=pPed;
	m_iCommandType=iCommandType;
	m_pEvent=0;
	m_pTask=pTask;
	m_iStage=ATTRACTOR_SCRIPT_TASK_STAGE;
	// Store the current status with the ped
	m_pPed->GetPedIntelligence()->GetPedIntelligence()->GetQueriableInterface2()->SetScriptedTaskStatus( (s16)m_iCommandType, (s8)m_iStage );
}
end of GW - 30/08/06 - once the script grabs the head of the queue, he is no longer considered part of the attractor
*/

void CPedScriptedTaskRecordData::SetAsGroupTask(CPed* pPed, const s32 iCommandType, const aiTask* pTask)
{
	Flush();
	m_pPed=pPed;
	m_iCommandType=iCommandType;
	m_pEvent=0;
	m_pTask=pTask;
	m_iStage=GROUP_TASK_STAGE;
	// Store the current status with the ped
	m_pPed->GetPedIntelligence()->GetQueriableInterface()->SetScriptedTaskStatus( m_iCommandType, (s8)m_iStage );
}

//Associate the record with a new event.
void CPedScriptedTaskRecordData::AssociateWithEvent(CEventScriptCommand* p) 
{
	Assert(m_pEvent);
	m_pEvent=p;
	Assert(0==m_pTask);
	m_iStage=EVENT_STAGE;
}

//Associate the record with a task. 
void CPedScriptedTaskRecordData::AssociateWithTask(aiTask* p) 
{
	m_pEvent=0;
	m_pTask=p;
	m_iStage=ACTIVE_TASK_STAGE;
}
	

int CPedScriptedTaskRecord::GetVacantSlot()
{
	int iVacantSlot=-1;
	int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		if(ms_scriptedTasks[i].IsVacant())
		{
			iVacantSlot=i;
			break;
		}
	}
#if !__FINAL
	if(-1==iVacantSlot)
	{
		SpewRecords();
	}
#endif
	return iVacantSlot;
}


#if !__FINAL
void CPedScriptedTaskRecord::SpewRecords()
{
	for(int i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		CPedScriptedTaskRecordData& r=ms_scriptedTasks[i];	
		if(!r.IsVacant())
		{
			pedDebugf1("SpewRecords CPedScriptedTaskRecord %d: Ped id: %p Command id: %d", i, r.m_pPed.Get(), r.m_iCommandType);
		}
	}
	Assertf(false,"No task records available.");
}
#endif

CPedScriptedTaskRecordData* CPedScriptedTaskRecord::GetRecordAssociatedWithEvent(CEventScriptCommand* p)
{
	CPedScriptedTaskRecordData* pRecord=0;
	int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		if(ms_scriptedTasks[i].m_pEvent==p)
		{
			pRecord=&ms_scriptedTasks[i];
			break;
		}
	}
	return pRecord;	
}

CPedScriptedTaskRecordData* CPedScriptedTaskRecord::GetRecordAssociatedWithTask(aiTask* p)
{
	CPedScriptedTaskRecordData* pRecord=0;
	int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		if(ms_scriptedTasks[i].m_pTask==p)
		{
			pRecord=&ms_scriptedTasks[i];
			break;
		}
	}
	return pRecord;	
}

void CPedScriptedTaskRecord::Process()
{
	int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		CPedScriptedTaskRecordData& r=ms_scriptedTasks[i];
		
		if(!r.IsVacant())
		{
			if(0==r.m_pPed)
			{
				//Ped has been removed so clear the record.
				r.Flush();
			}		
			else if(!r.m_pEvent && !r.m_pTask)
			{
				r.Flush();
			}
			else if(r.m_pEvent)
			{
				//If ped no longer has the event clear the record.
				CPedIntelligence* pPedIntelligence=r.m_pPed->GetPedIntelligence();
				r.m_iStage=CPedScriptedTaskRecordData::EVENT_STAGE;
				if(!pPedIntelligence->HasEvent((const CEvent*)static_cast<const fwEvent*>(r.m_pEvent)))
				{
					r.Flush();
				}
			}
			else if(r.m_pTask)
			{	
				//If ped no longer has the task then clear the record.
/*	GW - 30/08/06 - once the script grabs the head of the queue, he is no longer considered part of the attractor
				if(CPedScriptedTaskRecordData::ATTRACTOR_SCRIPT_TASK_STAGE==r.m_iStage)
				{
					if(CScriptedBrainTaskStore::GetTask(r.m_pPed)!=r.m_pTask)
					{
						r.Flush();
					} 
				}
				else 
//	end of GW - 30/08/06 - once the script grabs the head of the queue, he is no longer considered part of the attractor
*/
				if(CPedScriptedTaskRecordData::GROUP_TASK_STAGE==r.m_iStage)
				{
					/*CPedGroup* pPedGroup=r.m_pPed->GetPedsGroup();
					if(0==pPedGroup || pPedGroup->GetGroupIntelligence()->GetTaskScriptCommand(r.m_pPed)!=r.m_pTask)
					{
						r.Flush();
					} */
				}
				else
				{
					CPedIntelligence* pPedIntelligence=r.m_pPed->GetPedIntelligence();
					if(pPedIntelligence->GetTaskActive()==r.m_pTask)
					{
						r.m_iStage=CPedScriptedTaskRecordData::ACTIVE_TASK_STAGE;
					}
					else if(pPedIntelligence->GetTaskEventResponse()==r.m_pTask ||
					 		pPedIntelligence->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY)==r.m_pTask || 
					 		pPedIntelligence->GetTaskDefault()==r.m_pTask)
					{
						r.m_iStage=CPedScriptedTaskRecordData::DORMANT_TASK_STAGE;
					}
					else if(pPedIntelligence->HasTaskSecondary(r.m_pTask))
					{
						r.m_iStage=CPedScriptedTaskRecordData::SECONDARY_TASK_STAGE;
					}
					else 
					{
						r.Flush();
					}
				}
			}

			// If the status is still valid, store the current status with the ped
			if( !r.IsVacant() && r.m_pPed )
			{
				bool bAddThisTaskStatus = true;
				// Check to make sure there are no more than 1 entry per ped at any time
				for( s32 j = 0; j < i; j++ )
				{
					if( !ms_scriptedTasks[j].IsVacant() && ms_scriptedTasks[j].m_pPed && ms_scriptedTasks[j].m_pPed == r.m_pPed )
					{
#if __DEV
						if( ( r.m_iStage == CPedScriptedTaskRecordData::EVENT_STAGE &&
							ms_scriptedTasks[j].m_iStage == CPedScriptedTaskRecordData::EVENT_STAGE ) ||
							( r.m_iStage != CPedScriptedTaskRecordData::EVENT_STAGE &&
							ms_scriptedTasks[j].m_iStage != CPedScriptedTaskRecordData::EVENT_STAGE ) )
						{
							char info[1024];
							sprintf(info, "info: ");
							if( r.m_iStage == CPedScriptedTaskRecordData::EVENT_STAGE )
								strcat(info, "Stage1(event), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::ACTIVE_TASK_STAGE )
								strcat(info, "Stage1(active), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::DORMANT_TASK_STAGE )
								strcat(info, "Stage1(dormant), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::VACANT_STAGE )
								strcat(info, "Stage1(vacant), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::GROUP_TASK_STAGE )
								strcat(info, "Stage1(group), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::UNUSED_ATTRACTOR_SCRIPT_TASK_STAGE )
								strcat(info, "Stage1(attract), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::SECONDARY_TASK_STAGE )
								strcat(info, "Stage1(secondary), ");

							if( r.m_iStage == CPedScriptedTaskRecordData::EVENT_STAGE )
								strcat(info, "Stage2(event), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::ACTIVE_TASK_STAGE )
								strcat(info, "Stage2(active), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::DORMANT_TASK_STAGE )
								strcat(info, "Stage2(dormant), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::VACANT_STAGE )
								strcat(info, "Stage2(vacant), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::GROUP_TASK_STAGE )
								strcat(info, "Stage2(group), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::UNUSED_ATTRACTOR_SCRIPT_TASK_STAGE )
								strcat(info, "Stage2(attract), ");
							else if( r.m_iStage == CPedScriptedTaskRecordData::SECONDARY_TASK_STAGE )
								strcat(info, "Stage2(secondary), ");

							if (r.m_pPed->GetThreadThatCreatedMe())
							{
								Displayf("Ped was created by script called %s\n", const_cast<GtaThread*>(r.m_pPed->GetThreadThatCreatedMe())->GetScriptName());
								strcat(info, "Miss(");
								strcat(info, const_cast<GtaThread*>(r.m_pPed->GetThreadThatCreatedMe())->GetScriptName());
								strcat(info, "), ");
							}
							Displayf("Command Type of first record = %d\n", ms_scriptedTasks[j].m_iCommandType);
							if (ms_scriptedTasks[j].m_pEvent)
							{
								Displayf("Event of first record = %s\n", (const char *) (ms_scriptedTasks[j].m_pEvent->GetName()) );
								strcat(info, "E1(");
								strcat(info, (const char *) (ms_scriptedTasks[j].m_pEvent->GetName()));
								strcat(info, "), ");
							}
							if (ms_scriptedTasks[j].m_pTask)
							{
								Displayf("Task of first record = %s\n", (const char *) (ms_scriptedTasks[j].m_pTask->GetName()) );
								strcat(info, "T1(");
								strcat(info, (const char *) (ms_scriptedTasks[j].m_pTask->GetName()));
								strcat(info, "), ");
							}

							Displayf("Command Type of second record = %d\n", r.m_iCommandType);
							if (r.m_pEvent)
							{
								Displayf("Event of second record = %s\n", (const char *) (r.m_pEvent->GetName()) );
								strcat(info, "E2(");
								strcat(info, (const char *) (r.m_pEvent->GetName()));
								strcat(info, "), ");
							}
							if (r.m_pTask)
							{
								Displayf("Task of second record = %s\n", (const char *) (r.m_pTask->GetName()) );
								strcat(info, "T2(");
								strcat(info, (const char *) (r.m_pTask->GetName()));
								strcat(info, "), ");
							}

							Assertf( 0,  "More than one task recorded for a single ped! (%s)", info );
						}
#endif
						if( ms_scriptedTasks[j].m_iStage == CPedScriptedTaskRecordData::EVENT_STAGE )
						{
							bAddThisTaskStatus = false;
						}
					}
				}
				if( bAddThisTaskStatus )
				{
					r.m_pPed->GetPedIntelligence()->GetQueriableInterface()->SetScriptedTaskStatus( r.m_iCommandType, (s8)r.m_iStage );
				}
			}
		}
	}

#if !__FINAL
	int iCount=0;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		CPedScriptedTaskRecordData& r=ms_scriptedTasks[i];
		
		if(!r.IsVacant())
		{
			iCount++;
		}
	}		
	
	if(iCount > (MAX_NUM_SCRIPTED_TASKS-8))
	{
		//printf("GROUPEVENTINFO: Causing Event:%s (%d) to be replaced for group %d\n", m_pCurrentEvent->GetBaseEvent()->GetName(), iCurrentEventPriority, pedGroupId);
		printf("High level of script task records: %d \n", iCount);
	}
#endif
}

int CPedScriptedTaskRecord::GetStatus(const CPed* pPed, const int iCommandType )
{
	int iStatus=-1;
	if( pPed->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskCommand() == iCommandType ||
		SCRIPT_TASK_ANY==iCommandType )
	{
		iStatus = pPed->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskStage();
	}

    // in network games we need to check if there is a pending script task being set
    // on the ped, this would be the case if the local machine has requested giving a
    // task to a remotely controlled ped. Before a response has been received from the
    // owner the scripted task status should be EVENT_STAGE, although the queriable state
    // may still be in the VACANT_STAGE at this point
    if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
    {
        if(iStatus == -1)
        {
            iStatus = NetworkInterface::GetPendingScriptedTaskStatus(pPed);
        }
    }
	/*int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		const CPedScriptedTaskRecordData& r=ms_scriptedTasks[i];
		if(((SCRIPT_TASK_ANY==iCommandType)||(r.m_iCommandType==iCommandType)) && (r.m_pPed==pPed))
		{
			iStatus=r.m_iStage;
			break;
		}
	}*/
	return iStatus;
}

int CPedScriptedTaskRecord::GetStatus(const CPed* pPed)
{
	return pPed->GetPedIntelligence()->GetQueriableInterface()->GetScriptedTaskCommand() != SCRIPT_TASK_INVALID;
/*	bool bStatus=false;
	int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{
		const CPedScriptedTaskRecordData& r=ms_scriptedTasks[i];
		if(r.m_pPed==pPed)
		{
			bStatus=true;
			break;
		}
	}
	return bStatus;*/
}

#if __DEV
void CPedScriptedTaskRecord::CheckUniqueScriptCommand(CPed* pPed, const int iCommandType, const CEventScriptCommand* ASSERT_ONLY(pEvent), const char* ASSERT_ONLY(pString))
{
	int iCount=0;
	int i;
	for(i=0;i<MAX_NUM_SCRIPTED_TASKS;i++)
	{	
		const CPedScriptedTaskRecordData& r=ms_scriptedTasks[i];
		if(!r.IsVacant())
		{
			if(r.m_pPed==pPed && r.m_iCommandType==iCommandType && r.m_iStage==CPedScriptedTaskRecordData::EVENT_STAGE)
			{
				iCount++;
			}
		}
	}
	
	Assertf(iCount<=2,"Ped already has this script command, %d %s %s \n", iCommandType, (const char*) pEvent->m_pTask->GetName(), pString);
}
#endif	

