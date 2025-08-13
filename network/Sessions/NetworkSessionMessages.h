//
// name:		NetworkSessionMessages.h
// description:	Messages used by the session management code in network match
// written by:	Daniel Yelland
//

#ifndef NETWORK_SESSION_MESSAGES_H
#define NETWORK_SESSION_MESSAGES_H

#include "net/message.h"

#include "Network/NetworkTypes.h"
#include "Network/Sessions/NetworkSessionUtils.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Players/NetGamePlayerMessages.h"
#include "audio/radiostation.h"
#include "game/weather.h"

class CNetworkAssetVerifier;

//PURPOSE
// Message sent to a host to request to join a network session
struct CMsgJoinRequest
{
public:

    NET_MESSAGE_DECL(CMsgJoinRequest, CMSG_JOIN_REQUEST);

    //PURPOSE
    // Class constructor
    CMsgJoinRequest();

    //PURPOSE
    // Resets the contents of the message
	void Reset(
		const unsigned joinRequestFlags,
		const CNetGamePlayerDataMsg& playerDataMsg);

	// serialization constants
	static const unsigned SIZEOF_JOIN_REQUEST		= JoinFlags::JF_Num;
	static const unsigned SIZEOF_BUILD_TYPE			= datBitsNeeded<NUM_BUILD_TYPES>::COUNT;
	static const unsigned SIZEOF_SOCKET_PORT		= 32;
	static const unsigned SIZEOF_CLOUD_DATA_HASH	= 32;
	static const unsigned SIZEOF_POOL				= datBitsNeeded<POOL_NUM>::COUNT;
	static const unsigned MAX_TIMEOUT				= 255;
	static const unsigned SIZEOF_TIMEOUT			= datBitsNeeded<MAX_TIMEOUT>::COUNT;

    NET_MESSAGE_SER(bb, msg)
    {
		return bb.SerUns(msg.m_JoinRequestFlags, SIZEOF_JOIN_REQUEST)
			&& bb.SerUns(msg.m_BuildType, SIZEOF_BUILD_TYPE)
			&& bb.SerUns(msg.m_SocketPort, SIZEOF_SOCKET_PORT)
			&& msg.m_AssetVerifier.Serialise(bb)
			&& bb.SerUns(msg.m_CloudDataHash, SIZEOF_CLOUD_DATA_HASH)
			&& bb.SerUns(msg.m_Pool, SIZEOF_POOL)
			&& bb.SerUns(msg.m_NetworkTimeout, SIZEOF_TIMEOUT)
			&& CNetGamePlayerDataMsg::SerMsgPayload(bb, msg.m_PlayerData);
    }

    //PURPOSE
    // Returns the size of the message data in bits
    int GetMessageDataBitSize() const;

    //PURPOSE
    // Writes the contents of the message to a log file
    //PARAMS
    // received - Was this message sent or received?
    // seqNum   - Sequence number this message was sent
    // player   - Player this message was sent to/from
    void WriteToLogFile(bool bReceived, netSequence seqNum, const netPlayer &player);

	// needed when processing the join request
	unsigned m_JoinRequestFlags;
	unsigned m_BuildType;
	unsigned m_SocketPort;
	CNetworkAssetVerifier m_AssetVerifier;
	unsigned m_CloudDataHash;
	unsigned m_NetworkTimeout;
	eMultiplayerPool m_Pool;

	// when we request a join, the player data will be required if this is accepted in order to add the player
	CNetGamePlayerDataMsg m_PlayerData;
};

//PURPOSE
// Response to a join request
struct CMsgJoinResponse
{
	NET_MESSAGE_DECL(CMsgJoinResponse, CMSG_JOIN_RESPONSE);
	CMsgJoinResponse();

	static const unsigned SIZEOF_RESPONSE_CODE = datBitsNeeded<RESPONSE_NUM_CODES>::COUNT;
	static const unsigned SIZEOF_HOST_FLAGS = 32;
	static const unsigned SIZEOF_SESSION_PURPOSE = datBitsNeeded<SessionPurpose::SP_Max>::COUNT + 1; // can be -1

    static const unsigned SIZEOF_TIME_TOKEN = 32;
	static const unsigned SIZEOF_TIME = 32;
	static const unsigned SIZEOF_MODEL_VARIATION_ID = 3;
	static const unsigned SIZEOF_NUM_MM_GROUPS = datBitsNeeded<MAX_ACTIVE_MM_GROUPS>::COUNT;
	static const unsigned SIZEOF_ACTIVE_MM_GROUP = datBitsNeeded<MM_GROUP_MAX>::COUNT;
	static const unsigned SIZEOF_MM_GROUP = datBitsNeeded<RL_MAX_GAMERS_PER_SESSION>::COUNT;

	static const unsigned SIZEOF_ACTIVITY_SPECTATOR_MAX = datBitsNeeded<RL_MAX_GAMERS_PER_SESSION>::COUNT;
	static const unsigned SIZEOF_ACTIVITY_PLAYER_MAX = datBitsNeeded<RL_MAX_GAMERS_PER_SESSION>::COUNT;
	static const unsigned SIZEOF_TIME_CREATED = 64;
    static const unsigned SIZEOF_CREW_LIMITS = 8;

	NET_MESSAGE_SER(bb, msg)
	{
		bool bSuccess = true;
		bSuccess &= bb.SerUns(msg.m_ResponseCode, SIZEOF_RESPONSE_CODE);
		bSuccess &= bb.SerUns(msg.m_HostFlags, SIZEOF_HOST_FLAGS);
		bSuccess &= bb.SerBool(msg.m_bCanQueue);
		bSuccess &= bb.SerInt(msg.m_SessionPurpose, SIZEOF_SESSION_PURPOSE);
		bSuccess &= bb.SerUns(msg.m_TimeCreated, SIZEOF_TIME_CREATED);

        // crew limits
        bSuccess &= bb.SerUns(msg.m_UniqueCrewLimit, SIZEOF_CREW_LIMITS);
        bSuccess &= bb.SerUns(msg.m_MaxMembersPerCrew, SIZEOF_CREW_LIMITS);
        bSuccess &= bb.SerBool(msg.m_CrewsOnly);

		if(msg.m_SessionPurpose == SessionPurpose::SP_Freeroam)
		{
            bSuccess &= bb.SerUns(msg.m_HostTimeToken, SIZEOF_TIME_TOKEN);
			bSuccess &= bb.SerUns(msg.m_HostTime, SIZEOF_TIME);
			bSuccess &= bb.SerUns(msg.m_ModelVariationID, SIZEOF_MODEL_VARIATION_ID);

			bSuccess &= bb.SerUns(msg.m_NumActiveMatchmakingGroups, SIZEOF_NUM_MM_GROUPS);
			for(unsigned i = 0; i < msg.m_NumActiveMatchmakingGroups && i < MAX_ACTIVE_MM_GROUPS; i++)
			{
				bSuccess &= bb.SerUns(msg.m_MatchmakingGroupMax[i], SIZEOF_MM_GROUP);
				bSuccess &= bb.SerUns(msg.m_ActiveMatchmakingGroup[i], SIZEOF_ACTIVE_MM_GROUP);
			} 
		}
		else if(msg.m_SessionPurpose == SessionPurpose::SP_Activity)
		{
			bSuccess &= bb.SerBool(msg.m_bHasLaunched);
			bSuccess &= bb.SerUns(msg.m_ActivitySpectatorsMax, SIZEOF_ACTIVITY_SPECTATOR_MAX);
			bSuccess &= bb.SerUns(msg.m_ActivityPlayersMax, SIZEOF_ACTIVITY_PLAYER_MAX);

			if(msg.m_bHasLaunched)
			{
                bSuccess &= bb.SerUns(msg.m_HostTimeToken, SIZEOF_TIME_TOKEN);
				bSuccess &= bb.SerUns(msg.m_HostTime, SIZEOF_TIME);
				bSuccess &= bb.SerUns(msg.m_ModelVariationID, SIZEOF_MODEL_VARIATION_ID);
			}
		}

		return bSuccess;
	}

	int GetMessageDataBitSize() const;
	void WriteToLogFile(bool bReceived, netSequence seqNum, const netPlayer &player);

	eJoinResponseCode m_ResponseCode;
	unsigned m_HostFlags;				// These are needed so that we have the correct setting on becoming a migrated host
	bool m_bCanQueue;
	SessionPurpose m_SessionPurpose;

	// for the freeroam session
    unsigned m_HostTimeToken;           // The host's current network time token
	unsigned m_HostTime;				// The host's current network time
	unsigned m_ModelVariationID;		// Model variation ID used by the host, used to provide some variety over models streamed for ambient population
	unsigned m_NumActiveMatchmakingGroups; 
	unsigned m_MatchmakingGroupMax[MAX_ACTIVE_MM_GROUPS];
	unsigned m_ActiveMatchmakingGroup[MAX_ACTIVE_MM_GROUPS];
	u64 m_TimeCreated;				 // The creation time of the original session
	
	// for the transition / job session
	bool m_bHasLaunched;
	unsigned m_ActivitySpectatorsMax;
	unsigned m_ActivityPlayersMax;

    // for crew limits
    u8 m_UniqueCrewLimit; 
    u8 m_MaxMembersPerCrew;
    bool m_CrewsOnly; 
};

//PURPOSE
// Sent in transition session to maintain parameter states
struct MsgRequestTransitionParameters
{
    NET_MESSAGE_DECL(MsgRequestTransitionParameters, MSG_REQUEST_TRANSITION_PARAMETERS);

    void Reset(const rlGamerHandle& hGamer)
    {
        m_hGamer = hGamer;
    }

    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerUser(msg.m_hGamer);
    }

    rlGamerHandle m_hGamer;
};

//PURPOSE
// Sent in transition session to maintain parameter states
struct MsgTransitionParameters
{
	NET_MESSAGE_DECL(MsgTransitionParameters, MSG_TRANSITION_PARAMETERS);

	MsgTransitionParameters()
	{
		m_hGamer.Clear();
		m_nNumParameters = 0;

		for(unsigned i = 0; i < TRANSITION_MAX_PARAMETERS; i++)
		{
			m_Parameters[i].m_nID = 0;
			m_Parameters[i].m_nValue = 0;
		}
	}

	void Reset(const rlGamerHandle& hGamer, sTransitionParameter* pParameters, const unsigned nNumParameters)
	{
		m_hGamer = hGamer;
		m_nNumParameters = nNumParameters;

		for(unsigned i = 0; i < m_nNumParameters; i++)
		{
			m_Parameters[i].m_nID = pParameters[i].m_nID;
			m_Parameters[i].m_nValue = pParameters[i].m_nValue;
		}
	}

	NET_MESSAGE_SER(bb, msg)
	{
		bool bSuccess = bb.SerUser(msg.m_hGamer);
		bSuccess &= bb.SerUns(msg.m_nNumParameters, SIZEOF_PARAMETER_NUM);

		if (!gnetVerifyf(msg.m_nNumParameters <= TRANSITION_MAX_PARAMETERS, "m_nNumParameters[%u] overflow", msg.m_nNumParameters))
		{
			return false;
		}

		for(unsigned i = 0; i < msg.m_nNumParameters; i++)
		{
			bSuccess &= bb.SerUns(msg.m_Parameters[i].m_nID, 8);
			bSuccess &= bb.SerUns(msg.m_Parameters[i].m_nValue, 32);
		}

		return bSuccess;
	}

	static const unsigned SIZEOF_PARAMETER_NUM = datBitsNeeded<TRANSITION_MAX_PARAMETERS>::COUNT;

	rlGamerHandle m_hGamer;
	unsigned m_nNumParameters; 
	sTransitionParameter m_Parameters[TRANSITION_MAX_PARAMETERS];
};

struct MsgTransitionParameterString
{
	NET_MESSAGE_DECL(MsgTransitionParameterString, MSG_TRANSITION_PARAMETER_STRING);

	void Reset(const rlGamerHandle& hGamer, const char* szParameter)
	{
		m_hGamer = hGamer;
		safecpy(m_szParameter, szParameter);
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_hGamer)
			&& bb.SerStr(msg.m_szParameter, TRANSITION_PARAMETER_STRING_MAX);
	}

	rlGamerHandle m_hGamer;
	char m_szParameter[TRANSITION_PARAMETER_STRING_MAX];
};

struct MsgTransitionGamerInstruction
{
	NET_MESSAGE_DECL(MsgTransitionGamerInstruction, MSG_TRANSITION_GAMER_INSTRUCTION);

	void Reset(const rlGamerHandle& hFromGamer, const rlGamerHandle& hToGamer, const char* szGamerName, const int nInstruction, const int nInstructionParam, const unsigned nUniqueToken)
	{
		m_hFromGamer = hFromGamer;
		m_hToGamer = hToGamer;
        formatf(m_szGamerName, RL_MAX_DISPLAY_NAME_BUF_SIZE, szGamerName);
		m_nInstruction = nInstruction;
		m_nInstructionParam = nInstructionParam;
		m_nUniqueToken = nUniqueToken;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_hFromGamer)
			&& bb.SerUser(msg.m_hToGamer)
            && bb.SerStr(msg.m_szGamerName, RL_MAX_DISPLAY_NAME_BUF_SIZE)
			&& bb.SerInt(msg.m_nInstruction, 32)
			&& bb.SerInt(msg.m_nInstructionParam, 32)
			&& bb.SerUns(msg.m_nUniqueToken, 32);
	}

	rlGamerHandle m_hFromGamer;
	rlGamerHandle m_hToGamer;
    char m_szGamerName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	int m_nInstruction;
	int m_nInstructionParam; 
	unsigned m_nUniqueToken;
};

struct MsgSessionEstablished
{
	NET_MESSAGE_DECL(MsgSessionEstablished, MSG_SESSION_ESTABLISHED);

	MsgSessionEstablished() {}
	MsgSessionEstablished(const u64 sessionId, const rlGamerHandle& hGamer)
	{
		m_SessionId = sessionId;
		m_hFromGamer = hGamer;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_SessionId, 64)
			&& bb.SerUser(msg.m_hFromGamer);
	}

	u64 m_SessionId;
	rlGamerHandle m_hFromGamer;
};

struct MsgSessionEstablishedRequest
{
	NET_MESSAGE_DECL(MsgSessionEstablishedRequest, MSG_SESSION_ESTABLISHED_REQUEST);

	MsgSessionEstablishedRequest() {}
	MsgSessionEstablishedRequest(const u64 sessionId)
	{
		m_SessionId = sessionId;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_SessionId, 64);
	}

	u64 m_SessionId;
};

//PURPOSE
// Sent from a transition host to all peers to launch a transition
struct MsgTransitionLaunch
{
	NET_MESSAGE_DECL(MsgTransitionLaunch, MSG_TRANSITION_LAUNCH);

    static const unsigned SIZEOF_TIME_TOKEN = 32;
	static const unsigned SIZEOF_TIME = 32;
	static const unsigned SIZEOF_MODEL_VARIATION_ID = 3;
	static const unsigned SIZEOF_TIME_CREATED = 64;

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_HostTimeToken, SIZEOF_TIME_TOKEN)
			&& bb.SerUns(msg.m_HostTime, SIZEOF_TIME)
			&& bb.SerUns(msg.m_ModelVariationID, SIZEOF_MODEL_VARIATION_ID)
			&& bb.SerUns(msg.m_TimeCreated, SIZEOF_TIME_CREATED);
	}

    unsigned m_HostTimeToken;        // The host's current network time token
	unsigned m_HostTime;             // The host's current network time
	unsigned m_ModelVariationID;     // Model variation ID used by the host, used to provide some variety over models streamed for ambient population
	u64 m_TimeCreated;				 // The creation time of the original session
};

//PURPOSE
// Sent from a player to all freeroam players to indicate that we are launching a transition session
struct MsgTransitionLaunchNotify
{
	NET_MESSAGE_DECL(MsgTransitionLaunchNotify, MSG_TRANSITION_LAUNCH_NOTIFY);

	MsgTransitionLaunchNotify() {}
	MsgTransitionLaunchNotify(const rlGamerHandle& hGamer)
	{
		m_hGamer = hGamer;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_hGamer);
	}

	rlGamerHandle m_hGamer;
};

//PURPOSE
// Sent when a transition host has started transition back to game
struct MsgTransitionToGameStart
{
	NET_MESSAGE_DECL(MsgTransitionToGameStart, MSG_TRANSITION_TO_GAME_START);
    
    void Reset(const rlGamerHandle& hFromGamer, const int nGameMode, const bool bIsTransitionFromLobby);

    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerUser(msg.m_hGamer)
			&& bb.SerInt(msg.m_GameMode, 16)
			&& bb.SerBool(msg.m_IsTransitionFromLobby);
    }

    rlGamerHandle m_hGamer;
	int m_GameMode; 
	bool m_IsTransitionFromLobby;
};

//PURPOSE
// Sent when a transition host has started transition back to game
struct MsgTransitionToGameNotify
{
	NET_MESSAGE_DECL(MsgTransitionToGameNotify, MSG_TRANSITION_TO_GAME_NOTIFY);

	enum eNotificationType
	{
		NOTIFY_SWAP,
		NOTIFY_UPDATE,
		NOTIFY_MAX,
	};
	static const unsigned SIZEOF_NOTIFICATION_TYPE = datBitsNeeded<NOTIFY_MAX>::COUNT;

	void Reset(const rlGamerHandle& hFromGamer, const rlSessionInfo& hInfo, eNotificationType nNotificationType, bool bIsPrivate);

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_hGamer)
            && bb.SerUser(msg.m_SessionInfo)
			&& bb.SerUns(msg.m_NotificationType, SIZEOF_NOTIFICATION_TYPE)
			&& bb.SerBool(msg.m_bIsPrivate);
	}

    rlGamerHandle m_hGamer;
	rlSessionInfo m_SessionInfo;
	u8 m_NotificationType;
    bool m_bIsPrivate;
};

//PURPOSE
// Sent when leader starts activity join
struct MsgTransitionToActivityStart
{
    NET_MESSAGE_DECL(MsgTransitionToActivityStart, MSG_TRANSITION_TO_ACTIVITY_START);

    void Reset(const rlGamerHandle& hFromGamer, const bool bIsAsync);

    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerUser(msg.m_hGamer)
			&& bb.SerBool(msg.m_bIsAsync);
    }

    rlGamerHandle m_hGamer;
	bool m_bIsAsync;
};

//PURPOSE
// Sent when leader completes activity join
struct MsgTransitionToActivityFinish
{
    MsgTransitionToActivityFinish() : m_bDidSucceed(false), m_bIsPrivate(false) {} 

    NET_MESSAGE_DECL(MsgTransitionToActivityFinish, MSG_TRANSITION_TO_ACTIVITY_FINISH);

    void Reset(const rlGamerHandle& hFromGamer, const rlSessionInfo& hInfo, bool bIsPrivate);
    void Reset(const rlGamerHandle& hFromGamer);
    
    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerBool(msg.m_bDidSucceed)
            && bb.SerUser(msg.m_hGamer)
            && (msg.m_bDidSucceed ? bb.SerUser(msg.m_SessionInfo) : true)
            && (msg.m_bDidSucceed ? bb.SerBool(msg.m_bIsPrivate): true);
    }

    bool m_bDidSucceed; 
    rlGamerHandle m_hGamer;
    rlSessionInfo m_SessionInfo;
    bool m_bIsPrivate; 
};

//PURPOSE
// Sent from a peer to host
struct MsgLostConnectionToHost
{
	NET_MESSAGE_DECL(MsgLostConnectionToHost, MSG_LOST_HOST_CONNECTION);

	void Reset(const u64 sessionId, const rlGamerHandle& hGamer);

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_SessionId, 64)
			&& bb.SerUser(msg.m_hGamer);
	}

	u64 m_SessionId;
	rlGamerHandle m_hGamer;
};

//PURPOSE
// Sent from the host to all peers when a gamer is blacklisted.
struct MsgBlacklist
{
    NET_MESSAGE_DECL(MsgBlacklist, MSG_BLACKLIST);

    void Reset(const u64 sessionId, const rlGamerHandle& hGamer, eBlacklistReason nReason);

    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerUns(msg.m_SessionId, 64)
            && bb.SerUser(msg.m_hGamer)
			&& bb.SerInt(msg.m_nReason, datBitsNeeded<BLACKLIST_NUM + 1>::COUNT);
    }

    u64 m_SessionId;
    rlGamerHandle m_hGamer;
	int m_nReason;
};

//PURPOSE
// Sent from the host to the player to be kicked.
struct MsgKickPlayer
{
	NET_MESSAGE_DECL(MsgKickPlayer, MSG_KICK_PLAYER);

	void Reset(const u64 sessionId, const rlGamerId gamerId, const unsigned reason, const unsigned numComplaints, const rlGamerHandle& hGamer);

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_SessionId, 64) 
			&& bb.SerUser(msg.m_GamerId)
			&& bb.SerUns(msg.m_Reason, datBitsNeeded<KickReason::KR_Num>::COUNT)
			&& bb.SerUns(msg.m_NumberOfComplaints, RL_MAX_GAMERS_PER_SESSION)
			&& msg.m_hComplainer.IsValid() ? bb.SerUser(msg.m_hComplainer) : true;
	}

	u64 m_SessionId;
	rlGamerId m_GamerId;
	unsigned m_Reason;
	unsigned m_NumberOfComplaints;
	rlGamerHandle m_hComplainer;
};

//PURPOSE
// Sent from the host to sync the current session radio settings
struct MsgRadioStationSync
{
	static const unsigned MAX_RADIO_STATIONS = 32;
	static const unsigned SIZEOF_SESSION_ID				 = 64;
	static const unsigned SIZEOF_NUM_RADIO_STATIONS      = datBitsNeeded<MAX_RADIO_STATIONS>::COUNT;
	static const unsigned SIZEOF_RADIO_STATIONS_HASH     = 32;
	static const unsigned SIZEOF_RADIO_RANDOM_SEED       = 32;
	static const unsigned SIZEOF_TRACK_CATEGORY          = audRadioStation::audRadioStationSyncData::TrackId::CATEGORY_BITS;
	static const unsigned SIZEOF_TRACK_INDEX             = audRadioStation::audRadioStationSyncData::TrackId::INDEX_BITS;

    NET_MESSAGE_DECL(MsgRadioStationSync, MSG_RADIO_STATION_SYNC);

    void Reset(u64 nSessionID)
	{
		m_SessionId = nSessionID;
		m_nRadioStations = 0;
	}

	static unsigned GetHeaderBitSize()
	{
		return SIZEOF_SESSION_ID + 
			   SIZEOF_NUM_RADIO_STATIONS;
	}

	static unsigned GetRadioStationBitSize()
	{
		return SIZEOF_RADIO_STATIONS_HASH +
			   SIZEOF_RADIO_RANDOM_SEED +
			   32 + // currentTrackPlaytime
			   SIZEOF_TRACK_CATEGORY +
			   SIZEOF_TRACK_INDEX + 
			   (SIZEOF_TRACK_CATEGORY * audRadioStationHistory::kRadioHistoryLength) +
			   (SIZEOF_TRACK_INDEX * audRadioStationHistory::kRadioHistoryLength);
	}

    NET_MESSAGE_SER(bb, msg)
    {
        bool bSuccess = true; 
        bSuccess &= bb.SerUns(msg.m_SessionId, 64);
        bSuccess &= bb.SerUns(msg.m_nRadioStations, SIZEOF_NUM_RADIO_STATIONS);

        if (!gnetVerifyf(msg.m_nRadioStations <= MAX_RADIO_STATIONS, "m_nRadioStations[%u] overflow", msg.m_nRadioStations))
        {
            return false;
        }

        for(u8 i = 0; i < msg.m_nRadioStations && bSuccess; i++)
        {
			bSuccess &= bb.SerUns(msg.m_SyncData[i].nameHash, SIZEOF_RADIO_STATIONS_HASH);
			bSuccess &= bb.SerUns(msg.m_SyncData[i].randomSeed, SIZEOF_RADIO_RANDOM_SEED);           
			bSuccess &= bb.SerFloat(msg.m_SyncData[i].currentTrackPlaytime);

			bSuccess &= bb.SerUns(msg.m_SyncData[i].nextTrack.category, SIZEOF_TRACK_CATEGORY);
			bSuccess &= bb.SerUns(msg.m_SyncData[i].nextTrack.index,    SIZEOF_TRACK_INDEX);

			for(unsigned j = 0; j < audRadioStationHistory::kRadioHistoryLength; j++)
			{
				bSuccess &= bb.SerUns(msg.m_SyncData[i].trackHistory[j].category, SIZEOF_TRACK_CATEGORY);
				bSuccess &= bb.SerUns(msg.m_SyncData[i].trackHistory[j].index,    SIZEOF_TRACK_INDEX);
			}
        }

        return bSuccess;
    }

    u64 m_SessionId;
    u8 m_nRadioStations; 
    audRadioStation::audRadioStationSyncData m_SyncData[MAX_RADIO_STATIONS];
};

//PURPOSE
// Request radio sync from host
struct MsgRadioStationSyncRequest
{
    NET_MESSAGE_DECL(MsgRadioStationSyncRequest, MSG_RADIO_STATION_SYNC_REQUEST);

    void Reset(const u64 sessionId, const rlGamerId& nGamerID);

    NET_MESSAGE_SER(bb, msg)
    {
        return bb.SerUns(msg.m_SessionId, 64) 
            && bb.SerUser(msg.m_GamerID);
    }

    u64 m_SessionId;
    rlGamerId m_GamerID;
};

//PURPOSE
// Check queued join request (client -> host)
struct MsgCheckQueuedJoinRequest
{
	NET_MESSAGE_DECL(MsgCheckQueuedJoinRequest, MSG_CHECK_QUEUED_JOIN_REQUEST);

	void Reset(const rlGamerHandle& hGamer, bool bIsSpectator, s64 nCrewID, unsigned nUniqueToken, u64 nSessionToken)
	{
		m_hGamer = hGamer;
		m_bIsSpectator = bIsSpectator;
		m_nCrewID = nCrewID;
		m_nUniqueToken = nUniqueToken;
		m_nSessionToken = nSessionToken;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_hGamer) 
			&& bb.SerBool(msg.m_bIsSpectator)
			&& bb.SerInt(msg.m_nCrewID, 64)
			&& bb.SerUns(msg.m_nUniqueToken, 32)
			&& bb.SerUns(msg.m_nSessionToken, 64);
	}

	rlGamerHandle m_hGamer;
	bool m_bIsSpectator;
	s64 m_nCrewID;
	unsigned m_nUniqueToken;
	u64 m_nSessionToken;
};

//PURPOSE
// Check queued join request (client -> host)
struct MsgCheckQueuedJoinRequestInviteReply
{
	NET_MESSAGE_DECL(MsgCheckQueuedJoinRequestInviteReply, MSG_CHECK_QUEUED_JOIN_REQUEST_INVITE_REPLY);

	void Reset(const rlGamerHandle& hGamer, u64 nSessionToken, bool bFreeSlots)
	{
		m_hGamer = hGamer;
		m_nSessionToken = nSessionToken;
		m_bFreeSlots = bFreeSlots;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_hGamer)
			&& bb.SerUns(msg.m_nSessionToken, 64)
			&& bb.SerBool(msg.m_bFreeSlots);
	}

	rlGamerHandle m_hGamer;
	u64 m_nSessionToken;
	bool m_bFreeSlots;
};

//PURPOSE
// Check queued join request (client -> host)
struct MsgCheckQueuedJoinRequestReply
{
	NET_MESSAGE_DECL(MsgCheckQueuedJoinRequestReply, MSG_CHECK_QUEUED_JOIN_REQUEST_REPLY);

	void Reset(unsigned nUniqueToken, bool bInviteSent, u64 nSessionToken)
	{
		m_nUniqueToken = nUniqueToken;
		m_bReservedSlot = bInviteSent;
		m_nSessionToken = nSessionToken;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_nUniqueToken, 32) 
			&& bb.SerBool(msg.m_bReservedSlot)
			&& bb.SerUns(msg.m_nSessionToken, 64);
	}

	unsigned m_nUniqueToken;
	bool m_bReservedSlot;
	u64 m_nSessionToken;
};

//PURPOSE
//  Message sent with all of the player card stats.
struct MsgPlayerCardSync
{
	static const u32 MAX_BUFFER_SIZE = netMessage::MAX_BYTE_SIZEOF_PAYLOAD - sizeof(u32) - sizeof(rlGamerId);

	NET_MESSAGE_DECL(MsgPlayerCardSync, MSG_PLAYER_CARD_SYNC);

	MsgPlayerCardSync();

	void Reset(const rlGamerId& gamerID, const u32 sizeOfData, u8* inbuffer);

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_gamerID)
			&& bb.SerUns(msg.m_sizeOfData, 32)
			&& gnetVerifyf(msg.m_sizeOfData <= MAX_BUFFER_SIZE, "m_sizeOfData[%u] overflow", msg.m_sizeOfData)
			&& bb.SerBytes(msg.m_data, (int)(msg.m_sizeOfData));
	}

	rlGamerId  m_gamerID; 
	u32        m_sizeOfData;
	u8         m_data[MAX_BUFFER_SIZE];
};


//PURPOSE
//  Message sent to request a player card stats.
struct MsgPlayerCardRequest
{
	NET_MESSAGE_DECL(MsgPlayerCardRequest, MSG_PLAYER_CARD_REQUEST);

	MsgPlayerCardRequest();

	void Reset(const rlGamerId& gamerID);

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_gamerID);
	}

	rlGamerId  m_gamerID;
};

//PURPOSE
//  Message sent to connected players when there is a stall.
class MsgDebugStall
{
public:

	NET_MESSAGE_DECL(MsgDebugStall, MSG_DEBUG_STALL);

	MsgDebugStall() : m_StallLength(0), m_NetworkUpdateTimeMs(0) { }
	MsgDebugStall(const unsigned stallLengthIncludingNetworkUpdate, const unsigned networkUpdateTimeMs) 
		: m_StallLength(rage::Min<unsigned>(stallLengthIncludingNetworkUpdate, (1 << SIZEOF_STALL_LENGTH) - 1))
		, m_NetworkUpdateTimeMs(rage::Min<unsigned>(networkUpdateTimeMs, (1 << SIZEOF_NETWORK_UPDATE) - 1))
	{

	}

	static const unsigned SIZEOF_STALL_LENGTH   = 16;
	static const unsigned SIZEOF_NETWORK_UPDATE = 16;

	NET_MESSAGE_SER(bb, message) 
	{ 
		return bb.SerUns(message.m_StallLength, SIZEOF_STALL_LENGTH) && bb.SerUns(message.m_NetworkUpdateTimeMs, SIZEOF_NETWORK_UPDATE); 
	}

	int GetMessageDataBitSize() const { return SIZEOF_STALL_LENGTH + SIZEOF_NETWORK_UPDATE; }

	unsigned m_StallLength;
	unsigned m_NetworkUpdateTimeMs;
};

#endif  // NETWORK_SESSION_MESSAGES_H
