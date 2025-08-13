

#include "SaveLoad/savegame_photo_manager.h"


// Rage headers
#include "grcore/quads.h"
// #include "grcore/image.h"
// #include "grcore/texture_gnm.h"
#include "grcore/rendertarget_gnm.h"
#include "grcore/rendertarget_d3d11.h"
#include "grcore/rendertarget_durango.h"


// Game headers
//	#include "Core/game.h"
#include "game/user.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Cloud/UserContentManager.h"

#include "frontend/HudTools.h"

#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_async_buffer.h"
#include "SaveLoad/savegame_photo_cloud_deleter.h"
#include "SaveLoad/savegame_photo_local_list.h"
#include "SaveLoad/savegame_photo_local_metadata.h"
// #include "SaveLoad/savegame_photo_buffer.h"
// #include "SaveLoad/savegame_scriptdatafile.h"
//	#include "Renderer/sprite2d.h"
#include "Renderer/PostProcessFXHelper.h"
// #include "renderer/rendertargets.h"
// 
#include <time.h>


PARAM(saveLocalPhotos, "[savegame_photo_manager] save photos taken via the phone to the HDD instead of the cloud");
PARAM(loadLocalPhotos, "[savegame_photo_manager] load gallery photos from the HDD instead of the cloud");


const u64 CPhotoManager::ms_InvalidDate = 0;

const u32 CPhotoManager::m_crc_seed_table[IMAGE_CRC_VERSION] = { 0xe47ab81c };

// ****************************** CPhotoManager ************************************************

#if !__NO_OUTPUT
void CPhotoManager::StackTraceDisplayLine(size_t addr, const char *sym, size_t offset)
{
	photoDisplayf("%8" SIZETFMT "x - %s+%" SIZETFMT "x",addr,sym,offset);
}
#endif	//	!__NO_OUTPUT


CPhotoBuffer CPhotoManager::ms_ArrayOfPhotoBuffers[MAX_HIGH_QUALITY_PHOTOS];

CLowQualityScreenshot CPhotoManager::ms_ArrayOfLowQualityPhotos[NUMBER_OF_LOW_QUALITY_PHOTOS];

CPhotoMetadata CPhotoManager::ms_PhotoMetadata;

CTextureDictionariesForGalleryPhotos CPhotoManager::ms_TextureDictionariesForGalleryPhotos;

CQueueOfPhotoOperations CPhotoManager::ms_QueueOfPhotoOperations;

CSavegamePhotoMugshotUploader *CPhotoManager::ms_pMugshotUploader = NULL;

//	To fill in this, I'll need to call
//	CGenericGameStorage::QueueCreateSortedListOfFiles(bScanForSaving, true);
//	CGenericGameStorage::GetCreateSortedListOfFilesStatus();
//	and maybe CGenericGameStorage::GetCreateSortedListOfFilesStatus();

//	Should I move sm_CreateSortedListOfPhotosQueuedOperation and ms_MergedListOfPhotos out of CGenericGameStorage
//	Probably do the same for ms_PhotoMetadata

CSavegameQueuedOperation_PhotoSave CPhotoManager::ms_PhotoSave;
CSavegameQueuedOperation_SaveLocalPhoto CPhotoManager::ms_PhotoSaveLocal;


CSavegameQueuedOperation_SavePhotoForMissionCreator CPhotoManager::ms_SavePhotoForMissionCreator;
CSavegameQueuedOperation_LoadPhotoForMissionCreator CPhotoManager::ms_LoadPhotoForMissionCreator;

#if __LOAD_LOCAL_PHOTOS

CSavegameQueuedOperation_CreateSortedListOfLocalPhotos CPhotoManager::ms_CreateSortedListOfLocalPhotosQueuedOperation;

CSavegameQueuedOperation_LoadLocalPhoto CPhotoManager::ms_LocalPhotoLoad;

CSavegameQueuedOperation_DeleteFile CPhotoManager::ms_LocalPhotoDelete;

CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto CPhotoManager::ms_UpdateMetadataOfLocalAndCloudPhoto;

CSavegameQueuedOperation_UploadLocalPhotoToCloud CPhotoManager::ms_UploadLocalPhotoToCloud;

CSavegameQueuedOperation_UploadMugshot CPhotoManager::ms_UploadMugshot;

#else	//	__LOAD_LOCAL_PHOTOS

CMergedListOfPhotos CPhotoManager::ms_MergedListOfPhotos;

CSavegameQueuedOperation_CreateSortedListOfPhotos CPhotoManager::ms_CreateSortedListOfPhotosQueuedOperation;

CSavegameQueuedOperation_LoadUGCPhoto CPhotoManager::ms_CloudPhotoLoad;
#endif	//	__LOAD_LOCAL_PHOTOS


s32 CPhotoManager::ms_IndexOfLastLoadedCameraPhoto = -1;
MemoryCardError CPhotoManager::ms_StatusOfLastCameraPhotoTaken = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastRockstarEditorPhotoTaken = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastCreateLowQualityCopyOfCameraPhoto = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastMissionCreatorPhotoPreview = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastCameraPhotoSave = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastRockstarEditorPhotoSave = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastSaveScreenshotToFile = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastSaveScreenshotToBuffer = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastGivenPhotoSave = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastMissionCreatorPhotoTaken = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastMissionCreatorPhotoSave = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastMissionCreatorPhotoLoad = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastResaveMetadataForGalleryPhoto = MEM_CARD_ERROR;
MemoryCardError CPhotoManager::ms_StatusOfLastMugshotUpload = MEM_CARD_ERROR;
#if __LOAD_LOCAL_PHOTOS
MemoryCardError CPhotoManager::ms_StatusOfLastLocalPhotoUpload = MEM_CARD_ERROR;
#endif	//	__LOAD_LOCAL_PHOTOS

char *CPhotoManager::ms_pMetadataForResavingToCloud = NULL;

u8* CPhotoManager::ms_jpegDataForGivenPhotoSave = NULL;
u32 CPhotoManager::ms_jpegDataSizeForGivenPhotoSave = 0;
s32 CPhotoManager::ms_indexOfMetadataForGivenPhotoSave = -1;

strLocalIndex CPhotoManager::ms_TxdSlotForPhonePhoto = strLocalIndex(-1);
strLocalIndex CPhotoManager::ms_TxdSlotForMissionCreatorPhotoPreview = strLocalIndex(-1);

bool CPhotoManager::ms_bWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen = false;

#if __LOAD_LOCAL_PHOTOS
bool CPhotoManager::ms_bDisplayCloudPhotoEnumerationError = true;
#endif	//	__LOAD_LOCAL_PHOTOS

#if __BANK

static bool gTakeScreenShot = false;
static u32 gScreenshotScale = 1;

//	u32 CPhotoManager::ms_UndeletedEntryInMergedListForResavingJson = 0;
//	bool CPhotoManager::ms_bResaveJsonForSelectedEntry = false;

bool CPhotoManager::ms_bRemoveAllPhotoGalleryTxds = false;

//	bool CPhotoManager::ms_bDeleteFirstCloudPhotoInMergedList = false;

bool CPhotoManager::ms_bForceAllocToFailForThisQuality[NUMBER_OF_LOW_QUALITY_PHOTO_SETTINGS];

u32 CPhotoManager::ms_ScreenshotScaleForMissionCreatorPhoto = 4;
u32 CPhotoManager::ms_QualityOfMissionCreatorPhotoPreview = (u32) LOW_QUALITY_PHOTO_HALF;

bool CPhotoManager::ms_bDelayPhotoUpload = false;

#if __LOAD_LOCAL_PHOTOS
bool CPhotoManager::ms_bSaveLocalPhotos = true;
#else	//	__LOAD_LOCAL_PHOTOS
bool CPhotoManager::ms_bSaveLocalPhotos = false;
#endif	//	__LOAD_LOCAL_PHOTOS

bool CPhotoManager::ms_bReturnErrorForPhotoUpload = false;

bool CPhotoManager::ms_bRequestUploadPhotoToCloud = false;

char16 CPhotoManager::ms_OverrideMemeCharacter = 0;
u8 CPhotoManager::ms_NumberOfOverrideCharactersToDisplay = 1;

bool CPhotoManager::sm_bPhotoNetworkErrorNotSignedInLocally = false;
bool CPhotoManager::sm_bPhotoNetworkErrorCableDisconnected = false;
bool CPhotoManager::sm_bPhotoNetworkErrorPendingSystemUpdate = false;
bool CPhotoManager::sm_bPhotoNetworkErrorNotSignedInOnline = false;
bool CPhotoManager::sm_bPhotoNetworkErrorUserIsUnderage = false;
bool CPhotoManager::sm_bPhotoNetworkErrorNoUserContentPrivileges = false;
bool CPhotoManager::sm_bPhotoNetworkErrorNoSocialSharing = false;
bool CPhotoManager::sm_bPhotoNetworkErrorSocialClubNotAvailable = false;
bool CPhotoManager::sm_bPhotoNetworkErrorNotConnectedToSocialClub = false;
bool CPhotoManager::sm_bPhotoNetworkErrorOnlinePolicyIsOutOfDate = false;

CRockstarEditorPhotoWidgets CPhotoManager::ms_RockstarEditorPhotoWidgets;

#endif	//	__BANK

static const char *pNameOfPhonePhotoTexture = "PHONE_PHOTO_TEXTURE";
static const char *pNameOfMissionCreatorTexture = "MISSION_CREATOR_TEXTURE";


#if __BANK

// ****************************** CRockstarEditorPhotoWidgets ************************************************

void CRockstarEditorPhotoWidgets::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		m_bEnumeratePhotos = false;
		safecpy(m_EnumeratePhotosStateText, "");

		safecpy(m_PhotosHaveBeenEnumeratedText, "");

		m_iFakeNumberOfLocalPhotos = -1;
		safecpy(m_NumberOfPhotosText, "");

		m_bTakePhoto = false;
		safecpy(m_TakePhotoStateText, "");
		safecpy(m_PhotoBufferIsAllocatedText, "");

		m_bSavePhoto = false;
		safecpy(m_SavePhotoStateText, "");

		m_bDeletePhotoBuffer = false;
	}
}

void CRockstarEditorPhotoWidgets::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Rockstar Editor Photos");
		bank.AddToggle("Enumerate Photos", &m_bEnumeratePhotos);
		bank.AddText("Enumerate Photos State", m_EnumeratePhotosStateText, NELEM(m_EnumeratePhotosStateText), true);

		bank.AddText("Photos Enumerated?", m_PhotosHaveBeenEnumeratedText, NELEM(m_PhotosHaveBeenEnumeratedText), true);

		bank.AddSlider("Fake Number Of Photos", &m_iFakeNumberOfLocalPhotos, -1, 96, 1);
		bank.AddText("Number Of Photos", m_NumberOfPhotosText, NELEM(m_NumberOfPhotosText), true);

		bank.AddToggle("Take Photo", &m_bTakePhoto);
		bank.AddText("Take Photo State", m_TakePhotoStateText, NELEM(m_TakePhotoStateText), true);
		bank.AddText("Photo Buffer is Allocated", m_PhotoBufferIsAllocatedText, NELEM(m_PhotoBufferIsAllocatedText), true);

		bank.AddToggle("Save Photo", &m_bSavePhoto);
		bank.AddText("Save Photo State", m_SavePhotoStateText, NELEM(m_SavePhotoStateText), true);

		bank.AddToggle("Delete Photo Buffer", &m_bDeletePhotoBuffer);
	bank.PopGroup();
}

void CRockstarEditorPhotoWidgets::UpdateWidgets()
{
	char stateStrings[3][10] = { "COMPLETE", "BUSY", "ERROR" };

	if (m_bEnumeratePhotos)
	{
		m_bEnumeratePhotos = false;

		if (CPhotoManager::GetCreateSortedListOfPhotosStatus(true, true) != MEM_CARD_BUSY)
		{
			CPhotoManager::RequestCreateSortedListOfPhotos(true, true);
		}
	}

	safecpy(m_EnumeratePhotosStateText, stateStrings[CPhotoManager::GetCreateSortedListOfPhotosStatus(true, true)]);


	safecpy(m_PhotosHaveBeenEnumeratedText, CPhotoManager::IsListOfPhotosUpToDate(true)?"Yes":"No");

	formatf(m_NumberOfPhotosText, "%u", CPhotoManager::GetNumberOfPhotos(false));


	if (m_bTakePhoto)
	{
		m_bTakePhoto = false;

		CPhotoManager::RequestTakeRockstarEditorPhoto();
	}

	safecpy(m_TakePhotoStateText, stateStrings[CPhotoManager::GetTakeRockstarEditorPhotoStatus()]);

	safecpy(m_PhotoBufferIsAllocatedText, CPhotoManager::HasRockstarEditorPhotoBufferBeenAllocated()?"Yes":"No");

	if (m_bSavePhoto)
	{
		m_bSavePhoto = false;

		CPhotoManager::RequestSaveRockstarEditorPhoto();
	}

	safecpy(m_SavePhotoStateText, stateStrings[CPhotoManager::GetSaveRockstarEditorPhotoStatus()]);

	if (m_bDeletePhotoBuffer)
	{
		m_bDeletePhotoBuffer = false;

		CPhotoManager::RequestFreeMemoryForHighQualityRockstarEditorPhoto();
	}
}
#endif	//	__BANK






// ****************************** CPhotoMetadata ************************************************

u32 CPhotoMetadata::GetArrayIndexForPhotoType(eHighQualityPhotoType photoType) const
{
	switch (photoType)
	{
		case HIGH_QUALITY_PHOTO_CAMERA :
			return 0;

		case HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR :
			return 1;

		default:
			savegameAssertf(0, "CPhotoMetadata::GetArrayIndexForPhotoType - unexpected photo type. Expected HIGH_QUALITY_PHOTO_CAMERA or HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR");
	}

	return 0;
}

CSavegamePhotoMetadata &CPhotoMetadata::GetMetadata(eHighQualityPhotoType photoType)
{
	return m_MetadataForPhotos[GetArrayIndexForPhotoType(photoType)];
}

void CPhotoMetadata::SetTitle(eHighQualityPhotoType photoType, const char *pNewTitle)
{
	m_TitlesOfPhotos[GetArrayIndexForPhotoType(photoType)] = pNewTitle;
}


const char *CPhotoMetadata::GetTitle(eHighQualityPhotoType photoType) const
{
	return m_TitlesOfPhotos[GetArrayIndexForPhotoType(photoType)].c_str();
}

void CPhotoMetadata::SetDescription(eHighQualityPhotoType photoType, const char *pNewDesc)
{
	m_DescriptionsOfPhotos[GetArrayIndexForPhotoType(photoType)] = pNewDesc;
}

const char *CPhotoMetadata::GetDescription(eHighQualityPhotoType photoType) const
{
	return m_DescriptionsOfPhotos[GetArrayIndexForPhotoType(photoType)].c_str();
}



// ****************************** CPhotoManager ************************************************

void CPhotoManager::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		ms_pMetadataForResavingToCloud = NULL;

		// Create a txd for the texture
		fwTxd* pTxd = rage_new fwTxd(1);
		if (photoVerifyf(pTxd, "CPhotoManager::Init - failed to allocate txd for %s", pNameOfPhonePhotoTexture))
		{
			ms_TxdSlotForPhonePhoto = g_TxdStore.AddSlot(pNameOfPhonePhotoTexture);

			if (photoVerifyf(g_TxdStore.IsValidSlot(strLocalIndex(ms_TxdSlotForPhonePhoto)), "CPhotoManager::Init - failed to add a slot for %s texture dictionary", pNameOfPhonePhotoTexture))
			{
				g_TxdStore.Set(strLocalIndex(ms_TxdSlotForPhonePhoto), pTxd);

				strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(g_TxdStore.GetStreamingModuleId())->GetStreamingIndex(ms_TxdSlotForPhonePhoto);
				strStreamingEngine::GetInfo().SetDoNotDefrag(index);

				g_TxdStore.AddRef(strLocalIndex(ms_TxdSlotForPhonePhoto), REF_OTHER);

				pTxd->AddEntry(pNameOfPhonePhotoTexture, const_cast<rage::grcTexture*>(grcTexture::NoneBlack));
			}
		}


		// Create a txd for the texture
		pTxd = rage_new fwTxd(1);
		if (photoVerifyf(pTxd, "CPhotoManager::Init - failed to allocate txd for %s", pNameOfMissionCreatorTexture))
		{
			ms_TxdSlotForMissionCreatorPhotoPreview = g_TxdStore.AddSlot(pNameOfMissionCreatorTexture);

			if (photoVerifyf(g_TxdStore.IsValidSlot(strLocalIndex(ms_TxdSlotForMissionCreatorPhotoPreview)), "CPhotoManager::Init - failed to add a slot for %s texture dictionary", pNameOfMissionCreatorTexture))
			{
				g_TxdStore.Set(strLocalIndex(ms_TxdSlotForMissionCreatorPhotoPreview), pTxd);

				strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(g_TxdStore.GetStreamingModuleId())->GetStreamingIndex(ms_TxdSlotForMissionCreatorPhotoPreview);
				strStreamingEngine::GetInfo().SetDoNotDefrag(index);

				g_TxdStore.AddRef(strLocalIndex(ms_TxdSlotForMissionCreatorPhotoPreview), REF_OTHER);

				pTxd->AddEntry(pNameOfMissionCreatorTexture, const_cast<rage::grcTexture*>(grcTexture::NoneBlack));
			}
		}

		ms_pMugshotUploader = rage_new CSavegamePhotoMugshotUploader;
		if (savegameVerifyf(ms_pMugshotUploader, "CPhotoManager::Init - failed to create ms_pMugshotUploader"))
		{
			ms_pMugshotUploader->Init();
		}


#if __BANK
		int saveLocal = 0;
		if (PARAM_saveLocalPhotos.Get(saveLocal))
		{
			if (saveLocal > 0)
			{
				ms_bSaveLocalPhotos = true;
			}
			else
			{
				ms_bSaveLocalPhotos = false;
			}
		}
		else
		{
#if __LOAD_LOCAL_PHOTOS
			ms_bSaveLocalPhotos = true;
#else	//	__LOAD_LOCAL_PHOTOS
			ms_bSaveLocalPhotos = false;
#endif	//	__LOAD_LOCAL_PHOTOS
		}
#endif	//	__BANK
	}

#if __BANK
	for (u32 loop = 0; loop < NUMBER_OF_LOW_QUALITY_PHOTO_SETTINGS; loop++)
	{
		ms_bForceAllocToFailForThisQuality[loop] = false;
	}

	ms_RockstarEditorPhotoWidgets.Init(initMode);
#endif	//	__BANK

#if __LOAD_LOCAL_PHOTOS
#else	//	__LOAD_LOCAL_PHOTOS
	ms_MergedListOfPhotos.Init(initMode);
#endif	//	__LOAD_LOCAL_PHOTOS

	ms_TextureDictionariesForGalleryPhotos.Init(initMode);

	ms_bWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen = false;

#if __LOAD_LOCAL_PHOTOS
	ms_bDisplayCloudPhotoEnumerationError = true;
#endif	//	__LOAD_LOCAL_PHOTOS
}

void CPhotoManager::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		// Verify the TXD slot is still valid and has at least our ref
		if (Verifyf((g_TxdStore.IsValidSlot(ms_TxdSlotForPhonePhoto) != false && g_TxdStore.GetNumRefs(ms_TxdSlotForPhonePhoto) > 0), "CPhotoManager::Shutdown - entry \"%s\" (%d) has been released too many times", pNameOfPhonePhotoTexture, ms_TxdSlotForPhonePhoto.Get()))
		{
			//			g_TxdStore.RemoveRef(ms_TxdSlotForPhonePhoto, REF_OTHER);
			photoAssertf(g_TxdStore.GetNumRefs(ms_TxdSlotForPhonePhoto) == 1, "CPhotoManager::Shutdown - expected %s txd to have 1 ref but it has %d", pNameOfPhonePhotoTexture, g_TxdStore.GetNumRefs(strLocalIndex(ms_TxdSlotForPhonePhoto)));

			RemovePhonePhotoFromTxd(false, false, true);

			g_TxdStore.RemoveSlot(strLocalIndex(ms_TxdSlotForPhonePhoto));
		}

		// Verify the TXD slot is still valid and has at least our ref
		if (Verifyf((g_TxdStore.IsValidSlot(ms_TxdSlotForMissionCreatorPhotoPreview) != false && g_TxdStore.GetNumRefs(ms_TxdSlotForMissionCreatorPhotoPreview) > 0), "CPhotoManager::Shutdown - entry \"%s\" (%d) has been released too many times", pNameOfMissionCreatorTexture, ms_TxdSlotForMissionCreatorPhotoPreview.Get()))
		{
			photoAssertf(g_TxdStore.GetNumRefs(ms_TxdSlotForMissionCreatorPhotoPreview) == 1, "CPhotoManager::Shutdown - expected %s txd to have 1 ref but it has %d", pNameOfMissionCreatorTexture, g_TxdStore.GetNumRefs(ms_TxdSlotForMissionCreatorPhotoPreview));

			RemoveMissionCreatorPhotoFromTxd(false, false, true);

			g_TxdStore.RemoveSlot(ms_TxdSlotForMissionCreatorPhotoPreview);
		}

		if (ms_pMugshotUploader)
		{
			ms_pMugshotUploader->Shutdown();
			delete ms_pMugshotUploader;
			ms_pMugshotUploader = NULL;
		}
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		if (Verifyf((g_TxdStore.IsValidSlot(ms_TxdSlotForPhonePhoto) != false && g_TxdStore.GetNumRefs(ms_TxdSlotForPhonePhoto) > 0), "CPhotoManager::Shutdown - entry \"%s\" (%d) has been released too many times", pNameOfPhonePhotoTexture, ms_TxdSlotForPhonePhoto.Get()))
		{
			//			g_TxdStore.RemoveRef(ms_TxdSlotForPhonePhoto, REF_OTHER);
			photoAssertf(g_TxdStore.GetNumRefs(ms_TxdSlotForPhonePhoto) == 1, "CPhotoManager::Shutdown - expected %s txd to have 1 ref but it has %d", pNameOfPhonePhotoTexture, g_TxdStore.GetNumRefs(ms_TxdSlotForPhonePhoto));

			RemovePhonePhotoFromTxd(false, false, false);

//			g_TxdStore.RemoveSlot(ms_TxdSlotForPhonePhoto);
		}

		if (Verifyf((g_TxdStore.IsValidSlot(ms_TxdSlotForMissionCreatorPhotoPreview) != false && g_TxdStore.GetNumRefs(ms_TxdSlotForMissionCreatorPhotoPreview) > 0), "CPhotoManager::Shutdown - entry \"%s\" (%d) has been released too many times", pNameOfMissionCreatorTexture, ms_TxdSlotForMissionCreatorPhotoPreview.Get()))
		{
			photoAssertf(g_TxdStore.GetNumRefs(ms_TxdSlotForMissionCreatorPhotoPreview) == 1, "CPhotoManager::Shutdown - expected %s txd to have 1 ref but it has %d", pNameOfMissionCreatorTexture, g_TxdStore.GetNumRefs(ms_TxdSlotForMissionCreatorPhotoPreview));

			RemoveMissionCreatorPhotoFromTxd(false, false, false);

		}


		if (ms_pMetadataForResavingToCloud)
		{
			photoAssertf(0, "CPhotoManager::Shutdown - expected ms_pMetadataForResavingToCloud to have been freed by this stage");
			delete [] ms_pMetadataForResavingToCloud;
			ms_pMetadataForResavingToCloud = NULL;
		}

		if (ms_pMugshotUploader)
		{
			ms_pMugshotUploader->Shutdown();
		}
	}

#if __LOAD_LOCAL_PHOTOS
#else	//	__LOAD_LOCAL_PHOTOS
	ms_MergedListOfPhotos.Shutdown(shutdownMode);
#endif	//	__LOAD_LOCAL_PHOTOS

	u32 loop = 0;

	for (loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
	{
		ms_ArrayOfPhotoBuffers[loop].FreeBuffer();	//	Shutdown(shutdownMode);
	}

	for (loop = 0; loop < NUMBER_OF_LOW_QUALITY_PHOTOS; loop++)
	{
		ms_ArrayOfLowQualityPhotos[loop].Shutdown(shutdownMode);
	}

	ms_TextureDictionariesForGalleryPhotos.Shutdown(shutdownMode);
}

void CPhotoManager::Update()
{
	PF_AUTO_PUSH_TIMEBAR("CPhotoManager Update");
#if __BANK
	if (gTakeScreenShot)
	{
		RequestSaveScreenshotToFile(__PS3 ? "dbgSShotPS3" : "dbgSShot360", VideoResManager::GetUIWidth()/gScreenshotScale, VideoResManager::GetUIHeight()/gScreenshotScale);	//	, NULL, 0);
		gTakeScreenShot = false;
	}

	if (ms_bRemoveAllPhotoGalleryTxds)
	{
		ms_bRemoveAllPhotoGalleryTxds = false;
		RequestUnloadAllGalleryPhotos();
	}

	if (ms_bRequestUploadPhotoToCloud)
	{
		ms_bRequestUploadPhotoToCloud = false;
		CUndeletedEntryInMergedPhotoList undeletedEntry(0, false);
		const bool result = RequestUploadLocalPhotoToCloud(undeletedEntry);
		photoDebugf1("RequestUploadLocalPhotoToCloud returned %s", result ? "true" : "false");
	}

// 	if (ms_bDeleteFirstCloudPhotoInMergedList)
// 	{
// 		CUndeletedEntryInMergedPhotoList indexOfFirstCloudPhoto = ms_MergedListOfPhotos.GetIndexOfFirstPhoto(false);
// 		if (indexOfFirstCloudPhoto.IsValid())
// 		{
// 			RequestDeleteGalleryPhoto(indexOfFirstCloudPhoto);
// 		}
// 
// 		ms_bDeleteFirstCloudPhotoInMergedList = false;
// 	}

// 	if (ms_bResaveJsonForSelectedEntry)
// 	{
// 		CUndeletedEntryInMergedPhotoList undeletedEntry(ms_UndeletedEntryInMergedListForResavingJson, false);
// 
// 		CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);
// 
// 		if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::Update - failed to convert undeleted entry %d to a valid index when using debug code to test resaving of JSON data", undeletedEntry.GetIntegerValue()))
// 		{
// 			if (!RequestResaveMetadataForGalleryPhoto(mergedIndex))
// 			{
// 				photoAssertf(0, "CPhotoManager::Update - RequestResaveMetadataForGalleryPhoto failed for merged index %d when using debug code to test resaving of JSON data", mergedIndex.GetIntegerValue());
// 			}
// 		}
// 
// 		ms_bResaveJsonForSelectedEntry = false;
// 	}

	ms_QueueOfPhotoOperations.UpdateWidgets();

	ms_RockstarEditorPhotoWidgets.UpdateWidgets();

	CSavegamePhotoMetadata::UpdateDebug();
#endif	//	__BANK

	if (ms_pMugshotUploader)
	{
		ms_pMugshotUploader->Update();
	}

#if __LOAD_LOCAL_PHOTOS
	CSavegamePhotoCloudList::UpdateCloudDirectoryRequest();
	CSavegamePhotoCloudDeleter::Process();
#else	//	__LOAD_LOCAL_PHOTOS
//	if (ms_MergedListOfPhotos.GetIsCloudDirectoryRequestPending())	//	I don't know what to pass for the bOnlyGetNumberOfPhotos param. I might not need to call this anyway
	{
		ms_MergedListOfPhotos.UpdateCloudDirectoryRequest();
	}
#endif	//	__LOAD_LOCAL_PHOTOS

	ms_QueueOfPhotoOperations.Update();
}


void CPhotoManager::UpdateHistory(CPhotoOperationData &photoData, MemoryCardError photoProgress)
{
	switch (photoData.m_PhotoOperation)
	{
		case CPhotoOperationData::PHOTO_OPERATION_NONE :
			photoAssertf(0, "CPhotoManager::UpdateHistory - didn't expect to reach here for PHOTO_OPERATION_NONE");
			break;

		case CPhotoOperationData::PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS :
			//	Not sure if I need to do anything here
			break;

		case CPhotoOperationData::PHOTO_OPERATION_TAKE_CAMERA_PHOTO :
			ms_StatusOfLastCameraPhotoTaken = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO :
			ms_StatusOfLastRockstarEditorPhotoTaken = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO :
			ms_StatusOfLastCreateLowQualityCopyOfCameraPhoto = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW :
			ms_StatusOfLastMissionCreatorPhotoPreview = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_SAVE_CAMERA_PHOTO :
			ms_StatusOfLastCameraPhotoSave = photoProgress;

			if (photoProgress == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CPhotoManager::UpdateHistory - a photo has just been saved so clear m_bNumberOfPhotosIsUpToDate and m_bFullListOfMetadataIsUpToDate");
#if !__LOAD_LOCAL_PHOTOS
				ms_MergedListOfPhotos.SetResultsAreOutOfDate();	//	 MergedListIsUpToDate(false);
#endif	//	!__LOAD_LOCAL_PHOTOS
			}
			break;

		case CPhotoOperationData::PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO :
			ms_StatusOfLastRockstarEditorPhotoSave = photoProgress;

			if (photoProgress == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CPhotoManager::UpdateHistory - a photo has just been saved so clear m_bNumberOfPhotosIsUpToDate and m_bFullListOfMetadataIsUpToDate");
#if !__LOAD_LOCAL_PHOTOS
				ms_MergedListOfPhotos.SetResultsAreOutOfDate();	//	 MergedListIsUpToDate(false);
#endif	//	!__LOAD_LOCAL_PHOTOS
			}
			break;

		case CPhotoOperationData::PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO :
			ms_StatusOfLastMissionCreatorPhotoTaken = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO :
			ms_StatusOfLastMissionCreatorPhotoSave = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO :
			ms_StatusOfLastMissionCreatorPhotoLoad = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_CAMERA_PHOTO :
			//	Don't do anything
			//	If I need to check if the operation has succeeded I can maybe just check
			//		ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA].GetSaveBuffer()
			break;

		case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_ROCKSTAR_EDITOR_PHOTO :
			//	Don't do anything
			//	If I need to check if the operation has succeeded I can maybe just check
			//		ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR].GetSaveBuffer()
			break;

		case CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO :
			//	Don't do anything
			//	If I need to check if the operation has succeeded I can maybe just check
			//		ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR].GetSaveBuffer()
			break;

		case CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO :
//	Maybe I don't need to do anything here - GetLoadGalleryPhotoStatus() just calls IsThisPhotoLoadedIntoAnyTextureDictionary()
//			savegameAssertf(0, "CPhotoManager::UpdateHistory - PHOTO_OPERATION_LOAD_GALLERY_PHOTO not supported yet");
			break;

		case CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO :
//	Maybe I don't need to do anything here - GetUnloadGalleryPhotoStatus() calls IsThisPhotoLoadedIntoAnyTextureDictionary()
//			savegameAssertf(0, "CPhotoManager::UpdateHistory - PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO not supported yet");
			break;

		case CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO :
			{
				switch (photoProgress)
				{
				case MEM_CARD_COMPLETE :
#if !__LOAD_LOCAL_PHOTOS
					ms_MergedListOfPhotos.SetPhotoHasBeenDeleted(photoData.m_IndexOfPhotoWithinMergedList);
#endif	//	!__LOAD_LOCAL_PHOTOS
					break;
				case MEM_CARD_BUSY :
					photoAssertf(0, "CPhotoManager::UpdateHistory - PHOTO_OPERATION_DELETE_GALLERY_PHOTO - didn't expect to reach here for MEM_CARD_BUSY");
					break;
				case MEM_CARD_ERROR :
					break;
				}
			}
			break;

		case CPhotoOperationData::PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS :
//	Maybe I don't need to do anything here - GetUnloadAllGalleryPhotosStatus() calls AreAnyPhotosLoadedIntoTextureDictionaries()
//			savegameAssertf(0, "CPhotoManager::UpdateHistory - PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS not supported yet");
			break;

		case CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE :
			ms_StatusOfLastSaveScreenshotToFile = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER:
			ms_StatusOfLastSaveScreenshotToBuffer = photoProgress;

			if( photoProgress == MEM_CARD_COMPLETE && photoData.m_screenshotAsyncBuffer )
			{
				photoData.m_screenshotAsyncBuffer->SetPopulated( true );
			}
			break;

		case CPhotoOperationData::PHOTO_OPERATION_SAVE_GIVEN_PHOTO:
			ms_StatusOfLastGivenPhotoSave = photoProgress;

			if (photoProgress == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CPhotoManager::UpdateHistory - a photo has just been saved so clear m_bNumberOfPhotosIsUpToDate and m_bFullListOfMetadataIsUpToDate");
#if !__LOAD_LOCAL_PHOTOS
				ms_MergedListOfPhotos.SetResultsAreOutOfDate();	//	 MergedListIsUpToDate(false);
#endif	//	!__LOAD_LOCAL_PHOTOS
			}
			break;

		case CPhotoOperationData::PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO :
			//	Not sure what to do here yet
			//	Maybe I'll need to clear m_bNumberOfPhotosIsUpToDate and m_bFullListOfMetadataIsUpToDate here but for now I'll see if it works without doing that
			ms_StatusOfLastResaveMetadataForGalleryPhoto = photoProgress;
			break;

		case CPhotoOperationData::PHOTO_OPERATION_UPLOAD_MUGSHOT :
			ms_StatusOfLastMugshotUpload = photoProgress;
			break;

#if __LOAD_LOCAL_PHOTOS
		case CPhotoOperationData::PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD :
			ms_StatusOfLastLocalPhotoUpload = photoProgress;
			break;
#endif	//	__LOAD_LOCAL_PHOTOS
	}
}



// ***** CreateSortedListOfPhotos


bool CPhotoManager::RequestCreateSortedListOfPhotos(bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens)
{
#if !__NO_OUTPUT
	photoDisplayf("Callstack for CPhotoManager::RequestCreateSortedListOfPhotos is ");
	sysStack::PrintStackTrace(StackTraceDisplayLine);
#endif	//	!__NO_OUTPUT

	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS;
	photoData.m_bDisplayWarningScreens = bDisplayWarningScreens;
	photoData.m_bOnlyGetNumberOfPhotos = bOnlyGetNumberOfPhotos;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetCreateSortedListOfPhotosStatus(bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_CREATE_SORTED_LIST_OF_PHOTOS;
	photoData.m_bDisplayWarningScreens = bDisplayWarningScreens;
	photoData.m_bOnlyGetNumberOfPhotos = bOnlyGetNumberOfPhotos;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

#if __LOAD_LOCAL_PHOTOS
	return ms_CreateSortedListOfLocalPhotosQueuedOperation.GetStatus();
#else	//	__LOAD_LOCAL_PHOTOS
	return ms_CreateSortedListOfPhotosQueuedOperation.GetStatus();
#endif	//	__LOAD_LOCAL_PHOTOS
}

//	Is this still needed by the camera script?
void CPhotoManager::ClearCreateSortedListOfPhotosStatus()
{
#if __LOAD_LOCAL_PHOTOS
	ms_CreateSortedListOfLocalPhotosQueuedOperation.ClearStatus();
#else	//	__LOAD_LOCAL_PHOTOS
	ms_CreateSortedListOfPhotosQueuedOperation.ClearStatus();
#endif	//	__LOAD_LOCAL_PHOTOS
}

// ***** End of CreateSortedListOfPhotos



bool CPhotoManager::RequestUnloadAllGalleryPhotos()
{
#if !__NO_OUTPUT
	photoDisplayf("Callstack for CPhotoManager::RequestUnloadAllGalleryPhotos is ");
	sysStack::PrintStackTrace(StackTraceDisplayLine);
#endif	//	!__NO_OUTPUT

	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetUnloadAllGalleryPhotosStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UNLOAD_ALL_GALLERY_PHOTOS;

	//	What do I return if the photo operation isn't found in the queue?
	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	if (ms_TextureDictionariesForGalleryPhotos.AreAnyPhotosLoadedIntoTextureDictionaries())
	{
		return MEM_CARD_ERROR;
	}

	return MEM_CARD_COMPLETE;
}

MemoryCardError CPhotoManager::UnloadAllGalleryPhotos(bool UNUSED_PARAM(bFirstCall))
{
	CSavegamePhotoLocalMetadata::Init();
	return ms_TextureDictionariesForGalleryPhotos.UnloadAllPhotos();
}




// **************** TakeCameraPhoto ****************

void CPhotoManager::FillDetailsWhenTakingCameraPhoto(eHighQualityPhotoType photoType)
{
	ms_PhotoMetadata.GetMetadata(photoType).FillContents();
	if (photoType == HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR)
	{
		ms_PhotoMetadata.GetMetadata(photoType).SetTakenInRockstarEditor(true);
	}

	char newTitle[256];

	safecpy(newTitle, "", NELEM(newTitle));
	if (strlen(CUserDisplay::StreetName.GetName()) > 0)
	{
		safecat(newTitle, CUserDisplay::StreetName.GetName(), NELEM(newTitle));
	}
	else if (strlen(CUserDisplay::AreaName.GetName()) > 0)
	{
		safecat(newTitle, CUserDisplay::AreaName.GetName(), NELEM(newTitle));
	}

	ms_PhotoMetadata.SetTitle(photoType, newTitle);
	ms_PhotoMetadata.SetDescription(photoType, "");
}

bool CPhotoManager::RequestTakeCameraPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_TAKE_CAMERA_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetTakeCameraPhotoStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_TAKE_CAMERA_PHOTO;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastCameraPhotoTaken;
}


MemoryCardError CPhotoManager::TakePhoto(bool bFirstCall, eHighQualityPhotoType photoType)
{
	enum eTakePhotoProgress
	{
		TAKE_PHOTO_BEGIN,
		TAKE_PHOTO_UPDATE
	};

	static eTakePhotoProgress s_TakePhotoProgress = TAKE_PHOTO_BEGIN;

	if (bFirstCall)
	{
		s_TakePhotoProgress = TAKE_PHOTO_BEGIN;

		switch (photoType)
		{
		case HIGH_QUALITY_PHOTO_CAMERA :
			ms_StatusOfLastCameraPhotoTaken = MEM_CARD_BUSY;
			break;

		case HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR :
			ms_StatusOfLastRockstarEditorPhotoTaken = MEM_CARD_BUSY;
			break;

		case HIGH_QUALITY_PHOTO_MISSION_CREATOR :
			ms_StatusOfLastMissionCreatorPhotoTaken = MEM_CARD_BUSY;
			break;

		default :
			photoAssertf(0, "CPhotoManager::TakePhoto - unexpected photo type. It should be HIGH_QUALITY_PHOTO_CAMERA, HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR or HIGH_QUALITY_PHOTO_MISSION_CREATOR");
			break;
		}
	}

	u32 screenshotScale = 2;
	bool bFillPhotoDetails = true;

	switch (photoType)
	{
		case HIGH_QUALITY_PHOTO_CAMERA :
		case HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR :
			screenshotScale = 2;
			bFillPhotoDetails = true;
			break;

		case HIGH_QUALITY_PHOTO_MISSION_CREATOR :
			screenshotScale = GetScreenshotScaleForMissionCreatorPhoto();
			bFillPhotoDetails = false;
			break;

		default :
			photoAssertf(0, "CPhotoManager::TakePhoto - unexpected photo type. It should be HIGH_QUALITY_PHOTO_CAMERA, HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR or HIGH_QUALITY_PHOTO_MISSION_CREATOR");
			break;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_TakePhotoProgress)
	{
		case TAKE_PHOTO_BEGIN :
		{
			//	Allocate buffer and fill with JPEG for saving
			//	Don't save it at this point
			if (CHighQualityScreenshot::BeginTakeHighQualityPhoto(&ms_ArrayOfPhotoBuffers[photoType], screenshotScale WIN32PC_ONLY(, true)) == false)
			{
				photoDisplayf("CPhotoManager::TakePhoto - BeginTakeHighQualityPhoto failed");
				returnValue = MEM_CARD_ERROR;
			}
			else
			{
				if (bFillPhotoDetails)
				{
					FillDetailsWhenTakingCameraPhoto(photoType);
				}

				photoDisplayf("CPhotoManager::TakePhoto - BeginTakeHighQualityPhoto succeeded");
				s_TakePhotoProgress = TAKE_PHOTO_UPDATE;
			}
		}
		break;

		case TAKE_PHOTO_UPDATE :
		{
			if (CHighQualityScreenshot::HasScreenShotFinished())		//	ms_ArrayOfPhotoBuffers[photoType].
			{
				if (CHighQualityScreenshot::HasScreenShotFailed())		//	ms_ArrayOfPhotoBuffers[photoType].
				{
					photoDisplayf("CPhotoManager::TakePhoto - failed");
					returnValue = MEM_CARD_ERROR;
				}
				else
				{
					photoDisplayf("CPhotoManager::TakePhoto - succeeded");

					if (bFillPhotoDetails)
					{	//	Write the metadata to the JSON section of the complete photo buffer. Also write the title and description into the complete buffer.
//						ms_ArrayOfPhotoBuffers[photoType].SetMetadataString(ms_PhotoMetadata.GetMetadata(photoType));		//	I'll call this later once I'm sure that the player is signed in
						ms_ArrayOfPhotoBuffers[photoType].SetTitleString(ms_PhotoMetadata.GetTitle(photoType));
						ms_ArrayOfPhotoBuffers[photoType].SetDescriptionString(ms_PhotoMetadata.GetDescription(photoType));
					}

					returnValue = MEM_CARD_COMPLETE;
				}
			}
		}
		break;
	}

	return returnValue;
}

MemoryCardError CPhotoManager::TakeCameraPhoto(bool bFirstCall)
{
	return TakePhoto(bFirstCall, HIGH_QUALITY_PHOTO_CAMERA);
}

// **************** TakeRockstarEditorPhoto ****************

bool CPhotoManager::RequestTakeRockstarEditorPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetTakeRockstarEditorPhotoStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_TAKE_ROCKSTAR_EDITOR_PHOTO;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastRockstarEditorPhotoTaken;
}

MemoryCardError CPhotoManager::TakeRockstarEditorPhoto(bool bFirstCall)
{
	return TakePhoto(bFirstCall, HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR);
}


u32 CPhotoManager::GetCRCSeed(u8 version)
{
	if (photoVerifyf(version > 0 && version <= IMAGE_CRC_VERSION, "invalid version %d passed to CPhotoManager::GetCRCSeed()", version))
	{
		return m_crc_seed_table[version - 1];
	}
	else
	{
		return 0;
	}
}

//Image validation
u64 CPhotoManager::GenerateCRCFromImage(u8* pImage, u32 dataSize, u8 version)
{
	const u32 START_CRC_VALUE = GetCRCSeed(version);

	u32 dataHash = atDataHash((char*)pImage, dataSize, START_CRC_VALUE);

	//pack version number into upper 8 bits and datahash into lower 32 bits
	u64 crcValue = static_cast<u64>(version) << 56;
	crcValue += static_cast<u64>(dataHash);

	return crcValue;
}

eImageSignatureResult CPhotoManager::ValidateCRCForImage(u64 crcFromData, u8* pImage, u32 dataSize)
{
// For now, we won't validate the photos as we load them.
// At some point in the future, validation can be enabled via the Tunable.
	const bool bValidatePhotos = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("VALIDATE_SNAPMATIC_PHOTOS", 0xE9B2D3A0), false);
	if (!bValidatePhotos)
	{
		return eImageSignatureResult::IMAGE_SIGNATURE_VALID;
	}

// The RDR code doesn't do this check.
// We need to add it for GTA5 because we're adding the CRC after GTA5 has been released.
// We still want players to be able to view their existing local photos. They won't be able to upload them though.
	if (crcFromData == 0)
	{
		return eImageSignatureResult::IMAGE_SIGNATURE_STALE;
	}

	u32 dataHash = crcFromData & 0xFFFFFFFF;
	u8 version = crcFromData >> 56;

	u64 verifyCRC_sameVersion = GenerateCRCFromImage(pImage, dataSize, version);
	u32 verifyDataHash_sameVersion = verifyCRC_sameVersion & 0xFFFFFFFF;

	u64 verifyCRC_latest = GenerateCRCFromImage(pImage, dataSize, IMAGE_CRC_VERSION);
	u32 verifyDataHash_latest = verifyCRC_latest & 0xFFFFFFFF;

	if (dataHash == verifyDataHash_latest && version == IMAGE_CRC_VERSION)
		return eImageSignatureResult::IMAGE_SIGNATURE_VALID;
	else if (dataHash == verifyDataHash_sameVersion)
		return eImageSignatureResult::IMAGE_SIGNATURE_STALE;
	else
		return eImageSignatureResult::IMAGE_SIGNATURE_MISMATCH;
}


// **************** SaveCameraPhoto ****************

bool CPhotoManager::RequestSaveCameraPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_CAMERA_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetSaveCameraPhotoStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_CAMERA_PHOTO;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastCameraPhotoSave;
}

MemoryCardError CPhotoManager::SavePhoto(bool bFirstCall, eHighQualityPhotoType photoType)
{
	enum eSavePhotoProgress
	{
		SAVE_PHOTO_BEGIN,
		SAVE_PHOTO_UPDATE
	};

	static eSavePhotoProgress s_SavePhotoProgress = SAVE_PHOTO_BEGIN;
	static bool s_bSavePhotoLocally = false;

	if (bFirstCall)
	{
		s_SavePhotoProgress = SAVE_PHOTO_BEGIN;

		if (ms_bSaveLocalPhotos)
		{
			s_bSavePhotoLocally = true;
		}
		else
		{
			s_bSavePhotoLocally = false;
		}

		switch (photoType)
		{
		case HIGH_QUALITY_PHOTO_CAMERA :
			ms_StatusOfLastCameraPhotoSave = MEM_CARD_BUSY;
			break;

		case HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR :
			ms_StatusOfLastRockstarEditorPhotoSave = MEM_CARD_BUSY;
			break;

		default :
			photoAssertf(0, "CPhotoManager::SavePhoto - unexpected photo type. It should be HIGH_QUALITY_PHOTO_CAMERA or HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR");
			break;
		}
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_SavePhotoProgress)
	{
	case SAVE_PHOTO_BEGIN :
		{
			if ( (photoVerifyf(ms_PhotoSaveLocal.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::SavePhoto - can't queue a photo save while a local photo save is already in progress"))
				&& (photoVerifyf(ms_PhotoSave.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::SavePhoto - can't queue a photo save while a cloud photo save is already in progress")) )
			{
		//		if (photoVerifyf( (PhotoSlotIndex >= 0) && (PhotoSlotIndex < NUM_SAVED_PHOTO_SLOTS), "CScriptHud::BeginPhotoSave - photo slot %d is out of range 0 to %d", PhotoSlotIndex, NUM_SAVED_PHOTO_SLOTS))
				{
					bool bSuccessfullyQueued = false;

					if (s_bSavePhotoLocally)
					{
						u64 imageCRC = GenerateCRCFromImage(ms_ArrayOfPhotoBuffers[photoType].GetJpegBuffer(), ms_ArrayOfPhotoBuffers[photoType].GetExactSizeOfJpegData(), IMAGE_CRC_VERSION);
						ms_PhotoMetadata.GetMetadata(photoType).SetImageSignature(imageCRC);

						ms_PhotoMetadata.GetMetadata(photoType).GenerateNewUniqueId();
						ms_ArrayOfPhotoBuffers[photoType].SetMetadataString(ms_PhotoMetadata.GetMetadata(photoType));
						ms_PhotoSaveLocal.Init(ms_PhotoMetadata.GetMetadata(photoType).GetUniqueId(), &ms_ArrayOfPhotoBuffers[photoType]);
						bSuccessfullyQueued = CGenericGameStorage::PushOnToSavegameQueue(&ms_PhotoSaveLocal);
					}
					else
					{
						ms_PhotoSave.Init(ms_ArrayOfPhotoBuffers[photoType].GetJpegBuffer(), ms_ArrayOfPhotoBuffers[photoType].GetExactSizeOfJpegData(), ms_PhotoMetadata.GetMetadata(photoType), ms_PhotoMetadata.GetTitle(photoType), ms_PhotoMetadata.GetDescription(photoType));	//	PhotoSlotIndex + INDEX_OF_FIRST_SAVED_PHOTO_SLOT, bSaveToCloud ); // Use true for cloud operation.
						bSuccessfullyQueued = CGenericGameStorage::PushOnToSavegameQueue(&ms_PhotoSave);
					}

					if (bSuccessfullyQueued)
					{
						photoDisplayf("CPhotoManager::SavePhoto - photo save operation has been queued");
						s_SavePhotoProgress = SAVE_PHOTO_UPDATE;
					}
					else
					{
						photoAssertf(0, "CPhotoManager::SavePhoto - failed to queue the photo save operation");
						returnValue = MEM_CARD_ERROR;
					}
				}
			}
		}
		break;

	case SAVE_PHOTO_UPDATE :
		if (s_bSavePhotoLocally)
		{
			returnValue = ms_PhotoSaveLocal.GetStatus();
		}
		else
		{
			returnValue = ms_PhotoSave.GetStatus();
		}

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::SavePhoto - succeeded");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::SavePhoto - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}

MemoryCardError CPhotoManager::SaveCameraPhoto(bool bFirstCall)
{
	return SavePhoto(bFirstCall, HIGH_QUALITY_PHOTO_CAMERA);
}

// **************** SaveRockstarEditorPhoto ****************

bool CPhotoManager::RequestSaveRockstarEditorPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetSaveRockstarEditorPhotoStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_ROCKSTAR_EDITOR_PHOTO;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastRockstarEditorPhotoSave;
}

MemoryCardError CPhotoManager::SaveRockstarEditorPhoto(bool bFirstCall)
{
	return SavePhoto(bFirstCall, HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR);
}

// **************** Save a given photo directly ****************

void CPhotoManager::RequestSaveGivenPhoto( u8* pJPEGBuffer, u32 size, int indexOfMetadataForPhotoSave )
{
	if( pJPEGBuffer && size && indexOfMetadataForPhotoSave >= 0 )
	{
		ms_jpegDataForGivenPhotoSave = pJPEGBuffer;
		ms_jpegDataSizeForGivenPhotoSave = size;
		ms_indexOfMetadataForGivenPhotoSave = indexOfMetadataForPhotoSave;

		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_GIVEN_PHOTO;
		ms_QueueOfPhotoOperations.Push(photoData);
	}
	else
	{
		photoDisplayf("CPhotoManager::RequestGivenSavePhoto - failed");
	}
}

MemoryCardError CPhotoManager::GetSaveGivenPhotoStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_GIVEN_PHOTO;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastGivenPhotoSave;
}

// **************** End of Save a given photo directly ****************


// **************** TakeMissionCreator ****************

bool CPhotoManager::RequestTakeMissionCreatorPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetTakeMissionCreatorPhotoStatus()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_TAKE_MISSION_CREATOR_PHOTO;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastMissionCreatorPhotoTaken;
}

MemoryCardError CPhotoManager::TakeMissionCreatorPhoto(bool bFirstCall)
{
	return TakePhoto(bFirstCall, HIGH_QUALITY_PHOTO_MISSION_CREATOR);
}

CPhotoBuffer* CPhotoManager::GetMissionCreatorPhoto()
{
    return &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR];
}

u32 CPhotoManager::GetScreenshotScaleForMissionCreatorPhoto()
{
#if __BANK
	return ms_ScreenshotScaleForMissionCreatorPhoto;
#else	//	__BANK
	return 4;
#endif	//	__BANK
}


// **************** SaveMissionCreator ****************

bool CPhotoManager::RequestSaveMissionCreatorPhoto(const char *pFilename)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO;
	safecpy(photoData.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, pFilename);

	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetSaveMissionCreatorPhotoStatus(const char *pFilename)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_MISSION_CREATOR_PHOTO;
	safecpy(photoData.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, pFilename);

	if (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastMissionCreatorPhotoSave;
}

MemoryCardError CPhotoManager::SaveMissionCreatorPhoto(bool bFirstCall, const char *pFilename)
{
	enum eSaveMissionCreatorPhotoProgress
	{
		SAVE_MISSION_CREATOR_PHOTO_BEGIN,
		SAVE_MISSION_CREATOR_PHOTO_UPDATE
	};

	static eSaveMissionCreatorPhotoProgress s_SavePhotoProgress = SAVE_MISSION_CREATOR_PHOTO_BEGIN;

	if (bFirstCall)
	{
		s_SavePhotoProgress = SAVE_MISSION_CREATOR_PHOTO_BEGIN;
		ms_StatusOfLastMissionCreatorPhotoSave = MEM_CARD_BUSY;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_SavePhotoProgress)
	{
	case SAVE_MISSION_CREATOR_PHOTO_BEGIN :
		{
			if (photoVerifyf(ms_SavePhotoForMissionCreator.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::SaveMissionCreatorPhoto - can't queue a photo save while one is already in progress"))
			{
				ms_SavePhotoForMissionCreator.Init(pFilename, &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR]);
				if (CGenericGameStorage::PushOnToSavegameQueue(&ms_SavePhotoForMissionCreator))
				{
					photoDisplayf("CPhotoManager::SaveMissionCreatorPhoto - photo save operation has been queued");
					s_SavePhotoProgress = SAVE_MISSION_CREATOR_PHOTO_UPDATE;
				}
				else
				{
					photoAssertf(0, "CPhotoManager::SaveMissionCreatorPhoto - failed to queue the photo save operation");
					returnValue = MEM_CARD_ERROR;
				}
			}
		}
		break;

	case SAVE_MISSION_CREATOR_PHOTO_UPDATE :
		returnValue = ms_SavePhotoForMissionCreator.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::SaveMissionCreatorPhoto - succeeded");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::SaveMissionCreatorPhoto - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}


// **************** LoadMissionCreatorPhoto ****************

bool CPhotoManager::RequestLoadMissionCreatorPhoto(const char* szContentID, int nFileID, int nFileVersion, const rlScLanguage &language)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO;
    safecpy(photoData.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, szContentID);

    // build request
	safecpy(photoData.m_RequestData.m_PathToFileOnCloud, szContentID);
    photoData.m_RequestData.m_nFileID = nFileID;
    photoData.m_RequestData.m_nFileVersion = nFileVersion;
    photoData.m_RequestData.m_nLanguage = language;

	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetLoadMissionCreatorPhotoStatus(const char *pFilename)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_LOAD_MISSION_CREATOR_PHOTO;
	safecpy(photoData.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, pFilename);

	if (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastMissionCreatorPhotoLoad;
}


MemoryCardError CPhotoManager::LoadMissionCreatorPhoto(bool bFirstCall, const sRequestData &requestData)
{
	enum eLoadMissionCreatorPhotoProgress
	{
		LOAD_MISSION_CREATOR_PHOTO_BEGIN,
		LOAD_MISSION_CREATOR_PHOTO_UPDATE
	};

	static eLoadMissionCreatorPhotoProgress s_LoadPhotoProgress = LOAD_MISSION_CREATOR_PHOTO_BEGIN;

	if (bFirstCall)
	{
		s_LoadPhotoProgress = LOAD_MISSION_CREATOR_PHOTO_BEGIN;
		ms_StatusOfLastMissionCreatorPhotoLoad = MEM_CARD_BUSY;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_LoadPhotoProgress)
	{
	case LOAD_MISSION_CREATOR_PHOTO_BEGIN :
		{
			if (photoVerifyf(ms_LoadPhotoForMissionCreator.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::LoadMissionCreatorPhoto - can't queue a photo load while one is already in progress"))
			{
				ms_LoadPhotoForMissionCreator.Init(requestData, &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR], CPhotoBuffer::GetDefaultSizeOfJpegBuffer());
				if (CGenericGameStorage::PushOnToSavegameQueue(&ms_LoadPhotoForMissionCreator))
				{
					photoDisplayf("CPhotoManager::LoadMissionCreatorPhoto - photo load operation has been queued");
					s_LoadPhotoProgress = LOAD_MISSION_CREATOR_PHOTO_UPDATE;
				}
				else
				{
					photoAssertf(0, "CPhotoManager::LoadMissionCreatorPhoto - failed to queue the photo load operation");
					returnValue = MEM_CARD_ERROR;
				}
			}
		}
		break;

	case LOAD_MISSION_CREATOR_PHOTO_UPDATE :
		returnValue = ms_LoadPhotoForMissionCreator.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::LoadMissionCreatorPhoto - succeeded");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::LoadMissionCreatorPhoto - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}



// **************** FreeMemoryForHighQualityCameraPhoto ****************

bool CPhotoManager::RequestFreeMemoryForHighQualityCameraPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_CAMERA_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::FreeMemoryForHighQualityCameraPhoto(bool UNUSED_PARAM(bFirstCall))
{
	CHighQualityScreenshot::ResetStateForHighQualityPhoto(&ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA]);
	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA].FreeBuffer();
	return MEM_CARD_COMPLETE;
}


// **************** FreeMemoryForHighQualityRockstarEditorPhoto ****************

bool CPhotoManager::RequestFreeMemoryForHighQualityRockstarEditorPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_ROCKSTAR_EDITOR_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::FreeMemoryForHighQualityRockstarEditorPhoto(bool UNUSED_PARAM(bFirstCall))
{
	CHighQualityScreenshot::ResetStateForHighQualityPhoto(&ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR]);
	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_ROCKSTAR_EDITOR].FreeBuffer();
	return MEM_CARD_COMPLETE;
}


// **************** FreeMemoryForMissionCreatorPhoto ****************

bool CPhotoManager::RequestFreeMemoryForMissionCreatorPhoto()
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO;
	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::FreeMemoryForMissionCreatorPhoto(bool UNUSED_PARAM(bFirstCall))
{
	CHighQualityScreenshot::ResetStateForHighQualityPhoto(&ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR]);
	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR].FreeBuffer();
	return MEM_CARD_COMPLETE;
}


// **************** SaveScreenshotToFile ****************

bool CPhotoManager::RequestSaveScreenshotToFile(const char *pFilename, u32 screenshotWidth, u32 screenshotHeight)
{
#if !__NO_OUTPUT
	photoDisplayf("Callstack for CPhotoManager::RequestSaveScreenshotToFile is ");
	sysStack::PrintStackTrace(StackTraceDisplayLine);
#endif	//	!__NO_OUTPUT

	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE;
	safecpy(photoData.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, pFilename);

	VideoResManager::ConstrainScreenshotDimensions( screenshotWidth, screenshotHeight );
	photoData.m_WidthOfFileScreenshotOrLowQualityCopy = screenshotWidth;
	photoData.m_HeightOfFileScreenshotOrLowQualityCopy = screenshotHeight;

	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetSaveScreenshotToFileStatus(const char *pFilename, u32 screenshotWidth, u32 screenshotHeight)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE;
	safecpy(photoData.m_FilenameForReplayScreenshotOrMissionCreatorPhoto, pFilename);

	VideoResManager::ConstrainScreenshotDimensions( screenshotWidth, screenshotHeight );
	photoData.m_WidthOfFileScreenshotOrLowQualityCopy = screenshotWidth;
	photoData.m_HeightOfFileScreenshotOrLowQualityCopy = screenshotHeight;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastSaveScreenshotToFile;
}

MemoryCardError CPhotoManager::SaveScreenshotToFile(bool bFirstCall, const char *pFilename, u32 screenshotWidth, u32 screenshotHeight)
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	if (bFirstCall)
	{
		ms_StatusOfLastSaveScreenshotToFile = MEM_CARD_BUSY;

		CHighQualityScreenshot::TakeScreenShot(pFilename, screenshotWidth, screenshotHeight);	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		
		if (CHighQualityScreenshot::HasScreenShotFailed())	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		{
			photoDisplayf("CPhotoManager::SaveScreenshotToFile - TakeScreenShot failed (1)");

			returnValue = MEM_CARD_ERROR;
			CHighQualityScreenshot::ResetState();	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		}
	}
	else
	{
		if (CHighQualityScreenshot::HasScreenShotFinished())	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		{
			if (CHighQualityScreenshot::HasScreenShotFailed())	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
			{
				photoDisplayf("CPhotoManager::SaveScreenshotToFile - TakeScreenShot failed (2)");
				returnValue = MEM_CARD_ERROR;
			}
			else
			{
				photoDisplayf("CPhotoManager::SaveScreenshotToFile - TakeScreenShot succeeded");
				returnValue = MEM_CARD_COMPLETE;
			}
			CHighQualityScreenshot::ResetState();	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		}
	}

	return returnValue;
}

// **************** SaveScreenshotToBuffer ****************

bool CPhotoManager::RequestSaveScreenshotToBuffer( CSavegamePhotoAsyncBuffer& asyncBuffer, u32 screenshotWidth, u32 screenshotHeight )
{
#if !__NO_OUTPUT
	photoDisplayf("Callstack for CPhotoManager::RequestSaveScreenshotToBuffer is ");
	sysStack::PrintStackTrace(StackTraceDisplayLine);
#endif	//	!__NO_OUTPUT

	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER;
	photoData.m_screenshotAsyncBuffer = &asyncBuffer;

	VideoResManager::ConstrainScreenshotDimensions( screenshotWidth, screenshotHeight );
	photoData.m_WidthOfFileScreenshotOrLowQualityCopy = screenshotWidth;
	photoData.m_HeightOfFileScreenshotOrLowQualityCopy = screenshotHeight;

	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetSaveScreenshotToBufferStatus( CSavegamePhotoAsyncBuffer& asyncBuffer, u32 screenshotWidth, u32 screenshotHeight )
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_SAVE_SCREENSHOT_TO_BUFFER;
	photoData.m_screenshotAsyncBuffer = &asyncBuffer;

	VideoResManager::ConstrainScreenshotDimensions( screenshotWidth, screenshotHeight );
	photoData.m_WidthOfFileScreenshotOrLowQualityCopy = screenshotWidth;
	photoData.m_HeightOfFileScreenshotOrLowQualityCopy = screenshotHeight;

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastSaveScreenshotToBuffer;
}

MemoryCardError CPhotoManager::SaveScreenshotToBuffer( bool bFirstCall, CSavegamePhotoAsyncBuffer* asyncBuffer, u32 screenshotWidth, u32 screenshotHeight )
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	if (bFirstCall)
	{
		ms_StatusOfLastSaveScreenshotToFile = MEM_CARD_BUSY;

		CHighQualityScreenshot::TakeScreenShot( asyncBuffer->GetRawBuffer(), asyncBuffer->GetBufferSize(), screenshotWidth, screenshotHeight);	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].

		if (CHighQualityScreenshot::HasScreenShotFailed())	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		{
			photoDisplayf("CPhotoManager::SaveScreenshotToBuffer - TakeScreenShot failed (1)");

			returnValue = MEM_CARD_ERROR;
			CHighQualityScreenshot::ResetState();	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		}
	}
	else
	{
		if (CHighQualityScreenshot::HasScreenShotFinished())	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		{
			if (CHighQualityScreenshot::HasScreenShotFailed())	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
			{
				photoDisplayf("CPhotoManager::SaveScreenshotToBuffer - TakeScreenShot failed (2)");
				returnValue = MEM_CARD_ERROR;
			}
			else
			{
				photoDisplayf("CPhotoManager::SaveScreenshotToBuffer - TakeScreenShot succeeded");
				returnValue = MEM_CARD_COMPLETE;
			}
			CHighQualityScreenshot::ResetState();	//	ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_TO_FILE_OR_BUFFER].
		}
	}

	return returnValue;
}

// **************** CreateLowQualityCopyOfCameraPhoto ****************


bool CPhotoManager::RequestCreateLowQualityCopyOfCameraPhoto(eLowQualityPhotoSetting qualitySetting)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO;
	photoData.SetWidthAndHeightForGivenQuality(qualitySetting WIN32PC_ONLY(, true));

#if __BANK
	ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].SetForceMemoryAllocationToFail(false);

	s32 nQualitySetting = (s32) qualitySetting;
	if ( (nQualitySetting >= 0) && (nQualitySetting < NUMBER_OF_LOW_QUALITY_PHOTO_SETTINGS) )
	{
		if (ms_bForceAllocToFailForThisQuality[nQualitySetting])
		{
			ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].SetForceMemoryAllocationToFail(true);
		}
	}
#endif	//	__BANK

	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetCreateLowQualityCopyOfCameraPhotoStatus(eLowQualityPhotoSetting qualitySetting)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_CREATE_LOW_QUALITY_COPY_OF_CAMERA_PHOTO;
	photoData.SetWidthAndHeightForGivenQuality(qualitySetting WIN32PC_ONLY(, true));

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastCreateLowQualityCopyOfCameraPhoto;
}

MemoryCardError CPhotoManager::CreateLowQualityCopyOfCameraPhoto(bool bFirstCall, u32 desiredWidth, u32 desiredHeight)
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	if (bFirstCall)
	{
		ms_StatusOfLastCreateLowQualityCopyOfCameraPhoto = MEM_CARD_BUSY;

		bool bStartSuccessfully = ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto(&ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA], desiredWidth, desiredHeight);

		if (bStartSuccessfully)
		{
			photoDisplayf("CPhotoManager::CreateLowQualityCopyOfCameraPhoto - BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto succeeded (1)");
		}
		else
		{
			photoDisplayf("CPhotoManager::CreateLowQualityCopyOfCameraPhoto - BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto failed (1)");

			returnValue = MEM_CARD_ERROR;
		}
	}
	else
	{
		if (ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].HasLowQualityCopySucceeded())
		{
			photoDisplayf("CPhotoManager::CreateLowQualityCopyOfCameraPhoto - HasLowQualityCopySucceeded()");

//			returnValue = MEM_CARD_COMPLETE;

			if (AddPhonePhotoToTxd())
			{
				photoDisplayf("CPhotoManager::CreateLowQualityCopyOfCameraPhoto - AddPhonePhotoToTxd() succeeded");
				returnValue = MEM_CARD_COMPLETE;
			}
			else
			{
				photoDisplayf("CPhotoManager::CreateLowQualityCopyOfCameraPhoto - AddPhonePhotoToTxd() failed");
				returnValue = MEM_CARD_ERROR;
			}
		}
		else if (ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].HasLowQualityCopyFailed())
		{
			photoDisplayf("CPhotoManager::CreateLowQualityCopyOfCameraPhoto - HasLowQualityCopyFailed()");
			returnValue = MEM_CARD_ERROR;
		}
	}

	return returnValue;
}


// **************** RequestCreateMissionCreatorPhotoPreview ****************


bool CPhotoManager::RequestCreateMissionCreatorPhotoPreview(eLowQualityPhotoSetting qualitySetting)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW;
	photoData.SetWidthAndHeightForGivenQuality(qualitySetting WIN32PC_ONLY(, true));

#if __BANK
	ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].SetForceMemoryAllocationToFail(false);

	s32 nQualitySetting = (s32) qualitySetting;
	if ( (nQualitySetting >= 0) && (nQualitySetting < NUMBER_OF_LOW_QUALITY_PHOTO_SETTINGS) )
	{
		if (ms_bForceAllocToFailForThisQuality[nQualitySetting])
		{
			ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].SetForceMemoryAllocationToFail(true);
		}
	}
#endif	//	__BANK

	return ms_QueueOfPhotoOperations.Push(photoData);
}

MemoryCardError CPhotoManager::GetStatusOfCreateMissionCreatorPhotoPreview(eLowQualityPhotoSetting qualitySetting)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_CREATE_MISSION_CREATOR_PHOTO_PREVIEW;
	photoData.SetWidthAndHeightForGivenQuality(qualitySetting WIN32PC_ONLY(, true));

	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastMissionCreatorPhotoPreview;
}

MemoryCardError CPhotoManager::CreateMissionCreatorPhotoPreview(bool bFirstCall, u32 desiredWidth, u32 desiredHeight)
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	if (bFirstCall)
	{
		ms_StatusOfLastMissionCreatorPhotoPreview = MEM_CARD_BUSY;

		bool bStartSuccessfully = ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto(&ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_MISSION_CREATOR], desiredWidth, desiredHeight);

		if (bStartSuccessfully)
		{
			photoDisplayf("CPhotoManager::CreateMissionCreatorPhotoPreview - BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto succeeded (1)");
		}
		else
		{
			photoDisplayf("CPhotoManager::CreateMissionCreatorPhotoPreview - BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto failed (1)");

			returnValue = MEM_CARD_ERROR;
		}
	}
	else
	{
		if (ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].HasLowQualityCopySucceeded())
		{
			photoDisplayf("CPhotoManager::CreateMissionCreatorPhotoPreview - HasLowQualityCopySucceeded()");

			if (AddMissionCreatorPhotoToTxd())
			{
				photoDisplayf("CPhotoManager::CreateMissionCreatorPhotoPreview - AddMissionCreatorPhotoToTxd() succeeded");
				returnValue = MEM_CARD_COMPLETE;
			}
			else
			{
				photoDisplayf("CPhotoManager::CreateMissionCreatorPhotoPreview - AddMissionCreatorPhotoToTxd() failed");
				returnValue = MEM_CARD_ERROR;
			}
		}
		else if (ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].HasLowQualityCopyFailed())
		{
			photoDisplayf("CPhotoManager::CreateMissionCreatorPhotoPreview - HasLowQualityCopyFailed()");
			returnValue = MEM_CARD_ERROR;
		}
	}

	return returnValue;
}

eLowQualityPhotoSetting CPhotoManager::GetQualityOfMissionCreatorPhotoPreview()
{
#if __BANK
	return static_cast<eLowQualityPhotoSetting>(ms_QualityOfMissionCreatorPhotoPreview);
#else	//	__BANK
	return LOW_QUALITY_PHOTO_HALF;
#endif	//	__BANK
}



// I'm not sure if these need to be added to the queue of photo operations

void CPhotoManager::RequestFreeMemoryForLowQualityPhoto()
{
	RemovePhonePhotoFromTxd(true, false, false);
	ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].RequestFreeMemoryForLowQualityPhoto();
}

void CPhotoManager::RequestFreeMemoryForMissionCreatorPhotoPreview()
{
	RemoveMissionCreatorPhotoFromTxd(true, false, false);
	ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].RequestFreeMemoryForLowQualityPhoto();
}

void CPhotoManager::DrawLowQualityPhotoToPhone(bool bDrawToPhone, s32 PhotoRotation)
{
	ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].SetRenderToPhone(bDrawToPhone, static_cast<CLowQualityScreenshot::ePhotoRotation>(PhotoRotation) );
}

bool CPhotoManager::AddPhonePhotoToTxd()
{
	fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(strLocalIndex(ms_TxdSlotForPhonePhoto));
	if (photoVerifyf(pTxd, "CPhotoManager::AddPhonePhotoToTxd - failed to get pointer to txd with name %s", pNameOfPhonePhotoTexture))
	{
		RemovePhonePhotoFromTxd(false, true, false);

		photoAssertf(pTxd->GetCount() == 1, "CPhotoManager::AddPhonePhotoToTxd - %s Txd should only have 1 entry, but it has %d", pNameOfPhonePhotoTexture, pTxd->GetCount());

//		return pTxd->AddEntry(pNameOfPhonePhotoTexture, ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].GetGrcTexture());
		pTxd->SetEntryUnsafe(0, ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].GetGrcTexture());
		return true;
	}

	return false;
}

void CPhotoManager::RemovePhonePhotoFromTxd(bool ASSERT_ONLY(bAssertThatTextureIsCurrentPhonePhotoTexture), bool ASSERT_ONLY(bAssertIfTxdContainsThePhotoTexture), bool bShuttingDownTheGame)
{
	fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(strLocalIndex(ms_TxdSlotForPhonePhoto));
	if (photoVerifyf(pTxd, "CPhotoManager::RemovePhonePhotoFromTxd - failed to get pointer to txd with name %s", pNameOfPhonePhotoTexture))
	{
		grcTexture *pTextureInsideTxd = pTxd->Lookup(pNameOfPhonePhotoTexture);
//		if (photoVerifyf(pTextureInsideTxd, "CPhotoManager::RemovePhonePhotoFromTxd - failed to find a texture called %s inside the txd", pNameOfPhonePhotoTexture))
		if (pTextureInsideTxd)
		{
#if __ASSERT
			if (bAssertThatTextureIsCurrentPhonePhotoTexture)
			{
				photoAssertf( (pTextureInsideTxd == ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].GetGrcTexture()) || (pTextureInsideTxd == grcTexture::NoneBlack), "CPhotoManager::RemovePhonePhotoFromTxd - texture inside txd has address %p but texture for low quality photo has address %p", pTextureInsideTxd, ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].GetGrcTexture());
			}
#endif	//	__ASSERT

#if __ASSERT
			if (bAssertIfTxdContainsThePhotoTexture)
			{
				photoAssertf(pTextureInsideTxd == grcTexture::NoneBlack, "CPhotoManager::RemovePhonePhotoFromTxd - didn't expect a texture called %s to already exist in the txd. Removing it before adding the new texture", pNameOfPhonePhotoTexture);
			}
#endif	//	__ASSERT

//			pTxd->DeleteEntry(pNameOfPhonePhotoTexture);
			photoAssertf(pTxd->GetCount() == 1, "CPhotoManager::RemovePhonePhotoFromTxd - %s Txd should only have 1 entry, but it has %d", pNameOfPhonePhotoTexture, pTxd->GetCount());

			if (bShuttingDownTheGame)
			{
				pTxd->SetEntryUnsafe(0, NULL);
			}
			else
			{
				pTxd->SetEntryUnsafe(0, const_cast<rage::grcTexture*>(grcTexture::NoneBlack));
			}

			photoAssertf(pTextureInsideTxd->GetRefCount() == 1, "CPhotoManager::RemovePhonePhotoFromTxd - expected the ref count for the %s texture to be 1 before calling DecRef() but it's %d", pNameOfPhonePhotoTexture, pTextureInsideTxd->GetRefCount());
//			pTextureInsideTxd->DecRef();
		}
	}
}

bool CPhotoManager::AddMissionCreatorPhotoToTxd()
{
	fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(strLocalIndex(ms_TxdSlotForMissionCreatorPhotoPreview));
	if (photoVerifyf(pTxd, "CPhotoManager::AddMissionCreatorPhotoToTxd - failed to get pointer to txd with name %s", pNameOfMissionCreatorTexture))
	{
		RemoveMissionCreatorPhotoFromTxd(false, true, false);

		photoAssertf(pTxd->GetCount() == 1, "CPhotoManager::AddMissionCreatorPhotoToTxd - %s Txd should only have 1 entry, but it has %d", pNameOfMissionCreatorTexture, pTxd->GetCount());

		pTxd->SetEntryUnsafe(0, ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].GetGrcTexture());
		return true;
	}

	return false;
}

void CPhotoManager::RemoveMissionCreatorPhotoFromTxd(bool ASSERT_ONLY(bAssertThatTextureIsCurrentPhonePhotoTexture), bool ASSERT_ONLY(bAssertIfTxdContainsThePhotoTexture), bool bShuttingDownTheGame)
{
	fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(strLocalIndex(ms_TxdSlotForMissionCreatorPhotoPreview));
	if (photoVerifyf(pTxd, "CPhotoManager::RemoveMissionCreatorPhotoFromTxd - failed to get pointer to txd with name %s", pNameOfMissionCreatorTexture))
	{
		grcTexture *pTextureInsideTxd = pTxd->Lookup(pNameOfMissionCreatorTexture);

		if (pTextureInsideTxd)
		{
#if __ASSERT
			if (bAssertThatTextureIsCurrentPhonePhotoTexture)
			{
				photoAssertf( (pTextureInsideTxd == ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].GetGrcTexture()) || (pTextureInsideTxd == grcTexture::NoneBlack), "CPhotoManager::RemoveMissionCreatorPhotoFromTxd - texture inside txd has address %p but texture for low quality photo has address %p", pTextureInsideTxd, ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_MISSION_CREATOR_PREVIEW].GetGrcTexture());
			}
#endif	//	__ASSERT

#if __ASSERT
			if (bAssertIfTxdContainsThePhotoTexture)
			{
				photoAssertf(pTextureInsideTxd == grcTexture::NoneBlack, "CPhotoManager::RemoveMissionCreatorPhotoFromTxd - didn't expect a texture called %s to already exist in the txd. Removing it before adding the new texture", pNameOfMissionCreatorTexture);
			}
#endif	//	__ASSERT

			photoAssertf(pTxd->GetCount() == 1, "CPhotoManager::RemoveMissionCreatorPhotoFromTxd - %s Txd should only have 1 entry, but it has %d", pNameOfMissionCreatorTexture, pTxd->GetCount());

			if (bShuttingDownTheGame)
			{
				pTxd->SetEntryUnsafe(0, NULL);
			}
			else
			{
				pTxd->SetEntryUnsafe(0, const_cast<rage::grcTexture*>(grcTexture::NoneBlack));
			}

			photoAssertf(pTextureInsideTxd->GetRefCount() == 1, "CPhotoManager::RemoveMissionCreatorPhotoFromTxd - expected the ref count for the %s texture to be 1 before calling DecRef() but it's %d", pNameOfMissionCreatorTexture, pTextureInsideTxd->GetRefCount());
		}
	}
}

bool CPhotoManager::IsNameOfMissionCreatorTexture(const char *pTextureName, const char* ASSERT_ONLY(pTxdName))
{
	if (stricmp(pTextureName, pNameOfMissionCreatorTexture) == 0)
	{
		photoAssertf(stricmp(pTxdName, pNameOfMissionCreatorTexture) == 0, "CPhotoManager::IsNameOfMissionCreatorTexture - pTextureName is MISSION_CREATOR_TEXTURE so expected pTxdName to be MISSION_CREATOR_TEXTURE too");
		return true;
	}
#if __ASSERT
	else
	{
		photoAssertf(stricmp(pTxdName, pNameOfMissionCreatorTexture) != 0, "CPhotoManager::IsNameOfMissionCreatorTexture - pTextureName isn't MISSION_CREATOR_TEXTURE so didn't expect pTxdName to be MISSION_CREATOR_TEXTURE");
	}
#endif	//	__ASSERT

	return false;
}



#if __BANK
void CPhotoManager::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Delay Photo Upload", &ms_bDelayPhotoUpload);

	bank.AddToggle("Save Photos Locally", &ms_bSaveLocalPhotos);
	bank.AddToggle("Return Error For Photo Upload", &ms_bReturnErrorForPhotoUpload);
	
	bank.AddToggle("Request Upload Photo To Cloud", &ms_bRequestUploadPhotoToCloud);

	bank.AddSlider("Override Meme Character", &ms_OverrideMemeCharacter, 0, 9000, 1);
	bank.AddSlider("Number Of Override Characters To Display", &ms_NumberOfOverrideCharactersToDisplay, 1, 15, 1);

	ms_RockstarEditorPhotoWidgets.AddWidgets(bank);

	bank.PushGroup("Photo Network Errors");
		bank.AddToggle("User is not signed in locally", &sm_bPhotoNetworkErrorNotSignedInLocally);
		bank.AddToggle("Cable disconnected", &sm_bPhotoNetworkErrorCableDisconnected);
		bank.AddToggle("Pending System Update", &sm_bPhotoNetworkErrorPendingSystemUpdate);
		bank.AddToggle("User is not signed in online", &sm_bPhotoNetworkErrorNotSignedInOnline);
		bank.AddToggle("User is underage", &sm_bPhotoNetworkErrorUserIsUnderage);
		bank.AddToggle("No User Content privileges", &sm_bPhotoNetworkErrorNoUserContentPrivileges);
		bank.AddToggle("No social sharing", &sm_bPhotoNetworkErrorNoSocialSharing);
		bank.AddToggle("Social Club not available", &sm_bPhotoNetworkErrorSocialClubNotAvailable);
		bank.AddToggle("Not connected to Social Club", &sm_bPhotoNetworkErrorNotConnectedToSocialClub);
		bank.AddToggle("Online Policy is out of date", &sm_bPhotoNetworkErrorOnlinePolicyIsOutOfDate);
	bank.PopGroup();

	bank.AddToggle("Take Screenshot", &gTakeScreenShot);
	bank.AddSlider("Screenshot Scale (1/value)", &gScreenshotScale, 1, 8, 1);

//	bank.AddSlider("Resave Json Data for", &ms_UndeletedEntryInMergedListForResavingJson, 0, 11, 1);
//	bank.AddToggle("Resave Json Data", &ms_bResaveJsonForSelectedEntry);

	bank.AddToggle("Remove all Photo Gallery Txds", &ms_bRemoveAllPhotoGalleryTxds);

//	bank.AddToggle("Delete First Cloud Photo In Merged List", &ms_bDeleteFirstCloudPhotoInMergedList);

	bank.AddToggle("Force failure of allocation of Full Quality photos", &ms_bForceAllocToFailForThisQuality[LOW_QUALITY_PHOTO_ONE]);
	bank.AddToggle("Force failure of allocation of Half Quality photos", &ms_bForceAllocToFailForThisQuality[LOW_QUALITY_PHOTO_HALF]);
	bank.AddToggle("Force failure of allocation of Quarter Quality photos", &ms_bForceAllocToFailForThisQuality[LOW_QUALITY_PHOTO_QUARTER]);
	bank.AddToggle("Force failure of allocation of Eighth Quality photos", &ms_bForceAllocToFailForThisQuality[LOW_QUALITY_PHOTO_EIGHTH]);

	bank.AddSlider("Screenshot Scale For Mission Creator Photo", &ms_ScreenshotScaleForMissionCreatorPhoto, 1, 8, 1);
	bank.AddSlider("Quality Of Mission Creator Photo Preview", &ms_QualityOfMissionCreatorPhotoPreview, 0, 3, 1);

	ms_QueueOfPhotoOperations.InitWidgets(&bank);

	bank.AddToggle("Display Bounding Rectangles for Peds", &CSavegamePhotoMetadata::sm_bDisplayBoundingRectanglesForPeds);

	bank.AddToggle("sm_bSaveLocation", &CSavegamePhotoMetadata::sm_bSaveLocation);
	bank.AddToggle("sm_bSaveArea", &CSavegamePhotoMetadata::sm_bSaveArea);
	bank.AddToggle("sm_bSaveStreet", &CSavegamePhotoMetadata::sm_bSaveStreet);
	bank.AddToggle("sm_bSaveMissionName", &CSavegamePhotoMetadata::sm_bSaveMissionName);

 	bank.AddToggle("sm_bSaveSong", &CSavegamePhotoMetadata::sm_bSaveSong);
	bank.AddToggle("sm_bSaveRadioStation", &CSavegamePhotoMetadata::sm_bSaveRadioStation);
	bank.AddToggle("sm_bSaveMpSessionID", &CSavegamePhotoMetadata::sm_bSaveMpSessionID);
	bank.AddToggle("sm_bSaveClanID", &CSavegamePhotoMetadata::sm_bSaveClanID);
	bank.AddToggle("sm_bSaveUgcID", &CSavegamePhotoMetadata::sm_bSaveUgcID);
	bank.AddToggle("sm_bSaveTheGameMode", &CSavegamePhotoMetadata::sm_bSaveTheGameMode);
	bank.AddToggle("sm_bSaveTheGameTime", &CSavegamePhotoMetadata::sm_bSaveTheGameTime);
	bank.AddToggle("sm_bSaveTheMemeFlag", &CSavegamePhotoMetadata::sm_bSaveTheMemeFlag);
}
#endif	//	__BANK

bool CPhotoManager::ReadyForScreenShot()
{
	if (CHighQualityScreenshot::ReadyToTakeScreenShot())
	{
		return true;
	}

	return false;

// 	for (u32 loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		if (ms_ArrayOfPhotoBuffers[loop].ReadyToTakeScreenShot())
// 		{
// 			return true;
// 		}
// 	}
// 	return false;
}

grcRenderTarget* CPhotoManager::GetScreenshotRenderTarget()
{
	if (CHighQualityScreenshot::DoTakeScreenShot())
	{
		return CHighQualityScreenshot::GetRenderTarget();
	}

	return NULL;


// 	grcRenderTarget *pReturnRenderTarget = NULL;
// 
// 	for (u32 loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		if (ms_ArrayOfPhotoBuffers[loop].DoTakeScreenShot())
// 		{
// 			photoAssertf(pReturnRenderTarget == NULL, "CPhotoManager::GetScreenshotRenderTarget - more than one high quality photo is active at the same time");
// 			pReturnRenderTarget = ms_ArrayOfPhotoBuffers[loop].GetRenderTarget();
// 		}
// 	}
// 
// 	return pReturnRenderTarget;
}

#if RSG_ORBIS || RSG_DURANGO
void CPhotoManager::InsertFence()
{
	if (CHighQualityScreenshot::IsProcessingScreenshot())
	{
		CHighQualityScreenshot::InsertFence();
	}

// 	for (u32 loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		if (ms_ArrayOfPhotoBuffers[loop].IsProcessingScreenshot())
// 		{
// 			ms_ArrayOfPhotoBuffers[loop].InsertFence();
// 		}
// 	}
}
#endif // RSG_ORBIS || RSG_DURANGO

void CPhotoManager::Execute(grcRenderTarget* src)
{
	if (!ReadyForScreenShot()) 
	{
		Execute((grcTexture*) NULL);
		return;
	}

#if DEVICE_MSAA
	grcRenderTarget *resolvedSource = NULL;
	if (src->GetMSAA())
	{
		grcTextureFactory::CreateParams params; params.Format = grctfA8R8G8B8;
		resolvedSource = grcTextureFactory::GetInstance().CreateRenderTarget("Resolved picture source", grcrtPermanent, src->GetWidth(), src->GetHeight(), 32, &params);
		((grcRenderTargetMSAA*) src)->Resolve(resolvedSource);
		src = resolvedSource;
	}
#endif
#if __XENON
	grcTextureFactory::GetInstance().LockRenderTarget(0, src, NULL);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif

	Execute((grcTexture*) src);

#if DEVICE_MSAA
	if (resolvedSource) delete resolvedSource;
#endif
}

void CPhotoManager::Execute(grcTexture* src)
{
	if (src)
	{
		grcRenderTarget* screenShotRT = CPhotoManager::GetScreenshotRenderTarget();

		if(screenShotRT)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, screenShotRT, NULL);
			CSprite2d screenshotSprite;


			float x0 = -1.0f;
			float x1 = 1.0f;

			if (CHudTools::GetWideScreen() == false)
			{
				GRCDEVICE.Clear(true, Color32(0), false, 0.0f, 0U);
				x0 = -0.75f;
				x1 = 0.75f;
			}

			screenshotSprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
#if RSG_ORBIS
			screenshotSprite.BeginCustomList(CSprite2d::CS_BLIT_XZ_SWIZZLE, src);
#else
			screenshotSprite.BeginCustomList(CSprite2d::CS_BLIT, src);
#endif
#if SUPPORT_MULTI_MONITOR
			const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
#else
			const fwRect area(0.f, 0.f, 1.f, 1.f);
#endif
			grcDrawSingleQuadf(x0, 1.0f, x1, -1.0f, 0.0f, area.left, area.bottom, area.right, area.top, Color32(255, 255, 255 ,255));
			screenshotSprite.EndCustomList();
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if RSG_ORBIS || RSG_DURANGO
			InsertFence();
#endif // RSG_ORBIS || RSG_DURANGO
		}

		PHONEPHOTOEDITOR.SetDestTarget(screenShotRT);
		PHONEPHOTOEDITOR.Render();
	}

	u32 loop = 0;
// 	for (loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		ms_ArrayOfPhotoBuffers[loop].Execute();
// 	}
	CHighQualityScreenshot::Execute();

	for (loop = 0; loop < NUMBER_OF_LOW_QUALITY_PHOTOS; loop++)
	{
		ms_ArrayOfLowQualityPhotos[loop].Execute();
	}
}


void CPhotoManager::UpdateHighQualityScreenshots()
{
// 	for (u32 loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		ms_ArrayOfPhotoBuffers[loop].Update();
// 	}
	CHighQualityScreenshot::Update();
}

bool CPhotoManager::IsProcessingScreenshot()
{
	return CHighQualityScreenshot::IsProcessingScreenshot();

// 	for (u32 loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		if (ms_ArrayOfPhotoBuffers[loop].IsProcessingScreenshot())
// 		{
// 			return true;
// 		}
// 	}
// 
// 	return false;
}

bool CPhotoManager::AreRenderPhasesDisabled()
{
	return CHighQualityScreenshot::AreRenderPhasesDisabled();

// 	for (u32 loop = 0; loop < MAX_HIGH_QUALITY_PHOTOS; loop++)
// 	{
// 		if (ms_ArrayOfPhotoBuffers[loop].AreRenderPhasesDisabled())
// 		{
// 			return true;
// 		}
// 	}
// 
// 	return false;
}

bool CPhotoManager::RenderCameraPhoto()
{
	if (ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].IsRendering() && ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].GetRenderToPhone())
	{
		ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].Render(true);
//		DLC_Add(ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].Render, true);
		return true;
	}

	return false;
}


#if __BANK
void CPhotoManager::RenderCameraPhotoDebug()
{
	if (ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].IsRendering() && ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].GetRenderDebug())
	{
		ms_ArrayOfLowQualityPhotos[INDEX_OF_LOW_QUALITY_PHOTO_FOR_PHONE].Render(false);
	}
}
#endif	//	__BANK


/*
if the operation is in the queue then it's busy

PHOTO_OPERATION_TAKE_CAMERA_PHOTO,
		otherwise Check ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA] to see if it's finished

PHOTO_OPERATION_SAVE_CAMERA_PHOTO,
	need to store whether the last save SUCCEEDED or failed

PHOTO_OPERATION_LOAD_CAMERA_PHOTO,
	need to store the index OF the last photo that was successfully loaded
	maybe set the entry to -1 if a LOAD FAILs

PHOTO_OPERATION_FREE_MEMORY_FOR_HIGH_QUALITY_CAMERA_PHOTO,
	could maybe just Check GetSaveBuffer() for ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA] 

PHOTO_OPERATION_LOAD_GALLERY_PHOTO,
	need to store the index OF the last photo that was successfully loaded into this slot
	maybe set the entry to -1 if a LOAD FAILs

PHOTO_OPERATION_UPLOAD_GALLERY_PHOTO_TO_CLOUD,
	need to store if the last upload SUCCEEDED into this slot

PHOTO_OPERATION_SAVE_SCREENSHOT_TO_FILE
	if necessary, could store the filename OF the last successful save
*/



void CPhotoManager::SetCurrentPhonePhotoIsMugshot(bool bIsMugshot)
{
	ms_PhotoMetadata.GetMetadata(HIGH_QUALITY_PHOTO_CAMERA).SetIsMugshot(bIsMugshot);

	if (bIsMugshot)
	{
		char newTitle[256];

		safecpy(newTitle, "", NELEM(newTitle));

		if (TheText.DoesTextLabelExist("MUG_TITLE"))
		{
			safecpy(newTitle, TheText.Get("MUG_TITLE"), NELEM(newTitle));
		}

		ms_PhotoMetadata.SetTitle(HIGH_QUALITY_PHOTO_CAMERA, newTitle);
		ms_PhotoMetadata.SetDescription(HIGH_QUALITY_PHOTO_CAMERA, "");

		ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA].SetTitleString(ms_PhotoMetadata.GetTitle(HIGH_QUALITY_PHOTO_CAMERA));
		ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA].SetDescriptionString(ms_PhotoMetadata.GetDescription(HIGH_QUALITY_PHOTO_CAMERA));
	}
}

void CPhotoManager::SetArenaThemeAndVariationForCurrentPhoto(u32 arenaTheme, u32 arenaVariation)
{
	ms_PhotoMetadata.GetMetadata(HIGH_QUALITY_PHOTO_CAMERA).SetArenaThemeAndVariation(arenaTheme, arenaVariation);
}

void CPhotoManager::SetOnIslandXForCurrentPhoto(bool bOnIslandX)
{
	ms_PhotoMetadata.GetMetadata(HIGH_QUALITY_PHOTO_CAMERA).SetOnIslandX(bOnIslandX);
}

#if __LOAD_LOCAL_PHOTOS

void CPhotoManager::SetTextureDownloadSucceeded(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	ms_TextureDictionariesForGalleryPhotos.SetTextureDownloadSucceeded(uniquePhotoId);
}

void CPhotoManager::SetTextureDownloadFailed(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	ms_TextureDictionariesForGalleryPhotos.SetTextureDownloadFailed(uniquePhotoId);
}


void CPhotoManager::FillMergedListOfPhotos()
{
	CSavegamePhotoCloudList::FillListWithCloudDirectoryContents();
}

bool CPhotoManager::RequestListOfCloudPhotos()
{
	return CSavegamePhotoCloudList::RequestListOfCloudDirectoryContents();
}

bool CPhotoManager::GetIsCloudPhotoRequestPending()
{
	return CSavegamePhotoCloudList::GetIsCloudDirectoryRequestPending();
}

bool CPhotoManager::GetHasCloudPhotoRequestSucceeded()
{
	return CSavegamePhotoCloudList::GetHasCloudDirectoryRequestSucceeded();
}


bool CPhotoManager::IsListOfPhotosUpToDate(bool bLocalList)
{
	if (bLocalList)
	{
		return CSavegamePhotoLocalList::GetListHasBeenCreated();
	}
	else
	{
		return CSavegamePhotoCloudList::GetListIsUpToDate();
	}
}

u32 CPhotoManager::GetNumberOfPhotos(bool UNUSED_PARAM(bDeletedPhotosShouldBeCounted))
{
#if __BANK
	if (ms_RockstarEditorPhotoWidgets.OverrideNumberOfPhotosTaken())
	{
		return ms_RockstarEditorPhotoWidgets.GetNumberOfPhotosTakenOverride();
	}
#endif	//	__BANK

	return CSavegamePhotoLocalList::GetNumberOfPhotosInList();
}

// **************** Load Gallery Photo ****************

bool CPhotoManager::RequestLoadGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::RequestLoadGalleryPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO;
		photoData.m_UniquePhotoId = UniquePhotoId;
		return ms_QueueOfPhotoOperations.Push(photoData);
	}

	return false;
}


MemoryCardError CPhotoManager::GetLoadGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetLoadGalleryPhotoStatus - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO;
		photoData.m_UniquePhotoId = UniquePhotoId;

		//	What do I return if the photo operation isn't found in the queue?
		if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		//	Otherwise check if the photo with that index is loaded into a slot. Will this function have to take the slot index as well?
		//	savegameAssertf(0, "CPhotoManager::GetLoadGalleryPhotoStatus - still need to finish this");

		if (ms_TextureDictionariesForGalleryPhotos.IsThisPhotoLoadedIntoAnyTextureDictionary(UniquePhotoId))
		{
			return MEM_CARD_COMPLETE;
		}
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::LoadGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId)
{
	enum eLoadGalleryPhotoProgress
	{
		LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT,
		LOAD_GALLERY_PHOTO_WAIT_FOR_LOCAL_LOAD_TO_COMPLETE
	};

	static eLoadGalleryPhotoProgress s_LoadGalleryPhotoProgress = LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT;

	if (bFirstCall)
	{
		s_LoadGalleryPhotoProgress = LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_LoadGalleryPhotoProgress)
	{
	case LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT :
		{
//			if (photoVerifyf(ms_PhotoLoad.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::LoadGalleryPhoto - can't queue a photo load while one is already in progress"))
			{
//				if (photoVerifyf( (photoUniqueId.GetValue() >= 0) && (photoUniqueId.GetValue() < CSavegamePhotoLocalList::GetNumberOfPhotosInList()), "CPhotoManager::LoadGalleryPhoto - photo slot %d is out of range 0 to %u", photoUniqueId.GetValue(), CSavegamePhotoLocalList::GetNumberOfPhotosInList()))
				{
					bool bAlreadyLoaded = false;
					s32 freeTextureDictionaryIndex = ms_TextureDictionariesForGalleryPhotos.FindFreeTextureDictionary(photoUniqueId, bAlreadyLoaded);

					if (freeTextureDictionaryIndex >= 0)
					{
						if (bAlreadyLoaded)
						{
							returnValue = MEM_CARD_COMPLETE;
						}
						else	//	if (bAlreadyLoaded)
						{
							if (photoVerifyf(ms_LocalPhotoLoad.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::LoadGalleryPhoto - can't queue a local photo load while one is already in progress"))
							{
								ms_LocalPhotoLoad.Init(photoUniqueId, &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_GALLERY]);

								if (CGenericGameStorage::PushOnToSavegameQueue(&ms_LocalPhotoLoad))
								{
									s_LoadGalleryPhotoProgress = LOAD_GALLERY_PHOTO_WAIT_FOR_LOCAL_LOAD_TO_COMPLETE;
									photoDisplayf("CPhotoManager::LoadGalleryPhoto - local photo load has been queued for unique photo Id %d", photoUniqueId.GetValue());
								}
								else
								{
									photoAssertf(0, "CPhotoManager::LoadGalleryPhoto - failed to queue the local photo load for unique photo Id %d", photoUniqueId.GetValue());
									returnValue = MEM_CARD_ERROR;
								}
							}
						}	//	if (bAlreadyLoaded)
					}
					else	//	if (freeTextureDictionaryIndex >= 0)
					{
						returnValue = MEM_CARD_ERROR;
					}	//	if (freeTextureDictionaryIndex >= 0)
				}
			}
		}
		break;

	case LOAD_GALLERY_PHOTO_WAIT_FOR_LOCAL_LOAD_TO_COMPLETE :
		returnValue = ms_LocalPhotoLoad.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::LoadGalleryPhoto - local load completed");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::LoadGalleryPhoto - local load failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}

// **************** End of Load Gallery Photo ****************


// **************** Unload Gallery Photo ****************

bool CPhotoManager::RequestUnloadGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::RequestUnloadGalleryPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO;
		photoData.m_UniquePhotoId = UniquePhotoId;
		return ms_QueueOfPhotoOperations.Push(photoData);
	}

	return false;
}

MemoryCardError CPhotoManager::GetUnloadGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetUnloadGalleryPhotoStatus - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO;
		photoData.m_UniquePhotoId = UniquePhotoId;

		//	What do I return if the photo operation isn't found in the queue?
		if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		if (ms_TextureDictionariesForGalleryPhotos.IsThisPhotoLoadedIntoAnyTextureDictionary(UniquePhotoId))
		{
			return MEM_CARD_ERROR;
		}

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::UnloadGalleryPhoto(bool UNUSED_PARAM(bFirstCall), const CSavegamePhotoUniqueId &photoUniqueId)
{
	CSavegamePhotoLocalMetadata::Remove(photoUniqueId);
	return ms_TextureDictionariesForGalleryPhotos.UnloadPhoto(photoUniqueId);
}


// **************** End of Unload Gallery Photo ****************


// **************** Delete Gallery Photo ****************

CEntryInMergedPhotoListForDeletion CPhotoManager::RequestDeleteGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
#if !__NO_OUTPUT
	photoDisplayf("Callstack for CPhotoManager::RequestDeleteGalleryPhoto is ");
	sysStack::PrintStackTrace(StackTraceDisplayLine);
#endif	//	!__NO_OUTPUT

	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::RequestDeleteGalleryPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO;
		photoData.m_UniquePhotoId = UniquePhotoId;

		if (ms_QueueOfPhotoOperations.Push(photoData))
		{
			return CEntryInMergedPhotoListForDeletion(UniquePhotoId.GetValue());
		}
	}

	return CEntryInMergedPhotoListForDeletion(-1);
}

MemoryCardError CPhotoManager::GetDeleteGalleryPhotoStatus(const CEntryInMergedPhotoListForDeletion &entryForDeletion)
{
	CSavegamePhotoUniqueId UniquePhotoId;
	UniquePhotoId.Set(entryForDeletion.GetIntegerValue(), false);

	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO;
	photoData.m_UniquePhotoId = UniquePhotoId;

	//	What do I return if the photo operation isn't found in the queue?
	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

//	if (ms_MergedListOfPhotos.GetPhotoHasBeenDeleted(mergedIndex))
	if (!CSavegamePhotoLocalList::DoesIdExistInList(&UniquePhotoId))		//	I think this is the Local Photo equivalent of ms_MergedListOfPhotos.GetPhotoHasBeenDeleted()
	{
		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::DeleteGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId)
{
	enum eDeleteGalleryPhotoProgress
	{
		DELETE_LOCAL_GALLERY_PHOTO_BEGIN_DELETE,
		DELETE_LOCAL_GALLERY_PHOTO_CHECK_DELETE
	};

	//	ms_LocalPhotoDelete

	static eDeleteGalleryPhotoProgress s_DeleteGalleryPhotoProgress = DELETE_LOCAL_GALLERY_PHOTO_BEGIN_DELETE;

	if (bFirstCall)
	{
		s_DeleteGalleryPhotoProgress = DELETE_LOCAL_GALLERY_PHOTO_BEGIN_DELETE;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_DeleteGalleryPhotoProgress)
	{
	case DELETE_LOCAL_GALLERY_PHOTO_BEGIN_DELETE :
		{
			returnValue = MEM_CARD_ERROR;

//			if (photoVerifyf( (photoUniqueId.GetValue() >= 0) && (photoUniqueId.GetValue() < CSavegamePhotoLocalList::GetNumberOfPhotosInList()), "CPhotoManager::DeleteGalleryPhoto - photo slot %d is out of range 0 to %u", photoUniqueId.GetValue(), CSavegamePhotoLocalList::GetNumberOfPhotosInList()))
			{
				if (photoVerifyf(ms_LocalPhotoDelete.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::DeleteGalleryPhoto - can't queue a local photo delete while one is already in progress"))
				{
					char ms_NameOfLocalFileToDelete[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE];
					CSavegameFilenames::CreateNameOfLocalPhotoFile(ms_NameOfLocalFileToDelete, NELEM(ms_NameOfLocalFileToDelete), photoUniqueId);

					ms_LocalPhotoDelete.Init(ms_NameOfLocalFileToDelete, false, true);

					if (CGenericGameStorage::PushOnToSavegameQueue(&ms_LocalPhotoDelete))
					{
						s_DeleteGalleryPhotoProgress = DELETE_LOCAL_GALLERY_PHOTO_CHECK_DELETE;
						returnValue = MEM_CARD_BUSY;
						photoDisplayf("CPhotoManager::DeleteGalleryPhoto - local photo delete has been queued for unique photo Id %d", photoUniqueId.GetValue());
					}
					else
					{
						photoAssertf(0, "CPhotoManager::DeleteGalleryPhoto - failed to queue the local photo delete for unique photo Id %d", photoUniqueId.GetValue());
					}
				}
			}
		}
		break;

	case DELETE_LOCAL_GALLERY_PHOTO_CHECK_DELETE :
		{
			returnValue = ms_LocalPhotoDelete.GetStatus();

			if (returnValue == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CPhotoManager::DeleteGalleryPhoto - local delete completed");

				if (ms_TextureDictionariesForGalleryPhotos.IsThisPhotoLoadedIntoAnyTextureDictionary(photoUniqueId))
				{
					photoDisplayf("CPhotoManager::DeleteGalleryPhoto - unique photo Id %d is loaded so call UnloadGalleryPhoto", photoUniqueId.GetValue());
					switch (UnloadGalleryPhoto(true, photoUniqueId))	//	first param is unused so hopefully it doesn't matter what I specify
					{
					case MEM_CARD_COMPLETE :
						photoDisplayf("CPhotoManager::DeleteGalleryPhoto - UnloadGalleryPhoto succeeded");
						break;
					case MEM_CARD_BUSY :
						photoAssertf(0, "CPhotoManager::DeleteGalleryPhoto - expected UnloadGalleryPhoto to complete in one frame");
						break;
					case MEM_CARD_ERROR :
						photoAssertf(0, "CPhotoManager::DeleteGalleryPhoto - UnloadGalleryPhoto failed");
						break;
					}
				}
				else
				{
					photoDisplayf("CPhotoManager::DeleteGalleryPhoto - unique photo Id %d is not loaded so skip the UnloadGalleryPhoto call", photoUniqueId.GetValue());
				}

				CSavegamePhotoLocalList::Delete(photoUniqueId);

#if !__NO_OUTPUT
				photoDisplayf("CPhotoManager::DeleteGalleryPhoto - list of local photos after deleting unique photo Id %d is", photoUniqueId.GetValue());
				CSavegamePhotoLocalList::DisplayContentsOfList();
#endif	//	!__NO_OUTPUT

				if (CSavegamePhotoCloudList::GetListIsUpToDate())
				{
					if (CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId(photoUniqueId))
					{
						const char *pContentId = CSavegamePhotoCloudList::GetContentIdOfPhotoOnCloud(photoUniqueId);

						photoDisplayf("CPhotoManager::DeleteGalleryPhoto - a cloud photo exists with unique photo Id %d (content Id is %s) so I'll add it to the cloud delete queue", photoUniqueId.GetValue(), pContentId);
						if (CSavegamePhotoCloudDeleter::AddContentIdToQueue(pContentId))
						{
							CSavegamePhotoCloudList::RemovePhotoFromList(photoUniqueId);
						}
					}
					else
					{
						photoDisplayf("CPhotoManager::DeleteGalleryPhoto - no photo with unique photo Id %d exists on the cloud so I don't need to queue a cloud delete", photoUniqueId.GetValue());
					}
				}
				else
				{
					photoDisplayf("CPhotoManager::DeleteGalleryPhoto - list of cloud photos is not up to date so I can't check whether there is a corresponding cloud photo to delete");
				}
			}
			else if (returnValue == MEM_CARD_ERROR)
			{
				photoDisplayf("CPhotoManager::DeleteGalleryPhoto - local delete failed");
			}
		}
		break;
	}

	return returnValue;
}

// **************** End of Delete Gallery Photo ****************


// **************** Save a given photo directly ****************

MemoryCardError CPhotoManager::SaveGivenPhoto( bool bFirstCall )
{
	enum eSaveGivenPhotoProgress
	{
		SAVE_GIVEN_PHOTO_ALLOCATE_BUFFER,
		SAVE_GIVEN_PHOTO_FILL_METADATA_AND_BEGIN_SAVE,
		SAVE_GIVEN_PHOTO_CHECK_SAVE
	};

	static eSaveGivenPhotoProgress s_SavePhotoProgress = SAVE_GIVEN_PHOTO_ALLOCATE_BUFFER;

	if (bFirstCall)
	{
		s_SavePhotoProgress = SAVE_GIVEN_PHOTO_ALLOCATE_BUFFER;
		ms_StatusOfLastGivenPhotoSave = MEM_CARD_BUSY;			//	Check with James if this should be ms_StatusOfLastGivenPhotoSave instead of ms_StatusOfLastCameraPhotoSave
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_SavePhotoProgress)
	{
	case SAVE_GIVEN_PHOTO_ALLOCATE_BUFFER :
		//	Maybe I should rename AllocateAndFillBufferWhenLoadingCloudPhoto now that I'm using it here as well as in its original use in savegame_load_block.cpp
		if (ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].AllocateAndFillBufferWhenLoadingCloudPhoto(ms_jpegDataForGivenPhotoSave, ms_jpegDataSizeForGivenPhotoSave))
		{
			s_SavePhotoProgress = SAVE_GIVEN_PHOTO_FILL_METADATA_AND_BEGIN_SAVE;
		}
		else
		{
			returnValue = MEM_CARD_ERROR;
		}
		break;

	case SAVE_GIVEN_PHOTO_FILL_METADATA_AND_BEGIN_SAVE :
		{
			returnValue = MEM_CARD_ERROR;

			if ( (photoVerifyf(ms_PhotoSaveLocal.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::SaveGivenPhoto - can't queue a photo save while a local photo save is already in progress"))
				&& (photoVerifyf(ms_PhotoSave.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::SaveGivenPhoto - can't queue a photo save while a cloud photo save is already in progress")) )
			{
				CUndeletedEntryInMergedPhotoList metadataEntry( (u32)ms_indexOfMetadataForGivenPhotoSave, false);
				CSavegamePhotoMetadata const * pMetadata = GetMetadataForPhoto( metadataEntry );

				if (pMetadata)
				{
					CSavegamePhotoMetadata metaData(*pMetadata);
					metaData.FlagAsMemeEdited( true );

					u64 imageCRC = GenerateCRCFromImage(ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].GetJpegBuffer(), ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].GetExactSizeOfJpegData(), IMAGE_CRC_VERSION);
					metaData.SetImageSignature(imageCRC);

					metaData.GenerateNewUniqueId();

					if (photoVerifyf(ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].SetMetadataString(metaData), "CPhotoManager::SaveGivenPhoto - failed to write metadata string"))
					{
						if (photoVerifyf(ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].SetTitleString( GetTitleOfPhoto(metadataEntry) ), "CPhotoManager::SaveGivenPhoto - failed to write title string"))
						{
							if (photoVerifyf(ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].SetDescriptionString( GetDescriptionOfPhoto(metadataEntry) ), "CPhotoManager::SaveGivenPhoto - failed to write description string"))
							{
								ms_PhotoSaveLocal.Init(metaData.GetUniqueId(), &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS]);

								if (photoVerifyf(CGenericGameStorage::PushOnToSavegameQueue(&ms_PhotoSaveLocal), "CPhotoManager::SaveGivenPhoto - failed to queue the local photo save operation"))
								{
									photoDisplayf("CPhotoManager::SaveGivenPhoto - local photo save operation has been queued");
									s_SavePhotoProgress = SAVE_GIVEN_PHOTO_CHECK_SAVE;
									returnValue = MEM_CARD_BUSY;
								}
							}
						}
					}
				}
			}
		}
		break;

	case SAVE_GIVEN_PHOTO_CHECK_SAVE :
		returnValue = ms_PhotoSaveLocal.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::SaveGivenPhoto - succeeded");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::SaveGivenPhoto - failed");
			photoAssertf(0, "CPhotoManager::SaveGivenPhoto - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	if ( (returnValue == MEM_CARD_COMPLETE) || (returnValue == MEM_CARD_ERROR) )
	{
		ms_jpegDataForGivenPhotoSave = 0;
		ms_jpegDataSizeForGivenPhotoSave = 0;
		ms_indexOfMetadataForGivenPhotoSave = -1;

		ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_SAVING_MEME_EDITOR_RESULTS].FreeBuffer();
	}

	return returnValue;
}

// **************** End of Save a given photo directly ****************

// **************** ResaveMetadataForGalleryPhoto ****************

bool CPhotoManager::RequestResaveMetadataForGalleryPhoto(const CSavegamePhotoUniqueId &photoUniqueId)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO;
	photoData.m_UniquePhotoId = photoUniqueId;

	//	If there's already a resave operation in the queue (but not at the head) then maybe I don't need to queue another one
	if (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData, true))
	{
		return true;
	}

	return ms_QueueOfPhotoOperations.Push(photoData);
}


MemoryCardError CPhotoManager::GetResaveMetadataForGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetResaveMetadataForGalleryPhotoStatus - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO;
		photoData.m_UniquePhotoId = UniquePhotoId;

		//	What do I return if the photo operation isn't found in the queue?
		if (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		return ms_StatusOfLastResaveMetadataForGalleryPhoto;
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::ResaveMetadataForGalleryPhoto(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId)
{
//		Does the player need to be able to re-upload a photo after editing its name etc? Or should this be done automatically if the game knows that the photo already exists on the cloud?

	enum eResaveMetadataProgress
	{
		RESAVE_METADATA_BEGIN_RESAVE,
		RESAVE_METADATA_CHECK_RESAVE
	};

	static eResaveMetadataProgress s_ResaveMetadataProgress = RESAVE_METADATA_BEGIN_RESAVE;

	if (bFirstCall)
	{
		s_ResaveMetadataProgress = RESAVE_METADATA_BEGIN_RESAVE;
		ms_StatusOfLastResaveMetadataForGalleryPhoto = MEM_CARD_BUSY;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_ResaveMetadataProgress)
	{
		case RESAVE_METADATA_BEGIN_RESAVE :
		{
			if (photoVerifyf(ms_UpdateMetadataOfLocalAndCloudPhoto.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::ResaveMetadataForGalleryPhoto - can't queue a photo metadata resave while one is already in progress"))
			{
				ms_UpdateMetadataOfLocalAndCloudPhoto.Init(photoUniqueId, &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_RESAVING_METADATA_FOR_LOCAL_PHOTO]);

				if (CGenericGameStorage::PushOnToSavegameQueue(&ms_UpdateMetadataOfLocalAndCloudPhoto))
				{
					photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - photo metadata resave operation has been queued");
					s_ResaveMetadataProgress = RESAVE_METADATA_CHECK_RESAVE;
				}
				else
				{
					photoAssertf(0, "CPhotoManager::ResaveMetadataForGalleryPhoto - failed to queue the photo metadata resave operation");
					returnValue = MEM_CARD_ERROR;
				}
			}
		}
		break;

		case RESAVE_METADATA_CHECK_RESAVE :
		{
			returnValue = ms_UpdateMetadataOfLocalAndCloudPhoto.GetStatus();

#if !__NO_OUTPUT
			if (returnValue == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - succeeded");
			}
			else if (returnValue == MEM_CARD_ERROR)
			{
				photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - failed");
			}
#endif	//	!__NO_OUTPUT
		}
		break;
	}

	return returnValue;
}

// **************** End of ResaveMetadataForGalleryPhoto ****************

// **************** UploadLocalPhotoToCloud ****************
bool CPhotoManager::RequestUploadLocalPhotoToCloud(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::RequestUploadLocalPhotoToCloud - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD;
		photoData.m_UniquePhotoId = UniquePhotoId;
		return ms_QueueOfPhotoOperations.Push(photoData);
	}

	return false;
}

MemoryCardError CPhotoManager::GetUploadLocalPhotoToCloudStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetUploadLocalPhotoToCloudStatus - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UPLOAD_LOCAL_PHOTO_TO_CLOUD;
		photoData.m_UniquePhotoId = UniquePhotoId;

		//	What do I return if the photo operation isn't found in the queue?
		if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		return ms_StatusOfLastLocalPhotoUpload;
	}

	return MEM_CARD_ERROR;
}

MemoryCardError CPhotoManager::UploadLocalPhotoToCloud(bool bFirstCall, const CSavegamePhotoUniqueId &photoUniqueId)
{
	enum eUploadLocalPhotoProgress
	{
		UPLOAD_LOCAL_PHOTO_BEGIN,
		UPLOAD_LOCAL_PHOTO_UPDATE
	};

	static eUploadLocalPhotoProgress s_UploadLocalPhotoProgress = UPLOAD_LOCAL_PHOTO_BEGIN;

	if (bFirstCall)
	{
		s_UploadLocalPhotoProgress = UPLOAD_LOCAL_PHOTO_BEGIN;
		ms_StatusOfLastLocalPhotoUpload = MEM_CARD_BUSY;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_UploadLocalPhotoProgress)
	{
	case UPLOAD_LOCAL_PHOTO_BEGIN :
		{
			if (photoVerifyf(ms_UploadLocalPhotoToCloud.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::UploadLocalPhotoToCloud - can't queue a local photo upload while one is already in progress"))
			{
				ms_UploadLocalPhotoToCloud.Init(photoUniqueId, &ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_FOR_UPLOADING_LOCAL_PHOTO_TO_CLOUD]);
				if (CGenericGameStorage::PushOnToSavegameQueue(&ms_UploadLocalPhotoToCloud))
				{
					photoDisplayf("CPhotoManager::UploadLocalPhotoToCloud - photo upload operation has been queued");
					s_UploadLocalPhotoProgress = UPLOAD_LOCAL_PHOTO_UPDATE;
				}
				else
				{
					photoAssertf(0, "CPhotoManager::UploadLocalPhotoToCloud - failed to queue the photo upload operation");
					returnValue = MEM_CARD_ERROR;
				}
			}
		}
		break;

	case UPLOAD_LOCAL_PHOTO_UPDATE :
		returnValue = ms_UploadLocalPhotoToCloud.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::UploadLocalPhotoToCloud - succeeded");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::UploadLocalPhotoToCloud - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}

// **************** End of UploadLocalPhotoToCloud ****************

// **************** UploadMugshot ****************
bool CPhotoManager::RequestUploadMugshot(s32 multiplayerCharacterIndex)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UPLOAD_MUGSHOT;
	photoData.m_mugshotCharacterIndex = multiplayerCharacterIndex;
	return ms_QueueOfPhotoOperations.Push(photoData);
}


MemoryCardError CPhotoManager::GetUploadMugshotStatus(s32 multiplayerCharacterIndex)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UPLOAD_MUGSHOT;
	photoData.m_mugshotCharacterIndex = multiplayerCharacterIndex;

	//	What do I return if the photo operation isn't found in the queue?
	if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
	{
		return MEM_CARD_BUSY;
	}

	return ms_StatusOfLastMugshotUpload;
}

MemoryCardError CPhotoManager::UploadMugshot(bool bFirstCall, s32 mugshotCharacterIndex)
{
	enum eUploadMugshotProgress
	{
		UPLOAD_MUGSHOT_BEGIN,
		UPLOAD_MUGSHOT_UPDATE
	};

	static eUploadMugshotProgress s_UploadMugshotProgress = UPLOAD_MUGSHOT_BEGIN;

	if (bFirstCall)
	{
		s_UploadMugshotProgress = UPLOAD_MUGSHOT_BEGIN;
		ms_StatusOfLastMugshotUpload = MEM_CARD_BUSY;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_UploadMugshotProgress)
	{
	case UPLOAD_MUGSHOT_BEGIN :
		{
			if (photoVerifyf(ms_UploadMugshot.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::UploadMugshot - can't queue a mugshot upload while one is already in progress"))
			{
				ms_UploadMugshot.Init(&ms_ArrayOfPhotoBuffers[HIGH_QUALITY_PHOTO_CAMERA], mugshotCharacterIndex);
				if (CGenericGameStorage::PushOnToSavegameQueue(&ms_UploadMugshot))
				{
					photoDisplayf("CPhotoManager::UploadMugshot - mugshot upload operation has been queued");
					s_UploadMugshotProgress = UPLOAD_MUGSHOT_UPDATE;
				}
				else
				{
					photoAssertf(0, "CPhotoManager::UploadMugshot - failed to queue the mugshot upload operation");
					returnValue = MEM_CARD_ERROR;
				}
			}
		}
		break;

	case UPLOAD_MUGSHOT_UPDATE :
		returnValue = ms_UploadMugshot.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::UploadMugshot - succeeded");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::UploadMugshot - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}

//	Functions called by CSavegameQueuedOperation_UploadMugshot::Update()
bool CPhotoManager::RequestUploadOfMugshotPhoto(CPhotoBuffer *pPhotoBuffer, s32 mugshotCharacterIndex)
{
	if (!ms_pMugshotUploader)
	{
		return false;
	}

// 	if (ms_pMugshotUploader->IsAvailable() == false)
// 	{
// 		return false;
// 	}

	if (!pPhotoBuffer)
	{
		savegameDisplayf("CPhotoManager::RequestUploadOfMugshotPhoto - pPhotoBuffer is NULL");
		return false;
	}

	if (!pPhotoBuffer->GetJpegBuffer())
	{
		savegameDisplayf("CPhotoManager::RequestUploadOfMugshotPhoto - GetJpegBuffer() returned NULL");
		return false;
	}

	if (pPhotoBuffer->GetExactSizeOfJpegData() == 0)
	{
		savegameDisplayf("CPhotoManager::RequestUploadOfMugshotPhoto - size of jpeg buffer is 0");
		return false;
	}

	if (mugshotCharacterIndex < 0 || mugshotCharacterIndex > 4)
	{
		savegameDisplayf("CPhotoManager::RequestUploadOfMugshotPhoto - mugshotCharacterIndex is %d. It should be between 0 and 4", mugshotCharacterIndex);
		return false;
	}

#if RSG_ORBIS
	const char* platformExt = "_ps4";
#elif RSG_DURANGO
	const char* platformExt = "_xboxone";
#else
	const char* platformExt = "";
#endif

	char filename[64];
	sprintf(&filename[0], "publish/gta5/mugshots/mug%d%s.jpg", mugshotCharacterIndex, platformExt);

	return ms_pMugshotUploader->RequestUpload(pPhotoBuffer->GetJpegBuffer(), pPhotoBuffer->GetExactSizeOfJpegData(), &filename[0]);
}


MemoryCardError CPhotoManager::GetStatusOfUploadOfMugshotPhoto()
{
	if (!ms_pMugshotUploader)
	{
		return MEM_CARD_ERROR;
	}

	if (ms_pMugshotUploader->HasRequestFailed())
	{
		return MEM_CARD_ERROR;
	}
	else if (ms_pMugshotUploader->HasRequestSucceeded())
	{
		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_BUSY;
}
//	End of functions called by CSavegameQueuedOperation_UploadMugshot::Update()

// **************** End of UploadMugshot ****************


// ***** CreateSortedListOfPhotos

MemoryCardError CPhotoManager::CreateSortedListOfPhotos(bool bFirstCall, bool UNUSED_PARAM(bOnlyGetNumberOfPhotos), bool bDisplayWarningScreens)
{
	enum eCreateSortedListProgress
	{
		CREATE_SORTED_LIST_BEGIN,
		CREATE_SORTED_LIST_CHECK
	};

	static eCreateSortedListProgress s_CreateSortedListProgress = CREATE_SORTED_LIST_BEGIN;

	if (bFirstCall)
	{
		SetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen(false);
		s_CreateSortedListProgress = CREATE_SORTED_LIST_BEGIN;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_CreateSortedListProgress)
	{
	case CREATE_SORTED_LIST_BEGIN :
		{
			if (photoVerifyf(ms_CreateSortedListOfLocalPhotosQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::CreateSortedListOfPhotos - can't queue a CreateSortedListOfLocalPhotos operation while one is already in progress"))
			{
/* This check has been moved inside CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update()
				if (GetNumberOfPhotosIsUpToDate())
				{
					photoDisplayf("CPhotoManager::CreateSortedListOfPhotos - list of local photos is up to date so return COMPLETE immediately");

#if !__NO_OUTPUT
					CSavegamePhotoLocalList::DisplayContentsOfList();
#endif	//	!__NO_OUTPUT

					return MEM_CARD_COMPLETE;
				}
*/

				ms_CreateSortedListOfLocalPhotosQueuedOperation.Init(bDisplayWarningScreens);
				if (photoVerifyf(CGenericGameStorage::PushOnToSavegameQueue(&ms_CreateSortedListOfLocalPhotosQueuedOperation), "CPhotoManager::CreateSortedListOfPhotos - failed to queue the CreateSortedListOfLocalPhotos operation"))
				{
					s_CreateSortedListProgress = CREATE_SORTED_LIST_CHECK;
				}
			}
		}
		break;

	case CREATE_SORTED_LIST_CHECK :
		returnValue = ms_CreateSortedListOfLocalPhotosQueuedOperation.GetStatus();
		break;
	}

	return returnValue;
}

// ***** End of CreateSortedListOfPhotos



bool CPhotoManager::GetUniquePhotoIdFromUndeletedEntry(const CUndeletedEntryInMergedPhotoList &undeletedEntry, CSavegamePhotoUniqueId &returnUniqueId)
{
	if (CSavegamePhotoLocalList::GetUniquePhotoIdAtIndex(undeletedEntry.GetIntegerValue(), returnUniqueId))
	{
		returnUniqueId.SetIsMaximised(undeletedEntry.GetIsMaximised());
		return true;
	}

	return false;
}

void CPhotoManager::GetNameOfPhotoTextureDictionary(const CUndeletedEntryInMergedPhotoList &undeletedEntry, char *pTextureName, u32 LengthOfTextureName)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetNameOfPhotoTextureDictionary - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(UniquePhotoId, pTextureName, LengthOfTextureName);
	}
	else
	{
		safecpy(pTextureName, "", LengthOfTextureName);
	}
}


//	Metadata accessors

CSavegamePhotoMetadata const * CPhotoManager::GetMetadataForPhoto( const CUndeletedEntryInMergedPhotoList &undeletedEntry )
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetMetadataForPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetMetadataForPhoto - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			return pMetadata->GetMetadata();
		}
	}

	return 0;
}


const char *CPhotoManager::GetTitleOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetTitleOfPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetTitleOfPhoto - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			return pMetadata->GetTitleOfPhoto();
		}
	}

	return "";
}

bool CPhotoManager::SetTitleOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry, const char *pTitle)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	bool bPhotoWillBeResaved = false;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::SetTitleOfPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::SetTitleOfPhoto - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const char *pCurrentTitle = pMetadata->GetTitleOfPhoto();

			if (strcmp(pCurrentTitle, pTitle))
			{
				bPhotoWillBeResaved = RequestResaveMetadataForGalleryPhoto(UniquePhotoId);
			}

			pMetadata->SetTitleOfPhoto(pTitle);
		}
	}

	return bPhotoWillBeResaved;
}

const char *CPhotoManager::GetDescriptionOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetDescriptionOfPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetDescriptionOfPhoto - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			return pMetadata->GetDescriptionOfPhoto();
		}
	}

	return "";
}

bool CPhotoManager::GetCanPhotoBeUploadedToSocialClub(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetCanPhotoBeUploadedToSocialClub - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetCanPhotoBeUploadedToSocialClub - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			return pMetadata->GetHasValidSignature();
		}
	}

	return false;
}

void CPhotoManager::GetPhotoLocation(const CUndeletedEntryInMergedPhotoList &undeletedEntry, Vector3& vLocation)
{
	vLocation.Zero();

	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetPhotoLocation - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetPhotoLocation - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
			if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetPhotoLocation - failed to get pointer to CSavegamePhotoMetadata"))
			{
				pPhotoMetadata->GetPhotoLocation(vLocation);
			}
		}
	}
}

const char *CPhotoManager::GetSongTitle(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetSongTitle - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetSongTitle - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
			if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetSongTitle - failed to get pointer to CSavegamePhotoMetadata"))
			{
				return pPhotoMetadata->GetSongTitle();
			}
		}
	}

	return "";
}

const char *CPhotoManager::GetSongArtist(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetSongArtist - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetSongArtist - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
			if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetSongArtist - failed to get pointer to CSavegamePhotoMetadata"))
			{
				return pPhotoMetadata->GetSongArtist();
			}
		}
	}

	return "";
}



bool CPhotoManager::GetCreationDate(const CUndeletedEntryInMergedPhotoList &undeletedEntry, CDate &returnDate)
{
	u64 creationDate = CPhotoManager::ms_InvalidDate;

	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetCreationDate - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetCreationDate - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
			if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetCreationDate - failed to get pointer to CSavegamePhotoMetadata"))
			{
				creationDate = (u64) pPhotoMetadata->GetPosixCreationTime();
			}
		}
	}

	if (creationDate == CPhotoManager::ms_InvalidDate)
	{
		returnDate.Initialise();
		return false;
	}

	time_t t = static_cast<time_t>(creationDate);
	struct tm tInfo = *localtime(&t);		//	*gmtime(&t);

	returnDate.Set(tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec,
		tInfo.tm_mday, (tInfo.tm_mon + 1), (1900 + tInfo.tm_year) );

	return true;
}

bool CPhotoManager::GetIsMemePhoto( const CUndeletedEntryInMergedPhotoList &undeletedEntry )
{
	bool isMeme = false;

	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetIsMemePhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetIsMemePhoto - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
			if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetIsMemePhoto - failed to get pointer to CSavegamePhotoMetadata"))
			{
				isMeme = pPhotoMetadata->IsMemeEdited();
			}
		}
	}

	return isMeme;
}

bool CPhotoManager::GetHasPhotoBeenUploadedToSocialClub( const CUndeletedEntryInMergedPhotoList &undeletedEntry )
{
	bool bHasBeenUploaded = false;

	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(CSavegamePhotoCloudList::GetListIsUpToDate(), "CPhotoManager::GetHasPhotoBeenUploadedToSocialClub - list of cloud photos is not up to date"))
	{
		if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetHasPhotoBeenUploadedToSocialClub - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
		{
			const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

			if (photoVerifyf(pMetadata, "CPhotoManager::GetHasPhotoBeenUploadedToSocialClub - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
			{
				const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
				if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetHasPhotoBeenUploadedToSocialClub - failed to get pointer to CSavegamePhotoMetadata"))
				{
					const CSavegamePhotoUniqueId &uniqueId = pPhotoMetadata->GetUniqueId();

					photoAssertf(UniquePhotoId.GetValue() == uniqueId.GetValue(), "CPhotoManager::GetHasPhotoBeenUploadedToSocialClub - just checking if UniquePhotoId %d is ever different from uniqueId %d. Maybe I didn't need to get uniqueId at all. I could have just used UniquePhotoId", UniquePhotoId.GetValue(), uniqueId.GetValue());

					if (CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId(uniqueId))
					{
						bHasBeenUploaded = true;
					}
				}
			}
		}
	}

	return bHasBeenUploaded;
}

const char *CPhotoManager::GetContentIdOfUploadedPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(CSavegamePhotoCloudList::GetListIsUpToDate(), "CPhotoManager::GetContentIdOfUploadedPhoto - list of cloud photos is not up to date"))
	{
		if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetContentIdOfUploadedPhoto - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
		{
			const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

			if (photoVerifyf(pMetadata, "CPhotoManager::GetContentIdOfUploadedPhoto - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
			{
				const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
				if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetContentIdOfUploadedPhoto - failed to get pointer to CSavegamePhotoMetadata"))
				{
					const CSavegamePhotoUniqueId &uniqueId = pPhotoMetadata->GetUniqueId();

					photoAssertf(UniquePhotoId.GetValue() == uniqueId.GetValue(), "CPhotoManager::GetContentIdOfUploadedPhoto - just checking if UniquePhotoId %d is ever different from uniqueId %d. Maybe I didn't need to get uniqueId at all. I could have just used UniquePhotoId", UniquePhotoId.GetValue(), uniqueId.GetValue());

					if (photoVerifyf(CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId(uniqueId), "CPhotoManager::GetContentIdOfUploadedPhoto - cloud photo with unique Id %d doesn't exist", uniqueId.GetValue()))
					{
						return CSavegamePhotoCloudList::GetContentIdOfPhotoOnCloud(uniqueId);
					}
				}
			}
		}
	}

	return "";
}

bool CPhotoManager::GetIsMugshot(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CSavegamePhotoUniqueId UniquePhotoId;

	if (photoVerifyf(GetUniquePhotoIdFromUndeletedEntry(undeletedEntry, UniquePhotoId), "CPhotoManager::GetIsMugshot - failed to convert undeleted entry %d to a valid unique photo Id", undeletedEntry.GetIntegerValue()))
	{
		const CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(UniquePhotoId);

		if (photoVerifyf(pMetadata, "CPhotoManager::GetIsMugshot - failed to find metadata for unique photo Id %d", UniquePhotoId.GetValue()))
		{
			const CSavegamePhotoMetadata *pPhotoMetadata = pMetadata->GetMetadata();
			if (photoVerifyf(pPhotoMetadata, "CPhotoManager::GetIsMugshot - failed to get pointer to CSavegamePhotoMetadata"))
			{
				return pPhotoMetadata->GetIsMugshot();
			}
		}
	}

	return false;
}

#else	//	__LOAD_LOCAL_PHOTOS

void CPhotoManager::SetTextureDownloadSucceeded(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	ms_TextureDictionariesForGalleryPhotos.SetTextureDownloadSucceeded(indexWithinMergedPhotoList);
}


void CPhotoManager::SetTextureDownloadFailed(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	ms_TextureDictionariesForGalleryPhotos.SetTextureDownloadFailed(indexWithinMergedPhotoList);
}


//	Is this not called anywhere?
bool CPhotoManager::ReadMetadataFromString(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList, const char* pJsonString, u32 LengthOfJsonString)
{
	return ms_MergedListOfPhotos.ReadMetadataFromString(indexWithinMergedPhotoList, pJsonString, LengthOfJsonString);
}


bool CPhotoManager::GetHasValidMetadata(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	return ms_MergedListOfPhotos.GetHasValidMetadata(indexWithinMergedPhotoList);
}

const char *CPhotoManager::GetContentIdOfPhotoOnCloud(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	return ms_MergedListOfPhotos.GetContentIdOfPhotoOnCloud(indexWithinMergedPhotoList);
}

int CPhotoManager::GetFileID(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	return ms_MergedListOfPhotos.GetFileID(indexWithinMergedPhotoList);
}

int CPhotoManager::GetFileVersion(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	return ms_MergedListOfPhotos.GetFileVersion(indexWithinMergedPhotoList);
}

const rlScLanguage &CPhotoManager::GetLanguage(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	return ms_MergedListOfPhotos.GetLanguage(indexWithinMergedPhotoList);
}


void CPhotoManager::FillMergedListOfPhotos(bool bOnlyGetNumberOfPhotos)
{
	if (bOnlyGetNumberOfPhotos)
	{
		ms_MergedListOfPhotos.SetNumberOfUGCPhotos();
	}
	else
	{
		ms_MergedListOfPhotos.FillList();
	}
}

bool CPhotoManager::RequestListOfCloudPhotos(bool bOnlyGetNumberOfPhotos)
{
	return ms_MergedListOfPhotos.RequestListOfCloudDirectoryContents(bOnlyGetNumberOfPhotos);
}

bool CPhotoManager::GetIsCloudPhotoRequestPending(bool bOnlyGetNumberOfPhotos)
{
	return ms_MergedListOfPhotos.GetIsCloudDirectoryRequestPending(bOnlyGetNumberOfPhotos);
}

bool CPhotoManager::GetHasCloudPhotoRequestSucceeded(bool bOnlyGetNumberOfPhotos)
{
	return ms_MergedListOfPhotos.GetHasCloudDirectoryRequestSucceeded(bOnlyGetNumberOfPhotos);
}

bool CPhotoManager::GetNumberOfPhotosIsUpToDate()
{
	return ms_MergedListOfPhotos.GetNumberOfPhotosIsUpToDate();
}


u32 CPhotoManager::GetNumberOfPhotos(bool bDeletedPhotosShouldBeCounted)
{
	return ms_MergedListOfPhotos.GetNumberOfPhotos(bDeletedPhotosShouldBeCounted);
}


const char *CPhotoManager::GetContentId(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetContentId - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetContentIdOfPhotoOnCloud(mergedIndex);
	}

	return "";
}




bool CPhotoManager::RequestLoadGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::RequestLoadGalleryPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO;
		photoData.m_IndexOfPhotoWithinMergedList = mergedIndex;
		return ms_QueueOfPhotoOperations.Push(photoData);
	}

	return false;
}

MemoryCardError CPhotoManager::GetLoadGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetLoadGalleryPhotoStatus - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_LOAD_GALLERY_PHOTO;
		photoData.m_IndexOfPhotoWithinMergedList = mergedIndex;

		//	What do I return if the photo operation isn't found in the queue?
		if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		//	Otherwise check if the photo with that index is loaded into a slot. Will this function have to take the slot index as well?
		//	savegameAssertf(0, "CPhotoManager::GetLoadGalleryPhotoStatus - still need to finish this");

		if (ms_TextureDictionariesForGalleryPhotos.IsThisPhotoLoadedIntoAnyTextureDictionary(mergedIndex))
		{
			return MEM_CARD_COMPLETE;
		}
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::LoadGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	enum eLoadGalleryPhotoProgress
	{
		LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT,
		LOAD_GALLERY_PHOTO_WAIT_FOR_CLOUD_LOAD_TO_COMPLETE
	};

	static eLoadGalleryPhotoProgress s_LoadGalleryPhotoProgress = LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT;

	if (bFirstCall)
	{
		s_LoadGalleryPhotoProgress = LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_LoadGalleryPhotoProgress)
	{
	case LOAD_GALLERY_PHOTO_FIND_FREE_TEXTURE_DICTIONARY_SLOT :
		{
			//				if (photoVerifyf(ms_PhotoLoad.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::LoadGalleryPhoto - can't queue a photo load while one is already in progress"))
			{
				if (photoVerifyf( (indexWithinMergedPhotoList.GetIntegerValue() >= 0) && (indexWithinMergedPhotoList.GetIntegerValue() < ms_MergedListOfPhotos.GetNumberOfPhotos(true)), "CPhotoManager::LoadGalleryPhoto - photo slot %d is out of range 0 to %u", indexWithinMergedPhotoList.GetIntegerValue(), ms_MergedListOfPhotos.GetNumberOfPhotos(true)))
				{
					bool bAlreadyLoaded = false;
					s32 freeTextureDictionaryIndex = ms_TextureDictionariesForGalleryPhotos.FindFreeTextureDictionary(indexWithinMergedPhotoList, bAlreadyLoaded);

					if (freeTextureDictionaryIndex >= 0)
					{
						if (bAlreadyLoaded)
						{
							returnValue = MEM_CARD_COMPLETE;
						}
						else	//	if (bAlreadyLoaded)
						{
							s32 photoIndex = ms_MergedListOfPhotos.GetArrayIndex(indexWithinMergedPhotoList);

							if (photoVerifyf(photoIndex >= 0, "CPhotoManager::LoadGalleryPhoto - GetArrayIndex returned a negative number for merged list index %d", indexWithinMergedPhotoList.GetIntegerValue()))
							{
								CMergedListOfPhotos::ePhotoSource photoSource = ms_MergedListOfPhotos.GetPhotoSource(indexWithinMergedPhotoList);

								if (photoSource == CMergedListOfPhotos::PHOTO_SOURCE_NONE)
								{
									photoAssertf(0, "CPhotoManager::LoadGalleryPhoto - GetPhotoSource returned PHOTO_SOURCE_NONE for merged list index %d", indexWithinMergedPhotoList.GetIntegerValue());
									returnValue = MEM_CARD_ERROR;
								}
								else
								{
									if (photoVerifyf(ms_CloudPhotoLoad.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::LoadGalleryPhoto - can't queue a cloud photo load while one is already in progress"))
									{
										ms_CloudPhotoLoad.Init(indexWithinMergedPhotoList, CIndexOfPhotoOnCloud(photoIndex));	//	freeTextureDictionaryIndex, 

										if (CGenericGameStorage::PushOnToSavegameQueue(&ms_CloudPhotoLoad))
										{
											s_LoadGalleryPhotoProgress = LOAD_GALLERY_PHOTO_WAIT_FOR_CLOUD_LOAD_TO_COMPLETE;
											photoDisplayf("CPhotoManager::LoadGalleryPhoto - cloud load has been queued for merged index %d", indexWithinMergedPhotoList.GetIntegerValue());
										}
										else
										{
											photoAssertf(0, "CPhotoManager::LoadGalleryPhoto - failed to queue the cloud photo load for merged index %d", indexWithinMergedPhotoList.GetIntegerValue());
											returnValue = MEM_CARD_ERROR;
										}
									}
								}
							}
							else	//	if (photoVerifyf(photoIndex >= 0
							{
								returnValue = MEM_CARD_ERROR;
							}	//	if (photoVerifyf(photoIndex >= 0
						}	//	if (bAlreadyLoaded)
					}
					else	//	if (freeTextureDictionaryIndex >= 0)
					{
						returnValue = MEM_CARD_ERROR;
					}	//	if (freeTextureDictionaryIndex >= 0)
				}
			}
		}
		break;

	case LOAD_GALLERY_PHOTO_WAIT_FOR_CLOUD_LOAD_TO_COMPLETE :
		returnValue = ms_CloudPhotoLoad.GetStatus();

#if !__NO_OUTPUT
		if (returnValue == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CPhotoManager::LoadGalleryPhoto - cloud load completed");
		}
		else if (returnValue == MEM_CARD_ERROR)
		{
			photoDisplayf("CPhotoManager::LoadGalleryPhoto - cloud load failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	return returnValue;
}



bool CPhotoManager::RequestUnloadGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::RequestUnloadGalleryPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO;
		photoData.m_IndexOfPhotoWithinMergedList = mergedIndex;
		return ms_QueueOfPhotoOperations.Push(photoData);
	}

	return false;
}

MemoryCardError CPhotoManager::GetUnloadGalleryPhotoStatus(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetUnloadGalleryPhotoStatus - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_UNLOAD_GALLERY_PHOTO;
		photoData.m_IndexOfPhotoWithinMergedList = mergedIndex;

		//	What do I return if the photo operation isn't found in the queue?
		if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		if (ms_TextureDictionariesForGalleryPhotos.IsThisPhotoLoadedIntoAnyTextureDictionary(mergedIndex))
		{
			return MEM_CARD_ERROR;
		}

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::UnloadGalleryPhoto(bool UNUSED_PARAM(bFirstCall), const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	return ms_TextureDictionariesForGalleryPhotos.UnloadPhoto(indexWithinMergedPhotoList);
}



// **************** DeleteGalleryPhoto ****************

CEntryInMergedPhotoListForDeletion CPhotoManager::RequestDeleteGalleryPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
#if !__NO_OUTPUT
	photoDisplayf("Callstack for CPhotoManager::RequestDeleteGalleryPhoto is ");
	sysStack::PrintStackTrace(StackTraceDisplayLine);
#endif	//	!__NO_OUTPUT

	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::RequestDeleteGalleryPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO;
		photoData.m_IndexOfPhotoWithinMergedList = mergedIndex;
		if (ms_QueueOfPhotoOperations.Push(photoData))
		{
			return CEntryInMergedPhotoListForDeletion(mergedIndex.GetIntegerValue());
		}
	}

	return CEntryInMergedPhotoListForDeletion(-1);
}

MemoryCardError CPhotoManager::GetDeleteGalleryPhotoStatus(const CEntryInMergedPhotoListForDeletion &entryForDeletion)
{
	//	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);
	CIndexWithinMergedPhotoList mergedIndex(entryForDeletion.GetIntegerValue(), false);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetDeleteGalleryPhotoStatus - failed to convert entry for deletion %d to a valid index", entryForDeletion.GetIntegerValue()))
	{
		CPhotoOperationData photoData;
		photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_DELETE_GALLERY_PHOTO;
		photoData.m_IndexOfPhotoWithinMergedList = mergedIndex;

		//	What do I return if the photo operation isn't found in the queue?
		if  (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData))
		{
			return MEM_CARD_BUSY;
		}

		if (ms_MergedListOfPhotos.GetPhotoHasBeenDeleted(mergedIndex))
		{
			return MEM_CARD_COMPLETE;
		}
	}

	return MEM_CARD_ERROR;
}


MemoryCardError CPhotoManager::DeleteGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	enum eDeleteGalleryPhotoProgress
	{
		DELETE_GALLERY_PHOTO_BEGIN_DELETE,
		DELETE_GALLERY_PHOTO_BEGIN_DELETE_UGC_PHOTO,
		DELETE_GALLERY_PHOTO_CHECK_DELETE_UGC_PHOTO
	};

	static eDeleteGalleryPhotoProgress s_DeleteGalleryPhotoProgress = DELETE_GALLERY_PHOTO_BEGIN_DELETE;

	if (bFirstCall)
	{
		s_DeleteGalleryPhotoProgress = DELETE_GALLERY_PHOTO_BEGIN_DELETE;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_DeleteGalleryPhotoProgress)
	{
	case DELETE_GALLERY_PHOTO_BEGIN_DELETE :
		{
			returnValue = MEM_CARD_ERROR;
			if (photoVerifyf( (indexWithinMergedPhotoList.GetIntegerValue() >= 0) && (indexWithinMergedPhotoList.GetIntegerValue() < ms_MergedListOfPhotos.GetNumberOfPhotos(true)), "CPhotoManager::DeleteGalleryPhoto - photo slot %d is out of range 0 to %u", indexWithinMergedPhotoList.GetIntegerValue(), ms_MergedListOfPhotos.GetNumberOfPhotos(true)))
			{
				s32 photoIndex = ms_MergedListOfPhotos.GetArrayIndex(indexWithinMergedPhotoList);

				if (photoVerifyf(photoIndex >= 0, "CPhotoManager::DeleteGalleryPhoto - GetArrayIndex returned a negative number for merged list index %d", indexWithinMergedPhotoList.GetIntegerValue()))
				{
					CMergedListOfPhotos::ePhotoSource photoSource = ms_MergedListOfPhotos.GetPhotoSource(indexWithinMergedPhotoList);

					if (photoVerifyf(photoSource != CMergedListOfPhotos::PHOTO_SOURCE_NONE, "CPhotoManager::DeleteGalleryPhoto - GetPhotoSource returned PHOTO_SOURCE_NONE for merged list index %d", indexWithinMergedPhotoList.GetIntegerValue()))
					{
						s_DeleteGalleryPhotoProgress = DELETE_GALLERY_PHOTO_BEGIN_DELETE_UGC_PHOTO;
						returnValue = MEM_CARD_BUSY;
					}
				}
			}
		}
		break;

	case DELETE_GALLERY_PHOTO_BEGIN_DELETE_UGC_PHOTO :
		{
			returnValue = MEM_CARD_ERROR;

			const char *pContentID = ms_MergedListOfPhotos.GetContentIdOfPhotoOnCloud(indexWithinMergedPhotoList);
			if (photoVerifyf(UserContentManager::GetInstance().SetDeleted(pContentID, true, RLUGC_CONTENT_TYPE_GTA5PHOTO), "CPhotoManager::DeleteGalleryPhoto - DELETE_GALLERY_PHOTO_BEGIN_DELETE_UGC_PHOTO - SetDeleted for UGC photo has failed to start"))
			{
				photoDisplayf("CPhotoManager::DeleteGalleryPhoto - DELETE_GALLERY_PHOTO_BEGIN_DELETE_UGC_PHOTO - SetDeleted for UGC photo has been started");
				s_DeleteGalleryPhotoProgress = DELETE_GALLERY_PHOTO_CHECK_DELETE_UGC_PHOTO;
				returnValue = MEM_CARD_BUSY;
			}
		}
		break;

	case DELETE_GALLERY_PHOTO_CHECK_DELETE_UGC_PHOTO :
		{
			returnValue = MEM_CARD_BUSY;
			if (UserContentManager::GetInstance().HasModifyFinished())
			{
				if (UserContentManager::GetInstance().DidModifySucceed())
				{
					photoDisplayf("CPhotoManager::DeleteGalleryPhoto - DELETE_GALLERY_PHOTO_CHECK_DELETE_UGC_PHOTO - succeeded");
					returnValue = MEM_CARD_COMPLETE;
				}
				else
				{
					photoDisplayf("CPhotoManager::DeleteGalleryPhoto - DELETE_GALLERY_PHOTO_CHECK_DELETE_UGC_PHOTO - failed");
					returnValue = MEM_CARD_ERROR;
				}
			}
		}
		break;
	}

	if (returnValue == MEM_CARD_COMPLETE)
	{
		if (ms_TextureDictionariesForGalleryPhotos.IsThisPhotoLoadedIntoAnyTextureDictionary(indexWithinMergedPhotoList))
		{
			photoDisplayf("CPhotoManager::DeleteGalleryPhoto - photo slot %d is loaded so call UnloadGalleryPhoto", indexWithinMergedPhotoList.GetIntegerValue());
			switch (UnloadGalleryPhoto(true, indexWithinMergedPhotoList))	//	first param is unused so hopefully it doesn't matter what I specify
			{
			case MEM_CARD_COMPLETE :
				photoDisplayf("CPhotoManager::DeleteGalleryPhoto - UnloadGalleryPhoto succeeded");
				break;
			case MEM_CARD_BUSY :
				photoAssertf(0, "CPhotoManager::DeleteGalleryPhoto - expected UnloadGalleryPhoto to complete in one frame");
				break;
			case MEM_CARD_ERROR :
				photoAssertf(0, "CPhotoManager::DeleteGalleryPhoto - UnloadGalleryPhoto failed");
				break;
			}
		}
		else
		{
			photoDisplayf("CPhotoManager::DeleteGalleryPhoto - photo slot %d is not loaded so skip the UnloadGalleryPhoto call", indexWithinMergedPhotoList.GetIntegerValue());
		}
	}

	return returnValue;
}


// **************** Save a given photo directly ****************

MemoryCardError CPhotoManager::SaveGivenPhoto( bool bFirstCall )
{
	enum eSaveGivenPhotoProgress
	{
		SAVE_GIVEN_PHOTO_BEGIN,
		SAVE_GIVEN_PHOTO_UPDATE
	};

	static eSaveGivenPhotoProgress s_SavePhotoProgress = SAVE_GIVEN_PHOTO_BEGIN;

	if (bFirstCall)
	{
		s_SavePhotoProgress = SAVE_GIVEN_PHOTO_BEGIN;
		ms_StatusOfLastCameraPhotoSave = MEM_CARD_BUSY;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;
	bool cleanupStatics = false; 

	switch (s_SavePhotoProgress)
	{
	case SAVE_GIVEN_PHOTO_BEGIN :
		{
			if (photoVerifyf(ms_PhotoSave.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::SaveGivenPhoto - can't queue a photo save while one is already in progress"))
			{
				//		if (photoVerifyf( (PhotoSlotIndex >= 0) && (PhotoSlotIndex < NUM_SAVED_PHOTO_SLOTS), "CScriptHud::BeginPhotoSave - photo slot %d is out of range 0 to %d", PhotoSlotIndex, NUM_SAVED_PHOTO_SLOTS))
				{
					CUndeletedEntryInMergedPhotoList metadataEntry( (u32)ms_indexOfMetadataForGivenPhotoSave, false);
					CSavegamePhotoMetadata const * pMetadata = GetMetadataForPhoto( metadataEntry );

					CSavegamePhotoMetadata metaData( pMetadata ? *pMetadata : ms_PhotoMetadata.GetMetadata(HIGH_QUALITY_PHOTO_CAMERA) );
					metaData.FlagAsMemeEdited( true );

					ms_PhotoSave.Init( ms_jpegDataForGivenPhotoSave, ms_jpegDataSizeForGivenPhotoSave, metaData, GetTitleOfPhoto( metadataEntry ), GetDescriptionOfPhoto(metadataEntry) );	//	PhotoSlotIndex + INDEX_OF_FIRST_SAVED_PHOTO_SLOT, bSaveToCloud ); // Use true for cloud operation.
					if (CGenericGameStorage::PushOnToSavegameQueue(&ms_PhotoSave))
					{
						photoDisplayf("CPhotoManager::SaveGivenPhoto - photo save operation has been queued");
						s_SavePhotoProgress = SAVE_GIVEN_PHOTO_UPDATE;
					}
					else
					{
						cleanupStatics = true;

						photoAssertf(0, "CPhotoManager::SaveGivenPhoto - failed to queue the photo save operation");
						returnValue = MEM_CARD_ERROR;
					}

					//! Clean the meme flag incase we hi-jacked the phone photo meta-data
					metaData.FlagAsMemeEdited( false );
				}
			}
		}
		break;

	case SAVE_GIVEN_PHOTO_UPDATE :
		returnValue = ms_PhotoSave.GetStatus();

		if (returnValue == MEM_CARD_COMPLETE)
		{
			cleanupStatics = true;
#if !__NO_OUTPUT
			photoDisplayf("CPhotoManager::SaveGivenPhoto - succeeded");
#endif
		}
#if !__NO_OUTPUT
		else if (returnValue == MEM_CARD_ERROR)
		{
			cleanupStatics = true;
			photoDisplayf("CPhotoManager::SaveGivenPhoto - failed");
		}
#endif	//	!__NO_OUTPUT
		break;
	}

	if( cleanupStatics )
	{
		ms_jpegDataForGivenPhotoSave = 0;
		ms_jpegDataSizeForGivenPhotoSave = 0;
		ms_indexOfMetadataForGivenPhotoSave = -1;
	}

	return returnValue;
}


// **************** ResaveMetadataForGalleryPhoto ****************

bool CPhotoManager::RequestResaveMetadataForGalleryPhoto(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	CPhotoOperationData photoData;
	photoData.m_PhotoOperation = CPhotoOperationData::PHOTO_OPERATION_RESAVE_METADATA_FOR_GALLERY_PHOTO;
	photoData.m_IndexOfPhotoWithinMergedList = indexWithinMergedPhotoList;

	//	If there's already a resave operation in the queue (but not at the head) then maybe I don't need to queue another one
	if (ms_QueueOfPhotoOperations.DoesOperationExistInQueue(photoData, true))
	{
		return true;
	}

	return ms_QueueOfPhotoOperations.Push(photoData);
}



MemoryCardError CPhotoManager::ResaveMetadataForGalleryPhoto(bool bFirstCall, const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	enum eResaveMetadataProgress
	{
		//	Cloud photo
		RESAVE_CLOUD_METADATA_ALLOCATE_BUFFER_FOR_METADATA_STRING,
		RESAVE_CLOUD_METADATA_WRITE_NEW_METADATA_FOR_CLOUD_FILE,

		RESAVE_CLOUD_METADATA_BEGIN_UPDATE_UGC_METADATA,
		RESAVE_CLOUD_METADATA_CHECK_UPDATE_UGC_METADATA
	};

	static eResaveMetadataProgress s_ResaveMetadataProgress = RESAVE_CLOUD_METADATA_ALLOCATE_BUFFER_FOR_METADATA_STRING;

	if (bFirstCall)
	{
		switch (ms_MergedListOfPhotos.GetPhotoSource(indexWithinMergedPhotoList))
		{
		case CMergedListOfPhotos::PHOTO_SOURCE_NONE :
			photoAssertf(0, "CPhotoManager::ResaveMetadataForGalleryPhoto - source for photo %d is neither Cloud nor Local", indexWithinMergedPhotoList.GetIntegerValue());
			return MEM_CARD_ERROR;
			//				break;

		case CMergedListOfPhotos::PHOTO_SOURCE_CLOUD :
			s_ResaveMetadataProgress = RESAVE_CLOUD_METADATA_ALLOCATE_BUFFER_FOR_METADATA_STRING;
			break;
		}
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_ResaveMetadataProgress)
	{
		//	********** Cloud photo **********
	case RESAVE_CLOUD_METADATA_ALLOCATE_BUFFER_FOR_METADATA_STRING :
		{
			if (ms_pMetadataForResavingToCloud)
			{
				photoAssertf(0, "CPhotoManager::ResaveMetadataForGalleryPhoto - ALLOCATE_BUFFER_FOR_METADATA_STRING - expected metadata string to be NULL here");
				delete [] ms_pMetadataForResavingToCloud;
				ms_pMetadataForResavingToCloud = NULL;
			}

			ms_pMetadataForResavingToCloud = rage_new char[CPhotoBuffer::GetDefaultSizeOfJsonBuffer()];
			if (photoVerifyf(ms_pMetadataForResavingToCloud, "CPhotoManager::ResaveMetadataForGalleryPhoto - ALLOCATE_BUFFER_FOR_METADATA_STRING - failed to allocate memory for metadata string"))
			{
				s_ResaveMetadataProgress = RESAVE_CLOUD_METADATA_WRITE_NEW_METADATA_FOR_CLOUD_FILE;
			}
			//	should I set returnValue = MEM_CARD_ERROR; here?
		}
		break;

	case RESAVE_CLOUD_METADATA_WRITE_NEW_METADATA_FOR_CLOUD_FILE :
		if (ms_MergedListOfPhotos.WriteMetadataToString(indexWithinMergedPhotoList, ms_pMetadataForResavingToCloud, CPhotoBuffer::GetDefaultSizeOfJsonBuffer(), true))
		{
			photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - RESAVE_CLOUD_METADATA_WRITE_NEW_METADATA_FOR_CLOUD_FILE - WriteMetadataToString() succeeded. About to begin updating UGC metadata");
			s_ResaveMetadataProgress = RESAVE_CLOUD_METADATA_BEGIN_UPDATE_UGC_METADATA;
		}
		else
		{
			photoAssertf(0, "CPhotoManager::ResaveMetadataForGalleryPhoto - RESAVE_CLOUD_METADATA_WRITE_NEW_METADATA_FOR_CLOUD_FILE - WriteMetadataToString() failed");
			returnValue = MEM_CARD_ERROR;
		}
		break;

	case RESAVE_CLOUD_METADATA_BEGIN_UPDATE_UGC_METADATA :
		{
			returnValue = MEM_CARD_ERROR;

			const char *pContentID = ms_MergedListOfPhotos.GetContentIdOfPhotoOnCloud(indexWithinMergedPhotoList);
			const char *pTitle = ms_MergedListOfPhotos.GetTitleOfPhoto(indexWithinMergedPhotoList);
			const char *pDescription = ms_MergedListOfPhotos.GetDescriptionOfPhoto(indexWithinMergedPhotoList);
			const char *pMetadata = ms_pMetadataForResavingToCloud;

			rlUgc::SourceFile aUgcFile[1];

			if (photoVerifyf(UserContentManager::GetInstance().UpdateContent(pContentID, 
				pTitle, 
				pDescription,
				pMetadata,     //szDataJson
				aUgcFile,      //szRelPaths
				0,             //numRelPaths
				NULL,          //szTags
				RLUGC_CONTENT_TYPE_GTA5PHOTO), 
				"CPhotoManager::ResaveMetadataForGalleryPhoto - RESAVE_CLOUD_METADATA_BEGIN_UPDATE_UGC_METADATA - UpdateContent for UGC metadata has failed to start"))
			{
				photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - RESAVE_CLOUD_METADATA_BEGIN_UPDATE_UGC_METADATA - UpdateContent for UGC metadata has been started");
				s_ResaveMetadataProgress = RESAVE_CLOUD_METADATA_CHECK_UPDATE_UGC_METADATA;
				returnValue = MEM_CARD_BUSY;
			}
		}
		break;

	case RESAVE_CLOUD_METADATA_CHECK_UPDATE_UGC_METADATA :
		returnValue = MEM_CARD_BUSY;
		if (UserContentManager::GetInstance().HasModifyFinished())
		{
			if (UserContentManager::GetInstance().DidModifySucceed())
			{
				photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - RESAVE_CLOUD_METADATA_CHECK_UPDATE_UGC_METADATA - succeeded");
				returnValue = MEM_CARD_COMPLETE;
			}
			else
			{
				photoDisplayf("CPhotoManager::ResaveMetadataForGalleryPhoto - RESAVE_CLOUD_METADATA_CHECK_UPDATE_UGC_METADATA - failed");
				returnValue = MEM_CARD_ERROR;
			}
		}
		break;
	}

	if ( (returnValue == MEM_CARD_ERROR) || (returnValue == MEM_CARD_COMPLETE) )
	{
		// Cloud photo
		if (ms_pMetadataForResavingToCloud)
		{
			delete [] ms_pMetadataForResavingToCloud;
			ms_pMetadataForResavingToCloud = NULL;
		}
	}

	return returnValue;
}




// ***** CreateSortedListOfPhotos

MemoryCardError CPhotoManager::CreateSortedListOfPhotos(bool bFirstCall, bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens)
{
	enum eCreateSortedListProgress
	{
		CREATE_SORTED_LIST_BEGIN,
		CREATE_SORTED_LIST_CHECK
	};

	static eCreateSortedListProgress s_CreateSortedListProgress = CREATE_SORTED_LIST_BEGIN;

	if (bFirstCall)
	{
		SetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen(false);
		s_CreateSortedListProgress = CREATE_SORTED_LIST_BEGIN;
	}

	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (s_CreateSortedListProgress)
	{
	case CREATE_SORTED_LIST_BEGIN :
		{
			if (photoVerifyf(ms_CreateSortedListOfPhotosQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CPhotoManager::CreateSortedListOfPhotos - can't queue a CreateSortedListOfPhotos operation while one is already in progress"))
			{
				if (bOnlyGetNumberOfPhotos)
				{
					if (ms_MergedListOfPhotos.GetNumberOfPhotosIsUpToDate())
					{
						photoDisplayf("CPhotoManager::CreateSortedListOfPhotos - number of photos is up to date so return COMPLETE without sending a cloud query");
						return MEM_CARD_COMPLETE;
					}
				}
				else
				{
					if (ms_MergedListOfPhotos.GetFullListOfMetadataIsUpToDate())
					{
						photoDisplayf("CPhotoManager::CreateSortedListOfPhotos - merged list is up to date so return COMPLETE without sending a cloud query");
						return MEM_CARD_COMPLETE;
					}
				}

				ms_CreateSortedListOfPhotosQueuedOperation.Init(bOnlyGetNumberOfPhotos, bDisplayWarningScreens);
				if (photoVerifyf(CGenericGameStorage::PushOnToSavegameQueue(&ms_CreateSortedListOfPhotosQueuedOperation), "CPhotoManager::CreateSortedListOfPhotos - failed to queue the CreateSortedListOfPhotos operation"))
				{
					s_CreateSortedListProgress = CREATE_SORTED_LIST_CHECK;
				}
			}
		}
		break;

	case CREATE_SORTED_LIST_CHECK :
		returnValue = ms_CreateSortedListOfPhotosQueuedOperation.GetStatus();
		break;
	}

	return returnValue;
}

// ***** End of CreateSortedListOfPhotos


void CPhotoManager::GetNameOfPhotoTextureDictionary(const CUndeletedEntryInMergedPhotoList &undeletedEntry, char *pTextureName, u32 LengthOfTextureName)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetNameOfPhotoTextureDictionary - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(mergedIndex, pTextureName, LengthOfTextureName);
	}
	else
	{
		safecpy(pTextureName, "", LengthOfTextureName);
	}
}

//	Metadata accessors

CSavegamePhotoMetadata const * CPhotoManager::GetMetadataForPhoto( const CUndeletedEntryInMergedPhotoList &undeletedEntry )
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetMetadataForPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetMetadataForPhoto(mergedIndex);
	}

	return 0;
}

const char *CPhotoManager::GetTitleOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetTitleOfPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetTitleOfPhoto(mergedIndex);
	}

	return "";
}

void CPhotoManager::SetTitleOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry, const char *pTitle)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::SetTitleOfPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		const char *pCurrentTitle = ms_MergedListOfPhotos.GetTitleOfPhoto(mergedIndex);

		if (strcmp(pCurrentTitle, pTitle))
		{
			RequestResaveMetadataForGalleryPhoto(mergedIndex);
		}

		ms_MergedListOfPhotos.SetTitleOfPhoto(mergedIndex, pTitle);
	}
}

const char *CPhotoManager::GetDescriptionOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetDescriptionOfPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetDescriptionOfPhoto(mergedIndex);
	}

	return "";
}

/*
void CPhotoManager::SetDescriptionOfPhoto(const CUndeletedEntryInMergedPhotoList &undeletedEntry, const char *pDescription)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::SetDescriptionOfPhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		const char *pCurrentDescription = ms_MergedListOfPhotos.GetDescriptionOfPhoto(mergedIndex);

		if (strcmp(pCurrentDescription, pDescription))
		{
			RequestResaveMetadataForGalleryPhoto(mergedIndex);
		}

		ms_MergedListOfPhotos.SetDescriptionOfPhoto(mergedIndex, pDescription);
	}
}
*/

/*
bool CPhotoManager::GetIsPhotoBookmarked(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetIsPhotoBookmarked - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetIsPhotoBookmarked(mergedIndex);
	}

	return false;
}

void CPhotoManager::SetIsPhotoBookmarked(const CUndeletedEntryInMergedPhotoList &undeletedEntry, bool bBookmarked)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::SetIsPhotoBookmarked - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		if (bBookmarked != ms_MergedListOfPhotos.GetIsPhotoBookmarked(mergedIndex))
		{
			RequestResaveMetadataForGalleryPhoto(mergedIndex);
		}

		ms_MergedListOfPhotos.SetIsPhotoBookmarked(mergedIndex, bBookmarked);
	}
}
*/

void CPhotoManager::GetPhotoLocation(const CUndeletedEntryInMergedPhotoList &undeletedEntry, Vector3& vLocation)
{
	vLocation.Zero();

	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetPhotoLocation - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		ms_MergedListOfPhotos.GetPhotoLocation(mergedIndex, vLocation);
	}
}

const char *CPhotoManager::GetSongTitle(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetSongTitle - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetSongTitle(mergedIndex);
	}

	return "";
}

const char *CPhotoManager::GetSongArtist(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetSongArtist - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		return ms_MergedListOfPhotos.GetSongArtist(mergedIndex);
	}

	return "";
}



bool CPhotoManager::GetCreationDate(const CUndeletedEntryInMergedPhotoList &undeletedEntry, CDate &returnDate)
{
	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	u64 creationDate = CPhotoManager::ms_InvalidDate;

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetCreationDate - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		creationDate = ms_MergedListOfPhotos.GetCreationDate(mergedIndex);
	}

	if (creationDate == CPhotoManager::ms_InvalidDate)
	{
		returnDate.Initialise();
		return false;
	}

	time_t t = static_cast<time_t>(creationDate);
	struct tm tInfo = *localtime(&t);		//	*gmtime(&t);

	returnDate.Set(tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec,
		tInfo.tm_mday, (tInfo.tm_mon + 1), (1900 + tInfo.tm_year) );

	return true;
}

bool CPhotoManager::GetIsMemePhoto( const CUndeletedEntryInMergedPhotoList &undeletedEntry )
{
	bool isMeme = false;

	CIndexWithinMergedPhotoList mergedIndex = ms_MergedListOfPhotos.ConvertUndeletedEntryToIndex(undeletedEntry);

	if (photoVerifyf(mergedIndex.IsValid(), "CPhotoManager::GetIsMemePhoto - failed to convert undeleted entry %d to a valid index", undeletedEntry.GetIntegerValue()))
	{
		isMeme = ms_MergedListOfPhotos.GetIsMemePhoto(mergedIndex);
	}

	return isMeme;
}

#endif	//	__LOAD_LOCAL_PHOTOS


