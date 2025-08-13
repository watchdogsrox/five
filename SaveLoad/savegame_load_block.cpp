
#include "SaveLoad/savegame_load_block.h"

// Rage headers
#include "file/savegame.h"

// Game headers
//	#include "renderer/ScreenshotManager.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_get_file_size.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_users_and_devices.h"

SAVEGAME_OPTIMISATIONS()


CloudFileLoadListener *CSavegameLoadJpegFromCloud::ms_pCloudFileLoadListener = NULL;
eLoadJpegFromCloudState	CSavegameLoadJpegFromCloud::ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_DO_NOTHING;
eBlockLoadStateCloud CSavegameLoadJpegFromCloud::ms_CloudLoadSubstate = BLOCK_LOAD_STATE_CLOUD_INIT;

CPhotoBuffer *CSavegameLoadJpegFromCloud::ms_pBufferToContainJpeg = NULL;
u32 CSavegameLoadJpegFromCloud::ms_MaxSizeOfBuffer = 0;
rlCloudNamespace CSavegameLoadJpegFromCloud::ms_CloudNamespace = RL_CLOUD_NAMESPACE_INVALID;
sRequestData CSavegameLoadJpegFromCloud::ms_RequestData; 
bool CSavegameLoadJpegFromCloud::ms_bDisplayLoadError = true;

void CSavegameLoadJpegFromCloud::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		ms_pCloudFileLoadListener = rage_new CloudFileLoadListener;
		savegameAssertf(ms_pCloudFileLoadListener, "CSavegameLoadJpegFromCloud::Init - failed to create ms_pCloudFileLoadListener");
	}
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_DO_NOTHING;
	}
}

void CSavegameLoadJpegFromCloud::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		delete ms_pCloudFileLoadListener;
		ms_pCloudFileLoadListener = NULL;
	}
}


MemoryCardError CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud(const sRequestData &requestData, CPhotoBuffer *pBufferToContainJpeg, u32 maxSizeOfBuffer, rlCloudNamespace cloudNamespace, bool bDisplayLoadError)
{
	if (savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud - operation is not OPERATION_NONE"))
	{
		if (savegameVerifyf(ms_LoadJpegFromCloudStatus == LOAD_JPEG_FROM_CLOUD_DO_NOTHING, "CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud - "))
		{
			if (savegameVerifyf(pBufferToContainJpeg, "CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud - pBufferToContainJpeg is NULL"))
			{
				ms_pBufferToContainJpeg = pBufferToContainJpeg;
				ms_MaxSizeOfBuffer = maxSizeOfBuffer;
                ms_CloudNamespace = cloudNamespace;
                ms_RequestData = requestData;
				ms_bDisplayLoadError = bDisplayLoadError;

				ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_INITIAL_CHECKS;

				CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_PHOTO_FROM_CLOUD);
				CSavegameInitialChecks::BeginInitialChecks(false);

				return MEM_CARD_COMPLETE;
			}
		}
	}

	return MEM_CARD_ERROR;
}


void CSavegameLoadJpegFromCloud::EndLoadBlockCloud()
{
	ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_DO_NOTHING;
	Assertf(CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_PHOTO_FROM_CLOUD, "CSavegameLoadJpegFromCloud::EndLoadBlockCloud - SaveOperation should be OPERATION_LOADING_PHOTO_FROM_CLOUD");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


MemoryCardError CSavegameLoadJpegFromCloud::LoadJpegFromCloud()
{
	MemoryCardError returnStatus = MEM_CARD_BUSY;

	switch (ms_LoadJpegFromCloudStatus)
	{
	case LOAD_JPEG_FROM_CLOUD_DO_NOTHING :
		savegameAssertf(0, "CSavegameLoadJpegFromCloud::LoadJpegFromCloud - didn't expect ms_LoadJpegFromCloudStatus == LOAD_JPEG_FROM_CLOUD_DO_NOTHING");
		return MEM_CARD_COMPLETE;

	case LOAD_JPEG_FROM_CLOUD_INITIAL_CHECKS :
		{
			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

			MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
			if (SelectionState == MEM_CARD_ERROR)
			{
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					returnStatus = MEM_CARD_ERROR;
				}
				else
				{
					ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_HANDLE_ERRORS;
				}
			}
			else if (SelectionState == MEM_CARD_COMPLETE)
			{
				ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_BEGIN_LOAD;
			}
		}
		break;

	case LOAD_JPEG_FROM_CLOUD_BEGIN_LOAD :
		{
			ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_DO_LOAD;

			ms_CloudLoadSubstate = BLOCK_LOAD_STATE_CLOUD_INIT;
		}
		//	deliberately fall through here

	case LOAD_JPEG_FROM_CLOUD_DO_LOAD :
		{
			MemoryCardError LoadStatus = DoLoadBlockCloud();

			switch (LoadStatus)
			{
			case MEM_CARD_COMPLETE :
				returnStatus =  MEM_CARD_COMPLETE;
				break;

			case MEM_CARD_BUSY :
				// Don't do anything
				break;

			case MEM_CARD_ERROR :
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					returnStatus = MEM_CARD_ERROR;
				}
				else
				{
					ms_LoadJpegFromCloudStatus = LOAD_JPEG_FROM_CLOUD_HANDLE_ERRORS;
				}
				break;
			}
		}
		break;

	case LOAD_JPEG_FROM_CLOUD_HANDLE_ERRORS :
		CSavingMessage::Clear();

		if (CSavegameDialogScreens::HandleSaveGameErrorCode())
		{
			returnStatus = MEM_CARD_ERROR;
		}
		break;
	}

	if (returnStatus != MEM_CARD_BUSY)
	{
		savegameWarningf("Error in CSavegameLoadJpegFromCloud::LoadJpegFromCloud: code=%d", CSavegameDialogScreens::GetMostRecentErrorCode());

		CSavingMessage::Clear();

		EndLoadBlockCloud();

		// The following is called for savegames. I'm not sure if it's needed for photos
		CSavegameDialogScreens::SetMessageAsProcessing(false);
	}

	return returnStatus;
}


MemoryCardError CSavegameLoadJpegFromCloud::DoLoadBlockCloud()
{
	MemoryCardError RetStatus = MEM_CARD_BUSY;

	switch (ms_CloudLoadSubstate)
	{
		case BLOCK_LOAD_STATE_CLOUD_INIT:
			if (savegameVerifyf(ms_pCloudFileLoadListener, "CSavegameLoadJpegFromCloud::DoLoadBlockCloud - ms_pCloudFileLoadListener is NULL"))
			{
				if (ms_pCloudFileLoadListener->BeginLoadingJpegFromCloud(ms_RequestData, ms_pBufferToContainJpeg, ms_MaxSizeOfBuffer, ms_CloudNamespace, ms_bDisplayLoadError))
				{
					ms_CloudLoadSubstate = BLOCK_LOAD_STATE_CLOUD_UPDATE;
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

		case BLOCK_LOAD_STATE_CLOUD_UPDATE:
			{
				switch (ms_pCloudFileLoadListener->UpdateLoadingJpegFromCloud(ms_bDisplayLoadError))
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
			}
			break;

		default:
			break;
	}
	return(RetStatus);
}


/*
MemoryCardError CSavegameLoadBlock::DoLoadBlock(u8 *pBuffer)
{
	enum eBlockLoadState
	{
//	Savegames do a Creator check first. I've removed that from Block Load
		BLOCK_LOAD_STATE_BEGIN_LOAD,
		BLOCK_SAVE_STATE_CHECK_LOAD
	};

#if __XENON || __PPU || __WIN32PC || RSG_DURANGO || RSG_ORBIS
	static eBlockLoadState LoadState = BLOCK_LOAD_STATE_BEGIN_LOAD;

	switch (LoadState)
	{
		case BLOCK_LOAD_STATE_BEGIN_LOAD :
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

			if (!SAVEGAME.BeginLoad(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), CSavegameFilenames::GetNameOfFileOnDisc(), pBuffer, GetBlockBufferSize()) )
			{
				Assertf(0, "CSavegameLoadBlock::DoLoadBlock - BeginLoad failed");
				savegameDebugf1("CSavegameLoadBlock::DoLoadBlock - BeginLoad failed \n");
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOAD_FAILED);
				return MEM_CARD_ERROR;
			}

			LoadState = BLOCK_SAVE_STATE_CHECK_LOAD;
			break;

		case BLOCK_SAVE_STATE_CHECK_LOAD :
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			u32 ActualBufferSizeLoaded = 0;
			bool valid; // MIGRATE_FIXME - use for something
			if (SAVEGAME.CheckLoad(CSavegameUsers::GetUser(), valid, ActualBufferSizeLoaded))
			{	//	returns true for error or success, false for busy
				LoadState = BLOCK_LOAD_STATE_BEGIN_LOAD;
				SAVEGAME.EndLoad(CSavegameUsers::GetUser());

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

#if __XENON
//	I can't do this check on the PS3 because the allocated buffer size for a PS3 load includes the size of the 
//	save icon and the PS3 system files. The size is read from the listParam of the PS3 file.
				if (!ms_bIsPhotoFile)	//	I can't do this check for photos any more. In CalculateSizeOfRequiredBuffer(), I set the size of the buffer to the maximum size required for a photo
				{
					u32 ExpectedSize = GetBlockBufferSize();
					if ( (ActualBufferSizeLoaded != 0) && (ActualBufferSizeLoaded != ExpectedSize) )
					{
						Assertf(0, "CSavegameLoadBlock::DoLoadBlock - Buffer size returned by SAVEGAME.CheckLoad %d does not match buffer size calculated by game %d", ActualBufferSizeLoaded, ExpectedSize);
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LOAD_BUFFER_SIZE_MISMATCH);
						return MEM_CARD_ERROR;
					}
				}
#endif	//	__XENON

				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED2);
					return MEM_CARD_ERROR;
				}

				return MEM_CARD_COMPLETE;
			}
			break;
	}
#else	//	#if __XENON || __PPU
	Assertf(0, "CSavegameLoadBlock::DoLoadBlock - save game only implemented for 360, PC, PS3 and Orbis so far");
#endif
	return MEM_CARD_BUSY;
}
*/



// GSWTODO - Maybe pass a flag into this function to say whether it must complete in a single frame (network load)
//	or if it should display error messages (e.g. corrupt file) and wait for the player to press OK (memory card load)
// 	MemoryCardError CSavegameLoadBlock::DeSerialize()
// 	{
// 		//	Should probably assert that LoadBlock has already finished successfully
// 
// 		if (savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_BLOCK, "CSavegameLoadBlock::DeSerialize - no block load in progress"))
// 		{
// 			if (CGenericGameStorage::LoadBlockData() == false)
// 			{
// 				//	SAVE GAME ERROR MESSAGE HERE?
// 				savegameDebugf1("CSavegameLoadBlock::DeSerialize - LoadBlockData failed for some reason\n");
// 				CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile();
// 				EndLoadBlock();
// 				return MEM_CARD_ERROR;
// 			}
// 
// 			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOAD_SUCCESSFUL);
// 			savegameDebugf1("CSavegameLoadBlock::DeSerialize - Game successfully loaded \n");
// 		}
// 
// 		CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile();
// 		EndLoadBlock();
// 
// 		return MEM_CARD_COMPLETE;
// 	}



// ******************************************** CloudFileLoadListener ********************************************

CloudFileLoadListener::CloudFileLoadListener()
{
	Initialise();
}

void CloudFileLoadListener::Initialise()
{
	m_pBufferToContainJpeg = NULL;
	m_MaxSizeOfBuffer = 0;

	m_CloudRequestId = -1;
	m_bIsRequestPending = false;
	m_bLastLoadSucceeded = false;
}


bool CloudFileLoadListener::BeginLoadingJpegFromCloud(const sRequestData &requestData, CPhotoBuffer *pBufferToContainJpeg, u32 maxSizeOfBuffer, rlCloudNamespace cloudNamespace, bool bDisplayLoadError)
{
	if (!CSavegameUsers::HasPlayerJustSignedOut() && NetworkInterface::IsCloudAvailable())
	{
		savegameDisplayf("CloudFileLoadListener::BeginLoadingJpegFromCloud - requesting: %s from the cloud.", requestData.m_PathToFileOnCloud);

		if (RequestLoadOfCloudJpeg(requestData, pBufferToContainJpeg, maxSizeOfBuffer, cloudNamespace))
		{
			savegameDisplayf("CloudFileLoadListener::BeginLoadingJpegFromCloud - RequestLoadOfCloudJpeg succeeded");
			return true;
		}
		else
		{
			savegameDisplayf("CloudFileLoadListener::BeginLoadingJpegFromCloud - RequestLoadOfCloudJpeg failed");

			if (bDisplayLoadError)
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED);
			}
			else
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
			}
		}
	}
	else
	{
		photoDisplayf("CloudFileLoadListener::BeginLoadingJpegFromCloud - player signed out or cloud not available");
		CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
	}

	return false;
}


MemoryCardError CloudFileLoadListener::UpdateLoadingJpegFromCloud(bool bDisplayLoadError)
{
	savegameDisplayf("CloudFileLoadListener::UpdateLoadingJpegFromCloud - checking status of pending JPEG cloud request");
	if (GetIsRequestPending() == false)
	{
		if (GetLastLoadSucceeded())
		{
			savegameDisplayf("CloudFileLoadListener::UpdateLoadingJpegFromCloud - JPEG cloud load succeeded");
			return MEM_CARD_COMPLETE;
		}
		else
		{
			savegameDisplayf("CloudFileLoadListener::UpdateLoadingJpegFromCloud - JPEG cloud load failed");

			if (bDisplayLoadError)
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JPEG_FAILED);
			}
			else
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
			}
			return MEM_CARD_ERROR;
		}
	}

	return MEM_CARD_BUSY;
}


bool CloudFileLoadListener::RequestLoadOfCloudJpeg(const sRequestData &requestData, CPhotoBuffer *pBufferToContainJpeg, u32 maxSizeOfBuffer, rlCloudNamespace cloudNamespace)
{
	if(savegameVerifyf(!m_bIsRequestPending, "CloudFileLoadListener::RequestLoadOfCloudJpeg - A request is already in progress"))
	{
		Initialise();
		if (NetworkInterface::IsCloudAvailable())
		{
			savegameDisplayf("CloudFileLoadListener::RequestLoadOfCloudJpeg - requesting load of %s", requestData.m_PathToFileOnCloud);

			m_pBufferToContainJpeg = pBufferToContainJpeg;
			m_MaxSizeOfBuffer = maxSizeOfBuffer;

			m_CloudRequestId = -1;

            switch(cloudNamespace)
            {
            case RL_CLOUD_NAMESPACE_MEMBERS:
                m_CloudRequestId = CloudManager::GetInstance().RequestGetMemberFile(NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(), requestData.m_PathToFileOnCloud, maxSizeOfBuffer, eRequest_CacheAdd | eRequest_CacheForceCache);	//	last param is bCache
                break; 
            case RL_CLOUD_NAMESPACE_CREWS:
                m_CloudRequestId = CloudManager::GetInstance().RequestGetCrewFile(NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(), requestData.m_PathToFileOnCloud, maxSizeOfBuffer, eRequest_CacheAdd | eRequest_CacheForceCache);	//	last param is bCache
                break; 
            case RL_CLOUD_NAMESPACE_UGC:
                m_CloudRequestId = CloudManager::GetInstance().RequestGetUgcFile(NetworkInterface::GetLocalGamerIndex(), 
                                                                                 requestData.m_nFileID == 0 ? RLUGC_CONTENT_TYPE_GTA5PHOTO : RLUGC_CONTENT_TYPE_GTA5MISSION,
                                                                                 requestData.m_PathToFileOnCloud,
                                                                                 requestData.m_nFileID,
                                                                                 requestData.m_nFileVersion,
                                                                                 requestData.m_nLanguage,
                                                                                 maxSizeOfBuffer, 
                                                                                 eRequest_CacheAdd | eRequest_CacheForceCache);
                break; 
            case RL_CLOUD_NAMESPACE_TITLES:
                m_CloudRequestId = CloudManager::GetInstance().RequestGetTitleFile(requestData.m_PathToFileOnCloud, maxSizeOfBuffer, eRequest_CacheAdd | eRequest_CacheForceCache);	//	last param is bCache
                break; 
            case RL_CLOUD_NAMESPACE_GLOBAL:
                m_CloudRequestId = CloudManager::GetInstance().RequestGetGlobalFile(requestData.m_PathToFileOnCloud, maxSizeOfBuffer, eRequest_CacheAdd | eRequest_CacheForceCache);	//	last param is bCache
                break; 
            default:
                savegameAssertf(0, "CloudFileLoadListener::RequestLoadOfCloudJpeg - Invalid cloud namespace!");
                break;
            }

			if (m_CloudRequestId == -1)
			{
				return false;
			}
			else
			{
				m_bIsRequestPending = true;
				return true;
			}
		}
		else
		{
			savegameDisplayf("CloudFileLoadListener::RequestLoadOfCloudJpeg - cloud is not available");
		}
	}

	return false;
}

void CloudFileLoadListener::OnCloudEvent(const CloudEvent* pEvent)
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

	m_bLastLoadSucceeded = pEventData->bDidSucceed;

	if (m_bLastLoadSucceeded)
	{
		if (pEventData->nDataSize <= m_MaxSizeOfBuffer)
		{
			if (m_pBufferToContainJpeg->AllocateAndFillBufferWhenLoadingCloudPhoto( (u8*)pEventData->pData, pEventData->nDataSize))
			{
				savegameDisplayf("CloudFileLoadListener::OnCloudEvent - load succeeded");
			}
			else
			{
				savegameAssertf(0, "CloudFileLoadListener::OnCloudEvent - AllocateAndFillBuffer returned false");
				m_bLastLoadSucceeded = false;
			}
		}
		else
		{
			savegameAssertf(0, "CloudFileLoadListener::OnCloudEvent - cloud buffer length %u is greater than %u", pEventData->nDataSize, m_MaxSizeOfBuffer);
			m_bLastLoadSucceeded = false;
		}		
	}
	else
	{
		savegameDisplayf("CloudFileLoadListener::OnCloudEvent - load failed");
	}
}



