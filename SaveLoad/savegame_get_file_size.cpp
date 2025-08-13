
// Rage headers
#include "file/savegame.h"

// Game headers
#include "SaveLoad/savegame_get_file_size.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/savegame_channel.h"

SAVEGAME_OPTIMISATIONS()

MemoryCardError CSavegameGetFileSize::GetFileSize(const char *pFilename, bool bCalculateSizeOnDisk, u64 &fileSize)
{
	static int GetFileSizeState = 0;
	static u64 fileSizeToReturn = 0;

	switch (GetFileSizeState)
	{
		case 0 :
			fileSizeToReturn = 0;

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

			if (!SAVEGAME.BeginGetFileSize(CSavegameUsers::GetUser(), pFilename, bCalculateSizeOnDisk) )
			{
				Assertf(0, "CSavegameGetFileSize::GetFileSize - BeginGetFileSize failed");
				savegameDebugf1("CSavegameGetFileSize::GetFileSize - BeginGetFileSize failed \n");
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_GET_FILE_SIZE_FAILED);
				return MEM_CARD_ERROR;
			}
			GetFileSizeState = 1;
			break;

		case 1 :
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			if (SAVEGAME.CheckGetFileSize(CSavegameUsers::GetUser(), fileSizeToReturn))
			{	//	returns true for error or success, false for busy
				GetFileSizeState = 0;
				SAVEGAME.EndGetFileSize(CSavegameUsers::GetUser());

				fileSize = fileSizeToReturn;

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

				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_SIZE_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_SIZE_FAILED2);
					return MEM_CARD_ERROR;
				}

				return MEM_CARD_COMPLETE;
			}
			break;
	}

	return MEM_CARD_BUSY;
}

