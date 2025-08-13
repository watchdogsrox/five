// File header
#include "Event/Decision/EventDecisionMaker.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "Event/Decision/EventDecisionMakerManager.h"
#include "Event/EventEditable.h"
#include "Event/System/EventData.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerModifiableComponent
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CEventDecisionMakerModifiableComponent, CEventDecisionMakerModifiableComponent::MAX_STORAGE, atHashString("CEventDecisionMakerModifiableComponent",0x5fa721b3));

CEventDecisionMakerModifiableComponent::CEventDecisionMakerModifiableComponent()
{
#if !__FINAL
	m_name[0] = '\0';
#endif // !__FINAL
}

CEventDecisionMakerModifiableComponent::CEventDecisionMakerModifiableComponent(const CEventDecisionMakerModifiableComponent& other)
{
	s32 i;
	for(i = 0; i < other.m_responses.GetCount(); i++)
	{
		aiFatalAssertf(other.m_responses[i], "Response is NULL");
		m_responses.Push(rage_new CEventDecisionMakerResponseDynamic(*other.m_responses[i]));
	}

	for(i = 0; i < other.m_blockedEvents.GetCount(); i++)
	{
		m_blockedEvents.Push(other.m_blockedEvents[i]);
	}
}

CEventDecisionMakerModifiableComponent::~CEventDecisionMakerModifiableComponent()
{
	ClearAllResponses();
}

void CEventDecisionMakerModifiableComponent::MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, CEventResponseTheDecision &theDecision) const
{
	theDecision.Reset();
	for(s32 i = 0; i < m_responses.GetCount() && !theDecision.HasDecision(); i++)
	{
		m_responses[i]->MakeDecision(pPed, pEvent, eventSourceType, theDecision);
	}
}

void CEventDecisionMakerModifiableComponent::AddResponse(eEventType eventType, const CEventDataResponse_Decision& decision)
{
	CEventDecisionMakerResponseDynamic* pResponse = NULL;

	// Check if there already exists a response for this event type
	for(s32 i = 0; i < m_responses.GetCount(); i++)
	{
		if(m_responses[i]->GetEventType() == eventType)
		{
			// Found a matching event type - use this response
			pResponse = m_responses[i];
			break;
		}
	}

	// No response found, so try to allocate one
	if(!pResponse)
	{
		if(aiVerifyf(!m_responses.IsFull(), "Dynamic responses array is full to capacity [%d]", m_responses.GetMaxCount()))
		{
			pResponse = rage_new CEventDecisionMakerResponseDynamic(eventType);
			m_responses.Push(pResponse);
		}
	}

	if(pResponse)
	{
		pResponse->AddDecision(decision);
	}
}

void CEventDecisionMakerModifiableComponent::ClearResponse(eEventType eventType)
{
	for(s32 i = 0; i < m_responses.GetCount(); i++)
	{
		if(m_responses[i]->GetEventType() == eventType)
		{
			delete m_responses[i];
			m_responses.DeleteFast(i);
		}
	}
}

void CEventDecisionMakerModifiableComponent::ClearAllResponses()
{
	for(s32 i = 0; i < m_responses.GetCount(); i++)
	{
		delete m_responses[i];
	}

	m_responses.Reset();
}

void CEventDecisionMakerModifiableComponent::BlockEvent(eEventType eventType)
{
	if(!GetIsEventBlocked(eventType))
	{
		// Add the event to the blockedEvents list
		if(aiVerifyf(!m_blockedEvents.IsFull(), "Dynamic blocked events array is full to capacity [%d], eventType [%s] not blocked", m_blockedEvents.GetMaxCount(), CEventNames::GetEventName(eventType)))
		{
			m_blockedEvents.Push(eventType);
		}
	}
}

void CEventDecisionMakerModifiableComponent::UnblockEvent(eEventType eventType)
{
	for(s32 i = 0; i < m_blockedEvents.GetCount(); i++)
	{
		if(m_blockedEvents[i] == eventType)
		{
			m_blockedEvents.DeleteFast(i);
			return;
		}
	}
}

bool CEventDecisionMakerModifiableComponent::GetIsEventBlocked(eEventType eventType) const
{
	for(s32 i = 0; i < m_blockedEvents.GetCount(); i++)
	{
		if(m_blockedEvents[i] == eventType)
		{
			// Event blocked
			return true;
		}
	}

	// Event not blocked
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMaker
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CEventDecisionMaker, CEventDecisionMaker::MAX_STORAGE, atHashString("CEventDecisionMaker",0x65087004));

CEventDecisionMaker::CEventDecisionMaker(u32 uId, const CEventDataDecisionMaker* pData)
: m_uId(uId)
, m_pDecisionMakerData(pData)
, m_pDecisionMakerComponent(NULL)
#if !__FINAL
, m_pName(m_pDecisionMakerData ? m_pDecisionMakerData->GetName().GetCStr() : "null")
#endif // !__FINAL
{
}

CEventDecisionMaker::~CEventDecisionMaker()
{
	DestroyComponent();
}

void CEventDecisionMaker::MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, CEventResponseTheDecision &theDecision ) const
{
	aiFatalAssertf(pPed,   "pPed is NULL");
	aiFatalAssertf(pEvent, "pEvent is NULL");

	// Event response
	theDecision.Reset();

	if(!m_pDecisionMakerComponent || !m_pDecisionMakerComponent->GetIsEventBlocked(static_cast<eEventType>(pEvent->GetEventType())))
	{
		// Get the source type of the event
		EventSourceType eventSourceType = GetEventSourceType(pPed, pEvent);

		// Check any local decision data
		if(m_pDecisionMakerComponent)
		{
			m_pDecisionMakerComponent->MakeDecision(pPed, pEvent, eventSourceType, theDecision);
		}

		// Check any metadata decision data if no local decision has been found
		if( !theDecision.HasDecision() )
		{
			MakeDecisionFromMetadata(pPed, pEvent, eventSourceType, m_pDecisionMakerData, theDecision);
		}
	}

}

CEventDecisionMaker* CEventDecisionMaker::Clone(const char* pNewName) const
{
	CEventDecisionMaker* pClone = rage_new CEventDecisionMaker(GetId(), m_pDecisionMakerData);
	aiFatalAssertf(pClone, "Failed to clone CEventDecisionMaker");

	// Assign the new Id
	pClone->m_uId = atStringHash(pNewName);

	// Copy any component data
	if(m_pDecisionMakerComponent)
	{
		pClone->m_pDecisionMakerComponent = rage_new CEventDecisionMakerModifiableComponent(*m_pDecisionMakerComponent);
	}
	else
	{
		// As this is a clone, allocate a modifiable component
		pClone->CreateComponent();
	}

#if !__FINAL
	if(pClone->m_pDecisionMakerComponent)
	{
		// Store the new name for debugging purposes
		pClone->m_pDecisionMakerComponent->SetName(pNewName);
	}
#endif // !__FINAL

	return pClone;
}

void CEventDecisionMaker::AddResponse(eEventType eventType, const CEventDataResponse_Decision& decision)
{
	// Allocate storage if necessary
	if(CreateComponent())
	{
		m_pDecisionMakerComponent->AddResponse(eventType, decision);
	}
}

void CEventDecisionMaker::BlockEvent(eEventType eventType)
{
	// Allocate storage if necessary
	if(CreateComponent())
	{
		m_pDecisionMakerComponent->BlockEvent(eventType);
	}
}

EventSourceType CEventDecisionMaker::GetEventSourceType(const CPed* pPed, const CEventEditableResponse* pEvent) const
{
	const CEntity* pSourceEntity = pEvent->GetSourceEntity();
	if(pSourceEntity)
	{
		if(pSourceEntity->GetType() == ENTITY_TYPE_PED)
		{
			const CPed* pSourcePed = static_cast<const CPed*>(pSourceEntity);

			if(pPed->GetPedIntelligence()->IsThreatenedBy(*pSourcePed))
			{
				return EVENT_SOURCE_THREAT;
			}
			else if(pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed))
			{
				return EVENT_SOURCE_FRIEND;
			}
			else if(pSourcePed->IsPlayer())
			{
				return EVENT_SOURCE_PLAYER;
			}
		}
	}

	return EVENT_SOURCE_UNDEFINED;
}

void CEventDecisionMaker::MakeDecisionFromMetadata(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, const CEventDataDecisionMaker* pDecisionMakerData, CEventResponseTheDecision &theDecision ) const
{
	theDecision.Reset();
	if(pDecisionMakerData)
	{
		eEventType eventType = (eEventType)pEvent->GetEventType();

		// Use binary search to find where in the sorted array the responses for the current event
		// are potentially stored.
		int index = pDecisionMakerData->FindFirstPossibleCachedResponseForEventType(eventType);
		if(index >= 0)
		{
			// Loop through the array until we find responses belong to another event type, or
			// we reach the end of the array. In most cases, we would only access one or two elements of the array.
			const int numCachedResponses = pDecisionMakerData->GetNumCachedResponses();
			while(index < numCachedResponses)
			{
				if(pDecisionMakerData->GetCachedSortedEventTypeForResponse(index) != eventType)
				{
					// Found some other event, we can break since the array is sorted.
					break;
				}

				// Get the CEventDecisionMakerResponse object for the response, and use it to make the decision.
				const CEventDecisionMakerResponse* pResponse = pDecisionMakerData->GetCachedSortedEventResponse(index);
				if(pResponse)
				{
					pResponse->MakeDecision(pPed, pEvent, eventSourceType, theDecision);
					if ( theDecision.HasDecision() )
					{
						return;
					}
				}

				index++;
			}
		}

		// Get the parent decision-maker.
		const CEventDataDecisionMaker* pParentDecisionMaker = pDecisionMakerData->GetCachedDecisionMakerParent();

		// Check for NULL to avoid function call overhead - this is a fairly common case since we
		// don't have very deep hierarchies of decision-makers.
		if(pParentDecisionMaker)
		{
			MakeDecisionFromMetadata(pPed, pEvent, eventSourceType, pParentDecisionMaker, theDecision);
		}
	}
}

bool CEventDecisionMaker::CreateComponent()
{
	// Allocate storage if necessary
	if(!m_pDecisionMakerComponent)
	{
		m_pDecisionMakerComponent = rage_new CEventDecisionMakerModifiableComponent;
	}

	aiAssertf(m_pDecisionMakerComponent, "Dynamic component failed to allocate");
	return m_pDecisionMakerComponent != NULL;
}

void CEventDecisionMaker::DestroyComponent()
{
	delete m_pDecisionMakerComponent;
	m_pDecisionMakerComponent = NULL;
}

// Iterate over each possible response to see if the specified event type is supported
bool CEventDecisionMaker::HandlesEventOfType(int iEventType) const
{
	if (m_pDecisionMakerData)
	{
		// Like in MakeDecisionFromMetadata(), make use of a binary search to see if there is a response.
		int index = m_pDecisionMakerData->FindFirstPossibleCachedResponseForEventType((eEventType)iEventType);
		if(index >= 0)
		{
			// Unlike for MakeDecisionFromMetadata(), it should be enough to just look at the first element.
			const int numCachedResponses = m_pDecisionMakerData->GetNumCachedResponses();
			if(index < numCachedResponses && m_pDecisionMakerData->GetCachedSortedEventTypeForResponse(index) == iEventType)
			{
				return true;
			}
		}

		// Note: to really be correct, this should probably call to the parent decision-maker,
		// like we do in CEventDecisionMaker::MakeDecisionFromMetadata().
	}
	return false;
}
