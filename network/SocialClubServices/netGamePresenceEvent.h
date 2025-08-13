//
// filename:	netGamePresenceEvent.h
// description:	
//

#ifndef INC_NETGAMEPRESENCEEVENT_H_
#define INC_NETGAMEPRESENCEEVENT_H_

#include "system/bit.h"
#include "rline/rlgamerinfo.h"

namespace rage
{
	class RsonReader;
	class RsonWriter;
	class bkBank;

//PURPOSE:
// netGamePresenceEvent provides a envelope or transport wrapper to send game specific events
// to other gamers using the rlPresence system. The events are sent and received via a presence
// event dispatcher CGamePresenceEventDispatcher.h
//
// The presence events are sent as json object.  netGamePresenceEvent provides a framework to handle 
// the importing and export of an event for being sent, passed, received, and handled.
//
// netGamePresenceEvent is a pure virtual class requiring of the important functions be implemented per class
//
// A new event can be create as such
//
//			class C_NewEvent_PresenceEvent : public netGamePresenceEvent
//			{
//			public:
//				GAME_PRESENCE_EVENT_DECL(_NewEvent_);
//			
//				static void Trigger(...)
//				{
//					C_NewEvent_PresenceEvent evt;
//					CGamePresenceEventDispatcher::SendEventToCommunity(evt);
//				}
//			
//				static void HandleEvent( const RsonReader& rr, const char* /*channel*/ )
//				{
//					C_NewEvent_PresenceEvent evt;
//					if(evt.Deserialise(&rr))
//						evt.Apply(); 
//				}
//			protected:
//			
//				virtual bool Serialise(RsonWriter* rw) const
//				{
//					//Use RsonWriter to serialize the data associated with this event 
//				}
//				virtual bool Deserialise(const RsonReader* /*rw*/)
//				{
//					//Use the RsonReader to unpack the data
//				}
//				virtual bool Apply() const
//				{
//					//Will be called after an event is Deserialised to actually apply the data of
//					//the event as it applies to the game
//				}
//			};
//
// Then, you'll want to register the event with the Dispatcher by calling the following
//
//		PRESENCE_EVENT_DISPATCH.RegisterEvent(C_NewEvent_PresenceEvent::EVENT_NAME(), C_NewEvent_PresenceEvent::HandleEvent);
//

//Flag set for specifying what groups to sent this event to.
typedef u16 SendFlag;
enum PresenceEventSendFlags
{
	SendFlag_None       = 0,
	SendFlag_Inbox		= BIT0,
	SendFlag_Presence	= BIT1, 
	SendFlag_Self		= BIT2,
	SendFlag_AllFriends = BIT3,
	SendFlag_AllCrew	= BIT4,
};

#define NET_GAME_PRESENCE_SERVER_DEBUG (__BANK)
#if NET_GAME_PRESENCE_SERVER_DEBUG
#define NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(...) __VA_ARGS__
#else
#define NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(...)
#endif

#define GAME_PRESENCE_EVENT_NAME "gm.evt"

#define GAME_PRESENCE_EVENT_BASE_DECL(name)\
	public:\
	static const char* EVENT_NAME() { return #name; }\
	virtual const char* GetEventName() const { return EVENT_NAME(); }

#define GAME_PRESENCE_EVENT_DECL(name)\
	GAME_PRESENCE_EVENT_BASE_DECL(name); \
	protected: \
	virtual bool Serialise(RsonWriter* rw) const override; \
	virtual bool Deserialise(const RsonReader* rw) override; \
	virtual bool Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& gamerHandle, const bool isHandleAuthenticated) override;

#define GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(name)\
	GAME_PRESENCE_EVENT_BASE_DECL(name); \
	protected: \
	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(virtual bool Serialise_Debug(RsonWriter* rw) const override); \
	virtual bool Deserialise(const RsonReader* rw) override; \
	virtual bool Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& gamerHandle, const bool isHandleAuthenticated) override;
	
//PURPOSE:
// Game Presence events are one off messages sent or received via the rlPresence system
// Events can be sent from game, web, or other Social Club (Rockstar) services
class netGamePresenceEvent
{
public:
	static const int MAX_EVENT_NAME_SIZE = 32;
	typedef char PresEventName[MAX_EVENT_NAME_SIZE];

	netGamePresenceEvent() {}
	virtual ~netGamePresenceEvent() {}

	//PURPOSE:
	bool Export(RsonWriter* /*rw*/) const;

	//PURPOSE:
	//  Import the data
	static bool ImportTo(const RsonReader* in_mgs, RsonReader* out_event);
	static bool IsGamePresEvent(const RsonReader& rr);
	static bool GetEventTypeName(const RsonReader& rr, PresEventName& eventName);
	static bool GetEventData(const RsonReader& rr, RsonReader& out_data);

	//PURPOSE:
	//	Public interface to deserialise that does type checking.
	bool GetObjectAsType(const RsonReader& in_msgData);
	
	//PURPOSE:
	//	Handles an received event
	virtual void HandleEvent(const rlGamerInfo& gamerInfo, const rlScPresenceMessageSender& sender, netGamePresenceEvent& rEvent, const RsonReader& rr, const char* channel);

	//PURPOSE:
	//  Implemented as part of GAME_PRESENCE_EVENT_DECL()
	virtual const char* GetEventName() const = 0;

protected:

	//PURPOSE:
	//   Returns whether the authenticataed handle we have received is valid for this message
	virtual bool ValidateAuthenticatedHandle(const rlGamerHandle& UNUSED_PARAM(gamerHandle)) const 
	{
		// assume true, derived classes can implement a specific handler
		return true; 
	}

	//PURPOSE:
	//   Serialise the data from this event as json.  This function will be specific to each event	
	virtual bool Serialise(RsonWriter* rw) const = 0;

	//PURPOSE:
	//   Deserialise the data from json to update this event.  This function will be specific to each event
	virtual bool Deserialise(const RsonReader* /*rw*/) = 0;

	//PURPOSE:
	//Apply the data of the event object to carry out the action.
	virtual bool Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& gamerHandle, const bool isHandleAuthenticated) = 0;
};

//PURPOSE:
// For events that should only be sent from the server
// Supports debug serialization for testing purposes
class netGameServerPresenceEvent : public netGamePresenceEvent
{
protected:
	// prevent overriding the Serialise function in derived classes
	bool Serialise(RsonWriter* NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(rw)) const final
	{
#if NET_GAME_PRESENCE_SERVER_DEBUG
		return Serialise_Debug(rw);
#else
		return false;
#endif
	}
	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(virtual bool Serialise_Debug(RsonWriter* rw) const = 0);

	//PURPOSE:
	//   Returns whether the authenticated handle we have received is valid for this message
	bool ValidateAuthenticatedHandle(const rlGamerHandle& gamerHandle) const;
};


}//namespace rage

#endif // !INC_NETGAMEPRESENCEEVENT_H_
