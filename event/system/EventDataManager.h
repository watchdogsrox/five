#ifndef INC_EVENT_DATA_MANAGER_H
#define INC_EVENT_DATA_MANAGER_H

// Rage headers
#include "atl/array.h"
#include "parser/macros.h"
#include "EventData.h"

class CEventDataResponseTask;
class CEventDecisionMakerResponse;
class CEventDataDecisionMaker;


//////////////////////////////////////////////////////////////////////////
// CEventDataManager
//////////////////////////////////////////////////////////////////////////

class CEventDataManager
{
public:
	// Constructor
	CEventDataManager();

	// Initialise
	static void Init(unsigned initMode);

	// Shutdown
	static void Shutdown(unsigned shutdownMode);

	// Override events using specified file
	static void Override(const char* fileName);

	// Revert to original events
	static void Revert();

	// Access to the static manager
	static CEventDataManager& GetInstance() { return ms_instance; }


	// Find decision maker data given it's hash
	const CEventDataDecisionMaker * GetDecisionMaker(const u32 hash) const;

	// Find decision maker response given it's hash
	const CEventDecisionMakerResponse * GetDecisionMakerResponse(const u32 hash) const;

	// Find a response task given it's hash
	const CEventDataResponseTask * GetEventResponseTask(const u32 hash) const;

	// Get the name of an event type
#if !__NO_OUTPUT
	static const char * GetEventTypeName(eEventType event);
#endif

private:
	//
	// Pargen (xml) support
	bool LoadXmlMeta(const char * const pFilename);
#if __DEV
	void SaveXmlMeta(const char * const pFilename);
#endif // __DEV

private:
	//
	// Members
	//
	rage::atArray<CEventDataDecisionMaker*>			m_eventDecisionMaker;
	rage::atArray<CEventDecisionMakerResponse*>		m_eventDecisionMakerResponseData;
	rage::atArray<CEventDataResponseTask*>			m_eventResponseTaskData;

	// Static manager object
	static CEventDataManager ms_instance;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_EVENT_DATA_MANAGER_H
