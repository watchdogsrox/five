
#include "SaveLoad/savegame_photo_cloud_list.h"

// Rage headers
#include "rline/rl.h"

// Game headers
#include "Network/Cloud/UserContentManager.h"
#include "Network/NetworkInterface.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_cloud_deleter.h"
#include "SaveLoad/savegame_photo_local_list.h"




const u64 CListOfUGCPhotosOnCloud::ms_InvalidDate = 0;
const rlScLanguage CListOfUGCPhotosOnCloud::ms_UnknownLanguage = RLSC_LANGUAGE_UNKNOWN;


//	*************** CUGCPhotoOnCloud **************************************************************

void CUGCPhotoOnCloud::Initialise()
{
	m_SaveTime = 0;
#if !__LOAD_LOCAL_PHOTOS
	m_bHasBeenAddedToMergedList = false;
#endif	//	!__LOAD_LOCAL_PHOTOS
}

void CUGCPhotoOnCloud::Set(const char *pContentId, const char *pTitle, const char *pDescription, const u64 &saveTime, const char *pMetadata, int fileID, int fileVersion, const rlScLanguage &language)
{
	m_ContentIdString = pContentId;
	m_TitleString = pTitle;
	m_DescriptionString = pDescription;
	m_SaveTime = saveTime;
	m_MetadataString = pMetadata;
    m_FileID = fileID;
    m_FileVersion = fileVersion;
    m_Language = language;
#if !__LOAD_LOCAL_PHOTOS
	m_bHasBeenAddedToMergedList = false;	//	Is it safe to reset this here?
#endif	//	!__LOAD_LOCAL_PHOTOS
}

void CUGCPhotoOnCloud::Clear()
{
	m_ContentIdString.Clear();
	m_SaveTime = 0;

	m_TitleString.Clear();
	m_DescriptionString.Clear();
	m_MetadataString.Clear();

    m_FileID = -1;
    m_FileVersion = -1; 
    m_Language = RLSC_LANGUAGE_UNKNOWN;

#if !__LOAD_LOCAL_PHOTOS
	m_bHasBeenAddedToMergedList = false;
#endif	//	!__LOAD_LOCAL_PHOTOS
}


//	*************** CListOfUGCPhotosOnCloud **************************************************************

CListOfUGCPhotosOnCloud::CListOfUGCPhotosOnCloud()
{
	// bind query delegate
	m_QueryPhotoDelegate.Bind(this, &CListOfUGCPhotosOnCloud::OnContentResult);

	// reset the status member
	m_QueryPhotoStatus.Reset();

	for (u32 loop = 0; loop < MAX_PHOTOS_TO_LOAD_FROM_CLOUD; loop++)
	{
		m_ArrayOfCloudPhotos[loop].Initialise();
	}
	m_NumberOfCloudPhotosInArray = 0;

	m_nContentTotal = 0;

	m_bOnlyGetNumberOfPhotos = false;
}


void CListOfUGCPhotosOnCloud::Update()
{
	if(m_QueryPhotoOp == eUgcPhotoOp_Pending && !m_QueryPhotoStatus.Pending())
	{
		// query completed
		photoDisplayf("CListOfUGCPhotosOnCloud::Update - Request list of photos has %s", GetHasRequestSucceeded(m_bOnlyGetNumberOfPhotos) ? "succeeded" : "failed");

		if(GetHasRequestSucceeded(m_bOnlyGetNumberOfPhotos))
		{
			if (m_bOnlyGetNumberOfPhotos)
			{
				m_NumberOfCloudPhotosInArray = m_nContentTotal;
				if (m_NumberOfCloudPhotosInArray > MAX_PHOTOS_TO_LOAD_FROM_CLOUD)
				{
					m_NumberOfCloudPhotosInArray = MAX_PHOTOS_TO_LOAD_FROM_CLOUD;
				}
			}
			else
			{
				photoAssertf(static_cast<unsigned>(m_nContentTotal) >= m_NumberOfCloudPhotosInArray, "CListOfUGCPhotosOnCloud::Update - Request list of photos has succeeded. Total %d < Num %d", m_nContentTotal, m_NumberOfCloudPhotosInArray);
			}

			photoDisplayf("CListOfUGCPhotosOnCloud::Update - m_NumberOfCloudPhotosInArray is %u", m_NumberOfCloudPhotosInArray);
		}


		m_QueryPhotoOp = eUgcPhotoOp_Finished;
	}
}


void CListOfUGCPhotosOnCloud::OnContentResult(const int UNUSED_PARAM(nIndex),
											  const char* szContentID,
											  const rlUgcMetadata* pMetadata,
											  const rlUgcRatings* UNUSED_PARAM(pRatings),
											  const char* UNUSED_PARAM(szStatsJSON),
											  const unsigned UNUSED_PARAM(nPlayers),
											  const unsigned UNUSED_PARAM(nPlayerIndex),
											  const rlUgcPlayer* UNUSED_PARAM(pPlayer))
{
	photoAssertf(!m_bOnlyGetNumberOfPhotos, "CListOfUGCPhotosOnCloud::OnContentResult - didn't expect to get here if m_bOnlyGetNumberOfPhotos is true");

	if (m_NumberOfCloudPhotosInArray < MAX_PHOTOS_TO_LOAD_FROM_CLOUD)
	{
		if (photoVerifyf(pMetadata, "CListOfUGCPhotosOnCloud::OnContentResult - pMetadata is NULL"))
		{
			const char *pContentId = szContentID;
			const char *pTitle = pMetadata->GetContentName();
			const char *pDescription = pMetadata->GetDescription();
			u64 creation_date = pMetadata->GetCreatedDate();

            int fileID = pMetadata->GetFileId(0);
            int fileVersion = pMetadata->GetFileVersion(0);
            rlScLanguage language = pMetadata->GetLanguage();

			const char *pDataJSON = pMetadata->GetData();

			char pPathToJpeg[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
			if (photoVerifyf(pMetadata->ComposeCloudAbsPath(0, pPathToJpeg, sizeof(pPathToJpeg)), "Failed to compose cloud abs path to UGC photo"))
			{
				m_ArrayOfCloudPhotos[m_NumberOfCloudPhotosInArray].Set(pContentId, pTitle, pDescription, creation_date, pDataJSON, fileID, fileVersion, language);

				photoDisplayf("CListOfUGCPhotosOnCloud::OnContentResult - Added content. %u Name: %s, ID: %s, Total: %u", m_NumberOfCloudPhotosInArray, pTitle, pContentId, m_nContentTotal);
				m_NumberOfCloudPhotosInArray++;
			}
		}
	}
}


bool CListOfUGCPhotosOnCloud::RequestListOfDirectoryContents(bool bOnlyGetNumberOfPhotos)
{
	// check that cloud is available
	if(!photoVerifyf(NetworkInterface::IsCloudAvailable(), "CListOfUGCPhotosOnCloud::RequestListOfDirectoryContents - Cloud not available!"))
		return false;

	if(!photoVerifyf(m_QueryPhotoOp != eUgcPhotoOp_Pending, "CListOfUGCPhotosOnCloud::RequestListOfDirectoryContents - Operation already in progress!"))
		return false;

	m_bOnlyGetNumberOfPhotos = bOnlyGetNumberOfPhotos;

	m_NumberOfCloudPhotosInArray = 0;
	m_nContentTotal = 0;

	m_QueryPhotoStatus.Reset();
	m_QueryPhotoOp = eUgcPhotoOp_Idle;

	s32 maxResults = MAX_PHOTOS_TO_LOAD_FROM_CLOUD;
	if (m_bOnlyGetNumberOfPhotos)
	{
		maxResults = 0;
	}
	if (UserContentManager::GetInstance().QueryContent(RLUGC_CONTENT_TYPE_GTA5PHOTO, "GetMyContent", NULL, 0, maxResults, &m_nContentTotal, NULL, m_QueryPhotoDelegate, &m_QueryPhotoStatus))
	{
		m_QueryPhotoOp = eUgcPhotoOp_Pending;
		return true;
	}

	return false;
}


bool CListOfUGCPhotosOnCloud::GetIsRequestPending(bool ASSERT_ONLY(bOnlyGetNumberOfPhotos))
{
	photoAssertf(m_bOnlyGetNumberOfPhotos == bOnlyGetNumberOfPhotos, "CListOfUGCPhotosOnCloud::GetIsRequestPending - m_bOnlyGetNumberOfPhotos(%s) doesn't match bOnlyGetNumberOfPhotos(%s)", 
		m_bOnlyGetNumberOfPhotos?"true":"false",
		bOnlyGetNumberOfPhotos?"true":"false");

	return m_QueryPhotoOp == eUgcPhotoOp_Pending;
}


bool CListOfUGCPhotosOnCloud::GetHasRequestSucceeded(bool ASSERT_ONLY(bOnlyGetNumberOfPhotos))
{
	photoAssertf(m_bOnlyGetNumberOfPhotos == bOnlyGetNumberOfPhotos, "CListOfUGCPhotosOnCloud::GetHasRequestSucceeded - m_bOnlyGetNumberOfPhotos(%s) doesn't match bOnlyGetNumberOfPhotos(%s)", 
		m_bOnlyGetNumberOfPhotos?"true":"false",
		bOnlyGetNumberOfPhotos?"true":"false");

	return m_QueryPhotoStatus.Succeeded();
}


void CListOfUGCPhotosOnCloud::TidyUpAfterRequest()
{
	for (u32 loop = 0; loop < MAX_PHOTOS_TO_LOAD_FROM_CLOUD; loop++)
	{
		m_ArrayOfCloudPhotos[loop].Clear();
	}
}

u32 CListOfUGCPhotosOnCloud::GetNumberOfPhotos()
{
	return m_NumberOfCloudPhotosInArray;
}

	//	*************** Functions for adding cloud photos to merged list ***************************************
#if !__LOAD_LOCAL_PHOTOS
void CListOfUGCPhotosOnCloud::ClearMergedFlagsOfAllCloudPhotos()
{
	for (u32 loop = 0; loop < MAX_PHOTOS_TO_LOAD_FROM_CLOUD; loop++)
	{
		m_ArrayOfCloudPhotos[loop].SetHasBeenAddedToMergedList(false);
	}
}

#if __BANK
void CListOfUGCPhotosOnCloud::SetMergedFlagsOfAllCloudPhotos()
{
	for (u32 loop = 0; loop < MAX_PHOTOS_TO_LOAD_FROM_CLOUD; loop++)
	{
		m_ArrayOfCloudPhotos[loop].SetHasBeenAddedToMergedList(true);
	}
}
#endif	//	__BANK

s32 CListOfUGCPhotosOnCloud::GetIndexOfMostRecentCloudPhoto()
{
	s32 indexOfMostRecentSave = -1;
	u64 dateOfMostRecentSave = 0;

	for (s32 loop = 0; loop < m_NumberOfCloudPhotosInArray; loop++)
	{
		if (m_ArrayOfCloudPhotos[loop].GetHasBeenAddedToMergedList() == false)
		{
			u64 saveTimeOfThisSlot = m_ArrayOfCloudPhotos[loop].GetSaveTime();
			if (saveTimeOfThisSlot >= dateOfMostRecentSave)
			{
				indexOfMostRecentSave = loop;
				dateOfMostRecentSave = saveTimeOfThisSlot;
			}
		}
	}

	return indexOfMostRecentSave;
}

void CListOfUGCPhotosOnCloud::SetMergedFlagOfCloudPhoto(CIndexOfPhotoOnCloud photoArrayIndex)
{
	if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::SetMergedFlagOfCloudPhoto - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].SetHasBeenAddedToMergedList(true);
	}
}
#endif	//	!__LOAD_LOCAL_PHOTOS

/*
CDate &CListOfUGCPhotosOnCloud::GetSaveTimeOfCloudPhoto(CIndexOfPhotoOnCloud photoArrayIndex)
{
	if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetSaveTimeOfCloudPhoto - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetSaveTime();
	}

	static CDate InvalidDate;
	InvalidDate.Initialise();

	return InvalidDate;
}
*/

const char *CListOfUGCPhotosOnCloud::GetContentIdOfPhotoOnCloud(CIndexOfPhotoOnCloud indexWithinArrayOfCloudPhotos)
{
	if (photoVerifyf( (indexWithinArrayOfCloudPhotos.GetIntegerValue() >= 0) && (indexWithinArrayOfCloudPhotos.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfUGCPhotosOnCloud::GetContentIdOfPhotoOnCloud - indexWithinArrayOfCloudPhotos %d is out of range 0 to %d", indexWithinArrayOfCloudPhotos.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		return m_ArrayOfCloudPhotos[indexWithinArrayOfCloudPhotos.GetIntegerValue()].GetContentId();
	}

	return "";
}

const char *CListOfUGCPhotosOnCloud::GetTitle(CIndexOfPhotoOnCloud photoArrayIndex)
{
	if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetTitle - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetTitle();
	}

	return "";
}

const char *CListOfUGCPhotosOnCloud::GetDescription(CIndexOfPhotoOnCloud photoArrayIndex)
{
	if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetDescription - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetDescription();
	}

	return "";
}


const char *CListOfUGCPhotosOnCloud::GetMetadata(CIndexOfPhotoOnCloud photoArrayIndex)
{
	if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetMetadata - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetMetadata();
	}

	return "";
}

const u64 &CListOfUGCPhotosOnCloud::GetCreationDate(CIndexOfPhotoOnCloud photoArrayIndex)
{
	if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetCreationDate - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
	{
		return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetSaveTime();
	}

	return CListOfUGCPhotosOnCloud::ms_InvalidDate;
}

int CListOfUGCPhotosOnCloud::GetFileID(CIndexOfPhotoOnCloud photoArrayIndex)
{
    if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetFileID - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
    {
        return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetFileID();
    }

    return -1;
}

int CListOfUGCPhotosOnCloud::GetFileVersion(CIndexOfPhotoOnCloud photoArrayIndex)
{
    if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetFileVersion - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
    {
        return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetFileVersion();
    }

    return -1;
}

const rlScLanguage &CListOfUGCPhotosOnCloud::GetLanguage(CIndexOfPhotoOnCloud photoArrayIndex)
{
    if (photoVerifyf( (photoArrayIndex.GetIntegerValue() >= 0) && (photoArrayIndex.GetIntegerValue() < m_NumberOfCloudPhotosInArray), "CListOfPhotosOnCloud::GetLanguage - arrayIndex %d is outside the range 0 to %u (the number of cloud photos)", photoArrayIndex.GetIntegerValue(), m_NumberOfCloudPhotosInArray))
    {
        return m_ArrayOfCloudPhotos[photoArrayIndex.GetIntegerValue()].GetLanguage();
    }

    return CListOfUGCPhotosOnCloud::ms_UnknownLanguage;
}



#if __LOAD_LOCAL_PHOTOS

CListOfUGCPhotosOnCloud CSavegamePhotoCloudList::ms_ListOfPhotosOnCloud;

CDataForOneCloudPhoto CSavegamePhotoCloudList::ms_ListOfCloudPhotos[MAX_PHOTOS_TO_LOAD_FROM_CLOUD];
u32 CSavegamePhotoCloudList::ms_NumberOfCloudPhotos = 0;
bool CSavegamePhotoCloudList::ms_bListIsUpToDate = false;

CSavegamePhotoCloudList::MapOfUniqueIdToArrayIndex CSavegamePhotoCloudList::ms_MapOfUniqueIdToArrayIndex;


//	*************** CDataForOneCloudPhoto **************************************************************

void CDataForOneCloudPhoto::Set(const char *pContentId, const char *pTitle, const char *pMetadata, const u64 &creationDate)
{
	m_Title = pTitle;

	m_bHasValidMetadata = m_Metadata.ReadMetadataFromString(pMetadata);
	m_ContentId = pContentId;

	m_CreationDate = creationDate;
}

void CDataForOneCloudPhoto::Clear()
{
//	CSavegamePhotoMetadata m_Metadata;		//	CSavegamePhotoMetadata doesn't currently have a Clear function

	m_Title.Clear();
	m_ContentId.Clear();

	m_CreationDate = 0;

	m_bHasValidMetadata = false;
}


//	*************** CSavegamePhotoCloudList **************************************************************

void CSavegamePhotoCloudList::Init(unsigned UNUSED_PARAM(initMode))
{
	ms_NumberOfCloudPhotos = 0;
	ms_bListIsUpToDate = false;

	ms_MapOfUniqueIdToArrayIndex.Reset();
}


void CSavegamePhotoCloudList::FillListWithCloudDirectoryContents()
{
	if (!CSavegamePhotoLocalList::GetListHasBeenCreated())
	{
		photoDisplayf("CSavegamePhotoCloudList::FillListWithCloudDirectoryContents - local list hasn't been created yet so we won't be able to delete any orphaned cloud photos");
		photoAssertf(0, "CSavegamePhotoCloudList::FillListWithCloudDirectoryContents - local list hasn't been created yet so we won't be able to delete any orphaned cloud photos");
	}

	ms_NumberOfCloudPhotos = 0;
	ms_bListIsUpToDate = false;

	ms_MapOfUniqueIdToArrayIndex.Reset();

	const u32 numberOfPhotosReadFromCloud = ms_ListOfPhotosOnCloud.GetNumberOfPhotos();

	for (u32 loop = 0; loop < numberOfPhotosReadFromCloud; loop++)
	{
		AddToArray(ms_ListOfPhotosOnCloud.GetContentIdOfPhotoOnCloud(CIndexOfPhotoOnCloud(loop)),
			ms_ListOfPhotosOnCloud.GetTitle(CIndexOfPhotoOnCloud(loop)),
			ms_ListOfPhotosOnCloud.GetMetadata(CIndexOfPhotoOnCloud(loop)),
			ms_ListOfPhotosOnCloud.GetCreationDate(CIndexOfPhotoOnCloud(loop)));
	}

	ms_ListOfPhotosOnCloud.TidyUpAfterRequest();

	photoDisplayf("CSavegamePhotoCloudList::FillListWithCloudDirectoryContents has finished so set ms_bListIsUpToDate to true");
	ms_bListIsUpToDate = true;

	photoDisplayf("CSavegamePhotoCloudList::FillListWithCloudDirectoryContents - ms_NumberOfCloudPhotos = %u", ms_NumberOfCloudPhotos);
}


void CSavegamePhotoCloudList::AddToArray(const char *pContentId, const char *pTitle, const char *pMetadata, const u64 &creationDate)
{
	if (photoVerifyf(ms_NumberOfCloudPhotos < MAX_PHOTOS_TO_LOAD_FROM_CLOUD, "CSavegamePhotoCloudList::AddToArray - array already contains %d entries", MAX_PHOTOS_TO_LOAD_FROM_CLOUD))
	{
		ms_ListOfCloudPhotos[ms_NumberOfCloudPhotos].Set(pContentId, pTitle, pMetadata, creationDate);

		if (photoVerifyf(ms_ListOfCloudPhotos[ms_NumberOfCloudPhotos].GetHasValidMetadata(), "CSavegamePhotoCloudList::AddToArray - newly added data doesn't have valid metadata"))
		{
			const CSavegamePhotoUniqueId  &uniqueId = ms_ListOfCloudPhotos[ms_NumberOfCloudPhotos].GetUniqueId();
			const int nUniqueId = uniqueId.GetValue();

			if (nUniqueId == 0)
			{
				photoWarningf("CSavegamePhotoCloudList::AddToArray - entry with Content Id %s has unique id 0 so don't add it to the array", pContentId);
				//	 If the following line is uncommented, will it delete all Last Gen photos for this player?
				//	CSavegamePhotoCloudDeleter::AddContentIdToQueue(pContentId);
				ms_ListOfCloudPhotos[ms_NumberOfCloudPhotos].Clear();
			}
			else
			{
				bool bAddToArrayOfCloudPhotos = true;
				if (CSavegamePhotoLocalList::GetListHasBeenCreated())
				{
					if (!CSavegamePhotoLocalList::DoesIdExistInList(&uniqueId))
					{
						photoDisplayf("CSavegamePhotoCloudList::AddToArray - cloud photo has Unique Id %d and Content Id %s - couldn't find a local photo with that Unique Id. I used to delete the cloud photo in this situation", nUniqueId, pContentId);
// 						photoDisplayf("CSavegamePhotoCloudList::AddToArray - cloud photo has Unique Id %d and Content Id %s - couldn't find a local photo with that Unique Id so queue the cloud photo for deletion", nUniqueId, pContentId);
// 						CSavegamePhotoCloudDeleter::AddContentIdToQueue(pContentId);
// 						ms_ListOfCloudPhotos[ms_NumberOfCloudPhotos].Clear();
// 						bAddToArrayOfCloudPhotos = false;
					}
				}


				if (bAddToArrayOfCloudPhotos)
				{
					if (ms_MapOfUniqueIdToArrayIndex.Access(nUniqueId))
					{
						photoAssertf(0, "CSavegamePhotoCloudList::AddToArray - entry with unique id %d already exists in map so I'll queue this cloud photo for deletion (Content Id %s)", nUniqueId, pContentId);
						photoDisplayf("CSavegamePhotoCloudList::AddToArray - entry with unique id %d already exists in map so I'll queue this cloud photo for deletion (Content Id %s)", nUniqueId, pContentId);
						CSavegamePhotoCloudDeleter::AddContentIdToQueue(pContentId);
						ms_ListOfCloudPhotos[ms_NumberOfCloudPhotos].Clear();
						bAddToArrayOfCloudPhotos = false;
					}

					if (bAddToArrayOfCloudPhotos)
					{
						ms_MapOfUniqueIdToArrayIndex.Insert(nUniqueId, ms_NumberOfCloudPhotos);

						photoDisplayf("CSavegamePhotoCloudList::AddToArray - added photo with Content Id %s and Unique Id %d as array element %u", pContentId, nUniqueId, ms_NumberOfCloudPhotos);

						ms_NumberOfCloudPhotos++;
					}
				}
			}
		}
	}
}

bool CSavegamePhotoCloudList::RequestListOfCloudDirectoryContents()
{
	return ms_ListOfPhotosOnCloud.RequestListOfDirectoryContents(false);
}

void CSavegamePhotoCloudList::UpdateCloudDirectoryRequest()
{
	ms_ListOfPhotosOnCloud.Update();
}

bool CSavegamePhotoCloudList::GetIsCloudDirectoryRequestPending()
{
	return ms_ListOfPhotosOnCloud.GetIsRequestPending(false);
}

bool CSavegamePhotoCloudList::GetHasCloudDirectoryRequestSucceeded()
{
	return ms_ListOfPhotosOnCloud.GetHasRequestSucceeded(false);
}


void CSavegamePhotoCloudList::RemovePhotoFromList(const CSavegamePhotoUniqueId &photoUniqueId)
{
	const int nUniqueIdOfPhotoToRemove = photoUniqueId.GetValue();
	const u32 *pArrayIndex = ms_MapOfUniqueIdToArrayIndex.Access(nUniqueIdOfPhotoToRemove);

	if (photoVerifyf(pArrayIndex, "CSavegamePhotoCloudList::RemovePhotoFromList - failed to find unique Id %d in map", nUniqueIdOfPhotoToRemove))
	{
		u32 arrayIndexToRemove = *pArrayIndex;
		if (photoVerifyf(arrayIndexToRemove < ms_NumberOfCloudPhotos, "CSavegamePhotoCloudList::RemovePhotoFromList - arrayIndexToRemove %u is out of range 0 to %u", arrayIndexToRemove, ms_NumberOfCloudPhotos))
		{
			if (arrayIndexToRemove < (ms_NumberOfCloudPhotos - 1))
			{	//	Copy last element in array over the top of the one we're removing
				ms_ListOfCloudPhotos[arrayIndexToRemove] = ms_ListOfCloudPhotos[(ms_NumberOfCloudPhotos - 1)];

				//	Update map for the element that has just been moved
				const u32 arrayIndexToUpdate = arrayIndexToRemove;
				if (photoVerifyf(ms_ListOfCloudPhotos[arrayIndexToUpdate].GetHasValidMetadata(), "CSavegamePhotoCloudList::RemovePhotoFromList - array element that has just been moved doesn't have valid metadata"))
				{
					const CSavegamePhotoUniqueId &uniqueIdOfMovedElement = ms_ListOfCloudPhotos[arrayIndexToUpdate].GetUniqueId();
					const int nUniqueIdOfMovedElement = uniqueIdOfMovedElement.GetValue();

					if (photoVerifyf(ms_MapOfUniqueIdToArrayIndex.Access(nUniqueIdOfMovedElement), "CSavegamePhotoCloudList::RemovePhotoFromList - failed to find unique Id %d of moved element in map", nUniqueIdOfMovedElement))
					{
						ms_MapOfUniqueIdToArrayIndex[nUniqueIdOfMovedElement] = arrayIndexToUpdate;
					}
				}
			}

			ms_NumberOfCloudPhotos--;

			//	Remove photoUniqueId from map
			ms_MapOfUniqueIdToArrayIndex.Delete(nUniqueIdOfPhotoToRemove);
		}
	}
}


bool CSavegamePhotoCloudList::GetArrayIndexFromUniqueId(const CSavegamePhotoUniqueId &photoUniqueId, u32 &returnArrayIndex, const char* ASSERT_ONLY(pDebugString))
{
	returnArrayIndex = 0;
	if (photoVerifyf(ms_bListIsUpToDate, "%s - shouldn't be called until the UGC photo metadata has been read from the cloud", pDebugString))
	{
		const int nPhotoUniqueId = photoUniqueId.GetValue();
		const u32 *pArrayIndex = ms_MapOfUniqueIdToArrayIndex.Access(nPhotoUniqueId);

		if (photoVerifyf(pArrayIndex, "%s - failed to find unique Id %d in map", pDebugString, nPhotoUniqueId))
		{
			u32 arrayIndex = *pArrayIndex;
			if (photoVerifyf(arrayIndex < ms_NumberOfCloudPhotos, "%s - arrayIndex %u is out of range 0 to %u", pDebugString, arrayIndex, ms_NumberOfCloudPhotos))
			{
				returnArrayIndex = arrayIndex;
				return true;
			}
		}
	}

	return false;
}

bool CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId(const CSavegamePhotoUniqueId &photoUniqueId)
{
	if (photoVerifyf(ms_bListIsUpToDate, "CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
		const int nPhotoUniqueId = photoUniqueId.GetValue();
		const u32 *pArrayIndex = ms_MapOfUniqueIdToArrayIndex.Access(nPhotoUniqueId);

		if (pArrayIndex)
		{
			u32 arrayIndex = *pArrayIndex;
			if (photoVerifyf(arrayIndex < ms_NumberOfCloudPhotos, "CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId - arrayIndex %u is out of range 0 to %u", arrayIndex, ms_NumberOfCloudPhotos))
			{
				return true;
			}
		}
	}

	return false;
}


void CSavegamePhotoCloudList::SetTitleOfPhoto(const CSavegamePhotoUniqueId &photoUniqueId, const char *pTitle)
{
	u32 arrayIndex = 0;
	if (GetArrayIndexFromUniqueId(photoUniqueId, arrayIndex, "SetTitleOfPhoto"))
	{
		ms_ListOfCloudPhotos[arrayIndex].SetTitle(pTitle);
	}
}


bool CSavegamePhotoCloudList::GetHasValidMetadata(const CSavegamePhotoUniqueId &photoUniqueId)
{
	u32 arrayIndex = 0;
	if (GetArrayIndexFromUniqueId(photoUniqueId, arrayIndex, "GetHasValidMetadata"))
	{
		return ms_ListOfCloudPhotos[arrayIndex].GetHasValidMetadata();
	}

	return false;
}


const char *CSavegamePhotoCloudList::GetContentIdOfPhotoOnCloud(const CSavegamePhotoUniqueId &photoUniqueId)
{
	u32 arrayIndex = 0;
	if (GetArrayIndexFromUniqueId(photoUniqueId, arrayIndex, "GetContentIdOfPhotoOnCloud"))
	{
		return ms_ListOfCloudPhotos[arrayIndex].GetContentId();
	}

	return "";
}


CSavegamePhotoUniqueId CSavegamePhotoCloudList::GetUniqueIdOfOldestPhoto()
{
	CSavegamePhotoUniqueId uniqueIdOfOldestPhoto;
	u64 dateOfOldestPhoto = rlGetPosixTime();

	if (photoVerifyf(ms_bListIsUpToDate, "CSavegamePhotoCloudList::GetUniqueIdOfOldestPhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
		for (u32 loop = 0; loop < ms_NumberOfCloudPhotos; loop++)
		{
			if (ms_ListOfCloudPhotos[loop].GetCreationDate() < dateOfOldestPhoto)
			{
				uniqueIdOfOldestPhoto = ms_ListOfCloudPhotos[loop].GetUniqueId();
				dateOfOldestPhoto = ms_ListOfCloudPhotos[loop].GetCreationDate();
			}
		}
	}

	return uniqueIdOfOldestPhoto;
}

#endif	//	__LOAD_LOCAL_PHOTOS



