#ifndef INC_EVENT_RESPONSE_H
#define INC_EVENT_RESPONSE_H

// Game headers
#include "Task/System/Task.h"

class CEventResponse
{
public:

	// The types of task responses we can have to events
	enum ResponseTaskTypes
	{
		EventResponse = 0,
		PhysicalResponse,
		MovementResponse,

		// Max responses
		MaxResponseTaskTypes,
	};

	CEventResponse()
		: m_bOverrideResponseAsNonTemp(false)
		, m_bRunOnSecondary(false)
	{
		for(int i = 0; i < MaxResponseTaskTypes; i++)
		{
			m_pResponseTasks[i] = NULL;
		}
	}

	~CEventResponse()
	{
		ClearAllEventResponses();
	}

	// Get/Set an event response
	aiTask* GetEventResponse(ResponseTaskTypes type) const { return m_pResponseTasks[type]; }
	void    SetEventResponse(ResponseTaskTypes type, aiTask* pResponseTask)
	{
#if __ASSERT
		if(type != MovementResponse && pResponseTask != NULL)
		{
			Assertf(!((CTask*)pResponseTask)->IsMoveTask(), "Movement task is being added as non-movement event response task! (please update url:bugstar:2145209 with this callstack)");
		}
#endif
		m_pResponseTasks[type] = pResponseTask;
	}

	// Delete all stored responses
	void ClearEventResponse(ResponseTaskTypes type)                       { delete m_pResponseTasks[type]; m_pResponseTasks[type] = NULL; }
	void ClearAllEventResponses()
	{
		for(int i = 0; i < MaxResponseTaskTypes; i++)
		{
			ClearEventResponse(static_cast<ResponseTaskTypes>(i));
		}

		m_bOverrideResponseAsNonTemp = false;
		m_bRunOnSecondary = false;
	}

	// Returns the specified response type and clears our reference to it
	aiTask* ReleaseEventResponse(ResponseTaskTypes type) { aiTask* pResponseTask = m_pResponseTasks[type]; m_pResponseTasks[type] = NULL; return pResponseTask; }

	// Return true if we have any responses
	bool HasEventResponse() const
	{
		for(int i = 0; i < MaxResponseTaskTypes; i++)
		{
			if(GetEventResponse(static_cast<ResponseTaskTypes>(i)))
			{
				return true;
			}
		}

		return false;
	}

	// Get/Set temp event override
	bool GetOverrideResponseAsNonTemp() const { return m_bOverrideResponseAsNonTemp; }
	void SetOverrideResponseAsNonTemp(bool bOverrideResponseAsNonTemp) { m_bOverrideResponseAsNonTemp = bOverrideResponseAsNonTemp; }

	bool GetRunOnSecondary() const { return m_bRunOnSecondary; }
	void SetRunOnSecondary(bool b) { m_bRunOnSecondary = b; }

#if __BANK
	atString ToString() const
	{
		static const u32 uBUFFER_SIZE = 1024;
		char szBuffer[uBUFFER_SIZE];

		snprintf(szBuffer, uBUFFER_SIZE, "Event response: %s, Physical response: %s, Movement response: %s, Override as non temp: %s, Run on secondary: %s", 
			m_pResponseTasks[EventResponse] ? m_pResponseTasks[EventResponse]->GetName() : "none", 
			m_pResponseTasks[PhysicalResponse] ? m_pResponseTasks[PhysicalResponse]->GetName() : "none", 
			m_pResponseTasks[MovementResponse] ? m_pResponseTasks[MovementResponse]->GetName() : "none", 
			m_bOverrideResponseAsNonTemp ? "yes" : "no",
			m_bRunOnSecondary ? "yes" : "no"
		);

		return atString(szBuffer);
	}
#endif // __BANK

#if __DEV
	void Print() const
	{
		taskDisplayf("Event response: %s", m_pResponseTasks[EventResponse] ? m_pResponseTasks[EventResponse]->GetName() : "none");
		taskDisplayf("Physical response: %s", m_pResponseTasks[PhysicalResponse] ? m_pResponseTasks[PhysicalResponse]->GetName() : "none");
		taskDisplayf("Movement response: %s", m_pResponseTasks[MovementResponse] ? m_pResponseTasks[MovementResponse]->GetName() : "none");
		taskDisplayf("Override as non temp: %s", m_bOverrideResponseAsNonTemp ? "yes" : "no");
		taskDisplayf("Run on secondary: %s", m_bRunOnSecondary ? "yes" : "no");
	}
#endif //__DEV

private:

	// The tasks that are computed as event response tasks and given to peds
	RegdaiTask m_pResponseTasks[MaxResponseTaskTypes];

	// A temp event response can be overridden to be non-temp
	bool m_bOverrideResponseAsNonTemp;

	bool m_bRunOnSecondary;
};

#endif // INC_EVENT_RESPONSE_H
