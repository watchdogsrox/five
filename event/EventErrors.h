//
// EventErrors.h - script error events
//
// Copyright (C) 1999-2017 Rockstar Games.  All Rights Reserved.
//

#ifndef EVENT_ERRORS_H
#define EVENT_ERRORS_H

// --- Include Files ------------------------------------------------------------
#include "scene/RegdRefTypes.h"

// Framework headers
#include "event/Event.h"
#include "event/EventPriorities.h"
#include "script/value.h"



// Structures used by script events. Duplicate structs exist in a script header. They must match as these are used to retrieve the event data.

struct sErrorScriptID
{
	union{
		u64			m_unused;					// because sizeof(codeStruct) != sizeof(scriptStruct) otherwise!
		u32			m_scriptNameHash;
	};
};


// ================================================================================================================
// CEventScriptNoData - A handy templated class to handle script events that have no associated data
// ================================================================================================================
template<int EventType>
class CEventErrorNoData : public CEvent
{
public:

	virtual int GetEventType() const { return EventType; }

	virtual bool operator==(const fwEvent& event) const 
	{
		return (event.GetEventType() == this->GetEventType());
	}

	virtual bool RetrieveData(u8* UNUSED_PARAM(data), const unsigned UNUSED_PARAM(sizeOfData)) const
	{
		Assertf(0, "%s: This event has no data to retrieve", GetName().c_str());
		return false;
	}

	// currently, all script events are only processed by the scripts
	virtual bool IsExposedToPed(CPed *UNUSED_PARAM(pPed)) const { return false; } 

	virtual bool IsExposedToScripts() const { return true; }

};

// ================================================================================================================
// CEventScriptWithData - A handy templated class to handle script events with an associated struct that is retrieved by the scripts
// ================================================================================================================
template<int EventType, class EventData>
class CEventErrorWithData : public CEvent
{
public:

	CEventErrorWithData() {}

	CEventErrorWithData(const EventData& eventData) : m_EventData(eventData) {}

	virtual int GetEventType() const { return EventType; }

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventErrorWithData*>(&event));
			return (memcmp(&static_cast<const CEventErrorWithData*>(&event)->m_EventData, &m_EventData, sizeof(m_EventData)) == 0);
		}

		return false;
	}

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const
	{ 
		if (AssertVerify(data) && 
			Verifyf(sizeOfData == sizeof(m_EventData), "%s: Size of struct is wrong. Size \"%u\" should be \"%" SIZETFMT "u\".", GetName().c_str(), sizeOfData, sizeof(m_EventData)))
		{
			sysMemCpy(data, &m_EventData, sizeof(m_EventData));
			return true;
		}

		return false;	
	}

	// currently, all script events are only processed by the scripts
	virtual bool IsExposedToPed(CPed *UNUSED_PARAM(pPed)) const { return false; }

	virtual bool IsExposedToScripts() const { return true; }

	virtual int GetEventPriority() const { return E_PRIORITY_SCRIPT_WITH_DATA; }

protected:

	mutable EventData m_EventData;
};

// ================================================================================================================
// CEventErrorOverflow - triggered when a script stack overflows
// ================================================================================================================
class CEventErrorUnknownError : public CEventErrorWithData<EVENT_ERRORS_UNKNOWN_ERROR, sErrorScriptID>
{
public:

	CEventErrorUnknownError(void);

	CEventErrorUnknownError(const sErrorScriptID& eventData) : 
		CEventErrorWithData<EVENT_ERRORS_UNKNOWN_ERROR, sErrorScriptID>(eventData)
	{
	}

	virtual fwEvent* Clone() const {  return rage_new CEventErrorUnknownError(m_EventData); }
};

// ================================================================================================================
// CEventErrorOverflow - triggered when a script stack overflows
// ================================================================================================================
class CEventErrorStackOverflow : public CEventErrorWithData<EVENT_ERRORS_STACK_OVERFLOW, sErrorScriptID>
{
public:

	CEventErrorStackOverflow(void);

	CEventErrorStackOverflow(const sErrorScriptID& eventData) : 
		CEventErrorWithData<EVENT_ERRORS_STACK_OVERFLOW, sErrorScriptID>(eventData)
	{
	}

	virtual fwEvent* Clone() const {  return rage_new CEventErrorStackOverflow(m_EventData); }
};

// ================================================================================================================
// CEventErrorOverflow - triggered when a script stack overflows
// ================================================================================================================
class CEventErrorInstructionLimit : public CEventErrorWithData<EVENT_ERRORS_INSTRUCTION_LIMIT, sErrorScriptID>
{
public:

	CEventErrorInstructionLimit(void);

	CEventErrorInstructionLimit(const sErrorScriptID& eventData) : 
		CEventErrorWithData<EVENT_ERRORS_INSTRUCTION_LIMIT, sErrorScriptID>(eventData)
	{
	}

	virtual fwEvent* Clone() const {  return rage_new CEventErrorInstructionLimit(m_EventData); }
};

// ================================================================================================================
// CEventErrorOverflow - triggered when a script stack overflows
// ================================================================================================================
class CEventErrorArrayOverflow : public CEventErrorWithData<EVENT_ERRORS_ARRAY_OVERFLOW, sErrorScriptID>
{
public:

	CEventErrorArrayOverflow(void);

	CEventErrorArrayOverflow(const sErrorScriptID& eventData) : 
		CEventErrorWithData<EVENT_ERRORS_ARRAY_OVERFLOW, sErrorScriptID>(eventData)
	{
	}

	virtual fwEvent* Clone() const {  return rage_new CEventErrorArrayOverflow(m_EventData); }
};

// ================================================================================================================
// CEventEntityDestroyed - triggered when an entity is destroyed
// ================================================================================================================
/*class CEventEntityDestroyed : public CEventScriptWithData<EVENT_ENTITY_DESTROYED, sEntityId>
{
public:

	CEventEntityDestroyed(CPhysical* entity);

	CEventEntityDestroyed(const sEntityId& eventData) : 
		CEventScriptWithData<EVENT_ENTITY_DESTROYED, sEntityId>(eventData)
	{
	}

	virtual fwEvent* Clone() const { return rage_new CEventEntityDestroyed(m_pEntity.Get()); }

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const;

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventEntityDestroyed*>(&event));
			return m_pEntity.Get() == static_cast<const CEventEntityDestroyed*>(&event)->m_pEntity.Get();
		}

		return false;
	}

private: 

	RegdPhy m_pEntity;
};*/



#endif // EVENT_ERRORS_H

// eof

