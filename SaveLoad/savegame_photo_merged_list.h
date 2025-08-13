
#ifndef SAVEGAME_PHOTO_MERGED_LIST_H
#define SAVEGAME_PHOTO_MERGED_LIST_H

// Game headers
#include "SaveLoad/savegame_photo_cloud_list.h"
#include "SaveLoad/savegame_photo_metadata.h"


#if !__LOAD_LOCAL_PHOTOS

#define MAX_PHOTOS_TO_LIST (MAX_PHOTOS_TO_LOAD_FROM_CLOUD)



//	*************** CMergedListOfPhotos **************************************************************

class CMergedListOfPhotos
{
public:
	enum ePhotoSource
	{
		PHOTO_SOURCE_NONE,
		PHOTO_SOURCE_CLOUD
	};

	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	void UpdateCloudDirectoryRequest();

	void SetResultsAreOutOfDate() { m_bNumberOfPhotosIsUpToDate = false; m_bFullListOfMetadataIsUpToDate = false; }
	bool GetNumberOfPhotosIsUpToDate() { return m_bNumberOfPhotosIsUpToDate; }
	bool GetFullListOfMetadataIsUpToDate() { return m_bFullListOfMetadataIsUpToDate; }

	void SetNumberOfUGCPhotos();
	void FillList();

	bool RequestListOfCloudDirectoryContents(bool bOnlyGetNumberOfPhotos);
	bool GetIsCloudDirectoryRequestPending(bool bOnlyGetNumberOfPhotos);
	bool GetHasCloudDirectoryRequestSucceeded(bool bOnlyGetNumberOfPhotos);

	u32 GetNumberOfPhotos(bool bDeletedPhotosShouldBeCounted);

	ePhotoSource GetPhotoSource(const CIndexWithinMergedPhotoList &mergedListIndex);
	s32 GetArrayIndex(const CIndexWithinMergedPhotoList &mergedListIndex);

	void SetPhotoHasBeenDeleted(const CIndexWithinMergedPhotoList &mergedListIndex);
	bool GetPhotoHasBeenDeleted(const CIndexWithinMergedPhotoList &mergedListIndex);

	bool ReadMetadataFromString(const CIndexWithinMergedPhotoList &mergedListIndex, const char* pJsonString, u32 LengthOfJsonString);

	bool WriteMetadataToString(const CIndexWithinMergedPhotoList &mergedListIndex, char *pJsonString, u32 MaxLengthOfJsonString, bool bDebugLessData);

	//	Metadata accessors
	CSavegamePhotoMetadata const *GetMetadataForPhoto(const CIndexWithinMergedPhotoList &mergedListIndex);

	const char *GetTitleOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex);
	void SetTitleOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex, const char *pTitle);

	const char *GetDescriptionOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex);
	//	void SetDescriptionOfPhoto(const CIndexWithinMergedPhotoList &mergedListIndex, const char *pDescription);

	//	bool GetIsPhotoBookmarked(const CIndexWithinMergedPhotoList &mergedListIndex);
	//	void SetIsPhotoBookmarked(const CIndexWithinMergedPhotoList &mergedListIndex, bool bBookmarked);

	void GetPhotoLocation(const CIndexWithinMergedPhotoList &mergedListIndex, Vector3& vLocation);

	const char *GetSongTitle(const CIndexWithinMergedPhotoList &mergedListIndex);
	const char *GetSongArtist(const CIndexWithinMergedPhotoList &mergedListIndex);

	const u64 &GetCreationDate(const CIndexWithinMergedPhotoList &mergedListIndex);
	bool GetHasValidMetadata(const CIndexWithinMergedPhotoList &mergedListIndex);
	bool GetIsMemePhoto( const CIndexWithinMergedPhotoList &mergedListIndex ) const;

	const char *GetContentIdOfPhotoOnCloud(const CIndexWithinMergedPhotoList &mergedListIndex);
	int GetFileID(const CIndexWithinMergedPhotoList &mergedListIndex);
	int GetFileVersion(const CIndexWithinMergedPhotoList &mergedListIndex);
	const rlScLanguage &GetLanguage(const CIndexWithinMergedPhotoList &mergedListIndex);

	CIndexWithinMergedPhotoList ConvertUndeletedEntryToIndex(const CUndeletedEntryInMergedPhotoList &undeletedEntry);

// #if __BANK
// 	CUndeletedEntryInMergedPhotoList GetIndexOfFirstPhoto(bool bLocalPhoto);
// #endif	//	__BANK

private:
	//	Private functions

#if __BANK
	void DisplaySize();
#endif	//	__BANK

	void AddToMergedArray(ePhotoSource photoSource, s32 arrayIndex, const char *pContentId, const char *pTitle, const char *pDescription, const char *pMetadata, const u64 &creationDate, int fileID, int fileVersion, const rlScLanguage &language);

	//	Private Data
	CListOfUGCPhotosOnCloud *m_pListOfPhotosOnCloud;	//	Maybe this doesn't have to be a pointer any more. There's no cloud listener now.

	class CMergedListEntry
	{
	public:

		void Set(ePhotoSource photoSource, s32 ArrayIndex, const char *pContentId, const char *pTitle, const char *pDescription, const char *pMetadata, const u64 &creationDate, int fileID, int fileVersion, const rlScLanguage &language);

		ePhotoSource GetPhotoSource() { return m_PhotoSource; }
		s32 GetArrayIndex() { return  m_ArrayIndex; }

		void SetHasBeenDeleted(bool bDeleted) { m_bHasBeenDeleted = bDeleted; }
		bool GetHasBeenDeleted() { return m_bHasBeenDeleted; }

		bool ReadMetadataFromString(const char* pJsonString, u32 LengthOfJsonString);

		bool WriteMetadataToString(char *pJsonString, u32 MaxLengthOfJsonString, bool bDebugLessData);

		//	Metadata accessors
		CSavegamePhotoMetadata const & GetMetaData() const { return m_Metadata; }
		const char *GetTitle() { return m_Title; }
		void SetTitle(const char *pTitle) { m_Title = pTitle; }

		const char *GetDescription() { return m_Description; }
		void SetDescription(const char *pDescription) { m_Description = pDescription; }


		//		bool GetIsBookmarked() { return m_Metadata.GetIsBookmarked(); }
		//		void SetIsBookmarked(bool bBookmarked) { m_Metadata.SetIsBookmarked(bBookmarked); }

		void GetPhotoLocation(Vector3& vLocation) { m_Metadata.GetPhotoLocation(vLocation); }

		const char *GetSongTitle() { return m_Metadata.GetSongTitle(); }
		const char *GetSongArtist() { return m_Metadata.GetSongArtist(); }

		const char *GetContentId() { return m_ContentId.c_str(); }

		int GetFileID() { return m_FileID; }
		int GetFileVersion() { return m_FileVersion; }
		const rlScLanguage &GetLanguage() { return m_Language; }

#if __BANK
		//	Add the lengths of all atStrings to the sizeof(CMergedListEntry). Also call m_Metadata.GetSize()
		u32 GetSize();
#endif	//	__BANK

		const u64 &GetCreationDate() { return m_CreationDate; }
		bool GetHasValidMetadata() { return m_bHasValidMetadata; }
		inline bool GetIsMemePhoto() const { return m_Metadata.IsMemeEdited(); }

	private:
		ePhotoSource m_PhotoSource;
		s32			m_ArrayIndex;
		bool		m_bHasBeenDeleted;

		atString m_Title;
		atString m_Description;
		CSavegamePhotoMetadata m_Metadata;
		atString m_ContentId;

		u64			m_CreationDate;
		bool		m_bHasValidMetadata;
		int         m_FileID;
		int         m_FileVersion;
		rlScLanguage m_Language;
	};

	CMergedListEntry m_MergedList[MAX_PHOTOS_TO_LIST];
	u32 m_NumberOfMergedListEntries;
	u32 m_NumberOfDeletedPhotos;
	bool m_bNumberOfPhotosIsUpToDate;
	bool m_bFullListOfMetadataIsUpToDate;

	static const u64 ms_InvalidDate;
	static const rlScLanguage ms_UnknownLanguage;
};


#endif	//	!__LOAD_LOCAL_PHOTOS

#endif	//	SAVEGAME_PHOTO_MERGED_LIST_H



