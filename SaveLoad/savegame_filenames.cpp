
#include <stdio.h>

#if __XENON
	#include "system/xtl.h"
#endif

#include "frontend/PauseMenu.h"
#include "control/gamelogic.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "SaveLoad/savegame_photo_unique_id.h"
#include "SaveLoad/savegame_save.h"
#include "stats/StatsInterface.h"
#include "text/messages.h"
#include "text/TextConversion.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/ped.h"

SAVEGAME_OPTIMISATIONS()

//Cloud Single Player stats file names
static const char SP_STATS_PATH[] = { "GTA5/saves/spstats" };
//static const char* SP_STATS_FILENAME[] = { "save_default" , "save_char" };
//Cloud Multiplayer Player stats file names
static const char MP_STATS_PATH[] = { "GTA5/saves/mpstats" };
static const char* MP_STATS_FILENAME[] = { "save_default" , "save_char" };
//Cloud Photos path
static const char PHOTO_PATH[] = { "GTA5/saves/photo" };
#if __PS3
//Cloud PS3 Profile path
static const char PROFILE_PATH[] = { "GTA5/saves/profile" };
#endif // __PS3

char		CSavegameFilenames::ms_FilenameOfLocalFile[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "";
#if RSG_ORBIS
char		CSavegameFilenames::ms_FilenameOfBackupSaveFile[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "";
#endif //RSG_ORBIS
char16		CSavegameFilenames::ms_NameToDisplay[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];

char		CSavegameFilenames::ms_FilenameOfCloudFile[SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE] = "";

float CSavegameFilenames::ms_fCompletionPercentageForSinglePlayerDisplayName = 0.0f;
const char *CSavegameFilenames::ms_NameOfLastMissionPassed = NULL;

const char SavegameFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "SGTA5";
#if RSG_ORBIS
const char SavegameBackupFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "SBGTA5";
#endif //RSG_ORBIS
const char PhotoFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "PGTA5";
const char MPStatsFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "MPGTA5";
const char ReplayFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "REPLAY";
const char MissionRepeatFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "MISREP";

#if __PS3
//	const char PS3ProfileFileOnDiscPrefix[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = "profile";

#if __ASSERT
//Max size of PS3 directory name is 32, subtract
//  nine for the product code and one for the NULL character.
static const u32 MAX_SIZE_PS3_DIRECTORY = 22;
#endif	//	__ASSERT

#endif	//	__PS3

bool CSavegameFilenames::IsThisTheNameOfASavegameFile(const char *pFilenameToTest)
{
	const s32 LengthOfSaveGamePrefix = istrlen(SavegameFileOnDiscPrefix);
	if (!strnicmp(pFilenameToTest, SavegameFileOnDiscPrefix, LengthOfSaveGamePrefix))
	{
		return true;
	}

	return false;
}

#if RSG_ORBIS
bool CSavegameFilenames::IsThisTheNameOfABackupSavegameFile(const char *pFilenameToTest)
{
	const s32 LengthOfSaveGamePrefix = istrlen(SavegameBackupFileOnDiscPrefix);
	if (!strnicmp(pFilenameToTest, SavegameBackupFileOnDiscPrefix, LengthOfSaveGamePrefix))
	{
		return true;
	}

	return false;
}
#endif


#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
bool CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(const char *pFilenameToTest)
{
	char filenameOfBackupAutosave[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
	CreateNameOfLocalFile(filenameOfBackupAutosave, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, AUTOSAVE_SLOT_FOR_EPISODE_ZERO+TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);

	if (!strnicmp(pFilenameToTest, filenameOfBackupAutosave, istrlen(filenameOfBackupAutosave)))
	{
		return true;
	}

	return false;
}

s32 CSavegameFilenames::GetEpisodeIndexFromAutosaveBackupFilename(const char *pFilenameToTest)
{
	char filenameOfBackupAutosave[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];

	CreateNameOfLocalFile(filenameOfBackupAutosave, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, AUTOSAVE_SLOT_FOR_EPISODE_ZERO+TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
	if (!strnicmp(pFilenameToTest, filenameOfBackupAutosave, istrlen(filenameOfBackupAutosave)))
	{
		return 0;
	}

// 	CreateNameOfLocalFile(filenameOfBackupAutosave, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, AUTOSAVE_SLOT_FOR_EPISODE_ONE+TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
// 	if (!strnicmp(pFilenameToTest, filenameOfBackupAutosave, istrlen(filenameOfBackupAutosave)))
// 	{
// 		return 1;
// 	}

// 	CreateNameOfLocalFile(filenameOfBackupAutosave, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, AUTOSAVE_SLOT_FOR_EPISODE_TWO+TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
// 	if (!strnicmp(pFilenameToTest, filenameOfBackupAutosave, istrlen(filenameOfBackupAutosave)))
// 	{
// 		return 2;
// 	}

	return -1;
}
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

bool CSavegameFilenames::IsThisTheNameOfAReplaySaveFile(const char *pFilenameToTest)
{
	const s32 LengthOfSaveGamePrefix = istrlen(ReplayFileOnDiscPrefix);
	if (!strnicmp(pFilenameToTest, ReplayFileOnDiscPrefix, LengthOfSaveGamePrefix))
	{
		return true;
	}

	return false;
}

const char *CSavegameFilenames::StripPath(const char *pFilename)
{
	const char* lastFs = pFilename;
	while( *pFilename != '\0' )
	{
		if( *pFilename == '/' )
		{
			lastFs = &pFilename[1];
		}
		pFilename++;
	}

	return lastFs;
}

bool CSavegameFilenames::IsThisTheNameOfAPhotoFile(const char *pFilenameToTest)
{
	const char* lastFs = StripPath(pFilenameToTest);

	const s32 LengthOfPhotoPrefix = istrlen(PhotoFileOnDiscPrefix);
	if (!strncmp(lastFs, PhotoFileOnDiscPrefix, LengthOfPhotoPrefix))
	{
		return true;
	}

	return false;
}

s32 CSavegameFilenames::GetPhotoUniqueIdFromFilename(const char *pFilenameToTest)
{
	if (IsThisTheNameOfAPhotoFile(pFilenameToTest))
	{
		const char* lastFs = StripPath(pFilenameToTest);

		const s32 LengthOfPhotoPrefix = istrlen(PhotoFileOnDiscPrefix);
		lastFs += LengthOfPhotoPrefix;

		s32 uniqueId = 0;
		s32 numberOfValuesRead = sscanf(lastFs, "%d", &uniqueId);

		if (numberOfValuesRead == 1)
		{
			return uniqueId;
		}
	}

	return 0;
}

bool CSavegameFilenames::SetFilenameOfLocalFile(const char *pFilename)
{
	if (savegameVerifyf( (strlen(pFilename) < NELEM(ms_FilenameOfLocalFile)), "CSavegameFilenames::SetFilenameOfLocalFile - filename %s has more than %d characters", pFilename, NELEM(ms_FilenameOfLocalFile)))
	{
		safecpy(ms_FilenameOfLocalFile, pFilename, NELEM(ms_FilenameOfLocalFile));
		savegameDisplayf("CSavegameFilenames::SetFilenameOfLocalFile - ms_FilenameOfLocalFile is now %s", ms_FilenameOfLocalFile);
		return true;
	}

	return false;
}

bool CSavegameFilenames::SetFilenameOfCloudFile(const char *pFilename)
{
	if (savegameVerifyf( (strlen(pFilename) < NELEM(ms_FilenameOfCloudFile)), "CSavegameFilenames::SetFilenameOfCloudFile - filename %s has more than %d characters", pFilename, NELEM(ms_FilenameOfCloudFile)))
	{
		safecpy(ms_FilenameOfCloudFile, pFilename, NELEM(ms_FilenameOfCloudFile));
		savegameDisplayf("CSavegameFilenames::SetFilenameOfCloudFile - ms_FilenameOfCloudFile is now %s", ms_FilenameOfCloudFile);
		return true;
	}

	return false;
}


void CSavegameFilenames::CreateFilenameForNewCloudPhoto()
{
//	SetFilenameForNewCloudPhoto(ms_NameOfFileOnDisc, NELEM(ms_NameOfFileOnDisc));

	rage::fiDevice::SystemTime tm;
	rage::fiDevice::GetLocalSystemTime( tm );

	// since PS3 and PS4 both use the NP platform, and 360 and XboxOne use the XBL platform, need to differentiate the filename
#if RSG_ORBIS
	const char* platformExt = "_ps4";
#elif RSG_DURANGO
	const char* platformExt = "_xboxone";
#else
	const char* platformExt = "";
#endif

	formatf(ms_FilenameOfCloudFile, NELEM(ms_FilenameOfCloudFile), "%s/%s_%04d%02d%02d%02d%02d%02d%s.jpeg", PHOTO_PATH, PhotoFileOnDiscPrefix, tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond, platformExt);

	savegameDisplayf("CSavegameFilenames::CreateFilenameForNewCloudPhoto - ms_FilenameOfCloudFile is now %s", ms_FilenameOfCloudFile);
}

/*
void CSavegameFilenames::SetFilenameForNewCloudPhoto(char *pDiscName, s32 MaxLengthOfDiscName)
{
	rage::fiDevice::SystemTime tm;
	rage::fiDevice::GetLocalSystemTime( tm );
	//				sprintf(pDiscName, "%s/%s_.save", PHOTO_PATH, PhotoFileOnDiscPrefix, FileIndex);
	formatf(pDiscName, MaxLengthOfDiscName, "%s/%s_%04d%02d%02d%02d%02d%02d.jpeg", PHOTO_PATH, PhotoFileOnDiscPrefix, tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond );
	savegameDisplayf("CSavegameFilenames::SetFilenameForNewCloudPhoto - filename for cloud photo is %s", pDiscName);
}
*/

//	Allow 10 characters for the word "Episode" and its translations
//	#define MAX_CHARACTERS_FOR_EPISODE_WORD		(10)
//	Add 7 for space, two digits, space, hyphen, space, NULL
//	#define MAX_CHARACTERS_FOR_EPISODE_DATA		(MAX_CHARACTERS_FOR_EPISODE_WORD + 7)

//	Save date and time " - mm/dd/yy hh:mm:ss" 21 characters (including NULL)
#define MAX_CHARACTERS_FOR_TIME_DATE		(21)

//	const char Episode1DisplayNamePrefix[MAX_CHARACTERS_FOR_EPISODE_DATA] = "TLAD";
//	const char Episode2DisplayNamePrefix[MAX_CHARACTERS_FOR_EPISODE_DATA] = "TBoGT";


void CSavegameFilenames::CreateNameOfLocalFile(char *pFileNameToFillIn, s32 lengthOfInputArray, const int NumberOfSlot)
{
	if (savegameVerifyf(pFileNameToFillIn, "CSavegameFilenames::CreateNameOfLocalFile - pFileNameToFillIn is NULL"))
	{
		if (savegameVerifyf(lengthOfInputArray == NELEM(ms_FilenameOfLocalFile), "CSavegameFilenames::CreateNameOfLocalFile - length of input array must be %d", lengthOfInputArray))
		{
			eSavegameFileType SgFiletype = CSavegameList::FindSavegameFileTypeForThisSlot(NumberOfSlot);
			s32 BaseIndex = CSavegameList::GetBaseIndexForSavegameFileType(SgFiletype);
			s32 FileIndex = NumberOfSlot - BaseIndex;

			strcpy(pFileNameToFillIn, "");
			switch (SgFiletype)
			{
			case SG_FILE_TYPE_SAVEGAME :
				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%04d", SavegameFileOnDiscPrefix, FileIndex);
				break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
			case SG_FILE_TYPE_MULTIPLAYER_STATS :
				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%04d", MPStatsFileOnDiscPrefix, FileIndex);
				break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

			case SG_FILE_TYPE_MISSION_REPEAT :	//	Used by Andrew Minghella
				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%04d", MissionRepeatFileOnDiscPrefix, FileIndex);
				break;

#if GTA_REPLAY
			case SG_FILE_TYPE_REPLAY :
				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%04d", ReplayFileOnDiscPrefix, FileIndex);
				break;
#endif	//	GTA_REPLAY

#if USE_PROFILE_SAVEGAME
			case SG_FILE_TYPE_PS3_PROFILE :
				//	Probably don't want a number at the end of this filename - there is only one PS3 Profile
//				formatf(pFileNameToFillIn, lengthOfInputArray, "%s", PS3ProfileFileOnDiscPrefix);
				safecpy(pFileNameToFillIn, CProfileSettings::GetFilenameOfPSProfile(), lengthOfInputArray);
				break;

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
			case SG_FILE_TYPE_BACKUP_PROFILE :
				safecpy(pFileNameToFillIn, "PROFILEB", lengthOfInputArray);
				break;
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP
#endif	//	USE_PROFILE_SAVEGAME

			case SG_FILE_TYPE_LOCAL_PHOTO :
//				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%04d", PhotoFileOnDiscPrefix, FileIndex);	//	I need to change this to use a random unique number instead of FileIndex. Maybe remove the 04 from the formatting?
				savegameAssertf(0, "CSavegameFilenames::CreateNameOfLocalFile - this function should never be called for SG_FILE_TYPE_LOCAL_PHOTO. Local photos are not numbered 0, 1, 2... They have a randomly generated unique number in their filename. Call CSavegameFilenames::CreateNameOfLocalPhotoFile instead");
				break;
#if RSG_ORBIS
			case SG_FILE_TYPE_SAVEGAME_BACKUP :
				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%04d", SavegameBackupFileOnDiscPrefix, FileIndex);
				break;
#endif
			default :
				savegameAssertf(0, "CSavegameFilenames::CreateNameOfLocalFile - unsupported file type %d", SgFiletype);
				break;
			}

#if __PS3 && __ASSERT
			if (strlen(pFileNameToFillIn) > 0)
			{
				//Max size of PS3 directory name is 32, subtract
				//  nine for the product code and one for the NULL character.
				savegameAssertf(MAX_SIZE_PS3_DIRECTORY >= strlen(pFileNameToFillIn), "CSavegameFilenames::CreateNameOfLocalFile - Invalid size for filename - %s", pFileNameToFillIn);
			}
#endif // __PS3
		}
	}
}

void CSavegameFilenames::MakeValidSaveNameForLocalFile(int SlotNumber)
{
	savegameAssertf( (SlotNumber >= 0) && (SlotNumber < MAX_NUM_EXPECTED_SAVE_FILES), "CSavegameFilenames::MakeValidSaveNameForLocalFile - slot number %d is out of range 0 to %d", SlotNumber, MAX_NUM_EXPECTED_SAVE_FILES);

	eSavegameFileType SgFiletype = CSavegameList::FindSavegameFileTypeForThisSlot(SlotNumber);

	if (SgFiletype == SG_FILE_TYPE_LOCAL_PHOTO)
	{
		savegameAssertf(0, "CSavegameFilenames::MakeValidSaveNameForLocalFile - this function should never be called for SG_FILE_TYPE_LOCAL_PHOTO. Local photos are not numbered 0, 1, 2... They have a randomly generated unique number in their filename. Call CSavegameFilenames::MakeValidSaveNameForLocalPhotoFile instead");
		return;
	}
#if RSG_ORBIS
	if (SgFiletype == SG_FILE_TYPE_SAVEGAME)
	{
		//create name for backup
		CreateNameOfLocalFile(ms_FilenameOfBackupSaveFile, NELEM(ms_FilenameOfBackupSaveFile), SlotNumber+INDEX_OF_BACKUPSAVE_SLOT);
	}
#endif

	CreateNameOfLocalFile(ms_FilenameOfLocalFile, NELEM(ms_FilenameOfLocalFile), SlotNumber);
	savegameDisplayf("CSavegameFilenames::MakeValidSaveNameForLocalFile - ms_FilenameOfLocalFile is now %s", ms_FilenameOfLocalFile);
	//	sprintf(ms_NameOfFileOnDisc, "%s%02d", FileOnDiscPrefix, SlotNumber);
	//	And change the sort function to expect the new format filename above


	switch (SgFiletype)
	{
	case SG_FILE_TYPE_SAVEGAME :
		MakeDisplayNameForSavegame(SlotNumber);
		break;
#if RSG_ORBIS
	case SG_FILE_TYPE_SAVEGAME_BACKUP :
		MakeDisplayNameForSavegame(SlotNumber);
		break;
#endif

#if __ALLOW_LOCAL_MP_STATS_SAVES
	case SG_FILE_TYPE_MULTIPLAYER_STATS :
		{
			s32 BaseIndex = CSavegameList::GetBaseIndexForSavegameFileType(SgFiletype);
			s32 FileIndex = SlotNumber - BaseIndex;

			char TextLabelOfDisplayName[32];
			sysMemSet(TextLabelOfDisplayName, 0, 32);
			sprintf(TextLabelOfDisplayName, "MP_Save_%02d", FileIndex);

			if (TheText.DoesTextLabelExist(TextLabelOfDisplayName))
			{
				char *pMPSavegameFilename = TheText.Get(TextLabelOfDisplayName);
				CopyUtf8StringToWideString(pMPSavegameFilename, istrlen(pMPSavegameFilename), ms_NameToDisplay, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
			}
			else
			{
				if (FileIndex == 0)
				{
					CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, "Multiplayer Save", SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
				}
				else
				{
					char MPCharacterName[32];
					formatf(MPCharacterName, NELEM(MPCharacterName), "Multiplayer Character %02d", FileIndex);
					CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, MPCharacterName, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
				}
			}
		}
		break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	case SG_FILE_TYPE_MISSION_REPEAT :	//	Used by Andrew Minghella
		if (TheText.DoesTextLabelExist("SG_MISS_RPL"))
		{
			char *pMissionReplayFilename = TheText.Get("SG_MISS_RPL");
			CopyUtf8StringToWideString(pMissionReplayFilename, istrlen(pMissionReplayFilename), ms_NameToDisplay, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
		}
		else
		{
			CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, "Mission Replay", SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
		}
		break;

#if GTA_REPLAY
	case SG_FILE_TYPE_REPLAY :
		{
			const s32 baseIndex = CSavegameList::GetBaseIndexForSavegameFileType(SgFiletype);
			const s32 displayIndex = SlotNumber - baseIndex;

			char displayName[64];
			sysMemSet(displayName, 0, 64);
			sprintf(displayName, "Replay_%04d", displayIndex);

			CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, displayName, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
		}
		break;
#endif	//	GTA_REPLAY

#if USE_PROFILE_SAVEGAME
	case SG_FILE_TYPE_PS3_PROFILE :
// 		if (TheText.DoesTextLabelExist("SG_PROFILE"))
// 		{
// 			char *pPS3ProfileFilename = TheText.Get("SG_PROFILE");
// 			CopyUtf8StringToWideString(pPS3ProfileFilename, istrlen(pPS3ProfileFilename), ms_NameToDisplay, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
// 		}
// 		else
// 		{
// 			CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, "Profile", SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
// 		}
		safecpy(ms_NameToDisplay, CProfileSettings::GetDisplayNameOfPSProfile(), SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
		break;

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
	case SG_FILE_TYPE_BACKUP_PROFILE :
		//	Just use the same display name as the main profile?
		safecpy(ms_NameToDisplay, CProfileSettings::GetDisplayNameOfPSProfile(), SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
		break;
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP
#endif	//	USE_PROFILE_SAVEGAME

	case SG_FILE_TYPE_LOCAL_PHOTO :
	default :
		savegameAssertf(0, "CSavegameFilenames::MakeValidSaveNameForLocalFile - unsupported file type %d", SgFiletype);
		break;
	}
}



void CSavegameFilenames::CreateNameOfLocalPhotoFile(char *pFileNameToFillIn, s32 lengthOfInputArray, const CSavegamePhotoUniqueId &photoUniqueId)
{
	if (savegameVerifyf(pFileNameToFillIn, "CSavegameFilenames::CreateNameOfLocalPhotoFile - pFileNameToFillIn is NULL"))
	{
		if (savegameVerifyf(lengthOfInputArray == NELEM(ms_FilenameOfLocalFile), "CSavegameFilenames::CreateNameOfLocalPhotoFile - length of input array must be %d", lengthOfInputArray))
		{
//			if (savegameVerifyf(pPhotoUniqueId, "CSavegameFilenames::CreateNameOfLocalPhotoFile - pPhotoUniqueId is NULL"))
			{
				formatf(pFileNameToFillIn, lengthOfInputArray, "%s%d", PhotoFileOnDiscPrefix, photoUniqueId.GetValue());	//	This used to have 04 for the formatting
			}
		}
	}
}

void CSavegameFilenames::MakeValidSaveNameForLocalPhotoFile(const CSavegamePhotoUniqueId &photoUniqueId)
{
	CreateNameOfLocalPhotoFile(ms_FilenameOfLocalFile, NELEM(ms_FilenameOfLocalFile), photoUniqueId);
	savegameDisplayf("CSavegameFilenames::MakeValidSaveNameForLocalPhotoFile - ms_FilenameOfLocalFile is now %s", ms_FilenameOfLocalFile);

	if (TheText.DoesTextLabelExist("SG_PHOTO"))
	{
		char *pPhotoDisplayName = TheText.Get("SG_PHOTO");
		CopyUtf8StringToWideString(pPhotoDisplayName, istrlen(pPhotoDisplayName), ms_NameToDisplay, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
	}
	else
	{
		CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, "Photo", SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
	}
}

void CSavegameFilenames::MakeValidSaveNameForMpStatsCloudFile(int mpStatsIndex)
{
	savegameAssertf( (mpStatsIndex >= 0) && (mpStatsIndex < NUM_MULTIPLAYER_STATS_SAVE_SLOTS), "CSavegameFilenames::MakeValidSaveNameForMpStatsCloudFile - slot number %d is out of range 0 to %d", mpStatsIndex, NUM_MULTIPLAYER_STATS_SAVE_SLOTS);

	const char* filename = MP_STATS_FILENAME[mpStatsIndex > 0 ? 1 : 0];

	// since PS3 and PS4 both use the NP platform, and 360 and XboxOne use the XBL platform, need to differentiate the filename
#if RSG_ORBIS
	const char* platformExt = "_ps4";
#elif RSG_DURANGO
	const char* platformExt = "_xboxone";
#else
	const char* platformExt = "";
#endif

	formatf(ms_FilenameOfCloudFile, NELEM(ms_FilenameOfCloudFile), "%s/%s%04d%s.save", MP_STATS_PATH, filename, mpStatsIndex, platformExt);
	savegameDisplayf("CSavegameFilenames::MakeValidSaveNameForMpStatsCloudFile - ms_FilenameOfCloudFile is now %s", ms_FilenameOfCloudFile);

	char displayName[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
//	sysMemSet(displayName, 0, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
	formatf(displayName, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH, "MP Stats_%04d", mpStatsIndex);

	CTextConversion::CopyAsciiStringIntoChar16Buffer(ms_NameToDisplay, displayName, SAVE_GAME_MAX_DISPLAY_NAME_LENGTH);
}

//	Based on rage::Utf8ToWide. Takes a maxLengthOfOutputString
char16* CSavegameFilenames::CopyUtf8StringToWideString(const char* in, int lengthOfInputString, char16* out, int maxLengthOfOutputString)
{
	char16* outPos = out;
	int outputLength = 0;
	for(int i = 0; in[i] && (i < lengthOfInputString) && (outputLength < (maxLengthOfOutputString-1));)
	{
		int incr = Utf8ToWideChar(in + i, *outPos);
		Assertf(incr,"Bad UTF-8 encoding in %s, char %d",in,i);
		if(0 == incr)
		{
			break; //let's not go into an infinite loop just because there is bad encoding.
		}

		i += incr;
		outPos++;
		outputLength++;
	}
	*outPos = 0;
	return out;
}

//	Based on rage::WideToUtf8. Takes a maxNumberOfBytesInOutputString
char *CSavegameFilenames::CopyWideStringToUtf8String(const char16 *in, int lengthOfInputString, char *out, int maxNumberOfBytesInOutputString)
{
	u8 * utf8out = (u8*)out;
	int outputLength = 0;

	maxNumberOfBytesInOutputString--;	//	Take one off to leave space for the null terminator
	bool bNoSpaceForNextCharacter = false;

	for (int i=0; in[i] && (i<lengthOfInputString) && !bNoSpaceForNextCharacter; i++)
	{
		const char16 input = in[i];
		if (input<=0x7F)
		{
			if ((outputLength+1) <= maxNumberOfBytesInOutputString)
			{
				*utf8out++ = (u8)input;
				outputLength++;
			}
			else
			{
				bNoSpaceForNextCharacter = true;
			}
		}
		else if (input <= 0x7FF)
		{
			if ((outputLength+2) <= maxNumberOfBytesInOutputString)
			{
				*utf8out++ = u8(0xC0 + (input >> 6));
				*utf8out++ = u8(0x80 + (input & 0x3f));
				outputLength += 2;
			}
			else
			{
				bNoSpaceForNextCharacter = true;
			}
		}
		else
		{
			if ((outputLength+3) <= maxNumberOfBytesInOutputString)
			{
				*utf8out++ = u8(0xE0 +  (input >> 12));
				*utf8out++ = u8(0x80 + ((input >> 6)&0x3f));
				*utf8out++ = u8(0x80 +  (input & 0x3f));
				outputLength += 3;
			}
			else
			{
				bNoSpaceForNextCharacter = true;
			}
		}
	}
	*utf8out = 0;
	return out;
}


void CSavegameFilenames::SetCompletionPercentageForSinglePlayerDisplayName()
{
	ms_fCompletionPercentageForSinglePlayerDisplayName = 0.0f;

	StatId statTotalProgressMade("TOTAL_PROGRESS_MADE");
	if (StatsInterface::IsKeyValid(statTotalProgressMade) && StatsInterface::GetStatIsNotNull(statTotalProgressMade))
	{
		ms_fCompletionPercentageForSinglePlayerDisplayName = StatsInterface::GetFloatStat(statTotalProgressMade);
	}

	savegameDisplayf("CSavegameFilenames::SetCompletionPercentageForSinglePlayerDisplayName - ms_fCompletionPercentageForSinglePlayerDisplayName is %f", ms_fCompletionPercentageForSinglePlayerDisplayName);
}

void CSavegameFilenames::MakeDisplayNameForSavegame(int SlotNumber)
{
#if RSG_ORBIS
	savegameAssertf( (SlotNumber >= 0) && (SlotNumber < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS), "CSavegameFilenames::MakeDisplayNameForSavegame - slot number %d is out of range 0 to %d", SlotNumber, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS);
#else
	savegameAssertf( (SlotNumber >= 0) && (SlotNumber < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameFilenames::MakeDisplayNameForSavegame - slot number %d is out of range 0 to %d", SlotNumber, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
#endif
//	*** Display Name
//	Episode first
/*
	s32 CurrentEpisode = CGameLogic::GetCurrentLevelIndex();	//	EXTRACONTENT.GetCurrentEpisode();
	int LengthOfEpisodeWord = 0;
	if (CurrentEpisode > 0)
	{
		if (CurrentEpisode == 1)
		{	//	Episode 1 saves will say "TLAD - " instead of "Ep. 01 - "
			//	I can't use a "SAV_TLAD" text label in the gxt file because the autoload needs to know about the prefix
			//	and the E1 gxt file is not loaded when the autoload happens
			LengthOfEpisodeWord = strlen(Episode1DisplayNamePrefix);
			Assertf(LengthOfEpisodeWord <= MAX_CHARACTERS_FOR_EPISODE_WORD, "CSavegameFilenames::MakeValidSaveName - Episode1 word is too long");

			for (loop = 0; loop < LengthOfEpisodeWord; loop++)
			{
				ms_NameToDisplay[CurrentCharacterInDisplayName++] = Episode1DisplayNamePrefix[loop];
			}
		}
		else if (CurrentEpisode == 2)
		{
			LengthOfEpisodeWord = strlen(Episode2DisplayNamePrefix);
			Assertf(LengthOfEpisodeWord <= MAX_CHARACTERS_FOR_EPISODE_WORD, "CSavegameFilenames::MakeValidSaveName - Episode2 word is too long");

			for (loop = 0; loop < LengthOfEpisodeWord; loop++)
			{
				ms_NameToDisplay[CurrentCharacterInDisplayName++] = Episode2DisplayNamePrefix[loop];
			}
		}
		else
		{
			char *pEpisodeWord = TheText.Get("EPISODE");
			LengthOfEpisodeWord = CTextConversion::charByteCount(pEpisodeWord);
			Assertf(LengthOfEpisodeWord <= MAX_CHARACTERS_FOR_EPISODE_WORD, "CSavegameFilenames::MakeValidSaveName - Episode word is too long");

			loop = 0;
			for (loop = 0; loop < LengthOfEpisodeWord; loop++)
			{
				ms_NameToDisplay[CurrentCharacterInDisplayName++] = pEpisodeWord[loop];
			}
		}

		char EpisodeInfo[16];	//	should only really need 7 characters
		if ( (CurrentEpisode == 1) || (CurrentEpisode == 2) )
		{	//	Episode 1 does not need to append the episode number, it says "TLAD - " instead of "Ep. 01 - "
			sprintf(EpisodeInfo, " - ");
		}
		else
		{
			sprintf(EpisodeInfo, " %02d - ", CurrentEpisode);
		}
		size_t LengthOfEpisodeInfo = strlen(EpisodeInfo);

		CopyAsciiStringIntoChar16Buffer(&ms_NameToDisplay[CurrentCharacterInDisplayName], EpisodeInfo, (LengthOfEpisodeInfo+1));	// plus one for NULL character
		CurrentCharacterInDisplayName += LengthOfEpisodeInfo;
		Assertf(CurrentCharacterInDisplayName < MAX_CHARACTERS_FOR_EPISODE_DATA, "CSavegameFilenames::MakeValidSaveName - episode part of display name is too long");
	}
*/

//	Mission name
	StatId statLastMissionName("SP_LAST_MISSION_NAME");

	if (StatsInterface::IsKeyValid(statLastMissionName) && StatsInterface::GetStatIsNotNull(statLastMissionName))
	{
		ms_NameOfLastMissionPassed = TheText.Get(StatsInterface::GetIntStat(statLastMissionName), "SP_LAST_MISSION_NAME");
	}
	else
	{
		ms_NameOfLastMissionPassed = TheText.Get("ITBEG");  // in the beginning;
	}

#define MAX_MISSION_NAME_LENGTH	(256)
	char LastMissionName[MAX_MISSION_NAME_LENGTH];

	const int autosaveSlot = CSavegameList::GetAutosaveSlotNumberForCurrentEpisode();
	if ( (SlotNumber == autosaveSlot) 
#if RSG_ORBIS
		|| (SlotNumber == autosaveSlot + TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES) 
#endif	//	RSG_ORBIS
		)
	{
		char *pAutosaveTxt = TheText.Get("AUTOS_PFIX");
		formatf_n( LastMissionName, "(%s) ", pAutosaveTxt );
	}
	else
	{
		LastMissionName[0] = rage::TEXT_CHAR_TERMINATOR;
	}

	safecat( LastMissionName, ms_NameOfLastMissionPassed );

//	Completion percentage
	char percentageString[12];
	formatf(percentageString, NELEM(percentageString), " (%.1f%%)", ms_fCompletionPercentageForSinglePlayerDisplayName);
	savegameDisplayf("CSavegameFilenames::MakeDisplayNameForSavegame - percentageString is %s", percentageString);
	safecat( LastMissionName, percentageString );

	const s32 maxLengthOfOutputString = (SAVE_GAME_MAX_DISPLAY_NAME_LENGTH - MAX_CHARACTERS_FOR_TIME_DATE - 1);		//	 - MAX_CHARACTERS_FOR_EPISODE_DATA
	CopyUtf8StringToWideString(LastMissionName, MAX_MISSION_NAME_LENGTH, ms_NameToDisplay, maxLengthOfOutputString);

//	I used to replace any hyphen characters in the mission name with spaces here so that I could later find my hyphen separators for episode number and creation date.
//	I now just search for the last hyphen to find the creation date separator. I only need to do that search on XBox. See ExtractInfoFromDisplayName

//	For Xenon also store the save date and time
#if __XENON
	int MissionNameLength = (int)wcslen(ms_NameToDisplay);
	AppendCurrentDateAndTimeToDisplayName(MissionNameLength);
#endif	//	__XENON
}

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CSavegameFilenames::SetNameToDisplay(const char *pNameOfLastMissionPassed, float fPercentageComplete)
{
#define MAX_MISSION_NAME_LENGTH	(256)
	char LastMissionName[MAX_MISSION_NAME_LENGTH];
	LastMissionName[0] = rage::TEXT_CHAR_TERMINATOR;

// Do I need to add the Autosave prefix by doing something like this 
// 	char *pAutosaveTxt = TheText.Get("AUTOS_PFIX");
// 	formatf_n( LastMissionName, "(%s) ", pAutosaveTxt );

	safecat(LastMissionName, pNameOfLastMissionPassed);

//	Completion percentage
	char percentageString[12];
	formatf(percentageString, NELEM(percentageString), " (%.1f%%)", fPercentageComplete);
	savegameDisplayf("CSavegameFilenames::SetNameToDisplay - percentageString is %s", percentageString);
	safecat( LastMissionName, percentageString );

	const s32 maxLengthOfOutputString = (SAVE_GAME_MAX_DISPLAY_NAME_LENGTH - MAX_CHARACTERS_FOR_TIME_DATE - 1);
	CopyUtf8StringToWideString(LastMissionName, MAX_MISSION_NAME_LENGTH, ms_NameToDisplay, maxLengthOfOutputString);
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if __XENON
void CSavegameFilenames::AppendCurrentDateAndTimeToDisplayName(s32 CurrentCharacterInDisplayName)
{
	ms_NameToDisplay[CurrentCharacterInDisplayName++] = ' ';
	Assertf(CurrentCharacterInDisplayName < SAVE_GAME_MAX_DISPLAY_NAME_LENGTH, "CSavegameFilenames::AppendCurrentDateAndTimeToDisplayName - adding the time string pushed the string length over SAVE_GAME_MAX_DISPLAY_NAME_LENGTH characters");
	ms_NameToDisplay[CurrentCharacterInDisplayName++] = '-';
	Assertf(CurrentCharacterInDisplayName < SAVE_GAME_MAX_DISPLAY_NAME_LENGTH, "CSavegameFilenames::AppendCurrentDateAndTimeToDisplayName - adding the time string pushed the string length over SAVE_GAME_MAX_DISPLAY_NAME_LENGTH characters");
	ms_NameToDisplay[CurrentCharacterInDisplayName++] = ' ';
	Assertf(CurrentCharacterInDisplayName < SAVE_GAME_MAX_DISPLAY_NAME_LENGTH, "CSavegameFilenames::AppendCurrentDateAndTimeToDisplayName - adding the time string pushed the string length over SAVE_GAME_MAX_DISPLAY_NAME_LENGTH characters");

	char TimeString[MAX_CHARACTERS_FOR_TIME_DATE * 2];
	fiDevice::SystemTime LocalSysTime;
//	GetLocalTime( (LPSYSTEMTIME)&LocalSysTime );
	rage::fiDevice::GetLocalSystemTime( LocalSysTime );
	FormatTheFileTimeString(&TimeString[0], LocalSysTime);

	size_t LengthOfTimeString = strlen(TimeString);
	Assertf(LengthOfTimeString < MAX_CHARACTERS_FOR_TIME_DATE, "CSavegameFilenames::AppendCurrentDateAndTimeToDisplayName - Time string is too long");

	CTextConversion::CopyAsciiStringIntoChar16Buffer(&ms_NameToDisplay[CurrentCharacterInDisplayName], TimeString, (LengthOfTimeString+1));	//	plus one for NULL character
	CurrentCharacterInDisplayName += LengthOfTimeString;
	Assertf(CurrentCharacterInDisplayName < SAVE_GAME_MAX_DISPLAY_NAME_LENGTH, "CSavegameFilenames::AppendCurrentDateAndTimeToDisplayName - adding the time string pushed the string length over SAVE_GAME_MAX_DISPLAY_NAME_LENGTH characters");
}
#endif	//	__XENON


void CSavegameFilenames::FormatTheFileTimeString(char *pStringToFill, fiDevice::SystemTime LocalSysTime)
{
	char TempString[10];
	const bool bIsJapanese = (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_JAPANESE);
	strncpy(pStringToFill, "", MAX_LENGTH_SAVE_DATE_TIME);

	if (bIsJapanese) // Japanese use YYYY/MM/DD format
	{
		for (int loop = 0; loop < 6; loop++)
		{
			switch (loop)
			{
//	#if __JAPANESE_BUILD  // Japanese use YYYY/MM/DD format
				case 0 :	//	Year (2 digits)
					{
						int LastTwoDigits = (LocalSysTime.wYear % 100);
						sprintf(TempString, "%02d/", LastTwoDigits);
					}
					break;

				case 1 :	//	Month (2 digits)
					sprintf(TempString, "%02d/", LocalSysTime.wMonth);
					break;

				case 2 :	//	Day (2 digits)
					sprintf(TempString, "%02d ", LocalSysTime.wDay);
					break;

				case 3 :	//	Hours (2 digits)
					sprintf(TempString, "%02d:", LocalSysTime.wHour);
					break;

				case 4 :	//	Minutes (2 digits)
					sprintf(TempString, "%02d:", LocalSysTime.wMinute);
					break;

				case 5 :	//	Seconds (2 digits)
					sprintf(TempString, "%02d", LocalSysTime.wSecond);
					break;
//	#else
//	#endif
			}
			safecat(pStringToFill, TempString, MAX_LENGTH_SAVE_DATE_TIME);
		}
	}
	else
	{	//	not Japanese
		for (int loop = 0; loop < 6; loop++)
		{
			switch (loop)
			{
				case 0 :	//	Month (2 digits)
					sprintf(TempString, "%02d/", LocalSysTime.wMonth);
					break;

				case 1 :	//	Day (2 digits)
					sprintf(TempString, "%02d/", LocalSysTime.wDay);
					break;

				case 2 :	//	Year (2 digits)
					{
						int LastTwoDigits = (LocalSysTime.wYear % 100);
						sprintf(TempString, "%02d ", LastTwoDigits);
					}
					break;

				case 3 :	//	Hours (2 digits)
					sprintf(TempString, "%02d:", LocalSysTime.wHour);
					break;

				case 4 :	//	Minutes (2 digits)
					sprintf(TempString, "%02d:", LocalSysTime.wMinute);
					break;

				case 5 :	//	Seconds (2 digits)
					sprintf(TempString, "%02d", LocalSysTime.wSecond);
					break;
			}
			safecat(pStringToFill, TempString, MAX_LENGTH_SAVE_DATE_TIME);
		}
	}
}


bool CSavegameFilenames::IsThisTheNameOfAnAutosaveFile(const char *pFilenameToTest)
{
	char TempFileName[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
	CreateNameOfLocalFile(TempFileName, NELEM(TempFileName), AUTOSAVE_SLOT_FOR_EPISODE_ZERO);
	if (strcmp(TempFileName, pFilenameToTest) == 0)
	{
		return true;
	}

//	CreateNameOfFileOnDisc(TempFileName, AUTOSAVE_SLOT_FOR_EPISODE_ONE);
//	if (strcmp(TempFileName, pFilenameToTest) == 0)
//	{
//		return true;
//	}

//	CreateNameOfFileOnDisc(TempFileName, AUTOSAVE_SLOT_FOR_EPISODE_TWO);
//	if (strcmp(TempFileName, pFilenameToTest) == 0)
//	{
//		return true;
//	}

	return false;
}

s32 CSavegameFilenames::GetEpisodeIndexFromAutosaveFilename(const char *pFilenameToTest)
{
	if (pFilenameToTest)
	{
		char TempFileName[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];

		CreateNameOfLocalFile(TempFileName, NELEM(TempFileName), AUTOSAVE_SLOT_FOR_EPISODE_ZERO);
		if (strcmp(TempFileName, pFilenameToTest) == 0)
		{
			return 0;
		}

// 		CreateNameOfLocalFile(TempFileName, NELEM(TempFileName), AUTOSAVE_SLOT_FOR_EPISODE_ONE);
// 		if (strcmp(TempFileName, pFilenameToTest) == 0)
// 		{
// 			return 1;
// 		}

// 		CreateNameOfLocalFile(TempFileName, NELEM(TempFileName), AUTOSAVE_SLOT_FOR_EPISODE_TWO);
// 		if (strcmp(TempFileName, pFilenameToTest) == 0)
// 		{
// 			return 2;
// 		}
	}

	return -1;
}


void CSavegameFilenames::ExtractInfoFromDisplayName(const char16 *DisplayName, int *pReturnEpisodeNumber, char *pFileTimeString, int *pFirstCharOfMissionName, int *pLengthOfMissionName)
{
	int lengthOfDisplayName = (int)wcslen(DisplayName);

	s32 firstCharacterOfDateString = 0;
	s32 finalCharacterOfMissionName = lengthOfDisplayName - 1;

#if !RSG_ORBIS
//	PS4 doesn't have the creation date at the end of the display name so we don't need to search for the hyphen separator

	int char_loop = lengthOfDisplayName - 1;
	bool bFoundLastHyphen = false;

	while ( (char_loop > 0) && (!bFoundLastHyphen) )
	{
		if (DisplayName[char_loop] == '-')
		{
			bFoundLastHyphen = true;

			if ( (char_loop < 2)
				|| (DisplayName[char_loop-1] != ' ')
				|| (DisplayName[char_loop+1] != ' ') )
			{
				Assertf(0, "CSavegameFilenames::ExtractInfoFromDisplayName - Last hyphen in display name should be preceded and followed by a space");
			}
			else
			{
				finalCharacterOfMissionName = (char_loop-2);
				firstCharacterOfDateString = (char_loop+2);
			}
		}
		char_loop--;
	}
#endif	//	!RSG_ORBIS

	if (pReturnEpisodeNumber)
	{
		*pReturnEpisodeNumber = INDEX_OF_FIRST_LEVEL;
	}

	if (firstCharacterOfDateString > 0)
	{
		int StartCharOfDate = firstCharacterOfDateString;
		int finalCharacterOfDateString = lengthOfDisplayName - 1;
		int LengthOfString = finalCharacterOfDateString - firstCharacterOfDateString + 1;
		LengthOfString = MIN((LengthOfString+1), MAX_LENGTH_SAVE_DATE_TIME);	//	Need to leave one character to store the NULL terminating character
		if (pFileTimeString)
		{
//	I think this is okay. The time string shouldn't contain any characters that don't fit in a single ASCII byte
			CTextConversion::CopyChar16StringIntoAsciiBuffer(pFileTimeString, &DisplayName[StartCharOfDate], LengthOfString);
		}
	}

	int first_character_of_display_name = 0;

	if (pFirstCharOfMissionName)
	{
		*pFirstCharOfMissionName = first_character_of_display_name;
	}

	if (pLengthOfMissionName)
	{
		*pLengthOfMissionName = finalCharacterOfMissionName - first_character_of_display_name + 1;
	}
}




/*
//	Only change the contents of pReturnIndex if this function succeeds
void CSavegameFilenames::GetFileIndexFromFileName(const char *pDiscName, int *pReturnIndex)
{
	savegameDebugf1("CSavegameFilenames::GetFileIndexFromFileName - calling for %s\n", pDiscName);
	int length_of_prefix = strlen(GetSaveGameFileOnDiscPrefix());
	if ( ((int) strlen(pDiscName)) == (length_of_prefix + 2) )
	{
		if (strncmp(pDiscName, GetSaveGameFileOnDiscPrefix(), length_of_prefix) == 0)
		{
			int FirstDigit = 0;
			int SecondDigit = 0;
			if ( (pDiscName[length_of_prefix] >= '0') && (pDiscName[length_of_prefix] <= '9') )
			{
				FirstDigit = (int) (pDiscName[length_of_prefix] - '0');

				if ( (pDiscName[length_of_prefix+1] >= '0') && (pDiscName[length_of_prefix+1] <= '9') )
				{
					SecondDigit = (int) (pDiscName[length_of_prefix+1] - '0');

					*pReturnIndex = ( (FirstDigit*10) + SecondDigit);
				}
				else
				{
					Assertf(0, "CSavegameFilenames::GetFileIndexFromFileName - expected second character after SGTA4 in save game filename to be a number");
				}
			}
			else
			{
				Assertf(0, "CSavegameFilenames::GetFileIndexFromFileName - expected first character after SGTA4 in save game filename to be a number");
			}
		}
		else
		{
			Assertf(0, "CSavegameFilenames::GetFileIndexFromFileName - save game filename does not begin with SGTA4");
		}
	}
	else
	{
		Assertf(0, "CSavegameFilenames::GetFileIndexFromFileName - save game filename is not of the expected length");
	}
}
*/


