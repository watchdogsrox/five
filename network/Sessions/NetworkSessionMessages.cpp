//
// name:		NetworkSessionMessages.cpp
// description:	Messages used by the session management code in network match
// written by:	Daniel Yelland
//
#include "NetworkSessionMessages.h"

#include "fwnet/netlogsplitter.h"

#include "Network/NetworkInterface.h"
#include "Network/Players/NetworkPlayerMgr.h"

NETWORK_OPTIMISATIONS();

///////////////////////////////////////////////////////////////////////////////////////
// CMsgJoinRequest
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(CMsgJoinRequest);

CMsgJoinRequest::CMsgJoinRequest()
	: m_JoinRequestFlags(JoinFlags::JF_Default)
	, m_BuildType(DEFAULT_BUILD_TYPE_VALUE)
	, m_SocketPort(0)
	, m_CloudDataHash(0)
	, m_Pool(POOL_NORMAL)
	, m_NetworkTimeout(0)
{
}

void CMsgJoinRequest::Reset(
	const unsigned joinRequestFlags,
	const CNetGamePlayerDataMsg& playerDataMsg)
{
	m_JoinRequestFlags = joinRequestFlags;
	m_BuildType = DEFAULT_BUILD_TYPE_VALUE;
	m_SocketPort = CNetwork::GetSocketPort();
	m_AssetVerifier = CNetwork::GetAssetVerifier();
	m_CloudDataHash	= NetworkGameConfig::GetMatchmakingDataHash();
	m_Pool = NetworkGameConfig::GetMatchmakingPool();
	m_NetworkTimeout = (CNetwork::GetTimeoutTime() / 1000); //< in ms, convert to s

	gnetAssertf(m_NetworkTimeout < MAX_TIMEOUT, "Network :: Timeout value too large! Is: %d, Max: %d", m_NetworkTimeout, MAX_TIMEOUT);

	m_PlayerData = playerDataMsg;
}

int CMsgJoinRequest::GetMessageDataBitSize() const
{
	datNullExportBuffer bb;
	gnetVerify(m_AssetVerifier.Serialise(bb));
	const unsigned SIZEOF_ASSET_VERIFIER = bb.GetNumBitsWritten();

	const int dataSize = 
		SIZEOF_JOIN_REQUEST +
		SIZEOF_BUILD_TYPE + 
		SIZEOF_SOCKET_PORT + 
		SIZEOF_ASSET_VERIFIER +
		SIZEOF_CLOUD_DATA_HASH +
		SIZEOF_POOL +
		SIZEOF_TIMEOUT +
		m_PlayerData.GetMessageDataBitSize();

	return dataSize;
}

void CMsgJoinRequest::WriteToLogFile(bool OUTPUT_ONLY(received), netSequence OUTPUT_ONLY(seqNum), const netPlayer& OUTPUT_ONLY(player))
{
#if !__NO_OUTPUT
    netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());
    if(received)
		log.WriteMessageHeader(received, seqNum, "RECEIVED_JOIN_REQUEST", "", player.GetLogName());
    else
		log.WriteMessageHeader(received, seqNum, "SENDING_JOIN_REQUEST", "", player.GetLogName());

	log.WriteDataValue("JoinRequestFlags", "0x%x", m_JoinRequestFlags);
	log.WriteDataValue("Build Type", "%d", m_BuildType);
	log.WriteDataValue("Socket Port", "%d", m_SocketPort);
	log.WriteDataValue("Cloud Data Hash", "0x%08x", m_CloudDataHash);
	log.WriteDataValue("Multiplayer Pool", "%s", NetworkUtils::GetPoolAsString(m_Pool));
	log.WriteDataValue("Network Timeout", "%d", m_NetworkTimeout);

	m_PlayerData.WriteToLogFile(received, seqNum, player);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////
// CMsgJoinResponse
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(CMsgJoinResponse);

CMsgJoinResponse::CMsgJoinResponse()
	: m_ResponseCode(RESPONSE_DENY_UNKNOWN)
	, m_HostFlags(0)
	, m_bCanQueue(false)
	, m_SessionPurpose(SessionPurpose::SP_None)
	, m_HostTime(0)
	, m_ModelVariationID(0)
	, m_NumActiveMatchmakingGroups(0)
	, m_bHasLaunched(false)
	, m_ActivitySpectatorsMax(0)
	, m_ActivityPlayersMax(RL_MAX_GAMERS_PER_SESSION)
	, m_TimeCreated(0)
    , m_UniqueCrewLimit(0)
    , m_CrewsOnly(false)
    , m_MaxMembersPerCrew(0)
{

}

int CMsgJoinResponse::GetMessageDataBitSize() const
{
	int size = SIZEOF_RESPONSE_CODE +
			   SIZEOF_HOST_FLAGS + 
		       1 + // bool m_bCanQueue
			   SIZEOF_SESSION_PURPOSE + 
			   SIZEOF_TIME_CREATED;

	if(m_SessionPurpose == SessionPurpose::SP_Freeroam)
	{
		size += SIZEOF_TIME + 
				SIZEOF_MODEL_VARIATION_ID + 
				SIZEOF_NUM_MM_GROUPS + 
				(SIZEOF_MM_GROUP * MAX_ACTIVE_MM_GROUPS) +
				(SIZEOF_ACTIVE_MM_GROUP * MAX_ACTIVE_MM_GROUPS);
	}
	else if(m_SessionPurpose == SessionPurpose::SP_Activity)
	{
		size += 1 + // bool m_bHasLaunched
				SIZEOF_ACTIVITY_SPECTATOR_MAX + 
				SIZEOF_ACTIVITY_PLAYER_MAX;

		if(m_bHasLaunched)
		{
			size += SIZEOF_TIME + 
					SIZEOF_MODEL_VARIATION_ID;
		}
	}

	return size;
}

void CMsgJoinResponse::WriteToLogFile(bool bReceived, netSequence seqNum, const netPlayer &player)
{
	netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());
	if(bReceived)
		log.WriteMessageHeader(bReceived, seqNum, "RECEIVED_PARTY_JOIN_RESPONSE", "", player.GetLogName());
	else
		log.WriteMessageHeader(bReceived, seqNum, "SENDING_PARTY_JOIN_RESPONSE", "", player.GetLogName());

	log.WriteDataValue("Response Code", "%d", m_ResponseCode);
	log.WriteDataValue("Host Flags", "%d", m_HostFlags);
	log.WriteDataValue("Can Queue", "%s", m_bCanQueue ? "True" : "False");
	log.WriteDataValue("Session Purpose", "%d", static_cast<int>(m_SessionPurpose));

    log.WriteDataValue("Max Unique Crews", "%d", m_UniqueCrewLimit);
    log.WriteDataValue("Max Crew Members", "%d", m_MaxMembersPerCrew);
    log.WriteDataValue("Only Crews", "%s", m_CrewsOnly ? "True" : "False");

	if(m_SessionPurpose == SessionPurpose::SP_Freeroam)
	{
		log.WriteDataValue("Host Time", "%d", m_HostTime);
		log.WriteDataValue("Model Variation ID", "%d", m_ModelVariationID);
		log.WriteDataValue("Num Matchmaking Groups", "%d", m_NumActiveMatchmakingGroups);
		for(unsigned i = 0; i < m_NumActiveMatchmakingGroups; i++)
		{
			log.WriteDataValue("Matchmaking Group Max", "%d", m_MatchmakingGroupMax[i]);
			log.WriteDataValue("Active Matchmaking Group", "%d", m_ActiveMatchmakingGroup[i]);
		}
	}
	else if(m_SessionPurpose == SessionPurpose::SP_Activity)
	{
		log.WriteDataValue("Has Launched", "%s", m_bHasLaunched ? "True" : "False");
		log.WriteDataValue("Activity Spectators Max", "%d", m_ActivitySpectatorsMax);
		log.WriteDataValue("Activity Players Max", "%d", m_ActivityPlayersMax);

		if(m_bHasLaunched)
		{
			log.WriteDataValue("Host Time", "%d", m_HostTime);
			log.WriteDataValue("Model Variation ID", "%d", m_ModelVariationID);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionToGameStart
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionToGameStart);

void MsgTransitionToGameStart::Reset(const rlGamerHandle& hGamer, const int nGameMode, const bool bIsTransitionFromLobby)
{
    m_hGamer = hGamer;
	m_GameMode = nGameMode;
	m_IsTransitionFromLobby = bIsTransitionFromLobby;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionToGameNotify
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionToGameNotify);

void MsgTransitionToGameNotify::Reset(const rlGamerHandle& hFromGamer, const rlSessionInfo& hInfo, eNotificationType nNotificationType, bool bIsPrivate)
{
    m_hGamer = hFromGamer;
	m_SessionInfo = hInfo;
	m_NotificationType = static_cast<u8>(nNotificationType);
    m_bIsPrivate = bIsPrivate;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionToActivityStart
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionToActivityStart);

void MsgTransitionToActivityStart::Reset(const rlGamerHandle& hGamer, const bool bIsAsync)
{
    m_hGamer = hGamer;
	m_bIsAsync = bIsAsync;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionToActivityFinish
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionToActivityFinish);

void MsgTransitionToActivityFinish::Reset(const rlGamerHandle& hGamer, const rlSessionInfo& hInfo, bool bIsPrivate)
{
    m_bDidSucceed = hInfo.IsValid();
    m_hGamer = hGamer;
    m_SessionInfo = hInfo;
    m_bIsPrivate = bIsPrivate;
}

void MsgTransitionToActivityFinish::Reset(const rlGamerHandle& hGamer)
{
    m_bDidSucceed = false;
    m_hGamer = hGamer;
    m_SessionInfo.Clear();
    m_bIsPrivate = false;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgLostConnectionToHost
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgLostConnectionToHost);

void MsgLostConnectionToHost::Reset(const u64 sessionId, const rlGamerHandle& hGamer)
{
	m_SessionId = sessionId;
	m_hGamer = hGamer;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgBlacklist
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgBlacklist);

void MsgBlacklist::Reset(const u64 sessionId, const rlGamerHandle& hGamer, eBlacklistReason nReason)
{
	m_SessionId = sessionId;
	m_hGamer = hGamer;
	m_nReason = static_cast<int>(nReason);
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgKickPlayer
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgKickPlayer);

void MsgKickPlayer::Reset(const u64 sessionId, const rlGamerId gamerId, const unsigned reason, const unsigned numComplaints, const rlGamerHandle& hGamer)
{
	m_SessionId = sessionId;
	m_GamerId = gamerId;
	m_Reason = reason;
	m_NumberOfComplaints = numComplaints;
	m_hComplainer = hGamer;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgRadioStationSync
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgRadioStationSync);

///////////////////////////////////////////////////////////////////////////////////////
// MsgRadioStationSyncRequest
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgRadioStationSyncRequest);

void MsgRadioStationSyncRequest::Reset(const u64 sessionId, const rlGamerId& nGamerID)
{
    m_SessionId = sessionId;
    m_GamerID = nGamerID;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgRequestTransitionParameters
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgRequestTransitionParameters);

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionParameters
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionParameters);

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionParameterString
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionParameterString);

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionGamerInstruction
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionGamerInstruction);

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionStarted
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgSessionEstablished);
NET_MESSAGE_IMPL(MsgSessionEstablishedRequest);

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionLaunch
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionLaunch);

///////////////////////////////////////////////////////////////////////////////////////
// MsgTransitionLaunchNotify
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgTransitionLaunchNotify);

///////////////////////////////////////////////////////////////////////////////////////
// MsgAddQueuedJoinRequest
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgCheckQueuedJoinRequest);

///////////////////////////////////////////////////////////////////////////////////////
// MsgCheckQueuedJoinRequestInviteReply
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgCheckQueuedJoinRequestInviteReply);

///////////////////////////////////////////////////////////////////////////////////////
// MsgCheckQueuedJoinRequestReply
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL(MsgCheckQueuedJoinRequestReply);

///////////////////////////////////////////////////////////////////////////////////////
// MsgPlayerCardSync
///////////////////////////////////////////////////////////////////////////////////////

NET_MESSAGE_IMPL(MsgPlayerCardSync);

MsgPlayerCardSync::MsgPlayerCardSync()
{
	m_gamerID.Clear();
	m_sizeOfData = 0;
	sysMemSet(m_data, 0, MAX_BUFFER_SIZE);
}

void MsgPlayerCardSync::Reset(const rlGamerId& gamerID, const u32 sizeOfData, u8* inbuffer)
{
	if (gnetVerify(sizeOfData > 0) 
		&& gnetVerify(inbuffer) 
		&& gnetVerify(sizeOfData <= MAX_BUFFER_SIZE))
	{
		m_gamerID = gamerID;
		m_sizeOfData = sizeOfData;
		sysMemCpy(m_data, inbuffer, m_sizeOfData);
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// MsgPlayerCardRequest
///////////////////////////////////////////////////////////////////////////////////////

NET_MESSAGE_IMPL(MsgPlayerCardRequest);

MsgPlayerCardRequest::MsgPlayerCardRequest()
{
	m_gamerID.Clear();
}

void MsgPlayerCardRequest::Reset(const rlGamerId& gamerID)
{
	m_gamerID = gamerID;
}

///////////////////////////////////////////////////////////////////////////////////////
// MsgDebugStall
///////////////////////////////////////////////////////////////////////////////////////
NET_MESSAGE_IMPL( MsgDebugStall );

//eof 

