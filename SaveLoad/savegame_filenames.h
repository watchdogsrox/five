
#ifndef SAVEGAME_FILENAMES_H
#define SAVEGAME_FILENAMES_H

// Rage headers
#include "file/device.h"
#include "file/savegame.h"

// Game headers
#include "text/TextFile.h"
#include "SaveLoad/savegame_defines.h"


#define SAVE_GAME_MAX_DISPLAY_NAME_LENGTH	(128)
#define SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE		(22)	//	Max size of PS3 directory name is 32
																//	subtract nine for the product code and one for the NULL character

#define SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE		(64)

#define	MAX_LENGTH_SAVE_DATE_TIME (30)


// Forward declarations
class CSavegamePhotoUniqueId;


class CSavegameFilenames
{
private:
//	Private data
	static char ms_FilenameOfLocalFile[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
#if RSG_ORBIS
	static char ms_FilenameOfBackupSaveFile[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
#endif //RSG_ORBIS
	static rage::char16	ms_NameToDisplay[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];

	static char ms_FilenameOfCloudFile[SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE];

	static float ms_fCompletionPercentageForSinglePlayerDisplayName;
	static const char *ms_NameOfLastMissionPassed;

//	Private functions
	static void FormatTheFileTimeString(char *pStringToFill, fiDevice::SystemTime LocalSysTime);

	static char16* CopyUtf8StringToWideString(const char* in, int lengthOfInputString, char16* out, int maxLengthOfOutputString);

	static void MakeDisplayNameForSavegame(int SlotNumber);

//	static void SetFilenameForNewCloudPhoto(char *pDiscName, s32 MaxLengthOfDiscName);

#if __XENON
	static void AppendCurrentDateAndTimeToDisplayName(s32 CurrentCharacterInDisplayName);
#endif	//	__XENON

	static const char *StripPath(const char *pFilename);

public:
//	Public functions
	static char *CopyWideStringToUtf8String(const char16 *in, int lengthOfInputString, char *out, int maxNumberOfBytesInOutputString);

	static void ExtractInfoFromDisplayName(const char16 *DisplayName, int *pReturnEpisodeNumber, char *pFileTimeString, int *pFirstCharOfMissionName, int *pLengthOfMissionName);

	static const char *GetNameOfLastMissionPassed() { return ms_NameOfLastMissionPassed; }

	static float GetMissionCompletionPercentage() { return ms_fCompletionPercentageForSinglePlayerDisplayName; }
	static void SetCompletionPercentageForSinglePlayerDisplayName();

//	static void GetFileIndexFromFileName(const char *pDiscName, int *pReturnIndex);

//	static void  CreateNameOfFileOnDisc(char *pDiscName, s32 MaxLengthOfDiscName, const int NumberOfSlot, const bool addCloudPath);
//	static void  MakeValidSaveName(int SlotNumber, const bool addCloudPath = false);
	static void CreateNameOfLocalFile(char *pFileNameToFillIn, s32 lengthOfInputArray, const int NumberOfSlot);
	static void MakeValidSaveNameForLocalFile(int SlotNumber);

	static void CreateNameOfLocalPhotoFile(char *pFileNameToFillIn, s32 lengthOfInputArray, const CSavegamePhotoUniqueId &photoUniqueId);
	static void MakeValidSaveNameForLocalPhotoFile(const CSavegamePhotoUniqueId &photoUniqueId);


	static void MakeValidSaveNameForMpStatsCloudFile(int mpStatsIndex);

	static bool IsThisTheNameOfAnAutosaveFile(const char *pFilenameToTest);
	static s32 GetEpisodeIndexFromAutosaveFilename(const char *pFilenameToTest);

	static bool IsThisTheNameOfASavegameFile(const char *pFilenameToTest);
#if RSG_ORBIS
	static bool IsThisTheNameOfABackupSavegameFile(const char *pFilenameToTest);
#endif
#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	static bool IsThisTheNameOfAnAutosaveBackupFile(const char *pFilenameToTest);
	static s32 GetEpisodeIndexFromAutosaveBackupFilename(const char *pFilenameToTest);
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

	static bool IsThisTheNameOfAReplaySaveFile(const char *pFilenameToTest);

	static bool IsThisTheNameOfAPhotoFile(const char *pFilenameToTest);
	static s32 GetPhotoUniqueIdFromFilename(const char *pFilenameToTest);

	static bool SetFilenameOfLocalFile(const char *pFilename);
	static bool SetFilenameOfCloudFile(const char *pFilename);

	static void CreateFilenameForNewCloudPhoto();

	static const char* GetFilenameOfLocalFile() { return &ms_FilenameOfLocalFile[0]; }
#if RSG_ORBIS
	static const char* GetFilenameOfBackupSaveFile() { return &ms_FilenameOfBackupSaveFile[0]; }
#endif
	static const char* GetFilenameOfCloudFile() { return &ms_FilenameOfCloudFile[0]; }
	static const char16* GetNameToDisplay() { return &ms_NameToDisplay[0]; }

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static void SetNameToDisplay(const char *pNameOfLastMissionPassed, float fPercentageComplete);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
};


#endif	//	SAVEGAME_FILENAMES_H
