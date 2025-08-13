
#include "SaveLoad/savegame_photo_save_local.h"



#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_initial_checks.h"
//	#include "SaveLoad/savegame_list.h"
//	#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_users_and_devices.h"


SAVEGAME_OPTIMISATIONS()


CSavegamePhotoSaveLocal::eSavePhotoLocalState CSavegamePhotoSaveLocal::ms_SavePhotoStatus = CSavegamePhotoSaveLocal::SAVE_PHOTO_LOCAL_DO_NOTHING;
const CPhotoBuffer *CSavegamePhotoSaveLocal::ms_pPhotoBuffer = NULL;


void CSavegamePhotoSaveLocal::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {

    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_DO_NOTHING;

		ms_pPhotoBuffer = NULL;
	}
}

void CSavegamePhotoSaveLocal::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {

	}
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
		//	Should I assert here if ms_pPhotoBuffer isn't NULL

	}
}

MemoryCardError CSavegamePhotoSaveLocal::BeginSavePhotoLocal(const CSavegamePhotoUniqueId &uniqueId, const CPhotoBuffer *pPhotoBuffer)
{
	if (!savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegamePhotoSaveLocal::BeginSavePhotoLocal - operation is not OPERATION_NONE"))
	{
		return MEM_CARD_ERROR;
	}

	Assertf(ms_SavePhotoStatus == SAVE_PHOTO_LOCAL_DO_NOTHING, "CSavegamePhotoSaveLocal::BeginSavePhotoLocal - ms_SavePhotoStatus is %d. It hasn't been reset by an earlier photo save", ms_SavePhotoStatus);
	Assertf(ms_pPhotoBuffer == NULL, "CSavegamePhotoSaveLocal::BeginSavePhotoLocal - ms_pPhotoBuffer isn't NULL");

//	if (ms_SavePhotoStatus == SAVE_PHOTO_LOCAL_DO_NOTHING)
	{
		CSavegameFilenames::MakeValidSaveNameForLocalPhotoFile(uniqueId);

		ms_pPhotoBuffer = pPhotoBuffer;

// #if __PPU
// 		CSavegameList::SetSlotToUpdateOnceSaveHasCompleted(SlotNumber);
// #endif
		CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_LOCAL_PHOTO);

		ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_BEGIN_INITIAL_CHECKS;
		return MEM_CARD_COMPLETE;
	}

//	return MEM_CARD_ERROR;
}




void CSavegamePhotoSaveLocal::EndSavePhotoLocal()
{
	ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_DO_NOTHING;
	ms_pPhotoBuffer = NULL;

	savegameAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_LOCAL_PHOTO, "CSavegamePhotoSaveLocal::EndSavePhotoLocal - Operation should be OPERATION_SAVING_LOCAL_PHOTO but it's %d", (s32) CGenericGameStorage::GetSaveOperation());

	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


MemoryCardError CSavegamePhotoSaveLocal::DoSavePhotoLocal(bool bDisplayErrors)
{
	MemoryCardError ReturnValue = MEM_CARD_BUSY;
	switch (ms_SavePhotoStatus)
	{
		case SAVE_PHOTO_LOCAL_DO_NOTHING :
			return MEM_CARD_COMPLETE;

		case SAVE_PHOTO_LOCAL_BEGIN_INITIAL_CHECKS :
			{
				CSavegameInitialChecks::SetSizeOfBufferToBeSaved(ms_pPhotoBuffer->GetSizeOfCompleteBuffer());
				CSavegameInitialChecks::BeginInitialChecks(false);
				ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_DO_INITIAL_CHECKS;
			}
//			break;

		case SAVE_PHOTO_LOCAL_DO_INITIAL_CHECKS :
			{
				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);

				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
					ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_BEGIN_SAVE;
				}
			}
			break;

		case SAVE_PHOTO_LOCAL_BEGIN_SAVE :
			{
				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					ReturnValue = MEM_CARD_ERROR;
				}
				else
				{
					if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
						ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
					}
					else
					{
						if (!SAVEGAME.BeginSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), 
							CSavegameFilenames::GetNameToDisplay(), CSavegameFilenames::GetFilenameOfLocalFile(), 
							ms_pPhotoBuffer->GetCompleteBuffer(), ms_pPhotoBuffer->GetSizeOfCompleteBuffer(), 
							true) )	//	bool overwrite, CGameLogic::GetCurrentEpisodeIndex())
						{
							Assertf(0, "CSavegamePhotoSaveLocal::DoSavePhotoLocal - BeginSave failed");
							savegameDebugf1("CSavegamePhotoSaveLocal::DoSavePhotoLocal - BeginSave failed\n");
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_SAVE_FAILED);
							ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
						}
						else
						{
							ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_DO_SAVE;
						}
					}
				}
			}
			break;

		case SAVE_PHOTO_LOCAL_DO_SAVE :
		{
			bool bOutIsValid = false;
			bool bFileExists = false;

			if (SAVEGAME.CheckSave(CSavegameUsers::GetUser(), bOutIsValid, bFileExists))
			{	//	returns true for error or success, false for busy

				SAVEGAME.EndSave(CSavegameUsers::GetUser());

				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					ReturnValue = MEM_CARD_ERROR;
				}
				else
				{
					if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
						ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
					}
					else
					{
						if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
						{
							savegameDebugf1("CSavegamePhotoSaveLocal::DoSavePhotoLocal - SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1 \n");
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1);
							ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
						}
						else
						{
							if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
							{
								savegameDebugf1("CSavegamePhotoSaveLocal::DoSavePhotoLocal - SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2 \n");
								CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2);
								ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
							}
							else
							{
// #if __PPU
// 								CSavegameList::UpdateSlotDataAfterSave();
// #endif

								ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_BEGIN_ICON_SAVE;
							}
						}
					}
				}
			}
		}
			break;

		case SAVE_PHOTO_LOCAL_BEGIN_ICON_SAVE :
			{
				const char *pSaveGameIcon = CSavegameSave::GetSaveGameIcon();
				
				if (pSaveGameIcon)
				{
					if (!SAVEGAME.BeginIconSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), 
						CSavegameFilenames::GetFilenameOfLocalFile(), static_cast<const void *>(pSaveGameIcon), CSavegameSave::GetSizeOfSaveGameIcon()))
					{
						Assertf(0, "CSavegamePhotoSaveLocal::DoSavePhotoLocal - BeginIconSave failed");
						savegameDebugf1("CSavegamePhotoSaveLocal::DoSavePhotoLocal - BeginIconSave failed\n");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_ICON_SAVE_FAILED);
						ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
					}
					else
					{
						ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_DO_ICON_SAVE;
					}
				}
				else
				{
					ReturnValue = MEM_CARD_COMPLETE;	// No Icon so consider Save as completed
				}
			}
			break;

		case SAVE_PHOTO_LOCAL_DO_ICON_SAVE :
			{
				if (SAVEGAME.CheckIconSave(CSavegameUsers::GetUser()))
				{	//	returns true for error or success, false for busy
					SAVEGAME.EndIconSave(CSavegameUsers::GetUser());


					if (CSavegameUsers::HasPlayerJustSignedOut())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
						{
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
							ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
						}
						else
						{
							if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
							{
								savegameDebugf1("CSavegamePhotoSaveLocal::DoSavePhotoLocal - SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1 \n");
								CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1);
								ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
							}
							else
							{
								if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
								{
									savegameDebugf1("CSavegamePhotoSaveLocal::DoSavePhotoLocal - SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2 \n");
									CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2);
									ms_SavePhotoStatus = SAVE_PHOTO_LOCAL_HANDLE_ERRORS;
								}
								else
								{
									ReturnValue = MEM_CARD_COMPLETE;
								}
							}
						}
					}
				}
			}
			break;

		case SAVE_PHOTO_LOCAL_HANDLE_ERRORS :
			if(bDisplayErrors == false)
			{
				ReturnValue = MEM_CARD_ERROR;
				break;
			}

			CSavingMessage::Clear();

			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				ReturnValue = MEM_CARD_ERROR;
			}
			break;
	}

	if (ReturnValue != MEM_CARD_BUSY)
	{
		if (ReturnValue == MEM_CARD_ERROR)
		{
			CSavingMessage::Clear();
		}

		EndSavePhotoLocal();
		CSavegameDialogScreens::SetMessageAsProcessing(false);
	}

	return ReturnValue;
}


