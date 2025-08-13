//
// filename:	GamePresenceEventFeed.h
// description:	
//

#ifndef INC_GAMEPRESENCEEVENTDISPATCH_H_
#define INC_GAMEPRESENCEEVENTDISPATCH_H_

#include "atl/map.h"
#include "atl/array.h"
#include "atl/singleton.h"
#include "data/base.h"
#include "rline/rlpresence.h"
#include "rline/scpresence/rlscpresencemessage.h"
#include "rline/inbox/rlinbox.h"

#include "Network/SocialClubServices/netGamePresenceEvent.h"

namespace rage{
#if __BANK
	class bkBank;
#endif
}

class CGamePresenceEventDispatcher : public datBase
{
public:

	CGamePresenceEventDispatcher() : datBase() { }
	
	void Init();
	void Shutdown();

	//PURPSOSE:
	// Register a Game presence event with the dispatcher
	void RegisterEvent(const char* name, netGamePresenceEvent& staticInstance);

	void OnPresenceEvent(const rlPresenceEvent* event);

	static void SendEventToGamers(const netGamePresenceEvent& message, const rlGamerHandle* recipients, const int numRecps);
	static void SendEventToCommunity(const netGamePresenceEvent& message, SendFlag sendflag = SendFlag_Self);

#if __BANK
	void InitWidgets(bkBank& bank);
#endif
	
	//PURPOSE:
	// Specifically for processing a game.event event.
	void ProcessGameEvent(const rlGamerInfo& gamerInfo, const rlScPresenceMessageSender& sender, const RsonReader& rr, const char* channel);

	const netGamePresenceEvent* GetStaticEvent(netGamePresenceEvent::PresEventName& eventName) const;

protected:

	//PURPOSE:
	// Used to pass data directly to the handler.
	bool ProcessEvent(const rlGamerInfo& gamerInfo, const rlScPresenceMessageSender& sender, const char* eventName, const RsonReader& dataRR, const char* channel);

	struct MapEntry
	{
	public:
		MapEntry(netGamePresenceEvent& p) : m_pStaticInstance(&p) {}
		MapEntry() : m_pStaticInstance(NULL) {}

		netGamePresenceEvent* m_pStaticInstance;
	};
	
	typedef atMap<u32, MapEntry> EventHandlerMap;
	EventHandlerMap m_eventHandlerMap;
	
	rlPresence::Delegate m_presDelegate;
};

typedef atSingleton<CGamePresenceEventDispatcher> CGamePresenceEventDispatcherSingleton;
#define PRESENCE_EVENT_DISPATCH CGamePresenceEventDispatcherSingleton::GetInstance()

#endif // !INC_GAMEPRESENCEEVENTDISPATCH_H_