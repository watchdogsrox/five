/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SaveMigrationDefs.h
// PURPOSE : Common definitions for save migration UI types.
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef SAVE_MIGRATION_DEFS
#define SAVE_MIGRATION_DEFS

#include "Stats/StatsUtils.h"
#include "rline/savemigration/rlsavemigrationcommon.h"

namespace SaveMigrationDefs
{
	struct StoryModeSaveGameEntry
	{
	public:
		StoryModeSaveGameEntry() { }

		StoryModeSaveGameEntry(const char *playerDisplayName, const char* accountName, const char* recentMissionName, const char* totalCompletion, const char* uploadTimestamp, u64 uploadPosixTime) :
			m_playerDisplayName(playerDisplayName),
			m_accountName(accountName),
			m_recentMissionName(recentMissionName),
			m_totalCompletion(totalCompletion),
			m_uploadTimestamp(uploadTimestamp),
			m_uploadPosixTime(uploadPosixTime)
		{
		}

		const char *GetPlayerDisplayName() const { return m_playerDisplayName; }
		const char* GetAccountName() const { return m_accountName; }
		const char* GetRecentMissionName() const { return m_recentMissionName; }
		const char* GetTotalCompletion() const { return m_totalCompletion; }
		const char* GetUploadTimestamp() const { return m_uploadTimestamp; }
		u64 GetUploadPosixTime() const { return m_uploadPosixTime; }

	private:
		ConstString m_playerDisplayName;
		ConstString m_accountName;
		ConstString m_recentMissionName;
		ConstString m_totalCompletion;
		ConstString m_uploadTimestamp;
		u64 m_uploadPosixTime;
	};

	int SortByUploadTimeDescending(const StoryModeSaveGameEntry* const lhs, const StoryModeSaveGameEntry* const rhs)
	{
		if (uiVerifyf(lhs != nullptr && rhs != nullptr, "Encountered null element(s) when attempting to sort collection"))
		{
			const u64 c_uploadTimeLhs = lhs->GetUploadPosixTime();
			const u64 c_uploadTimeRhs = rhs->GetUploadPosixTime();

			// Greater-than to sort descending here
			if (c_uploadTimeLhs > c_uploadTimeRhs)
			{
				return -1;
			}
			else if (c_uploadTimeLhs == c_uploadTimeRhs)
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
		return 0;
	}

	StoryModeSaveGameEntry CreateFormattedSaveGameEntry(const char *playerDisplayName, int accountId, int missionId, float totalCompletion, u64 uploadTimestamp)
	{
		// TODO_UI SAVE_MIGRATION: Retrieve human-friendly account name
		char accountNameBuffer[32];
		formatf(accountNameBuffer, 32, "%d", accountId);

		const char* missionName = TheText.DoesTextLabelExist(missionId) ? TheText.Get(missionId) : "";

		char completionBuffer[16];
		formatf(completionBuffer, 16, "%.1f%%", totalCompletion);

		char uploadTimeBuffer[32];
		const time_t c_uploadTimeUTC = uploadTimestamp;
#if IS_GEN9_PLATFORM && RSG_PROSPERO
		tm dateData;
		const tm* const c_uploadTimeLocal = localtime_s(&c_uploadTimeUTC, &dateData);
#else
		const tm *const c_uploadTimeLocal = localtime(&c_uploadTimeUTC);
#endif
		strftime(uploadTimeBuffer, 31, "%m/%d/%Y %H:%M", c_uploadTimeLocal);

		StoryModeSaveGameEntry createdEntry(playerDisplayName, accountNameBuffer, missionName, completionBuffer, uploadTimeBuffer, uploadTimestamp);
		return createdEntry;
	}

	StoryModeSaveGameEntry CreateFormattedSaveGameEntry(const rlSaveMigrationRecordMetadata& record)
	{
		return CreateFormattedSaveGameEntry(record.m_PlayerDisplayName, record.m_AccountId, record.m_LastCompletedMissionId, record.m_CompletionPercentage, record.m_UploadPosixTime);
	}

	StoryModeSaveGameEntry CreateFormattedSaveGameEntry_Placeholder()
	{
		return CreateFormattedSaveGameEntry("placeholder", 1, -1282745799, 51.5f, 1629232580);
	}

	// MP SECTION
	const char* FindSaveMigrationMPAccountStat(const rlSaveMigrationMPAccount record, const char* statToFind)
	{
		const int c_statsCount = record.m_Stats.GetCount();
		for (int i = 0; i < c_statsCount; ++i)
		{
			if (strcmp(record.m_Stats[i].m_Name, statToFind) == 0)
			{
				return record.m_Stats[i].m_Value;
			}
		}
		return "\0";
	}

	struct MultiplayerCharacterEntry
	{
	public:
		MultiplayerCharacterEntry() { }

		MultiplayerCharacterEntry(const char* profileName, const char* rpEarned, const char* headshotTexture, int rank, int slotIndex, bool isActive) :
			m_profileName(profileName),
			m_rpEarned(rpEarned),
			m_headshotTexture(headshotTexture),
			m_rank(rank),
			m_slotIndex(slotIndex),
			m_isActive(isActive)
		{
		}

		const char* GetProfileName() const { return m_profileName; }
		const char* GetRpEarned() const { return m_rpEarned; }
		const char* GetCharacterHeadshotTexture() const { return m_headshotTexture; }
		int GetRank() const { return m_rank; }
		int GetSlotIndex() const { return m_rank; }
		bool GetIsActive() const { return m_isActive; }

	private:
		ConstString m_profileName;
		ConstString m_rpEarned;
		ConstString m_headshotTexture;
		int			m_rank;
		int			m_slotIndex;
		bool		m_isActive;
	};

	MultiplayerCharacterEntry CreateFormattedMPCharacterEntry(const char* profileName, int rpEarned, const char* headshotTexture, int slotIndex, bool isActive)
	{	
		// TODO_UI: replace this with a GET_FM_RANK_FROM_XP_VALUE equivalent 
		int rank = rpEarned;

		char rpEarnedBuffer[20];
		formatf(rpEarnedBuffer, 20, "% RP", rpEarned);

		MultiplayerCharacterEntry createdEntry(profileName, rpEarnedBuffer, headshotTexture, rank, slotIndex, isActive);
		return createdEntry;
	}

	//TODO_UI Populate this as stats become available
	MultiplayerCharacterEntry CreateFormattedMPCharacterEntry(const rlSaveMigrationMPAccount record, int characterSlot)
	{
		if (characterSlot == 0)
		{
			const char* xpEarnedValue = FindSaveMigrationMPAccountStat(record, "MP0_CHAR_XP_FM");
			int xpEarned = parUtils::StringToInt(xpEarnedValue);
		
			bool isCharacterActive = StringToBool(FindSaveMigrationMPAccountStat(record, "MP0_CHAR_ISACTIVE"));
			return CreateFormattedMPCharacterEntry("Character 1", xpEarned, "", characterSlot, isCharacterActive);
		}
		else
		{
			const char* xpEarnedValue = FindSaveMigrationMPAccountStat(record, "MP1_CHAR_XP_FM");
			int xpEarned = parUtils::StringToInt(xpEarnedValue);
		
			bool isCharacterActive = StringToBool(FindSaveMigrationMPAccountStat(record, "MP1_CHAR_ISACTIVE"));
			return CreateFormattedMPCharacterEntry("Character 2", xpEarned, "", characterSlot, isCharacterActive);
		}
	}

	struct MultiplayerModeSaveGameEntry
	{
	public:
		MultiplayerModeSaveGameEntry() { }

		MultiplayerModeSaveGameEntry(const char* gamerTag, const char* lastPlayed, u64 lastPlayedPosixTime, const char* ugcPublishedMissionNumber, const char* bankedCash, const char* eligibleFunds, MultiplayerCharacterEntry characterEntry, MultiplayerCharacterEntry secondCharacterEntry) :
			m_gamerTag(gamerTag),
			m_lastPlayed(lastPlayed),
			m_lastPlayedPosixTime(lastPlayedPosixTime),
			m_ugcPublishedMissionNumber(ugcPublishedMissionNumber),
			m_bankedCash(bankedCash),
			m_eligibleFunds(eligibleFunds),
			m_characterEntry(characterEntry),
			m_secondCharacterEntry(secondCharacterEntry)			
		{
		}
		ConstString GetGamerTag() const { return m_gamerTag; }
		ConstString GetLastPlayedTime() const { return m_lastPlayed; }
		u64 GetLastPlayedPosixTime() const { return m_lastPlayedPosixTime; }
		ConstString GetUGCPublishedMissionNumber() const { return m_ugcPublishedMissionNumber; }
		ConstString GetBankedCash() const { return m_bankedCash; }
		ConstString GetEligibleFunds() const { return m_eligibleFunds; }
		const MultiplayerCharacterEntry& GetCharacter(int index) const
		{
			uiAssertf(index == 0 || index == 1, "Invalid character index %d passed in", index);
			return (index == 0) ? m_characterEntry : m_secondCharacterEntry;
		}
		
	private:
		ConstString m_gamerTag;
		ConstString m_lastPlayed;
		u64 m_lastPlayedPosixTime;
		ConstString m_ugcPublishedMissionNumber;
		ConstString m_bankedCash;
		ConstString m_eligibleFunds;
		MultiplayerCharacterEntry m_characterEntry;
		MultiplayerCharacterEntry m_secondCharacterEntry;
	};

	// Order of profiles listed will be based on most recent first. 
	int SortByLastPlayedDescending(const MultiplayerModeSaveGameEntry* const lhs, const MultiplayerModeSaveGameEntry* const rhs)
	{
		if (uiVerifyf(lhs != nullptr && rhs != nullptr, "Encountered null element(s) when attempting to sort collection"))
		{

			const u64 c_uploadTimeLhs = lhs->GetLastPlayedPosixTime();
			const u64 c_uploadTimeRhs = rhs->GetLastPlayedPosixTime();

			// Greater-than to sort descending here
			if (c_uploadTimeLhs > c_uploadTimeRhs)
			{
				return -1;
			}
			else if (c_uploadTimeLhs == c_uploadTimeRhs)
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
		return 0;
	}

	//  Order characters by most recently player, prioritise non active accounts
	int SortByNonActiveAccountFirst(const MultiplayerCharacterEntry* const lhs, const MultiplayerCharacterEntry* const rhs)
	{
		if (uiVerifyf(lhs != nullptr && rhs != nullptr, "Encountered null element(s) when attempting to sort collection"))
		{

			const bool c_characterActiveLhs = lhs->GetIsActive();
			const bool c_characterActiveRhs = rhs->GetIsActive();

			if (c_characterActiveLhs && !c_characterActiveRhs)
			{
				return -1;
			}
			else if (!c_characterActiveLhs && c_characterActiveRhs)
			{
				return 1;
			}
			else
			{
				// TODO_UI: Sort by Last Played timestamp when available in retrieved data
				return 0;
			}
		}
		return 0;
	}

	MultiplayerModeSaveGameEntry CreateFormattedMPSaveGameEntry(const char* gamerTag, u64 lastPlayed, u64 ugcPublishedMissionNumber, int bankedCash, int  eligibleFunds, MultiplayerCharacterEntry characterEntry, MultiplayerCharacterEntry secondCharacterEntry)
	{
		char lastPlayedTimeBuffer[32];
		const time_t c_lastPlayedTimeUTC = lastPlayed;
#if IS_GEN9_PLATFORM && RSG_PROSPERO
		tm dateData;
		const tm* const c_lastPlayedTimeLocal = localtime_s(&c_lastPlayedTimeUTC, &dateData);
#else
		const tm *const c_lastPlayedTimeLocal = localtime(&c_lastPlayedTimeUTC);
#endif
		strftime(lastPlayedTimeBuffer, 31, "%m/%d/%Y %H:%M", c_lastPlayedTimeLocal);

		char c_ugcPublishedMissionNumberBuffer[32];
		formatf(c_ugcPublishedMissionNumberBuffer, 32, "%d", ugcPublishedMissionNumber);

		const int c_cashBufferLength = 18; // Space to store "-$999,999,999,999\0"		
		char bankedCashRemainingBuffer[c_cashBufferLength];
		CFrontendStatsMgr::FormatInt64ToCash(bankedCash, bankedCashRemainingBuffer, c_cashBufferLength);

		char eligibleCashRemainingBuffer[c_cashBufferLength];
		CFrontendStatsMgr::FormatInt64ToCash(eligibleFunds, eligibleCashRemainingBuffer, c_cashBufferLength);

		MultiplayerModeSaveGameEntry createdEntry(gamerTag, lastPlayedTimeBuffer, lastPlayed, c_ugcPublishedMissionNumberBuffer, bankedCashRemainingBuffer, eligibleCashRemainingBuffer, characterEntry, secondCharacterEntry);
		return createdEntry;
	}

	//TODO Populate this as stats become available
	MultiplayerModeSaveGameEntry CreateFormattedMPSaveGameEntry(const rlSaveMigrationMPAccount& record)
	{		
		MultiplayerCharacterEntry characterEntry;
		MultiplayerCharacterEntry secondCharacterEntry;

		characterEntry = CreateFormattedMPCharacterEntry(record, 0);
		secondCharacterEntry = CreateFormattedMPCharacterEntry(record, 1);		

		const char* lastPlayedValue = FindSaveMigrationMPAccountStat(record, "PROFILE_STATS_LAST_FLUSH");
		u64 lastPlayed = parUtils::StringToInt64(lastPlayedValue);

		const char* bankBalanceValue = FindSaveMigrationMPAccountStat(record, "BANK_BALANCE");
		int bankBalance = parUtils::StringToInt(bankBalanceValue);

		return CreateFormattedMPSaveGameEntry(record.m_Gamertag, lastPlayed, 0, bankBalance, bankBalance, characterEntry, secondCharacterEntry);

	}

} // SaveMigrationDefs

#endif // SAVE_MIGRATION_DEFS
