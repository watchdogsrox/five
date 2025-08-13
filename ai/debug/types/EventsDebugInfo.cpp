#include "ai\debug\types\EventsDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "Peds\PedIntelligence.h"

bool CEventsDebugInfo::ValidateInput()
{
	if (!m_Ped || !m_Ped->GetPedIntelligence())
		return false;

	return true;
}

CEventsDebugInfo::CEventsDebugInfo(const CPed* pPed, fwFlags32 uLogFlags, const DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_uLogFlags(uLogFlags)
	, m_Ped(pPed)
{

}

void CEventsDebugInfo::PrintCurrentEvents()
{
#if __DEV
	const CPedIntelligence* pPedIntelligence = m_Ped->GetPedIntelligence();
	aiAssert(pPedIntelligence);

	ColorPrintLn(Color_green, "Current Events (%d)", pPedIntelligence->CountEventGroupEvents());
	PushIndent();

	for( s32 i = 0; i < pPedIntelligence->CountEventGroupEvents(); i++ )
	{
		CEvent* pEvent = pPedIntelligence->GetEventByIndex(i);
		ColorPrintLn(Color_green, "%s", (const char*)pEvent->GetName());
	}

	PopIndent();
#endif // __DEV
}

void CEventsDebugInfo::PrintEventsHistory()
{
#if DEBUG_EVENT_HISTORY

	ColorPrintLn(Color_blue, "Events History");
	PushIndent();

	CPedIntelligence* pPedIntelligence = m_Ped->GetPedIntelligence();
	if (pPedIntelligence)
	{
		const u32 uEventHistoryCount = pPedIntelligence->GetEventHistoryCount();
		for (u32 uEventIdx = 0; uEventIdx < uEventHistoryCount; ++uEventIdx)
		{
			const CPedIntelligence::SEventHistoryEntry& rEntry = pPedIntelligence->GetEventHistoryEntryAt(uEventIdx);
			const CPedIntelligence::SEventHistoryEntry::SState::Enum eState = rEntry.GetState();

			// Discard filtered events
			if (!m_uLogFlags.IsFlagSet(LF_EVENTS_HISTORY_SHOW_REMOVED_EVENTS) && (eState == CPedIntelligence::SEventHistoryEntry::SState::REMOVED))
			{
				continue;
			}

			Color32 stateColor = GetEventStateColor(eState);

			// Print Event info
			ColorPrintLn(stateColor, "%s", GetEventHistoryEntryDescription(*pPedIntelligence, rEntry).c_str());

			// Print State
			if (eState != CPedIntelligence::SEventHistoryEntry::SState::CREATED)
			{
				PushIndent();
					ColorPrintLn(stateColor, "|=> %s", GetEventHistoryEntryStateDescription(rEntry, m_uLogFlags).c_str());
				PopIndent();
			}
		}
	}

	PopIndent();

#endif // DEBUG_EVENT_HISTORY
}

void CEventsDebugInfo::PrintDecisionMaker()
{
	ColorPrintLn(Color_blue, "Decision Maker");
	PushIndent();

	CPedIntelligence* pPedIntelligence = m_Ped->GetPedIntelligence();
	if (pPedIntelligence)
	{
		const CPedEventDecisionMaker& rPedDecisionMaker = pPedIntelligence->GetPedDecisionMaker();

		const u32 uNumResponses = rPedDecisionMaker.GetNumResponses();
		for (u32 uResponseIdx = 0; uResponseIdx < uNumResponses; ++uResponseIdx)
		{
			const CPedEventDecisionMaker::SResponse& rResponse = rPedDecisionMaker.GetResponseAt(uResponseIdx);

			// Print response info
			const float fCooldownTimeLeft = rResponse.GetCooldownTimeLeft();
			const float fCooldownDistance = rResponse.Cooldown.MaxDistance > 0.0f ? Dist(m_Ped->GetTransform().GetPosition(), rResponse.vEventPosition).Getf() : 0.0f;
			ColorPrintLn(Color_blue, "%s, Cooldown [TL: %.2f, Distance: %.2f/%.2f]", CEventNames::GetEventName(rResponse.eventType), fCooldownTimeLeft, fCooldownDistance, rResponse.Cooldown.MaxDistance);
		}
	}

	PopIndent();
}

void CEventsDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("EVENTS");
	PushIndent();
		PushIndent();
			PushIndent();
				if (m_uLogFlags.IsFlagSet(LF_PED_ID))
				{
					ColorPrintLn(Color_orange, "%s", AILogging::GetEntityDescription(m_Ped).c_str());
				}
				if (m_uLogFlags.IsFlagSet(LF_CURRENT_EVENTS))
				{
					PrintCurrentEvents();
				}
				if (m_uLogFlags.IsFlagSet(LF_DECISION_MAKER))
				{
					PrintDecisionMaker();
				}
				if (m_uLogFlags.IsFlagSet(LF_EVENTS_HISTORY))
				{
					PrintEventsHistory();
				}
}

#if DEBUG_EVENT_HISTORY

atString CEventsDebugInfo::GetEventHistoryEntryDescription(const CPedIntelligence& rPedIntelligence, const CPedIntelligence::SEventHistoryEntry& rEntry)
{
	static const u32 uBUFFER_MAX_SIZE = 1024;
	static char szBuffer[uBUFFER_MAX_SIZE];
	
	const char* pszPreffix = rEntry.pEvent && rPedIntelligence.GetCurrentEvent() && rEntry.pEvent == rPedIntelligence.GetCurrentEvent() ? "C: " : "";
	//const char* pszReady = rEntry.bProcessedReady ? "T" : "F";
	const char* pszValidForPed = rEntry.bProcesedValidForPed ? "T" : "F";
	//const char* pszAffectsPed = rEntry.bProcessedAffectsPed ? "T" : "F";
	const char* pszCalculatedResponse = rEntry.bProcessedCalculatedResponse ? "T" : "F";
	const char* pszWillRespond = rEntry.bProcessedWillRespond ? "T" : "F";
	snprintf(szBuffer, uBUFFER_MAX_SIZE, "%s%d, %s, S: %s, P:%d(%s%s%s), H:%d, D:%4.2f/%4.2f", pszPreffix, rEntry.uCreatedTS, rEntry.sEventDescription.c_str(), rEntry.sSourceDescription.c_str(), 
		rEntry.uProcessedTS, pszValidForPed, pszCalculatedResponse, pszWillRespond,
		rEntry.uSelectedAsHighestTS, rEntry.GetDelayTimerLast(), rEntry.GetDelayTimerMax());
	return atString(szBuffer);
}

atString CEventsDebugInfo::GetEventHistoryEntryStateDescription(const CPedIntelligence::SEventHistoryEntry& rEntry, const fwFlags32 uLogFlags)
{
	// Calculate state description string
	atString sStateDesc("");

	const CPedIntelligence::SEventHistoryEntry::SState::Enum eState = rEntry.GetState();

	static const u32 uBUFFER_MAX_SIZE = 1024;
	static char szBuffer[uBUFFER_MAX_SIZE];

	snprintf(szBuffer, uBUFFER_MAX_SIZE, "%d, %s", rEntry.uModifiedTS, CPedIntelligence::SEventHistoryEntry::SState::GetName(eState));

	sStateDesc = szBuffer;

	if (eState == CPedIntelligence::SEventHistoryEntry::SState::ACCEPTED && (uLogFlags.IsFlagSet(LF_EVENTS_HISTORY_TASKS_ALL) || uLogFlags.IsFlagSet(LF_EVENTS_HISTORY_TASKS_TRANSITIONS)))
	{
		for (u32 uTaskTreeIdx = 0; uTaskTreeIdx < PED_TASK_TREE_MAX; ++uTaskTreeIdx)
		{
			if (rEntry.eBeforeAcceptedActiveTaskType[uTaskTreeIdx] == rEntry.eAfterAcceptedActiveTaskType[uTaskTreeIdx])
			{
				//if (uLogFlags.IsFlagSet(LF_EVENTS_HISTORY_TASKS_ALL))
				{
					snprintf(szBuffer, uBUFFER_MAX_SIZE, ", %s"
						, TASKCLASSINFOMGR.GetTaskName(rEntry.eBeforeAcceptedActiveTaskType[uTaskTreeIdx]));
					sStateDesc += szBuffer;
				}
			}
			else
			{
				snprintf(szBuffer, uBUFFER_MAX_SIZE, ", %s->%s"
					, TASKCLASSINFOMGR.GetTaskName(rEntry.eBeforeAcceptedActiveTaskType[uTaskTreeIdx])
					, TASKCLASSINFOMGR.GetTaskName(rEntry.eAfterAcceptedActiveTaskType[uTaskTreeIdx]));
				sStateDesc += szBuffer;
			}
		}
	}

	return sStateDesc;
}

Color32 CEventsDebugInfo::GetEventStateColor(const CPedIntelligence::SEventHistoryEntry::SState::Enum eState)
{
	// Calculate state color
	Color32 stateColor = Color_blue;
	switch (eState)
	{
	case CPedIntelligence::SEventHistoryEntry::SState::DISCARDED_CANNOT_INTERRUPT_SAME_EVENT:
	case CPedIntelligence::SEventHistoryEntry::SState::DISCARDED_PRIORITY:
		stateColor = Color_red;
		break;

	case CPedIntelligence::SEventHistoryEntry::SState::TRYING_TO_ABORT_TASK:
		stateColor = Color_yellow;
		break;

	case CPedIntelligence::SEventHistoryEntry::SState::ACCEPTED:
		stateColor = Color_green;
		break;

	case CPedIntelligence::SEventHistoryEntry::SState::REMOVED:
		stateColor = Color_grey;
		break;

	case CPedIntelligence::SEventHistoryEntry::SState::EXPIRED:
		stateColor = Color_LightGray;
		break;

	case CPedIntelligence::SEventHistoryEntry::SState::DELETED:
		stateColor = Color_orange;
		break;

	default:
		stateColor = Color_blue;
		break;
	}

	return stateColor;
}

#endif // DEBUG_EVENT_HISTORY

#endif // AI_DEBUG_OUTPUT_ENABLED


