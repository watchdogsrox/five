
#include "SaveLoad/savegame_photo_operation_queue.h"


// Game headers
#include "renderer/rendertargets.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "system/system.h"


// ****************************** CPhotoOperationData ************************************************

CPhotoOperationData::CPhotoOperationData()
#if !__LOAD_LOCAL_PHOTOS
	: m_IndexOfPhotoWithinMergedList(INVALID_MERGED_LIST_INDEX, false)	// -1
#endif	//	!__LOAD_LOCAL_PHOTOS
{
	m_PhotoOperation = PHOTO_OPERATION_NONE;
	formatf(m_FilenameForReplayScreenshotOrMissionCreatorPhoto, "");
	m_WidthOfFileScreenshotOrLowQualityCopy = 0;
	m_HeightOfFileScreenshotOrLowQualityCopy = 0;

	m_bDisplayWarningScreens = false;
	m_bOnlyGetNumberOfPhotos = false;

	m_screenshotAsyncBuffer = NULL;

	m_mugshotCharacterIndex = -1;
}

bool CPhotoOperationData::operator == (const CPhotoOperationData &OtherPhotoOperation) const
{
	//	Should I bother comparing the width and height?
	if ( (m_PhotoOperation == OtherPhotoOperation.m_PhotoOperation)
#if __LOAD_LOCAL_PHOTOS
		&& (m_UniquePhotoId == OtherPhotoOperation.m_UniquePhotoId)
#else	//	__LOAD_LOCAL_PHOTOS
		&& (m_IndexOfPhotoWithinMergedList == OtherPhotoOperation.m_IndexOfPhotoWithinMergedList)
#endif	//	__LOAD_LOCAL_PHOTOS
		//		&& (m_bDisplayWarningScreens == OtherPhotoOperation.m_bDisplayWarningScreens)		//	Maybe I don't need to compare m_bDisplayWarningScreens
		&& (m_bOnlyGetNumberOfPhotos == OtherPhotoOperation.m_bOnlyGetNumberOfPhotos)
		&& (strcmp(m_FilenameForReplayScreenshotOrMissionCreatorPhoto, OtherPhotoOperation.m_FilenameForReplayScreenshotOrMissionCreatorPhoto) == 0) 
		&& (m_screenshotAsyncBuffer == OtherPhotoOperation.m_screenshotAsyncBuffer) 
		&& (m_mugshotCharacterIndex == OtherPhotoOperation.m_mugshotCharacterIndex) )
	{
		return true;
	}

	return false;
}


CPhotoOperationData& CPhotoOperationData::operator=(const CPhotoOperationData& other)
{
	m_PhotoOperation = other.m_PhotoOperation;
	m_RequestData = other.m_RequestData;
#if __LOAD_LOCAL_PHOTOS
	m_UniquePhotoId = other.m_UniquePhotoId;
#else	//	__LOAD_LOCAL_PHOTOS
	m_IndexOfPhotoWithinMergedList = other.m_IndexOfPhotoWithinMergedList;
#endif	//	__LOAD_LOCAL_PHOTOS
	safecpy(m_FilenameForReplayScreenshotOrMissionCreatorPhoto, other.m_FilenameForReplayScreenshotOrMissionCreatorPhoto);
	m_WidthOfFileScreenshotOrLowQualityCopy = other.m_WidthOfFileScreenshotOrLowQualityCopy;
	m_HeightOfFileScreenshotOrLowQualityCopy = other.m_HeightOfFileScreenshotOrLowQualityCopy;
	m_bDisplayWarningScreens = other.m_bDisplayWarningScreens;
	m_bOnlyGetNumberOfPhotos = other.m_bOnlyGetNumberOfPhotos;
	m_screenshotAsyncBuffer = other.m_screenshotAsyncBuffer;
	m_mugshotCharacterIndex = other.m_mugshotCharacterIndex;

	return *this;
}

void CPhotoOperationData::SetWidthAndHeightForGivenQuality(eLowQualityPhotoSetting qualitySetting WIN32PC_ONLY(, bool bUseConsoleResolution))
{
	u32 finalWidth;
	u32 finalHeight;

	//! The downscale values are deliberatly one out, as the low quality photos are always intended to be half that of the full image...
	VideoResManager::eDownscaleFactor scaling( VideoResManager::DOWNSCALE_EIGHTH );

	switch (qualitySetting)
	{
	case LOW_QUALITY_PHOTO_ONE :
		scaling = VideoResManager::DOWNSCALE_HALF;
		break;
	case LOW_QUALITY_PHOTO_HALF :
		scaling = VideoResManager::DOWNSCALE_QUARTER;
		break;

	case LOW_QUALITY_PHOTO_EIGHTH :
		scaling = VideoResManager::DOWNSCALE_SIXTEENTH;
		break;

	default :
		photoAssertf(0, "CPhotoOperationData::SetWidthAndHeightForGivenQuality - "
			"unexpected quality setting %d so using LOW_QUALITY_PHOTO_QUARTER", (s32) qualitySetting);

	case LOW_QUALITY_PHOTO_QUARTER :
		break;
	}

#if RSG_PC
	if (bUseConsoleResolution)
	{
		VideoResManager::GetScreenshotDimensionsOfConsoles( finalWidth, finalHeight, scaling );
	}
	else
#endif
	{
		VideoResManager::GetScreenshotDimensions( finalWidth, finalHeight, scaling );
	}
	m_WidthOfFileScreenshotOrLowQualityCopy = finalWidth;
	m_HeightOfFileScreenshotOrLowQualityCopy = finalHeight;
}


// ****************************** CQueuedPhotoOperation ************************************************

//	Return MEM_CARD_COMPLETE or MEM_CARD_ERROR to Pop() from queue
MemoryCardError CQueuedPhotoOperation::Update()
{
	MemoryCardError memCardProgress = MEM_CARD_BUSY;

	switch (m_Data.m_PhotoOperation)
	{
	case CPhotoOperationData::PHOTO_OPERATION_NONE :
		photoAssertf(0, "CQueuedPhotoOperation::Update - didn't expect this to ever get called for PHOTO_OPERATION_NONE");
		break;

	case CPhotoOperationData::PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS :
		memCardProgress = CPhotoManager::CreateSortedListOfPhotos(m_bFirstCall, m_Data.m_bOnlyGetNumberOfPhotos, m_Data.m_bDisplayWarningScreens);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_TAKE_CAMERA_PHOTO :
		memCardProgress = CPhotoManager::TakeCameraPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO :
		memCardProgress = CPhotoManager::TakeRockstarEditorPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO :
		memCardProgress = CPhotoManager::CreateLowQualityCopyOfCameraPhoto(m_bFirstCall, m_Data.m_WidthOfFileScreenshotOrLowQualityCopy, m_Data.m_HeightOfFileScreenshotOrLowQualityCopy);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW :
		memCardProgress = CPhotoManager::CreateMissionCreatorPhotoPreview(m_bFirstCall, m_Data.m_WidthOfFileScreenshotOrLowQualityCopy, m_Data.m_HeightOfFileScreenshotOrLowQualityCopy);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_SAVE_CAMERA_PHOTO :
		memCardProgress = CPhotoManager::SaveCameraPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO :
		memCardProgress = CPhotoManager::SaveRockstarEditorPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO :
		memCardProgress = CPhotoManager::TakeMissionCreatorPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO :
		memCardProgress = CPhotoManager::SaveMissionCreatorPhoto(m_bFirstCall, m_Data.m_FilenameForReplayScreenshotOrMissionCreatorPhoto);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO :
		memCardProgress = CPhotoManager::LoadMissionCreatorPhoto(m_bFirstCall, m_Data.m_RequestData);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_CAMERA_PHOTO :
		memCardProgress = CPhotoManager::FreeMemoryForHighQualityCameraPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_ROCKSTAR_EDITOR_PHOTO :
		memCardProgress = CPhotoManager::FreeMemoryForHighQualityRockstarEditorPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO :
		memCardProgress = CPhotoManager::FreeMemoryForMissionCreatorPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO :
#if __LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::LoadGalleryPhoto(m_bFirstCall, m_Data.m_UniquePhotoId);
#else	//	__LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::LoadGalleryPhoto(m_bFirstCall, m_Data.m_IndexOfPhotoWithinMergedList);
#endif	//	__LOAD_LOCAL_PHOTOS
		break;

	case CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO :
#if __LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::UnloadGalleryPhoto(m_bFirstCall, m_Data.m_UniquePhotoId);
#else	//	__LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::UnloadGalleryPhoto(m_bFirstCall, m_Data.m_IndexOfPhotoWithinMergedList);
#endif	//	__LOAD_LOCAL_PHOTOS
		break;

	case CPhotoOperationData::PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS :
		memCardProgress = CPhotoManager::UnloadAllGalleryPhotos(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO :
#if __LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::DeleteGalleryPhoto(m_bFirstCall, m_Data.m_UniquePhotoId);
#else	//	__LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::DeleteGalleryPhoto(m_bFirstCall, m_Data.m_IndexOfPhotoWithinMergedList);
#endif	//	__LOAD_LOCAL_PHOTOS
		break;

	case CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE :
		memCardProgress = CPhotoManager::SaveScreenshotToFile(m_bFirstCall, m_Data.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, m_Data.m_WidthOfFileScreenshotOrLowQualityCopy, m_Data.m_HeightOfFileScreenshotOrLowQualityCopy);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER :
		memCardProgress = CPhotoManager::SaveScreenshotToBuffer(m_bFirstCall, m_Data.m_screenshotAsyncBuffer, m_Data.m_WidthOfFileScreenshotOrLowQualityCopy, m_Data.m_HeightOfFileScreenshotOrLowQualityCopy);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_SAVE_GIVEN_PHOTO :
		memCardProgress = CPhotoManager::SaveGivenPhoto(m_bFirstCall);
		break;

	case CPhotoOperationData::PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO :
#if __LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::ResaveMetadataForGalleryPhoto(m_bFirstCall, m_Data.m_UniquePhotoId);
#else	//	__LOAD_LOCAL_PHOTOS
		memCardProgress = CPhotoManager::ResaveMetadataForGalleryPhoto(m_bFirstCall, m_Data.m_IndexOfPhotoWithinMergedList);
#endif	//	__LOAD_LOCAL_PHOTOS
		break;

	case CPhotoOperationData::PHOTO_OPERATION_UPLOAD_MUGSHOT :
		memCardProgress = CPhotoManager::UploadMugshot(m_bFirstCall, m_Data.m_mugshotCharacterIndex);
		break;

#if __LOAD_LOCAL_PHOTOS
	case CPhotoOperationData::PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD :
		memCardProgress = CPhotoManager::UploadLocalPhotoToCloud(m_bFirstCall, m_Data.m_UniquePhotoId);
		break;
#endif	//	__LOAD_LOCAL_PHOTOS
	}

	m_bFirstCall = false;

	return memCardProgress;
}


// ****************************** CQueueOfPhotoOperations ************************************************

void CQueueOfPhotoOperations::Init(unsigned initMode)
{
	if (INIT_CORE == initMode)
	{
		m_PhotoOperationQueue.Reset();
	}
}


void CQueueOfPhotoOperations::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{

}


//	bool CQueueOfPhotoOperations::Push(CPhotoOperationData::ePhotoOperation photoOperation, s32 indexWithinMergedPhotoList, const char *pFilenameForReplayScreenshot)
bool CQueueOfPhotoOperations::Push(CPhotoOperationData &photoData)
{
	photoAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CQueueOfPhotoOperations::Push - can only call this on the Update thread");

	//	pNewOperation->m_Status = MEM_CARD_ERROR;

	//	GSW_TODO - I should probably check the current contents of the queue that could affect this new operation

	if (photoVerifyf(!m_PhotoOperationQueue.IsFull(), "CQueueOfPhotoOperations::Push - queue is full"))
	{
		//	assert if this type already exists in the queue?
		//	if (photoVerifyf(!m_PhotoOperationQueue.Find(Operation), "CQueueOfPhotoOperations::Push - this operation already exists in the queue"))
		{
			CQueuedPhotoOperation newOperation;
			//			newOperation.m_Data.m_PhotoOperation = photoOperation;
			//			newOperation.m_Data.m_IndexOfPhotoWithinMergedList = indexWithinMergedPhotoList;
			//			safecpy(newOperation.m_Data.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, pFilenameForReplayScreenshot);
			newOperation.m_Data = photoData;
			newOperation.m_bFirstCall = true;
			//			newOperation.m_Status = MEM_CARD_BUSY;

			m_PhotoOperationQueue.Push(newOperation);

			return true;
		}
	}

	return false;
}

//	bool CQueueOfPhotoOperations::DoesOperationExistInQueue(CPhotoOperationData::ePhotoOperation photoOperation, s32 indexWithinMergedPhotoList)
bool CQueueOfPhotoOperations::DoesOperationExistInQueue(CPhotoOperationData &photoData, bool bIgnoreTopOfQueue)
{
	s32 NumberOfEntriesInQueue = m_PhotoOperationQueue.GetCount();


	s32 startIndex = 0;
	if (bIgnoreTopOfQueue)
	{
		startIndex = 1;
	}

	for (s32 loop = startIndex; loop < NumberOfEntriesInQueue; loop++)
	{
		//		if ( (m_PhotoOperationQueue[loop].m_Data.m_PhotoOperation == photoOperation)
		//			&& (m_PhotoOperationQueue[loop].m_Data.m_IndexOfPhotoWithinMergedList == indexWithinMergedPhotoList) )
		if (m_PhotoOperationQueue[loop].m_Data == photoData)
		{
			return true;
		}
	}

	return false;
}


void CQueueOfPhotoOperations::Update()
{
	photoAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CQueueOfPhotoOperations::Update - can only call this on the Update thread");

	if (!m_PhotoOperationQueue.IsEmpty())
	{
		CQueuedPhotoOperation &queuedOp = m_PhotoOperationQueue.Top();

		MemoryCardError operationProgress = queuedOp.Update();

		if (operationProgress != MEM_CARD_BUSY)
		{
			CPhotoManager::UpdateHistory(queuedOp.m_Data, operationProgress);

			m_PhotoOperationQueue.Drop();
		}
	}
}


#if __BANK
void CQueueOfPhotoOperations::InitWidgets(bkBank *pParentBank)
{
	char WidgetTitle[32];

	pParentBank->PushGroup("Photo Operation Queue");

	m_PhotoQueueCount = 0;
	pParentBank->AddSlider("Number of entries in queue (read-only)", &m_PhotoQueueCount, 0, MAX_SIZE_OF_PHOTO_QUEUE, 1);

	for (u32 loop = 0; loop < MAX_SIZE_OF_PHOTO_QUEUE; loop++)
	{
		formatf(WidgetTitle, "Widget Index %u", loop);
		safecpy(m_PhotoQueueDebugDescriptions[loop], "");
		pParentBank->AddText(WidgetTitle, m_PhotoQueueDebugDescriptions[loop], MAX_LENGTH_OF_PHOTO_QUEUE_DEBUG_DESCRIPTION, true);
	}

	pParentBank->PopGroup();
}

void CQueueOfPhotoOperations::UpdateWidgets()
{
	m_PhotoQueueCount = m_PhotoOperationQueue.GetCount();

	for (s32 loop = 0; loop < MAX_SIZE_OF_PHOTO_QUEUE; loop++)
	{
		safecpy(m_PhotoQueueDebugDescriptions[loop], "");
		if (loop < m_PhotoQueueCount)
		{
			if (photoVerifyf(m_PhotoOperationQueue[loop].m_Data.m_PhotoOperation != CPhotoOperationData::PHOTO_OPERATION_NONE, "CQueueOfPhotoOperations::UpdateWidgets - expected element %d (relative to tail) to have a valid entry", loop))
			{
				switch (m_PhotoOperationQueue[loop].m_Data.m_PhotoOperation)
				{
				case CPhotoOperationData::PHOTO_OPERATION_NONE :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_NONE");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_TAKE_CAMERA_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_TAKE_CAMERA_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_SAVE_CAMERA_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_SAVE_CAMERA_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_CAMERA_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_FREE_MEMORY_HIGH_QUALITY_CAMERA_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_ROCKSTAR_EDITOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "FREE_MEMORY_FOR_HIGH_QUALITY_ROCKSTAR_EDITOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_LOAD_GALLERY_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_DELETE_GALLERY_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_SAVE_GIVEN_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_SAVE_GIVEN_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO");
					break;

				case CPhotoOperationData::PHOTO_OPERATION_UPLOAD_MUGSHOT :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_UPLOAD_MUGSHOT");
					break;

#if __LOAD_LOCAL_PHOTOS
				case CPhotoOperationData::PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD :
					safecpy(m_PhotoQueueDebugDescriptions[loop], "PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD");
					break;
#endif	//	__LOAD_LOCAL_PHOTOS
				}
			}
		}
	}
}
#endif	//	__BANK

// ****************************** End of CQueueOfPhotoOperations ************************************************


