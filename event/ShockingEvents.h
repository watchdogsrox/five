#ifndef SHOCKING_EVENTS_H
#define SHOCKING_EVENTS_H


////////////////////////////////////////////////////////////////////////
//
//	CShockingEvents
//
//	Maintains a list of Shocking things happening around the player,
//	as driven by the EventHandler and the AI.  The idea is that the
//	ped-control code can then use this information to pick out things
//	to look at, run away from, and generally react to.
//
////////////////////////////////////////////////////////////////////////

// rage headers
#include "fwutil/Flags.h"

// game headers
#include "event/EventShocking.h"
#include "event/ShockingEventEnums.h"
#include "event/system/EventData.h"

class CPed;

namespace rage
{
	class fwEvent;
}

// static class
class CShockingEventsManager 
{
public:
	static u32 Add(CEventShocking& ev, bool createdByScript = false, float overrideLifeTime = -1.0f);

	//clear all events - for busted and dead
	static void ClearAllEvents(bool clearNonScriptCreated = true, bool clearScriptCreated = true);
	static void ClearEventsInSphere(Vec3V_In sphereCenter, const float& sphereRadius, bool clearNonScriptCreated = true, bool clearScriptCreated = true);
	static void ClearAllEventsWithSourceEntity(const CEntity* pSourceEntity);
	static void ClearAllBlockingAreas();

	static void ClearAllEventsCB()
	{	ClearAllEvents();	}

	// PURPOSE:  Turn off all shocking event processing completely.
	static void SetDisableShockingEvents(bool bDisabled) { ms_bDisableEvents = bDisabled; }

	// PURPOSE:  Set if new shocking events  of a given type can be added to the global event queue.
	static void SetSuppressingShockingEvents(bool bSuppress, eSEventShockLevels iLevel)
	{
		ms_SuppressedShockingEventPriorities.ChangeFlag(BIT(iLevel), bSuppress);
	}

	// PURPOSE:  Helper to suppress all shocking events.
	static void SuppressAllShockingEvents()
	{
		ms_SuppressedShockingEventPriorities.SetAllFlags();
	}

	// PURPOSE:  Stop suppressing the creation of new shocking events.
	// This should be done immediately before the scripts run.
	static void ProcessPreScripts()
	{
		ms_SuppressedShockingEventPriorities.ClearAllFlags();
	}

	// PURPOSE:  Accessor for determining if a particular shocking event is suppressed.
	static bool IsShockingEventSuppressed(const CEventShocking& event)
	{
		return ms_SuppressedShockingEventPriorities.GetAllFlags() ? 
			ms_SuppressedShockingEventPriorities.IsFlagSet(BIT(event.GetTunables().m_Priority)) : false;
	}

private:
	static bool	EventAlreadyExists(CEventShocking& event);

public:
#if __BANK
	static void RenderShockingEvents();
	static bool RENDER_VECTOR_MAP;
	static bool RENDER_AS_LIST;
	static bool RENDER_ON_3D_SCREEN;
	static bool RENDER_EVENT_EFFECT_AREA;

	static bool RENDER_SHOCK_EVENT_LEVEL_INTERESTING;
	static bool RENDER_SHOCK_EVENT_LEVEL_AFFECTSOTHERS;
	static bool RENDER_SHOCK_EVENT_LEVEL_POTENTIALLYDANGEROUS;
	static bool RENDER_SHOCK_EVENT_LEVEL_SERIOUSDANGER;
#endif

private:

	// whether to do any of this processing	
	static bool			ms_bDisableEvents;
	
	// Shocking events can be disabled on a per-priority basis through script.
	static fwFlags8		ms_SuppressedShockingEventPriorities;
};

#endif //#ifndef SHOCKING_EVENTS
