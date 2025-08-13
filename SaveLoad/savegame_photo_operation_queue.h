

#ifndef SAVEGAME_PHOTO_OPERATION_QUEUE_H
#define SAVEGAME_PHOTO_OPERATION_QUEUE_H

// Rage headers
#include "atl/queue.h"

// Game headers
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_photo_unique_id.h"


enum eLowQualityPhotoSetting
{
	LOW_QUALITY_PHOTO_ONE,
	LOW_QUALITY_PHOTO_HALF,
	LOW_QUALITY_PHOTO_QUARTER,
	LOW_QUALITY_PHOTO_EIGHTH,
	NUMBER_OF_LOW_QUALITY_PHOTO_SETTINGS
};


// Forward declarations
class CSavegamePhotoAsyncBuffer;

// ****************************** CPhotoOperationData ************************************************

class CPhotoOperationData
{
public:
	enum ePhotoOperation
	{
		PHOTO_OPERATION_NONE,
		PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS,
		PHOTO_OPERATION_TAKE_CAMERA_PHOTO,
		PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO,
		PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO,
		PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW,
		PHOTO_OPERATION_SAVE_CAMERA_PHOTO,
		PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO,
		PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO,
		PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO,
		PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO,
		PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_CAMERA_PHOTO,
		PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_ROCKSTAR_EDITOR_PHOTO,
		PHOTO_OPERATION_FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO,
		PHOTO_OPERATION_LOAD_GALLERY_PHOTO,
		PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO,
		PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS,
		PHOTO_OPERATION_DELETE_GALLERY_PHOTO,
		PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE,
		PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER,
		PHOTO_OPERATION_SAVE_GIVEN_PHOTO,
		PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO,
		PHOTO_OPERATION_UPLOAD_MUGSHOT
#if __LOAD_LOCAL_PHOTOS
		, PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD
#endif	//	__LOAD_LOCAL_PHOTOS
	};

	static const s32 INVALID_MERGED_LIST_INDEX	= -1;	//	Use for operations where an index into the merged list isn't relevant. Basically anything not involving the photo gallery in the pause menu

	CPhotoOperationData();

	bool operator == (const CPhotoOperationData &OtherPhotoOperation) const;

	CPhotoOperationData& operator=(const CPhotoOperationData& other);

	void SetWidthAndHeightForGivenQuality(eLowQualityPhotoSetting qualitySetting WIN32PC_ONLY(, bool bUseConsoleResolution));

	static const s32 c_sScreenshotNameLength = 128;

	ePhotoOperation m_PhotoOperation;
#if __LOAD_LOCAL_PHOTOS
	CSavegamePhotoUniqueId	m_UniquePhotoId;
#else	//	__LOAD_LOCAL_PHOTOS
	CIndexWithinMergedPhotoList m_IndexOfPhotoWithinMergedList;
#endif	//	__LOAD_LOCAL_PHOTOS

	//	Used by SaveScreenshotToFile, SaveMissionCreatorPhoto and LoadMissionCreatorPhoto
	char m_FilenameForReplayScreenshotOrMissionCreatorPhoto[c_sScreenshotNameLength];
	sRequestData m_RequestData;

	//	Used by SaveScreenshotToFile
	u32 m_WidthOfFileScreenshotOrLowQualityCopy;
	u32 m_HeightOfFileScreenshotOrLowQualityCopy;

	//	Used by CreateSortedList
	bool m_bDisplayWarningScreens;
	bool m_bOnlyGetNumberOfPhotos;

	//  Used by SaveScreenshotToBuffer
	CSavegamePhotoAsyncBuffer* m_screenshotAsyncBuffer;

	s32 m_mugshotCharacterIndex;
};

// ****************************** CQueuedPhotoOperation ************************************************

class CQueuedPhotoOperation
{
public :
	CQueuedPhotoOperation() {}

	//	Return MEM_CARD_COMPLETE or MEM_CARD_ERROR to Pop() from queue
	MemoryCardError Update();

	CPhotoOperationData m_Data;
	bool m_bFirstCall;
};


// ****************************** CQueueOfPhotoOperations ************************************************

class CQueueOfPhotoOperations
{
public:
	//	Public functions
	CQueueOfPhotoOperations() {}

	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	void Update();

	bool Push(CPhotoOperationData &photoData);	//	assert if this type already exists in the queue?

	bool DoesOperationExistInQueue(CPhotoOperationData &photoData, bool bIgnoreTopOfQueue = false);

	bool IsEmpty() { return m_PhotoOperationQueue.IsEmpty(); }

#if __BANK
	void InitWidgets(bkBank *pParentBank);
	void UpdateWidgets();
#endif	//	__BANK

private:

	static const u32 MAX_SIZE_OF_PHOTO_QUEUE = 16;
	atQueue<CQueuedPhotoOperation, MAX_SIZE_OF_PHOTO_QUEUE> m_PhotoOperationQueue;

#if __BANK
#define MAX_LENGTH_OF_PHOTO_QUEUE_DEBUG_DESCRIPTION	(32)
	char m_PhotoQueueDebugDescriptions[MAX_SIZE_OF_PHOTO_QUEUE][MAX_LENGTH_OF_PHOTO_QUEUE_DEBUG_DESCRIPTION];
	s32 m_PhotoQueueCount;
#endif	//	__BANK
};


#endif	//	SAVEGAME_PHOTO_OPERATION_QUEUE_H

