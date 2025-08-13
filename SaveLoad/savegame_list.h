
#ifndef SAVEGAME_LIST_H
#define SAVEGAME_LIST_H


// Rage headers
#include "file\savegame.h"

// Game headers
#include "SaveLoad/savegame_date.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_queued_operations.h"

//	Allow a bit more than MAX_NUM_SAVE_GAMES_TO_DISPLAY so that the profile file doesn't take up a slot
//	that would have been used by a proper savegame during enumeration. It also leaves a bit of leeway for
//	other files that might be in the folder and gives a bit of working space for the sort function.
#if RSG_ORBIS
	#define MAX_NUM_PHOTOS_TO_ENUMERATE			(NUMBER_OF_LOCAL_PHOTOS * 2)
	#define MAX_NUM_SAVE_FILES_TO_ENUMERATE		(MAX_NUM_SAVES_TO_SORT_INTO_SLOTS * 2)		//	On PS4, we'll enumerate photos separately from savegames
#else
	#define MAX_NUM_SAVE_FILES_TO_ENUMERATE		(MAX_NUM_EXPECTED_SAVE_FILES * 2)			//	On other platforms, we'll enumerate photos and savegames together
#endif	//	RSG_ORBIS

//	Forward declarations
class sveDict;

struct CSavegameDisplayData
{
public:
//	Public data
	int m_FirstCharacterOfMissionName;
	int	m_LengthOfMissionName;
	int	m_EpisodeNumber;
	char m_SlotSaveTime[MAX_LENGTH_SAVE_DATE_TIME];
	CDate m_SaveTime;
	bool m_bIsDamaged;
	bool m_bHasBeenCheckedForDamage;

//	Public functions
	void Initialise();
};

class CSavegameList
{
private:
//	Private data
	static CSavegameDisplayData ms_SavegameDisplayData[MAX_NUM_SAVES_TO_SORT_INTO_SLOTS];

	static fiSaveGame::Content ms_SaveGameSlots[MAX_NUM_SAVE_FILES_TO_ENUMERATE];
	static s32 ms_NumberOfSaveGameSlots;

	static int ms_EpisodeNumberForSaveToBeLoaded;

#if RSG_ORBIS
	static fiSaveGame::Content ms_EnumeratedPhotos[MAX_NUM_PHOTOS_TO_ENUMERATE];
#endif	//	RSG_ORBIS

#if RSG_ORBIS
	static bool ms_bSaveGamesHaveAlreadyBeenScanned;	//	An attempt to speed up the displaying of the saving/loading screens on PS3.
														//	Only enumerate the contents and get the file times the first time that scan is called.
														//	Every time a save is made, the data for that slot will need to be updated correctly.

	static int ms_SlotToUpdateOnceSaveHasCompleted;		//	Once a save has completed, this entry in ms_SaveGameSlots will need to be updated.
	static int ms_SlotToUpdateOnceDeleteHasCompleted;	//	Once a delete has completed, this entry in ms_SaveGameSlots will need to be updated.
#endif	//	RSG_ORBIS


#if RSG_ORBIS
	static CSavegameQueuedOperation_PS4DamagedCheck ms_PS4DamagedCheck;
	static s32 ms_NextSlotToScanForDamage;
	static bool ms_bBackgroundScanForDamagedSavegamesHasCompleted;
	static CSavegameQueuedOperation_CreateSortedListOfSavegames ms_EnumerateSavegames;
#endif	//	RSG_ORBIS

//	Private functions
//	static void ClearThisSlot(int SlotIndex);

#if RSG_ORBIS
	static bool ShouldEnumeratePhotos();
#endif	//	RSG_ORBIS

	static void FillListOfLocalPhotos(fiSaveGame::Content *pArrayOfSaveGames, u32 sizeOfArray);

	static bool IsThisSaveGameSlotEmpty(fiSaveGame::Content &SaveGameSlot);

//	*************** Access functions for ms_SaveGameSlots ***************************************
#if RSG_ORBIS
	static void GetModificationTime(int SlotIndex, u64 &ModificationTime);
	static void SetModificationTime(int SlotIndex, u32 modificationTimeHigh, u32 modificationTimeLow);
#endif
//	*************** End of Access functions for ms_SaveGameSlots ********************************

//	*************** Access functions for ms_SavegameDisplayData ***************************************
	static void ConstructStringFromDate(int SlotIndex);

	static void ExtractInfoFromDisplayName(int SlotIndex);
//	*************** End of Access functions for ms_SavegameDisplayData ********************************

public:
//	Public functions
	static void Init(unsigned initMode);

	static void ClearSlotData(void);

#if !__NO_OUTPUT
	static void PrintFilenames();
#endif	//	!__NO_OUTPUT

	static bool SortSaveGameSlots();
	static bool SortSaveGameSlotsAndSizes(fiSaveGame::Content *pArrayOfSaveGames, int *pArrayOfSaveGameSizes, int NumberOfValidSlots);

	static int GetAutosaveSlotNumberForCurrentEpisode();

	static void ClearNumberOfSaveGameSlots() { ms_NumberOfSaveGameSlots = 0; }
	static int GetNumberOfSaveGameSlots() { return ms_NumberOfSaveGameSlots; }

	static bool IsSaveGameSlotEmpty(int SlotIndex);
	static bool IsThisAnAutosaveSlot(int SlotIndex);

	static const char *GetNameOfSavedGameForMenu(int SlotNumber);

	static int GetEpisodeNumberForTheSaveGameToBeLoaded() { return ms_EpisodeNumberForSaveToBeLoaded; }
	static void SetEpisodeNumberForTheSaveGameToBeLoadedToThisSlot(int SlotNumber);

#if RSG_ORBIS
 	static void SetSlotToUpdateOnceSaveHasCompleted(int SlotToUpdate) { ms_SlotToUpdateOnceSaveHasCompleted = SlotToUpdate; }
	static void SetSlotToUpdateOnceDeleteHasCompleted(int SlotToUpdate) { ms_SlotToUpdateOnceDeleteHasCompleted = SlotToUpdate; }

	static void UpdateSlotDataAfterSave(u32 modificationTimeHigh, u32 modificationTimeLow, bool savingBackupSave);
	static void UpdateSlotDataAfterDelete();

	static bool GetSaveGamesHaveAlreadyBeenScanned();
	static void SetSaveGamesHaveAlreadyBeenScanned(bool bAlreadyScanned) { ms_bSaveGamesHaveAlreadyBeenScanned = bAlreadyScanned; }

	static s32 ConvertBackupSlotToSavegameSlot(s32 slotIndex);
#endif	//	RSG_ORBIS

#if RSG_ORBIS
	static void BackgroundScanForDamagedSavegames();
#endif	//	RSG_ORBIS

	static bool BeginEnumeration();
	static bool CheckEnumeration();

	static MemoryCardError CheckIfSlotIsDamaged(int SlotIndex);

	static MemoryCardError GetTimeAndMissionNameFromDisplayName(int SaveGameSlot);

	static void ResetFileTimeScan();
	static MemoryCardError GetFileTimes();

	static MemoryCardError BeginGameLoad(int SlotNumber);

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static MemoryCardError BeginGameLoadForExport(int SlotNumber);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static s32 FindFreeManualSaveSlot();

	static s32 FindMostRecentSavegame();

	static eSavegameFileType FindSavegameFileTypeForThisSlot(s32 SlotIndex);
	static s32 GetBaseIndexForSavegameFileType(eSavegameFileType FileType);

//	*************** Access functions for ms_SavegameDisplayData ***************************************
	static const char* GetSlotSaveTimeString(int SlotIndex);

	static bool GetIsDamaged(int SlotIndex);

#if RSG_ORBIS
	static bool HasBeenCheckedForDamage(int SlotIndex);
#endif	//	__PPU

	static CDate &GetSaveTime(s32 SlotIndex);

	static void GetDisplayNameAndDateForThisSaveGameItem(s32 SlotIndex, char *pNameToFillIn, char *pDateToFillIn, bool *pbReturnSlotIsEmpty, bool *pbReturnSlotIsDamaged);

//	*************** End of Access functions for ms_SavegameDisplayData ********************************
};

#endif	//	SAVEGAME_LIST_H


