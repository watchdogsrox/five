
#ifndef SAVEGAME_SAVE_H
#define SAVEGAME_SAVE_H

// Game headers
#include "SaveLoad/savegame_data.h"
#include "SaveLoad/savegame_defines.h"


enum eGenericSaveState
{
	GENERIC_SAVE_DO_NOTHING = 0,
	GENERIC_SAVE_CHECK_IF_THIS_IS_AN_AUTOSAVE,
	GENERIC_SAVE_INITIAL_CHECKS,
	GENERIC_SAVE_CHECK_IF_THE_AUTOSAVE_TO_BE_OVERWRITTEN_IS_DAMAGED,
	GENERIC_SAVE_FIND_THE_NAME_OF_THE_SLOT_THAT_WILL_BE_OVERWRITTEN_BY_THE_AUTOSAVE,
	GENERIC_SAVE_CHECK_THAT_PLAYER_WANTS_TO_OVERWRITE_EXISTING_AUTOSAVE,
	GENERIC_SAVE_CREATE_SAVE_BUFFER,
	GENERIC_SAVE_FILL_BUFFER,
	GENERIC_SAVE_BEGIN_SAVE,
	GENERIC_SAVE_DO_SAVE,
	GENERIC_SAVE_HANDLE_ERRORS,
	GENERIC_SAVE_AUTOSAVE_FAILED_TIDY_UP,
	GENERIC_SAVE_AUTOSAVE_FAILED,
	GENERIC_SAVE_CHECK_PLAYER_WANTS_TO_SWITCH_OFF_AUTOSAVE,
	GENERIC_SAVE_SWITCH_OFF_AUTOSAVE
};


class CSavegameSave
{
private:
//	Private data
	static eGenericSaveState	ms_GenericSaveStatus;

	static char *ms_pSaveGameIcon;
	static u32	ms_SizeOfSaveGameIcon;

	static s32 ms_SlotNumber;
	static bool ms_bAutosave;

	static bool ms_bSaveHasJustOccurred;
#if RSG_ORBIS
	static bool ms_bAllowBackupSave;
	static bool ms_bSavingABackupOnly;
	static bool ms_savingBackupSave;
#endif

#if RSG_PC
	static const u32 cMaxLengthOfCloudSaveMetadataString = 1024;
	static char ms_CloudSaveMetadataString[cMaxLengthOfCloudSaveMetadataString];
#endif	//	RSG_PC

	static eTypeOfSavegame ms_SaveGameType;

//	Private functions
	static MemoryCardError DoSave(bool savingBackupSave);

	static void EndGenericSave();

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
	static void RegisterCloudSaveFile();
#endif	//	RSG_PC

public:
//	Public functions
	static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);

	static MemoryCardError BeginGameSave(int SlotNumber, eTypeOfSavegame savegameType);

	static MemoryCardError GenericSave(bool bDisplayErrors = true);

	static bool GetSaveHasJustOccurred() { return ms_bSaveHasJustOccurred; }
	static void SetSaveHasJustOccurred(bool bJustOccurred) { ms_bSaveHasJustOccurred = bJustOccurred; }

	static char *GetSaveGameIcon() { return ms_pSaveGameIcon; }
	static u32 GetSizeOfSaveGameIcon() { return ms_SizeOfSaveGameIcon; }
};

#endif	//	SAVEGAME_SAVE_H

