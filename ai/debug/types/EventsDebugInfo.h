#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"
#include "Peds\PedIntelligence.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CEventsDebugInfo : public CAIDebugInfo
{
public:

	enum ELogFlags
	{
		LF_PED_ID							  = BIT0,
		LF_CURRENT_EVENTS					  = BIT1,
		LF_DECISION_MAKER					  = BIT2,
		LF_EVENTS_HISTORY					  = BIT3,
		LF_EVENTS_HISTORY_TASKS_ALL			  = BIT4,
		LF_EVENTS_HISTORY_TASKS_TRANSITIONS	  = BIT5,
		LF_EVENTS_HISTORY_SHOW_REMOVED_EVENTS = BIT6,
		//...
		LF_EVENTS_HISTORY_ALL = LF_PED_ID | LF_EVENTS_HISTORY | LF_EVENTS_HISTORY_TASKS_ALL | LF_EVENTS_HISTORY_SHOW_REMOVED_EVENTS,
		LF_ALL = 0xFFFFFFFF
	};

	CEventsDebugInfo(const CPed* pPed
		, fwFlags32 uLogFlags = LF_PED_ID | LF_CURRENT_EVENTS | LF_DECISION_MAKER| LF_EVENTS_HISTORY | LF_EVENTS_HISTORY_TASKS_TRANSITIONS | LF_EVENTS_HISTORY_SHOW_REMOVED_EVENTS
		, const DebugInfoPrintParams printParams = DebugInfoPrintParams());

	virtual ~CEventsDebugInfo() {}

	virtual void Print();;

	// Utility functions
#if DEBUG_EVENT_HISTORY
	static atString GetEventHistoryEntryDescription(const CPedIntelligence& rPedIntelligence, const CPedIntelligence::SEventHistoryEntry& rEntry);
	static atString GetEventHistoryEntryStateDescription(const CPedIntelligence::SEventHistoryEntry& rEntry, fwFlags32 uLogFlags);
	static Color32  GetEventStateColor(const CPedIntelligence::SEventHistoryEntry::SState::Enum eState);
#endif // DEBUG_EVENT_HISTORY

private:

	bool ValidateInput();

	void PrintCurrentEvents();
	void PrintEventsHistory();
	void PrintDecisionMaker();

	RegdConstPed m_Ped;
	fwFlags32 m_uLogFlags;
};

#endif // AI_DEBUG_OUTPUT_ENABLED