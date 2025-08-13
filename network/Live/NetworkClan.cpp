//
// NetworkClan.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// Rage headers
#include "rline/rlgamerinfo.h"
#include "rline/rlfriend.h"
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"
#include "rline/rl.h"
#include "string/stringbuilder.h"

// Framework headers
#include "fwnet/netchannel.h"
#include "fwsys/gameskeleton.h"

// Network headers
#include "NetworkClan.h"
#include "Network/Live/LiveManager.h"
#include "Network/NetworkInterface.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Event/EventGroup.h"
#include "Event/EventNetwork.h"
#include "frontend/GameStreamMgr.h"
#include "Stats/StatsInterface.h"
#include "text/TextFile.h"

#if __BANK
#include "Network/Live/LiveManager.h"
#include "fwsys/timer.h"
#include "bank/bank.h"
#include "bank/text.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#endif // __BANK


NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, clan, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_clan

//Wait time to reset active crew stat
static const u32 TIMER_TO_RESET_STAT_ACTIVE_CREW = 1*60*1000;

//Spew rlClanRank
void   PrintClanRank(const rlClanRank& OUTPUT_ONLY(rank))
{
#if !__NO_OUTPUT
	gnetDebug1("     ClanRank:");
	gnetDebug1("....................  Name = \"%s\"", rank.m_RankName);
	gnetDebug1("...................  Order = \"%d\"", rank.m_RankOrder);
#endif // !__NO_OUTPUT
}

//Spew rlClanDesc
void   PrintClanDesc(const rlClanDesc& OUTPUT_ONLY(desc))
{
#if !__NO_OUTPUT
	gnetDebug1("     ClanDesc:");
	gnetDebug1("......................  Id = \"%" I64FMT "d\"", desc.m_Id);
	gnetDebug1("....................  Name = \"%s\"", desc.m_ClanName);
	gnetDebug1(".....................  Tag = \"%s\"", desc.m_ClanTag);
	gnetDebug1("...................  Motto = \"%s\"", desc.m_ClanMotto);
	gnetDebug1("............  Member Count = \"%d\"", desc.m_MemberCount);
#endif // !__NO_OUTPUT
}

//Spew rlRosPlayerInfo
void   PrintRosPlayerInfo(const rlRosPlayerInfo& OUTPUT_ONLY(player))
{
#if !__NO_OUTPUT
	char buff[255];
	gnetDebug1("     RosPlayerInfo = \"%s\":", player.m_GamerHandle.ToString(buff, sizeof(buff)));
	gnetDebug1("................  NickName = \"%s\"", player.m_Nickname);
	gnetDebug1("................  Gamertag = \"%s\"", player.m_Gamertag);
	gnetDebug1("..............  RockstarId = \"%" I64FMT "d\"", player.m_RockstarId);
	gnetDebug1(".........  PlayerAccountId = \"%d\"", player.m_PlayerAccountId);
#endif // !__NO_OUTPUT
}

//Spew rlClanMembershipData
void   PrintClanMembershipData(const rlClanMembershipData& OUTPUT_ONLY(memberClanInfo))
{
#if !__NO_OUTPUT
	gnetDebug1("     ClanMembershipData Id=\"%" I64FMT "d\":", memberClanInfo.m_Id);
	PrintClanDesc(memberClanInfo.m_Clan);
	PrintClanRank(memberClanInfo.m_Rank);
#endif // !__NO_OUTPUT
}

//Spew rlClanMember
void   PrintClanMemberData(const rlClanMember& OUTPUT_ONLY(member))
{
#if !__NO_OUTPUT
	gnetDebug1("Clan information ");
	PrintRosPlayerInfo(member.m_MemberInfo);
	PrintClanMembershipData(member.m_MemberClanInfo);
#endif // !__NO_OUTPUT
}

//copy clan information from one member to another.
void   CopyClanRank(const rlClanRank& from, rlClanRank& to)
{
	formatf(to.m_RankName, RL_MAX_NAME_BUF_SIZE, from.m_RankName);
	to.m_Id          = from.m_Id;
	to.m_RankOrder   = from.m_RankOrder;
	to.m_SystemFlags = from.m_SystemFlags;
}

//copy clan information from one member to another.
void   CopyClanDesc(const rlClanDesc& from, rlClanDesc& to)
{
	formatf(to.m_ClanName, RL_CLAN_NAME_MAX_CHARS, from.m_ClanName);
	formatf(to.m_ClanTag, RL_CLAN_TAG_MAX_CHARS, from.m_ClanTag);
	formatf(to.m_ClanMotto, RL_CLAN_MOTTO_MAX_CHARS, from.m_ClanMotto);
	to.m_Id               = from.m_Id;
	to.m_MemberCount      = from.m_MemberCount;
	to.m_CreatedTimePosix = from.m_CreatedTimePosix;
	to.m_IsSystemClan     = from.m_IsSystemClan;
	to.m_IsOpenClan       = from.m_IsOpenClan;
}

//copy clan information from one member to another.
void   CopyClanMembershipData(const rlClanMembershipData& from, rlClanMembershipData& to)
{
	//------ rlClanMembershipData
	to.m_Id = from.m_Id;
	//------ rlClanDesc
	CopyClanDesc(from.m_Clan, to.m_Clan);
	//------ rlClanRank
	CopyClanRank(from.m_Rank, to.m_Rank);
}

//copy clan information from one member to another.
void   CopyClanMember(const rlClanMember& from, rlClanMember& to)
{
	gnetAssert(!to.IsValid());

	to.Clear();

	//------ rlRosPlayerInfo
	formatf(to.m_MemberInfo.m_Nickname, RL_MAX_NAME_BUF_SIZE, from.m_MemberInfo.m_Nickname);
	formatf(to.m_MemberInfo.m_Gamertag, RL_MAX_NAME_BUF_SIZE, from.m_MemberInfo.m_Gamertag);
	to.m_MemberInfo.m_GamerHandle     = from.m_MemberInfo.m_GamerHandle;
	to.m_MemberInfo.m_RockstarId      = from.m_MemberInfo.m_RockstarId;
	to.m_MemberInfo.m_PlayerAccountId = from.m_MemberInfo.m_PlayerAccountId;

	//------ rlClanMembershipData
	CopyClanMembershipData(from.m_MemberClanInfo, to.m_MemberClanInfo);
}


//Keep all valid members tidy in the left. Return the first empty member.
unsigned   ShiftLeftClanMembers(rlClanMember* clans, const unsigned totalcount)
{
	u32 index = 0;

	for (u32 i=0; i<totalcount && index<totalcount; i++)
	{
		while(index<totalcount && clans[index].IsValid())
		{
			++index;
		}

		if (index == i || index > i)
			continue;

		if (index >= totalcount)
			break;

		if (!clans[i].IsValid())
			continue;

		if (!clans[index].IsValid() && clans[i].IsValid())
		{
			CopyClanMember(clans[i], clans[index]);
			clans[i].Clear();

			while(index<totalcount && clans[index].IsValid())
			{
				index++;
			}
		}
	}

	return index;
}

//Keep all valid members tydy in the left. Return the first empty member.
bool   ClanMemberHandleExists(const rlClanMember* clans, const unsigned totalcount, const rlGamerHandle& handle)
{
	bool exists = false;

	if (gnetVerify(clans))
	{
		for (u32 i=0; i<totalcount && !exists; i++)
		{
			exists = (clans[i].IsValid() && clans[i].m_MemberInfo.m_GamerHandle == handle);
		}
	}

	return exists;
}

//Spew clan membership data
void 
ClanMembershipData::PrintMembershipData()
{
#if !__NO_OUTPUT
	char buff[255];
	gnetDebug1("Clan info for gamer=\"%s\", Count=\"%d\", Total Count=\"%d\", Active=\"%d\""
														,m_GamerHandle.ToString(buff, sizeof(buff))
														,m_Count
														,m_TotalCount
														,m_Active);

	for (u32 i=0; i<m_Count; i++)
	{
		gnetDebug1("................  Id = \"%" I64FMT "d\"", m_Data[i].m_Clan.m_Id);
		gnetDebug1("..............  Name = \"%s\"", m_Data[i].m_Clan.m_ClanName);
		gnetDebug1("...............  Tag = \"%s\"", m_Data[i].m_Clan.m_ClanTag);
		gnetDebug1(".............  Motto = \"%s\"", m_Data[i].m_Clan.m_ClanMotto);
		gnetDebug1("......  Member Count = \"%d\"", m_Data[i].m_Clan.m_MemberCount);
		gnetDebug1("..............  Rank = \"%s\"", m_Data[i].m_Rank.m_RankName);
		gnetDebug1("........  Rank Order = \"%d\"", m_Data[i].m_Rank.m_RankOrder);
	}
#endif // !__NO_OUTPUT
}

bool
ClanMembershipData::Reset(const rlClanId& id)
{
	bool found = false;
	for (u32 i=0; i<MAX_NUM_CLAN_MEMBERSHIPS && !found; i++)
	{
		if (m_Data[i].m_Clan.m_Id == id)
		{
			Reset(i);
			found = true;
		}
	}

	return found;
}

void 
ClanMembershipData::Reset(const unsigned idx)
{
	if (gnetVerify(idx < MAX_NUM_CLAN_MEMBERSHIPS) && gnetVerify(m_Count>0))
	{
		// Shift all clans one pos left in the array, overwriting the reseted one
		for(int i=idx+1; i<m_Count; ++i)
		{
			CopyClanMembershipData(m_Data[i], m_Data[i-1]);
		}

		// Clear latest clan before setting it as 'free'
		m_Data[m_Count-1].Clear();
		
		m_Count--;

		if (m_Active == (int)idx)
		{
			m_Active = -1;
		}
	}
}

void 
ClanMembershipData::Clear()
{
	m_GamerHandle.Clear();
	m_Count = 0;
	m_TotalCount = 0;
	m_Active = -1;
	m_Pending = false;
	for (u32 i=0; i<MAX_NUM_CLAN_MEMBERSHIPS; i++)
	{
		m_Data[i].Clear();
	}
}

const rlClanMembershipData*  
ClanMembershipData::Get(const u32 idx) const
{
	const rlClanMembershipData* membership = NULL;

	if (gnetVerify(idx < MAX_NUM_CLAN_MEMBERSHIPS) && gnetVerify(m_Data[idx].IsValid()))
	{
		membership = reinterpret_cast<const rlClanMembershipData *>( &m_Data[idx] );
	}

	gnetAssert(membership);

	return membership;
}

const rlClanMembershipData*  
ClanMembershipData::GetActive() const
{
	const rlClanMembershipData* membership = NULL;

	if (gnetVerify(ActiveIsValid()))
	{
		membership = reinterpret_cast<const rlClanMembershipData *>( &m_Data[m_Active] );
	}

	gnetAssert(membership);

	return membership;
}

bool 
ClanMembershipData::IsValid(const u32 idx) const
{
	if (gnetVerify(idx < MAX_NUM_CLAN_MEMBERSHIPS))
	{
		return m_Data[idx].IsValid();
	}

	return false;
}


bool 
ClanMembershipData::ActiveIsValid() const
{
	if (0 <= m_Active && m_Active < MAX_NUM_CLAN_MEMBERSHIPS)
	{
		return m_Data[m_Active].IsValid();
	}

	return false;
}

bool
ClanMembershipData::SetActiveClanMembership(const u32 index)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
	{
		if (gnetVerify(index < m_Count) && gnetVerify(m_Data[index].IsValid()))
		{
			m_Active = index;
			result = true;
		}
	}

	return result;
}

void ClanMembershipData::UpdateActiveClanMembership()
{
	m_Active = -1;

	for(int index=0; index < m_Count; ++index)
	{
		if(m_Data[index].m_IsPrimary && m_Data[index].IsValid())
		{
			m_Active = index;
			return;
		}
	}
}

bool
ClanMembershipData::AddActiveClanMembershipData(const rlGamerHandle& gamer, const rlClanMembershipData& memberData)
{
	gnetAssert(!m_Pending);

	bool added = false;

	// Try to reuse a clan slot if already present in the array
	for (u32 i=0; i<m_Count && !added; ++i)
	{
		if (m_Data[i].m_Clan.m_Id == memberData.m_Clan.m_Id)
		{
			CopyClanMembershipData(memberData, m_Data[i]);
			SetActiveClanMembership(i);
			added = true;
		}
	}

	// If wasn't found, add it on first available position
	if(!added && gnetVerify(m_Count<MAX_NUM_CLAN_MEMBERSHIPS))
	{
		const u32 idx = m_Count;

		CopyClanMembershipData(memberData, m_Data[idx]);

		m_Count++;

		SetActiveClanMembership(idx);

		added = true;
	}

	if (added)
	{
		m_GamerHandle = gamer;
	}

	gnetAssertf(added, "Failed to add new clan membership data.");

	return added;
}

void 
ClanMembershipData::SetGamerHandle(const rlGamerHandle& gamer)
{
	gnetAssert(!m_GamerHandle.IsValid() || m_GamerHandle == gamer);
	m_GamerHandle = gamer;
}

void 
ClanMembershipData::SetPendingRefresh(const rlGamerHandle& gamer)
{
	if (gnetVerify(!PendingRefresh()))
	{
		SetGamerHandle(gamer);
		m_Pending = true;
	}
}

ClanMembershipData& 
ClanMembershipData::operator=(const ClanMembershipData& that)
{
	m_GamerHandle = that.m_GamerHandle;
	m_Count       = that.m_Count;
	m_TotalCount  = that.m_TotalCount;
	m_Active      = that.m_Active;
	m_Pending     = that.m_Pending;

	for (u32 i=0; i<MAX_NUM_CLAN_MEMBERSHIPS; i++)
	{
		m_Data[i] = that.m_Data[i];
	}

	return *this;
}


NetworkClan::NetworkClan()
: m_RanksCount(0)
 ,m_RanksTotalCount(0)
 ,m_OpenClanCount(0)
 ,m_OpenClanTotalCount(0)
 ,m_InvitesReceivedCount(0)
 ,m_InvitesReceivedTotalCount(0)
 ,m_InvitesSentCount(0)
 ,m_InvitesSentTotalCount(0)
 ,m_PreviousInvitesReceivedCount(0)
 ,m_JoinRequestsReceivedCount(0)
 ,m_JoinRequestsReceivedTotalCount(0)
 ,m_MembersCount(0)
 ,m_MembersTotalCount(0)
 ,m_WallCount(0)
 ,m_WallTotalCount(0)
 ,m_bRequestMaintenanceInviteRefresh(false)
 ,m_bRequestMaintenanceJoinRequestRefresh(false)
 ,m_pendingTimerSetActiveCrew(0)
 ,m_pendingJoinClanId(-1)
#if __BANK
 ,m_BankMembershipComboName(NULL)
 ,m_BankMembershipComboTag(NULL)
 ,m_BankMembershipComboRank(NULL)
 ,m_BankOpenClansComboName(NULL)
 ,m_BankOpenClansComboTag(NULL)
 ,m_BankMembersComboName(NULL)
 ,m_BankMembersComboRank(NULL)
 ,m_BankWallComboWriter(NULL)
 ,m_BankWallComboMessage(NULL)
 ,m_BankInvitesComboReceived(NULL)
 ,m_BankInvitesComboClanReceived(NULL)
 ,m_BankInvitesComboSent(NULL)
 ,m_BankInvitesComboClanSent(NULL)
 ,m_BankPlayersCombo(NULL)
 ,m_BankMembershipPlayersCombo(NULL)
 ,m_BankFriendsCombo(NULL)
 ,m_BankMembershipFriendsCombo(NULL)
 ,m_BankRankCombo(NULL)
 ,m_BankRankUpdateNameText(NULL)
 ,m_BankRankKick(NULL)
 ,m_BankRankInvite(NULL)
 ,m_BankRankPromote(NULL)
 ,m_BankRankDemote(NULL)
 ,m_BankRankRankManager(NULL)
 ,m_BankRankAllowOpen(NULL)
 ,m_BankRankWriteOnWall(NULL)
 ,m_BankRankDeleteFromWall(NULL)
 ,m_EmblemRefd(RL_INVALID_CLAN_ID)
#endif // __BANK
{
	Clear();
}

NetworkClan::~NetworkClan()
{
	Shutdown(SHUTDOWN_CORE);
}

void 
NetworkClan::Init(const unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		m_EventDelegator.Bind(this, &NetworkClan::OnClanEvent);
		rlClan::AddDelegate(&m_EventDelegator);
		m_RosDeletator.Bind(this, &NetworkClan::OnRosEvent);
		rlRos::AddDelegate(&m_RosDeletator);
	}

	m_EmblemMgr.Init();
}

void 
NetworkClan::Shutdown(const u32 shutdownMode)
{
	for(int i = 0; i < MAX_NUM_CLAN_OPS; ++i)
	{
		if (m_OpPool[i].Pending())
		{
			rlClan::Cancel(m_OpPool[i].GetStatus());
		}

		gnetDebug1("Shutdown clan operation in index '%d'", i);

		m_OpPool[i].Clear();
	}

	if(shutdownMode == SHUTDOWN_SESSION)
	{
	}
	else if (shutdownMode == SHUTDOWN_CORE)
	{
		if (m_LocalPlayerDesc.IsValid())
		{
			ReleaseEmblemReference(m_LocalPlayerDesc.m_Id   ASSERT_ONLY(, "NetworkClan"));
			m_EmblemRefd=RL_INVALID_CLAN_ID;
		}

		rlClan::RemoveDelegate(&m_EventDelegator);
		rlRos::RemoveDelegate(&m_RosDeletator);
		m_EmblemMgr.Shutdown();
		m_primaryCrewMetadata.Clear();
		m_Delegator.Clear();
	}
}

void
NetworkClan::Clear()
{
	for(int i = 0; i < MAX_NUM_CLAN_OPS; ++i)
	{
		if (m_OpPool[i].Pending())
		{
			rlClan::Cancel(m_OpPool[i].GetStatus());
		}

		gnetDebug1("Clear clan operation in index '%d'", i);

		m_OpPool[i].Clear();
	}

	m_RanksCount = 0;
	m_RanksTotalCount = 0;
	m_OpenClanCount = 0;
	m_OpenClanTotalCount = 0;
	m_InvitesReceivedCount = 0;
	m_InvitesReceivedTotalCount = 0;
	m_InvitesSentCount = 0;
	m_InvitesSentTotalCount = 0;
	m_JoinRequestsReceivedCount = 0;
	m_JoinRequestsReceivedTotalCount = 0;
	m_MembersCount = 0;
	m_MembersTotalCount = 0;
	m_WallCount = 0;
	m_WallTotalCount = 0;
	m_bRequestMaintenanceInviteRefresh = false;
	m_bRequestMaintenanceJoinRequestRefresh = false;

	if (m_LocalPlayerDesc.IsValid())
	{
		ReleaseEmblemReference(m_LocalPlayerDesc.m_Id  ASSERT_ONLY(, "NetworkClan"));
		m_EmblemRefd=RL_INVALID_CLAN_ID;
	}

	m_Membership.Clear();
	m_LocalPlayerMembership.Clear();
	m_primaryCrewMetadata.Clear();
	m_LocalPlayerDesc.Clear();

    for (u32 i=0; i<MAX_NUM_CLAN_RANKS; i++)
    {
        m_Rank[i].Clear();
    }

    for (u32 i=0; i<MAX_NUM_CLANS; i++)
    {
        m_OpenClan[i].Clear();
    }

	for (u32 i=0; i<MAX_NUM_CLAN_INVITES; i++)
	{
		m_InvitesReceived[i].Clear();
		m_InvitesSent[i].Clear();
	}

	for (u32 i=0; i<MAX_NUM_CLAN_JOIN_REQUESTS; i++)
	{
		m_JoinRequestsReceived[i].Clear();
	}

	for (u32 i=0; i<MAX_NUM_CLAN_MEMBERS; i++)
	{
		m_Members[i].Clear();
	}

    for (u32 i=0; i<MAX_NUM_CLAN_WALL_MSGS; i++)
    {
        m_Wall[i].Clear();
    }
}

void 
NetworkClan::Update()
{
	if (!rlClan::IsInitialized())
		return;

	if (!CLiveManager::IsOnline())
		return;

	if (!NetworkInterface::HasValidRosCredentials())
		return;

	//We missed to setup active crew clan when a event was received.
	if (m_pendingTimerSetActiveCrew > 0)
	{
		if (StatsInterface::IsKeyValid(STAT_ACTIVE_CREW))
		{
			const unsigned curTime = sysTimer::GetSystemMsTime();
			if (curTime > m_pendingTimerSetActiveCrew)
			{
				m_pendingTimerSetActiveCrew = 0;

				if (m_LocalPlayerDesc.IsValid())
				{
					StatsInterface::SetStatData(STAT_ACTIVE_CREW, (u64)m_LocalPlayerDesc.m_Id, STATUPDATEFLAG_ASSERTONLINESTATS);
				}
				else
				{
					StatsInterface::SetStatData(STAT_ACTIVE_CREW, (u64)RL_INVALID_CLAN_ID, STATUPDATEFLAG_ASSERTONLINESTATS);
				}
			}
		}
		else
		{
			m_pendingTimerSetActiveCrew = sysTimer::GetSystemMsTime() + TIMER_TO_RESET_STAT_ACTIVE_CREW;
		}
	}

	BANK_ONLY( Bank_Update(); );

	m_EmblemMgr.Update();
	m_primaryCrewMetadata.Update();
	
	//No Social Club Account
	if (!NetworkInterface::HasValidRockstarId())
		return;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (rlClan::IsFullServiceReady(localGamerIndex) && CLiveManager::IsOnlineRos(localGamerIndex))
	{
		if (m_bRequestMaintenanceInviteRefresh)
		{
			gnetDebug1("Refreshing Crew Invites due to request");
			RefreshInvitesReceived();
			m_bRequestMaintenanceInviteRefresh = false;
		}

		if( m_bRequestMaintenanceJoinRequestRefresh )
		{
			gnetDebug1("Refreshing Crew Join Requests due to request");
			RefreshJoinRequestsReceived();
			m_bRequestMaintenanceJoinRequestRefresh = false;
		}
	}
	
	if(m_WarningMessage.IsActive())
	{
		m_WarningMessage.Update();
	}

	ProcessOps();
}

void 
NetworkClan::ProcessOps()
{
	for (int i = 0; i < MAX_NUM_CLAN_OPS; ++i)
	{
		NetworkClanOp& rOp = m_OpPool[i];
		if (!rOp.IsIdle())
		{
			if (!rOp.Pending())
			{
				if (rOp.Succeeded())
				{
					ProcessOperationSucceeded(rOp);
				}
				else if (rOp.Failed())
				{
					ProcessOperationFailed(rOp);
				}
				else if (rOp.Canceled())
				{
					ProcessOperationCanceled(rOp);
				}

				// Clear the operation only if it didnt need to be processed or if it has been processed
				// We dont clear the operation because it might need to process the success for more than one frame.
				if(! (rOp.Succeeded() && rOp.NeedProcessSucceed() && !rOp.HasBeenProcessed() ) )
				{
					rOp.Clear();
				}
			}

			return;
		}
	}
}


void
NetworkClan::OnClanEvent(const rlClanEvent& evt)
{
	switch (evt.GetId())
	{
		case RLCLAN_EVENT_SYNCHED_MEMBERSHIPS:
		case RLCLAN_EVENT_PRIMARY_CLAN_CHANGED:
		{
			const bool bIsServerSync = ( evt.GetId() == RLCLAN_EVENT_SYNCHED_MEMBERSHIPS );
			const int eventLocalGamerIdx = bIsServerSync ? static_cast<const rlClanEventSynchedMemberships*>(&evt)->m_LocalGamerIndex : static_cast<const rlClanEventPrimaryClanChanged*>(&evt)->m_LocalGamerIndex;

			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == eventLocalGamerIdx)
			{
				//When the local player's primary changes, we need to release emblem for the old primary, an request the new one.
				if (m_LocalPlayerDesc.IsValid() && m_EmblemRefd!=RL_INVALID_CLAN_ID)
				{
					gnetDebug1("Primary Changed - releasing ref for emblem to crew id %d", (int)m_EmblemRefd);
					ReleaseEmblemReference(m_EmblemRefd  ASSERT_ONLY(, "NetworkClan"));
					m_EmblemRefd=RL_INVALID_CLAN_ID;
				}

				bool bHasPrimaryClan = false;
				if (rlClan::HasPrimaryClan(eventLocalGamerIdx))
				{
					gnetDebug1("Set Primary clan, eventLocalGamerIdx='%d'", eventLocalGamerIdx);
					bHasPrimaryClan = true;

					const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(eventLocalGamerIdx);
					CopyClanDesc(clanDesc, m_LocalPlayerDesc);
					PrintClanDesc(m_LocalPlayerDesc);

					if (StatsInterface::IsKeyValid(STAT_ACTIVE_CREW))
					{
						StatsInterface::SetStatData(STAT_ACTIVE_CREW, (u64)clanDesc.m_Id, STATUPDATEFLAG_ASSERTONLINESTATS);
					}
					else
					{
						m_pendingTimerSetActiveCrew = sysTimer::GetSystemMsTime() + TIMER_TO_RESET_STAT_ACTIVE_CREW;
					}

					const rlClanMembershipData& memberData = rlClan::GetPrimaryMembership(eventLocalGamerIdx);
					if(memberData.IsValid())
					{
						m_LocalPlayerMembership.AddActiveClanMembershipData(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), memberData);
						m_LocalPlayerMembership.PrintMembershipData();
					}

					m_primaryCrewMetadata.SetClanId(clanDesc.m_Id, true);
					m_bRequestMaintenanceJoinRequestRefresh = true;
				}
				else //No longer have a valid primary crew (i.e. we just left our last one.)
				{
					gnetDebug1("No longer have a valid primary crew (i.e. we just left our last one.)");

					m_LocalPlayerDesc.Clear();

					if (StatsInterface::IsKeyValid(STAT_ACTIVE_CREW))
					{
						StatsInterface::SetStatData(STAT_ACTIVE_CREW, (u64)RL_INVALID_CLAN_ID, STATUPDATEFLAG_ASSERTONLINESTATS);
					}
					else
					{
						m_pendingTimerSetActiveCrew = sysTimer::GetSystemMsTime() + TIMER_TO_RESET_STAT_ACTIVE_CREW;
					}

					m_primaryCrewMetadata.Clear();
				}
				
				//Request the emblem for the new one.
				if (m_LocalPlayerDesc.IsValid())
				{
					gnetDebug1("Requesting emblem to crew id %d", (int)m_LocalPlayerDesc.m_Id);
					if(RequestEmblemForClan(m_LocalPlayerDesc.m_Id ASSERT_ONLY(, "NetworkClan")))
					{
						m_EmblemRefd=m_LocalPlayerDesc.m_Id;
					}
				}

				if(!bIsServerSync || (bHasPrimaryClan && CLiveManager::GetSocialClubMgr().IsLocalInitialLink()))
				{
					gnetDebug1("NetworkClan::OnClanEvent-->invoke CEventNetworkPrimaryClanChanged");
					GetEventScriptNetworkGroup()->Add(CEventNetworkPrimaryClanChanged());
				}

				CLiveManager::GetSocialClubMgr().ResetLocalInitialLink();
			}
		}
		break;

		case RLCLAN_EVENT_JOINED:
		{
			const rlClanEventJoined* e = static_cast<const rlClanEventJoined*>(&evt);

			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
			{
				gnetDebug1("Set Primary clan, eventLocalGamerIdx='%d'", e->m_LocalGamerIndex);

				const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(e->m_LocalGamerIndex);
				gnetAssert(clanDesc.IsValid());
				CopyClanDesc(clanDesc, m_LocalPlayerDesc);
				PrintClanDesc(m_LocalPlayerDesc);

				const rlClanMembershipData& memberData = rlClan::GetPrimaryMembership(e->m_LocalGamerIndex);
				gnetAssert(memberData.IsValid());
				m_LocalPlayerMembership.AddActiveClanMembershipData(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), memberData);
				m_LocalPlayerMembership.PrintMembershipData();

				GetEventScriptNetworkGroup()->Add(CEventNetworkClanJoined());

				// check if we joined a clan we had an invite for. if so, refresh
				for(int inv=0; inv<m_InvitesReceivedCount; ++inv)
				{
					if( m_InvitesReceived[inv].m_Clan.m_Id == e->m_ClanDesc.m_Id )
					{
						RefreshInvitesReceived();
						break;
					}
				}

				m_primaryCrewMetadata.SetClanId(clanDesc.m_Id, true);
			}
		}
		break;

		case RLCLAN_EVENT_KICKED:
		{
			const rlClanEventKicked* e = static_cast<const rlClanEventKicked*>(&evt);

			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
			{
				bool bPrimary = m_LocalPlayerDesc.m_Id == e->m_ClanId;
				GetEventScriptNetworkGroup()->Add(CEventNetworkClanKicked(e->m_ClanId, bPrimary));
				gnetDebug1("Received rlClanEventKicked for local gamer from crew %d %s", (int)e->m_ClanId, bPrimary ? "[Primary]" : "");
			}
		}
		break;

		case RLCLAN_EVENT_LEFT:
		{
			const rlClanEventLeft* e = static_cast<const rlClanEventLeft*>(&evt);
			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
			{
				bool bPrimary = rlClan::HasPrimaryClan(e->m_LocalGamerIndex) && rlClan::GetPrimaryClan(e->m_LocalGamerIndex).m_Id == e->m_ClanId;
				
				if (e->m_ClanId == m_LocalPlayerDesc.m_Id)
				{
					m_LocalPlayerDesc.Clear();
				}
				m_LocalPlayerMembership.Reset(e->m_ClanId);
				m_LocalPlayerMembership.PrintMembershipData();

				GetEventScriptNetworkGroup()->Add(CEventNetworkClanLeft(e->m_ClanId, bPrimary));
			}
		}
		break;

		case RLCLAN_EVENT_INVITE_RECEIVED:
		{
			const rlClanEventInviteRecieved* e = static_cast<const rlClanEventInviteRecieved*>(&evt);

			if(NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
			{
				gnetDebug1("Received rlClanEventInviteRecieved for active local gamer");
				if (rlClan::IsFullServiceReady(e->m_LocalGamerIndex))
				{
					// this is a little clumsy because there is no way to match this specific invite to the invites returned by rlClan::GetInvites
					// we only have the clan Id (ideally, we would get the inviteId or the inviter)
					// to manage this, take a snapshot of the current invites we know about so that we can check for new ones when the get completes
					// this is needed so that we recognize different invites to the same clan Id (i.e. a previous invite from an unblocked player)
					m_PreviousInvitesReceivedCount = m_InvitesReceivedCount;
					for(u32 i = 0; i < m_PreviousInvitesReceivedCount; i++)
						m_PreviousInvitesReceived[i] = m_InvitesReceived[i];
					
					// add this event to our pending list (we need to cache some of the information here that isn't present in the later event)
					m_PendingReceivedInvites.Push(PendingReceivedInvite(e->m_ClanId, e->m_RankName, e->m_Message));

					RefreshInvitesReceived();
				}
			}
			else
			{
				gnetDebug1("Event RLCLAN_EVENT_INVITE_RECEIVED not dealt with. Not for active local gamer");
			}
		} 
		break;

		case RLCLAN_EVENT_FRIEND_JOINED:
		{
			const rlClanEventFriendJoined* e = static_cast<const rlClanEventFriendJoined*>(&evt);
			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
			{
				NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_FRIEND_INFO_EVENT);
				if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
				{
#if !__NO_OUTPUT
					char szToString[RL_MAX_GAMER_HANDLE_CHARS];
					gnetDebug1("Start NetworkClanOp_FriendEventData - RLCLAN_EVENT_FRIEND_JOINED - For Gamer %s", e->m_friendGamerHandle.ToString(szToString));
#endif // !__NO_OUTPUT

					//Create a new opData to handle this stuff
					NetworkClanOp_FriendEventData* pData = 
						rage_new NetworkClanOp_FriendEventData(NetworkClanOp_FriendEventData::JOINED, e->m_friendGamerHandle, e->m_ClanId);
					pNetClanOp->SetOpData(pData);
					pNetClanOp->SetNeedProcessSucceed();

					//Now start operation to get the clanDesc
					rlClan::GetDesc(e->m_LocalGamerIndex, e->m_ClanId, &pData->m_clanDesc, pNetClanOp->GetStatus());
				}
			}
		}
		break;
		case RLCLAN_EVENT_FRIEND_FOUNDED:
		{
			const rlClanEventFriendFounded* e = static_cast<const rlClanEventFriendFounded*>(&evt);
			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
			{
				NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_FRIEND_INFO_EVENT);
				if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
				{
#if !__NO_OUTPUT
					char szToString[RL_MAX_GAMER_HANDLE_CHARS];
					gnetDebug1("Start NetworkClanOp_FriendEventData - RLCLAN_EVENT_FRIEND_FOUNDED - For Gamer %s", e->m_friendGamerHandle.ToString(szToString));
#endif // !__NO_OUTPUT

					//Create a new opData to handle this stuff
					NetworkClanOp_FriendEventData* pData = 
						rage_new NetworkClanOp_FriendEventData(NetworkClanOp_FriendEventData::FOUNDED, e->m_friendGamerHandle, e->m_ClanId);
					pNetClanOp->SetOpData(pData);
					pNetClanOp->SetNeedProcessSucceed();

					//Now start operation to get the clanDesc
					rlClan::GetDesc(e->m_LocalGamerIndex, e->m_ClanId, &pData->m_clanDesc, pNetClanOp->GetStatus());
				}
			}
		}
		break;

 		case RLCLAN_EVENT_MEMBER_RANK_CHANGED:
 		{
 			const rlClanEventMemberRankChange* e = static_cast<const rlClanEventMemberRankChange*>(&evt);
 			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == e->m_LocalGamerIndex)
 			{
 				//Send an event to script...at some point.
				GetEventScriptNetworkGroup()->Add(CEventNetworkClanRankChanged(e->m_ClanId,e->m_rankOrder, e->m_rankName,e->m_bPromotion));
 			}
 		}
 		break;

		case RLCLAN_EVENT_NOTIFY_JOIN_REQUEST:
		{
			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == static_cast<const rlClanEventNotifyJoinRequest*>(&evt)->m_LocalGamerIndex)
			{
				m_bRequestMaintenanceJoinRequestRefresh = true;
			}
		}
		break;
	}
}

bool 
NetworkClan::IsIdle() const
{
	for (int i = 0; i < MAX_NUM_CLAN_OPS; ++i)
	{
		if (!m_OpPool[i].IsIdle())
		{
			return false;
		}
	}

	return true;
}

void 
NetworkClan::HandleClanCreatedSuccess()
{
}

void
NetworkClan::HandleClanMembershipRefreshSuccess()
{
	if (0 < m_LocalPlayerMembership.m_Count)
	{
		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		if (rlClan::HasPrimaryClan(localGamerIndex))
		{
			const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(localGamerIndex);
			gnetAssert(clanDesc.IsValid());

			bool found = false;
			for (u32 i=0; i<m_LocalPlayerMembership.m_Count && !found; i++)
			{
				if (m_LocalPlayerMembership.m_Data[i].m_Clan.m_Id == clanDesc.m_Id)
				{
					m_LocalPlayerMembership.SetActiveClanMembership(i);
					found = true;
				}
			}

#if __ASSERT
			if (gnetVerify(found) && gnetVerify(m_LocalPlayerMembership.GetActive()))
			{
				const rlClanMembershipData& memberData = rlClan::GetPrimaryMembership(localGamerIndex);
				gnetAssert(memberData.IsValid());
				gnetAssert(m_LocalPlayerMembership.GetActive()->IsValid());
				gnetAssert(m_LocalPlayerMembership.GetActive()->m_Clan.m_Id == memberData.m_Clan.m_Id);
			}
#endif // __ASSERT
		}
	}
}

void  
NetworkClan::ProcessOperationSucceeded(NetworkClanOp& op)
{
	gnetDebug1(".... \"%s\" Succeeded.", GetOperationName(op.GetOp()));

	switch(op.GetOp())
	{
	case OP_NONE:
		break;
	case OP_CREATE_CLAN:
		HandleClanCreatedSuccess();
		BANK_ONLY(m_bank_UpdateClanDetailsRequested = true);
		break;
	case OP_LEAVE_CLAN:
		m_LocalPlayerMembership.ClearActive();
		BANK_ONLY(m_bank_UpdateClanDetailsRequested = true);
		break;
	case OP_KICK_CLAN_MEMBER:
		break;
	case OP_INVITE_PLAYER:
        m_InvitesSentCount = 0;
		break;
	case OP_REJECT_INVITE:
	case OP_ACCEPT_INVITE:
		{
			gnetDebug1("Requesting invite refresh due to invite acceptance or reject");
			m_bRequestMaintenanceInviteRefresh = true;
		}
		break;
	case OP_CANCEL_INVITE:
		break;
	case OP_ACCEPT_REQUEST:
		{
			if( gnetVerifyf( op.GetOpData(), "An OP_ACCEPT_REQUEST was successful, but didn't have the data we wanted on it?!" ) )
			{
				NetworkClanOp_InviteRequestData* pData = verify_cast<NetworkClanOp_InviteRequestData*>(op.GetOpData());
				if( pData )
				{
					u32 RequestIndex = FindReceivedJoinRequestIndex(pData->m_Id);
					if( RequestIndex != ~0u )
						RejectRequest(RequestIndex);
				}
			}
		}
		break;
	case OP_REJECT_REQUEST:
		m_bRequestMaintenanceJoinRequestRefresh = true;
		break;
	case OP_JOIN_OPEN_CLAN:
		break;
    case OP_RANK_CREATE:
        m_RanksCount = 0;
        break;
    case OP_RANK_DELETE:
        m_RanksCount = 0;
        break;
    case OP_RANK_UPDATE:
        m_RanksCount = 0;
        break;
    case OP_MEMBER_UPDATE_RANK:
        m_MembersCount = 0;
        break;
    case OP_REFRESH_RANKS:
        break;

	case OP_SET_PRIMARY_CLAN:
		{
			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			if (gnetVerify(rlClan::HasPrimaryClan(localGamerIndex)) && gnetVerify(NetworkInterface::GetActiveGamerInfo()))
			{
				const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(localGamerIndex);
				gnetAssert(clanDesc.IsValid());
				CopyClanDesc(clanDesc, m_LocalPlayerDesc);
				PrintClanDesc(m_LocalPlayerDesc);

				const rlClanMembershipData& memberData = rlClan::GetPrimaryMembership(localGamerIndex);
				gnetAssert(memberData.IsValid());
				m_LocalPlayerMembership.AddActiveClanMembershipData(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), memberData);
				m_LocalPlayerMembership.PrintMembershipData();

				m_primaryCrewMetadata.SetClanId(clanDesc.m_Id, true);
			}
			BANK_ONLY(m_bank_UpdateClanDetailsRequested = true);
		}
		break;

	case OP_REFRESH_MINE:
		{
			HandleClanMembershipRefreshSuccess();
			m_LocalPlayerMembership.PrintMembershipData();
			BANK_ONLY(m_bank_UpdateClanDetailsRequested = true);
		}
		break;

    case OP_REFRESH_MEMBERSHIP_FOR:
		{
			m_Membership.UpdateActiveClanMembership();
			m_Membership.PrintMembershipData();
			op.SetProcessed();
		}
        break;

	case OP_REFRESH_INVITES_RECEIVED:
		{
			gnetDebug1("Currently have %d received invites", m_InvitesReceivedCount);

			// run round our invites to check if we need to cache or generate notification data
			// do this backwards so that we work through invites newest to oldest
			for(int i = static_cast<int>(m_InvitesReceivedCount) - 1; i >= 0; i--)
			{
				int pendingInviteToAction = -1;
				if(!m_PendingReceivedInvites.IsEmpty())
				{
					// check if we already knew about this invite...
					bool newInvite = true;
					for(u32 p = 0; p < m_PreviousInvitesReceivedCount; p++)
					{
						if(m_PreviousInvitesReceived[p].m_Id == m_InvitesReceived[i].m_Id)
						{
							newInvite = false;
							break;
						}
					}

					// if we didn't, check our pending list...
					if(newInvite)
					{
						unsigned pendingCount = static_cast<u32>(m_PendingReceivedInvites.GetCount());
						for(u32 p = 0; p < pendingCount; p++)
						{
							if(m_PendingReceivedInvites[p].m_ClanId == m_InvitesReceived[i].m_Clan.m_Id)
							{
								gnetDebug1("Received invite to %s [%" I64FMT "d] from %s, Id: [%" I64FMT "d]",
									m_InvitesReceived[i].m_Clan.m_ClanName,
									m_InvitesReceived[i].m_Clan.m_Id,
									m_InvitesReceived[i].m_Inviter.m_GamerHandle.ToString(),
									m_InvitesReceived[i].m_Id);

								// flag that this is an invite that needs actioned
								pendingInviteToAction = p;
								break;
							}
						}
					}
				}
					
				// check if we are allowed to received this invite...
				if(CanReceiveInvite(m_InvitesReceived[i].m_Inviter.m_GamerHandle))
				{
					// if this has a pending action
					if(pendingInviteToAction >= 0)
					{
						bool bHasMessage = strlen(m_PendingReceivedInvites[pendingInviteToAction].m_Message) > 0;
						GetEventScriptNetworkGroup()->Add(CEventNetworkClanInviteReceived(
							m_InvitesReceived[i].m_Clan.m_Id,
							m_InvitesReceived[i].m_Clan.m_ClanName,
							m_InvitesReceived[i].m_Clan.m_ClanTag,
							m_PendingReceivedInvites[pendingInviteToAction].m_RankName,
							bHasMessage));
					}
				}
				else
				{
					// for all invites (pending received or not), reject the invite so that it's removed from our lists
					gnetDebug1("Rejecting invite to %s [%" I64FMT "d] from %s, Id: [%" I64FMT "d]",
						m_InvitesReceived[i].m_Clan.m_ClanName,
						m_InvitesReceived[i].m_Clan.m_Id,
						m_InvitesReceived[i].m_Inviter.m_GamerHandle.ToString(),
						m_InvitesReceived[i].m_Id);

					RejectInvite(i);
				}

				// in either case, remove our actionable invite
				if(pendingInviteToAction >= 0)
				{
					m_PendingReceivedInvites.Delete(pendingInviteToAction);
				}
			}
		}
		break;
	case OP_REFRESH_INVITES_SENT:
		break;
	case OP_REFRESH_MEMBERS:
		{
			//Count the number of players in a clan
			int playerClanCount = 0;
			if (NetworkInterface::IsNetworkOpen() && NetworkInterface::GetNumActivePlayers() > 0)
			{
				//Need to iterate over the entire active player list because the list may not be contiguous.
				for(ActivePlayerIndex i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
				{
					const CNetGamePlayer* pPlayer = NetworkInterface::GetActivePlayerFromIndex(i);
					if (pPlayer && pPlayer->GetClanDesc().IsValid())
					{
						playerClanCount++;
					}
				}
			}
			else //If we're not in networking or we don't have an active (local) net player yet, do the non-networking one player way.
			{
				int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
				if(rlClan::HasPrimaryClan(localGamerIndex))
				{
					playerClanCount = 1;
				}
			}
		}
		break;
    case OP_REFRESH_OPEN_CLANS:
		BANK_ONLY(m_bank_UpdateClanDetailsRequested = true);
		break;
	case OP_CLAN_SEARCH:
#if __BANK
		{
			gnetDebug1("Dumping clans from refresh");
			for (int i=0; i < m_OpenClanCount; ++i )
			{
				PrintClanDesc(m_OpenClan[i]);
			}
			BANK_ONLY(m_bank_UpdateClanDetailsRequested = true);
		}
#endif
        break;
	case OP_CLAN_METADATA:
#if __BANK
		{
			gnetDebug1("Retrieved %d of %d metaData", m_Bank_MetaDatareturnCount, m_Bank_MetaDatatotalCount);
			for (int i = 0; i< m_Bank_MetaDatareturnCount; ++i)
			{
				gnetDebug1("%d: %s - %s", i, m_aBankMetaDataEnum[i].m_SetName, m_aBankMetaDataEnum[i].m_EnumName );
			}
		}
#endif
		break;
    case OP_REFRESH_WALL:
        break;
	case OP_WRITE_WALL_MESSAGE:
		break;
	case OP_DELETE_WALL_MESSAGE:
		break;
	case OP_FRIEND_INFO_EVENT:
		{
			NetworkClanOp_FriendEventData* pData = static_cast<NetworkClanOp_FriendEventData*>(op.GetOpData());
			if (pData)
			{
				// Start the name request for the friend event
				if(pData->m_gamertagRequestStatus.None())
				{
					const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
					if(!rlFriendsManager::IsFriendsWith(localGamerIndex, pData->m_gh) ||
					   !rlFriendsManager::LookupFriendsName(localGamerIndex, &pData->m_gh, 1, &pData->m_displayName, &pData->m_gamertag, &pData->m_gamertagRequestStatus))
					{
						pData->m_gamertagRequestStatus.ForceFailed();
					}
				}
				// If we're done and successful
				if(pData->m_gamertagRequestStatus.Succeeded())
				{
					pData->HandleSuccess();
					op.SetProcessed();
				}
				// otherwise ...
				if(pData->m_gamertagRequestStatus.Failed() || pData->m_gamertagRequestStatus.Canceled())
				{
					op.SetProcessed();
				}
			}
		}	
		break;

	case OP_REFRESH_JOIN_REQUESTS_RECEIVED:
		{
			if(m_JoinRequestsReceivedCount>0)
			{
				// Send an event if we're still in the same clan we requested the invite request for.
				NetworkClanOp_InviteRequestData* pData = static_cast<NetworkClanOp_InviteRequestData*>(op.GetOpData());
				if(pData && m_LocalPlayerDesc.m_Id == (rlClanId)pData->m_Id)
				{
					GetEventScriptNetworkGroup()->Add(CEventNetworkClanInviteRequestReceived( (rlClanId) pData->m_Id));
				}
			}
		}
		break;
	}

	DispatchEvent(op);
}

void  
NetworkClan::ProcessOperationFailed(NetworkClanOp& op)
{
	gnetWarning(".... \"%s\" Failed.", GetOperationName(op.GetOp()));
	gnetWarning(".... Reason: \"%s\"", GetErrorName(op.GetStatusRef()));

	switch(op.GetOp())
	{
	case OP_REJECT_INVITE:
	case OP_ACCEPT_INVITE:
		{
			switch(op.GetStatusRef().GetResultCode())
			{
			case RL_CLAN_ERR_INVITE_DOES_NOT_EXIST:
			case RL_CLAN_ERR_INVITE_PRIVATE:
			case RL_CLAN_ERR_NOT_FOUND:
				gnetDebug1("Requesting invite refresh due to invite acceptance or reject");
				m_bRequestMaintenanceInviteRefresh = true;
				break;
			default:
				// nothing;
				break;
			}
		}
		break;

	default:
		break;
	}

	DispatchEvent(op);
}

void  
NetworkClan::ProcessOperationCanceled(NetworkClanOp& op)
{
	gnetWarning(".... \"%s\" Canceled.", GetOperationName(op.GetOp()));

	DispatchEvent(op);
}

const char*  
NetworkClan::GetErrorName(const netStatus& thisReference)
{
	if (gnetVerify(thisReference.Failed()))
	{
		switch (thisReference.GetResultCode())
		{
		case RL_CLAN_SUCCESS:                            return "RL_CLAN_UNKNOWN";//"CLAN_SUCCESS"; switching to unknown to handle lower-level parsing/server unavailablity issues
		case RL_CLAN_ERR_NAME_EXISTS:                    return "CLAN_ERR_NAME_EXISTS";
		case RL_CLAN_ERR_TAG_EXISTS:                     return "CLAN_ERR_TAG_EXISTS";
		case RL_CLAN_ERR_ALREADY_IN_CLAN:                return "CLAN_ERR_ALREADY_IN_CLAN";
		case RL_CLAN_ERR_PLAYER_NOT_FOUND:               return "CLAN_ERR_PLAYER_NOT_FOUND";
		case RL_CLAN_ERR_PERMISSION_ERROR:               return "CLAN_ERR_PERMISSION_ERROR";
		case RL_CLAN_ERR_INVITE_EXISTS:                  return "CLAN_ERR_INVITE_EXISTS";
		case RL_CLAN_ERR_INVITE_MAX_SENT_COUNT_EXCEEDED: return "CLAN_ERR_INVITE_MAX_SENT_COUNT_EXCEEDED";
		case RL_CLAN_ERR_INVITE_DOES_NOT_EXIST:          return "CLAN_ERR_INVITE_DOES_NOT_EXIST";
        case RL_CLAN_ERR_INVITE_PRIVATE:                 return "CLAN_ERR_INVITE_PRIVATE";
		case RL_CLAN_ERR_NOT_FOUND:                      return "CLAN_ERR_NOT_FOUND";
	
        case RL_CLAN_ERR_ROCKSTAR_ID_DOES_NOT_EXIST:     return "CLAN_ERR_RSTAR_ID_NOT_EXIST";
        case RL_CLAN_ERR_SC_NICKNAME_DOES_NOT_EXIST:     return "CLAN_ERR_SC_NICK_NOT_EXIST";
		case RL_CLAN_ERR_CLAN_MAX_JOIN_COUNT_EXCEEDED:	 return "CLAN_ERR_CLAN_MAX_JOIN_COUNT_EXCEEDED";
		case RL_CLAN_ERR_CLAN_MAX_MEMBER_COUNT_EXCEEDED: return "CLAN_ERR_MAX_MEMBER_COUNT_EXCEED";
		case RL_CLAN_ERR_RATE_LIMIT_EXCEEDED:			 return "CLAN_ERR_RATE_LIMIT_EXCEEDED";
		case RL_CLAN_ERR_NO_PRIVILEGE:					 return "CLAN_ERR_NO_PRIVILEGE";

		case RL_CLAN_ERR_PARSER_FAILED:					 return "CLAN_ERR_PARSER_FAILED";
		case RL_CLAN_ERR_CLAN_PERMISSION_ERROR:			 return "CLAN_ERR_CLAN_PERMISSION_ERROR";
        case RL_CLAN_ERR_PLAYER_BANNED:			         return "CLAN_ERR_PLAYER_BANNED";
		case RL_CLAN_ERR_UNKNOWN:						 return "RL_CLAN_UNKNOWN"; // making this explicit so it appears we're handling it
		}

		return "RL_CLAN_UNKNOWN";
	}

	return "CLAN_NOT_FAILED";
}

//Clan

bool  
NetworkClan::ServiceIsValid() const
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	return rlClan::IsServiceReady(localGamerIndex);
}

bool  
NetworkClan::CreateClan(const char* clanName, const char* clanTag, const bool isOpenClan)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& gnetVerify(clanName)
		&& gnetVerify(strlen(clanName) <= RL_CLAN_NAME_MAX_CHARS)
		&& gnetVerify(clanTag)
		&& gnetVerify(strlen(clanTag) <= RL_CLAN_TAG_MAX_CHARS))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_CREATE_CLAN);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::Create(localGamerIndex, clanName, clanTag, isOpenClan, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool
NetworkClan::SetPrimaryClan(const rlClanId clan)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_SET_PRIMARY_CLAN);
	if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
	{
		result = rlClan::SetPrimaryClan(localGamerIndex, clan, pNetClanOp->GetStatus());
		gnetAssert(result);
	}

	return result;
};

bool  
NetworkClan::LeaveClan()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_LEAVE_CLAN);
	if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one")
		&& gnetVerify(HasPrimaryClan()) )
    {
        result = rlClan::Leave(localGamerIndex, m_LocalPlayerDesc.m_Id, pNetClanOp->GetStatus());
        gnetAssert(result);
	}

	return result;
}


int 
NetworkClan::GetLocalGamerMembershipCount()
{
	//Use the instant version of GetMine to update
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (rlClan::IsFullServiceReady(localGamerIndex))
	{
		//Refresh the local memberhips counts using the fast version
		if(RefreshLocalMemberships(true))
			return m_LocalPlayerMembership.m_Count;
		else
		{
			gnetError("Request crew membership count before crew memberships have been sync'd");
			return -1;
		}
	}
	
	return 0;
}


u32
NetworkClan::GetMembershipCount(const rlGamerHandle& gamerHandle) const
{
	u32 result = 0;

	if (gnetVerify(gamerHandle.IsValid()))
	{
		if (gamerHandle.IsLocal())
		{
			result = m_LocalPlayerMembership.m_Count;
		}
		else
		{
			gnetAssertf(m_Membership.m_GamerHandle == gamerHandle, "Retrieving membership count from the wrong remote guy");
			result = m_Membership.m_Count;
		}
	}

	return result;
}


bool 
NetworkClan::IsActiveMembershipValid() const
{
	return m_Membership.ActiveIsValid();
}


const rlClanMembershipData*  
NetworkClan::GetMembership(const rlGamerHandle& gamerHandle, const u32 index) const 
{
	if (gnetVerify(gamerHandle.IsValid()))
	{
		if (gamerHandle.IsLocal())
		{
			if (gnetVerify(index < m_LocalPlayerMembership.m_Count) && gnetVerify(m_LocalPlayerMembership.IsValid(index)))
			{
				return m_LocalPlayerMembership.Get(index);
			}
		}
		else
		{
			gnetAssertf(m_Membership.m_GamerHandle == gamerHandle, "Retrieving crew information from the wrong remote guy");
			// TU MAGIC: If index == -1 in a remote, we return the active clan instead
			if (index == (u32)-1 && gnetVerify(m_Membership.ActiveIsValid()))
			{
				return m_Membership.GetActive();
			}
			else if (gnetVerify(index < m_Membership.m_Count) && gnetVerify(m_Membership.IsValid(index)))
			{
				return m_Membership.Get(index);
			}
		}
	}

    return NULL;
}


const rlClanMembershipData* 
NetworkClan::GetLocalMembershipDataByClanId( rlClanId clanId ) const
{
	for (int i = 0; i < m_LocalPlayerMembership.m_Count; ++i)
	{
		const rlClanMembershipData* pData = m_LocalPlayerMembership.Get(i);	
		if(pData && pData->m_Clan.m_Id == clanId)
			return pData;
	}

	return NULL;
}

bool
NetworkClan::HasPrimaryClan()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (!m_LocalPlayerDesc.IsValid() && rlClan::HasPrimaryClan(localGamerIndex) && gnetVerify(NetworkInterface::GetActiveGamerInfo()))
	{
		const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(localGamerIndex);
		gnetAssert(clanDesc.IsValid());
		CopyClanDesc(clanDesc, m_LocalPlayerDesc);
		PrintClanDesc(m_LocalPlayerDesc);

		const rlClanMembershipData& memberData = rlClan::GetPrimaryMembership(localGamerIndex);
		gnetAssert(memberData.IsValid());
		m_LocalPlayerMembership.AddActiveClanMembershipData(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), memberData);
		m_LocalPlayerMembership.PrintMembershipData();

		m_primaryCrewMetadata.SetClanId(clanDesc.m_Id, true);
	}

	return m_LocalPlayerDesc.IsValid(); 
}

bool
NetworkClan::HasPrimaryClan(const rlGamerHandle& gamerHandle)
{
	bool exists = false;

	if (gamerHandle.IsValid())
	{
		if (gamerHandle.IsLocal())
		{
			gnetAssert(gamerHandle.GetLocalIndex() == NetworkInterface::GetLocalGamerIndex());

			if (!m_LocalPlayerDesc.IsValid() && rlClan::HasPrimaryClan(gamerHandle.GetLocalIndex()) && gnetVerify(NetworkInterface::GetActiveGamerInfo()))
			{
				const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(gamerHandle.GetLocalIndex());
				gnetAssert(clanDesc.IsValid());
				CopyClanDesc(clanDesc, m_LocalPlayerDesc);
				PrintClanDesc(m_LocalPlayerDesc);

				const rlClanMembershipData& memberData = rlClan::GetPrimaryMembership(gamerHandle.GetLocalIndex());
				gnetAssert(memberData.IsValid());
				m_LocalPlayerMembership.AddActiveClanMembershipData(NetworkInterface::GetActiveGamerInfo()->GetGamerHandle(), memberData);
				m_LocalPlayerMembership.PrintMembershipData();

				m_primaryCrewMetadata.SetClanId(clanDesc.m_Id, true);
			}

			exists = m_LocalPlayerDesc.IsValid();
		}
		else
		{
			CNetGamePlayer *pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
			if (pPlayer)
			{
				exists = pPlayer->GetClanMembershipInfo().IsValid();
			}
		}
	}

	return exists;
}

const rlClanDesc*
NetworkClan::GetPrimaryClan(const rlGamerHandle& gamerHandle) const
{
	const rlClanDesc* desc = NULL;

	if (gamerHandle.IsValid())
	{
		if (gamerHandle.IsLocal())
		{
			desc = &(m_LocalPlayerDesc);
		}
		else
		{
			CNetGamePlayer *pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
			if (pPlayer)
			{
				desc = &(pPlayer->GetClanDesc());
			}
		}

		gnetAssert(desc);
	}

	return desc;
}

const rlClanMembershipData*
NetworkClan::GetPrimaryClanMembershipData(const rlGamerHandle& gamerHandle) const
{
	const rlClanMembershipData* desc = NULL;

	if (gamerHandle.IsValid())
	{
		if (gamerHandle.IsLocal())
		{
			desc = m_LocalPlayerMembership.GetActive();
		}
		else
		{
			CNetGamePlayer *pPlayer = NetworkInterface::GetPlayerFromGamerHandle(gamerHandle);
			if (pPlayer)
			{
				desc = &(pPlayer->GetClanMembershipInfo());
			}
		}

		gnetAssert(desc);
	}

	return desc;
}

bool
NetworkClan::RefreshLocalMemberships(bool forceInstant /*false*/)
{
	bool result = false;
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsFullServiceReady(localGamerIndex)))
	{
		rlGamerHandle gamerHandle;
		if(!gnetVerify(rlPresence::GetGamerHandle(localGamerIndex, &gamerHandle)))
		{
			return false;
		}

		m_LocalPlayerMembership.Clear();
		m_LocalPlayerMembership.SetGamerHandle(gamerHandle);

		//Use the instant version  first since we cache this stuff in the inards of the rlClan 
		result = rlClan::GetMine(localGamerIndex
				,&m_LocalPlayerMembership.m_Data[0]
				,MAX_NUM_CLAN_MEMBERSHIPS
				,&m_LocalPlayerMembership.m_Count
				,&m_LocalPlayerMembership.m_TotalCount
				,NULL);

		//If the instant way worked, handle the callback functionality.
		if(result)
		{
			HandleClanMembershipRefreshSuccess();
		}
		//if the instant version didn't work, do it the long way, unless we explicitly don't want to.
		else if(!forceInstant)
		{
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_MINE);
			if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
			{
				result = rlClan::GetMine(localGamerIndex
					,&m_LocalPlayerMembership.m_Data[0]
					,MAX_NUM_CLAN_MEMBERSHIPS
					,&m_LocalPlayerMembership.m_Count
					,&m_LocalPlayerMembership.m_TotalCount
					,pNetClanOp->GetStatus());

				gnetAssert(result);
			}
		}
	}

	return result;
}

bool      
NetworkClan::RefreshMemberships(const rlGamerHandle& gamerHandle, const int pageIndex)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
    {
		if (gnetVerify(gamerHandle.IsValid()))
		{
			if (gamerHandle.IsLocal())
			{
				result = RefreshLocalMemberships();
			}
			else
			{
				if(m_Membership.m_GamerHandle == gamerHandle || gnetVerifyf(!CheckForAnyOpsOfType(OP_REFRESH_MEMBERSHIP_FOR), "Tried to refresh memberships of remote while other remote's are being requested"))
				{
#if __BANK
					if(HasMembershipsReadyOrRequested(gamerHandle) && !RefreshMembershipsPending(gamerHandle)) 
					{ 
						gnetDebug3("Re-downloading already requested remote player's membership data. Is this intended?"); 
					}
#endif

					m_Membership.Clear();
					m_Membership.SetGamerHandle(gamerHandle);

					NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_MEMBERSHIP_FOR);
					pNetClanOp->SetNeedProcessSucceed();
					if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
					{
						result = rlClan::GetMembershipFor(localGamerIndex
															,pageIndex
															,m_Membership.m_GamerHandle
															,&m_Membership.m_Data[0]
															,MAX_NUM_CLAN_MEMBERSHIPS
															,&m_Membership.m_Count
															,&m_Membership.m_TotalCount
															,pNetClanOp->GetStatus());
					}
				}
			}

			gnetAssertf(result, "There's been an error in RefreshMemberships. Please check for any error output in the logs previous to this assert.");
		}
    }

    return result;
}

bool
NetworkClan::RefreshMembershipsPending(const rlGamerHandle& gamerHandle)
{
	gnetAssertf(gamerHandle.IsValid(), "Invalid gamer handle - gamerHandle");

	if (m_Membership.m_GamerHandle == gamerHandle)
	{
		return CheckForAnyOpsOfType(OP_REFRESH_MEMBERSHIP_FOR);
	}
	return false;
}

const rlClanRank*  
NetworkClan::GetRank(const u32 rank) const 
{
	if (gnetVerify(rank < m_RanksCount)
        && gnetVerify(m_Rank[rank].IsValid()))
    {
        return reinterpret_cast<const rlClanRank *>(&m_Rank[rank]);
    }

    return NULL;
}

bool      
NetworkClan::RefreshRanks()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)) && gnetVerify(HasPrimaryClan()))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_RANKS);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::GetRanks(localGamerIndex
										,0
										,m_LocalPlayerDesc.m_Id
										,&m_Rank[0]
										,MAX_NUM_CLAN_RANKS
										,&m_RanksCount
										,&m_RanksTotalCount
										,pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool      
NetworkClan::NextRanks()
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)) && gnetVerify(HasPrimaryClan())
			&& gnetVerify(m_RanksCount > 0)
			&& gnetVerify(m_RanksCount <= MAX_NUM_CLAN_RANKS))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_MINE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
        {
            result = rlClan::GetRanks(localGamerIndex
										,m_RanksCount - 1
										,m_LocalPlayerDesc.m_Id
										,&m_Rank[0]
										,MAX_NUM_CLAN_RANKS
										,&m_RanksCount
										,&m_RanksTotalCount
										,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool      
NetworkClan::CreateRank(const char* rankName, s64 systemFlags)
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)) && gnetVerify(HasPrimaryClan()))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_RANK_CREATE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::RankCreate(localGamerIndex, m_LocalPlayerDesc.m_Id, rankName, systemFlags, pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool      
NetworkClan::DeleteRank(const u32 rank)
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)) && gnetVerify(HasPrimaryClan())
			&& gnetVerify(rank < m_RanksCount))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_RANK_DELETE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
        {
			result = rlClan::RankDelete(localGamerIndex, m_LocalPlayerDesc.m_Id, m_Rank[rank].m_Id, pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool      
NetworkClan::UpdateRank(const u32 rank, const char* rankName, s64 systemFlags, int adjustOrder)
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)) && gnetVerify(HasPrimaryClan())
			&& gnetVerifyf(rank < m_RanksCount, "rank=%u, RanksCount=%u", rank, m_RanksCount))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_RANK_UPDATE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
        {
			result = rlClan::RankUpdate(localGamerIndex
										,m_LocalPlayerDesc.m_Id
										,m_Rank[rank].m_Id
										,adjustOrder
										,rankName
										,systemFlags
										,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}


const rlClanDesc*  
NetworkClan::GetOpenClan(const u32 index) const 
{
	if (gnetVerify(index < m_OpenClanCount) && gnetVerify(m_OpenClan[index].IsValid()))
    {
        return reinterpret_cast<const rlClanDesc *>(&m_OpenClan[index]);
    }

    return NULL;
}

bool      
NetworkClan::RefreshOpenClans()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_OPEN_CLANS);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
		    result = rlClan::GetOpenClans(localGamerIndex,0
											,&m_OpenClan[0]
											,MAX_NUM_CLANS
											,&m_OpenClanCount
											,&m_OpenClanTotalCount
											,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}


bool      
NetworkClan::NextOpenClans()
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& gnetVerify(m_OpenClanCount > 0)
		&& gnetVerify(m_OpenClanCount <= MAX_NUM_CLANS)
		&& !CheckForAnyOpsOfType(OP_REFRESH_OPEN_CLANS))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_OPEN_CLANS);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
        {
             result = rlClan::GetOpenClans(localGamerIndex,0
											,&m_OpenClan[0]
											,MAX_NUM_CLANS
											,&m_OpenClanCount
											,&m_OpenClanTotalCount
											,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool NetworkClan::LocalGamerHasMembership( rlClanId clanId) const
{
	for (u32 i=0; i<m_LocalPlayerMembership.m_Count; i++)
	{
		if (m_LocalPlayerMembership.m_Data[i].m_Clan.m_Id == clanId)
		{
			return true;
		}
	}

	return false;
}

bool      
NetworkClan::JoinOpenClan(const u32 index)
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& gnetVerifyf(index < m_OpenClanCount, "index=%d, OpenClanCount=%d", index, m_OpenClanCount))
    {
		//Do a quick refresh of the local memberhips (force to be instant)
		RefreshLocalMemberships(true);

		//Check to see if the player is already a member of this clan
		if (LocalGamerHasMembership(m_OpenClan[index].m_Id))
		{
			gnetDisplay("Local gamer already has membership to crew %d.  Ignoring JoinCrew request", (int) m_OpenClan[index].m_Id);
			return true;
		}

		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_JOIN_OPEN_CLAN);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
        {
			result = rlClan::Join(localGamerIndex, m_OpenClan[index].m_Id, pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool NetworkClan::AttemptJoinClan(const rlClanId clanId)
{
	if(!HasPrimaryClan())
	{
		m_pendingJoinClanId = clanId;
		// If we don't have primary, then this new clan will become the primary and kick them from their session.  Let's show a confirmation
		// Script thinks this is easier than  just handling the warning screen themselves
		CWarningMessage::Data messageData;
		messageData.m_TextLabelBody = "CRW_JOINCONFIRM_NO_NAME";
		messageData.m_iFlags = FE_WARNING_YES_NO;
		messageData.m_acceptPressed = datCallback(MFA(NetworkClan::AcceptJoinPendingClan), (datBase*)this);
		messageData.m_declinePressed = datCallback(MFA(NetworkClan::DeclineJoinPendingClan), (datBase*)this);
		messageData.m_bCloseAfterPress = true;
		m_WarningMessage.SetMessage(messageData);
		return true; // Always return true since we now require a confirmation from code before we "actually" send the request to join the clan
	}
	else
	{
		// If we already have a primary, immediately join this new clan
		return JoinClan(clanId);
	} 
}

void NetworkClan::AcceptJoinPendingClan()
{
	// gnetVerify that the pending join clan id is valid.  This should never happen
	if(netVerify(m_pendingJoinClanId != -1))
	{
		JoinClan(m_pendingJoinClanId);
	}
}

void NetworkClan::DeclineJoinPendingClan()
{
	m_pendingJoinClanId = -1;
}

bool
NetworkClan::JoinClan(const rlClanId clanId)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_JOIN_OPEN_CLAN);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::Join(localGamerIndex, clanId, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

const rlClanJoinRequestRecieved* NetworkClan::GetReceivedJoinRequest( const u32 index ) const
{
	if (gnetVerify(index < m_JoinRequestsReceivedCount)
		&& gnetVerify(m_JoinRequestsReceived[index].IsValid()))
	{
		return &m_JoinRequestsReceived[index];
	}

	return NULL;
}

const u32 NetworkClan::FindReceivedJoinRequestIndex( const rlClanRequestId id ) const
{
	for(u32 i=0; i < m_JoinRequestsReceivedCount;++i)
	{
		if( m_JoinRequestsReceived[i].m_RequestId == id
			&& gnetVerify(m_JoinRequestsReceived[i].IsValid())
			)
			return i;
	}

	return ~0u;
}

bool NetworkClan::AcceptRequest(const u32 index)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if(gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& gnetVerify(HasPrimaryClan()) )
	{
		if( const rlClanJoinRequestRecieved* pRequest = GetReceivedJoinRequest(index) )
		{
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_ACCEPT_REQUEST);
			if( gnetVerifyf(pNetClanOp, "No Available Clan Ops to start a new one!") )
			{
				result = rlClan::Invite(localGamerIndex, m_LocalPlayerDesc.m_Id, -1, true, pRequest->m_Player.m_GamerHandle, pNetClanOp->GetStatus());
				if( result )
				{
					NetworkClanOp_InviteRequestData* pData = rage_new NetworkClanOp_InviteRequestData(pRequest->m_RequestId);
					pNetClanOp->SetOpData(pData);
				}
			}
		}
	}

	return result;
}

bool NetworkClan::RejectRequest(const u32 index)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if(gnetVerify(rlClan::IsServiceReady(localGamerIndex)) )
	{
		if( const rlClanJoinRequestRecieved* pRequest = GetReceivedJoinRequest(index) )
		{
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REJECT_REQUEST);
			if( gnetVerifyf(pNetClanOp, "No Available Clan Ops to start a new one!") )
			{
				result = rlClan::DeleteJoinRequest(localGamerIndex, pRequest->m_RequestId, pNetClanOp->GetStatus());
			}
		}
	}

	return result;
}

//Invites
const rlClanInvite*
NetworkClan::GetReceivedInvite(const u32 invite) const
{
	if (gnetVerify(invite < m_InvitesReceivedCount)
        && gnetVerify(m_InvitesReceived[invite].IsValid()))
    {
        return &m_InvitesReceived[invite];
	}

	return NULL;
}

const rlClanInvite*
NetworkClan::GetSentInvite(const u32 invite) const
{
	if (gnetVerify(invite < m_InvitesSentCount)
        && gnetVerify(m_InvitesSent[invite].IsValid()))
	{
		return &m_InvitesSent[invite];
	}

	return NULL;
}

bool
NetworkClan::CanSendInvite(const rlGamerHandle& PROSPERO_ONLY(rTarget))
{
#if RSG_PROSPERO
	// we can't communicate in either direction with blocked players
	if(CLiveManager::IsInBlockList(rTarget))
		return false;
#endif

	// no restrictions
	return true; 
}

bool
NetworkClan::CanReceiveInvite(const rlGamerHandle& PROSPERO_ONLY(rTarget))
{
#if RSG_PROSPERO
	// we can't communicate in either direction with blocked players
	if(CLiveManager::IsInBlockList(rTarget))
		return false;
#endif

	// no restrictions
	return true;
}

bool
NetworkClan::InviteGamer(const rlGamerHandle& rTarget)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if(CanSendInvite(rTarget)
		&& gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& gnetVerify(HasPrimaryClan())
		&& gnetVerify(rTarget.IsValid()))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_INVITE_PLAYER);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::Invite(localGamerIndex, m_LocalPlayerDesc.m_Id, -1, true, rTarget, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool
NetworkClan::InvitePlayer(const int player)
{
	CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(player));
	const rlGamerHandle& gamerHandle = gnetVerify(pPlayer) ? pPlayer->GetGamerInfo().GetGamerHandle() : rlGamerHandle();

	return InviteGamer(gamerHandle);
}

bool
NetworkClan::InviteFriend(const unsigned friendIndex)
{
	bool result = false;

	if (gnetVerify(friendIndex < rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex())))
    {
		rlGamerHandle gamerHandle;
		if (rlFriendsManager::GetGamerHandle(friendIndex, &gamerHandle))
		{
			result = InviteGamer(gamerHandle);
		}
    }

    return result;
}

bool
NetworkClan::CancelInvite(const u32 invite)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& gnetVerify(invite < m_InvitesSentCount)
		&& gnetVerify(m_InvitesSent[invite].IsValid()))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_CANCEL_INVITE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::DeleteInvite(localGamerIndex, m_InvitesSent[invite].m_Id, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool
NetworkClan::AcceptInvite(const u32 invite)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
        && gnetVerify(invite < m_InvitesReceivedCount)
        && gnetVerify(m_InvitesReceived[invite].IsValid()))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_ACCEPT_INVITE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::Join(localGamerIndex, m_InvitesReceived[invite].m_Clan.m_Id, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
    }

    return result;
}

bool
NetworkClan::RejectInvite(const u32 invite)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(invite < m_InvitesReceivedCount)
		&& gnetVerify(m_InvitesReceived[invite].IsValid())
		&& RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REJECT_INVITE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::DeleteInvite(localGamerIndex, m_InvitesReceived[invite].m_Id, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool 
NetworkClan::RefreshJoinRequestsReceived()
{
	bool result = false;

	m_JoinRequestsReceivedCount = 0;
	m_JoinRequestsReceivedTotalCount = 0;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& m_LocalPlayerDesc.IsValid())
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_JOIN_REQUESTS_RECEIVED);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			// Set the requested clan Id as opData
			NetworkClanOp_InviteRequestData* pData = rage_new NetworkClanOp_InviteRequestData(m_LocalPlayerDesc.m_Id);
			pNetClanOp->SetOpData(pData);

			result = rlClan::GetRecievedJoinRequests(localGamerIndex
				,0
				,m_LocalPlayerDesc.m_Id
				,m_JoinRequestsReceived
				,MAX_NUM_CLAN_JOIN_REQUESTS
				,&m_JoinRequestsReceivedCount
				,&m_JoinRequestsReceivedTotalCount
				,pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool      
NetworkClan::RefreshInvitesReceived()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsFullServiceReady(localGamerIndex)))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_INVITES_RECEIVED);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
            result = rlClan::GetInvites(localGamerIndex
										,0
										,m_InvitesReceived
										,MAX_NUM_CLAN_INVITES
										,&m_InvitesReceivedCount
										,&m_InvitesReceivedTotalCount
										,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

	return result;
}

bool      
NetworkClan::NextInvitesReceived()
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
		&& !CheckForAnyOpsOfType(OP_REFRESH_INVITES_RECEIVED)
		&& gnetVerify(m_InvitesReceivedCount > 0 && m_InvitesReceivedCount <= MAX_NUM_CLAN_INVITES))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_INVITES_RECEIVED);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
            result = rlClan::GetInvites(localGamerIndex
										,m_InvitesReceivedCount - 1
										,m_InvitesReceived
										,MAX_NUM_CLAN_INVITES
										,&m_InvitesReceivedCount
										,&m_InvitesReceivedTotalCount
										,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool      
NetworkClan::RefreshInvitesSent()
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_INVITES_SENT);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
            result = rlClan::GetSentInvites(localGamerIndex
											,0
											,m_InvitesSent
											,MAX_NUM_CLAN_INVITES
											,&m_InvitesSentCount
											,&m_InvitesSentTotalCount
											,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

	return result;
}

bool      
NetworkClan::NextInvitesSent()
{
	bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
            && gnetVerify(m_InvitesReceivedCount > 0 && m_InvitesReceivedCount <= MAX_NUM_CLAN_INVITES)
			&& !CheckForAnyOpsOfType(OP_REFRESH_INVITES_SENT))
    {
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_INVITES_SENT);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
            result = rlClan::GetSentInvites(localGamerIndex
											,m_InvitesSentCount - 1
											,m_InvitesSent
											,MAX_NUM_CLAN_INVITES
											,&m_InvitesSentCount
											,&m_InvitesSentTotalCount
											,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
	}

	return result;
}

//Members
bool
NetworkClan::IsMember(const rlGamerHandle& gamerHandle) const
{
	// this only checks the members that we have a record for (and not all possible members)
	for(u32 i = 0; i < m_MembersCount; i++)
	{
		if(m_Members[i].m_MemberInfo.m_GamerHandle == gamerHandle)
			return true;
	}
	return false;
}

const rlClanMember*
NetworkClan::GetMember(const u32 member) const
{
	if (gnetVerify(member < m_MembersCount)
        && gnetVerify(m_Members[member].IsValid()))
	{
		return &m_Members[member];
	}

	return NULL;
}

bool          
NetworkClan::KickMember(const u32 member)
{
	bool result = false;
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(member < m_MembersCount)
		&& gnetVerify(m_Members[member].IsValid())
		&& gnetVerify(HasPrimaryClan())
		&& RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_KICK_CLAN_MEMBER);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::Kick(localGamerIndex, m_LocalPlayerDesc.m_Id, m_Members[member].m_MemberInfo.m_GamerHandle, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

	return result;
}

bool
NetworkClan::RefreshMembers()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
        && gnetVerify(HasPrimaryClan()))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_MEMBERS);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
            result = rlClan::GetMembersByClanId(localGamerIndex
												,0
												,m_LocalPlayerDesc.m_Id
												,m_Members
												,MAX_NUM_CLAN_MEMBERS
												,&m_MembersCount
												,&m_MembersTotalCount
												,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool
NetworkClan::NextMembers()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
	{
		if (gnetVerify(HasPrimaryClan())
            && gnetVerify(m_MembersCount > 0 && m_MembersCount <= MAX_NUM_CLAN_MEMBERS)
			&& !CheckForAnyOpsOfType(OP_REFRESH_MEMBERS))
        {
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_MEMBERS);
			if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
			{
				result = rlClan::GetMembersByClanId(localGamerIndex
					,m_MembersCount - 1
					,m_LocalPlayerDesc.m_Id
					,m_Members
					,MAX_NUM_CLAN_MEMBERS
					,&m_MembersCount
					,&m_MembersTotalCount
					,pNetClanOp->GetStatus());
				gnetAssert(result);
			}
        }
    }

    return result;
}

//Wall
const rlClanWallMessage*
NetworkClan::GetWall(const u32 msgIndex) const
{
	if (gnetVerify(msgIndex < m_WallCount)
        && gnetVerify(m_Wall[msgIndex].IsValid()))
	{
		return &m_Wall[msgIndex];
	}

	return NULL;
}

bool
NetworkClan::RefreshWall()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
        && gnetVerify(HasPrimaryClan()))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_WALL);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
            result = rlClan::GetWallMessages(localGamerIndex
											 ,0
											 ,m_LocalPlayerDesc.m_Id
											 ,m_Wall
											 ,MAX_NUM_CLAN_WALL_MSGS
											 ,&m_WallCount
											 ,&m_WallTotalCount
											 ,pNetClanOp->GetStatus());
            gnetAssert(result);
        }
    }

    return result;
}

bool
NetworkClan::NextWall()
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
	{
		if (gnetVerify(HasPrimaryClan())
            && gnetVerify(m_WallCount > 0 && m_WallCount <= MAX_NUM_CLAN_WALL_MSGS)
			&& !CheckForAnyOpsOfType(OP_REFRESH_WALL))
        {
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_REFRESH_WALL);
			if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
			{
				result = rlClan::GetWallMessages(localGamerIndex
					,m_WallCount - 1
					,m_LocalPlayerDesc.m_Id
					,m_Wall
					,MAX_NUM_CLAN_WALL_MSGS
					,&m_WallCount
					,&m_WallTotalCount
					,pNetClanOp->GetStatus());
			}
            gnetAssert(result);
        }
    }

    return result;
}

bool
NetworkClan::PostToWall(const char* message)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex))
			&& gnetVerify(message)
            && gnetVerify(strlen(message) < RL_CLAN_WALL_MSG_MAX_CHARS)
            && gnetVerify(HasPrimaryClan()))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_WRITE_WALL_MESSAGE);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			result = rlClan::WriteWallMessage(localGamerIndex, m_LocalPlayerDesc.m_Id, message, pNetClanOp->GetStatus());
			gnetAssert(result);
		}
	}

    return result;
}

bool
NetworkClan::DeleteFromWall(const u32 msgIndex)
{
	bool result = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
	{
		if (gnetVerify(HasPrimaryClan())
            && gnetVerify(msgIndex < m_WallCount)
            && gnetVerify(m_Wall[msgIndex].IsValid()))
        {
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_DELETE_WALL_MESSAGE);
			if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
			{
				result = rlClan::DeleteWallMessage(localGamerIndex, m_LocalPlayerDesc.m_Id, m_Wall[msgIndex].m_Id, pNetClanOp->GetStatus());
				gnetAssert(result);
			}
        }
    }

    return result;
}
bool
NetworkClan::MemberUpdateRankId(const u32 member, bool promote)
{
    bool result = false;

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

    if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
    {
        if (gnetVerify(member < m_MembersCount)
            && gnetVerify(m_Members[member].IsValid())
            && gnetVerify(HasPrimaryClan()))
        {
			NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_KICK_CLAN_MEMBER);
			if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
			{
				result = rlClan::MemberUpdateRankId(localGamerIndex
					,m_Members[member].m_MemberInfo.m_GamerHandle
					,m_LocalPlayerDesc.m_Id
					,promote
					,pNetClanOp->GetStatus());
				gnetAssert(result);
			}
        }
    }
    return result;
}

const char*
NetworkClan::GetOperationName(const int operation)
{
    switch(operation)
    {
	case OP_NONE:                       return "OP_NONE";
	case OP_CREATE_CLAN:                return "OP_CREATE_CLAN";
	case OP_LEAVE_CLAN:                 return "OP_LEAVE_CLAN";
	case OP_KICK_CLAN_MEMBER:           return "OP_KICK_CLAN_MEMBER";
	case OP_INVITE_PLAYER:              return "OP_INVITE_PLAYER";
	case OP_REJECT_INVITE:              return "OP_REJECT_INVITE";
	case OP_ACCEPT_INVITE:              return "OP_ACCEPT_INVITE";
	case OP_CANCEL_INVITE:              return "OP_CANCEL_INVITE";
	case OP_ACCEPT_REQUEST:				return "OP_ACCEPT_REQUEST";
	case OP_REJECT_REQUEST:				return "OP_REJECT_REQUEST";
	case OP_JOIN_OPEN_CLAN:             return "OP_JOIN_OPEN_CLAN";
	case OP_RANK_CREATE:                return "OP_RANK_CREATE";
	case OP_RANK_DELETE:                return "OP_RANK_DELETE";
	case OP_RANK_UPDATE:                return "OP_RANK_UPDATE";
	case OP_REFRESH_RANKS:              return "OP_REFRESH_RANKS";
	case OP_MEMBER_UPDATE_RANK:         return "OP_MEMBER_UPDATE_RANK";
	case OP_REFRESH_MINE:               return "OP_REFRESH_MINE";
	case OP_REFRESH_MEMBERSHIP_FOR:     return "OP_REFRESH_MEMBERSHIP_FOR";
	case OP_REFRESH_INVITES_RECEIVED:   return "OP_REFRESH_INVITES_RECEIVED";
	case OP_REFRESH_INVITES_SENT:       return "OP_REFRESH_INVITES_SENT";
	case OP_REFRESH_MEMBERS:            return "OP_REFRESH_MEMBERS";
	case OP_REFRESH_OPEN_CLANS:         return "OP_REFRESH_OPEN_CLANS";
	case OP_REFRESH_JOIN_REQUESTS_RECEIVED: return "OP_REFRESH_JOIN_REQUESTS_RECEIVED";
	case OP_CLAN_SEARCH:				return "OP_CLAN_SEARCH";
	case OP_CLAN_METADATA:				return "OP_CLAN_METADATA";
	case OP_SET_PRIMARY_CLAN:           return "OP_SET_PRIMARY_CLAN";
	case OP_REFRESH_WALL:               return "OP_REFRESH_WALL";
	case OP_WRITE_WALL_MESSAGE:         return "OP_WRITE_WALL_MESSAGE";
	case OP_DELETE_WALL_MESSAGE:        return "OP_DELETE_WALL_MESSAGE";
	case OP_FRIEND_INFO_EVENT:			return "OP_FRIEND_INFO_EVENT";
	}

	return "OP_UNKNOWN";
};

//////////////////////////////////////////////////////////////////////////



bool NetworkClan::GetIsFounderClan( const rlClanDesc &Clan )
{
#if !__FINAL
	static const int FOUNDER_CLAN_POSIX_TIME = 1325376000;  //For dev we use 1/1/2012 to get a hint of split.
#else
	static const int FOUNDER_CLAN_POSIX_TIME = 1337054400;	//May 15, 2012
#endif

	return Clan.IsValid() && Clan.m_CreatedTimePosix < FOUNDER_CLAN_POSIX_TIME && !Clan.IsRockstarClan();
}

//////////////////////////////////////////////////////////////////////////

const char* NetworkClan::GetUIFormattedClanTag(const rlClanMembershipData* pClan, char* out_tag, int size_of_buffer, bool bIgnoreRank/*=true*/)
{
	return GetUIFormattedClanTag(pClan->m_Clan, (bIgnoreRank ? -1 : pClan->m_Rank.m_RankOrder), out_tag, size_of_buffer);
}

const char* NetworkClan::GetUIFormattedClanTag(const rlClanDesc& Clan, int iRankOrder, char* out_tag, int size_of_buffer )
{
	gnetAssertf( size_of_buffer >= FORMATTED_CLAN_TAG_LEN-FORMATTED_CLAN_COLOR_LEN, "Clan tag buffer must support at least %i characters, but you fed me %i", FORMATTED_CLAN_TAG_LEN-FORMATTED_CLAN_COLOR_LEN, size_of_buffer);

	if(!Clan.IsValid()) //if we've got an invalid clan, just return an empty string
	{
		formatf(out_tag, size_of_buffer,"");
		return out_tag;
	}

	return NetworkClan::GetUIFormattedClanTag(Clan.m_IsSystemClan, Clan.IsRockstarClan(), Clan.m_ClanTag, iRankOrder, Color32(Clan.m_clanColor), out_tag, size_of_buffer);
}

const char* NetworkClan::GetUIFormattedClanTag(bool IsSystemClan, bool bRockstarClan, const char* clanTag, int iRankOrder, Color32 color, char* out_tag, int size_of_buffer )
{
	//If 'Rockstar' is in the Clan name, we'll add some magic.

	atStringBuilder builder(out_tag, size_of_buffer);

	//build up the tag from left to right
	//First char is left tag for public or private
	builder.AppendOrFail(IsSystemClan ? '{' : '(');

	//Add the little R* logo for special Rockstar clans
	builder.AppendOrFail(bRockstarClan ? '*' : '_');

	//If the clan is founder clan, we do a fancy little close tag thing, otherwise do the plain one.
	builder.AppendOrFail((iRankOrder>=0 && iRankOrder <= 4) ? static_cast<char>('0' + iRankOrder) : 'n');	// '0' is highest rank and '4' is the lowest.  Anything out of range will be sent as 'n' which will flag SF to hide the hierarchy bar

	//Now the body of the tag
	builder.AppendOrFail(clanTag);

	char colorField[8];
	formatf(colorField, 8, "#%02x%02x%02x", color.GetRed(), color.GetGreen(), color.GetBlue());

	builder.AppendOrFail(colorField);

	return out_tag;
}
//////////////////////////////////////////////////////////////////////////

bool NetworkClan::RequestEmblemForClan( rlClanId clanId  ASSERT_ONLY(, const char* requesterName))
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);

	return m_EmblemMgr.RequestEmblem(emblemDesc ASSERT_ONLY(, requesterName));
}


void NetworkClan::ReleaseEmblemReference( rlClanId clanId  ASSERT_ONLY(, const char* releaserName) )
{
	if (IsEmblemForClanReady(clanId))
	{
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
		m_EmblemMgr.ReleaseEmblem(emblemDesc ASSERT_ONLY(, releaserName));
	}
}

bool NetworkClan::IsEmblemForClanReady( rlClanId clanId )
{
	return (GetClanEmblemNameForClan(clanId) != '\0');
}

bool NetworkClan::IsEmblemForClanReady( rlClanId clanId, const char*& pOut_Result )
{
	return GetClanEmblemNameForClan(clanId, pOut_Result);
}

bool NetworkClan::IsEmblemForClanFailed( rlClanId clanId )
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
	return m_EmblemMgr.RequestFailed(emblemDesc);
}

bool NetworkClan::IsEmblemForClanPending( rlClanId clanId )
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
	return m_EmblemMgr.RequestPending(emblemDesc);
}

bool NetworkClan::IsEmblemForClanRetrying( rlClanId clanId ) const
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
	return m_EmblemMgr.RequestRetrying(emblemDesc);
}

const char *NetworkClan::GetClanEmblemNameForClan( rlClanId clanId )
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
	return m_EmblemMgr.GetEmblemName(emblemDesc);
}

bool NetworkClan::GetClanEmblemNameForClan( rlClanId clanId, const char*& pOut_Result )
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
	return m_EmblemMgr.GetEmblemName(emblemDesc, pOut_Result);
}

int NetworkClan::GetClanEmblemTxdSlotForClan( rlClanId clanId )
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId, RL_CLAN_EMBLEM_SIZE_128);
	return m_EmblemMgr.GetEmblemTXDSlot(emblemDesc);
}


const char* NetworkClan::GetClanEmblemTXDNameForLocalGamerClan()
{

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlClan::HasPrimaryClan(localGamerIndex))
	{
		const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(localGamerIndex);
		return GetClanEmblemNameForClan(clanDesc.m_Id);
	}

	return "\0";
}

NetworkCrewEmblemMgr& NetworkClan::GetCrewEmblemMgr()
{
	//Yes, this is silly, but it's in prep for refactoring stuff, so please ignore for now.
	return CLiveManager::GetNetworkClan().m_EmblemMgr;
}


#if __BANK

void  
NetworkClan::InitWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Network");

	if (gnetVerify(bank))
	{
		//Clan
		sysMemSet(m_BankClanName, 0, RL_CLAN_NAME_MAX_CHARS);
		sysMemSet(m_BankClanTag, 0, RL_CLAN_TAG_MAX_CHARS);

		//Players
		m_BankPlayersCombo = 0;
		m_BankMembershipPlayersCombo = 0;
		m_BankPlayersComboIndex = 0;
		m_BankPlayers[0] = "--No Players--";
		m_BankNumPlayers  = 1;

		//Friends
		m_BankFriendsCombo = 0;
		m_BankMembershipFriendsCombo = 0;
		m_BankFriendsComboIndex = 0;
		m_BankFriends[0] = "--No Friends--";
		m_BankNumFriends  = 1;

		//Memberships
		m_BankMembershipComboName = 0;
		m_BankMembershipComboTag = 0;
		m_BankMembershipComboRank = 0;
		m_BankMembershipComboIndex = 0;
		m_BankMembershipName[0] = "--No Memberships--";
		m_BankMembershipTag[0]  = "N/A";
		m_BankMembershipRank[0] = "N/A";
		m_BankNumMemberships  = 1;

        //Ranks
		sysMemSet(m_BankRankUpdateName, 0, RL_CLAN_NAME_MAX_CHARS);
        m_BankRankUpdateNameText = 0;
        m_BankRanksName[0] = "-- No Ranks--";
        m_BankRankSystemFlags = 0;
        m_BankRankCombo = 0;
        m_BankRankComboIndex = 0;
        m_BankRankKick = 0;
        m_BankRankInvite = 0;
        m_BankRankPromote = 0;
        m_BankRankDemote = 0;
        m_BankRankRankManager = 0;
        m_BankRankAllowOpen = 0;
        m_BankRankWriteOnWall = 0;
        m_BankRankDeleteFromWall = 0;
        m_BankNumRanks = 1;

		//Open Clans
		m_BankOpenClansComboName = 0;
		m_BankOpenClansComboTag = 0;
		m_BankOpenClansComboIndex = 0;
		m_BankOpenClansName[0] = "--No Open Clans--";
		m_BankOpenClansTag[0] = "N/A";
		m_BankNumOpenClans  = 1;

		safecpy(m_BankFindStr, "Rockstar");
		m_BankFindPage = 0;

		//Invites
		m_BankInvitesComboReceived = 0;
		m_BankInvitesComboClanReceived = 0;
		m_BankInvitesComboIndexReceived = 0;
		m_BankInvitesReceived[0] = "--No Invites--";
		m_BankInvitesClanReceived[0] = "--No Invites--";
		m_BankNumInvitesReceived  = 1;

		m_BankInvitesComboSent = 0;
		m_BankInvitesComboClanSent = 0;
		m_BankInvitesComboIndexSent = 0;
		m_BankInvitesSent[0] = "--No Invites--";
		m_BankInvitesClanSent[0] = "--No Invites--";
		m_BankNumInvitesSent  = 1;

		//Members
		m_BankMembersComboName = 0;
		m_BankMembersComboRank = 0;
		m_BankMembersComboIndex = 0;
		m_BankMembersName[0] = "--No Members--";
        m_BankMembersRank[0] = "N/A";
		m_BankNumMembers  = 1;

		//Wall
		sysMemSet(m_BankWallNewMessage, 0, RL_CLAN_WALL_MSG_MAX_CHARS);
		m_BankWallComboWriter = 0;
		m_BankWallComboMessage = 0;
		m_BankWallComboIndex = 0;
		m_BankWallWriter[0] = "--No Message--";
        m_BankWallMessage[0] = "N/A";
		m_BankNumWalls = 1;

		// Accomplishments
		sysMemSet( m_BankAccomplishmentName, 0, BANK_ACCOMPLISHMENT_NAME_MAX_CHARS );
		sysMemSet( m_BankAccomplishmentCount, 0, BANK_ACCOMPLISHMENT_COUNT_MAX_CHARS );

		bkGroup* group = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live/Debug Clans"));
		if (group)
		{
			bank->DeleteGroup(*group);
		}

		bkGroup *debugGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live"));
		if(gnetVerify(debugGroup))
		{
			bool resetCurrentGroupAtEnd = false;

			if(debugGroup != bank->GetCurrentGroup())
			{
				bank->SetCurrentGroup(*debugGroup);
				resetCurrentGroupAtEnd = true;
			}

			bank->PushGroup("Debug Clans");
			{
				bank->AddSeparator();

				bank->AddText("Clan Name", m_BankClanName, RL_CLAN_NAME_MAX_CHARS);
				bank->AddText("Clan Tag", m_BankClanTag, RL_CLAN_TAG_MAX_CHARS);
				bank->AddButton("Create Clan", datCallback(MFA(NetworkClan::Bank_CreateClan), (datBase*)this));

				bank->PushGroup("Memberships");
                {
					m_BankMembershipComboName = bank->AddCombo("Clan Name", &m_BankMembershipComboIndex, m_BankNumMemberships, (const char **)m_BankMembershipName);
                    m_BankMembershipComboTag = bank->AddCombo("Clan Tag", &m_BankMembershipComboIndex, m_BankNumMemberships, (const char **)m_BankMembershipTag);
                    m_BankMembershipComboRank = bank->AddCombo("Clan Rank", &m_BankMembershipComboIndex, m_BankNumMemberships, (const char **)m_BankMembershipRank);

					bank->AddSeparator();
					bank->AddButton("Update Clan Details", datCallback(MFA(NetworkClan::Bank_UpdateClanDetails), (datBase*)this));
					bank->AddButton("Set as Primary Clan", datCallback(MFA(NetworkClan::Bank_SetPrimaryClan), (datBase*)this));
					bank->AddButton("Leave Clan", datCallback(MFA(NetworkClan::Bank_LeaveClan), (datBase*)this));
					bank->AddButton("Refresh Memberships", datCallback(MFA(NetworkClan::Bank_RefreshMemberships), (datBase*)this));
					bank->AddButton("Dump Local Membership Info", datCallback(MFA(NetworkClan::Bank_DumpMembershipInfo), (datBase*)this));

					bank->AddSeparator();
					m_BankMembershipPlayersCombo = bank->AddCombo ("Players", &m_BankPlayersComboIndex, m_BankNumPlayers, (const char **)m_BankPlayers);
					bank->AddButton("Refresh Player Membership", datCallback(MFA(NetworkClan::Bank_RefreshPlayerMemberships), (datBase*)this));
					bank->AddSeparator();

					bank->AddSeparator();
					m_BankMembershipFriendsCombo = bank->AddCombo ("Friends", &m_BankFriendsComboIndex, m_BankNumFriends, (const char **)m_BankFriends);
					bank->AddButton("Refresh Friend Membership", datCallback(MFA(NetworkClan::Bank_RefreshFriendMemberships), (datBase*)this));
					bank->AddSeparator();

                    bank->PushGroup("Members");
                    {
                        m_BankMembersComboName = bank->AddCombo("Members", &m_BankMembersComboIndex, m_BankNumMembers, (const char **)m_BankMembersName);
                        m_BankMembersComboRank = bank->AddCombo("Members Rank", &m_BankMembersComboIndex, m_BankNumMembers, (const char **)m_BankMembersRank);
                        bank->AddButton("Kick Member", datCallback(MFA(NetworkClan::Bank_KickMember), (datBase*)this));
                        bank->AddButton("Refresh Members", datCallback(MFA(NetworkClan::Bank_RefreshMembers), (datBase*)this));
                        bank->AddButton("Next Members", datCallback(MFA(NetworkClan::NextMembers), (datBase*)this));
                    }
                    bank->PopGroup();

                    bank->PushGroup("Ranks");
                    {
                        m_BankRankCombo = bank->AddCombo("Ranks", &m_BankRankComboIndex, m_BankNumRanks, (const char **)m_BankRanksName, datCallback(MFA(NetworkClan::Bank_RankComboSelect), (datBase*)this));
                        bank->AddButton("Refresh Ranks", datCallback(MFA(NetworkClan::Bank_RefreshRanks), (datBase*)this));
                        bank->AddButton("Next Ranks", datCallback(MFA(NetworkClan::NextRanks), (datBase*)this));
                        bank->AddButton("Delete Rank", datCallback(MFA(NetworkClan::Bank_RankDelete), (datBase*)this));
                        bank->AddButton("Create Rank", datCallback(MFA(NetworkClan::Bank_RankCreate), (datBase*)this));
                        bank->AddButton("Update Rank", datCallback(MFA(NetworkClan::Bank_RankUpdate), (datBase*)this));
                        bank->AddButton("Move Rank Up", datCallback(MFA(NetworkClan::Bank_RankMoveUp), (datBase*)this));
                        bank->AddButton("Move Rank Down", datCallback(MFA(NetworkClan::Bank_RankMoveDown), (datBase*)this));
                        m_BankRankUpdateNameText = bank->AddText("Rank Name", m_BankRankUpdateName, RL_CLAN_NAME_MAX_CHARS);
                        m_BankRankKick = bank->AddToggle("Kick", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_KICK);
                        m_BankRankInvite = bank->AddToggle("Invite", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_INVITE);
                        m_BankRankPromote = bank->AddToggle("Promote", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_PROMOTE);
                        m_BankRankDemote = bank->AddToggle("Demote", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_DEMOTE);
                        m_BankRankRankManager = bank->AddToggle("RankManager", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_RANKMANAGER);
                        m_BankRankWriteOnWall = bank->AddToggle("WriteOnWall", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_WRITEONWALL);
                        m_BankRankDeleteFromWall = bank->AddToggle("DeleteFromWall", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_DELETEFROMWALL);
                        m_BankRankAllowOpen = bank->AddToggle("AllowOpen", &m_BankRankSystemFlags, RL_CLAN_PERMISSION_CREWEDIT);
                    }
                    bank->PopGroup();

                    bank->PushGroup("Wall");
                    {
                        m_BankWallComboWriter = bank->AddCombo("Writer", &m_BankWallComboIndex, m_BankNumWalls, (const char **)m_BankWallWriter);
                        m_BankWallComboMessage = bank->AddCombo("Message", &m_BankWallComboIndex, m_BankNumWalls, (const char **)m_BankWallMessage);
                        bank->AddButton("Refresh Wall", datCallback(MFA(NetworkClan::Bank_RefreshWall), (datBase*)this));
                        bank->AddButton("Next Wall", datCallback(MFA(NetworkClan::NextWall), (datBase*)this));
                        bank->AddButton("Delete Message", datCallback(MFA(NetworkClan::Bank_DeleteFromWall), (datBase*)this));
                        bank->AddSeparator();

                        bank->AddText("New Message", m_BankWallNewMessage, RL_CLAN_WALL_MSG_MAX_CHARS);
                        bank->AddButton("Post Message", datCallback(MFA(NetworkClan::Bank_PostToWall), (datBase*)this));
                    }
                    bank->PopGroup();
                }
				bank->PopGroup();

                bank->PushGroup("Open Clans");
                {
                    m_BankOpenClansComboName = bank->AddCombo("Clan Name", &m_BankOpenClansComboIndex, m_BankNumOpenClans, (const char **)m_BankOpenClansName);
                    m_BankOpenClansComboTag = bank->AddCombo("Clan Tag", &m_BankOpenClansComboIndex, m_BankNumOpenClans, (const char **)m_BankOpenClansTag);
                    bank->AddButton("Refresh Open Clans", datCallback(MFA(NetworkClan::RefreshOpenClans), (datBase*)this));
                    bank->AddButton("Next Open Clans", datCallback(MFA(NetworkClan::NextOpenClans), (datBase*)this));
                    bank->AddButton("Join Open Clan", datCallback(MFA(NetworkClan::Bank_JoinOpenClan), (datBase*)this));

					bank->PushGroup("Find");
					{
						bank->AddText("Find String", m_BankFindStr, RL_CLAN_NAME_MAX_CHARS, false);
						bank->AddText("Results Page", &m_BankFindPage, false);
						bank->AddButton("Search for Clans",datCallback(MFA(NetworkClan::Bank_SearchForFindClans), (datBase*)this));
					}
					bank->PopGroup();

					bank->PushGroup("MetaData");
					{
						bank->AddText("Metadata REquest Clan ID", &m_Bank_MetaDataclanId, false);
						bank->AddButton("Request MetaData",datCallback(MFA(NetworkClan::Bank_RequestMetaData), (datBase*)this));
						bank->AddButton("Print Local Primary Crew Metadat", datCallback(MFA(NetworkClan::Bank_PrintLocalPrimaryMetadata), (datBase*)this));
						bank->AddButton("Print Debug Metadata", datCallback(MFA(NetworkClan::Bank_PringDebugMetadata), (datBase*)this));
					}
					bank->PopGroup();
                }
				bank->PopGroup();

				bank->PushGroup("Invites");
				{
					m_BankPlayersCombo = bank->AddCombo ("Players", &m_BankPlayersComboIndex, m_BankNumPlayers, (const char **)m_BankPlayers);
					bank->AddButton("Invite Player to Primary Crew", datCallback(MFA(NetworkClan::Bank_InvitePlayerToPrimaryClan), (datBase*)this));
					bank->AddSeparator();

					m_BankFriendsCombo = bank->AddCombo ("Friends", &m_BankFriendsComboIndex, m_BankNumFriends, (const char **)m_BankFriends);
					bank->AddButton("Invite Friend to Primary Crew", datCallback(MFA(NetworkClan::Bank_InviteFriendToPrimaryClan), (datBase*)this));
					bank->AddSeparator();

					bank->PushGroup("Received");
					{
                        bank->AddButton("Refresh Received Invites", datCallback(MFA(NetworkClan::RefreshInvitesReceived), (datBase*)this));
                        bank->AddButton("Next Received Invites", datCallback(MFA(NetworkClan::NextInvitesReceived), (datBase*)this));
						m_BankInvitesComboReceived = bank->AddCombo("Gamer Name", &m_BankInvitesComboIndexReceived, m_BankNumInvitesReceived, (const char **)m_BankInvitesReceived);
						m_BankInvitesComboClanReceived = bank->AddCombo("Clan Name", &m_BankInvitesComboIndexReceived, m_BankNumInvitesReceived, (const char **)m_BankInvitesClanReceived);
						bank->AddButton("Accept Invite", datCallback(MFA(NetworkClan::Bank_AcceptInvite), (datBase*)this));
						bank->AddButton("Reject Invite", datCallback(MFA(NetworkClan::Bank_RejectInvite), (datBase*)this));
					}
					bank->PopGroup();

					bank->PushGroup("Sent");
					{
                        bank->AddButton("Refresh Sent Invites", datCallback(MFA(NetworkClan::RefreshInvitesSent), (datBase*)this));
                        bank->AddButton("Next Sent Invites", datCallback(MFA(NetworkClan::NextInvitesSent), (datBase*)this));
						m_BankInvitesComboSent = bank->AddCombo("Gamer Name", &m_BankInvitesComboIndexSent, m_BankNumInvitesSent, (const char **)m_BankInvitesSent);
						m_BankInvitesComboClanSent = bank->AddCombo("Clan Name", &m_BankInvitesComboIndexSent, m_BankNumInvitesSent, (const char **)m_BankInvitesClanSent);
						bank->AddButton("Cancel Invite", datCallback(MFA(NetworkClan::Bank_CancelInvite), (datBase*)this));
					}
					bank->PopGroup();
				}
				bank->PopGroup();

				bank->AddButton("Create a fake invite", datCallback(MFA(NetworkClan::Bank_MakeFakeRecvdInvite), (datBase*)this));
			}
			bank->PopGroup();

			if(resetCurrentGroupAtEnd)
			{
				bank->UnSetCurrentGroup(*debugGroup);
			}
		}

		m_EmblemMgr.Bank_InitWidgets(bank, "NetworkClan");
	}
}

void
NetworkClan::Bank_Update()
{
	if (!IsIdle())
		return;

	if (!rlClan::IsInitialized())
		return;

	if (!CLiveManager::IsOnline())
		return;

	if (m_bank_UpdateClanDetailsRequested)
	{
		Bank_UpdateClanDetails();
	}

	m_Bank_CrewMetadata.Update();
	m_EmblemMgr.Bank_Update();
}

void
NetworkClan::Bank_UpdateClanDetails()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (!rlClan::IsServiceReady(localGamerIndex) 
		&& (m_BankClanName[0] != '\0' || m_BankClanTag[0] != '\0' 
		|| m_BankRanksName[0] != '\0' || m_BankWallNewMessage[0] != '\0'))
	{
		sysMemSet(m_BankClanName, 0, COUNTOF(m_BankClanName));
		sysMemSet(m_BankClanTag, 0, COUNTOF(m_BankClanTag));
		sysMemSet(m_BankRankUpdateName, 0, COUNTOF(m_BankRankUpdateName));
		sysMemSet(m_BankWallNewMessage, 0, COUNTOF(m_BankWallNewMessage));
	}

	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();

	if (m_BankMembershipComboName || m_BankMembershipComboTag || m_BankMembershipComboRank)
	{
		//Update the membership list
		m_BankNumMemberships = CLiveManager::GetNetworkClan().GetMembershipCount(pGamerInfo->GetGamerHandle());
		for (u32 i=0; i<m_BankNumMemberships; i++)
		{
			const rlClanMembershipData* membership = CLiveManager::GetNetworkClan().GetMembership(pGamerInfo->GetGamerHandle(), i);
			m_BankMembershipName[i] = &(membership->m_Clan.m_ClanName[0]);
			m_BankMembershipTag[i]  = &(membership->m_Clan.m_ClanTag[0]);
			m_BankMembershipRank[i] = &(membership->m_Rank.m_RankName[0]);
		}
		//Avoid empty combo boxes
		int nNumMemberships = m_BankNumMemberships;
		if(nNumMemberships == 0)
		{
			m_BankMembershipTag[0]  = "N/A";
			m_BankMembershipRank[0] = "N/A";
			nNumMemberships = 1;
		}

		if (m_BankMembershipComboName)
		{
			m_BankMembershipComboName->UpdateCombo("Clan Name", &m_BankMembershipComboIndex, nNumMemberships, (const char **)m_BankMembershipName);
		}
		if (m_BankMembershipComboTag)
		{
			m_BankMembershipComboTag->UpdateCombo("Clan Tag", &m_BankMembershipComboIndex, nNumMemberships, (const char **)m_BankMembershipTag);
		}
		if (m_BankMembershipComboRank)
		{
			m_BankMembershipComboRank->UpdateCombo("Clan Rank", &m_BankMembershipComboIndex, nNumMemberships, (const char **)m_BankMembershipRank);
		}
	}

	if (m_BankOpenClansComboName || m_BankOpenClansComboTag)
	{
		//Update the open clan list
		m_BankNumOpenClans = CLiveManager::GetNetworkClan().GetOpenClanCount();
		for (u32 i=0; i<m_BankNumOpenClans; i++)
		{
			const rlClanDesc* clan = CLiveManager::GetNetworkClan().GetOpenClan(i);
			m_BankOpenClansName[i] = &(clan->m_ClanName[0]);
			m_BankOpenClansTag[i] = &(clan->m_ClanName[0]);
		}
		//Avoid empty combo boxes
		int nNumOpenClans = m_BankNumOpenClans;
		if(nNumOpenClans == 0)
		{
			m_BankOpenClansName[0] = "--No Clans--";
			m_BankOpenClansTag[0] = "N/A";
			nNumOpenClans = 1;
		}

		if (m_BankOpenClansComboName)
		{
			m_BankOpenClansComboName->UpdateCombo("Clan Name", &m_BankOpenClansComboIndex, nNumOpenClans, (const char **)m_BankOpenClansName);
		}
		if (m_BankOpenClansComboTag)
		{
			m_BankOpenClansComboTag->UpdateCombo("Clan Tag", &m_BankOpenClansComboIndex, nNumOpenClans, (const char **)m_BankOpenClansTag);
		}
	}

	if (m_BankRankCombo)
	{
		//Update the rank list
		m_BankNumRanks = CLiveManager::GetNetworkClan().GetRankCount();
		for (u32 i=0; i<m_BankNumRanks; i++)
		{
			const rlClanRank* rank = CLiveManager::GetNetworkClan().GetRank(i);
			m_BankRanksName[i] = &(rank->m_RankName[0]);
		}
		//Avoid empty combo boxes
		int nNumRanks = m_BankNumRanks;
		if(nNumRanks == 0)
		{
			m_BankRanksName[0] = "--No Ranks--";
			nNumRanks = 1;
		}

		m_BankRankCombo->UpdateCombo("Ranks", &m_BankRankComboIndex, nNumRanks, (const char**)m_BankRanksName, datCallback(MFA(NetworkClan::Bank_RankComboSelect), (datBase*)this));
	}

	if (m_BankMembersComboName || m_BankMembersComboRank)
	{
		//Update the member list
		m_BankNumMembers = CLiveManager::GetNetworkClan().GetMemberCount();
		for (u32 i=0; i<m_BankNumMembers; i++)
		{
			const rage::rlClanMember* member = CLiveManager::GetNetworkClan().GetMember(i);
			if (gnetVerify(member) && gnetVerify(member->IsValid()))
			{
				m_BankMembersName[i] = reinterpret_cast<const char*>(&(member->m_MemberInfo.m_Gamertag[0]));
				m_BankMembersRank[i] = &(member->m_MemberClanInfo.m_Rank.m_RankName[0]);
			}
		}
		//Avoid empty combo boxes
		int nNumMembers = m_BankNumMembers;
		if(nNumMembers == 0)
		{
			m_BankMembersName[0] = "--No Members--";
			m_BankMembersRank[0] = "N/A";
			nNumMembers  = 1;
		}

		if (m_BankMembersComboName)
		{
			m_BankMembersComboName->UpdateCombo("Members", &m_BankMembersComboIndex, nNumMembers, (const char **)m_BankMembersName);
		}
		if (m_BankMembersComboRank)
		{
			m_BankMembersComboRank->UpdateCombo("Members Rank", &m_BankMembersComboIndex, nNumMembers, (const char **)m_BankMembersRank);
		}
	}

	if (m_BankWallComboWriter || m_BankWallComboMessage)
	{
		//Update the wall list
		m_BankNumWalls = CLiveManager::GetNetworkClan().GetWallCount();
		for (u32 i=0; i<m_BankNumWalls; i++)
		{
			const rage::rlClanWallMessage* msg = CLiveManager::GetNetworkClan().GetWall(i);
			if (gnetVerify(msg) && gnetVerify(msg->IsValid()))
			{
				m_BankWallWriter[i] = reinterpret_cast<const char*>(&(msg->m_Writer.m_Gamertag[0]));
				m_BankWallMessage[i] = &(msg->m_Message[0]);
			}
		}
		//Avoid empty combo boxes
		int nNumWalls = m_BankNumWalls;
		if(nNumWalls == 0)
		{
			m_BankWallWriter[0] = "--No Messages--";
			m_BankWallMessage[0] = "N/A";
			nNumWalls  = 1;
		}

		if (m_BankWallComboWriter)
		{
			m_BankWallComboWriter->UpdateCombo("Writer", &m_BankWallComboIndex, nNumWalls, (const char **)m_BankWallWriter);
		}
		if (m_BankWallComboMessage)
		{
			m_BankWallComboMessage->UpdateCombo("Message", &m_BankWallComboIndex, nNumWalls, (const char **)m_BankWallMessage);
		}
	}

	if (m_BankInvitesComboReceived || m_BankInvitesComboClanReceived)
	{
		//Update the Received invite list
		m_BankNumInvitesReceived = CLiveManager::GetNetworkClan().GetInvitesReceivedCount();
		for (u32 i=0; i<m_BankNumInvitesReceived; i++)
		{
			const rlClanInvite* invite = CLiveManager::GetNetworkClan().GetReceivedInvite(i);
			if (gnetVerify(invite))
			{
				gnetAssert(invite->IsValid());
				m_BankInvitesClanReceived[i] = reinterpret_cast<const char *>(&(invite->m_Clan.m_ClanName[0]));
				m_BankInvitesReceived[i] = reinterpret_cast<const char *>(&(invite->m_Inviter.m_Gamertag[0]));
			}
		}
		//Avoid empty combo boxes
		int nNumInvitesReceived = m_BankNumInvitesReceived;
		if(nNumInvitesReceived == 0)
		{
			m_BankInvitesReceived[0] = "--No Invites--";
			m_BankInvitesClanReceived[0] = "--No Invites--";
			nNumInvitesReceived  = 1;
		}

		if (m_BankInvitesComboReceived)
		{
			m_BankInvitesComboReceived->UpdateCombo("Gamer Name", &m_BankInvitesComboIndexReceived, nNumInvitesReceived, (const char **)m_BankInvitesReceived);
		}
		if (m_BankInvitesComboClanReceived)
		{
			m_BankInvitesComboClanReceived->UpdateCombo("Clan Name", &m_BankInvitesComboIndexReceived, nNumInvitesReceived, (const char **)m_BankInvitesClanReceived);
		}
	}

	if (m_BankInvitesComboSent || m_BankInvitesComboClanSent)
	{
		//Update the invite list
		m_BankNumInvitesSent = CLiveManager::GetNetworkClan().GetInvitesSentCount();
		for (u32 i=0; i<m_BankNumInvitesSent; i++)
		{
			const rlClanInvite* invite = CLiveManager::GetNetworkClan().GetSentInvite(i);
			if (gnetVerify(invite))
			{
				m_BankInvitesClanSent[i] = reinterpret_cast<const char *>(&(invite->m_Clan.m_ClanName[0]));
				m_BankInvitesSent[i] = reinterpret_cast<const char *>(&(invite->m_Invitee.m_Gamertag[0]));
			}
		}
		//Avoid empty combo boxes
		int nNumInvitesSent = m_BankNumInvitesSent;
		if(nNumInvitesSent == 0)
		{
			m_BankInvitesSent[0] = "--No Invites--";
			m_BankInvitesClanSent[0] = "--No Invites--";
			nNumInvitesSent  = 1;
		}

		if (m_BankInvitesComboSent)
		{
			m_BankInvitesComboSent->UpdateCombo("Gamer Name", &m_BankInvitesComboIndexSent, nNumInvitesSent, (const char **)m_BankInvitesSent);
		}
		if (m_BankInvitesComboClanSent)
		{
			m_BankInvitesComboClanSent->UpdateCombo("Clan Name", &m_BankInvitesComboIndexSent, nNumInvitesSent, (const char **)m_BankInvitesClanSent);
		}
	}

	if (m_BankPlayersCombo || m_BankMembershipPlayersCombo)
	{
		//Update the player list
		m_BankNumPlayers = 0;
		unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers; index++)
		{
			const netPlayer *player = remoteActivePlayers[index];

			if(!player->IsBot())
			{
				m_BankPlayers[m_BankNumPlayers] = player->GetLogName();
				m_BankNumPlayers++;
			}
		}
		//Avoid empty combo boxes
		int nNumPlayers = m_BankNumPlayers;
		if(nNumPlayers == 0)
		{
			m_BankPlayers[0] = "--No Players--";
			nNumPlayers  = 1;
		}

		if (m_BankPlayersCombo)
		{
			m_BankPlayersCombo->UpdateCombo("Players", &m_BankPlayersComboIndex, nNumPlayers, (const char **)m_BankPlayers);
		}
		if (m_BankMembershipPlayersCombo)
		{
			m_BankMembershipPlayersCombo->UpdateCombo("Players", &m_BankPlayersComboIndex, nNumPlayers, (const char **)m_BankPlayers);
		}
	}

	if (m_BankFriendsCombo || m_BankMembershipFriendsCombo)
	{
		//Update the friend list
		m_BankNumFriends = CLiveManager::GetFriendsPage()->m_NumFriends;
		for (u32 i=0; i<m_BankNumFriends; i++)
		{
			m_BankFriends[i] = CLiveManager::GetFriendsPage()->m_Friends[i].GetName();
		}
		//Avoid empty combo boxes
		int nNumFriends = m_BankNumFriends;
		if(nNumFriends == 0)
		{
			m_BankFriends[0] = "--No Friends--";
			nNumFriends  = 1;
		}

		if (m_BankFriendsCombo)
		{
			m_BankFriendsCombo->UpdateCombo("Friends", &m_BankFriendsComboIndex, nNumFriends, (const char **)m_BankFriends);
		}

		if (m_BankMembershipFriendsCombo)
		{
			m_BankMembershipFriendsCombo->UpdateCombo("Friends", &m_BankFriendsComboIndex, nNumFriends, (const char **)m_BankFriends);
		}
	}

	m_bank_UpdateClanDetailsRequested = false;
}

void  
NetworkClan::Bank_CreateClan()
{
	CLiveManager::GetNetworkClan().CreateClan(m_BankClanName, m_BankClanTag, false);
}

void  
NetworkClan::Bank_SetPrimaryClan()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		if (GetPrimaryClan()->IsValid() && GetPrimaryClan()->m_Id == clan->m_Clan.m_Id)
			return;

		CLiveManager::GetNetworkClan().SetPrimaryClan(clan->m_Clan.m_Id);
	}
}

void  
NetworkClan::Bank_LeaveClan()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SETUP PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().LeaveClan();
	}
}

void  
NetworkClan::Bank_RankComboSelect()
{
    if (m_BankRankComboIndex >= 0 && m_BankRankComboIndex < CLiveManager::GetNetworkClan().GetRankCount())
    {
        const rlClanRank* rank = CLiveManager::GetNetworkClan().GetRank(m_BankRankComboIndex);
        if (gnetVerify(rank))
        {
            m_BankRankSystemFlags = (u32)rank->m_SystemFlags;

            safecpy(m_BankRankUpdateName, &(rank->m_RankName[0]), strlen(rank->m_RankName) + 1);
            m_BankRankUpdateNameText->SetString(m_BankRankUpdateName);

            m_BankRankKick->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_KICK) == RL_CLAN_PERMISSION_KICK);
            m_BankRankInvite->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_INVITE) == RL_CLAN_PERMISSION_INVITE);
            m_BankRankPromote->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_PROMOTE) == RL_CLAN_PERMISSION_PROMOTE);
            m_BankRankDemote->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_DEMOTE) == RL_CLAN_PERMISSION_DEMOTE);
            m_BankRankRankManager->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_RANKMANAGER) == RL_CLAN_PERMISSION_RANKMANAGER);
            m_BankRankWriteOnWall->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_WRITEONWALL) == RL_CLAN_PERMISSION_WRITEONWALL);
            m_BankRankDeleteFromWall->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_DELETEFROMWALL) == RL_CLAN_PERMISSION_DELETEFROMWALL);
            m_BankRankAllowOpen->SetBool((m_BankRankSystemFlags & RL_CLAN_PERMISSION_CREWEDIT) == RL_CLAN_PERMISSION_CREWEDIT);
        }
    }
}

void  
NetworkClan::Bank_RankCreate()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().CreateRank(m_BankRankUpdateName, m_BankRankSystemFlags);
	}
}

void  
NetworkClan::Bank_RankUpdate()
{
	CLiveManager::GetNetworkClan().UpdateRank(m_BankRankComboIndex, m_BankRankUpdateName, m_BankRankSystemFlags, 0);
}

void  
NetworkClan::Bank_RankMoveUp()
{
	CLiveManager::GetNetworkClan().UpdateRank(m_BankRankComboIndex, 0, -1, -1);
}

void  
NetworkClan::Bank_RankMoveDown()
{
	CLiveManager::GetNetworkClan().UpdateRank(m_BankRankComboIndex, 0, -1, +1);
}

void  
NetworkClan::Bank_RankDelete()
{
	CLiveManager::GetNetworkClan().DeleteRank(m_BankRankComboIndex);
}

void  
NetworkClan::Bank_RefreshRanks()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().RefreshRanks();
	}
}

void  
NetworkClan::Bank_InvitePlayerToPrimaryClan()
{
	if(!CLiveManager::IsOnline())
		return; 

	unsigned playerIndex = 0;

	unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
    const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();
        
    if(gnetVerifyf(numRemoteActivePlayers > 0, "No Active Remote players to invite"))
	{
		for(unsigned index = 0; index < numRemoteActivePlayers; index++)
		{
			const netPlayer *player = remoteActivePlayers[index];

			if(!player->IsBot())
			{
				if (playerIndex == (unsigned)m_BankPlayersComboIndex)
				{
					playerIndex = player->GetPhysicalPlayerIndex();

					CLiveManager::GetNetworkClan().InvitePlayer(playerIndex);

					return;
				}

				playerIndex++;
			}
		}
	}
}

void  
NetworkClan::Bank_InviteFriendToPrimaryClan()
{
	if(!CLiveManager::IsOnline())
		return; 

	if(gnetVerifyf(m_BankNumFriends > 0, "You have no friends to Invite"))
		CLiveManager::GetNetworkClan().InviteFriend(m_BankFriendsComboIndex);
}

void
NetworkClan::Bank_AcceptInvite()
{
	CLiveManager::GetNetworkClan().AcceptInvite(m_BankInvitesComboIndexReceived);
}

void
NetworkClan::Bank_RejectInvite()
{
	CLiveManager::GetNetworkClan().RejectInvite(m_BankInvitesComboIndexReceived);
}

void
NetworkClan::Bank_CancelInvite()
{
	CLiveManager::GetNetworkClan().CancelInvite(m_BankInvitesComboIndexSent);
}

void
NetworkClan::Bank_KickMember()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().KickMember(m_BankMembersComboIndex);
	}
}

void
NetworkClan::Bank_RefreshMembers()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().RefreshMembers();
	}
}

void
NetworkClan::Bank_RefreshWall()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().RefreshWall();
	}
}

void
NetworkClan::Bank_DeleteFromWall()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().DeleteFromWall(m_BankWallComboIndex);
	}
}

void
NetworkClan::Bank_PostToWall()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	const rlClanMembershipData* clan = CLiveManager::GetNetworkClan().GetMembership(gi->GetGamerHandle(), m_BankMembershipComboIndex);
	if (clan && clan->IsValid())
	{
		const rlClanDesc* primaryclan = CLiveManager::GetNetworkClan().GetPrimaryClan(gi->GetGamerHandle());
		if (!primaryclan || primaryclan->m_Id != clan->m_Clan.m_Id)
		{
			gnetError("PRIMARY LOCAL CLAN DOESNT MATCH THIS INDEX - USE WIDGET TO SET PRIMARY CLAN");
			return;
		}

		CLiveManager::GetNetworkClan().PostToWall(m_BankWallNewMessage);
	}
}

void  
NetworkClan::Bank_RefreshMemberships()
{
	if(!CLiveManager::IsOnline())
		return; 

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();
	CLiveManager::GetNetworkClan().RefreshMemberships(gi->GetGamerHandle(), 0);

	m_bank_UpdateClanDetailsRequested = true;
}

void
NetworkClan::Bank_DumpMembershipInfo()
{
	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();

	u32 membershipCount = CLiveManager::GetNetworkClan().GetMembershipCount(pGamerInfo->GetGamerHandle());
	gnetDebug1("---------------%d Memberships----------------", membershipCount);
	for (u32 i=0; i<membershipCount; i++)
	{
		const rlClanMembershipData* pMemberhsip = GetMembership(pGamerInfo->GetGamerHandle(), i);
		
		gnetDebug1("%" I64FMT "d [%s] : %s %s", pMemberhsip->m_Clan.m_Id, pMemberhsip->m_Clan.m_ClanTag, pMemberhsip->m_Clan.m_ClanName, pMemberhsip->m_IsPrimary ? "(PRIM)" : "");
	}
}


void  
NetworkClan::Bank_RefreshPlayerMemberships()
{
	unsigned playerIndex = 0;

	unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
    const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();
    
    for(unsigned index = 0; index < numRemoteActivePlayers; index++)
    {
        const netPlayer *player = remoteActivePlayers[index];

		if(!player->IsBot())
		{
			if (playerIndex == (unsigned)m_BankPlayersComboIndex)
			{
				CLiveManager::GetNetworkClan().RefreshMemberships(player->GetGamerInfo().GetGamerHandle(), 0);
				break;
			}

			playerIndex++;
		}
	}
}

void  
NetworkClan::Bank_RefreshFriendMemberships()
{
	rlFriend& pfriend = CLiveManager::GetFriendsPage()->m_Friends[m_BankFriendsComboIndex];
	rlGamerHandle handle;
	pfriend.GetGamerHandle(&handle);
	if (handle.IsValid())
	{
		CLiveManager::GetNetworkClan().RefreshMemberships(handle, 0);
	}
}

void
NetworkClan::Bank_JoinOpenClan()
{
	CLiveManager::GetNetworkClan().JoinOpenClan(m_BankOpenClansComboIndex);
}

void 
NetworkClan::Bank_SearchForFindClans()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (gnetVerify(rlClan::IsServiceReady(localGamerIndex)))
	{
		NetworkClanOp* pNetClanOp = GetAvailableClanOp(OP_CLAN_SEARCH);
		if (gnetVerifyf(pNetClanOp, "No Available Clan Ops to start new one"))
		{
			gnetVerify(rlClan::GetAllByClanName(localGamerIndex
				,m_BankFindPage
				,m_BankFindStr
				,&m_OpenClan[0]
				,MAX_NUM_CLANS
				,&m_OpenClanCount
				,&m_OpenClanTotalCount
				,pNetClanOp->GetStatus()));
		}
	}

	return;
}

void NetworkClan::Bank_RequestMetaData()
{
	m_Bank_CrewMetadata.Clear();
	m_Bank_CrewMetadata.SetClanId(m_Bank_MetaDataclanId);
}
void NetworkClan::Bank_PringDebugMetadata()
{
	m_Bank_CrewMetadata.DebugPrint();
}
void NetworkClan::Bank_PrintLocalPrimaryMetadata()
{
	m_primaryCrewMetadata.DebugPrint();
}


void NetworkClan::Bank_MakeFakeRecvdInvite()
{
	rlGamerHandle lclgmr = NetworkInterface::GetLocalGamerHandle();
	m_InvitesReceived[0].m_Clan = m_LocalPlayerDesc;
	safecpy(m_InvitesReceived[0].m_Inviter.m_Gamertag, "Hellokitty");
	safecpy(m_InvitesReceived[0].m_Message, "This is the message that came with the invite");
	m_InvitesReceived[0].m_Id = 1;
	m_InvitesReceived[0].m_Inviter.m_RockstarId = 1; 
	m_InvitesReceived[0].m_Inviter.m_GamerHandle = lclgmr;
	m_InvitesReceived[0].m_Invitee = m_InvitesReceived[0].m_Inviter;

	m_InvitesReceivedCount = 1;
}

#endif  //__BANK


void NetworkClan::NetworkClanOp::Clear() 
{
#if !__NO_OUTPUT
	if (m_Operation > OP_NONE)
		gnetDebug1("Clear clan operation '%s'", GetOperationName(m_Operation));
#endif // !__NO_OUTPUT

	Reset(OP_NONE); 
}

void NetworkClan::NetworkClanOp::Reset(eNetworkClanOps op)
{
	if (m_NetStatus.Pending())
	{
		rlClan::Cancel(&m_NetStatus);
	}

	m_Operation = op;
	m_NetStatus.Reset();

	if (m_pOpData)
	{
		delete m_pOpData;
	}
	m_pOpData = NULL;
	m_Processed=false;
	m_NeedProcessSucceed=false;
}

NetworkClan::NetworkClanOp* NetworkClan::GetAvailableClanOp( eNetworkClanOps opType )
{
	for(int i = 0; i < MAX_NUM_CLAN_OPS; ++i)
	{
		NetworkClanOp& rOp = m_OpPool[i];
		if (rOp.HasNoOp())
		{
			gnetDebug1("Set clan operation '%s' in index='%d'", GetOperationName(opType), i);

			rOp.Reset(opType);

			return &rOp;
		}
	}

#if !__NO_OUTPUT
	gnetDebug1("No Clan operation available.");
	for(int i=0; i<MAX_NUM_CLAN_OPS; ++i)
		gnetDebug1("Clan operation '%s' in progress in index='%d'", GetOperationName(m_OpPool[i].GetOp()), i);
#endif // !__NO_OUTPUT

	return NULL;
}

bool NetworkClan::CheckForAnyOpsOfType( eNetworkClanOps opType ) const
{
	for(int i = 0; i < MAX_NUM_CLAN_OPS; ++i)
	{
		if( (m_OpPool[i].Pending() || !m_OpPool[i].HasBeenProcessed() ) && m_OpPool[i].GetOp() == opType)
		{
			return true;
		}
	}

	return false;
}

void NetworkClan::OnRosEvent( const rlRosEvent& evnt)
{
	const unsigned evtId = evnt.GetId();

	switch (evtId)
	{
	case RLROS_EVENT_LINK_CHANGED:
		{
			gnetDebug1("NetworkClan::OnRosEvent(RLROS_EVENT_LINK_CHANGED)");
			const rlRosEventLinkChanged& lc = static_cast<const rlRosEventLinkChanged&>(evnt);
			const int eventLocalGamerIdx = lc.m_LocalGamerIndex;

			if (NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetLocalGamerIndex() == eventLocalGamerIdx)
			{
				if (lc.m_RockstarId==InvalidRockstarId)  // m_RockstarId invalid means we unlinked our account.
				{
					gnetDebug1("RockstarId invalid, we unlinked our account, cleaning localPlayerDesc.");
					m_LocalPlayerDesc.Clear();
					if (StatsInterface::IsKeyValid(STAT_ACTIVE_CREW))
					{
						StatsInterface::SetStatData(STAT_ACTIVE_CREW, (u64)RL_INVALID_CLAN_ID, STATUPDATEFLAG_ASSERTONLINESTATS);
					}
					else
					{
						m_pendingTimerSetActiveCrew = sysTimer::GetSystemMsTime() + TIMER_TO_RESET_STAT_ACTIVE_CREW;
					}
					m_primaryCrewMetadata.Clear();
				}
			}
		}
	case RLROS_EVENT_ONLINE_STATUS_CHANGED:
		{
			gnetDebug1("NetworkClan::OnRosEvent(RLROS_EVENT_ONLINE_STATUS_CHANGED)");

			const rlRosEventOnlineStatusChanged& changeEvnt = static_cast<const rlRosEventOnlineStatusChanged&>(evnt);
			int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

			// Matches our local gamer index, or we have no local gamer index
			if((changeEvnt.m_LocalGamerIndex == localGamerIndex || !RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)) &&
				// is a valid local gamer index
				RL_IS_VALID_LOCAL_GAMER_INDEX(changeEvnt.m_LocalGamerIndex) &&
				// is online
				rlRos::IsOnline(changeEvnt.m_LocalGamerIndex) &&
				// is valid credentials
				rlRos::GetCredentials(changeEvnt.m_LocalGamerIndex).IsValid() &&
				// is valid rockstar id
				changeEvnt.m_RockstarId != InvalidRockstarId)
			{
				gnetDebug1("Request Crew Invites Refresh because local index changed (%i)", changeEvnt.m_LocalGamerIndex );
				m_bRequestMaintenanceInviteRefresh = true;
				m_bRequestMaintenanceJoinRequestRefresh = true;
			}
		}
		break;
	}
}

void NetworkClan::AddDelegate( Delegate* dlgt )
{
	m_Delegator.AddDelegate(dlgt);
}

void NetworkClan::RemoveDelegate( Delegate* dlgt )
{
	m_Delegator.RemoveDelegate(dlgt);
}

void NetworkClan::DispatchEvent( const NetworkClanOp& op )
{
	NetworkClanEvent e(op.GetOp(), op.GetStatusRef());
	gnetDebug1("Dispatching NetworkClanEvent %s, Status: %d", GetOperationName(e.m_opType), e.m_statusRef.GetStatus());
	m_Delegator.Dispatch(e);
}

void NetworkClan::NetworkClanOp_FriendEventData::HandleSuccess()
{
	const char* friendName = "";
	#if RSG_DURANGO
	if (m_displayName[0]!='\0')
		friendName = m_displayName;
	else
		friendName = m_gamertag;
	#else
	friendName = m_gamertag;
	#endif

	gnetDebug1("Received NetworkClanOp_FriendEventData to crew %s [%d] from %s",m_clanDesc.m_ClanTag, (int)m_clanDesc.m_Id, friendName);
	CGameStream* GameStream = CGameStreamMgr::GetGameStream();
	if( GameStream != NULL )
	{
		
		char FinalString[2 * MAX_CHARS_IN_MESSAGE];

		char *pMainString = m_op == JOINED ? TheText.Get("CREW_FEED_FRIEND_JOINED_CREW") : TheText.Get("CREW_FEED_FRIEND_CREATED_CREW");

		CSubStringWithinMessage aSubStrings[2];
		aSubStrings[0].SetLiteralString(friendName, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
		aSubStrings[1].SetLiteralString(m_clanDesc.m_ClanName, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
		CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
			NULL, 0, 
			aSubStrings, 2, 
			FinalString, NELEM(FinalString) );
		
		GameStream->PostCrewTag( !m_clanDesc.m_IsOpenClan, false, m_clanDesc.m_ClanTag, 0, false, FinalString, true, m_clanDesc.m_Id, NULL, Color32(m_clanDesc.m_clanColor) );
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CGamersPrimaryClanData::Start( rlGamerHandle* aGamers, const unsigned nGamers )
{
	bool result = false;

	gnetAssertf(Idle(), "Invalid status %d", m_crews.m_RequestStatus.GetStatus());
	if (!Idle())
		return result;

	if(gnetVerify(aGamers) && gnetVerify(nGamers<=RL_MAX_GAMERS_PER_SESSION) && gnetVerify(m_gamers.size() == 0))
	{
		for(unsigned i=0; i<nGamers; ++i)
			m_gamers.Grow() = aGamers[i];

		result = rlClan::GetPrimaryClans(NetworkInterface::GetLocalGamerIndex()
										,m_gamers.GetElements()
										,m_gamers.GetCount()
										,m_crews.m_Data.GetElements()
										,&m_crews.m_iResultSize
										,NULL
										,&m_crews.m_RequestStatus);

		gnetAssertf(result, "Unable to request my GAMERS's memberships for some reason!");
	}

	return result;
}

void  CGamersPrimaryClanData::Clear( )
{
	if ( gnetVerify(!m_crews.m_RequestStatus.Pending()) )
	{
		m_crews.Clear();
		m_gamers.Reset();
		m_crews.m_RequestStatus.Reset();
	}
}

void  CGamersPrimaryClanData::Cancel( )
{
	if ( gnetVerify(m_crews.m_RequestStatus.Pending()) )
	{
		rlClan::Cancel(&m_crews.m_RequestStatus);
	}
}

//eof


