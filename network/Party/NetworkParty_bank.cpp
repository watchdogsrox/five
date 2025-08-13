//
// NetworkParty_bank.cpp
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
#if RSG_BANK
#include "NetworkParty.h"

// rage includes
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "net/netdiag.h"
#include "fwnet/netchannel.h"
#include "rline/rlfriend.h"

// game includes
#include "network/live/livemanager.h"

RAGE_DEFINE_SUBCHANNEL(net, partybank, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_partybank

void CNetworkParty::BankInit()
{
	m_pFriendsCombo = NULL;
	m_BankFriendsComboIndex = -1;
	for (int i = 0; i < COUNTOF(m_BankFriends); i++)
	{
		m_BankFriends[i] = "";
	}

	m_pInvitesCombo = NULL;
	m_BankInvitesComboIndex = -1;
	for (int i = 0; i < COUNTOF(m_BankInvites); i++)
	{
		m_BankInvites[i] = "";
	}

	m_pPartyMembersCombo = NULL;
	m_BankPartyMembersComboIndex = -1;
	for (int i = 0; i < COUNTOF(m_BankPartyMembers); i++)
	{
		m_BankPartyMembers[i] = "";
	}

	m_bBankInitialised = false;
}

void CNetworkParty::InitWidgets()
{
	if (m_bBankInitialised)
		return;

	bkBank* pNetworkBank = BANKMGR.FindBank("Network");
	if(gnetVerify(pNetworkBank))
	{
		bkGroup* pLiveGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live"));
		if(gnetVerify(pLiveGroup))
		{
			bkGroup* pPartyGroup = pNetworkBank->PushGroup("Debug Party");
				pNetworkBank->AddTitle("Friends");
				m_pFriendsCombo = pPartyGroup->AddCombo("Friends", &m_BankFriendsComboIndex, 1, (const char**)m_BankFriends);
				pNetworkBank->AddButton("Refresh Friends", datCallback(MFA(CNetworkParty::Bank_RefreshFriends), (datBase*)this));
				pNetworkBank->AddButton("Invite Friend", datCallback(MFA(CNetworkParty::Bank_InviteFriendToParty), (datBase*)this));
				pNetworkBank->AddSeparator();

				pNetworkBank->AddTitle("Controls");
				pNetworkBank->AddButton("Host Party", datCallback(MFA(CNetworkParty::Bank_HostParty), (datBase*)this));
				pNetworkBank->AddButton("Leave Party", datCallback(MFA(CNetworkParty::Bank_LeaveParty), (datBase*)this));
				pNetworkBank->AddSeparator();

				pNetworkBank->AddTitle("Invites");
				m_pInvitesCombo = pPartyGroup->AddCombo("Invites", &m_BankInvitesComboIndex, 1, (const char**)m_BankInvites);
				pNetworkBank->AddButton("Refresh Invites", datCallback(MFA(CNetworkParty::Bank_RefreshInvites), (datBase*)this));
				pNetworkBank->AddButton("Accept Invite", datCallback(MFA(CNetworkParty::Bank_AcceptInvite), (datBase*)this));
				pNetworkBank->AddSeparator();

				pNetworkBank->AddTitle("Party Members");
				m_pPartyMembersCombo = pPartyGroup->AddCombo("Party Members", &m_BankPartyMembersComboIndex, 1, (const char**)m_BankPartyMembers);
				pNetworkBank->AddButton("Refresh Party", datCallback(MFA(CNetworkParty::Bank_RefreshPartyMembers), (datBase*)this));
				pNetworkBank->AddSeparator();

			pNetworkBank->PopGroup();
		}
	}

	m_bBankInitialised = true;
}

void CNetworkParty::Bank_HostParty()
{
	HostParty();
}

void CNetworkParty::Bank_LeaveParty()
{
	LeaveParty();
}

void CNetworkParty::Bank_RefreshFriends()
{
	if(!m_pFriendsCombo)
		return;

	// update the friend list
	rlFriendsPage* pPage = CLiveManager::GetFriendsPage();
	int nFriends = pPage->m_NumFriends;
	int nOnlineFriends = 0;
	for(u32 i = 0; i < nFriends; i++)
	{
		rlFriend& pFriend = pPage->m_Friends[i];
		if(pFriend.IsValid() && pFriend.IsOnline())
		{
			m_BankFriends[i] = pPage->m_Friends[i].GetName();
			nOnlineFriends++;
		}
	}

	// avoid empty combo box
	if(nOnlineFriends == 0)
	{
		m_BankFriends[0] = "--No Friends--";
		nOnlineFriends = 1;
	}

	m_BankFriendsComboIndex = Clamp(m_BankFriendsComboIndex, 0, static_cast<int>(nOnlineFriends) - 1);
	m_pFriendsCombo->UpdateCombo("Friends", &m_BankFriendsComboIndex, nOnlineFriends, (const char**)m_BankFriends);
}

void CNetworkParty::Bank_RefreshInvites()
{
	if (!m_pInvitesCombo)
		return;

	PartyInviteMgr&	inviteMgr = CLiveManager::GetPartyInviteMgr();
	unsigned nInvites = inviteMgr.GetNumInvites();
	for (int i = 0; i < nInvites; i++)
	{
		const PartyInvite* invite = inviteMgr.GetInvite(i);
		if (invite)
		{
			m_BankInvites[i] = invite->GetInviterName();
		}
	}

	if (nInvites == 0)
	{
		m_BankInvites[0] = "--No Invites--";
		nInvites = 1;
	}

	m_BankInvitesComboIndex = Clamp(m_BankInvitesComboIndex, 0, static_cast<int>(nInvites) - 1);
	m_pInvitesCombo->UpdateCombo("Invites", &m_BankInvitesComboIndex, nInvites, (const char**)m_BankInvites);
}

void CNetworkParty::Bank_AcceptInvite()
{
	CLiveManager::GetPartyInviteMgr().AcceptInvite(m_BankInvitesComboIndex);
}

void CNetworkParty::Bank_RefreshPartyMembers()
{
	if (!m_bBankInitialised)
		return;

	if (!m_pPartyMembersCombo)
		return;

	rlGamerInfo partyMembers[RL_MAX_GAMERS_PER_SESSION];
	unsigned numPartyMembers = GetPartyMembers(partyMembers, RL_MAX_GAMERS_PER_SESSION);
	for (int i = 0; i < numPartyMembers; i++)
	{
		m_BankPartyMembers[i] = partyMembers[i].GetName();
	}

	if (numPartyMembers == 0)
	{
		m_BankPartyMembers[0] = "--Empty Party--";
		numPartyMembers = 1;
	}

	m_BankPartyMembersComboIndex = Clamp(m_BankPartyMembersComboIndex, 0, static_cast<int>(numPartyMembers) - 1);
	m_pPartyMembersCombo->UpdateCombo("Party Members", &m_BankPartyMembersComboIndex, numPartyMembers, (const char**)m_BankPartyMembers);
}

void CNetworkParty::Bank_InviteFriendToParty()
{
	rlFriendsPage* pPage = CLiveManager::GetFriendsPage();
	rlFriend& pFriend = pPage->m_Friends[m_BankFriendsComboIndex];
	if(pFriend.IsValid() && pFriend.IsOnline())
	{
		rlGamerHandle hFriend;
		pFriend.GetGamerHandle(&hFriend);
		if (!IsPartyMember(hFriend))
		{
			SendPartyInvites(&hFriend, 1);
		}
	}
}

#endif