// File header
#include "Event/EventHandler.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "ai\debug\system\AIDebugLogManager.h"
#include "Event/Event.h"
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedTaskRecord.h"
#include "Task/Physics/TaskNM.h"
#include "Task/System/TaskClassInfo.h"

// rage headers
#include "Profile/timebars.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventHandlerHistory
//////////////////////////////////////////////////////////////////////////

void CEventHandlerHistory::Flush()
{
	Assert(!(m_pStoredEventNonTemp && m_pCurrentEventNonTemp == m_pStoredEventNonTemp));

	ClearAllEvents();
	ClearStoredEvent();
}

CEvent* CEventHandlerHistory::GetCurrentEvent() const
{
	if(m_pCurrentEventTemp)
	{
		return m_pCurrentEventTemp;
	}
	else if(m_pCurrentEventNonTemp)
	{
		return m_pCurrentEventNonTemp;
	}
	else if(m_pCurrentMovementEvent)
	{
		return m_pCurrentMovementEvent;
	}
	else
	{
		return NULL;
	}
}

void CEventHandlerHistory::SetCurrentEvent(const CEvent& rCurrentEvent, const bool bOverrideResponseAsNonTemp)
{
	// Don't store script commands because the event response tasks 
	// are put in the primary task slot.  Consequently, there is no need 
	// to remember the event because the task will never be removed until
	// another script command displaces the task
	if(rCurrentEvent.GetEventType() == EVENT_SCRIPT_COMMAND)
	{
		return;
	}

	if(!rCurrentEvent.IsTemporaryEvent() || bOverrideResponseAsNonTemp)
	{        
		Assert(!(m_pCurrentEventNonTemp && m_pStoredEventNonTemp));

		ClearTempEvent();
		ClearNonTempEvent();
		ClearStoredEvent();
		m_pCurrentEventNonTemp = static_cast<CEvent*>(rCurrentEvent.Clone());
	}
	else
	{
		if(m_pCurrentEventNonTemp) 
		{
			StoreNonTempEvent();
			m_pCurrentEventNonTemp = NULL;
		}

		ClearTempEvent();
		m_pCurrentEventTemp = static_cast<CEvent*>(rCurrentEvent.Clone());  
	}
}

void CEventHandlerHistory::SetMovementEvent(const CEvent& rCurrentEvent)
{
	ClearMovementEvent();
	m_pCurrentMovementEvent = static_cast<CEvent*>(rCurrentEvent.Clone());
}

int CEventHandlerHistory::GetCurrentEventPriority() const
{
	if(m_pCurrentEventTemp)
	{
		return m_pCurrentEventTemp->GetEventPriority();
	}
	else if(m_pCurrentEventNonTemp)
	{
		return m_pCurrentEventNonTemp->GetEventPriority();
	}
	else
	{
		return -1;
	}
}

void CEventHandlerHistory::ClearAllEvents()
{
	ClearTempEvent();
	ClearNonTempEvent(); 
	ClearMovementEvent();
}

void CEventHandlerHistory::ClearTempEvent()
{
	if(m_pCurrentEventTemp)
	{
		delete m_pCurrentEventTemp;
		m_pCurrentEventTemp = NULL;
	}
}

void CEventHandlerHistory::ClearNonTempEvent()
{
	if(m_pCurrentEventNonTemp)
	{
		delete m_pCurrentEventNonTemp;
		m_pCurrentEventNonTemp = NULL;
	}
}

void CEventHandlerHistory::ClearStoredEvent()
{
	if(m_pStoredEventNonTemp)
	{
		delete m_pStoredEventNonTemp;
		m_pStoredEventNonTemp = NULL;
	}
}

void CEventHandlerHistory::ClearMovementEvent()
{
	if(m_pCurrentMovementEvent)
	{
		delete m_pCurrentMovementEvent;
		m_pCurrentMovementEvent = NULL;
	}
}

void CEventHandlerHistory::ClearRagdollResponse()
{
	if(m_pCurrentEventTemp)
	{
		m_pCurrentEventTemp->ClearRagdollResponse();
	}

	if(m_pCurrentEventNonTemp)
	{
		m_pCurrentEventNonTemp->ClearRagdollResponse();
	}
}

bool CEventHandlerHistory::IsRespondingToEvent(const int iEventType) const
{
	if(iEventType == EVENT_INVALID)
	{
		return (m_pCurrentEventTemp || m_pCurrentEventNonTemp || m_pStoredEventNonTemp);
	}
	else
	{
		if(m_pCurrentEventTemp && m_pCurrentEventTemp->GetEventType() == iEventType)
		{
			return true;
		}

		if(m_pCurrentEventNonTemp && m_pCurrentEventNonTemp->GetEventType() == iEventType)
		{
			return true;
		}

		if(m_pStoredEventNonTemp && m_pStoredEventNonTemp->GetEventType() == iEventType)
		{
			return true;
		}
	}

	return false;
}

bool CEventHandlerHistory::TakesPriorityOverCurrentEvent(const CEvent& newEvent, const bool bOverrideResponseAsNonTemp)
{
	if(m_pCurrentEventNonTemp)
	{
		// Currently responding to an active (non-temp) event
		// Need to take priority over only the active event
		Assert(!m_pCurrentEventTemp);
		Assert(!m_pStoredEventNonTemp);
		return newEvent.HasPriority(*m_pCurrentEventNonTemp);
	}
	else if(m_pCurrentEventTemp)
	{
		Assert(!m_pCurrentEventNonTemp);
		if(newEvent.IsTemporaryEvent() && !bOverrideResponseAsNonTemp)
		{
			return newEvent.HasPriority(*m_pCurrentEventTemp);
		}
		else
		{
			return newEvent.HasPriority(*m_pCurrentEventTemp) && (NULL == m_pStoredEventNonTemp || newEvent.HasPriority(*m_pStoredEventNonTemp));
		}			
	}
	else
	{
		return true;
	}
}

void CEventHandlerHistory::TickStoredEvent()
{
	if(m_pStoredEventNonTemp)
	{
		Assert(NULL == m_pCurrentEventNonTemp);
		
		// Active event was stored because a passive event occurred
		// If the passive event is finished then release the stored active event
		if(NULL == m_pCurrentEventTemp)
		{
			m_pCurrentEventNonTemp = m_pStoredEventNonTemp;
			m_pStoredEventNonTemp = NULL;
		}
	}
}

void CEventHandlerHistory::StoreNonTempEvent()
{
	Assert(m_pCurrentEventNonTemp);
	Assert(m_pStoredEventNonTemp != m_pCurrentEventNonTemp);
	
	ClearStoredEvent();
	m_pStoredEventNonTemp = m_pCurrentEventNonTemp;
}

#if __DEV

void CEventHandlerHistory::Print() const
{
	taskDisplayf("Aborted task: %s", m_pAbortedTask ? m_pAbortedTask->GetName() : "none" );
	CEvent* pCurrent = GetCurrentEvent();
	if (GetCurrentEvent())
	{
		taskDisplayf("Current event: %s - priority:%d, persist:%s, time:%d/%d", pCurrent->GetName().c_str(), pCurrent->GetEventPriority(), pCurrent->IsPersistant()?"true" : "false", pCurrent->GetAccumulatedTime(), pCurrent->GetLifeTime());
	}

	taskDisplayf("Current non temp event: %s", m_pCurrentEventNonTemp ? m_pCurrentEventNonTemp->GetName() : "none" );
	taskDisplayf("Stored non temp event: %s", m_pStoredEventNonTemp ? m_pStoredEventNonTemp->GetName() : "none" );
	taskDisplayf("Current temp event: %s", m_pCurrentEventTemp ? m_pCurrentEventTemp->GetName() : "none" );
	taskDisplayf("Current movement event: %s", m_pCurrentMovementEvent ? m_pCurrentMovementEvent->GetName() : "none" );
}

#endif // __DEV

//////////////////////////////////////////////////////////////////////////
// CEventHandler
//////////////////////////////////////////////////////////////////////////

CEventHandler::CEventHandler(CPed* pPed)
: m_pPed(pPed)
{
	ResetResponse();
}

void CEventHandler::HandleEvents()
{
	Assert(!m_pPed->IsNetworkClone());

	// Release any stored events
	m_history.TickStoredEvent();

	// Cache the CPedIntelligence
	CPedIntelligence* pPedIntelligence = m_pPed->GetPedIntelligence();

	// Get the event response task, if any, that is being aborted to make way for a new event response
	CTask* pAbortedTaskEventResponse = m_history.GetAbortedTask();

	CTask* pTaskEventResponse = NULL;
	CTask* pTempTaskEventResponse = NULL;
	CTask* pNonTempTaskEventResponse = NULL;
	CTask* pMovementTaskEventResponse = NULL;
	// all will be set here
	pPedIntelligence->GetTasksEventRespone(pTaskEventResponse, pTempTaskEventResponse, pNonTempTaskEventResponse, pMovementTaskEventResponse );

	// First update any communication events so any events they contain can be removed
	pPedIntelligence->UpdateCommunicationEvents(m_pPed);

	// Allow tasks to filter any events
	//AllowTasksToHandleEvents(m_pPed);

	// Get the highest priority event from the ped's event group
#if DEBUG_EVENT_HISTORY
	pPedIntelligence->EventHistoryMarkProcessedEvents();
#endif // DEBUG_EVENT_HISTORY
	CEvent* pHighestPriorityEvent = pPedIntelligence->GetHighestPriorityEvent();
	// Using a helper class, for handling protection of the event being processed
	CEventBeingProcessed eventBeingProcessed(m_pPed, pHighestPriorityEvent);

	// Increment the accumulate time of each recorded event
	pPedIntelligence->TickEvents();

	// If an event response task is being aborted then re-record the task that is being aborted
	if(pAbortedTaskEventResponse)
	{
		if(NULL == pTaskEventResponse)
		{
			m_history.SetAbortedTask(NULL);
		}
		else if(pAbortedTaskEventResponse == pTaskEventResponse)
		{
			m_history.SetAbortedTask(pTaskEventResponse);
		}
		else
		{
			// Aborted task was response to temp event that has finished to reveal a non-temp event response task
			m_history.SetAbortedTask(NULL);
		}
	}

	// If there is no event response task then clear the recorded tasks and events
	if(!pTempTaskEventResponse && !pNonTempTaskEventResponse)
	{
		m_history.SetAbortedTask(NULL);
	}

	if(!pTempTaskEventResponse)
	{
		// Clear the "passive" event
		m_history.ClearTempEvent();
	}

	if(!pNonTempTaskEventResponse)
	{
		// Clear the "active" event
		m_history.ClearNonTempEvent();
	}

	if(!pMovementTaskEventResponse)
	{
		// Clear the "movement" event
		m_history.ClearMovementEvent();
	}

	// If the ped isn't responding to anything and there is no new event 
	// then the ped has nothing to respond to
	if(!pHighestPriorityEvent)
	{
		if(!pTaskEventResponse)
		{
			m_history.SetAbortedTask(NULL);
			m_history.ClearAllEvents();
		}

		pPedIntelligence->RemoveInvalidEvents(false, true);
		return;
	}

#if DEBUG_EVENT_HISTORY
	pPedIntelligence->SetEventHistoryEntrySelectedAsHighest(*pHighestPriorityEvent);
#endif // DEBUG_EVENT_HISTORY

	bool bForcePriority = false;
	// B* 2065900: make sure death events can't be permanently blocked by
	// high priority event give ped task events with no task running.
	if (pHighestPriorityEvent 
		&& pHighestPriorityEvent->GetEventType()==EVENT_DEATH
		&& GetCurrentEvent() && GetCurrentEvent()->GetEventType()==EVENT_GIVE_PED_TASK)
	{
		const CEventGivePedTask& giveTaskEvent = static_cast<const CEventGivePedTask&>(*GetCurrentEvent());
		if (giveTaskEvent.GetEventPriority()==E_PRIORITY_DRAGGING && m_pPed)
		{
			const CPedIntelligence* pPedIntelligence = m_pPed->GetPedIntelligence();
			if (pPedIntelligence && pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_DRAGGED_TO_SAFETY)==NULL)
			{
				bForcePriority = true;
			}
		}
	}

	// Test if the highest priority event takes precedence over the event 
	// that the ped is currently handling
	if(m_history.TakesPriorityOverCurrentEvent(*pHighestPriorityEvent, m_response.GetOverrideResponseAsNonTemp()) || bForcePriority)
	{
		// we're about to consider responding, so make sure the event has a valid response, if this is a decision maker response
		// now generate the response task
		// If this response isn't valid loop through and find the next highest priority event
		while(pHighestPriorityEvent)
		{
			// If the ped is set up to block all non-temporary events, check to see if this one should be ignored
			if(m_pPed->GetBlockingOfNonTemporaryEvents() && 
				!pHighestPriorityEvent->IsTemporaryEvent() &&
				pHighestPriorityEvent->GetEventPriority() < E_PRIORITY_BLOCK_NON_TEMP_EVENTS &&
				pHighestPriorityEvent->GetEventType() != EVENT_SCRIPT_COMMAND &&
				pHighestPriorityEvent->GetEventType() != EVENT_LEADER_ENTERED_CAR_AS_DRIVER &&
				pHighestPriorityEvent->GetEventType() != EVENT_LEADER_EXITED_CAR_AS_DRIVER && 
				pHighestPriorityEvent->GetEventType() != EVENT_SCENARIO_FORCE_ACTION)
			{
				if(!pTaskEventResponse)
				{
					m_history.SetAbortedTask(NULL);
					m_history.ClearAllEvents();
				}

				pPedIntelligence->RemoveInvalidEvents(false, true);
				return;
			}

			// If the highest priority event is the same kind of event that the ped is currently responding to 
			// then don't bother with the latest event.  However, if the event is a script command then respond
			// to the event
			eEventType currentResponseEventType = EVENT_NONE;
			bool bCanBeInterruptedBySameEvent = false;
			if (m_history.GetCurrentEvent())
			{
				currentResponseEventType = static_cast<eEventType>(m_history.GetCurrentEvent()->GetEventType());
				bCanBeInterruptedBySameEvent = m_history.GetCurrentEvent()->CanBeInterruptedBySameEvent();
			}
			// If the ped does not have a current event, we compare the new one with the event the current pTaskEventResponse responded to.
			// NOTE[egarcia]: IsResponseForEventType is synchronized in online games for some types of tasks and prevents a task from being interrupted 
			// even if we have lost the current event info (after the ped ownership has been transferred, for example).
			// If we ever synchronize peds events, this would not be necessary.
			else if (pTaskEventResponse)
			{
				currentResponseEventType = pTaskEventResponse->GetIsResponseForEventType();
				// NOTE[egarcia]: We don't know all the old event information. We use the new one to check if this event type can be interrupted.
				// It should be fine if it just depends on the type of event and not on the event instance data.
				bCanBeInterruptedBySameEvent = pHighestPriorityEvent->CanBeInterruptedBySameEvent(); 
			}

			if (currentResponseEventType == static_cast<eEventType>(pHighestPriorityEvent->GetEventType()) &&
				!bCanBeInterruptedBySameEvent)
			{
#if DEBUG_EVENT_HISTORY
				pPedIntelligence->SetEventHistoryEntryState(*pHighestPriorityEvent, CPedIntelligence::SEventHistoryEntry::SState::DISCARDED_CANNOT_INTERRUPT_SAME_EVENT);
#endif // DEBUG_EVENT_HISTORY

				pPedIntelligence->FlagEventForDeletion(pHighestPriorityEvent);
				pPedIntelligence->RemoveInvalidEvents(false, true);

				if(!pTaskEventResponse)
				{
					m_history.SetAbortedTask(NULL);
					m_history.ClearAllEvents();
				}
				return;
			}

			// If its a decision maker based response make sure there is a valid response
			if(pHighestPriorityEvent->HasEditableResponse())
			{
				CEventEditableResponse* pEventEditable = static_cast<CEventEditableResponse*>(pHighestPriorityEvent);
				if(!pEventEditable->HasCalculatedResponse())
				{
					pEventEditable->ComputeResponseTaskType(m_pPed);
				}

				// Valid event, continue
				if(pEventEditable->WillRespond())
				{
					break;
				}
				// No valid response, try and find another high priority event
				else
				{
					// We've already ticked all the events above, so GetHighestPriorityEvent() below won't actually give us the right result anymore.
					// So, untick them them prior to querying.
					pPedIntelligence->UnTickEvents();
					CEvent* pNewEvent = pPedIntelligence->GetHighestPriorityEvent();
					// Now, retick the events so we don't leave them in the queue.
					pPedIntelligence->TickEvents();
					if(aiVerifyf(pNewEvent!=pHighestPriorityEvent, "Same event returned, infinite loop averted!"))
					{
						pHighestPriorityEvent = pNewEvent;
						eventBeingProcessed.SetEvent(pHighestPriorityEvent);
						if(pHighestPriorityEvent && !m_history.TakesPriorityOverCurrentEvent(*pHighestPriorityEvent, m_response.GetOverrideResponseAsNonTemp()))
						{
							pHighestPriorityEvent = NULL;
							eventBeingProcessed.SetEvent(NULL);
						}
					}
					else
					{
						pHighestPriorityEvent = NULL;
						eventBeingProcessed.SetEvent(NULL);
					}
				}
			}
			else
			{
				break;
			}
		}

		// The event was invalidated as there was no decision maker response that was higher
		// priority than the current response, return
		if(pHighestPriorityEvent == NULL)
		{
			if(!pTaskEventResponse)
			{
				m_history.SetAbortedTask(NULL);
				m_history.ClearAllEvents();
			}

			pPedIntelligence->RemoveInvalidEvents(false, true);
			return;
		}

		bool bReadyForNewTask = true;
		bool bHasMainEventResponseTask = false;

		// First try to compute a response to the task
		ComputeEventResponseTask(pHighestPriorityEvent);

		// Invalid to have both an EventResponse and a MovementResponse
		Assert(!(m_response.GetEventResponse(CEventResponse::EventResponse) && m_response.GetEventResponse(CEventResponse::MovementResponse)));

		// If no main response was generated, there is no need to abort the task
		if(m_response.GetEventResponse(CEventResponse::EventResponse) || m_response.GetEventResponse(CEventResponse::PhysicalResponse))
		{
			bHasMainEventResponseTask = true;
			
			//Check if the response runs on the primary task tree.
			if(!m_response.GetRunOnSecondary())
			{
#if DEBUG_DRAW && __DEV
				bool bDebugDrawInitialEventDelay = false;
				if (bDebugDrawInitialEventDelay)
				{
					char delayText[128];
					sprintf(delayText, "Event: original delayText = %f", pHighestPriorityEvent->m_fInitialDelay);
					grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()), Color_black, delayText, true, 333);
				}
#endif

				//Try to abort the primary task.
				aiTask* pTaskActive = pPedIntelligence->GetTaskActive();
				if(pTaskActive && !pTaskActive->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, pHighestPriorityEvent))
				{
					// If ped is using ragdoll, and the event is a ragdoll controlling event then it NEEDS tasks to abort
					// UNLESS the task that's running is some special ragdoll controlling task
					// This assert is intended to help us catch tasks that aren't aborting in favour of ragdoll control tasks when they should
					if(m_pPed->GetUsingRagdoll() && 
						(pHighestPriorityEvent->RequiresAbortForRagdoll() || pHighestPriorityEvent->RequiresAbortForMelee(m_pPed)))
					{
						aiTask* pTaskActiveSimplest = pPedIntelligence->GetTaskActiveSimplest();
						if(pTaskActiveSimplest->GetTaskType() == CTaskTypes::TASK_DYING_DEAD ||
							pTaskActiveSimplest->GetTaskType() == CTaskTypes::TASK_NM_SCRIPT_CONTROL ||
							pTaskActive->GetTaskType() == CTaskTypes::TASK_INCAPACITATED)
						{
							bReadyForNewTask = false;
						}
						else
						{
							CTask *pActiveTask = (CTask *) pTaskActiveSimplest;
							CTaskNMBehaviour* pTaskNM = pActiveTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pActiveTask : NULL;
							// NM tasks don't have to abort for ragdoll event if they don't want to because they handle it themselves
							if(pTaskNM)
							{
								bReadyForNewTask = false;
							}
							else
							{
#if __DEV
								Warningf("Task (%s) didn't abort when required for ragdoll", (const char*)pTaskActive->GetName());
								m_pPed->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
#endif
								// Force the task to abort, the ped is already rag dolling so this should be the safest solution at this point
								if(!pTaskActive->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, pHighestPriorityEvent))
								{
									bReadyForNewTask = false;
								}
							}
						}
					}
					else
					{
						bReadyForNewTask = false;
					}
				}
				else if(pTaskActive)
				{
					// MakeAbortable() succeeded, check if the aborted task wants to get deleted.
					if(pTaskActive->MayDeleteOnAbort())
					{
						// It wouldn't be much point in deleting the task at default priority, because a new task will get created
						// immediately (not waiting for the higher priority task to end).
						if(pPedIntelligence->GetTaskDefault() != pTaskActive)
						{
							delete pTaskActive;
							pTaskActive = NULL;
						}
					}
				}
			}
			//The response runs on the secondary task tree.
			else
			{
				//Try to abort the secondary task.
				aiTask* pTaskActive = pPedIntelligence->GetTaskSecondaryActive();
				if(pTaskActive && !pTaskActive->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, pHighestPriorityEvent))
				{
					bReadyForNewTask = false;
				}
				else if(pTaskActive && pTaskActive->MayDeleteOnAbort())		// MakeAbortable() succeeded, check if the aborted task wants to get deleted.
				{
					delete pTaskActive;
					pTaskActive = NULL;
				}
			}
		}
		// If a movement event response task has been generated, 
		// make sure the active movement task is aborted before it is added
		else if(m_response.GetEventResponse(CEventResponse::MovementResponse))
		{
			aiTask* pTaskActiveMovement = pPedIntelligence->GetActiveMovementTask();
			if(pTaskActiveMovement && !pTaskActiveMovement->MakeAbortable(CTask::ABORT_PRIORITY_URGENT, pHighestPriorityEvent))
			{
				m_response.ClearEventResponse(CEventResponse::MovementResponse);
				pTaskActiveMovement = NULL;
				bReadyForNewTask = false;
			}
			else if(pTaskActiveMovement && pTaskActiveMovement->MayDeleteOnAbort())		// MakeAbortable() succeeded, check if the aborted task wants to get deleted.
			{
				delete pTaskActiveMovement;
				pTaskActiveMovement = NULL;
			}
		}

#if DEBUG_EVENT_HISTORY
		if (!bReadyForNewTask)
		{
			pPedIntelligence->SetEventHistoryEntryState(*pHighestPriorityEvent, CPedIntelligence::SEventHistoryEntry::SState::TRYING_TO_ABORT_TASK);
		}
#endif // DEBUG_EVENT_HISTORY

		// Test if the task has no event response task and hasn't just aborted an event response task
		if(!pTaskEventResponse && !pAbortedTaskEventResponse)
		{   
			if(!bReadyForNewTask)
			{
				// Can't immediately interrupt the ped's active task so retain the event for a later time
				pHighestPriorityEvent->UnTick();
				DeleteResponse();
			}
			else if(pHighestPriorityEvent)
			{
				eventBeingProcessed.Clear(); // Event gets deleted in call to SetEventResponseTask
				SetEventResponseTask(*pHighestPriorityEvent);
			}
		}  
		else if((!pTaskEventResponse && pAbortedTaskEventResponse) || bReadyForNewTask)
		{
			// Aborted task has finished
			m_history.SetAbortedTask(NULL);

			if(pHighestPriorityEvent)
			{
				eventBeingProcessed.Clear(); // Event gets deleted in call to SetEventResponseTask
				SetEventResponseTask(*pHighestPriorityEvent);
			}
		}  
		else if(bHasMainEventResponseTask)
		{
			DeleteResponse();
			aiAssert(pTaskEventResponse);
			pHighestPriorityEvent->UnTick();
			m_history.SetAbortedTask(pTaskEventResponse);
		}
	}
#if DEBUG_EVENT_HISTORY
	else
	{
		pPedIntelligence->SetEventHistoryEntryState(*pHighestPriorityEvent, CPedIntelligence::SEventHistoryEntry::SState::DISCARDED_PRIORITY);
	}
#endif // DEBUG_EVENT_HISTORY

	aiAssert(!m_response.HasEventResponse());
	DeleteResponse();
	pPedIntelligence->RemoveInvalidEvents(false, true);
}

int CEventHandler::GetCurrentEventType() const
{
	const CEvent* pEvent = m_history.GetCurrentEvent();
	if(pEvent)
	{
		return pEvent->GetEventType();
	}

	return -1;
}

void CEventHandler::Flush()
{
	m_response.ClearAllEventResponses();
	m_history.Flush();      
}

void CEventHandler::FlushImmediately()
{
	// Don't delete the tasks because any non-null tasks will have been given
	// to the ped's task manager
	m_history.Flush();
}

void CEventHandler::ComputeEventResponseTask(CEvent* pEvent)
{
	ResetResponse();

	if(pEvent->ShouldCreateResponseTaskForPed(m_pPed))
	{
		pEvent->CreateResponseTask(m_pPed, m_response);
	}
}

void CEventHandler::SetEventResponseTask(CEvent& event)
{
	// Store the event response in the history
	if(m_response.GetEventResponse(CEventResponse::EventResponse))
	{
		// Not sure, but if this event just caused a secondary task to run, we probably
		// shouldn't remember it as the "current event". If needed, it's probably better
		// to remember it in a separate variable in CEventHandlerHistory.
		if(!m_response.GetRunOnSecondary())
		{
			m_history.SetCurrentEvent(event, m_response.GetOverrideResponseAsNonTemp());
		}
	}
	else if(m_response.GetEventResponse(CEventResponse::MovementResponse))
	{
		m_history.SetMovementEvent(event);
	}

	// Cache the CPedIntelligence
	CPedIntelligence* pPedIntelligence = m_pPed->GetPedIntelligence();
	CTaskManager* pTaskManager = pPedIntelligence->GetTaskManager();

#if DEBUG_EVENT_HISTORY
	pPedIntelligence->EventHistoryOnEventSetTaskBegin(event);
#endif // DEBUG_EVENT_HISTORY

	// These events get added into the primary task slot
	if(event.GetEventType() == EVENT_SCRIPT_COMMAND || event.GetEventType() == EVENT_GIVE_PED_TASK)
	{
		s32 iTaskPriority = 0;

		if(event.GetEventType() == EVENT_SCRIPT_COMMAND)
		{
			iTaskPriority = static_cast<CEventScriptCommand&>(event).GetTaskPriority();
		}
		else if(event.GetEventType() == EVENT_GIVE_PED_TASK)
		{
			iTaskPriority = static_cast<CEventGivePedTask&>(event).GetTaskPriority();
		}

		if(iTaskPriority == PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP)
		{
			pPedIntelligence->ClearTaskTempEventResponse();
		}
		else if(iTaskPriority >= PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP)
		{
			pPedIntelligence->ClearTaskEventResponse();
		}

		if(event.GetEventType() == EVENT_SCRIPT_COMMAND)
		{
			CEventScriptCommand* pEventSciptCommand = static_cast<CEventScriptCommand*>(&event);
			CPedScriptedTaskRecordData* pTaskRecordData = CPedScriptedTaskRecord::GetRecordAssociatedWithEvent(pEventSciptCommand);
			if(pTaskRecordData)
			{
				pTaskRecordData->AssociateWithTask(m_response.GetEventResponse(CEventResponse::EventResponse));
			}

#if __DEV
			// Record the task in the debug list
			if(m_response.GetEventResponse(CEventResponse::EventResponse))
			{
				pPedIntelligence->AddScriptHistoryString(pTaskRecordData ? CTheScripts::GetScriptTaskName(pTaskRecordData->m_iCommandType) : TASKCLASSINFOMGR.GetTaskName(m_response.GetEventResponse(CEventResponse::EventResponse)->GetTaskType()), 
															TASKCLASSINFOMGR.GetTaskName(m_response.GetEventResponse(CEventResponse::EventResponse)->GetTaskType()), 
															pEventSciptCommand->m_ScriptName, 
															pEventSciptCommand->m_iProgramCounter);
			}
#endif // __DEV
		}

		//! DMKH. Ok, so the active task has allowed us to abort it in preference of this one. In that case, we can optionally
		//! delete all tasks between the active task and the one we are just about to insert.
		if(event.CanDeleteActiveTask())
		{
			pPedIntelligence->ClearTasksAbovePriority(iTaskPriority);
		}

		pPedIntelligence->AddTaskAtPriority(m_response.ReleaseEventResponse(CEventResponse::EventResponse), iTaskPriority, true);
		m_response.SetOverrideResponseAsNonTemp(false);
	}
	else
	{
		if(m_response.GetEventResponse(CEventResponse::EventResponse))
		{
			if(m_response.GetRunOnSecondary())
			{
				//There is only one priority on the secondary task tree.
				pTaskManager->SetTask(PED_TASK_TREE_SECONDARY, m_response.ReleaseEventResponse(CEventResponse::EventResponse), PED_TASK_SECONDARY_PARTIAL_ANIM);
			}
			else
			{
				// Make sure to get rid of physical response if we're not adding a new one
				if(m_response.GetEventResponse(CEventResponse::PhysicalResponse) == NULL)
				{
					pTaskManager->SetTask(PED_TASK_TREE_PRIMARY, NULL, PED_TASK_PRIORITY_PHYSICAL_RESPONSE);
				}

				if(event.IsTemporaryEvent() && !m_response.GetOverrideResponseAsNonTemp())
				{
					pTaskManager->SetTask(PED_TASK_TREE_PRIMARY, m_response.ReleaseEventResponse(CEventResponse::EventResponse), PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
				}
				else
				{
					// Get rid of temp event response if we're not adding a new one
					pPedIntelligence->AddTaskAtPriority(NULL, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
					pPedIntelligence->AddTaskAtPriority(m_response.ReleaseEventResponse(CEventResponse::EventResponse), PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP);
				}
			}

			m_response.SetOverrideResponseAsNonTemp(false);
		}

		if(m_response.GetEventResponse(CEventResponse::PhysicalResponse))
		{
			pPedIntelligence->AddTaskAtPriority(m_response.ReleaseEventResponse(CEventResponse::PhysicalResponse), PED_TASK_PRIORITY_PHYSICAL_RESPONSE, true);
		}

		// If a movement event response task has been generated, process that
		if(m_response.GetEventResponse(CEventResponse::MovementResponse))
		{
			pPedIntelligence->AddTaskMovementResponse(m_response.ReleaseEventResponse(CEventResponse::MovementResponse));
		}
	}

#if DEBUG_EVENT_HISTORY
	pPedIntelligence->EventHistoryOnEventSetTaskEnd(event);
#endif // DEBUG_EVENT_HISTORY

	// Remove the event we are responding to
	pPedIntelligence->RemoveEvent(&event);
}

#if __DEV

void CEventHandler::Print() const
{
	taskDisplayf("--------------Event handler:");
	taskDisplayf("response:");
	m_response.Print();
	taskDisplayf("history:");
	m_history.Print();
}

#endif //___DEV

////////////////////////////////////////////////////////////////////////////////

CEventBeingProcessed::CEventBeingProcessed(CPed* ASSERT_ONLY(pPed), CEvent* pEvent)
: ASSERT_ONLY(m_pPed(pPed),)
  m_pEvent(pEvent)
{
	aiAssert(m_pPed);
	ASSERT_ONLY(SetEventDeletionGuard(true));
	if(m_pEvent)
		m_pEvent->SetBeingProcessed(true);
}

CEventBeingProcessed::~CEventBeingProcessed()
{
	ASSERT_ONLY(SetEventDeletionGuard(false));
	if(m_pEvent)
		m_pEvent->SetBeingProcessed(false);
}

void CEventBeingProcessed::SetEvent(CEvent* pEvent)
{
	if(m_pEvent)
		m_pEvent->SetBeingProcessed(false);
	m_pEvent = pEvent;
	if(m_pEvent)
		m_pEvent->SetBeingProcessed(true);
}

void CEventBeingProcessed::Clear()
{
	SetEvent(NULL);
	ASSERT_ONLY(SetEventDeletionGuard(false));
}

#if __ASSERT
void CEventBeingProcessed::SetEventDeletionGuard(bool /*b*/)
{
//	m_pPed->GetPedIntelligence()->GetEventGroup().SetDeletionGuard(b);
}
#endif // __ASSERT
