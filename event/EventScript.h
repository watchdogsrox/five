//
// EventScript.h - AI events that the scripts are interested in
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef EVENT_SCRIPT_H
#define EVENT_SCRIPT_H

// --- Include Files ------------------------------------------------------------
#include "scene/RegdRefTypes.h"

// Framework headers
#include "event/Event.h"
#include "event/EventPriorities.h"
#include "script/value.h"

// Structures used by script events. Duplicate structs exist in a script header. They must match as these are used to retrieve the event data.

struct sEntityId
{
	scrValue EntityId;
};

struct sVehicleId
{
	scrValue VehicleId;
};

struct sCrimeType
{
	scrValue bChangedWantedLevel;
	scrValue CrimeType;
};

struct sStatHash
{
	scrValue StatHash;
};

struct sDeadPedSeen
{
	scrValue DeadPedId;
	scrValue FindingPedId;
};

// ================================================================================================================
// CEventScriptNoData - A handy templated class to handle script events that have no associated data
// ================================================================================================================
template<int EventType>
class CEventScriptNoData : public CEvent
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
class CEventScriptWithData : public CEvent
{
public:

	CEventScriptWithData() {}

	CEventScriptWithData(const EventData& eventData) : m_EventData(eventData) {}

	virtual int GetEventType() const { return EventType; }

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventScriptWithData*>(&event));
			return (memcmp(&static_cast<const CEventScriptWithData*>(&event)->m_EventData, &m_EventData, sizeof(m_EventData)) == 0);
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
// CEventCrimeReported - triggered when a crime is reported on the local player
// ================================================================================================================
class CEventCrimeReported : public CEventScriptWithData<EVENT_CRIME_REPORTED, sCrimeType>
{
public:

	CEventCrimeReported(int CrimeType, bool bChangedWantedLevel);

	CEventCrimeReported(const sCrimeType& eventData) :
	CEventScriptWithData<EVENT_CRIME_REPORTED, sCrimeType>(eventData)
	{
	}

	virtual fwEvent* Clone() const { return rage_new CEventCrimeReported(m_EventData); }
};

// ================================================================================================================
// CEventEntityDamaged - triggered when an entity is damaged
// ================================================================================================================
class CEventEntityDamaged : public CEventScriptWithData<EVENT_ENTITY_DAMAGED, sEntityId>
{
public:

	CEventEntityDamaged(CPhysical* entity);

	CEventEntityDamaged(const sEntityId& eventData) : 
	CEventScriptWithData<EVENT_ENTITY_DAMAGED, sEntityId>(eventData)
	{
	}

	virtual fwEvent* Clone() const { return rage_new CEventEntityDamaged(m_pEntity.Get()); }

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const;

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventEntityDamaged*>(&event));
			return m_pEntity.Get() == static_cast<const CEventEntityDamaged*>(&event)->m_pEntity.Get();
		}

		return false;
	}

private: 

	RegdPhy m_pEntity;
};

// ================================================================================================================
// CEventEntityDestroyed - triggered when an entity is destroyed
// ================================================================================================================
class CEventEntityDestroyed : public CEventScriptWithData<EVENT_ENTITY_DESTROYED, sEntityId>
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
};

// ================================================================================================================
// CEventPlayerUnableToEnterVehicle - triggered when a player can't enter a vehicle
// ================================================================================================================
class CEventPlayerUnableToEnterVehicle : public CEventScriptWithData<EVENT_PLAYER_UNABLE_TO_ENTER_VEHICLE, sVehicleId>
{
public:

	CEventPlayerUnableToEnterVehicle(CVehicle& vehicle);

	CEventPlayerUnableToEnterVehicle(const sVehicleId& eventData) : 
	CEventScriptWithData<EVENT_PLAYER_UNABLE_TO_ENTER_VEHICLE, sVehicleId>(eventData)
	{
	}

	virtual fwEvent* Clone() const { return rage_new CEventPlayerUnableToEnterVehicle(m_EventData); }
};

// ================================================================================================================
// CEventPedSeenDeadPed - triggered when a ped has seen a dead ped
// ================================================================================================================
class CEventPedSeenDeadPed : public CEventScriptWithData<EVENT_PED_SEEN_DEAD_PED, sDeadPedSeen>
{
public:

	CEventPedSeenDeadPed(CPed& deadPed, CPed& findingPed);
	CEventPedSeenDeadPed(CPhysical* deadEntity, CPhysical* findingEntity);

	CEventPedSeenDeadPed(const sDeadPedSeen& eventData) : 
	CEventScriptWithData<EVENT_PED_SEEN_DEAD_PED, sDeadPedSeen>(eventData)
	{
	}

	virtual fwEvent* Clone() const { return rage_new CEventPedSeenDeadPed(m_deadEntity.Get(), m_findingEntity.Get()); }

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const;

	virtual bool operator==(const fwEvent& event) const
	{
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventPedSeenDeadPed*>(&event));
			return m_deadEntity.Get() == static_cast<const CEventPedSeenDeadPed*>(&event)->m_deadEntity.Get() &&
				m_findingEntity.Get() == static_cast<const CEventPedSeenDeadPed*>(&event)->m_findingEntity.Get();
		}

		return false;
	}

private: 

	RegdPhy m_deadEntity;
	RegdPhy m_findingEntity;
};

// ================================================================================================================
// CEventStatChangedValue - triggered when a stat has changed its value
// ================================================================================================================
class CEventStatChangedValue : public CEventScriptWithData<EVENT_STAT_VALUE_CHANGED, sStatHash>
{
public:
	CEventStatChangedValue(const int statHash) {m_EventData.StatHash.Int = statHash;}
	CEventStatChangedValue(const sStatHash& eventData) 
		: CEventScriptWithData<EVENT_STAT_VALUE_CHANGED, sStatHash>(eventData) {;}

	virtual fwEvent* Clone() const {return rage_new CEventStatChangedValue(m_EventData);}

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventStatChangedValue*>(&event));
			return m_EventData.StatHash == static_cast<const CEventStatChangedValue*>(&event)->m_EventData.StatHash;
		}

		return false;
	}
};

#endif // EVENT_SCRIPT_H

// eof

