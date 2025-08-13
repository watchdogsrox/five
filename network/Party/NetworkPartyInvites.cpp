//
// NetworkPartyInvites.cpp
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
#include "NetworkPartyInvites.h"

// rage includes
#include "fwnet/netchannel.h"
#include "rline/rlgamerinfo.h"

// game includes
#include "network/NetworkInterface.h"

PartyInviteMgr::PartyInviteMgr()
{
	ClearAll();
}

void PartyInviteMgr::Init()
{
	ClearAll();
}

void PartyInviteMgr::Shutdown()
{
	ClearAll();
}

void PartyInviteMgr::Update(const int timeStep)
{
	// clear out timed out invites
	bool bFinished = false;
	while(!bFinished)
	{
		bFinished = true; 
		int nUnacceptedInvites = m_PartyInvites.GetCount();
		for(int i = 0; i < nUnacceptedInvites; i++)
		{
			PartyInvite* pInvite = &m_PartyInvites[i];
			pInvite->Update(timeStep);

			if(pInvite->TimedOut())
			{
				gnetDebug3("Party invite from \"%s\" timed out", pInvite->m_InviterName);
				m_PartyInvites.Delete(i);
				bFinished = false;
				break;
			}
		}
	}
}

void PartyInviteMgr::ClearAll()
{
	m_PartyInvites.clear();
}

void PartyInviteMgr::AddInvite(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName, const char* /*salutation*/)
{
	if (gnetVerify(inviterName))
	{
		// check for duplicates.  If a duplicate is found, replace it with the new invitation.
		int numInvites = m_PartyInvites.GetCount();
		for(int i = 0; i < numInvites; ++i)
		{
			if(inviter == m_PartyInvites[i].m_Inviter)
			{
				gnetDebug1("PartyInvite :: Received duplicate invite from \"%s\"", inviterName);
				RemoveInvite(i);
				break;
			}
		}

		// pop the oldest invite off the stack
		if (m_PartyInvites.IsFull())
		{
			m_PartyInvites.Pop();
		}

		PartyInvite sInvite;
		if(sInvite.Reset(sessionInfo, inviter, inviterName))
		{
			m_PartyInvites.Push(sInvite);
			SortInvites();
		}
	}
}

void PartyInviteMgr::RemoveInvite(const unsigned idx)
{
	if(gnetVerify(idx < m_PartyInvites.GetCount()))
	{
		m_PartyInvites.Delete(idx);
	}
}

bool PartyInviteMgr::AcceptInvite(const unsigned idx)
{
	const PartyInvite* pInvite = GetInvite(idx);
	if(!gnetVerify(pInvite))
		return false;

	if(!gnetVerify(pInvite->IsValid()))
		return false;

	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if(!gnetVerify(pInfo))
		return false;

	if (!gnetVerify(!NetworkInterface::IsInParty()))
	{
		return false;
	}

	gnetDebug1("PartyInvite :: Accepted invitation from \"%s\"", pInvite->m_InviterName);

	NetworkInterface::JoinParty(pInvite->m_SessionInfo);

	RemoveInvite(idx);

	return true;
}

unsigned PartyInviteMgr::GetNumInvites()
{
	return m_PartyInvites.GetCount();
}

unsigned PartyInviteMgr::GetInviteIndex(const rlGamerHandle& inviter)
{
	for(int i = 0; i < m_PartyInvites.GetCount(); ++i)
	{
		if(m_PartyInvites[i].m_Inviter == inviter)
		{
			return i;
		}
	}

	return (unsigned)-1;
}

const PartyInvite* PartyInviteMgr::GetInvite(const unsigned idx)
{
	return gnetVerify(idx < GetNumInvites()) ? &m_PartyInvites[idx] : NULL;
}

int ComparePartyInvites(const PartyInvite* pA, const PartyInvite* pB)
{
	return pA->GetTimeout() > pB->GetTimeout();
}

void PartyInviteMgr::SortInvites()
{
	int nNumInvites = m_PartyInvites.GetCount();
	if(nNumInvites > 1)
	{
		m_PartyInvites.QSort(0, nNumInvites, ComparePartyInvites);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  PartyInvite
///////////////////////////////////////////////////////////////////////////////

PartyInvite::PartyInvite()
{
	this->Clear();
}

bool PartyInvite::IsValid() const
{
	return m_SessionInfo.IsValid();
}

void PartyInvite::Clear()
{
	m_InviterName[0] = '\0';
	m_SessionInfo.Clear();
	m_Inviter.Clear();
}

bool PartyInvite::Reset(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName)
{
	gnetAssert(inviterName);
	gnetAssert(sessionInfo.IsValid());
	gnetAssert(inviter.IsValid());
	gnetAssert(strlen(inviterName) > 0);

	bool success = false;

	this->Clear();

	const rlGamerInfo* mainGamer = NetworkInterface::GetActiveGamerInfo();
	if(gnetVerify(mainGamer))
	{
		m_SessionInfo = sessionInfo;
		m_Inviter = inviter;
		m_Timeout = PARTY_INVITE_TIMEOUT_MS;
		safecpy(m_InviterName, inviterName, COUNTOF(m_InviterName));
		success = true;
	}

	return success;
}

void PartyInvite::Update(const int timeStep)
{
	m_Timeout -= timeStep;

	if(m_Timeout <= 0)
	{
		gnetDebug1("PartyInvite :: Unaccepted invite from \"%s\" timed out.", m_InviterName);
		m_Timeout = 0;
	}
}

bool PartyInvite::TimedOut() const
{
	return m_Timeout <= 0;
}

int PartyInvite::GetTimeout() const
{
	return m_Timeout;
}

const char* PartyInvite::GetInviterName() const
{
	return m_InviterName;
}

const rlSessionInfo& PartyInvite::GetSessionInfo() const
{
	return m_SessionInfo;
}