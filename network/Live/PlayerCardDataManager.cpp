//
// PlayerCardDataManager.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

//rage
#include "fwdecorator/decoratorExtension.h"
#include "mathext/linearalgebra.h"
#include "rline/clan/rlclan.h"
#include "net/nethardware.h"

//game
#include "Frontend/ui_channel.h"
#include "Network/Cloud/UserContentManager.h"
#include "Network/Network.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Network/Live/PlayerCardDataParserHelper.h"
#include "Network/Live/LiveManager.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkSessionUtils.h"
#include "Peds/PlayerInfo.h"
#include "Stats/StatsInterface.h"
#include "frontend/PauseMenuData.h"
#include "Script/script_channel.h"
#include "Stats/StatsTypes.h"

#if RSG_DURANGO
#define NOT_USING_DISPLAY_NAMES_ONLY(...)
#else
#define NOT_USING_DISPLAY_NAMES_ONLY(...)	__VA_ARGS__
#endif

#define NUM_SP_SLOTS MAX_NUM_SP_CHARS
#define NUM_MP_SLOTS MAX_NUM_MP_ACTIVE_CHARS
#define CREDS_TIMEOUT (2000)

const u32 SCRIPTED_CORONA_ABANDON_TIME = 60 * 1000;	// requests that go unqueried for this long will be canceled

FRONTEND_NETWORK_OPTIMISATIONS()

#define MAX_CLAN_MEMBER_SIZE MAX_CLAN_SIZE+1


rlProfileStatsValue BasePlayerCardDataManager::PlayerListData::sm_InvalidStatValue;

#if __BANK
bool CFriendPlayerCardDataManager::ms_bDebugHideAllFriends = false;
#endif

PARAM(spewplayercard, "If present then log player card stats output.");
PARAM(hideRockstarDevs, "If present then log player card stats output.");
PARAM(friendslistcaching, "Uses caching to populate friends list stats");
PARAM(friendslistcachingverbose, "Verbose logging for friends list caching");

struct CachedStats
{
	CachedStats()
		: m_numStats(0)
		, m_retrieved(false)
		, m_lastRetrievalAttempt(0)
	{
		sysMemSet(m_statIds, 0, sizeof(int) * BasePlayerCardDataManager::MAX_STATS);
	}
	//cache entry for players who were requested but not retrieved
	explicit CachedStats(const rlGamerHandle& gamerHandle)
		: m_numStats(0) // : CachedStats()
		, m_retrieved(false)
		, m_lastRetrievalAttempt(0)
	{
		sysMemSet(m_statIds, 0, sizeof(int) * BasePlayerCardDataManager::MAX_STATS);
		m_gamerHandle = gamerHandle;
		m_lastRetrievalAttempt = sysTimer::GetSystemMsTime();
	}

	explicit CachedStats(const rlGamerHandle& gamerHandle, const rlProfileStatsValue* stats, const int* statIds,
		const unsigned numStats)
		: m_gamerHandle(gamerHandle)
		, m_numStats(numStats)
		, m_retrieved(true)
		, m_lastRetrievalAttempt(0)
	{
		sysMemSet(m_statIds, 0, sizeof(int) * BasePlayerCardDataManager::MAX_STATS);
		SetStats(stats, statIds, numStats);
	}

	void SetStats(const rlProfileStatsValue* stats, const int* statIds, const unsigned numStats)
	{
		m_numStats = numStats;
		m_retrieved = true;
		sysMemCpy(m_statIds, statIds, numStats * sizeof(unsigned));
		sysMemCpy(m_stats, stats, numStats * sizeof(rlProfileStatsValue));
	}

	bool HasStats() const
	{
		return m_retrieved;
	}

	const rlProfileStatsValue* GetStatById(const int statId)
	{
		auto it = std::find(std::begin(m_statIds), std::end(m_statIds), statId);
		return it != std::end(m_statIds) ? &m_stats[it - std::begin(m_statIds)] : nullptr;
	}

	rlGamerHandle m_gamerHandle;
	rlProfileStatsValue m_stats[BasePlayerCardDataManager::MAX_STATS];
	int m_statIds[BasePlayerCardDataManager::MAX_STATS];
	unsigned m_numStats;
	bool m_retrieved;
	u32 m_lastRetrievalAttempt;
};

struct CachedStatsRequest
{
	CachedStatsRequest()
	{ Clear(); }

	CachedStatsRequest(const rlGamerHandle& gamerHandle, const unsigned hash, const bool hasStats)
		: m_gamerHandle(gamerHandle)
		, m_hash(hash)
		, m_hasStats(hasStats)
	{ ClearRequest(); }

	void Clear()
	{
		m_gamerHandle.Clear();
		m_hash = 0;
		m_hasStats = false;
		ClearRequest();
	}

	//keeps the gamer handle active
	void ClearRequest()
	{
		m_requestId = INVALID_CACHE_REQUEST_ID;
		m_results = nullptr;
		m_manager = nullptr;
	}

	rlGamerHandle m_gamerHandle;
	unsigned m_hash;
	CacheRequestID m_requestId;
	bool m_hasStats;

	rlProfileStatsReadResults* m_results; //results to write into
	BasePlayerCardDataManager* m_manager; //manager that requested
	
};

static unsigned s_lastCacheTime = 0;
static unsigned STATS_REFRESH_INTERVAL = 3600000;
static bool		s_enableFriendsListStatCaching = false;
static atArray<CachedStatsRequest> s_cachedStatsRequests;

void ClearCachedStatsRequests()
{
	for (CachedStatsRequest& req : s_cachedStatsRequests)
	{
		req.ClearRequest();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerCardXMLDataManager::Init()
{
	Reset();
	LoadDataXMLFile();
}

void CPlayerCardXMLDataManager::Reset()
{
	m_spFriends.Reset();
	m_mpFriends.Reset();
	m_spCrew.Reset();
	m_mpCrew.Reset();
	m_mpPlayers.Reset();
	m_spPackedStats.Reset();
	m_mpPackedStats.Reset();
	m_mpPackedPlayersStats.Reset();
	m_titles.Reset();
}

void CPlayerCardXMLDataManager::LoadDataXMLFile()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PLAYER_CARD_SETUP);
	if (DATAFILEMGR.IsValid(pData))
	{
		if (playercardVerify(pData->m_filename))
		{
			parTree* pTree = PARSER.LoadTree(pData->m_filename, "xml");
			if(pTree)
			{
				parTreeNode* pRootNode = pTree->GetRoot();
				if(pRootNode)
				{
					parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
					for(; it != pRootNode->EndChildren(); ++it)
					{
						unsigned hash = atStringHash((*it)->GetElement().GetName());

						if(hash == ATSTRINGHASH("MiscData",0xa0c11b98))
						{
							LoadDataXMLFile((*it), m_miscData);
						}
						else if(hash == ATSTRINGHASH("SP_Friend_Stats",0x58a5352b))
						{
							LoadDataXMLFile((*it), m_spFriends, true);
							playercardAssertf(m_spFriends.GetStatHashes().GetCount() <= MAX_PROFILE_STATS_FOR_SP_FRIENDS, "CPlayerCardXMLDataManager::LoadDataXMLFile - MAX_PROFILE_STATS_FOR_SP_FRIENDS needs to be increased. Its current value is %u, but the number of m_spFriends stats read from %s.xml is %d", 
								MAX_PROFILE_STATS_FOR_SP_FRIENDS, pData->m_filename, m_spFriends.GetStatHashes().GetCount());
						}
						else if(hash == ATSTRINGHASH("SP_Packed_Stats",0xfe7476f4))
						{
							LoadDataXMLFile((*it), m_spPackedStats, true);
						}
						else if(hash == ATSTRINGHASH("SP_Card",0xeb4963b5))
						{
							LoadDataXMLFile((*it), PCTYPE_SP);
						}
						else if(hash == ATSTRINGHASH("MP_Friend_Stats",0xc42bb7ad))
						{
							LoadDataXMLFile((*it), m_mpFriends, false);
							playercardAssertf(m_mpFriends.GetStatHashes().GetCount() <= MAX_PROFILE_STATS_FOR_MP_FRIENDS, "CPlayerCardXMLDataManager::LoadDataXMLFile - MAX_PROFILE_STATS_FOR_MP_FRIENDS needs to be increased. Its current value is %u, but the number of m_mpFriends stats read from %s.xml is %d", 
								MAX_PROFILE_STATS_FOR_MP_FRIENDS, pData->m_filename, m_mpFriends.GetStatHashes().GetCount());
						}
						else if(hash == ATSTRINGHASH("Mission_Creator",0x51ffa032))
						{
							LoadDataXMLFile((*it), m_missionCreator);
						}
						else if(hash == ATSTRINGHASH("MP_Packed_Stats",0x5bca32d2))
						{
							LoadDataXMLFile((*it), m_mpPackedStats, false);
						}
						else if(hash == ATSTRINGHASH("Freemode_Card",0x5a6a9b54))
						{
							LoadDataXMLFile((*it), PCTYPE_FREEMODE);
						}
						else if(hash == ATSTRINGHASH("SP_Crew_Stats",0x580b7714))
						{
							LoadDataXMLFile((*it), m_spCrew, true);
							playercardAssertf(m_spCrew.GetStatHashes().GetCount() <= MAX_PROFILE_STATS_FOR_SP_CREWS, "CPlayerCardXMLDataManager::LoadDataXMLFile - MAX_PROFILE_STATS_FOR_SP_CREWS needs to be increased. Its current value is %u, but the number of m_spCrew stats read from %s.xml is %d", 
								MAX_PROFILE_STATS_FOR_SP_CREWS, pData->m_filename, m_spCrew.GetStatHashes().GetCount());
						}
						else if(hash == ATSTRINGHASH("MP_Crew_Stats",0x312a493c))
						{
							LoadDataXMLFile((*it), m_mpCrew, false);
							playercardAssertf(m_mpCrew.GetStatHashes().GetCount() <= MAX_PROFILE_STATS_FOR_MP_CREWS, "CPlayerCardXMLDataManager::LoadDataXMLFile - MAX_PROFILE_STATS_FOR_MP_CREWS needs to be increased. Its current value is %u, but the number of m_mpCrew stats read from %s.xml is %d", 
								MAX_PROFILE_STATS_FOR_MP_CREWS, pData->m_filename, m_mpCrew.GetStatHashes().GetCount());
						}
						else if(hash == ATSTRINGHASH("TitleLists",0x61a52f8b))
						{
							m_titles.LoadDataXMLFile((*it));
						}
					}
				}

				delete pTree;
			}
		}
	}
}

void  CPlayerCardXMLDataManager::LoadDataXMLFile(parTreeNode* pNode, PlayerCardStatInfo& out_info, const bool cIsSP)
{
	out_info.Init(cIsSP);

	parTreeNode::ChildNodeIterator statIt = pNode->BeginChildren();
	for(; statIt != pNode->EndChildren(); ++statIt)
	{
		bool isStat = stricmp((*statIt)->GetElement().GetName(), "stat") == 0;
		if(isStat || (stricmp((*statIt)->GetElement().GetName(), "multiStat") == 0))
		{
			char buffer[50];
			const char* pStatName = (*statIt)->GetElement().FindAttributeStringValue("name", "", buffer, sizeof(buffer), false, true);

			if((*statIt)->GetElement().FindAttributeBoolValue("isDebugOnly", false, false))
			{
#if __FINAL
				continue;
#endif
			}

			bool isSP = (*statIt)->GetElement().FindAttributeBoolValue("isSP", false);
			bool isMP = (*statIt)->GetElement().FindAttributeBoolValue("isMP", false);

			playercardAssertf(!(isSP && isMP), "CPlayerCardXMLDataManager::LoadDataXMLFile - Stat can't be both a MP and a SP stat.");

			if(!isSP)
			{
				isSP = isMP ? !isMP : cIsSP;
			}

			if(isStat)
			{
				out_info.AddStat(pStatName, isSP);
			}
			else
			{
				out_info.AddMultiStat(pStatName, isSP);
			}
		}
	}
}

void CPlayerCardXMLDataManager::LoadDataXMLFile(parTreeNode* pNode, UGCRanks& out_info)
{
	out_info.m_icon = pNode->GetElement().FindAttributeIntValue("icon", 0, true);

	const int numChildren = pNode->FindNumChildren();
	parTreeNode* pChild = NULL;

	for(int i=0; i<numChildren; ++i)
	{
		pChild = pNode->FindChildWithName("rank", pChild);
		if(pChild)
		{
			UGCRanks::Rank rank;
			char buffer[50];
			rank.m_value = pChild->GetElement().FindAttributeFloatValue("value", 0, true);
			rank.m_color = CHudColour::GetColourFromKey(pChild->GetElement().FindAttributeStringValue("color", "HUD_COLOUR_WHITE", buffer, NELEM(buffer), false, true));

			out_info.AddRank(rank);
		}
		else
		{
			break;
		}
	}
}

void CPlayerCardXMLDataManager::LoadDataXMLFile(parTreeNode* pNode, PlayerCardPackedStatInfo& out_info, const bool cIsSP)
{
	out_info.Init(cIsSP);

	parTreeNode::ChildNodeIterator statIt = pNode->BeginChildren();
	for(; statIt != pNode->EndChildren(); ++statIt)
	{
		bool isStat = stricmp((*statIt)->GetElement().GetName(), "stat") == 0;
		if(isStat || (stricmp((*statIt)->GetElement().GetName(), "multiStat") == 0))
		{
			char aliasBuffer[50];
			char realNameBuffer[50];
			char typeBuffer[10];
			const char* pAlias = (*statIt)->GetElement().FindAttributeStringValue("alias", "", aliasBuffer, sizeof(aliasBuffer), false, true);
			const char* pRealName = (*statIt)->GetElement().FindAttributeStringValue("realName", "", realNameBuffer, sizeof(realNameBuffer), false, true);
			const char* pType = (*statIt)->GetElement().FindAttributeStringValue("type", "int", typeBuffer, sizeof(typeBuffer), false, true);
			int index = (*statIt)->GetElement().FindAttributeIntValue("index", 0, true);
			bool isInt = pType ? (stricmp(pType, "int") == 0): false;

			bool isSP = (*statIt)->GetElement().FindAttributeBoolValue("isSP", false);
			bool isMP = (*statIt)->GetElement().FindAttributeBoolValue("isMP", false);

			playercardAssertf(!(isSP && isMP), "CPlayerCardXMLDataManager::LoadDataXMLFile - Stat can't be both a MP and a SP stat.");

			if(!isSP)
			{
				isSP = isMP ? !isMP : cIsSP;
			}

			if(isStat)
			{
				out_info.AddStat(pAlias, pRealName, isInt, index, isSP);
			}
			else
			{
				out_info.AddMultiStat(pAlias, pRealName, isInt, index, isSP);
			}
		}
	}
}

void CPlayerCardXMLDataManager::LoadDataXMLFile(parTreeNode* pNode, eCardTypes cardType)
{
	const int bufferSize = 50;
	char buffer[bufferSize];

	PlayerCard& rPlayerCard = m_playerCards[cardType];
	rPlayerCard.m_title = atStringHash(pNode->GetElement().FindAttributeStringValue("title", "", buffer, bufferSize, false, true));
	rPlayerCard.m_isSP = pNode->GetElement().FindAttributeBoolValue("isSP", 0, false);

	// These 3 stats are MP only, and rankStat is used in SP while the other 2 aren't
	#if RSG_PC
	rPlayerCard.m_usingKeyboard.SetStat(pNode->GetElement().FindAttributeStringValue("usingKeyboard", "", buffer, bufferSize, false, false), true, false);
	#endif
	rPlayerCard.m_rankStat.SetStat(pNode->GetElement().FindAttributeStringValue("rankStat", "", buffer, bufferSize, false, true), true, false);
	rPlayerCard.m_carStat.SetStat(pNode->GetElement().FindAttributeStringValue("carStat", "", buffer, bufferSize, false, false), true, false);
	rPlayerCard.m_boatStat.SetStat(pNode->GetElement().FindAttributeStringValue("boatStat", "", buffer, bufferSize, false, false), true, false);
	rPlayerCard.m_heliStat.SetStat(pNode->GetElement().FindAttributeStringValue("heliStat", "", buffer, bufferSize, false, false), true, false);
	rPlayerCard.m_planeStat.SetStat(pNode->GetElement().FindAttributeStringValue("planeStat", "", buffer, bufferSize, false, false), true, false);
	rPlayerCard.m_timestamp.SetStat(pNode->GetElement().FindAttributeStringValue("timestamp", "", buffer, bufferSize, false, true), false, rPlayerCard.m_isSP);

	rPlayerCard.m_teamId = pNode->GetElement().FindAttributeIntValue("teamId", PlayerCard::DEFAULT_TEAMID, true);
	rPlayerCard.m_rankIcon = pNode->GetElement().FindAttributeIntValue("rankIcon", PlayerCard::DEFAULT_RANK_ICON, !rPlayerCard.m_isSP);

	rPlayerCard.m_color = CHudColour::GetColourFromKey(pNode->GetElement().FindAttributeStringValue("color", PLAYER_CARD_DEFAULT_COLOR_STR, buffer, bufferSize, false, !rPlayerCard.m_isSP));

	parTreeNode::ChildNodeIterator it = pNode->BeginChildren();
	for(; it != pNode->EndChildren(); ++it)
	{
		bool isSlot = stricmp((*it)->GetElement().GetName(), "slot") == 0;

		if(isSlot || (stricmp((*it)->GetElement().GetName(), "titleSlot") == 0))
		{
			if((*it)->GetElement().FindAttributeBoolValue("isDebugOnly", false, false))
			{
#if __FINAL
				continue;
#endif
			}

			CardSlot slot;
			slot.m_textHash = atStringHash((*it)->GetElement().FindAttributeStringValue("text", "", buffer, bufferSize, false, true));
			slot.m_compareTextHash = atStringHash((*it)->GetElement().FindAttributeStringValue("compareText", "", buffer, bufferSize, false));
			slot.m_titleListHash = atStringHash((*it)->GetElement().FindAttributeStringValue("titleList", "", buffer, bufferSize, false, !rPlayerCard.m_isSP));
			slot.m_isCash = (*it)->GetElement().FindAttributeBoolValue("isCash", false);
			slot.m_isPercent = (*it)->GetElement().FindAttributeBoolValue("isPercent", false);
			slot.m_showPercentBar = (*it)->GetElement().FindAttributeBoolValue("showPercentBar", false);
			slot.m_isMissionName = (*it)->GetElement().FindAttributeBoolValue("isMissionName", false);
			slot.m_isCrew = (*it)->GetElement().FindAttributeBoolValue("isCrew", false);
			slot.m_isRatio = (*it)->GetElement().FindAttributeBoolValue("isRatio", false);
			slot.m_isRatioStr = (*it)->GetElement().FindAttributeBoolValue("isRatioStr", false);
			slot.m_isSumPercent = (*it)->GetElement().FindAttributeBoolValue("isSumPercent", false);
			slot.m_color = CHudColour::GetColourFromKey((*it)->GetElement().FindAttributeStringValue("color", PLAYER_CARD_DEFAULT_COLOR_STR, buffer, bufferSize, false, rPlayerCard.m_isSP));


			if((*it)->GetElement().FindAttribute("stat"))
			{
				slot.m_stat.SetStat((*it)->GetElement().FindAttributeStringValue("stat", "", buffer, bufferSize, false, true), false, rPlayerCard.m_isSP);
			}
			else if((*it)->GetElement().FindAttribute("multiStat"))
			{
				slot.m_stat.SetStat((*it)->GetElement().FindAttributeStringValue("multiStat", "", buffer, bufferSize, false, true), true, rPlayerCard.m_isSP);
			}

			if((*it)->GetElement().FindAttribute("extraStat"))
			{
				slot.m_extraStat.SetStat((*it)->GetElement().FindAttributeStringValue("extraStat", "", buffer, bufferSize, false, true), false, rPlayerCard.m_isSP);
			}
			else if((*it)->GetElement().FindAttribute("extraMultiStat"))
			{
				slot.m_extraStat.SetStat((*it)->GetElement().FindAttributeStringValue("extraMultiStat", "", buffer, bufferSize, false, true), true, rPlayerCard.m_isSP);
			}

			if(isSlot)
			{
				rPlayerCard.m_slots.PushAndGrow(slot);
			}
			else
			{
				rPlayerCard.m_titleSlots.PushAndGrow(slot);
			}
		}
	}
}

void CPlayerCardXMLDataManager::LoadDataXMLFile(parTreeNode* pNode, MiscData& out_miscData)
{
	const int bufferSize = 50;
	char buffer[bufferSize];

	out_miscData.m_jipUnlockStat.SetStat(pNode->GetElement().FindAttributeStringValue("jipUnlocked", "", buffer, bufferSize, false, true), true, false);
	out_miscData.m_characterSlotStat = pNode->GetElement().FindAttributeStringValue("charSlotStat", "", buffer, bufferSize, false, true);
	out_miscData.m_gameCompletionStat = pNode->GetElement().FindAttributeStringValue("gameCompletion", "", buffer, bufferSize, false, true);
	
	#if RSG_PC
	out_miscData.m_usingKeyboard = pNode->GetElement().FindAttributeStringValue("usingKeyboard", "", buffer, bufferSize, false, true);
	#endif
	out_miscData.m_allowsSpectatingStat = pNode->GetElement().FindAttributeStringValue("allowsSpectatingStat", "", buffer, bufferSize, false, true);
	out_miscData.m_isSpectatingStat = pNode->GetElement().FindAttributeStringValue("isSpectatingStat", "", buffer, bufferSize, false, true);
	out_miscData.m_crewRankStat = pNode->GetElement().FindAttributeStringValue("crewRankStat", "", buffer, bufferSize, false, true);
	out_miscData.m_badSportStat = pNode->GetElement().FindAttributeStringValue("badSportStat", "", buffer, bufferSize, false, true);
	out_miscData.m_badSportTunable = pNode->GetElement().FindAttributeStringValue("badSportTunable", "", buffer, bufferSize, false, true);
	out_miscData.m_dirtyPlayerTunable = pNode->GetElement().FindAttributeStringValue("dirtyPlayerTunable", "", buffer, bufferSize, false, true);
	out_miscData.m_bitTag = pNode->GetElement().FindAttributeStringValue("bitTag", "", buffer, bufferSize, false, true);
	out_miscData.m_teamIdTag = pNode->GetElement().FindAttributeStringValue("teamIdTag", "", buffer, bufferSize, false, true);

	out_miscData.m_defaultDirtyPlayerLimit = pNode->GetElement().FindAttributeFloatValue("defaultDirtyPlayerLimit", 0.0f, true);
	out_miscData.m_defaultBadSportLimit = pNode->GetElement().FindAttributeFloatValue("defaultBadSportLimit", 0.0f, true);

	out_miscData.m_maxMenuSlots = pNode->GetElement().FindAttributeIntValue("maxPlayers", 0, true);
	out_miscData.m_playingGTASPIcon = pNode->GetElement().FindAttributeIntValue("playingGTASPIcon", 0, true);
	out_miscData.m_playingGTAMPIcon = pNode->GetElement().FindAttributeIntValue("playingGTAMPIcon", 0, true);
	out_miscData.m_playingGTAMPWithRankIcon = pNode->GetElement().FindAttributeIntValue("playingGTAMPWithRankIcon", 0, true);
	out_miscData.m_kickIcon = pNode->GetElement().FindAttributeIntValue("kickIcon", 0, true);
	out_miscData.m_bountyIcon = pNode->GetElement().FindAttributeIntValue("bountyIcon", 0, true);
	out_miscData.m_spectatingIcon = pNode->GetElement().FindAttributeIntValue("spectatingIcon", 0, true);
	out_miscData.m_inviteIcon = pNode->GetElement().FindAttributeIntValue("inviteIcon", 0, true);
	out_miscData.m_inviteAcceptedIcon = pNode->GetElement().FindAttributeIntValue("inviteAcceptedIcon", 0, true);
	out_miscData.m_activeHeadsetIcon = pNode->GetElement().FindAttributeIntValue("activeHeadsetIcon", 0, true);
	out_miscData.m_inactiveHeadsetIcon = pNode->GetElement().FindAttributeIntValue("inactiveHeadsetIcon", 0, true);
	out_miscData.m_mutedHeadsetIcon = pNode->GetElement().FindAttributeIntValue("mutedHeadsetIcon", 0, true);
	out_miscData.m_emptySlotColor = pNode->GetElement().FindAttributeIntValue("emptySlotColor", 0, true);
	out_miscData.m_offlineSlotColor = pNode->GetElement().FindAttributeIntValue("offlineSlotColor", 0, true);
	out_miscData.m_spectTeamId = pNode->GetElement().FindAttributeIntValue("spectTeam", 0, true);
	out_miscData.m_bountyBit = BIT(pNode->GetElement().FindAttributeIntValue("bountyBit", 0, true));
	out_miscData.m_spectatingBit = BIT(pNode->GetElement().FindAttributeIntValue("spectatingBit", 0, true));

	out_miscData.m_localPlayerSlotColor = CHudColour::GetColourFromKey(pNode->GetElement().FindAttributeStringValue("localPlayerSlotColor", PLAYER_CARD_DEFAULT_COLOR_STR, buffer, bufferSize, false, true));
	out_miscData.m_freemodeSlotColor = CHudColour::GetColourFromKey(pNode->GetElement().FindAttributeStringValue("freemodeSlotColor", PLAYER_CARD_DEFAULT_COLOR_STR, buffer, bufferSize, false, true));
	out_miscData.m_inMatchSlotColor = CHudColour::GetColourFromKey(pNode->GetElement().FindAttributeStringValue("inMatchSlotColor", PLAYER_CARD_DEFAULT_COLOR_STR, buffer, bufferSize, false, true));
	out_miscData.m_spSlotColor = CHudColour::GetColourFromKey(pNode->GetElement().FindAttributeStringValue("spSlotColor", PLAYER_CARD_DEFAULT_COLOR_STR, buffer, bufferSize, false, true));
}

void PlayerCardStatInfo::Init(bool isSP)
{
	Reset();

	m_isSP = isSP;

	//	m_multiStatHashes.ResizeGrow(isSP ? NUM_SP_SLOTS : NUM_MP_SLOTS);
}

void PlayerCardStatInfo::Reset()
{
	m_isSP = true;

	m_statDatas.Reset();
	m_statHashes.Reset();
}

void PlayerCardStatInfo::AddStat(const char* pStatName, bool isSP, bool isMulti /*=false*/)
{
	if(!playercardVerify(pStatName))
	{
		return;
	}

	int hash = atStringHash(pStatName);

	const sStatDescription* pDesc = StatsInterface::GetStatDesc(hash);
	if(playercardVerifyf(pDesc, "PlayerCardStatInfo::AddStat - Couldn't find stat %s(0x%08x)", pStatName, hash))
	{
		if(playercardVerifyf(pDesc->GetIsProfileStat(), "PlayerCardStatInfo::AddStat - %s(0x%08x) isn't a profile stat, we need to make it one for the player card to work.", pStatName, hash))
		{
			m_statHashes.PushAndGrow(hash);

			StatData statData;
#if !__NO_OUTPUT
			statData.m_name = atString(pStatName);
#endif	
			statData.m_isSP = isSP;
			statData.m_isMulti = isMulti;

			m_statDatas.PushAndGrow(statData);
		}
	}
}

void PlayerCardStatInfo::AddMultiStat(const char* pStatName, bool isSP)
{
	if(!playercardVerify(pStatName))
	{
		return;
	}

	const int numSlots = isSP ? NUM_SP_SLOTS : NUM_MP_SLOTS;
	for(int i=0; i<numSlots; ++i)
	{
		char buffer[50];
		formatf(buffer, "%s%d_%s", isSP ? "SP" : "MP", i, pStatName);
		AddStat(buffer, isSP, true);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
int UGCRanks::CompareFunc(const Rank* candidateA, const Rank* candidateB)
{
	return ((candidateA->m_value < candidateB->m_value) ? -1 : 1);
}

void UGCRanks::AddRank(const UGCRanks::Rank& rRank)
{
	m_ranks.PushAndGrow(rRank);
	m_ranks.QSort(0, -1, CompareFunc);
}

eHUD_COLOURS UGCRanks::GetRankColor(float currValue) const
{
	for(int i=m_ranks.size()-1; 0<=i; --i)
	{
		if(m_ranks[i].m_value <= currValue)
		{
			return m_ranks[i].m_color;
		}
	}

	return HUD_COLOUR_WHITE;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
int PCardTitleListInfo::TitleList::CompareFunc(const Title* candidateA, const Title* candidateB)
{
	return ((candidateA->m_value < candidateB->m_value) ? -1 : 1);
}

void PCardTitleListInfo::TitleList::AddTitle(const TitleList::Title& rTitle)
{
	m_titles.PushAndGrow(rTitle);
	m_titles.QSort(0, -1, CompareFunc);
}

u32 PCardTitleListInfo::TitleList::GetTitle(float currValue) const
{
	for(int i=m_titles.size()-1; 0<=i; --i)
	{
		if(m_titles[i].m_value <= currValue)
		{
			return m_titles[i].m_titleHash;
		}
	}

	return 0;
}

void PCardTitleListInfo::TitleList::LoadDataXMLFile(parTreeNode* pNode)
{
	parTreeNode::ChildNodeIterator it = pNode->BeginChildren();
	for(; it != pNode->EndChildren(); ++it)
	{
		unsigned hash = atStringHash((*it)->GetElement().GetName());
		if(hash == ATSTRINGHASH("title",0x72aad81d))
		{
			char buffer[30] = {0};
			Title& rTitle = m_titles.Grow();

			rTitle.m_titleHash = atStringHash((*it)->GetElement().FindAttributeStringValue("name", "", buffer, NELEM(buffer), false, true));
			rTitle.m_value = (*it)->GetElement().FindAttributeFloatValue("value", 0.0f, true);
		}
	}
}

u32 PCardTitleListInfo::GetTitle(u32 listName, float currValue) const
{
	for(int i=0; i<m_titleLists.size(); ++i)
	{
		if(m_titleLists[i].m_listName == listName)
		{
			return m_titleLists[i].GetTitle(currValue);
		}
	}

	return 0;
}

void PCardTitleListInfo::LoadDataXMLFile(parTreeNode* pNode)
{
	parTreeNode::ChildNodeIterator it = pNode->BeginChildren();
	for(; it != pNode->EndChildren(); ++it)
	{
		unsigned hash = atStringHash((*it)->GetElement().GetName());
		if(hash == ATSTRINGHASH("TitleList",0xc255e2d3))
		{
			char buffer[30] = {0};
			TitleList& rTitleList = m_titleLists.Grow();

			rTitleList.m_listName = atStringHash((*it)->GetElement().FindAttributeStringValue("name", "", buffer, NELEM(buffer), false, true));
			rTitleList.LoadDataXMLFile((*it));
		}
	}
}

void PCardTitleListInfo::Reset()
{
	m_titleLists.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
void PlayerCardPackedStatInfo::Init(bool isSP)
{
	Reset();
	m_isSP = isSP;
}

void PlayerCardPackedStatInfo::Reset()
{
	m_isSP = true;
	m_statDatas.Reset();
}

void PlayerCardPackedStatInfo::AddStat(const char* pStatAlias, const char* pStatRealName, bool isInt, u32 index, bool isSP, bool isMulti /*=false*/)
{
	if(!playercardVerify(pStatAlias) || !playercardVerify(pStatRealName))
	{
		return;
	}

	int nameHash = atStringHash(pStatRealName);

	const sStatDescription* pDesc = StatsInterface::GetStatDesc(nameHash);
	if(playercardVerifyf(pDesc, "PlayerCardPackedStatInfo::AddStat - Couldn't find stat %s(0x%08x), alias=%s(0x%08x)", pStatRealName, nameHash, pStatAlias, atStringHash(pStatAlias)))
	{
		if(playercardVerifyf(pDesc->GetIsProfileStat(), "PlayerCardPackedStatInfo::AddStat - %s(0x%08x) isn't a profile stat, we need to make it one for the player card to work. alias=%s(0x%08x)", pStatRealName, nameHash, pStatAlias, atStringHash(pStatAlias)))
		{
#if __ASSERT
			if(isInt)
			{
				playercardAssertf(index < 8, "PlayerCardPackedStatInfo::AddStat - There can be only up to 8 int stats stored in a pack stat.  alias=%s(0x%08x), stat=%s(0x%08x), index=%d", pStatAlias, atStringHash(pStatAlias), pStatRealName, nameHash, index);
			}
			else
			{
				playercardAssertf(index < 64, "PlayerCardPackedStatInfo::AddStat - There can be only up to 64 bool stats stored in a pack stat.  alias=%s(0x%08x), stat=%s(0x%08x), index=%d", pStatAlias, atStringHash(pStatAlias), pStatRealName, nameHash, index);
			}
#endif

			StatData statData;

			statData.m_alias = pStatAlias;
			statData.m_realName = pStatRealName;
			statData.m_isInt = isInt;
			statData.m_index = index;
			statData.m_isSP = isSP;
			statData.m_isMulti = isMulti;

			m_statDatas.PushAndGrow(statData);
		}
	}
}

void PlayerCardPackedStatInfo::AddMultiStat(const char* pStatAlias, const char* pStatRealName, bool isInt, u32 index, bool isSP)
{
	if(!playercardVerify(pStatAlias) || !playercardVerify(pStatRealName))
	{
		return;
	}

	const int numSlots = isSP ? NUM_SP_SLOTS : NUM_MP_SLOTS;
	for(int i=0; i<numSlots; ++i)
	{
		char aliasBuffer[50];
		char nameBuffer[50];
		formatf(aliasBuffer, "%s%d_%s", isSP ? "SP" : "MP", i, pStatAlias);
		formatf(nameBuffer, "%s%d_%s", isSP ? "SP" : "MP", i, pStatRealName);
		AddStat(aliasBuffer, nameBuffer, isInt, index, isSP, true);
	}
}

const PlayerCardPackedStatInfo::StatData* PlayerCardPackedStatInfo::GetStatData(u32 statHash) const
{
	for(int i=0; i<m_statDatas.size(); ++i)
	{
		if(m_statDatas[i].m_alias.GetHash() == statHash)
		{
			return &m_statDatas[i];
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
u32 CPlayerCardXMLDataManager::CardStat::GetStat(int characterSlot) const
{
	if(playercardVerifyf(0 <= characterSlot && characterSlot < MAX_CHARACTER_SLOTS, "CPlayerCardXMLDataManager::CardStat::GetStat - characterSlot=%d, MAX_CHARACTER_SLOTS=%d", characterSlot, MAX_CHARACTER_SLOTS))
	{
		return m_stats[characterSlot];
	}
	else
	{
		return m_stats[0];
	}
}

void CPlayerCardXMLDataManager::CardStat::SetStat(const char* pStat, bool isMultistat, bool isSP)
{
	if(isMultistat)
	{
		const int bufferSize = 50;
		char buffer[bufferSize] = {0};

		const char* pMode = isSP ? "SP" : "MP";

		for(int i=0; i<MAX_CHARACTER_SLOTS; ++i)
		{
			formatf(buffer, "%s%d_%s", pMode, i, pStat);
			m_stats[i] = atStringHash(buffer);
		}
	}
	else
	{
		u32 hash = atStringHash(pStat);
		for(int i=0; i<MAX_CHARACTER_SLOTS; ++i)
		{
			m_stats[i] = hash;
		}
	}
}

bool CPlayerCardXMLDataManager::CardStat::IsStat(u32 statHash) const
{
	for(int i=0; i<MAX_CHARACTER_SLOTS; ++i)
	{
		if(m_stats[i] == statHash)
		{
			return true;
		}
	}

	return false;
}

CPlayerCardXMLDataManager::PlayerCard::eNetworkedStatIds CPlayerCardXMLDataManager::PlayerCard::GetNetIdByStatHash(u32 statHash) const
{
	if(m_rankStat.IsStat(statHash))
	{
		return NETSTAT_RANK;
	}
	else if(m_carStat.IsStat(statHash))
	{
		return NETSTAT_CAR_ACCESS;
	}
	else if(m_planeStat.IsStat(statHash))
	{
		return NETSTAT_PLANE_ACCESS;
	}
	else if(m_boatStat.IsStat(statHash))
	{
		return NETSTAT_BOAT_ACCESS;
	}
	else if(m_heliStat.IsStat(statHash))
	{
		return NETSTAT_HELI_ACCESS;
	}
#if RSG_PC
	else if(SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_usingKeyboard == statHash)
	{
		return NETSTAT_USING_KEYBOARD;
	}
#endif
	else if(SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_jipUnlockStat.IsStat(statHash))
	{
		return NETSTAT_TUT_COMPLETE;
	}
	else if (SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_allowsSpectatingStat == statHash)
	{
		return NETSTAT_ALLOWS_SPECTATING;
	}
	else if (SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_isSpectatingStat == statHash)
	{
		return NETSTAT_IS_SPECTATING;
	}
	else if (SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_crewRankStat == statHash)
	{
		return NETSTAT_CREWRANK;
	}
	else if (SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_badSportStat == statHash)
	{
		return NETSTAT_BAD_SPORT;
	}

	for(int i=0; i<m_slots.size() && i<=NETSTAT_END_MISC-NETSTAT_START_MISC; ++i)
	{
		if(m_slots[i].m_stat.IsStat(statHash))
		{
			return static_cast<eNetworkedStatIds>(NETSTAT_START_MISC + i);
		}
	}

	return NETSTAT_INVALID;
}

u32 CPlayerCardXMLDataManager::PlayerCard::GetStatHashByNetId(eNetworkedStatIds statId, int characterSlot, bool extraStat /*= false*/) const
{
	if(statId == NETSTAT_RANK)
	{
		return m_rankStat.GetStat(characterSlot);
	}
	else if(statId == NETSTAT_CAR_ACCESS)
	{
		return m_carStat.GetStat(characterSlot);
	}
	else if(statId == NETSTAT_PLANE_ACCESS)
	{
		return m_planeStat.GetStat(characterSlot);
	}
	else if(statId == NETSTAT_BOAT_ACCESS)
	{
		return m_boatStat.GetStat(characterSlot);
	}
	else if(statId == NETSTAT_HELI_ACCESS)
	{
		return m_heliStat.GetStat(characterSlot);
	}
#if RSG_PC
	else if(statId == NETSTAT_USING_KEYBOARD)
	{
		return SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_usingKeyboard;
	}
#endif
	else if(statId == NETSTAT_TUT_COMPLETE)
	{
		return SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_jipUnlockStat.GetStat(characterSlot);
	}
	else if (statId == NETSTAT_ALLOWS_SPECTATING)
	{
		return SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_allowsSpectatingStat;
	}
	else if (statId == NETSTAT_IS_SPECTATING)
	{
		return SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_isSpectatingStat;
	}
	else if(statId == NETSTAT_CREWRANK)
	{
		return SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_crewRankStat;
	}
	else if(statId == NETSTAT_BAD_SPORT)
	{
		return SPlayerCardXMLDataManager::GetInstance().GetMiscData().m_badSportStat;
	}

	if(NETSTAT_START_MISC <= statId && statId < NETSTAT_SYNCED_STAT_COUNT)
	{
		int index = statId-NETSTAT_START_MISC;
		if(playercardVerifyf(0 <= index && index < m_slots.size(), "CPlayerCardXMLDataManager::PlayerCard::GetStatHashByNetId - index=%d, m_slots.size()=%d", index, m_slots.size()))
		{
			if(extraStat)
			{
				return m_slots[index].m_extraStat.GetStat(characterSlot);
			}
			else
			{
				return m_slots[index].m_stat.GetStat(characterSlot);
			}
		}
	}

	if(NETSTAT_START_TITLE_RATIO_STATS <= statId && statId <= NETSTAT_END_TITLE_RATIO_STATS)
	{
		int index = statId-NETSTAT_START_TITLE_RATIO_STATS;
		if(playercardVerifyf(0 <= index, "CPlayerCardXMLDataManager::PlayerCard::GetStatHashByNetId - index=%d", index) &&
			index < m_titleSlots.size())
		{
			if(extraStat)
			{
				return m_titleSlots[index].m_extraStat.GetStat(characterSlot);
			}
			else
			{
				return m_titleSlots[index].m_stat.GetStat(characterSlot);
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

// We declare those fixed arrays here so we dont have to dynamically allocate them when opening the pause menu.
static atFixedArray< BasePlayerCardDataManager::PlayerListData::CRequestedRecordsSpFriends::FixedRecord, MAX_FRIEND_STAT_QUERY > s_requestedRecordsFriendsSP;
static atFixedArray< BasePlayerCardDataManager::PlayerListData::CRequestedRecordsMpFriends::FixedRecord, MAX_FRIEND_STAT_QUERY > s_requestedRecordsFriendsMP;
static atFixedArray< BasePlayerCardDataManager::PlayerListData::CRequestedRecordsSpCrews::FixedRecord, MAX_CREW_MEMBERS_STAT_QUERY > s_requestedRecordsCrewsSP;
static atFixedArray< BasePlayerCardDataManager::PlayerListData::CRequestedRecordsMpCrews::FixedRecord, MAX_CREW_MEMBERS_STAT_QUERY > s_requestedRecordsCrewsMP;
static atFixedArray< BasePlayerCardDataManager::PlayerListData::CRequestedRecordsMpPlayers::FixedRecord, MAX_FRIEND_STAT_QUERY > s_requestedRecordsPlayersMP;

BasePlayerCardDataManager::GamerData::GamerData()
{
    m_gamerName = "";
    m_nameHash = 0;
    m_userIdHash = 0;
    m_isDefaultOnline = false;
    m_inviteBlockedLabelHash = 0;
    scriptDebugf3("m_inviteBlockedLabelHash cleared.");

    Reset();
}

void BasePlayerCardDataManager::PlayerListData::CreateRequestedRecords(eTypeOfProfileStats typeOfProfileStats)
{
	if (playercardVerifyf(!m_pRequestedRecords, "PlayerListData::CreateRequestedRecords - expected m_pRequestedRecords to be NULL before calling this"))
	{
		switch (typeOfProfileStats)
		{
		case PROFILE_STATS_SP_FRIENDS :
			m_pRequestedRecords = rage_new CRequestedRecordsSpFriends(s_requestedRecordsFriendsSP OUTPUT_ONLY(, "CRequestedRecordsSpFriends"));
			break;

		case PROFILE_STATS_MP_FRIENDS :
			m_pRequestedRecords = rage_new CRequestedRecordsMpFriends(s_requestedRecordsFriendsMP OUTPUT_ONLY(, "CRequestedRecordsSpFriends"));
			break;

		case PROFILE_STATS_SP_CREWS :
			m_pRequestedRecords = rage_new CRequestedRecordsSpCrews(s_requestedRecordsCrewsSP OUTPUT_ONLY(, "CRequestedRecordsSpCrews"));
			break;

		case PROFILE_STATS_MP_CREWS :
			m_pRequestedRecords = rage_new CRequestedRecordsMpCrews(s_requestedRecordsCrewsMP OUTPUT_ONLY(, "CRequestedRecordsMpCrews"));
			break;

		case PROFILE_STATS_MP_PLAYERS :
			m_pRequestedRecords = rage_new CRequestedRecordsMpPlayers(s_requestedRecordsPlayersMP OUTPUT_ONLY(, "CRequestedRecordsMpPlayers"));
			break;
		}
	}

	if (playercardVerifyf(m_pRequestedRecords, "PlayerListData::CreateRequestedRecords - failed to create m_pRequestedRecords for typeOfProfileStats=%d", (s32)typeOfProfileStats))
	{
		playercardDebugf1("PlayerListData::CreateRequestedRecords - MaxStatsInRecord=%u SizeOfOneRecord=%u", 
			m_pRequestedRecords->GetMaxStats(), m_pRequestedRecords->GetSizeOfOneRecord());
	}
}

void BasePlayerCardDataManager::PlayerListData::Reset()
{
	playercardDebugf1("BasePlayerCardDataManager :: PlayerListData :: Reset");

	m_hasRecords.Reset();
	m_gamerHandles.Reset();
	m_gamerDatas.Reset();

	m_statsResults.Clear();

	if (m_pRequestedRecords)
	{
		m_pRequestedRecords->Clear();
	}
}

void  BasePlayerCardDataManager::PlayerListData::ReceivedRecords(const rlGamerHandle& handle)
{
	playercardDebugf1("BasePlayerCardDataManager :: PlayerListData :: ReceivedRecords for %s", handle.ToString());

	int index = 0;
	if (AssertVerify(Exists(handle, index)))
	{
		m_hasRecords[index] = true;
	}
}

void BasePlayerCardDataManager::PlayerListData::AddGamer(const rlGamerHandle& handle, const char* pName)
{
	playercardDebugf1("BasePlayerCardDataManager :: PlayerListData :: AddGamer %s - %s", handle.ToString(), pName);
	int index = 0;
	if (!Exists(handle, index))
	{
		if (handle.IsLocal())
		{
			m_hasRecords.PushAndGrow(true);
		}
		else
		{
			m_hasRecords.PushAndGrow(false);
		}

		m_gamerHandles.PushAndGrow(handle);

		GamerData data;
		data.SetName(pName);

		m_gamerDatas.PushAndGrow(data);
		m_gamerDatas.Top().m_isDefaultOnline = true;
		m_gamerDatas.Top().m_isOnline = true;

		if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::AddGamer - m_pRequestedRecords is NULL"))
		{
			m_pRequestedRecords->AddOneRecord();
		}
	}
}

int BasePlayerCardDataManager::PlayerListData::Exists(const rlGamerHandle& handle, int& index) const
{
	index = -1;

	for (int i=0; i<m_gamerHandles.GetCount(); i++)
	{
		if (m_gamerHandles[i] == handle)
		{
			index = i;
			return true;
		}
	}

	return false;
}

void BasePlayerCardDataManager::PlayerListData::RemoveGamer(const rlGamerHandle& handle)
{
	playercardDebugf1("BasePlayerCardDataManager :: PlayerListData :: RemoveGamer %s", handle.ToString());
	int index = 0;
	if (Exists(handle, index))
	{
		m_gamerHandles.Delete(index);
		m_gamerDatas.Delete(index);

		if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::RemoveGamer - m_pRequestedRecords is NULL")
			&& playercardVerifyf(index < m_pRequestedRecords->GetCount(), "BasePlayerCardDataManager::PlayerListData::RemoveGamer - index %d out of bounds %d", index, m_pRequestedRecords->GetCount()))
		{
			m_pRequestedRecords->DeleteRecord(index);
		}

		m_hasRecords.Delete(index);
	}
}

bool  BasePlayerCardDataManager::PlayerListData::NeedsToUpdateRecords() const
{
	for (int i=0; i<m_hasRecords.GetCount(); i++)
	{
		if (!m_hasRecords[i])
		{
			return true;
		}
	}

	return false;
}

/*
bool  BasePlayerCardDataManager::PlayerListData::PrepareRecordsForRead(atArray< rlGamerHandle >& gamerHandles
																		,atArray< FixedRecord >& requestedRecords)
{
	gamerHandles.clear();
	requestedRecords.clear();

	if (NeedsToUpdateRecords())
	{
		for (int i=0; i<m_hasRecords.GetCount(); i++)
		{
			if (!m_hasRecords[i] && !m_gamerHandles[i].IsLocal())
			{
				gamerHandles.PushAndGrow(m_gamerHandles[i]);
				requestedRecords.PushAndGrow(m_requestedRecords[i]);
			}
		}
	}

	return (gamerHandles.GetCount() > 0);
}
*/

bool BasePlayerCardDataManager::PlayerListData::HasReceivedRecords(const rlGamerHandle& handle) const
{
	int index = 0;

	if (playercardVerify(Exists(handle, index)) && playercardVerify(m_hasRecords.GetCount() == m_gamerHandles.GetCount()))
	{
		return m_hasRecords[index];
	}

	return false;
}

void BasePlayerCardDataManager::PlayerListData::InitializeStatsResults(const atArray<int>* pStats, unsigned maxRows)
{
	playercardDebugf1("BasePlayerCardDataManager :: PlayerListData :: InitializeStatsResults - maxRows: %u", maxRows);
	if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::InitializeStatsResults - m_pRequestedRecords is NULL"))
	{
		if (maxRows == 0)
		{
			maxRows = m_pRequestedRecords->GetCount();
		}
		else
		{
			maxRows = rage::Min(maxRows, (unsigned)m_pRequestedRecords->GetCapacity());
		}

		m_statsResults.Reset(pStats->GetElements(), 
			MIN(pStats->size(), GetRequestedRecordsMaxStats()), 
			maxRows, 
			m_pRequestedRecords->GetElements(),
			m_pRequestedRecords->GetSizeOfOneRecord() );
	}
}

const rlProfileStatsValue* BasePlayerCardDataManager::PlayerListData::GetStatValueFromStatsResults(int playerIndex, int statIndex, const char* ASSERT_ONLY(pClassName)) const
{
	if(playercardVerifyf(0 <= playerIndex && playerIndex < m_gamerHandles.size(), "%s - GetStatValueFromStatsResults - playerIndex(%d) is out of range. Max value is %d", pClassName, playerIndex, m_gamerHandles.size()) &&
		playercardVerifyf(0 <= statIndex && statIndex < GetRequestedRecordsMaxStats(), "%s - GetStatValueFromStatsResults - statIndex(%d) is out of range. Max value is %u", pClassName, statIndex, GetRequestedRecordsMaxStats()))
	{
		const rlGamerHandle& rGamerHandle = m_gamerHandles[playerIndex];
		for(int i=0; i< m_statsResults.GetNumRows(); ++i)
		{
			const rlGamerHandle* pGamerHandle = m_statsResults.GetGamerHandle(i);
			if(pGamerHandle && *pGamerHandle == rGamerHandle)
			{
				if (uiVerify(statIndex < (int)m_statsResults.GetNumStatIds()))
				{
					return m_statsResults.GetValue(i, statIndex);
				}
			}
		}
	}

	return NULL;
}

bool BasePlayerCardDataManager::PlayerListData::StatsResultsGetBufferResults(u8* inBuffer, const u32 inbufferSize, const u32 row, u32& numStatsReceived)
{
	const bool lz4Compression = (RSG_DURANGO || RSG_ORBIS || RSG_PC);
	PlayerCardDataParserHelper parserhelper(lz4Compression);

	return parserhelper.GetBufferResults(inBuffer, inbufferSize, &m_statsResults, row, numStatsReceived);
}

void BasePlayerCardDataManager::PlayerListData::RequestedRecordsResizeGrow(s32 numberOfGamers)
{
	if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::RequestedRecordsResizeGrow - m_pRequestedRecords is NULL"))
	{
		m_pRequestedRecords->ResizeGrow(numberOfGamers);
	}
}

u32 BasePlayerCardDataManager::PlayerListData::GetRequestedRecordsMaxStats() const
{
	if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::GetRequestedRecordsMaxStats - m_pRequestedRecords is NULL"))
	{
		return m_pRequestedRecords->GetMaxStats();
	}

	return 0;
}

u32 BasePlayerCardDataManager::PlayerListData::GetRequestedRecordsCount() const
{
	if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::GetRequestedRecordsCount - m_pRequestedRecords is NULL"))
	{
		return m_pRequestedRecords->GetCount();
	}

	return 0;
}

const rlProfileStatsValue& BasePlayerCardDataManager::PlayerListData::GetStatValueFromRequestedRecord(unsigned gamerIndex, unsigned statIndex) const
{
	if (playercardVerifyf(m_pRequestedRecords, "BasePlayerCardDataManager::PlayerListData::GetStatValueFromRequestedRecord - m_pRequestedRecords is NULL"))
	{
		return m_pRequestedRecords->GetStatValue(gamerIndex, statIndex);
	}

	return sm_InvalidStatValue;
}

bool BasePlayerCardDataManager::PlayerListData::ReadStatsByGamer(const rlGamerHandle* gamers, const unsigned numGamers, netStatus* status)
{
	return rlProfileStats::ReadStatsByGamer(NetworkInterface::GetLocalGamerIndex(), gamers, numGamers, &m_statsResults, status);
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

void ClearStats()
{
	s_lastCacheTime = 0;
	s_cachedStatsRequests.clear();
}

void BasePlayerCardDataManager::ClearCachedStats()
{
	playercardDebugf1("BasePlayerCardDataManager :: ClearCachedStats");
	ClearStats();
}

void BasePlayerCardDataManager::InitTunables()
{
	STATS_REFRESH_INTERVAL = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLAYERCARD_DATA_MANAGER_STATS_REFRESH_INTERVAL", 0xe1bff549), STATS_REFRESH_INTERVAL);
	s_enableFriendsListStatCaching = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLAYERCARD_DATA_MANAGER_ENABLE_STATS_CACHING", 0xcfb07c85), s_enableFriendsListStatCaching);
}

BasePlayerCardDataManager::BasePlayerCardDataManager()
	: m_readBufferIndex(0)
	, m_bAllowDoubleBuffering(false)
	, m_bNeedsRefresh(false)
#if RSG_DURANGO
	, m_gamerNamesReqID(CDisplayNamesFromHandles::INVALID_REQUEST_ID)
#endif
	, m_usingCachedStats(false)
{
	Reset();
}

BasePlayerCardDataManager::~BasePlayerCardDataManager()
{
	Reset();

	for (CachedStatsRequest& lookup : s_cachedStatsRequests)
	{
		lookup.m_results = nullptr;
		lookup.m_manager = nullptr;
	}
}

void BasePlayerCardDataManager::Init(const CPlayerCardXMLDataManager& rStatManager)
{
	Reset();
	m_pStats = GetStatIds(rStatManager);
	
	
	#if RSG_ORBIS
	if (!g_rlNp.GetNpAuth().IsNpAvailable(NetworkInterface::GetLocalGamerIndex()))
	{
		//rlAssertf(false, "GetNpUnavailabilityReason called when Np is available");
	
		int iReason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason(NetworkInterface::GetLocalGamerIndex());
		if ( iReason == rlNpAuth::NP_REASON_GAME_UPDATE )
			SetState(ERROR_NEEDS_GAME_UPDATE);
		else if ( iReason == rlNpAuth::NP_REASON_SYSTEM_UPDATE)
			SetState(ERROR_PROFILE_SYSTEM_UPDATE);
		else if ( iReason == rlNpAuth::NP_REASON_SIGNED_OUT )
			SetState(ERROR_PROFILE_CHANGE);
		else if ( iReason == rlNpAuth::NP_REASON_AGE)
			SetState(ERROR_REASON_AGE);
	} 
	else
	#endif

	if(netHardware::IsLinkConnected() && CLiveManager::IsOnline() && !CLiveManager::CheckIsAgeRestricted() && !rlFriendsManager::ActivationFailed())
	{
		if(playercardVerifyf(m_pStats, "%s::Init - Need to have valid stats to get the player card.", ClassIdentifier()))
		{
			if(!ChecksCredentials() || NetworkInterface::HasValidRosCredentials())
			{
				PopulateGamerHandles();
			}
			else
			{
				m_timeoutTimer.Start();
				SetState(STATE_WAITING_FOR_CREDS);
			}
		}
		else
		{
			SetState(ERROR_FAILED_POPULATING_GAMERS);
		}
	}
	else
	{
		SetState(ERROR_FAILED_POPULATING_GAMERS);
	}
}

void BasePlayerCardDataManager::Update()
{
#if __ASSERT
	playercardAssertf(GetReadDataBuffer().GetRequestedRecordsPointerHasBeenSet(), "%s has not initialised its m_pRequestedRecords (read buffer)", ClassIdentifier());
	if (m_bAllowDoubleBuffering)
	{
		playercardAssertf(GetWriteDataBuffer().GetRequestedRecordsPointerHasBeenSet(), "%s has not initialised its m_pRequestedRecords (write buffer)", ClassIdentifier());
	}
#endif // __ASSERT

	if(m_state == STATE_WAITING_FOR_CREDS)
	{
		if(NetworkInterface::HasValidRosCredentials() && !CLiveManager::IsReadingFriends())
		{
			PopulateGamerHandles();
		}
		else if(m_timeoutTimer.IsComplete(CREDS_TIMEOUT))
		{
			SetState(ERROR_FAILED_POPULATING_GAMERS);
		}
	}
#if RSG_DURANGO
	else if(m_state == STATE_POPULATING_GAMER_NAMES)
	{
		int gamerCount = GetWriteDataBuffer().m_gamerHandles.GetCount();

		rlDisplayName displayNames[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST];
		int nameRetrievalStatus = CLiveManager::GetFindDisplayName().GetDisplayNames(m_gamerNamesReqID, displayNames, gamerCount);
		if(nameRetrievalStatus == CDisplayNamesFromHandles::DISPLAY_NAMES_SUCCEEDED)
		{
			// clear request id
			m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

			atArray<GamerData>& gamerDatas = GetWriteDataBuffer().m_gamerDatas;
			for(int i = 0; i < gamerDatas.GetCount() && i < CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST; ++i)
			{
				gamerDatas[i].SetName(displayNames[i]);
			}
			OnGamerNamesPopulated();
		}
		else if (nameRetrievalStatus == CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED)
		{
			// clear request id
			m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

			// fall back to gamer tags instead of denying friends list functionality
			OnGamerNamesPopulated();
		}
	}
#endif
	else if(m_state == STATE_POPULATING_STATS)
	{
		if (m_usingCachedStats)
		{
			if (m_cacheLoadComplete)
			{
				m_statsQueryStatus.Reset();
				RefreshGamerData();
				FixupGamerHandles();
				SetState(STATE_STATS_POPULATED);

				ClearCachedStatsRequests();

				PlayerListData& readData = const_cast<PlayerListData&>(GetReadDataBuffer());
				for (int i = 0; i < readData.m_hasRecords.size(); ++i)
				{
					readData.m_hasRecords[i] = true;
				}

#if !__NO_OUTPUT
				PrintProfileStatsResults(readData.m_statsResults);
#endif

				playercardNetDebugf1("%s::Update :: Cache load complete", ClassIdentifier());
			}
			return;
		}

		if(!m_statsQueryStatus.Pending())
		{
			if(m_statsQueryStatus.Succeeded())
			{
				m_statsQueryStatus.Reset();

				RefreshGamerData();
				FixupGamerHandles();
				SetState(STATE_STATS_POPULATED);

				PlayerListData& readData = const_cast<PlayerListData&>(GetReadDataBuffer());

				if (rlProfileStats::MAX_NUM_GAMERS_FOR_STATS_READ != -1)
				{
					playercardNetDebugf1("%s::Update :: Stats results are being capped by tunable MAX_NUM_GAMERS_FOR_STATS_READ. Max results: %u", ClassIdentifier(), rlProfileStats::MAX_NUM_GAMERS_FOR_STATS_READ);
				}

				// cache the stats
				ClearCachedStats();
				const unsigned numStats = readData.m_statsResults.GetNumStatIds();
				OUTPUT_ONLY(const unsigned numGamers = readData.m_statsResults.GetNumRows());
				OUTPUT_ONLY(const unsigned numRequestedGamers = readData.GetRequestedRecordsCount());
				const unsigned numGamerHandles = readData.m_gamerHandles.GetCount();

				for (unsigned i = 0; i < numGamerHandles; ++i)
				{
					const rlGamerHandle& requestedGamerHandle = readData.m_gamerHandles[i];
					playercardNetAssertf(requestedGamerHandle.IsValid(), "%s::Update :: Requested gamer handle should be valid", ClassIdentifier());

					const int statsResultsGamerIndex = readData.m_statsResults.FindGamerHandle(requestedGamerHandle);
					const int requestedGamerIndex = GetGamerIndex(requestedGamerHandle);
					const bool foundGamerHandle = statsResultsGamerIndex != -1;
					playercardNetDebugf1("[%u] statsQueryStatus for %s, index: %d", i, requestedGamerHandle.ToString(), requestedGamerIndex);

					CachedStats cachedStats(requestedGamerHandle);
					char cacheBlockName[256];
					formatf(cacheBlockName, "%s_stats", requestedGamerHandle.ToString());
					unsigned cacheBlockHash = atDataHash(cacheBlockName, sizeof(cacheBlockName));
					OUTPUT_ONLY(const char* gamerName = GetName(requestedGamerHandle));

					if (foundGamerHandle && requestedGamerHandle.IsValid())
					{
						readData.m_hasRecords[requestedGamerIndex] = true;

						rlProfileStatsValue stats[MAX_STATS];
						int statIds[MAX_STATS];

						for (unsigned j = 0; j < numStats; ++j)
						{
							const rlProfileStatsValue& value = readData.GetStatValueFromRequestedRecord(statsResultsGamerIndex, j);
							stats[j] = value;
							statIds[j] = readData.m_statsResults.GetStatId(j);
#if !__NO_OUTPUT
							const PlayerCardStatInfo* pStatInfo = SPlayerCardManager::GetInstance().GetStatInfo();
							const PlayerCardStatInfo::StatData& rStatData = pStatInfo->GetStatDatas()[j];
							const char* pStatName = rStatData.m_name.c_str();
							u32 statHash = atStringHash(pStatName);

							if (PARAM_friendslistcachingverbose.Get())
							{
								switch (value.GetType())
								{
								case RL_PROFILESTATS_TYPE_INT32:
								{
									playercardNetDebugf1("%s::Update :: Caching stat for gamer. Gamer name: %s, Stats index: %u, Requested gamer index: %u, Stat Id: %u, Stat Name: %s, Hash %u, Type: int32, Value: %d",
										ClassIdentifier(), gamerName, statsResultsGamerIndex, requestedGamerIndex, statIds[j], pStatName, statHash, value.GetInt32());
									break;
								}
								case RL_PROFILESTATS_TYPE_INT64:
								{
									playercardNetDebugf1("%s::Update :: Caching stat for gamer. Gamer name: %s, Requested gamer index: %u, Stat Id: %u, Stat Id: %u, Stat Name: %s, Hash %u, Type: int64 Value: %" I64FMT "d",
										ClassIdentifier(), gamerName, statsResultsGamerIndex, requestedGamerIndex, statIds[j], pStatName, statHash, value.GetInt64());
									break;
								}
								case RL_PROFILESTATS_TYPE_FLOAT:
								{
									playercardNetDebugf1("%s::Update :: Caching stat for gamer. Gamer name: %s, Requested gamer index: %u, Stat Id: %u, Stat Id: %u, Stat Name: %s, Hash %u, Type: float, Value: %f",
										ClassIdentifier(), gamerName, statsResultsGamerIndex, requestedGamerIndex, statIds[j], pStatName, statHash, value.GetFloat());
									break;
								}

								case RL_PROFILESTATS_TYPE_INVALID:
								{
									playercardNetDebugf1("%s::Update :: Caching stat for gamer. Gamer name: %s, Requested gamer index: %u, Stat Id: %u, Stat Id: %u, Stat Name: %s, Hash %u, Type: INVALID",
										ClassIdentifier(), gamerName, statsResultsGamerIndex, requestedGamerIndex, statIds[j], pStatName, statHash);
									break;
								}
								}

							}
#endif
						}

						cachedStats.SetStats(&stats[0], statIds, numStats);
					}

					if(CloudCache::AddCacheFile(cacheBlockHash, 0, false, CachePartition::Default, &cachedStats, sizeof(CachedStats)) != INVALID_CACHE_REQUEST_ID)
					{
						playercardNetDebugf1("%s::Update :: Adding cached stats block for gamer: %s. Gamer name: %s, Hash: %u, Stats results index: %u, Requested gamers index: %u, Has stats: %s",
							ClassIdentifier(), requestedGamerHandle.ToString(), gamerName, cacheBlockHash, statsResultsGamerIndex, requestedGamerIndex, BOOL_TO_STRING(cachedStats.HasStats()));
						s_cachedStatsRequests.PushAndGrow(CachedStatsRequest(requestedGamerHandle, cacheBlockHash, cachedStats.HasStats()));
					}
				}

#if !__NO_OUTPUT
				PrintProfileStatsResults(readData.m_statsResults);
#endif

				playercardNetDebugf1("%s::Update :: Stats query finished. Num stats: %u, Num gamers: %u, HasRecords size: %u, Requested records: %u, Stats max rows: %u",
					ClassIdentifier(), numStats, numGamers, readData.m_hasRecords.GetCount(), numRequestedGamers, readData.m_statsResults.GetMaxRows());

				s_lastCacheTime = sysTimer::GetSystemMsTime();

#if !__NO_OUTPUT
				if (PARAM_spewplayercard.Get())
				{
					const PlayerCardStatInfo* pStatInfo = SPlayerCardManager::GetInstance().GetStatInfo();
					const PlayerCardPackedStatInfo* pPackedInfo = SPlayerCardManager::GetInstance().GetPackedStatInfos();
					if(pStatInfo)
					{
						const PlayerListData& readData = GetReadDataBuffer();
						for(unsigned i=0; i < readData.GetStatsResultsNumRows(); ++i)
						{
							const rlGamerHandle* pGamerHandle = readData.GetStatsResultsGamerHandle(i);
							const char* pName = pGamerHandle ? GetName(*pGamerHandle) : NULL;
							if(pName && SPlayerCardManager::GetInstance().GetDataManager()->GetGamerIndex(*pGamerHandle) != -1)
							{
								playercardDebugf1("%s::Update - Stats recieved for gamer %s", ClassIdentifier(), pName);
								for(unsigned j=0; j<pStatInfo->GetStatDatas().size(); ++j)
								{
									const PlayerCardStatInfo::StatData& rStatData = pStatInfo->GetStatDatas()[j];
									const char* pStatName = rStatData.m_name.c_str();
									u32 statHash = atStringHash(pStatName);

									const rlProfileStatsValue& rStatValue = readData.GetStatValueFromRequestedRecord(i, j);
									if(rStatValue.IsValid())
									{
										switch (rStatValue.GetType())
										{
										case RL_PROFILESTATS_TYPE_INT32:
										{
											playercardDebugf1("	BasePlayerCardDataManager::Update - Stat%d = %s(0x%08x, %u), Index: %u, Type: int32, Value: %d", j, pStatName, statHash, statHash, i, rStatValue.GetInt32());
											break;
										}
										case RL_PROFILESTATS_TYPE_INT64:
										{
											playercardDebugf1("	BasePlayerCardDataManager::Update - Stat%d = %s(0x%08x, %u), Index: %u, Type: int64, Value: %" I64FMT "d", j, pStatName, statHash, statHash, i, rStatValue.GetInt64());
											break;
										}
										case RL_PROFILESTATS_TYPE_FLOAT:
										{
											playercardDebugf1("	BasePlayerCardDataManager::Update - Stat%d = %s(0x%08x, %u), Index: %u, Type: float, Value: %f", j, pStatName, statHash, statHash, i, rStatValue.GetFloat());
											break;
										}
										case RL_PROFILESTATS_TYPE_INVALID:
										{
											playercardDebugf1("	BasePlayerCardDataManager::Update - Stat%d = %s(0x%08x, %u), Index: %u, Type: UNKNOWN", j, pStatName, statHash, statHash, i);
											break;
										}
										}
									}
								}

								if(pPackedInfo)
								{
									for(unsigned j=0; j<pPackedInfo->GetStatDatas().size(); ++j)
									{
										const PlayerCardPackedStatInfo::StatData& rPackedData = pPackedInfo->GetStatDatas()[j];
										const char* pPackedAlias = rPackedData.m_alias.TryGetCStr();
										const char* pPackedName = rPackedData.m_realName.TryGetCStr();
										u32 packedAliasHash = rPackedData.m_alias.GetHash();
										u32 packedNameHash = rPackedData.m_realName.GetHash();
										int packedIndex = rPackedData.m_index;
										bool isInt = rPackedData.m_isInt;

										const rlProfileStatsValue* pStatValue = SPlayerCardManager::GetInstance().GetStat(*pGamerHandle, packedNameHash);
										if(pStatValue && pStatValue->IsValid())
										{
											if(pStatValue->GetType() == RL_PROFILESTATS_TYPE_INT64)
											{
												const u64 statValue = static_cast<u64>(pStatValue->GetInt64());
												if(isInt)
												{
													const u64 numberOfBits = 8;
													const u64 shift = numberOfBits*packedIndex;
													const int value = SPlayerCardManager::GetInstance().ExtractPackedValue(statValue, shift, numberOfBits);

													playercardDebugf1("	BasePlayerCardDataManager::Update - PackedStat%d = %s(0x%08x, %d) for alias %s(0x%08x, %d) is an int at index %d using %" I64FMT "u bits (shift=%" I64FMT "u) with value of %d", j, pPackedName, packedNameHash, packedNameHash, pPackedAlias, packedAliasHash, packedAliasHash, packedIndex, numberOfBits, shift, value);
												}
												else
												{
													const u64 numberOfBits = 1;
													const u64 shift = numberOfBits*packedIndex;
													const int value = SPlayerCardManager::GetInstance().ExtractPackedValue(statValue, shift, numberOfBits);

													playercardDebugf1("	BasePlayerCardDataManager::Update - PackedStat%d =  %s(0x%08x, %d) for alias %s(0x%08x, %d) is an bool at index %d using %" I64FMT "u bits (shift=%" I64FMT "u) with value of %s", j, pPackedName, packedNameHash, packedNameHash, pPackedAlias, packedAliasHash, packedAliasHash, packedIndex, numberOfBits, shift, value?"true":"false");
												}
											}
											else
											{
												playercardDebugf1("	BasePlayerCardDataManager::Update - PackedStat%d =  %s(0x%08x, %d) for alias %s(0x%08x, %d) isn't of type int64. type=%d", j, pPackedName, packedNameHash, packedNameHash, pPackedAlias, packedAliasHash, packedAliasHash, pStatValue->GetType());
											}
										}
										else
										{
											playercardDebugf1("	BasePlayerCardDataManager::Update - Unable to find packed stat %s(0x%08x, %d) for alias %s(0x%08x, %d)", pPackedName, packedNameHash, packedNameHash, pPackedAlias, packedAliasHash, packedAliasHash);
										}
									}
								}
							}
						}
					}
				}
#endif // !__NO_OUTPUT
			}
			else
			{
				playercardErrorf("%s::Update - Stats Query Failed, resultCode=%d", ClassIdentifier(), m_statsQueryStatus.GetResultCode());
				SetState(ERROR_FAILED_POPULATING_STATS);
			}
		}
	}
}

void BasePlayerCardDataManager::OnGamerHandlesPopulated()
{
	playercardNetDebugf1("BasePlayerCardDataManager :: OnGamerHandlesPopulated");
	const atArray<rlGamerHandle>& gamerHandles = GetWriteDataBuffer().m_gamerHandles;
	if(gamerHandles.GetCount())
	{
#if RSG_DURANGO
		SetState(STATE_POPULATING_GAMER_NAMES);
		// clear previous request
		if (m_gamerNamesReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
		{
			uiErrorf("Gamer handles populated with previous display name request (id: %d)", m_gamerNamesReqID);
			CLiveManager::GetFindDisplayName().CancelRequest(m_gamerNamesReqID);
			m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
		}
		m_gamerNamesReqID = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), gamerHandles.GetElements(), gamerHandles.GetCount());		
#else
		OnGamerNamesPopulated();
#endif
	}
	else
	{
		SetState(STATE_SUCCESS);
	}
}

bool BasePlayerCardDataManager::GetCachedGamerStats(const int OUTPUT_ONLY(localGamerIndex), const rlGamerHandle* gamers, unsigned numGamers, rlProfileStatsReadResults* results)
{
	if (!PARAM_friendslistcaching.Get() && !s_enableFriendsListStatCaching)
	{
		return false;
	}

	//refresh the cache 
	const unsigned currentTime = sysTimer::GetSystemMsTime();
	if (currentTime > (s_lastCacheTime + STATS_REFRESH_INTERVAL))
	{
		return false;
	}

	if (s_cachedStatsRequests.empty())
	{
		return false;
	}

	bool success = true;

	atArray<CachedStatsRequest*> cachedGamerStats;

	for (unsigned i = 0; (i < numGamers) && success; ++i)
	{
		auto it = std::find_if(
			std::begin(s_cachedStatsRequests),
			std::end(s_cachedStatsRequests),
			[gamers, i](CachedStatsRequest& cached) { return cached.m_gamerHandle == gamers[i]; }
		);

		if (it != s_cachedStatsRequests.end() && it->m_hasStats)
		{
			cachedGamerStats.PushAndGrow(it);
		}

		success = it != s_cachedStatsRequests.end();
	}

	m_usingCachedStats = success;

	//can populate stats from cache, all gamers were found
	if (success)
	{
		const int numToRead = rlProfileStats::MAX_NUM_GAMERS_FOR_STATS_READ == -1 ? cachedGamerStats.GetCount() : rage::Min(rlProfileStats::MAX_NUM_GAMERS_FOR_STATS_READ, static_cast<int>(cachedGamerStats.GetCount()));

		playercardNetDebugf1("%s::GetCachedGamerStats :: Num gamers to read from cache: %u", ClassIdentifier(), numToRead);
		results->SetNumRows(numToRead);

		for (unsigned i = 0; i < numToRead; ++i)
		{
			//rlProfileStatsValue stats[MAX_STATS];
			CachedStatsRequest* cachedStatLookup = cachedGamerStats[i];

			results->SetGamerHandle(i, cachedStatLookup->m_gamerHandle);

			const CacheRequestID openRequestId = CloudCache::OpenCacheFile(cachedStatLookup->m_hash, CachePartition::Default);
			OUTPUT_ONLY(const char* gamerName = GetName(cachedStatLookup->m_gamerHandle));

			playercardNetDebugf1("%s::GetCachedGamerStats :: Opening cache file for gamer: %s. Name: %s, Request id: %u. Lookup hash: %u", ClassIdentifier(), 
				cachedStatLookup->m_gamerHandle.ToString(), gamerName,
				openRequestId, cachedStatLookup->m_hash);
			
			if (openRequestId != INVALID_CACHE_REQUEST_ID)
			{
				cachedStatLookup->m_requestId = openRequestId;
				cachedStatLookup->m_results = results;
				cachedStatLookup->m_manager = this;
			}
			else
			{
				playercardNetDebugf1("%s::GetCachedGamerStats :: Failed to open cache file. Gamer: %s, Hash: %d", ClassIdentifier(), cachedStatLookup->m_gamerHandle.ToString(), cachedStatLookup->m_hash);
			}
		}
	}

	playercardNetDebugf1("%s::GetCachedGamerStats :: Retrieving stats from cache for %u gamers. Local gamer index: %u, Success: %s", ClassIdentifier(), numGamers, localGamerIndex, BOOL_TO_STRING(success));

	return success;
}

void BasePlayerCardDataManager::OnGamerNamesPopulated()
{
	playercardNetDebugf1("BasePlayerCardDataManager :: OnGamerNamesPopulated");
	PlayerListData& writeData = GetWriteDataBuffer();
	const s32 numberOfGamers = writeData.m_gamerHandles.GetCount();

	if(numberOfGamers > 0)
	{
		RefreshGamerData();

		writeData.RequestedRecordsResizeGrow(numberOfGamers);

		playercardAssertf(m_pStats->size() <= writeData.GetRequestedRecordsMaxStats(), "%s::Update - Too many stats are listed.  Want %d, but max is %u", 
			ClassIdentifier(), m_pStats->size(), writeData.GetRequestedRecordsMaxStats());
		playercardDebugf1("%s::OnGamerNamesPopulated player stats size is: %d", ClassIdentifier(), m_pStats->size());

		if(playercardVerify(!m_pStats->empty()))
		{
			m_statsQueryStatus.Reset();

			if(NetworkInterface::HasValidRosCredentials())
			{
				writeData.InitializeStatsResults(m_pStats);

				playercardNetDebugf1("%s::OnGamerNamesPopulated :: Reading gamer stats", ClassIdentifier());
				if (!GetCachedGamerStats(NetworkInterface::GetLocalGamerIndex(), writeData.m_gamerHandles.GetElements(), writeData.m_gamerHandles.GetCount(), &writeData.m_statsResults))
				{
					if (playercardVerify(rlProfileStats::ReadStatsByGamer(NetworkInterface::GetLocalGamerIndex()
						, writeData.m_gamerHandles.GetElements()
						, writeData.m_gamerHandles.size()
						, &writeData.m_statsResults
						, &m_statsQueryStatus)))
					{
						SetState(STATE_POPULATING_STATS);
						APPEND_PF_READBYGAMER(CPauseMenu::GetCurrentScreen().GetValue(), writeData.m_gamerHandles.size())
					}
					else
					{
						playercardErrorf("%s::OnGamerNamesPopulated - rlProfileStats::ReadStatsByGamer Failed", ClassIdentifier());
						SetState(ERROR_FAILED_POPULATING_GAMERS);
					}
				}
				else
				{
					SetState(STATE_POPULATING_STATS);
				}
			}
			else
			{
				playercardErrorf("%s::OnGamerNamesPopulated - NetworkInterface::HasValidRosCredentials() are Invalid", ClassIdentifier());
				SetState(ERROR_FAILED_POPULATING_GAMERS);
			}
		}
		else
		{
			SetState(STATE_STATS_POPULATED);
		}
	}
	else
	{
		SetState(STATE_STATS_POPULATED);
	}
}

void BasePlayerCardDataManager::CreateRequestedRecords(PlayerListData::eTypeOfProfileStats typeOfProfileStats)
{
	m_Data[0].CreateRequestedRecords(typeOfProfileStats);
	if (m_bAllowDoubleBuffering)
	{
		m_Data[1].CreateRequestedRecords(typeOfProfileStats);
	}
}

// void BasePlayerCardDataManager::ReserveRequestedRecords(s32 arraySizeToReserve)
// {
// 	m_Data[0].RequestedRecordsResizeGrow(arraySizeToReserve);
// 	if (m_bAllowDoubleBuffering)
// 	{
// 		m_Data[1].RequestedRecordsResizeGrow(arraySizeToReserve);
// 	}
// }

void BasePlayerCardDataManager::SwapListDataBuffer()
{
	playercardDebugf3("Swapping player list data read buffer %d to %d", m_readBufferIndex, GetWriteDataBufferIndex());

	// Change the read index to point to our write buffer
	m_readBufferIndex = GetWriteDataBufferIndex();

	// Clear the old read buffer, which is now the write buffer
	GetWriteDataBuffer().Reset();
}

const BasePlayerCardDataManager::GamerData* BasePlayerCardDataManager::FindGamerDataByUserId(const char* userId) const
{
	u32 userIdHash = atStringHash(userId);
	const PlayerListData& readData = GetReadDataBuffer();

	for(int j=0; j < readData.m_gamerDatas.size(); ++j)
	{
		if(userIdHash == readData.m_gamerDatas[j].m_userIdHash)
		{
			return &readData.m_gamerDatas[j];
		}
	}

	return NULL;
}

bool BasePlayerCardDataManager::IsValidPlayer(const CNetGamePlayer* pNetPlayer) const
{
	bool retVal = false;
	if(pNetPlayer)
	{
		retVal = true;

		if(m_listRules.m_hideLocalPlayer && pNetPlayer->IsLocal())
		{
			retVal = false;
		}
		else if(m_listRules.m_hidePlayersInTutorial)
		{
			if(NetworkInterface::GetLocalPlayer() && NetworkInterface::ArePlayersInDifferentTutorialSessions(*NetworkInterface::GetLocalPlayer(), *pNetPlayer))
			{
				retVal = false;
			}
		}
	}

	return retVal;
}

bool BasePlayerCardDataManager::HasPlayedMode(const rlGamerHandle& rGamerHandle) const
{
	u64 lastTimePlayed = 0;

	const CPlayerCardXMLDataManager& rXML = SPlayerCardManager::GetInstance().GetXMLDataManager();
	const CPlayerCardXMLDataManager::PlayerCard& rCard = CPauseMenu::IsSP() ? rXML.GetPlayerCard(CPlayerCardXMLDataManager::PCTYPE_SP) : rXML.GetPlayerCard(CPlayerCardXMLDataManager::PCTYPE_FREEMODE);
	int timestampHash = static_cast<int>(rCard.m_timestamp.GetStat(0));

	const rlProfileStatsValue* pTimestamp = GetStat(rXML, rGamerHandle, timestampHash);

	if(pTimestamp && pTimestamp->IsValid())
	{
		switch(pTimestamp->GetType())
		{
		case RL_PROFILESTATS_TYPE_INT64:
			lastTimePlayed = pTimestamp->GetInt64();
			break;
		default:
			playercardAssertf(0, "%s::GetTimestampValue - stat %s(0x%08x) is an unhandled stat type (%d).", ClassIdentifier(), SPlayerCardManager::GetInstance().GetStatName(timestampHash), timestampHash, (int)pTimestamp->GetType());
			break;
		}

#if !__FINAL && 0 // && 0 because the stats are "probably" stable enough now.
		// In Dev, lets ignore accounts older than 2 weeks, they probably have bad stats.
		const u64 cOldDevPeriod = 60*60*24*14;
		if(cOldDevPeriod < (rlGetPosixTime() - lastTimePlayed))
		{
			playercardDebugf1("%s::GetTimestampValue - rlGetPosixTime=%" I64FMT "u, lastTimePlayed=%" I64FMT "u, diff=%" I64FMT "u, limit=%" I64FMT "u", ClassIdentifier(), rlGetPosixTime(), lastTimePlayed, rlGetPosixTime() - lastTimePlayed, cOldDevPeriod);
			lastTimePlayed = 0;
		}
#endif
	}

	playercardDebugf1("%s::HasPlayedMode - '%s'='%s'.", ClassIdentifier(), NetworkUtils::LogGamerHandle(rGamerHandle), lastTimePlayed != 0 ? "true":"false");

	return lastTimePlayed != 0;
}

#if !__NO_OUTPUT
void BasePlayerCardDataManager::PrintProfileStatsResults(const rlProfileStatsReadResults& results)
{
	for (unsigned i = 0; i < results.GetNumRows(); ++i)
	{
		const rlGamerHandle* gamerHandle = results.GetGamerHandle(i);
		playercardNetDebugf1("PrintProfilestatsReadResults :: Gamer: %s, Name: %s", gamerHandle->ToString(), GetName(*gamerHandle));
	}
}
#endif

void CFriendPlayerCardDataManager::OnCacheFileLoaded(const CacheRequestID requestId, bool success)
{
	auto it = std::find_if(std::begin(s_cachedStatsRequests), std::end(s_cachedStatsRequests), [requestId](CachedStatsRequest& lookup) { return lookup.m_requestId == requestId; });

	if (it != std::end(s_cachedStatsRequests))
	{
		if (it->m_manager != this) return;

		if (!success)
		{
			playercardNetWarningf("%s::OnCacheFileLoaded :: Failed to load stats from cache. Gamer: %s, RequestID: %u", ClassIdentifier(), it->m_gamerHandle.ToString(), requestId);
			return;
		}

		fiStream* dataStream = CloudCache::GetCacheDataStream(requestId);
		CachedStats* cachedStats = Alloca(CachedStats, 1);
		dataStream->Read(cachedStats, sizeof(CachedStats));

#if !__NO_OUTPUT
		const char* gamerName = GetName(cachedStats->m_gamerHandle);
		const int gamerIndex = GetGamerIndex(cachedStats->m_gamerHandle);
#endif

		rlProfileStatsReadResults* results = it->m_results;

		if (playercardNetVerifyf(results, "%s::OnCacheFileLoaded :: Results not set", ClassIdentifier()))
		{
			playercardDebugf1("CFriendPlayerCardDataManager :: OnCacheFileLoaded");
			const int* statIds = results->GetRequestedStatIds();

			int row = -1;
			for (unsigned k = 0; k < results->GetNumRows(); ++k)
			{
				const rlGamerHandle* gh = results->GetGamerHandle(k);

				if (gh != nullptr && *gh == cachedStats->m_gamerHandle)
				{
					row = k;
					break;
				}
			}

			if (row == -1)
			{
				playercardNetWarningf("%s::OnCacheFileLoaded :: Couldn't find gamer in results. Gamer: %s, CloudRequestID: %u, Num result rows: %u, Num cache lookups: %u", ClassIdentifier(),
					cachedStats->m_gamerHandle.ToString(), requestId, results->GetNumRows(), s_cachedStatsRequests.GetCount());
				return;
			}

			for (unsigned j = 0; j < results->GetNumStatIds(); ++j)
			{
				int statId = statIds[j];

				const rlProfileStatsValue* cachedStatValue = cachedStats->GetStatById(statId);

				if (cachedStatValue)
				{
					rlProfileStatsValue* resultsValue = const_cast<rlProfileStatsValue*>(results->GetValue(row, j));

					if (playercardNetVerifyf(resultsValue,
						"%s::OnCacheFileLoaded :: Couldn't find results value. Gamer: %s, Row: %u, Col: %u, Num rows: %u", ClassIdentifier(), cachedStats->m_gamerHandle.ToString(), row, j, results->GetNumRows()))
					{
						*resultsValue = *cachedStatValue;
					}

#if !__NO_OUTPUT
					if (PARAM_friendslistcachingverbose.Get())
					{
						const PlayerCardStatInfo* pStatInfo = SPlayerCardManager::GetInstance().GetStatInfo();
						const PlayerCardStatInfo::StatData& rStatData = pStatInfo->GetStatDatas()[j];
						const char* pStatName = rStatData.m_name.c_str();
						u32 statHash = atStringHash(pStatName);
						switch (resultsValue->GetType())
						{
						case RL_PROFILESTATS_TYPE_INT32:
						{
							playercardNetDebugf1("%s::OnCacheFileLoaded :: Stat - Id: %d, Type: %d, Name: %s, Hash: %u, Type: int32, Value: %d", ClassIdentifier(), statId, cachedStatValue->GetType(), pStatName, statHash, resultsValue->GetInt32());
							break;
						}

						case RL_PROFILESTATS_TYPE_INT64:
						{
							playercardNetDebugf1("%s::OnCacheFileLoaded :: Stat - Id: %d, Type: %d, Name: %s, Hash: %u, Type: int64, Value: %" I64FMT "d", ClassIdentifier(), statId, cachedStatValue->GetType(), pStatName, statHash, resultsValue->GetInt64());
							break;
						}

						case RL_PROFILESTATS_TYPE_FLOAT:
						{
							playercardNetDebugf1("%s::OnCacheFileLoaded :: Stat - Id: %d, Type: %d, Name: %s, Hash: %u, Type: float, Value: %f", ClassIdentifier(), statId, cachedStatValue->GetType(), pStatName, statHash, resultsValue->GetFloat());
							break;
						}
						case RL_PROFILESTATS_TYPE_INVALID:
						{
							playercardNetDebugf1("%s::OnCacheFileLoaded :: Stat - Id: %d, Type: %d, Name: %s, Hash: %u, Type: INVALID", ClassIdentifier(), statId, cachedStatValue->GetType(), pStatName, statHash);
							break;
						}
						}
					}
#endif
				}
			}
		}

		playercardNetDebugf1("%s::OnCacheFileLoaded :: Loaded stats for gamer: %s, Name: %s, Index: %u, Num stats: %u, Retrieved stats: %s", 
			ClassIdentifier(), cachedStats->m_gamerHandle.ToString(), gamerName, gamerIndex, cachedStats->m_numStats, BOOL_TO_STRING(cachedStats->m_retrieved));
		it->m_requestId = INVALID_CACHE_REQUEST_ID;

		//mark load as complete once all requests have been processed
		it = std::find_if(std::begin(s_cachedStatsRequests), std::end(s_cachedStatsRequests), [requestId](CachedStatsRequest& lookup) { return lookup.m_requestId != INVALID_CACHE_REQUEST_ID; });
		if (it == s_cachedStatsRequests.end())
		{
			m_cacheLoadComplete = true;
		}
	}
}

const rlProfileStatsValue* BasePlayerCardDataManager::GetStat(int playerIndex, int statIndex) const
{
	const PlayerListData& readData = GetReadDataBuffer();
	return readData.GetStatValueFromStatsResults(playerIndex, statIndex, ClassIdentifier());
}

int BasePlayerCardDataManager::GetNumPages() const
{
	int pageSize =  GetPageSize();
	if(pageSize)
	{
		return 1 + (GetTotalUncachedGamerCount() / GetPageSize());
	}
	else
	{
		return 1;
	}
}

void BasePlayerCardDataManager::RefreshGamerData()
{
	// Refresh Read Data
	PlayerListData& readData = const_cast<PlayerListData&>(GetReadDataBuffer());
	for(int i=0; i<readData.m_gamerHandles.GetCount(); ++i)
	{
		CommonRefreshGamerData(readData.m_gamerDatas[i], readData.m_gamerHandles[i]);
	}

	if(m_bAllowDoubleBuffering)
	{
		// Refresh Write Data
		PlayerListData& writeData = GetWriteDataBuffer();
		for(int i=0; i<writeData.m_gamerHandles.GetCount(); ++i)
		{
			CommonRefreshGamerData(writeData.m_gamerDatas[i], writeData.m_gamerHandles[i]);
		}
	}
}

void BasePlayerCardDataManager::CommonRefreshGamerData(GamerData& rData, const rlGamerHandle& rGamerHandle)
{
	bool wasInvited = rData.m_hasBeenInvited;
	bool wasInSession = rData.m_isInSameSession;
	bool wasInTransition = rData.m_isInTransition;
	bool wasOnCall = rData.m_isOnCall;

	rData.Reset();

	const CNetworkInvitedPlayers::sInvitedGamer* pInvitedPlayer = NULL;

	if(CNetwork::GetNetworkSession().IsTransitionActive())
	{
		if(CNetwork::GetNetworkSession().HasTransitionPlayerFullyJoined(rGamerHandle))
		{
			rData.m_hasFullyJoined = true;
			rData.m_hasBeenInvited = true;
			rData.m_isWaitingForInviteAck = true;
		}
		else
		{
			pInvitedPlayer = &CNetwork::GetNetworkSession().GetTransitionInvitedPlayers().GetInvitedPlayer(rGamerHandle);
		}

		rData.m_isInTransition = CNetwork::GetNetworkSession().IsTransitionMember(rGamerHandle) && !CNetwork::GetNetworkSession().IsTransitionMemberSpectating(rGamerHandle);

	}
	else
	{
		if(NetworkInterface::GetActivePlayerIndexFromGamerHandle(rGamerHandle) != MAX_NUM_ACTIVE_PLAYERS)
		{
			rData.m_hasFullyJoined = true;
		}

		pInvitedPlayer = &CNetwork::GetNetworkSession().GetInvitedPlayers().GetInvitedPlayer(rGamerHandle);
	}

	if(pInvitedPlayer && pInvitedPlayer->IsValid())
	{
		if(pInvitedPlayer->m_bHasAck)
		{
			rData.m_hasBeenInvited = true;
		}
		else
		{
			rData.m_isWaitingForInviteAck = pInvitedPlayer->IsWithinAckThreshold();
		}

		if (CNetwork::GetNetworkSession().IsInviteDecisionMade(pInvitedPlayer->m_hGamer) && (static_cast<int>(CNetwork::GetNetworkSession().GetInviteStatus(pInvitedPlayer->m_hGamer)) & InviteResponseFlags::IRF_RejectBlocked) != 0)
		{
			rData.m_isBusy = false;
			rData.m_inviteBlockedLabelHash = ATSTRINGHASH("FM_PLY_BLCK", 0xC8613699);
		}
		else
		{
			rData.m_isBusy = IsPlayerBusy(pInvitedPlayer->m_nStatus);
		}
	}

	if (wasOnCall && !CNetwork::GetNetworkSession().HasTransitionPlayerFullyJoined(rGamerHandle))
	{
		rData.m_isOnCall = wasOnCall;
	}

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(rGamerHandle);
	if(pPlayer || rData.m_isInTransition)
	{
		rData.m_isValid = true;
		rData.m_isOnline = true;
		rData.m_isInSameTitle = true;
		rData.m_isInSameTitleOnline = true;
		rData.m_isInSameSession = true;

		if(pPlayer)
		{
			if(CNetwork::GetNetworkSession().IsTransitionActive())
			{
				rData.m_isHost = pPlayer->IsLocal();
			}
			else
			{
				rData.m_isHost = pPlayer->IsHost();
			}

			const CPlayerCardXMLDataManager::MiscData& rMiscData = SPlayerCardManager::GetInstance().GetXMLDataManager().GetMiscData();
			fwDecoratorExtension::Get(pPlayer->GetPlayerInfo(), rMiscData.m_teamIdTag, rData.m_teamId);
		}
	}
	else
	{
		const rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(rGamerHandle);
		if(pFriend)
		{
			rData.m_isValid = true;
			rData.m_isOnline = pFriend->IsOnline();

			rData.m_isInSameTitle = pFriend->IsInSameTitle();
			if(rData.m_isOnline && rData.m_isInSameTitle)
			{
				bool bIsSameSession = CNetwork::GetNetworkSession().IsSessionMember(rGamerHandle);
				bool bIsTransitionSession = CNetwork::GetNetworkSession().IsTransitionMember(rGamerHandle);
				bool bIsInASession = bIsSameSession || bIsTransitionSession || pFriend->IsInSession();
				rData.m_isInSameTitleOnline =  bIsInASession;
			}
		}
#if PARTY_PLATFORM
		else if(CLiveManager::IsOnline() && CLiveManager::IsInPlatformParty())
		{
			rlPlatformPartyMember partyMember[MAX_NUM_PARTY_MEMBERS];
			unsigned numMembers = CLiveManager::GetPlatformPartyInfo(partyMember, MAX_NUM_PARTY_MEMBERS);
			for(unsigned i=0; i<numMembers; ++i)
			{
				rlGamerHandle tempGamerHandle;

				partyMember[i].GetGamerHandle(&tempGamerHandle);
				if(tempGamerHandle == rGamerHandle)
				{
					rData.m_isValid = true;
					rData.m_isOnline = true;
					rData.m_isInSameTitle = partyMember[i].IsInSession();
					rData.m_isInSameTitleOnline = rData.m_isInSameTitle;

					break;
				}
			}
		}
#endif
	}

	if(wasInvited &&
		((wasInSession && !rData.m_isInSameSession) || (wasInTransition && !rData.m_isInTransition)))
	{
		if(CNetwork::GetNetworkSession().IsTransitionEstablished() && !rData.m_isInTransition && !CNetwork::GetNetworkSession().IsActivitySession())
		{
			CNetwork::GetNetworkSession().CancelTransitionInvites(&rGamerHandle, 1);
			CNetwork::GetNetworkSession().RemoveTransitionInvites(&rGamerHandle, 1);
		}

		if(CNetwork::GetNetworkSession().IsSessionEstablished() && !rData.m_isInSameSession)
		{
			CNetwork::GetNetworkSession().CancelInvites(&rGamerHandle, 1);
			CNetwork::GetNetworkSession().RemoveInvites(&rGamerHandle, 1);
		}
	}

	// If we have an error set by script but the player then joins, we should clear the error.  
	if (rData.m_inviteBlockedLabelHash && (!wasInTransition && rData.m_isInTransition) )
	{
		rData.m_inviteBlockedLabelHash = 0;
	}

	if(!rData.m_isOnline)
	{
		rData.m_isDefaultOnline = false;
	}

	
	playercardDebugf3(" ==== Gamerhandle: %s ====", rData.GetName());
	playercardDebugf3(" hasBeenInvited: %d  isInSameTitle: %d",rData.m_hasBeenInvited, rData.m_isInSameTitle);
	playercardDebugf3(" isInSameSession: %d, isInSameTitleOnline: %d",rData.m_isInSameSession, rData.m_isInSameTitleOnline);

}

bool BasePlayerCardDataManager::IsPlayerBusy(unsigned nStatus)
{
	if (nStatus & InviteResponseFlags::IRF_StatusInLobby ||
		nStatus & InviteResponseFlags::IRF_StatusInActivity ||
		nStatus & InviteResponseFlags::IRF_StatusInStore ||
		nStatus & InviteResponseFlags::IRF_StatusInCreator ||
		nStatus & InviteResponseFlags::IRF_RejectCannotAccessMultiplayer)
		return true;

	return false;
}

const atArray<int>* BasePlayerCardDataManager::GetStatIds(const CPlayerCardXMLDataManager& rStatManager) const
{
	const PlayerCardStatInfo* pInfo = GetStatInfo(rStatManager);
	return pInfo ? &(pInfo->GetStatHashes()) : NULL;
}

const rlProfileStatsValue* BasePlayerCardDataManager::GetStat(const CPlayerCardXMLDataManager& rStatManager, const rlGamerHandle& gamerHandle, int statHash) const
{
	const atArray<int>* pStats = GetStatIds(rStatManager);
	if(pStats)
	{
		int statIndex = pStats->Find(statHash);
		if(playercardVerifyf(statIndex != -1, "%s::GetStat - Trying to get a stat with hash %s(0x%08x, %d), but the playerCardSetup.xml doesn't know about it.", ClassIdentifier(), SPlayerCardManager::GetInstance().GetStatName(statHash), statHash, statHash))
		{
			return GetStat(gamerHandle, statIndex);
		}
	}

	return NULL;
}

void BasePlayerCardDataManager::Reset()
{
	playercardDebugf1("BasePlayerCardDataManager :: Resetting %s", ClassIdentifier());

#if __BANK
	sysStack::PrintStackTrace();
#endif // __BANK

	if(m_statsQueryStatus.Pending())
		rlProfileStats::Cancel(&m_statsQueryStatus);

	m_statsQueryStatus.Reset();

	// Reset Write Buffer
	GetWriteDataBuffer().Reset();

	m_timeoutTimer.Zero();
	
#if RSG_DURANGO
	if (m_gamerNamesReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
	{
		CLiveManager::GetFindDisplayName().CancelRequest(m_gamerNamesReqID);
	}
	m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
#endif
	SetState(STATE_INVALID);

	m_state = STATE_INVALID;
	m_pStats = NULL;
}

void BasePlayerCardDataManager::SetState(eState newState)
{
	if(newState != m_state)
	{
		playercardDebugf2("%s::SetState -- Changing State from %d to %d", ClassIdentifier(), m_state, newState);
		m_state = newState;

		if(newState == STATE_SUCCESS && m_bAllowDoubleBuffering)
		{
			// The manager has succeeded in requesting data, let's swap the read/write buffers if we allow it
			SwapListDataBuffer();
		}
	}
}

bool BasePlayerCardDataManager::IsPlayerListDirty()
{
	// Should we use the read or write buffer to test against a dirty player list?
	// We check the read buffer if the manager has succeeded
	bool bCheckReadBuffer = HasSucceeded();
	bool bCantFindGamerIndex = false;
	const atArray<rlGamerHandle>& gamers = GetAllGamerHandles();

	for(int i = 0; i < gamers.size(); ++i)
	{
		if(bCheckReadBuffer)
			bCantFindGamerIndex = GetGamerIndex(gamers[i]) == -1;
		else
			bCantFindGamerIndex =  GetWriteDataBuffer().m_gamerHandles.Find((gamers[i])) == -1;

		if(bCantFindGamerIndex)
		{
			bool bIsLocalPlayer = gamers[i] == NetworkInterface::GetLocalGamerHandle();
			bool bIsSameSession = CNetwork::GetNetworkSession().IsSessionMember(gamers[i]);
			bool bIsTransitionSession = CNetwork::GetNetworkSession().IsTransitionMember(gamers[i]);
			bool bOverrideLocalPlayerCheck = CanDownloadLocalPlayer();

			// CanDownloadLocalPlayer() now can override behaviour to no flag local players as dirty.  This will allow us to 
			// query off local players on a case by case basis.
					
			if ( (bOverrideLocalPlayerCheck || !bIsLocalPlayer) && !bIsSameSession && bIsTransitionSession)
				return true;
		}
	}
		
	return false;
}

bool BasePlayerCardDataManager::ArePlayerStatsAccessible(const rlGamerHandle& gamerHandle) const
{
	return gamerHandle.IsLocal() || GetReadDataBuffer().HasReceivedRecords(gamerHandle);
}

bool BasePlayerCardDataManager::Exists(const rlGamerHandle& handle) const
{
	const PlayerListData& writeData = GetWriteDataBuffer();

	for(u32 i=0; i<writeData.m_gamerHandles.GetCount(); ++i)
	{
		if (handle == writeData.m_gamerHandles[i])
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CActivePlayerCardDataManager::CActivePlayerCardDataManager()
	: BasePlayerCardDataManager()
{
	CreateRequestedRecords(PlayerListData::PROFILE_STATS_MP_PLAYERS);
}

void CActivePlayerCardDataManager::PopulateGamerHandles()
{
	playercardDebugf1("CActivePlayerCardDataManager :: PopulateGamerHandles");
	PlayerListData& writeData = GetWriteDataBuffer();
	if(writeData.m_gamerHandles.max_size() == 0)
	{
		writeData.m_gamerHandles.Reserve(MAX_NUM_ACTIVE_PLAYERS);
		writeData.m_gamerDatas.Reserve(MAX_NUM_ACTIVE_PLAYERS);
		m_currentHandles.Reserve(MAX_NUM_ACTIVE_PLAYERS);
	}
	else
	{
		writeData.m_gamerHandles.clear();
		writeData.m_gamerDatas.clear();
		m_currentHandles.clear();
	}

	for(u8 i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
	{
		const CNetGamePlayer* pPlayer = NetworkInterface::GetActivePlayerFromIndex(i);
		if(IsValidPlayer(pPlayer) && !pPlayer->IsSpectator())
		{
			writeData.m_gamerHandles.PushAndGrow(pPlayer->GetGamerInfo().GetGamerHandle());
			writeData.m_gamerDatas.Grow();
			writeData.m_hasRecords.PushAndGrow(true);

#if RSG_DURANGO
			if (pPlayer->GetGamerInfo().HasDisplayName())
				writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetDisplayName());
			else
				NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetName()));
#else
			NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetName()));
#endif // RSG_DURANGO
		}
	}

	playercardNetDebugf1("%s::PopulateGamerHandles :: Gamer handles size: %u, HasRecords size: %u", ClassIdentifier(), writeData.m_gamerHandles.GetCount(), writeData.m_hasRecords.GetCount());

	m_currentHandles = writeData.m_gamerHandles;
	RefreshGamerData();
	OnGamerHandlesPopulated();
}

void CActivePlayerCardDataManager::OnGamerNamesPopulated()
{
	playercardNetDebugf1("CActivePlayerCardDataManager :: OnGamerNamesPopulated");
	SetState(STATE_STATS_POPULATED);
}

const PlayerCardStatInfo* CActivePlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPPlayersStats();
}

const PlayerCardPackedStatInfo* CActivePlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPPackedPlayersStats();
}

void CActivePlayerCardDataManager::RefreshGamerData()
{
	playercardDebugf1("CActivePlayerCardDataManager :: RefreshGamerData");
	m_currentHandles.clear();
	for(u8 i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
	{
		const CNetGamePlayer* pPlayer = NetworkInterface::GetActivePlayerFromIndex(i);
		if(IsValidPlayer(pPlayer) && !pPlayer->IsSpectator())
		{
			m_currentHandles.PushAndGrow(pPlayer->GetGamerInfo().GetGamerHandle());
		}
	}

	BasePlayerCardDataManager::RefreshGamerData();
}

void CActivePlayerCardDataManager::Reset()
{
	ClearRefreshFlag();
	BasePlayerCardDataManager::Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CFriendPlayerCardDataManager::CFriendPlayerCardDataManager():
	BasePlayerCardDataManager(),
	m_iFriendIndex(0),
	m_iRequestedFriendIndex(-1)
{
}

void CFriendPlayerCardDataManager::RequestStatsQuery(int iStartIndex)
{
	playercardDebugf1("CFriendPlayerCardDataManager :: RequestStatsQuery %d", iStartIndex);
	m_iRequestedFriendIndex = iStartIndex;

	PlayerListData& writeData = GetWriteDataBuffer();
	if (m_iFriendIndex >= writeData.m_gamerHandles.size())
	{
		playercardAssertf(m_iFriendIndex >= writeData.m_gamerHandles.size(), "m_iFriendIndex out of bounds! m_iFriendIndex = %d.",m_iFriendIndex);
	}
}

void CFriendPlayerCardDataManager::PopulateGamerHandles()
{
	playercardDebugf1("CFriendPlayerCardDataManager :: PopulateGamerHandles");
	PlayerListData& writeData = GetWriteDataBuffer();

	writeData.m_gamerHandles.Reset();
	writeData.m_hasRecords.Reset();
#if __BANK
	if( ms_bDebugHideAllFriends )
	{
		InitUGCQueries();
		OnGamerHandlesPopulated();
		return;
	}
#endif

	CLiveManager::GetFriendsPage()->FillFriendArray(writeData.m_gamerHandles);
	writeData.m_gamerDatas.ResizeGrow(writeData.m_gamerHandles.GetCount());

	for(int i=0; i < writeData.m_gamerDatas.GetCount(); ++i)
	{
		const rlFriend* pFriend = CLiveManager::GetFriendsPage()->GetFriend(writeData.m_gamerHandles[i]);
		writeData.m_gamerDatas[i].SetName(pFriend ? pFriend->GetName() : "");
		writeData.m_hasRecords.PushAndGrow(false);
	}

	if (m_iRequestedFriendIndex > -1 && writeData.m_gamerHandles.size() > 0)
		m_iFriendIndex = m_iRequestedFriendIndex % writeData.m_gamerHandles.size();


	playercardNetDebugf1("%s::PopulateGamerHandles :: Gamer handles size: %u, HasRecords size: %u", ClassIdentifier(), writeData.m_gamerHandles.GetCount(), writeData.m_hasRecords.GetCount());

	InitUGCQueries();
	OnGamerHandlesPopulated();
}

void CFriendPlayerCardDataManager::OnGamerNamesPopulated()
{
	playercardNetDebugf1("CFriendPlayerCardDataManager :: OnGamerNamesPopulated");
	PlayerListData& writeData = GetWriteDataBuffer();
	const s32 numberOfGamers = writeData.m_gamerHandles.GetCount();

	if(numberOfGamers > 0)
	{
		RefreshGamerData();

		// Cap the size at MAX_FRIEND_STAT_QUERY
		// That is the highest number of players that iUpperBound is ever set to in this function.
		// iUpperBound is then used in the call to rlProfileStats::ReadStatsByGamer()
		const u32 numberOfRecords = MIN(numberOfGamers, MAX_FRIEND_STAT_QUERY);
		writeData.RequestedRecordsResizeGrow(numberOfRecords);

		playercardAssertf(m_pStats->size() <= writeData.GetRequestedRecordsMaxStats(), "%s::Update - Too many stats are listed.  Want %d, but max is %u", 
			ClassIdentifier(), m_pStats->size(), writeData.GetRequestedRecordsMaxStats());
		playercardDebugf1("%s::OnGamerNamesPopulated player stats size is: %d", ClassIdentifier(), m_pStats->size());

		if(playercardVerify(!m_pStats->empty()))
		{
			m_statsQueryStatus.Reset();
			m_cacheLoadComplete = false;
			if(NetworkInterface::HasValidRosCredentials())
			{
				writeData.InitializeStatsResults(m_pStats);

				rage::rlGamerHandle* pGamerHandleStart = &(writeData.m_gamerHandles[m_iFriendIndex]);

				int iUpperBound = MAX_FRIEND_STAT_QUERY;
				//int iMaxNumberOfFriends = rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex() );

				if ( (m_iFriendIndex + MAX_FRIEND_STAT_QUERY) >= numberOfGamers)
				{
					iUpperBound = (numberOfGamers - m_iFriendIndex);
				}

				// Lets do some validation here to ensure the paging variables are right.
				playercardAssertf(iUpperBound >= 0, "%s::OnGamerNamesPopulated - iUpperBound has an invalid value (negative) iUpperBound=%d.", ClassIdentifier(), iUpperBound);

				if (iUpperBound <= 0) 
				{
					playercardErrorf("%s::OnGamerNamesPopulated - iUpperBound has an invalid value (negative) iUpperBound=%d.", ClassIdentifier(),iUpperBound);
					SetState(ERROR_FAILED_POPULATING_GAMERS);
					return;
				}
				
				if (playercardVerify(iUpperBound < MAX_GAMERS_READS) 
					&& playercardVerify(m_iFriendIndex <= numberOfGamers)
					&& playercardVerify(iUpperBound > 0)
					&& playercardVerify(m_iFriendIndex + iUpperBound <= numberOfGamers))
				{
					playercardNetDebugf1("%s::OnGamerNamesPopulated :: Reading gamer stats. Numer of gamers: %u, Upper bound: %u, Friend index: %u", ClassIdentifier(), numberOfGamers, iUpperBound, m_iFriendIndex);
					if (!GetCachedGamerStats(NetworkInterface::GetLocalGamerIndex(), writeData.m_gamerHandles.GetElements(), writeData.m_gamerHandles.GetCount(), &writeData.m_statsResults))
					{
						if (playercardVerify(rlProfileStats::ReadStatsByGamer(NetworkInterface::GetLocalGamerIndex()
							, pGamerHandleStart
							, iUpperBound
							, &writeData.m_statsResults
							, &m_statsQueryStatus)))
						{
							SetState(STATE_POPULATING_STATS);
							APPEND_PF_READBYGAMER(CPauseMenu::GetCurrentScreen().GetValue(), iUpperBound)
						}
						else
						{
							playercardErrorf("%s::OnGamerNamesPopulated - rlProfileStats::ReadStatsByGamer Failed", ClassIdentifier());
							SetState(ERROR_FAILED_POPULATING_GAMERS);
						}
					}
					else
					{
						SetState(STATE_POPULATING_STATS);
					}
				}
				else
				{
					playercardErrorf("%s::OnGamerNamesPopulated - rlProfileStats::ReadStatsByGamer Failed", ClassIdentifier());
					SetState(ERROR_FAILED_POPULATING_GAMERS);
				}
			}
			else
			{
				playercardErrorf("%s::OnGamerNamesPopulated - NetworkInterface::HasValidRosCredentials() are Invalid", ClassIdentifier());
				SetState(ERROR_FAILED_POPULATING_GAMERS);
			}
		}
		else
		{
			SetState(STATE_STATS_POPULATED);
		}
	}
	else
	{
		SetState(STATE_STATS_POPULATED);
	}
}

void CFriendPlayerCardDataManager::FixupGamerHandles()
{
	playercardDebugf1("CFriendPlayerCardDataManager :: FixupGamerHandles");
	PlayerListData& writeData = GetWriteDataBuffer();

	m_allFriendHandles = writeData.m_gamerHandles;

	int backIndex = writeData.m_gamerHandles.size()-1;
	for(int frontIndex=0; frontIndex<backIndex; ++frontIndex)
	{
		// If the player isn't currently in the session with you, then we need to check the stats to make sure the player has played the game.
		if(!writeData.m_gamerDatas[frontIndex].m_isInSameTitle &&
			(!NetworkInterface::IsNetworkOpen() || NetworkInterface::GetPlayerFromGamerHandle(writeData.m_gamerHandles[frontIndex]) == NULL)
			)
		{
			if(!HasPlayedMode(writeData.m_gamerHandles[frontIndex]))
			{
				for(; frontIndex<backIndex; --backIndex)
				{
					if(!NetworkInterface::IsNetworkOpen() || NetworkInterface::GetPlayerFromGamerHandle(writeData.m_gamerHandles[backIndex]) == NULL)
					{
						if(HasPlayedMode(writeData.m_gamerHandles[backIndex]) && (writeData.m_gamerDatas[frontIndex].m_isOnline == writeData.m_gamerDatas[backIndex].m_isOnline))
						{
							SWAP(writeData.m_gamerHandles[frontIndex], writeData.m_gamerHandles[backIndex]);
							SWAP(writeData.m_gamerDatas[frontIndex], writeData.m_gamerDatas[backIndex]);
							break;
						}
					}
				}
			}
		}
	}
}


CSPFriendPlayerCardDataManager::CSPFriendPlayerCardDataManager() : 
	CFriendPlayerCardDataManager()
{
	CreateRequestedRecords(PlayerListData::PROFILE_STATS_SP_FRIENDS);

// The PlayerCardDataManagers get created every time the pause menu opens - not once at the start of the game.
// So maybe it's not a good idea to reserve the array elements here.
// The player might never choose to view the Friends tab in the pause menu.
//	ReserveRequestedRecords(MAX_FRIEND_STAT_QUERY);
}

const PlayerCardStatInfo* CSPFriendPlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetSPFriendStats();
}

const PlayerCardPackedStatInfo* CSPFriendPlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetSPPackedStats();
}


CMPFriendPlayerCardDataManager::CMPFriendPlayerCardDataManager() : 
	CFriendPlayerCardDataManager()
{
	CreateRequestedRecords(PlayerListData::PROFILE_STATS_MP_FRIENDS);

// The PlayerCardDataManagers get created every time the pause menu opens - not once at the start of the game.
// So maybe it's not a good idea to reserve the array elements here.
// The player might never choose to view the Friends tab in the pause menu.
//	ReserveRequestedRecords(MAX_FRIEND_STAT_QUERY);
}

const PlayerCardStatInfo* CMPFriendPlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPFriendStats();
}

const PlayerCardPackedStatInfo* CMPFriendPlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPPackedStats();
}

void CFriendPlayerCardDataManager::Reset()
{
	m_allFriendHandles.Reset();
	BasePlayerCardDataManager::Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CPartyPlayerCardDataManager::CPartyPlayerCardDataManager() : 
	BasePlayerCardDataManager()
{
	CreateRequestedRecords(PlayerListData::PROFILE_STATS_MP_FRIENDS);
}

void CPartyPlayerCardDataManager::RefreshGamerData()
{
	playercardDebugf1("CPartyPlayerCardDataManager :: RefreshGamerData");
	BuildGamerHandles(m_currentHandles, false);

	BasePlayerCardDataManager::RefreshGamerData();
}

const PlayerCardStatInfo* CPartyPlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPFriendStats();
}

const PlayerCardPackedStatInfo* CPartyPlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPPackedStats();
}

void CPartyPlayerCardDataManager::PopulateGamerHandles()
{
	playercardDebugf1("CPartyPlayerCardDataManager :: PopulateGamerHandles");
	BuildGamerHandles(GetWriteDataBuffer().m_gamerHandles, true);

	InitUGCQueries();
	OnGamerHandlesPopulated();
}

void CPartyPlayerCardDataManager::BuildGamerHandles(atArray<rlGamerHandle>& rGamerHandles, bool buildGamerData)
{
	PlayerListData& writeData = GetWriteDataBuffer();

	rGamerHandles.Reset();
	if(buildGamerData)
	{
		writeData.m_gamerDatas.Reset();
	}

	if(NetworkInterface::GetActiveGamerInfo())
	{
		rGamerHandles.Grow() = NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();

		if(buildGamerData)
		{
			writeData.m_gamerDatas.Grow();
			writeData.m_hasRecords.PushAndGrow(false);

			#if RSG_DURANGO
			if (NetworkInterface::GetActiveGamerInfo()->HasDisplayName())
				writeData.m_gamerDatas.Top().SetName(NetworkInterface::GetActiveGamerInfo()->GetDisplayName());
			else
				NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(NetworkInterface::GetActiveGamerInfo()->GetName()));
			#else
			NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(NetworkInterface::GetActiveGamerInfo()->GetName()));
			#endif
			
		}
	}

	/*if(NetworkInterface::IsNetworkOpen() && CNetwork::GetNetworkSession().IsInParty())
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetPartyMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(unsigned i=0; i<nGamers; ++i)
		{
			if(rGamerHandles.Find(aGamers[i].GetGamerHandle()) == -1)
			{
				rGamerHandles.Grow() = aGamers[i].GetGamerHandle();

				if(buildGamerData)
				{
					writeData.m_gamerDatas.Grow();

					#if RSG_DURANGO
					if (aGamers[i].HasDisplayName())
						writeData.m_gamerDatas.Top().SetName(aGamers[i].GetDisplayName());
					else
						NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(aGamers[i].GetName()));
					#else
					NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(aGamers[i].GetName()));
					#endif
					
				}
			}
		}
	}*/

#if PARTY_PLATFORM
	if(CLiveManager::IsInPlatformParty())
	{
		rlPlatformPartyMember partyMember[MAX_NUM_PARTY_MEMBERS];
		unsigned numMembers = CLiveManager::GetPlatformPartyInfo(partyMember, MAX_NUM_PARTY_MEMBERS);
		for(unsigned i=0; i<numMembers; ++i)
		{
			rlGamerHandle gamerHandle;

			partyMember[i].GetGamerHandle(&gamerHandle);
			if(rGamerHandles.Find(gamerHandle) == -1)
			{
				rGamerHandles.Grow() = gamerHandle;

				if(buildGamerData)
				{
					writeData.m_gamerDatas.Grow();
					writeData.m_hasRecords.PushAndGrow(false);
					NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(partyMember[i].GetDisplayName()));
				}
			}
		}
	}
#endif
}
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CScriptedDirectorPlayerCardDataManager::CScriptedDirectorPlayerCardDataManager():
	CScriptedPlayerCardDataManager()
{
}


void CScriptedDirectorPlayerCardDataManager::OnGamerNamesPopulated()
{
	playercardNetDebugf1("CScriptedDirectorPlayerCardDataManager :: OnGamerNamesPopulated");
	BasePlayerCardDataManager::OnGamerNamesPopulated();
}


bool CScriptedDirectorPlayerCardDataManager::IsValidPlayer(const CNetGamePlayer* pNetPlayer) const
{
	bool retVal = false;
	if(pNetPlayer)
	{
		return pNetPlayer->IsLocal();
	}

	return retVal;
}

void CScriptedDirectorPlayerCardDataManager::ValidateNewGamerList()
{
	if (m_currentHandles.size() > 0)
	{
		if (playercardVerify(m_currentHandles.size() == MAX_NUM_PLAYERS) && playercardVerify(m_currentHandles[0].IsLocal()))
		{
			playercardDebugf1("ValidateNewGamerList() - Player found '%s'", NetworkUtils::LogGamerHandle(m_currentHandles[0]));
		}
		else
		{
			playercardErrorf("ValidateNewGamerList() - Player NOT found '%s'", NetworkUtils::LogGamerHandle(m_currentHandles[0]));
			ClearGamerHandles();
		}
	}

	m_hasValidatedList = true;
}

void CScriptedDirectorPlayerCardDataManager::PopulateGamerHandles()
{
	playercardDebugf1("CScriptedDirectorPlayerCardDataManager :: PopulateGamerHandles");
	PlayerListData& writeData = GetWriteDataBuffer();

	writeData.m_gamerHandles.Reset();
	writeData.m_gamerDatas.Reset();

	bool haveNames = m_currentHandles.size() == m_currentNames.size();

	writeData.m_gamerHandles.Reserve(m_currentHandles.size());
	writeData.m_gamerDatas.Reserve(m_currentHandles.size());


	
	if (playercardVerify(MAX_NUM_PLAYERS == m_currentHandles.size()))
	{
		if(haveNames)
		{
			writeData.m_gamerHandles.Append() = m_currentHandles[0];
			writeData.m_gamerDatas.Append().SetName(m_currentNames[0]);
			writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
			writeData.m_gamerDatas.Top().m_isOnline = true;

			if (m_currentHandles[0].IsLocal())
				writeData.m_hasRecords.PushAndGrow(true);
			else
				writeData.m_hasRecords.PushAndGrow(false);
		}
		else
		{
			const rlGamerInfo* pi = NetworkInterface::GetActiveGamerInfo();
			if (playercardVerify(pi))
			{
				writeData.m_gamerHandles.Append() = m_currentHandles[0];
				writeData.m_gamerDatas.Append();
				NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(pi->GetName()));
				writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
				writeData.m_gamerDatas.Top().m_isOnline = true;

				if (m_currentHandles[0].IsLocal())
					writeData.m_hasRecords.PushAndGrow(true);
				else
					writeData.m_hasRecords.PushAndGrow(false);
			}
		}
	}

	m_currentNames.Reset();

	if(m_hasValidatedList)
	{
		InitUGCQueries();
		if(haveNames)
		{
			OnGamerNamesPopulated();
		}
		else
		{
			OnGamerHandlesPopulated();
		}
	}
	else
	{
		SetState(STATE_WAITING_FOR_GAMERS);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CScriptedPlayerCardDataManager::CScriptedPlayerCardDataManager():
	BasePlayerCardDataManager(),
	m_hasValidatedList(false)
{
	CreateRequestedRecords(PlayerListData::PROFILE_STATS_MP_FRIENDS);
}

const PlayerCardStatInfo* CScriptedPlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPFriendStats();
}

const PlayerCardPackedStatInfo* CScriptedPlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPPackedStats();
}

void CScriptedPlayerCardDataManager::ValidateNewGamerList()
{
	playercardDebugf1("CScriptedPlayerCardDataManager :: ValidateNewGamerList");
	bool haveNames = m_currentHandles.size() == m_currentNames.size();

	// If we don't have the names, we'll need to make sure we can get the names.
	if(!haveNames)
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{
			bool found = false;
			const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(m_currentHandles[i]);

			if(IsValidPlayer(pPlayer) && !pPlayer->IsSpectator())
			{
				found = true;
			}
			else if(!pPlayer)
			{
				rlGamerHandle& rCurrHandle = m_currentHandles[i];
				for(unsigned j=0; j<nGamers; ++j)
				{
					if(rCurrHandle == aGamers[j].GetGamerHandle())
					{
						found = true;
						break;
					}
				}
			}

			if(!found)
			{
				playercardErrorf("ValidateNewGamerList() - Player NOT found '%s'", NetworkUtils::LogGamerHandle(m_currentHandles[i]));
				m_currentHandles.Delete(i);
				--i;
			}
			else
			{
				playercardDebugf1("ValidateNewGamerList() - Player found '%s'", NetworkUtils::LogGamerHandle(m_currentHandles[i]));
			}
		}
	}

	m_hasValidatedList = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
#if RSG_DURANGO
void CScriptedPlayerCardDataManager::PopulateGamerHandles()
{
PlayerListData& writeData = GetWriteDataBuffer();

	writeData.m_gamerHandles.Reset();
	writeData.m_gamerDatas.Reset();

	bool haveNames = m_currentHandles.size() == m_currentNames.size();

	writeData.m_gamerHandles.Reserve(m_currentHandles.size());
	writeData.m_gamerDatas.Reserve(m_currentHandles.size());

	if(haveNames)
	{
		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{
			writeData.m_gamerHandles.Append() = m_currentHandles[i];
			writeData.m_gamerDatas.Append().SetName(m_currentNames[i]);
			writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
			writeData.m_gamerDatas.Top().m_isOnline = true;

			if (m_currentHandles[i].IsLocal())
				writeData.m_hasRecords.PushAndGrow(true);
			else
				writeData.m_hasRecords.PushAndGrow(false);
		}
	}
	else
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{
			rlGamerHandle& rCurrHandle = m_currentHandles[i];

			const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(rCurrHandle);
			if(pPlayer)
			{
				if(IsValidPlayer(pPlayer) && !pPlayer->IsSpectator())
				{
					writeData.m_gamerHandles.Append() = rCurrHandle;
					writeData.m_gamerDatas.Append();
					
					if (pPlayer->GetGamerInfo().HasDisplayName())
						writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetDisplayName());
					else
						writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetName());
					writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
					writeData.m_gamerDatas.Top().m_isOnline = true;

					if (m_currentHandles[i].IsLocal())
						writeData.m_hasRecords.PushAndGrow(true);
					else
						writeData.m_hasRecords.PushAndGrow(false);
				}
			}
			else
			{
				for(unsigned j=0; j<nGamers; ++j)
				{
					if(rCurrHandle == aGamers[j].GetGamerHandle())
					{
						writeData.m_gamerHandles.Append() = rCurrHandle;
						writeData.m_gamerDatas.Append();

						if (aGamers[j].HasDisplayName())
							writeData.m_gamerDatas.Top().SetName(aGamers[j].GetDisplayName());
						else
							writeData.m_gamerDatas.Top().SetName(aGamers[j].GetName());

						writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
						writeData.m_gamerDatas.Top().m_isOnline = true;

						if (m_currentHandles[i].IsLocal())
							writeData.m_hasRecords.PushAndGrow(true);
						else
							writeData.m_hasRecords.PushAndGrow(false);
						break;
					}
				}
			}
		}
	}

	m_currentNames.Reset();

	if(m_hasValidatedList)
	{
		InitUGCQueries();
		if(haveNames)
		{
			OnGamerNamesPopulated();
		}
		else
		{
			OnGamerHandlesPopulated();
		}
	}
	else
	{
		SetState(STATE_WAITING_FOR_GAMERS);
	}
}
#else

void CScriptedPlayerCardDataManager::PopulateGamerHandles()
{
	playercardDebugf1("CScriptedPlayerCardDataManager :: PopulateGamerHandles");
	PlayerListData& writeData = GetWriteDataBuffer();

	writeData.m_gamerHandles.Reset();
	writeData.m_gamerDatas.Reset();

	bool haveNames = m_currentHandles.size() == m_currentNames.size();

	writeData.m_gamerHandles.Reserve(m_currentHandles.size());
	writeData.m_gamerDatas.Reserve(m_currentHandles.size());

	if(haveNames)
	{
		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{

			writeData.m_gamerHandles.Append() = m_currentHandles[i];
			writeData.m_gamerDatas.Append().SetName(m_currentNames[i]);
			writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
			writeData.m_gamerDatas.Top().m_isOnline = true;

			if (m_currentHandles[i].IsLocal())
				writeData.m_hasRecords.PushAndGrow(true);
			else
				writeData.m_hasRecords.PushAndGrow(false);
		}
	}
	else
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{
			rlGamerHandle& rCurrHandle = m_currentHandles[i];

			const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(rCurrHandle);
			if(pPlayer)
			{
				if(IsValidPlayer(pPlayer) && !pPlayer->IsSpectator())
				{
					writeData.m_gamerHandles.Append() = rCurrHandle;
					writeData.m_gamerDatas.Append();
					NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetName()));
					writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
					writeData.m_gamerDatas.Top().m_isOnline = true;
					
					if (rCurrHandle.IsLocal())
						writeData.m_hasRecords.PushAndGrow(true);
					else if (CNetwork::GetNetworkSession().IsSessionMember(rCurrHandle))
						writeData.m_hasRecords.PushAndGrow(true);
					else
						writeData.m_hasRecords.PushAndGrow(false);
				}
			}
			else
			{
				for(unsigned j=0; j<nGamers; ++j)
				{
					if(rCurrHandle == aGamers[j].GetGamerHandle()  )
					{
						writeData.m_gamerHandles.Append() = rCurrHandle;
						writeData.m_gamerDatas.Append();
						NOT_USING_DISPLAY_NAMES_ONLY(writeData.m_gamerDatas.Top().SetName(aGamers[j].GetName()));
						writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
						writeData.m_gamerDatas.Top().m_isOnline = true;

						if (rCurrHandle.IsLocal())
							writeData.m_hasRecords.PushAndGrow(true);
						else if (CNetwork::GetNetworkSession().IsSessionMember(rCurrHandle))
							writeData.m_hasRecords.PushAndGrow(true);
						else
							writeData.m_hasRecords.PushAndGrow(false);
						break;
					}
				}
			}
		}
	}

	m_currentNames.Reset();

	if(m_hasValidatedList)
	{
		InitUGCQueries();
		if(haveNames)
		{
			OnGamerNamesPopulated();
		}
		else
		{
			OnGamerHandlesPopulated();
		}
	}
	else
	{
		SetState(STATE_WAITING_FOR_GAMERS);
	}
}
#endif

void CScriptedPlayerCardDataManager::OnGamerNamesPopulated()
{
	playercardDebugf1("CScriptedPlayerCardDataManager :: OnGamerNamesPopulated");
	PlayerListData& writeData = GetWriteDataBuffer();
	if (writeData.m_gamerHandles.size() == 1 && writeData.m_gamerHandles[0] == NetworkInterface::GetLocalGamerHandle())
	{
		SetState(STATE_STATS_POPULATED);
	}
	else
	{
		BasePlayerCardDataManager::OnGamerNamesPopulated();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CScriptedCoronaPlayerCardDataManager::CScriptedCoronaPlayerCardDataManager() 
: BasePlayerCardDataManager()
{
	CreateRequestedRecords(PlayerListData::PROFILE_STATS_MP_FRIENDS);
}

void CScriptedCoronaPlayerCardDataManager::Init(const CPlayerCardXMLDataManager& rStatManager)
{
	PlayerListData& writeData = GetWriteDataBuffer();
	if (CanReset() || writeData.GetGamerCount() == 0)
	{
		BasePlayerCardDataManager::Init(rStatManager);
	}
}

const PlayerCardStatInfo* CScriptedCoronaPlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPFriendStats();
}

const PlayerCardPackedStatInfo* CScriptedCoronaPlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	return &rStatManager.GetMPPackedStats();
}

void CScriptedCoronaPlayerCardDataManager::RemoveGamerHandle(const rlGamerHandle& handle)
{
	playercardDebugf1("CScriptedCoronaPlayerCardDataManager :: RemoveGamerHandle %s", handle.ToString());
	if (Exists(handle))
	{
		int index = 0;
		
		PlayerListData& writeData = GetWriteDataBuffer();
		if (playercardVerify(writeData.Exists(handle, index)) && playercardVerify(index < m_currentNames.GetCount()))
		{
			gnetDebug1("[corona_sync] RemoveGamerHandle - '%s'.", NetworkUtils::LogGamerHandle(handle));

			m_currentNames.Delete(index);
			writeData.RemoveGamer(handle);
			m_receivedRecords.Delete(index);

			//Send our player data to this guy
			if (!handle.IsLocal())
			{
				if (SCPlayerCardCoronaManager::GetInstance().Exists(handle))
				{
					SCPlayerCardCoronaManager::GetInstance().Remove(handle);
				}
			}

#if RSG_DURANGO
			// clear previous request
			if (m_gamerNamesReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
			{
				uiErrorf("Gamer handles populated with previous display name request (id: %d)", m_gamerNamesReqID);
				CLiveManager::GetFindDisplayName().CancelRequest(m_gamerNamesReqID);
				m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			}
			const atArray<rlGamerHandle>& gamerHandles = GetWriteDataBuffer().m_gamerHandles;
			if(gamerHandles.GetCount())
			{
				m_gamerNamesReqID = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), gamerHandles.GetElements(), gamerHandles.GetCount(), SCRIPTED_CORONA_ABANDON_TIME);		
			}
#endif
			//Refresh players menu...
			FlagForRefresh();
		}
	}
}

void CScriptedCoronaPlayerCardDataManager::SetGamerHandleName(const rlGamerHandle& handle, const char* pName) 
{
	if (playercardVerify(Exists(handle)))
	{
		int index = 0;

		PlayerListData& writeData = GetWriteDataBuffer();
		if (playercardVerify(writeData.Exists(handle, index)))
		{
			m_currentNames[index] = pName;
		}
	}
}

void CScriptedCoronaPlayerCardDataManager::AddGamerHandle(const rlGamerHandle& handle, const char* pName) 
{
	playercardDebugf1("CScriptedCoronaPlayerCardDataManager :: AddGamerHandle %s - %s", handle.ToString(), pName);
	if (!Exists(handle))
	{
		const bool isvalid = NetworkInterface::GetPlayerFromGamerHandle(handle) 
								|| CNetwork::GetNetworkSession().IsTransitionMember(handle);

		playercardAssertf(isvalid, "Gamer '%s' does not exist in session.", NetworkUtils::LogGamerHandle(handle));
		if (isvalid)
		{

			// Hide Rockstar employees on final to prevent leaking gamer tags
#if __FINAL
			CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(handle);
			if(pPlayer && (pPlayer->IsRockstarDev() ||
				pPlayer->IsRockstarQA() || 
				pPlayer->GetMatchmakingGroup() == MM_GROUP_SCTV))
			{
				return;
			}
#else
			const bool bHideRockstarEmployees = PARAM_hideRockstarDevs.Get();
			if(bHideRockstarEmployees)
			{
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(handle);
				if(pPlayer && (pPlayer->IsRockstarDev() || 
					pPlayer->IsRockstarQA() || 
					pPlayer->GetMatchmakingGroup() == MM_GROUP_SCTV))
				{
					return;
				}
			}
#endif // __FINAL
			
			if (WasInitialized())
			{
				BasePlayerCardDataManager::ValidityRules rules;
				SPlayerCardManager::GetInstance().Init(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS, rules);
			}

			PlayerListData& writeData = GetWriteDataBuffer();
			if (playercardVerify(writeData.m_gamerHandles.GetCount() <= RL_MAX_GAMERS_PER_SESSION))
			{
				gnetDebug1("[corona_sync] AddGamerHandle - '%s'.", NetworkUtils::LogGamerHandle(handle));

				m_currentNames.PushAndGrow(atString(pName));
				m_receivedRecords.PushAndGrow(0);
				writeData.AddGamer(handle, pName);

				//Send our player data to this guy
				if (!handle.IsLocal())
				{
					if (!SCPlayerCardCoronaManager::GetInstance().Exists(handle))
					{
						SCPlayerCardCoronaManager::GetInstance().Add(handle);
					}
					else
					{
						gnetDebug1("[corona_sync] AddGamerHandle - '%s' already exists.", NetworkUtils::LogGamerHandle(handle));
					}
				}

				//Call PopulateGamerHandles - sets.
				PopulateGamerHandles();

				// Move the displayName request into AddGamerHandle because previous location didn't give a reliable 
				// method to send DisplayName requests every time we added a player (CNetworkSession also calls this method).
				#if RSG_DURANGO
				// clear previous request
				if (m_gamerNamesReqID != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
				{
					uiErrorf("Gamer handles populated with previous display name request (id: %d)", m_gamerNamesReqID);
					CLiveManager::GetFindDisplayName().CancelRequest(m_gamerNamesReqID);
					m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
				}
				const atArray<rlGamerHandle>& gamerHandles = GetWriteDataBuffer().m_gamerHandles;
				if(gamerHandles.GetCount())
				{
					m_gamerNamesReqID = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), gamerHandles.GetElements(), gamerHandles.GetCount(), SCRIPTED_CORONA_ABANDON_TIME);		
				}
				#endif

				//Refresh players menu...
				FlagForRefresh();
			}
		}
	}
}

bool CScriptedCoronaPlayerCardDataManager::Exists(const rlGamerHandle& handle) const
{
	const PlayerListData& writeData = GetWriteDataBuffer();

	for(u32 i=0; i<writeData.m_gamerHandles.GetCount(); ++i)
	{
		if (handle == writeData.m_gamerHandles[i])
		{
			return true;
		}
	}

	return false;
}

void CScriptedCoronaPlayerCardDataManager::Update()
{
	BasePlayerCardDataManager::Update();

	if (WasInitialized())
	{
		if (!CNetwork::GetNetworkSession().IsTransitionActive())
		{
			PlayerListData& writeData = GetWriteDataBuffer();
			if (writeData.GetGamerCount() > 0)
			{
				Reset();
			}
		}

#if RSG_DURANGO
		// We have to do a bit of duplicate code here because we cannot rely on a consistent state since the callback
		// on RecievedPlayerCardData could switch it from under us before we've finished the request.
		if (CDisplayNamesFromHandles::INVALID_REQUEST_ID != m_gamerNamesReqID)
		{
			int gamerCount = GetWriteDataBuffer().m_gamerHandles.GetCount();

			//if (gamerCount == 0)
			//{
			//	SetState(STATE_SUCCESS);
			//	return;
			//}

            if(CLiveManager::GetFindDisplayName().GetRequestIndexByRequestId(m_gamerNamesReqID) != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
            {
                uiDebugf1("Display name request not found, completing with fallback (id: %d)", m_gamerNamesReqID);
                
                // request has likely been abandoned
                m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

                // fall back to gamer tags instead of denying friends list functionality
                OnGamerNamesPopulated();
            }
            else
            {
                rlDisplayName displayNames[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST];
                int nameRetrievalStatus = CLiveManager::GetFindDisplayName().GetDisplayNames(m_gamerNamesReqID, displayNames, gamerCount);
                if(nameRetrievalStatus == CDisplayNamesFromHandles::DISPLAY_NAMES_SUCCEEDED)
                {
                    // clear request id
                    m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

                    atArray<GamerData>& gamerDatas = GetWriteDataBuffer().m_gamerDatas;
                    for(int i = 0; i < gamerDatas.GetCount() && i < CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST; ++i)
                    {
                        gamerDatas[i].SetName(displayNames[i]);
                    }
                    OnGamerNamesPopulated();
                }
                else if(nameRetrievalStatus == CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED)
                {
                    // clear request id
                    m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

                    // fall back to gamer tags instead of denying friends list functionality
                    OnGamerNamesPopulated();
                }
            }
		}
#endif // RSG_DURANGO
	}
}

void CScriptedCoronaPlayerCardDataManager::PopulateGamerHandles()
{
	if (!m_pStats)
		return;

	playercardDebugf1("CScriptedCoronaPlayerCardDataManager :: PopulateGamerHandles");
	if (CNetwork::GetNetworkSession().IsTransitionActive())
	{
		PlayerListData& writeData = GetWriteDataBuffer();
		if (CNetwork::GetNetworkSession().GetTransitionMemberCount() > writeData.GetGamerCount())
		{
			rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
			unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

			for (int i=0; i<nGamers; i++)
			{

				if (!Exists(aGamers[i].GetGamerHandle()))
				{
					if (  CNetwork::GetNetworkSession().IsTransitionHost())
						AddGamerHandle(aGamers[i].GetGamerHandle(), aGamers[i].GetName());
					else if ( !CNetwork::GetNetworkSession().IsHostPlayerAndRockstarDev(aGamers[i].GetGamerHandle()))
						AddGamerHandle(aGamers[i].GetGamerHandle(), aGamers[i].GetName());
				}
			}

			OnGamerHandlesPopulated();
		}
	}
}

void CScriptedCoronaPlayerCardDataManager::ReceivePlayerCardData(const rlGamerInfo& gamerInfo, u8* data, const u32 sizeOfData)
{
	playercardDebugf1("CScriptedCoronaPlayerListDataManager :: ReceivePlayerCardData %s", gamerInfo.GetGamerHandle().ToString());
	//This can happen for players joining.
	const CPlayerCardManager::CardTypes cachedType = SPlayerCardManager::GetInstance().GetCurrentType();
	BasePlayerCardDataManager::ValidityRules rules;
	SPlayerCardManager::GetInstance().Init(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS, rules);

	if (playercardVerify(data) && playercardVerify(sizeOfData > 0) && playercardVerify(m_pStats))
	{
		const rlGamerHandle& gamerHandle = gamerInfo.GetGamerHandle();

		//Add the gamer handle if it doesnt exist yet.
		if (!Exists(gamerHandle))
		{
			playercardDebugf1("[corona_sync] Gamer %s not found in gamer handles.", NetworkUtils::LogGamerHandle(gamerInfo.GetGamerHandle()));
			return;
		}

		PlayerListData& writeData = GetWriteDataBuffer();
		const u32 numberOfGamers = writeData.m_gamerHandles.GetCount();

		bool found = false;
		u32 row = 0;

		for (; row<numberOfGamers && !found; row++)
		{
			if (writeData.m_gamerHandles[row] == gamerHandle)
			{
				found = true;
				break;
			}
		}

		playercardAssertf(found, "[corona_sync] Gamer %s not found in gamer handles.", NetworkUtils::LogGamerHandle(gamerInfo.GetGamerHandle()));

		//Send our player data to this guy
		if (found && playercardVerify(writeData.m_pRequestedRecords) &&
			playercardVerifyf(row < writeData.m_pRequestedRecords->GetCapacity(), "Out of bounds %u %u", row, writeData.m_pRequestedRecords->GetCapacity()))
		{
			if (!m_statsQueryStatus.Pending())
			{
				m_statsQueryStatus.Reset();
				m_statsQueryStatus.SetPending();
			}

			writeData.InitializeStatsResults(m_pStats, numberOfGamers);

			writeData.SetStatsResultsNumRows(numberOfGamers);

			u32 numStatsReceived = 0;

			writeData.SetStatsResultsGamerHandle(row, gamerHandle);

			if(playercardVerify(writeData.StatsResultsGetBufferResults(data, sizeOfData, row, numStatsReceived)))
			{
				playercardDebugf1("[corona_sync] ReceivedRecords() '%s' - CountOfStats='%d', this message numStats='%d'.", NetworkUtils::LogGamerHandle(gamerHandle), m_pStats->size(), numStatsReceived);

				writeData.ReceivedRecords(gamerHandle);

				int index = 0;
				if (AssertVerify(GetReadDataBuffer().Exists(gamerHandle, index)))
				{
					m_receivedRecords[index] += numStatsReceived;
					if(m_receivedRecords[index]>=m_pStats->size())
						playercardDebugf1("[corona_sync] ReceivedRecords() '%s' - TotalStatsReceived='%d' >= m_pStats->size()='%d' - The Card is ready!.", NetworkUtils::LogGamerHandle(gamerHandle), m_receivedRecords[index], m_pStats->size());
				}

				if (m_statsQueryStatus.Pending())
					m_statsQueryStatus.SetSucceeded();
				else
					m_statsQueryStatus.ForceSucceeded();

				SetState(STATE_POPULATING_STATS);

				Update();
			}
			else
			{
				if (m_statsQueryStatus.Pending())
					m_statsQueryStatus.SetFailed();
				else
					m_statsQueryStatus.ForceFailed();
			}
		}
	}

	SPlayerCardManager::GetInstance().SetCurrentType(cachedType);
}

void CScriptedCoronaPlayerCardDataManager::RefreshGamerData()
{
	playercardDebugf1("CScriptedCoronaPlayerListDataManager :: RefreshGamerData");
	// Refresh Read Data
	PlayerListData& readData = const_cast<PlayerListData&>(GetReadDataBuffer());
	for(int i=0; i<readData.m_gamerHandles.GetCount(); ++i)
	{
		if (!readData.m_hasRecords[i])
		{
			CommonRefreshGamerData(readData.m_gamerDatas[i], readData.m_gamerHandles[i]);
		}
	}
}

#if RSG_DURANGO
void CScriptedCoronaPlayerCardDataManager::OnGamerNamesPopulated()
{
	OnGamerHandlesPopulated();
}
#endif

void CScriptedCoronaPlayerCardDataManager::OnGamerHandlesPopulated()
{
	if (!m_pStats)
		return;

	playercardNetDebugf1("CScriptedCoronaPlayerCardDataManager :: OnGamerHandlesPopulated");
	PlayerListData& writeData = GetWriteDataBuffer();
	if (writeData.m_gamerHandles.size() == 1 && writeData.m_gamerHandles[0] == NetworkInterface::GetLocalGamerHandle())
	{
		SetState(STATE_STATS_POPULATED);
	}
	else if (writeData.NeedsToUpdateRecords())
	{
		RefreshGamerData();

		if (m_pStats)
		{
			playercardAssertf(m_pStats->size() <= writeData.GetRequestedRecordsMaxStats(), "CScriptedCoronaPlayerCardDataManager::Update - Too many stats are listed.  Want %d, but max is %u", m_pStats->size(), writeData.GetRequestedRecordsMaxStats());
			playercardDebugf1("[corona_sync] CScriptedCoronaPlayerCardDataManager::OnGamerHandlesPopulated player stats size is: %d", m_pStats->size());

			if(playercardVerify(!m_pStats->empty()))
			{
				if (!m_statsQueryStatus.Pending())
				{
					m_statsQueryStatus.Reset();
					m_statsQueryStatus.SetPending();
				}

				SetState( STATE_POPULATING_STATS );
			}
			else
			{
				SetState(STATE_STATS_POPULATED);
			}
		}
	}
	else
	{
		SetState(STATE_STATS_POPULATED);
	}
}

void CScriptedCoronaPlayerCardDataManager::Reset()
{
	playercardDebugf1("[corona_sync] Resetting CScriptedCoronaPlayerCardDataManager");
	
	#if __BANK
	sysStack::PrintStackTrace();
	#endif

	m_currentNames.clear();
	m_receivedRecords.Reset();

	if (m_statsQueryStatus.Pending())
		m_statsQueryStatus.SetCanceled();
	m_statsQueryStatus.Reset();

	GetWriteDataBuffer().Reset();

	m_timeoutTimer.Zero();

	SetState(STATE_INVALID);

	#if RSG_DURANGO
	if (CDisplayNamesFromHandles::INVALID_REQUEST_ID != m_gamerNamesReqID)
	{
		if (CLiveManager::GetFindDisplayName().GetRequestIndexByRequestId(m_gamerNamesReqID) != CDisplayNamesFromHandles::INVALID_REQUEST_ID )
		{
			CLiveManager::GetFindDisplayName().CancelRequest(m_gamerNamesReqID);
		}

		m_gamerNamesReqID = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	
	}
	#endif

	m_pStats = NULL;

	if(AssertVerify(SCPlayerCardCoronaManager::IsInstantiated()))
		SCPlayerCardCoronaManager::GetInstance().Reset();
}

bool CScriptedCoronaPlayerCardDataManager::ArePlayerStatsAccessible(const rlGamerHandle& gamerHandle) const
{
	bool result = BasePlayerCardDataManager::ArePlayerStatsAccessible(gamerHandle);
	if(!result)
	{
		gnetDebug1("[corona_sync] ArePlayerStatsAccessible failed !result - '%s'.", NetworkUtils::LogGamerHandle(gamerHandle));
		return false;
	}

	if(gamerHandle==NetworkInterface::GetLocalGamerHandle())
	{
		return true;
	}

	int index = 0;
	if (AssertVerify(GetReadDataBuffer().Exists(gamerHandle, index)))
	{
		if (AssertVerify(m_pStats))
		{
			bool bResult = result && m_receivedRecords[index]>=m_pStats->size();

			if (!bResult)
			{
				gnetDebug1("[corona_sync] ArePlayerStatsAccessible failed m_receivedRecords[index]>=m_pStats->size() - '%s'.", NetworkUtils::LogGamerHandle(gamerHandle));
			}

			return bResult;
		}
		else
		{
			gnetDebug1("[corona_sync] ArePlayerStatsAccessible failed !m_pStats - '%s'.", NetworkUtils::LogGamerHandle(gamerHandle));
		}
	}

	gnetDebug1("[corona_sync] ArePlayerStatsAccessible failed !GetReadDataBuffer().Exists(gamerHandle, index) - '%s'.", NetworkUtils::LogGamerHandle(gamerHandle));
	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
#if RSG_DURANGO
void CScriptedCoronaPlayerListDataManager::PopulateGamerHandles()
{
PlayerListData& writeData = GetWriteDataBuffer();

	writeData.m_gamerHandles.Reset();
	writeData.m_gamerDatas.Reset();

	bool haveNames = m_currentHandles.size() == m_currentNames.size();

	writeData.m_gamerHandles.Reserve(m_currentHandles.size());
	writeData.m_gamerDatas.Reserve(m_currentHandles.size());

	if(haveNames)
	{
		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{
			writeData.m_gamerHandles.Append() = m_currentHandles[i];
			writeData.m_gamerDatas.Append().SetName(m_currentNames[i]);
			writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
			writeData.m_gamerDatas.Top().m_isOnline = true;

			if (m_currentHandles[i].IsLocal())
				writeData.m_hasRecords.PushAndGrow(true);
			else
				writeData.m_hasRecords.PushAndGrow(false);
		}
	}
	else
	{
		rlGamerInfo aGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned nGamers = CNetwork::GetNetworkSession().GetTransitionMembers(aGamers, RL_MAX_GAMERS_PER_SESSION);

		for(u32 i=0; i<m_currentHandles.size(); ++i)
		{
			rlGamerHandle& rCurrHandle = m_currentHandles[i];

			const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(rCurrHandle);
			if(pPlayer)
			{
				if(IsValidPlayer(pPlayer) && !pPlayer->IsSpectator())
				{
					writeData.m_gamerHandles.Append() = rCurrHandle;
					writeData.m_gamerDatas.Append();
					
					if (pPlayer->GetGamerInfo().HasDisplayName())
						writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetDisplayName());
					else
						writeData.m_gamerDatas.Top().SetName(pPlayer->GetGamerInfo().GetName());
					writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
					writeData.m_gamerDatas.Top().m_isOnline = true;

					if (rCurrHandle.IsLocal())
						writeData.m_hasRecords.PushAndGrow(true);
					else
						writeData.m_hasRecords.PushAndGrow(false);
				}
			}
			else
			{
				for(unsigned j=0; j<nGamers; ++j)
				{
					if(rCurrHandle == aGamers[j].GetGamerHandle())
					{
						writeData.m_gamerHandles.Append() = rCurrHandle;
						writeData.m_gamerDatas.Append();

						if (aGamers[j].HasDisplayName())
							writeData.m_gamerDatas.Top().SetName(aGamers[j].GetDisplayName());
						else
							writeData.m_gamerDatas.Top().SetName(aGamers[j].GetName());

						writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
						writeData.m_gamerDatas.Top().m_isOnline = true;

						if (rCurrHandle.IsLocal())
							writeData.m_hasRecords.PushAndGrow(true);
						else
							writeData.m_hasRecords.PushAndGrow(false);

						break;
					}
				}
			}
		}
	}

	m_currentNames.Reset();

	if(m_hasValidatedList)
	{
		InitUGCQueries();
		if(haveNames)
		{
			OnGamerNamesPopulated();
		}
		else
		{
			OnGamerHandlesPopulated();
		}
	}
	else
	{
		SetState(STATE_WAITING_FOR_GAMERS);
	}
}
#endif

CScriptedCoronaPlayerListDataManager::CScriptedCoronaPlayerListDataManager() : CScriptedPlayerCardDataManager()
{
	// Embracing the madness to make this work.  Should be false by default turned on by add/removegamerhandle.
	// doing so opens a whole world full of code built upon broken code which breaks everything.  Sorry.	
	FlagForRefresh();					
}

void CScriptedCoronaPlayerListDataManager::OnGamerHandlesPopulated()
{
	playercardNetDebugf1("CScriptedCoronaPlayerListDataManager :: OnGamerHandlesPopulated");
	RefreshGamerData();
	OnGamerNamesPopulated();
}

void CScriptedCoronaPlayerListDataManager::RemoveGamerHandle(const rlGamerHandle& handle)
{
	playercardDebugf1("CScriptedCoronaPlayerListDataManager :: RemoveGamerHandle %s", handle.ToString());
	if (Exists(handle))
	{
		int index = 0;

		PlayerListData& writeData = GetWriteDataBuffer();
		if (playercardVerify(writeData.Exists(handle, index)))
		{
			gnetDebug1("[corona_sync] RemoveGamerHandle - '%s'.", NetworkUtils::LogGamerHandle(handle));

			writeData.RemoveGamer(handle);

			//Send our player data to this guy
			if (!handle.IsLocal())
			{
				if (SCPlayerCardCoronaManager::GetInstance().Exists(handle))
				{
					SCPlayerCardCoronaManager::GetInstance().Remove(handle);
				}
			}

			//Refresh players menu...
			FlagForRefresh();
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
CClanPlayerCardDataManager::CClanPlayerCardDataManager()
	: BasePlayerCardDataManager()
	, m_ClanId(RL_INVALID_CLAN_ID)
	, m_bIsLocalPlayersPrimaryClan(false)
	, m_bShouldGetOfflineMembers(true)
	, m_pageIndex(0)
	, m_queuedPageIndex(0)
{
	// We need to use the largest of MP_FRIENDS, MP_CREWS and SP_CREWS
	// Currently, MP_FRIENDS is the largest. It's likely to remain that way.
#if (MAX_PROFILE_STATS_FOR_MP_FRIENDS >= MAX_PROFILE_STATS_FOR_MP_CREWS) && (MAX_PROFILE_STATS_FOR_MP_FRIENDS >= MAX_PROFILE_STATS_FOR_SP_CREWS)
	PlayerListData::eTypeOfProfileStats typeOfProfileStats = PlayerListData::PROFILE_STATS_MP_FRIENDS;
#elif (MAX_PROFILE_STATS_FOR_MP_CREWS >= MAX_PROFILE_STATS_FOR_SP_CREWS)
	PlayerListData::eTypeOfProfileStats typeOfProfileStats = PlayerListData::PROFILE_STATS_MP_CREWS;
#else
	PlayerListData::eTypeOfProfileStats typeOfProfileStats = PlayerListData::PROFILE_STATS_SP_CREWS;
#endif

	CreateRequestedRecords(typeOfProfileStats);

	Reset();
}

CClanPlayerCardDataManager::~CClanPlayerCardDataManager()
{
	Reset();
}

void CClanPlayerCardDataManager::LoadPage(int page)
{
	m_queuedPageIndex = page;
}

void CClanPlayerCardDataManager::PopulateGamerHandles()
{
	playercardDebugf1("CClanPlayerCardDataManager :: PopulateGamerHandles");
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (!rlClan::IsServiceReady(localGamerIndex) || !CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub() )
	{
		SetState(ERROR_NO_SOCIALCLUB);
		return;
	}

	// invalid, then just bail to error
	if( m_ClanId == RL_INVALID_CLAN_ID )
	{
		playercardErrorf("%s::PopulateGamerHandles - m_ClanId is invalid", ClassIdentifier());
		SetState(ERROR_FAILED_INVALID_DATA);
		return;
	}

	m_onlineClanMembers.Reset();
	m_onlineClanMembers.ResizeGrow(MAX_CLAN_SIZE);
	m_clanMembers.Reset();
	m_clanMembers.ResizeGrow(MAX_CLAN_MEMBER_SIZE);

	PlayerListData& writeData = GetWriteDataBuffer();
	writeData.m_gamerHandles.Reset();
	writeData.m_gamerDatas.Reset();

	m_returnedClanCount = 0;
	m_totalClanCount = 0; 

	char szParams[256] = {0};
	snprintf(szParams, sizeof(szParams), "@crewid,%" I64FMT "d", m_ClanId);

	if(rlPresenceQuery::FindGamers(*NetworkInterface::GetActiveGamerInfo(), 
		m_listRules.m_hideLocalPlayer,
		"CrewmatesOnline", 
		szParams, 
		m_onlineClanMembers.GetElements(), 
		m_onlineClanMembers.size(), 
		&m_onlineMemberCount, 
		&m_onlineQueryStatus))
	{
		if(!m_bShouldGetOfflineMembers || rlClan::GetMembersByClanId(localGamerIndex,
			0,
			m_ClanId,
			m_clanMembers.GetElements(), m_clanMembers.size(),
			&m_returnedClanCount,
			&m_totalClanCount,
			&m_clanQueryStatus))
		{
			SetState(STATE_POPULATING_GAMERS);
		}
		else
		{
			playercardErrorf("%s::PopulateGamerHandles - rlClan::GetMembersByClanId Failed", ClassIdentifier());
		}
	}
	else
	{
		playercardErrorf("%s::PopulateGamerHandles - PresenceQuery::FindGamers Failed", ClassIdentifier());
	}

	if(m_state != STATE_POPULATING_GAMERS)
	{
		if(m_clanQueryStatus.Pending())
		{
			rlClan::Cancel(&m_clanQueryStatus);
			m_clanQueryStatus.Reset();
		}

		if(m_onlineQueryStatus.Pending())
		{
			rlGetTaskManager()->CancelTask(&m_onlineQueryStatus);
			m_onlineQueryStatus.Reset();
		}

		m_state	= ERROR_FAILED_POPULATING_GAMERS;
	}
}

void CClanPlayerCardDataManager::Update()
{
	if(m_state == STATE_POPULATING_GAMERS)
	{
		if(!m_onlineQueryStatus.Pending() && (!m_bShouldGetOfflineMembers || !m_clanQueryStatus.Pending()))
		{
			if(m_onlineQueryStatus.Succeeded() && (!m_bShouldGetOfflineMembers || m_clanQueryStatus.Succeeded()))
			{
				playercardDebugf1("CClanPlayerCardDataManager::Update - m_onlineMemberCount=%d, m_returnedClanCount=%d", m_onlineMemberCount, m_returnedClanCount);

				m_clanQueryStatus.Reset();

				PlayerListData& writeData = GetWriteDataBuffer();
				int clanPlayerCount = 0;

				writeData.m_gamerHandles.Reset();
				writeData.m_gamerDatas.Reset();

				if (m_returnedClanCount+m_onlineMemberCount > MAX_CLAN_SIZE)
					clanPlayerCount = MAX_CLAN_SIZE;
				else
					clanPlayerCount = m_returnedClanCount+m_onlineMemberCount;

				writeData.m_gamerHandles.Reserve(clanPlayerCount);
				writeData.m_gamerDatas.Reserve(clanPlayerCount);

				m_pageIndex = m_queuedPageIndex;

				const rlGamerHandle& rLocalGamerHandle = NetworkInterface::GetActiveGamerInfo()->GetGamerHandle();
								
				for( int i = 0; i<m_onlineMemberCount; ++i)
				{
					const rlGamerHandle& rGamerHandle = m_onlineClanMembers[i].m_GamerHandle;
					const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(rGamerHandle);

					if((pPlayer==NULL || IsValidPlayer(pPlayer)) && (rLocalGamerHandle != rGamerHandle || !m_listRules.m_hideLocalPlayer))
					{
						writeData.m_gamerHandles.Append() = rGamerHandle;
						writeData.m_gamerDatas.Append();
						writeData.m_gamerDatas.Top().SetName(m_onlineClanMembers[i].GetName());
						writeData.m_gamerDatas.Top().m_isDefaultOnline = true;
						writeData.m_gamerDatas.Top().m_isOnline = true;
						if (rGamerHandle.IsLocal())
							writeData.m_hasRecords.PushAndGrow(true);
						else
							writeData.m_hasRecords.PushAndGrow(false);
					}
				}


				for (int i = 0; i < m_returnedClanCount; i++)
				{

					// Fill up the remaining slots with crew members not online.   Early out if full 
					// gamer handles list.
					if (writeData.m_gamerHandles.GetCapacity() == writeData.m_gamerHandles.GetCount())
						break;

					if(writeData.m_gamerHandles.Find(m_clanMembers[i].m_MemberInfo.m_GamerHandle) == -1)
					{
						const rlGamerHandle& rGamerHandle = m_clanMembers[i].m_MemberInfo.m_GamerHandle;
						const CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(rGamerHandle);

						if((pPlayer==NULL || IsValidPlayer(pPlayer)) &&
							m_clanMembers[i].m_MemberInfo.m_GamerHandle.IsValid())
						{
							writeData.m_gamerHandles.Append() = rGamerHandle;
							writeData.m_gamerDatas.Append();
							writeData.m_gamerDatas.Top().SetName(m_clanMembers[i].m_MemberInfo.m_Gamertag);
							if (rGamerHandle.IsLocal())
								writeData.m_hasRecords.PushAndGrow(true);
							else
								writeData.m_hasRecords.PushAndGrow(false);
						}
					}
				}

				m_totalClanCount = writeData.m_gamerHandles.size();

				m_onlineClanMembers.Reset();
				m_clanMembers.Reset();

				OnGamerHandlesPopulated();
			}
			else if(m_onlineQueryStatus.Failed() || (m_bShouldGetOfflineMembers && m_clanQueryStatus.Failed()))
			{
				SetState(ERROR_FAILED_POPULATING_GAMERS);
			}
			else
			{
				m_clanQueryStatus.Reset();
				m_onlineQueryStatus.Reset();
				PopulateGamerHandles();
			}
		}
	}

	BasePlayerCardDataManager::Update();
}

void CClanPlayerCardDataManager::Reset()
{
	if(m_clanQueryStatus.Pending())
		rlClan::Cancel(&m_clanQueryStatus);

	if(m_onlineQueryStatus.Pending())
		rlGetTaskManager()->CancelTask(&m_onlineQueryStatus);

	m_onlineClanMembers.Reset();
	m_clanMembers.Reset();
	m_clanQueryStatus.Reset();
	m_onlineQueryStatus.Reset();

	m_onlineMemberCount = 0;
	m_returnedClanCount = 0;
	m_totalClanCount = 0;

	BasePlayerCardDataManager::Reset();
}

void CClanPlayerCardDataManager::SetClan(rlClanId newClan, bool bIsLocalPlayersPrimary)
{
	if( m_ClanId != newClan )
	{
		Reset();
		m_ClanId = newClan;
	}
	m_bIsLocalPlayersPrimaryClan = bIsLocalPlayersPrimary;
}


const PlayerCardStatInfo* CClanPlayerCardDataManager::GetStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	if(CPauseMenu::IsSP())
	{
		return &rStatManager.GetSPCrewStats();
	}
	else if(NetworkInterface::IsNetworkOpen() && CNetwork::GetNetworkSession().IsTransitionActive())
	{
		return &rStatManager.GetMPFriendStats();
	}
	else
	{
		return &rStatManager.GetMPCrewStats();
	}
}

const PlayerCardPackedStatInfo* CClanPlayerCardDataManager::GetPackedStatInfo(const CPlayerCardXMLDataManager& rStatManager) const
{
	if(CPauseMenu::IsSP())
	{
		return &rStatManager.GetSPPackedStats();
	}
	else if(NetworkInterface::IsNetworkOpen() && CNetwork::GetNetworkSession().IsTransitionActive())
	{
		return &rStatManager.GetMPPackedStats();
	}
	else
	{
		return &rStatManager.GetMPPackedStats();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

void CPlayerCardCoronaManager::Shutdown(const unsigned UNUSED_PARAM(shutdownMode))
{
	m_players.Reset();
}

void CPlayerCardCoronaManager::Reset()
{
	if (!CNetwork::GetNetworkSession().IsTransitionActive())
	{
		playercardDebugf1("[corona_sync] CPlayerCardCoronaManager::Reset - %d players still in array", m_players.size());
		for(int i=0; i<m_players.size(); i++)
		{
			playercardDebugf1("[corona_sync]     %s", NetworkUtils::LogGamerHandle(m_players[i].m_handle));
		}
		m_players.Reset();
	}
}

bool CPlayerCardCoronaManager::Exists(const rlGamerHandle& handle) const 
{
	for (int i=0; i<m_players.GetCount(); i++)
	{
		if (m_players[i].m_handle == handle)
			return true;
	}

	return false;
}

bool CPlayerCardCoronaManager::RequestFromGamer(const rlGamerHandle& handle)
{
	const int index = GetIndex(handle);
	if (-1 < index)
	{
		m_players[index].m_currentStatIndex = 0;
		return SendMessage(index);
	}
	else
	{
		playercardWarningf("[corona_sync] RequestFromGamer - FAILED - Gamer='%s' does not exist.", NetworkUtils::LogGamerHandle(handle));
	}
	return false;
}

int CPlayerCardCoronaManager::GetIndex(const rlGamerHandle& handle) const 
{
	for (int i=0; i<m_players.GetCount(); i++)
	{
		if (m_players[i].m_handle == handle)
			return i;
	}

	return -1;
}

void CPlayerCardCoronaManager::Add(const rlGamerHandle& handle)
{
	if (!playercardVerify(!m_players.IsFull()))
		return;

	if (Exists(handle))
	{
		playercardDebugf1("[corona_sync] CPlayerCardCoronaManager::Add() - Already Exists - '%s'.", NetworkUtils::LogGamerHandle(handle));
		return;
	}

	if(playercardVerify(CNetwork::GetNetworkSession().IsTransitionMember(handle)))
	{
		playercardDebugf1("[corona_sync] CPlayerCardCoronaManager::Add() - Success - '%s'.", NetworkUtils::LogGamerHandle(handle));

		sPlayerMsgInfo player(handle);
		m_players.Push(player);

		//Request player card stats
		CNetwork::GetNetworkSession().SendPlayerCardDataRequest(handle);
	}
	else
	{
		playercardDebugf1("[corona_sync] CPlayerCardCoronaManager::Add() - Fail - '%s'.", NetworkUtils::LogGamerHandle(handle));
	}
}

void CPlayerCardCoronaManager::Remove(const rlGamerHandle& handle)
{
	const int index = GetIndex(handle);

	if (-1 < index)
	{
		m_players.Delete(index);
	}
}

void CPlayerCardCoronaManager::Update( )
{
	for (int i=0; i<m_players.GetCount(); i++)
		SendMessage(i);
}

bool CPlayerCardCoronaManager::SendMessage(const int index)
{
	if (playercardVerify(-1 < index) && SPlayerCardManager::IsInstantiated())
	{
		if (m_statIdsCount <= 0 || m_players[index].m_currentStatIndex < m_statIdsCount)
		{
			//This can happen for players joining.
			CPlayerCardManagerCurrentTypeHelper helper(CPlayerCardManager::CARDTYPE_CORONA_PLAYERS);

			const atArray<int>* managerStatIds = SPlayerCardManager::GetInstance().GetStatIds();
			if (managerStatIds)
			{
				//save up the count
				m_statIdsCount = managerStatIds->GetCount();

				if (m_players[index].m_currentStatIndex < managerStatIds->GetCount())
				{
					const bool lz4Compression = (RSG_DURANGO || RSG_ORBIS || RSG_PC);
					PlayerCardDataParserHelper parserhelper(lz4Compression);

					const int currIdx  = m_players[index].m_currentStatIndex;
					const int* statIds = managerStatIds->GetElements();

					const u32 numStatIds = (managerStatIds->GetCount() - currIdx) > MAX_NUM_STATS_IN_MESSAGE ? MAX_NUM_STATS_IN_MESSAGE : managerStatIds->GetCount() - currIdx;

					//Create the buffer.
					if (parserhelper.CreateDataBuffer(&statIds[currIdx], numStatIds))
					{
						playercardDebugf1("[corona_sync] SendPlayerCardData() - CountOfStats='%d'n=, this message numStats='%d'.", managerStatIds->GetCount(), numStatIds);
						//Send Buffer
						CNetwork::GetNetworkSession().SendPlayerCardData(m_players[index].m_handle, parserhelper.GetBufferSize(), parserhelper.Getbuffer());
						m_players[index].m_currentStatIndex += (int)numStatIds;

						return true;
					}
				}
			}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerCardManager::Init(CardTypes type, const BasePlayerCardDataManager::ValidityRules& rListRules)
{
	m_currentType = type;
	BasePlayerCardDataManager* pDataManager = GetDataManager();
	if(pDataManager)
	{
		pDataManager->SetListRules(rListRules);

		if(!pDataManager->WasInitialized())
		{
			pDataManager->Init(GetXMLDataManager());
		}
	}
}

void CPlayerCardManager::Update()
{
	m_activePlayers.Update();
	m_spFriendPlayers.Update();
	m_mpFriendPlayers.Update();
	m_partyPlayers.Update();
	m_clanPlayers.Update();
	m_lastJobPlayers.Update();
	m_joinedPlayers.Update();
	m_matchedPlayers.Update();
	m_playlistJoinedPlayers.Update();
	m_directorPlayers.Update();
	m_invitableSessionPlayers.Update();
}

BasePlayerCardDataManager* CPlayerCardManager::GetDataManager(CardTypes type)
{
	const CPlayerCardManager* pThis = this;
	return const_cast<BasePlayerCardDataManager*>(pThis->GetDataManager(type));
}

const BasePlayerCardDataManager* CPlayerCardManager::GetDataManager(CardTypes type) const
{
	switch(type)
	{
	case CARDTYPE_ACTIVE_PLAYER:
		return &m_activePlayers;
	case CARDTYPE_SP_FRIEND:
		return &m_spFriendPlayers;
	case CARDTYPE_MP_FRIEND:
		return &m_mpFriendPlayers;
	case CARDTYPE_PARTY:
		return &m_partyPlayers;
	case CARDTYPE_CLAN:
		return &m_clanPlayers;
	case CARDTYPE_LAST_JOB:
		return &m_lastJobPlayers;

	//Corona and Joined are the same fucking thing.
	case CARDTYPE_CORONA_PLAYERS:
		return &m_joinedPlayers;
	//Corona and Joined are the same fucking thing.
	case CARDTYPE_JOINED:
		return &m_joinedPlayers;

	case CARDTYPE_INVITABLE_SESSION_PLAYERS:
		return &m_invitableSessionPlayers;
	case CARDTYPE_MATCHED:
		return &m_matchedPlayers;
	case CARDTYPE_CORONA_JOINED_PLAYLIST:
		return &m_playlistJoinedPlayers;
	case CARDTYPE_DIRECTOR:
		return &m_directorPlayers;

	default:
		playercardAssertf(0, "CPlayerCardManager::GetDataManager - trying to get an unknown type(%d) of PlayerCardDataManager", type);
		break;
	}

	return NULL;
}

const PlayerCardStatInfo* CPlayerCardManager::GetStatInfo() const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		return pBase->GetStatInfo(GetXMLDataManager());
	}

	return NULL;
}

const atArray<int>* CPlayerCardManager::GetStatIds() const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		return pBase->GetStatIds(GetXMLDataManager());
	}

	return NULL;
}

const rlProfileStatsValue* CPlayerCardManager::GetStat(const rlGamerHandle& gamerHandle, int statHash) const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		return pBase->GetStat(GetXMLDataManager(), gamerHandle, statHash);
	}

	return NULL;
}

const PlayerCardStatInfo::StatData* CPlayerCardManager::GetStatData(int statHash) const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		const atArray<int>* pStats = pBase->GetStatIds(GetXMLDataManager());
		const PlayerCardStatInfo* pStatInfo = pBase->GetStatInfo(GetXMLDataManager());
		if(pStats && pStatInfo)
		{
			int statIndex = pStats->Find(statHash);
			if(playercardVerifyf(statIndex != -1, "CPlayerCardManager::GetStat - Trying to get a stat with hash (0x%08x, %d), but the playerCardSetup.xml doesn't know about it.", statHash, statHash))
			{
				return &(pStatInfo->GetStatDatas()[statIndex]);
			}
		}
	}

	return NULL;
}

#if! __NO_OUTPUT
const char* CPlayerCardManager::GetStatName(int statHash) const
{
	const PlayerCardStatInfo::StatData* pData = GetStatData(statHash);
	return pData ? pData->m_name : "(null)";
}
#endif //!__NO_OUTPUT

const rlProfileStatsValue* CPlayerCardManager::GetCorrectedMultiStatForCurrentGamer(int statHash, int characterSlot) const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		const PlayerCardStatInfo* pInfo = pBase->GetStatInfo(GetXMLDataManager());
		if(pInfo)
		{
			const atArray<int>* pStats = &pInfo->GetStatHashes();
			const atArray<PlayerCardStatInfo::StatData>* pStatDatas = &pInfo->GetStatDatas();
	
			if(pStats && pStatDatas)
			{
				int statIndex = pStats->Find(statHash);
				if(playercardVerifyf(statIndex != -1, "CPlayerCardManager::GetCorrectedMultiStatForCurrentGamer - Trying to get a stat with hash (0x%08x, %d, %s), but the playerCardSetup.xml doesn't know about it.", statHash, statHash, StatsInterface::GetKeyName(statHash)))
				{
					if((*pStatDatas)[statIndex].m_isMulti && (statIndex+characterSlot) < pStats->size())
					{
						return pBase->GetStat(m_currentGamerHandle, statIndex+characterSlot);
					}
					else
					{
						return pBase->GetStat(m_currentGamerHandle, statIndex);
					}
				}
			}
		}
	}

	return NULL;
}

const PlayerCardPackedStatInfo* CPlayerCardManager::GetPackedStatInfos() const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		return pBase->GetPackedStatInfo(GetXMLDataManager());
	}

	return NULL;
}

const PlayerCardPackedStatInfo::StatData* CPlayerCardManager::GetPackedStatInfo(u32 statHash) const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	if(pBase)
	{
		const PlayerCardPackedStatInfo* pInfo = pBase->GetPackedStatInfo(GetXMLDataManager());
		if(pInfo)
		{
			return pInfo->GetStatData(statHash);
		}
	}

	return NULL;
}

bool CPlayerCardManager::GetPackedStatValue(int playerIndex, u32 statHash, int& outVal) const
{
	const BasePlayerCardDataManager* pBase = GetDataManager();
	const PlayerCardPackedStatInfo::StatData* pData = GetPackedStatInfo(statHash);
	if(pBase && pData)
	{
		const atArray<int>* pStats = GetStatIds();
		int statIndex = pStats->Find(pData->m_realName.GetHash());
		if(playercardVerifyf(statIndex != -1, "CPlayerCardManager::GetPackedStatValue - Trying to get a stat (0x%08x, %d), but the playerCardSetup.xml doesn't know about it. alias=%s, realName=%s", statHash, statHash, pData->m_alias.GetCStr(), pData->m_realName.GetCStr()))
		{
			const rlProfileStatsValue* pStat = pBase->GetStat(playerIndex, statIndex);
			if(pStat && playercardVerifyf(pStat->GetType() == RL_PROFILESTATS_TYPE_INT64, "CPlayerCardManager::GetPackedStatValue - Packed Stats are only supposed as being a s64 type (using type %d). %s(0x%08x, %d) alias=%s, realName=%s", pStat->GetType(), GetStatName(statHash), statHash, statHash, pData->m_alias.GetCStr(), pData->m_realName.GetCStr()))
			{
				u64 statValue = static_cast<u64>(pStat->GetInt64());
				u64 numberOfBits = pData->m_isInt ? 8 : 1;
				u64 shift = numberOfBits*pData->m_index;
				outVal = ExtractPackedValue(statValue, shift, numberOfBits);
				return true;
			}
		}
	}

	return false;
}

int CPlayerCardManager::ExtractPackedValue(u64 data, u64 shift, u64 numberOfBits) const
{
	u64 mask        = (1 << numberOfBits) - 1;
	u64 maskShifted = (mask << shift);
	return (int)((data & maskShifted) >> shift);
}

// eof

