//
// AchMgr.h
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#ifndef ACHMGR_H
#define ACHMGR_H

//rage
#include "rline/rlachievement.h"
#include "rline/scachievements/rlscachievements.h"

// game
#include "Network/xlast/Fuzzy.schema.h"

// steam
#if __STEAM_BUILD
#pragma warning(disable: 4265)
#include "../../3rdParty/Steam/public/steam/steam_api.h"
#pragma warning(error: 4265)
#endif

enum
{
	MAX_ACHIEVEMENTS = 80 // This must be at least as big as Achievements::COUNT
};

// PURPOSE 
//   Read Social Club achievements.
struct AchSocialClubReader
{
public:
	AchSocialClubReader() : m_count(0), m_total(0) {}
	bool Read();

public:
	u32                  m_count;
	u32                  m_total;
	netStatus            m_status;
	rlScAchievementInfo  m_achsInfo[MAX_ACHIEVEMENTS];
};

//PURPOSE
// Manage Achievements.
class AchMgr
{
	friend class CLiveManager;

private:

	struct AchOp
	{
		enum OpType
		{
			OP_READ,
			OP_WRITE
		};

		explicit AchOp(const OpType type)
			: m_Type(type)
			, m_Next(0)
		{
		}

		OpType m_Type;
		rlAchievement m_Ach;
		netStatus m_Status;
		AchOp* m_Next;
	};

private:
	//Read Social Club Achievements status
	AchSocialClubReader  m_socialclubachs;

	//List of pending achievement operations
	AchOp* m_PendingAchievementOps;

	//Holds all achievements information
	rlAchievementInfo m_AchievementsInfo[MAX_ACHIEVEMENTS];

	//Holds the number of achievements read after a read command
	int m_NumAchRead;

	// Ids of passed achievements
	int m_AchievementsPassed[MAX_ACHIEVEMENTS];

	// Number of achievements passed
	unsigned m_NumAchPassed;

	//Achievement Passed have changed
	bool m_AchievementsPassedDirty;

#if RSG_NP
	bool m_ConfirmedUnlockedTrophies[MAX_ACHIEVEMENTS];
	bool m_bTrophiesEnabled;
	bool m_bOwnedSave;
#endif // RSG_NP

	//Set to TRUE when we need to refresh achievement information.
	bool m_NeedToRefreshAchievements;

#if __STEAM_BUILD
	uint64	m_SteamAppID;
	bool	m_bSteamInitialized;

	ISteamUser*			m_pSteamUser;
	ISteamUserStats*	m_pSteamUserStats;
#endif

public:
	~AchMgr();

	//PURPOSE
	// Awards an achievement to the player. The player on which
	// this is invoked must be local.
	//NOTES
	// This function is implemented for the benefit of a script
	// command that updates an achievement for the local player.
	bool AwardAchievement(const int id);

	//PURPOSE
	//	Get/Sets achievement progress for a given achievement (XB1 only)
	bool SetAchievementProgress(const int id, const int progress);
	int GetAchievementProgress(const int id);
	void ClearAchievementProgress();

	//PURPOSE
	// Returns true if the achievement has been passed.
	bool HasAchievementBeenPassed(const int id);

#if RSG_NP
	//PURPOSE
	// Returns true if the achievement has been passed.
	bool HasTrophyBeenPassed(const int achievementId);
#endif // RSG_NP

	//PURPOSE
	// Retrieves an array of bits with each bit corresponding to the index
	// of an achievement.
	//RETURNS
	// Number of achievements.
	//NOTES
	// Bit zero of the bit array corresponds to bit zero of the first byte
	// in the bits array. Bit 8 corresponds to bit zero of the second
	// byte in the bits array.
	unsigned GetAchievementsBits(u8* bits, const unsigned maxBits);

	//PURPOSE
	// Sets the achievements bit array. A side effect is to
	// also set the achievements considered "passed" (i.e.
	// HasAchievementBeenPassed() will return true for bits set
	// to 1 in the bit array).
	void SetAchievementsBits(const u8* bits, const unsigned numBits);

	//PURPOSE
	// Returns true if the achievements passed have been changed since the last
	// Telemetry upload.
	bool GetAchievementsPassedDirty() const { return m_AchievementsPassedDirty; }

	//PURPOSE
	// Clears the flag that sets the achievements passed as dirty.
	void ClearAchievementsPassedDirtyFlag() { m_AchievementsPassedDirty = false; }

#if RSG_NP
	// PURPOSE
	//	Set enabled state of trophy support on PS3
	void SetTrophiesEnabled(bool enabled);

	// PURPOSE
	//	Get enabled state of trophy support on PS3
	bool GetTrophiesEnabled();

	// PURPOSE
	//	Set ownership state of save
	void SetOwnedSave(bool owned);

	// PURPOSE
	//	Get ownership state of save
	bool GetOwnedSave();
#endif

#if __STEAM_BUILD
	//PURPOSE
	//  Returns true if the achievement on Steam has been passed.
	bool HasSteamAchievementBeenPassed(const int id);
	void SyncSteamAchievements();
	STEAM_CALLBACK( AchMgr, OnAchievementStored, UserAchievementStored_t, m_CallbackAchievementStored );
	STEAM_CALLBACK( AchMgr, OnAchievementsReceived, UserStatsReceived_t, m_CallbackStatsReceived );
	STEAM_CALLBACK( AchMgr, OnStatsStored, UserStatsStored_t, m_CallbackStatStored);
	static int sm_iPreviousAchievementProgress[player_schema::Achievements::COUNT];
	static bool sm_bFirstAchievementReceiveCB;
	static bool sm_bSyncSteamAchievements;
#endif

	//PURPOSE
	// Clears the record of earned achievements.
	void ClearAchievements();

	//PURPOSE
	// Callback for Ros events 
	void OnRosEvent(const rlRosEvent& evt);

	//PURPOSE
	// Accessor to Social Club Achievements.
	const AchSocialClubReader& GetSocialClubAchs() {return m_socialclubachs;};

	//PURPOSE
	// Accessor to Platform Achievements.
	const rlAchievementInfo* GetAchievementsInfo() const {return m_AchievementsInfo;}

	//PURPOSE
	// Return the number of achievements that we have read from the platform.
	u32 GetNumberOfAchievements() const {return m_NumAchRead;}

	// PURPOSE
	// Returns true if the AchMgr needs to refresh achievements
	bool NeedToRefreshAchievements() { return m_NeedToRefreshAchievements; }

	// PURPOSE
	//	Returns true if the AchMgr has pending achievement operations
	bool HasPendingOperations() { return m_PendingAchievementOps != NULL; }

private:
	AchMgr() : m_PendingAchievementOps(NULL), m_NumAchRead(0), m_NumAchPassed(0), m_NeedToRefreshAchievements(false)
#if __STEAM_BUILD
		, m_bSteamInitialized(false)
		, m_SteamAppID(0)
		, m_pSteamUser(NULL)
		, m_pSteamUserStats(NULL)
		, m_CallbackAchievementStored(this, &AchMgr::OnAchievementStored)
		, m_CallbackStatsReceived(this, &AchMgr::OnAchievementsReceived)
		, m_CallbackStatStored(this, &AchMgr::OnStatsStored)
#endif
	{
	}

	void Init();
	void Shutdown(unsigned shutdownMode);
	void Update();

	//PURPOSE
	// Updates achievements that have pending operations.
	void UpdateAchievements();

	//PURPOSE
	// Updates achievements info.
	//NOTES
	//  It's used in the beginning of the game to get the current achievements
	//  and every time the achievements are updated.
	void RefreshAchievementsInfo();

	//PURPOSE
	// Awards an achievement.
	void SetAchievementPassed(const int id);

#if __STEAM_BUILD
	//PURPOSE
	//  Awards an achievement on Steam.
	void SetSteamAchievementPassed(const int id);
#endif
};


#endif // ACHMGR_H

// eof

