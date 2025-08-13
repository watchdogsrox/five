/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedShockingEvents.h
// PURPOSE :	A class to hold the list of events that are 'shocking' the ped
//				Pulled out of the old CPedBase class.
// AUTHOR :		Mike Currington.
// CREATED :	28/10/10
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_PEDSHOCKINGEVENTS_H
#define INC_PEDSHOCKINGEVENTS_H

// rage includes
#include "atl/queue.h"

// game includes
#include "Event\ShockingEventEnums.h"

class CPedShockingEvent
{
public:
	CPedShockingEvent();

	// PURPOSE:	Records that the ped has responded to a given shocking event.
	void SetShockingEventReactedTo(u32 uiEventID);

	// PURPOSE:	Resets the shocking events memory array.
	void ClearShockingEventsMemory()
	{
		m_ShockingEventsMemory.Reset();
	}

	// PURPOSE:	Checks to see if event Id has been recorded already.
	//			This function will return true if event exists in "memory".
	bool HasReactedToShockingEvent(u32 uiEventID) const
	{	
		return m_ShockingEventsMemory.Find(uiEventID);
	}

protected:
	static const int kShockingEventsMemorySize = 5;

	atQueue<u32, kShockingEventsMemorySize>		m_ShockingEventsMemory;
};

#endif // INC_PEDBASE_H
