
#include "SaveLoad/savegame_defines.h"

#if !__LOAD_LOCAL_PHOTOS


#include "SaveLoad/savegame_photo_merged_list.h"


// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/savegame_channel.h"

const u64 CMergedListOfPhotos::ms_InvalidDate = 0;
const rlScLanguage CMergedListOfPhotos::ms_UnknownLanguage = RLSC_LANGUAGE_UNKNOWN;

//	*************** CMergedListOfPhotos::CMergedListEntry **************************************************************

void CMergedListOfPhotos::CMergedListEntry::Set(ePhotoSource photoSource, s32 ArrayIndex, const char *pContentId, const char *pTitle, const char *pDescription, const char *pMetadata, const u64 &creationDate, int fileID, int fileVersion, const rlScLanguage &language)
{
	m_PhotoSource = photoSource;
	m_ArrayIndex = ArrayIndex;
	m_bHasBeenDeleted = false;

	m_Title = pTitle;
	m_Description = pDescription;

	m_bHasValidMetadata = m_Metadata.ReadMetadataFromString(pMetadata);
	m_ContentId = pContentId;
    m_FileID = fileID;
    m_FileVersion = fileVersion;
    m_Language = language;

	m_CreationDate = creationDate;
}


bool CMergedListOfPhotos::CMergedListEntry::ReadMetadataFromString(const char* pJsonString, u32 UNUSED_PARAM(LengthOfJsonString))
{
	return m_Metadata.ReadMetadataFromString(pJsonString);
}

bool CMergedListOfPhotos::CMergedListEntry::WriteMetadataToString(char *pJsonString, u32 MaxLengthOfJsonString, bool bDebugLessData)
{
	return m_Metadata.WriteMetadataToString(pJsonString, MaxLengthOfJsonString, bDebugLessData);
}


#if __BANK
//	Add the lengths of all atStrings to the sizeof(CMergedListEntry). Also call m_Metadata.GetSize()
u32 CMergedListOfPhotos::CMergedListEntry::GetSize()
{
	u32 returnSize = sizeof(m_PhotoSource);
	returnSize += sizeof(m_ArrayIndex);
	returnSize += sizeof(m_bHasBeenDeleted);

	returnSize += sizeof(m_ContentId);
	returnSize += m_ContentId.GetLength();

    returnSize += sizeof(m_FileID);
    returnSize += sizeof(m_FileVersion);
    returnSize += sizeof(m_Language);
    
    returnSize += sizeof(m_Title);
	returnSize += m_Title.GetLength();

	returnSize += sizeof(m_Description);
	returnSize += m_Description.GetLength();

	returnSize += sizeof(m_bHasValidMetadata);
	returnSize += sizeof(m_CreationDate);

	returnSize += m_Metadata.GetSize();

	return returnSize;
}
#endif	//	__BANK

//	*************** CMergedListOfPhotos **************************************************************

void CMergedListOfPhotos::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		m_pListOfPhotosOnCloud = rage_new CListOfUGCPhotosOnCloud;
		photoAssertf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::Init - failed to create m_pListOfPhotosOnCloud");
	}

	m_NumberOfMergedListEntries = 0;
	m_NumberOfDeletedPhotos = 0;
	m_bNumberOfPhotosIsUpToDate = false;
	m_bFullListOfMetadataIsUpToDate = false;
}

void CMergedListOfPhotos::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		delete m_pListOfPhotosOnCloud;
		m_pListOfPhotosOnCloud = NULL;
	}
}

void CMergedListOfPhotos::SetNumberOfUGCPhotos()
{
	m_NumberOfMergedListEntries = 0;
	m_NumberOfDeletedPhotos = 0;
	m_bNumberOfPhotosIsUpToDate = false;
	m_bFullListOfMetadataIsUpToDate = false;

	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::SetNumberOfUGCPhotos - m_pListOfPhotosOnCloud is NULL"))
	{
		m_NumberOfMergedListEntries = m_pListOfPhotosOnCloud->GetNumberOfPhotos();

		photoDisplayf("CMergedListOfPhotos::SetNumberOfUGCPhotos has finished so set m_bNumberOfPhotosIsUpToDate to true, but leave m_bFullListOfMetadataIsUpToDate as false");
		m_bNumberOfPhotosIsUpToDate = true;
	}

	photoDisplayf("CMergedListOfPhotos::SetNumberOfUGCPhotos - m_NumberOfMergedListEntries = %u", m_NumberOfMergedListEntries);
}

void CMergedListOfPhotos::FillList()
{
	m_NumberOfMergedListEntries = 0;
	m_NumberOfDeletedPhotos = 0;
	m_bNumberOfPhotosIsUpToDate = false;
	m_bFullListOfMetadataIsUpToDate = false;

	bool bNoEntriesLeftToAdd = false;

	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::FillList - m_pListOfPhotosOnCloud is NULL"))
	{
		m_pListOfPhotosOnCloud->ClearMergedFlagsOfAllCloudPhotos();
	}

//	Only have to deal with cloud photos
	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::FillList - m_pListOfPhotosOnCloud is NULL"))
	{
		while ( (m_NumberOfMergedListEntries < MAX_PHOTOS_TO_LIST) && !bNoEntriesLeftToAdd)
		{
			s32 mostRecentCloudPhoto = m_pListOfPhotosOnCloud->GetIndexOfMostRecentCloudPhoto();
			if (mostRecentCloudPhoto == -1)
			{
				bNoEntriesLeftToAdd = true;
			}
			else
			{
				m_pListOfPhotosOnCloud->SetMergedFlagOfCloudPhoto(CIndexOfPhotoOnCloud(mostRecentCloudPhoto));

				AddToMergedArray(PHOTO_SOURCE_CLOUD, mostRecentCloudPhoto, 
					m_pListOfPhotosOnCloud->GetContentIdOfPhotoOnCloud(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
					m_pListOfPhotosOnCloud->GetTitle(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
					m_pListOfPhotosOnCloud->GetDescription(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
					m_pListOfPhotosOnCloud->GetMetadata(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
                    m_pListOfPhotosOnCloud->GetCreationDate(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
                    m_pListOfPhotosOnCloud->GetFileID(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
                    m_pListOfPhotosOnCloud->GetFileVersion(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)),
                    m_pListOfPhotosOnCloud->GetLanguage(CIndexOfPhotoOnCloud(mostRecentCloudPhoto)));
			}
		}

		m_pListOfPhotosOnCloud->TidyUpAfterRequest();

		photoDisplayf("CMergedListOfPhotos::FillList has finished so set m_bNumberOfPhotosIsUpToDate and m_bFullListOfMetadataIsUpToDate to true");
		m_bNumberOfPhotosIsUpToDate = true;
		m_bFullListOfMetadataIsUpToDate = true;

#if __BANK
		DisplaySize();
#endif	//	__BANK
	}

#if !__NO_OUTPUT
	photoDisplayf("CMergedListOfPhotos::FillList - m_NumberOfMergedListEntries = %u", m_NumberOfMergedListEntries);
	for (u32 loop = 0; loop < m_NumberOfMergedListEntries; loop++)
	{
		const char *pPhotoSourceString = "";
		switch (m_MergedList[loop].GetPhotoSource())
		{
		case PHOTO_SOURCE_NONE :
			pPhotoSourceString = "PHOTO_SOURCE_NONE";
			break;

		case PHOTO_SOURCE_CLOUD :
			pPhotoSourceString = "PHOTO_SOURCE_CLOUD";
			break;
		}
		photoDisplayf("CMergedListOfPhotos::FillList - %u Source=%s Index=%d", loop, pPhotoSourceString, m_MergedList[loop].GetArrayIndex());
	}
#endif	//	!__NO_OUTPUT
}

#if __BANK
void CMergedListOfPhotos::DisplaySize()
{
	u32 totalSize = 0;
	for (u32 loop = 0; loop < MAX_PHOTOS_TO_LIST; loop++)
	{
//		photoDisplayf("CMergedListOfPhotos::DisplaySize - size of entry %u = %u", loop, m_MergedList[loop].GetSize());
		totalSize += m_MergedList[loop].GetSize();
	}

	photoDisplayf("CMergedListOfPhotos::DisplaySize - m_NumberOfMergedListEntries = %u", m_NumberOfMergedListEntries);
	photoDisplayf("CMergedListOfPhotos::DisplaySize - m_NumberOfDeletedPhotos = %u", m_NumberOfDeletedPhotos);
	photoDisplayf("CMergedListOfPhotos::DisplaySize - total size of all %d slots = %u", MAX_PHOTOS_TO_LIST, totalSize);
}
#endif	//	__BANK


void CMergedListOfPhotos::AddToMergedArray(ePhotoSource photoSource, s32 arrayIndex, const char *pContentId, const char *pTitle, const char *pDescription, const char *pMetadata, const u64 &creationDate, int fileID, int fileVersion, const rlScLanguage &language)
{
	if (photoVerifyf(m_NumberOfMergedListEntries < MAX_PHOTOS_TO_LIST, "CMergedListOfPhotos::AddToMergedArray - array already contains %d entries", MAX_PHOTOS_TO_LIST))
	{
		m_MergedList[m_NumberOfMergedListEntries].Set(photoSource, arrayIndex, pContentId, pTitle, pDescription, pMetadata, creationDate, fileID, fileVersion, language);
		m_NumberOfMergedListEntries++;
	}
}


bool CMergedListOfPhotos::RequestListOfCloudDirectoryContents(bool bOnlyGetNumberOfPhotos)
{
	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::RequestListOfCloudDirectoryContents - m_pListOfPhotosOnCloud is NULL"))
	{
		return m_pListOfPhotosOnCloud->RequestListOfDirectoryContents(bOnlyGetNumberOfPhotos);
	}

	return false;
}

void CMergedListOfPhotos::UpdateCloudDirectoryRequest()
{
	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::UpdateCloudDirectoryRequest - m_pListOfPhotosOnCloud is NULL"))
	{
		m_pListOfPhotosOnCloud->Update();
	}
}

bool CMergedListOfPhotos::GetIsCloudDirectoryRequestPending(bool bOnlyGetNumberOfPhotos)
{
	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::GetIsCloudDirectoryRequestPending - m_pListOfPhotosOnCloud is NULL"))
	{
		return m_pListOfPhotosOnCloud->GetIsRequestPending(bOnlyGetNumberOfPhotos);
	}

	return false;
}

bool CMergedListOfPhotos::GetHasCloudDirectoryRequestSucceeded(bool bOnlyGetNumberOfPhotos)
{
	if (photoVerifyf(m_pListOfPhotosOnCloud, "CMergedListOfPhotos::GetHasCloudDirectoryRequestSucceeded - m_pListOfPhotosOnCloud is NULL"))
	{
		return m_pListOfPhotosOnCloud->GetHasRequestSucceeded(bOnlyGetNumberOfPhotos);
	}

	return false;
}

u32 CMergedListOfPhotos::GetNumberOfPhotos(bool bDeletedPhotosShouldBeCounted)
{
	if (bDeletedPhotosShouldBeCounted)
	{
		return m_NumberOfMergedListEntries;
	}
	else
	{
		if (photoVerifyf(m_NumberOfMergedListEntries >= m_NumberOfDeletedPhotos, "CMergedListOfPhotos::GetNumberOfPhotos - m_NumberOfDeletedPhotos %u is greater than m_NumberOfMergedListEntries %u", m_NumberOfDeletedPhotos, m_NumberOfMergedListEntries))
		{
			return m_NumberOfMergedListEntries - m_NumberOfDeletedPhotos;
		}
		else
		{
			return 0;
		}
	}
}

CMergedListOfPhotos::ePhotoSource CMergedListOfPhotos::GetPhotoSource(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetPhotoSource - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetPhotoSource - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetPhotoSource();
	}
	}

	return PHOTO_SOURCE_NONE;
}

s32 CMergedListOfPhotos::GetArrayIndex(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetArrayIndex - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetArrayIndex - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetArrayIndex();
	}
	}

	return -1;
}

void CMergedListOfPhotos::SetPhotoHasBeenDeleted(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::SetPhotoHasBeenDeleted - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::SetPhotoHasBeenDeleted - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		if (photoVerifyf(!m_MergedList[integerIndex].GetHasBeenDeleted(), "CMergedListOfPhotos::SetPhotoHasBeenDeleted - photo slot %d has already been marked as deleted", integerIndex))
		{
			m_MergedList[integerIndex].SetHasBeenDeleted(true);
			m_NumberOfDeletedPhotos++;

				//	Maybe I'll need to clear m_bNumberOfPhotosIsUpToDate and m_bFullListOfMetadataIsUpToDate here but for now I'll see if it works without doing that
		}
	}
}
}

bool CMergedListOfPhotos::GetPhotoHasBeenDeleted(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetPhotoHasBeenDeleted - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetPhotoHasBeenDeleted - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetHasBeenDeleted();
	}
	}

	return false;
}


bool CMergedListOfPhotos::ReadMetadataFromString(const CIndexWithinMergedPhotoList &mergedListIndex, const char* pJsonString, u32 LengthOfJsonString)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::ReadMetadataFromString - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::ReadMetadataFromString - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].ReadMetadataFromString(pJsonString, LengthOfJsonString);
	}
	}

	return false;
}

bool CMergedListOfPhotos::WriteMetadataToString(const CIndexWithinMergedPhotoList &mergedListIndex, char *pJsonString, u32 MaxLengthOfJsonString, bool bDebugLessData)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::WriteMetadataToString - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::WriteMetadataToString - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].WriteMetadataToString(pJsonString, MaxLengthOfJsonString, bDebugLessData);
	}
	}

	return false;
}

//	Metadata accessors
CSavegamePhotoMetadata const * CMergedListOfPhotos::GetMetadataForPhoto( const CIndexWithinMergedPhotoList &mergedListIndex )
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetMetadataForPhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
		s32 integerIndex = mergedListIndex.GetIntegerValue();
		if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetMetadataForPhoto - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
		{
			return &m_MergedList[integerIndex].GetMetaData();
		}
	}

	return NULL;
}

const char *CMergedListOfPhotos::GetTitleOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetTitleOfPhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetTitleOfPhoto - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetTitle();
	}
	}

	return "";
}

void CMergedListOfPhotos::SetTitleOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex, const char *pTitle)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::SetTitleOfPhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::SetTitleOfPhoto - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		m_MergedList[integerIndex].SetTitle(pTitle);		
	}
}
}

const char *CMergedListOfPhotos::GetDescriptionOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetDescriptionOfPhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetDescriptionOfPhoto - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetDescription();
	}
	}

	return "";
}

/*
void CMergedListOfPhotos::SetDescriptionOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex, const char *pDescription)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::SetDescriptionOfPhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
		s32 integerIndex = mergedListIndex.GetIntegerValue();
		if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::SetDescriptionOfPhoto - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
		{
			m_MergedList[integerIndex].SetDescription(pDescription);
		}
	}
}
*/

/*
bool CMergedListOfPhotos::GetIsPhotoBookmarked(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetIsPhotoBookmarked - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetIsPhotoBookmarked - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetIsBookmarked();
	}
	}

	return false;
}

void CMergedListOfPhotos::SetIsPhotoBookmarked(const CIndexWithinMergedPhotoList &mergedListIndex, bool bBookmarked)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::SetIsPhotoBookmarked - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::SetIsPhotoBookmarked - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		m_MergedList[integerIndex].SetIsBookmarked(bBookmarked);
	}
}
}
*/

void CMergedListOfPhotos::GetPhotoLocation(const CIndexWithinMergedPhotoList &mergedListIndex, Vector3& vLocation)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetPhotoLocation - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetPhotoLocation - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		m_MergedList[integerIndex].GetPhotoLocation(vLocation);
	}
}
}

const char *CMergedListOfPhotos::GetSongTitle(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetSongTitle - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetSongTitle - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetSongTitle();
	}
	}

	return "";
}

const char *CMergedListOfPhotos::GetSongArtist(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetSongArtist - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetSongArtist - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetSongArtist();
	}
	}

	return "";
}

const u64 &CMergedListOfPhotos::GetCreationDate(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetCreationDate - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetCreationDate - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetCreationDate();
	}
	}

	return CMergedListOfPhotos::ms_InvalidDate;
}

bool CMergedListOfPhotos::GetHasValidMetadata(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetHasValidMetadata - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetHasValidMetadata - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetHasValidMetadata();
	}
	}

	return false;
}

bool CMergedListOfPhotos::GetIsMemePhoto( const CIndexWithinMergedPhotoList &mergedListIndex ) const
{
	bool isMeme = false;

	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetIsMemePhoto - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
		s32 integerIndex = mergedListIndex.GetIntegerValue();
		if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetIsMemePhoto - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
		{
			isMeme = m_MergedList[integerIndex].GetIsMemePhoto();
		}
	}

	return isMeme;
}

const char *CMergedListOfPhotos::GetContentIdOfPhotoOnCloud(const CIndexWithinMergedPhotoList &mergedListIndex)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetContentIdOfPhotoOnCloud - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 integerIndex = mergedListIndex.GetIntegerValue();
	if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetContentIdOfPhotoOnCloud - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
	{
		return m_MergedList[integerIndex].GetContentId();
	}
	}

	return "";
}


int CMergedListOfPhotos::GetFileID(const CIndexWithinMergedPhotoList &mergedListIndex)
{
    if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetFileID - shouldn't be called until the UGC photo metadata has been read from the cloud"))
    {
        s32 integerIndex = mergedListIndex.GetIntegerValue();
        if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetFileID - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
        {
            return m_MergedList[integerIndex].GetFileID();
        }
    }

    return -1;
}

int CMergedListOfPhotos::GetFileVersion(const CIndexWithinMergedPhotoList &mergedListIndex)
{
    if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetFileVersion - shouldn't be called until the UGC photo metadata has been read from the cloud"))
    {
        s32 integerIndex = mergedListIndex.GetIntegerValue();
        if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetFileVersion - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
        {
            return m_MergedList[integerIndex].GetFileVersion();
        }
    }

    return -1;
}

const rlScLanguage &CMergedListOfPhotos::GetLanguage(const CIndexWithinMergedPhotoList &mergedListIndex)
{
    if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::GetLanguage - shouldn't be called until the UGC photo metadata has been read from the cloud"))
    {
        s32 integerIndex = mergedListIndex.GetIntegerValue();
        if (photoVerifyf( (integerIndex >= 0) && (integerIndex < m_NumberOfMergedListEntries), "CMergedListOfPhotos::GetLanguage - photo slot %d is out of range 0 to %u", integerIndex, m_NumberOfMergedListEntries))
        {
            return m_MergedList[integerIndex].GetLanguage();
        }
    }

    return CMergedListOfPhotos::ms_UnknownLanguage;
}

CIndexWithinMergedPhotoList CMergedListOfPhotos::ConvertUndeletedEntryToIndex(const CUndeletedEntryInMergedPhotoList &undeletedEntry)
{
	if (photoVerifyf(m_bFullListOfMetadataIsUpToDate, "CMergedListOfPhotos::ConvertUndeletedEntryToIndex - shouldn't be called until the UGC photo metadata has been read from the cloud"))
	{
	s32 numberOfUndeletedEntriesToCount = undeletedEntry.GetIntegerValue();

	for (s32 loop = 0; loop < m_NumberOfMergedListEntries; loop++)
	{
		if (m_MergedList[loop].GetHasBeenDeleted() == false)
		{
			if (numberOfUndeletedEntriesToCount == 0)
			{
				return CIndexWithinMergedPhotoList(loop, undeletedEntry.GetIsMaximised());
			}
			numberOfUndeletedEntriesToCount--;
		}
	}
	}

	return CIndexWithinMergedPhotoList(-1, false);
}


// #if __BANK
// CUndeletedEntryInMergedPhotoList CMergedListOfPhotos::GetIndexOfFirstPhoto(bool bLocalPhoto)
// {
// 	s32 indexOfUndeletedPhoto = -1;
// 	for (s32 loop = 0; loop < m_NumberOfMergedListEntries; loop++)
// 	{
// 		if (m_MergedList[loop].GetHasBeenDeleted() == false)
// 		{
// 			indexOfUndeletedPhoto++;
// 			ePhotoSource photoSource = m_MergedList[loop].GetPhotoSource();
// 
// 			if (!bLocalPhoto && photoSource == PHOTO_SOURCE_CLOUD)
// 			{
// 				return CUndeletedEntryInMergedPhotoList(indexOfUndeletedPhoto, false);
// 			}
// 		}
// 	}
// 
// 	return CUndeletedEntryInMergedPhotoList(-1, false);
// }
// #endif	//	__BANK
// 



#endif	//	!__LOAD_LOCAL_PHOTOS

