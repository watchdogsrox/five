//
// filename:	GamePresenceEvents.h
// description:	
//
#ifndef INC_GAMEPRESENCEEVENTS_H_
#define INC_GAMEPRESENCEEVENTS_H_

#include "netGamePresenceEvent.h"
#include "rline/rl.h"
#include "rline/rlsessioninfo.h"
#include "string/string.h"
#include "atl/string.h"

#include "rline/clan/rlclancommon.h"
#include "rline/ugc/rlugccommon.h"

#include "network/NetworkTypes.h"
#include "network/SocialClubServices/netLocalizedCloudStrings.h"
#include "network/Sessions/NetworkSessionUtils.h"
#include "network/NetworkInterface.h"
#include "script/value.h"

namespace rage
{
	class bkBank;
    struct ReceivedMessageData;
}

struct scrUGCStateUpdate_Data;
struct scrBountyInboxMsg_Data;

namespace GamePresenceEvents
{
	void RegisterEvents();
	void Update();
	void OpenNetworking();
	void CloseNetworking();
	void OnPlayerMessageReceived(const ReceivedMessageData &messageData);

	void SetRejectMismatchedAuthenticatedSender(const bool reject);
	
	BANK_ONLY(void AddWidgets(bkBank &bank));

	class DeferredGamePresenceEventsList;
}

class netDeferredGamePresenceEventIntf
{
	friend class GamePresenceEvents::DeferredGamePresenceEventsList;

public:
	virtual ~netDeferredGamePresenceEventIntf() {}

protected:

	//Used to pump the event
	virtual bool Update() = 0;

	//Implemented on each class to create (rage_new) a deep copy of this instance to push to the other list.
	virtual netDeferredGamePresenceEventIntf* CreateCopy() const = 0;

	void AddToDeferredList() const;
};

//////////////////////////////////////////////////////////////////////////
// class C_ReplaceMe_PresenceEvent : public netGamePresenceEvent
// {
// public:
// 	GAME_PRESENCE_EVENT_DECL(<event_TYPE_NAME>)
// 
// 	C_ReplaceMe_PresenceEvent() 
// 	: netGamePresenceEvent()
// 	{
// 
// 	}
// 
// 	static void Trigger(/*List of paramas*/, SendFlag flags);

// 	// 
// #if __BANK
// 	static void AddWidgets(bkBank& bank);
// #endif
// 
// protected:
// 	virtual bool Serialise(RsonWriter* rw) const;
// 	virtual bool Deserialise(const RsonReader* rr);
// 	virtual bool Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& sender, const bool isAuthenticatedSender) const;
// };

// PURPOSE:
//	Handles receiving a stat update from another gamer (usually a friend or 
//	crew member) and sends the update to script via a script event
class CStatUpdatePresenceEvent : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(StatUpdate);

public:

	CStatUpdatePresenceEvent()
		: m_statNameHash(0)
		, m_type(VAL_INT)
	{
		m_from[0] = '\0';
		m_iVal = 0;
		m_value2 = 0;
		m_stringData[0] = '\0';
	}
 
	CStatUpdatePresenceEvent(u32 statNameHash, const char* gamerTag, int value, u32 value2, const char* stringData)
		: m_statNameHash(statNameHash)
		, m_type(VAL_INT)
		, m_value2(value2)
	{ 
		safecpy(m_from, gamerTag);
		m_iVal = value;
		safecpy(m_stringData, stringData);
	}

	CStatUpdatePresenceEvent(u32 statNameHash, const char* gamerTag, float value, u32 value2, const char* stringData)
		: m_statNameHash(statNameHash)
		, m_type(VAL_FLOAT)
		, m_value2(value2)
	{
		safecpy(m_from, gamerTag);
		m_fVal = value;
		safecpy(m_stringData, stringData);
	}
	
	static void Trigger(u32 statId, int value, u32 value2, const char* stringData, SendFlag flags);
	static void Trigger(u32 statId, float value, u32 value2, const char* stringData, SendFlag flags);

	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	u32 m_statNameHash;
	enum ValType{ VAL_INT, VAL_FLOAT } m_type;

	union 
	{
		float m_fVal;
		int m_iVal;
	};
	u32 m_value2;

	char m_from[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	char m_stringData[16*sizeof(scrValue)]; //from a scrTextLabel63, see definition
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE:
//	Used to communicate when a friend has joined another crew.
class CFriendCrewJoinedPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(FriendCrewJoined);

public:

	CFriendCrewJoinedPresenceEvent();
	CFriendCrewJoinedPresenceEvent(const rlClanDesc& clanId, const char* name);

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(const rlClanDesc& clanId, SendFlag flags = SendFlag_AllFriends));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlClanDesc m_clanDesc;
	char m_from[RL_MAX_DISPLAY_NAME_BUF_SIZE];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE:
//	Used to communicate when a friend has created a crew.
class CFriendCrewCreatedPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(FriendCreatedCrew);

public:
	
	CFriendCrewCreatedPresenceEvent();
	CFriendCrewCreatedPresenceEvent(const char* name);

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(SendFlag flags = SendFlag_AllFriends));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	//rlClanDesc m_clanDesc;
	char m_from[RL_MAX_DISPLAY_NAME_BUF_SIZE];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE:
//	
class CMissionVerifiedPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(mission_verified);

public:

	CMissionVerifiedPresenceEvent();

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(const char* missionName, SendFlag flags = SendFlag_AllFriends));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlGamerHandle m_creatorGH;
	char m_creatorDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	char m_missionContentId[RLUGC_MAX_CONTENTID_CHARS];
	char m_missionName[RLUGC_MAX_CONTENT_NAME_CHARS];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used for sending and receiving player updates on UGC scores.	
class CUGCStatUpdatePresenceEvent : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(UGCStatUpdate)

public:

	CUGCStatUpdatePresenceEvent() 
		: netGamePresenceEvent()
		, m_score(0)
		, m_score2(0)
		, m_missionType(0)
		, m_missionSubType(0)
		, m_raceLaps(0)
		, m_swapSenderWithCoplayer(false)
	{
		m_fromGamerDisplayName[0] = '\0';
		m_missionContentId[0] = '\0';
		m_missionName[0] = '\0';
		m_CoPlayerDisplayName[0] = '\0';
	}

	static void Trigger(const char* content_mission_id, int score, SendFlag flags);
	static void Trigger(const rlGamerHandle* pRecips, const int numRecips, const scrUGCStateUpdate_Data* pData);

	BANK_ONLY(static void AddWidgets(bkBank& bank));

	const char*				GetFromGamerTag() const {return m_fromGamerDisplayName;}
	const char*				GetMissoinName() const { return m_missionName; }
	const char*				GetContentId() const { return m_missionContentId; }
	const rlGamerHandle&	GetGamerHandle() const { return m_fromGH; }
	const int				GetScore() const { return m_score; }
	const char*				GetCoPlayer() const {return m_CoPlayerDisplayName; }
	void					GetMissionTypeInfo(int& outMissionType, int& outMissionSubType, int& outLaps) const
	{
		outMissionType = m_missionType; outMissionSubType = m_missionSubType; outLaps = m_raceLaps;
	}
	bool					GetSwapSenderWithCoPlayer() const { return m_swapSenderWithCoplayer; }

protected:

	rlGamerHandle m_fromGH;
	char m_fromGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	char m_missionContentId[RLUGC_MAX_CONTENTID_CHARS];
	int m_score;
	int m_score2;

	char m_missionName[RLUGC_MAX_CONTENT_NAME_CHARS];
	char m_CoPlayerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	int m_missionType;
	int m_missionSubType;
	int m_raceLaps;
	bool m_swapSenderWithCoplayer;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	BASE class for text type presence messages that could have text 
//	Also derives from netDeferredGamePresenceEventIntf to have the interface for getting added to the deferred list
class CRockstarMsgPresenceEvent_Base : public netGameServerPresenceEvent, public netDeferredGamePresenceEventIntf
{

protected: 

	CRockstarMsgPresenceEvent_Base() 
		: netGameServerPresenceEvent()
		, m_bCloudString(false)
	{
		m_message[0] = '\0';
		m_cloudStringKey[0] = '\0';
	}

	//netGameServerPresenceEvent pure virtuals
	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(virtual bool Serialise_Debug(RsonWriter* rw) const override);

	//netGamePresenceEvent pure virtuals
	virtual bool Deserialise(const RsonReader* rr);
	virtual bool Apply(const rlGamerInfo& gamerInfo, const rlGamerHandle& sender, const bool isAuthenticatedSender) final;
	
	//netDeferredGamePresenceEventIntf pure virtuals
	virtual bool Update();
	
	//pure virtuals required for this class
	virtual bool HandleTextReceived() const = 0;
	bool DoCloudTextRequest() const;
	void ReleaseCloudRequest() const;

	const char* GetText() const;
	
	char m_message[384];  //This is oversized, but whatever.
	bool m_bCloudString;
	char m_cloudStringKey[netLocalizedStringInCloudMgr::CLOUD_STRING_KEY_SIZE];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Messaging received for the local player from the web SCAdmin tool
class CRockstarMsgPresenceEvent : public CRockstarMsgPresenceEvent_Base
{
	// use BASE_DECL as CRockstarMsgPresenceEvent_Base already implements serialization
	GAME_PRESENCE_EVENT_BASE_DECL(rockstar_message)

public:

	CRockstarMsgPresenceEvent() 
	: CRockstarMsgPresenceEvent_Base()
	{
	}

	static void CacheNewMessages(bool bCache) { ms_bCacheNewMsg = bCache; }
	static bool HasNewMessage() { return ms_bHasNewMsg; }

	static const char* GetLastMessage()
	{ 
		ms_bHasNewMsg = false;
		return ms_sLastMsg.c_str(); 		
	}

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(const char* msg, SendFlag flags, bool bUseCloudLoc = false));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	virtual netDeferredGamePresenceEventIntf* CreateCopy() const;
	virtual bool HandleTextReceived() const;

private:

	static bool ms_bCacheNewMsg;
	static bool ms_bHasNewMsg;
	static atString ms_sLastMsg;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Messaging for a crew from services from the web SCAdmin tool
class CRockstarCrewMsgPresenceEvent : public CRockstarMsgPresenceEvent_Base
{
	// use BASE_DECL as CRockstarMsgPresenceEvent_Base already implements serialization
	GAME_PRESENCE_EVENT_BASE_DECL(crew_message)

public:
	
	CRockstarCrewMsgPresenceEvent() 
	: CRockstarMsgPresenceEvent_Base()
	{
		m_clanId = RL_INVALID_CLAN_ID;
		m_clanTag[0] = '\0';
	}

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(const char* msg, rlClanId clanId, const char* clanTAg, SendFlag flags));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(virtual bool Serialise_Debug(RsonWriter* rw) const override);
	virtual bool Deserialise(const RsonReader* rr);

	virtual netDeferredGamePresenceEventIntf* CreateCopy() const;
	virtual bool HandleTextReceived() const;

	rlClanId m_clanId;
	char m_clanTag[RL_CLAN_TAG_MAX_CHARS];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to award injections of stuff like cash or XP
class CGameAwardPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(game_award)

public:

	CGameAwardPresenceEvent() 
	: netGameServerPresenceEvent()
	{
 
	}
 
	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static bool Trigger_Debug(u32 awardTypeHash, float value, SendFlag flags, const rlGamerHandle* pRecips, const int numRecips));
	BANK_ONLY(static void AddWidgets(bkBank& bank));
 
protected:

	char m_from[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	u32 m_awardTypeHash;
	float m_awardAmount;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to invite a player to a voice session
class CVoiceSessionInvite : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(vinv)

public:

	CVoiceSessionInvite() 
	: netGamePresenceEvent()
	{
		m_SessionInfo.Clear();
		m_hGamer.Clear();
		m_szGamerDisplayName[0] = '\0';
	}

	CVoiceSessionInvite(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hFrom, const char* szGamerName) 
	: netGamePresenceEvent()
	{
		m_SessionInfo = hSessionInfo;
		m_hGamer = hFrom;
		m_SessionInfo.ToString(m_szSessionInfo, &m_nSessionInfoSize);
		safecpy(m_szGamerDisplayName, szGamerName);
	}

	static void Trigger(const rlGamerHandle& hTo, const rlSessionInfo& hSessionInfo); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hGamer;
	unsigned m_nSessionInfoSize;
	char m_szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
	char m_szGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to respond to a voice session invite
class CVoiceSessionResponse : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(vres)

public:

	CVoiceSessionResponse() 
	: netGamePresenceEvent()
	{
		m_nResponse = 0;
		m_nResponseCode = 0;
		m_hGamer.Clear();
	}

	CVoiceSessionResponse(unsigned nResponse, int nResponseCode, const rlGamerHandle& hFrom) 
	: netGamePresenceEvent()
	{
		m_nResponse = nResponse;
		m_nResponseCode = nResponseCode;
		m_hGamer = hFrom;
	}

	static void Trigger(const rlGamerHandle& hTo, const unsigned nResponse, const int nResponseCode); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	unsigned m_nResponse;
	int m_nResponseCode;
	rlGamerHandle m_hGamer;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	 Used to invite a player to a session
class CGameInvite : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(ginv)

public:

	CGameInvite() 
		: netGamePresenceEvent()
	{
		Clear();
	}

	CGameInvite(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hFrom)
	{
		Clear();

		m_SessionInfo = hSessionInfo;
		m_hGamer = hFrom;
		m_SessionInfo.ToString(m_szSessionInfo);
	}

	CGameInvite(
		const rlSessionInfo& hSessionInfo,
		const rlGamerHandle& hFrom,
		const char* szGamerName,
		const int nGameMode,
		const char* szContentID,
		const rlGamerHandle& hContentCreator,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent,
		const unsigned nFlags) 
		: netGamePresenceEvent()
	{
		Clear();

		m_SessionInfo = hSessionInfo;
		m_hGamer = hFrom;
		m_SessionInfo.ToString(m_szSessionInfo);
		safecpy(m_szGamerDisplayName, szGamerName);
		m_GameMode = nGameMode;
		safecpy(m_szContentId, szContentID);
		m_hContentCreator = hContentCreator;
		safecpy(m_UniqueMatchId, pUniqueMatchId);
		m_InviteFrom = nInviteFrom;
		m_InviteType = nInviteType;
		m_PlaylistLength = nPlaylistLength;
		m_PlaylistCurrent = nPlaylistCurrent;
		m_Flags = nFlags;
	}

	void Clear()
	{
		m_szGamerDisplayName[0] = '\0';
		m_GameMode = MultiplayerGameMode::GameMode_Invalid;
		m_szContentId[0] = '\0';
		m_hContentCreator.Clear();
		m_UniqueMatchId[0] = '\0';
		m_InviteFrom = 0;
		m_InviteType = 0;
		m_PlaylistLength = 0;
		m_PlaylistCurrent = 0;
		m_Flags = 0;
		m_CrewID = RL_INVALID_CLAN_ID;
		m_InviteId = INVALID_INVITE_ID;
		m_StaticDiscriminatorEarly = 0;
		m_StaticDiscriminatorInGame = 0;
		m_JoinQueueToken = 0;
	};

	static void Trigger(
		const rlGamerHandle* hTo, 
		const int numRecips, 
		const rlSessionInfo& hSessionInfo, 
		const char* szContentID, 
		const rlGamerHandle& hContentCreator, 
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength, 
		const unsigned nPlaylistCurrent,
		const unsigned nFlags);

	static void TriggerFromJoinQueue(const rlGamerHandle& hTo, const rlSessionInfo& hSessionInfo, unsigned nJoinQueueToken); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hGamer;
	char m_szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
	char m_szGamerDisplayName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	int m_GameMode;
	char m_szContentId[UGC_CONTENT_ID_MAX];
	rlGamerHandle m_hContentCreator;
	char m_UniqueMatchId[MAX_UNIQUE_MATCH_ID_CHARS];
	int m_InviteFrom;
	int m_InviteType;
	unsigned m_PlaylistLength;
	unsigned m_PlaylistCurrent;
	unsigned m_Flags;
	unsigned m_CrewID;
	unsigned m_InviteId;
	unsigned m_StaticDiscriminatorEarly;
	unsigned m_StaticDiscriminatorInGame;
	unsigned m_JoinQueueToken;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to cancel a presence invite
class CGameInviteCancel : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(ginvc)

public:

	CGameInviteCancel() 
		: netGamePresenceEvent()
	{
		m_SessionInfo.Clear();
		m_hGamer.Clear();
	}

	CGameInviteCancel(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hFrom) 
		: netGamePresenceEvent()
	{
		m_SessionInfo = hSessionInfo;
		m_hGamer = hFrom;
		m_SessionInfo.ToString(m_szSessionInfo, &m_nSessionInfoSize);
	}

	static void Trigger(const rlGamerHandle& hTo, const rlSessionInfo& hSessionInfo); 
	static void Trigger(const rlGamerHandle* hTo, const unsigned gamerHandleCount, const rlSessionInfo& hSessionInfo); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hGamer;
	unsigned m_nSessionInfoSize;
	char m_szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to reply to a game invite
class CGameInviteReply : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(grep)

public:

    CGameInviteReply() 
        : netGamePresenceEvent()
        , m_ResponseFlags(0)
		, m_InviteFlags(0)
		, m_InviteId(0)
        , m_CharacterRank(1)
        , m_bDecisionMade(false)
        , m_bDecision(false)
    {
		m_Invitee.Clear();
        m_ClanTag[0] = '\0';
    }

    CGameInviteReply(const rlGamerHandle& hFrom, const rlSessionToken& sessionToken, const unsigned nInviteId, const unsigned responseFlags, const unsigned inviteFlags, const int characterRank, const char* clanTag, const bool bDecisionMade, const bool bDecision)
        : netGamePresenceEvent()
        , m_Invitee(hFrom)
        , m_SessionToken(sessionToken)
		, m_InviteId(nInviteId)
        , m_ResponseFlags(responseFlags)
		, m_InviteFlags(inviteFlags)
        , m_CharacterRank(characterRank)
        , m_bDecisionMade(bDecisionMade)
        , m_bDecision(bDecision)
    {
        safecpy(m_ClanTag, clanTag);
    }

    static void Trigger(const rlGamerHandle& hTo, const rlSessionToken& sessionToken, const unsigned nInviteId, const unsigned responseFlags, const unsigned inviteFlags, const int characterRank, const char* clanTag, const bool bDecisionMade, const bool bDecision);
    BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

    rlGamerHandle m_Invitee;
    rlSessionToken m_SessionToken;
	unsigned m_InviteId;
    unsigned m_ResponseFlags;
	unsigned m_InviteFlags; 
    int m_CharacterRank;
    bool m_bDecisionMade;
	bool m_bDecision;
	char m_ClanTag[MAX_CLAN_TAG_LENGTH];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to invite a player to a tournament
class CTournamentInvite : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(tinv)

public:

    CTournamentInvite() 
        : netGameServerPresenceEvent()
    {
        m_SessionInfo.Clear();
		m_hSessionHost.Clear();
        m_szContentId[0] = '\0';
        m_TournamentEventId = 0;
		m_InviteFlags = InviteFlags::IF_None;
    }

    CTournamentInvite(const rlSessionInfo& hSessionInfo, const char* szContentID, const unsigned tournamentEventId, const unsigned inviteFlags) 
        : netGameServerPresenceEvent()
    {
        m_SessionInfo = hSessionInfo;
        m_SessionInfo.ToString(m_szSessionInfo);
        safecpy(m_szContentId, szContentID);
        m_TournamentEventId = tournamentEventId;
		m_InviteFlags = inviteFlags;
    }
	
	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(const rlGamerHandle& target));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

    rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hSessionHost;
    char m_szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
    char m_szContentId[UGC_CONTENT_ID_MAX];
    unsigned m_TournamentEventId;
	unsigned m_InviteFlags;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to inform players in another session where the local player is
class CFollowInvite : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(finv)

public:

    CFollowInvite() 
        : netGamePresenceEvent()
    {
        m_SessionInfo.Clear();
        m_hGamer.Clear();
    }

    CFollowInvite(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hGamer) 
        : netGamePresenceEvent()
    {
        m_SessionInfo = hSessionInfo;
        m_hGamer = hGamer;
        m_SessionInfo.ToString(m_szSessionInfo);
    }

    static void Trigger(const rlGamerHandle* hToGamers, const int nGamers, const rlSessionInfo& hSessionInfo); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

    rlSessionInfo m_SessionInfo;
    rlGamerHandle m_hGamer; 
    char m_szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to invite to a session from web service. 
//	Sent to the session host.
class CAdminInvite : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(ainv)

public:

    CAdminInvite() 
        : netGameServerPresenceEvent()
    {
        m_SessionInfo.Clear();
		m_hGamer.Clear();
		m_hHostGamer.Clear();
		m_InviteFlags = InviteFlags::IF_None;
    }

    CAdminInvite(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hInvitee, const unsigned inviteFlags) 
        : netGameServerPresenceEvent()
    {
        m_SessionInfo = hSessionInfo;
		m_hGamer = hInvitee;
		m_hHostGamer.Clear();
        m_SessionInfo.ToString(m_szSessionInfo);
        m_hGamer.ToString(m_szGamerHandle);
		m_InviteFlags = inviteFlags;
    }

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug());
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

    rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hGamer;
	rlGamerHandle m_hHostGamer;
	unsigned m_InviteFlags; 
	char m_szSessionInfo[rlSessionInfo::TO_STRING_BUFFER_SIZE];
    char m_szGamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to inform a host that an admin invite has been sent to 
//	the indicated player.
class CAdminInviteNotification : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(anot)

public:

	CAdminInviteNotification()
		: netGameServerPresenceEvent()
	{
		m_SessionInfo.Clear();
		m_hInvitedGamer.Clear();
	}

	CAdminInviteNotification(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hInvitee)
		: netGameServerPresenceEvent()
	{
		m_SessionInfo = hSessionInfo;
		m_hInvitedGamer = hInvitee;
	}

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug());
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hInvitedGamer;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Request to join session queue 
class CJoinQueueRequest : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(jreq)

public:

	CJoinQueueRequest() 
		: netGamePresenceEvent()
	{
		m_hGamer.Clear();
		m_bIsSpectator = false;
		m_nCrewID = RL_INVALID_CLAN_ID;
		m_bConsumePrivate = false;
		m_nUniqueToken = 0;
	}

	CJoinQueueRequest(const rlGamerHandle& hGamer, bool bIsSpectator, s64 nCrewID, bool bConsumePrivate, unsigned nUniqueToken) 
		: netGamePresenceEvent()
	{
		m_hGamer = hGamer;
		m_bIsSpectator = bIsSpectator;
		m_nCrewID = nCrewID;
		m_bConsumePrivate = bConsumePrivate;
		m_nUniqueToken = nUniqueToken;
	}

	static void Trigger(const rlGamerHandle& hTo, const rlGamerHandle& hGamer, bool bIsSpectator, s64 nCrewID, bool bConsumePrivate, unsigned nUniqueToken); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	rlGamerHandle m_hGamer;
	bool m_bIsSpectator;
	s64 m_nCrewID; 
	bool m_bConsumePrivate;
	unsigned m_nUniqueToken; 
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Update status of a join queue
class CJoinQueueUpdate : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(jqup)

public:

	CJoinQueueUpdate() 
		: netGamePresenceEvent()
	{
		m_bCanQueue = false;
		m_nUniqueToken = 0;
	}

	CJoinQueueUpdate(bool bCanQueue, unsigned nUniqueToken) 
		: netGamePresenceEvent()
	{
		m_bCanQueue = bCanQueue;
		m_nUniqueToken = nUniqueToken;
	}

	static void Trigger(const rlGamerHandle& hTo, bool bCanQueue, unsigned nUniqueToken); 
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	bool m_bCanQueue;
	unsigned m_nUniqueToken;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	
class msgRequestKickFromHost
{
public:
	NET_MESSAGE_DECL(msgRequestKickFromHost, MSG_REQUEST_KICK_FROM_HOST);
	msgRequestKickFromHost() 
		: m_kickReason(0) 
	{}

	msgRequestKickFromHost(const KickReason rsn) 
		: m_kickReason( (u8) rsn ) 
	{}
	int GetMessageDataBitSize() const { return 8; }

	NET_MESSAGE_SER(bb, message) { return bb.SerUns(message.m_kickReason, 8); }

	u8 m_kickReason;
};

class CFingerOfGodPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(finger)

public:
	CFingerOfGodPresenceEvent() 
	: netGameServerPresenceEvent()
	, m_typeHash(FOG_TYPE_NONE)
	, m_uData(0)
	{
		m_charData[0] = '\0';

#if __ASSERT
		ATSTRINGHASH("vehicle", (u32)FOG_TYPE_VEHICLE);
		ATSTRINGHASH("kick", (u32)FOG_TYPE_KICK);
		ATSTRINGHASH("blacklist", (u32)FOG_TYPE_BLACKLIST);
		ATSTRINGHASH("relax", (u32)FOG_TYPE_RELAX);
		ATSTRINGHASH("smitethee", (u32)FOG_TYPE_SMITETHEE);
#endif
	}
 
#if NET_GAME_PRESENCE_SERVER_DEBUG
	static void TriggerKick();
	static void TriggerBlacklist();
	static void TriggerSmite();
	static void TriggerRelax();
	static void TriggerVeh(const char* veh);
#endif

	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	enum eFogType {
		FOG_TYPE_NONE = 0,
		FOG_TYPE_VEHICLE = 0xDD245B9C,		//ATSTRINGHASH("vehicle", 0xDD245B9C),
		FOG_TYPE_KICK = 0x84643284,			//ATSTRINGHASH("kick", 0x84643284), 
		FOG_TYPE_BLACKLIST = 0x1d106c54,	//ATSTRINGHASH("blacklist", 0x1d106c54), 
		FOG_TYPE_RELAX = 0x4349E22B,		//ATSTRINGHASH("relax", 0x4349E22B),
		FOG_TYPE_SMITETHEE = 0xC2F12A5B,	//ATSTRINGHASH("smitethee", 0xC2F12A5B),
	};

	eFogType m_typeHash;
	union
	{
		int m_iData;
		u32 m_uData;
		float m_fData;
	};
	char m_charData[64];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	
class CNewsItemPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(news)

public:
  
	CNewsItemPresenceEvent() 
	: netGameServerPresenceEvent()
	, m_activeDate(0)
	, m_expireDate(0)
	, m_trackingId(0)
	, m_priority(0)
	, m_secondaryPriority(-1)
	{
		m_newsStoryKey[0] = '\0';
	}
  
	static const int MAX_NUM_TYPE_CHAR = 32;
	static const int MAX_NUM_TYPES = 10;
	typedef atFixedArray<atHashString, MAX_NUM_TYPES> TypeHashList;
	static const int NEW_STORY_KEY_SIZE = 64;

	const char*		GetStoryKey() const				{ return m_newsStoryKey; }
	u32				GetActiveDate() const			{ return m_activeDate;	}
	u32				GetExpireDate() const			{ return m_expireDate; }
	u32				GetTrackingId() const			{ return m_trackingId; }
	int				GetStoryPriority() const		{return m_priority; }
	int				GetSecondaryPriority() const	{return m_secondaryPriority; }
	const TypeHashList&	GetTypeList() const			{ return m_types; }

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(const char* storyKey, SendFlag flags));
	
#if __BANK
	static void AddWidgets(bkBank& bank);
	void Set(const char* key, const TypeHashList& typeList);
#endif

protected:

	char m_newsStoryKey[NEW_STORY_KEY_SIZE];
	u32 m_activeDate;
	u32 m_expireDate;
	u32 m_trackingId;
	int m_priority;
	int m_secondaryPriority;
	TypeHashList m_types;
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	
class CBountyPresenceEvent : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(bounty)

public:
	CBountyPresenceEvent()
		: netGamePresenceEvent()
		, m_iOutcome(0)
		, m_iCash(0)
		, m_iRank(0)
		, m_iTime(0)
	{
		m_tagFrom[0] = '\0';
		m_tagTarget[0] = '\0';
	}

	static void Trigger(const rlGamerHandle* pRecips, const int numRecips, const scrBountyInboxMsg_Data* inData);
	BANK_ONLY(static void AddWidgets(bkBank& bank));

	char m_tagFrom[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	char m_tagTarget[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	int	m_iOutcome;
	int m_iCash;
	int m_iRank;
	int m_iTime;

protected:

};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Force an update of our session presence data
class CForceSessionUpdatePresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(ForceSessionUpdate)

public:
	
	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug());
	BANK_ONLY(static void AddWidgets(bkBank& bank));
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to generate an event that can be interpreted by the
//	the game (using the hash and two, optional, parameters)
class CGameTriggerEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(gtri)

public:

	static const int MAX_STRING = 64;

	CGameTriggerEvent()
		: netGameServerPresenceEvent()
		, m_nEventHash(0)
		, m_nEventParam1(0)
		, m_nEventParam2(0)
	{
		m_szEventString[0] = '\0';
	}

	CGameTriggerEvent(int nEventHash, int nEventParam1, int nEventParam2, const char* szEventString)
		: netGameServerPresenceEvent()
		, m_nEventHash(nEventHash)
		, m_nEventParam1(nEventParam1)
		, m_nEventParam2(nEventParam2)
	{
		safecpy(m_szEventString, szEventString);
	}

	NET_GAME_PRESENCE_SERVER_DEBUG_ONLY(static void Trigger_Debug(int nEventHash, int nEventParam1, int nEventParam2, const char* szEventString, int nSendFlags));
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	u32 m_nEventHash;
	int m_nEventParam1;
	int m_nEventParam2;
	char m_szEventString[MAX_STRING];
};

//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to send cross-session texts between players
class CTextMessageEvent : public netGamePresenceEvent
{
	GAME_PRESENCE_EVENT_DECL(TextMessage)

public:

	static const int MAX_STRING = 256;
	CTextMessageEvent()
		: netGamePresenceEvent()
	{
		m_szTextMessage[0] = '\0';
	}

	CTextMessageEvent(const char* szTextMessage)
		: netGamePresenceEvent()
	{
		safecpy(m_szTextMessage, szTextMessage);
		m_hFromGamer = NetworkInterface::GetLocalGamerHandle();
	}

	static void Trigger(const rlGamerHandle* dest, const char* szTextMessage);
	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	char m_szTextMessage[MAX_STRING];
	rlGamerHandle m_hFromGamer;
};

#if __NET_SHOP_ACTIVE
//////////////////////////////////////////////////////////////////////////
// PURPOSE: 
//	Used to award injections of stuff like cash or Inventory via the game server.
class GameServerPresenceEvent : public netGameServerPresenceEvent
{
	GAME_PRESENCE_EVENT_SERVER_ONLY_DECL(gs_award)

public:

	GameServerPresenceEvent() : netGameServerPresenceEvent() {}

	BANK_ONLY(static void AddWidgets(bkBank& bank));

protected:

	bool m_fullRefreshRequested;
	bool m_updatePlayerBalance;
	atArray<int> m_items;
	u32 m_awardTypeHash;
	s32 m_awardAmount;
};
#endif // __NET_SHOP_ACTIVE

#endif // !INC_GAMEPRESENCEEVENTS_H_
