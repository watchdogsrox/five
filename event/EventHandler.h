#ifndef INC_EVENT_HANDLER_H
#define INC_EVENT_HANDLER_H

// Framework headers
#include "fwevent/eventregdref.h"

// Game headers
#include "Event/Event.h"
#include "Event/EventResponse.h"

// Forward declarations
class CEvent;
class CPed;
class CTask;

// Keep a record of events and tasks to allow events to be stored until tasks can be 
// aborted and to allow non-temp events to be stored until temp events have been dealt with
class CEventHandlerHistory
{
public:

    CEventHandlerHistory()
	: m_pAbortedTask(NULL)
	, m_pCurrentEventNonTemp(NULL)
	, m_pCurrentEventTemp(NULL)
	, m_pStoredEventNonTemp(NULL)
	, m_pCurrentMovementEvent(NULL)
    {
    }
    
	~CEventHandlerHistory()
	{
		Flush();
	}
    
    void Flush();
    
    // Get/Set the task that is being aborted to make way for a new event response task
    CTask* GetAbortedTask() const { return m_pAbortedTask; }
    void SetAbortedTask(CTask* pAbortedTask) { m_pAbortedTask = pAbortedTask; }
    
    // Get/Set the event that the ped is currently responding to
	CEvent* GetCurrentEvent() const;
    CEvent* GetCurrentEventActive() const { return m_pCurrentEventNonTemp; }
    CEvent* GetCurrentEventPassive() const { return m_pCurrentEventTemp; }
    void SetCurrentEvent(const CEvent& rCurrentEvent, const bool bOverrideResponseAsNonTemp);
	void SetMovementEvent(const CEvent& rCurrentEvent);

	// Get priority of current event
	int GetCurrentEventPriority() const;

	// Clear all events
    void ClearAllEvents();
    void ClearTempEvent();
    void ClearNonTempEvent();
	void ClearStoredEvent();
	void ClearMovementEvent();

	// Clears the ragdoll response from active events
	void ClearRagdollResponse();
	
	// Test if current event is of specific type
	bool IsRespondingToEvent(const int iEventType) const;

	// Test if a new event takes priority over the current event
    bool TakesPriorityOverCurrentEvent(const CEvent& newEvent, const bool bOverrideResponseAsNonTemp);

	// Get non-temp event that is being stored until temp event is handled
	CEvent* GetStoredActiveEvent() const { return m_pStoredEventNonTemp; }
	// Test if the temp event is finished being handled and restore the non-temp event 
	// as the current event
	void TickStoredEvent();

#if __DEV
	void Print() const;
#endif //__DEV
       
private:

	// Store non-temp event when it is usurped by a temp event
	void StoreNonTempEvent();

	//
	// Members
	//

	// The task that is being aborted to allow a new event response task
    RegdTask  m_pAbortedTask;

	// Current event that is non-temp (we want to remember this event if it is replaced by a temp event)
	// A non-temp might be a damage event or a hate event
    RegdEvent m_pCurrentEventNonTemp;

	// Current event that is temp
	// A temp event is usually an event that helps the ped navigate around the map
    RegdEvent m_pCurrentEventTemp;

	// Current non-temp event that is stored
    RegdEvent m_pStoredEventNonTemp;

	// Used by the movement event handler 
	RegdEvent m_pCurrentMovementEvent;
};

// Handle CPed events and produce a CTask response
class CEventHandler
{
public:

    CEventHandler(CPed* pPed);
	~CEventHandler() {}

	// Called from CPedIntelligence to compute an event response and give it to the relevant ped
    void HandleEvents();

	CTask* AllowTasksToHandleEvents( CTask* pTask );

	// Queries to the history
    int GetCurrentEventType() const;
    CEvent* GetCurrentEvent() const { return m_history.GetCurrentEvent(); }
    bool IsRespondingToEvent(const int iEventType) const { return m_history.IsRespondingToEvent(iEventType); }

	// Reset the history
	void ResetHistory() { m_history.ClearAllEvents(); }

	// Clear everything out
    void Flush();
    void FlushImmediately();

	// Access the event response
	const CEventResponse& GetResponse() const { return m_response; }
	CEventResponse& GetResponse() { return m_response; }

	// Record current response event
	void OverrideCurrentEventResponseEvent(CEvent& event) { m_history.SetCurrentEvent(event, false); }

	// Call this before computing a new event response task
    void ResetResponse() { m_response.ClearAllEventResponses(); }
  
	// Clears the ragdoll response from active events
	void ClearRagdollResponse() { m_history.ClearRagdollResponse(); }


#if __DEV
	// Debug printing for the event queue, etc
	void Print() const;
#endif //__DEV

protected:

	// Test the event type and call the appropriate function to work out the event response task
	void ComputeEventResponseTask(CEvent* pEvent);

	// Test all the task pointers and give non-null tasks to the ped
	void SetEventResponseTask(CEvent& event);

	// Deletes any already calculated responses
	void DeleteResponse() { m_response.ClearAllEventResponses(); }

	//
	// Members
	//

	// The ped to be given response tasks as a response to the ped's highest priority event
	CPed* m_pPed;

	// The history of recent events and tasks that need to be remembered
	CEventHandlerHistory m_history;

	// The response to the current event
	CEventResponse m_response;
};

////////////////////////////////////////////////////////////////////////////////

class CEventBeingProcessed
{
public:

	CEventBeingProcessed(CPed* pPed, CEvent* pEvent);
	~CEventBeingProcessed();

	void SetEvent(CEvent* pEvent);
	CEvent* GetEvent() { return m_pEvent; }

	void Clear();

private:

#if __ASSERT
	void SetEventDeletionGuard(bool b);
#endif // __ASSERT

	ASSERT_ONLY(CPed* m_pPed);
	CEvent* m_pEvent;
};

#endif // INC_EVENT_HANDLER_H
