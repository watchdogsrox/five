#ifndef INC_EVENT_LEADER_H
#define INC_EVENT_LEADER_H

// Game headers
#include "Event/Event.h"
#include "Event/EventPriorities.h"
#include "scene/RegdRefTypes.h"

//////////////////////////////////////////////////////////////////////////
// CEventLeaderEnteredCarAsDriver
//////////////////////////////////////////////////////////////////////////

// Leader got in car
class CEventLeaderEnteredCarAsDriver : public CEvent
{
public:

	CEventLeaderEnteredCarAsDriver();
	virtual ~CEventLeaderEnteredCarAsDriver();

	virtual CEvent* Clone() const { return rage_new CEventLeaderEnteredCarAsDriver(); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_LEADER_ENTERED_CAR_AS_DRIVER; }
	virtual int GetEventPriority() const { return E_PRIORITY_LEADER_ENTERED_CAR_AS_DRIVER; }

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;

	// Can this event be interrupted by an event of the same type?
	// Usually an event won't replace one of the same type
	virtual bool CanBeInterruptedBySameEvent() const { return true; }

	//
	//
	//

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	//
	// 
	//

#if !__FINAL
	// Debug function to return event name as a string for error output
	virtual atString GetName() const { return atString("LeaderEnteredCarAsDriver"); }
#endif

protected:

	virtual bool TakesPriorityOver(const CEvent& otherEvent) const { return otherEvent.GetEventType() != EVENT_IN_WATER; }
};

//////////////////////////////////////////////////////////////////////////
// CEventLeaderExitedCarAsDriver
//////////////////////////////////////////////////////////////////////////

// Leader got out car
class CEventLeaderExitedCarAsDriver : public CEvent
{
public:
	CEventLeaderExitedCarAsDriver()	
	{
#if !__FINAL
		m_EventType = EVENT_LEADER_EXITED_CAR_AS_DRIVER;
#endif
	}

	virtual ~CEventLeaderExitedCarAsDriver() {}

	virtual CEvent* Clone() const { return rage_new CEventLeaderExitedCarAsDriver(); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_LEADER_EXITED_CAR_AS_DRIVER; }
	virtual int GetEventPriority() const { return E_PRIORITY_LEADER_EXITED_CAR_AS_DRIVER; }

	//
	//
	//

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;


#if !__FINAL
	// Debug function to return event name as a string for error output
	virtual atString GetName() const {return atString("LeaderExitedCarAsDriver");}
#endif

};

//////////////////////////////////////////////////////////////////////////
// CEventLeaderHolsteredWeapon
//////////////////////////////////////////////////////////////////////////

// Leader put their gun away
class CEventLeaderHolsteredWeapon : public CEvent
{
public:

	CEventLeaderHolsteredWeapon()
	{
#if !__FINAL
		m_EventType = EVENT_LEADER_HOLSTERED_WEAPON;
#endif
	}

	virtual ~CEventLeaderHolsteredWeapon() {}

	virtual CEvent* Clone() const { return rage_new CEventLeaderHolsteredWeapon(); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_LEADER_HOLSTERED_WEAPON; }
	virtual int GetEventPriority() const { return E_PRIORITY_LEADER_HOLSTERED_WEAPON; }
	inline virtual bool IsTemporaryEvent() const { return true; }

	//
	//
	//

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

#if !__FINAL
	// Debug function to return event name as a string for error output
	virtual atString GetName() const {return atString("LeaderHolsteredWeapon");}
#endif

protected:

	virtual bool TakesPriorityOver(const CEvent& UNUSED_PARAM(otherEvent)) const { return true; }
};

//////////////////////////////////////////////////////////////////////////
// CEventLeaderUnholsteredWeapon
//////////////////////////////////////////////////////////////////////////

// Leader drew their gun
class CEventLeaderUnholsteredWeapon : public CEvent
{
public:

	CEventLeaderUnholsteredWeapon()
	{
#if !__FINAL
		m_EventType = EVENT_LEADER_UNHOLSTERED_WEAPON;
#endif
	}

	virtual ~CEventLeaderUnholsteredWeapon() {}

	virtual CEvent* Clone() const { return rage_new CEventLeaderUnholsteredWeapon(); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_LEADER_UNHOLSTERED_WEAPON; }
	virtual int GetEventPriority() const { return E_PRIORITY_LEADER_UNHOLSTERED_WEAPON; }
	virtual bool IsTemporaryEvent() const { return true; }

	//
	//
	//

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

#if !__FINAL
	// Debug function to return event name as a string for error output
	virtual atString GetName() const { return atString("LeaderUnholsteredWeapon"); }
#endif

protected:

	virtual bool TakesPriorityOver(const CEvent& UNUSED_PARAM(otherEvent)) const { return true; }
};

//////////////////////////////////////////////////////////////////////////
// CEventLeaderEnteredCover
//////////////////////////////////////////////////////////////////////////

// Leader entered cover
class CEventLeaderEnteredCover : public CEvent
{
public:
	CEventLeaderEnteredCover()
	{
#if !__FINAL
		m_EventType = EVENT_LEADER_ENTERED_COVER;
#endif
	}

	virtual ~CEventLeaderEnteredCover() {}

	virtual CEvent* Clone() const { return rage_new CEventLeaderEnteredCover(); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_LEADER_ENTERED_COVER; }
	virtual int GetEventPriority() const { return E_PRIORITY_LEADER_ENTERED_COVER; }

	// Can this event be interrupted by an event of the same type?
	// Usually an event won't replace one of the same type
	virtual bool CanBeInterruptedBySameEvent() const { return true; }

	//
	//
	//

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;

	//
	//
	//

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

#if !__FINAL
	virtual atString GetName() const {return atString("LeaderEnteredCover");}
#endif

protected:

	virtual bool TakesPriorityOver(const CEvent& UNUSED_PARAM(otherEvent)) const { return true; }
};

//////////////////////////////////////////////////////////////////////////
// CEventLeaderLeftCover
//////////////////////////////////////////////////////////////////////////

// Leader left cover
class CEventLeaderLeftCover : public CEvent
{
public:

	CEventLeaderLeftCover()
	{
#if !__FINAL
		m_EventType = EVENT_LEADER_LEFT_COVER;
#endif
	}

	virtual ~CEventLeaderLeftCover() {}

	virtual CEvent* Clone() const { return rage_new CEventLeaderLeftCover(); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_LEADER_LEFT_COVER; }
	virtual int GetEventPriority() const { return E_PRIORITY_LEADER_LEFT_COVER; }

	// Can this event be interrupted by an event of the same type?
	// Usually an event won't replace one of the same type
	virtual bool CanBeInterruptedBySameEvent() const { return true; }

	//
	//
	//

	// Should the event generate a response task for the specific ped?
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;

	// Query the event for the type of task to create
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_DO_NOTHING; }

#if !__FINAL
	virtual atString GetName() const {return atString("LeaderLeftCover");}
#endif

protected:

	virtual bool TakesPriorityOver(const CEvent& UNUSED_PARAM(otherEvent)) const { return true; }
};

#endif // INC_EVENT_LEADER_H
