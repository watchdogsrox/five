
#include "savegame_delete.h"

// Framework headers
#include "fwsys/gameskeleton.h"


// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_users_and_devices.h"


SAVEGAME_OPTIMISATIONS()


CloudFileDeleter *CSavegameDelete::ms_pCloudFileDeleter = NULL;
eSavegameDeleteState CSavegameDelete::ms_DeleteStatus = SAVEGAME_DELETE_DO_NOTHING;
bool CSavegameDelete::ms_bDeleteFromCloud = false;

#if RSG_ORBIS
bool CSavegameDelete::ms_bDeleteMount = true;
#endif	//	RSG_ORBIS


void CSavegameDelete::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		ms_pCloudFileDeleter = rage_new CloudFileDeleter;
		savegameAssertf(ms_pCloudFileDeleter, "CSavegameDelete::Init - failed to create ms_pCloudFileDeleter");
	}
// 	if(initMode == INIT_SESSION)
// 	{
// 		ms_DeleteStatus = SAVEGAME_DELETE_DO_NOTHING;
// 	}
}

void CSavegameDelete::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		delete ms_pCloudFileDeleter;
		ms_pCloudFileDeleter = NULL;
	}
	else if(shutdownMode == SHUTDOWN_SESSION)
	{
		ms_DeleteStatus = SAVEGAME_DELETE_DO_NOTHING;
	}
}


void CSavegameDelete::EndDelete()
{
	ms_DeleteStatus = SAVEGAME_DELETE_DO_NOTHING;
	savegameAssertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_DELETING_LOCAL) || (CGenericGameStorage::GetSaveOperation() == OPERATION_DELETING_CLOUD), "CSavegameDelete::EndDelete - SaveOperation should be OPERATION_DELETING_LOCAL or OPERATION_DELETING_CLOUD");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


bool CSavegameDelete::Begin(const char *pFilename, bool bDeleteFromCloud, bool ORBIS_ONLY(bIsAPhoto))
{
	if (CGenericGameStorage::GetSaveOperation() != OPERATION_NONE)
	{
		savegameAssertf(0, "CSavegameDelete::Begin - SaveOperation is not OPERATION_NONE");
		return false;
	}

	if (ms_DeleteStatus != SAVEGAME_DELETE_DO_NOTHING)
	{
		savegameAssertf(0, "CSavegameDelete::Begin - expected DeleteStatus to be SAVEGAME_DELETE_DO_NOTHING but it's %d", ms_DeleteStatus);
		return false;
	}

	bool bFilenameHasBeenSet = false;
	if (bDeleteFromCloud)
	{
		bFilenameHasBeenSet = CSavegameFilenames::SetFilenameOfCloudFile(pFilename);
	}
	else
	{
		bFilenameHasBeenSet = CSavegameFilenames::SetFilenameOfLocalFile(pFilename);
	}

	if (!bFilenameHasBeenSet)
	{
		savegameAssertf(0, "CSavegameDelete::Begin - failed to set filename %s", pFilename);
		return false;
	}

	ms_bDeleteFromCloud = bDeleteFromCloud;

#if RSG_ORBIS
	if (bIsAPhoto)
	{
		ms_bDeleteMount = false;
	}
	else
	{
		ms_bDeleteMount = true;
	}
#endif	//	RSG_ORBIS


	ms_DeleteStatus = SAVEGAME_DELETE_BEGIN_INITIAL_CHECKS;
	if (ms_bDeleteFromCloud)
	{
		savegameDisplayf("CSavegameDelete::Begin - beginning a cloud delete for %s", CSavegameFilenames::GetFilenameOfCloudFile());
		CGenericGameStorage::SetSaveOperation(OPERATION_DELETING_CLOUD);
	}
	else
	{
		savegameDisplayf("CSavegameDelete::Begin - beginning a local delete for %s", CSavegameFilenames::GetFilenameOfLocalFile());
		CGenericGameStorage::SetSaveOperation(OPERATION_DELETING_LOCAL);
	}

	return true;
}

MemoryCardError CSavegameDelete::Update()
{
	MemoryCardError returnState = MEM_CARD_BUSY;

	switch (ms_DeleteStatus)
	{
		case SAVEGAME_DELETE_DO_NOTHING :
			return MEM_CARD_COMPLETE;

		case SAVEGAME_DELETE_BEGIN_INITIAL_CHECKS :
			{
				savegameAssertf((CGenericGameStorage::GetSaveOperation() == OPERATION_DELETING_LOCAL) || (CGenericGameStorage::GetSaveOperation() == OPERATION_DELETING_CLOUD), "CSavegameDelete::Update - expected SaveOperation to be OPERATION_DELETING_LOCAL or OPERATION_DELETING_CLOUD");
				CSavegameInitialChecks::BeginInitialChecks(false);
				ms_DeleteStatus = SAVEGAME_DELETE_INITIAL_CHECKS;
			}
//			break;

		case SAVEGAME_DELETE_INITIAL_CHECKS :
			{
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_DELETING_LOCAL)
				{
					CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOCAL_DELETING);
				}
				else
				{
					CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_DELETING);
				}

				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						savegameDisplayf("CSavegameDelete::Update - InitialChecks failed. Player has signed out");
						returnState = MEM_CARD_ERROR;
					}
					else
					{
						savegameDisplayf("CSavegameDelete::Update - InitialChecks failed");
						ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
					if (ms_bDeleteFromCloud)
					{
						ms_DeleteStatus = SAVEGAME_DELETE_BEGIN_CLOUD_DELETE;
					}
					else
					{
						ms_DeleteStatus = SAVEGAME_DELETE_BEGIN_LOCAL_DELETE;
					}
				}
			}
			break;

		case SAVEGAME_DELETE_BEGIN_LOCAL_DELETE :
			{
				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
					returnState = MEM_CARD_ERROR;
				}
				else if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
					ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
				}
				else
				{
#if RSG_ORBIS
					if (ms_bDeleteMount)
					{
						if (!SAVEGAME.BeginDeleteMount(CSavegameUsers::GetUser(), CSavegameFilenames::GetFilenameOfLocalFile(), true))
						{
							savegameAssertf(0, "CSavegameDelete::Update - BeginDeleteMount failed");
							savegameDebugf1("CSavegameDelete::Update - BeginDeleteMount failed \n");
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_DELETE_FAILED);
							ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
						}
						else
						{
							savegameDisplayf("CSavegameDelete::Update - started a local delete mount");
							ms_DeleteStatus = SAVEGAME_DELETE_DO_LOCAL_DELETE;
						}
					}
					else
#endif	//	RSG_ORBIS
					{
						if (!SAVEGAME.BeginDelete(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), CSavegameFilenames::GetFilenameOfLocalFile()))
						{
							savegameAssertf(0, "CSavegameDelete::Update - BeginDelete failed");
							savegameDebugf1("CSavegameDelete::Update - BeginDelete failed \n");
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_DELETE_FAILED);
							ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
						}
						else
						{
							savegameDisplayf("CSavegameDelete::Update - started a local delete");
							ms_DeleteStatus = SAVEGAME_DELETE_DO_LOCAL_DELETE;
						}
					}
				}				
			}
			break;

		case SAVEGAME_DELETE_DO_LOCAL_DELETE :
		{
#if __XENON
			SAVEGAME.UpdateClass();
#endif

			bool bDeleteHasFinished = false;

#if RSG_ORBIS
			if (ms_bDeleteMount)
			{
				if (SAVEGAME.CheckDeleteMount(CSavegameUsers::GetUser()))
				{	//	returns true for error or success, false for busy
					SAVEGAME.EndDeleteMount(CSavegameUsers::GetUser());
					bDeleteHasFinished = true;

					savegameDisplayf("CSavegameDelete::Update - CheckDeleteMount has returned TRUE");
				}
			}
			else
#endif	//	RSG_ORBIS
			{
				if (SAVEGAME.CheckDelete(CSavegameUsers::GetUser()))
				{	//	returns true for error or success, false for busy
					SAVEGAME.EndDelete(CSavegameUsers::GetUser());
					bDeleteHasFinished = true;

					savegameDisplayf("CSavegameDelete::Update - CheckDelete has returned TRUE");
				}
			}

			if (bDeleteHasFinished)
			{
				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					savegameDisplayf("CSavegameDelete::Update - local delete failed. Player has signed out");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
					returnState = MEM_CARD_ERROR;
				}
				else if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
				{
					savegameDisplayf("CSavegameDelete::Update - local delete failed. Storage device has been removed");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
					ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
				}
				else if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
					savegameDisplayf("CSavegameDelete::Update - SAVE_GAME_DIALOG_CHECK_LOCAL_DELETE_FAILED1");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOCAL_DELETE_FAILED1);
					ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
				}
				else if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					savegameDisplayf("CSavegameDelete::Update - SAVE_GAME_DIALOG_CHECK_LOCAL_DELETE_FAILED2");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOCAL_DELETE_FAILED2);
					ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
				}
				else
				{
					savegameDisplayf("CSavegameDelete::Update - local delete has completed");
//					ms_DeleteStatus = SAVEGAME_DELETE_WAIT_FOR_DELETING_MESSAGE_TO_FINISH;
					returnState = MEM_CARD_COMPLETE;
				}
			}
		}
			break;

		case SAVEGAME_DELETE_BEGIN_CLOUD_DELETE :
			{
				if (savegameVerifyf(ms_pCloudFileDeleter, "CSavegameDelete::Update - ms_pCloudFileDeleter is NULL"))
				{
					if (ms_pCloudFileDeleter->RequestDeletionOfCloudFile(CSavegameFilenames::GetFilenameOfCloudFile()))
					{
						savegameDisplayf("CSavegameDelete::Update - RequestDeletionOfCloudFile succeeded");
						ms_DeleteStatus = SAVEGAME_DELETE_DO_CLOUD_DELETE;
					}
					else
					{
						savegameDisplayf("CSavegameDelete::Update - RequestDeletionOfCloudFile failed");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_DELETE_FAILED);
						ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
					}
				}
				else
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_DELETE_FAILED);
					ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
				}
			}
			break;

		case SAVEGAME_DELETE_DO_CLOUD_DELETE :
			{
				if (savegameVerifyf(ms_pCloudFileDeleter, "CSavegameDelete::Update - ms_pCloudFileDeleter is NULL"))
				{
					savegameDisplayf("CSavegameDelete::Update - checking status of pending cloud request");
					if (ms_pCloudFileDeleter->GetIsRequestPending() == false)
					{
						if (ms_pCloudFileDeleter->GetLastDeletionSucceeded())
						{
							savegameDisplayf("CSavegameDelete::Update - cloud delete succeeded");
							returnState = MEM_CARD_COMPLETE;
						}
						else
						{
							savegameDisplayf("CSavegameDelete::Update - cloud delete failed");
							returnState = MEM_CARD_ERROR;
						}
					}
				}
				else
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_CLOUD_DELETE_FAILED);
					ms_DeleteStatus = SAVEGAME_DELETE_HANDLE_ERRORS;
				}
			}
			break;
/*
		case SAVEGAME_DELETE_WAIT_FOR_DELETING_MESSAGE_TO_FINISH :
			if (CSavingMessage::HasLoadingMessageDisplayedForLongEnough())
			{
//				CSavegameDialogScreens::SetMessageAsProcessing(false);	//	GSWTODO - copied from CSavegameLoad::GenericLoad() Not sure if it's relevant here

				returnState = MEM_CARD_COMPLETE;
			}
			break;
*/
		case SAVEGAME_DELETE_HANDLE_ERRORS :
			CSavingMessage::Clear();

			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				returnState = MEM_CARD_ERROR;
			}
			break;
	}

	if ( (returnState == MEM_CARD_ERROR) || (returnState == MEM_CARD_COMPLETE) )
	{
#if !__NO_OUTPUT
		if (returnState == MEM_CARD_COMPLETE)
		{
			savegameDisplayf("CSavegameDelete::Update - succeeded");
		}
		else
		{
			savegameDisplayf("CSavegameDelete::Update - failed");
		}
#endif	//	!__NO_OUTPUT
		CSavingMessage::Clear();

		EndDelete();
		CSavegameDialogScreens::SetMessageAsProcessing(false);
	}

	return returnState;
}

// ******************************************** Delete Cloud File ********************************************

CloudFileDeleter::CloudFileDeleter()
{
	Initialise();
}

void CloudFileDeleter::Initialise()
{
	m_CloudRequestId = -1;
	m_bIsRequestPending = false;
	m_bLastDeletionSucceeded = false;
}

bool CloudFileDeleter::RequestDeletionOfCloudFile(const char *pPathToFileToDelete)
{
	if(savegameVerifyf(!m_bIsRequestPending, "CloudFileDeleter::RequestDeletionOfCloudFile - A request is already in progress"))
	{
		Initialise();
		if (NetworkInterface::IsCloudAvailable())
		{
			savegameDisplayf("CloudFileDeleter::RequestDeletionOfCloudFile - requesting delete of %s", pPathToFileToDelete);

			m_CloudRequestId = CloudManager::GetInstance().RequestDeleteMemberFile(NetworkInterface::GetLocalGamerIndex(), pPathToFileToDelete);
			m_bIsRequestPending = true;
			return true;
		}
		else
		{
			savegameDisplayf("CloudFileDeleter::RequestDeletionOfCloudFile - cloud is not available");
		}
	}

	return false;
}

void CloudFileDeleter::OnCloudEvent(const CloudEvent* pEvent)
{
	// we only care about requests
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
		return;

	// grab event data
	const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

	// check if we're interested
	if(pEventData->nRequestID != m_CloudRequestId)
		return;

	m_bIsRequestPending = false;
	m_CloudRequestId = -1;

	m_bLastDeletionSucceeded = pEventData->bDidSucceed;

#if !__NO_OUTPUT
	if (m_bLastDeletionSucceeded)
	{
		savegameDisplayf("CloudFileDeleter::OnCloudEvent - deletion succeeded");
	}
	else
	{
		savegameDisplayf("CloudFileDeleter::OnCloudEvent - deletion failed");
	}
#endif	//	!__NO_OUTPUT
}


