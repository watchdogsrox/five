#if __WIN32PC
//
// name:        NetworkTextChat.h
//

// game headers

#include "fwnet/netchannel.h"
#include "Network/Chat/NetworkTextChat.h"

#include "frontend/ProfileSettings.h"
#include "Network/NetworkInterface.h"
#include "Network/Voice/NetworkVoice.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
//#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Peds/PlayerPed.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, textchat)
#undef __net_channel
#define __net_channel net_textchat

PARAM(netTextChatEnableFilter, "[TextChat] Enable text filtering");

static const unsigned DEFAULT_CHAT_FILTER_HOLDING_PEN_MAX_REQUESTS = 10;
static const unsigned DEFAULT_CHAT_FILTER_HOLDING_PEN_INTERVAL_MS = 500;
static const unsigned DEFAULT_CHAT_FILTER_SPAM_MAX_REQUESTS = 30;
static const unsigned DEFAULT_CHAT_FILTER_SPAM_INTERVAL_MS = 10000;

bool s_IsTextFilterEnabled = false;
unsigned s_FilterHoldingMaxRequests = DEFAULT_CHAT_FILTER_HOLDING_PEN_MAX_REQUESTS;
unsigned s_FilterHoldingPenIntervalMs = DEFAULT_CHAT_FILTER_HOLDING_PEN_INTERVAL_MS;
unsigned s_FilterSpamMaxRequests = DEFAULT_CHAT_FILTER_SPAM_MAX_REQUESTS;
unsigned s_FilterSpamIntervalMs = DEFAULT_CHAT_FILTER_SPAM_INTERVAL_MS;

bool IsTextChatFilteringEnabled()
{
	return s_IsTextFilterEnabled || PARAM_netTextChatEnableFilter.Get();
}

struct CMsgTextChatStatus
{
    NET_MESSAGE_DECL(CMsgTextChatStatus, CMSG_TEXT_CHAT_STATUS);

    CMsgTextChatStatus()
        : m_TextChatFlags(0)
    {
    }

    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerUns(msg.m_TextChatFlags, 32)
            //Don't send remotely set flags.
            && AssertVerify(0 == (msg.m_TextChatFlags & CNetGamePlayer::TEXT_LOCAL_MASK));
    }

    int GetMessageDataBitSize() const
    {
        return 32;
    }

    unsigned m_TextChatFlags;
};

NET_MESSAGE_IMPL(CMsgTextChatStatus);

static const unsigned DEFAULT_TEXT_CHAT_VALID_SEND_GROUPS = SentFromGroup::Groups_CloseContacts | SentFromGroup::Group_SameTeam;

CNetworkTextChat::CNetworkTextChat()
: m_TeamOnlyChat(false)
, m_IsInitialized(false)
, m_IsSpectatorToNonSpectatorChatEnabled(false)
, m_IsTransitioning(false)
, m_IsScriptControllingTeams(false)
, m_AllowedSentFromGroups(DEFAULT_TEXT_CHAT_VALID_SEND_GROUPS)
{
    // Initialize our message handler.  This will be registered with
    // the network in Init().
    m_Dlgt.Bind(this, &CNetworkTextChat::OnNetEvent);
	m_TextChatDlgt.Bind(this, &CNetworkTextChat::OnTextChatEvent);
}

CNetworkTextChat::~CNetworkTextChat()
{
    this->Shutdown();
}

bool
CNetworkTextChat::Init(netConnectionManager* cxnMgr)
{
    bool success = false;

    if(AssertVerify(!m_IsInitialized))
    {
        netInterface::AddDelegate(&m_Dlgt);
		m_TextChat.AddDelegate(&m_TextChatDlgt);

		m_TextChat.Init(TextChat::MAX_LOCAL_TYPERS,
                            TextChat::MAX_REMOTE_TYPERS,
                            cxnMgr,
							NETWORK_GAME_CHANNEL_ID,
							NETWORK_SESSION_ACTIVITY_CHANNEL_ID);

		SetTeamOnlyChat(false);
		
		m_IsScriptControllingTeams = false;
		m_IsInitialized = true;
		success = true;
    }

    return success;
}

void CNetworkTextChat::Shutdown()
{
    if(m_IsInitialized)
    {
		gnetDebug3("CNetworkTextChat::Shutdown()");
		m_TextChat.RemoveDelegate(&m_TextChatDlgt);
		m_TextChat.Shutdown();

        netInterface::RemoveDelegate(&m_Dlgt);

        m_IsInitialized = false;

		m_ChatHistory.Reset();
    }
}

bool
CNetworkTextChat::IsInitialized() const
{
    return m_IsInitialized;
}

void CNetworkTextChat::Update()
{
    if(m_IsInitialized)
    {
        //Update the collection of typers who can send and receive text chat
        this->UpdateTypers();

        if(m_TextChat.IsInitialized())
        {
            m_TextChat.Update();
        }

		if(IsTextChatFilteringEnabled())
		{
			const int numChatHistories = m_ChatHistory.GetNumSlots();
			for(int i = 0; i < numChatHistories; ++i)
			{
				ChatHistoryMap::Entry* entry = m_ChatHistory.GetEntry(i);
				if(entry)
				{
					PlayerChatHistory& history = entry->data;
					history.Update();
				}
			}
		}
    }
}

void
CNetworkTextChat::AddDelegate(Delegate* dlgt)
{
	m_Delegator.AddDelegate(dlgt);
}

void
CNetworkTextChat::RemoveDelegate(Delegate* dlgt)
{
	m_Delegator.RemoveDelegate(dlgt);
}

bool
CNetworkTextChat::AddTyper(const rlGamerInfo& gamerInfo,
						   const EndpointId endpointId)
{
    bool success = false;

	if(gnetVerifyf(m_TextChat.IsInitialized(), "Text chat not initialized"))
	{
		success = m_TextChat.AddTyper(gamerInfo, endpointId);
		if(success)
		{
			RockstarId rockstarId = gamerInfo.GetGamerHandle().GetRockstarId();
			if(m_ChatHistory.Access(rockstarId))
			{
				gnetDebug3("AddTyper :: %d removed from chat history", (int)rockstarId);
				m_ChatHistory.Delete(rockstarId);
			}

			gnetAssertf(m_ChatHistory.GetNumUsed() < TextChat::MAX_TYPERS, "Too many players in the chat history (%d)", m_ChatHistory.GetNumUsed());
			gnetDebug3("AddTyper :: %d added to chat history", (int)rockstarId);
			m_ChatHistory.Insert(rockstarId, PlayerChatHistory(rockstarId, MakeFunctor(*this, &CNetworkTextChat::OnTextFiltered)));
		}
		else
		{
			gnetDebug3("AddTyper :: Failed for gamer %s", gamerInfo.GetName());
		}
	}

    return success;
}

void
CNetworkTextChat::RemoveTyper(const rlGamerInfo& gamerInfo)
{
	if(gnetVerifyf(m_TextChat.IsInitialized(), "Text chat not initialized"))
	{
		bool success = m_TextChat.RemoveTyper(gamerInfo.GetGamerId());
		if(success)
		{
			RockstarId rockstarId = gamerInfo.GetGamerHandle().GetRockstarId();
			if(m_ChatHistory.Access(rockstarId))
			{
				gnetDebug3("RemoveTyper :: %d removed from chat history", (int)rockstarId);
				m_ChatHistory.Delete(rockstarId);
			}
			else
			{
				gnetDebug3("RemoveTyper :: %d was already previously removed from chat history", (int)rockstarId);
			}
		}
		else
		{
			gnetDebug3("RemoveTyper :: Failed for gamer %s - already removed", gamerInfo.GetName());
		}
	}
}

void
CNetworkTextChat::RemoveAllRemoteTypers()
{
	if(gnetVerifyf(m_TextChat.IsInitialized(), "Text chat not initialized"))
	{
		gnetDebug3("CNetworkTextChat::RemoveAllRemoteTypers()");
		m_TextChat.RemoveAllRemoteTypers();

		CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();
		bool isLocalPlayerInChatHistory = false;
		RockstarId rockstarId = InvalidRockstarId;
		if(localPlayer)
		{
			rockstarId = localPlayer->GetGamerInfo().GetGamerHandle().GetRockstarId();
			isLocalPlayerInChatHistory = m_ChatHistory.Access(rockstarId) != NULL;
		}

		m_ChatHistory.Reset();

		if(isLocalPlayerInChatHistory)
		{
			// Re-add the local player to the chat history
			gnetAssertf(m_ChatHistory.GetNumUsed() < TextChat::MAX_TYPERS, "Too many players in the chat history (%d)", m_ChatHistory.GetNumUsed());
			m_ChatHistory.Insert(rockstarId, PlayerChatHistory(rockstarId, MakeFunctor(*this, &CNetworkTextChat::OnTextFiltered)));
		}
	}
}

void
CNetworkTextChat::RemoveAllTypers()
{
	if(gnetVerifyf(m_TextChat.IsInitialized(), "Text chat not initialized"))
	{
		m_TextChat.RemoveAllTypers();
		m_ChatHistory.Reset();
	}
}

bool
CNetworkTextChat::IsPlayerTyping(const rlGamerId& gamerId) const
{
    return m_TextChat.IsInitialized()
            && m_TextChat.IsTyping(gamerId);
}

bool
CNetworkTextChat::IsAnyPlayerTyping() const
{
    return m_TextChat.IsInitialized()
            && m_TextChat.IsAnyTyping();
}

bool
CNetworkTextChat::PlayerHasKeyboard(const rlGamerId& gamerId) const
{
    const CNetGamePlayer* player =
        NetworkInterface::GetPlayerFromGamerId(gamerId);

    return player && player->HasKeyboard();
}

bool
CNetworkTextChat::SetPlayerMuted(const rlGamerId& gamerId, const bool muted)
{
    bool success = false;

    CNetGamePlayer* player =
        NetworkInterface::GetPlayerFromGamerId(gamerId);

    if(AssertVerify(player))
    {
        player->SetLocalTextChatFlag(CNetGamePlayer::TEXT_LOCAL_FORCE_MUTED, muted);

        success = true;
    }

    return success;
}

bool
CNetworkTextChat::IsPlayerMutedByMe(const rlGamerId& gamerId) const
{
	const VoiceGamerSettings* pSettings = FindVoiceGamerSettingsForGamerId(gamerId);
	return pSettings && pSettings->IsMutedByMe();
}

bool
CNetworkTextChat::AmIMutedByPlayer(const rlGamerId& gamerId) const
{
	const VoiceGamerSettings* pSettings = FindVoiceGamerSettingsForGamerId(gamerId);
	return pSettings && pSettings->IsMutedByHim();
}

bool
CNetworkTextChat::IsPlayerBlockedByMe(const rlGamerId& gamerId) const
{
	const VoiceGamerSettings* pSettings = FindVoiceGamerSettingsForGamerId(gamerId);
	return pSettings && pSettings->IsBlockedByMe();
}

bool
CNetworkTextChat::AmIBlockedByPlayer(const rlGamerId& gamerId) const
{
	const VoiceGamerSettings* pSettings = FindVoiceGamerSettingsForGamerId(gamerId);
	return pSettings && pSettings->IsBlockedByHim();
}

bool
CNetworkTextChat::CanIHearPlayer(const rlGamerId& gamerId) const
{
	const VoiceGamerSettings* pSettings = FindVoiceGamerSettingsForGamerId(gamerId);
	return pSettings && pSettings->CanCommunicateTextWith();
}

void
CNetworkTextChat::SetTeamOnlyChat(const bool teamOnlyChat)
{
    m_TeamOnlyChat = teamOnlyChat;
	m_TextChat.SetTextChatRecipients(m_TeamOnlyChat ? TextChat::SEND_TO_TEAM : TextChat::SEND_TO_EVERYONE);
}

void
CNetworkTextChat::SetSpectatorToNonSpectatorChat(const bool enable)
{
	m_IsSpectatorToNonSpectatorChatEnabled = enable;
}

bool 
CNetworkTextChat::SubmitText(const rlGamerInfo& gamerInfo, const char* text, bool bTeamChat)
{
	//@@: location AHOOK_0067_CHK_LOCATION_C
	// We might not have the typer during a session transition
	if(m_TextChat.HaveTyper(gamerInfo.GetGamerId()))
	{
		CNetworkTelemetry::RecordGameChatMessage(gamerInfo.GetGamerId(), text, bTeamChat); 
		AppendChatLog(gamerInfo.GetGamerHandle(), text);
		return m_TextChat.SubmitText(gamerInfo.GetGamerId(), text, bTeamChat);
	}

	return false;
}

bool 
CNetworkTextChat::SetLocalPlayerIsTyping(const rlGamerId& gamerId)
{
	return m_TextChat.SetLocalPlayerIsTyping(gamerId);
}

//private:

void CNetworkTextChat::HandleTextStatus(const ReceivedMessageData &messageData)
{
    CMsgTextChatStatus msg;
	netPlayer* pPlayer = NULL;

    AssertVerify(msg.Import(messageData.m_MessageData, messageData.m_MessageDataSize));
    pPlayer = messageData.m_ToPlayer;

	if (pPlayer)
	{

		ActivePlayerIndex idx = pPlayer->GetActivePlayerIndex();
		CNetGamePlayer* pNetPlayer = NetworkInterface::GetPlayerMgr().GetActivePlayerFromIndex(idx);

		if (pNetPlayer)
		{
			pNetPlayer->SetRemoteTextChatFlags(msg.m_TextChatFlags);
		}
	}
}

void CNetworkTextChat::OnNetEvent(const ReceivedMessageData &messageData)
{	 
	unsigned msgId = 0;

	if (gnetVerify(messageData.IsValid()))
	{
		if(netMessage::GetId(&msgId, messageData.m_MessageData, messageData.m_MessageDataSize))
		{		
			if (msgId == CMsgTextChatStatus::MSG_ID())
			{
				this->HandleTextStatus(messageData);
			}
		}
	}
}
/*
void 
CNetworkTextChat::OnNetEvent(CNetGamePlayer* player,
							 const netEvent* evt)
{
    const unsigned evtId = evt->GetId();

    if(NET_EVENT_FRAME_RECEIVED == evtId)
    {
        const netEventFrameReceived* fr = evt->m_FrameReceived;
        unsigned msgId;

        if(netMessage::GetId(&msgId, fr->m_Payload, fr->m_SizeofPayload))
        {
			if(msgId == CMsgTextChatStatus::MSG_ID())
            {
                this->HandleTextStatus(player, fr);
            }
        }
    }
    else if(NET_EVENT_BANDWIDTH_EXCEEDED == evtId)
    {
        //FIXME (KB) - make an attempt to prioritize typers.
    }
}
*/
void 
CNetworkTextChat::OnTextChatEvent(rage::TextChat* UNUSED_PARAM(textChat),
								  const rlGamerHandle& gamerHandle,
								  const char* text,
								  bool teamOnly)
{
	// check if we are allowed to receive from this player
	if(CLiveManager::CanReceiveFrom(gamerHandle, RL_INVALID_CLAN_ID, m_AllowedSentFromGroups OUTPUT_ONLY(, "NetworkTextChat")))
	{
		// Append to file
		AppendChatLog(gamerHandle, text);

		// todo: spam check here
		// todo: periodic profanity check here

		m_Delegator.Dispatch(this, gamerHandle, text, teamOnly);
	}
}

void
CNetworkTextChat::OnTextFiltered(const rlGamerHandle& sender, const char* text)
{
	PlayerChatHistory* history = m_ChatHistory.Access(sender.GetRockstarId());
	if(history)
	{
		history->AppendFilteredText(text);
	}
}

void
CNetworkTextChat::UpdateTypers()
{
    if(m_TextChat.IsInitialized())
    {
        CNetGamePlayer* myPlayer = NetworkInterface::GetLocalPlayer();

        const int localGamerIdx = myPlayer->GetGamerInfo().GetLocalIndex();
        const bool haveKeyboard = m_TextChat.HasKeyboard(localGamerIdx);

        myPlayer->SetLocalTextChatFlag(CNetGamePlayer::TEXT_LOCAL_HAS_KEYBOARD, haveKeyboard);

		if (m_IsTransitioning)
		{
			if(!CNetwork::GetNetworkSession().IsTransitionEstablished() || CNetwork::GetNetworkSession().IsTransitionLeaving())
			{
				m_IsTransitioning = false;
				m_TextChat.SetInTransition(false);
				RemoveAllRemoteTypers();
			}
		}
		else
		{
			if(CNetwork::GetNetworkSession().IsTransitionEstablished() && !CNetwork::GetNetworkSession().IsTransitionLeaving())
			{
				m_IsTransitioning = true;
				m_TextChat.SetInTransition(true);
				RemoveAllRemoteTypers();
			}
		}

		if (m_IsTransitioning)
		{
			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

			for(unsigned i = 0; i < nGamers; i++)
			{
				// find their voice gamer entry
				VoiceGamerSettings* pGamer = NetworkInterface::GetVoice().FindGamer(aGamers[i].GetGamerHandle());

				if(!gnetVerify(pGamer))
				{
					gnetError("UpdateTypers :: No settings for %s!", aGamers[i].GetName());
					continue; 
				}

				const bool bHaveTyper = m_TextChat.HaveTyper(aGamers[i].GetGamerId());
				bool bCanChat = pGamer->CanCommunicateTextWith();

				if(bCanChat)
				{
					if(!bHaveTyper)
					{
						// get net endpointId
						EndpointId endpointId;
						CNetwork::GetNetworkSession().GetTransitionNetEndpointId(aGamers[i], &endpointId);

						// only add with a valid net endpointId
						if(!pGamer->IsLocal() && NET_IS_VALID_ENDPOINT_ID(endpointId))
						{
							gnetDebug1("Add Typer \"%s\"", aGamers[i].GetName());
							m_TextChat.AddTyper(aGamers[i], endpointId);
						}
					}
				}
				else if(!bCanChat && bHaveTyper)
				{
					gnetDebug1("UpdateTypers() - !bCanChat && bHaveTyper - Remove Typer \"%s\"", aGamers[i].GetName());
					m_TextChat.RemoveTyper(aGamers[i].GetGamerId());
				}
			}
		}
		else
		{
			//The message that we send to players to signal changes in our text chat status.
			CMsgTextChatStatus msgTs;
			CNetGamePlayer* player = NULL;

			for (ActivePlayerIndex i = 0; i < MAX_NUM_ACTIVE_PLAYERS; i++)
			{
				player = NetworkInterface::GetPlayerMgr().GetActivePlayerFromIndex(i);

				if(!player)
				{
					continue;
				}

				//Determine if we've locally blocked or muted the player.
				//Also let the player know if we have a keyboard.
				const rlGamerInfo& gamerInfo = player->GetGamerInfo();
				bool haveTyper = m_TextChat.HaveTyper(gamerInfo.GetGamerId());
				const bool isMuted = m_TextChat.IsMuted(gamerInfo.GetGamerHandle());
				const bool isBlocked = !m_TextChat.HasTextChatPrivileges(localGamerIdx);


				// NOTE: These flags are unused. Muting and blocking is done at a higher level.
				player->SetLocalTextChatFlag(CNetGamePlayer::TEXT_LOCAL_HAS_KEYBOARD, haveKeyboard);
				player->SetLocalTextChatFlag(CNetGamePlayer::TEXT_LOCAL_MUTED, isMuted);
				player->SetLocalTextChatFlag(CNetGamePlayer::TEXT_LOCAL_BLOCKED, isBlocked);

				bool canChat = (player->GetConnectionId() >= 0) || player->IsMyPlayer();

				if(canChat && !haveTyper)
				{
					haveTyper = AddTyper(gamerInfo, player->GetEndpointId());
				}

				if(canChat && !player->IsMyPlayer())
				{
					if(player->AreTextChatFlagsDirty())
					{
						msgTs.m_TextChatFlags =
							CNetGamePlayer::LocalToRemoteTextChatFlags(player->GetLocalTextChatFlags());

						NetworkInterface::SendReliableMessage(player, msgTs);

						player->CleanDirtyTextChatFlags();
					}

					if(player->GetPlayerPed()
							&& (NetworkInterface::IsSessionActive() || NetworkInterface::IsInSpectatorMode()))
					{
						const bool sameTeam = (myPlayer->GetTeam() >= 0 && myPlayer->GetTeam() == player->GetTeam());

						// Set the Same Team state only if the players can chat. Otherwise, the typer won't exist in this
						//  player's typer list. This can occur if one teammate is spectating and one is still alive, and for some reason adding the player failed above.
						if(haveTyper && !m_IsScriptControllingTeams)
						{
							m_TextChat.SetOnSameTeam(player->GetEndpointId(), sameTeam);
						}

						CNetObjPlayer* pOtherPlayerObj = player->GetPlayerPed() ? static_cast<CNetObjPlayer*>(player->GetPlayerPed()->GetNetworkObject()) : NULL;
						const bool otherPlayerSpectating = (pOtherPlayerObj && pOtherPlayerObj->IsSpectating());

						if (NetworkInterface::IsInSpectatorMode() && canChat)
						{
							canChat = otherPlayerSpectating || m_IsSpectatorToNonSpectatorChatEnabled;
						}
					}

					VoiceGamerSettings* pSettings = NULL;

					if (player)
					{
						pSettings = NetworkInterface::GetVoice().FindGamer(player->GetGamerInfo().GetGamerHandle());
					}

					if (pSettings)
					{
						// we just use the voice mute/block states for text chat as well
						canChat = canChat && pSettings->CanCommunicateTextWith();
					}
				}

				if(!canChat && haveTyper && !m_IsScriptControllingTeams)
				{
					RemoveTyper(gamerInfo);
				}
			}
		}
    }
}

const VoiceGamerSettings* CNetworkTextChat::FindVoiceGamerSettingsForGamerId(const rlGamerId& gamerId) const
{
	VoiceGamerSettings* pVoiceGamerSettings = NULL;

	if(m_IsTransitioning)
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(unsigned i = 0; i < nGamers; i++)
		{
			if(aGamers[i].GetGamerId() == gamerId)
			{
				// find their voice gamer entry
				pVoiceGamerSettings = NetworkInterface::GetVoice().FindGamer(aGamers[i].GetGamerHandle());
				break;
			}
		}
	}
	else
	{
		const CNetGamePlayer* player = NetworkInterface::GetPlayerFromGamerId(gamerId);
		if (player)
		{
			pVoiceGamerSettings = NetworkInterface::GetVoice().FindGamer(player->GetGamerInfo().GetGamerHandle());

		}
	}

	return pVoiceGamerSettings;
}

void CNetworkTextChat::AppendChatLog(const rlGamerHandle& gamerHandle, const char* text)
{
	RockstarId rockstarId = NULL;

	if(m_IsTransitioning)
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(unsigned i = 0; i < nGamers; i++)
		{
			if(aGamers[i].GetGamerHandle() == gamerHandle)
			{
				rockstarId = aGamers[i].GetGamerHandle().GetRockstarId();
				break;
			}
		}
	}
	else
	{
		CNetGamePlayer* player = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
		if(!gnetVerifyf(player, "Could not get player from gamerHandle"))
			return;
		rockstarId = player->GetGamerInfo().GetGamerHandle().GetRockstarId();
	}

	PlayerChatHistory* history = m_ChatHistory.Access(rockstarId);

	if(!history && m_IsTransitioning)
	{
		if(rockstarId != InvalidRockstarId)
		{

			gnetAssertf(m_ChatHistory.GetNumUsed() < TextChat::MAX_TYPERS, "Too many players in the chat history (%d)", m_ChatHistory.GetNumUsed());
			m_ChatHistory.Insert(rockstarId, PlayerChatHistory(rockstarId, MakeFunctor(*this, &CNetworkTextChat::OnTextFiltered)));
			history = m_ChatHistory.Access(rockstarId);
		}
	}

	if(gnetVerifyf(history, "Could not get a history for player %d", rockstarId))
	{
		history->AppendText(text);
	}
}

bool CNetworkTextChat::GetChatHistoryWithPlayer(const RockstarId& rockstarId, RsonWriter* rw)
{
	bool result = false;
	PlayerChatHistory* reportedPlayerHistory = m_ChatHistory.Access(rockstarId);
	PlayerChatHistory* localPlayerHistory = m_ChatHistory.Access(NetworkInterface::GetLocalGamerHandle().GetRockstarId());
	if(gnetVerifyf(reportedPlayerHistory, "Could not get a history for player %d", rockstarId) && gnetVerifyf(localPlayerHistory, "Could not get a history for local player"))
	{
		result = rw->BeginArray("chatlog", NULL);

		// We need to write the history in chronological order, so we'll loop over the two history and write the oldest line 
		// until we've read all the lines for both players

		int reportedPlayerIndex = reportedPlayerHistory->index;
		int localPlayerIndex = localPlayerHistory->index;

		int reportedPlayerConsumed = 0;
		int localPlayerConsumed = 0;

		while(reportedPlayerConsumed!=MAX_CHATLOG_SIZE || localPlayerConsumed!=MAX_CHATLOG_SIZE)
		{
			ChatHistoryLine* line = NULL;
			RockstarId playerWhoWroteTheLine=0;

			// if we're the oldest next message and not finished already (in which case we would be the oldest message)
			// Or the other is finished
			if(reportedPlayerConsumed!=MAX_CHATLOG_SIZE && 
				(reportedPlayerHistory->history[reportedPlayerIndex].timestamp <= localPlayerHistory->history[localPlayerIndex].timestamp
				 || localPlayerConsumed==MAX_CHATLOG_SIZE) ) 
			{
				playerWhoWroteTheLine=rockstarId;
				line = & reportedPlayerHistory->history[reportedPlayerIndex];
				reportedPlayerConsumed++;
				reportedPlayerIndex++;
				if(reportedPlayerIndex>=MAX_CHATLOG_SIZE)
					reportedPlayerIndex=0;
			}
			if(!playerWhoWroteTheLine && localPlayerConsumed!=MAX_CHATLOG_SIZE) 
			{
				playerWhoWroteTheLine=NetworkInterface::GetLocalGamerHandle().GetRockstarId();
				line = & localPlayerHistory->history[localPlayerIndex];
				localPlayerConsumed++;
				localPlayerIndex++;
				if(localPlayerIndex>=MAX_CHATLOG_SIZE)
					localPlayerIndex=0;
			}
			// Append the line
			if(gnetVerifyf(line, "No next line to append") && strlen(line->message)>0 && playerWhoWroteTheLine)
			{
				rw->Begin(NULL, NULL);
				result = result && rw->WriteInt64("t", line->timestamp);
				result = result && rw->WriteInt64("id", playerWhoWroteTheLine);
				result = result && rw->WriteString("m", line->message);
				rw->End();
			}
		}

		result = result && rw->End();
	}
	return result;
}

bool CNetworkTextChat::IsTyperValid(const EndpointId endpointId) const
{
	return m_TextChat.HaveTyper(endpointId);
}

void CNetworkTextChat::PlayerHasLeft(const rlGamerInfo& gamerInfo)
{
	if(!m_IsInitialized)
	{
		gnetDebug1("PlayerHasLeft :: Not removing %s, Textchat not Initialised", gamerInfo.GetName());
		return; 
	}

	gnetAssertf(gamerInfo.IsValid(), "PlayerHasLeft :: Invalid gamer info");

	gnetDebug1("PlayerHasLeft :: Removing typer %s", gamerInfo.GetName());
	RemoveTyper(gamerInfo);
}

void CNetworkTextChat::OnTunablesRead()
{
	m_AllowedSentFromGroups = CLiveManager::GetValidSentFromGroupsFromTunables(DEFAULT_TEXT_CHAT_VALID_SEND_GROUPS, "TEXT_CHAT");

	s_IsTextFilterEnabled = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_CHAT_FILTER_ENABLED", 0x31c99855), s_IsTextFilterEnabled);
	s_FilterHoldingMaxRequests = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_CHAT_FILTER_HOLDING_PEN_MAX_REQUESTS", 0x197ba46b), s_FilterHoldingMaxRequests);
	s_FilterHoldingPenIntervalMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_CHAT_FILTER_HOLDING_PEN_INTERVAL_MS", 0xa233de30), s_FilterHoldingPenIntervalMs);
	s_FilterSpamMaxRequests = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_CHAT_FILTER_SPAM_MAX_REQUESTS", 0xa5417a51), s_FilterSpamMaxRequests);
	s_FilterSpamIntervalMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_CHAT_FILTER_SPAM_INTERVAL_MS", 0x80c6f91c), s_FilterSpamIntervalMs);
}

CNetworkTextChat::PlayerChatHistory::PlayerChatHistory(RockstarId pid, NetworkTextFilter::OnReceivedTextProcessedCB callback)
	: playerid(pid)
	, index (0)
{
	// build a handle so we don't need to keep doing this
	gamerHandle.ResetSc(playerid);

	if(IsTextChatFilteringEnabled())
	{
		filter.Init(
			callback,
			gamerHandle,
			s_FilterHoldingMaxRequests,
			s_FilterHoldingPenIntervalMs,
			s_FilterSpamMaxRequests,
			s_FilterSpamIntervalMs
			OUTPUT_ONLY(, "TextChat"));
	}
}

void CNetworkTextChat::PlayerChatHistory::Update()
{
	if(IsTextChatFilteringEnabled())
	{
		filter.Update();
	}
}

void CNetworkTextChat::PlayerChatHistory::AppendText(const char* text)
{
	if(IsTextChatFilteringEnabled() && filter.IsInitialised())
	{
		// if filtering is enabled, throw it at that first
		filter.AddReceivedText(gamerHandle, text);
	}
	else
	{
		// otherwise, we can treat this as filtered text
		AppendFilteredText(text);
	}
}

void CNetworkTextChat::PlayerChatHistory::AppendFilteredText(const char* textMessage)
{
	formatf(history[index].message, "%s", textMessage);
	history[index].timestamp = rlGetPosixTime();
	index++;
	if (index >= MAX_CHATLOG_SIZE)
		index = 0;
}

void CNetworkTextChat::PlayerChatHistory::Reset()
{
	for (int i = 0; i < MAX_CHATLOG_SIZE; i++)
		history[i].Reset();
	index = 0;
	filter.Shutdown();
}

#endif