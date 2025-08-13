#ifndef EVENT_NAMES_H
#define EVENT_NAMES_H

//
// name:		EventNames.h
// description:	Class that contains name and size information for every event type.
//				Every event must have an entry or it will assert on construction
//

// Game headers
#include "Event/System/EventData.h"

#if !__NO_OUTPUT

class CEventName
{
public:
	CEventName() { m_pName = NULL; m_iEventSize = 0;}
	const char* m_pName;
	s32 m_iEventSize;
};	

class CEventNames
{
public:
	static void			AddEventNames();
	static const char*	GetEventName(eEventType eventType);
	static void			PrintEventNamesAndSizes();
	static bool			EventNameExists(eEventType eventType);

private:
	
	static CEventName ms_EventNames[NUM_EVENTTYPE];
	static bool ms_bEventsCounted;
};

#endif // !__NO_OUTPUT

#endif //EVENT_NAMES_H
