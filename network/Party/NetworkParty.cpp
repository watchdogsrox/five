//
// NetworkParty.cpp
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
#include "NetworkParty.h"

// rage includes
#include "diag/seh.h"
#include "fwnet/netchannel.h"
#include "net/netdiag.h"
#include "rline/rlgamerinfo.h"

// game includes
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/NetworkInterface.h"
#include "Network/NetworkTypes.h"
#include "text/TextFile.h"

RAGE_DEFINE_SUBCHANNEL(net, party, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_party

CNetworkParty::CNetworkParty()
	: m_PartyState(STATE_PARTY_NONE)
	, m_IsInitialised(false)
{
	m_PartyDlgt.Bind(this, &CNetworkParty::OnPartyEvent);

#if RSG_BANK
	BankInit();
#endif
}

bool CNetworkParty::Init()
{
	gnetAssertf(STATE_PARTY_NONE == m_PartyState, "Init :: Initialising CNetworkParty when it is in an invalid state!");

	m_Owner.GetGamerData.Bind(this, &CNetworkParty::HandleGetGamerData);
	m_Owner.HandleJoinRequest.Bind(this, &CNetworkParty::HandleJoinRequest);
	
	m_Party.Init(CLiveManager::GetRlineAllocator(), m_Owner, &CNetwork::GetConnectionManager(), NETWORK_PARTY_CHANNEL_ID);
	m_Party.AddDelegate(&m_PartyDlgt);

	SetPartyState(STATE_PARTY_NONE);

	m_IsInitialised = true;

	return true;
}

void CNetworkParty::Shutdown()
{
	if(m_PartyStatus.Pending())
	{
		gnetDebug1("Shutdown :: Party status is pending. Cancelling");
		rlGetTaskManager()->CancelTask(&m_PartyStatus);
	}

	m_PartyStatus.Reset();
	SetPartyState(STATE_PARTY_NONE);

	m_Party.RemoveDelegate(&m_PartyDlgt);
	m_IsInitialised = false;
}

void CNetworkParty::Update()
{
	m_Party.Update();

	switch(m_PartyState)
	{
	case STATE_PARTY_NONE:
		break; 

	case STATE_PARTY_HOSTING:
	case STATE_PARTY_JOINING:
		if(m_PartyStatus.Succeeded())
		{
			SetPartyState(STATE_PARTY_MEMBER);
		}
		else if(!m_PartyStatus.Pending())
		{
			gnetDebug3("Update :: STATE_PARTY_JOINING - Shutting down voice");
			CNetwork::ShutdownVoice();
			SetPartyState(STATE_PARTY_NONE);
		}
		break;

	case STATE_PARTY_MEMBER:
		break; 

	case STATE_PARTY_LEAVING:
		if(!m_PartyStatus.Pending())
		{
			gnetDebug3("Update :: STATE_PARTY_LEAVING - Shutting down voice");
			CNetwork::ShutdownVoice();
			SetPartyState(STATE_PARTY_NONE);
		}
		break;

	default:
		gnetAssertf(0, "Update :: Invalid party session state!");
		break;
	}

#if RSG_BANK
	// update party widget
	Bank_RefreshPartyMembers();
#endif
}

bool CNetworkParty::HostParty()
{
	gnetDebug1("HostParty :: Hosting party");

	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("HostParty - Party not initialised!");
		return false;
	}

	if(!gnetVerify(!IsInParty()))
	{
		gnetError("HostParty - Already in a party!");
		return false;
	}

	if(!gnetVerify(!IsPartyStatusPending()))
	{
		gnetError("HostParty - Party Status Pending!");
		return false;
	}

	if(CNetwork::LanSessionsOnly())
	{
		gnetWarning("HostParty - LAN sessions only.");
		return false; 
	}

	// check we have access via profile and privilege
	if(!CanParty())
		return false;

	// reset party state
	m_PartyState = STATE_PARTY_NONE;

	// we use voice for parties
	CNetwork::InitVoice();

	// create party config
	NetworkGameConfig ngConfig; 
	player_schema::MatchingAttributes mmAttrs;
	ngConfig.Reset(mmAttrs, 0, 0, MAX_NUM_PARTY_MEMBERS, MAX_NUM_PARTY_MEMBERS);
	ngConfig.SetGameMode(MultiplayerGameMode::GameMode_Party);
	ngConfig.SetSessionPurpose(SessionPurpose::SP_Party);
	ngConfig.SetVisible(false);

#if !__NO_OUTPUT
	ngConfig.DebugPrint(true);
#endif

	if(!gnetVerify(ngConfig.IsValid()))
	{
		gnetError("HostParty - Invalid Match config");
		return false;
	}

	bool bSuccess = m_Party.Host(NetworkInterface::GetLocalGamerIndex(), 
								 ngConfig.GetMatchingAttributes(),  
								 MAX_NUM_PARTY_MEMBERS, 
								 &m_PartyStatus);
	if(!gnetVerify(bSuccess))
	{
		gnetError("HostParty - Error Hosting Party Session!");
		return false;
	}

	// all checks passed - update state
	SetPartyState(STATE_PARTY_HOSTING);
	return true;
}

bool CNetworkParty::JoinParty(const rlSessionInfo& tSessionInfo)
{
	gnetDebug1("JoinParty :: Joining party");

	if(!gnetVerify(m_IsInitialised))
	{
		gnetError("JoinParty - Not Initialised!");
		return false;
	}

	if(!gnetVerify(tSessionInfo.IsValid()))
	{
		gnetError("JoinParty - Invalid Session Info!");
		return false;
	}

	if(!gnetVerify(!IsInParty()))
	{
		gnetError("JoinParty - Already in a party!");
		return false;
	}

	if(!gnetVerify(!IsPartyStatusPending()))
	{
		gnetError("JoinParty - Party Status Pending!");
		return false;
	}

	if(CNetwork::LanSessionsOnly())
	{
		gnetWarning("JoinParty - LAN sessions only.");
		return false; 
	}

	// check we have access via profile and privilege
	if(!CanParty())
		return false;

	// reset party state
	m_PartyState = STATE_PARTY_NONE;

	// initialise voice (internally handles already being initialised)
	CNetwork::InitVoice();

	// build join request
	u8 aData[256] = {0};
	unsigned nSizeOfData = 0;
	bool bSuccess = true; // BuildJoinRequest(aData, 256, nSizeOfData, true, true);

	// verify that we've initialised the party
	if(!gnetVerify(bSuccess))
	{
		gnetError("JoinPartySession :: Failed to build Join Request!");
		return false;
	}

	// call into party
	bSuccess = m_Party.Join(NetworkInterface::GetLocalGamerIndex(), tSessionInfo, aData, nSizeOfData, &m_PartyStatus);
	if(!gnetVerify(bSuccess))
	{
		gnetError("JoinPartySession :: Error joining party session!");
		return false;
	}

	// all checks passed - update state
	SetPartyState(STATE_PARTY_JOINING);
	return true;
}

bool CNetworkParty::LeaveParty()
{
	if(!gnetVerify(IsInParty()))
	{
		gnetError("LeaveParty :: Not in a party!");
		return false;
	}

	if(!gnetVerify(!IsPartyStatusPending()))
	{
		gnetError("LeaveParty :: Party Status Pending!");
		return false;
	}

	bool bSuccess = m_Party.Leave(&m_PartyStatus);
	if(!gnetVerify(bSuccess))
	{
		gnetError("LeaveParty :: Error Leaving Party Session!");
		return false;
	}

	// all checks passed - update state
	SetPartyState(STATE_PARTY_LEAVING);
	return true;
}

bool CNetworkParty::HandleJoinRequest(snSession* /*session*/, const rlGamerInfo& /*gamerInfo*/, snJoinRequest* /*joinRequest*/) const
{
	// TODO:
	// Custom party join request logic, default to rlSession join logic for now.
	return true;
}

void CNetworkParty::HandleGetGamerData(snSession* /*session*/, const rlGamerInfo& /*gamerInfo*/, snGetGamerData* /*gamerData*/) const
{
	// TODO:
	// Custom party gamer data logic
}

bool CNetworkParty::IsInParty() const
{
	return m_Party.Exists();
	//return m_Party.IsCreated();
}

bool CNetworkParty::IsPartyLeader() const
{
	return m_Party.IsLeader();
}

unsigned CNetworkParty::GetPartyMemberCount() const
{
	return m_Party.GetGamerCount();
}

bool CNetworkParty::IsPartyMember(const rlGamerHandle& gamerHandle) const
{
	return m_Party.IsMember(gamerHandle);
}

unsigned CNetworkParty::GetPartyMembers(rlGamerInfo* members, const unsigned maxMembers) const
{
	return m_Party.GetMembers(members, maxMembers);	
}

bool CNetworkParty::GetPartyEndpointId(const rlGamerInfo& sInfo, EndpointId* endpointId)
{
	// check we have a valid member first
	if(!m_Party.IsMember(sInfo.GetGamerHandle()))
		return false;

	// forward through session function
	return m_Party.GetPeerEndpointId(sInfo.GetPeerInfo().GetPeerId(), endpointId);
}

bool CNetworkParty::GetPartyLeader(rlGamerInfo* leader) const
{
	return m_Party.GetLeader(leader);	
}

bool CNetworkParty::IsPartyInvitable() const
{
	if(!gnetVerify(IsInParty()))
	{
		gnetError("IsPartyInvitable :: Not in a party!");
		return false;
	}

	return m_Party.IsInvitable();	
}

bool CNetworkParty::SendPartyInvites(const rlGamerHandle* pHandles, const unsigned nGamers)
{
	if(!gnetVerify(IsInParty()))
	{
		gnetError("SendPartyInvites :: Not in a party!");
		return false;
	}

	if(!gnetVerify(!IsPartyStatusPending()))
	{
		gnetError("SendPartyInvites - Party Status Pending!");
		return false;
	}

	bool bSuccess = m_Party.SendPartyInvites(NetworkInterface::GetLocalGamerIndex(), 
											 pHandles, 
											 nGamers, 
											 TheText.Get("NT_INV_PARTY_BO"), 
											 &m_PartyStatus);

	if(!gnetVerify(bSuccess))
		gnetError("SendPartyInvites :: Failed to send invites!");

	return bSuccess;
}

bool CNetworkParty::KickPartyMembers(const rlGamerHandle* pHandles, const unsigned nGamers)
{
	if(!gnetVerify(IsInParty()))
	{
		gnetError("KickPartyMembers :: Not in a party!");
		return false;
	}

	if(!gnetVerify(!IsPartyStatusPending()))
	{
		gnetError("SendPartyInvites - Party Status Pending!");
		return false;
	}

	bool bSuccess = m_Party.Kick(pHandles, nGamers, snSession::KickType::KICK_INFORM_PEERS, &m_PartyStatus);

	if(!gnetVerify(bSuccess))
		gnetError("KickPartyMembers :: Failed to send invites!");

	return bSuccess;
}

bool CNetworkParty::NotifyEnteringGame(const rlSessionInfo& sessionInfo)
{
	rtry
	{
		rcheck(IsInParty(), catchall, );
		rcheck(IsPartyLeader(), catchall, );
		rverify(sessionInfo.IsValid(), catchall, );

		return m_Party.NotifyEnteringGame(sessionInfo);
	}
	rcatchall
	{
		return false;
	}
}

bool CNetworkParty::NotifyLeavingGame()
{
	rtry
	{
		rcheck(IsInParty(), catchall, );
		rcheck(IsPartyLeader(), catchall, );

		return m_Party.NotifyLeavingGame();
	}
	rcatchall
	{
		return false;
	}
}

void CNetworkParty::SetPartyState(const ePartyState state)
{
	if(state != m_PartyState)
	{
#if !__NO_OUTPUT
		static const char* STATE_NAMES[] =
		{
			"STATE_PARTY_NONE",
			"STATE_PARTY_HOSTING",
			"STATE_PARTY_JOINING",
			"STATE_PARTY_MEMBER",
			"STATE_PARTY_LEAVING",
		};

		CompileTimeAssert(COUNTOF(STATE_NAMES) == NUM_PARTY_STATES);
		gnetDebug1("SetPartyState :: New state : %s, Previous State : %s", STATE_NAMES[state], STATE_NAMES[m_PartyState]);
#endif  //__NO_OUTPUT

		m_PartyState = state;
	}
}

bool CNetworkParty::CanParty()
{
	if(!gnetVerify(NetworkInterface::IsLocalPlayerOnline(false)))
	{
		gnetError("CanParty :: Local player not online!");
		return false; 
	}

	// check that local player has online privileges
	if(!gnetVerify(CLiveManager::CheckOnlinePrivileges(NetworkInterface::GetLocalGamerIndex())))
	{
		gnetError("CanParty :: Local player does not have multiplayer privileges!");
		return false; 
	}

	// check that local player has ROS privilege
	if(!gnetVerify(CLiveManager::HasRosPrivilege(RLROS_PRIVILEGEID_MULTIPLAYER)))
	{
		gnetError("CanParty :: No ROS multiplayer privilege!");
		return false; 
	}

	// checks passed
	return true; 
}

void CNetworkParty::OnPartyEvent(snParty* /*pSession*/, const snPartyEvent* pEvent)
{
	if(SNPARTY_EVENT_ENTER_GAME == pEvent->GetId())
	{
		// ...
	}
	else if(SNPARTY_EVENT_LEAVE_GAME == pEvent->GetId())
	{
		// ...
	}
}