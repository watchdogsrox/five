
#include "SaveLoad/savegame_save_block.h"

#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_users_and_devices.h"


SAVEGAME_OPTIMISATIONS()


// ******************************************** CloudFileSaveListener ********************************************

enum eTypeOfFileToUpload
{
	UPLOADING_FILE_TYPE_JPEG,
	UPLOADING_FILE_TYPE_JSON
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	,UPLOADING_FILE_TYPE_SINGLE_PLAYER_SAVEGAME
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
};

class CloudFileSaveListener : public CloudListener
{
public:
	CloudFileSaveListener();

	virtual ~CloudFileSaveListener() {}

	bool BeginSavingFileToCloud(const char *pFilename, const u8 *pBufferToSave, u32 sizeOfBuffer, eTypeOfFileToUpload fileType);
	MemoryCardError UpdateSavingFileToCloud(eTypeOfFileToUpload fileType);

	virtual void OnCloudEvent(const CloudEvent* pEvent);

	bool GetIsRequestPending() { return m_bIsRequestPending; }
	bool GetLastSaveSucceeded() { return m_bLastSaveSucceeded; }

private:
	void Initialise();
	bool RequestSaveOfCloudFile(const char *pPathToFileForSaving, const u8 *pBufferToSave, u32 sizeOfBuffer);

	CloudRequestID m_CloudRequestId;
	bool m_bIsRequestPending;
	bool m_bLastSaveSucceeded;
};


CloudFileSaveListener::CloudFileSaveListener()
{
	Initialise();
}

void CloudFileSaveListener::Initialise()
{
	m_CloudRequestId = -1;
	m_bIsRequestPending = false;
	m_bLastSaveSucceeded = false;
}

bool CloudFileSaveListener::BeginSavingFileToCloud(const char *pFilename, const u8 *pBufferToSave, u32 sizeOfBuffer, eTypeOfFileToUpload fileType)
{
	if (!CSavegameUsers::HasPlayerJustSignedOut() && NetworkInterface::IsCloudAvailable())
	{
		savegameDisplayf("CloudFileSaveListener::BeginSavingFileToCloud - sending: %s (%u bytes) to the cloud.", pFilename, sizeOfBuffer);

		if (RequestSaveOfCloudFile(pFilename, pBufferToSave, sizeOfBuffer))
		{
			savegameDisplayf("CloudFileSaveListener::BeginSavingFileToCloud - RequestSaveOfCloudFile succeeded");
			return true;
		}
		else
		{
			savegameDisplayf("CloudFileSaveListener::BeginSavingFileToCloud - RequestSaveOfCloudFile failed");

			SaveGameDialogCode dialogCode = SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_JPEG_FAILED;
			switch (fileType)
			{
				case UPLOADING_FILE_TYPE_JPEG :
					dialogCode = SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_JPEG_FAILED;
					break;
				case UPLOADING_FILE_TYPE_JSON :
					dialogCode = SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_JSON_FAILED;
					break;
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				case UPLOADING_FILE_TYPE_SINGLE_PLAYER_SAVEGAME :
					dialogCode = SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_SP_SAVEGAME_FAILED;
					break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
			}
			CSavegameDialogScreens::SetSaveGameError(dialogCode);
		}
	}
	else
	{
		photoDisplayf("CloudFileSaveListener::BeginSavingFileToCloud - player signed out or cloud not available");
		CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
	}

	return false;
}

MemoryCardError CloudFileSaveListener::UpdateSavingFileToCloud(eTypeOfFileToUpload fileType)
{
#if !__NO_OUTPUT
	const char *pFileTypeString = "JPEG";
#endif	//	!__NO_OUTPUT

	SaveGameDialogCode dialogCode = SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_JPEG_FAILED;
	switch (fileType)
	{
	case UPLOADING_FILE_TYPE_JPEG :
		dialogCode = SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_JPEG_FAILED;
#if !__NO_OUTPUT
		pFileTypeString = "JPEG";
#endif	//	!__NO_OUTPUT
		break;
	case UPLOADING_FILE_TYPE_JSON :
		dialogCode = SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_JSON_FAILED;
#if !__NO_OUTPUT
		pFileTypeString = "JSON";
#endif	//	!__NO_OUTPUT
		break;
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	case UPLOADING_FILE_TYPE_SINGLE_PLAYER_SAVEGAME :
		dialogCode = SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_SP_SAVEGAME_FAILED;
#if !__NO_OUTPUT
		pFileTypeString = "SINGLE PLAYER SAVEGAME";
#endif	//	!__NO_OUTPUT
		break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	}

	savegameDisplayf("CloudFileSaveListener::UpdateSavingFileToCloud - checking status of pending %s cloud request", pFileTypeString);
	if (GetIsRequestPending() == false)
	{
		if (GetLastSaveSucceeded())
		{
			savegameDisplayf("CloudFileSaveListener::UpdateSavingFileToCloud - %s cloud save succeeded", pFileTypeString);
			return MEM_CARD_COMPLETE;
		}
		else
		{
			savegameDisplayf("CloudFileSaveListener::UpdateSavingFileToCloud - %s cloud save failed", pFileTypeString);
			CSavegameDialogScreens::SetSaveGameError(dialogCode);
			return MEM_CARD_ERROR;
		}
	}

	return MEM_CARD_BUSY;
}


bool CloudFileSaveListener::RequestSaveOfCloudFile(const char *pPathToFileForSaving, const u8 *pBufferToSave, u32 sizeOfBuffer)
{
	if(savegameVerifyf(!m_bIsRequestPending, "CloudFileSaveListener::RequestSaveOfCloudFile - A request is already in progress"))
	{
		Initialise();
		if (NetworkInterface::IsCloudAvailable())
		{
			savegameDisplayf("CloudFileSaveListener::RequestSaveOfCloudFile - requesting save of %s", pPathToFileForSaving);

			m_CloudRequestId = CloudManager::GetInstance().RequestPostMemberFile(NetworkInterface::GetLocalGamerIndex(), pPathToFileForSaving, pBufferToSave, sizeOfBuffer, true);	//	last param is bCache
			m_bIsRequestPending = true;
			return true;
		}
		else
		{
			savegameDisplayf("CloudFileSaveListener::RequestSaveOfCloudFile - cloud is not available");
		}
	}

	return false;
}

void CloudFileSaveListener::OnCloudEvent(const CloudEvent* pEvent)
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

	m_bLastSaveSucceeded = pEventData->bDidSucceed;

#if !__NO_OUTPUT
	if (m_bLastSaveSucceeded)
	{
		savegameDisplayf("CloudFileSaveListener::OnCloudEvent - save succeeded");
	}
	else
	{
		savegameDisplayf("CloudFileSaveListener::OnCloudEvent - save failed");
	}
#endif	//	!__NO_OUTPUT
}


// ******************************************** CSavegameSaveBlock ********************************************

CloudFileSaveListener *CSavegameSaveBlock::ms_pCloudFileSaveListener = NULL;
eSaveBlockState	CSavegameSaveBlock::ms_SaveBlockStatus = SAVE_BLOCK_DO_NOTHING;
//	eLocalBlockSaveSubstate CSavegameSaveBlock::ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_SAVE;
eCloudBlockSaveSubstate CSavegameSaveBlock::ms_CloudBlockSaveSubstate = CLOUD_BLOCK_SAVE_SUBSTATE_INIT;


u32 CSavegameSaveBlock::ms_SizeOfBufferToBeSaved = 0;
const u8 *CSavegameSaveBlock::ms_pBufferToBeSaved = NULL;
//	bool CSavegameSaveBlock::ms_IsCloudOperation = false;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
bool CSavegameSaveBlock::ms_bIsAPhoto = false;

const char *CSavegameSaveBlock::ms_pJsonData = NULL;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME


void CSavegameSaveBlock::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
//		ms_IsCloudOperation = false;
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		ms_pJsonData = NULL;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

		ms_pCloudFileSaveListener = rage_new CloudFileSaveListener;
		savegameAssertf(ms_pCloudFileSaveListener, "CSavegameSaveBlock::Init - failed to create ms_pCloudFileSaveListener");
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_SaveBlockStatus = SAVE_BLOCK_DO_NOTHING;
	}
}

void CSavegameSaveBlock::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		delete ms_pCloudFileSaveListener;
		ms_pCloudFileSaveListener = NULL;
	}
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
	}
}

/*
MemoryCardError CSavegameSaveBlock::BeginSaveBlock(int SlotNumber, const u8 *pBufferToSave, u32 SizeOfBufferToSave, const bool isCloudOperation, const char *pJsonData, bool bIsPhotoFile)
{
	if (savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameSaveBlock::BeginSaveBlock - operation is not OPERATION_NONE"))
	{
		if (savegameVerifyf(ms_SaveBlockStatus == SAVE_BLOCK_DO_NOTHING, "CSavegameSaveBlock::BeginSaveBlock - "))
		{
			if (savegameVerifyf(SizeOfBufferToSave > 0, "CSavegameSaveBlock::BeginSaveBlock - buffer to save has size %d", SizeOfBufferToSave))
			{
				if (savegameVerifyf(pBufferToSave, "CSavegameSaveBlock::BeginSaveBlock - buffer to save is NULL"))
				{
					if (isCloudOperation && bIsPhotoFile)
					{
						CSavegameFilenames::CreateFilenameForNewCloudPhoto();
					}
					else
					{
						photoAssertf(!bIsPhotoFile, "CSavegameSaveBlock::BeginSaveBlock - local photos aren't supported");
						CSavegameFilenames::MakeValidSaveName(SlotNumber, isCloudOperation);
					}

					if (!isCloudOperation)
					{
						savegameAssertf(0, "CSavegameSaveBlock::BeginSaveBlock - just checking if this is ever called for local saves now - Graeme");
#if __PPU
						CSavegameList::SetSlotToUpdateOnceSaveHasCompleted(SlotNumber);
#endif	//	__PPU
					}

					ms_SaveBlockStatus = SAVE_BLOCK_INITIAL_CHECKS;

					ms_SizeOfBufferToBeSaved = SizeOfBufferToSave;
					ms_pBufferToBeSaved = pBufferToSave;
					ms_IsCloudOperation = isCloudOperation;

					ms_bPhotoForMissionCreator = false;

					CSavegameInitialChecks::SetSizeOfBufferToBeSaved(ms_SizeOfBufferToBeSaved);
					CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_BLOCK);
					CSavegameInitialChecks::BeginInitialChecks(false);

					if (pJsonData && (!isCloudOperation || !bIsPhotoFile))
					{
						savegameAssertf(0, "CSavegameSaveBlock::BeginSaveBlock - only expected a json file to be specified for a cloud photo");
						pJsonData = NULL;
					}
					ms_pJsonData = pJsonData;

					return MEM_CARD_COMPLETE;
				}
			}
		}
	}

	return MEM_CARD_ERROR;
}
*/

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
MemoryCardError CSavegameSaveBlock::BeginUploadOfSavegameToCloud(const char *pFilename, const u8 *pBufferToSave, u32 SizeOfBufferToSave, const char *pJsonData)
{
	return BeginSaveToCloud(pFilename, pBufferToSave, SizeOfBufferToSave, false, pJsonData);
}
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

MemoryCardError CSavegameSaveBlock::BeginSaveOfMissionCreatorPhoto(const char *pFilename, const u8 *pBufferToSave, u32 SizeOfBufferToSave)
{
	return BeginSaveToCloud(pFilename, pBufferToSave, SizeOfBufferToSave
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		, true, NULL
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		);
}

MemoryCardError CSavegameSaveBlock::BeginSaveToCloud(const char *pFilename, const u8 *pBufferToSave, u32 SizeOfBufferToSave
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	, bool bIsAPhoto, const char *pJsonData
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	)
{
	if (savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameSaveBlock::BeginSaveToCloud - operation is not OPERATION_NONE"))
	{
		if (savegameVerifyf(ms_SaveBlockStatus == SAVE_BLOCK_DO_NOTHING, "CSavegameSaveBlock::BeginSaveToCloud - "))
		{
			if (savegameVerifyf(SizeOfBufferToSave > 0, "CSavegameSaveBlock::BeginSaveToCloud - buffer to save has size %d", SizeOfBufferToSave))
			{
				if (savegameVerifyf(pBufferToSave, "CSavegameSaveBlock::BeginSaveToCloud - buffer to save is NULL"))
				{
					CSavegameFilenames::SetFilenameOfCloudFile(pFilename);

					ms_SaveBlockStatus = SAVE_BLOCK_INITIAL_CHECKS;

					ms_SizeOfBufferToBeSaved = SizeOfBufferToSave;
					ms_pBufferToBeSaved = pBufferToSave;
//					ms_IsCloudOperation = true;

					CSavegameInitialChecks::SetSizeOfBufferToBeSaved(ms_SizeOfBufferToBeSaved);

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
					ms_pJsonData = pJsonData;
					ms_bIsAPhoto = bIsAPhoto;
					if (!ms_bIsAPhoto)
					{
						CGenericGameStorage::SetSaveOperation(OPERATION_UPLOADING_SAVEGAME_TO_CLOUD);
					}
					else
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
					{
						CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_PHOTO_TO_CLOUD);
					}

					CSavegameInitialChecks::BeginInitialChecks(false);

					return MEM_CARD_COMPLETE;
				}
			}
		}
	}

	return MEM_CARD_ERROR;
}


// void CSavegameSaveBlock::EndSaveBlock()
// {
// 	ms_SaveBlockStatus = SAVE_BLOCK_DO_NOTHING;
// 	Assertf(CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_BLOCK, "CSavegameSaveBlock::EndSaveBlock - Operation should be OPERATION_SAVING_BLOCK");
// 	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
// }


MemoryCardError CSavegameSaveBlock::SaveBlock(bool bDisplayErrors)
{
	MemoryCardError ReturnValue = MEM_CARD_BUSY;
	switch (ms_SaveBlockStatus)
	{
		case SAVE_BLOCK_DO_NOTHING :
			return MEM_CARD_COMPLETE;
//			break;

		case SAVE_BLOCK_INITIAL_CHECKS :
			{
//	This gets displayed over the top of dialog messages in autosave so I've commented it out
//				if (ms_IsCloudOperation)
				{
					CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_SAVING);
				}
// 				else
// 				{
// 					CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);
// 				}

				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_SaveBlockStatus = SAVE_BLOCK_HANDLE_ERRORS;
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
					ms_SaveBlockStatus = SAVE_BLOCK_BEGIN_SAVE;
				}
			}
			break;

		case SAVE_BLOCK_BEGIN_SAVE :
			{
				ms_SaveBlockStatus = SAVE_BLOCK_DO_SAVE;

//				if (ms_IsCloudOperation)
				{
					ms_CloudBlockSaveSubstate = CLOUD_BLOCK_SAVE_SUBSTATE_INIT;
				}
// 				else
// 				{
// 					ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_SAVE;
// 				}
			}
			//	deliberately fall through here

		case SAVE_BLOCK_DO_SAVE :
		{
			MemoryCardError SaveResult;
//			if( ms_IsCloudOperation )
			{
				SaveResult = DoSaveBlockCloud();
			}
// 			else
// 			{
// 				SaveResult = DoSaveBlock();
// 			}

			switch (SaveResult)
			{
				case MEM_CARD_COMPLETE :
					{
						//	CPad::FixPadsAfterSave(); rage
//						CStatsMgr::PlayerSaveGame( (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING) );
						ReturnValue = MEM_CARD_COMPLETE;
					}
					break;

				case MEM_CARD_BUSY :
					//	Do nothing - have to wait till it's complete
					break;
				case MEM_CARD_ERROR :
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_SaveBlockStatus = SAVE_BLOCK_HANDLE_ERRORS;
					}
					break;

				default :
					Assertf(0, "CSavegameSaveBlock::SaveBlock - unexpected return value from CSavegameSaveBlock::DoSaveBlock");
					break;
			}
		}
			break;

		case SAVE_BLOCK_HANDLE_ERRORS :
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

//		if( ms_IsCloudOperation )
		{
			EndSaveBlockCloud();
		}
// 		else
// 		{
// 			EndSaveBlock();
// 		}

// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
		CSavegameDialogScreens::SetMessageAsProcessing(false);
	}

	return ReturnValue;
}

MemoryCardError CSavegameSaveBlock::DoSaveBlockCloud()
{
	MemoryCardError RetStatus = MEM_CARD_BUSY;

	switch (ms_CloudBlockSaveSubstate)
	{
		case CLOUD_BLOCK_SAVE_SUBSTATE_INIT:
			if (savegameVerifyf(ms_pCloudFileSaveListener, "CSavegameSaveBlock::DoSaveBlockCloud - ms_pCloudFileSaveListener is NULL"))
			{
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				if (ms_pCloudFileSaveListener->BeginSavingFileToCloud(CSavegameFilenames::GetFilenameOfCloudFile(), ms_pBufferToBeSaved, ms_SizeOfBufferToBeSaved, ms_bIsAPhoto?UPLOADING_FILE_TYPE_JPEG:UPLOADING_FILE_TYPE_SINGLE_PLAYER_SAVEGAME))
#else	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				if (ms_pCloudFileSaveListener->BeginSavingFileToCloud(CSavegameFilenames::GetFilenameOfCloudFile(), ms_pBufferToBeSaved, ms_SizeOfBufferToBeSaved, UPLOADING_FILE_TYPE_JPEG))
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				{
					ms_CloudBlockSaveSubstate = CLOUD_BLOCK_SAVE_SUBSTATE_UPDATE;
				}
				else
				{
					RetStatus = MEM_CARD_ERROR;
				}
			}
			else
			{
				RetStatus = MEM_CARD_ERROR;
			}
			break;

		case CLOUD_BLOCK_SAVE_SUBSTATE_UPDATE:
			{
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				switch (ms_pCloudFileSaveListener->UpdateSavingFileToCloud(ms_bIsAPhoto?UPLOADING_FILE_TYPE_JPEG:UPLOADING_FILE_TYPE_SINGLE_PLAYER_SAVEGAME))
#else	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				switch (ms_pCloudFileSaveListener->UpdateSavingFileToCloud(UPLOADING_FILE_TYPE_JPEG))
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				{
					case MEM_CARD_COMPLETE :
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
						if (!ms_bIsAPhoto && ms_pJsonData)
						{
							savegameDisplayf("CSavegameSaveBlock::DoSaveBlockCloud - this savegame has json data so begin saving the json data now");
							ms_CloudBlockSaveSubstate = CLOUD_BLOCK_SAVE_SUBSTATE_INIT_JSON_SAVE;	// Keep going for the json file.
						}
						else
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
						{
							savegameDisplayf("CSavegameSaveBlock::DoSaveBlockCloud - UpdateSavingFileToCloud has returned MEM_CARD_COMPLETE");
							return MEM_CARD_COMPLETE;
						}
						break;
					case MEM_CARD_BUSY :
						break;
					case MEM_CARD_ERROR :
						savegameDisplayf("CSavegameSaveBlock::DoSaveBlockCloud - UpdateSavingFileToCloud has returned MEM_CARD_ERROR");
						RetStatus = MEM_CARD_ERROR;
						break;
				}
			}
			break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		// Write the json file along side the data file.
		case CLOUD_BLOCK_SAVE_SUBSTATE_INIT_JSON_SAVE:
			{
				const char* tmpNameOfFile = CSavegameFilenames::GetFilenameOfCloudFile();
				int len = istrlen( tmpNameOfFile );
				if( len > 4 )
				{
					char jsonPath[SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE];
					safecpy( jsonPath, tmpNameOfFile, NELEM(jsonPath) );
					strcpy( &jsonPath[len-4], "json" );

					int jsonLength = istrlen(ms_pJsonData);

					if (ms_pCloudFileSaveListener->BeginSavingFileToCloud(jsonPath, (const u8*) ms_pJsonData, jsonLength, UPLOADING_FILE_TYPE_JSON))
					{
						ms_CloudBlockSaveSubstate = CLOUD_BLOCK_SAVE_SUBSTATE_UPDATE_JSON_SAVE;
					}
					else
					{
						RetStatus = MEM_CARD_ERROR;
					}
				}
				else
				{
					savegameAssertf(0, "CSavegameSaveBlock::DoSaveBlockCloud - failed to append json to %s", CSavegameFilenames::GetFilenameOfCloudFile());

					RetStatus = MEM_CARD_ERROR;	//	MEM_CARD_COMPLETE;
				}
			}
			break;

		case CLOUD_BLOCK_SAVE_SUBSTATE_UPDATE_JSON_SAVE:
			switch (ms_pCloudFileSaveListener->UpdateSavingFileToCloud(UPLOADING_FILE_TYPE_JSON))
			{
			case MEM_CARD_COMPLETE :
				RetStatus = MEM_CARD_COMPLETE;
				break;

			case MEM_CARD_BUSY :
				break;

			case MEM_CARD_ERROR :
				RetStatus = MEM_CARD_ERROR;
				break;
			}
			break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

// 		default:
// 			break;
	}

	return(RetStatus);
}

void CSavegameSaveBlock::EndSaveBlockCloud()
{
	ms_SaveBlockStatus = SAVE_BLOCK_DO_NOTHING;
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	if (ms_bIsAPhoto)
	{
		Assertf(CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_PHOTO_TO_CLOUD, "CSavegameSaveBlock::EndSaveBlock - Operation should be OPERATION_SAVING_PHOTO_TO_CLOUD");
	}
	else
	{
		Assertf(CGenericGameStorage::GetSaveOperation() == OPERATION_UPLOADING_SAVEGAME_TO_CLOUD, "CSavegameSaveBlock::EndSaveBlock - Operation should be OPERATION_UPLOADING_SAVEGAME_TO_CLOUD");
	}
#else	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	Assertf(CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_PHOTO_TO_CLOUD, "CSavegameSaveBlock::EndSaveBlock - Operation should be OPERATION_SAVING_PHOTO_TO_CLOUD");
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}

/*
MemoryCardError CSavegameSaveBlock::DoSaveBlock()
{
#if __XENON || __PPU || __WIN32PC || RSG_ORBIS

#if __BANK
	char outFilename[CGenericGameStorage::ms_MaxLengthOfPcSaveFileName];	//	SAVE_GAME_MAX_FILENAME_LENGTH+8];
	FileHandle fileHandle = INVALID_FILE_HANDLE;
#endif	//	__BANK

	switch (ms_LocalBlockSaveSubstate)
	{
		case LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_SAVE :
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

#if __BANK
			if (CGenericGameStorage::ms_bSaveToPC)
			{
				ASSET.PushFolder(CGenericGameStorage::ms_NameOfPCSaveFolder);

				if (strlen(CGenericGameStorage::ms_NameOfPCSaveFile) > 0)
				{
					strcpy(outFilename, CGenericGameStorage::ms_NameOfPCSaveFile);
				}
				else
				{
					strcpy(outFilename, CSavegameFilenames::GetNameOfFileOnDisc());
				}

//				strcat(outFilename, ".pic");
				strcat(outFilename, ".xml");

				fileHandle = CFileMgr::OpenFileForWriting(outFilename);
				if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CSavegameSaveBlock::DoSaveBlock - failed to open savegame file %s on PC for writing. Maybe the folder X:/gta5/build/dev/saves doesn't exist", outFilename))
				{
					CFileMgr::Write(fileHandle, (const char *) ms_pBufferToBeSaved, ms_SizeOfBufferToBeSaved);
					CFileMgr::CloseFile(fileHandle);
				}
				ASSET.PopFolder();
			}
#endif	//	__BANK

#if __BANK
			if (!CGenericGameStorage::ms_bSaveToPC || CGenericGameStorage::ms_bSaveToConsoleWhenSavingToPC)
#endif	//	__BANK
			{
				if (!SAVEGAME.BeginSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), CSavegameFilenames::GetNameToDisplay(), CSavegameFilenames::GetNameOfFileOnDisc(), ms_pBufferToBeSaved, ms_SizeOfBufferToBeSaved, true) )	//	bool overwrite, CGameLogic::GetCurrentEpisodeIndex())
				{
					Assertf(0, "CSavegameSaveBlock::DoSaveBlock - BeginSave failed");
					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - BeginSave failed\n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_SAVE_FAILED);
					return MEM_CARD_ERROR;
				}
				ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_CHECK_SAVE;
			}
#if __BANK
			else
			{
				savegameDisplayf("CSavegameSaveBlock::DoSaveBlock - Finished saving to the PC and the widgets aren't set to save to the console so finish now");
				return MEM_CARD_COMPLETE;
			}
#endif	//	__BANK

			//	break;

		case LOCAL_BLOCK_SAVE_SUBSTATE_CHECK_SAVE :
		{
			bool bOutIsValid = false;
			bool bFileExists = false;
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			if (SAVEGAME.CheckSave(CSavegameUsers::GetUser(), bOutIsValid, bFileExists))
			{	//	returns true for error or success, false for busy

				// GSWTODO - bOutIsValid could be quite useful.
				//	I think it only returns TRUE if the save was finished successfully.
				//	So should be false if Memory card is removed half way through saving.

//				Assertf(bOutIsValid, "CSavegameSaveBlock::DoSaveBlock - CheckSave - bOutIsValid is false (whatever that means)");
//				Assertf(bFileExists, "CSavegameSaveBlock::DoSaveBlock - CheckSave - bFileExists is true. Is that good?");
				SAVEGAME.EndSave(CSavegameUsers::GetUser());

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
					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2);
					return MEM_CARD_ERROR;
				}

// 				#if __PPU
// 					CSavegameList::UpdateSlotDataAfterSave();
// 				#endif

#if __PPU
				if (!SAVEGAME.BeginGetFileModifiedTime(CSavegameUsers::GetUser(), CSavegameFilenames::GetNameOfFileOnDisc()))
				{
					Assertf(0, "CSavegameSaveBlock::DoSaveBlock - BeginGetFileModifiedTime failed");
					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - BeginGetFileModifiedTime failed\n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_GET_FILE_MODIFIED_TIME_FAILED);
					return MEM_CARD_ERROR;
				}
				ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_GET_MODIFIED_TIME;
#else
//				ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_ICON_SAVE;
				return MEM_CARD_COMPLETE;
#endif	//	__PPU
			}
		}
			break;

#if __PPU
		case LOCAL_BLOCK_SAVE_SUBSTATE_GET_MODIFIED_TIME :
			{
	// #if __XENON
	// 			SAVEGAME.UpdateClass();
	// #endif
				u32 ModifiedTimeHigh = 0;
				u32 ModifiedTimeLow = 0;
				if (SAVEGAME.CheckGetFileModifiedTime(CSavegameUsers::GetUser(), ModifiedTimeHigh, ModifiedTimeLow))
				{	//	returns true for error or success, false for busy

					SAVEGAME.EndGetFileModifiedTime(CSavegameUsers::GetUser());

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
						savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1 \n");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1);
						return MEM_CARD_ERROR;
					}

					if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
					{
						savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2 \n");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2);
						return MEM_CARD_ERROR;
					}

					CSavegameList::UpdateSlotDataAfterSave(ModifiedTimeHigh, ModifiedTimeLow);

//					ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_ICON_SAVE;
					return MEM_CARD_COMPLETE;
				}
			}
			break;
#endif	//	__PPU


// 		case LOCAL_BLOCK_SAVE_SUBSTATE_BEGIN_ICON_SAVE :
// 			if (ms_pSaveGameBlockIcon)
// 			{
// 				if (!SAVEGAME.BeginIconSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), CSavegameFilenames::GetNameOfFileOnDisc(), static_cast<const void *>(ms_pSaveGameBlockIcon), ms_SizeOfSaveGameBlockIcon))
// 				{
// 					Assertf(0, "CSavegameSaveBlock::DoSaveBlock - BeginIconSave failed");
// 					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - BeginIconSave failed\n");
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_ICON_SAVE_FAILED);
// 					return MEM_CARD_ERROR;
// 				}
// 				ms_LocalBlockSaveSubstate = LOCAL_BLOCK_SAVE_SUBSTATE_CHECK_ICON_SAVE;
// 			}
// 			else
// 			{
// 				return MEM_CARD_COMPLETE;
// 			}
// 			break;

// 		case LOCAL_BLOCK_SAVE_SUBSTATE_CHECK_ICON_SAVE :
// 
// #if __XENON
// 			SAVEGAME.UpdateClass();
// #endif
// 			if (SAVEGAME.CheckIconSave(CSavegameUsers::GetUser()))
// 			{	//	returns true for error or success, false for busy
// 				SAVEGAME.EndIconSave(CSavegameUsers::GetUser());
// 
// 				if (CSavegameUsers::HasPlayerJustSignedOut())
// 				{
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
// 					return MEM_CARD_ERROR;
// 				}
// 
// 				if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
// 				{
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
// 					return MEM_CARD_ERROR;
// 				}
// 
// 				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
// 				{
// 					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1 \n");
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1);
// 					return MEM_CARD_ERROR;
// 				}
// 
// 				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
// 				{
// 					savegameDebugf1("CSavegameSaveBlock::DoSaveBlock - SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2 \n");
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2);
// 					return MEM_CARD_ERROR;
// 				}
// 
// 				return MEM_CARD_COMPLETE;
// 			}
// 			break;
	}
#else	//	#if __XENON || __PPU
	Assertf(0, "CSavegameSaveBlock::DoSaveBlock - save game only implemented for 360, PC, PS3 and Orbis so far");
#endif

	return MEM_CARD_BUSY;
}
*/


