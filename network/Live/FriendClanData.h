//
// ClanNetStatusArray.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef FRIEND_CLAN_DATA
#define FRIEND_CLAN_DATA

// rage
#include "rline\clan\rlclancommon.h"
#include "rline/rlfriendsmanager.h"
#include "data\base.h"

// game
#include "Frontend/CCrewMenu.h" // for VISIBLE_CREW_ITEMS
#include "Network/Live/ClanNetStatusArray.h"

class CGroupClanData: public datBase
{
public:
	virtual ~CGroupClanData() {}

	virtual bool Init(); // Returns true if it's already up to date.
	virtual void Clear();

	virtual int GetResultSize() const = 0;
	virtual const netStatus& GetStatus() const = 0;
	virtual netStatus& GetStatus() = 0;

	virtual const rlClanMember& operator[](int index) const = 0;
	virtual rlClanMember& operator[](int index) = 0;

	virtual bool IsDataSourceReady() const = 0;

	int GetGroupCountForClan(rlClanId thisClan);
	int GetGroupClanCount() { PerformFixup(); return m_groupSizes.GetCount(); }

	const rlClanDesc* GetGroupClanDesc(int index) 
	{
		PerformFixup(); 
		if( index < 0 || index > m_groupSizes.GetCount() ) 
			return NULL;
		return m_groupSizes[index].m_pClanData; 
	}


	struct GroupMembership
	{
		GroupMembership() : m_pClanData(NULL), m_GroupMembersInClan(0) {};

		rlClanDesc*	m_pClanData;
		int			m_GroupMembersInClan;
	};

protected:
	void PerformFixup();

	virtual u32& GetResultSizeRef() = 0;
	virtual rlClanMember* GetResultElements() = 0;
	virtual void FillGroupArray(atArray<rlGamerHandle>& group) = 0;

	atArray<rlGamerHandle> m_myGroup;
	atArray<GroupMembership>	m_groupSizes;
};


class CFriendClanData: public CGroupClanData
{
public:
	virtual void Clear();

	virtual int GetResultSize() const {return m_groupCrew.m_iResultSize;}
	virtual const netStatus& GetStatus() const {return m_groupCrew.m_RequestStatus;}
	virtual netStatus& GetStatus() {return m_groupCrew.m_RequestStatus;}

	virtual const rlClanMember& operator[](int index) const { return m_groupCrew[index]; }
	virtual rlClanMember& operator[](int index)       { return m_groupCrew[index]; }

	virtual bool IsDataSourceReady() const { return true; }

protected:
	virtual u32& GetResultSizeRef() {return m_groupCrew.m_iResultSize;}
	virtual rlClanMember* GetResultElements() {return m_groupCrew.m_Data.GetElements();}
	virtual void FillGroupArray(atArray<rlGamerHandle>& group);

	ClanNetStatusArray<rlClanMember, rlFriendsPage::MAX_FRIEND_PAGE_SIZE> m_groupCrew;
};

class CCrewMembersClanData: public CGroupClanData
{
public:
	CCrewMembersClanData() : CGroupClanData(), m_LastFetchCrew(RL_INVALID_CLAN_ID) {};

	void SetLastCrew(rlClanId newClan) { m_LastFetchCrew = newClan; }

	virtual bool Init();
	virtual void Clear();
	virtual bool IsDataSourceReady() const;

	virtual int GetResultSize() const {return m_groupCrew.m_iResultSize;}
	virtual const netStatus& GetStatus() const {return m_groupCrew.m_RequestStatus;}
	virtual netStatus& GetStatus() {return m_groupCrew.m_RequestStatus;}

	virtual const rlClanMember& operator[](int index) const { return m_groupCrew[index]; }
	virtual rlClanMember& operator[](int index)       { return m_groupCrew[index]; }


protected:
	virtual void FillGroupArray(atArray<rlGamerHandle>& group);

	virtual u32& GetResultSizeRef() {return m_groupCrew.m_iResultSize;}
	virtual rlClanMember* GetResultElements() {return m_groupCrew.m_Data.GetElements();}

	ClanNetStatusArray<rlClanMember, VISIBLE_CREW_ITEMS * CREW_UI_PAGE_COUNT> m_groupCrew;
	rlClanId m_LastFetchCrew;
};


class CPartyClanData: public CGroupClanData
{
public:
	virtual void Clear();

	virtual int GetResultSize() const {return m_groupCrew.m_iResultSize;}
	virtual const netStatus& GetStatus() const {return m_groupCrew.m_RequestStatus;}
	virtual netStatus& GetStatus() {return m_groupCrew.m_RequestStatus;}

	virtual const rlClanMember& operator[](int index) const { return m_groupCrew[index]; }
	virtual rlClanMember& operator[](int index)       { return m_groupCrew[index]; }

	virtual bool IsDataSourceReady() const { return true; }

protected:
	virtual u32& GetResultSizeRef() {return m_groupCrew.m_iResultSize;}
	virtual rlClanMember* GetResultElements() {return m_groupCrew.m_Data.GetElements();}
	virtual void FillGroupArray(atArray<rlGamerHandle>& group);

	ClanNetStatusArray<rlClanMember, RL_MAX_GAMERS_PER_SESSION> m_groupCrew;
};


#endif // FRIEND_CLAN_DATA

//eof
