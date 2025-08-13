//
// filename:	netGamePresenceEvent.cpp
// description:	
//
#include "netGamePresenceEvent.h"

//rage
#include "data/rson.h"

//framework
#include "fwnet/netchannel.h"

//Game
#undef __net_channel
#define __net_channel net_presevt
RAGE_DEFINE_SUBCHANNEL(net, presevt);

//Handle support for the older gta5.game.event tag
#define __OLD_GAME_EVENT_TAG_SUPPORT 1
#if __OLD_GAME_EVENT_TAG_SUPPORT
#define OLD_GTA5_GAME_EVENT_NAME "gta5.game.event"
#endif

#if NET_GAME_PRESENCE_SERVER_DEBUG
PARAM(netPresenceDisablePresenceDebug, "[Presence] Disables that we can process server only messages in BANK builds");
#endif

//////////////////////////////////////////////////////////////////////////
//  The JSON for the event looks like this
//
//////////////////////////////////////////////////////////////////////////
//
//gm.evt: {
//	e:"event_type",
//	d:{ <Serialise() fills this in>
//	  }
// }
bool netGamePresenceEvent::Export( RsonWriter* rw ) const
{
	bool success = false;
	success = 
		rw->Begin(GAME_PRESENCE_EVENT_NAME, NULL) 
			&&rw->WriteString("e", GetEventName())
			&& rw->Begin("d", NULL)
				&& gnetVerifyf(Serialise(rw), "Failed to serialize %s", GetEventName())
			&& rw->End()
		&& rw->End();

	return success;
}

bool netGamePresenceEvent::ImportTo( const RsonReader* in_msg, RsonReader* out_rw )
{
#if __OLD_GAME_EVENT_TAG_SUPPORT
	if(in_msg->HasMember(OLD_GTA5_GAME_EVENT_NAME))
	{
		gnetError("ImportTo :: Received message with old style gta5.game.event");
		return in_msg->GetMember(OLD_GTA5_GAME_EVENT_NAME, out_rw);
	}
#endif

	return in_msg->GetMember(GAME_PRESENCE_EVENT_NAME, out_rw);
}

bool netGamePresenceEvent::GetObjectAsType( const RsonReader& in_msgData )
{
	if (!netGamePresenceEvent::IsGamePresEvent(in_msgData))
	{
		return false;
	}

	netGamePresenceEvent::PresEventName eventName;
	bool bIsSameType = netGamePresenceEvent::GetEventTypeName(in_msgData, eventName) && (strcmp(GetEventName(), eventName) == 0);
	if (!bIsSameType)
	{
		return false;
	}

	RsonReader rrEventData;
	if(!GetEventData(in_msgData, rrEventData))
	{
		return false;
	}

	return Deserialise(&rrEventData);
}

#if __OLD_GAME_EVENT_TAG_SUPPORT
bool IsOldGameEvent(const RsonReader& rr)
{
	if(rr.CheckName(OLD_GTA5_GAME_EVENT_NAME))
		return true;

	RsonReader fstMmber;
	return rr.GetFirstMember(&fstMmber) && fstMmber.CheckName(OLD_GTA5_GAME_EVENT_NAME);
}
#endif

bool netGamePresenceEvent::IsGamePresEvent( const RsonReader& rr )
{
#if __OLD_GAME_EVENT_TAG_SUPPORT
	if (IsOldGameEvent(rr))
	{
		return true;
	}
#endif

	if(rr.CheckName(GAME_PRESENCE_EVENT_NAME))
		return true;

	RsonReader fstMmber;
	return rr.GetFirstMember(&fstMmber) && fstMmber.CheckName(GAME_PRESENCE_EVENT_NAME);
}

bool netGamePresenceEvent::GetEventTypeName( const RsonReader& rr, PresEventName& eventName )
{
	RsonReader temp;
	if(ImportTo(&rr, &temp))
	{
		return temp.GetMember("e", &temp) && temp.AsString(eventName);
	}

	return false;
}

bool netGamePresenceEvent::GetEventData( const RsonReader& rr, RsonReader& out_data )
{
	RsonReader temp;
	if(ImportTo(&rr, &temp))
	{
		return temp.GetMember("d", &out_data);
	}

	return false;
}

void netGamePresenceEvent::HandleEvent(const rlGamerInfo& gamerInfo, const rlScPresenceMessageSender& sender, netGamePresenceEvent& rEvent, const RsonReader& rr, const char*)
{
	if(sender.IsAuthenticated() && !ValidateAuthenticatedHandle(sender.m_GamerHandle))
	{
		gnetWarning("HandleEvent :: ValidateAuthenticatedHandle failed, dropping message");
		return;
	}

	if(rEvent.Deserialise(&rr))
	{
		rEvent.Apply(gamerInfo, sender.m_GamerHandle, sender.IsAuthenticated());
	}
}

bool netGameServerPresenceEvent::ValidateAuthenticatedHandle(const rlGamerHandle& gamerHandle) const
{
	// for server only events, the gamer handle must be invalid
	if(gamerHandle.IsValid() NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(&& PARAM_netPresenceDisablePresenceDebug.Get()))
	{
		gnetWarning("ValidateAuthenticatedHandle :: Received message with valid handle that can only be sent from the server!");
		return false;
	}
	return true; 
}

#undef __net_channel


