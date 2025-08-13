// File header
#include "Event/EventGroup.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Event/EventNetwork.h"
#include "Event/EventReactions.h"	// CEventExplosionHeard
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedDebugVisualiser.h"
#include "Script/script.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "fwsys/timer.h"


AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventGroupPed
//////////////////////////////////////////////////////////////////////////

CEventGroupPed::CEventGroupPed(CPed* pPed)
: m_pPed(pPed)
{
}

fwEvent* CEventGroupPed::Add(const fwEvent& eventBase)
{
	const CEvent& event = static_cast<const CEvent&>(eventBase);

	// Peds controlled by another machine in a network game are not allowed to run any tasks
	if(m_pPed && m_pPed->IsNetworkClone())
	{
		return NULL;
	}

	if(m_pPed)
	{
		// Test if the event affects the ped.
		if(!event.AffectsPed(m_pPed))
		{
#if __BANK
			if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
			{
				aiDisplayf("F(%d): Ped (%s, %p) received and rejected event in affects ped (%s)", fwTimer::GetFrameCount(), m_pPed->GetModelName(), m_pPed, event.GetName().c_str());
			}
			// Event doesn't affect ped so don't accept event.
#endif // __BANK
			return NULL;
		}

		//! Hack. Marge gun shot events from AI peds in MP games as we are blowing the event pull.
		if(NetworkInterface::IsGameInProgress())
		{
			if(event.GetEventType()==EVENT_SHOT_FIRED && event.GetSourcePed() && !event.GetSourcePed()->IsPlayer())
			{
				for(int i = 0; i < GetNumEvents(); i++)
				{
					CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(i));
					if(pEvent)
					{
						if(pEvent->GetEventType()==EVENT_SHOT_FIRED && !pEvent->HasExpired() && (pEvent->GetSourceEntity() == event.GetSourceEntity()))
						{
#if __BANK
							if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
							{
								aiDisplayf("F(%d): Ped (%s, %p) received and rejected duplicate gun shot event (%s)", fwTimer::GetFrameCount(), m_pPed->GetModelName(), m_pPed, event.GetName().c_str());
							}
#endif // __BANK
							return NULL;
						}
					}
				}
			}
		}

		m_pPed->GetPedIntelligence()->RecordEventForScript(event.GetEventType(), event.GetEventPriority());			

		// The new event
		fwEvent* pAddedEvent = NULL;

		AI_EVENT_GROUP_LOCK(this)

		if (CEvent::CanCreateEvent())
		{
			// Add event through base class
			pAddedEvent = CEventGroupAI::Add(event);
		}

		if(pAddedEvent)
		{
#if __BANK
			if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
			{
				aiDisplayf("F(%d): Ped (%s, %p) received and added event (%s)", fwTimer::GetFrameCount(), m_pPed->GetModelName(), m_pPed, event.GetName().c_str());
			}
			// Event doesn't affect ped so don't accept event.
#endif // __BANK

			static_cast<CEvent*>(pAddedEvent)->OnEventAdded(m_pPed);
			return pAddedEvent;
		}
	}

#if __BANK
	if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
	{
		aiDisplayf("F(%d): Ped (%s, %p) received and rejected event in ComputeResponseTaskType (%s)", fwTimer::GetFrameCount(), m_pPed->GetModelName(), m_pPed, event.GetName().c_str());
	}
	// Event doesn't affect ped so don't accept event.
#endif // __BANK

	// Can't add event, ignore
	return NULL;
}

fwEvent* CEventGroupPed::GetLowestPriorityEventThatCanBeRemoved() const
{
	AI_EVENT_GROUP_LOCK(this)

	fwEvent* pLowestPriorityEvent = NULL;
	int iLowestPriority = 0;

	for(int i=0; i<GetNumEvents(); i++)
	{
		fwEvent* pEvent = GetEventByIndex(i);

		if(AssertVerify(pEvent) && pEvent->GetEventType() != EVENT_SCRIPT_COMMAND)
		{
			if(!pEvent->GetBeingProcessed() && (!pLowestPriorityEvent || pEvent->GetEventPriority() <= iLowestPriority))
			{
				iLowestPriority = pEvent->GetEventPriority();
				pLowestPriorityEvent = pEvent;
			}
		}
	}

	return pLowestPriorityEvent;
}

void CEventGroupPed::RemoveInvalidEvents(bool bEverythingButScriptEvents, bool bFlagForDeletion)
{
	AI_EVENT_GROUP_LOCK(this)

	int curr = 0;

	while(curr < GetNumEvents())
	{
		bool bDelete = false;

		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(curr));
		if(pEvent)
		{
			// Remove events whose response has now been calculated but none is found
			bool bInvalidResponse = false;
			if( pEvent && pEvent->HasEditableResponse() )
			{
				CEventEditableResponse* pEventEditable = static_cast<CEventEditableResponse*>(pEvent);
				if( pEventEditable->HasCalculatedResponse() )
				{
					if(!pEventEditable->WillRespond())
					{
						bInvalidResponse = true;
					}
				}
			}

			if(bInvalidResponse || !pEvent->IsValidForPed(m_pPed) || (bEverythingButScriptEvents && pEvent->GetEventType() != EVENT_SCRIPT_COMMAND))
			{
				if( bFlagForDeletion )
				{
					pEvent->SetFlagForDeletion(true);
				}
				else
				{
					bDelete = true;
				}
			}
		}

		if (bDelete)
		{
			Delete(curr);
		}
		else
		{
			curr++;
		}
	}
}

CEvent* CEventGroupPed::GetHighestPriorityEvent() const
{
	AI_EVENT_GROUP_LOCK(this)

	int iHighestPriority = -1;
	atFixedArray<CEvent*, SIZEOF_PED_EVENT_GROUP> aHighestPriorityEvents;

	Assert(m_pPed);

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(i));
		Assert(pEvent);
		pEvent->SetBeingProcessed(true);

		//! HACK. Permit sense checks if this flag is set. This means events already in the queue will pass, but new events are still subject to tests.
		if(m_pPed->GetPedResetFlag(CPED_RESET_FLAG_PermitEventDuringScenarioExit))
		{
			pEvent->DisableSenseChecks();
		}

		const int iPriority = pEvent->GetEventPriority();
		bool bNewBestPriority = false;
		// If two events have the same priority, return the one that is closest to the ped experiencing them.
		if(pEvent->GetEventType() == EVENT_SCRIPT_COMMAND)
		{
			if(iPriority > iHighestPriority)
			{
				if(pEvent->IsEventReady(m_pPed) &&
					pEvent->IsValidForPed(m_pPed) &&
					pEvent->AffectsPed(m_pPed))
				{		
					bNewBestPriority = true;
				}
			}
		}
		else
		{
			if(iPriority >= iHighestPriority)
			{
				if(pEvent->IsEventReady(m_pPed) &&
					pEvent->IsValidForPed(m_pPed) &&
					pEvent->AffectsPed(m_pPed))
				{			      
					if( pEvent && pEvent->HasEditableResponse() )
					{
						// We allow decision maker driven events through only if either they haven't yet
						// been evaluated for a response or if they have and have a valid response
						CEventEditableResponse* pEventEditable = static_cast<CEventEditableResponse*>(pEvent);
						if( !pEventEditable->HasCalculatedResponse() || pEventEditable->WillRespond() )
						{
							bNewBestPriority = true;
						}
					}
					else
					{
						bNewBestPriority = true;
					}
				}
			}
			else
			{
				// Needs to be called to ensure gun shot events get activated
				pEvent->IsEventReady(m_pPed);
			}
		}

		if (bNewBestPriority)
		{
			//potentially triggered for both equal and greater priorities above, only clear array on greater
			if (iPriority > iHighestPriority)
			{
				for(s32 i = 0; i < aHighestPriorityEvents.GetCount(); i++)
				{
					aHighestPriorityEvents[i]->SetBeingProcessed(false);
				}
				aHighestPriorityEvents.clear();
				iHighestPriority = iPriority;
			}
			aHighestPriorityEvents.Push(pEvent);
		}
		else
		{
			pEvent->SetBeingProcessed(false);
		}
	}

	for(s32 i = 0; i < aHighestPriorityEvents.GetCount(); i++)
	{
		aHighestPriorityEvents[i]->SetBeingProcessed(false);
	}

	// Return the closest event of those with equal priority
	// Return NULL if no events in queue
	if (aHighestPriorityEvents.GetCount() == 0)
	{
		return NULL;
	}
	// Return the only element if the size was one
	else if (aHighestPriorityEvents.GetCount() == 1)
	{
		return aHighestPriorityEvents[0];
	}
	// Otherwise, return the closest event to the ped
	return ReturnClosestEventToPed(m_pPed, aHighestPriorityEvents);
}

// Searches the array and returns the even that is physically closest to the given ped.
// assumes the array has size > 1, because of the previous checks made in GetHighestPriorityEvent()
CEvent* CEventGroupPed::ReturnClosestEventToPed(const CPed* pPed, const atFixedArray<CEvent*, SIZEOF_PED_EVENT_GROUP> &aEvents) const
{
	Assertf(aEvents.GetCount() > 1, "ReturnClosesetEventToPed called with less than 2 elements in the array!");

	//cache the ped position
	const Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//initialize the closest event to be the first in the list
	CEvent* pClosest = aEvents[0];
	ScalarV dClosestSq = DistSquared(vPedPosition, pClosest->GetEntityOrSourcePosition());

	//traverse the array, comparing elements 1-n
	for(u32 i=1; i < aEvents.GetCount(); i++)
	{
		CEvent* pEvent = aEvents[i];
		const ScalarV dEventSq = DistSquared(vPedPosition, pEvent->GetEntityOrSourcePosition());
		if (IsLessThanAll(dEventSq, dClosestSq))
		{
			pClosest = pEvent;
			dClosestSq = dEventSq;
		}
	}

	//return closest event
	return pClosest;
}


void CEventGroupPed::UpdateCommunicationEvents(CPed* pPed)
{
	AI_EVENT_GROUP_LOCK(this)

	int curr = 0;

	while (curr < GetNumEvents())
	{
		bool bDelete = false;

		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(curr));
		if(AssertVerify(pEvent) && pEvent->GetEventType() == EVENT_COMMUNICATE_EVENT)
		{
			if(pEvent->IsEventReady(pPed))
			{
				bDelete = true;
			}
		}

		if (bDelete)
		{
			Delete(curr);
		}
		else
		{
			curr++;
		}
	}
}

bool CEventGroupPed::HasScriptCommandOfTaskType(const int iTaskType) const
{
	AI_EVENT_GROUP_LOCK(this)

	bool bFound = false;

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(i));
		if(AssertVerify(pEvent) && pEvent->GetEventType() == EVENT_SCRIPT_COMMAND)
		{
			CEventScriptCommand* pScriptCommandEvent = static_cast<CEventScriptCommand*>(pEvent);
			if(pScriptCommandEvent->GetTask() && pScriptCommandEvent->GetTask()->GetTaskType() == iTaskType)
			{
				bFound = true;
				break;
			}
		}
	}

	return bFound;
}

bool CEventGroupPed::HasRagdollEvent() const
{
	AI_EVENT_GROUP_LOCK(this)

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(i));
		if(pEvent->RequiresAbortForRagdoll() && !pEvent->HasExpired())
		{
			return true;
		}
	}
	return false;
}

bool CEventGroupPed::HasPairedDamageEvent() const
{
	AI_EVENT_GROUP_LOCK(this)

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(i));
		if( pEvent->GetEventPriority() == E_PRIORITY_DAMAGE_PAIRED_MOVE && !pEvent->HasExpired() )
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CEventGroupVehicle
//////////////////////////////////////////////////////////////////////////

CEventGroupVehicle::CEventGroupVehicle(CVehicle* pVehicle)
:m_pVehicle(pVehicle)
{

}

fwEvent* CEventGroupVehicle::Add(const fwEvent& eventBase)
{
	const CEvent& event = static_cast<const CEvent&>(eventBase);

	// Peds controlled by another machine in a network game are not allowed to run any tasks
	if(m_pVehicle && m_pVehicle->IsNetworkClone())
	{
		return NULL;
	}

	if(m_pVehicle)
	{
		if(m_pVehicle->GetBlockingOfNonTemporaryEvents() && !event.IsTemporaryEvent())
		{
#if __BANK
			if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
			{
				aiDisplayf("F(%d): Vehicle (%s, %p) rejected event (%s) due to blocking non-temp events.", fwTimer::GetFrameCount(), m_pVehicle->GetModelName(), m_pVehicle, event.GetName().c_str());
			}
#endif // __BANK

			return NULL;
		}

		// Test if the event affects the ped.
		if(!event.AffectsVehicle(m_pVehicle))
		{
#if __BANK
			if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
			{
				aiDisplayf("F(%d): Vehicle (%s, %p) received and rejected event in affects vehicle (%s)", fwTimer::GetFrameCount(), m_pVehicle->GetModelName(), m_pVehicle, event.GetName().c_str());
			}
			// Event doesn't affect ped so don't accept event.
#endif // __BANK
			return NULL;
		}	

		// The new event
		fwEvent* pAddedEvent = NULL;

		AI_EVENT_GROUP_LOCK(this)

		if (CEvent::CanCreateEvent())
		{
			// Add event through base class
			pAddedEvent = CEventGroupAI::Add(event);
		}

		if(pAddedEvent)
		{
#if __BANK
			if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
			{
				aiDisplayf("F(%d): Vehicle (%s, %p) received and added event (%s)", fwTimer::GetFrameCount(), m_pVehicle->GetModelName(), m_pVehicle, event.GetName().c_str());
			}
			// Event doesn't affect ped so don't accept event.
#endif // __BANK

			//static_cast<CEvent*>(pAddedEvent)->OnEventAdded(m_pVehicle);
			return pAddedEvent;
		}
	}

#if __BANK
	if( CPedDebugVisualiserMenu::ms_menuFlags.m_bLogAIEvents )
	{
		aiDisplayf("F(%d): Vehicle (%s, %p) received and rejected event in ComputeResponseTaskType (%s)", fwTimer::GetFrameCount(), m_pVehicle->GetModelName(), m_pVehicle, event.GetName().c_str());
	}
	// Event doesn't affect ped so don't accept event.
#endif // __BANK

	// Can't add event, ignore
	return NULL;
}

CEvent* CEventGroupVehicle::GetHighestPriorityEvent() const
{
	AI_EVENT_GROUP_LOCK(this)

	int iHighestPriority = -1;
	atFixedArray<CEvent*, SIZEOF_VEHICLE_EVENT_GROUP> aHighestPriorityEvents;

	Assert(m_pVehicle);

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* pEvent = static_cast<CEvent*>(GetEventByIndex(i));
		Assert(pEvent);
		const int iPriority = pEvent->GetEventPriority();
		bool bNewBestPriority = false;
		// If two events have the same priority, return the one that is closest to the ped experiencing them.

		{
			if(iPriority >= iHighestPriority)
			{
				if(/*pEvent->IsEventReady(NULL) &&*/
					pEvent->IsValidForPed(NULL) &&
					pEvent->AffectsVehicle(m_pVehicle))
				{			      
					{
						bNewBestPriority = true;
					}
				}
			}
			else
			{
				// Needs to be called to ensure gun shot events get activated
				//pEvent->IsEventReady(NULL);
			}
		}
		if (bNewBestPriority)
		{
			//potentially triggered for both equal and greater priorities above, only clear array on greater
			if (iPriority > iHighestPriority)
			{
				aHighestPriorityEvents.clear();
				iHighestPriority = iPriority;
			}
			aHighestPriorityEvents.Push(pEvent);
		}
	}
	// Return the closest event of those with equal priority
	// Return NULL if no events in queue
	if (aHighestPriorityEvents.GetCount() == 0)
	{
		return NULL;
	}
	// Return the only element if the size was one
	else if (aHighestPriorityEvents.GetCount() == 1)
	{
		return aHighestPriorityEvents[0];
	}
	// Otherwise, return the closest event to the ped
	return ReturnClosestEventToVehicle(m_pVehicle, aHighestPriorityEvents);
}

// Searches the array and returns the even that is physically closest to the given ped.
// assumes the array has size > 1, because of the previous checks made in GetHighestPriorityEvent()
CEvent* CEventGroupVehicle::ReturnClosestEventToVehicle(const CVehicle* pVehicle, const atFixedArray<CEvent*, SIZEOF_VEHICLE_EVENT_GROUP> &aEvents) const
{
	Assertf(aEvents.GetCount() > 1, "ReturnClosesetEventToPed called with less than 2 elements in the array!");

	//cache the ped position
	const Vec3V vPedPosition = pVehicle->GetVehiclePosition();

	//initialize the closest event to be the first in the list
	CEvent* pClosest = aEvents[0];
	ScalarV dClosestSq = DistSquared(vPedPosition, pClosest->GetEntityOrSourcePosition());

	//traverse the array, comparing elements 1-n
	for(u32 i=1; i < aEvents.GetCount(); i++)
	{
		CEvent* pEvent = aEvents[i];
		const ScalarV dEventSq = DistSquared(vPedPosition, pEvent->GetEntityOrSourcePosition());
		if (IsLessThanAll(dEventSq, dClosestSq))
		{
			pClosest = pEvent;
			dClosestSq = dEventSq;
		}
	}

	//return closest event
	return pClosest;
}

//////////////////////////////////////////////////////////////////////////
// CEventGroupGlobal
//////////////////////////////////////////////////////////////////////////

#if __ASSERT
__THREAD u32 CLockingEventGroupBase::m_LockCount = 0;
#endif // _ASSERT

CEventGroupGlobal* CEventGroupGlobal::ms_pEventGlobalGroup = NULL;

fwEvent* CEventGroupGlobal::Add(const fwEvent& event)
{
	AI_EVENT_GROUP_LOCK(this)
	return CEventGroupAI::Add(event);
}

void CEventGroupGlobal::AddOrCombine(const CEvent& ev)
{
	AI_EVENT_GROUP_LOCK(this)

	// Note: there is great similarity between this function and
	// CShockingEventsManager::Add(). We may want to just move the code from
	// CShockingEventsManager::Add() to here.

	// Note 2: we could just override the virtual Add() to always do this
	// for all events, if we don't want to rely on the user calling
	// AddOrCombine(). If we end up doing this stuff for more event types,
	// we should consider that.

	// If this is a CEventExplosionHeard, try to combine it with an existing
	// CEventExplosionHeard object if possible.
	if(ev.GetEventType() == EVENT_EXPLOSION_HEARD)
	{
		const int numEvents = GetNumEvents();
		for(int i = 0; i < numEvents; i++)
		{
			fwEvent* pExisting = GetEventByIndex(i);
			if(AssertVerify(pExisting) && pExisting->GetEventType() == EVENT_EXPLOSION_HEARD)
			{
				if(static_cast<CEventExplosionHeard*>(pExisting)->TryToCombine(static_cast<const CEventExplosionHeard&>(ev)))
				{
					// The event was combined with an existing one, return. The event is
					// owned by the user, who is responsible for deleting it (Add() would have cloned it).
					return;
				}
			}
		}
	}
	else if (ev.GetEventType() == EVENT_SUSPICIOUS_ACTIVITY)
	{
		const int numEvents = GetNumEvents();
		for(int i = 0; i < numEvents; i++)
		{
			fwEvent* pExisting = GetEventByIndex(i);
			if(AssertVerify(pExisting) && pExisting->GetEventType() == EVENT_SUSPICIOUS_ACTIVITY)
			{
				if(static_cast<CEventSuspiciousActivity*>(pExisting)->TryToCombine(static_cast<const CEventSuspiciousActivity&>(ev)))
				{
					// The event was combined with an existing one, return. The event is
					// owned by the user, who is responsible for deleting it (Add() would have cloned it).
					return;
				}
			}
		}
	}
	Add(ev);
}


void CEventGroupGlobal::ProcessAndFlush()
{
	AI_EVENT_GROUP_LOCK(this)

	// Clean the queue from anything but shocking events (or similar events
	// that need to stay in the queue). Also, allow each event to do any
	// processing that may be needed (such as adding pedgen-blocking areas).
	int curr = 0;
	while (curr < GetNumEvents())
	{
		CEvent* ev = static_cast<CEvent*>(GetEventByIndex(curr));

		if(ev->ProcessInGlobalGroup())
		{
			// Leave it in the queue.
			curr++;
			continue;
		}
		Remove(*ev);
	}
}

void CEventGroupGlobal::AddEventsToPed(CPed* pPed)
{
	if(pPed->GetDeathState() == DeathState_Dead)
	{
		return;
	}

	AI_EVENT_GROUP_LOCK(this)

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* ev = static_cast<CEvent*>(GetEventByIndex(i));
		if (AssertVerify(ev) && 
			static_cast<CEvent*>(ev)->IsExposedToPed(pPed) && 
			CEvent::CanCreateEvent())
		{
			CEvent* pClone = static_cast<CEvent*>(ev->Clone());
			pPed->GetPedIntelligence()->AddEvent(*pClone);
			delete pClone;
		}
	}
}

void CEventGroupGlobal::AddEventsToVehicle(CVehicle* pVehicle)
{
	if (!pVehicle->GetIntelligence()->ShouldHandleEvents())
	{
		return;
	}

	AI_EVENT_GROUP_LOCK(this)

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* ev = static_cast<CEvent*>(GetEventByIndex(i));
		if (AssertVerify(ev) && 
			static_cast<CEvent*>(ev)->IsExposedToVehicles() && 
			CEvent::CanCreateEvent())
		{
			CEvent* pClone = static_cast<CEvent*>(ev->Clone());
			//pPed->GetPedIntelligence()->AddEvent(*pClone);
			pVehicle->GetIntelligence()->AddEvent(*pClone);
			delete pClone;
		}
	}
}

void CEventGroupGlobal::AddEventsToScriptGroup()
{
	AI_EVENT_GROUP_LOCK(this)

	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEvent* ev = static_cast<CEvent*>(GetEventByIndex(i));
		if (AssertVerify(ev) && 
			static_cast<CEvent*>(ev)->IsExposedToScripts() && 
			CEvent::CanCreateEvent())
		{
			CEvent* pClone = static_cast<CEvent*>(ev->Clone());
			GetEventScriptAIGroup()->Add(*pClone);
			delete pClone;
		}
	}
}

void CEventGroupGlobal::Shutdown()
{
	if(CEventGroupGlobal::ms_pEventGlobalGroup)
	{
		delete CEventGroupGlobal::ms_pEventGlobalGroup;
	}

	CEventGroupGlobal::ms_pEventGlobalGroup = NULL;
}

CEventGroupGlobal* GetEventGlobalGroup()
{
	if(NULL == CEventGroupGlobal::ms_pEventGlobalGroup)
	{
		CEventGroupGlobal::ms_pEventGlobalGroup = rage_new CEventGroupGlobal();
	}

	return CEventGroupGlobal::ms_pEventGlobalGroup;
}

//////////////////////////////////////////////////////////////////////////
// CEventGroupScriptErrors
//////////////////////////////////////////////////////////////////////////

CEventGroupScriptErrors* CEventGroupScriptErrors::ms_pEventScriptErrorsGroup = NULL;

void CEventGroupScriptErrors::Shutdown()
{
	if(CEventGroupScriptErrors::ms_pEventScriptErrorsGroup)
	{
		delete CEventGroupScriptErrors::ms_pEventScriptErrorsGroup;
	}

	CEventGroupScriptErrors::ms_pEventScriptErrorsGroup = NULL;
}

CEventGroupScriptErrors* GetEventScriptErrorsGroup()
{
	if(NULL == CEventGroupScriptErrors::ms_pEventScriptErrorsGroup)
	{
		CEventGroupScriptErrors::ms_pEventScriptErrorsGroup = rage_new CEventGroupScriptErrors();
	}

	return CEventGroupScriptErrors::ms_pEventScriptErrorsGroup;
}

//////////////////////////////////////////////////////////////////////////
// CEventGroupScriptAI
//////////////////////////////////////////////////////////////////////////

CEventGroupScriptAI* CEventGroupScriptAI::ms_pEventScriptAIGroup = NULL;

void CEventGroupScriptAI::Shutdown()
{
	if(CEventGroupScriptAI::ms_pEventScriptAIGroup)
	{
		delete CEventGroupScriptAI::ms_pEventScriptAIGroup;
	}

	CEventGroupScriptAI::ms_pEventScriptAIGroup = NULL;
}

CEventGroupScriptAI* GetEventScriptAIGroup()
{
	if(NULL == CEventGroupScriptAI::ms_pEventScriptAIGroup)
	{
		CEventGroupScriptAI::ms_pEventScriptAIGroup = rage_new CEventGroupScriptAI();
	}

	return CEventGroupScriptAI::ms_pEventScriptAIGroup;
}

//////////////////////////////////////////////////////////////////////////
// CEventGroupScriptNetwork
//////////////////////////////////////////////////////////////////////////

CEventGroupScriptNetwork* CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup = NULL;

void CEventGroupScriptNetwork::Shutdown()
{
	if(CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup)
	{
		delete CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup;
	}

	CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup = NULL;
}

void CEventGroupScriptNetwork::FlushAllAndMovePending()
{
	for (int i=0; i<GetNumEvents(); i++)
	{
		CEventNetwork* pNetEvent = static_cast<CEventNetwork*>(GetEventByIndex(i));
		if (AssertVerify(pNetEvent))
		{
			pNetEvent->SpewDeletionInfo();
		}
	}

	CEventGroupScript<SIZEOF_NETWORK_EVENT_GROUP>::FlushAllAndMovePending();
}

CEventGroupScriptNetwork* GetEventScriptNetworkGroup()
{
	if(NULL == CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup)
	{
		CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup = rage_new CEventGroupScriptNetwork();
	}

	return CEventGroupScriptNetwork::ms_pEventScriptNetworkGroup;
}


fwEvent* CEventGroupScriptNetwork::Add(const fwEvent& event)
{
	fwEvent* pEvent = CEventGroupScript<SIZEOF_NETWORK_EVENT_GROUP>::Add(event);

	// we want to know if this fails - consequences are very bad.
	if (!CEventNetwork::GetPool())
	{
		aiErrorf("Failed to add network event %s to queue! Pool not initialised", event.GetName().c_str());
	}
	else if (!pEvent)
	{
#if !__FINAL
		for(int i = 0; i < GetNumEvents(); i++)
		{
			CEventNetwork* ev = static_cast<CEventNetwork*>(GetEventByIndex(i));

			if (ev)
			{
				Displayf("%d : %s\n", i, ev->GetName().c_str());
				OUTPUT_ONLY(ev->SpewEventInfo());
			}
			else
			{
				Displayf("%d : -none-\n", i);
			}
		}

		aiAssertf(pEvent, "Failed to add network event %s to queue! Bailing! (Num Events:%d, Used Spaces:%d, Free Spaces:%d, Pool size:%d)", event.GetName().c_str(), GetNumEvents(), CEvent::GetPool()->GetNoOfUsedSpaces(), CEventNetwork::GetPool()->GetNoOfFreeSpaces(), CEventNetwork::GetPool()->GetPoolSize());
#endif

		// dump all events and bail from multiplayer
		if(NetworkInterface::CanBail())
		{
			CEventGroupScript<SIZEOF_NETWORK_EVENT_GROUP>::FlushAll();
			NetworkInterface::Bail(BAIL_NETWORK_ERROR, BAIL_CTX_NETWORK_ERROR_EXHAUSTED_EVENTS_POOL);
		}
#if !__FINAL
		else
		{
			// if not in multiplayer, quit
			Quitf("Failed to add network event %s to queue! Quitting! (Num Events:%d, Used Spaces:%d, Free Spaces:%d, Pool size:%d)", event.GetName().c_str(), GetNumEvents(), CEventNetwork::GetPool()->GetNoOfUsedSpaces(), CEventNetwork::GetPool()->GetNoOfFreeSpaces(), CEventNetwork::GetPool()->GetPoolSize());
		}
#endif
	}

	
	
	return pEvent;
}

fwEvent* CEventGroupScriptNetwork::AddRemote(const fwEvent& event)
{
	return CEventGroupScript<SIZEOF_NETWORK_EVENT_GROUP>::Add(event);
}

bool CEventGroupScriptNetwork::CollateJoinScriptEventForPlayer(const netPlayer& player, CEventNetworkPlayerJoinScript::eSource source, const scrThreadId threadId)
{
	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEventNetwork* ev = static_cast<CEventNetwork*>(GetEventByIndex(i));

		if (ev && ev->GetEventType() == EVENT_NETWORK_PLAYER_JOIN_SCRIPT)
		{
			CEventNetworkPlayerJoinScript* pJoinScriptEvent = static_cast<CEventNetworkPlayerJoinScript*>(ev);

			if (pJoinScriptEvent->CollateEvent(player, source, threadId))
			{
				return true;
			}
		}
	}

	return false;
}


bool CEventGroupScriptNetwork::CollateLeftScriptEventForPlayer(const netPlayer& player, CEventNetworkPlayerLeftScript::eSource source, const scrThreadId threadId)
{
	for(int i = 0; i < GetNumEvents(); i++)
	{
		CEventNetwork* ev = static_cast<CEventNetwork*>(GetEventByIndex(i));

		if (ev && ev->GetEventType() == EVENT_NETWORK_PLAYER_LEFT_SCRIPT)
		{
			CEventNetworkPlayerLeftScript* pLeftScriptEvent = static_cast<CEventNetworkPlayerLeftScript*>(ev);

			if (pLeftScriptEvent->CollateEvent(player, source, threadId))
			{
				return true;
			}
		}
	}

	return false;
}
