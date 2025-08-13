
#include "savegame_migration_metadata.h"
#include "savegame_migration_metadata_parser.h"

// #if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

#include <time.h>

// Rage headers
#include "file/asset.h"
#include "parser/manager.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_date.h"


void CSaveGameMigrationMetadata::SetCompletedMissionLocalizedText(const char *pLocalizedText)
{
	savegameDisplayf("CSaveGameMigrationMetadata::SetCompletedMissionLocalizedText - setting m_LastCompletedMissionLocalizedText to %s", pLocalizedText);
	m_LastCompletedMissionLocalizedText = pLocalizedText;
}

void CSaveGameMigrationMetadata::SetCompletionPercentage(float percentage)
{
	savegameDisplayf("CSaveGameMigrationMetadata::SetCompletionPercentage - setting m_fCompletionPercentage to %.2f", percentage);
	m_fCompletionPercentage = percentage;
}

void CSaveGameMigrationMetadata::SetCompletedMissionTextKeyHash(u32 textKeyHash)
{
	savegameDisplayf("CSaveGameMigrationMetadata::SetCompletedMissionTextKeyHash - setting m_HashOfTextKeyOfLastCompletedMission to %u", textKeyHash);
	m_HashOfTextKeyOfLastCompletedMission = textKeyHash;
}

void CSaveGameMigrationMetadata::SetSavePosixTime(CDate &savedDateTime)
{
#if RSG_ORBIS
	//	It seems that savedDateTime already has 2000 added to it on PS4
	int year = savedDateTime.GetYear();	//	 + 2000;
#else
	int year = savedDateTime.GetYear() + 2000;
#endif
	int month = savedDateTime.GetMonth();
	int day = savedDateTime.GetDay();
	int hour = savedDateTime.GetHour();
	int minute = savedDateTime.GetMinute();
	int second = savedDateTime.GetSecond();

	savegameDisplayf("CSaveGameMigrationMetadata::SetSavePosixTime - year=%d month=%d day=%d hour=%d minute=%d second=%d", 
		year, month, day, hour, minute, second);

	tm inputTime;
	inputTime.tm_year = year - 1900;	// Year - 1900
	inputTime.tm_mon = month - 1;		// Month, where 0 = jan
	inputTime.tm_mday = day;			// Day of the month
	inputTime.tm_hour = hour;
	inputTime.tm_min = minute;
	inputTime.tm_sec = second;
	inputTime.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown

	time_t posixTime;
	posixTime = mktime(&inputTime);
	u64 posixTime64 = static_cast<u64>(posixTime);

	m_SavePosixTime = posixTime64 & 0xffffffff;

#if !__NO_OUTPUT
	u32 posixTimeHigh32 = posixTime64 >> 32;
	if (posixTimeHigh32 != 0)
	{
		savegameErrorf("CSaveGameMigrationMetadata::SetSavePosixTime - Expected posixTimeHigh32 to be 0, but it's %u", posixTimeHigh32);
		savegameAssertf(0, "CSaveGameMigrationMetadata::SetSavePosixTime - Expected posixTimeHigh32 to be 0, but it's %u", posixTimeHigh32);
	}
	savegameDisplayf("CSaveGameMigrationMetadata::SetSavePosixTime - posixTime64 = %" I64FMT "u Setting m_SavePosixTime to the lower 32 bits = %u", posixTime64, m_SavePosixTime);
#endif // !__NO_OUTPUT
}

void CSaveGameMigrationMetadata::SetCompressedSize(u32 compressedSize)
{
	savegameDisplayf("CSaveGameMigrationMetadata::SetCompressedSize - setting m_CompressedSize to %u", compressedSize);
	m_CompressedSize = compressedSize;
}

void CSaveGameMigrationMetadata::SetUncompressedSize(u32 uncompressedSize)
{
	savegameDisplayf("CSaveGameMigrationMetadata::SetUncompressedSize - setting m_UncompressedSize to %u", uncompressedSize);
	m_UncompressedSize = uncompressedSize;
}

bool CSaveGameMigrationMetadata::SaveToLocalFile()
{
#if !__FINAL
	ASSET.PushFolder("X:");
	PARSER.SaveObject("exported_savegame_metadata", "xml", this, parManager::XML);
	ASSET.PopFolder();
#endif // !__FINAL

	return true;
}

// #endif //__ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

