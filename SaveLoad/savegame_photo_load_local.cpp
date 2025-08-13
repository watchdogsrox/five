
#include "SaveLoad/savegame_photo_load_local.h"



#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_get_file_size.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_users_and_devices.h"


SAVEGAME_OPTIMISATIONS()


CSavegamePhotoLoadLocal::eLoadPhotoLocalState CSavegamePhotoLoadLocal::ms_LoadLocalPhotoStatus = CSavegamePhotoLoadLocal::LOAD_PHOTO_LOCAL_DO_NOTHING;

CPhotoBuffer *CSavegamePhotoLoadLocal::ms_pPhotoBuffer = NULL;
u32 CSavegamePhotoLoadLocal::m_SizeOfAllocatedBuffer = 0;


void CSavegamePhotoLoadLocal::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{

	}
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_DO_NOTHING;

		ms_pPhotoBuffer = NULL;
		m_SizeOfAllocatedBuffer = 0;
	}
}

void CSavegamePhotoLoadLocal::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{

	}
	else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		//	Should I assert here if ms_pPhotoBuffer isn't NULL

	}
}

MemoryCardError CSavegamePhotoLoadLocal::BeginLocalPhotoLoad(const CSavegamePhotoUniqueId &uniqueId, CPhotoBuffer *pPhotoBuffer)
{
	if (!savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegamePhotoLoadLocal::BeginLocalPhotoLoad - operation is not OPERATION_NONE"))
	{
		return MEM_CARD_ERROR;
	}

	if (!savegameVerifyf(pPhotoBuffer, "CSavegamePhotoLoadLocal::BeginLocalPhotoLoad - pPhotoBuffer is NULL"))
	{
		return MEM_CARD_ERROR;
	}

	Assertf(ms_LoadLocalPhotoStatus == LOAD_PHOTO_LOCAL_DO_NOTHING, "CSavegamePhotoLoadLocal::BeginLocalPhotoLoad - ms_LoadLocalPhotoStatus is %d. It hasn't been reset by an earlier photo load", ms_LoadLocalPhotoStatus);
	Assertf(ms_pPhotoBuffer == NULL, "CSavegamePhotoLoadLocal::BeginLocalPhotoLoad - ms_pPhotoBuffer isn't NULL");

	CSavegameFilenames::MakeValidSaveNameForLocalPhotoFile(uniqueId);

	ms_pPhotoBuffer = pPhotoBuffer;

	CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_LOCAL_PHOTO);

	ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_BEGIN_INITIAL_CHECKS;
	return MEM_CARD_COMPLETE;
}

MemoryCardError CSavegamePhotoLoadLocal::DoLocalPhotoLoad(bool bDisplayErrors)
{
	MemoryCardError ReturnValue = MEM_CARD_BUSY;
	switch (ms_LoadLocalPhotoStatus)
	{
		case LOAD_PHOTO_LOCAL_DO_NOTHING :
			return MEM_CARD_COMPLETE;

		case LOAD_PHOTO_LOCAL_BEGIN_INITIAL_CHECKS :
			{
				CSavegameInitialChecks::BeginInitialChecks(false);
				ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_DO_INITIAL_CHECKS;
			}
//			break;

		case LOAD_PHOTO_LOCAL_DO_INITIAL_CHECKS :
			{
				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOADING);

				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
					ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_GET_FILE_SIZE;
				}
			}
			break;

		case LOAD_PHOTO_LOCAL_GET_FILE_SIZE :
			{
				u64 FileSize = 0;
				MemoryCardError GetFileSizeStatus = CSavegameGetFileSize::GetFileSize(CSavegameFilenames::GetFilenameOfLocalFile(), false, FileSize);

				switch (GetFileSizeStatus)
				{
				case MEM_CARD_COMPLETE :
					{
						if (FileSize == 0)
						{
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_SIZE_FAILED1);
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
						}
						else
						{
							m_SizeOfAllocatedBuffer = static_cast<u32>(FileSize);
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_ALLOCATE_BUFFER;
						}
					}
					break;

				case MEM_CARD_BUSY :
					// Don't do anything
					break;

				case MEM_CARD_ERROR :
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
					}
					break;
				}
			}
			break;

		case LOAD_PHOTO_LOCAL_ALLOCATE_BUFFER :
			if (savegameVerifyf(ms_pPhotoBuffer, "CSavegamePhotoLoadLocal::DoLocalPhotoLoad - ms_pPhotoBuffer is NULL"))
			{
				if (ms_pPhotoBuffer->AllocateBufferForLoadingLocalPhoto(m_SizeOfAllocatedBuffer))
				{
					ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_BEGIN_CREATOR_CHECK;
				}
				else
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FOR_LOAD_FAILED);
					ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
				}
			}
			else
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FOR_LOAD_FAILED);
				ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
			}
			break;

		case LOAD_PHOTO_LOCAL_BEGIN_CREATOR_CHECK :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				ReturnValue = MEM_CARD_ERROR;
			}
			else
			{
				if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
					ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
				}
				else
				{
					ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_DO_CREATOR_CHECK;
					if (CGenericGameStorage::DoCreatorCheck())
					{
						if (!SAVEGAME.BeginGetCreator(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), CSavegameFilenames::GetFilenameOfLocalFile()) )
						{
							Assertf(0, "CSavegamePhotoLoadLocal::DoLocalPhotoLoad - BeginGetCreator failed");
							savegameDebugf1("CSavegamePhotoLoadLocal::DoLocalPhotoLoad - BeginGetCreator failed \n");
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_GET_CREATOR_FAILED);
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
						}
					}
				}
			}
			break;

		case LOAD_PHOTO_LOCAL_DO_CREATOR_CHECK :
			{
				bool bIsCreator = false;

				if (!CGenericGameStorage::DoCreatorCheck() || SAVEGAME.CheckGetCreator(CSavegameUsers::GetUser(), bIsCreator))
				{	//	returns true for error or success, false for busy
					if (CGenericGameStorage::DoCreatorCheck())
					{
						SAVEGAME.EndGetCreator(CSavegameUsers::GetUser());
					}

					if (CSavegameUsers::HasPlayerJustSignedOut())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
						{
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
						}
						else
						{
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_BEGIN_LOAD;

							if (CGenericGameStorage::DoCreatorCheck())
							{
								if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
								{
									savegameDebugf1("CSavegamePhotoLoadLocal::DoLocalPhotoLoad - SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED1 \n");
									CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED1);
									ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
								}
								else
								{
									if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
									{
										savegameDebugf1("CSavegamePhotoLoadLocal::DoLocalPhotoLoad - SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED2 \n");
										CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED2);
										ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
									}
									else
									{
										if (!bIsCreator)
										{
											CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_IS_NOT_CREATOR);
											ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
										}
									}
								}
							}
						}
					}
				}
			}
			break;

		case LOAD_PHOTO_LOCAL_BEGIN_LOAD :
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
						ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
					}
					else
					{
						ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_DO_LOAD;

						if (!SAVEGAME.BeginLoad(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), CSavegameFilenames::GetFilenameOfLocalFile(), ms_pPhotoBuffer->GetCompleteBufferForLoading(), m_SizeOfAllocatedBuffer) )	//	 ms_pPhotoBuffer->GetSizeOfCompleteBuffer()) )
						{
							Assertf(0, "CSavegamePhotoLoadLocal::DoLocalPhotoLoad - BeginLoad failed");
							savegameDebugf1("CSavegamePhotoLoadLocal::DoLocalPhotoLoad - BeginLoad failed \n");
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOAD_FAILED);
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
						}
					}
				}
			}
			break;

		case LOAD_PHOTO_LOCAL_DO_LOAD :
			{
				u32 ActualBufferSizeLoaded = 0;
				bool valid; // MIGRATE_FIXME - use for something
				if (SAVEGAME.CheckLoad(CSavegameUsers::GetUser(), valid, ActualBufferSizeLoaded))
				{	//	returns true for error or success, false for busy
					SAVEGAME.EndLoad(CSavegameUsers::GetUser());

					if (CSavegameUsers::HasPlayerJustSignedOut())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
						{
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
							ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
						}
						else
						{
							if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
							{
								CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED1);
								ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
							}
							else
							{
								if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
								{
									CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED2);
									ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_HANDLE_ERRORS;
								}
								else
								{
									if (ms_pPhotoBuffer && CSavegamePhotoMetadata::ShouldCorrectAkamaiOnLocalLoad())
									{
										ms_pPhotoBuffer->CorrectAkamaiTag();
									}

									ReturnValue = MEM_CARD_COMPLETE;
								}
							}
						}
					}
				}
			}
			break;

		case LOAD_PHOTO_LOCAL_HANDLE_ERRORS :
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

		EndLocalPhotoLoad();
		CSavegameDialogScreens::SetMessageAsProcessing(false);
	}

	return ReturnValue;
}

void CSavegamePhotoLoadLocal::EndLocalPhotoLoad()
{
	ms_LoadLocalPhotoStatus = LOAD_PHOTO_LOCAL_DO_NOTHING;
	ms_pPhotoBuffer = NULL;

	savegameAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_LOCAL_PHOTO, "CSavegamePhotoLoadLocal::EndLocalPhotoLoad - Operation should be OPERATION_LOADING_LOCAL_PHOTO but it's %d", (s32) CGenericGameStorage::GetSaveOperation());

	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}

