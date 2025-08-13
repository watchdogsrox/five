
#ifndef SAVEGAME_PHOTO_CLOUD_LIST_H
#define SAVEGAME_PHOTO_CLOUD_LIST_H

// Rage headers
#include "atl/map.h"
#include "atl/string.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "rline/ugc/rlugc.h"

// Game headers
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_photo_metadata.h"
#include "SaveLoad/savegame_photo_unique_id.h"


#define MAX_PHOTOS_TO_LOAD_FROM_CLOUD (96)

class CUGCPhotoOnCloud
{
public:
	void Initialise();

	void Set(const char *pContentId, const char *pTitle, const char *pDescription, const u64 &saveTime, const char *pMetadata, int fileID, int fileVersion, const rlScLanguage &language);
	void Clear();

	const char *GetTitle() { return m_TitleString.c_str(); }
	const char *GetDescription() { return m_DescriptionString.c_str(); }
	const char *GetMetadata() { return m_MetadataString.c_str(); }

	u64 &GetSaveTime() { return m_SaveTime; }

	const char *GetContentId() { return m_ContentIdString.c_str(); }
	int GetFileID() { return m_FileID; }
	int GetFileVersion() { return m_FileVersion; }
	const rlScLanguage &GetLanguage() { return m_Language; }

#if !__LOAD_LOCAL_PHOTOS
	void SetHasBeenAddedToMergedList(bool bHasBeenAdded) { m_bHasBeenAddedToMergedList = bHasBeenAdded; }
	bool GetHasBeenAddedToMergedList() { return m_bHasBeenAddedToMergedList; }
#endif	//	!__LOAD_LOCAL_PHOTOS

private:
	//	char m_ContentId[RLUGC_MAX_CONTENTID_CHARS];
	atString m_ContentIdString;
	u64 m_SaveTime;

	atString m_TitleString;
	atString m_DescriptionString;
	atString m_MetadataString;

	int m_FileID;
	int m_FileVersion;
	rlScLanguage m_Language;

#if !__LOAD_LOCAL_PHOTOS
	bool m_bHasBeenAddedToMergedList;
#endif	//	!__LOAD_LOCAL_PHOTOS
};


class CListOfUGCPhotosOnCloud
{
public:
	CListOfUGCPhotosOnCloud();

	//	virtual 
	~CListOfUGCPhotosOnCloud() {}

	bool RequestListOfDirectoryContents(bool bOnlyGetNumberOfPhotos);

	void Update();

	bool GetIsRequestPending(bool bOnlyGetNumberOfPhotos);
	bool GetHasRequestSucceeded(bool bOnlyGetNumberOfPhotos);

	void TidyUpAfterRequest();

	u32 GetNumberOfPhotos();

	const char *GetContentIdOfPhotoOnCloud(CIndexOfPhotoOnCloud indexWithinArrayOfCloudPhotos);

	//	*************** Functions for adding cloud photos to merged list ***************************************
#if !__LOAD_LOCAL_PHOTOS
	void ClearMergedFlagsOfAllCloudPhotos();

#if __BANK
	void SetMergedFlagsOfAllCloudPhotos();
#endif	//	__BANK

	s32 GetIndexOfMostRecentCloudPhoto();

	void SetMergedFlagOfCloudPhoto(CIndexOfPhotoOnCloud photoArrayIndex);
#endif	//	!__LOAD_LOCAL_PHOTOS

	//	CDate &GetSaveTimeOfCloudPhoto(CIndexOfPhotoOnCloud photoArrayIndex);

	const char *GetTitle(CIndexOfPhotoOnCloud photoArrayIndex);
	const char *GetDescription(CIndexOfPhotoOnCloud photoArrayIndex);
	const char *GetMetadata(CIndexOfPhotoOnCloud photoArrayIndex);
	const u64 &GetCreationDate(CIndexOfPhotoOnCloud photoArrayIndex);
	int GetFileID(CIndexOfPhotoOnCloud photoArrayIndex);
	int GetFileVersion(CIndexOfPhotoOnCloud photoArrayIndex);
	const rlScLanguage &GetLanguage(CIndexOfPhotoOnCloud photoArrayIndex);

	//	*************** Functions for adding cloud photos to merged list ***************************************

private:

	// query delegate callback
	void OnContentResult(const int nIndex,
		const char* szContentID,
		const rlUgcMetadata* pMetadata,
		const rlUgcRatings* pRatings,
		const char* szStatsJSON,
		const unsigned nPlayers,
		const unsigned nPlayerIndex,
		const rlUgcPlayer* pPlayer);

	// Private data
	enum eUgcPhotoOp
	{
		eUgcPhotoOp_Idle,
		eUgcPhotoOp_Pending,
		eUgcPhotoOp_Finished
	};

	CUGCPhotoOnCloud m_ArrayOfCloudPhotos[MAX_PHOTOS_TO_LOAD_FROM_CLOUD];
	u32 m_NumberOfCloudPhotosInArray;

	rlUgc::QueryContentDelegate m_QueryPhotoDelegate;
	int m_nContentTotal;
	netStatus m_QueryPhotoStatus;
	eUgcPhotoOp m_QueryPhotoOp;

	bool m_bOnlyGetNumberOfPhotos;

	static const u64 ms_InvalidDate;
	static const rlScLanguage ms_UnknownLanguage;
};




#if __LOAD_LOCAL_PHOTOS

//	Based on CMergedListEntry
class CDataForOneCloudPhoto
{
public:
	void Set(const char *pContentId, const char *pTitle, const char *pMetadata, const u64 &creationDate);
	void Clear();

	//	Metadata accessors
	void SetTitle(const char *pTitle) { m_Title = pTitle; }

	const char *GetContentId() { return m_ContentId.c_str(); }

	bool GetHasValidMetadata() { return m_bHasValidMetadata; }

	const CSavegamePhotoUniqueId &GetUniqueId() const { return m_Metadata.GetUniqueId(); }

	const u64 &GetCreationDate() const { return m_CreationDate; }

private:
	CSavegamePhotoMetadata m_Metadata;

	u64 m_CreationDate;

	atString m_Title;
	atString m_ContentId;

	bool		m_bHasValidMetadata;
};


//	Based on CMergedListOfPhotos
class CSavegamePhotoCloudList
{
public:
	static void Init(unsigned initMode);

	static void UpdateCloudDirectoryRequest();

	static void SetListIsOutOfDate() { ms_bListIsUpToDate = false; }
	static bool GetListIsUpToDate() { return ms_bListIsUpToDate; }

	static void FillListWithCloudDirectoryContents();

	static bool RequestListOfCloudDirectoryContents();
	static bool GetIsCloudDirectoryRequestPending();
	static bool GetHasCloudDirectoryRequestSucceeded();

	static u32 GetNumberOfPhotos() { return ms_NumberOfCloudPhotos; }

	static void RemovePhotoFromList(const CSavegamePhotoUniqueId &photoUniqueId);

	//	Metadata accessors
	static void SetTitleOfPhoto(const CSavegamePhotoUniqueId &photoUniqueId, const char *pTitle);

	static bool GetHasValidMetadata(const CSavegamePhotoUniqueId &photoUniqueId);

	static const char *GetContentIdOfPhotoOnCloud(const CSavegamePhotoUniqueId &photoUniqueId);

	static bool DoesCloudPhotoExistWithThisUniqueId(const CSavegamePhotoUniqueId &photoUniqueId);

	static void AddToArray(const char *pContentId, const char *pTitle, const char *pMetadata, const u64 &creationDate);

	static CSavegamePhotoUniqueId GetUniqueIdOfOldestPhoto();

private:
	//	Private functions
	static bool GetArrayIndexFromUniqueId(const CSavegamePhotoUniqueId &photoUniqueId, u32 &returnArrayIndex, const char *pDebugString);
private:
	//	Private Data
	static CListOfUGCPhotosOnCloud ms_ListOfPhotosOnCloud;	//Doesn't have to be a pointer any more. There's no cloud listener now.

	static CDataForOneCloudPhoto ms_ListOfCloudPhotos[MAX_PHOTOS_TO_LOAD_FROM_CLOUD];
	static u32 ms_NumberOfCloudPhotos;
	static bool ms_bListIsUpToDate;

	typedef atMap<int, u32> MapOfUniqueIdToArrayIndex;
	static MapOfUniqueIdToArrayIndex ms_MapOfUniqueIdToArrayIndex;
};

#endif	//	__LOAD_LOCAL_PHOTOS

#endif	//	SAVEGAME_PHOTO_CLOUD_LIST_H


