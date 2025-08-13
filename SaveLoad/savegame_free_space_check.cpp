
// Rage headers
#include "file/savegame.h"

// Game headers
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_free_space_check.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_users_and_devices.h"

SAVEGAME_OPTIMISATIONS()


//	 Does this get called when saving a block? It probably shouldn't. I think Saving a block is only done for cloud photos now
MemoryCardError CSavegameFreeSpaceCheck::CheckForFreeSpaceForCurrentFilename(bool bInitialise, u32 SizeOfNewFile, int &SpaceRequired)
{
	int UnusedParam = 0;
	int UnusedParam2 = 0;
	return CheckForFreeSpaceForNamedFile(FREE_SPACE_CHECK, bInitialise, CSavegameFilenames::GetFilenameOfLocalFile(), SizeOfNewFile, SpaceRequired, UnusedParam, UnusedParam2);
}

MemoryCardError CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile(eTypeOfSpaceCheck TypeOfSpaceCheck, bool bInitialise, const char *pFilename, u32 SizeOfNewFile, int &SpaceRequired, int &TotalFreeStorageDeviceSpace, int &SizeOfSystemFiles)
{
	TotalFreeStorageDeviceSpace = 0;
	SizeOfSystemFiles = 0;

	if (bInitialise)
	{
		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
			return MEM_CARD_ERROR;
		}

		if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
		{
			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
			return MEM_CARD_ERROR;
		}

		bool bCheckBegunSuccessfully = false;
		if (TypeOfSpaceCheck == FREE_SPACE_CHECK)
		{
			bCheckBegunSuccessfully = SAVEGAME.BeginFreeSpaceCheck(CSavegameUsers::GetUser(), pFilename, SizeOfNewFile);
		}
		else
		{
			Assertf(EXTRA_SPACE_CHECK == TypeOfSpaceCheck, "CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile - expected TypeOfSpaceCheck to be FREE_SPACE_CHECK or EXTRA_SPACE_CHECK");
			bCheckBegunSuccessfully = SAVEGAME.BeginExtraSpaceCheck(CSavegameUsers::GetUser(), pFilename, SizeOfNewFile);
		}
		if (!bCheckBegunSuccessfully)
		{
			Assertf(0, "CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile - BeginFreeSpaceCheck failed");
			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_FREE_SPACE_CHECK_FAILED);
			return MEM_CARD_ERROR;
		}
	}
	else
	{
		bool bSpaceCheckHasFinished = false;
		if (TypeOfSpaceCheck == FREE_SPACE_CHECK)
		{
			bSpaceCheckHasFinished = SAVEGAME.CheckFreeSpaceCheck(CSavegameUsers::GetUser(), SpaceRequired);
		}
		else
		{
			Assertf(EXTRA_SPACE_CHECK == TypeOfSpaceCheck, "CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile - expected TypeOfSpaceCheck to be FREE_SPACE_CHECK or EXTRA_SPACE_CHECK");
			bSpaceCheckHasFinished = SAVEGAME.CheckExtraSpaceCheck(CSavegameUsers::GetUser(), SpaceRequired, TotalFreeStorageDeviceSpace, SizeOfSystemFiles);
		}

		if (bSpaceCheckHasFinished)
		{
			if (TypeOfSpaceCheck == FREE_SPACE_CHECK)
			{
				SAVEGAME.EndFreeSpaceCheck(CSavegameUsers::GetUser());
			}
			else
			{
				Assertf(EXTRA_SPACE_CHECK == TypeOfSpaceCheck, "CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile - expected TypeOfSpaceCheck to be FREE_SPACE_CHECK or EXTRA_SPACE_CHECK");
				SAVEGAME.EndExtraSpaceCheck(CSavegameUsers::GetUser());
			}

			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
				return MEM_CARD_ERROR;
			}

			return MEM_CARD_COMPLETE;
		}
	}

	return MEM_CARD_BUSY;
}


