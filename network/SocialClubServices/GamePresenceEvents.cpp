//
// filename:	GamePresenceEvents.cpp
// description:	
//
#include "GamePresenceEvents.h"
#include "GamePresenceEventDispatcher.h"

//rage
#include "bank/bank.h"
#include "data/rson.h"
#include "rline/rlpresence.h"

//framework
#include "fwnet/netchannel.h"

//Game
#include "audio/frontendaudioentity.h"
#include "event/events.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/GameStream.h"
#include "frontend/GameStreamMgr.h"
#include "game/cheat.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/cloud/Tunables.h"
#include "network/NetworkInterface.h"
#include "network/Network.h"
#include "network/Sessions/NetworkSession.h"
#include "network/Live/livemanager.h"
#include "network/Live/InviteMgr.h"
#include "network/Sessions/NetworkSession.h"
#include "network/Sessions/NetworkVoiceSession.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "network/SocialClubServices/SocialClubInboxManager.h"
#include "network/Voice/NetworkVoice.h"
#include "peds/PlayerInfo.h"
#include "peds/PedIntelligence.h"
#include "script/commands_socialclub.h"
#include "task/Combat/TaskDamageDeath.h"
#include "task/Physics/TaskNMRelax.h"
#include "text/messages.h"
#include "vehicles/vehicle.h"

NETWORK_OPTIMISATIONS()

//PURPOSE:
//  Implementation of event for sending and receiving via rlPresence system to other gamers
//	or from gamers or the web.  See netGamePresenceEvent.h for further details.

#undef __net_channel
#define __net_channel net_presevt
RAGE_DECLARE_SUBCHANNEL(net, presevt); //defined in netGamePresenceEvent.cpp

#define GAME_INVITE_VERSION (4)

using namespace rage;

NetworkPlayerEventDelegate sPlyrEventDlgt;

//================================================================================================================
// GamePresenceEvents 
//================================================================================================================
void GamePresenceEvents::RegisterEvents()
{
#define REGISTER_PRES_EVENT(_TYPE_) { static _TYPE_ s_##_TYPE_##Inst; PRESENCE_EVENT_DISPATCH.RegisterEvent(_TYPE_::EVENT_NAME(), s_##_TYPE_##Inst); }

	// these events can be received from other players
	REGISTER_PRES_EVENT(CStatUpdatePresenceEvent);
	REGISTER_PRES_EVENT(CVoiceSessionInvite);
	REGISTER_PRES_EVENT(CVoiceSessionResponse);
	REGISTER_PRES_EVENT(CGameInvite);
	REGISTER_PRES_EVENT(CGameInviteCancel);
    REGISTER_PRES_EVENT(CGameInviteReply);
    REGISTER_PRES_EVENT(CFollowInvite);
	REGISTER_PRES_EVENT(CJoinQueueRequest);
	REGISTER_PRES_EVENT(CJoinQueueUpdate);
	REGISTER_PRES_EVENT(CTextMessageEvent);

	// these events can only be received from the server
	REGISTER_PRES_EVENT(CFriendCrewJoinedPresenceEvent);
	REGISTER_PRES_EVENT(CFriendCrewCreatedPresenceEvent);
	REGISTER_PRES_EVENT(CMissionVerifiedPresenceEvent);
	REGISTER_PRES_EVENT(CRockstarMsgPresenceEvent);
	REGISTER_PRES_EVENT(CRockstarCrewMsgPresenceEvent);
	REGISTER_PRES_EVENT(CGameAwardPresenceEvent);
	REGISTER_PRES_EVENT(CTournamentInvite);
	REGISTER_PRES_EVENT(CAdminInvite);
	REGISTER_PRES_EVENT(CAdminInviteNotification);
	REGISTER_PRES_EVENT(CFingerOfGodPresenceEvent);
	REGISTER_PRES_EVENT(CNewsItemPresenceEvent);
	REGISTER_PRES_EVENT(CForceSessionUpdatePresenceEvent);
	REGISTER_PRES_EVENT(CGameTriggerEvent);

#if __NET_SHOP_ACTIVE
	REGISTER_PRES_EVENT(GameServerPresenceEvent);
#endif

	sPlyrEventDlgt.Bind(&GamePresenceEvents::OnPlayerMessageReceived);
}

//Bank stuff for general support
#if RSG_BANK

bool s_Bank_SendToSelf = true;
bool s_Bank_SendToFriends = false;
bool s_Bank_SendToCrew = false;
bool s_Bank_UseInbox = false;
bool s_Bank_UsePresence = true;

//
//  Helper function to grab the send flags controlled by RAG
//
SendFlag Helper_GetSendFlags()
{
	SendFlag flags = SendFlag_None;
	if(s_Bank_SendToSelf)
	{
		flags |= SendFlag_Self;
	}

	if(s_Bank_SendToFriends)
	{
		flags |= SendFlag_AllFriends;
	}

	if(s_Bank_SendToCrew)
	{
		flags |= SendFlag_AllCrew;
	}

	if(s_Bank_UseInbox)
	{
		flags |= SendFlag_Inbox;
	}

	if(s_Bank_UsePresence)
	{
		flags |= SendFlag_Presence;
	}

	return flags;
}

void TriggerAll()
{
	// build bank send flags
	const unsigned sendFlags = Helper_GetSendFlags();

	// dummy recipient list (use the local gamer so that we receive the event)
	rlGamerHandle recipientList[1]; 
	recipientList[0] = NetworkInterface::GetLocalGamerHandle();

	// trigger p2p events
	static const char* s_UgcStatUpdateContentId = "TestContentId";
	CUGCStatUpdatePresenceEvent::Trigger(s_UgcStatUpdateContentId, 100, static_cast<SendFlag>(sendFlags));

	scrBountyInboxMsg_Data bountyData;
	safecpy(bountyData.m_fromGTag, "From");
	safecpy(bountyData.m_targetGTag, "Target");
	bountyData.m_iOutcome.Int = 0;
	bountyData.m_iCash.Int = 10;
	bountyData.m_iRank.Int = 10;
	bountyData.m_iTime.Int = 5000;
	CBountyPresenceEvent::Trigger(recipientList, 1, &bountyData);

#if NET_GAME_PRESENCE_SERVER_DEBUG
	// trigger server events
	rlClanDesc clanDesc;
	CFriendCrewJoinedPresenceEvent::Trigger_Debug(clanDesc, static_cast<SendFlag>(sendFlags));

	CFriendCrewCreatedPresenceEvent::Trigger_Debug(static_cast<SendFlag>(sendFlags));

	static const char* s_MissionVerifiedName = "TestContentId";
	CMissionVerifiedPresenceEvent::Trigger_Debug(s_MissionVerifiedName, static_cast<SendFlag>(sendFlags));

	static const char* s_Msg = "Test Message";
	CRockstarMsgPresenceEvent::Trigger_Debug(s_Msg, static_cast<SendFlag>(sendFlags));
	
	static const char* s_CrewMsg = "Crew Message";
	CRockstarCrewMsgPresenceEvent::Trigger_Debug(s_CrewMsg, 10, "TestCrewTag", static_cast<SendFlag>(sendFlags));

	CGameAwardPresenceEvent::Trigger_Debug(ATSTRINGHASH("TestAward", 0x7f431481), 1.0, static_cast<SendFlag>(sendFlags), recipientList, 1);

	static const char* s_NewsStoryKey = "News Key";
	CNewsItemPresenceEvent::Trigger_Debug(s_NewsStoryKey, static_cast<SendFlag>(sendFlags));

	CFingerOfGodPresenceEvent::TriggerBlacklist();

	CForceSessionUpdatePresenceEvent::Trigger_Debug();

	CGameTriggerEvent::Trigger_Debug(ATSTRINGHASH("TestEvent", 0x3bbe9858), 0, 1, "Test", static_cast<SendFlag>(sendFlags));
	
	static const char* s_TextMsg = "Text Message";
	CTextMessageEvent::Trigger(recipientList, s_TextMsg);

	CTournamentInvite::Trigger_Debug(recipientList[0]);
#endif
}

void GamePresenceEvents::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Presence and Inbox Events");

		bank.AddToggle("Use Inbox", &s_Bank_UseInbox);
		bank.AddToggle("Use Presence", &s_Bank_UsePresence);
		bank.AddSeparator();
		bank.AddToggle("Send To Self", &s_Bank_SendToSelf);
		bank.AddToggle("Send To Friends", &s_Bank_SendToFriends);
		bank.AddToggle("Send To Crew", &s_Bank_SendToCrew);
		bank.AddButton("Trigger", CFA(TriggerAll));
		bank.AddSeparator("Events");

		//Add an entry for each event as necessary.
		CStatUpdatePresenceEvent::AddWidgets(bank);
		CFriendCrewJoinedPresenceEvent::AddWidgets(bank);
		CFriendCrewCreatedPresenceEvent::AddWidgets(bank);
		CMissionVerifiedPresenceEvent::AddWidgets(bank);
		CRockstarMsgPresenceEvent::AddWidgets(bank);
		CRockstarCrewMsgPresenceEvent::AddWidgets(bank);
		CGameAwardPresenceEvent::AddWidgets(bank);
		CUGCStatUpdatePresenceEvent::AddWidgets(bank);
		CNewsItemPresenceEvent::AddWidgets(bank);
		CBountyPresenceEvent::AddWidgets(bank);
		CFingerOfGodPresenceEvent::AddWidgets(bank);
		CForceSessionUpdatePresenceEvent::AddWidgets(bank);
		CGameTriggerEvent::AddWidgets(bank);
		CTextMessageEvent::AddWidgets(bank);
		CTournamentInvite::AddWidgets(bank);
		NET_SHOP_ONLY(GameServerPresenceEvent::AddWidgets(bank);)

	bank.PopGroup();
}
#endif // RSG_BANK

namespace GamePresenceEvents
{
	class DeferredGamePresenceEventsList
	{
	public:

		~DeferredGamePresenceEventsList()
		{
			//Clean up the stuff that was bequeathed to us
			for (int i = 0; i < m_deferredEventList.GetCount(); ++i)
			{
				delete m_deferredEventList[i];
			}
			m_deferredEventList.Reset();
		}

		void AddToList(netDeferredGamePresenceEventIntf* pNewItem)
		{
			m_deferredEventList.PushAndGrow(pNewItem);
		}

		void Update()
		{
			if(m_deferredEventList.GetCount() == 0)
			{
				return;
			}

			//Call Update on each one.  When update returns false, it need to be deleted and removed
			atArray<int> removeList;
			for(int i = 0; i < m_deferredEventList.GetCount(); ++i)
			{
				if(m_deferredEventList[i]->Update() == false)
				{
					removeList.PushAndGrow(i);
				}
			}

			if(removeList.GetCount() == 0)
			{
				return;
			}

			//Clean up the stuff that was bequeathed to us, but go backwards since the removeList is in order
			for(int removeIter = removeList.GetCount() - 1; removeIter >= 0; --removeIter)
			{
				delete m_deferredEventList[removeList[removeIter]];
				m_deferredEventList.DeleteFast(removeList[removeIter]);
			}
		}

	private:
		atArray<netDeferredGamePresenceEventIntf*> m_deferredEventList;
	};

	void OpenNetworking()
	{
		netInterface::AddDelegate(&sPlyrEventDlgt);
	}

	void CloseNetworking()
	{
		netInterface::RemoveDelegate(&sPlyrEventDlgt);
	}
}

GamePresenceEvents::DeferredGamePresenceEventsList s_DeferredEventList;
void GamePresenceEvents::Update()
{
	s_DeferredEventList.Update();
}

void netDeferredGamePresenceEventIntf::AddToDeferredList() const
{
	netDeferredGamePresenceEventIntf* pNewOne = this->CreateCopy();
	s_DeferredEventList.AddToList(pNewOne);
}

NET_MESSAGE_IMPL(msgRequestKickFromHost);
void GamePresenceEvents::OnPlayerMessageReceived(const ReceivedMessageData &messageData)
{
	unsigned msgId = 0;

	if(gnetVerify(messageData.IsValid()))
	{
		if(netMessage::GetId(&msgId, messageData.m_MessageData, messageData.m_MessageDataSize))
		{
			if(msgId == msgRequestKickFromHost::MSG_ID())
			{
				if(NetworkInterface::IsHost() && messageData.m_FromPlayer != nullptr)
				{
					msgRequestKickFromHost msg;
					if(gnetVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize)))
					{
						CNetwork::GetNetworkSession().KickPlayer(messageData.m_FromPlayer, static_cast<KickReason>(msg.m_kickReason), 0, RL_INVALID_GAMER_HANDLE);
					}
				}
			}
		}
	}
}

static bool s_RejectMismatchedAuthenticatedSender = false;
void GamePresenceEvents::SetRejectMismatchedAuthenticatedSender(const bool reject)
{
	if(s_RejectMismatchedAuthenticatedSender != reject)
	{
		gnetDebug1("GamePresenceEvents::SetRejectMismatchedAuthenticatedSender :: %s -> %s", s_RejectMismatchedAuthenticatedSender ? "True" : "False", reject ? "True" : "False");
		s_RejectMismatchedAuthenticatedSender = reject;
	}
}

//================================================================================================================
// CStatUpdatePresenceEvent 
//================================================================================================================
void CStatUpdatePresenceEvent::Trigger(u32 statId, int value, u32 value2, const char* stringData, SendFlag flags)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	char localGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	gnetVerify(rlPresence::GetDisplayName(localGamerIndex, localGamerDisplayName));

	CStatUpdatePresenceEvent evt(statId, localGamerDisplayName, value, value2, stringData);
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
}

void CStatUpdatePresenceEvent::Trigger(u32 statId, float value, u32 value2, const char* stringData, SendFlag flags)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	char localGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	gnetVerify(rlPresence::GetDisplayName(localGamerIndex, localGamerDisplayName));

	CStatUpdatePresenceEvent evt((u32)statId, localGamerDisplayName, value, value2, stringData);
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
}

bool CStatUpdatePresenceEvent::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteUns("stat", m_statNameHash));
		rverifyall(rw->WriteString("from", m_from));

		if(m_type == VAL_FLOAT)
		{
			rverifyall(rw->WriteFloat("fval", m_fVal));
		}
		else
		{
			rverifyall(rw->WriteInt("ival", m_iVal));
		}

		if(m_value2 != 0)
		{
			rverifyall(rw->WriteUns("v2", m_value2));
		}

		if(strlen(m_stringData) > 0)
		{
			rverifyall(rw->WriteString("sv", m_stringData));
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CStatUpdatePresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr->ReadUns("stat", m_statNameHash));
		rverifyall(rr->ReadString("from", m_from));
	
		if(rr->ReadFloat("fval", m_fVal))
			m_type = VAL_FLOAT;
		else
		{
			// from Serialise, if it's not a float then it must be an int
			rverifyall(rr->ReadInt("ival", m_iVal));
			m_type = VAL_INT;
		}

		if(!rr->ReadUns("v2", m_value2))
			m_value2 = 0;
		if(!rr->ReadString("sv", m_stringData))
			m_stringData[0] = '\0';
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CStatUpdatePresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender : todo : check whether we need to validate this in script or can reject in code under certain conditions

	// send an event to script appropriate to the event.	
	if(m_type == VAL_FLOAT)
	{
		CEventNetworkPresence_StatUpdate scrEvent(m_statNameHash, m_from, m_fVal, m_value2, m_stringData);
		GetEventScriptNetworkGroup()->Add(scrEvent);
	}
	else
	{
		CEventNetworkPresence_StatUpdate scrEvent(m_statNameHash, m_from, m_iVal, m_value2, m_stringData);
		GetEventScriptNetworkGroup()->Add(scrEvent);
	}

	return true;
}

#if RSG_BANK
char s_Bank_STATUPDATE_statName[64];
int s_Bank_STATUPDATE_iValue = 0;
float s_Bank_STATUPDATE_fValue = 0.0f;
void Bank_CStatUpdatePresenceEvent_TriggerInt()
{
	//If the string starts with a #, it's a number.
	int hashVal = atStringHash(s_Bank_STATUPDATE_statName);
	if(s_Bank_STATUPDATE_statName[0] == '#')
	{
		sscanf(s_Bank_STATUPDATE_statName, "#%d", &hashVal);
	}
	CStatUpdatePresenceEvent::Trigger(hashVal, s_Bank_STATUPDATE_iValue, 0, "", Helper_GetSendFlags());
}

void Bank_CStatUpdatePresenceEvent_TriggerFloat()
{
	//If the string starts with a #, it's a number.
	int hashVal = atStringHash(s_Bank_STATUPDATE_statName);
	if(s_Bank_STATUPDATE_statName[0] == '#')
	{
		sscanf(s_Bank_STATUPDATE_statName, "#%d", &hashVal);
	}
	CStatUpdatePresenceEvent::Trigger(hashVal, s_Bank_STATUPDATE_fValue,0, "", Helper_GetSendFlags());
}

void CStatUpdatePresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(s_Bank_STATUPDATE_statName, "TIMES_CHEATED");

	bank.PushGroup("CStatUpdatePresenceEvent");
		bank.AddText("Stat Name", s_Bank_STATUPDATE_statName, sizeof(s_Bank_STATUPDATE_statName), false);
		bank.AddText("Stat Value (INT)", &s_Bank_STATUPDATE_iValue, false);
		bank.AddText("Stat Value (FLOAT)", &s_Bank_STATUPDATE_fValue, false);
		bank.AddButton("Trigger Int", CFA(Bank_CStatUpdatePresenceEvent_TriggerInt));
		bank.AddButton("Trigger Float", CFA(Bank_CStatUpdatePresenceEvent_TriggerFloat));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CFriendCrewJoinedPresenceEvent 
//================================================================================================================
CFriendCrewJoinedPresenceEvent::CFriendCrewJoinedPresenceEvent(const rlClanDesc& clanDesc, const char* name)
	: netGameServerPresenceEvent()
	, m_clanDesc(clanDesc)
{
	safecpy(m_from, name);
}

CFriendCrewJoinedPresenceEvent::CFriendCrewJoinedPresenceEvent()
	: netGameServerPresenceEvent()
{
	m_clanDesc.Clear();
	m_from[0] = '\0';
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CFriendCrewJoinedPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteInt64("clanid", m_clanDesc.m_Id));
		rverifyall(rw->WriteString("tag", m_clanDesc.m_ClanTag));
		rverifyall(rw->WriteString("name", m_clanDesc.m_ClanName));
		rverifyall(rw->WriteBool("open", m_clanDesc.m_IsOpenClan));
		rverifyall(rw->WriteString("from", m_from));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CFriendCrewJoinedPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		m_clanDesc.Clear();
		rverifyall(rr->ReadInt64("clanid", m_clanDesc.m_Id));
		rverifyall(rr->ReadString("tag", m_clanDesc.m_ClanTag));
		rverifyall(rr->ReadString("name", m_clanDesc.m_ClanName));
		rverifyall(rr->ReadBool("open", m_clanDesc.m_IsOpenClan));
		rverifyall(rr->ReadString("from", m_from));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CFriendCrewJoinedPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)

	// check if crew messages should be disabled
	if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_DISABLE_ALL_FRIEND_JOINED_CREW_MESSAGES", 0x5e003930), false))
	{
		gnetDebug1("CFriendCrewJoinedPresenceEvent::Apply :: Blocking (all) due to tunable");
		return false;
	}

	gnetDebug1("Received CFriendCrewJoinedPresenceEvent to crew %s [%d] from %s",m_clanDesc.m_ClanTag, (int)m_clanDesc.m_Id, m_from);
	CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
	if(pGameStream != nullptr)
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];

		char *pMainString = TheText.Get("CREW_FEED_FRIEND_JOINED_CREW");

		CSubStringWithinMessage aSubStrings[2];
		aSubStrings[0].SetLiteralString(m_from, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
		aSubStrings[1].SetLiteralString(m_clanDesc.m_ClanName, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			nullptr, 0, 
			aSubStrings, 2, 
			FinalString, NELEM(FinalString));

		pGameStream->PostCrewTag(!m_clanDesc.m_IsOpenClan, false, m_clanDesc.m_ClanTag, 0, false, FinalString, true, m_clanDesc.m_Id, nullptr, Color32(m_clanDesc.m_clanColor));
	}
	return true;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CFriendCrewJoinedPresenceEvent::Trigger_Debug(const rlClanDesc& clanDesc, SendFlag flags /*= SendFlag_AllFriends*/)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	char localGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	gnetVerify(rlPresence::GetDisplayName(localGamerIndex, localGamerDisplayName));

	CFriendCrewJoinedPresenceEvent evt(clanDesc, localGamerDisplayName);
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
}
#endif

#if RSG_BANK
rlClanDesc s_Bank_clanDesc;
void FakeFriendJoinCrew()
{
	CFriendCrewJoinedPresenceEvent::Trigger_Debug(s_Bank_clanDesc, Helper_GetSendFlags());
};

void CFriendCrewJoinedPresenceEvent::AddWidgets(bkBank& bank)
{
	s_Bank_clanDesc.Clear();
	safecpy(s_Bank_clanDesc.m_ClanName, "Fake Crew Name");
	safecpy(s_Bank_clanDesc.m_ClanTag, "FAKE");
	s_Bank_clanDesc.m_Id = 724;

	bank.PushGroup("CFriendCrewJoinedPresenceEvent");
		bank.AddText("Crew Tag", s_Bank_clanDesc.m_ClanTag, sizeof(s_Bank_clanDesc.m_ClanTag), false);
		bank.AddText("Crew Name", s_Bank_clanDesc.m_ClanName, sizeof(s_Bank_clanDesc.m_ClanName), false);
		bank.AddButton("Send Fake Invite event", CFA(FakeFriendJoinCrew));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CFriendCrewCreatedPresenceEvent 
//================================================================================================================
CFriendCrewCreatedPresenceEvent::CFriendCrewCreatedPresenceEvent() 
	: netGameServerPresenceEvent()
{
	m_from[0] = '\0';
}

CFriendCrewCreatedPresenceEvent::CFriendCrewCreatedPresenceEvent(const char* name) 
	: netGameServerPresenceEvent()
{
	safecpy(m_from,name);
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CFriendCrewCreatedPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("from", m_from));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CFriendCrewCreatedPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr->ReadString("from", m_from));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CFriendCrewCreatedPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)

	// check if crew messages should be disabled
	if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_DISABLE_ALL_FRIEND_CREATED_CREW_MESSAGES", 0x14d983ab), false))
	{
		gnetDebug1("CFriendCrewCreatedPresenceEvent::Apply :: Blocking (all) due to tunable");
		return false;
	}

	gnetDebug1("CFriendCrewCreatedPresenceEvent :: Received from %s", m_from);
	CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
	if(pGameStream != nullptr)
	{
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];

		char *pMainString = TheText.Get("CREW_FEED_FRIEND_CREATED_CREW_BASIC");

		CSubStringWithinMessage aSubStrings[1];
		aSubStrings[0].SetLiteralString(m_from, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
		CMessages::InsertNumbersAndSubStringsIntoString(
			pMainString,
			nullptr, 0,
			aSubStrings, 1,
			FinalString, NELEM(FinalString));

		pGameStream->PostTicker(FinalString, true);
	}
	return true;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CFriendCrewCreatedPresenceEvent::Trigger_Debug(SendFlag flags /*= SendFlag_AllFriends*/)
{
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	char localGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	gnetVerify(rlPresence::GetDisplayName(localGamerIndex, localGamerDisplayName));

	CFriendCrewCreatedPresenceEvent evt(localGamerDisplayName);
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
}
#endif

#if RSG_BANK
void FakeFriendCreatedCrew()
{
	CFriendCrewCreatedPresenceEvent::Trigger_Debug(Helper_GetSendFlags());
}

void CFriendCrewCreatedPresenceEvent::AddWidgets(bkBank& bank)
{
	bank.PushGroup("CFriendCrewCreatedPresenceEvent");
		bank.AddButton("Send", CFA(FakeFriendCreatedCrew));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CMissionVerifiedPresenceEvent 
//================================================================================================================
CMissionVerifiedPresenceEvent::CMissionVerifiedPresenceEvent() 
	: netGameServerPresenceEvent()
{
	m_creatorGH.Clear();
	m_creatorDisplayName[0] = '\0';
	m_missionName[0] = '\0';
	m_missionContentId[0] = '\0';
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CMissionVerifiedPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		char ghStr[RL_MAX_GAMER_HANDLE_CHARS];
		rverifyall(m_creatorGH.ToString(ghStr));
		rverifyall(rw->WriteString("gh", ghStr));
		rverifyall(rw->WriteString("gtag", m_creatorDisplayName));
		rverifyall(rw->WriteString("mid", m_missionContentId));
		rverifyall(rw->WriteString("mn", m_missionName));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CMissionVerifiedPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		char ghStr[RL_MAX_GAMER_HANDLE_CHARS];
		rverifyall(rr->ReadString("gh", ghStr));
		rverifyall(m_creatorGH.FromString(ghStr));
		rverifyall(rr->ReadString("mid", m_missionContentId));
		rverifyall(rr->ReadString("mn", m_missionName));

		// optional
		rr->ReadString("gtag", m_creatorDisplayName);
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CMissionVerifiedPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender : todo : check whether we need to validate this in script or can reject in code under certain conditions
	
	// check if crew messages should be disabled
	if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_DISABLE_ALL_MISSION_VERIFIED_MESSAGES", 0x96445d0f), false))
	{
		gnetDebug1("CMissionVerifiedPresenceEvent::Apply :: Blocking (all) due to tunable");
		return false;
	}

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	//See if we got a gamer tag.
	const char* friendName = nullptr;
	char localGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];

	if(strlen(m_creatorDisplayName) > 0)
	{
		friendName = m_creatorDisplayName;
	}
	else //check for the player in the friends list.
	{
		rlGamerHandle localgh;
		if(rlFriendsManager::IsFriendsWith(localGamerIndex, m_creatorGH))
		{
			// TODO JRM: Friends names cannot be synchronous lookups.
			const rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(m_creatorGH);
			if(pFriend)
			{
				friendName = pFriend->GetDisplayName();
			}
		}
		//It may be us
		else if(rlPresence::GetGamerHandle(localGamerIndex, &localgh) && m_creatorGH == localgh)
		{
			if(rlPresence::GetDisplayName(localGamerIndex, localGamerDisplayName))
			{
				friendName = localGamerDisplayName;
			}
		}
	}

	if(friendName && strlen(friendName) > 0)
	{
		gnetDebug1("Received CMissionVerifiedPresenceEvent for %s, mission %s", friendName, m_missionName);
		CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
		if(pGameStream != nullptr)
		{
			char FinalString[2 * MAX_CHARS_IN_MESSAGE];

			char *pMainString = TheText.Get("FEED_MISSION_VERIFIED");

			CSubStringWithinMessage aSubStrings[2];
			aSubStrings[0].SetLiteralString(friendName, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
			aSubStrings[1].SetLiteralString(m_missionName, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
			CMessages::InsertNumbersAndSubStringsIntoString(
				pMainString,
				nullptr, 0,
				aSubStrings, 2,
				FinalString, NELEM(FinalString));

			pGameStream->PostTicker(FinalString, true);
		}
	}
	
	return true;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CMissionVerifiedPresenceEvent::Trigger_Debug(const char* missionName, SendFlag  flags /*= SendFlag_AllFriends*/)
{
	//Sent only via web except from bank for testing?
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	rlGamerHandle gh;
	if(!rlPresence::GetGamerHandle(localGamerIndex, &gh))
	{
		return;
	}
	
	CMissionVerifiedPresenceEvent evt;
	evt.m_creatorGH = gh;
	gnetVerify(rlPresence::GetDisplayName(localGamerIndex, evt.m_creatorDisplayName));
	safecpy(evt.m_missionName, missionName);
	formatf(evt.m_missionContentId, "Fake0x%x", atStringHash(missionName));

	CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
}
#endif

#if RSG_BANK
char bank_MissionVerified_MissionName[RLUGC_MAX_CONTENT_NAME_CHARS];
void CMissionVerifiedPresenceEvent_SendFakeEvent()
{
	CMissionVerifiedPresenceEvent::Trigger_Debug(bank_MissionVerified_MissionName, Helper_GetSendFlags());
}

void CMissionVerifiedPresenceEvent::AddWidgets(bkBank& bank)
{
	bank_MissionVerified_MissionName[0] = '\0';
	bank.PushGroup("CMissionVerifiedPresenceEvent");
		bank.AddText("Fake Mission Name", bank_MissionVerified_MissionName, sizeof(bank_MissionVerified_MissionName), false);
		bank.AddButton("Send Fake Mission Verified event", CFA(CMissionVerifiedPresenceEvent_SendFakeEvent));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CUGCStatUpdatePresenceEvent 
//================================================================================================================
void CUGCStatUpdatePresenceEvent::Trigger(const char* content_mission_id, int score, SendFlag flags)
{
	CUGCStatUpdatePresenceEvent evt;
	safecpy(evt.m_missionContentId, content_mission_id);
	evt.m_score = score;

	//Sent only via web except from bank for testing?
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();	
	if(!gnetVerify(rlPresence::GetGamerHandle(localGamerIndex, &evt.m_fromGH) && rlPresence::GetDisplayName(localGamerIndex, evt.m_fromGamerDisplayName)))
	{
		return;
	}

	if(flags & SendFlag_Inbox)
	{
		SC_INBOX_MGR.SendEventToCommunity(evt, flags);
	}

	if(flags & SendFlag_Presence)
	{
		CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
	}
}

void CUGCStatUpdatePresenceEvent::Trigger(const rlGamerHandle* pRecips, const int numRecips, const scrUGCStateUpdate_Data* pData)
{
	if(!gnetVerifyf(pRecips && numRecips > 0, "Invalid recip list in CUGCStatUpdatePresenceEvent::Trigger"))
	{
		return;
	}

	CUGCStatUpdatePresenceEvent evt;

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();	
	if(!gnetVerify(rlPresence::GetGamerHandle(localGamerIndex, &evt.m_fromGH) && rlPresence::GetDisplayName(localGamerIndex, evt.m_fromGamerDisplayName)))
	{
		return;
	}

	safecpy(evt.m_missionContentId, pData->m_contentId);
	evt.m_score = pData->m_score.Int;
	evt.m_score2 = pData->m_score2.Int;
	safecpy(evt.m_missionName, pData->m_missionName);
	safecpy(evt.m_CoPlayerDisplayName, pData->m_CoPlayerName);
	evt.m_missionType = pData->m_MissionType.Int;
	evt.m_missionSubType = pData->m_MissionSubType.Int;
	evt.m_raceLaps = pData->m_Laps.Int;
	evt.m_swapSenderWithCoplayer = pData->m_swapSenderWithCoPlayer.Int != 0;

	SC_INBOX_MGR.SendEventToGamers(evt, SendFlag_Inbox, pRecips, numRecips);
}

bool CUGCStatUpdatePresenceEvent::Serialise(RsonWriter* rw) const
{
	rtry
	{
		char ghStr[RL_MAX_GAMER_HANDLE_CHARS];

		rverifyall(rw->WriteString("mid", m_missionContentId));
		rverifyall(rw->WriteInt("scr", m_score));
		rverifyall(rw->WriteInt("sc2", m_score2));
		rverifyall(rw->WriteString("from", m_fromGamerDisplayName));
		rverifyall(m_fromGH.ToString(ghStr));
		rverifyall(rw->WriteString("gh", ghStr));
		rverifyall(rw->WriteString("mn", m_missionName));

		//Some data may not be there, so only write it conditionally.
		if(strlen(m_CoPlayerDisplayName) > 0)
		{
			//If the co player is the sender, encode that in the name string
			// GamerName#
			const unsigned MAX_STRING_LENGTH = RL_MAX_DISPLAY_NAME_BUF_SIZE + 2;
			char tempStr[MAX_STRING_LENGTH];
			formatf(tempStr, "%s%s", m_CoPlayerDisplayName, m_swapSenderWithCoplayer ? "#" : "");
			rverifyall(rw->WriteString("cp", tempStr));
		}

		if(m_missionType >= 0 || m_missionSubType >= 0 || m_raceLaps > 0)
		{
			char tempStr[32];
			formatf(tempStr, "%d:%d:%d", m_missionType, m_missionSubType, m_raceLaps);
			rverifyall(rw->WriteString("mti", tempStr));
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CUGCStatUpdatePresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		char ghStr[RL_MAX_GAMER_HANDLE_CHARS];
		rverifyall(rr->ReadString("mid", m_missionContentId));
		rverifyall(rr->ReadInt("scr", m_score));
		rverifyall(rr->ReadInt("sc2", m_score2));
		rverifyall(rr->ReadString("from", m_fromGamerDisplayName));
		rverifyall(rr->ReadString("gh", ghStr));
		rverifyall(m_fromGH.FromString(ghStr));
		rverifyall(rr->ReadString("mn", m_missionName));

		// check on CoPlayer info
		if(rr->HasMember("cp"))
		{
			const unsigned MAX_STRING_LENGTH = RL_MAX_DISPLAY_NAME_BUF_SIZE + 2;
			char tempStr[MAX_STRING_LENGTH];
			rverifyall(rr->ReadString("cp", tempStr));

			// check if it has the '#' thing at the end signifying a swap is necessary
			char* swapHashChar = strchr(tempStr, '#');
			if(swapHashChar)
			{
				m_swapSenderWithCoplayer = true;
				*swapHashChar = '\0';  //replace with 0 to terminate the string
			}

			safecpy(m_CoPlayerDisplayName, tempStr);
		}
		else
		{
			m_CoPlayerDisplayName[0] = '\0';
			m_swapSenderWithCoplayer = false;
		}

		// mission type stuff
		bool hasValidMissionData = false;
		if(rr->HasMember("mti"))
		{
			char tempStr[32];
			rverifyall(rr->ReadString("mti", tempStr));
			rverifyall(sscanf(tempStr, "%d:%d:%d", &m_missionType, &m_missionSubType, &m_raceLaps) == 3);
			hasValidMissionData = true; 
		}
		
		if(!hasValidMissionData)
		{
			m_missionType = -1;
			m_missionSubType = -1;
			m_raceLaps = 0;
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CUGCStatUpdatePresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender : todo : check whether we need to validate this in script or can reject in code under certain conditions
	gnetError("CUGCStatUpdatePresenceEvent::Apply is not implemented since it is handled by script");
	return false;
}

#if RSG_BANK
char bank_UGCStatUpdate_contentId[RLUGC_MAX_CONTENTID_CHARS];
int bank_UGCStatUpdate_score = 0;
void Bank_SendUGCStatUpdate()
{
	CUGCStatUpdatePresenceEvent::Trigger(bank_UGCStatUpdate_contentId, bank_UGCStatUpdate_score, Helper_GetSendFlags());
}

void CUGCStatUpdatePresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(bank_UGCStatUpdate_contentId, "INVALID");
	bank.PushGroup("CUGCStatUpdatePresenceEvent");
	bank.AddText("Content ID", bank_UGCStatUpdate_contentId, sizeof(bank_UGCStatUpdate_contentId), false);
	bank.AddText("Score", &bank_UGCStatUpdate_score, false);
	bank.AddButton("Send", CFA(Bank_SendUGCStatUpdate));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CRockstarMsgPresenceEvent 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CRockstarMsgPresenceEvent_Base::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		if(m_bCloudString)
		{
			rverifyall(rw->WriteString("cloud", m_cloudStringKey));
		}
		else
		{
			rverifyall(rw->WriteString("msg", m_message));
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CRockstarMsgPresenceEvent_Base::Deserialise(const RsonReader* rr)
{
	rtry
	{
		if(rr->HasMember("cloud")) //Old way
		{
			m_bCloudString = true;
			rverifyall(rr->ReadString("cloud", m_cloudStringKey));
		}
		else if(rr->HasMember("c")) //NEW way
		{
			m_bCloudString = true;
			rverifyall(rr->ReadString("c", m_cloudStringKey));
		}
		else
		{
			m_bCloudString = false;
			rverifyall(rr->ReadString("msg", m_message));
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CRockstarMsgPresenceEvent_Base::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)

	// when it comes in, if it's a cloud message, we need to request the text of the message first.
	if(m_bCloudString)
	{
		if(DoCloudTextRequest())
		{
			//we've been deferred and push off.
			return true;
		}
		else
		{
			return false;
		}
	}
	else //Otherwise, the message is there already.
	{
		return HandleTextReceived();
	}
}

bool CRockstarMsgPresenceEvent_Base::DoCloudTextRequest() const
{
	//Make the request if we haven't already and push ourselves into a list to wait.
	if(STRINGS_IN_CLOUD.ReqestString(m_cloudStringKey))
	{
		//Now add it to the deferred list and it will monitored and dealt with
		AddToDeferredList();
		return true;
	}
	else
	{
		netError("'%s'::Apply failed because request for cloud string '%s' failed to start", GetEventName(), m_cloudStringKey);
		return false;
	}
}

bool CRockstarMsgPresenceEvent_Base::Update()
{
	//Check to see if the string is already around.
	if(!STRINGS_IN_CLOUD.IsRequestPending(m_cloudStringKey))
	{
		//If it hasn't failed, send it to the game stream
		if(!STRINGS_IN_CLOUD.IsRequestFailed(m_cloudStringKey))
		{
			//Our text should be here.  Handle it
			HandleTextReceived();

			//returning false because our time is done.
			return false;
		}
		else
		{
			gnetError("'%s'::Apply failed because request for cloud string '%s' failed", GetEventName(), m_cloudStringKey);
			return false; //returning false because our time is done.
		}	
	}
	else
	{
		return true; //Keep returning true until our time has come.
	}
}

const char* CRockstarMsgPresenceEvent_Base::GetText() const
{
	//When it comes in, if it's a cloud message, we need to request the text of the message first.
	if(m_bCloudString)
	{
		const char* returnString = STRINGS_IN_CLOUD.GetStringForRequest(m_cloudStringKey);
		gnetAssertf(strlen(returnString) > 0, "Cloud string'%s' for '%s' event was empty", m_cloudStringKey, GetEventName());
		return returnString;
	}
	else
	{
		return m_message;
	}
}

void CRockstarMsgPresenceEvent_Base::ReleaseCloudRequest() const
{
	if(m_bCloudString)
		STRINGS_IN_CLOUD.ReleaseRequest(m_cloudStringKey);
}

//================================================================================================================
// CRockstarMsgPresenceEvent 
//================================================================================================================
bool CRockstarMsgPresenceEvent::ms_bCacheNewMsg = false;
bool CRockstarMsgPresenceEvent::ms_bHasNewMsg = false;
atString CRockstarMsgPresenceEvent::ms_sLastMsg; 

bool CRockstarMsgPresenceEvent::HandleTextReceived() const
{
	gnetDebug1("CRockstarMsgPresenceEvent::HandleTextReceived");

    // check if we have a tunable to display R* messages
    if(!Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_ROCKSTARMESSAGE", 0x3e20c65a), false))
    {
        gnetDebug1("CRockstarMsgPresenceEvent::HandleTextReceived :: No tunable override");
        return false;
    }

    //Make sure we have some valid text
	const char* msgText = GetText();
	if(msgText == nullptr || strlen(msgText) == 0)
	{
		return false;
	}

	char FinalString[2 * MAX_CHARS_IN_MESSAGE];

	char* pMainString = TheText.Get("FEED_ROCKSTAR_MSG");

	CSubStringWithinMessage aSubStrings[1];
	aSubStrings[0].SetLiteralString(msgText, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
	CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
		nullptr, 0, 
		aSubStrings, 1, 
		FinalString, NELEM(FinalString));
	
	if(ms_bCacheNewMsg)
	{
		// Cache new message for script to retrieve later
		ms_sLastMsg = FinalString;
		ms_bHasNewMsg = true;
		gnetDebug1("Caching Rockstar SC Admin message for script: %s", ms_sLastMsg.c_str());
	}
	else
	{
		// We're not caching messages for script, let's post out to the feed
		CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
		if(pGameStream != nullptr)
		{
			pGameStream->PostTicker(FinalString, true);
		}

		g_FrontendAudioEntity.PlaySound("GOD_PAGE_MSG","HUD_FRONTEND_DEFAULT_SOUNDSET");
	}

	ReleaseCloudRequest();

	return true;
}

netDeferredGamePresenceEventIntf* CRockstarMsgPresenceEvent::CreateCopy() const
{
	CRockstarMsgPresenceEvent* pTheNewOne = rage_new CRockstarMsgPresenceEvent();
	*pTheNewOne = *this;
	return pTheNewOne;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CRockstarMsgPresenceEvent::Trigger_Debug(const char* msg, SendFlag flags, bool bUseCloudLoc)
{
	CRockstarMsgPresenceEvent evt;
	if(bUseCloudLoc)
	{
		evt.m_bCloudString = true;
		safecpy(evt.m_cloudStringKey , msg);
	}
	else
	{
		safecpy(evt.m_message, msg);
	}
	
	if(flags & SendFlag_Inbox)
		SC_INBOX_MGR.SendEventToCommunity(evt, flags);
	else
		CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
}
#endif

#if RSG_BANK
char bank_RockstarMessageString[128];
bool bank_RockstarMessageBCloudLoc = false;
void Bank_SendRockstarMsg()
{
	CRockstarMsgPresenceEvent::Trigger_Debug(bank_RockstarMessageString, Helper_GetSendFlags(), bank_RockstarMessageBCloudLoc);
}

void CRockstarMsgPresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(bank_RockstarMessageString, "princess");
	bank_RockstarMessageBCloudLoc = false;

	bank.PushGroup("CRockstarMsgPresenceEvent");
	bank.AddToggle("Cloud Localized", &bank_RockstarMessageBCloudLoc);
	bank.AddText("Message", bank_RockstarMessageString, sizeof(bank_RockstarMessageString), false);
	bank.AddButton("Send", CFA(Bank_SendRockstarMsg));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CRockstarCrewMsgPresenceEvent 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CRockstarCrewMsgPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(CRockstarMsgPresenceEvent_Base::Serialise_Debug(rw));
		rverifyall(rw->WriteInt("crewId", static_cast<int>(m_clanId)));
		rverifyall(rw->WriteString("crewTag", m_clanTag));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CRockstarCrewMsgPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int readCrewId = 0;
		rverifyall(CRockstarMsgPresenceEvent_Base::Deserialise(rr));
		rverifyall(rr->ReadInt("crewId", readCrewId));
		rverifyall(rr->ReadString("crewTag", m_clanTag));

		m_clanId = static_cast<rlClanId>(readCrewId);;
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CRockstarCrewMsgPresenceEvent::HandleTextReceived() const
{
	gnetDebug1("CRockstarCrewMsgPresenceEvent::HandleTextReceived");

	// check if crew messages should be disabled
	if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_DISABLE_ALL_PRESENCE_CREW_MESSAGES", 0x64bb2623), false))
	{
		gnetDebug1("CRockstarCrewMsgPresenceEvent::HandleTextReceived :: Blocking (all) due to tunable");
		return false;
	}

	// grab the clan data here to check clan specific tunable
	const rlClanMembershipData* pClanData = CLiveManager::GetNetworkClan().GetLocalMembershipDataByClanId(m_clanId);

	// check if crew messages (without a non-local crew) should be disabled
	if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_DISABLE_PRESENCE_CREW_MESSAGES_WHEN_NOT_IN_CREW", 0x1670910c), true))
	{
		if(pClanData == nullptr)
		{
			gnetDebug1("CRockstarCrewMsgPresenceEvent::HandleTextReceived :: Blocking due to tunable disabling non-crew messages. CrewId: %d", static_cast<int>(m_clanId));
			return false;
		}
	}

	CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
	if(pGameStream != nullptr)
	{
		//Make sure we have some valid text
		const char* msgText = GetText();
		if(msgText == nullptr || strlen(msgText) == 0)
		{
			return false;
		}

		char FinalString[2 * MAX_CHARS_IN_MESSAGE];

		char* pMainString = TheText.Get("FEED_CREW_MSG");

		CSubStringWithinMessage aSubStrings[1];
		aSubStrings[0].SetLiteralString(msgText, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);
		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			nullptr, 0, 
			aSubStrings, 1, 
			FinalString, NELEM(FinalString));

		if(pClanData)
		{
			// If we found a matching clan (this should always be the case), retrieve the clan desc information to post to the feed
			rlClanDesc clanDesc = pClanData->m_Clan;
			pGameStream->PostCrewTag(!clanDesc.m_IsOpenClan, false, clanDesc.m_ClanTag, pClanData->m_Rank.m_RankOrder, false, FinalString, true, m_clanId, nullptr, Color32(clanDesc.m_clanColor));
		}
		else
		{
			// For some reason the player was not in the clan the message was designated for, make default crew tag post
			pGameStream->PostCrewTag(false, false, m_clanTag, 0, false, FinalString, true, m_clanId, nullptr, Color32());
		}

		g_FrontendAudioEntity.PlaySound("GOD_PAGE_MSG","HUD_FRONTEND_DEFAULT_SOUNDSET");

		ReleaseCloudRequest();
	}

	return true;
}

netDeferredGamePresenceEventIntf* CRockstarCrewMsgPresenceEvent::CreateCopy() const
{
	CRockstarCrewMsgPresenceEvent* pTheNewOne = rage_new CRockstarCrewMsgPresenceEvent();
	*pTheNewOne = *this;
	return pTheNewOne;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CRockstarCrewMsgPresenceEvent::Trigger_Debug(const char* msg, rlClanId clanId, const char* clanTag, SendFlag flags)
{
	CRockstarCrewMsgPresenceEvent evt;
	evt.m_clanId = clanId;
	safecpy(evt.m_clanTag, clanTag);
	safecpy(evt.m_message, msg);

	if(flags & SendFlag_Inbox)
	{
		SC_INBOX_MGR.SendEventToCommunity(evt, flags);
	}
	else
	{
		CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
	}
}
#endif

#if RSG_BANK
int bank_CrewMsg_CrewId = 724;
char bank_CrewMsg_CrewTag[RL_CLAN_TAG_MAX_CHARS];
char bank_CrewMsg_Msg[128];
void Bank_SendCrewMessageEvent()
{
	CRockstarCrewMsgPresenceEvent::Trigger_Debug(bank_CrewMsg_Msg, (rlClanId)bank_CrewMsg_CrewId,  bank_CrewMsg_CrewTag, Helper_GetSendFlags());
}

void CRockstarCrewMsgPresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(bank_CrewMsg_CrewTag, "CREW");
	safecpy(bank_CrewMsg_Msg, "Hey CREW, Here's a message");
	bank.PushGroup("CRockstarCrewMsgPresenceEvent");
		bank.AddButton("Send", CFA(Bank_SendCrewMessageEvent));
	bank.PopGroup();
}
#endif

//================================================================================================================
// CGameAwardPresenceEvent 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CGameAwardPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteUns("type", m_awardTypeHash));
		rverifyall(rw->WriteFloat("amt", m_awardAmount));
		rverifyall(rw->WriteString("from", m_from));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CGameAwardPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr->ReadUns("type", m_awardTypeHash));
		rverifyall(rr->ReadFloat("amt", m_awardAmount));
		rverifyall(rr->ReadString("from", m_from));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CGameAwardPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	
	if (m_awardTypeHash == ATSTRINGHASH("cash", 0x69E6CD7E))
	{
		gnetDebug1("Recieved CGameAwardPresenceEvent for CASH of %6.2f", m_awardAmount);
	}
	else if(m_awardTypeHash == ATSTRINGHASH("xp", 0x168AA1BA))
	{
		gnetDebug1("Recieved CGameAwardPresenceEvent for XP of %6.2f", m_awardAmount);
	}

	return true;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CGameAwardPresenceEvent::Trigger_Debug(u32 awardTypeHash, float value, SendFlag flags, const rlGamerHandle* pRecips, const int numRecips)
{
	CGameAwardPresenceEvent evt;
	evt.m_awardTypeHash = awardTypeHash;
	evt.m_awardAmount = value;

	//Sent only via web except from bank for testing?
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();	
	if(!gnetVerify(rlPresence::GetDisplayName(localGamerIndex, evt.m_from)))
	{
		return false;
	}

	if(flags & SendFlag_Inbox)
	{
		SC_INBOX_MGR.SendEventToGamers(evt, flags, pRecips, numRecips);
	}
	else if(flags & SendFlag_Presence)
	{
		gnetAssertf(pRecips == nullptr && numRecips == 0, "Not ready to send messages to gamers via presence");
		CGamePresenceEventDispatcher::SendEventToCommunity(evt, flags);
	}

	return true;
}
#endif

 #if RSG_BANK
char bankGameAward_Type[64];
float bankGameAward_Amt = 0.0f;
void Bank_GameAward_Send()
{
	CGameAwardPresenceEvent::Trigger_Debug(atStringHash(bankGameAward_Type), bankGameAward_Amt, Helper_GetSendFlags(), nullptr, 0);
}

void CGameAwardPresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(bankGameAward_Type, "cash");
	bankGameAward_Amt = 1000.0f;
	bank.PushGroup("CGameAwardPresenceEvent");
		bank.AddText("Type", bankGameAward_Type, sizeof(bankGameAward_Type), false);
		bank.AddText("Amount", &bankGameAward_Amt, false);
		bank.AddButton("Send", CFA(Bank_GameAward_Send));
	bank.PopGroup();
}
#endif //RSG_BANK

//================================================================================================================
// CVoiceSessionInvite 
//================================================================================================================
void CVoiceSessionInvite::Trigger(const rlGamerHandle& hTo, const rlSessionInfo& hSessionInfo)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	CVoiceSessionInvite evt(hSessionInfo, NetworkInterface::GetLocalGamerHandle(), NetworkInterface::GetLocalGamerName());
	CGamePresenceEventDispatcher::SendEventToGamers(evt, &hTo, 1);
}

bool CVoiceSessionInvite::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("si", m_szSessionInfo));
		rverifyall(rw->WriteBinary("gh", &m_hGamer, sizeof(rlGamerHandle)));
		rverifyall(rw->WriteString("gn", m_szGamerDisplayName));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CVoiceSessionInvite::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int nLength = 0;
		rverifyall(rr->ReadString("si", m_szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(rr->ReadBinary("gh", &m_hGamer, sizeof(rlGamerHandle), &nLength));
		rverifyall(rr->ReadString("gn", m_szGamerDisplayName));
		rverifyall(m_SessionInfo.FromString(m_szSessionInfo));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CVoiceSessionInvite::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CVoiceSessionInvite :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CVoiceSessionInvite :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hGamer.ToString(), sender.ToString());
		m_hGamer = sender;
	}

	CNetwork::GetNetworkVoiceSession().OnReceivedInvite(m_SessionInfo, m_hGamer, m_szGamerDisplayName);
	return true;
}

//================================================================================================================
// CVoiceSessionResponse 
//================================================================================================================
void CVoiceSessionResponse::Trigger(const rlGamerHandle& hTo, const unsigned nResponse, const int nResponseCode)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	CVoiceSessionResponse evt(nResponse, nResponseCode, NetworkInterface::GetLocalGamerHandle());
	CGamePresenceEventDispatcher::SendEventToGamers(evt, &hTo, 1);
}

bool CVoiceSessionResponse::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteUns("r", m_nResponse));
		rverifyall(rw->WriteInt("rc", m_nResponseCode));
		rverifyall(rw->WriteBinary("gh", &m_hGamer, sizeof(rlGamerHandle)));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CVoiceSessionResponse::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int nLength = 0;
		rverifyall(rr->ReadUns("r", m_nResponse));
		rverifyall(rr->ReadInt("rc", m_nResponseCode));
		rverifyall(rr->ReadBinary("gh", &m_hGamer, sizeof(rlGamerHandle), &nLength));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CVoiceSessionResponse::Apply(const rlGamerInfo&, const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CVoiceSessionResponse :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CVoiceSessionResponse :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hGamer.ToString(), sender.ToString());
		m_hGamer = sender;
	}

	CNetwork::GetNetworkVoiceSession().OnReceivedResponse(m_nResponse, m_nResponseCode, m_hGamer);
	return true;
}

//================================================================================================================
// CGameInvite 
//================================================================================================================
void CGameInvite::Trigger(
	const rlGamerHandle* hTo, 
	const int nRecipients, 
	const rlSessionInfo& hSessionInfo, 
	const char* szContentID, 
	const rlGamerHandle& hContentCreator, 
	const char* pUniqueMatchId, 
	const int nInviteFrom, 
	const int nInviteType, 
	const unsigned nPlaylistLength, 
	const unsigned nPlaylistCurrent, 
	const unsigned nFlags)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	unsigned nInviteFlags = nFlags;

#if RSG_XDK
	if(CLiveManager::IsOverallReputationBad())
		nInviteFlags |= InviteFlags::IF_BadReputation;
#endif

	CGameInvite tInvite(hSessionInfo, NetworkInterface::GetLocalGamerHandle(), NetworkInterface::GetLocalGamerName(), CNetwork::GetNetworkGameMode(), szContentID, hContentCreator, pUniqueMatchId, nInviteFrom, nInviteType, nPlaylistLength, nPlaylistCurrent, nInviteFlags);
	tInvite.m_InviteId = InviteMgr::GenerateInviteId();
	tInvite.m_StaticDiscriminatorEarly = NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_Early);
	tInvite.m_StaticDiscriminatorInGame = NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_InGame);

#if !__NO_OUTPUT
	gnetDebug1("CGameInvite::Trigger :: InviteId: 0x%08x, Disc: 0x%08x / 0x%08x, Token: 0x%016" I64FMT "x, ContentId: %s",
		tInvite.m_InviteId,
		tInvite.m_StaticDiscriminatorEarly,
		tInvite.m_StaticDiscriminatorInGame,
		tInvite.m_SessionInfo.GetToken().GetValue(), 
		tInvite.m_szContentId);

	for(unsigned i = 0; i < nRecipients; i++)
		gnetDebug1("\tTo: %s", NetworkUtils::LogGamerHandle(hTo[i]));
#endif

	CGamePresenceEventDispatcher::SendEventToGamers(tInvite, hTo, nRecipients);
}

void CGameInvite::TriggerFromJoinQueue(const rlGamerHandle& hTo, const rlSessionInfo& hSessionInfo, unsigned nJoinQueueToken)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	unsigned nFlags = 0;

#if RSG_XDK
	if(CLiveManager::IsOverallReputationBad())
		nFlags |= InviteFlags::IF_BadReputation;
#endif

	CGameInvite tInvite(hSessionInfo, NetworkInterface::GetLocalGamerHandle());
	tInvite.m_InviteId = InviteMgr::GenerateInviteId();
	tInvite.m_StaticDiscriminatorEarly = NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_Early);
	tInvite.m_StaticDiscriminatorInGame = NetworkGameConfig::GetStaticDiscriminator(StaticDiscriminatorType::Type_InGame);
	tInvite.m_JoinQueueToken = nJoinQueueToken; 
	tInvite.m_Flags = nFlags;

	gnetDebug1("CGameInvite::TriggerFromJoinQueue :: InviteId: 0x%08x, Disc: 0x%08x / 0x%08x, Token: 0x%016" I64FMT "x, ContentId: %s", 
		tInvite.m_InviteId, 
		tInvite.m_StaticDiscriminatorEarly,
		tInvite.m_StaticDiscriminatorInGame,
		tInvite.m_SessionInfo.GetToken().GetValue(), 
		tInvite.m_szContentId);

	CGamePresenceEventDispatcher::SendEventToGamers(tInvite, &hTo, 1);
}

bool CGameInvite::Serialise(RsonWriter* rw) const
{
	rtry
	{
		const unsigned nVersion = GAME_INVITE_VERSION;

		rverifyall(rw->WriteUns("v", nVersion));
		rverifyall(rw->WriteString("s", m_szSessionInfo));
		rverifyall(rw->WriteBinary("h", &m_hGamer, sizeof(rlGamerHandle)));
		rverifyall(rw->WriteString("n", m_szGamerDisplayName));
		rverifyall(rw->WriteInt("gm", m_GameMode));
		rverifyall(rw->WriteString("c", m_szContentId));
		rverifyall(rw->WriteBinary("cc", &m_hContentCreator, sizeof(rlGamerHandle)));

		rverifyall(rw->WriteString("mid", m_UniqueMatchId));
		rverifyall(rw->WriteInt("if", m_InviteFrom));
		rverifyall(rw->WriteInt("it", m_InviteType));

		rverifyall(rw->WriteUns("l", m_PlaylistLength));
		rverifyall(rw->WriteUns("p", m_PlaylistCurrent));
		rverifyall(rw->WriteUns("f", m_Flags));

		// optional join queue
		if(m_JoinQueueToken != 0)
			rverifyall(rw->WriteUns("jq", m_JoinQueueToken));

		// discriminator and inviteId
		rverifyall(rw->WriteUns("ed", m_StaticDiscriminatorEarly));
		rverifyall(rw->WriteUns("gd", m_StaticDiscriminatorInGame));
		rverifyall(rw->WriteUns("u", m_InviteId));

		// send crew ID if we have one
		NetworkClan& tClan = CLiveManager::GetNetworkClan();
		if(tClan.HasPrimaryClan())
		{
			unsigned nCrewId = static_cast<unsigned>(tClan.GetPrimaryClan()->m_Id);
			rverifyall(rw->WriteUns("cr", nCrewId));
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CGameInvite::Deserialise(const RsonReader* rr)
{
	int nLength = 0;
    unsigned nVersion; 

	rtry
	{
		// for security, verify that some key fields do not have duplicate entries
		rverifyall(!rr->HasDuplicateEntries("v"));
		rverifyall(!rr->HasDuplicateEntries("h"));
		rverifyall(!rr->HasDuplicateEntries("n"));
		rverifyall(!rr->HasDuplicateEntries("f")); 
		
		rverifyall(rr->ReadUns("v", nVersion));
		rcheck(nVersion == GAME_INVITE_VERSION, catchall, gnetDebug1("CGameInvite :: Rejecting invite with different GAME_INVITE_VERSION"));

		rverifyall(rr->ReadString("s", m_szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(rr->ReadBinary("h", &m_hGamer, sizeof(rlGamerHandle), &nLength));
		rverifyall(rr->ReadString("n", m_szGamerDisplayName));
		rverifyall(rr->ReadInt("gm", m_GameMode));
		rverifyall(rr->ReadString("c", m_szContentId));
		rverifyall(rr->ReadBinary("cc", &m_hContentCreator, sizeof(rlGamerHandle), &nLength));

		rverifyall(rr->ReadString("mid", m_UniqueMatchId));
		rverifyall(rr->ReadInt("if", m_InviteFrom));
		rverifyall(rr->ReadInt("it", m_InviteType));

		rverifyall(rr->ReadUns("l", m_PlaylistLength));
		rverifyall(rr->ReadUns("p", m_PlaylistCurrent));
		rverifyall(rr->ReadUns("f", m_Flags));

		// optional from join queue
		m_JoinQueueToken = 0;
		rr->ReadUns("jq", m_JoinQueueToken);

		// discriminator and inviteId
		rverifyall(rr->ReadUns("ed", m_StaticDiscriminatorEarly));
		rverifyall(rr->ReadUns("gd", m_StaticDiscriminatorInGame));
		rverifyall(rr->ReadUns("u", m_InviteId));

		// may not be sent if inviter does not have a crew
		m_CrewID = RL_INVALID_CLAN_ID;
		rr->ReadUns("cr", m_CrewID);

		// fail silently, this can pull data socially and cross version
		rverifyall(m_SessionInfo.FromString(m_szSessionInfo, nullptr, true));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CGameInvite::Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CGameInvite :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CGameInvite :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hGamer.ToString(), sender.ToString());
		m_hGamer = sender;
	}

	if(m_JoinQueueToken != 0)
	{
		gnetDebug1("CGameInvite :: Receiving join queue request from %s with Id: 0x%08x, Disc: 0x%08x / 0x%08x, Token: 0x%016" I64FMT "x, ContentId: %s, JoinQueueToken: 0x%08x", m_szGamerDisplayName, m_InviteId, m_StaticDiscriminatorEarly, m_StaticDiscriminatorInGame, m_SessionInfo.GetToken().GetValue(), m_szContentId, m_JoinQueueToken);
		CLiveManager::GetInviteMgr().HandleInviteFromJoinQueue(m_SessionInfo, gamerInfo, m_hGamer, m_InviteId, m_JoinQueueToken, InviteFlags::IF_ViaJoinQueue);
	}
	else
	{
		gnetDebug1("CGameInvite :: Receiving invite from %s with Id: 0x%08x, Disc: 0x%08x / 0x%08x, Token: 0x%016" I64FMT "x, ContentId: %s, Flags: 0x%08x", m_szGamerDisplayName, m_InviteId, m_StaticDiscriminatorEarly, m_StaticDiscriminatorInGame, m_SessionInfo.GetToken().GetValue(), m_szContentId, m_Flags);
		CLiveManager::GetInviteMgr().AddRsInvite(gamerInfo, m_SessionInfo, m_hGamer, m_szGamerDisplayName, m_StaticDiscriminatorEarly, m_StaticDiscriminatorInGame, m_InviteId, m_GameMode, m_CrewID, m_szContentId, m_hContentCreator, m_UniqueMatchId, m_InviteFrom, m_InviteType, m_PlaylistLength, m_PlaylistCurrent, m_Flags);
	}

	return true;
}

//================================================================================================================
// CGameInviteCancel 
//================================================================================================================
void CGameInviteCancel::Trigger(const rlGamerHandle& hTo, const rlSessionInfo& hSessionInfo)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	CGameInviteCancel evt(hSessionInfo, NetworkInterface::GetLocalGamerHandle());
	CGamePresenceEventDispatcher::SendEventToGamers(evt, &hTo, 1);
}

void CGameInviteCancel::Trigger(const rlGamerHandle* hTo, const unsigned gamerHandleCount, const rlSessionInfo& hSessionInfo)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	CGameInviteCancel evt(hSessionInfo, NetworkInterface::GetLocalGamerHandle());
	CGamePresenceEventDispatcher::SendEventToGamers(evt, hTo, gamerHandleCount);
}

bool CGameInviteCancel::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("si", m_szSessionInfo));
		rverifyall(rw->WriteBinary("gh", &m_hGamer, sizeof(rlGamerHandle)));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CGameInviteCancel::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int nLength = 0;
		rverifyall(rr->ReadString("si", m_szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(rr->ReadBinary("gh", &m_hGamer, sizeof(rlGamerHandle), &nLength));
		rverifyall(m_SessionInfo.FromString(m_szSessionInfo));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CGameInviteCancel::Apply(const rlGamerInfo&, const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CGameInviteCancel :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CGameInviteCancel :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hGamer.ToString(), sender.ToString());
		m_hGamer = sender;
	}

	CLiveManager::GetInviteMgr().CancelRsInvite(m_SessionInfo, m_hGamer);
	return true;
}

//================================================================================================================
// CGameInviteReply 
//================================================================================================================
void CGameInviteReply::Trigger(const rlGamerHandle& hTo, const rlSessionToken& sessionToken, const unsigned inviteId, const unsigned responseFlags, const unsigned inviteFlags, const int characterRank, const char* clanTag, const bool bDecisionMade, const bool bDecision)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	gnetDebug1("CGameInviteReply :: Sending to %s - InviteId: 0x%08x, ResponseFlags: %x0x, InviteFlags: 0x%x, DecisionMade: %s, Decision: %s", NetworkUtils::LogGamerHandle(hTo), inviteId, responseFlags, inviteFlags, bDecisionMade ? "True" : "False", bDecisionMade ? (bDecision ? "Accepted" : "Rejected") : "TBD");

	CGameInviteReply evt(NetworkInterface::GetLocalGamerHandle(), sessionToken, inviteId, responseFlags, inviteFlags, characterRank, clanTag, bDecisionMade, bDecision);
	CGamePresenceEventDispatcher::SendEventToGamers(evt, &hTo, 1);
}

bool CGameInviteReply::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteBinary("gh", &m_Invitee, sizeof(rlGamerHandle)));
		rverifyall(rw->WriteUns64("st", m_SessionToken.m_Value));
		rverifyall(rw->WriteUns("u", m_InviteId));
		rverifyall(rw->WriteUns("rf", m_ResponseFlags));
		rverifyall(rw->WriteUns("if", m_InviteFlags));
		rverifyall(rw->WriteInt("cr", m_CharacterRank));
		rverifyall(rw->WriteString("ct", m_ClanTag));
		rverifyall(rw->WriteBool("dm", m_bDecisionMade));
		rverifyall(rw->WriteBool("d", m_bDecision));
	}
	rcatchall
	{
		return false;
	}  
	return true;
}

bool CGameInviteReply::Deserialise(const RsonReader* rr)
{
	rtry
	{
		// for security, verify that some key fields do not have duplicate entries
		rverifyall(!rr->HasDuplicateEntries("gh"));
		rverifyall(!rr->HasDuplicateEntries("if"));
		rverifyall(!rr->HasDuplicateEntries("rf"));

		int nLength = 0;
		rverifyall(rr->ReadBinary("gh", &m_Invitee, sizeof(rlGamerHandle), &nLength));
		rverifyall(rr->ReadUns64("st", m_SessionToken.m_Value));
		rverifyall(rr->ReadUns("u", m_InviteId));
		rverifyall(rr->ReadUns("rf", m_ResponseFlags));
		rverifyall(rr->ReadUns("if", m_InviteFlags));
		rverifyall(rr->ReadInt("cr", m_CharacterRank));
		rverifyall(rr->ReadString("ct", m_ClanTag));
		rverifyall(rr->ReadBool("dm", m_bDecisionMade));
		rverifyall(rr->ReadBool("d", m_bDecision));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CGameInviteReply::Apply(const rlGamerInfo&, const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_Invitee != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CGameInviteReply :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_Invitee.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CGameInviteReply :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_Invitee.ToString(), sender.ToString());
		m_Invitee = sender;
	}

	gnetDebug1("CGameInviteReply :: Received from %s - InviteId: 0x%08x, ResponseFlags: 0x%x, DecisionMade: %s, Decision: %s", NetworkUtils::LogGamerHandle(m_Invitee), m_InviteId, m_ResponseFlags, m_bDecisionMade ? "True" : "False", m_bDecisionMade ? (m_bDecision ? "Accepted" : "Rejected") : "TBD");
    CLiveManager::GetInviteMgr().OnInviteResponseReceived(m_Invitee, m_SessionToken, m_InviteId, m_ResponseFlags, m_InviteFlags, m_CharacterRank, m_ClanTag, m_bDecisionMade, m_bDecision);
    return true;
}

//================================================================================================================
// CTournamentInvite 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
void CTournamentInvite::Trigger_Debug(const rlGamerHandle& target)
{
	if(!CNetwork::GetNetworkSession().IsSessionActive())
		return;

	rlSessionInfo hInfo = CNetwork::GetNetworkSession().GetSnSession().GetSessionInfo();

	CTournamentInvite tEvent(hInfo, "", 0, false);
	CGamePresenceEventDispatcher::SendEventToGamers(tEvent, &target, 1);
}
#endif

#if RSG_BANK
char s_Bank_TournamentInvite_UserId[64];
void Bank_CTournamentInvite_SendInvite()
{
	rlGamerHandle hGamer;
	hGamer.FromUserId(s_Bank_TournamentInvite_UserId);

	CTournamentInvite::Trigger_Debug(hGamer);
}

void CTournamentInvite::AddWidgets(bkBank& bank)
{
	bank.PushGroup("CTournamentInvite");
	bank.AddText("User ID", s_Bank_TournamentInvite_UserId, sizeof(s_Bank_TournamentInvite_UserId), false);
	bank.AddButton("Send Invite", CFA(Bank_CTournamentInvite_SendInvite));
	bank.PopGroup();
}
#endif

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CTournamentInvite::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("s", m_szSessionInfo));
		rverifyall(rw->WriteString("c", m_szContentId));
		rverifyall(rw->WriteUns("e", m_TournamentEventId));
		rverifyall(rw->WriteUns("f", m_InviteFlags));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CTournamentInvite::Deserialise(const RsonReader* rr)
{
	rtry
	{
		// for security, verify that some key fields do not have duplicate entries
		rverifyall(!rr->HasDuplicateEntries("f"));
		rverifyall(!rr->HasDuplicateEntries("h"));

		char szGamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
		rverifyall(rr->ReadString("s", m_szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(rr->ReadString("h", szGamerHandle, RL_MAX_GAMER_HANDLE_CHARS));
		rverifyall(rr->ReadString("c", m_szContentId));
		rverifyall(rr->ReadUns("e", m_TournamentEventId));

		rverifyall(m_hSessionHost.FromString(szGamerHandle));
		rverifyall(m_SessionInfo.FromString(m_szSessionInfo));

		m_InviteFlags = InviteFlags::IF_None;
		rr->ReadUns("f", m_InviteFlags);

		// get admin to switch to flags but support the deprecated individual parameters for now
		unsigned value = 0;
		if(rr->ReadUns("t", value) && value == 1)
			m_InviteFlags |= InviteFlags::IF_Spectator;
		if(rr->ReadUns("ad", value) && value == 1)
			m_InviteFlags |= InviteFlags::IF_IsRockstarDev;
	}
	rcatchall
	{
		return false;
	}	
	return true;
}

bool CTournamentInvite::Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	CLiveManager::GetInviteMgr().OnReceivedTournamentInvite(gamerInfo, m_SessionInfo, m_hSessionHost, m_szContentId, m_TournamentEventId, m_InviteFlags);
    return true;
}

//================================================================================================================
// CFollowInvite 
//================================================================================================================
void CFollowInvite::Trigger(const rlGamerHandle* hToGamers, const int nGamers, const rlSessionInfo& hSessionInfo)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	if(!gnetVerify(hSessionInfo.IsValid()))
	{
		gnetError("CFollowInvite :: Invalid session info!");
		return;
	}

	CFollowInvite evt(hSessionInfo, NetworkInterface::GetLocalGamerHandle());
	CGamePresenceEventDispatcher::SendEventToGamers(evt, hToGamers, nGamers);
}

bool CFollowInvite::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("s", m_szSessionInfo));
		rverifyall(rw->WriteBinary("gh", &m_hGamer, sizeof(rlGamerHandle)));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CFollowInvite::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int nLength = 0;
		rverifyall(rr->ReadString("s", m_szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(rr->ReadBinary("gh", &m_hGamer, sizeof(rlGamerHandle), &nLength));
		rverifyall(m_SessionInfo.FromString(m_szSessionInfo));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CFollowInvite::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CFollowInvite :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CFollowInvite :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hGamer.ToString(), sender.ToString());
		m_hGamer = sender;
	}

	CLiveManager::GetInviteMgr().SetFollowInvite(m_SessionInfo, m_hGamer);
	return true;
}

//================================================================================================================
// CAdminInvite 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
void CAdminInvite::Trigger_Debug()
{

}

bool CAdminInvite::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("s", m_szSessionInfo));
		rverifyall(rw->WriteString("h", m_szGamerHandle));
		rverifyall(rw->WriteUns("f", m_InviteFlags));
	}
	rcatchall
	{
		return false; 
	}
	return true;
}
#endif

bool CAdminInvite::Deserialise(const RsonReader* rr)
{
	rtry
	{
		// for security, verify that some key fields do not have duplicate entries
		rverifyall(!rr->HasDuplicateEntries("f"));
		rverifyall(!rr->HasDuplicateEntries("h")); 
		
		rverifyall(rr->ReadString("s", m_szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(m_SessionInfo.FromString(m_szSessionInfo));
		rverifyall(rr->ReadString("h", m_szGamerHandle, RL_MAX_GAMER_HANDLE_CHARS));
		rverifyall(m_hGamer.FromString(m_szGamerHandle));

		m_InviteFlags = InviteFlags::IF_None;
		rr->ReadUns("f", m_InviteFlags);

		// always apply this flag
		m_InviteFlags |= InviteFlags::IF_ViaAdmin;

		// get admin to switch to flags but support the deprecated individual parameters for now
		unsigned value = 0;
		if(rr->ReadUns("t", value) && value == 1)
			m_InviteFlags |= InviteFlags::IF_Spectator;
		if(rr->ReadUns("ad", value) && value == 1)
			m_InviteFlags |= InviteFlags::IF_IsRockstarDev;

		rverifyall(rr->ReadString("hg", m_szGamerHandle, RL_MAX_GAMER_HANDLE_CHARS));
		rverifyall(m_hHostGamer.FromString(m_szGamerHandle));
	}
	rcatchall
	{
		return false;
	}
    return true;
}

bool CAdminInvite::Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	rtry
	{
		rcheck(m_hGamer.IsValid(), catchall, gnetError("CAdminInvite::Apply :: Invalid gamer handle!"));
		rcheck(m_hGamer.IsLocal(), catchall, gnetError("CAdminInvite::Apply :: Non-local gamer handle!"));
		rcheck(m_hHostGamer.IsValid(), catchall, gnetError("CAdminInvite::Apply :: Invalid host handle!"));
		rcheck(m_SessionInfo.IsValid(), catchall, gnetError("CAdminInvite::Apply :: Invalid session info!"));

		gnetDebug1("CAdminInvite::Apply :: For %s, Session: 0x%016" I64FMT "x, Flags: 0x%x", NetworkUtils::LogGamerHandle(m_hGamer), m_SessionInfo.GetToken().m_Value, m_InviteFlags);

		// player is local, we are being invited
		CLiveManager::GetInviteMgr().OnReceivedAdminInvite(
			gamerInfo, 
			m_SessionInfo, 
			m_hHostGamer,
			m_InviteFlags);
	}
	rcatchall
	{
		return false;
	}
	return true;
}

//================================================================================================================
// CAdminInviteNotification 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CAdminInviteNotification::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		char szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
		char szGamerHandle[RL_MAX_GAMER_HANDLE_CHARS];

		rverifyall(m_SessionInfo.ToString(szSessionInfo) != nullptr);
		rverifyall(m_hInvitedGamer.ToString(szGamerHandle) != nullptr);
		rverifyall(rw->WriteString("s", szSessionInfo));
		rverifyall(rw->WriteString("h", szGamerHandle));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CAdminInviteNotification::Deserialise(const RsonReader* rr)
{
	rtry
	{
		char szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
		char szGamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
		rverifyall(rr->ReadString("s", szSessionInfo, rlSessionInfo::TO_STRING_BUFFER_SIZE));
		rverifyall(m_SessionInfo.FromString(szSessionInfo));
		rverifyall(rr->ReadString("h", szGamerHandle, RL_MAX_GAMER_HANDLE_CHARS));
		rverifyall(m_hInvitedGamer.FromString(szGamerHandle));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CAdminInviteNotification::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)

	// check that the gamer handle is valid
	if(!m_hInvitedGamer.IsValid())
	{
		gnetError("CAdminInviteNotification::Apply :: Invalid invited gamer!");
		return true;
	}

	// check that the session information is valid
	if(!m_SessionInfo.IsValid())
	{
		gnetError("CAdminInviteNotification::Apply :: Invalid session info!");
		return true;
	}

	gnetDebug1("CAdminInviteNotification::Apply :: For %s, Session: 0x%016" I64FMT "x", NetworkUtils::LogGamerHandle(m_hInvitedGamer), m_SessionInfo.GetToken().m_Value);

	// check that we are host of this session
	CNetworkSession& rNS = CNetwork::GetNetworkSession();
	if(!((rNS.IsMySessionToken(m_SessionInfo.GetToken()) && rNS.IsHost()) || (rNS.IsMyTransitionToken(m_SessionInfo.GetToken()) && rNS.IsTransitionHost())))
	{
		gnetError("CAdminInviteNotification::Apply :: Not in or session host of session with token: 0x%016" I64FMT "x", m_SessionInfo.GetToken().m_Value);
		return true;
	}

	// inform the invite manager
	CLiveManager::GetInviteMgr().AddAdminInviteNotification(m_hInvitedGamer);

	return true;
}

//================================================================================================================
// CJoinQueueRequest 
//================================================================================================================
void CJoinQueueRequest::Trigger(const rlGamerHandle& hTo, const rlGamerHandle& hGamer, bool bIsSpectator, s64 nCrewID, bool bConsumePrivate, unsigned nUniqueToken)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	CJoinQueueRequest evt(hGamer, bIsSpectator, nCrewID, bConsumePrivate, nUniqueToken);
	CGamePresenceEventDispatcher::SendEventToGamers(evt, &hTo, 1);
}

bool CJoinQueueRequest::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteBinary("gh", &m_hGamer, sizeof(rlGamerHandle)));
		rverifyall(rw->WriteBool("sp", m_bIsSpectator));
		rverifyall(rw->WriteInt64("c", m_nCrewID));
		rverifyall(rw->WriteBool("cp", m_bConsumePrivate));
		rverifyall(rw->WriteUns("ut", m_nUniqueToken));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CJoinQueueRequest::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int nLength = 0;
		rverifyall(rr->ReadBinary("gh", &m_hGamer, sizeof(rlGamerHandle), &nLength));
		rverifyall(rr->ReadBool("sp", m_bIsSpectator));
		rverifyall(rr->ReadInt64("c", m_nCrewID));
		rverifyall(rr->ReadBool("cp", m_bConsumePrivate));
		rverifyall(rr->ReadUns("ut", m_nUniqueToken));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CJoinQueueRequest::Apply(const rlGamerInfo&, const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CJoinQueueRequest :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CJoinQueueRequest :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hGamer.ToString(), sender.ToString());
		m_hGamer = sender;
	}

	CNetwork::GetNetworkSession().AddQueuedJoinRequest(m_hGamer, m_bIsSpectator, m_nCrewID, m_bConsumePrivate, m_nUniqueToken);
	return true;
}

//================================================================================================================
// CJoinQueueUpdate 
//================================================================================================================
void CJoinQueueUpdate::Trigger(const rlGamerHandle& hTo, bool bCanQueue, unsigned nUniqueToken)
{
	if(!NetworkInterface::IsLocalPlayerOnline())
		return;

	CJoinQueueUpdate evt(bCanQueue, nUniqueToken);
	CGamePresenceEventDispatcher::SendEventToGamers(evt, &hTo, 1);
}

bool CJoinQueueUpdate::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteBool("cq", m_bCanQueue));
		rverifyall(rw->WriteUns("ut", m_nUniqueToken));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CJoinQueueUpdate::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr->ReadBool("cq", m_bCanQueue));
		rverifyall(rr->ReadUns("ut", m_nUniqueToken));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CJoinQueueUpdate::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - no embedded sender
	CNetwork::GetNetworkSession().HandleJoinQueueUpdate(m_bCanQueue, m_nUniqueToken);
	return true;
}

//================================================================================================================
// CNewsItemPresenceEvent 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CNewsItemPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("k", m_newsStoryKey));
		rverifyall(rw->WriteUns("ts", m_activeDate));
	}
	rcatchall
	{
		return false;
	}
	return true;
}
#endif

bool CNewsItemPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		// all optional except the key
		rr->ReadUns("ts", m_activeDate);

		typedef char NewsTypeDef[MAX_NUM_TYPE_CHAR];
		NewsTypeDef typeArray[MAX_NUM_TYPES];

		int numRead = rr->ReadArray("t", typeArray, MAX_NUM_TYPES);
		m_types.Reset();
		for(int i = 0; i < numRead && i < MAX_NUM_TYPES; i++)
		{
			m_types.Append() = typeArray[i];
		}

		rr->ReadUns("exp", m_expireDate);  //This is optional and only used for news stories delivered via static sources.
		rr->ReadUns("sk", m_trackingId); // This is currently optional. The name to read from may still change.
		rr->ReadInt("p", m_priority);
		rr->ReadInt("sp", m_secondaryPriority); // optional: used to sort items within the same priority range.

		rverifyall(rr->ReadString("k", m_newsStoryKey));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CNewsItemPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	return false;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CNewsItemPresenceEvent::Trigger_Debug(const char* storyKey, SendFlag flags)
{
	CNewsItemPresenceEvent evt;
	safecpy(evt.m_newsStoryKey, storyKey);
	evt.m_activeDate = 0;

	if(flags & SendFlag_Inbox)
	{
		SC_INBOX_MGR.AddTagToList("news");
		SC_INBOX_MGR.SendEventToCommunity(evt, flags);
		SC_INBOX_MGR.ClearTagList();
	}
	else
	{
		gnetError("News event not setup to work via presence");
	}
}
#endif

#if RSG_BANK
char s_Bank_NewsItem_Key[32];
void Bank_NewsSend()
{
	CNewsItemPresenceEvent::Trigger_Debug(s_Bank_NewsItem_Key, Helper_GetSendFlags());
}

void CNewsItemPresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(s_Bank_NewsItem_Key, "TestStory1");
	bank.PushGroup("CNewsItemPresenceEvent");
		bank.AddText("News Story Key", s_Bank_NewsItem_Key, sizeof(s_Bank_NewsItem_Key), false);
		bank.AddButton("Send Via Inbox", CFA(Bank_NewsSend));
	bank.PopGroup();
}

void CNewsItemPresenceEvent::Set(const char* key, const TypeHashList& typeList)
{
	safecpy(m_newsStoryKey, key);
	m_activeDate = 0;
	m_types = typeList;
}
#endif

//================================================================================================================
// CFingerOfGodPresenceEvent 
//================================================================================================================
bool CFingerOfGodPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		char typeString[32];
		typeString[0] = '\0';

		rverifyall(rr->ReadString("t", typeString));

		m_typeHash = static_cast<eFogType>(atStringHash(typeString));
		switch(m_typeHash)
		{
		case FOG_TYPE_VEHICLE: rverifyall(rr->ReadString("d", m_charData)); break;
		case FOG_TYPE_KICK: /* nothing to deserialize. Just kick the player */ break;
		case FOG_TYPE_BLACKLIST: /* nothing to deserialize. Just blacklist the player */ break;
		case FOG_TYPE_RELAX: /* nothing to deserialize. */ break;
		case FOG_TYPE_SMITETHEE: /* nothing to deserialize. */ break;
		default:
			rverify(0, catchall, gnetError("CFingerOfGodPresenceEvent :: Unhandled Type: %d", static_cast<int>(m_typeHash)));
			break;
		}

		rcheck(NetworkInterface::IsNetworkOpen(), catchall, gnetWarning("CFingerOfGodPresenceEvent :: Rejecting - Not in Multiplayer"));
	}
	rcatchall
	{
		m_typeHash = FOG_TYPE_NONE;
		return false;
	}
	return true;
}

bool CFingerOfGodPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	switch(m_typeHash)
	{
	case FOG_TYPE_VEHICLE:
		{
			CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
			if(pPlayerPed)
			{
				//Make sure we're not already in a vehicle.
				CVehicle *pVeh = pPlayerPed->GetVehiclePedInside();
				if(!pVeh)	
				{
					CCheat::CreateVehicleAdminIntf(m_charData);
				}
			}
		}
		break;
	case FOG_TYPE_KICK:
	case FOG_TYPE_BLACKLIST:
		{
			const KickReason kickReason = (m_typeHash == FOG_TYPE_KICK) ? KickReason::KR_ScAdmin : KickReason::KR_ScAdminBlacklist;
			if(NetworkInterface::IsHost())
			{
				const netPlayer* pLocalPLayer = NetworkInterface::GetLocalPlayer();
				if(gnetVerify(pLocalPLayer))
				{
					CNetwork::GetNetworkSession().KickPlayer(pLocalPLayer, kickReason, 0, RL_INVALID_GAMER_HANDLE);
				}
			}
			else //Request it from the host.
			{
				const netPlayer* pHostPlayer = NetworkInterface::GetHostPlayer();
				if(pHostPlayer)
				{
					msgRequestKickFromHost msg(kickReason);
					NetworkInterface::SendMsg(pHostPlayer, msg, 0);
				}
			}
		}
		break;
	case FOG_TYPE_RELAX:
		{
			//Get the player ped
			CPed* pPlayerPed = FindPlayerPed();
			if(pPlayerPed)
			{
				CEventSwitch2NM relaxEvent(10000, rage_new CTaskNMRelax(1000, 10000));
				pPlayerPed->SwitchToRagdoll(relaxEvent);
			}
		}
		break;
	case FOG_TYPE_SMITETHEE:
		{
			CPed* pPlayerPed = FindPlayerPed();
			if(pPlayerPed)
			{
				// Just give the player the death task.
				CDeathSourceInfo info(pPlayerPed);
				CTask* pTaskDie = rage_new CTaskDyingDead(&info, CTaskDyingDead::Flag_ragdollDeath);
				CEventGivePedTask deathEvent(PED_TASK_PRIORITY_PRIMARY, pTaskDie);
				pPlayerPed->GetPedIntelligence()->AddEvent(deathEvent);
			}
		}
		break;
	case FOG_TYPE_NONE:
		break;
	}
	return false;
}

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool CFingerOfGodPresenceEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteInt("t", m_typeHash));

		switch (m_typeHash)
		{
		case FOG_TYPE_VEHICLE: rverifyall(rw->WriteString("d", m_charData)); break;
		case FOG_TYPE_KICK: break; // no data
		case FOG_TYPE_BLACKLIST: break; // no data
		case FOG_TYPE_RELAX: break; // no data
		case FOG_TYPE_SMITETHEE: break; // no data
		case FOG_TYPE_NONE: break; // no data
		}
	}
	rcatchall
	{
		return false;
	}
	return true;
}

void CFingerOfGodPresenceEvent::TriggerKick()
{
	CFingerOfGodPresenceEvent evt;
	evt.m_typeHash = FOG_TYPE_KICK;
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, SendFlag_Self);
}

void CFingerOfGodPresenceEvent::TriggerBlacklist()
{
	CFingerOfGodPresenceEvent evt;
	evt.m_typeHash = FOG_TYPE_BLACKLIST;
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, SendFlag_Self);
}

void CFingerOfGodPresenceEvent::TriggerSmite()
{
	CFingerOfGodPresenceEvent evt;
	evt.m_typeHash = FOG_TYPE_SMITETHEE;
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, SendFlag_Self);
}

void CFingerOfGodPresenceEvent::TriggerRelax()
{
	CFingerOfGodPresenceEvent evt;
	evt.m_typeHash = FOG_TYPE_RELAX;
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, SendFlag_Self);
}

void CFingerOfGodPresenceEvent::TriggerVeh(const char* veh)
{
	CFingerOfGodPresenceEvent evt;
	evt.m_typeHash = FOG_TYPE_VEHICLE;
	safecpy(evt.m_charData, veh);
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, SendFlag_Self);
}
#endif

#if RSG_BANK
char bank_fingerVehicle[64];
void DoVehicle()
{
	CFingerOfGodPresenceEvent::TriggerVeh(bank_fingerVehicle);
}

void CFingerOfGodPresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(bank_fingerVehicle, "fbi");
	bank.PushGroup("CFingerOfGodPresenceEvent");
		bank.AddButton("Smite", CFA(CFingerOfGodPresenceEvent::TriggerSmite));
		bank.AddButton("Kick", CFA(CFingerOfGodPresenceEvent::TriggerKick));
		bank.AddButton("Relax", CFA(CFingerOfGodPresenceEvent::TriggerRelax));
		bank.AddText("Vehicle Type", bank_fingerVehicle, sizeof(bank_fingerVehicle), false);
		bank.AddButton("Vehicle", CFA(DoVehicle));
	bank.PopGroup();
}
#endif //RSG_BANK

//================================================================================================================
// CBountyPresenceEvent 
//================================================================================================================
bool CBountyPresenceEvent::Serialise(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteString("Ft", m_tagFrom));
		rverifyall(rw->WriteString("Tt", m_tagTarget));
		rverifyall(rw->WriteInt("o", m_iOutcome));
		rverifyall(rw->WriteInt("c", m_iCash));
		rverifyall(rw->WriteInt("r", m_iRank));

		if(m_iTime > 0)
			rverifyall(rw->WriteInt("t", m_iTime));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CBountyPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr->ReadString("Ft", m_tagFrom));
		rverifyall(rr->ReadString("Tt", m_tagTarget));
		rverifyall(rr->ReadInt("o", m_iOutcome));
		rverifyall(rr->ReadInt("c", m_iCash));
		rverifyall(rr->ReadInt("r", m_iRank));

		// time is optional
		m_iTime = 0;
		rr->ReadInt("t", m_iTime);
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CBountyPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender : todo : check whether we need to validate this in script or can reject in code under certain conditions
	gnetError("CBountyPresenceEvent::Apply has no apply implemented in code");
	return false;
}

void CBountyPresenceEvent::Trigger(const rlGamerHandle* pRecips, const int numRecips, const scrBountyInboxMsg_Data* inData)
{
	CBountyPresenceEvent evt;
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	
	if(!gnetVerify(rlPresence::GetDisplayName(localGamerIndex, evt.m_tagFrom)))
	{
		gnetError(" CBountyPresenceEvent::Trigger unable to get gamer handle for player");
		return;
	}

	safecpy(evt.m_tagTarget, inData->m_targetGTag);
	evt.m_iOutcome = inData->m_iOutcome.Int;
	evt.m_iCash = inData->m_iCash.Int;
	evt.m_iRank = inData->m_iRank.Int;
	evt.m_iTime = inData->m_iTime.Int;

	SC_INBOX_MGR.SendEventToGamers(evt, SendFlag_Inbox, pRecips, numRecips);
}

#if RSG_BANK
CBountyPresenceEvent bankBountyEvent;
void Bank_SendBountyEvent()
{
	if(Helper_GetSendFlags() & SendFlag_Inbox)
		SC_INBOX_MGR.SendEventToCommunity(bankBountyEvent, Helper_GetSendFlags());
}

void CBountyPresenceEvent::AddWidgets(bkBank& bank)
{
	safecpy(bankBountyEvent.m_tagFrom, "Conor");
	safecpy(bankBountyEvent.m_tagTarget, "DannyBoy123");
	bankBountyEvent.m_iCash = bankBountyEvent.m_iOutcome = bankBountyEvent.m_iRank = bankBountyEvent.m_iTime = 0;

	bank.PushGroup("CBountyPresenceEvent");
		bank.AddText("From:", bankBountyEvent.m_tagFrom, sizeof(bankBountyEvent.m_tagFrom), false);
		bank.AddText("TARGET:", bankBountyEvent.m_tagTarget, sizeof(bankBountyEvent.m_tagTarget), false);
		bank.AddText("Cash", &bankBountyEvent.m_iCash, false);
		bank.AddText("Outcome", &bankBountyEvent.m_iOutcome, false);
		bank.AddText("Rank", &bankBountyEvent.m_iRank, false);
		bank.AddText("Time", &bankBountyEvent.m_iTime, false);
		bank.AddButton("SendEVent", CFA(Bank_SendBountyEvent));
	bank.PopGroup();
}
#endif //BANK

//================================================================================================================
// CForceSessionUpdatePresenceEvent 
//================================================================================================================

#if NET_GAME_PRESENCE_SERVER_DEBUG
void CForceSessionUpdatePresenceEvent::Trigger_Debug()
{
	CForceSessionUpdatePresenceEvent evt;
	CGamePresenceEventDispatcher::SendEventToCommunity(evt, Helper_GetSendFlags());
}

bool CForceSessionUpdatePresenceEvent::Serialise_Debug(RsonWriter* UNUSED_PARAM(rw)) const
{
	return true;
}
#endif

bool CForceSessionUpdatePresenceEvent::Deserialise(const RsonReader* UNUSED_PARAM(rr))
{
	return true;
}

bool CForceSessionUpdatePresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	CNetwork::GetNetworkSession().ForcePresenceUpdate();
	return true;
}

#if RSG_BANK
void CForceSessionUpdatePresenceEvent::AddWidgets(bkBank& bank)
{
	bank.PushGroup("CForceSessionUpdatePresenceEvent");
	bank.AddButton("Trigger", CFA(Trigger_Debug));
	bank.PopGroup();
}
#endif //BANK

//================================================================================================================
// CGameTriggerEvent 
//================================================================================================================
#if NET_GAME_PRESENCE_SERVER_DEBUG
void CGameTriggerEvent::Trigger_Debug(int nEventHash, int nEventParam1, int nEventParam2, const char* szEventString, int nSendFlags)
{
    int nLocalGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if(!RL_IS_VALID_LOCAL_GAMER_INDEX(nLocalGamerIndex))
        return;

    CGameTriggerEvent tEvent(nEventHash, nEventParam1, nEventParam2, szEventString);
    CGamePresenceEventDispatcher::SendEventToCommunity(tEvent, static_cast<SendFlag>(nSendFlags));
}

bool CGameTriggerEvent::Serialise_Debug(RsonWriter* rw) const
{
	rtry
	{
		rverifyall(rw->WriteUns("event", m_nEventHash));

		// all parameters are optional
		if(m_nEventParam1 != 0)
			rverifyall(rw->WriteInt("p1", m_nEventParam1));
		if(m_nEventParam2 != 0)
			rverifyall(rw->WriteInt("p2", m_nEventParam2));
		if(strlen(m_szEventString) > 0)
			rverifyall(rw->WriteString("ps", m_szEventString));
	}
	rcatchall
	{
		return false;
	}
    return true;
}
#endif

bool CGameTriggerEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr->ReadUns("event", m_nEventHash));

		// all parameters are optional
		if(rr->ReadInt("p1", m_nEventParam1))
			m_nEventParam1 = 0;
		if(rr->ReadInt("p2", m_nEventParam2))
			m_nEventParam2 = 0;
		if(rr->ReadString("ps", m_szEventString))
			m_szEventString[0] = '\0';
	}
	rcatchall
	{
		return false;
	}
    return true;
}

bool CGameTriggerEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
    CEventNetworkPresenceTriggerEvent scrEvent(static_cast<int>(m_nEventHash), m_nEventParam1, m_nEventParam2, m_szEventString);
    GetEventScriptNetworkGroup()->Add(scrEvent);
    return true;
}

#if RSG_BANK
char s_GameTriggerEventName[CGameTriggerEvent::MAX_STRING];
int s_GameTriggerEventParam1 = 0;
int s_GameTriggerEventParam2 = 0;
char s_GameTriggerEventString[CGameTriggerEvent::MAX_STRING];
void Bank_CGameTriggerEvent_Trigger()
{
    // if the string starts with a #, it's a number
    int nHashValue = atStringHash(s_GameTriggerEventName);
    if(s_GameTriggerEventName[0] == '#')
        sscanf(s_GameTriggerEventName, "#%d", &nHashValue);

    CGameTriggerEvent::Trigger_Debug(nHashValue, s_GameTriggerEventParam1, s_GameTriggerEventParam2, s_GameTriggerEventString, Helper_GetSendFlags());
}

void CGameTriggerEvent::AddWidgets(bkBank& bank)
{
    s_GameTriggerEventName[0] = '\0';
    s_GameTriggerEventString[0] = '\0';

    bank.PushGroup("CGameTriggerEvent");
    bank.AddText("Event Name", s_GameTriggerEventName, sizeof(s_GameTriggerEventName), false);
    bank.AddText("Event Param 1", &s_GameTriggerEventParam1, false);
    bank.AddText("Event Param 2", &s_GameTriggerEventParam2, false);
    bank.AddText("Event String", s_GameTriggerEventString, sizeof(s_GameTriggerEventString), false);
    bank.AddButton("Trigger", CFA(Bank_CGameTriggerEvent_Trigger));
    bank.PopGroup();
}
#endif

//================================================================================================================
// CTextMessageEvent 
//================================================================================================================
void CTextMessageEvent::Trigger(const rlGamerHandle* dest, const char* szTextMessage)
{
	int nLocalGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(nLocalGamerIndex))
		return;

	CTextMessageEvent tEvent(szTextMessage);
	CGamePresenceEventDispatcher::SendEventToGamers(tEvent, dest, 1);
}

bool CTextMessageEvent::Serialise(RsonWriter* rw) const
{
	rtry
	{
		if(strlen(m_szTextMessage) > 0)
			rverifyall(rw->WriteString("ps", m_szTextMessage));

		rverifyall(rw->WriteBinary("gh", &m_hFromGamer, sizeof(rlGamerHandle)));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CTextMessageEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		int nLength = 0;
		rverifyall(rr->ReadString("ps", m_szTextMessage));
		rverifyall(rr->ReadBinary("gh", &m_hFromGamer, sizeof(rlGamerHandle), &nLength));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool CTextMessageEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& sender, const bool isAuthenticatedSender)
{
	gnetDebug1("CTextMessageEvent :: Received CTextMessageEvent : %s ", m_szTextMessage);

	// authenticated sender - check if we should replace our embedded handle
	if(isAuthenticatedSender && sender.IsValid())
	{
		if(m_hFromGamer != sender && s_RejectMismatchedAuthenticatedSender)
		{
			gnetDebug1("CTextMessageEvent :: Mismatched embedded sender (%s) with Authenticated sender (%s) - Rejecting message...", m_hFromGamer.ToString(), sender.ToString());
			return false;
		}

		gnetDebug1("CTextMessageEvent :: Replacing embedded sender (%s) with Authenticated sender (%s)", m_hFromGamer.ToString(), sender.ToString());
		m_hFromGamer = sender;
	}

	CNetwork::GetVoice().OnTextMessageReceivedViaPresence(m_szTextMessage, m_hFromGamer);
	return true;
}

#if RSG_BANK
char s_TextMessageEventString[CGameTriggerEvent::MAX_STRING];
void Bank_CTextMessageEvent_Trigger()
{
	CTextMessageEvent::Trigger(&NetworkInterface::GetLocalGamerHandle(), s_TextMessageEventString);
}

void CTextMessageEvent::AddWidgets(bkBank& bank)
{
	s_TextMessageEventString[0] = '\0';

	bank.PushGroup("CTextMessageEvent");
	bank.AddText("Event String", s_TextMessageEventString, sizeof(s_TextMessageEventString), false);
	bank.AddButton("Trigger", CFA(Bank_CTextMessageEvent_Trigger));
	bank.PopGroup();
}
#endif

#if __NET_SHOP_ACTIVE
//================================================================================================================
// GameServerPresenceEvent 
//================================================================================================================
#if RSG_BANK
void GameServerPresenceEvent::AddWidgets(bkBank& UNUSED_PARAM(bank))
{

}
#endif

#if NET_GAME_PRESENCE_SERVER_DEBUG
bool GameServerPresenceEvent::Serialise_Debug(RsonWriter* UNUSED_PARAM(rw)) const
{
	// not implemented
	return true;
}
#endif

bool GameServerPresenceEvent::Deserialise(const RsonReader* rr)
{
	rtry
	{
		rverifyall(rr != nullptr);
		rverifyall(rr->ReadBool("balance", m_updatePlayerBalance));
		rverifyall(rr->ReadBool("fullRefresh", m_fullRefreshRequested));

		// if we're given items, we get a listing of the items we were given to request updates.
		RsonReader itemsListRR;
		if(rr->GetMember("items", &itemsListRR))
		{
			bool itemListSuccess = false;
			RsonReader itemIter;
			for(itemListSuccess = itemsListRR.GetFirstMember(&itemIter); itemListSuccess; itemListSuccess = itemIter.GetNextSibling(&itemIter))
			{
				int tempInt = 0;
				if(itemIter.AsInt(tempInt))
				{
					m_items.PushAndGrow(tempInt);
				}
			}
		}

		rverifyall(rr->ReadUns("awardType", m_awardTypeHash));
		rverifyall(rr->ReadInt("awardAmount", m_awardAmount));
	}
	rcatchall
	{
		return false;
	}
	return true;
}

bool GameServerPresenceEvent::Apply(const rlGamerInfo& UNUSED_PARAM(gamerInfo), const rlGamerHandle& UNUSED_PARAM(sender), const bool UNUSED_PARAM(isAuthenticatedSender))
{
	// authenticated sender - this is a server message only, will be validated that the handle is invalid (if authenticated)
	gnetDebug1("GameServerPresenceEvent::Apply");

	sNetworkScAdminPlayerUpdate eventData;

	eventData.m_fullRefreshRequested.Int = m_fullRefreshRequested;
	eventData.m_updatePlayerBalance.Int  = m_updatePlayerBalance;
	eventData.m_awardTypeHash.Int        = m_awardTypeHash;
	eventData.m_awardAmount.Int          = m_awardAmount;

	u32 numItems = 0;

	for(int i=0; i<m_items.GetCount(); i++, numItems++)
		eventData.m_items[i].Int = m_items[i];

	for(int i=numItems; i<sNetworkScAdminPlayerUpdate::MAX_NUM_ITEMS; i++)
		eventData.m_items[i].Int = 0;

	GetEventScriptNetworkGroup()->Add(CEventNetworkScAdminPlayerUpdated(&eventData));

	return true;
}

#endif // __NET_SHOP_ACTIVE