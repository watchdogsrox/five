//
// NetworkSession.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORK_VOICE_SESSION_H
#define NETWORK_VOICE_SESSION_H

// rage includes
#include "atl/array.h"
#include "rline/rlgamerinfo.h"
#include "rline/scpresence/rlscpresence.h"
#include "snet/session.h"
#include "net/transaction.h"
#include "net/tunneler.h"

namespace rage
{
	class sysMemAllocator;
}

class CNetworkVoiceSession
{
public:

	static const unsigned kMAX_VOICE_PLAYERS = 8;

    CNetworkVoiceSession();

    bool Init(sysMemAllocator* pAlloc, netConnectionManager* pCxnMan, const u32 nChannelID);
    void Shutdown(bool isSudden);

	void Update();

	bool Host();
	bool Join(const rlSessionInfo &tSessionInfo);
	bool Leave();

    void Bail(bool bEndEvent);

	void InviteGamer(const rlGamerHandle& hGamer);
	void RespondToRequest(bool bAccept, int nResponseCode);

	void OnReceivedInvite(const rlSessionInfo& hInfo, const rlGamerHandle& hFrom, const char* szGamerName);
	void OnReceivedResponse(int nResponse, const int nResponseCode, const rlGamerHandle& hFrom);

	bool IsInVoiceSession();
    bool IsVoiceSessionActive();
    bool IsBusy();

	unsigned GetGamers(rlGamerInfo tGamers[], const unsigned nMaxGamers); 
	bool GetEndpointId(const rlGamerInfo& sInfo, EndpointId* pEndpointId);
    bool CanChat(const rlGamerHandle& hGamer);
    bool IsInSession(const rlGamerHandle& hGamer) { return m_Session.IsMember(hGamer); }

	void SetTimeout(unsigned nTimeout);

    // message sending
    template<typename T>
    void SendMessage(const rlGamerHandle& hGamer, const T& tMsg)
    {
        // grab gamer info
        rlGamerInfo hInfo;
        if(m_Session.GetGamerInfo(hGamer, &hInfo))
            m_Session.SendMsg(hInfo.GetPeerInfo().GetPeerId(), tMsg, NET_SEND_RELIABLE);
    }

private:

	enum 
	{
		kState_Idle,
		kState_Hosting,
		kState_Joining,
		kState_Started,
		kState_Leaving,
		kState_Destroying
	};

	int m_nSessionState; 

	static const unsigned MAX_ACTIVE_INVITES = 15;
	struct sInviteRequest
	{
		unsigned m_nInviteTime;
		rlGamerHandle m_hGamer;
		bool m_bResponseReceived;
	};
	atFixedArray<sInviteRequest, MAX_ACTIVE_INVITES> m_InviteRequests;

    struct sVoiceGamer
    {
        rlGamerHandle m_hGamer;
        bool m_bHasAccepted;
    };
    atFixedArray<sVoiceGamer, kMAX_VOICE_PLAYERS> m_VoiceGamers;

#if RSG_NP
	struct sConnectionRequest
	{
		enum 
		{
			kCR_Idle,
			kCR_RequestedPresence,
			kCR_RequestedTunnel,
			kCR_SentSessionInfo,
			kCR_HasSessionInfo,
			kCR_HasResponse,
		};

		sConnectionRequest() : m_nState(kCR_Idle) { m_Status.Reset(); }

		int m_nState;
		rlGamerHandle m_hGamer;
		rlScPresenceAttribute m_aPeerInfo; 
		netTunnelRequest m_TunnelRequest;
		unsigned m_nTransactionCount;
		unsigned m_nTransactionID;
		netStatus m_Status;
	};
	atArray<sConnectionRequest*> m_ConnectionRequests;
	netAddress m_HostNetAddr;
#endif

	// pre-session player connection
	void GenerateScriptEvent(const rlGamerHandle& hGamer, unsigned nResponse, int nResponseCode);

	// pre-session player connection
#if RSG_NP
	bool SendSessionInfo(sConnectionRequest* pRequest); 
#endif
	void SendJoinResponse(unsigned nResponse, int nResponseCode, const netAddress* pNetAddress = NULL);

	// delegate callbacks
	void OnSessionEvent(snSession* pSession, const snEvent* pEvent);
    void OnConnectionEvent(netConnectionManager* pCxnMgr, const netEvent* pEvent);

	// session callbacks
	bool OnHandleJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest);
    void OnGetGamerData(snSession* pSession, const rlGamerInfo& gamerInfo, snGetGamerData* pGamerData);
	bool ShouldMigrate();

#if RSG_NP
	// transaction handlers
	void OnRequest(netTransactor* pTransactor, netRequestHandler* pRequestHandler, const netRequest* pRequest);
	void OnResponse(netTransactor* pTransactor, netResponseHandler* pResponseHandler, const netResponse* pResponse);
	void OnTransactionError(const rlGamerHandle& gamerInfo, const char* szError);
#endif

	// session flow
	snSession m_Session;
	netStatus m_SessionStatus; 
	
	// delegates for session/connection manager event handlers
	snSession::Delegate m_SessionDlgt;
    netConnectionManager::Delegate m_CxnMgrDlgt;
    netConnectionManager* m_pCxnMan;

	// general setup
	bool m_bIsInitialised;
	u32 m_nLastTime; 
	u32 m_nChannelID; 
	bool m_bVoiceShutdownPending;
    bool m_bLeavePending;

    // accept handling
    bool m_bHasAccepted;
    bool m_bAcceptPending; 

	// timeouts
	unsigned m_nJoinRequestTimeout;
	unsigned m_nJoinResponseTimeout; 

	// for the joiner
	bool m_bIsInviteResponsePending;
	unsigned m_nTimeInviteReceived;
	rlSessionInfo m_PendingSessionInfo;
	rlGamerHandle m_hInviter;
	char m_szInviterName[RL_MAX_NAME_BUF_SIZE];

#if RSG_NP
	// out of session transactor
	netTransactor m_Transactor; 
	netRequestHandler m_RequestHandler;
	netResponseHandler m_ResponseHandler;
#endif
	
#if __BANK

public:

	void InitWidgets();
	void Bank_RefreshResponseButtons(bool bCanRespond);

	void Bank_HostVoiceSession();
	void Bank_LeaveVoiceSession(); 
	void Bank_ConnectToFriend(); 
	void Bank_RefreshFriends(); 
	void Bank_RespondAccept(); 
	void Bank_RespondReject(); 
	void Bank_RespondBusy(); 
	void Bank_RespondTimedOut(); 

#endif // __BANK
};

#endif  // NETWORK_VOICE_SESSION_H
