//
// NetworkPartyInvites.h
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORKPARTY_INVITES_H
#define NETWORKPARTY_INVITES_H

//Rage includes
#include "snet/party.h"
#include "net/task.h"

#if RSG_BANK
#include "bank/bkmgr.h"
#endif

class PartyInvite
{
	friend class PartyInviteMgr;

public:

	PartyInvite();

	bool TimedOut() const;
	bool IsValid() const;

	int GetTimeout() const;
	const char* GetInviterName() const;
	const rlSessionInfo& GetSessionInfo() const;

private:

	enum
	{
		PARTY_INVITE_TIMEOUT_MS = (5 * 60 * 1000) // 5m
	};

	void Clear();
	bool Reset(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName);
	void Update(const int timeStep);

	char m_InviterName[RL_MAX_NAME_BUF_SIZE];
	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_Inviter;
	int m_Timeout;
};

//PURPOSE
// Game level party invite management
class PartyInviteMgr
{
public:

	enum
	{
		MAX_PARTY_INVITES = 8,
	};

     PartyInviteMgr();
	~PartyInviteMgr() {}

	void Init();
	void Shutdown();
    void Update(const int timeStep);
	void ClearAll();

	void AddInvite(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName, const char* salutation);
	void RemoveInvite(const unsigned idx);
	bool AcceptInvite(const unsigned idx);
	const PartyInvite* GetInvite(const unsigned idx);
	unsigned GetInviteIndex(const rlGamerHandle& inviter); // Returns (unsigned)-1 if not found.
	unsigned GetNumInvites();

private:

	void SortInvites();

	atFixedArray<PartyInvite, MAX_PARTY_INVITES> m_PartyInvites;
};

#endif  // NETWORKPARTY_INVITES_H
