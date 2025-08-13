/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedShockingEvents.h
// PURPOSE :	A class to hold the list of events that are 'shocking' the ped
//				Pulled out of the old CPedBase class.
// AUTHOR :		Mike Currington.
// CREATED :	28/10/10
/////////////////////////////////////////////////////////////////////////////////
#include "Peds/PedShockingEvents.h"

// rage includes
#include "fwmaths/random.h"
#include "fwsys/timer.h"

// game includes
#include "event/ShockingEvents.h"

using namespace rage;

AI_OPTIMISATIONS()

CPedShockingEvent::CPedShockingEvent()
{
	ClearShockingEventsMemory();
}


void CPedShockingEvent::SetShockingEventReactedTo(u32 uiEventID)
{
	// Hopefully we can get away without checking this in non-dev builds,
	// as the user probably has made the check already.
	Assert(!m_ShockingEventsMemory.Find(uiEventID));

	if(m_ShockingEventsMemory.IsFull())
	{
		m_ShockingEventsMemory.Drop();
	}
	m_ShockingEventsMemory.Push(uiEventID);
}
