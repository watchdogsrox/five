//
// NetworkVoiceSession.h
//
// Copyright (C) Rockstar Games.  All Rights Reserved.
//
#include "NetworkVoiceSession.h"

// rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "net/message.h"
#include "rline/rlpresence.h"
#include "system/param.h"
#include "system/simpleallocator.h"

// framework Includes
#include "fwnet/netchannel.h"

// game includes
#include "network/SocialClubServices/GamePresenceEvents.h"
#include "Event/EventGroup.h"
#include "Event/EventNetwork.h"
#include "frontend/PauseMenu.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "network/General/NetworkUtil.h"
#include "network/Live/livemanager.h"
#include "network/Sessions/NetworkGameConfig.h"
#include "network/Voice/NetworkVoice.h"
#include "network/xlast/Fuzzy.schema.h"

// parameters
PARAM(voiceSessionAutoAccept, "[network] Auto accepts voice connection requests");
PARAM(voiceSessionPresence, "[network] Auto accepts voice connection requests");

NETWORK_OPTIMISATIONS();

RAGE_DEFINE_SUBCHANNEL(net, voicesession, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_voicesession

// constants
static const unsigned kJOIN_REQUEST_TIMEOUT_DEFAULT = 25000;
static const unsigned kJOIN_RESPONSE_TIMEOUT_DEFAULT = kJOIN_REQUEST_TIMEOUT_DEFAULT - 2000;

// response codes
enum 
{
	kRequest_Accept		= BIT0,
	kRequest_Reject		= BIT1,
	kRequest_Busy		= BIT2,
	kRequest_TimedOut	= BIT3,
	kRequest_Blocked	= BIT5,
	kNumRequestBits		= 5,
};

#define SCRIPT_ACCEPT (1)

#if RSG_NP

// constants
static const unsigned kTRANSACTION_TIMEOUT_DEFAULT = 5000;

//PURPOSE
//  Sent to give a remote connection information about a session
struct snMsgSessionInfo
{
	NET_MESSAGE_DECL(snMsgSessionInfo, SN_MSG_SESSION_INFO);

	void Reset(const rlSessionInfo& sessionInfo, const rlGamerHandle& hGamer, const char* szName)
	{
		m_SessionInfo = sessionInfo;
		m_hGamer = hGamer;
		formatf(m_szGamerTag, szName);
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUser(msg.m_SessionInfo) &&
			   bb.SerUser(msg.m_hGamer) &&
			   bb.SerStr(msg.m_szGamerTag, RL_MAX_NAME_BUF_SIZE);
	}

	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_hGamer;
	char m_szGamerTag[RL_MAX_NAME_BUF_SIZE];
};

//PURPOSE
//  Sent to acknowledge session info receipt
struct snMsgSessionInfoResponse
{
	NET_MESSAGE_DECL(snMsgSessionInfoResponse, SN_MSG_SESSION_INFO_RESPONSE);
	NET_MESSAGE_SER(,) { return true; }
};

//PURPOSE
//  Sent to indicate answer to request
struct snMsgSessionJoinRequest
{
	NET_MESSAGE_DECL(snMsgSessionJoinRequest, SN_MSG_SESSION_JOIN_REQUEST);

	void Reset(const unsigned nResponse, const int nResponseCode)
	{
		m_nResponse = nResponse;
		m_nResponseCode = nResponseCode;
	}

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_nResponse, kNumRequestBits) && 
			   bb.SerInt(msg.m_nResponseCode, 32);
	}

	unsigned m_nResponse;
	int m_nResponseCode;
};

//PURPOSE
//  Sent to acknowledge join response receipt
struct snMsgSessionJoinRequestResponse
{
	NET_MESSAGE_DECL(snMsgSessionJoinRequestResponse, SN_MSG_SESSION_JOIN_REQUEST_RESPONSE);
	NET_MESSAGE_SER(,) { return true; }
};

NET_MESSAGE_IMPL(snMsgSessionInfo);
NET_MESSAGE_IMPL(snMsgSessionInfoResponse);
NET_MESSAGE_IMPL(snMsgSessionJoinRequest);
NET_MESSAGE_IMPL(snMsgSessionJoinRequestResponse);

#endif

//PURPOSE
//  Sent to accept the chat request. We join the session as soon as we receive the 
//  event. This signals that chat can start (for the host)
struct snMsgSessionAcceptChat
{
	NET_MESSAGE_DECL(snMsgSessionAcceptChat, SN_MSG_SESSION_ACCEPT_CHAT);

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

NET_MESSAGE_IMPL(snMsgSessionAcceptChat);

CNetworkVoiceSession::CNetworkVoiceSession()
: m_bIsInitialised(false)
, m_nSessionState(kState_Idle)
, m_nLastTime(0)
, m_nChannelID(NET_INVALID_CHANNEL_ID)
, m_bVoiceShutdownPending(false)
, m_bLeavePending(false)
, m_bAcceptPending(false)
, m_bHasAccepted(false)
, m_bIsInviteResponsePending(false)
, m_nTimeInviteReceived(0)
, m_nJoinRequestTimeout(kJOIN_REQUEST_TIMEOUT_DEFAULT)
, m_nJoinResponseTimeout(kJOIN_RESPONSE_TIMEOUT_DEFAULT)
{
	// bind up delegates
	m_SessionDlgt.Bind(this, &CNetworkVoiceSession::OnSessionEvent);
    m_CxnMgrDlgt.Bind(this, &CNetworkVoiceSession::OnConnectionEvent);
	
	// reset players
	m_InviteRequests.Reset();

#if RSG_NP
	// transaction handlers
	m_RequestHandler.Bind(this, &CNetworkVoiceSession::OnRequest);
	m_ResponseHandler.Bind(this, &CNetworkVoiceSession::OnResponse);
	
	// reset connection list
	m_ConnectionRequests.Reset();
#endif

	// reset status trackers
	m_SessionStatus.Reset();
}

bool CNetworkVoiceSession::Init(sysMemAllocator* pAlloc, netConnectionManager* pCxnMan, const u32 nChannelID)
{
	if(!gnetVerifyf(!m_bIsInitialised, "Init :: Already initialised!"))
		return false;

	// setup the session callbacks
	snSessionOwner sOwner;
	sOwner.GetGamerData.Bind(this, &CNetworkVoiceSession::OnGetGamerData);
	sOwner.HandleJoinRequest.Bind(this, &CNetworkVoiceSession::OnHandleJoinRequest);
	sOwner.ShouldMigrate.Bind(this, &CNetworkVoiceSession::ShouldMigrate);

	// initialise the session
	bool bSuccess = m_Session.Init(pAlloc, sOwner, pCxnMan, nChannelID, "gta5");
	if(!gnetVerifyf(bSuccess, "Init :: Error initialising session"))
		return false;

	// add delegates
	m_Session.AddDelegate(&m_SessionDlgt);

    // connection
    m_pCxnMan = pCxnMan;
    m_pCxnMan->AddChannelDelegate(&m_CxnMgrDlgt, NETWORK_SESSION_VOICE_CHANNEL_ID);

#if RSG_NP
	// setup the transactor
	m_Transactor.Init(pCxnMan);
	m_Transactor.AddRequestHandler(&m_RequestHandler, nChannelID);

	// reset connection list
	m_ConnectionRequests.Reset();
#endif

	// reset invites
	m_nSessionState = kState_Idle;
	m_nChannelID = nChannelID;
	m_InviteRequests.Reset();

	// initialise state
	m_bVoiceShutdownPending = false;
	m_bIsInviteResponsePending = false;
    m_bLeavePending = false;

	// successfully initialised
    m_bIsInitialised = true;

	return true;
}

void CNetworkVoiceSession::Shutdown(bool isSudden)
{
	gnetDebug1("Shutdown :: Initialised: %s", m_bIsInitialised ? "true" : "false");
	if(!m_bIsInitialised)
		return;

	// punt pending invites
	m_InviteRequests.Reset();

	// shutdown session
	m_Session.RemoveDelegate(&m_SessionDlgt);
    m_Session.Shutdown(isSudden, eBailReason::BAIL_INVALID);

    // empty voice gamer list
    m_VoiceGamers.Reset();

    // shutdown connection delegate
    m_pCxnMan->RemoveChannelDelegate(&m_CxnMgrDlgt);
    m_pCxnMan = NULL;

    if(m_SessionStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_SessionStatus);
    m_SessionStatus.Reset();

	// flag voice shutdown
	m_bVoiceShutdownPending = true;

#if RSG_NP
	// delete individual requests
	int nRequests = m_ConnectionRequests.GetCount();
	for(int i = 0; i < nRequests; i++)
		delete m_ConnectionRequests[i];

	// ... and punt the list
	m_ConnectionRequests.Reset();

	// kill transactor
	m_Transactor.Shutdown();
#endif
   
	// uninitialise
	m_bIsInitialised = false;
}

void CNetworkVoiceSession::Bail(bool bEndEvent)
{
    gnetDebug1("Bail :: End event: %s", bEndEvent ? "True" : "False");

    // generate event
    if(bEndEvent)
        GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceSessionEnded());

    // handle voice session here - mirror how the main session is bailed
    Shutdown(true);
    Init(CLiveManager::GetRlineAllocator(), &CNetwork::GetConnectionManager(), NETWORK_SESSION_VOICE_CHANNEL_ID);
}

void CNetworkVoiceSession::Update()
{
	const u32 nCurrentTime = sysTimer::GetSystemMsTime() | 0x01;

	// only update this if we need to
	if(m_nSessionState > kState_Idle)
		m_Session.Update(nCurrentTime);

	// update transactor
#if RSG_NP
	const u32 nDelta = nCurrentTime - m_nLastTime;
	m_Transactor.Update(nDelta);
#endif

	// manage session flow
	switch(m_nSessionState)
	{
	case kState_Idle:
		break;
	case kState_Hosting:
		if(m_SessionStatus.Succeeded())
		{
			gnetDebug1("Update :: Hosted session.");
			m_nSessionState = kState_Started;
			
			// generate event
			GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceSessionStarted());
		}
		else if(!m_SessionStatus.Pending())
		{
			// failed
			gnetError("Update :: Failed to host session.");
			m_nSessionState = kState_Idle;

            // bail from the session
            Bail(true);
		}
		break;
	case kState_Joining:
		if(m_SessionStatus.Succeeded())
		{
			gnetDebug1("Update :: Joined session.");
			CNetwork::InitVoice();
			m_nSessionState = kState_Started;

			// generate events
            GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceConnectionRequested(m_hInviter, m_szInviterName));
            GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceSessionStarted());
        }
		else if(!m_SessionStatus.Pending())
		{
			// failed
			gnetError("Update :: Failed to join session.");
			m_nSessionState = kState_Idle;

            // bail from the session
            Bail(true);
        }
		break;
	case kState_Started:
		break;
	case kState_Leaving:
		if(m_SessionStatus.Succeeded())
		{
			gnetDebug1("Update :: Leaving session.");
			m_nSessionState = kState_Destroying;
			m_SessionStatus.Reset();
			m_Session.Destroy(&m_SessionStatus);
		}
		else if(!m_SessionStatus.Pending())
		{
			// failed
			gnetError("Update :: Failed to leave session.");
			m_nSessionState = kState_Idle;

            // bail from the session
            Bail(true);
		}
		break;
	case kState_Destroying:
		if(m_SessionStatus.Succeeded())
		{
			gnetDebug1("Update :: Destroyed session.");
			m_nSessionState = kState_Idle;

            // clear any parameters
            m_bHasAccepted = false;
            m_bLeavePending = false;
            m_bAcceptPending = false;

			// no longer need voice
			m_bVoiceShutdownPending = true; 

            // empty voice gamer list
            m_VoiceGamers.Reset();

            // generate event
            GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceSessionEnded());
		}
		else if(!m_SessionStatus.Pending())
		{
			// failed
			gnetError("Update :: Failed to destroy session.");
			m_nSessionState = kState_Idle;

            // bail from the session
            Bail(true);
		}
		break;
	default:
		break;
	}

    // manage accept pending request
    if(m_bAcceptPending)
    {
        if(m_nSessionState >= kState_Started)
        {
            gnetDebug1("Update :: Processing accept. Informing existing players!");
            
            // create and send the message
            snMsgSessionAcceptChat tMsg;
            tMsg.m_hGamer = NetworkInterface::GetLocalGamerHandle();
            m_Session.BroadcastMsg(tMsg, NET_SEND_RELIABLE);
            BANK_ONLY(NetworkInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());

            // kill flag
            m_bAcceptPending = false;
        }
    }

    // manage a pending leave request
    if(m_bLeavePending)
    {
        // idle state
        if(m_nSessionState == kState_Idle)
        {
            gnetDebug1("Update :: Idle state. Cancelling leave pending!");
            m_bLeavePending = false;
        }

        // leave when not busy
        if(!IsBusy())
        {
            // we need to be in the started state
            if(m_nSessionState != kState_Started)
            {
                gnetDebug1("Update :: Forcing state to started to leave!");
                m_nSessionState = kState_Started;
            }

            gnetDebug1("Update :: Leaving from pending leave!");
            Leave();

            // reset flag
            m_bLeavePending = false;
        }
    }

	// manage active invite requests
	bool bFinished = false;
	while(!bFinished)
	{
		// assume finished
		bFinished = true; 

		// check for timeout in response
		int nRequests = m_InviteRequests.GetCount();
		for(int i = 0; i < nRequests; i++)
		{
			if(!m_InviteRequests[i].m_bResponseReceived && (nCurrentTime - m_InviteRequests[i].m_nInviteTime) > m_nJoinRequestTimeout)
			{
				// fire event for response timeout
				GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceConnectionResponse(m_InviteRequests[i].m_hGamer, CEventNetworkVoiceConnectionResponse::RESPONSE_ERROR, 0));	

				// punt this request
				m_InviteRequests.Delete(i);
				bFinished = false;

				break;
			}
		}
	}

#if RSG_NP
	// manage connection requests
	int nRequests = m_ConnectionRequests.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		sConnectionRequest* pRequest = m_ConnectionRequests[i];
		switch(pRequest->m_nState)
		{
		case sConnectionRequest::kCR_RequestedPresence:
			if(!pRequest->m_Status.Pending())
			{
                // must have a presence peer address for this to work
				if(pRequest->m_Status.Succeeded() && pRequest->m_aPeerInfo.IsValid())
				{
					// log
					gnetDebug1("Received peer address for %s!", NetworkUtils::LogGamerHandle(pRequest->m_hGamer));

					// grab presence attribute
					char paBuf[rlPeerAddress::TO_STRING_BUFFER_SIZE] = {""};
					pRequest->m_aPeerInfo.GetValue(paBuf, rlPeerAddress::TO_STRING_BUFFER_SIZE);

					// initialise peer address from presence string
					rlPeerAddress addrPeer;
					addrPeer.FromString(paBuf);

					// reset status 
					pRequest->m_Status.Reset();

					const bool isBilateral = false;
					const netTunnelDesc tunnelDesc(NET_TUNNELTYPE_ONLINE, NET_TUNNEL_REASON_DIRECT, m_Session.GetSessionInfo().GetToken().m_Value, isBilateral);

					// open direct connection to this player
					bool bSuccess = CNetwork::GetConnectionManager().OpenTunnel(addrPeer, 
																				tunnelDesc, 
																				&pRequest->m_TunnelRequest, 
																				&pRequest->m_Status);

					if(!gnetVerify(bSuccess))
					{
						gnetError("Cannot open tunnel to %s!", NetworkUtils::LogGamerHandle(pRequest->m_hGamer));
						break;
					}

					// update state
					pRequest->m_nState = sConnectionRequest::kCR_RequestedTunnel;
				}
				else
				{
					// failed
                    gnetError("Failed to get peer address for %s!", NetworkUtils::LogGamerHandle(pRequest->m_hGamer));
					
					// generate error for script
					GenerateScriptEvent(pRequest->m_hGamer, 0, 0);

					// delete request
					m_ConnectionRequests.Delete(i);
					delete pRequest;
				}
			}
			break;
		case sConnectionRequest::kCR_RequestedTunnel:
			if(pRequest->m_Status.Succeeded() && pRequest->m_TunnelRequest.Succeeded())
			{
				// log
				gnetDebug1("Opened tunnel to %s!", NetworkUtils::LogGamerHandle(pRequest->m_hGamer));

				// reset
				pRequest->m_nTransactionCount = 0;

				// send request
				bool bSuccess = SendSessionInfo(pRequest);
				if(bSuccess)
					pRequest->m_nState = sConnectionRequest::kCR_SentSessionInfo;
			}
			else if(!pRequest->m_Status.Pending())
			{
				// failed
				gnetError("Failed to open tunnel to %s!", NetworkUtils::LogGamerHandle(pRequest->m_hGamer));
				
				// generate error for script
				GenerateScriptEvent(pRequest->m_hGamer, 0, 0);

				// delete request
				m_ConnectionRequests.Delete(i);
				delete pRequest;
			}
			break;
		case sConnectionRequest::kCR_SentSessionInfo:
			// state will be moved on by response handler - if we end up here, we've timed out
			if(!m_ResponseHandler.Pending())
			{
				// failed
				gnetError("Update :: Timeout sending session info to %s!", NetworkUtils::LogGamerHandle(pRequest->m_hGamer));
				
				// generate error for script
				GenerateScriptEvent(pRequest->m_hGamer, 0, 0);
				
				// delete request
				m_ConnectionRequests.Delete(i);
				delete pRequest;
			}
			break;
		default:
			break; 
		}
	}
#endif
	
	// for joining players
	if(m_bIsInviteResponsePending)
	{
		// check for timeout 
		if((nCurrentTime - m_nTimeInviteReceived) > m_nJoinResponseTimeout)
			SendJoinResponse(kRequest_TimedOut, 0);
	}

	// need to call from Update
	if(m_bVoiceShutdownPending)
	{
		gnetDebug1("Update :: Processing voice shutdown flag");
		CNetwork::ShutdownVoice();
		m_bVoiceShutdownPending = false;
	}

	// update last time
	m_nLastTime = nCurrentTime;
}

bool CNetworkVoiceSession::Host()
{
	// not available on LAN
	if(CNetwork::LanSessionsOnly())
		return false;

	// make sure the player is online
	if(!NetworkInterface::IsLocalPlayerOnline())
	{
		gnetError("Host :: Not online!");
		return false;
	}

	// make sure the player is online
	if(!NetworkInterface::CheckOnlinePrivileges())
	{
		gnetError("Host :: No multiplayer privileges!");
		return false;
	}

	// not in the correct state
	if(!gnetVerify(m_nSessionState == kState_Idle))
	{
		gnetError("Host :: Invalid state to host!");
		return false;
	}

    // clear any parameters
    m_bHasAccepted = false;
    m_bLeavePending = false;
    m_bAcceptPending = false;

	// setup host config
	NetworkGameConfig ngConfig;
	player_schema::MatchingAttributes mmAttrs;
	ngConfig.Reset(mmAttrs, MultiplayerGameMode::GameMode_Voice, 0xFF, kMAX_VOICE_PLAYERS, kMAX_VOICE_PLAYERS);
	ngConfig.SetVisible(false);

	// we don't want invites / JvP on voice sessions
	unsigned nCreateFlags = RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;

#if RSG_XDK
	// we don't want this to be a party session
	nCreateFlags |= (RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED | RL_SESSION_CREATE_FLAG_DISPLAY_NAMES_NOT_REQUIRED);
#endif

	// host session
	m_SessionStatus.Reset();
	bool bSuccess = m_Session.Host(NetworkInterface::GetLocalGamerIndex(), RL_NETMODE_ONLINE, kMAX_VOICE_PLAYERS, kMAX_VOICE_PLAYERS, ngConfig.GetMatchingAttributes(), nCreateFlags, NULL, 0, &m_SessionStatus);

	// update state
	if(bSuccess)
	{
		gnetDebug1("Host :: Hosting");
		m_nSessionState = kState_Hosting;

		// initialise voice
		CNetwork::InitVoice();
	}

	return bSuccess;
}

bool CNetworkVoiceSession::Join(const rlSessionInfo &tSessionInfo)
{
	// make sure the player is online
	if(!NetworkInterface::IsLocalPlayerOnline())
	{
		gnetError("Join :: Not online!");
		return false;
	}

	// make sure the player is online
	if(!NetworkInterface::CheckOnlinePrivileges())
	{
		gnetError("Join :: No multiplayer privileges!");
		return false;
	}
	
	// verify the session is valid
	if(!gnetVerify(tSessionInfo.IsValid()))
		return false;

	// not in the correct state
	if(!gnetVerify(m_nSessionState == kState_Idle))
	{
		gnetError("Join :: Invalid state to join!");
		return false;
	}

    // clear any parameters
    m_bHasAccepted = false;
    m_bLeavePending = false;
    m_bAcceptPending = false;

	// we don't want invites / JvP on voice sessions
	unsigned nCreateFlags = RL_SESSION_CREATE_FLAG_PRESENCE_DISABLED;

#if RSG_XDK
	// we don't want this to be a party session
	nCreateFlags |= (RL_SESSION_CREATE_FLAG_PARTY_SESSION_DISABLED | RL_SESSION_CREATE_FLAG_DISPLAY_NAMES_NOT_REQUIRED);
#endif

	// join session
	m_SessionStatus.Reset();
	bool bSuccess = m_Session.Join(NetworkInterface::GetLocalGamerIndex(),
								   tSessionInfo,
								   RL_NETMODE_ONLINE,
								   RL_SLOT_PRIVATE,
								   nCreateFlags,
								   NULL,
								   0,
								   &NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(),
								   1,
								   &m_SessionStatus);
	// update state
	if(bSuccess)
	{
		gnetDebug1("Join :: Joining");
		m_nSessionState = kState_Joining;
	}

	return bSuccess;
}

bool CNetworkVoiceSession::Leave()
{
	// check that we're in the session
	if(!gnetVerify(m_nSessionState == kState_Started))
	{
		gnetError("Leave :: Invalid state to leave!");
		return false;
	}

	gnetDebug1("Leave");

	// call leave
	m_SessionStatus.Reset();
	bool bSuccess = m_Session.Leave(NetworkInterface::GetLocalGamerIndex(), &m_SessionStatus);

	// update state
	if(bSuccess)
	{
		m_nSessionState = kState_Leaving;

		// clear out states
		m_bIsInviteResponsePending = false;

		// punt active invite requests
		m_InviteRequests.Reset();

#if RSG_NP
		// punt connection requests
		int nRequests = m_ConnectionRequests.GetCount();
		for(int i = 0; i < nRequests; i++)
        {
            // cancel presence request
            if(m_ConnectionRequests[i]->m_nState == sConnectionRequest::kCR_RequestedPresence)
                rlPresence::CancelQuery(&m_ConnectionRequests[i]->m_Status);

            // cancel request if in flight
            CNetwork::GetConnectionManager().CancelTunnelRequest(&m_ConnectionRequests[i]->m_TunnelRequest);
			netAssert(!m_ConnectionRequests[i]->m_Status.Pending());

			delete m_ConnectionRequests[i];
        }

		// empty list
		m_ConnectionRequests.Reset();
#endif
	}
	else
	{
		gnetDebug1("Leave :: Leave failed. Bailing!");
		Bail(true);
	}

	return bSuccess;
}

bool CNetworkVoiceSession::IsInVoiceSession()
{
	// if we are in the session started state specifically
	return (m_nSessionState == kState_Started) && (m_bHasAccepted || m_Session.IsHost());
}

bool CNetworkVoiceSession::IsVoiceSessionActive()
{
    return (m_nSessionState > kState_Idle) || m_bIsInviteResponsePending;
}

bool CNetworkVoiceSession::IsBusy()
{
	// return true if our session status is pending
	return m_SessionStatus.Pending();
}

unsigned CNetworkVoiceSession::GetGamers(rlGamerInfo tGamers[], const unsigned nMaxGamers)
{
	// forward through session function
	return m_Session.GetGamers(tGamers, nMaxGamers);
}

bool CNetworkVoiceSession::GetEndpointId(const rlGamerInfo& sInfo, EndpointId* pEndpointId)
{
	*pEndpointId = NET_INVALID_ENDPOINT_ID;

	// check we have a valid member first
	if(!m_Session.IsMember(sInfo.GetGamerHandle()))
		return false;

    // the peer network address may no longer be valid if the connection has been reassigned
    // instead, get the connection ID and retrieve the current address
    int nConnectionID = m_Session.GetPeerCxn(sInfo.GetPeerInfo().GetPeerId());
    if(nConnectionID >= 0)
    {
        *pEndpointId = m_Session.GetCxnMgr()->GetEndpointId(nConnectionID);
        return true;
    }
    return false;
}

bool CNetworkVoiceSession::CanChat(const rlGamerHandle& hGamer)
{
    // look for this gamer in the voice gamer list
    int nGamers = m_VoiceGamers.GetCount();
    for(int i = 0; i < nGamers; i++)
    {
        if(m_VoiceGamers[i].m_hGamer == hGamer)
            return true;
    }

    // wait for accepted message
    return false;
}

void CNetworkVoiceSession::SetTimeout(unsigned nTimeout)
{
	m_nJoinRequestTimeout = nTimeout;
	m_nJoinResponseTimeout = nTimeout - 2000;
}

void CNetworkVoiceSession::InviteGamer(const rlGamerHandle& hGamer)
{
	// check that the gamer handle is valid
	if(!gnetVerify(hGamer.IsValid()))
	{
		gnetError("InviteGamer :: Invalid gamer handle!");
		return;
	}

	// check if we're leaving already
	if(m_nSessionState >= kState_Leaving)
	{
		gnetError("InviteGamer :: Leaving session!");
		return;
	}

#if RSG_NP
	if(!PARAM_voiceSessionPresence.Get())
	{
		// setup a presence helper
		sConnectionRequest* pRequest = rage_new sConnectionRequest();
		pRequest->m_hGamer = hGamer;
		pRequest->m_aPeerInfo.Reset(rlScAttributeId::PeerAddress.Name, "");
		pRequest->m_Status.Reset();
		
        // if we already have this guy, connect directly
        CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
        if(pPlayer)
        {
            // reset status 
            pRequest->m_Status.Reset();

			const bool isBilateral = false;
			const netTunnelDesc tunnelDesc(NET_TUNNELTYPE_ONLINE, NET_TUNNEL_REASON_DIRECT, m_Session.GetSessionInfo().GetToken().m_Value, isBilateral);

            // open direct connection to this player
            bool bSuccess = CNetwork::GetConnectionManager().OpenTunnel(pPlayer->GetPeerInfo().GetPeerAddress(), 
                                                                        tunnelDesc, 
                                                                        &pRequest->m_TunnelRequest, 
                                                                        &pRequest->m_Status);

            if(!gnetVerify(bSuccess))
            {
                gnetError("Cannot open tunnel to %s!", pPlayer->GetLogName());
                delete pRequest;
                return;
            }

            // update state
            pRequest->m_nState = sConnectionRequest::kCR_RequestedTunnel;
        }
        else
        {
            // call server to retrieve peer info for this player
            rlPresence::GetAttributesForGamer(NetworkInterface::GetActiveGamerInfo()->GetLocalIndex(), pRequest->m_hGamer, &pRequest->m_aPeerInfo, 1, &pRequest->m_Status);
            
            // update state
            pRequest->m_nState = sConnectionRequest::kCR_RequestedPresence;
        }

		// add presence helper to tracked list
		m_ConnectionRequests.PushAndGrow(pRequest);

		// log out request
		gnetDebug1("InviteGamer :: Peer address requested for %s!", NetworkUtils::LogGamerHandle(hGamer));
	}
	else
#endif
	{
		// track invite status
		sInviteRequest tRequest; 
		tRequest.m_hGamer = hGamer;
		tRequest.m_nInviteTime = sysTimer::GetSystemMsTime();
		tRequest.m_bResponseReceived = false;
		m_InviteRequests.Push(tRequest);

		// invite can be sent outside multiplayer so use presence
		CVoiceSessionInvite::Trigger(hGamer, m_Session.GetSessionInfo());

		// log out request
		gnetDebug1("InviteGamer :: Invite sent to %s!", NetworkUtils::LogGamerHandle(hGamer));
	}
}

void CNetworkVoiceSession::RespondToRequest(bool bAccept, int nResponseCode)
{
	// make sure we have a pending response
	if(!m_bIsInviteResponsePending)
		return;

	// send response
	SendJoinResponse(bAccept ? kRequest_Accept : kRequest_Reject, nResponseCode);

    // accept handling
    m_bHasAccepted = bAccept;
    if(!m_bHasAccepted)
    {
        if(m_nSessionState >= kState_Idle)
        {
            gnetDebug1("RespondToRequest :: Rejected. Queuing leave!");
            m_bLeavePending = true;
        }
    }
    else
    {
        gnetDebug1("RespondToRequest :: Accepted. Queuing accept!");
        m_bAcceptPending = true;
    }
}

void CNetworkVoiceSession::OnReceivedInvite(const rlSessionInfo& hInfo, const rlGamerHandle& hFrom, const char* szGamerName)
{
	gnetDebug1("OnReceivedInvite :: Received Invite from %s!", szGamerName);

	// stash inviter
	m_hInviter = hFrom;

	// check we have privilege
	if(!NetworkInterface::CheckOnlinePrivileges())
		SendJoinResponse(kRequest_Blocked, 0);
	// if we're already in a session or in process of invite, send busy response
    else if(m_nSessionState > kState_Idle || m_bIsInviteResponsePending || CLiveManager::IsInPlatformPartyChat())
		SendJoinResponse(kRequest_Busy, 0);
	else
	{
		// stash info on inviter
		snprintf(m_szInviterName, RL_MAX_NAME_BUF_SIZE, "%s", szGamerName);
        
        // join the session when we receive the request
        Join(hInfo);

#if !__FINAL
		if(PARAM_voiceSessionAutoAccept.Get())
		{
			// accept request
			SendJoinResponse(kRequest_Accept, SCRIPT_ACCEPT);
		}
		else
#endif
		{
			// add rag response buttons
#if __BANK
			Bank_RefreshResponseButtons(true);
#endif
			// keep request
			m_bIsInviteResponsePending = true;
			m_nTimeInviteReceived = sysTimer::GetSystemMsTime();
			m_PendingSessionInfo = hInfo;
		}
	}
}
		
void CNetworkVoiceSession::OnReceivedResponse(int nResponse, const int nResponseCode, const rlGamerHandle& hFrom)
{
	// log
	gnetDebug1("OnReceivedResponse :: Received request response from %s! Response: %d, Code: %d", NetworkUtils::LogGamerHandle(hFrom), nResponse, nResponseCode);

	// find the corresponding player
	int nRequests = m_InviteRequests.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(hFrom == m_InviteRequests[i].m_hGamer)
		{
			GenerateScriptEvent(m_InviteRequests[i].m_hGamer, nResponse, nResponseCode);
			
			// if we have accepted, flag so that we don't timeout
			if(nResponse == kRequest_Accept)
				m_InviteRequests[i].m_bResponseReceived = true; 
			// otherwise, drop the invite request
			else
				m_InviteRequests.Delete(i);

			// bail out
			break;
		}
	}
}

#if RSG_NP
void CNetworkVoiceSession::OnRequest(netTransactor* pTransactor, netRequestHandler* UNUSED_PARAM(pRequestHandler), const netRequest* pRequest)
{
	gnetDebug1("OnRequest :: Received Request!");
	gnetAssert(&m_Transactor == pTransactor);

	unsigned nMessageID;
	netMessage::GetId(&nMessageID, pRequest->m_Data, pRequest->m_SizeofData);

	if(snMsgSessionInfo::MSG_ID() == nMessageID)
	{
		snMsgSessionInfo sMessage;
		sMessage.Import(pRequest->m_Data, pRequest->m_SizeofData);

		// log
		gnetDebug1("OnRequest :: Received session info!");

		// acknowledge session info received
		snMsgSessionInfoResponse sResponse;
		pTransactor->SendResponse(pRequest->m_TxInfo, sResponse);

		// stash inviter
		m_hInviter = sMessage.m_hGamer;

		// check we have privilege
		if(!NetworkInterface::CheckOnlinePrivileges())
			SendJoinResponse(kRequest_Blocked, 0); // TODO: why is the address not passed in this case?
		// if we're already in a session or in process of invite, send busy response
		else if(m_nSessionState > kState_Idle || m_bIsInviteResponsePending || CLiveManager::IsInPlatformPartyChat())
			SendJoinResponse(kRequest_Busy, 0, &pRequest->m_TxInfo.m_Addr);
		else
		{
			// we need this to send the join response later
			m_HostNetAddr = pRequest->m_TxInfo.m_Addr;
			
			// stash info on inviter
			snprintf(m_szInviterName, RL_MAX_NAME_BUF_SIZE, "%s", sMessage.m_szGamerTag);

            // we join the session when we get the request
            Join(sMessage.m_SessionInfo);

#if !__FINAL
			if(PARAM_voiceSessionAutoAccept.Get())
			{
				SendJoinResponse(kRequest_Accept, SCRIPT_ACCEPT);
			}
			else
#endif
			{
				// add rag response buttons
#if __BANK
				Bank_RefreshResponseButtons(true);
#endif
				// keep request
				m_bIsInviteResponsePending = true;
				m_nTimeInviteReceived = sysTimer::GetSystemMsTime();
				m_PendingSessionInfo = sMessage.m_SessionInfo;
			}
		}
	}
	else if(snMsgSessionJoinRequest::MSG_ID() == nMessageID)
	{
		snMsgSessionJoinRequest sMessage;
		sMessage.Import(pRequest->m_Data, pRequest->m_SizeofData);

		// log
		gnetDebug1("OnRequest :: Received request response! Response: %d. Code: %d", sMessage.m_nResponse, sMessage.m_nResponseCode);

		// acknowledge join request received
		snMsgSessionJoinRequestResponse sResponse;
		pTransactor->SendResponse(pRequest->m_TxInfo, sResponse);

		// find the corresponding player
		int nRequests = m_ConnectionRequests.GetCount();
		for(int i = 0; i < nRequests; i++)
		{
			sConnectionRequest* pCR = m_ConnectionRequests[i];
			if(pCR->m_TunnelRequest.GetNetAddress() == pRequest->m_TxInfo.m_Addr)
			{
				// roll up state
				if(pCR->m_nState < sConnectionRequest::kCR_HasResponse)
					pCR->m_nState = sConnectionRequest::kCR_HasResponse;

                // let script know
				GenerateScriptEvent(pCR->m_hGamer, sMessage.m_nResponse, sMessage.m_nResponseCode);

				// punt the connection if we're not moving forward
				if(sMessage.m_nResponse != kRequest_Accept)
				{
					m_ConnectionRequests.Delete(i);
					delete pCR;
				}
				break;
			}
		}
	}
}

void CNetworkVoiceSession::OnResponse(netTransactor* UNUSED_PARAM(pTransactor), netResponseHandler* UNUSED_PARAM(pResponseHandler), const netResponse* pResponse)
{
	switch(pResponse->m_Code)
	{
	case netResponse::REQUEST_RECEIVED:		gnetDebug1("OnResponse :: Response - REQUEST_RECEIVED!"); break;
	case netResponse::REQUEST_ANSWERED:		gnetDebug1("OnResponse :: Response - REQUEST_ANSWERED!"); break;
	case netResponse::REQUEST_TIMED_OUT:	gnetDebug1("OnResponse :: Response - REQUEST_TIMED_OUT!"); break;
	case netResponse::REQUEST_CANCELED:		gnetDebug1("OnResponse :: Response - REQUEST_CANCELED!"); break;
	case netResponse::REQUEST_FAILED:		gnetDebug1("OnResponse :: Response - REQUEST_FAILED!"); break;
	default:
		break;
	}

	// bail out if this response failed
	if(!(pResponse->Complete() && pResponse->Answered()))
		return;

	// grab message ID
	unsigned nMessageID;
	netMessage::GetId(&nMessageID, pResponse->m_Data, pResponse->m_SizeofData);

	// check ID
	if(snMsgSessionInfoResponse::MSG_ID() == nMessageID)
	{
		// log
		gnetDebug1("OnResponse :: Received session info ack!");

		// find the corresponding player
		int nRequests = m_ConnectionRequests.GetCount();
		for(int i = 0; i < nRequests; i++)
		{
			sConnectionRequest* pCR = m_ConnectionRequests[i];
			if(pCR->m_TunnelRequest.GetNetAddress() == pResponse->m_TxInfo.m_Addr)
			{
				// roll up state
				if(pCR->m_nState < sConnectionRequest::kCR_HasSessionInfo)
					pCR->m_nState = sConnectionRequest::kCR_HasSessionInfo;

				break;
			}
		}
	}
	else if(snMsgSessionJoinRequestResponse::MSG_ID() == nMessageID)
	{
		// log
		gnetDebug1("OnResponse :: Received request response ack!");
	}
}

#endif

void CNetworkVoiceSession::OnSessionEvent(snSession* OUTPUT_ONLY(pSession), const snEvent* pEvent)
{
	// log event
	gnetDebug1("SessionEvent :: Game:0x%016" I64FMT "x - [%s]", pSession->Exists() ? pSession->GetSessionInfo().GetToken().m_Value : 0, pEvent->GetAutoIdNameFromId(pEvent->GetId()));

	switch(pEvent->GetId())
	{
	case SNET_EVENT_ADDED_GAMER:
		{
			rlGamerInfo tInfo = pEvent->m_AddedGamer->m_GamerInfo;
			const EndpointId endpointId = pEvent->m_AddedGamer->m_EndpointId;

			// get added gamer event
			gnetDebug1("OnSessionEvent :: %s gamer %s has joined!", tInfo.IsRemote() ? "Remote" : "Local", tInfo.GetName());

            // should we add this player...
            bool bAddGamer = false;

            // the local gamer and existing gamers get added automatically
            if(tInfo.IsLocal())
            {
                gnetDebug1("OnSessionEvent :: Adding local voice gamer %s!", tInfo.GetName());
                bAddGamer = true;
            }
            else if(!m_Session.IsEstablished())
            {
                gnetDebug1("OnSessionEvent :: Adding existing remote gamer %s!", tInfo.GetName());
                bAddGamer = true;
            }

            if(bAddGamer)
            {
                sVoiceGamer tGamer;
                tGamer.m_hGamer = tInfo.GetGamerHandle();
                tGamer.m_bHasAccepted = true;
                m_VoiceGamers.Push(tGamer);
            }

            // add player to voice
			if(!NetworkInterface::GetVoice().IsInitialized())
				CNetwork::InitVoice();

            NetworkInterface::GetVoice().PlayerHasJoined(tInfo, endpointId);

#if RSG_NP
			if(tInfo.IsRemote())
			{
				// find the corresponding player
				int nRequests = m_ConnectionRequests.GetCount();
				for(int i = 0; i < nRequests; i++)
				{
					sConnectionRequest* pCR = m_ConnectionRequests[i];
					if(pCR->m_TunnelRequest.GetEndpointId() == endpointId)
					{
						// delete request
						delete pCR;
						m_ConnectionRequests.Delete(i);
						break;
					}
				}
			}
#endif
		}
		break; 

	case SNET_EVENT_REMOVED_GAMER:
		{
			// get removed gamer event
			const snEventRemovedGamer* pEventRG = pEvent->m_RemovedGamer;

			// log 
			gnetDebug1("OnSessionEvent :: %s gamer %s has left!", pEventRG->m_GamerInfo.IsRemote() ? "Remote" : "Local", pEventRG->m_GamerInfo.GetName());

			// generate event if this is not the local player
			if(!pEventRG->m_GamerInfo.IsLocal())
				GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceConnectionTerminated(pEventRG->m_GamerInfo.GetGamerHandle(), pEventRG->m_GamerInfo.GetName()));

            // remove from the voice gamer list
            int nGamers = m_VoiceGamers.GetCount();
            for(int i = 0; i < nGamers; i++)
            {
                if(m_VoiceGamers[i].m_hGamer == pEventRG->m_GamerInfo.GetGamerHandle())
                {
                    gnetDebug1("OnSessionEvent :: Removing voice gamer entry!");
                    m_VoiceGamers.Delete(i);
                    break;
                }
            }

            // remove player from voice
            NetworkInterface::GetVoice().PlayerHasLeft(pEvent->m_AddedGamer->m_GamerInfo);

			u64 nHostPeedID;
			if(!pEventRG->m_GamerInfo.IsLocal() && m_Session.GetHostPeerId(&nHostPeedID))
            {
                if(pEventRG->m_GamerInfo.GetPeerInfo().GetPeerId() == nHostPeedID)
                {
                    gnetDebug1("OnSessionEvent :: Host left. Flagging leave!");
                    m_bLeavePending = true;
                }
            }
		}
		break;

	default:
		break;
	}
}

void CNetworkVoiceSession::OnConnectionEvent(netConnectionManager* UNUSED_PARAM(pCxnMgr), const netEvent* pEvent)
{
    switch(pEvent->GetId())
    {
        // a frame of data was received.
    case NET_EVENT_FRAME_RECEIVED:
        {
            const netEventFrameReceived* pFrame = pEvent->m_FrameReceived;

            unsigned nMsgID;
            if(!netMessage::GetId(&nMsgID, pFrame->m_Payload, pFrame->m_SizeofPayload))
                break;

            if(nMsgID == snMsgSessionAcceptChat::MSG_ID())
            {
                // verify message contents
                snMsgSessionAcceptChat sMsg;
                if(!gnetVerify(sMsg.Import(pFrame->m_Payload, pFrame->m_SizeofPayload)))
                    break;

                // accepted
                GenerateScriptEvent(sMsg.m_hGamer, kRequest_Accept, SCRIPT_ACCEPT);

                // logging
                gnetDebug1("OnConnectionEvent :: Adding remote voice gamer %s!", NetworkUtils::LogGamerHandle(sMsg.m_hGamer));

                // add to our list of voice gamers
                sVoiceGamer tGamer;
                tGamer.m_hGamer = sMsg.m_hGamer;
                tGamer.m_bHasAccepted = true;
                m_VoiceGamers.Push(tGamer);
            }
        }
        break;
    }
}

void CNetworkVoiceSession::OnGetGamerData(snSession* UNUSED_PARAM(pSession), const rlGamerInfo& UNUSED_PARAM(gamerInfo), snGetGamerData* pGamerData)
{
	// no data
	pGamerData->m_SizeofData = 0;
}

bool CNetworkVoiceSession::OnHandleJoinRequest(snSession* UNUSED_PARAM(pSession), const rlGamerInfo& gamerInfo, snJoinRequest* ORBIS_ONLY(pRequest))
{
	// log
	gnetDebug1("OnHandleJoinRequest :: Received join request from %s!", gamerInfo.GetName());

	// find the corresponding player
	int nInvites = m_InviteRequests.GetCount();
	for(int i = 0; i < nInvites; i++)
	{
		if(gamerInfo.GetGamerHandle() == m_InviteRequests[i].m_hGamer)
		{
			// we found the request, accept and drop the tracked structure
			m_InviteRequests.Delete(i);
			return true; 
		}
	}

#if RSG_NP
	// find the corresponding player
	int nRequests = m_ConnectionRequests.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		sConnectionRequest* pCR = m_ConnectionRequests[i];
		if(pCR->m_TunnelRequest.GetNetAddress() == pRequest->m_Addr)
		{
			delete pCR;
			m_ConnectionRequests.Delete(i);
			return true; 
		}
	}
#endif

	// we haven't got a request out for this player to join
	return false;
}

bool CNetworkVoiceSession::ShouldMigrate()
{
	// handle the case where the host disconnects during session join
	if(m_Session.GetGamerCount() == 1 && m_Session.IsMember(NetworkInterface::GetLocalGamerHandle()) && !m_Session.IsHost())
	{
		// we've been asked to migrate - handle the case where 
		gnetDebug1("ShouldMigrate :: Host has left during join! Flagging leave!");
		m_bLeavePending = true;
	}

	// do not migrate these sessions
	return false;
}

void CNetworkVoiceSession::GenerateScriptEvent(const rlGamerHandle& hGamer, unsigned nResponse, int nResponseCode)
{
	// mirrored enum to avoid including headers
	CEventNetworkVoiceConnectionResponse::eResponse nScriptResponse;
	switch(nResponse)
	{
	case kRequest_Accept: nScriptResponse = CEventNetworkVoiceConnectionResponse::RESPONSE_ACCEPTED; break;
	case kRequest_Reject: nScriptResponse = CEventNetworkVoiceConnectionResponse::RESPONSE_REJECTED; break;
	case kRequest_Busy: nScriptResponse = CEventNetworkVoiceConnectionResponse::RESPONSE_BUSY; break;
	case kRequest_TimedOut: nScriptResponse = CEventNetworkVoiceConnectionResponse::RESPONSE_TIMED_OUT; break;
	case kRequest_Blocked: nScriptResponse = CEventNetworkVoiceConnectionResponse::RESPONSE_BLOCKED; break;
	default: nScriptResponse = CEventNetworkVoiceConnectionResponse::RESPONSE_ERROR; break;
	}
	GetEventScriptNetworkGroup()->Add(CEventNetworkVoiceConnectionResponse(hGamer, nScriptResponse, nResponseCode));
}

#if RSG_NP

bool CNetworkVoiceSession::SendSessionInfo(sConnectionRequest* pRequest)
{
	// grab gamer info
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(!gnetVerifyf(pInfo, "SendSessionInfo :: Local gamer not signed in!"))
		return false;

	gnetDebug1("SendSessionInfo :: Sending session info!");

	// add our session info
	snMsgSessionInfo sMsgInfo;
	sMsgInfo.Reset(m_Session.GetSessionInfo(), pInfo->GetGamerHandle(), pInfo->GetName());

	// reset 
	pRequest->m_nTransactionID = NET_INVALID_TRANSACTION_ID;

	// send request
	// TODO: this request is sent unreliably without resends (no connection has been established),
	// so it's susceptible to packet loss.
	bool bSuccess = m_Transactor.SendRequestOutOfBand(pRequest->m_TunnelRequest.GetEndpointId(),
													 m_nChannelID,
													 &pRequest->m_nTransactionID,
													 sMsgInfo,
													 m_nJoinRequestTimeout,
													 &m_ResponseHandler);

	if(!gnetVerify(bSuccess))
	{
		gnetError("SendSessionInfo :: Failed to send session info!");
		return false;
	}

	return true;
}
#endif

void CNetworkVoiceSession::SendJoinResponse(unsigned nResponse, int nResponseCode, const netAddress* ORBIS_ONLY(pNetAddress))
{
	gnetDebug1("SendJoinResponse :: Sending join response (%d) with code (%d)!", nResponse, nResponseCode);

	m_bIsInviteResponsePending = false;

	// remove rag response buttons
#if __BANK
	Bank_RefreshResponseButtons(false);
#endif

#if RSG_NP
	if(!PARAM_voiceSessionPresence.Get())
	{
		// setup response
		snMsgSessionJoinRequest sResponse;
		sResponse.Reset(nResponse, nResponseCode);

		// we don't need to track this
		unsigned nTransactionID = NET_INVALID_TRANSACTION_ID;

		// send request
        bool bSuccess = m_Transactor.SendRequestOutOfBand(pNetAddress ? *pNetAddress : m_HostNetAddr,
														 m_nChannelID,
														 &nTransactionID,
														 sResponse,
														 kTRANSACTION_TIMEOUT_DEFAULT,
														 &m_ResponseHandler);

		if(!gnetVerify(bSuccess))
		{
			gnetError("SendJoinResponse :: Failed to send join response!");
			return;
		}
	}
	else
#endif
	{
		CVoiceSessionResponse::Trigger(m_hInviter, nResponse, nResponseCode);
	}
}

#if __BANK

static rage::bkBank*	g_pvNetworkBank = NULL;
static rage::bkGroup*	g_pvVoiceGroup = NULL;
static rage::bkCombo*	g_pvFriendsCombo = NULL;
static rage::bkButton*	g_pvRespondAcceptButton = NULL;
static rage::bkButton*	g_pvRespondRejectButton = NULL;
static rage::bkButton*	g_pvRespondBusyButton = NULL;
static rage::bkButton*	g_pvRespondTimeoutButton = NULL;
static const char*		g_BankFriends[rlFriendsPage::MAX_FRIEND_PAGE_SIZE];
static int				g_BankFriendsComboIndex = -1;
static u32				g_BankNumFriends = 0;

void CNetworkVoiceSession::InitWidgets()
{
	g_pvNetworkBank = BANKMGR.FindBank("Network");
	if(gnetVerify(g_pvNetworkBank))
	{
		bkGroup* pLiveGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live"));
		if(gnetVerify(pLiveGroup))
		{
			// update the friend list
			g_BankNumFriends = Min((int)CLiveManager::GetFriendsPage()->m_NumFriends, rlFriendsPage::MAX_FRIEND_PAGE_SIZE);
			for(u32 i = 0; i < g_BankNumFriends; i++)
			{
				g_BankFriends[i] = CLiveManager::GetFriendsPage()->m_Friends[i].GetName();
			}

			// avoid empty combo box
			int nNumFriends = g_BankNumFriends;
			if(nNumFriends == 0)
			{
				g_BankFriends[0] = "--No Friends--";
				nNumFriends  = 1;
			}

			g_pvVoiceGroup = g_pvNetworkBank->PushGroup("Debug Voice Session");
				g_pvNetworkBank->AddButton("Host Voice Session", datCallback(MFA(CNetworkVoiceSession::Bank_HostVoiceSession), (datBase*)this));
				g_pvNetworkBank->AddButton("Leave Session", datCallback(MFA(CNetworkVoiceSession::Bank_LeaveVoiceSession), (datBase*)this));
				g_pvNetworkBank->AddButton("Connect to Friend", datCallback(MFA(CNetworkVoiceSession::Bank_ConnectToFriend), (datBase*)this));
				g_pvNetworkBank->AddButton("Refresh Friends", datCallback(MFA(CNetworkVoiceSession::Bank_RefreshFriends), (datBase*)this));
				g_pvFriendsCombo = g_pvNetworkBank->AddCombo("Friends", &g_BankFriendsComboIndex, g_BankNumFriends, (const char**)g_BankFriends);
			g_pvNetworkBank->PopGroup();
		}
	}
}

void CNetworkVoiceSession::Bank_RefreshResponseButtons(bool bCanRespond)
{
	if(!g_pvVoiceGroup)
		return; 

	if(bCanRespond)
	{
		// check a button so that we don't double create
		if(!g_pvRespondAcceptButton)
		{
			g_pvRespondAcceptButton = g_pvVoiceGroup->AddButton("Respond Accept", datCallback(MFA(CNetworkVoiceSession::Bank_RespondAccept), (datBase*)this));
			g_pvRespondRejectButton = g_pvVoiceGroup->AddButton("Respond Reject", datCallback(MFA(CNetworkVoiceSession::Bank_RespondReject), (datBase*)this));
			g_pvRespondBusyButton = g_pvVoiceGroup->AddButton("Respond Busy", datCallback(MFA(CNetworkVoiceSession::Bank_RespondBusy), (datBase*)this));
			g_pvRespondTimeoutButton = g_pvVoiceGroup->AddButton("Respond Timed Out", datCallback(MFA(CNetworkVoiceSession::Bank_RespondTimedOut), (datBase*)this));
		}
	}
	else
	{
		if(g_pvRespondAcceptButton)
		{
			g_pvRespondAcceptButton->Destroy();
			g_pvRespondAcceptButton = NULL;
		}
		if(g_pvRespondRejectButton)
		{
			g_pvRespondRejectButton->Destroy();
			g_pvRespondRejectButton = NULL;
		}
		if(g_pvRespondBusyButton)
		{
			g_pvRespondBusyButton->Destroy();
			g_pvRespondBusyButton = NULL;
		}
		if(g_pvRespondTimeoutButton)
		{
			g_pvRespondTimeoutButton->Destroy();
			g_pvRespondTimeoutButton = NULL;
		}
	}
}

void CNetworkVoiceSession::Bank_HostVoiceSession()
{
	Host();
}

void CNetworkVoiceSession::Bank_LeaveVoiceSession()
{
	Leave(); 
}

void CNetworkVoiceSession::Bank_ConnectToFriend()
{
	int nFriendIndex = CLiveManager::GetFriendsPage()->GetFriendIndex(g_BankFriends[g_BankFriendsComboIndex]);
	if(nFriendIndex < 0)
		return;

	rlFriend& pFriend = CLiveManager::GetFriendsPage()->m_Friends[nFriendIndex];
	if(!pFriend.IsOnline())
		return;

	rlGamerHandle gamerHandle;
	pFriend.GetGamerHandle(&gamerHandle);
	InviteGamer(gamerHandle);
}

void CNetworkVoiceSession::Bank_RefreshFriends()
{
	if(!g_pvFriendsCombo)
		return;

	// reset friend count 
	g_BankNumFriends = 0;

	// update the friend list
	g_BankNumFriends = CLiveManager::GetFriendsPage()->m_NumFriends;
	for(u32 i = 0; i < g_BankNumFriends; i++)
	{
		g_BankFriends[i] = CLiveManager::GetFriendsPage()->m_Friends[i].GetName();
	}

	// avoid empty combo box
	int nFriends = g_BankNumFriends;
	if(nFriends == 0)
	{
		g_BankFriends[0] = "--No Friends--";
		nFriends  = 1;
	}

	g_BankFriendsComboIndex = Clamp(g_BankFriendsComboIndex, 0, static_cast<int>(nFriends) - 1);
	g_pvFriendsCombo->UpdateCombo("Friends", &g_BankFriendsComboIndex, nFriends, (const char**)g_BankFriends);
}

void CNetworkVoiceSession::Bank_RespondAccept()
{
	if(m_bIsInviteResponsePending)
	{
		SendJoinResponse(kRequest_Accept, SCRIPT_ACCEPT);
		
		// join the session
		Join(m_PendingSessionInfo);
	}
}

void CNetworkVoiceSession::Bank_RespondReject()
{
	if(m_bIsInviteResponsePending)
		SendJoinResponse(kRequest_Reject, 0);
}

void CNetworkVoiceSession::Bank_RespondBusy()
{
	if(m_bIsInviteResponsePending)
		SendJoinResponse(kRequest_Busy, 0);
}

void CNetworkVoiceSession::Bank_RespondTimedOut()
{
	if(m_bIsInviteResponsePending)
		SendJoinResponse(kRequest_TimedOut, 0);
}
#endif
