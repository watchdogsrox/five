
#if __PPU
#include <cell/rtc.h>
#endif

// Game headers
#include "Core/game.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "savegame_damaged_check.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_load.h"
#include "SaveLoad/savegame_photo_local_list.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/savegame_new_game_checks.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Text/TextConversion.h"

SAVEGAME_OPTIMISATIONS()

fiSaveGame::Content CSavegameList::ms_SaveGameSlots[MAX_NUM_SAVE_FILES_TO_ENUMERATE];
s32		CSavegameList::ms_NumberOfSaveGameSlots;

CSavegameDisplayData CSavegameList::ms_SavegameDisplayData[MAX_NUM_SAVES_TO_SORT_INTO_SLOTS];

int CSavegameList::ms_EpisodeNumberForSaveToBeLoaded = INDEX_OF_FIRST_LEVEL;

#if RSG_ORBIS
fiSaveGame::Content CSavegameList::ms_EnumeratedPhotos[MAX_NUM_PHOTOS_TO_ENUMERATE];
#endif	//	RSG_ORBIS

#if RSG_ORBIS
bool CSavegameList::ms_bSaveGamesHaveAlreadyBeenScanned = false;
int CSavegameList::ms_SlotToUpdateOnceSaveHasCompleted = -1;
int CSavegameList::ms_SlotToUpdateOnceDeleteHasCompleted = -1;
#endif	//	RSG_ORBIS

#if RSG_ORBIS
CSavegameQueuedOperation_PS4DamagedCheck CSavegameList::ms_PS4DamagedCheck;
s32 CSavegameList::ms_NextSlotToScanForDamage = -1;
bool CSavegameList::ms_bBackgroundScanForDamagedSavegamesHasCompleted = false;
CSavegameQueuedOperation_CreateSortedListOfSavegames CSavegameList::ms_EnumerateSavegames;
#endif

void CSavegameDisplayData::Initialise()
{
	m_FirstCharacterOfMissionName = 0;
	m_LengthOfMissionName = 0;
	m_EpisodeNumber = 0;

	m_bIsDamaged = false;
	m_bHasBeenCheckedForDamage = false;

	for (int loop = 0; loop < MAX_LENGTH_SAVE_DATE_TIME; loop++)
	{
		m_SlotSaveTime[loop] = '\0';
	}

	m_SaveTime.Initialise();
}

// void CSavegameList::ClearThisSlot(int SlotIndex)
// {
// 	ms_SaveGameSlots[SlotIndex].DeviceId = 0;
// 	ms_SaveGameSlots[SlotIndex].ContentType = 0;
// 	ms_SaveGameSlots[SlotIndex].DisplayName[0] = '\0';	//	_TCHAR
// 	ms_SaveGameSlots[SlotIndex].Filename[0] = '\0';
// }


#if RSG_ORBIS
bool CSavegameList::GetSaveGamesHaveAlreadyBeenScanned()
{
	if (ShouldEnumeratePhotos())
	{
		if (CSavegamePhotoLocalList::GetListHasBeenCreated())
		{
			savegameDisplayf("CSavegameList::GetSaveGamesHaveAlreadyBeenScanned - list of local photos has already been created so return true");
			return true;
		}
	}
	else
	{
		if (ms_bSaveGamesHaveAlreadyBeenScanned)
		{
			savegameDisplayf("CSavegameList::GetSaveGamesHaveAlreadyBeenScanned - list of savegames has already been created so return true");
			return true;
		}
	}

	return false;
}

s32 CSavegameList::ConvertBackupSlotToSavegameSlot(s32 slotIndex)
{
	if(slotIndex >= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
		return   (slotIndex - INDEX_OF_BACKUPSAVE_SLOT);
	else
		return slotIndex;
}
#endif	//	RSG_ORBIS


void CSavegameList::ClearSlotData(void)
{
#if RSG_ORBIS
	if (ShouldEnumeratePhotos())
	{
		if (CSavegamePhotoLocalList::GetListHasBeenCreated())
		{
			savegameDisplayf("CSavegameList::ClearSlotData - list of local photos has already been created so return immediately");
			return;
		}

		for (u32 photoLoop = 0; photoLoop < MAX_NUM_PHOTOS_TO_ENUMERATE; photoLoop++)
		{
			ms_EnumeratedPhotos[photoLoop].Clear();
		}
	}
	else
#endif	//	RSG_ORBIS
	{
#if RSG_ORBIS
		if (ms_bSaveGamesHaveAlreadyBeenScanned)
		{
			savegameDisplayf("CSavegameList::ClearSlotData - list of savegames has already been created so return immediately");
			return;
		}
#endif

		int loop;
		for (loop = 0; loop < MAX_NUM_SAVE_FILES_TO_ENUMERATE; loop++)
		{
			ms_SaveGameSlots[loop].Clear();
		}

		for (loop = 0; loop < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS; loop++)
		{
			ms_SavegameDisplayData[loop].Initialise();
		}
	}
}

void CSavegameList::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
//	#if __PPU
//		SAVEGAME.SetMaxNumberOfSaveGameFilesToEnumerate(MAX_NUM_SAVE_FILES_TO_ENUMERATE);
//	#endif

#if RSG_ORBIS
		ms_SlotToUpdateOnceSaveHasCompleted = -1;
		ms_SlotToUpdateOnceDeleteHasCompleted = -1;
#endif	//	RSG_ORBIS
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_NumberOfSaveGameSlots = 0;

		ms_EpisodeNumberForSaveToBeLoaded = INDEX_OF_FIRST_LEVEL;
	}
}


int CSavegameList::GetAutosaveSlotNumberForCurrentEpisode()
{
/*
	s32 CurrentEpisode = CGameLogic::GetCurrentEpisodeIndex();

	switch (CurrentEpisode)
	{
		case 1 :
			return AUTOSAVE_SLOT_FOR_EPISODE_ONE;
			break;

		case 2 :
			return AUTOSAVE_SLOT_FOR_EPISODE_TWO;
			break;
	}
*/
	return AUTOSAVE_SLOT_FOR_EPISODE_ZERO;
}


// *********************************************************************************************
// FUNCTION: GetNameOfSavedGameForMenu()
//	
// Description: 
// *********************************************************************************************
const char* CSavegameList::GetNameOfSavedGameForMenu(int SlotNumber)
{
	static char TempAsciiBuffer[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
	TempAsciiBuffer[0] = '\0';

	if (savegameVerifyf( (SlotNumber >= 0) && (SlotNumber < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::GetNameOfSavedGameForMenu - slot number %d should be >= 0 and < %d", SlotNumber, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		int FirstCharacter = ms_SavegameDisplayData[SlotNumber].m_FirstCharacterOfMissionName;
		CSavegameFilenames::CopyWideStringToUtf8String(&ms_SaveGameSlots[SlotNumber].DisplayName[FirstCharacter], ms_SavegameDisplayData[SlotNumber].m_LengthOfMissionName, TempAsciiBuffer, NELEM(TempAsciiBuffer));

	//	episode = ms_EpisodeNumber[SlotNumber];
	}

	return TempAsciiBuffer;
}


bool CSavegameList::IsThisSaveGameSlotEmpty(fiSaveGame::Content &SaveGameSlot)
{
	if (SaveGameSlot.Filename[0] == '\0')
	{
		Assertf(SaveGameSlot.ContentType == 0, "CSavegameList::IsThisSaveGameSlotEmpty - Filename is empty for this slot, expected ContentType to be 0 too");
		Assertf(SaveGameSlot.DeviceId == 0, "CSavegameList::IsThisSaveGameSlotEmpty - Filename is empty for this slot, expected DeviceId to be 0 too");
		Assertf(SaveGameSlot.DisplayName[0] == '\0', "CSavegameList::IsThisSaveGameSlotEmpty - Filename is empty for this slot, expected DisplayName to be empty too");
		return true;
	}

	return false;
}

bool CSavegameList::IsSaveGameSlotEmpty(int SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::IsSaveGameSlotEmpty - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS) )
	{
		return IsThisSaveGameSlotEmpty(ms_SaveGameSlots[SlotIndex]);
	}

	return false;
}

bool CSavegameList::IsThisAnAutosaveSlot(int SlotIndex)
{
	if (SlotIndex == AUTOSAVE_SLOT_FOR_EPISODE_ZERO)
	{
		return true;
	}

// 	if (SlotIndex == AUTOSAVE_SLOT_FOR_EPISODE_ONE)
// 	{
// 		return true;
// 	}

// 	if (SlotIndex == AUTOSAVE_SLOT_FOR_EPISODE_TWO)
// 	{
// 		return true;
// 	}

	return false;
}

#if !__NO_OUTPUT
void CSavegameList::PrintFilenames()
{
	for (u32 loop = 0; loop < MAX_NUM_SAVE_FILES_TO_ENUMERATE; loop++)
	{
		if (strlen(ms_SaveGameSlots[loop].Filename) > 0)
		{
			savegameDisplayf("CSavegameList::PrintFilenames - slot %u contains %s\n", loop, ms_SaveGameSlots[loop].Filename);
		}
	}
}
#endif	//	!__NO_OUTPUT


#if RSG_ORBIS
bool CSavegameList::ShouldEnumeratePhotos()
{
	switch (CGenericGameStorage::GetSaveOperation())
	{
	case OPERATION_NONE :
	case OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES :		//	Only set by CSavegameFrontEnd::SaveGameListScreenSetup - load menu
	case OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES :		//	Only set by CSavegameFrontEnd::SaveGameListScreenSetup - manual save menu

//	case OPERATION_SCANNING_CLOUD_FOR_LOADING_PHOTOS :			//	Do I need to call CGenericGameStorage::SetSaveOperation()
//	case OPERATION_SCANNING_CLOUD_FOR_SAVING_PHOTOS :				//	for these somewhere?

	case OPERATION_LOADING_SAVEGAME_FROM_CONSOLE :	//	This also handles the autoloading of the most recent file found by CSavegameAutoload
	case OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE :
	case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
	case OPERATION_LOADING_PHOTO_FROM_CLOUD :

	case OPERATION_SAVING_SAVEGAME_TO_CONSOLE :
	case OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE :
	case OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE :
	case OPERATION_AUTOSAVING :
	case OPERATION_SAVING_MPSTATS_TO_CLOUD :
	case OPERATION_SAVING_PHOTO_TO_CLOUD :

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	case OPERATION_UPLOADING_SAVEGAME_TO_CLOUD :
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

#if RSG_ORBIS
	case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
#endif
	case OPERATION_CHECKING_FOR_FREE_SPACE :	//	Only set by CSaveConfirmationMessage
	case OPERATION_CHECKING_IF_FILE_EXISTS :
	case OPERATION_DELETING_LOCAL :
	case OPERATION_DELETING_CLOUD :
	case OPERATION_ENUMERATING_SAVEGAMES :
#if __ALLOW_LOCAL_MP_STATS_SAVES
	case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
	case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		break;

	case OPERATION_SAVING_LOCAL_PHOTO :
	case OPERATION_LOADING_LOCAL_PHOTO :
	case OPERATION_ENUMERATING_PHOTOS :		//	called by CSavegameQueuedOperation_CreateSortedListOfLocalPhotos
		return true;
//		break;
	}

	return false;
}
#endif	//	RSG_ORBIS

void CSavegameList::FillListOfLocalPhotos(fiSaveGame::Content *pArrayOfSaveGames, u32 sizeOfArray)
{
	CSavegamePhotoLocalList::Init();
	for (u32 loop = 0; loop < sizeOfArray; loop++)
	{
		if (CSavegameFilenames::IsThisTheNameOfAPhotoFile(pArrayOfSaveGames[loop].Filename))
		{
			s32 nUniqueId = CSavegameFilenames::GetPhotoUniqueIdFromFilename(pArrayOfSaveGames[loop].Filename);
			if (nUniqueId != 0)
			{
				savegameDebugf1("CSavegameList::FillListOfLocalPhotos - adding %d to local photo list\n", nUniqueId);

				CSavegamePhotoUniqueId uniqueId;
				uniqueId.Set(nUniqueId, false);
				CSavegamePhotoLocalList::Add(uniqueId);

//				safecpy(pArrayOfSaveGames[loop].Filename, "", NELEM(pArrayOfSaveGames[loop].Filename));
			}
		}
	}
	CSavegamePhotoLocalList::SetListHasBeenCreated(true);
#if !__NO_OUTPUT
	photoDisplayf("CSavegameList::FillListOfLocalPhotos - list of local photos is");
	CSavegamePhotoLocalList::DisplayContentsOfList();
#endif	//	!__NO_OUTPUT
}

bool CSavegameList::SortSaveGameSlots()
{
#if RSG_ORBIS
	if (ShouldEnumeratePhotos())
	{
		savegameAssertf(!CSavegamePhotoLocalList::GetListHasBeenCreated(), "CSavegameList::SortSaveGameSlots - don't expect to get back in here after the list of local photos has been created");

		FillListOfLocalPhotos(ms_EnumeratedPhotos, MAX_NUM_PHOTOS_TO_ENUMERATE);
		return true;
	}
	else
#endif	//	RSG_ORBIS
	{
#if RSG_ORBIS
		Assertf(!ms_bSaveGamesHaveAlreadyBeenScanned, "CSavegameList::SortSaveGameSlots - don't expect to get back in here after ms_bSaveGamesHaveAlreadyBeenScanned has been set");
#endif

		return SortSaveGameSlotsAndSizes(ms_SaveGameSlots, NULL, ms_NumberOfSaveGameSlots);
	}
}


bool CSavegameList::SortSaveGameSlotsAndSizes(fiSaveGame::Content *pArrayOfSaveGames, int *pArrayOfSaveGameSizes, int NumberOfValidSlots)
{
	u32 loop, loop2;

	if (NumberOfValidSlots > MAX_NUM_SAVE_FILES_TO_ENUMERATE)
	{
		CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_TOO_MANY_SAVE_FILES_IN_FOLDER);
		return false;
	}

#if !RSG_ORBIS
	//	On PS4, we'll enumerate the local photos separately from the savegames
	FillListOfLocalPhotos(pArrayOfSaveGames, MAX_NUM_SAVE_FILES_TO_ENUMERATE);
#endif	//	!RSG_ORBIS

	fiSaveGame::Content TempSaveGameSlot;
	int TempSaveGameSize;
	char NameToSearchFor[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
	// arrange the save games into their correct slots according to Filename
	// Only have to ensure that the first MAX_NUM_SAVES_TO_SORT_INTO_SLOTS slots are correct
	for (loop = 0; loop < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS; loop++)
	{
		bool bAlreadySwapped = false;
		CSavegameFilenames::CreateNameOfLocalFile(NameToSearchFor, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, loop);
		u32 LengthOfStringToSearchFor = ustrlen(NameToSearchFor);
		for (loop2 = 0; loop2 < MAX_NUM_SAVE_FILES_TO_ENUMERATE; loop2++)	//	start loop2 at 0 as well to deal with situations where there is only one file (e.g. savegame7) and it's in slot 0
		{
			if ( (strlen(pArrayOfSaveGames[loop2].Filename) == LengthOfStringToSearchFor) && (strcmp(pArrayOfSaveGames[loop2].Filename, NameToSearchFor) == 0) )
			{	//	Found a match
				if (bAlreadySwapped)
				{
					savegameDebugf1("CSavegameList::SortSaveGameSlotsAndSizes - two save game files with the same name %s\n", NameToSearchFor);
#if !__FINAL
					for (int debug_slot_loop = 0; debug_slot_loop < MAX_NUM_SAVE_FILES_TO_ENUMERATE; debug_slot_loop++)
					{
						savegameDebugf1("CSavegameList::SortSaveGameSlotsAndSizes - slot %d contains %s\n", debug_slot_loop, pArrayOfSaveGames[debug_slot_loop].Filename);
					}
#endif
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_TWO_SAVE_FILES_WITH_SAME_NAME);
					return false;
				}
				bAlreadySwapped = true;

				if (loop2 != loop)
				{
					TempSaveGameSlot = pArrayOfSaveGames[loop];
					pArrayOfSaveGames[loop] = pArrayOfSaveGames[loop2];
					pArrayOfSaveGames[loop2] = TempSaveGameSlot;

					if (pArrayOfSaveGameSizes)
					{
						TempSaveGameSize = pArrayOfSaveGameSizes[loop];
						pArrayOfSaveGameSizes[loop] = pArrayOfSaveGameSizes[loop2];
						pArrayOfSaveGameSizes[loop2] = TempSaveGameSize;
					}
				}
			}
		}

		if (!bAlreadySwapped)
		{	//	file doesn't exist at all so have to make sure that the corresponding slot is empty
			//	Will try swapping this slot with an empty slot in the range MAX_NUM_SAVES_TO_SORT_INTO_SLOTS to MAX_NUM_SAVE_FILES_TO_ENUMERATE
			//	If that causes problems then I'll try just clearing pArrayOfSaveGames[loop]
			if (IsThisSaveGameSlotEmpty(pArrayOfSaveGames[loop]) == false)
			{
				bool bFoundAnEmptySlot = false;
				loop2 = (MAX_NUM_SAVE_FILES_TO_ENUMERATE-1);
				while ((loop2 >= MAX_NUM_SAVES_TO_SORT_INTO_SLOTS) && !bFoundAnEmptySlot)
				{
					if (IsThisSaveGameSlotEmpty(pArrayOfSaveGames[loop2]))
					{
						bFoundAnEmptySlot = true;
					}
					else
					{
						loop2--;
					}
				}

				savegameAssertf(bFoundAnEmptySlot, "CSavegameList::SortSaveGameSlotsAndSizes - need to swap one of the first %d slots with an empty slot but there aren't any empty slots", MAX_NUM_SAVES_TO_SORT_INTO_SLOTS);
				if (bFoundAnEmptySlot)
				{
					TempSaveGameSlot = pArrayOfSaveGames[loop];
					pArrayOfSaveGames[loop] = pArrayOfSaveGames[loop2];
					pArrayOfSaveGames[loop2] = TempSaveGameSlot;

					if (pArrayOfSaveGameSizes)
					{
						TempSaveGameSize = pArrayOfSaveGameSizes[loop];
						pArrayOfSaveGameSizes[loop] = pArrayOfSaveGameSizes[loop2];
						pArrayOfSaveGameSizes[loop2] = TempSaveGameSize;
					}
				}
			}
		}
	}

	return true;
}

#if RSG_ORBIS
void CSavegameList::UpdateSlotDataAfterSave(u32 modificationTimeHigh, u32 modificationTimeLow, bool savingBackupSave)
{
	int slotToUpdateOnceSaveHasCompleted = ms_SlotToUpdateOnceSaveHasCompleted;
	if(savingBackupSave)
	{
		//make sure we get the correct slot numbers for the main save and the backup save
		if(ms_SlotToUpdateOnceSaveHasCompleted < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
			slotToUpdateOnceSaveHasCompleted = ms_SlotToUpdateOnceSaveHasCompleted + INDEX_OF_BACKUPSAVE_SLOT;
	}

	if (savegameVerifyf((slotToUpdateOnceSaveHasCompleted >= 0) && (slotToUpdateOnceSaveHasCompleted < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::UpdateSlotDataAfterSave - slot index %d is out of range 0 to %d", slotToUpdateOnceSaveHasCompleted, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		ms_SavegameDisplayData[slotToUpdateOnceSaveHasCompleted].m_bIsDamaged = false;
		ms_SavegameDisplayData[slotToUpdateOnceSaveHasCompleted].m_bHasBeenCheckedForDamage = true;

		Assertf( (sizeof(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].DisplayName) / sizeof(char16) ) == 128, "CSavegameList::UpdateSlotDataAfterSave - expected DisplayName of fiSaveGame::Content to be 128 characters long");
		Assertf(SAVE_GAME_MAX_DISPLAY_NAME_LENGTH == 128, "CSavegameList::UpdateSlotDataAfterSave - expected SAVE_GAME_MAX_DISPLAY_NAME_LENGTH to be 128");

		wcsncpy(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].DisplayName, CSavegameFilenames::GetNameToDisplay(), NELEM(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].DisplayName));


		SetModificationTime(slotToUpdateOnceSaveHasCompleted, modificationTimeHigh, modificationTimeLow);

		u64 fileTime;
		GetModificationTime(slotToUpdateOnceSaveHasCompleted, fileTime);
		time_t SaveTimeForThisSlot = static_cast<time_t>(fileTime);
		ms_SavegameDisplayData[slotToUpdateOnceSaveHasCompleted].m_SlotSaveTime[0] = '\0';
		if (ms_SavegameDisplayData[slotToUpdateOnceSaveHasCompleted].m_SaveTime.ExtractDateFromTimeT(SaveTimeForThisSlot))
		{
			ConstructStringFromDate(slotToUpdateOnceSaveHasCompleted);
		}

		savegameDisplayf("CSavegameList::UpdateSlotDataAfterSave - Timestamp of newly saved file is %s", CSavegameList::GetSlotSaveTimeString(slotToUpdateOnceSaveHasCompleted));

		ExtractInfoFromDisplayName(slotToUpdateOnceSaveHasCompleted);

//		strncpy(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].Filename, CSavegameFilenames::GetNameOfFileOnDisc(), SAVE_GAME_MAX_FILENAME_LENGTH);
		if(!savingBackupSave)
			safecpy(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].Filename, CSavegameFilenames::GetFilenameOfLocalFile(), NELEM(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].Filename));
		else
			safecpy(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].Filename, CSavegameFilenames::GetFilenameOfBackupSaveFile(), NELEM(ms_SaveGameSlots[slotToUpdateOnceSaveHasCompleted].Filename));

//		ms_SaveGameSlots[ms_SlotToUpdateOnceSaveHasCompleted].DeviceId = 0;		//	I think this is always 0 on PS3
//		ms_SaveGameSlots[ms_SlotToUpdateOnceSaveHasCompleted].ContentType = 0;	//	I think this is always 0 on PS3


//		rage::fiDevice::SystemTime tm;
//		rage::fiDevice::GetLocalSystemTime( tm );
//		char temp[128];
//		sprintf( temp, "%04d%02d%02d%02d%02d%02d", tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond );
//		savegameDisplayf("CSavegameList::UpdateSlotDataAfterSave - Current clock time is %s", temp);

// 		CellRtcDateTime CurrentRtcDateTime;
// 		time_t CurrentTimeT;
// 		cellRtcGetCurrentClock(&CurrentRtcDateTime, 0);
// 		cellRtcConvertDateTimeToTime_t(&CurrentRtcDateTime, &CurrentTimeT);
// 		ms_SavegameDisplayData[ms_SlotToUpdateOnceSaveHasCompleted].m_SlotSaveTime[0] = '\0';
// 		if (ms_SavegameDisplayData[ms_SlotToUpdateOnceSaveHasCompleted].m_SaveTime.ExtractDateFromTimeT(CurrentTimeT))
// 		{
// 			ConstructStringFromDate(ms_SlotToUpdateOnceSaveHasCompleted);
// 		}
// 		savegameDisplayf("CSavegameList::UpdateSlotDataAfterSave - Current clock time is %s", CSavegameList::GetSlotSaveTimeString(ms_SlotToUpdateOnceSaveHasCompleted));
	}
}

void CSavegameList::UpdateSlotDataAfterDelete()
{
	if (savegameVerifyf((ms_SlotToUpdateOnceDeleteHasCompleted >= 0) && (ms_SlotToUpdateOnceDeleteHasCompleted < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::UpdateSlotDataAfterDelete - slot index %d is out of range 0 to %d", ms_SlotToUpdateOnceDeleteHasCompleted, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		savegameDisplayf("CSavegameList::UpdateSlotDataAfterDelete - clearing the data for slot %d", ms_SlotToUpdateOnceDeleteHasCompleted);
		ms_SaveGameSlots[ms_SlotToUpdateOnceDeleteHasCompleted].Clear();
		ms_SavegameDisplayData[ms_SlotToUpdateOnceDeleteHasCompleted].Initialise();
	}
}
#endif	//	RSG_ORBIS

#if RSG_ORBIS
void CSavegameList::BackgroundScanForDamagedSavegames()
{
	if (!ms_bBackgroundScanForDamagedSavegamesHasCompleted)
	{
		static bool s_bEnumerationHasBeenQueued = false;
		if (!CSavegameNewGameChecks::ShouldNewGameChecksBePerformed() && !CNetwork::IsNetworkOpen() && !CNetwork::HasCalledFirstEntryOpen())
		{	//	Do I need to check this? When this returns False, I'm treating the new game checks as complete
			if  (!ms_bSaveGamesHaveAlreadyBeenScanned)
			{
				if (!s_bEnumerationHasBeenQueued)
				{
					if (ms_EnumerateSavegames.GetStatus() != MEM_CARD_BUSY)
					{
						ms_EnumerateSavegames.Init(false);
						CGenericGameStorage::PushOnToSavegameQueue(&ms_EnumerateSavegames);
						s_bEnumerationHasBeenQueued = true;
					}
				}
			}
			else
			{	//	Only check for damage after the files have been enumerated, sorted and their times have been read
				if (ms_PS4DamagedCheck.GetStatus() != MEM_CARD_BUSY)
				{
					//	Use TOTAL_NUMBER_OF_FILES_TO_SCAN_FOR_DAMAGE instead of MAX_NUM_SAVES_TO_SORT_INTO_SLOTS to save a bit of time.
					//	This will avoid checking the following three files for damage - the Profile, the Mission Repeat Save, the Replay Save
					const s32 TOTAL_NUMBER_OF_FILES_TO_SCAN_FOR_DAMAGE = TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES + NUM_BACKUPSAVE_SLOTS;

					bool bFoundASaveToScan = false;
					ms_NextSlotToScanForDamage++;

					while (!bFoundASaveToScan && (ms_NextSlotToScanForDamage < TOTAL_NUMBER_OF_FILES_TO_SCAN_FOR_DAMAGE) )
					{
						if (IsSaveGameSlotEmpty(ms_NextSlotToScanForDamage))
						{
							savegameDisplayf("CSavegameList::BackgroundScanForDamagedSavegames - slot %d is empty so don't scan it for damage", ms_NextSlotToScanForDamage);
						}
						else
						{
							eSavegameFileType fileTypeForThisSlot = FindSavegameFileTypeForThisSlot(ms_NextSlotToScanForDamage);

							switch (fileTypeForThisSlot)
							{
								case SG_FILE_TYPE_SAVEGAME :
									bFoundASaveToScan = true;
									break;

								case SG_FILE_TYPE_SAVEGAME_BACKUP :
									{
										s32 correspondingMainSaveSlot = ConvertBackupSlotToSavegameSlot(ms_NextSlotToScanForDamage);
#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
										if (IsThisAnAutosaveSlot(correspondingMainSaveSlot))
										{	//	When using Save Data Memory for the main autosave or Download0 for the backup autosave, we need to use the most recent of the main and backup
											//	so we always need to check if the backup is damaged even if the main is fine.
											bFoundASaveToScan = true;
										}
										else
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
										{
											if (!IsSaveGameSlotEmpty(correspondingMainSaveSlot) && HasBeenCheckedForDamage(correspondingMainSaveSlot) && !GetIsDamaged(correspondingMainSaveSlot))
											{
												savegameDisplayf("CSavegameList::BackgroundScanForDamagedSavegames - the corresponding main save slot is not damaged so we don't need to check backup slot %d", ms_NextSlotToScanForDamage);
											}
											else
											{
												bFoundASaveToScan = true;
											}
										}
									}
									break;

								default :
									savegameAssertf(0, "CSavegameList::BackgroundScanForDamagedSavegames - filetype %d for slot %d not handled", (s32) fileTypeForThisSlot, ms_NextSlotToScanForDamage);
									break;
							}
						}

						if (!bFoundASaveToScan)
						{
							ms_NextSlotToScanForDamage++;
						}
					}

					if (ms_NextSlotToScanForDamage == TOTAL_NUMBER_OF_FILES_TO_SCAN_FOR_DAMAGE)
					{
						ms_bBackgroundScanForDamagedSavegamesHasCompleted = true;
						savegameDebugf1("CSavegameList::BackgroundScanForDamagedSavegames has finished. Time = %d\n",sysTimer::GetSystemMsTime());
					}
					else
					{
						//check whether we are in a good condition to issue this request
						//if not roll back the last checked slot and continue 
						if( CGenericGameStorage::IsSafeToUseSaveLibrary())
						{
							eSavegameFileType fileTypeForThisSlot = FindSavegameFileTypeForThisSlot(ms_NextSlotToScanForDamage);
							switch (fileTypeForThisSlot)
							{
								case SG_FILE_TYPE_SAVEGAME :
									savegameDisplayf("CSavegameList::BackgroundScanForDamagedSavegames - slot %d is a main save slot. About to scan it for damage", ms_NextSlotToScanForDamage);
									break;
								case SG_FILE_TYPE_SAVEGAME_BACKUP :
									savegameDisplayf("CSavegameList::BackgroundScanForDamagedSavegames - slot %d is a backup save slot. About to scan it for damage", ms_NextSlotToScanForDamage);
									break;
								default:
									savegameDisplayf("CSavegameList::BackgroundScanForDamagedSavegames - slot %d is a ??? save slot. About to scan it for damage", ms_NextSlotToScanForDamage);
									break;
							}
							ms_PS4DamagedCheck.Init(ms_NextSlotToScanForDamage);
							CGenericGameStorage::PushOnToSavegameQueue(&ms_PS4DamagedCheck);
						}
						else
						{
							ms_NextSlotToScanForDamage--;
						}
					}
				}
			}
		}
	}
}
#endif	// RSG_ORBIS


bool CSavegameList::BeginEnumeration()
{
#if RSG_ORBIS
	if (ShouldEnumeratePhotos())
	{
		return SAVEGAME.BeginPhotoEnumeration(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), ms_EnumeratedPhotos, MAX_NUM_PHOTOS_TO_ENUMERATE);
	}
	else
#endif	//	RSG_ORBIS
	{
#if __PPU
		SAVEGAME.SetMaxNumberOfSaveGameFilesToEnumerate(MAX_NUM_SAVE_FILES_TO_ENUMERATE);
#endif
		return SAVEGAME.BeginEnumeration(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), ms_SaveGameSlots, MAX_NUM_SAVE_FILES_TO_ENUMERATE);
	}
}

bool CSavegameList::CheckEnumeration()
{
	return SAVEGAME.CheckEnumeration(CSavegameUsers::GetUser(), ms_NumberOfSaveGameSlots);
}

#define __DO_DAMAGED_CHECKS_FOR_PHOTOS	(0)

MemoryCardError CSavegameList::CheckIfSlotIsDamaged(int SlotIndex)
{
	if ( (SlotIndex < 0) || (SlotIndex >= MAX_NUM_SAVES_TO_SORT_INTO_SLOTS) )
	{
		savegameAssertf(0, "CSavegameList::CheckIfSlotIsDamaged - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS);
		return MEM_CARD_ERROR;
	}

#if RSG_ORBIS
	if (ms_SavegameDisplayData[SlotIndex].m_bHasBeenCheckedForDamage)
	{
		// Checking if a slot is damaged takes a long time on PS3
		// If it's already been scanned once then I'll just assume that I can reuse the value of m_bIsDamaged
		return MEM_CARD_COMPLETE;
	}
#endif	//	RSG_ORBIS

	MemoryCardError returnValue = CSavegameDamagedCheck::CheckIfSlotIsDamaged(ms_SaveGameSlots[SlotIndex].Filename, &ms_SavegameDisplayData[SlotIndex].m_bIsDamaged);

	if (MEM_CARD_COMPLETE == returnValue)
	{
		ms_SavegameDisplayData[SlotIndex].m_bHasBeenCheckedForDamage = true;
	}

	return returnValue;
}




MemoryCardError CSavegameList::GetTimeAndMissionNameFromDisplayName(int SaveGameSlot)
{
	//	I'll try MAX_NUM_SAVES_TO_SORT_INTO_SLOTS here for now, but this function does expect the display name
	//	in a specific format that may only apply for savegames so I might have to change the assert to check
	//	TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES and avoid calling this function for other file types
	savegameAssertf( (SaveGameSlot >= 0) && (SaveGameSlot < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::GetTimeAndMissionNameFromDisplayName - slot number %d should be between 0 and %d", SaveGameSlot, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS);
	ms_SavegameDisplayData[SaveGameSlot].m_SlotSaveTime[0] = '\0';
	ms_SavegameDisplayData[SaveGameSlot].m_EpisodeNumber = 0;
	ms_SavegameDisplayData[SaveGameSlot].m_FirstCharacterOfMissionName = 0;
	ms_SavegameDisplayData[SaveGameSlot].m_LengthOfMissionName = 0;
//	What about these two?
//	static time_t		ms_PS3SaveTimes[MAX_NUM_SAVE_GAMES_TO_DISPLAY];
//	static bool			ms_bIsDamaged[MAX_NUM_SAVE_GAMES_TO_DISPLAY];

	if (!IsSaveGameSlotEmpty(SaveGameSlot))
	{
#if __DEV
		static char TempAsciiBuffer[128];
		CSavegameFilenames::CopyWideStringToUtf8String(ms_SaveGameSlots[SaveGameSlot].DisplayName, (int)wcslen(ms_SaveGameSlots[SaveGameSlot].DisplayName), TempAsciiBuffer, NELEM(TempAsciiBuffer));
		savegameDebugf3("CSavegameList::GetTimeAndMissionNameFromDisplayName - slot %d has name %s\n", SaveGameSlot, TempAsciiBuffer);
#endif
		savegameDebugf3("CSavegameList::GetTimeAndMissionNameFromDisplayName - slot %d has file name %s\n", SaveGameSlot, ms_SaveGameSlots[SaveGameSlot].Filename);

#if RSG_ORBIS
		u64 fileTime;
		GetModificationTime(SaveGameSlot, fileTime);
		time_t SaveTimeForThisSlot = static_cast<time_t>(fileTime);

		if (ms_SavegameDisplayData[SaveGameSlot].m_SaveTime.ExtractDateFromTimeT(SaveTimeForThisSlot))
		{
			ConstructStringFromDate(SaveGameSlot);
		}
#endif

		ExtractInfoFromDisplayName(SaveGameSlot);
#if !RSG_ORBIS
		ms_SavegameDisplayData[SaveGameSlot].m_SaveTime.ExtractDateFromString(GetSlotSaveTimeString(SaveGameSlot));
#endif
		savegameDebugf3("CSavegameList::GetTimeAndMissionNameFromDisplayName - slot %d saved at %s\n", SaveGameSlot, GetSlotSaveTimeString(SaveGameSlot));
	}

	return MEM_CARD_COMPLETE;
}


void CSavegameList::ResetFileTimeScan()
{
#if RSG_ORBIS
	if (ShouldEnumeratePhotos())
	{
		savegameDisplayf("CSavegameList::ResetFileTimeScan - enumerating photos so return immediately");
		return;
	}
	else
#endif	//	RSG_ORBIS
	{
#if RSG_ORBIS
		if (ms_bSaveGamesHaveAlreadyBeenScanned)
		{
			return;
		}
#endif

		//	Make sure this doesn't happen half-way through the load
		ms_EpisodeNumberForSaveToBeLoaded = INDEX_OF_FIRST_LEVEL;
		// establish the default episode for this disc - if no suitable autoload is found then boot into this episode
	//	ms_EpisodeNumberForSaveToBeLoaded = EXTRACONTENT.GetDiscDefaultEpisode();
	//	if (EXTRACONTENT.GetIsDLCFromDisc_())
	//	{
	//		if (ms_AutoLoadEpisodeForDiscBuild != -1)
	//		{
	//			ms_EpisodeNumberForSaveToBeLoaded = ms_AutoLoadEpisodeForDiscBuild;
	//		}
	//	}

		for (int loop = 0; loop < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS; loop++)
		{
			ms_SavegameDisplayData[loop].m_SaveTime.Initialise();
		}
	}
}

MemoryCardError CSavegameList::GetFileTimes()
{
#if RSG_ORBIS
	if (ShouldEnumeratePhotos())
	{
		savegameDisplayf("CSavegameList::GetFileTimes - enumerating photos so return immediately");
		return MEM_CARD_COMPLETE;
	}
#endif	//	RSG_ORBIS


	s32 CurrentSlotForGetFileTime = 0;
#if RSG_ORBIS
	if (ms_bSaveGamesHaveAlreadyBeenScanned)
	{
		// Rather than get the file times for all the slots again (which takes a long time on PS3),
		//	we'll try just using the existing values in ms_PS3SaveTimes
		return MEM_CARD_COMPLETE;
	}
#endif

	// I used to call CheckIfSlotIsDamaged (on 360 anyway) before calling GetTimeAndMissionNameFromDisplayName
	// I've removed the damaged check from here to speed up the time to display the Load/Save menus
	// Hopefully the time can still be read from damaged saves
	// The damaged check will now need to be done where necessary at some point after GetFileTimes()

	//	Graeme 21.09.12 - I'm going to try getting the date for photos too. This is so that the save photo routine knows which is the oldest photo for overwriting
	//	I'll also have to save the date and time in the photo's display name on 360

	CurrentSlotForGetFileTime = 0;
	while (CurrentSlotForGetFileTime < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS)
	{	//	Get the file times for all saves
		 if (!IsSaveGameSlotEmpty(CurrentSlotForGetFileTime))
		 {
			savegameAssertf(!CSavegameFilenames::IsThisTheNameOfAPhotoFile(ms_SaveGameSlots[CurrentSlotForGetFileTime].Filename), "CSavegameList::GetFileTimes - didn't expect to find any photos in this function");

			if ( (CSavegameFilenames::IsThisTheNameOfASavegameFile(ms_SaveGameSlots[CurrentSlotForGetFileTime].Filename))
#if RSG_ORBIS
				|| (CSavegameFilenames::IsThisTheNameOfABackupSavegameFile(ms_SaveGameSlots[CurrentSlotForGetFileTime].Filename))
#endif
//				|| (CSavegameFilenames::IsThisTheNameOfAPhotoFile(ms_SaveGameSlots[CurrentSlotForGetFileTime].Filename)) 
				)
			{	//	Ignore any file that does not begin with the save game or photo prefix

				MemoryCardError FileTimeReadStatus = GetTimeAndMissionNameFromDisplayName(CurrentSlotForGetFileTime);
				if (FileTimeReadStatus == MEM_CARD_ERROR)
				{
					return MEM_CARD_ERROR;
				}
				else if (FileTimeReadStatus == MEM_CARD_BUSY)
				{
					savegameAssertf(0, "CSavegameList::GetFileTimes - didn't expect GetTimeAndMissionNameFromDisplayName to return MEM_CARD_BUSY");
				}
			}
		 }

		CurrentSlotForGetFileTime++;
	}

#if RSG_ORBIS
//	By this stage, the slots will have been sorted and all the times will have been grabbed.
//	Normally we'll get here through the SlowPS3Scan but if the slow scan has been
//	switched off then we'll get here when the save/load menu is first displayed.
	ms_bSaveGamesHaveAlreadyBeenScanned = true;
#endif

	return MEM_CARD_COMPLETE;
}

MemoryCardError CSavegameList::BeginGameLoad(int SlotNumber)
{
	if (!savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameList::BeginGameLoad - SaveOperation is not OPERATION_NONE"))
	{
		return MEM_CARD_ERROR;
	}

	if (savegameVerifyf(CSavegameLoad::GetLoadStatus() == GENERIC_LOAD_DO_NOTHING, "CSavegameList::BeginGameLoad - expected LoadStatus to be GENERIC_LOAD_DO_NOTHING"))
	{
		CSavegameFilenames::MakeValidSaveNameForLocalFile(SlotNumber);

		CSavegameLoad::BeginLoad(SAVEGAME_SINGLE_PLAYER);
		CNetworkTelemetry::LoadGame();
	}

	return MEM_CARD_COMPLETE;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
MemoryCardError CSavegameList::BeginGameLoadForExport(int SlotNumber)
{
	if (!savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameList::BeginGameLoadForExport - SaveOperation is not OPERATION_NONE"))
	{
		return MEM_CARD_ERROR;
	}

	if (savegameVerifyf(CSavegameLoad::GetLoadStatus() == GENERIC_LOAD_DO_NOTHING, "CSavegameList::BeginGameLoadForExport - expected LoadStatus to be GENERIC_LOAD_DO_NOTHING"))
	{
		CSavegameFilenames::MakeValidSaveNameForLocalFile(SlotNumber);

		CSavegameLoad::BeginLoadForExport();
	}

	return MEM_CARD_COMPLETE;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES



void CSavegameList::SetEpisodeNumberForTheSaveGameToBeLoadedToThisSlot(int SlotNumber)
{
	//	This shouldn't be called for photos, MP stats or PS3 profiles so use  so check in range 0 to TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES
#if RSG_ORBIS
	if (savegameVerifyf((SlotNumber >= 0) && (SlotNumber < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS), "CSavegameList::SetEpisodeNumberForTheSaveGameToBeLoadedToThisSlot - SlotNumber %d is out of range 0 to %d", SlotNumber, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS))
#else
	if (savegameVerifyf((SlotNumber >= 0) && (SlotNumber < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameList::SetEpisodeNumberForTheSaveGameToBeLoadedToThisSlot - SlotNumber %d is out of range 0 to %d", SlotNumber, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES))
#endif
	{
		ms_EpisodeNumberForSaveToBeLoaded = ms_SavegameDisplayData[SlotNumber].m_EpisodeNumber;
	}
}


//	*************** Access functions for ms_SaveGameSlots ***************************************
#if RSG_ORBIS
void CSavegameList::GetModificationTime(int SlotIndex, u64 &ModificationTime)
{
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE), "CSavegameList::GetModificationTime - slot index is out of range"))
	{
		ModificationTime = static_cast<u64>(ms_SaveGameSlots[SlotIndex].ModificationTimeHigh) << 32;
		ModificationTime += static_cast<u64>(ms_SaveGameSlots[SlotIndex].ModificationTimeLow);
	}
	else
	{
		ModificationTime = 0;
	}
}

void CSavegameList::SetModificationTime(int SlotIndex, u32 modificationTimeHigh, u32 modificationTimeLow)
{
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE), "CSavegameList::SetModificationTime - slot index is out of range"))
	{
		ms_SaveGameSlots[SlotIndex].ModificationTimeHigh = modificationTimeHigh;
		ms_SaveGameSlots[SlotIndex].ModificationTimeLow = modificationTimeLow;
	}
}
#endif	//	RSG_ORBIS

s32 CSavegameList::FindFreeManualSaveSlot()
{
	for (s32 slot_index = 0; slot_index < NUM_MANUAL_SAVE_SLOTS; slot_index++)
	{
		if (IsSaveGameSlotEmpty(slot_index))
		{
			return slot_index;
		}		
	}

	return -1;
}


//	CSavegameList::FindMostRecentSavegame
//	
//	I've based this on CSavegameAutoload::GetFileTimes()
//	CSavegameAutoload::GetFileTimes() ignores backup autosaves if the main autosave doesn't exist.
//	It also never attempts to load backup manual saves. I can't remember if there was a good reason for that.
//	The backup manual saves are stored in the normal savegame folder. (Only the backup of the autosave is stored in download0).
//	So the player could delete a main manual save through the PS4 UI but leave its backup.
//	For now, I won't consider backup manual saves.
//	
//	CSavegameFrontEnd::CreateListSortedByDate() does consider backup manual saves if the main manual save doesn't exist.
s32 CSavegameList::FindMostRecentSavegame()
{
	s32 mostRecentSlot = -1;
	CDate dateOfMostRecentSlot;

	for (s32 loop = 0; loop < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS; loop++)
	{
		if (!IsSaveGameSlotEmpty(loop))
		{
//			if (!GetIsDamaged(loop))	//	Unfortunately, I can't check this here as there's no guarantee that the saves have all been checked for damage at this stage
			{
				savegameAssertf(!CSavegameFilenames::IsThisTheNameOfAPhotoFile(ms_SaveGameSlots[loop].Filename), "CSavegameList::FindMostRecentSavegame - didn't expect to find any photos in this function");

				bool bCheckTheDateOfThisSave = false;

				if (CSavegameFilenames::IsThisTheNameOfASavegameFile(ms_SaveGameSlots[loop].Filename))
				{
					bCheckTheDateOfThisSave = true;
				}
#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
				else if (CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(ms_SaveGameSlots[loop].Filename))
				{
					s32 indexOfCorrespondingMainAutosave = CSavegameList::ConvertBackupSlotToSavegameSlot(loop);

					if (savegameVerifyf( (indexOfCorrespondingMainAutosave >= 0) && (indexOfCorrespondingMainAutosave < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::FindMostRecentSavegame - ConvertBackupSlotToSavegameSlot returned %d. Expected it to be between 0 and %d", indexOfCorrespondingMainAutosave, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
					{
						if (!IsSaveGameSlotEmpty(indexOfCorrespondingMainAutosave))
						{
							if (savegameVerifyf(CSavegameFilenames::IsThisTheNameOfAnAutosaveFile(ms_SaveGameSlots[indexOfCorrespondingMainAutosave].Filename), "CSavegameList::FindMostRecentSavegame - slot %d is not an autosave", indexOfCorrespondingMainAutosave))
							{
								bCheckTheDateOfThisSave = true;		//	We'll only check the date of a backup autosave if the corresponding main autosave exists
							}
						}
					}
				}
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

				if (bCheckTheDateOfThisSave)
				{
					if (GetSaveTime(loop) > dateOfMostRecentSlot)
					{
						dateOfMostRecentSlot = GetSaveTime(loop);
						mostRecentSlot = loop;
					}
				}
			}
		}
	}

	return mostRecentSlot;
}


eSavegameFileType CSavegameList::FindSavegameFileTypeForThisSlot(s32 SlotIndex)
{
	if (SlotIndex >= 0)
	{
#if RSG_ORBIS
		if (SlotIndex < INDEX_OF_BACKUPSAVE_SLOT)
#elif __ALLOW_LOCAL_MP_STATS_SAVES
		if (SlotIndex < INDEX_OF_FIRST_MULTIPLAYER_STATS_SAVE_SLOT)
#else
		if (SlotIndex < INDEX_OF_MISSION_REPEAT_SAVE_SLOT)
#endif
		{
			return SG_FILE_TYPE_SAVEGAME;
		}
#if RSG_ORBIS
		else if (SlotIndex < (INDEX_OF_BACKUPSAVE_SLOT + NUM_BACKUPSAVE_SLOTS) )
		{
			return SG_FILE_TYPE_SAVEGAME_BACKUP;
		}
#endif
#if __ALLOW_LOCAL_MP_STATS_SAVES
		else if (SlotIndex < INDEX_OF_MISSION_REPEAT_SAVE_SLOT)
		{
			return SG_FILE_TYPE_MULTIPLAYER_STATS;
		}
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		else if (SlotIndex < (INDEX_OF_MISSION_REPEAT_SAVE_SLOT + NUM_MISSION_REPEAT_SAVE_SLOTS) )
		{
			return SG_FILE_TYPE_MISSION_REPEAT;
		}
#if GTA_REPLAY
		else if (SlotIndex < (INDEX_OF_REPLAYSAVE_SLOT + NUM_REPLAYSAVE_SLOTS) )
		{
			return SG_FILE_TYPE_REPLAY;
		}
#endif	//	GTA_REPLAY
#if USE_PROFILE_SAVEGAME
		else if (SlotIndex < (INDEX_OF_PS3_PROFILE_SLOT + NUM_PS3_PROFILE_SLOTS) )
		{	//	PS3 Profile - there should only be one of these
			return SG_FILE_TYPE_PS3_PROFILE;
		}
#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
		else if (SlotIndex < (INDEX_OF_BACKUP_PROFILE_SLOT + NUM_BACKUP_PROFILE_SLOTS) )
		{	//	Backup Profile - there should only be one of these
			return SG_FILE_TYPE_BACKUP_PROFILE;
		}
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP
#endif	//	USE_PROFILE_SAVEGAME
		else if (SlotIndex < (INDEX_OF_FIRST_LOCAL_PHOTO + NUMBER_OF_LOCAL_PHOTOS) )
		{
			return SG_FILE_TYPE_LOCAL_PHOTO;
		}
	}

	savegameAssertf(0, "CSavegameList::FindSavegameFileTypeForThisSlot - SlotIndex %d is out of range 0 to %d", SlotIndex, MAX_NUM_EXPECTED_SAVE_FILES);
	return SG_FILE_TYPE_UNKNOWN;
}

s32 CSavegameList::GetBaseIndexForSavegameFileType(eSavegameFileType FileType)
{
	switch (FileType)
	{
		case SG_FILE_TYPE_UNKNOWN :
			savegameAssertf(0, "CSavegameList::GetBaseIndexForSavegameFileType - didn't expect SG_FILE_TYPE_UNKNOWN to ever occur");
			return -1;
//			break;

		case SG_FILE_TYPE_SAVEGAME :
			return 0;
//			break;
#if RSG_ORBIS
		case SG_FILE_TYPE_SAVEGAME_BACKUP :
			return INDEX_OF_BACKUPSAVE_SLOT;
//			break;
#endif

#if __ALLOW_LOCAL_MP_STATS_SAVES
		case SG_FILE_TYPE_MULTIPLAYER_STATS :
			return INDEX_OF_FIRST_MULTIPLAYER_STATS_SAVE_SLOT;
//			break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

		case SG_FILE_TYPE_MISSION_REPEAT :	//	Used by Andrew Minghella
			return INDEX_OF_MISSION_REPEAT_SAVE_SLOT;
//			break;
#if GTA_REPLAY
		case SG_FILE_TYPE_REPLAY :
			return INDEX_OF_REPLAYSAVE_SLOT;
//			break;
#endif	//	GTA_REPLAY
#if USE_PROFILE_SAVEGAME
		case SG_FILE_TYPE_PS3_PROFILE :
			return INDEX_OF_PS3_PROFILE_SLOT;
//			break;

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP
		case SG_FILE_TYPE_BACKUP_PROFILE :
			return INDEX_OF_BACKUP_PROFILE_SLOT;
//			break;
#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#endif	//	USE_PROFILE_SAVEGAME

		case SG_FILE_TYPE_LOCAL_PHOTO :
			return INDEX_OF_FIRST_LOCAL_PHOTO;
//			break;
	}

	return -1;
}


//	*************** End of Access functions for ms_SaveGameSlots ********************************


//	*************** Access functions for ms_SavegameDisplayData ***************************************
void CSavegameList::ConstructStringFromDate(int SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::ConstructStringFromDate - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		ms_SavegameDisplayData[SlotIndex].m_SaveTime.ConstructStringFromDate(&ms_SavegameDisplayData[SlotIndex].m_SlotSaveTime[0], MAX_LENGTH_SAVE_DATE_TIME);
	}
}

void CSavegameList::ExtractInfoFromDisplayName(int SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::ExtractInfoFromDisplayName - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		CSavegameFilenames::ExtractInfoFromDisplayName(&ms_SaveGameSlots[SlotIndex].DisplayName[0], &ms_SavegameDisplayData[SlotIndex].m_EpisodeNumber, 
#if RSG_ORBIS
			NULL, 
#else
			&ms_SavegameDisplayData[SlotIndex].m_SlotSaveTime[0], 
#endif
			&ms_SavegameDisplayData[SlotIndex].m_FirstCharacterOfMissionName, &ms_SavegameDisplayData[SlotIndex].m_LengthOfMissionName);
	}
}

const char* CSavegameList::GetSlotSaveTimeString(int SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::GetSlotSaveTimeString - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		Assertf(!ms_SavegameDisplayData[SlotIndex].m_bIsDamaged, "CSavegameList::GetSlotSaveTimeString - attempting to get the time and date of a damaged slot");
		return &(ms_SavegameDisplayData[SlotIndex].m_SlotSaveTime[0]);
	}
	return NULL;
}

bool CSavegameList::GetIsDamaged(int SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::GetIsDamaged - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		savegameAssertf(ms_SavegameDisplayData[SlotIndex].m_bHasBeenCheckedForDamage, "CSavegameList::GetIsDamaged - this should only be called after CheckIfSlotIsDamaged has been called - Graeme");
		return ms_SavegameDisplayData[SlotIndex].m_bIsDamaged;
	}
	return true;
}

#if RSG_ORBIS
bool CSavegameList::HasBeenCheckedForDamage(int SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::HasBeenCheckedForDamage - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		return ms_SavegameDisplayData[SlotIndex].m_bHasBeenCheckedForDamage;
	}

	return false;
}
#endif	//	__PPU

CDate &CSavegameList::GetSaveTime(s32 SlotIndex)
{
	if (savegameVerifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVES_TO_SORT_INTO_SLOTS), "CSavegameList::GetSaveTime - slot index %d is out of range 0 to %d", SlotIndex, MAX_NUM_SAVES_TO_SORT_INTO_SLOTS))
	{
		return ms_SavegameDisplayData[SlotIndex].m_SaveTime;
	}

	static CDate InvalidDate;
	InvalidDate.Initialise();

	return InvalidDate;
}

void CSavegameList::GetDisplayNameAndDateForThisSaveGameItem(s32 SlotIndex, char *pNameToFillIn, char *pDateToFillIn, bool *pbReturnSlotIsEmpty, bool *pbReturnSlotIsDamaged)
{
	if (pbReturnSlotIsEmpty)
	{
		*pbReturnSlotIsEmpty = true;
	}
	else
	{
		Assertf(0, "CSavegameList::GetDisplayNameAndDateForThisSaveGameItem - you need to pass this function a pointer to a bool to hold SlotIsEmpty");
		return;
	}
	
	if (pbReturnSlotIsDamaged)
	{
		*pbReturnSlotIsDamaged = false;
	}
	else
	{
		Assertf(0, "CSavegameList::GetDisplayNameAndDateForThisSaveGameItem - you need to pass this function a pointer to a bool to hold SlotIsDamaged");
		return;
	}

	if (!pNameToFillIn)
	{
		Assertf(0, "CSavegameList::GetDisplayNameAndDateForThisSaveGameItem - you need to pass this function a pointer to a string to hold the name");
		return;
	}

	if (!pDateToFillIn)
	{
		Assertf(0, "CSavegameList::GetDisplayNameAndDateForThisSaveGameItem - you need to pass this function a pointer to a string to hold the date");
		return;
	}

	if (!IsSaveGameSlotEmpty(SlotIndex))  // check if valid data in slot:
	{
		if (GetIsDamaged(SlotIndex))
		{
			*pbReturnSlotIsEmpty = false;				
			*pbReturnSlotIsDamaged = true;
		}
		else
		{
			strcpy(pDateToFillIn, GetSlotSaveTimeString(SlotIndex));
			Displayf("Save Game %d saved at %s\n", SlotIndex, pDateToFillIn);

//	Remove the seconds from the time/date
			int LengthOfSaveGameDate = istrlen(pDateToFillIn);
			Assertf( (LengthOfSaveGameDate == 0) || (LengthOfSaveGameDate == 17), "CSavegameList::GetDisplayNameAndDateForThisSaveGameItem - expected string containing date to be 17 characters long");
			if (LengthOfSaveGameDate == 17)
			{
				if (pDateToFillIn[14] == ':')
				{
					pDateToFillIn[14] = '\0';
				}
				else
				{
					Assertf(0, "CSavegameList::GetDisplayNameAndDateForThisSaveGameItem - expected a : followed by two digits at the end of the date/time string");
				}
			}

			strcpy(pNameToFillIn, GetNameOfSavedGameForMenu(SlotIndex));

			*pbReturnSlotIsEmpty = false;				
		}
	}
}

//	*************** End of Access functions for ms_SavegameDisplayData ********************************

