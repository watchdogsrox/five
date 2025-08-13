//
// filename:	GamePresenceEventDispatcher.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "GamePresenceEventDispatcher.h"
#include "netGamePresenceEvent.h"

//RAGE
#include "system/param.h"
#include "rline/rlpresence.h"

//framework
#include "fwnet/netchannel.h"

#include "network/Live/LiveManager.h"
#include "Network/NetworkInterface.h"

//game
#include "event/EventNetwork.h"
#include "event/EventGroup.h"

NETWORK_OPTIMISATIONS()

#undef __net_channel
#define __net_channel net_presevt
RAGE_DECLARE_SUBCHANNEL(net, presevt); //defined in netGamePresenceEvent.cpp

PARAM(presevnt_onefriend, "The one and only friend to send events to");
PARAM(presevnt_disable, "Disable sending and receiving events");
PARAM(presevnt_sendtoself, "Send the events to self");

void CGamePresenceEventDispatcher::Init()
{
	//Register to handle presence events.
	m_presDelegate.Bind(this, &CGamePresenceEventDispatcher::OnPresenceEvent);
	rlPresence::AddDelegate(&m_presDelegate);
}

void CGamePresenceEventDispatcher::Shutdown()
{
	rlPresence::RemoveDelegate(&m_presDelegate);
}

void CGamePresenceEventDispatcher::OnPresenceEvent( const rlPresenceEvent* evt )
{
	if( evt->GetId() == rlPresenceEventScMessage::EVENT_ID() )
	{
		const rlPresenceEventScMessage* pSCEvent = static_cast<const rlPresenceEventScMessage*>(evt);

		//We only process rlScPresenceMessagePublish messages here
		if(evt->m_ScMessage->m_Message.IsA<rlScPresenceMessagePublish>())
		{
			//rlScPresenceMessagePublish is the envelope that our game event is wrapped in, so
			//open the envelope
			rlScPresenceMessagePublish pubMessage;
			
			if(netVerify(pubMessage.Import(pSCEvent->m_Message)))
			{
				//The contents of our game presence event is packed in msg, so crach that up
				RsonReader msgData(pubMessage.m_Message, ustrlen(pubMessage.m_Message));
				if(netGamePresenceEvent::IsGamePresEvent(msgData))
				{
					ProcessGameEvent(pSCEvent->m_GamerInfo, pSCEvent->m_Sender, msgData, pubMessage.m_Channel);
				}
			}
		}
	}
}

// gm.evt:{
//		e:"event_type",
//		from:"gamertag",
//		d:{
//		}
// }

void CGamePresenceEventDispatcher::ProcessGameEvent( const rlGamerInfo& gamerInfo, const rlScPresenceMessageSender& sender, const RsonReader& gameEventMsg, const char* channel )
{
	RsonReader presEventRR;
	if(!gnetVerifyf(netGamePresenceEvent::ImportTo(&gameEventMsg, &presEventRR), "Failed to import received message as a netGamePresenceEvent" ))
		return;

	RsonReader eventNameRR;
	if(presEventRR.GetMember("e", &eventNameRR))
	{
		char eventName[netGamePresenceEvent::MAX_EVENT_NAME_SIZE];
		if(eventNameRR.AsString(eventName))
		{
			//Get the data for the msg as it's own reader to pass into the handler.
			RsonReader eventData;
			if(gnetVerify(presEventRR.GetMember("d", &eventData)))
			{
				ProcessEvent(gamerInfo, sender, eventName, eventData, channel);
			}
		}
	}
}

bool CGamePresenceEventDispatcher::ProcessEvent(const rlGamerInfo& gamerInfo, const rlScPresenceMessageSender& sender, const char* eventName, const RsonReader& dataRR, const char* channel )
{
	const MapEntry* pEntry = m_eventHandlerMap.Access(atStringHash(eventName));
	if(gnetVerifyf(pEntry, "ProcessEvent :: No Handler for %s found", eventName))
	{
		if(gnetVerify(pEntry->m_pStaticInstance))
        {
            gnetDebug1("ProcessEvent :: Received %s for %s", eventName, gamerInfo.GetName());
            pEntry->m_pStaticInstance->HandleEvent(gamerInfo, sender,(*pEntry->m_pStaticInstance), dataRR, channel);
        }
		return true;
	}

	return false;
}

void CGamePresenceEventDispatcher::SendEventToGamers(const netGamePresenceEvent& message, const rlGamerHandle* recipients, const int numRecps)
{
	int myGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(myGamerIndex))
	{
		gnetError("SendEventToGamers :: Invalid gamer index in SendEventToGamers");
		return;
	}

	if(!rlPresence::IsOnline(myGamerIndex) || !rlRos::GetCredentials(myGamerIndex).IsValid())
	{
		gnetDebug1("SendEventToGamers :: Not able to do SendEventToGamers when player is not online or invalid credentials");
		return;
	}

	char pubMessageBuffer[1024];
	RsonWriter rw(pubMessageBuffer, RSON_FORMAT_JSON);

	//Since the msg portion of the publish message is going to be an actual JSON object, we need to wrap it in { }
	bool success =	rw.Begin(nullptr, nullptr) 
		&&	gnetVerify(message.Export(&rw))
		&& rw.End();

	if(!success)
	{
		gnetError("SendEventToGamers :: Failed to Export PresenceEvent %s to buffer", message.GetEventName());
		return;
	}

	int numRecipientsToSendTo = numRecps;
	if(!gnetVerify(numRecipientsToSendTo <= RLSC_PRESENCE_MAX_MESSAGE_RECIPIENTS))
	{
		gnetDebug1("SendEventToGamers :: Too many recipients: %d, Capping at: %d", numRecps, numRecipientsToSendTo);
		numRecipientsToSendTo = RLSC_PRESENCE_MAX_MESSAGE_RECIPIENTS;
	}

	if(recipients != nullptr && numRecipientsToSendTo > 0)
	{
		gnetDebug1("SendEventToGamers :: Sending %s to list of %d gamers", message.GetEventName(), numRecipientsToSendTo);

		rlScPresenceMessagePublish pubEvent("self", pubMessageBuffer);
		rlPresence::PostMessage(myGamerIndex, recipients, numRecipientsToSendTo, pubEvent, 0);
	}
}

void CGamePresenceEventDispatcher::SendEventToCommunity( const netGamePresenceEvent& message, SendFlag sendflag )
{
	int myGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(myGamerIndex))
	{
		gnetError("SendEventToCommunity :: Invalid gamer index in SendEventToCommunity");
		return;
	}

	if(!rlPresence::IsOnline(myGamerIndex) || !rlRos::GetCredentials(myGamerIndex).IsValid())
	{
		gnetDebug1("SendEventToCommunity :: Not able to do SendEventToCommunity when player is not online or invalid credentials");
		return;
	}

	char pubMessageBuffer[RLSC_PRESENCE_MESSAGE_MAX_SIZE];
	RsonWriter rw(pubMessageBuffer, RSON_FORMAT_JSON);
	
	//Since the msg portion of the publish message is going to be an actual JSON object, we need to wrap it in { }
	bool success = 
		rw.Begin(nullptr, nullptr) 
		&& gnetVerify(message.Export(&rw))
		&& rw.End();

	if(!success)
	{
		gnetError("SendEventToCommunity :: Failed to Export PresenceEvnent %s to buffer", message.GetEventName());
		return;
	}
	
	if(sendflag & SendFlag_AllFriends)
	{
		gnetDebug1("SendEventToCommunity :: Sending %s to All Friends", message.GetEventName());
		rlPresence::PublishToManyFriends(myGamerIndex, pubMessageBuffer);
	}

	if(sendflag & SendFlag_AllCrew)
	{
		const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(myGamerIndex);
		if(clanDesc.IsValid())
		{
			gnetDebug1("SendEventToCommunity :: Sending %s to All Crew", message.GetEventName());
			rlPresence::PublishToCrew(myGamerIndex, pubMessageBuffer);
		}
	}
	
	if(sendflag & SendFlag_Self || PARAM_presevnt_sendtoself.Get())
	{
		rlGamerHandle gh;
		if(rlPresence::GetGamerHandle(myGamerIndex, &gh))
		{
			gnetDebug1("SendEventToCommunity :: Sending %s to Self", message.GetEventName());
			rlScPresenceMessagePublish pubEvent("self", pubMessageBuffer);
			rlPresence::PostMessage(myGamerIndex, &gh, 1, pubEvent, 0);
		}
	}
}

void CGamePresenceEventDispatcher::RegisterEvent( const char* name, netGamePresenceEvent& rStaticInstance )
{
	//Check to see if there is already one in there with that name
	u32 nameHash = atStringHash(name);
	const MapEntry* pEntry = m_eventHandlerMap.Access(nameHash);
	if(gnetVerifyf(pEntry == nullptr, "Event with name %s already registered", name))
	{
		MapEntry newEntry(rStaticInstance);
		m_eventHandlerMap.Insert(nameHash, newEntry);
		gnetDebug3("RegisterEvent :: Registering %s", name);
	}
}

const netGamePresenceEvent* CGamePresenceEventDispatcher::GetStaticEvent( netGamePresenceEvent::PresEventName& eventName ) const
{
	const MapEntry* pEntry = m_eventHandlerMap.Access(atStringHash(eventName));
	if(pEntry)
	{
		return pEntry->m_pStaticInstance;
	}

	return nullptr;
}

#undef __net_channel
