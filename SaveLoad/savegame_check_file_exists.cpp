
#include "SaveLoad/savegame_check_file_exists.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"

SAVEGAME_OPTIMISATIONS()

eCheckFileExistsState CSavegameCheckFileExists::ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_DO_NOTHING;

s32 CSavegameCheckFileExists::ms_SlotIndexToCheck = -1;
bool CSavegameCheckFileExists::ms_bFileExists = false;
bool CSavegameCheckFileExists::ms_bFileIsDamaged = false;


void CSavegameCheckFileExists::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
		ms_SlotIndexToCheck = -1;

		ms_bFileExists = false;
		ms_bFileIsDamaged = false;

		ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_DO_NOTHING;
	}
}


MemoryCardError CSavegameCheckFileExists::BeginCheckFileExists(int SlotNumber)
{
	if (savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameCheckFileExists::BeginCheckFileExists - operation is not OPERATION_NONE"))
	{
		if (savegameVerifyf(ms_CheckFileExistsStatus == CHECK_FILE_EXISTS_DO_NOTHING, "CSavegameCheckFileExists::BeginCheckFileExists - "))
		{
			ms_SlotIndexToCheck = SlotNumber;

			CSavegameFilenames::MakeValidSaveNameForLocalFile(SlotNumber);

			ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_INITIAL_CHECKS;

			CGenericGameStorage::SetSaveOperation(OPERATION_CHECKING_IF_FILE_EXISTS);
			CSavegameInitialChecks::BeginInitialChecks(false);

			return MEM_CARD_COMPLETE;
		}
	}

	return MEM_CARD_ERROR;
}


void CSavegameCheckFileExists::EndCheckFileExists()
{
	ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_DO_NOTHING;
	Assertf(CGenericGameStorage::GetSaveOperation() == OPERATION_CHECKING_IF_FILE_EXISTS, "CSavegameCheckFileExists::EndCheckFileExists - SaveOperation should be OPERATION_CHECKING_IF_FILE_EXISTS");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


MemoryCardError CSavegameCheckFileExists::CheckFileExists()
{
	bool bReturnError = false;

	switch (ms_CheckFileExistsStatus)
	{
		case CHECK_FILE_EXISTS_DO_NOTHING :
			savegameAssertf(0, "CSavegameCheckFileExists::CheckFileExists - didn't expect ms_CheckFileExistsStatus == CHECK_FILE_EXISTS_DO_NOTHING");
			return MEM_CARD_COMPLETE;
//			break;
		case CHECK_FILE_EXISTS_INITIAL_CHECKS :
			{
				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						bReturnError = true;
					}
					else
					{
						ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_HANDLE_ERRORS;
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
					// Copied from CSavegameLoadBlock::LoadBlock(). Not sure if it's necessary here.
					CSavegameDialogScreens::SetMessageAsProcessing(false);

					if (CSavegameList::IsSaveGameSlotEmpty(ms_SlotIndexToCheck))
					{
						ms_bFileExists = false;
						ms_bFileIsDamaged = false;
						EndCheckFileExists();
						return MEM_CARD_COMPLETE;
					}
					else
					{
						ms_bFileExists = true;
						ms_bFileIsDamaged = false;
						ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_DO_DAMAGE_CHECK;
					}
				}
			}
			break;

		case CHECK_FILE_EXISTS_DO_DAMAGE_CHECK :
			{
				MemoryCardError DamageCheckState = CSavegameList::CheckIfSlotIsDamaged(ms_SlotIndexToCheck);
				if (DamageCheckState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						bReturnError = true;
					}
					else
					{
						ms_CheckFileExistsStatus = CHECK_FILE_EXISTS_HANDLE_ERRORS;
					}
				}
				else if (DamageCheckState == MEM_CARD_COMPLETE)
				{
					ms_bFileIsDamaged = CSavegameList::GetIsDamaged(ms_SlotIndexToCheck);
					EndCheckFileExists();
					return MEM_CARD_COMPLETE;
				}
			}			
			break;

		case CHECK_FILE_EXISTS_HANDLE_ERRORS :
			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				bReturnError = true;
			}
			break;
	}

	if (bReturnError)
	{
		// Copied from CSavegameLoadBlock::CalculateSizeOfRequiredBuffer(). Not sure if it's necessary here.
		CSavegameDialogScreens::SetMessageAsProcessing(false);

		EndCheckFileExists();

		return MEM_CARD_ERROR;
	}

	return MEM_CARD_BUSY;
}


