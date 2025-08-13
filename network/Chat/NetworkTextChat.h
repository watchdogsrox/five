#if __WIN32PC

#ifndef NETWORKTEXTCHAT_H
#define NETWORKTEXTCHAT_H

// network headers
#include "network/General/NetworkTextFilter.h"

// rage headers
#include "atl/delegate.h"
#include "avchat/textchat.h"

#include "fwnet/netinterface.h"

class CNetworkPlayerMgr;
class CNetGamePlayer;
struct VoiceGamerSettings;


class CNetworkTextChat 
{
	typedef atDelegator<void (CNetworkTextChat* textChat, const rlGamerHandle& gamerHandle, const char* text, bool bTeamOnly)> Delegator;
public:

	//PURPOSE
	//  Text chat delegate type.
	//  The signature for event handlers is:
	//  void OnEvent(CNetworkTextChat* textChat, const rlGamerId& gamerId, const char* text)
	//
	typedef Delegator::Delegate Delegate;

    CNetworkTextChat();

    ~CNetworkTextChat();

    bool Init(netConnectionManager* cxnMgr);

    void Shutdown();

    bool IsInitialized() const;

    void Update();

	//PURPOSE
	//  Adds a delegate that will be called when text messages are received.
	void AddDelegate(Delegate* dlgt);

	//PURPOSE
	//  Removes a delegate.
	void RemoveDelegate(Delegate* dlgt);

	bool AddTyper(const rlGamerInfo& gamerInfo,
				  const EndpointId endpointId);

	void RemoveTyper(const rlGamerInfo& gamerInfo);

	void RemoveAllRemoteTypers();

	void RemoveAllTypers();

    //Returns true if the player with the given gamer id is currently
    //voice chatting.
    bool IsPlayerTyping(const rlGamerId& gamerId) const;

    //Returns true if any player is currently voice chatting.
    bool IsAnyPlayerTyping() const;

    //Returns true if the player with the given gamer id has a keyboard
    //connected.
    bool PlayerHasKeyboard(const rlGamerId& gamerId) const;

    //Mutes/unmutes the given player
    bool SetPlayerMuted(const rlGamerId& gamerId,
                        const bool muted);

    //Returns true if the player with the given id is muted.
    bool IsPlayerMutedByMe(const rlGamerId& gamerId) const;

    //Returns true if the player with the given id has muted us.
    bool AmIMutedByPlayer(const rlGamerId& gamerId) const;

    //Returns true if the player with the given id is blocked.
    bool IsPlayerBlockedByMe(const rlGamerId& gamerId) const;

    //Returns true if the player with the given id has blocked us.
    bool AmIBlockedByPlayer(const rlGamerId& gamerId) const;

	// Can the player talk? Did we mute/block each other?
	bool CanIHearPlayer(const rlGamerId& gamerId) const;

    //Enable/disable team-only chat.  When enabled only players on
    //the same team can chat with each other.
    //Applies only to in-game chat, not lobby chat.
    void SetTeamOnlyChat(const bool teamOnlyChat);

	//Enable/disable chat from non-spectators to spectators.
	void SetSpectatorToNonSpectatorChat(const bool enable);

	//Queues a single string of text to be sent from a local player
	bool SubmitText(const rlGamerInfo& gamerInfo, const char* text, bool bTeamChat);

	//Tells the text chat system that the specified local gamer is typing.
	bool SetLocalPlayerIsTyping(const rlGamerId& gamerId);

	bool GetChatHistoryWithPlayer(const RockstarId& rockstarId, RsonWriter* rw);
	
	//Allows the text chat system to let script control setting up teams.
	void SetScriptControllingTeams(bool bOverride)		{ m_IsScriptControllingTeams = bOverride; }

	//Allows the text chat system to let script control setting up teams.
	bool GetScriptControllingTeams()					{ return m_IsScriptControllingTeams; }

	// Used to manually control who is on the same team as the local player.
	void SetOnSameTeam(const EndpointId endpointId, bool onSameTeam) { m_TextChat.SetOnSameTeam(endpointId, onSameTeam); }

	bool IsTyperValid(const EndpointId endpointId) const;
	void PlayerHasLeft(const rlGamerInfo& gamerInfo);

	void OnTunablesRead();

private:

    // Handles voice status updates
    void HandleTextStatus(const ReceivedMessageData &messageData);

    void OnNetEvent(const ReceivedMessageData &messageData);

	void OnTextChatEvent(
		rage::TextChat* textChat,
		const rlGamerHandle& gamerHandle ,
		const char* text,
		bool bTeamOnly);

	//Text that has passed our filtering
	void OnTextFiltered(const rlGamerHandle& sender, const char* text);
	
	//Updates the collection of typers who we can send and receive text chat
    void UpdateTypers();

	// Append a line of chat to the history
	void AppendChatLog(const rlGamerHandle& gamerHandle, const char* text);

	const VoiceGamerSettings* FindVoiceGamerSettingsForGamerId(const rlGamerId& gamerId) const;

    NetworkPlayerEventDelegate m_Dlgt;
	TextChat::Delegate m_TextChatDlgt;
	Delegator m_Delegator;

    // the player representing my machine
    CNetGamePlayer* m_myPlayer;

    TextChat m_TextChat;

    bool m_TeamOnlyChat : 1;
    bool m_IsInitialized : 1;
	bool m_IsSpectatorToNonSpectatorChatEnabled	: 1;
	bool m_IsTransitioning : 1;
	bool m_IsScriptControllingTeams : 1;

	unsigned m_AllowedSentFromGroups;

	// Chat history
	static const int MAX_CHATLOG_SIZE = 25;
	struct ChatHistoryLine
	{
		char message[MAX_TEXT_MESSAGE_LEN];
		u64 timestamp;

		ChatHistoryLine() : timestamp(0)
		{
			memset(message, 0, MAX_TEXT_MESSAGE_LEN);
		}

		void Reset() { memset(message, 0, MAX_TEXT_MESSAGE_LEN); timestamp = 0; }
	};

	struct PlayerChatHistory
	{
		ChatHistoryLine history[MAX_CHATLOG_SIZE];
		RockstarId playerid;
		int index;

		PlayerChatHistory(RockstarId pid, NetworkTextFilter::OnReceivedTextProcessedCB callback);

		void Update();
		void Reset();
		void AppendText(const char* text);
		void AppendFilteredText(const char* text);

	private:

		NetworkTextFilter filter;
		rlGamerHandle gamerHandle;
	};

	typedef atMap<RockstarId,PlayerChatHistory, atMapHashFn<u64>, atMapEquals<u64> > ChatHistoryMap;

	ChatHistoryMap m_ChatHistory;

#if __DEV
    RecorderId m_BandwidthRecorderId;
#endif
};

#endif  // NETWORKTEXTCHAT_H
#endif  // __WIN32PC
