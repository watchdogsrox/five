// 
// events_durango.h 
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved. 
// 

#ifndef EVENTS_DURANGO_H
#define EVENTS_DURANGO_H

#if RSG_DURANGO

#include "atl/functor.h"
#include "rline/rlworker.h"
//#include "snet/session.h"

#include "Network/Live/NetworkRichPresence.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsUtils.h"
#include "Stats/StatsTypes.h"
#include "Stats/stats_channel.h"

#include "vectormath/vec3v.h"
#include <wtypes.h>

using namespace rage;

namespace rage
{
	class snSession;
}

class events_durango
{
public:

	enum MatchType
	{
		MatchType_Competitive = 1,
		MatchType_LocalCoop,
		MatchType_OnlineRanked,
		MatchType_OnlinePrivate,
		MatchType_PartyMatch,
	};

	enum ExitStatus
	{
		ExitStatus_Clean,
		ExitStatus_Error,
	};

	static bool Init();
	static bool Shutdown();
	
	static bool WriteEvent_EnemyDefeatedSP(int totalKills);
	static bool WriteEvent_HeldUpShops(int heldUpShops);
	static bool WriteEvent_HordeWavesSurvived(int hordeWavesSurvived);
	static bool WriteEvent_MoneyTotalSpent(int character, int totalMoneySpent);
	static bool WriteEvent_MultiplayerRoundStart(const snSession* pSession, int nGameplayModeId, MatchType kMatchType);
	static bool WriteEvent_MultiplayerRoundEnd(const snSession* pSession, int nGameplayModeId, MatchType kMatchType, unsigned nTimeInMs, ExitStatus kExitStatus);
	static bool WriteEvent_PlatinumAwards(int numPlatinumAwards);
	static bool WriteEvent_PlayerDefeatedSP(int totalKills);
	static bool WriteEvent_PlayerKillsMP(int totalKills);
	static bool WriteEvent_PlayerSessionEnd();
	static bool WriteEvent_PlayerSessionPause();
	static bool WriteEvent_PlayerSessionResume();
	static bool WriteEvent_PlayerSessionStart();
	static bool WriteEvent_PlayerRespawnedSP();
	static bool WriteEvent_TotalCustomRacesWon(int totalRacesWon);

	// Hero Events
	static bool WriteEvent_GameProgressSP(float gameProgress);
	static bool WriteEvent_PlayingTime(u64 nTimeInMs);
	static bool WriteEvent_CashEarnedSP(s64 cashEarned);
	static bool WriteEvent_StolenCars(unsigned stolenCars);
	static bool WriteEvent_StarsAttained(unsigned starsAttained);
	static bool WriteEvent_MultiplayerPlaytime(u64 nTimeInMs);
	static bool WriteEvent_CashEarnedMP(s64 cashEarned);
	static bool WriteEvent_KillDeathRatio(float killDeathRatio);
	static bool WriteEvent_ArchEnemy(u64 enemy);
	static bool WriteEvent_HighestRankMP(int highestRank);

	// Achievements and Presence
	static bool WriteAchievementEvent(int achId, int localGamerIndex, int achievementProgress = IAchievements::FULL_UNLOCK_PROGRESS);
	static bool WritePresenceContextEvent(const rlGamerInfo& gamerInfo, const richPresenceFieldStat* stats, int numStats);
	static int  GetAchievementProgress(int achId);

	// Presence - Active (Twitch)
	static void WriteActiveContext(const rlGamerInfo& gamerInfo, const richPresenceFieldStat* stats, int numStats);

	// Stat Write callbacks
	static void OnStatWrite(StatId& statKey, void* pData, const unsigned sizeofData, const u32 flags);

	// Stat Write callbacks before a savegame
	static void OnPreSavegame(StatId& statKey, void* pData, const unsigned sizeofData);

	// Session Shutdown callback
	static void ClearAchievementProgress();

	static PCWSTR GetUserId(int localGamerIndex);

private:

	static LPCGUID GetPlayerSessionId();
	static PCWSTR GetMultiplayerCorrelationId();
	static int GetGameplayModeId();
	static int GetSectionId();
	static int GetDifficultyLevelId();
	static int GetPlayerRoleId();
	static int GetPlayerWeaponId();
	static int GetEnemyRoleId();
	static int GetKillTypeId();
	static Vec3V_Out GetPlayerLocation();
	static int GetEnemyWeaponId();
	static int GetMatchTypeId();
	static int GetMatchTimeInSeconds();
	static int GetExitStatusId();

	static ULONG NoOpAch(PCWSTR,LPCGUID,int) { gnetWarning("NoOpAch Event called - probably a bad index"); return 0; }
	static ULONG NoOpPresence(PCWSTR,LPCGUID,PCWSTR) { return 0; }

	static bool WriteEvent_MultiplayerRoundStart(PCWSTR correlationId, const GUID& playerSessionId, int nGameplayModeId, MatchType kMatchType);
	static bool WriteEvent_MultiplayerRoundEnd(PCWSTR correlationId, const GUID& playerSessionId, int nGameplayModeId, MatchType kMatchType, unsigned nTimeInMs, ExitStatus kExitStatus);

	struct EventRegistrationWorker : public rlWorker
	{
		EventRegistrationWorker();
		bool Start(const int cpuAffinity);
		bool Stop();

	private:
		bool Start(const char*, const unsigned , const int )
		{
			FastAssert(false);
			return false;
		}

		virtual void Perform();

		ULONG result;
	};

public:
	static const int XB1_ACHIEVEMENT_EVENTS = 78;
	static const int XB1_PRESENCE_CONTEXT_EVENTS = 11;

private:
	static Functor3Ret<ULONG, PCWSTR,LPCGUID,int> m_fAchievementEvents[XB1_ACHIEVEMENT_EVENTS];
	static Functor3Ret<ULONG, PCWSTR,LPCGUID,int> m_fAchievementEventsJA[XB1_ACHIEVEMENT_EVENTS];
	static int m_iAchievementThresholds[XB1_ACHIEVEMENT_EVENTS];

	static Functor3Ret<ULONG, PCWSTR, LPCGUID, PCWSTR> m_fPresenceContextEvents[XB1_PRESENCE_CONTEXT_EVENTS];
	static Functor3Ret<ULONG, PCWSTR, LPCGUID, PCWSTR> m_fPresenceContextEventsJA[XB1_PRESENCE_CONTEXT_EVENTS];

	// Active Presence Contexts for TwitchTV Xbox One App
	static Functor3Ret<ULONG, PCWSTR, LPCGUID, PCWSTR> m_fActiveContext[XB1_PRESENCE_CONTEXT_EVENTS];

	static EventRegistrationWorker sm_Worker;
	static bool sm_Initialized;

	// Hero Stats
	static u64 m_LastArchenemy;

	// Track achievement progress in code to prevent duplicate writes and spam
	static int sm_iPreviousAchievementProgress[XB1_ACHIEVEMENT_EVENTS];

};

#endif  //RSG_DURANGO

#endif  //RLXBL_EVENTS_H

