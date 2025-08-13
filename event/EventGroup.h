#ifndef INC_EVENT_GROUP_H
#define INC_EVENT_GROUP_H

// Rage headers
#include "atl/queue.h"
#include "Vector/Vector3.h"

// Framework headers
#include "fwEvent/EventQueue.h"

// Game headers
#include "Event/Event.h"
#include "Event/EventNetwork.h"
#include "Network/NetworkInterface.h"
#include "Scene/RegdRefTypes.h"
#include "Script/script.h"
#include "vehicleAi/task/DeferredTasks.h"

// Forward declarations
class CEvent;
class CPed;

#define SIZEOF_PED_EVENT_GROUP 16
#define SIZEOF_VEHICLE_EVENT_GROUP 4
#define SIZEOF_GLOBAL_EVENT_GROUP 32
#define SIZEOF_SCRIPT_EVENT_GROUP 32
#define SIZEOF_NETWORK_EVENT_GROUP CEventNetwork::MAX_NETWORK_EVENTS 

//////////////////////////////////////////////////////////////////////////
// CEventGroupAI 
//////////////////////////////////////////////////////////////////////////
template<int SizeOfQueue, typename Parent = void>
class CEventGroupAI : public fwEventQueue<SizeOfQueue, Parent>
{
public:

	virtual const char* GetEventGroupName() = 0;

	fwEvent* Add(const fwEvent& event)
	{
        NetworkInterface::OnAIEventAdded(event);

		fwEvent* pNewEvent = NULL;

		if (CEvent::CanCreateEvent())
		{
			pNewEvent = fwEventQueue<SizeOfQueue, Parent>::Add(event);

#if !__FINAL
			if (pNewEvent)
			{
				aiAssertf(CEventNames::EventNameExists(static_cast<eEventType>(pNewEvent->GetEventType())), "Event (%d) doesn't have a definition in EventNames!", pNewEvent->GetEventType());
			}
			else
			{
				static u32 s_LastTimeDisplayed = 0;
				const u32 currentTime = fwTimer::GetTimeInMilliseconds();
				if(currentTime - s_LastTimeDisplayed > 1000)	// Limit spew to once per second.
				{
					aiDisplayf("Failed to add event %s to event group %s", event.GetName().c_str(), GetEventGroupName());
					s_LastTimeDisplayed = currentTime;
				}
			}
#endif //!__FINAL
		}

		return pNewEvent;
	}

};

//////////////////////////////////////////////////////////////////////////
// CEventGroupScript
//////////////////////////////////////////////////////////////////////////

template <int SizeOfQueue>
class CEventGroupScript : public CEventGroupAI<SizeOfQueue>
{
public:

	virtual const char* GetEventGroupName() = 0;
	virtual bool CanCreateEvent() { return CEvent::CanCreateEvent(); }

	virtual fwEvent* Add(const fwEvent& event)
	{
		fwEvent* pEvent = NULL;

		if (CanCreateEvent())
		{
			// add events to the pending queue if the scripts are currently being processed
			if (CTheScripts::GetUpdatingTheThreads())
			{
				pEvent = m_pendingQueue.Add(event);

				if (!pEvent)
				{
					aiDisplayf("Failed to add event to pending queue for event group %s", GetEventGroupName());
				}
			}
			else
			{
				pEvent = fwEventQueue<SizeOfQueue>::Add(event);

				if (!pEvent)
				{
					aiDisplayf("Failed to add event to event group %s", GetEventGroupName());
				}
			}
		}

		return pEvent;
	}

	void FlushAll()
	{
		fwEventQueue<SizeOfQueue>::FlushAll();
		m_pendingQueue.FlushAll();
	}

	virtual void FlushAllAndMovePending()
	{
		fwEventQueue<SizeOfQueue>::FlushAll();

		// move events from the pending queue to the main queue
		for(int i = 0; i < m_pendingQueue.GetNumEvents(); i++)
		{
			if (AssertVerify(m_pendingQueue.GetEventByIndex(i)))
			{
				fwEvent* pClone = m_pendingQueue.GetEventByIndex(i)->Clone();
				Add(*pClone);
				delete pClone;
			}
		}

		m_pendingQueue.FlushAll();
	}

private:

	// this is used for events triggered during the processing of the scripts. They are added to the main queue after the script processing is done.
	fwEventQueue<SizeOfQueue> m_pendingQueue;
};

//////////////////////////////////////////////////////////////////////////
// CEventGroupBase
//////////////////////////////////////////////////////////////////////////

class CLockingEventGroupBase
{
public:
	void Lock() const
	{
		if (aiDeferredTasks::g_Running)
		{
			aiDeferredTasks::g_EventGroupToken.Lock();
			ASSERT_ONLY(++m_LockCount;)
		}
	}

	void Unlock() const
	{
		if (aiDeferredTasks::g_Running)
		{
			aiDeferredTasks::g_EventGroupToken.Unlock();
#if __ASSERT
			Assert(m_LockCount > 0);
			--m_LockCount;
#endif // __ASSERT
		}
	}

	void EnterProtectedSection() const
	{
		// Not locking here enforces thread safe event group access while iterating over events, using event pointers, etc.
		Assertf(!aiDeferredTasks::g_Running || m_LockCount > 0, "Locking event group access not using AI_EVENT_GROUP_LOCK?");
	}

	void ExitProtectedSection() const {}

private:
#if __ASSERT
	static __THREAD u32 m_LockCount;
#endif // __ASSERT
};

class CEventGroupCriticalSection
{
public:
	CEventGroupCriticalSection(const CLockingEventGroupBase* pEventGroup) : m_pEventGroup(pEventGroup)
	{
		pEventGroup->Lock();
	}

	~CEventGroupCriticalSection()
	{
		m_pEventGroup->Unlock();
	}

private:
	const CLockingEventGroupBase* m_pEventGroup;
};

#define AI_EVENT_GROUP_LOCK(group) CEventGroupCriticalSection cs(group);

//////////////////////////////////////////////////////////////////////////
// CEventGroupPed
//////////////////////////////////////////////////////////////////////////

// This class is really just an array of events experienced by a ped
// per frame.  The events can be queried by type and priority.  As 
// events get added to the array, the relevant ped is queried to decide
// whether the event should be accepted or rejected.
class CEventGroupPed : public CEventGroupAI<SIZEOF_PED_EVENT_GROUP, CLockingEventGroupBase>
{
public:

	CEventGroupPed(CPed* pPed);

	virtual const char* GetEventGroupName() { return "PED_AI"; }

	virtual fwEvent* Add(const fwEvent& event);

	virtual fwEvent* GetLowestPriorityEventThatCanBeRemoved() const;

	void UpdateCommunicationEvents(CPed* pPed);

	bool HasScriptCommandOfTaskType(const int iTaskType) const;

	bool HasRagdollEvent() const;
	bool HasPairedDamageEvent() const;

	void RemoveInvalidEvents(bool bEverythingButScriptEvents = false, bool bFlagForDeletion = false);

	//
	// Event querying
	//

	CEvent* GetHighestPriorityEvent() const;

protected:

	// Owner
	CPed* m_pPed;

private:

	// Searches the array and returns the event that is physically closest to the given ped.
	CEvent* ReturnClosestEventToPed(const CPed* pPed, const atFixedArray<CEvent*, SIZEOF_PED_EVENT_GROUP> &aEvents) const;
};

class CEventGroupVehicle : public CEventGroupAI<SIZEOF_VEHICLE_EVENT_GROUP, CLockingEventGroupBase>
{
public:

	CEventGroupVehicle(CVehicle* pVehicle);

	virtual const char* GetEventGroupName() { return "VEHICLE_AI"; }

	virtual fwEvent* Add(const fwEvent& event);

	CEvent* GetHighestPriorityEvent() const;
protected:

	CVehicle* m_pVehicle;

private:

	CEvent* ReturnClosestEventToVehicle(const CVehicle* pPed, const atFixedArray<CEvent*, SIZEOF_VEHICLE_EVENT_GROUP> &aEvents) const;

};

//////////////////////////////////////////////////////////////////////////
// CEventGroupGlobal
//////////////////////////////////////////////////////////////////////////

// This class contains a list of events per frame that might
// affect all peds.  Every event in the list is passed on to 
// each ped to individually reject/accept.  Events like explosions,
// weather events, fear, etc, might get added to the global group.
class CEventGroupGlobal : public CEventGroupAI<SIZEOF_GLOBAL_EVENT_GROUP, CLockingEventGroupBase>
{
public:

	friend CEventGroupGlobal* GetEventGlobalGroup();

	virtual const char* GetEventGroupName() { return "GLOBAL_AI"; }

	virtual fwEvent* Add(const fwEvent& event);

	void AddOrCombine(const CEvent& ev);

	void ProcessAndFlush();

	void AddEventsToPed(CPed* pPed);
	void AddEventsToVehicle(CVehicle* pVehicle);
	void AddEventsToScriptGroup();
	void Shutdown();

private:

	// Only one instance can be constructed through GetEventGlobalGroup
	CEventGroupGlobal() {}

	// Instance
	static CEventGroupGlobal* ms_pEventGlobalGroup;
};

// Forward declaration (can't just use the friend declaration above)
CEventGroupGlobal* GetEventGlobalGroup();

//////////////////////////////////////////////////////////////////////////
// CEventGroupScriptErrors
//////////////////////////////////////////////////////////////////////////

// This class contains a list of AI script events per frame 
class CEventGroupScriptErrors : public CEventGroupScript<SIZEOF_SCRIPT_EVENT_GROUP>
{
public:

	virtual const char* GetEventGroupName() { return "SCRIPT_ERRORS"; }

	friend CEventGroupScriptErrors* GetEventScriptErrorsGroup();

	void Shutdown();

private:

	// Only one instance can be constructed through GetEventScriptGroup
	CEventGroupScriptErrors() {}

	// Instance
	static CEventGroupScriptErrors* ms_pEventScriptErrorsGroup;
};

// Forward declaration (can't just use the friend declaration above)
CEventGroupScriptErrors* GetEventScriptErrorsGroup();

//////////////////////////////////////////////////////////////////////////
// CEventGroupScriptAI
//////////////////////////////////////////////////////////////////////////

// This class contains a list of AI script events per frame 
class CEventGroupScriptAI : public CEventGroupScript<SIZEOF_SCRIPT_EVENT_GROUP>
{
public:

	virtual const char* GetEventGroupName() { return "SCRIPT_AI"; }

	friend CEventGroupScriptAI* GetEventScriptAIGroup();

	void Shutdown();

private:

	// Only one instance can be constructed through GetEventScriptGroup
	CEventGroupScriptAI() {}

	// Instance
	static CEventGroupScriptAI* ms_pEventScriptAIGroup;
};

// Forward declaration (can't just use the friend declaration above)
CEventGroupScriptAI* GetEventScriptAIGroup();

//////////////////////////////////////////////////////////////////////////
// CEventGroupScriptNetwork
//////////////////////////////////////////////////////////////////////////

// This class contains a list of network events per frame 
class CEventGroupScriptNetwork : public CEventGroupScript<SIZEOF_NETWORK_EVENT_GROUP>
{
public:

	virtual const char* GetEventGroupName() { return "SCRIPT_NETWORK"; }
	virtual bool CanCreateEvent() { return CEventNetwork::CanCreateEvent(); }

	friend CEventGroupScriptNetwork* GetEventScriptNetworkGroup();

	void Shutdown();

	virtual void FlushAllAndMovePending();

	virtual fwEvent* Add(const fwEvent& event);
	virtual fwEvent* AddRemote(const fwEvent& event);

	bool CollateJoinScriptEventForPlayer(const netPlayer& player, CEventNetworkPlayerJoinScript::eSource, const scrThreadId threadId);
	bool CollateLeftScriptEventForPlayer(const netPlayer& player, CEventNetworkPlayerLeftScript::eSource, const scrThreadId threadId);

private:

	// Only one instance can be constructed through GetEventNetworkGroup
	CEventGroupScriptNetwork() {}

	// Instance
	static CEventGroupScriptNetwork* ms_pEventScriptNetworkGroup;
};

// Forward declaration (can't just use the friend declaration above)
CEventGroupScriptNetwork* GetEventScriptNetworkGroup();

#endif // INC_EVENT_GROUP_H
