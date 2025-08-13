//
// PlayerCardDataManager.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef PLAYER_CARD_DATA_MANAGER_H
#define PLAYER_CARD_DATA_MANAGER_H

// rage
#include "atl/atfixedstring.h"
#include "atl/hashstring.h"
#include "atl/singleton.h"
#include "atl/string.h"
#include "net/status.h"
#include "rline/clan/rlclancommon.h"
#include "rline/presence/rlpresencequery.h"
#include "rline/profilestats/rlprofilestatscommon.h"
#include "rline/ugc/rlugc.h"
#include "string/stringhash.h"
#include "vector/color32.h"

// game
#include "Frontend/hud_colour.h"
#include "Frontend/SimpleTimer.h"

#define PLAYER_CARD_DEFAULT_COLOR HUD_COLOUR_PURE_WHITE
#define PLAYER_CARD_DEFAULT_COLOR_STR "HUD_COLOUR_PURE_WHITE"
#define PLAYER_CARD_INVALID_TEAM_ID (-101)
#define PLAYER_CARD_SOLO_TEAM_ID (-1)
#define PLAYER_CARD_NUMBER_OF_STATS (7)


#define MAX_PROFILE_STATS_FOR_SP_FRIENDS		(40)		// currently 28 stats are read from playerCardSetup.xml
#define MAX_PROFILE_STATS_FOR_MP_FRIENDS		(320)		// currently 308 stats are read from playerCardSetup.xml
#define MAX_PROFILE_STATS_FOR_SP_CREWS			(30)		// currently 18 stats are read from playerCardSetup.xml
#define MAX_PROFILE_STATS_FOR_MP_CREWS			(260)		// currently 249 stats are read from playerCardSetup.xml
#define MAX_PROFILE_STATS_FOR_MP_PLAYERS		(1)			// currently no stats are read from playerCardSetup.xml

#define MAX_FRIEND_STAT_QUERY (16)

// If you try to have a different number here than MAX_FRIEND_STAT_QUERY, that won't actually work right now. See what's done in CClanPlayerCardDataManager
#define MAX_CREW_MEMBERS_STAT_QUERY MAX_FRIEND_STAT_QUERY

namespace rage
{
	class parTreeNode;
}

class CNetGamePlayer;

class PlayerCardStatInfo
{
public:
	struct StatData
	{
#if !__NO_OUTPUT
		atString m_name;
#endif
		bool m_isSP;
		bool m_isMulti;

		StatData(){ Reset(); }

		void Reset()
		{
			m_isSP = true;
			m_isMulti = false;
#if !__NO_OUTPUT
			m_name = "";
#endif
		}
	};


	void Init(bool isSP);
	void Reset();

	void AddStat(const char* pStatName, bool isSP, bool isMulti = false);
	void AddMultiStat(const char* pStatName, bool isSP);

	const atArray<StatData>& GetStatDatas() const {return m_statDatas;}
	const atArray<int>& GetStatHashes() const {return m_statHashes;}

private:
	bool m_isSP;

	atArray<StatData> m_statDatas;
	atArray<int> m_statHashes;
};

class UGCRanks
{
public:
	struct Rank
	{
		float m_value;
		eHUD_COLOURS m_color;

		Rank()
		{
			m_value = 0.0f;
			m_color = HUD_COLOUR_WHITE;
		}
	};

	UGCRanks()
	{
		m_icon = 0;
		m_ranks.Reserve(5);
	}

	void AddRank(const Rank& rRank);
	eHUD_COLOURS GetRankColor(float currValue) const;

	int m_icon;

private:

	static int CompareFunc(const Rank* candidateA, const Rank* candidateB);

	atArray<Rank> m_ranks;
};

class PCardTitleListInfo
{
public:
	class TitleList
	{
	public:
		struct Title
		{
			float m_value;
			u32 m_titleHash;

			Title()
			{
				m_value = 0.0f;
				m_titleHash = 0;
			}
		};

		TitleList()
		{
			m_listName = 0;
			m_titles.Reserve(5);
		}

		void AddTitle(const Title& rTitle);
		u32 GetTitle(float currValue) const;
		void LoadDataXMLFile(parTreeNode* pNode);

		u32 m_listName;

	private:
		static int CompareFunc(const Title* candidateA, const Title* candidateB);

		atArray<Title> m_titles;
	};


	u32 GetTitle(u32 titleName, float currValue) const;
	void LoadDataXMLFile(parTreeNode* pNode);
	void Reset();

private:
	atArray<TitleList> m_titleLists;
};

class PlayerCardPackedStatInfo
{
public:
	struct StatData
	{
		atHashWithStringNotFinal m_alias;
		atHashWithStringNotFinal m_realName;
		u32 m_index;
		bool m_isInt;
		bool m_isSP;
		bool m_isMulti;

		StatData() { Reset(); }
		void Reset()
		{
			m_alias = "";
			m_realName = "";
			m_index = 0;
			m_isInt = false;
			m_isSP = true;
			m_isMulti = false;
		}
	};

	void Init(bool isSP);
	void Reset();

	void AddStat(const char* pStatAlias, const char* pStatRealName, bool isInt, u32 index, bool isSP, bool isMulti = false);
	void AddMultiStat(const char* pStatAlias, const char* pStatRealName, bool isInt, u32 index, bool isSP);

	const atArray<StatData>& GetStatDatas() const {return m_statDatas;}
	const StatData* GetStatData(u32 statHash) const;

private:

	bool m_isSP;
	atArray<StatData> m_statDatas;
};

class CPlayerCardXMLDataManager
{
public:

	struct CardStat
	{
		enum
		{
			MAX_CHARACTER_SLOTS = 5
		};

		CardStat()
		{
			for(int i=0; i<MAX_CHARACTER_SLOTS; ++i)
			{
				m_stats[i] = 0;
			}
		}

		u32 GetStat(int characterSlot) const;
		void SetStat(const char* pStat, bool isMultistat, bool isSP);

		bool IsStat(u32 statHash) const;

	private:
		atRangeArray<u32, MAX_CHARACTER_SLOTS> m_stats;
	};

	struct CardSlot
	{
		CardStat m_stat;
		CardStat m_extraStat;
		u32 m_textHash;
		u32 m_compareTextHash;
		u32 m_titleListHash;
		eHUD_COLOURS m_color;
		bool m_isRatio; // m_stat / m_extraStat
		bool m_isRatioStr;  // prints out "m_stat / m_extraStat"
		bool m_isSumPercent; // m_stat / (m_stat+m_extraStat)
		bool m_isCash;
		bool m_isPercent;
		bool m_showPercentBar;
		bool m_isMissionName;
		bool m_isCrew;

		CardSlot()
		{
			m_textHash = 0;
			m_compareTextHash = 0;
			m_titleListHash = 0;
			m_color = HUD_COLOUR_INVALID;
			m_isRatio = false;
			m_isRatioStr = false;
			m_isSumPercent = false;
			m_isCash = false;
			m_isPercent = false;
			m_showPercentBar = false;
			m_isMissionName = false;
			m_isCrew = false;
		}
	};

	struct PlayerCard
	{
		enum eDefaultValues
		{
			DEFAULT_TEAMID = -1,
			DEFAULT_RANK_ICON = 0,
		};

		enum eNetworkedStatIds
		{
			NETSTAT_INVALID = -1,
			NETSTAT_PACKED_BOOLS = 0, // This is a special packed stat.
			NETSTAT_START_TITLE_RATIO_STATS, // 1
			NETSTAT_TR_1 = NETSTAT_START_TITLE_RATIO_STATS,
			NETSTAT_TR_2, // 2
			NETSTAT_END_TITLE_RATIO_STATS = NETSTAT_TR_2,
			NETSTAT_START_PROFILESTATS, // 3
			NETSTAT_RANK = NETSTAT_START_PROFILESTATS,
			NETSTAT_ALLOWS_SPECTATING, // 4
			NETSTAT_IS_SPECTATING, // 5
			NETSTAT_CREWRANK, // 6
			NETSTAT_BAD_SPORT, // 7
			NETSTAT_START_MISC, // 8
			NETSTAT_MISC_0_A = NETSTAT_START_MISC,
			NETSTAT_MISC_1_A, // 9
			NETSTAT_MISC_2_A, // 10
			NETSTAT_MISC_3_A, // 11
			NETSTAT_MISC_4_A, // 12
			NETSTAT_MISC_5_A, // 13
			NETSTAT_MISC_6_A,
			NETSTAT_END_MISC = NETSTAT_MISC_6_A,
			NETSTAT_SYNCED_STAT_COUNT, // 14
			NETSTAT_PLANE_ACCESS, // 15
			NETSTAT_BOAT_ACCESS, // 16
			NETSTAT_HELI_ACCESS, // 17
			NETSTAT_CAR_ACCESS, // 18
			NETSTAT_TUT_COMPLETE, // 19
			NETSTAT_USING_KEYBOARD, // 20
			NETSTAT_TOTAL_STATS, // 21
			NETSTAT_PLANE_ACCESS_BIT = 0,
			NETSTAT_BOAT_ACCESS_BIT,
			NETSTAT_HELI_ACCESS_BIT,
			NETSTAT_CAR_ACCESS_BIT,
			NETSTAT_TUT_COMPLETE_BIT,
		};

		atArray<CardSlot> m_slots;
		atArray<CardSlot> m_titleSlots;
		CardStat m_rankStat;
		CardStat m_carStat;
		CardStat m_boatStat;
		CardStat m_heliStat;
		CardStat m_planeStat;
		#if RSG_PC
		CardStat m_usingKeyboard;
		#endif
		CardStat m_timestamp;

		u32 m_title;
		int m_teamId;
		eHUD_COLOURS m_color;
		int m_rankIcon;
		bool m_isSP;

		PlayerCard()
		{
			m_slots.Reserve(PLAYER_CARD_NUMBER_OF_STATS);
			m_titleSlots.Reserve(2);

			m_title = 0;
			m_teamId = DEFAULT_TEAMID;
			m_color = PLAYER_CARD_DEFAULT_COLOR;
			m_rankIcon = DEFAULT_RANK_ICON;
			m_isSP = false;
		}

		eNetworkedStatIds GetNetIdByStatHash(u32 statHash) const;
		u32 GetStatHashByNetId(eNetworkedStatIds statId, int characterSlot, bool extraStat = false) const;
	};

	enum eCardTypes
	{
		PCTYPE_SP,
		PCTYPE_FREEMODE,
		MAX_PCTYPES,
	};

	struct MiscData
	{
		CardStat m_jipUnlockStat;
		atHashWithStringBank m_characterSlotStat;
		atHashWithStringBank m_gameCompletionStat;
		atHashWithStringBank m_allowsSpectatingStat;
		#if RSG_PC
		atHashWithStringBank m_usingKeyboard;
		#endif
		atHashWithStringBank m_isSpectatingStat;
		atHashWithStringBank m_crewRankStat;
		atHashWithStringBank m_badSportStat;
		atHashWithStringBank m_badSportTunable;
		atHashWithStringBank m_dirtyPlayerTunable;
		atHashWithStringBank m_bitTag;
		atHashWithStringBank m_teamIdTag;
		float m_defaultBadSportLimit;
		float m_defaultDirtyPlayerLimit;
		int m_maxMenuSlots;
		int m_playingGTASPIcon;
		int m_playingGTAMPIcon;
		int m_playingGTAMPWithRankIcon;
		int m_kickIcon;
		int m_bountyIcon;
		int m_spectatingIcon;
		int m_inviteIcon;
		int m_inviteAcceptedIcon;
		int m_activeHeadsetIcon;
		int m_inactiveHeadsetIcon;
		int m_mutedHeadsetIcon;
		int m_friendMenuId;
		int m_emptySlotColor;
		int m_offlineSlotColor;
		int m_localPlayerSlotColor;
		int m_freemodeSlotColor;
		int m_inMatchSlotColor;
		int m_spSlotColor;
		int m_spectTeamId;
		int m_bountyBit;
		int m_spectatingBit;

		MiscData()
		{
			m_characterSlotStat = "";
			m_gameCompletionStat = "";
			m_allowsSpectatingStat = "";
			#if RSG_PC
			m_usingKeyboard = "";
			#endif
			m_isSpectatingStat = "";
			m_crewRankStat = "";
			m_badSportStat = "";
			m_badSportTunable = "";
			m_dirtyPlayerTunable = "";
			m_bitTag = "";
			m_teamIdTag = "";


			m_defaultBadSportLimit = 0.0f;
			m_defaultDirtyPlayerLimit = 0.0f;

			m_maxMenuSlots = 0;
			m_playingGTASPIcon = 0;
			m_playingGTAMPIcon = 0;
			m_playingGTAMPWithRankIcon = 0;
			m_kickIcon = 0;
			m_bountyIcon = 0;
			m_spectatingIcon = 0;
			m_inviteIcon = 0;
			m_inviteAcceptedIcon = 0;
			m_activeHeadsetIcon = 0;
			m_inactiveHeadsetIcon = 0;
			m_mutedHeadsetIcon = 0;
			m_friendMenuId = 0;
			m_emptySlotColor = 0;
			m_offlineSlotColor = 0;
			m_localPlayerSlotColor = 0;
			m_freemodeSlotColor = 0;
			m_inMatchSlotColor = 0;
			m_spSlotColor = 0;
			m_spectTeamId = 0;
			m_bountyBit = 0;
			m_spectatingBit = 0;
		}
	};

	CPlayerCardXMLDataManager() {Init();}
	~CPlayerCardXMLDataManager() {Reset();}

	void Init();
	void Reset();

	const PlayerCardStatInfo& GetSPFriendStats() const { return m_spFriends; }
	const PlayerCardStatInfo& GetMPFriendStats() const { return m_mpFriends; }
	const PlayerCardStatInfo& GetMPPlayersStats() const { return m_mpPlayers; }
	const PlayerCardStatInfo& GetSPCrewStats() const { return m_spCrew; }
	const PlayerCardStatInfo& GetMPCrewStats() const { return m_mpCrew; }
	const UGCRanks& GetMissionCreatorStats() const { return m_missionCreator; }
	const PlayerCardPackedStatInfo& GetSPPackedStats() const { return m_spPackedStats; }
	const PlayerCardPackedStatInfo& GetMPPackedStats() const { return m_mpPackedStats; }
	const PlayerCardPackedStatInfo& GetMPPackedPlayersStats() const { return m_mpPackedPlayersStats; }

	const PlayerCard& GetPlayerCard(eCardTypes type) const {return m_playerCards[type];}
	const PCardTitleListInfo& GetTitleInfo() const {return m_titles;}
	const MiscData& GetMiscData() const {return m_miscData;}

private:
	void LoadDataXMLFile();
	void LoadDataXMLFile(parTreeNode* pNode, PlayerCardStatInfo& out_info, const bool cIsSP);
	void LoadDataXMLFile(parTreeNode* pNode, UGCRanks& out_info);
	void LoadDataXMLFile(parTreeNode* pNode, PlayerCardPackedStatInfo& out_info, const bool cIsSP);
	void LoadDataXMLFile(parTreeNode* pNode, eCardTypes cardType);
	void LoadDataXMLFile(parTreeNode* pNode, MiscData& out_miscData);

	PlayerCardStatInfo m_spFriends;
	PlayerCardStatInfo m_mpFriends;
	PlayerCardStatInfo m_spCrew;
	PlayerCardStatInfo m_mpCrew;
	PlayerCardStatInfo m_mpPlayers; // Empty since the players network the needed stats
	UGCRanks m_missionCreator;
	PlayerCardPackedStatInfo m_spPackedStats;
	PlayerCardPackedStatInfo m_mpPackedStats;
	PlayerCardPackedStatInfo m_mpPackedPlayersStats; // Empty since the players network the needed stats
	PCardTitleListInfo m_titles;
	MiscData m_miscData;

	atRangeArray<PlayerCard, MAX_PCTYPES> m_playerCards;
};

typedef atSingleton<CPlayerCardXMLDataManager> SPlayerCardXMLDataManager;


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class BasePlayerCardDataManager
{
public:
	enum
	{
		MAX_UGC_QUERY = 100,
		MAX_STATS = 800,
		MAX_GAMERS_READS = 33
	};

	enum eState
	{
		STATE_WAITING_FOR_CREDS,
		STATE_WAITING_FOR_GAMERS,
		STATE_POPULATING_GAMERS,
		STATE_POPULATING_GAMER_NAMES,
		STATE_GAMERS_POPULATED,
		STATE_POPULATING_STATS,
		STATE_STATS_POPULATED,
		STATE_SUCCESS = STATE_STATS_POPULATED,

		STATE_INVALID,

		START_ERRORS,
		ERROR_FAILED_POPULATING_GAMERS = START_ERRORS,
		ERROR_FAILED_POPULATING_STATS,
		ERROR_FAILED_INVALID_DATA,
		ERROR_NO_SOCIALCLUB,
		
#if RSG_ORBIS
		ERROR_NEEDS_GAME_UPDATE,
		ERROR_PROFILE_CHANGE,
		ERROR_PROFILE_SYSTEM_UPDATE,
		ERROR_REASON_AGE,
#endif
	};

	struct GamerData
	{
		private:
			atFixedString<RL_MAX_DISPLAY_NAME_BUF_SIZE> m_gamerName;
		public:
			bool m_isValid:1;
			bool m_isOnline:1;
			bool m_isInSameTitle:1;
			bool m_isInSameTitleOnline:1;
			bool m_isInSameSession:1;
			bool m_isHost:1;
			bool m_hasBeenInvited:1;
			bool m_isWaitingForInviteAck:1;
			bool m_isInTransition:1;
			bool m_hasFullyJoined:1;
			bool m_isDefaultOnline:1;
			bool m_isOnCall:1;
			bool m_isBusy:1;
			u32 m_nameHash;
			int m_teamId;
			u32 m_userIdHash;
			u32 m_inviteBlockedLabelHash;

			GamerData();

			void Reset()
			{
				m_isValid = false;
				m_isOnline = m_isDefaultOnline;
				m_isInSameTitle = false;
				m_isInSameTitleOnline = false;
				m_isInSameSession = false;
				m_isHost = false;
				m_hasBeenInvited = false;
				m_isBusy = false;
				m_isWaitingForInviteAck = false;
				m_isInTransition = false;
				m_hasFullyJoined = false;
				m_isOnCall = false;
				m_teamId = PLAYER_CARD_INVALID_TEAM_ID;

			}

			void SetName(const char* pName)
			{
				m_gamerName = pName;
				m_nameHash = atStringHash(pName);
			}

			const char* GetName() const
			{
				return m_gamerName.c_str();
			}
	};

	struct ValidityRules
	{
		bool m_hideLocalPlayer;
		bool m_hidePlayersInTutorial;

		ValidityRules() {Reset();}
		void Reset()
		{
			m_hideLocalPlayer = false;
			m_hidePlayersInTutorial = false;
		}
	};

	typedef rlProfileStatsFixedRecord< MAX_STATS > FixedRecord;

	BasePlayerCardDataManager();
	virtual ~BasePlayerCardDataManager();
	virtual void    Reset();
	virtual bool CanReset() const {return true;}

	static void ClearCachedStats();
	static void InitTunables();

	virtual void Init(const CPlayerCardXMLDataManager& rStatManager);
	virtual void Update();

	bool GetCachedGamerStats(const int localGamerIndex, const rlGamerHandle* gamers, unsigned numGamers, rlProfileStatsReadResults* results);

	void SetListRules(const ValidityRules& listRules) {m_listRules = listRules;}

	bool        WasInitialized() const {return m_state != STATE_INVALID;}
	bool               IsValid() const {return HasSucceeded() || HasBufferedReadData();}
	bool          HasSucceeded() const {return m_state == STATE_SUCCESS;}
	bool              HadError() const {return m_state >= START_ERRORS;}
	int 			  GetError() const { return m_state; }
	bool AreGamerTagsPopulated() const {return m_state > STATE_POPULATING_GAMERS && m_state < STATE_INVALID;}
	bool   IsStatsQueryPending() const {return m_statsQueryStatus.Pending();}

	virtual bool NeedsRefresh()	const { return m_bNeedsRefresh;}
	void FlagForRefresh()	{ m_bNeedsRefresh = true; }
	void ClearRefreshFlag()	{ m_bNeedsRefresh = false; }

	const rlProfileStatsValue* GetStat(const rlGamerHandle& rGamerHandle, int statIndex) const { return GetStat(GetGamerIndex(rGamerHandle), statIndex);}
	const rlProfileStatsValue* GetStat(int playerIndex, int statIndex) const;

	virtual bool ArePlayerStatsAccessible(const rlGamerHandle& gamerHandle) const;
	virtual bool Exists(const rlGamerHandle& handle) const;

//	int GetStatIndex(int statId) const { return m_pStatIds->Find(statId);}

	const char* GetName(int index) const {return GetReadDataBuffer().m_gamerDatas[index].GetName();}
	const char* GetName(const rlGamerHandle& rGamerHandle) const {int index = GetGamerIndex(rGamerHandle); return (0 <= index) ? GetName(index) : "(UNKNOWN PLAYER NAME)";}
#if !__NO_OUTPUT
	void PrintProfileStatsResults(const rlProfileStatsReadResults& results);
#endif

	const rlGamerHandle& GetGamerHandle(int index) const {return GetReadDataBuffer().m_gamerHandles[index];}
	const GamerData& GetGamerData(int index) const {return GetReadDataBuffer().m_gamerDatas[index];}
	GamerData& GetGamerData(int index) { return const_cast<GamerData&>(GetReadDataBuffer().m_gamerDatas[index]);}
	inline int GetGamerIndex(const rlGamerHandle& rGamerHandle) const {return GetReadDataBuffer().m_gamerHandles.Find(rGamerHandle);}

	int GetReadGamerCount() const {return GetReadDataBuffer().m_gamerHandles.size();} // The number in the current cached set of players (Read Buffer)
	int GetWriteGamerCount() { return GetWriteDataBuffer().m_gamerHandles.size();} // The number in the current cached set of players (Write Buffer)

	virtual int GetCurrPage() const {return 0;}
	virtual int GetQueuedPage() const {return 0;}
	virtual int GetNumPages() const;
	virtual int GetPageSize() const {return GetReadGamerCount();}
	virtual int GetTotalUncachedGamerCount() const {return GetReadGamerCount();} // includes the current cached page.
	virtual void LoadPage(int UNUSED_PARAM(page)) {}

	virtual void RefreshGamerData();
	void CommonRefreshGamerData(GamerData& rData, const rlGamerHandle& rGamerHandle);
	bool IsPlayerBusy(unsigned nStatus);

	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const = 0;
	const atArray<int>* GetStatIds(const CPlayerCardXMLDataManager& rStatManager) const;
	const rlProfileStatsValue* GetStat(const CPlayerCardXMLDataManager& rStatManager, const rlGamerHandle& gamerHandle, int statHash) const;

	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const = 0;

	virtual const atArray<rlGamerHandle>& GetAllGamerHandles() const {return GetReadDataBuffer().m_gamerHandles;}
	virtual bool IsPlayerListDirty();
	virtual bool CanDownloadLocalPlayer() const { return false; }

	bool HasPlayedMode(const rlGamerHandle& rGamerHandle) const;
	virtual bool GetIsPrimaryClan() const { return false; }
	bool HasBufferedReadData() const { return m_bAllowDoubleBuffering && GetReadDataBuffer().GetStatsResultsMaxRows() != 0; }
	virtual void RefreshStats() { this->OnGamerNamesPopulated(); }

protected:
	virtual bool IsValidPlayer(const CNetGamePlayer* pNetPlayer) const;

	virtual void OnGamerNamesPopulated();
	virtual void OnGamerHandlesPopulated();
	virtual void PopulateGamerHandles() = 0;
	virtual void FixupGamerHandles() {};

	virtual void InitUGCQueries() {}
	const GamerData* FindGamerDataByUserId (const char* userId) const;

	// Data Buffering -- For lists which don't allow double buffering, the read buffer is also the write buffer
	u8 GetReadDataBufferIndex()		const {	return m_readBufferIndex; }
	u8 GetWriteDataBufferIndex()	const { return m_bAllowDoubleBuffering ? 1 - GetReadDataBufferIndex() : GetReadDataBufferIndex(); }

	public:
	struct PlayerListData
	{
		atArray< rlGamerHandle >	m_gamerHandles;//Doesn't have players that haven't played the game in here.
		atArray< GamerData >		m_gamerDatas;
		rlProfileStatsReadResults	m_statsResults;
		atArray< bool >				m_hasRecords;
		enum eTypeOfProfileStats
		{
			PROFILE_STATS_SP_FRIENDS,
			PROFILE_STATS_MP_FRIENDS,
			PROFILE_STATS_SP_CREWS,
			PROFILE_STATS_MP_CREWS,
			PROFILE_STATS_MP_PLAYERS
		};

		PlayerListData() : m_pRequestedRecords(NULL) {}

		void CreateRequestedRecords(eTypeOfProfileStats typeOfProfileStats);
		void Reset();

		u32           GetGamerCount() const {return m_gamerHandles.GetCount();}
		bool   NeedsToUpdateRecords() const;

		void        ReceivedRecords(const rlGamerHandle& handle);
		void               AddGamer(const rlGamerHandle& handle, const char* pName);
		int                  Exists(const rlGamerHandle& handle, int& index) const;
		void            RemoveGamer(const rlGamerHandle& handle);
//		bool  PrepareRecordsForRead(atArray< rlGamerHandle >& gamerHandles, atArray< FixedRecord >& requestedRecords);
		bool	 HasReceivedRecords(const rlGamerHandle& handle) const;

		// When maxRows is 0, we'll use the size of the m_requestedRecords array. This is the default behavior.
		void InitializeStatsResults(const atArray<int>* pStats, unsigned maxRows=0);

		unsigned GetStatsResultsMaxRows() const { return m_statsResults.GetMaxRows(); }

		bool SetStatsResultsNumRows(const unsigned num) { return m_statsResults.SetNumRows(num); }
		unsigned GetStatsResultsNumRows() const { return m_statsResults.GetNumRows(); }

		bool SetStatsResultsGamerHandle(const unsigned row, const rlGamerHandle& gamerHandle) { return m_statsResults.SetGamerHandle(row, gamerHandle); }
		const rlGamerHandle* GetStatsResultsGamerHandle(unsigned row) const { return m_statsResults.GetGamerHandle(row); }

		const rlProfileStatsValue* GetStatValueFromStatsResults(int playerIndex, int statIndex, const char* pClassName) const;

		bool StatsResultsGetBufferResults(u8* inBuffer, const u32 inbufferSize, const u32 row, u32& numStatsReceived);

		void RequestedRecordsResizeGrow(s32 numberOfGamers);
		u32 GetRequestedRecordsMaxStats() const;
		u32 GetRequestedRecordsCount() const;
		const rlProfileStatsValue& GetStatValueFromRequestedRecord(unsigned gamerIndex, unsigned statIndex) const;

		bool ReadStatsByGamer(const rlGamerHandle* gamers, const unsigned numGamers, netStatus* status);

#if __ASSERT
		bool GetRequestedRecordsPointerHasBeenSet() const {  return (m_pRequestedRecords != NULL); }
#endif // __ASSERT

			static rlProfileStatsValue sm_InvalidStatValue;

			class CRequestedRecordsBase
			{
			public:
				virtual ~CRequestedRecordsBase() {}

				virtual int GetCount() const = 0;
				virtual int GetCapacity() const = 0;
				virtual void Clear() = 0;
				virtual unsigned GetSizeOfOneRecord() const = 0;
				virtual unsigned GetMaxStats() const = 0;
				virtual rlProfileStatsRecordBase* GetElements() = 0;
				virtual void AddOneRecord() = 0;
				virtual void DeleteRecord(int index) = 0;
				virtual void ResizeGrow(s32 numberOfGamers) = 0;
				virtual const rlProfileStatsValue& GetStatValue(unsigned gamerIndex, unsigned statIndex) const = 0;
			};

			template <unsigned MaxStatsInRecord, unsigned MaxRecords >
			class CRequestedRecords : public CRequestedRecordsBase
			{
			public:
				typedef rlProfileStatsFixedRecord< MaxStatsInRecord > FixedRecord;

				CRequestedRecords(atFixedArray< FixedRecord, MaxRecords >& recordsArray OUTPUT_ONLY(, const char* debugName))
					: m_requestedRecords(recordsArray)
					OUTPUT_ONLY(, m_debugName(debugName))
				{}
				virtual ~CRequestedRecords() {}

				virtual int GetCount() const { return m_requestedRecords.GetCount(); }
				virtual int GetCapacity() const { return MaxRecords; }
				virtual void Clear() { m_requestedRecords.clear(); }
				virtual unsigned GetSizeOfOneRecord() const { return static_cast<unsigned>(sizeof(FixedRecord)); }
				virtual unsigned GetMaxStats() const { return MaxStatsInRecord; }
				virtual rlProfileStatsRecordBase* GetElements() { return m_requestedRecords.GetElements(); }

				virtual void AddOneRecord()
				{
					if(!m_requestedRecords.IsFull())
					{
						FixedRecord records;
						m_requestedRecords.Push(records);
					}
				}

				virtual void DeleteRecord(int index)
				{
					m_requestedRecords.Delete(index);
				}

				virtual void ResizeGrow(s32 numberOfGamers)
				{
					if (playercardVerifyf(numberOfGamers <= MaxRecords, "CRequestedRecords_%s<%d,%d> - Cannot resize request records to %d, max is %d", m_debugName, MaxStatsInRecord, MaxRecords, numberOfGamers, MaxRecords))
					{
						m_requestedRecords.Resize(numberOfGamers);
					}
				}

				virtual const rlProfileStatsValue& GetStatValue(unsigned gamerIndex, unsigned statIndex) const
				{
					if (playercardVerifyf(gamerIndex >=0 && gamerIndex < MaxRecords && gamerIndex < m_requestedRecords.GetCount(), "CRequestedRecords_%s<%d,%d>::GetStatValue - invalid index %d", m_debugName, MaxStatsInRecord, MaxRecords, gamerIndex))
					{
						return m_requestedRecords[gamerIndex].GetValue(statIndex);
					}
					static rlProfileStatsValue s_emptyValue;
					return s_emptyValue;
				}

			private:
				atFixedArray< FixedRecord, MaxRecords >&    m_requestedRecords;
				OUTPUT_ONLY(const char* m_debugName);
			};

			typedef CRequestedRecords<MAX_PROFILE_STATS_FOR_SP_FRIENDS, MAX_FRIEND_STAT_QUERY> CRequestedRecordsSpFriends;
			typedef CRequestedRecords<MAX_PROFILE_STATS_FOR_MP_FRIENDS, MAX_FRIEND_STAT_QUERY> CRequestedRecordsMpFriends;
			typedef CRequestedRecords<MAX_PROFILE_STATS_FOR_SP_CREWS, MAX_CREW_MEMBERS_STAT_QUERY> CRequestedRecordsSpCrews;
			typedef CRequestedRecords<MAX_PROFILE_STATS_FOR_MP_CREWS, MAX_CREW_MEMBERS_STAT_QUERY> CRequestedRecordsMpCrews;
			typedef CRequestedRecords<MAX_PROFILE_STATS_FOR_MP_PLAYERS, MAX_FRIEND_STAT_QUERY> CRequestedRecordsMpPlayers;

			CRequestedRecordsBase    *m_pRequestedRecords;
	};

	protected:

	void CreateRequestedRecords(PlayerListData::eTypeOfProfileStats typeOfProfileStats);
//	void ReserveRequestedRecords(s32 arraySizeToReserve);

	      PlayerListData& GetWriteDataBuffer()       { return m_Data[GetWriteDataBufferIndex()]; }
	const PlayerListData& GetWriteDataBuffer() const { return m_Data[GetWriteDataBufferIndex()]; }
	const PlayerListData&  GetReadDataBuffer() const { return m_Data[GetReadDataBufferIndex()]; }

	void SwapListDataBuffer();
	void SetState(eState newState);

	virtual const char* ClassIdentifier() const {return "BasePlayerCardDataManager";};
	virtual bool ChecksCredentials() const {return true;}

	PlayerListData m_Data[2];	// Read / Write data buffers
	u8 m_readBufferIndex;		// The index in the array which to read the data -- when swapping buffers we just change this index
	bool m_bAllowDoubleBuffering;

	eState m_state;
	ValidityRules m_listRules;
#if RSG_DURANGO
	int m_gamerNamesReqID;
#endif

protected:
	CSystemTimer m_timeoutTimer;

	netStatus m_statsQueryStatus;
	bool m_usingCachedStats;
	bool m_cacheLoadComplete;

	const atArray<int>* m_pStats;

private: // members
	bool m_bNeedsRefresh;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CActivePlayerCardDataManager: public BasePlayerCardDataManager
{
public:
	CActivePlayerCardDataManager();

	virtual void RefreshGamerData();
	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;

	virtual const atArray<rlGamerHandle>& GetAllGamerHandles() const {return m_currentHandles;}
	virtual void Reset();

protected:
	virtual void OnGamerNamesPopulated();
	virtual void PopulateGamerHandles();

	virtual const char* ClassIdentifier() const {return "CActivePlayerCardDataManager";}

private:
	atArray<rlGamerHandle> m_currentHandles;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CFriendPlayerCardDataManager: public BasePlayerCardDataManager, CloudCacheListener
{
public:
	CFriendPlayerCardDataManager();

	virtual void Reset();
	virtual const atArray<rlGamerHandle>& GetAllGamerHandles() const {return m_allFriendHandles;}

	virtual void OnCacheFileLoaded(const CacheRequestID, bool) override;

	void RequestStatsQuery(int iStartIndex);
	void RefreshGamerStats()					{ PopulateGamerHandles(); }
#if __BANK
	static bool ms_bDebugHideAllFriends;
#endif

protected:
	virtual void PopulateGamerHandles();
	virtual void FixupGamerHandles();
	virtual void OnGamerNamesPopulated();

	virtual const char* ClassIdentifier() const {return "CFriendPlayerCardDataManager";}

private:
	int						m_iFriendIndex;
	int						m_iRequestedFriendIndex;
	atArray<rlGamerHandle>  m_allFriendHandles;
};

class CSPFriendPlayerCardDataManager: public CFriendPlayerCardDataManager
{
public:
	CSPFriendPlayerCardDataManager();

	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;

protected:
	virtual void InitUGCQueries() {}

	virtual const char* ClassIdentifier() const {return "CSPFriendPlayerCardDataManager";}

};

class CMPFriendPlayerCardDataManager: public CFriendPlayerCardDataManager
{
public:
	CMPFriendPlayerCardDataManager();

	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;

protected:
	virtual const char* ClassIdentifier() const {return "CMPFriendPlayerCardDataManager";}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CPartyPlayerCardDataManager: public BasePlayerCardDataManager
{
public:
	CPartyPlayerCardDataManager();

	virtual void RefreshGamerData();
	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const atArray<rlGamerHandle>& GetAllGamerHandles() const {return m_currentHandles;}

protected:
	virtual void PopulateGamerHandles();
	virtual const char* ClassIdentifier() const {return "CPartyPlayerCardDataManager";}

private:
	void BuildGamerHandles(atArray<rlGamerHandle>& rGamerHandles, bool buildGamerData);

	atArray<rlGamerHandle> m_currentHandles;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptedPlayerCardDataManager: public BasePlayerCardDataManager
{
public:
	CScriptedPlayerCardDataManager();

	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;

	void ClearGamerHandles() {m_currentHandles.clear(); m_currentNames.clear();}
	void AddGamerHandle(const rlGamerHandle& rGamerHandle) {m_currentHandles.PushAndGrow(rGamerHandle);}
	void AddGamerHandle(const rlGamerHandle& rGamerHandle, const char* pName) {m_currentHandles.PushAndGrow(rGamerHandle); m_currentNames.PushAndGrow(atString(pName));}

	virtual void ValidateNewGamerList();
	virtual const atArray<rlGamerHandle>& GetAllGamerHandles() const {return m_currentHandles;}

	virtual bool NeedsRefresh() const {return m_state == STATE_WAITING_FOR_GAMERS && m_hasValidatedList;}

protected:
	virtual void PopulateGamerHandles();
	virtual void OnGamerNamesPopulated();

	virtual const char* ClassIdentifier() const {return "CScriptedPlayerCardDataManager";}

	bool m_hasValidatedList;
	atArray<rlGamerHandle> m_currentHandles;
	atArray<atString> m_currentNames;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptedDirectorPlayerCardDataManager : public CScriptedPlayerCardDataManager
{
	static const u32 MAX_NUM_PLAYERS = 1;

public:
	CScriptedDirectorPlayerCardDataManager();

	virtual bool CanDownloadLocalPlayer() const { return true; }

	virtual void ValidateNewGamerList();

protected:
	virtual void PopulateGamerHandles();

	virtual bool IsValidPlayer(const CNetGamePlayer* pNetPlayer) const;

	virtual const char* ClassIdentifier() const {return "CScriptedDirectorPlayerCardDataManager";}

	virtual void OnGamerNamesPopulated();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptedCoronaPlayerCardDataManager: public BasePlayerCardDataManager
{
public:
	CScriptedCoronaPlayerCardDataManager();
	virtual ~CScriptedCoronaPlayerCardDataManager(){Reset();}

	virtual void   Init(const CPlayerCardXMLDataManager& rStatManager);
	virtual void Update();

	virtual const PlayerCardStatInfo*             GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;

	virtual void    Reset();
	virtual bool CanReset() const {return m_state >= STATE_INVALID;}

	void    SetGamerHandleName(const rlGamerHandle& rGamerHandle, const char* pName);
	void        AddGamerHandle(const rlGamerHandle& rGamerHandle, const char* pName);
	void     RemoveGamerHandle(const rlGamerHandle& rGamerHandle);

	virtual const atArray<rlGamerHandle>& GetAllGamerHandles() const {return GetWriteDataBuffer().m_gamerHandles;}

	virtual bool IsPlayerListDirty() {return false;}

	void ReceivePlayerCardData(const rlGamerInfo& gamerInfo, u8* data, const u32 sizeOfData);

	virtual bool Exists(const rlGamerHandle& handle) const;

	virtual void RefreshGamerData();
	bool ArePlayerStatsAccessible(const rlGamerHandle& gamerHandle) const;

protected:

	virtual void PopulateGamerHandles();
	virtual void OnGamerHandlesPopulated();

	#if RSG_DURANGO
	virtual void OnGamerNamesPopulated();
	#endif

	virtual const char* ClassIdentifier() const {return "[corona_sync] CScriptedCoronaPlayerCardDataManager";}
	bool ChecksCredentials() const {return false;}

	atArray<atString> m_currentNames;
	atArray< int > m_receivedRecords;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptedJoinedPlayerListDataManager: public CScriptedPlayerCardDataManager
{
public:
	CScriptedJoinedPlayerListDataManager():
		CScriptedPlayerCardDataManager()
		{
			m_bAllowDoubleBuffering = true;
		}

protected:
	virtual const char* ClassIdentifier() const {return "CScriptedJoinedPlayerListDataManager";}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptedCoronaPlayerListDataManager: public CScriptedPlayerCardDataManager
{
public:

	CScriptedCoronaPlayerListDataManager();

	void AddGamerHandle(const rlGamerHandle& rGamerHandle) {m_currentHandles.PushAndGrow(rGamerHandle); FlagForRefresh();}
	void AddGamerHandle(const rlGamerHandle& rGamerHandle, const char* pName) {m_currentHandles.PushAndGrow(rGamerHandle); m_currentNames.PushAndGrow(atString(pName)); FlagForRefresh();}

	void RemoveGamerHandle(const rlGamerHandle& rGamerHandle);

protected:

	#if RSG_DURANGO
	virtual void PopulateGamerHandles();
	#endif
	virtual void OnGamerHandlesPopulated();
	
	virtual const char* ClassIdentifier() const {return "CScriptedCoronaPlayerListDataManager";}

};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CClanPlayerCardDataManager: public BasePlayerCardDataManager
{
	enum
	{
		MAX_CLAN_SIZE = MAX_CREW_MEMBERS_STAT_QUERY // should be a multiple of 16, to match the number of players shown at a time.
	};

public:
	CClanPlayerCardDataManager();
	virtual ~CClanPlayerCardDataManager();
	virtual void Update();
	virtual void Reset();
	void SetShouldGetOfflineMembers(bool shouldGet) {m_bShouldGetOfflineMembers = shouldGet;}
	virtual void SetClan(rlClanId newClan, bool bIsLocalPlayersPrimary);
	rlClanId GetClan() const { return m_ClanId; }
	virtual bool GetIsPrimaryClan() const { return m_bIsLocalPlayersPrimaryClan; }

	virtual int GetCurrPage() const {return m_pageIndex;}
	virtual int GetQueuedPage() const {return m_queuedPageIndex;}
	virtual int GetPageSize() const {return MAX_CLAN_SIZE;}
	//virtual int GetTotalUncachedGamerCount() const {return (int)m_totalClanCount;} // includes the current cached page.
	virtual void LoadPage(int page);

protected:
	virtual void PopulateGamerHandles();
	virtual const PlayerCardStatInfo* GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const PlayerCardPackedStatInfo* GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const;
	virtual const char* ClassIdentifier() const {return "CClanPlayerCardDataManager";}

private:
	atArray<rlClanMember> m_clanMembers;
	atArray<rlGamerQueryData> m_onlineClanMembers;
	netStatus m_clanQueryStatus;
	netStatus m_onlineQueryStatus;
	rlClanId m_ClanId;
	int m_pageIndex;
	int m_queuedPageIndex;
	unsigned m_onlineMemberCount;
	unsigned m_returnedClanCount;
	unsigned m_totalClanCount;
	bool	m_bIsLocalPlayersPrimaryClan;
	bool	m_bShouldGetOfflineMembers;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//PURPOSE
//  Class responsible to manage corona player stats. Sends local player card stats to all corona players
class CPlayerCardCoronaManager
{
private:
	static const u32 MAX_NUM_STATS_IN_MESSAGE = 104;
	static const u32 MAX_NUM_CORONA_PLAYER    = 32;

	//PURPOSE
	//  Necessary info to manage each player we need to send our stats.
	struct sPlayerMsgInfo
	{
	public:
		sPlayerMsgInfo() 
			: m_currentStatIndex(0) 
		{;}

		sPlayerMsgInfo(const rlGamerHandle& handle) 
			: m_currentStatIndex(0) 
		{
			m_handle = handle;
		}

	public:
		rlGamerHandle  m_handle;
		int            m_currentStatIndex;
	};

public:
	CPlayerCardCoronaManager() : m_statIdsCount(0) {;}
	~CPlayerCardCoronaManager() {m_players.Reset();}

	void Shutdown(const unsigned shutdownMode);
	void             Reset( );

	void                Add(const rlGamerHandle& handle);
	void             Remove(const rlGamerHandle& handle);
	void             Update( );
	bool             Exists(const rlGamerHandle& handle) const;
	bool   RequestFromGamer(const rlGamerHandle& handle);

private:
	int      GetIndex(const rlGamerHandle& handle) const ;
	bool  SendMessage(const int index);

private:
	//Player array that we need to send the player stats.
	atFixedArray< sPlayerMsgInfo, MAX_NUM_CORONA_PLAYER > m_players;

	//Easy access to the max number of stat id's needed to send.
	int m_statIdsCount;
};

typedef atSingleton<CPlayerCardCoronaManager> SCPlayerCardCoronaManager;

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayerCardManager
{
public:

	enum CardTypes
	{
		CARDTYPE_INVALID = -1,
		CARDTYPE_ACTIVE_PLAYER,
		CARDTYPE_SP_FRIEND,
		CARDTYPE_MP_FRIEND,
		CARDTYPE_PARTY,
		CARDTYPE_CLAN,

		CARDTYPE_START_SCRIPTED_TYPES = 100, // Needs to match PLAYER_LIST_TYPE in script
		CARDTYPE_LAST_JOB = CARDTYPE_START_SCRIPTED_TYPES,

		//--------------------------------------------------------
		//Corona and joined players are the same fucking thing
		//--------------------------------------------------------
		CARDTYPE_JOINED,
		CARDTYPE_CORONA_PLAYERS,
		//--------------------------------------------------------
		//--------------------------------------------------------

		CARDTYPE_INVITABLE_SESSION_PLAYERS,
		CARDTYPE_MATCHED,
		CARDTYPE_CORONA_JOINED_PLAYLIST,
		CARDTYPE_DIRECTOR,
		CARDTYPE_END_SCRIPTED_TYPES = CARDTYPE_DIRECTOR
	};

	enum CrewStatus
	{
		CREWSTATUS_PENDING,
		CREWSTATUS_HASDATA,
		CREWSTATUS_NODATA,
	};

	CPlayerCardManager(): m_currentType(CARDTYPE_INVALID), m_currentGamerHandle(RL_INVALID_GAMER_HANDLE), m_crewStatus(CREWSTATUS_NODATA), m_currentClanId(RL_INVALID_CLAN_ID) {}

	void Init(CardTypes type, const BasePlayerCardDataManager::ValidityRules& rListRules);
	void Update();

	inline CardTypes GetCurrentType() const {return m_currentType;}
	inline void SetCurrentType(const CardTypes type) {m_currentType=type;}

	bool IsValid()									  { return m_currentType != CARDTYPE_INVALID; }
	inline BasePlayerCardDataManager* GetDataManager() {return GetDataManager(m_currentType);}
	inline const BasePlayerCardDataManager* GetDataManager() const {return GetDataManager(m_currentType);}
	BasePlayerCardDataManager* GetDataManager(CardTypes type);
	const BasePlayerCardDataManager* GetDataManager(CardTypes type) const;

	const CPlayerCardXMLDataManager& GetXMLDataManager() const {return SPlayerCardXMLDataManager::GetInstance();}

	const PlayerCardStatInfo* GetStatInfo() const;
	const atArray<int>* GetStatIds() const;
	const PlayerCardStatInfo::StatData* GetStatData(int statHash) const;
	const PlayerCardPackedStatInfo* GetPackedStatInfos() const;
	const PlayerCardPackedStatInfo::StatData* GetPackedStatInfo(u32 statHash) const;
	bool GetPackedStatValue(int playerIndex, u32 statHash, int& outVal) const;
	int ExtractPackedValue(u64 data, u64 shift, u64 numberOfBits) const;

	void SetCurrentGamer(rlGamerHandle gamerHandle) {m_currentGamerHandle = gamerHandle;}
	const rlGamerHandle& GetCurrentGamer() const {return m_currentGamerHandle;}
	int GetCurrentGamerIndex() const {return GetDataManager() ? GetDataManager()->GetGamerIndex(m_currentGamerHandle) : 0;}
	void ClearCurrentGamer() {m_currentGamerHandle = RL_INVALID_GAMER_HANDLE;}
	void SetCurrentCrewColor(const Color32& color) {m_crewColor = color;}
	const Color32& GetCurrentCrewColor() const {return m_crewColor;}
	void SetCrewStatus(CrewStatus crewStatus) {m_crewStatus = crewStatus;}
	CrewStatus GetCrewStatus() const {return m_crewStatus;}
	void SetClanId(const rlClanId& clanId) {m_currentClanId = clanId;}
	const rlClanId& GetClanId() const {return m_currentClanId;}

	const rlProfileStatsValue* GetStat(const rlGamerHandle& gamerHandle, int statHash) const;
	const rlProfileStatsValue* GetStatForCurrentGamer(int statHash) const {return GetStat(m_currentGamerHandle, statHash);}
	const rlProfileStatsValue* GetCorrectedMultiStatForCurrentGamer(int statHash, int characterSlot) const;

#if! __NO_OUTPUT
	const char* GetStatName(int statHash) const;
#endif //!__NO_OUTPUT

private:
	rlGamerHandle m_currentGamerHandle; // Stores the gamer handle that the menus are looking at, so script can get stats for it.
	Color32       m_crewColor;
	CrewStatus    m_crewStatus;
	rlClanId      m_currentClanId;

	CardTypes								m_currentType;
	CActivePlayerCardDataManager			m_activePlayers;
	CSPFriendPlayerCardDataManager			m_spFriendPlayers;
	CMPFriendPlayerCardDataManager			m_mpFriendPlayers;
	CPartyPlayerCardDataManager				m_partyPlayers;
	CClanPlayerCardDataManager				m_clanPlayers;
	CScriptedPlayerCardDataManager			m_lastJobPlayers;
	CScriptedCoronaPlayerListDataManager	m_invitableSessionPlayers;
	CScriptedPlayerCardDataManager			m_matchedPlayers;
	CScriptedPlayerCardDataManager			m_playlistJoinedPlayers;
	CScriptedDirectorPlayerCardDataManager	m_directorPlayers;
	//Players that joined the corona.. these both lists are the same!!! what the fuck!!!
	CScriptedCoronaPlayerCardDataManager  m_joinedPlayers;
};

typedef atSingleton<CPlayerCardManager> SPlayerCardManager;


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CPlayerCardManagerCurrentTypeHelper
{
public:
	CPlayerCardManagerCurrentTypeHelper(CPlayerCardManager::CardTypes cardTypeInit) 
		: m_cachedType(SPlayerCardManager::GetInstance().GetCurrentType())
	{
		BasePlayerCardDataManager::ValidityRules rules;
		SPlayerCardManager::GetInstance().Init(cardTypeInit, rules);
	}

	~CPlayerCardManagerCurrentTypeHelper()
	{
		SPlayerCardManager::GetInstance().SetCurrentType(m_cachedType);
	}

private:
	CPlayerCardManager::CardTypes m_cachedType;

};

#endif // PLAYER_CARD_DATA_MANAGER_H

// eof

