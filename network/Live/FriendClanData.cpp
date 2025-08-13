//
// FriendClanData.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

//game
#include "Network/Live/FriendClanData.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Network/NetworkInterface.h"
#include "Network/Sessions/NetworkSession.h"
#include "Frontend/ui_channel.h"

FRONTEND_NETWORK_OPTIMISATIONS()
//OPTIMISATIONS_OFF();

bool CGroupClanData::Init()
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) || !rlClan::IsServiceReady(localGamerIndex) )
		return false;

	// Fetch my FRIEND's memberships
	if( !GetStatus().Pending() && IsDataSourceReady() )
	{
		atArray<rlGamerHandle> myNewGroup;
		FillGroupArray(myNewGroup);

		bool needsRefresh = false;
		if(GetStatus().Succeeded())
		{
			if(myNewGroup.size() != m_myGroup.size())
			{
				needsRefresh = true;
			}
			else
			{
				for(int newIndex=0; newIndex<myNewGroup.size() && !needsRefresh; ++newIndex)
				{
					bool found = myNewGroup[newIndex] == m_myGroup[newIndex];

					// The order of the friends list periodically changes, so we'll have to search through the whole list if we haven't found anything.
					for(int oldIndex=0; oldIndex<m_myGroup.size() && !found; ++oldIndex)
					{
						if(myNewGroup[newIndex] == m_myGroup[newIndex])
						{
							found = true;
						}
					}

					if(!found)
					{
						needsRefresh = true;
					}
				}
			}
		}
		else
		{
			needsRefresh = true;
		}

		if(needsRefresh)
		{
			GetStatus().Reset();
			m_myGroup = myNewGroup;

			if( myNewGroup.size() > 0)
			{
				gnetVerifyf(
					rlClan::GetPrimaryClans(NetworkInterface::GetLocalGamerIndex(), myNewGroup.GetElements(), myNewGroup.GetCount()
					, GetResultElements(), &GetResultSizeRef(), NULL, &GetStatus())
					, "Unable to request my FRIEND's memberships for some reason!");
			}
			else
			{
				// no friends, instant success
				GetStatus().ForceSucceeded();
			}
		}
	}

	return GetStatus().Succeeded();
}

void CGroupClanData::Clear()
{
	m_groupSizes.Reset();
	m_myGroup.Reset();
}

// PURPOSE: Specialized functors for rlClanId keys.
template <>
struct atMapHashFn<rlClanId>
{
	unsigned operator ()(rlClanId key) const { return atHash64(static_cast<u64>(key)); }
};

template <>
struct atMapEquals<rlClanId>
{
	bool operator ()(const rlClanId left, const rlClanId right) const { return (left == right); }
};


int SortFriends(const CGroupClanData::GroupMembership* a, const CGroupClanData::GroupMembership* b)
{
	// sort by size first. More friends = more fun
	if( a->m_GroupMembersInClan < b->m_GroupMembersInClan )
		return -1;

	if( a->m_GroupMembersInClan > b->m_GroupMembersInClan )
		return 1;

	// failing that, sort by clan name (if available, which it totally should)
	if( uiVerifyf( a->m_pClanData && b->m_pClanData, "Can't compare FriendMemberships without no clan datas!") )
	{
		return strcmp(a->m_pClanData->m_ClanName, b->m_pClanData->m_ClanName );
	}

	return 0;
}



void CGroupClanData::PerformFixup()
{
	// if we have no friends, OR we've already done this, then skip it
	if( GetResultSize() == 0 || !m_groupSizes.empty() )
		return;

	// PREPARE FRIENDSHIP STUFF
	// That is, cull duplicates and count friends

	typedef atMap<rlClanId, GroupMembership> OccupancyMap;
	OccupancyMap tempMap;

	// get a unique list of clans and memberships our friends are in
	for(int i=0; i < GetResultSize(); ++i)
	{
		GroupMembership& curEntry = tempMap[ (*this)[i].m_MemberClanInfo.m_Clan.m_Id ];
		// just set it to the last occurrence of the clanDesc; they'll all be the same anyway
		curEntry.m_pClanData = &(*this)[i].m_MemberClanInfo.m_Clan;
		++curEntry.m_GroupMembersInClan;
	}

	// now allocate enough space in our sorted array
	uiAssert(m_groupSizes.empty());
	m_groupSizes.Resize(tempMap.GetNumUsed());

	// copy the contents over
	OccupancyMap::Iterator entry = tempMap.CreateIterator();
	int iCount = 0;
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		m_groupSizes[iCount++] = *entry;
	}

	// sort the new keys
	m_groupSizes.QSort(0,-1, SortFriends);
}

int CGroupClanData::GetGroupCountForClan(rlClanId thisClan)
{
	PerformFixup();

	for(int i=0; i < m_groupSizes.GetCount(); ++i )
	{
		if( m_groupSizes[i].m_pClanData->m_Id == thisClan )
		{
			return m_groupSizes[i].m_GroupMembersInClan;
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void CFriendClanData::Clear()
{
	m_groupCrew.Clear();
	CGroupClanData::Clear();
}

void CFriendClanData::FillGroupArray(atArray<rlGamerHandle>& group)
{
	CLiveManager::GetFriendsPage()->FillFriendArray(group);
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
bool CCrewMembersClanData::Init()
{
	CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CLAN));
	if( pCard->GetClan() != m_LastFetchCrew )
		m_groupCrew.Clear();

	return CGroupClanData::Init();
}


void CCrewMembersClanData::Clear()
{
	m_LastFetchCrew = RL_INVALID_CLAN_ID;
	m_groupCrew.Clear();
	CGroupClanData::Clear();
}

bool CCrewMembersClanData::IsDataSourceReady() const
{
	CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CLAN));
	return !pCard || pCard->IsValid();
}

void CCrewMembersClanData::FillGroupArray(atArray<rlGamerHandle>& group)
{
	CClanPlayerCardDataManager* pCard = static_cast<CClanPlayerCardDataManager*>(SPlayerCardManager::GetInstance().GetDataManager(CPlayerCardManager::CARDTYPE_CLAN));

	if( uiVerifyf(pCard, "Not sure how there's no ClanPlayerCardManager...") )
	{
		const atArray<rlGamerHandle>& rAllGamerHandles = pCard->GetAllGamerHandles();

		group = rAllGamerHandles;
	}

}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void CPartyClanData::Clear()
{
	m_groupCrew.Clear();
	CGroupClanData::Clear();
}

void CPartyClanData::FillGroupArray(atArray<rlGamerHandle>&)
{
	/*if(NetworkInterface::IsNetworkOpen() && CNetwork::GetNetworkSession().IsInParty())
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetPartyMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(unsigned i=0; i<nGamers; ++i)
		{
			group.Grow() = aGamers[i].GetGamerHandle();
		}
	}*/
}

//eof
