#ifndef SAVEGAME_MIGRATION_METADATA_H
#define SAVEGAME_MIGRATION_METADATA_H

// Game headers
#include "SaveLoad/savegame_defines.h"

// #if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

// Rage headers
#include "parser/macros.h"

// Forward declarations
class CDate;

class CSaveGameMigrationMetadata
{
public:
	CSaveGameMigrationMetadata() 
		: m_fCompletionPercentage(0.0f),
		m_SavePosixTime(0),
		m_HashOfTextKeyOfLastCompletedMission(0),
		m_CompressedSize(0),
		m_UncompressedSize(0) {}

	void SetCompletedMissionLocalizedText(const char *pLocalizedText);
	void SetCompletionPercentage(float percentage);
	float GetCompletionPercentage() { return m_fCompletionPercentage; }

	void SetCompletedMissionTextKeyHash(u32 textKeyHash);
	u32 GetCompletedMissionTextKeyHash() { return m_HashOfTextKeyOfLastCompletedMission; }

	void SetSavePosixTime(CDate &savedDateTime);
	u32 GetSavePosixTime() { return m_SavePosixTime; }

	void SetCompressedSize(u32 compressedSize);
	void SetUncompressedSize(u32 uncompressedSize);

	bool SaveToLocalFile();

private:
	atString m_LastCompletedMissionLocalizedText;
	float m_fCompletionPercentage;

	u32 m_SavePosixTime;

	u32 m_HashOfTextKeyOfLastCompletedMission;

	u32 m_CompressedSize;
	u32 m_UncompressedSize;

	PAR_SIMPLE_PARSABLE;
};

// #endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_MIGRATION_METADATA_H
