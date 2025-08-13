//
// name:		NetworkSessionUtils.cpp
// description:	Utility classes used by the session management code in network match
// written by:	Daniel Yelland
//
#include "NetworkSessionUtils.h"

#include "system/nelem.h"

#include "fwnet/netchannel.h"

#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkSessionMessages.h"

// these prevent spam in the net channel
RAGE_DEFINE_CHANNEL(net_invitedplayers, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netInvitedPlayersDebug(fmt, ...)	RAGE_DEBUGF1(net_invitedplayers, fmt, ##__VA_ARGS__)


namespace NetworkSessionUtils
{
	const char* GetKickReasonAsString(const KickReason kickReason)
	{
		switch(kickReason)
		{
		case KickReason::KR_VotedOut:			return "KR_VotedOut"; break;
		case KickReason::KR_PeerComplaints:		return "KR_PeerComplaints"; break;
		case KickReason::KR_ConnectionError:	return "KR_ConnectionError"; break;
		case KickReason::KR_NatType:			return "KR_NatType"; break;
		case KickReason::KR_ScAdmin:			return "KR_ScAdmin"; break;
		case KickReason::KR_ScAdminBlacklist:	return "KR_ScAdminBlacklist"; break;
		default: break;
		}

		return "KR_Invalid";
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// CNetworkRecentPlayers
///////////////////////////////////////////////////////////////////////////////////////
CNetworkRecentPlayers::CNetworkRecentPlayers()
{
    for(int i = 0; i < COUNTOF(m_RecentPlayersPile); ++i)
    {
        m_RecentPlayersPool.push_back(&m_RecentPlayersPile[i]);
    }
}

CNetworkRecentPlayers::~CNetworkRecentPlayers()
{
}

void CNetworkRecentPlayers::AddRecentPlayer(const rlGamerInfo& gamerInfo)
{
    bool haveRecentPlayer = false;

    const unsigned count = (unsigned) m_RecentPlayers.size();
    const rlGamerId& gamerId = gamerInfo.GetGamerId();

    for(int i = 0; i < (int) count; ++i)
    {
        if(gamerId == m_SortedRecentPlayers[i]->m_GamerId)
        {
            haveRecentPlayer = true;
            break;
        }
    }

    if(!haveRecentPlayer)
    {
        RecentPlayer* mg = NULL;

        if(!m_RecentPlayersPool.empty())
        {
            mg = *m_RecentPlayersPool.begin();
            m_RecentPlayersPool.pop_front();

            m_SortedRecentPlayers[m_RecentPlayers.size()] = mg;
        }
        else if(!m_RecentPlayers.empty())
        {
            mg = *m_RecentPlayers.begin();
            m_RecentPlayers.pop_front();
        }

        if(mg)
        {
            mg->Reset(gamerInfo);
            m_RecentPlayers.push_back(mg);

            std::sort(&m_SortedRecentPlayers[0],
                        &m_SortedRecentPlayers[m_RecentPlayers.size()],
                        RecentPlayerPred());
        }
    }
}

bool CNetworkRecentPlayers::IsRecentPlayer(const rlGamerHandle& hPlayer) const
{
	const unsigned numPlayers = static_cast<int>(m_RecentPlayers.size());
	for(unsigned i = 0; i < numPlayers; ++i)
	{
		if(hPlayer == m_SortedRecentPlayers[i]->m_hGamer)
			return true;
	}
	return false;
}

void CNetworkRecentPlayers::ClearRecentPlayers()
{
    while(!m_RecentPlayers.empty())
    {
        RecentPlayer* mg = *m_RecentPlayers.begin();
        m_RecentPlayers.pop_front();
        m_RecentPlayersPool.push_back(mg);
    }
}

unsigned CNetworkRecentPlayers::GetNumRecentPlayers() const
{
    return static_cast<unsigned>(m_RecentPlayers.size());
}

const char* CNetworkRecentPlayers::GetRecentPlayerName(const unsigned idx) const
{
    return gnetVerify(idx < m_RecentPlayers.size())
            ? m_SortedRecentPlayers[idx]->m_Name
            : nullptr;
}

const rlGamerHandle* CNetworkRecentPlayers::GetRecentPlayerHandle(const unsigned idx) const
{
    return gnetVerify(idx < m_RecentPlayers.size())
            ? &m_SortedRecentPlayers[idx]->m_hGamer
            : nullptr;
}

bool
CNetworkRecentPlayers::RecentPlayerPred::operator()(RecentPlayer* a, RecentPlayer* b) const
{
    return strnicmp(a->m_Name, b->m_Name, sizeof(a->m_Name)) < 0;
}

///////////////////////////////////////////////////////////////////////////////
//  CNetworkRecentPlayers::RecentPlayer
///////////////////////////////////////////////////////////////////////////////
CNetworkRecentPlayers::RecentPlayer::RecentPlayer()
{
    this->Clear();
}

void
CNetworkRecentPlayers::RecentPlayer::Clear()
{
    m_Name[0] = '\0';
    m_GamerId.Clear();
    m_hGamer.Clear();
}

void
CNetworkRecentPlayers::RecentPlayer::Reset(const rlGamerInfo& gamerInfo)
{
    safecpy(m_Name, gamerInfo.GetName(), COUNTOF(m_Name));
    m_GamerId = gamerInfo.GetGamerId();
    m_hGamer = gamerInfo.GetGamerHandle();
}

///////////////////////////////////////////////////////////////////////////////////////
// CBlacklistedGamers
///////////////////////////////////////////////////////////////////////////////////////

CBlacklistedGamers::CBlacklistedGamers()
: m_BlackoutTime(DEFAULT_TIMEOUT)
{
    for(int i = 0; i < COUNTOF(m_BlacklistPile); ++i)
    {
        m_BlacklistPool.push_back(&m_BlacklistPile[i]);
    }
}

CBlacklistedGamers::~CBlacklistedGamers()
{
}

void CBlacklistedGamers::Update()
{
    // grab current time
    unsigned nCurrentTime = sysTimer::GetSystemMsTime();

    // time of 0 implies no timeout
    if(m_BlackoutTime == 0)
        return;

    // check blacklist for expiry
    Blacklist::iterator it = m_Blacklist.begin();
    Blacklist::const_iterator stop = m_Blacklist.end();

    for(; stop != it; ++it)
    {
        // if time has expired, remove this tracked invite
        if ( (nCurrentTime > m_BlackoutTime) && ((nCurrentTime - m_BlackoutTime) > (*it)->m_nTimeAdded) ) //need to also check m_BlackoutTime > nCurrentTime otherwise will subtract into unsigned and rollover causing the value to be > m_nTimeAdded when it really isn't (lavalley).
        {
            gnetDebug1("BlacklistGamer :: Removing %s. Time: %d. Time now: %d", NetworkUtils::LogGamerHandle((*it)->m_GamerHandle), (*it)->m_nTimeAdded, nCurrentTime);

            BlacklistedGamer* blg = *it;
            m_Blacklist.erase(it);
            m_BlacklistPool.push_back(blg);
        }
    }
}

void CBlacklistedGamers::BlacklistGamer(const rlGamerHandle& hGamer, const eBlacklistReason nReason)
{
    this->WhitelistGamer(hGamer);

    BlacklistedGamer* blg = NULL;

    if(!m_BlacklistPool.empty())
    {
        blg = *m_BlacklistPool.begin();
        m_BlacklistPool.pop_front();
    }
    else if(!m_Blacklist.empty())
    {
        blg = *m_Blacklist.begin();
        m_Blacklist.pop_front();
    }

    if(blg)
    {
        blg->m_GamerHandle = hGamer;
		blg->m_Reason = nReason;
        blg->m_nTimeAdded = sysTimer::GetSystemMsTime();
        m_Blacklist.push_back(blg);

        gnetDebug1("BlacklistedGamers :: Blacklisting %s. Reason: %d. Time: %d", NetworkUtils::LogGamerHandle(hGamer), nReason, blg->m_nTimeAdded);

        if(CNetwork::GetNetworkSession().IsHost())
        {
            gnetDebug1("BlacklistedGamers :: Sending blacklist message");

            MsgBlacklist msg;

            msg.Reset(CNetwork::GetNetworkSession().GetSnSession().GetConfig().m_SessionId, hGamer, nReason);
            CNetwork::GetNetworkSession().GetSnSession().BroadcastMsg(msg, NET_SEND_IMMEDIATE);
        }
    }
}

void CBlacklistedGamers::WhitelistGamer(const rlGamerHandle& hGamer)
{
    Blacklist::iterator it = m_Blacklist.begin();
    Blacklist::const_iterator stop = m_Blacklist.end();

    for(; stop != it; ++it)
    {
        if(hGamer == (*it)->m_GamerHandle)
        {
            BlacklistedGamer* blg = *it;
            m_Blacklist.erase(it);
            m_BlacklistPool.push_back(blg);
            break;
        }
    }
}

void CBlacklistedGamers::ClearBlacklist()
{
    gnetDebug1("BlacklistedGamers :: ClearBlacklist");

    while(!m_Blacklist.empty())
    {
        this->WhitelistGamer((*m_Blacklist.begin())->m_GamerHandle);
    }
}

bool CBlacklistedGamers::IsBlacklisted(const rlGamerHandle& hGamer) const
{
    bool isBlack = false;
	//@@: location CBLACKLISTEDGAMERS_ISBLACKLISTED
    Blacklist::const_iterator it = m_Blacklist.begin();
    Blacklist::const_iterator stop = m_Blacklist.end();

    for(; stop != it; ++it)
    {
        if(hGamer == (*it)->m_GamerHandle)
        {
            isBlack = true;
            break;
        }
    }

    return isBlack;
}

eBlacklistReason CBlacklistedGamers::GetBlacklistReason(const rlGamerHandle &hGamer) const
{
	Blacklist::const_iterator it = m_Blacklist.begin();
	Blacklist::const_iterator stop = m_Blacklist.end();

	for(; stop != it; ++it)
	{
		if(hGamer == (*it)->m_GamerHandle)
		{
			return (*it)->m_Reason;
		}
	}

	gnetAssertf(0, "GetBlacklistReason :: Not Blacklisted!");
	return BLACKLIST_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////////////
// CNetworkInvitedPlayers
///////////////////////////////////////////////////////////////////////////////////////

const CNetworkInvitedPlayers::sInvitedGamer CNetworkInvitedPlayers::sm_InvalidInvitedGamer;

CNetworkInvitedPlayers::CNetworkInvitedPlayers(const SessionType sessionType)
	: m_SessionType(sessionType)
{
}

CNetworkInvitedPlayers::~CNetworkInvitedPlayers()
{
}

void SetInviteFlag(unsigned& nFlags, unsigned nFlagToSet, bool bValue)
{
	if(bValue)
		nFlags |= nFlagToSet;
	else
		nFlags &= ~nFlagToSet;
}

void CNetworkInvitedPlayers::Update()
{
	// grab current time
	unsigned nCurrentTime = sysTimer::GetSystemMsTime();

	// check existing gamers
	int nInvitedGamers = m_InvitedPlayers.GetCount();
	for(int i = 0; i < nInvitedGamers; i++)
	{
		// player is in the session, keep around
		if(NetworkInterface::GetPlayerFromGamerHandle(m_InvitedPlayers[i].m_hGamer))
			continue;

		// if time has expired, remove this tracked invite
		if((nCurrentTime > TIME_TO_HOLD_INVITE) && ((nCurrentTime - TIME_TO_HOLD_INVITE) > m_InvitedPlayers[i].m_nTimeAdded))
		{
            netInvitedPlayersDebug("[%u] Update :: Removing invite to %s. Expired.", m_SessionType, NetworkUtils::LogGamerHandle(m_InvitedPlayers[i].m_hGamer));
			m_InvitedPlayers.Delete(i);
			--i;
			--nInvitedGamers;
			continue;
		}

		// check if we need to follow up a presence invite with a platform invite
		if(m_InvitedPlayers[i].IsImportant() && m_InvitedPlayers[i].IsViaPresence() && !m_InvitedPlayers[i].m_bHasAck && !m_InvitedPlayers[i].m_bFollowUpPlatformInviteSent)
		{
			if((nCurrentTime > TIME_TO_WAIT_FOR_ACK) && ((nCurrentTime - TIME_TO_WAIT_FOR_ACK) > m_InvitedPlayers[i].m_nTimeAdded))
			{
				netInvitedPlayersDebug("[%u] Update :: No Ack for Presence Invite to %s in %dms. Sending Platform Invite!", m_SessionType, NetworkUtils::LogGamerHandle(m_InvitedPlayers[i].m_hGamer), TIME_TO_WAIT_FOR_ACK);

				// send invite
				if(CNetwork::GetNetworkSession().IsInCorona())
					CNetwork::GetNetworkSession().SendTransitionInvites(&m_InvitedPlayers[i].m_hGamer, 1);
				else if(CNetwork::GetNetworkSession().IsSessionActive())
					CNetwork::GetNetworkSession().SendInvites(&m_InvitedPlayers[i].m_hGamer, 1);

				// set as platform invite
				SetInviteFlag(m_InvitedPlayers[i].m_nFlags, FLAG_VIA_PRESENCE, false);

				// update states
				m_InvitedPlayers[i].m_bHasAck = true;
				m_InvitedPlayers[i].m_bFollowUpPlatformInviteSent = true;
			}
		}
	}
}

void CNetworkInvitedPlayers::AddInvite(const rlGamerHandle& hGamer, unsigned nFlags)
{
	// if we already have this gamer, update flags and time
	const int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
	{
		// set non type flags
		SetInviteFlag(m_InvitedPlayers[nIndex].m_nFlags, FLAG_IMPORTANT, (nFlags & FLAG_IMPORTANT) != 0);
		SetInviteFlag(m_InvitedPlayers[nIndex].m_nFlags, FLAG_ALLOW_TYPE_CHANGE, (nFlags & FLAG_IMPORTANT) != 0);

        netInvitedPlayersDebug("[%u] AddInvite :: Existing Invite For: %s", m_SessionType, NetworkUtils::LogGamerHandle(hGamer));
        if(((nFlags & FLAG_ALLOW_TYPE_CHANGE) != 0) && m_InvitedPlayers[nIndex].IsViaPresence() != ((nFlags & FLAG_VIA_PRESENCE) != 0))
        {
            netInvitedPlayersDebug("[%u] AddInvite :: Was %s, Now %s", m_SessionType, m_InvitedPlayers[nIndex].IsViaPresence() ? "Presence" : "Platform", !m_InvitedPlayers[nIndex].IsViaPresence() ? "Presence" : "Platform");
			SetInviteFlag(m_InvitedPlayers[nIndex].m_nFlags, FLAG_VIA_PRESENCE, (nFlags & FLAG_VIA_PRESENCE) != 0);
			m_InvitedPlayers[nIndex].m_bHasAck = m_InvitedPlayers[nIndex].IsViaPresence() ? false : true; 
        }

		// updated existing record
		return;
	}

	// invited gamer pool is full
	if(m_InvitedPlayers.IsFull())
	{
		int nOldestTimeAdded = 0;
		int nOldestIndex = -1;

		// find the oldest unacknowledged invite
		int nInvitedGamers = m_InvitedPlayers.GetCount();
		for(int i = 0; i < nInvitedGamers; i++)
		{
			// player is in the session, keep around
			if(NetworkInterface::GetPlayerFromGamerHandle(m_InvitedPlayers[i].m_hGamer))
				continue;

			// if this is an important invite that hasn't been acknowledged and we haven't followed up with a platform invite yet, keep around
			if(((m_InvitedPlayers[i].m_nFlags & FLAG_IMPORTANT) != 0) && !m_InvitedPlayers[i].m_bHasAck && !m_InvitedPlayers[i].m_bFollowUpPlatformInviteSent)
				continue;

			// check against existing invite to chuck
			if((nOldestIndex < 0) || (m_InvitedPlayers[i].m_nTimeAdded < nOldestTimeAdded))
			{
				nOldestTimeAdded = m_InvitedPlayers[i].m_nTimeAdded;
				nOldestIndex = i;
			}
		}

		// remove this gamer
		if(nOldestIndex >= 0)
		{
			netInvitedPlayersDebug("[%u] AddInvite :: Removing oldest invite. For: %s (Added %dms ago)", m_SessionType, NetworkUtils::LogGamerHandle(m_InvitedPlayers[nOldestIndex].m_hGamer), sysTimer::GetSystemMsTime() - m_InvitedPlayers[nOldestIndex].m_nTimeAdded);
			m_InvitedPlayers.Delete(nOldestIndex);
		}
		else // all in session
			return;
	}

	sInvitedGamer tInvitedGamer;
	tInvitedGamer.m_hGamer = hGamer;
	tInvitedGamer.m_nTimeAdded = sysTimer::GetSystemMsTime();
	tInvitedGamer.m_nFlags = nFlags;
	tInvitedGamer.m_bHasAck = tInvitedGamer.IsViaPresence() ? false : true; 

	netInvitedPlayersDebug("[%u] AddInvite :: Adding %s invite to %s. Important: %s, Time: %d", m_SessionType, tInvitedGamer.IsViaPresence() ? "Presence" : tInvitedGamer.IsScriptDummy() ? "Script" : "Platform", NetworkUtils::LogGamerHandle(hGamer), tInvitedGamer.IsImportant() ? "True" : "False", sysTimer::GetSystemMsTime());

	// add new invite
	m_InvitedPlayers.Push(tInvitedGamer);
}

void CNetworkInvitedPlayers::AddInvites(const rlGamerHandle* pGamers, const unsigned nGamers, unsigned nFlags)
{
	for(unsigned i = 0; i < nGamers; i++)
		AddInvite(pGamers[i], nFlags);
}

void CNetworkInvitedPlayers::RemoveInvite(const rlGamerHandle& hGamer)
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex < 0)
		return;

    netInvitedPlayersDebug("[%u] RemoveInvite :: Removing Invite For: %s.", m_SessionType, NetworkUtils::LogGamerHandle(m_InvitedPlayers[nIndex].m_hGamer));

	// remove gamer at found index
	m_InvitedPlayers.Delete(nIndex);
}

void CNetworkInvitedPlayers::RemoveInvites(const rlGamerHandle* pGamers, const unsigned nGamers)
{
	for(unsigned i = 0; i < nGamers; i++)
		RemoveInvite(pGamers[i]);
}

bool CNetworkInvitedPlayers::DidInvite(const rlGamerHandle& hGamer) const
{
	return (m_InvitedPlayers.Find(hGamer) >= 0);
}

bool CNetworkInvitedPlayers::DidInviteViaPresence(const rlGamerHandle& hGamer) const
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
		return m_InvitedPlayers[nIndex].IsViaPresence();

	return false;
}

bool CNetworkInvitedPlayers::AckInvite(const rlGamerHandle& hGamer)
{
    int nIndex = m_InvitedPlayers.Find(hGamer);
    if(nIndex >= 0)
    {
        netInvitedPlayersDebug("[%u] AckInvite :: For: %s, Was: %s", m_SessionType, NetworkUtils::LogGamerHandle(hGamer), m_InvitedPlayers[nIndex].m_bHasAck ? "Acked" : "Not Acked");
        m_InvitedPlayers[nIndex].m_bHasAck = true;
        return true; 
    }

    return false;
}

bool CNetworkInvitedPlayers::IsAcked(const rlGamerHandle& hGamer) const
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
		return m_InvitedPlayers[nIndex].m_bHasAck;

	return false;
}

bool CNetworkInvitedPlayers::SetDecisionMade(const rlGamerHandle& hGamer)
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
	{
		netInvitedPlayersDebug("[%u] SetDecisionMade :: For: %s", m_SessionType, NetworkUtils::LogGamerHandle(hGamer));
		m_InvitedPlayers[nIndex].m_bDecisionMade = true;
		return true; 
	}

	netInvitedPlayersDebug("[%u] SetDecisionMade :: Gamer Not Found: %s", m_SessionType, NetworkUtils::LogGamerHandle(hGamer));

	return false;
}

bool CNetworkInvitedPlayers::IsDecisionMade(const rlGamerHandle& hGamer)
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
		return m_InvitedPlayers[nIndex].m_bDecisionMade;

	return false;
}

bool CNetworkInvitedPlayers::SetInviteStatus(const rlGamerHandle& hGamer, unsigned nStatus)
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
	{
		netInvitedPlayersDebug("[%u] SetInviteStatus :: Status: %u, For: %s", m_SessionType, nStatus, NetworkUtils::LogGamerHandle(hGamer));
		m_InvitedPlayers[nIndex].m_nStatus = nStatus;
		return true; 
	}

	netInvitedPlayersDebug("[%u] SetInviteStatus :: Gamer Not Found: %s, Status: %u", m_SessionType, NetworkUtils::LogGamerHandle(hGamer), nStatus);

	return false;
}

unsigned CNetworkInvitedPlayers::GetInviteStatus(const rlGamerHandle& hGamer)
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
		return m_InvitedPlayers[nIndex].m_nStatus;

	return InviteResponseFlags::IRF_None;
}

const CNetworkInvitedPlayers::sInvitedGamer& CNetworkInvitedPlayers::GetInvitedPlayer(const rlGamerHandle& hGamer) const
{
	int nIndex = m_InvitedPlayers.Find(hGamer);
	if(nIndex >= 0)
		return m_InvitedPlayers[nIndex];

	return sm_InvalidInvitedGamer;
}

unsigned CNetworkInvitedPlayers::GetNumInvitedPlayers()
{
    return m_InvitedPlayers.GetCount();
}

const CNetworkInvitedPlayers::sInvitedGamer& CNetworkInvitedPlayers::GetInvitedPlayerFromIndex(int nIndex) const
{
    return m_InvitedPlayers[nIndex];
}

void CNetworkInvitedPlayers::ClearAllInvites()
{
#if !__NO_OUTPUT
	// log players
	int nInvitedGamers = m_InvitedPlayers.GetCount();
	for(int i = 0; i < nInvitedGamers; i++)
		netInvitedPlayersDebug("[%u] ClearAllInvites :: Removing invite to %s", m_SessionType, NetworkUtils::LogGamerHandle(m_InvitedPlayers[i].m_hGamer));
#endif

	m_InvitedPlayers.Reset();
}

bool CNetworkInvitedPlayers::sInvitedGamer::IsWithinAckThreshold() const
{
	return sysTimer::GetSystemMsTime() - TIME_TO_WAIT_FOR_ACK <= m_nTimeAdded;
}
