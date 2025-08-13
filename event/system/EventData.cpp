// File header
#include "Event/System/EventData.h"

// Game headers
#include "Event/Decision/EventDecisionMakerManager.h"
#include "Event/System/EventDataManager.h"
#include "Event/System/EventDataMeta_parser.h"

#include "ai/aichannel.h"

#include <algorithm>	// std::sort(), std::lower_bound()

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

CompileTimeAssert(parser_eEventType_Count==eEVENTTYPE_MAX+2);	// +2 is because the first enum value is -1 (EVENT_INVALID), and then we count ourselves also 

namespace
{
	// PURPOSE:	Temporary struct used for sorting some array data.
	struct CEventDataTempSortStruct
	{
		bool operator<(const CEventDataTempSortStruct& other) const
		{
			if(m_EventType < other.m_EventType)
			{
				return true;
			}
			if(m_EventType > other.m_EventType)
			{
				return false;
			}
			return m_OriginalOrder < other.m_OriginalOrder;
		}

		u16									m_EventType;
		u16									m_OriginalOrder;
		const CEventDecisionMakerResponse*	m_Response;
	};

}	// anon namespace


void CEventDataDecisionMaker::PostLoad()
{
	int eventResponseCount = GetNumEventResponses();

	// Build an array of CEventDataTempSortStruct objects.
	atRangeArray<CEventDataTempSortStruct, MAX_EVENTRESPONSES> sortArray;
	int numInSortArray = 0;
	for(s32 i = 0; i < eventResponseCount; i++)
	{
		const CEventDecisionMakerResponse* pResponse = CEventDataManager::GetInstance().GetDecisionMakerResponse(GetEventResponse(i).GetHash());

		if(aiVerifyf(pResponse, "Failed to find decision-maker response %s.", GetEventResponse(i).GetCStr()))
		{
			eEventType eventType = pResponse->GetEventType();
			aiAssert(eventType != EVENT_INVALID);
			aiAssert(eventType >= 0 && (int)eventType <= 0xffff && (u16)eventType != (u16)EVENT_INVALID);
			sortArray[numInSortArray].m_EventType = (u16)eventType;
			sortArray[numInSortArray].m_OriginalOrder = (u16)i;
			sortArray[numInSortArray].m_Response = pResponse;
			numInSortArray++;
		}
	}

	// Sort the array based on the event type.
	// Note that we use m_OriginalOrder as a tie-breaker, which means that if there are multiple responses
	// to the same event type, their order is preserved and the result should be the same as before.
	if(numInSortArray > 0)
	{
		std::sort(&sortArray[0], &sortArray[0] + numInSortArray);
	}

	// Build the parallel arrays m_CachedSortedEventResponse and m_CachedSortedEventTypeForResponse,
	// based on the sorted data.

	m_CachedSortedEventResponse.Reset();
	m_CachedSortedEventResponse.Resize(numInSortArray);

	m_CachedSortedEventTypeForResponse.Reset();
	m_CachedSortedEventTypeForResponse.Resize(numInSortArray);

	for(int i = 0; i < numInSortArray; i++)
	{
		m_CachedSortedEventResponse[i] = sortArray[i].m_Response;
		m_CachedSortedEventTypeForResponse[i] = sortArray[i].m_EventType;
	}

	// Look up the parent decision-maker.
	u32 parentHash = GetDecisionMakerParentRef();
	m_CachedDecisionMakerParent = CEventDataManager::GetInstance().GetDecisionMaker(parentHash);
	aiAssertf(parentHash == 0 || m_CachedDecisionMakerParent,
			"Failed to find decision-maker %s as the parent of %s.",
			m_Name.GetCStr(), DecisionMakerParentRef.GetCStr());
}


int CEventDataDecisionMaker::FindFirstPossibleCachedResponseForEventType(eEventType eventType) const
{
	const int numCachedResponses = m_CachedSortedEventTypeForResponse.GetCount();
	if(numCachedResponses > 0 && eventType != EVENT_INVALID)
	{
		// m_CachedSortedEventTypeForResponse is a sorted array, and we use
		// std::lower_bound() to do a binary search here.
		const u16* start = &m_CachedSortedEventTypeForResponse[0];
		const u16* end = start + numCachedResponses;
		const u16 val = (u16)eventType;
		const u16* found = std::lower_bound(start, end, val);

		// Compute the index from the pointer.
		const int index = (int)(found - start);

		// lower_bound should have given us an element within the array,
		// or immediately after. Note that the element we have the index to
		// here might not contain the event type we are looking for.
		aiAssert(index >= 0 && index <= numCachedResponses);

		return index;
	}
	return -1;
}