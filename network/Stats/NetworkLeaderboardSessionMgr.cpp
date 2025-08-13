// 
// networkleaderboardsessionmgr.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
//

//Rage headers
#include "rline/rlstats.h"
#include "rline/rlfriend.h"
#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "basetypes.h"
#endif

//Framework headers
#include "fwnet/netscgameconfigparser.h"
#include "fwnet/netleaderboardcommon.h"
#include "fwnet/netleaderboardread.h"
#include "fwnet/netleaderboardwrite.h"
#include "fwnet/netchannel.h"
#include "fwsys/timer.h"

//Game headers
#include "networkleaderboardsessionmgr.h "
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "Stats/StatsInterface.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, leaderboard_mgr, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_leaderboard_mgr


// --------------------- CNetworkReadLeaderboards

CNetworkReadLeaderboards::CNetworkReadLeaderboards() 
: netLeaderboardReadMgr()
, m_Initialized(false)
{
}

CNetworkReadLeaderboards::~CNetworkReadLeaderboards()
{
	Shutdown();
}

void  
CNetworkReadLeaderboards::Init(sysMemAllocator* allocator)
{
	if (gnetVerify(allocator))
	{
		m_Initialized = true;
		netLeaderboardReadMgr::Init(allocator);
	}
}

void  
CNetworkReadLeaderboards::Shutdown()
{
	if (m_Initialized)
	{
		m_Initialized = false;
		ClearCachedDisplayData();
		netLeaderboardReadMgr::Shutdown();
	}
}

void
CNetworkReadLeaderboards::Update()
{
	if (!m_Initialized)
		return;

	netLeaderboardReadMgr::Update();

	BANK_ONLY( UpdateWidgets() );
}

const netLeaderboardRead*
CNetworkReadLeaderboards::ReadByGamer(const unsigned id
									  ,const rlLeaderboard2Type type
									  ,const rlLeaderboard2GroupSelector& groupSelector
									  ,const rlClanId clanId
									  ,const unsigned numGroups, const rlLeaderboard2GroupSelector* groupSelectors)
{
	const netLeaderboardRead* result = NULL;

	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	if (!m_Initialized)
		return result;

	const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard( id );
	if (!gnetVerify(lbConf))
		return result;

	unsigned numGamers = 0;

	rlGamerHandle gamers[MAX_NUM_ACTIVE_PLAYERS];

	unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
	const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();

	for(unsigned index = 0; index < numActivePlayers; index++)
	{
		const netPlayer *player = activePlayers[index];
		const rlGamerHandle gamerHandler = player->GetGamerInfo().GetGamerHandle();
		gnetAssertf(gamerHandler.IsValid(), "Gamer handler is not valid.");

		if (CanReadStatistics(gamerHandler) && !IsGamerPresent(id, gamerHandler))
		{
			gnetDebug1("Add player \"%s\" to read statistics for leaderboard \"%s\"", player->GetLogName(), lbConf->GetName());
			gamers[numGamers] = gamerHandler;
			numGamers++;
		}
		else if(!CanReadStatistics(gamerHandler))
		{
			gnetWarning("Can not read statistics for player \"%s\"", player->GetLogName());
		}
		else if(!CanReadStatistics(gamerHandler))
		{
			gnetDebug1("We already have player \"%s\" statistics for leaderboard \"%s\"", player->GetLogName(), lbConf->GetName());
		}
	}

	if (gnetVerify(0 < numGamers))
	{
		result = AddReadByRow(NetworkInterface::GetLocalGamerIndex(), id, type, groupSelector, clanId, numGamers, &gamers[0], 0 , NULL, numGroups, groupSelectors);
	}

	return result;
}

const netLeaderboardRead*
CNetworkReadLeaderboards::ReadByFriends(const unsigned id
										,const rlLeaderboard2Type type
										,const rlLeaderboard2GroupSelector& groupSelector
										,const rlClanId clanId
										,const unsigned numGroups, const rlLeaderboard2GroupSelector* groupSelectors
										,const bool includeLocalPlayer
										,const int friendStartIndex
										,const int maxNumFriends)
{
	const netLeaderboardRead* result = NULL;

	if (!NetworkInterface::IsLocalPlayerOnline())
		return result;

	if (!m_Initialized)
		return result;

	const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard( id );
	if (!gnetVerify(lbConf))
		return result;

	const u32 LB_MAX_NUM_FREINDS = 101; //100 friends plus one for the local player.
	gnetAssert(maxNumFriends < LB_MAX_NUM_FREINDS);

	rlGamerHandle friends[LB_MAX_NUM_FREINDS];

	unsigned numFriends = 0;

	if (includeLocalPlayer)
	{
		const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
		if (pInfo)
		{
			friends[0] = pInfo->GetGamerHandle();
			numFriends++;
		}
	}

	int maxFriends = Min((int)rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex()), maxNumFriends);
	for (int friendIndex=friendStartIndex; friendIndex < maxFriends; friendIndex++)
	{
		rlFriendsManager::GetGamerHandle(friendIndex, &friends[numFriends]);
		numFriends++;
	}

	if (gnetVerify(0 < numFriends))
	{
		result = AddReadByRow(NetworkInterface::GetLocalGamerIndex(),id, type, groupSelector, clanId, numFriends, &friends[0], 0 , NULL, numGroups, groupSelectors);
	}

	return result;
}

void
CNetworkReadLeaderboards::ClearAll()
{
	Clear();
}

void
CNetworkReadLeaderboards::ClearLeaderboardRead(const unsigned id, const rlLeaderboard2Type type, const int lbIndex)
{
	Delete(id, type, lbIndex);
}

void
CNetworkReadLeaderboards::ClearLeaderboardRead(const netLeaderboardRead* pThisLb)
{
	Delete(pThisLb);
}

bool
CNetworkReadLeaderboards::CacheScriptDisplayData(const int id, const int columnsBitSets, CLeaderboardScriptDisplayData::sRowData& row)
{
	bool result = false;

	netLeaderboardManageAllocatorScopeHelper allocHelper(m_Allocator);

	for (int i=0; i<m_ScriptDisplayData.GetCount() && !result; i++)
	{
		if (id == m_ScriptDisplayData[i].m_Id)
		{
			gnetAssert(m_ScriptDisplayData[i].m_ColumnsBitSets == columnsBitSets);
			gnetAssertf(m_ScriptDisplayData[i].m_Rows[0].m_Columns.GetCount() == row.m_Columns.GetCount(),
				"Trying to add a row to leaderboard cache with different number of columns than existing rows. Expected Columns: %d Columns trying to add: %d. Cached ID: %d",
				m_ScriptDisplayData[i].m_Rows[0].m_Columns.GetCount(),
				row.m_Columns.GetCount(),
				id);

			if (gnetVerifyf(m_ScriptDisplayData[i].m_Rows.GetAvailable() > 0, "Tried to add more than %d rows to a leaderboard cache", m_ScriptDisplayData[i].m_Rows.GetCount()))
			{
				m_ScriptDisplayData[i].m_Rows.Append() = row;
			}

			result = true;
		}
	}

	if (!result)
	{
		if (gnetVerifyf(m_ScriptDisplayData.GetAvailable() > 0, "Tried to cache too many leaderboards, they must be cleaned up when no longer in use."))
		{
			CLeaderboardScriptDisplayData &lb = m_ScriptDisplayData.Append();
			lb.m_Rows.SetCount(0);
			lb.m_Id = id;
			lb.m_ColumnsBitSets = columnsBitSets;
			lb.m_Time = sysTimer::GetSystemMsTime();
			lb.m_Rows.Append() = row;
			result = true;
		}

	}

	return result;
}

void 
CNetworkReadLeaderboards::ClearCachedDisplayData()
{
	netLeaderboardManageAllocatorScopeHelper allocHelper(m_Allocator);
	m_ScriptDisplayData.clear();
	m_ScriptDisplayData.Reset();
}

bool 
CNetworkReadLeaderboards::ClearCachedDisplayDataById(const int id)
{
	netLeaderboardManageAllocatorScopeHelper allocHelper(m_Allocator);

	bool found = false;

	for (int i=0; i<m_ScriptDisplayData.GetCount() && !found; i++)
	{
		if (id == m_ScriptDisplayData[i].m_Id)
		{
			m_ScriptDisplayData.DeleteFast(i);

			found = true;
			break;
		}
	}

	return found;
}

#if __BANK

static const char* s_BankLeaderboardTypes[] = { "Invalid", "Player", "Clan", "Clan_Member" , "Group", "Group_Member" };
static int   s_BankLeaderboardTypeIdx = 0;

//Players
static const char*  s_BankPlayers[MAX_NUM_ACTIVE_PLAYERS+1]; // Extra space for invalid gamer.
static bkCombo*     s_BankPlayersCombo;
static int          s_BankPlayersComboIndex;
static u32          s_BankNumPlayers;

//Friends
static const char*  s_BankFriends[rlFriendsPage::MAX_FRIEND_PAGE_SIZE+1]; // Extra space for no friend.
static bkCombo*     s_BankFriendsCombo;
static int          s_BankFriendsComboIndex;
static u32          s_BankNumFriends;

enum eBankReadOps
{
	 ReadSessionGamersByRow
	,ReadAllFriendsByRow
	,ReadByRow
	,ReadByRank
	,ReadByRadius
};
static const char* s_BankReadOps[] = { "ReadSessionGamersByRow"
								,"ReadAllFriendsByRow"
								,"ReadByRow"
								,"ReadByRank"
								,"ReadByRadius" };
static int s_BankReadOpsIdx = 0;

static int  s_BankLeaderboardIdx = 0;
static atArray< const char* > s_BankLbNames;

static int  s_BankClanIds[MAX_LEADERBOARD_CLAN_IDS];
static u32  s_BankNumClanIds = 0;

static rlLeaderboard2GroupSelector s_BankGroupSelectors[MAX_LEADERBOARD_READ_GROUPS];
static u32  s_BankNumGroupSelectors = 0;

static rlLeaderboard2GroupSelector s_BankGroupSelector;
static int  s_BankClanId       = RL_INVALID_CLAN_ID;
static int  s_BankPivotClanId  = RL_INVALID_CLAN_ID;
static u32  s_BankRadius       = 0;
static u32  s_BankNumberOfRows = 0;
static u32  s_BankRankStart    = 1;
static bool s_BankIncludeLocalPlayer = false;
static bool s_BankUseActiveClanId    = false;
static bool s_BankUseOnlyLocalPlayer = false;

void
CNetworkReadLeaderboards::InitWidgets(bkBank* UNUSED_PARAM(in_pBank))
{
	bkBank* bank = BANKMGR.FindBank("Network");

	if (gnetVerify(bank))
	{
		bkGroup* group = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live/Debug Leaderboards/Debug Session/Debug Read"));
		if (group)
		{
			bank->DeleteGroup(*group);
		}

		bkGroup* debugGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live/Debug Leaderboards/Debug Session"));
		if(gnetVerify(debugGroup))
		{
			bool resetCurrentGroupAtEnd = false;

			if(debugGroup != bank->GetCurrentGroup())
			{
				bank->SetCurrentGroup(*debugGroup);
				resetCurrentGroupAtEnd = true;
			}

			s_BankGroupSelector.m_NumGroups = 0;
			for (int i=0; i<RL_LEADERBOARD2_MAX_GROUPS; i++)
			{
				s_BankGroupSelector.m_Group[i].m_Category[0] = '\0';
				s_BankGroupSelector.m_Group[i].m_Id[0]       = '\0';
			}

			s_BankNumGroupSelectors = 0;
			for (int i=0; i<MAX_LEADERBOARD_READ_GROUPS; i++)
			{
				s_BankGroupSelectors[i].m_NumGroups = 0;
				for (int j=0; j<RL_LEADERBOARD2_MAX_GROUPS; j++)
				{
					s_BankGroupSelectors[i].m_Group[j].m_Category[0] = '\0';
					s_BankGroupSelectors[i].m_Group[j].m_Id[0]       = '\0';
				}
			}

			s_BankNumClanIds = 0;
			for (int i=0; i<MAX_LEADERBOARD_CLAN_IDS; i++)
				s_BankClanIds[i] = 0;

			//Players
			s_BankPlayersCombo = 0;
			s_BankPlayersComboIndex = 0;
			s_BankPlayers[0] = "--No Player--";
			s_BankNumPlayers  = 1;

			//Friends
			s_BankFriendsCombo = 0;
			s_BankFriendsComboIndex = 0;
			s_BankFriends[0] = "--No Friend--";
			s_BankNumFriends  = 1;

			const LeaderboardsConfiguration::LeaderboardConfigList& leaderboardList = GAME_CONFIG_PARSER.GetLeaderboardList();
			s_BankLbNames.clear();
			for( int i = 0; i < leaderboardList.GetCount(); i++)
			{
				s_BankLbNames.PushAndGrow(leaderboardList[i].GetName());
			}

			s_BankLeaderboardIdx = 0;

			bank->PushGroup("Options");
				bank->AddCombo ("Current lb", &s_BankLeaderboardIdx, s_BankLbNames.GetCount(), (const char **)s_BankLbNames.GetElements(), NullCB);
				bank->AddCombo ("Type", &s_BankLeaderboardTypeIdx, (int) (sizeof(s_BankLeaderboardTypes) / sizeof(char *)), (const char **)s_BankLeaderboardTypes);
				bank->AddSlider("Clan Id",              &s_BankClanId,      MIN_INT32, MAX_INT32, 1);
				bank->AddSlider("Pivot Clan Id",        &s_BankPivotClanId, MIN_INT32, MAX_INT32, 1);
				bank->AddSlider("Radius",               &s_BankRadius,              0, MAX_INT32, 1);
				bank->AddSlider("Rank Start",           &s_BankRankStart,           0, MAX_INT32, 1);
				bank->AddSlider("Max Number Of Rows",   &s_BankNumberOfRows,        0, MAX_INT32, 1);
				bank->AddToggle("Include Local Player", &s_BankIncludeLocalPlayer);
				bank->AddToggle("Use Active Clan",      &s_BankUseActiveClanId);
				bank->AddToggle("Use only local Player",&s_BankUseOnlyLocalPlayer);

				s_BankPlayersCombo = bank->AddCombo ("Use Player", &s_BankPlayersComboIndex, s_BankNumPlayers, (const char **)s_BankPlayers);
				s_BankFriendsCombo = bank->AddCombo ("Use Friend", &s_BankFriendsComboIndex, s_BankNumFriends, (const char **)s_BankFriends);

				bank->PushGroup("Group Selector");
					bank->AddSlider("Number of Groups", &s_BankGroupSelector.m_NumGroups, 0, RL_LEADERBOARD2_MAX_GROUPS, 1);
					for (int i=0; i<RL_LEADERBOARD2_MAX_GROUPS; i++)
					{
						bank->PushGroup("Group");
							bank->AddText("Category", s_BankGroupSelector.m_Group[i].m_Category, COUNTOF(s_BankGroupSelector.m_Group[i].m_Category), false);
							bank->AddText("Id",       s_BankGroupSelector.m_Group[i].m_Id,       COUNTOF(s_BankGroupSelector.m_Group[i].m_Id),       false);
						bank->PopGroup();
					}
				bank->PopGroup();

				bank->PushGroup("Clan IDs");
					bank->AddSlider("Number of Clan Ids", &s_BankNumClanIds, 0, MAX_LEADERBOARD_CLAN_IDS, 1);
					bank->PushGroup("IDs");
						for (int i=0; i<MAX_LEADERBOARD_CLAN_IDS; i++)
							bank->AddSlider("Clan Id", &s_BankClanIds[i], MIN_INT32, MAX_INT32, 1);
					bank->PopGroup();
				bank->PopGroup();

				bank->PushGroup("Group Selectors");
					bank->AddSlider("Number of Group Selectors", &s_BankNumGroupSelectors, 0, MAX_LEADERBOARD_READ_GROUPS, 1);
					bank->PushGroup("Groups");
						for (int i=0; i<MAX_LEADERBOARD_READ_GROUPS; i++)
						{
							bank->AddSlider("Number of Groups", &s_BankGroupSelectors[i].m_NumGroups, 0, RL_LEADERBOARD2_MAX_GROUPS, 1);
							for (int j=0; j<RL_LEADERBOARD2_MAX_GROUPS; j++)
							{
								bank->PushGroup("Group");
									bank->AddText("Category", s_BankGroupSelectors[i].m_Group[j].m_Category, COUNTOF(s_BankGroupSelectors[i].m_Group[j].m_Category), false);
									bank->AddText("Id",       s_BankGroupSelectors[i].m_Group[j].m_Id,       COUNTOF(s_BankGroupSelectors[i].m_Group[j].m_Id),       false);
								bank->PopGroup();
							}
						}
					bank->PopGroup();
				bank->PopGroup();
			bank->PopGroup();

			bank->PushGroup("Debug Read");
			{
				bank->AddCombo ("Read Operation", &s_BankReadOpsIdx, (int) (sizeof(s_BankReadOps) / sizeof(char *)), (const char **)s_BankReadOps);
				bank->AddButton("Do Read Statistics", datCallback(MFA(CNetworkReadLeaderboards::Bank_ReadStatistics), (datBase*)this));
				bank->AddSeparator("");
				bank->AddButton("Print All Rows", datCallback(MFA(CNetworkReadLeaderboards::Bank_PrintTop), (datBase*) this));
				bank->AddButton("Clear data", datCallback(MFA(CNetworkReadLeaderboards::ClearAll), (datBase*)this));
				bank->AddSeparator("");
			}
			bank->PopGroup();

			if(resetCurrentGroupAtEnd)
			{
				bank->UnSetCurrentGroup(*debugGroup);
			}
		}
	}
}

void
CNetworkReadLeaderboards::UpdateWidgets()
{
	if(!CLiveManager::IsOnline())
		return; 

	static u32 lastTime = fwTimer::GetSystemTimeInMilliseconds();
	u32 thisTime = fwTimer::GetSystemTimeInMilliseconds();

	if((thisTime > lastTime) && ((thisTime - lastTime) > 5000))
	{
		lastTime = thisTime;

		if (s_BankPlayersCombo)
		{
			s_BankNumPlayers  = 0;

			unsigned                 numActivePlayers = netInterface::GetNumActivePlayers();
			const netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();

			for(unsigned index = 0; index < numActivePlayers; index++)
			{
				const netPlayer *player = activePlayers[index];
				if(!player->IsBot())
				{
					s_BankPlayers[s_BankNumPlayers] = player->GetLogName();
					s_BankNumPlayers++;
				}
			}
			//Avoid empty combo boxes
			int nNumPlayers = s_BankNumPlayers;
			if(nNumPlayers == 0)
			{
				s_BankPlayers[0] = "--No Player--";
				nNumPlayers  = 1;
			}
			else if(gnetVerify(s_BankNumPlayers <= MAX_NUM_ACTIVE_PLAYERS))
			{
				s_BankPlayers[s_BankNumPlayers] = "--No Player--";
				s_BankNumPlayers++;
				nNumPlayers = s_BankNumPlayers;
			}

			s_BankPlayersCombo->UpdateCombo("Use Player", &s_BankPlayersComboIndex, nNumPlayers, (const char **)s_BankPlayers);
		}

		if (s_BankFriendsCombo)
		{
			//Update the friend list
			s_BankNumFriends = CLiveManager::GetFriendsPage()->m_NumFriends;
			for (u32 i=0; i<s_BankNumFriends; i++)
			{
				s_BankFriends[i] = CLiveManager::GetFriendsPage()->m_Friends[i].GetName();
			}
			//Avoid empty combo boxes
			int nNumFriends = s_BankNumFriends;
			if(nNumFriends == 0)
			{
				s_BankFriends[0] = "--No Friend--";
				nNumFriends  = 1;
			}
			else if(gnetVerify(s_BankNumFriends <= rlFriendsPage::MAX_FRIEND_PAGE_SIZE))
			{
				s_BankFriends[s_BankNumFriends] = "--No Friend--";
				s_BankNumFriends++;
				nNumFriends = s_BankNumFriends;
			}

			s_BankFriendsCombo->UpdateCombo("Use Friend", &s_BankFriendsComboIndex, nNumFriends, (const char **)s_BankFriends);
		}
	}
}

void
CNetworkReadLeaderboards::Bank_PrintTop()
{
	const LeaderboardsConfiguration::LeaderboardConfigList& leaderboardList = GAME_CONFIG_PARSER.GetLeaderboardList();
	const Leaderboard& lb = leaderboardList[s_BankLeaderboardIdx];
	for (unsigned j=0; j<CountOfLeaderboards(lb.m_id); j++)
	{
		const netLeaderboardRead* lbr = GetLeaderboardByIndex(lb.m_id, j);
		if (lbr)
		{
			lbr->PrintStatistics();
		}
	}
}

void 
CNetworkReadLeaderboards::Bank_ReadStatistics()
{
	if (s_BankUseActiveClanId)
	{
		if (s_BankClanId == 0 && CLiveManager::IsOnline() && CLiveManager::GetNetworkClan().HasPrimaryClan())
		{
			const rlClanDesc* clan = CLiveManager::GetNetworkClan().GetPrimaryClan();
			if (gnetVerify(clan))
			{
				s_BankClanId = (int)clan->m_Id;
			}
		}
	}

	rlGamerHandle  gamerHandle;
	const char* playerName = s_BankPlayers[s_BankPlayersComboIndex];
	if ( stricmp(playerName, "--No Player--") != 0 )
	{
		unsigned playerIndex = 0;
		unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers; index++)
		{
			const netPlayer *player = remoteActivePlayers[index];
			if(!player->IsBot())
			{
				if (playerIndex == (unsigned)s_BankPlayersComboIndex)
				{
					gamerHandle = player->GetGamerInfo().GetGamerHandle();
					break;
				}

				playerIndex++;
			}
		}
	}

	rlGamerHandle  friendHandle;
	const char* friendName = s_BankFriends[s_BankFriendsComboIndex];
	if ( stricmp(friendName, "--No Friend--") != 0 )
	{
		rlFriend& pfriend = CLiveManager::GetFriendsPage()->m_Friends[s_BankFriendsComboIndex];
		pfriend.GetGamerHandle(&friendHandle);
	}

	const LeaderboardsConfiguration::LeaderboardConfigList& leaderboardList = GAME_CONFIG_PARSER.GetLeaderboardList();
	const Leaderboard& lbConf = leaderboardList[s_BankLeaderboardIdx];

	if (s_BankLeaderboardTypeIdx == 0)
		return;

	switch(s_BankReadOpsIdx)
	{
	case ReadSessionGamersByRow:
		{
			ReadByGamer(lbConf.m_id
						,(rlLeaderboard2Type)s_BankLeaderboardTypeIdx
						,s_BankGroupSelector
						,(rlClanId)s_BankClanId
						,s_BankNumGroupSelectors, &s_BankGroupSelectors[0]);
		}
		break;

	case ReadAllFriendsByRow:
		{
			ReadByFriends(lbConf.m_id
						,(rlLeaderboard2Type)s_BankLeaderboardTypeIdx
						,s_BankGroupSelector
						,(rlClanId)s_BankClanId
						,s_BankNumGroupSelectors, &s_BankGroupSelectors[0]
						,s_BankIncludeLocalPlayer, 0, 100);
		}
		break;

	case ReadByRow:
		{
			if (s_BankUseOnlyLocalPlayer && !gamerHandle.IsValid())
			{
				const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
				if (pGamerInfo)
				{
					gamerHandle = pGamerInfo->GetGamerHandle();
				}
			}

			unsigned numGamers = 0; 
			if (gamerHandle.IsValid() || friendHandle.IsValid())
			{
				numGamers = 1;
			}

			AddReadByRow(NetworkInterface::GetLocalGamerIndex()
						,lbConf.m_id
						,(rlLeaderboard2Type)s_BankLeaderboardTypeIdx
						,s_BankGroupSelector
						,(rlClanId)s_BankClanId
						,numGamers, gamerHandle.IsValid() ? &gamerHandle : &friendHandle
						,s_BankNumClanIds, (const rlClanId*) &s_BankClanIds[0]
						,s_BankNumGroupSelectors, &s_BankGroupSelectors[0]);
		}
		break;

	case ReadByRank:
		{
			AddReadByRank(NetworkInterface::GetLocalGamerIndex()
							,lbConf.m_id
							,(rlLeaderboard2Type)s_BankLeaderboardTypeIdx
							,s_BankGroupSelector
							,(rlClanId)s_BankClanId
							,s_BankNumberOfRows
							,s_BankRankStart);
		}
		break;

	case ReadByRadius:
		{
			if (s_BankUseOnlyLocalPlayer && !gamerHandle.IsValid())
			{
				const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
				if (pGamerInfo)
				{
					gamerHandle = pGamerInfo->GetGamerHandle();
				}
			}

			AddReadByRadius(NetworkInterface::GetLocalGamerIndex()
							,lbConf.m_id, (rlLeaderboard2Type)s_BankLeaderboardTypeIdx
							,s_BankGroupSelector
							,s_BankRadius
							,gamerHandle.IsValid() ? gamerHandle : friendHandle
							,(rlClanId)s_BankPivotClanId);
		}
		break;
	}
}

#endif //__BANK



// --------------------- CNetworkWriteLeaderboards
void  
CNetworkWriteLeaderboards::Init(sysMemAllocator* allocator)
{
	if (gnetVerify(allocator))
	{
		m_Initialized = true;
		netLeaderboardWriteMgr::Init(allocator);
		m_State = STATE_NONE;
	}
}

void  
CNetworkWriteLeaderboards::Shutdown()
{
	if (m_Initialized)
	{
		m_State = STATE_NONE;
		m_Initialized = false;
		netLeaderboardWriteMgr::Shutdown();
	}
}

void 
CNetworkWriteLeaderboards::Update()
{
	if (!m_Initialized)
	{
		return;
	}

	//If it has finished flushing we can check again for
	//  pending leaderboards to be flushed.
	const bool checkForFlushing = Finished();

	netLeaderboardWriteMgr::Update();

	if (m_State == STATE_FLUSH)
	{
		DoFlush();
	}

	//Check for pending leaderboards to be flushed.
	if (checkForFlushing && !InProgress() && 0 < GetLeaderboardCount())
	{
		Flush();
	}
	else if (m_State == STATE_NONE)
	{
		//Flush is needed.
		if (!checkForFlushing && !InProgress() && 0 < GetLeaderboardCount())
		{
			Flush();
		}
	}
}

bool 
CNetworkWriteLeaderboards::Flush()
{
	if(gnetVerify(m_Initialized && m_State == STATE_NONE))
	{
		if (m_matchEnded)
		{
			m_matchEnded = false;

			//Lets get vehicle leaderboards written with the match end leaderboards.
			if (NetworkInterface::IsGameInProgress())
				StatsInterface::TryVehicleLeaderboardWriteOnMatchEnd();
		}

		gnetDebug1("Flush requested.");
		m_State = STATE_FLUSH;
		return true;
	}

	return false;
}

void 
CNetworkWriteLeaderboards::DoFlush()
{
	gnetDebug1("Start Flush All.");

	bool flushAll = (m_State == STATE_FLUSH);

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

	if (gnetVerify(m_State == STATE_FLUSH))
	{
		if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) || !NetworkInterface::IsOnlineRos())
		{
			gnetWarning("Ignoring request to flush LB because local gamer isn't online.");
			flushAll = false;
		}
	}

	if (flushAll)
	{
		FlushAll(localGamerIndex);
	}
	else
	{
		gnetError("Failed leaderboard Flush");
		DeleteAll();
	}

	m_State = STATE_NONE;

	gnetDebug1("End Flush All.");
}

#if __BANK
void
CNetworkWriteLeaderboards::InitWidgets( bkBank* UNUSED_PARAM(in_pBank) )
{
	bkBank* bank = BANKMGR.FindBank("Network");

	if (gnetVerify(bank))
	{
		bkGroup* group = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live/Debug Leaderboards/Debug Session/Debug Write"));
		if (group)
		{
			bank->DeleteGroup(*group);
		}

		bkGroup *debugGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live/Debug Leaderboards/Debug Session"));
		if(gnetVerify(debugGroup))
		{
			bool resetCurrentGroupAtEnd = false;

			m_BankUpdateColumns = 0;
			m_Valuei = 0;
			m_Valuef = 0.0f;

			if(debugGroup != bank->GetCurrentGroup())
			{
				bank->SetCurrentGroup(*debugGroup);
				resetCurrentGroupAtEnd = true;
			}

			bank->PushGroup("Debug Writer");
			{
				bank->AddSlider("Max number of columns to set", &m_BankUpdateColumns, 0, BANK_MAX_COLUMNS, 1);
				bank->AddSlider("Upload Value int", &m_Valuei, 0, (int)0xFFFF, 1);
				bank->AddSlider("Upload Value float", &m_Valuef, 0.0f, (float)0xFFFF, 1.0f);
				bank->AddSeparator("");
				bank->AddButton("Add leaderboard to be updated.", datCallback(MFA(CNetworkWriteLeaderboards::Bank_UploadStatistics), (datBase*)this));
				bank->AddButton("Finish Upload.", datCallback(MFA(CNetworkWriteLeaderboards::Bank_FinishUploadStatistics), (datBase*)this));

				bank->AddSeparator();

				bank->AddButton("Clear Data for another upload", datCallback(MFA(CNetworkWriteLeaderboards::Bank_ClearData), (datBase*)this));
			}
			bank->PopGroup();

			if(resetCurrentGroupAtEnd)
			{
				bank->UnSetCurrentGroup(*debugGroup);
			}
		}
	}
}

void
CNetworkWriteLeaderboards::Bank_ClearData()
{
	if (Pending())
	{
		Cancel();
	}

	DeleteAll();
}

void
CNetworkWriteLeaderboards::Bank_UploadStatistics()
{
	const LeaderboardsConfiguration::LeaderboardConfigList& leaderboardList = GAME_CONFIG_PARSER.GetLeaderboardList();
	const Leaderboard& lbConf = leaderboardList[s_BankLeaderboardIdx];

	unsigned numValues = lbConf.m_columns.GetCount();
	gnetAssertf(numValues <= BANK_MAX_COLUMNS, "Change value for BANK_MAX_COLUMNS to %d", numValues);

	rlStatValue inputValues[BANK_MAX_COLUMNS];
	unsigned    inputIds[BANK_MAX_COLUMNS];

	if (m_BankUpdateColumns > 0 && numValues > m_BankUpdateColumns)
	{
		numValues = m_BankUpdateColumns;
	}

	for (unsigned j=0; j<numValues; j++)
	{
		const LeaderboardInput* input = GAME_CONFIG_PARSER.GetInputById( lbConf.m_columns[j].m_aggregation.m_gameInput.m_inputId );
		if (!gnetVerify(input))
			continue;

		inputIds[j] = input->m_id;
		switch(input->m_dataType)
		{
		case InputDataType_int:
		case InputDataType_long:
			inputValues[j].Int64Val = m_Valuei;
			break;

		case InputDataType_double:
			inputValues[j].DoubleVal = m_Valuef;
			break;

		default:
			gnetAssertf(0, "Invalid stat Index=\"%d\", FieldId=\"%s:%d\", Type=\"%d\"", j, input->GetName(), input->m_id, input->m_dataType);
		}
	}

	rlLeaderboard2GroupSelector groupSelector;
	groupSelector.m_NumGroups = s_BankGroupSelector.m_NumGroups;
	for (int i=0; i<groupSelector.m_NumGroups; i++)
	{
		formatf(groupSelector.m_Group[i].m_Category, COUNTOF(groupSelector.m_Group[i].m_Category), s_BankGroupSelector.m_Group[i].m_Category);
		formatf(groupSelector.m_Group[i].m_Id, COUNTOF(groupSelector.m_Group[i].m_Id), s_BankGroupSelector.m_Group[i].m_Id);
	}

	netLeaderboardWrite* lb = AddLeaderboard(lbConf.m_id, groupSelector, s_BankClanId);
	if (gnetVerify(lb))
	{
		for(int column=0; column<numValues; column++)
		{
			lb->AddColumn(inputValues[column], inputIds[column]);
		}
	}
}

void
CNetworkWriteLeaderboards::Bank_FinishUploadStatistics()
{
	gnetVerify(Flush());
}

#endif //__BANK

CNetworkLeaderboardMgr::CNetworkLeaderboardMgr() 
:m_pAllocator(NULL)
,m_bInitialized(false)
{

}

CNetworkLeaderboardMgr::~CNetworkLeaderboardMgr()
{
	if(m_bInitialized)
	{
		Shutdown();		
	}
}

void CNetworkLeaderboardMgr::Init(sysMemAllocator* allocator)
{
	gnetAssert(!m_bInitialized && allocator);
	m_pAllocator = allocator;
	m_LeaderboardReadMgr.Init(m_pAllocator);
	m_LeaderboardWriteMgr.Init(m_pAllocator);
	m_bInitialized = true;
}

void CNetworkLeaderboardMgr::Shutdown()
{
	gnetAssert(m_bInitialized);
	gnetAssertf(!m_LeaderboardReadMgr.Pending(), "Session Leaderboard reader has pending operations.");
	m_LeaderboardReadMgr.Shutdown();

	gnetAssertf(!m_LeaderboardWriteMgr.Pending(), "Session Leaderboard writer has pending operations.");
	m_LeaderboardWriteMgr.Shutdown();
	m_bInitialized = false;
}

void CNetworkLeaderboardMgr::Update()
{
	m_LeaderboardWriteMgr.Update();
	m_LeaderboardReadMgr.Update();

#if __BANK
	Bank_Update();
#endif
}

#if __BANK
void CNetworkLeaderboardMgr::Bank_Update()
{
	m_LeaderboardReadMgr.UpdateWidgets();
}

void CNetworkLeaderboardMgr::InitWidgets( bkBank* pBank )
{
	m_LeaderboardReadMgr.InitWidgets(pBank);
	m_LeaderboardWriteMgr.InitWidgets(pBank);
}

#endif

//#undef __net_channel
//#define __net_channel net

//eof 
