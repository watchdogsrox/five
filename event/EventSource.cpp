// Game headers
#include "Event/Events.h"
#include "Event/EventSource.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventSource
//////////////////////////////////////////////////////////////////////////

int CEventSource::ComputeEventSourceType(const CEvent& event, const CPed& pedActedOnByEvent)
{
	int iSourceType = EVENT_SOURCE_UNDEFINED;

	const CEntity* pEntityCreatedEvent = event.GetSourceEntity();
	if(NULL == pEntityCreatedEvent)
	{
		iSourceType = EVENT_SOURCE_UNDEFINED;
	}
	else if(pEntityCreatedEvent->GetType() == ENTITY_TYPE_PED)
	{
		const CPed* pPedCreatedEvent = static_cast<const CPed*>(pEntityCreatedEvent);

		const CPedIntelligence* pPedIntelligence = pedActedOnByEvent.GetPedIntelligence();

		if(pPedIntelligence->IsThreatenedBy(*pPedCreatedEvent))
		{
			iSourceType = EVENT_SOURCE_THREAT;
		}
		else if(pPedIntelligence->IsFriendlyWith(*pPedCreatedEvent))
		{
			iSourceType = EVENT_SOURCE_FRIEND;
		}
		else if(pPedCreatedEvent->IsPlayer())
		{
			iSourceType = EVENT_SOURCE_PLAYER;
		}
	}
	else
	{
		iSourceType = EVENT_SOURCE_UNDEFINED;
	}

	return iSourceType;
}
