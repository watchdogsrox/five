
#ifndef SAVEGAME_PHOTO_MANAGER_H
#define SAVEGAME_PHOTO_MANAGER_H

// Rage headers
#include "atl/array.h"

// Game headers
#include "Renderer/ScreenshotManager.h"
#include "SaveLoad/savegame_date.h"
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_photo_merged_list.h"
#include "SaveLoad/savegame_photo_mugshot_uploader.h"
#include "SaveLoad/savegame_photo_operation_queue.h"
#include "SaveLoad/savegame_photo_texture_dictionaries.h"
#include "SaveLoad/savegame_photo_unique_id.h"
#include "SaveLoad/savegame_queued_operations.h"


// Forward declarations
class CPhotoOperationData;
class CSavegamePhotoAsyncBuffer;

#define IMAGE_CRC_VERSION 1

enum eHighQualityPhotoType
{
	HIGH_QUALITY_PHOTO_CAMERA,
	HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR,
	HIGH_QUALITY_PHOTO_GALLERY,
	HIGH_QUALITY_PHOTO_MISSION_CREATOR,
	HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS,
	HIGH_QUALITY_PHOTO_FOR_RESAVING_METADATA_FOR_LOCAL_PHOTO,
	HIGH_QUALITY_PHOTO_FOR_UPLOADING_LOCAL_PHOTO_TO_CLOUD,
//	HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER,
	MAX_HIGH_QUALITY_PHOTOS
};

enum eImageSignatureResult
{
	IMAGE_SIGNATURE_MISMATCH = -1,
	IMAGE_SIGNATURE_VALID,
	IMAGE_SIGNATURE_STALE
};


#if __BANK

// ****************************** CRockstarEditorPhotoWidgets ************************************************

class CRockstarEditorPhotoWidgets
{
public:
	void Init(unsigned initMode);

	void AddWidgets(bkBank& bank);

	void UpdateWidgets();

	bool OverrideNumberOfPhotosTaken() { return (m_iFakeNumberOfLocalPhotos >= 0); }
	u32 GetNumberOfPhotosTakenOverride() { return ( (u32) m_iFakeNumberOfLocalPhotos); }

private:
	bool m_bEnumeratePhotos;
	char m_EnumeratePhotosStateText[16];

	char m_PhotosHaveBeenEnumeratedText[16];

	s32 m_iFakeNumberOfLocalPhotos;
	char m_NumberOfPhotosText[8];

	bool m_bTakePhoto;
	char m_TakePhotoStateText[16];
	char m_PhotoBufferIsAllocatedText[8];

	bool m_bSavePhoto;
	char m_SavePhotoStateText[16];

	bool m_bDeletePhotoBuffer;
};
#endif	//	__BANK


// ****************************** CPhotoMetadata ************************************************

class CPhotoMetadata
{
public:
	CSavegamePhotoMetadata &GetMetadata(eHighQualityPhotoType photoType);
	
	void SetTitle(eHighQualityPhotoType photoType, const char *pNewTitle);
	const char *GetTitle(eHighQualityPhotoType photoType) const;

	void SetDescription(eHighQualityPhotoType photoType, const char *pNewDesc);
	const char *GetDescription(eHighQualityPhotoType photoType) const;
	
private:
	u32 GetArrayIndexForPhotoType(eHighQualityPhotoType photoType) const;

	atRangeArray<CSavegamePhotoMetadata, 2> m_MetadataForPhotos;
	atRangeArray<atString, 2> m_TitlesOfPhotos;
	atRangeArray<atString, 2> m_DescriptionsOfPhotos;
};


// ****************************** CPhotoManager ************************************************

class CPhotoManager
{
	friend class CQueuedPhotoOperation;

public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void UpdateHistory(CPhotoOperationData &photoData, MemoryCardError photoProgress);

	static bool IsQueueEmpty() { return ms_QueueOfPhotoOperations.IsEmpty(); }


	static u32 GetNumberOfPhotos(bool bDeletedPhotosShouldBeCounted);

#if __LOAD_LOCAL_PHOTOS
	static bool IsListOfPhotosUpToDate(bool bLocalList);
#else	//	__LOAD_LOCAL_PHOTOS
	static bool GetNumberOfPhotosIsUpToDate();
#endif	//	__LOAD_LOCAL_PHOTOS

	static bool RequestCreateSortedListOfPhotos(bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens);
	static MemoryCardError GetCreateSortedListOfPhotosStatus(bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens);
	static void ClearCreateSortedListOfPhotosStatus();

	//	Look up in the merged list. See if it needs loaded from the cloud or HDD
	//	CScriptHud::BeginPhotoLoad(PhotoSlotIndex)
	//		Check CScriptHud::GetPhotoLoadStatus()
	//	CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto()
	//		if (CLowQualityScreenshot::HasLowQualityCopySucceeded())
	//		else if (CLowQualityScreenshot::HasLowQualityCopyFailed())
	//	Finally call CHighQualityScreenshot::FreeMemoryForHighQualityPhoto();
	static bool RequestLoadGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static MemoryCardError GetLoadGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

	static bool RequestUnloadGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static MemoryCardError GetUnloadGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

	static bool RequestUnloadAllGalleryPhotos();
	static MemoryCardError GetUnloadAllGalleryPhotosStatus();

	static CEntryInMergedPhotoListForDeletion RequestDeleteGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static MemoryCardError GetDeleteGalleryPhotoStatus(const CEntryInMergedPhotoListForDeletion &entryForDeletion);

	static MemoryCardError GetResaveMetadataForGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

#if __LOAD_LOCAL_PHOTOS
	static bool RequestUploadLocalPhotoToCloud(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static MemoryCardError GetUploadLocalPhotoToCloudStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

	static const char *GetUGCContentIdForUploadLocalPhoto() { return ms_UploadLocalPhotoToCloud.GetUGCContentId(); }
	static const char *GetLimelightAuthTokenForUploadLocalPhoto() { return ms_UploadLocalPhotoToCloud.GetLimelightAuthToken(); }
	static const char *GetLimelightFilenameForUploadLocalPhoto() { return ms_UploadLocalPhotoToCloud.GetLimelightFilename(); }

	static int GetNetStatusResultCodeForUploadLocalPhoto() { return ms_UploadLocalPhotoToCloud.GetNetStatusResultCode(); }

	static bool RequestUploadMugshot(s32 multiplayerCharacterIndex);
	static MemoryCardError GetUploadMugshotStatus(s32 multiplayerCharacterIndex);

//	Functions called by CSavegameQueuedOperation_UploadMugshot::Update()
	static bool RequestUploadOfMugshotPhoto(CPhotoBuffer *pPhotoBuffer, s32 mugshotCharacterIndex);
	static MemoryCardError GetStatusOfUploadOfMugshotPhoto();
#endif	//	__LOAD_LOCAL_PHOTOS

	static bool RequestTakeCameraPhoto();
	static MemoryCardError GetTakeCameraPhotoStatus();

	static bool RequestTakeRockstarEditorPhoto();
	static MemoryCardError GetTakeRockstarEditorPhotoStatus();
	static bool HasRockstarEditorPhotoBufferBeenAllocated() { return ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR].BufferIsAllocated(); }

	static bool RequestSaveCameraPhoto();
	static MemoryCardError GetSaveCameraPhotoStatus();

	static bool RequestSaveRockstarEditorPhoto();
	static MemoryCardError GetSaveRockstarEditorPhotoStatus();

	static void RequestSaveGivenPhoto(u8* pJPEGBuffer, u32 size, int indexOfMetadataForPhotoSave );
	static MemoryCardError GetSaveGivenPhotoStatus();

	static bool RequestTakeMissionCreatorPhoto();
	static MemoryCardError GetTakeMissionCreatorPhotoStatus();
    static CPhotoBuffer* GetMissionCreatorPhoto();

	static bool RequestSaveMissionCreatorPhoto(const char *pFilename);
	static MemoryCardError GetSaveMissionCreatorPhotoStatus(const char *pFilename);

	static bool RequestLoadMissionCreatorPhoto(const char* szContentID, int nFileID, int nFileVersion, const rlScLanguage &language);
	static MemoryCardError GetLoadMissionCreatorPhotoStatus(const char *szContentID);

	static bool RequestFreeMemoryForHighQualityCameraPhoto();
	static bool RequestFreeMemoryForHighQualityRockstarEditorPhoto();
	static bool RequestFreeMemoryForMissionCreatorPhoto();

	static bool RequestSaveScreenshotToFile(const char *pFilename, u32 screenshotWidth, u32 screenshotHeight);
	static MemoryCardError GetSaveScreenshotToFileStatus(const char *pFilename, u32 screenshotWidth, u32 screenshotHeight);

	static bool RequestSaveScreenshotToBuffer( CSavegamePhotoAsyncBuffer& asyncBuffer, u32 screenshotWidth, u32 screenshotHeight);
	static MemoryCardError GetSaveScreenshotToBufferStatus( CSavegamePhotoAsyncBuffer& asyncBuffer, u32 screenshotWidth, u32 screenshotHeight);

	static bool RequestCreateLowQualityCopyOfCameraPhoto(eLowQualityPhotoSetting qualitySetting);
	static MemoryCardError GetCreateLowQualityCopyOfCameraPhotoStatus(eLowQualityPhotoSetting qualitySetting);

	static bool RequestCreateMissionCreatorPhotoPreview(eLowQualityPhotoSetting qualitySetting);
	static MemoryCardError GetStatusOfCreateMissionCreatorPhotoPreview(eLowQualityPhotoSetting qualitySetting);

	static eLowQualityPhotoSetting GetQualityOfMissionCreatorPhotoPreview();

	static bool IsNameOfMissionCreatorTexture(const char *pTextureName, const char* pTxdName);

	// I'm not sure if these need to be added to the queue of photo operations
	static void RequestFreeMemoryForMissionCreatorPhotoPreview();
	static void RequestFreeMemoryForLowQualityPhoto();
	static void DrawLowQualityPhotoToPhone(bool bDrawToPhone, s32 PhotoRotation);

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif	//	__BANK

	static bool ReadyForScreenShot();

	static grcRenderTarget* GetScreenshotRenderTarget();

#if RSG_ORBIS || RSG_DURANGO
	static void InsertFence();
#endif // RSG_ORBIS || RSG_DURANGO

	static void Execute(grcRenderTarget* src);

	static void Execute(grcTexture* src);

	static void UpdateHighQualityScreenshots();

	static bool IsProcessingScreenshot();

	static bool AreRenderPhasesDisabled();

	static bool RenderCameraPhoto();

#if __BANK
	static void RenderCameraPhotoDebug();

	static bool GetReturnErrorForPhotoUpload() { return ms_bReturnErrorForPhotoUpload; }

	static char16 GetOverrideMemeCharacter() { return ms_OverrideMemeCharacter; }
	static u8 GetNumberOfOverrideCharactersToDisplay() { return ms_NumberOfOverrideCharactersToDisplay; }

	static bool GetDelayPhotoUpload() { return ms_bDelayPhotoUpload; }
#endif	//	__BANK

	static void SetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen(bool bAlreadySeen) { ms_bWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen = bAlreadySeen; }
	static bool GetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen() { return ms_bWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen; }

	static void GetNameOfPhotoTextureDictionary(const CUndeletedEntryInMergedPhotoList &undeletedEntry, char *pTextureName, u32 LengthOfTextureName);

//	Metadata accessors
	static CSavegamePhotoMetadata const * GetMetadataForPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static const char *GetTitleOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static bool SetTitleOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry, const char *pTitle);

	static const char *GetDescriptionOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
//	static void SetDescriptionOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry, const char *pDescription);

	static bool GetCanPhotoBeUploadedToSocialClub(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

//	static bool GetIsPhotoBookmarked(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
//	static void SetIsPhotoBookmarked(const CUndeletedEntryInMergedPhotoList &undeletedEntry, bool bBookmarked);
	
	static void GetPhotoLocation(const CUndeletedEntryInMergedPhotoList &undeletedEntry, Vector3& vLocation);

	static const char *GetSongTitle(const CUndeletedEntryInMergedPhotoList &undeletedEntry);
	static const char *GetSongArtist(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

	static bool GetCreationDate(const CUndeletedEntryInMergedPhotoList &undeletedEntry, CDate &returnDate);
	static bool GetIsMemePhoto( const CUndeletedEntryInMergedPhotoList &undeletedEntry );

	static void SetCurrentPhonePhotoIsMugshot(bool bIsMugshot);
	static bool GetIsMugshot(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

	static void SetArenaThemeAndVariationForCurrentPhoto(u32 arenaTheme, u32 arenaVariation);

	static void SetOnIslandXForCurrentPhoto(bool bOnIslandX);

#if __LOAD_LOCAL_PHOTOS
	static bool GetHasPhotoBeenUploadedToSocialClub( const CUndeletedEntryInMergedPhotoList &undeletedEntry );
	static const char *GetContentIdOfUploadedPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

	static void SetTextureDownloadSucceeded(const CSavegamePhotoUniqueId &uniquePhotoId);
	static void SetTextureDownloadFailed(const CSavegamePhotoUniqueId &uniquePhotoId);

	//	These four are called by CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update()
	static void FillMergedListOfPhotos();
	static bool RequestListOfCloudPhotos();
	static bool GetIsCloudPhotoRequestPending();
	static bool GetHasCloudPhotoRequestSucceeded();

	static void SetDisplayCloudPhotoEnumerationError(bool bDisplay) { ms_bDisplayCloudPhotoEnumerationError = bDisplay; }
	static bool ShouldDisplayCloudPhotoEnumerationError() { return ms_bDisplayCloudPhotoEnumerationError; }
#else	//	__LOAD_LOCAL_PHOTOS

	//	I'm not sure if this is called anywhere
	static bool ReadMetadataFromString(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList, const char* pJsonString, u32 LengthOfJsonString);	

	static void SetTextureDownloadSucceeded(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	static void SetTextureDownloadFailed(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);

	static bool GetHasValidMetadata(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	static const char *GetContentIdOfPhotoOnCloud(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	static int GetFileID(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	static int GetFileVersion(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	static const rlScLanguage &GetLanguage(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);

	//	These four are called by CSavegameQueuedOperation_CreateSortedListOfPhotos::Update()
	static void FillMergedListOfPhotos(bool bOnlyGetNumberOfPhotos);
	static bool RequestListOfCloudPhotos(bool bOnlyGetNumberOfPhotos);
	static bool GetIsCloudPhotoRequestPending(bool bOnlyGetNumberOfPhotos);
	static bool GetHasCloudPhotoRequestSucceeded(bool bOnlyGetNumberOfPhotos);

	static const char *GetContentId(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

#endif	//	__LOAD_LOCAL_PHOTOS

	static eImageSignatureResult ValidateCRCForImage(u64 crcValue, u8* pImage, u32 dataSize);

public:

#if __BANK
	static bool sm_bPhotoNetworkErrorNotSignedInLocally;
	static bool sm_bPhotoNetworkErrorCableDisconnected;
	static bool sm_bPhotoNetworkErrorPendingSystemUpdate;
	static bool sm_bPhotoNetworkErrorNotSignedInOnline;
	static bool sm_bPhotoNetworkErrorUserIsUnderage;
	static bool sm_bPhotoNetworkErrorNoUserContentPrivileges;
	static bool sm_bPhotoNetworkErrorNoSocialSharing;
	static bool sm_bPhotoNetworkErrorSocialClubNotAvailable;
	static bool sm_bPhotoNetworkErrorNotConnectedToSocialClub;
	static bool sm_bPhotoNetworkErrorOnlinePolicyIsOutOfDate;
#endif	//	__BANK

private:
//	Private functions

//	These private functions are called by CQueuedPhotoOperation::Update(). CQueuedPhotoOperation is a friend of this class
	static MemoryCardError CreateSortedListOfPhotos(bool bFirstCall, bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens);

	static void FillDetailsWhenTakingCameraPhoto(eHighQualityPhotoType photoType);

	static MemoryCardError TakePhoto(bool bFirstCall, eHighQualityPhotoType photoType);
	static MemoryCardError TakeCameraPhoto(bool bFirstCall);
	static MemoryCardError TakeRockstarEditorPhoto(bool bFirstCall);

	static u32 GetCRCSeed(u8 version);
	static u64 GenerateCRCFromImage(u8* pImage, u32 dataSize, u8 version);

	static MemoryCardError SavePhoto(bool bFirstCall, eHighQualityPhotoType photoType);
	static MemoryCardError SaveCameraPhoto(bool bFirstCall);
	static MemoryCardError SaveRockstarEditorPhoto(bool bFirstCall);

	static MemoryCardError SaveGivenPhoto(bool bFirstCall);

	static MemoryCardError TakeMissionCreatorPhoto(bool bFirstCall);
	static MemoryCardError SaveMissionCreatorPhoto(bool bFirstCall, const char *pFilename);

	static u32 GetScreenshotScaleForMissionCreatorPhoto();

	static MemoryCardError LoadMissionCreatorPhoto(bool bFirstCall, const sRequestData &requestData);

	static MemoryCardError FreeMemoryForHighQualityCameraPhoto(bool bFirstCall);
	static MemoryCardError FreeMemoryForHighQualityRockstarEditorPhoto(bool bFirstCall);
	static MemoryCardError FreeMemoryForMissionCreatorPhoto(bool bFirstCall);

	static MemoryCardError SaveScreenshotToFile(bool bFirstCall, const char *pFilename, u32 screenshotWidth, u32 screenshotHeight);
	static MemoryCardError SaveScreenshotToBuffer(bool bFirstCall, CSavegamePhotoAsyncBuffer* asyncBuffer, u32 screenshotWidth, u32 screenshotHeight);

	static MemoryCardError CreateLowQualityCopyOfCameraPhoto(bool bFirstCall, u32 desiredWidth, u32 desiredHeight);
	static MemoryCardError CreateMissionCreatorPhotoPreview(bool bFirstCall, u32 desiredWidth, u32 desiredHeight);

	static MemoryCardError UnloadAllGalleryPhotos(bool bFirstCall);

#if __LOAD_LOCAL_PHOTOS
	static bool GetUniquePhotoIdFromUndeletedEntry(const CUndeletedEntryInMergedPhotoList &undeletedEntry, CSavegamePhotoUniqueId &returnUniqueId);

	static MemoryCardError LoadGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId);

	static MemoryCardError UnloadGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId);

	static MemoryCardError DeleteGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId);

	static bool RequestResaveMetadataForGalleryPhoto(const CSavegamePhotoUniqueId &photoUniqueId);
	static MemoryCardError ResaveMetadataForGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId);

	static MemoryCardError UploadLocalPhotoToCloud(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId);

	static MemoryCardError UploadMugshot(bool bFirstCall, s32 mugshotCharacterIndex);
#else	//	__LOAD_LOCAL_PHOTOS
	static MemoryCardError LoadGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);

	static MemoryCardError UnloadGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);

	static MemoryCardError DeleteGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);

	static bool RequestResaveMetadataForGalleryPhoto(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	static MemoryCardError ResaveMetadataForGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
#endif	//	__LOAD_LOCAL_PHOTOS

	static bool AddPhonePhotoToTxd();
	static void RemovePhonePhotoFromTxd(bool bAssertThatTextureIsCurrentPhonePhotoTexture, bool bAssertIfTxdContainsThePhotoTexture, bool bShuttingDownTheGame);
	static bool AddMissionCreatorPhotoToTxd();
	static void RemoveMissionCreatorPhotoFromTxd(bool bAssertThatTextureIsCurrentMissionCreatorTexture, bool bAssertIfTxdContainsTheMissionCreatorTexture, bool bShuttingDownTheGame);

//	Private data
	static const u64 ms_InvalidDate;

	static CPhotoBuffer ms_ArrayOfPhotoBuffers[MAX_HIGH_QUALITY_PHOTOS];

//	#define NUMBER_OF_LOW_QUALITY_PHOTOS_TO_DISPLAY_IN_GALLERY	(12)
	#define NUMBER_OF_LOW_QUALITY_PHOTOS_TO_DISPLAY_ON_PHONE	(2)

	#define INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE (0)	//	(NUMBER_OF_LOW_QUALITY_PHOTOS_TO_DISPLAY_IN_GALLERY)
	#define INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW (1)	//	(NUMBER_OF_LOW_QUALITY_PHOTOS_TO_DISPLAY_IN_GALLERY)

	#define NUMBER_OF_LOW_QUALITY_PHOTOS	(NUMBER_OF_LOW_QUALITY_PHOTOS_TO_DISPLAY_ON_PHONE)	//	NUMBER_OF_LOW_QUALITY_PHOTOS_TO_DISPLAY_IN_GALLERY + 

	//	If I don't end up putting the 12 gallery photos back into this array then replace the array with a single CLowQualityScreenshot variable
	static CLowQualityScreenshot ms_ArrayOfLowQualityPhotos[NUMBER_OF_LOW_QUALITY_PHOTOS];

	static CPhotoMetadata ms_PhotoMetadata;

	static CTextureDictionariesForGalleryPhotos ms_TextureDictionariesForGalleryPhotos;

	//	Queue so that only one load/save/take photo can be done at a time
	static CQueueOfPhotoOperations ms_QueueOfPhotoOperations;

	static CSavegamePhotoMugshotUploader *ms_pMugshotUploader;

#if __LOAD_LOCAL_PHOTOS
	static CSavegameQueuedOperation_CreateSortedListOfLocalPhotos ms_CreateSortedListOfLocalPhotosQueuedOperation;

	static CSavegameQueuedOperation_LoadLocalPhoto ms_LocalPhotoLoad;

	static CSavegameQueuedOperation_DeleteFile ms_LocalPhotoDelete;

	static CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto ms_UpdateMetadataOfLocalAndCloudPhoto;

	static CSavegameQueuedOperation_UploadLocalPhotoToCloud ms_UploadLocalPhotoToCloud;

	static CSavegameQueuedOperation_UploadMugshot ms_UploadMugshot;
#else	//	__LOAD_LOCAL_PHOTOS
	static CMergedListOfPhotos ms_MergedListOfPhotos;

	static CSavegameQueuedOperation_CreateSortedListOfPhotos ms_CreateSortedListOfPhotosQueuedOperation;

	static CSavegameQueuedOperation_LoadUGCPhoto ms_CloudPhotoLoad;
#endif	//	__LOAD_LOCAL_PHOTOS


	static CSavegameQueuedOperation_PhotoSave ms_PhotoSave;
	static CSavegameQueuedOperation_SaveLocalPhoto ms_PhotoSaveLocal;

	static CSavegameQueuedOperation_SavePhotoForMissionCreator ms_SavePhotoForMissionCreator;
	static CSavegameQueuedOperation_LoadPhotoForMissionCreator ms_LoadPhotoForMissionCreator;	//	This is used to load the photo for a mission so that it can be resaved with a new UGC ID when the mission has been significantly modified

//	I'll need to change this over to use CUndeletedEntryInMergedPhotoList
//	once the camera's media app is displaying cloud photos. Currently it only displays local photos
	static s32 ms_IndexOfLastLoadedCameraPhoto;
	static MemoryCardError ms_StatusOfLastCameraPhotoTaken;
	static MemoryCardError ms_StatusOfLastRockstarEditorPhotoTaken;
	static MemoryCardError ms_StatusOfLastCreateLowQualityCopyOfCameraPhoto;
	static MemoryCardError ms_StatusOfLastMissionCreatorPhotoPreview;
	static MemoryCardError ms_StatusOfLastCameraPhotoSave;
	static MemoryCardError ms_StatusOfLastRockstarEditorPhotoSave;
	static MemoryCardError ms_StatusOfLastSaveScreenshotToFile;
	static MemoryCardError ms_StatusOfLastSaveScreenshotToBuffer;
	static MemoryCardError ms_StatusOfLastGivenPhotoSave;
	static MemoryCardError ms_StatusOfLastMissionCreatorPhotoTaken;
	static MemoryCardError ms_StatusOfLastMissionCreatorPhotoSave;
	static MemoryCardError ms_StatusOfLastMissionCreatorPhotoLoad;
	static MemoryCardError ms_StatusOfLastResaveMetadataForGalleryPhoto;
	static MemoryCardError ms_StatusOfLastMugshotUpload;
#if __LOAD_LOCAL_PHOTOS
	static MemoryCardError ms_StatusOfLastLocalPhotoUpload;
#endif	//	__LOAD_LOCAL_PHOTOS

	static char *ms_pMetadataForResavingToCloud;

	static u8* ms_jpegDataForGivenPhotoSave;
	static u32 ms_jpegDataSizeForGivenPhotoSave;
	static s32 ms_indexOfMetadataForGivenPhotoSave;

	static strLocalIndex ms_TxdSlotForPhonePhoto;
	static strLocalIndex ms_TxdSlotForMissionCreatorPhotoPreview;

	static bool ms_bWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen;
#if __LOAD_LOCAL_PHOTOS
	static bool ms_bDisplayCloudPhotoEnumerationError;
#endif	//	__LOAD_LOCAL_PHOTOS

#if __BANK
//	static u32 ms_UndeletedEntryInMergedListForResavingJson;
//	static bool ms_bResaveJsonForSelectedEntry;

	static bool ms_bRemoveAllPhotoGalleryTxds;

//	static bool ms_bDeleteFirstCloudPhotoInMergedList;

	static bool ms_bForceAllocToFailForThisQuality[NUMBER_OF_LOW_QUALITY_PHOTO_SETTINGS];

	static u32 ms_ScreenshotScaleForMissionCreatorPhoto;
	static u32 ms_QualityOfMissionCreatorPhotoPreview;

	static bool ms_bDelayPhotoUpload;

	static bool ms_bSaveLocalPhotos;

	static bool ms_bReturnErrorForPhotoUpload;

	static bool ms_bRequestUploadPhotoToCloud;

	static char16 ms_OverrideMemeCharacter;
	static u8 ms_NumberOfOverrideCharactersToDisplay;

	static CRockstarEditorPhotoWidgets ms_RockstarEditorPhotoWidgets;
#else	//	__BANK

#if __LOAD_LOCAL_PHOTOS
	static const bool ms_bSaveLocalPhotos = true;
#else	//	__LOAD_LOCAL_PHOTOS
	static const bool ms_bSaveLocalPhotos = false;
#endif	//	__LOAD_LOCAL_PHOTOS
#endif	//	__BANK

	static const u32 m_crc_seed_table[IMAGE_CRC_VERSION];

#if !__NO_OUTPUT
	static void	StackTraceDisplayLine(size_t addr,const char* sym,size_t offset);
#endif	//	!__NO_OUTPUT
};


#endif	//	SAVEGAME_PHOTO_MANAGER_H




