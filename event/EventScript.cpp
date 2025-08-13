//
// scriptEventTypes.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------

#include "event/EventScript.h"
#include "peds/ped.h"

#include "script/script.h"

CEventEntityDamaged::CEventEntityDamaged(CPhysical* entity) :
m_pEntity(entity)
{
	m_EventData.EntityId.Int = -1; // this is not assigned until the data is retrieved from the event. This is to avoid generating too many script GUIDs
}

bool CEventEntityDamaged::RetrieveData(u8* data, const unsigned sizeOfData) const
{
	if (!m_pEntity)
	{
		return false;
	}

	// only assign a script GUID when the script is interested in this event
	m_EventData.EntityId.Int = CTheScripts::GetGUIDFromEntity(*m_pEntity);

	return CEventScriptWithData<EVENT_ENTITY_DAMAGED, sEntityId>::RetrieveData(data, sizeOfData);
}

CEventEntityDestroyed::CEventEntityDestroyed(CPhysical* entity) :
m_pEntity(entity)
{
	m_EventData.EntityId.Int = -1; // this is not assigned until the data is retrieved from the event. This is to avoid generating too many script GUIDs
}

bool CEventEntityDestroyed::RetrieveData(u8* data, const unsigned sizeOfData) const
{
	if (!m_pEntity)
	{
		return false;
	}

	// only assign a script GUID when the script is interested in this event
	m_EventData.EntityId.Int = CTheScripts::GetGUIDFromEntity(*m_pEntity);

	return CEventScriptWithData<EVENT_ENTITY_DESTROYED, sEntityId>::RetrieveData(data, sizeOfData);
}

CEventCrimeReported::CEventCrimeReported(int CrimeType, bool bChangedWantedLevel)
{
	m_EventData.CrimeType.Int = CrimeType;
	m_EventData.bChangedWantedLevel.Int = bChangedWantedLevel ? 1 : 0;
}

CEventPlayerUnableToEnterVehicle::CEventPlayerUnableToEnterVehicle(CVehicle& entity) 
{
	m_EventData.VehicleId.Int = CTheScripts::GetGUIDFromEntity(entity);
}

CEventPedSeenDeadPed::CEventPedSeenDeadPed(CPed& deadPed, CPed& findingPed)
{
	m_deadEntity = (CPhysical*)&deadPed;
	m_findingEntity = (CPhysical*)&findingPed;
}

CEventPedSeenDeadPed::CEventPedSeenDeadPed(CPhysical* deadEntity, CPhysical* findingEntity)
{
	m_deadEntity = deadEntity;
	m_findingEntity = findingEntity;
}

bool CEventPedSeenDeadPed::RetrieveData(u8* data, const unsigned sizeOfData) const
{
	if (!m_deadEntity || !m_findingEntity)
	{
		return false;
	}

	// only assign a script GUID when the script is interested in this event
	m_EventData.DeadPedId.Int = CTheScripts::GetGUIDFromEntity(*m_deadEntity);
	m_EventData.FindingPedId.Int = CTheScripts::GetGUIDFromEntity(*m_findingEntity);

	return CEventScriptWithData<EVENT_PED_SEEN_DEAD_PED, sDeadPedSeen>::RetrieveData(data, sizeOfData);
}
