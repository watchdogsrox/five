// 
// networkleaderboardsessionmgr.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
//

#ifndef NETWORK_LEADERBOARD_SESSION_MGR_H
#define NETWORK_LEADERBOARD_SESSION_MGR_H

//Rage headers
#include "rline/rlgamerinfo.h"
#include "rline/clan/rlclancommon.h"

//Framework Headers
#include "fwnet/nettypes.h"
#include "fwnet/netleaderboardmgr.h"
#include "fwnet/netleaderboardwrite.h"
#include "fwnet/netleaderboardcommon.h"
#include "Script/script.h"

namespace rage
{
	class rlSession;
};


class CLeaderboardScriptDisplayData
{
public:
	struct sStatValue
	{
	public:
		sStatValue() {Clear();}

		void Clear()
		{
			m_IntVal = 0;
			m_FloatVal = 0;
		}

		union{int m_IntVal;float m_FloatVal;};
	};

	struct sRowData
	{
	public:
		scrTextLabel63         m_GamerName;
		scrTextLabel63         m_CoDriverName;
		rlGamerHandle          m_GamerHandle;
		rlGamerHandle          m_CoDriverHandle;
		bool                   m_CustomVehicle;
		int                    m_Rank;
		int                    m_RowFlags;

		typedef atFixedArray< sStatValue, 32 > ColumnList;
		ColumnList  m_Columns;

	public:
		sRowData() {Clear();}
		~sRowData() {Clear();}

		void Clear()
		{
			m_GamerName[0] = '\0';
			m_CoDriverName[0] = '\0';
			m_GamerHandle.Clear();
			m_CoDriverHandle.Clear();
			m_CustomVehicle = false;
			m_Rank = -1;
			m_Columns.clear();
			m_Columns.Reset();
			m_RowFlags = 0;
		}
	};

public:
	int m_Id;
	int m_ColumnsBitSets;
	u32 m_Time;

	typedef atFixedArray < sRowData, 12 > RowList;
	RowList m_Rows;

public:
	CLeaderboardScriptDisplayData() {Clear();}
	~CLeaderboardScriptDisplayData() {Clear();}

	void  Clear()
	{
		m_Time = 0;
		m_Id = -1;
		m_Rows.clear();
		m_Rows.Reset();
	}
};


class CNetworkReadLeaderboards : public netLeaderboardReadMgr
{
private:
	bool  m_Initialized;

public:
	CNetworkReadLeaderboards();
	~CNetworkReadLeaderboards();

	virtual void  Init(sysMemAllocator* allocator);
	virtual void  Shutdown();
	virtual void  Update();

	//Read statistics for players in the session.
	const netLeaderboardRead*
		ReadByGamer(const unsigned id
					,const rlLeaderboard2Type type
					,const rlLeaderboard2GroupSelector& groupSelector
					,const rlClanId clanId
					,const unsigned numGroups, const rlLeaderboard2GroupSelector* groupSelectors);

	//Read statistics for all friends.
	const netLeaderboardRead*
		ReadByFriends(const unsigned id
						,const rlLeaderboard2Type type
						,const rlLeaderboard2GroupSelector& groupSelector
						,const rlClanId clanId
						,const unsigned numGroups, const rlLeaderboard2GroupSelector* groupSelectors
						,const bool includeLocalPlayer
						,const int friendStartIndex
						,const int maxNumFriends);

	//Clear read data.
	void  ClearAll();

	//Clear read data.
	void  ClearLeaderboardRead(const unsigned id, const rlLeaderboard2Type type, const int lbIndex);
	void  ClearLeaderboardRead(const netLeaderboardRead* pThisLb);

	//PURPOSE
	//Cache script leaderboard display data.
	bool  CacheScriptDisplayData(const int id, const int columnsBitSets, CLeaderboardScriptDisplayData::sRowData& row);
	void  ClearCachedDisplayData();
	bool  ClearCachedDisplayDataById(const int id);

	typedef atFixedArray < CLeaderboardScriptDisplayData, 5 > LeaderboardScriptDisplayDataList;
	const LeaderboardScriptDisplayDataList& GetDisplayData() const {return m_ScriptDisplayData;}

private:

	//Cached Leaderboards display data
	LeaderboardScriptDisplayDataList  m_ScriptDisplayData;

#if __BANK

public:
	void  InitWidgets(bkBank* pBank);
	void  UpdateWidgets();

	void  Bank_ReadStatistics();
	void  Bank_PrintTop();

#endif // __BANK
};

class CNetworkWriteLeaderboards : public netLeaderboardWriteMgr
{
private:
	bool  m_Initialized;
	bool  m_matchEnded;

public:
	CNetworkWriteLeaderboards() : m_Initialized(false), m_matchEnded(false)
	{}
	~CNetworkWriteLeaderboards()
	{
		Shutdown();
	}

	void          Init(sysMemAllocator* allocator);
	void          Shutdown();
	virtual	void  Update();
	virtual bool  Flush();

	void MatchEnded() {m_matchEnded=true;}

protected:
	void DoFlush();

	enum State
	{
		STATE_NONE,
		STATE_FLUSH,
	} m_State;

#if __BANK

public:
	static const u32 BANK_MAX_COLUMNS = 71;

	int       m_BankUpdateColumns;
	int       m_Valuei;
	float     m_Valuef;

	void  InitWidgets(bkBank* pBank);
	void  Bank_UploadStatistics();
	void  Bank_FinishUploadStatistics();
	void  Bank_ClearData();

#endif
};

class CNetworkLeaderboardMgr : public datBase
{
public:
	CNetworkLeaderboardMgr();
	~CNetworkLeaderboardMgr();

	void Init(sysMemAllocator* allocator);
	void Shutdown();
	void Update();

	bool Pending();

	//PURPOSE
	//  Access to session leaderboard read manager.
	CNetworkReadLeaderboards& GetLeaderboardReadMgr() { return m_LeaderboardReadMgr; }

	//PURPOSE
	//  Access to session leaderboard write manager.
	CNetworkWriteLeaderboards& GetLeaderboardWriteMgr() { return m_LeaderboardWriteMgr; }

#if __BANK
	void   InitWidgets(bkBank* pBank);

#endif

private:
	
	CNetworkReadLeaderboards m_LeaderboardReadMgr;
	CNetworkWriteLeaderboards m_LeaderboardWriteMgr;

	sysMemAllocator* m_pAllocator;
	bool m_bInitialized;

#if __BANK
	void	Bank_Update();
#endif
};


#endif // NETWORK_LEADERBOARD_SESSION_MGR_H

//eof
