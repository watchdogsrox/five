//
// NetworkParty.h
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORKPARTY_H
#define NETWORKPARTY_H

//Rage includes
#include "snet/party.h"
#include "net/task.h"
#include "rline/rlfriendsmanager.h"
#include "rline/rl.h"

// Game includes
#include "NetworkPartyInvites.h"

//PURPOSE
// Game level party management support
class CNetworkParty
{
public:

     CNetworkParty();
	~CNetworkParty() {}

	bool Init();
	void Shutdown();
    void Update();

	bool HostParty();
	bool LeaveParty();
	bool JoinParty(const rlSessionInfo& tSessionInfo);
	bool SendPartyInvites(const rlGamerHandle* gamerHandles, const unsigned numGamers);
	bool KickPartyMembers(const rlGamerHandle* gamerHandles, const unsigned numGamers);

	bool NotifyEnteringGame(const rlSessionInfo& sessionInfo);
	bool NotifyLeavingGame();

	bool HandleJoinRequest(snSession* /*session*/, const rlGamerInfo& /*gamerInfo*/ , snJoinRequest* /*joinRequest*/) const;
	void HandleGetGamerData(snSession* session, const rlGamerInfo& gamerInfo, snGetGamerData* gamerData) const;

	bool IsInParty() const;
	bool IsPartyLeader() const;
	bool IsPartyMember(const rlGamerHandle& gamerHandle) const;
	bool IsPartyInvitable() const;
	bool IsPartyStatusPending() const { return m_PartyStatus.Pending(); }
	bool IsPartyActive() const	{ return m_PartyState != STATE_PARTY_NONE; }

	snParty& GetRlParty() { return m_Party; }
	unsigned GetPartyMemberCount() const;
	unsigned GetPartyMembers(rlGamerInfo* members, const unsigned maxMembers) const;
	bool GetPartyEndpointId(const rlGamerInfo& sInfo, EndpointId* endpointId);
	bool GetPartyLeader(rlGamerInfo* leader) const;

private:

	//PURPOSE
	// Enumeration of possible party session states
	enum ePartyState
	{
		STATE_PARTY_NONE,
		STATE_PARTY_HOSTING,
		STATE_PARTY_JOINING,
		STATE_PARTY_MEMBER,
		STATE_PARTY_LEAVING,
		NUM_PARTY_STATES
	};

	void SetPartyState(const ePartyState state);
	bool CanParty();
	void OnPartyEvent(snParty* pSession, const snPartyEvent* pEvent);
	

	bool m_IsInitialised;
	snParty::Delegate m_PartyDlgt;
	snParty m_Party;
	snSessionOwner m_Owner;
	ePartyState m_PartyState;
	netStatus m_PartyStatus;

#if __BANK
public:
	void InitWidgets();

private:
	void BankInit();

	void Bank_HostParty();
	void Bank_LeaveParty();
	void Bank_RefreshFriends();
	void Bank_InviteFriendToParty(); 
	void Bank_RefreshInvites();
	void Bank_AcceptInvite();
	void Bank_RefreshPartyMembers(); 

	bkCombo* m_pFriendsCombo;
	const char*	m_BankFriends[rlFriendsPage::MAX_FRIEND_PAGE_SIZE];
	int	m_BankFriendsComboIndex;

	bkCombo* m_pInvitesCombo;
	const char*	m_BankInvites[PartyInviteMgr::MAX_PARTY_INVITES];
	int m_BankInvitesComboIndex;

	bkCombo* m_pPartyMembersCombo;
	const char* m_BankPartyMembers[RL_MAX_GAMERS_PER_SESSION];
	int m_BankPartyMembersComboIndex;

	bool m_bBankInitialised;
#endif

};

#endif  //NETWORKPARTY_H
