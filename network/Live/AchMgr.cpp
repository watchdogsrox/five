//
// AchMgr.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

//rage
#include "fwsys/gameskeleton.h"

//game
#include "frontend/ProfileSettings.h"
#include "Network/Live/AchMgr.h"
#include "Network/Live/LiveManager.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/xlast/Fuzzy.schema.h"
#include "Network/Live/Events_durango.h"

#if __WIN32PC
#include "rline/scachievements/rlscachievements.h"
#endif // __WIN32PC

NETWORK_OPTIMISATIONS()

CompileTimeAssert((int) MAX_ACHIEVEMENTS >= (int) player_schema::Achievements::COUNT);

RAGE_DEFINE_SUBCHANNEL(net, achievement, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_achievement

#if __STEAM_BUILD
int AchMgr::sm_iPreviousAchievementProgress[player_schema::Achievements::COUNT] = {0};
bool AchMgr::sm_bFirstAchievementReceiveCB = true;
bool AchMgr::sm_bSyncSteamAchievements = false;

PARAM(resetSteamAchievements, "Resets your achievement progress on Steam");

#endif

// --------------- AchSocialClubReader

bool AchSocialClubReader::Read( )
{
	bool result = false;

	if (!gnetVerify(NetworkInterface::IsLocalPlayerOnline()))
		return result;

	if (!gnetVerify(NetworkInterface::HasValidRosCredentials()))
		return result;

	m_status.Reset();

	m_count = 0;
	m_total = 0;

	result = rlScAchievements::GetAchievementsByGamer
								(NetworkInterface::GetLocalGamerIndex()
								,NetworkInterface::GetLocalGamerHandle()
								,m_achsInfo
						#if RSG_NP
								,player_schema::Achievements::PLATINUM
						#else
								,player_schema::Achievements::ACH00
						#endif // RSG_NP
								,MAX_ACHIEVEMENTS
								,&m_count
								,&m_total
								,&m_status);

	gnetAssertf(result, "Failed to start Social Club achievements read for local gamer.");

	return result;
}


// --------------- AchMgr

AchMgr::~AchMgr()
{
	Shutdown(SHUTDOWN_CORE);
}

void
AchMgr::Init()
{
#if __ASSERT
	//Make sure the achievement ids start at 1 and are sequential.
	//This is necessary in order to implement the achievement bitmask
	//returned by GetAchievementsBits().
	int id = 1; // 0 is player_schema::Achievements::PLATINUM
#if RSG_NP
	const int count = player_schema::Achievements::COUNT-1;
#else
	const int count = player_schema::Achievements::COUNT;
#endif
	int achIds[count];
	for(int i=0; i<count; ++i,++id)
	{
		achIds[i] = player_schema::Achievements::IndexToId(id);
		gnetAssert(achIds[i] > 0);
	}

	std::sort(&achIds[0], &achIds[count]);

	for(int i=0; i<count; ++i)
	{
		gnetAssertf(i+1 == achIds[i], "i+1 '%d' == '%d' achIds[i]", i+1, achIds[i]);
	}
#endif  //__ASSERT

	gnetAssert(!m_PendingAchievementOps);
	gnetAssert(0 == m_NumAchRead);
	gnetAssert(0 == m_NumAchPassed);

	ClearAchievements();

	m_NeedToRefreshAchievements = true;
	m_AchievementsPassedDirty = false;

#if __STEAM_BUILD
	if (SteamUtils() != NULL)
	{
		m_SteamAppID =  SteamUtils()->GetAppID();
	}
	else
	{
		m_SteamAppID = 0;
	}
	m_pSteamUserStats = SteamUserStats();
	m_pSteamUser = SteamUser();

	if (m_pSteamUser && m_pSteamUserStats)
	{
		m_bSteamInitialized = true;

		if (m_pSteamUser->BLoggedOn())
		{
			m_pSteamUserStats->RequestCurrentStats();
		}
	}
#endif
}

void
AchMgr::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		while(m_PendingAchievementOps)
		{
			UpdateAchievements();
		}

		gnetAssert(!m_PendingAchievementOps);

		ClearAchievements();

		m_NumAchRead = 0;

		m_NeedToRefreshAchievements = false;
	}
	else if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		// wait until all pending operations have finished
		while(m_PendingAchievementOps)
		{
			UpdateAchievements();
			sysIpcSleep(1);
		}
		gnetAssert(!m_PendingAchievementOps);
	}
}

void
AchMgr::Update()
{
	if(m_PendingAchievementOps)
	{
		UpdateAchievements();
	}

	if(m_NeedToRefreshAchievements && !m_PendingAchievementOps)
	{
		RefreshAchievementsInfo();
	}

#if __STEAM_BUILD
	if (!m_NeedToRefreshAchievements && !m_PendingAchievementOps)
	{
		if (sm_bSyncSteamAchievements && CLiveManager::IsOnline())
		{
			SyncSteamAchievements();
		}
	}
#endif
}

#if __STEAM_BUILD
void AchMgr::OnAchievementStored(UserAchievementStored_t* pCallback)
{
	if (pCallback == NULL)
		return;

	if (m_SteamAppID == pCallback->m_nGameID)
	{
		if (pCallback->m_nMaxProgress == 0)
		{
			gnetDisplay("Steam Achievement Unlocked: %s", pCallback->m_rgchAchievementName);
		}
		else
		{
			gnetDisplay("Steam Achievement %s: (%d, %d)", pCallback->m_rgchAchievementName, pCallback->m_nCurProgress, pCallback->m_nMaxProgress);
		}
	}
}

void AchMgr::OnStatsStored(UserStatsStored_t* pCallback)
{
	if (pCallback == NULL)
		return;

	if (m_SteamAppID == pCallback->m_nGameID)
	{
		gnetDisplay("Steam Stat Stored, Result: %d", pCallback->m_eResult);
	}
}

void AchMgr::OnAchievementsReceived( UserStatsReceived_t * pCallback)
{
	if (sm_bFirstAchievementReceiveCB)
	{
		gnetDisplay("Initial Steam UserStatsReceived_t callback");
		ClearAchievementProgress();
		sm_bFirstAchievementReceiveCB = false;
	}

	if (PARAM_resetSteamAchievements.Get() && m_pSteamUserStats)
	{
		// iterate through all achievements
		for (int i = 1; i <= player_schema::Achievements::COUNT; i++)
		{
			int achId = player_schema::Achievements::IndexToId(i);
			const char* achievementName = player_schema::Achievements::IdToChar(achId);
			bool achieved = false;
			if (m_pSteamUserStats->GetAchievement(achievementName, &achieved) && achieved)
			{
				gnetDisplay("Clearing achievement '%s'", achievementName);
				m_pSteamUserStats->ClearAchievement(achievementName);
			}
		}

		m_pSteamUserStats->StoreStats();
	}
	// This is in an else case, because resetting steam achievemnets means we're immediately fire of all of these. I'd rather
	// require disablign the command line to test this functionality.
	else if (m_SteamAppID == pCallback->m_nGameID)
	{
		if (k_EResultOK == pCallback->m_eResult)
		{
			sm_bSyncSteamAchievements = true;
		}
	}
}
#endif // __STEAM_BUILD

bool
AchMgr::AwardAchievement(const int id)
{
#if __STEAM_BUILD
	if (!HasSteamAchievementBeenPassed(id) && AssertVerify(id > 0))
	{
		SetSteamAchievementPassed(id);
	}
#endif // __STEAM_BUILD

	bool success = HasAchievementBeenPassed(id);

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();

	if(!success && AssertVerify(id > 0) && gi && AssertVerify(gi->IsSignedIn()))
	{
		SetAchievementPassed(id);
		CNetworkTelemetry::EarnedAchievement(id);

#if RSG_DURANGO || RSG_PC
		{
			AchOp* ach = rage_new AchOp(AchOp::OP_WRITE);

			if(AssertVerify(NULL != ach))
			{
				if(AssertVerify(ach->m_Ach.Init(NETWORK_CPU_AFFINITY)) && AssertVerify(ach->m_Ach.Write(*gi, id, &ach->m_Status)))
				{
					ach->m_Next = m_PendingAchievementOps;
					m_PendingAchievementOps = ach;
					success = true;
				}
				else
				{
					delete ach;
				}
			}
		}
#endif 
	}

#if RSG_NP
	if (m_bOwnedSave && m_bTrophiesEnabled && !HasTrophyBeenPassed(id))
	{
		AchOp* ach = rage_new AchOp(AchOp::OP_WRITE);

		if(AssertVerify(NULL != ach))
		{
			if(AssertVerify(ach->m_Ach.Init(NETWORK_CPU_AFFINITY))
				&& AssertVerify(ach->m_Ach.Write(*gi, id, &ach->m_Status)))
			{
				ach->m_Next = m_PendingAchievementOps;
				m_PendingAchievementOps = ach;
				success = true;

				m_ConfirmedUnlockedTrophies[id] = true;
			}
			else
			{
				delete ach;
			}
		}
	}
#endif  // RSG_NP

	gnetDebug1("AchMgr::AwardAchievement - id='%d', result='%s'", id, success ? "True":"False");

	return success;
}

bool 
AchMgr::SetAchievementProgress(const int DURANGO_ONLY(id) STEAMBUILD_ONLY(id), const int DURANGO_ONLY(progress) STEAMBUILD_ONLY(progress))
{
#if RSG_DURANGO
	return events_durango::WriteAchievementEvent(id, NetworkInterface::GetLocalGamerIndex(), progress);
#elif __STEAM_BUILD
	rtry
	{
		rverify(id >= 0, catchall, );
		rverify(id < player_schema::Achievements::COUNT, catchall, );
		rverify(SteamUserStats(), catchall, );

		char statName[256] = {0};
		formatf(statName, "Stat_%s", player_schema::Achievements::IdToChar(id));
		gnetDebug1("SetAchievementProgress %s (%d) - %d", statName, id, progress);

		rcheck(sm_iPreviousAchievementProgress[id] != progress, catchall, gnetError("Trying to write achievement progress %d for ID %d despite no change", progress, id));
		sm_iPreviousAchievementProgress[id] = progress;

		rverify(SteamUserStats()->SetStat(statName, progress), catchall, );
		rverify(SteamUserStats()->StoreStats(), catchall, );
		return true;
	}
	rcatchall
	{
		return false;
	}
#else
	return false;
#endif
}


int 
AchMgr::GetAchievementProgress(const int DURANGO_ONLY(id) STEAMBUILD_ONLY(id))
{
#if RSG_DURANGO
	return events_durango::GetAchievementProgress(id);
#elif __STEAM_BUILD
	if (gnetVerify(id >= 0 && id < COUNTOF(sm_iPreviousAchievementProgress)))
	{
		return sm_iPreviousAchievementProgress[id];
	}
	return 0;
#else
	return 0;
#endif
}

void AchMgr::ClearAchievementProgress()
{
	gnetDisplay("ClearAchievementProgress");

#if RSG_DURANGO
	events_durango::ClearAchievementProgress();
#elif __STEAM_BUILD
	for (int i = 1; i < player_schema::Achievements::COUNT; i++)
	{
		char statName[256] = {0};
		formatf(statName, "Stat_%s", player_schema::Achievements::IdToChar(i));
		if (SteamUserStats() && SteamUserStats()->GetStat(statName, &sm_iPreviousAchievementProgress[i]))
		{
			gnetDisplay("Read for %s (%d), progress: %d", statName, i, sm_iPreviousAchievementProgress[i]);
		}
	}
#endif
}

bool
AchMgr::HasAchievementBeenPassed(const int id)
{

#if RSG_NP
	gnetAssertf(id >= 0, "Invalid id='%d', must be >= 0", id);
#else
	gnetAssertf(id > 0, "Invalid id='%d', must be > 0", id);
#endif //RSG_NP

	bool passed = false;

	for(int i = 0; i < (int) m_NumAchPassed; ++i)
	{
		if(id == m_AchievementsPassed[i])
		{
			passed = true;
			break;
		}
	}

	return passed;
}

#if __STEAM_BUILD
bool AchMgr::HasSteamAchievementBeenPassed(const int id)
{
	bool passed = false;

	gnetAssert(id > 0);
	gnetAssert(m_pSteamUserStats);

	if (m_pSteamUserStats)
	{
		const char* achievementName = player_schema::Achievements::IdToChar(id);
		gnetAssertf(achievementName, "Could not find a proper achievement name for Steam. Achievement ID: %d", id);

		if (achievementName)
			m_pSteamUserStats->GetAchievement(achievementName, &passed);
	}

	return passed;
}

void AchMgr::SyncSteamAchievements()
{
	// iterate through all achievements
	for (int i = 1; i <= player_schema::Achievements::COUNT; i++)
	{
		// If they are owned on RGSC but not on Steam...
		int achId = player_schema::Achievements::IndexToId(i);
		if (achId > 0 && HasAchievementBeenPassed(achId) && !HasSteamAchievementBeenPassed(achId))
		{
			gnetDisplay("Awarding steam achievement index %d, ID: %d, because it was unlocked on SC but not on Steam.", i, achId);
			// Award on Steam!
			SetSteamAchievementPassed(achId);
		}
	}

	sm_bSyncSteamAchievements = false;
}
#endif

#if RSG_NP
bool
AchMgr::HasTrophyBeenPassed(const int achievementId)
{
	gnetAssert(achievementId > 0);
	return m_ConfirmedUnlockedTrophies[achievementId];
}
#endif // RSG_NP

unsigned
AchMgr::GetAchievementsBits(u8* bits, const unsigned maxBits)
{
	gnetAssert(maxBits >= MAX_ACHIEVEMENTS);
	CompileTimeAssert(COUNTOF(m_AchievementsPassed) == MAX_ACHIEVEMENTS);

	sysMemSet(bits, 0, (maxBits + 7) / 8);

	for(int i = 0; i < (int) m_NumAchPassed; ++i)
	{
		const int bitIdx = m_AchievementsPassed[i] - 1;
		gnetAssert(bitIdx >= 0);

		if(i < (int) maxBits && AssertVerify(bitIdx >= 0))
		{
			bits[bitIdx/8] |= u8(1 << (bitIdx % 8));
		}
	}

	return MAX_ACHIEVEMENTS;
}

void
AchMgr::SetAchievementsBits(const u8* bits, const unsigned numBits)
{
	gnetAssert(numBits <= MAX_ACHIEVEMENTS);
	CompileTimeAssert(COUNTOF(m_AchievementsPassed) == MAX_ACHIEVEMENTS);

	ClearAchievements();

	for(int i = 0; i < (int) numBits; ++i)
	{
		if(bits[i/8] & u8(1 << (i % 8)))
		{
			SetAchievementPassed(i + 1);
		}
	}
}

#if RSG_NP
void AchMgr::SetTrophiesEnabled(bool bEnabled)
{
	m_bTrophiesEnabled = bEnabled;
}

bool AchMgr::GetTrophiesEnabled()
{
	return m_bTrophiesEnabled;
}

void AchMgr::SetOwnedSave(bool owned)
{
	m_bOwnedSave = owned;
}

bool AchMgr::GetOwnedSave()
{
	return m_bOwnedSave;
}
#endif // RSG_NP

void
AchMgr::RefreshAchievementsInfo()
{
	m_NeedToRefreshAchievements = false;

	const rlGamerInfo* gi = NetworkInterface::GetActiveGamerInfo();

	if(gi && AssertVerify(gi->IsSignedIn()))
	{
		AchOp* ach = rage_new AchOp(AchOp::OP_READ);

		if(AssertVerify(NULL != ach))
		{
			if(AssertVerify(ach->m_Ach.Init(NETWORK_CPU_AFFINITY))
				&& AssertVerify(ach->m_Ach.Read(*gi, *gi, &m_AchievementsInfo[0], COUNTOF(m_AchievementsInfo), &m_NumAchRead, &ach->m_Status)))
			{
				ach->m_Next = m_PendingAchievementOps;
				m_PendingAchievementOps = ach;
			}
			else
			{
				delete ach;
			}
		}
	}
}

void
AchMgr::SetAchievementPassed(const int id)
{
	gnetAssert(id > 0);

	if(!HasAchievementBeenPassed(id) && AssertVerify(m_NumAchPassed < MAX_ACHIEVEMENTS))
	{
		m_AchievementsPassedDirty = true;
		m_AchievementsPassed[m_NumAchPassed++] = id;
	}
}

#if __STEAM_BUILD
void AchMgr::SetSteamAchievementPassed(const int id)
{
	gnetAssertf(m_pSteamUserStats, "Could not get user stats for Steam user");
	gnetAssertf(m_bSteamInitialized, "Steam has not been initialized before trying to set an achievement");

	if (m_pSteamUserStats && m_bSteamInitialized)
	{
		const char* achievementName = player_schema::Achievements::IdToChar(id);
		m_pSteamUserStats->SetAchievement(achievementName);
		m_pSteamUserStats->StoreStats();
	}
}
#endif

void
AchMgr::ClearAchievements()
{
	sysMemSet(m_AchievementsPassed, 0xFF, sizeof(m_AchievementsPassed));
#if RSG_NP
	sysMemSet(m_ConfirmedUnlockedTrophies, 0x0, sizeof(m_ConfirmedUnlockedTrophies));
#endif
	m_NumAchPassed = 0;
}

void
AchMgr::UpdateAchievements()
{
	AchOp* n  = m_PendingAchievementOps;
	AchOp** p = &m_PendingAchievementOps;

	while(n)
	{
		if(!n->m_Status.Pending())
		{
			if(AchOp::OP_READ == n->m_Type)
			{
				for (int i=0; i<m_NumAchRead; i++)
				{
					if(!m_AchievementsInfo[i].IsAchieved()){continue;}

#if RSG_NP
					if (m_AchievementsInfo[i].GetId() != player_schema::Achievements::PLATINUM)
#endif
					SetAchievementPassed(m_AchievementsInfo[i].GetId());

#if RSG_NP
					m_ConfirmedUnlockedTrophies[i] = true;
#endif
				}

				BANK_ONLY(CLiveManager::LiveBank_RefreshAchInfo();)
			}

			*p = n->m_Next;
			delete n;
			n = *p;
		}
		else
		{
			n->m_Ach.Update();
			p = &n->m_Next;
			n = n->m_Next;
		}
	}
}

void 
AchMgr::OnRosEvent(const rlRosEvent& evt)
{
	gnetDebug1("OnRosEvent :: %s received", evt.GetAutoIdNameFromId(evt.GetId()));

	switch(evt.GetId())
	{
	case RLROS_EVENT_CAN_READ_ACHIEVEMENTS:
		{
			const rlRosEventCanReadAchievements& statusEvent = static_cast<const rlRosEventCanReadAchievements&>(evt);
			gnetDebug1("RLROS_EVENT_CAN_READ_ACHIEVEMENTS :: '%s'", statusEvent.m_Success ? "Success" : "Failed");
			if (statusEvent.m_Success)
			{
#if RSG_ORBIS || RSG_DURANGO
				gnetVerify( m_socialclubachs.Read( ) );
#endif
			}
		}
		break;
	default:
		break;
	}
}

// eof

