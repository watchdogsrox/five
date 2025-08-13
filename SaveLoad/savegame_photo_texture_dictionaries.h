
#ifndef SAVEGAME_PHOTO_TEXTURE_DICTIONARIES_H
#define SAVEGAME_PHOTO_TEXTURE_DICTIONARIES_H


// Game headers
#include "SaveLoad/savegame_defines.h"


#if __LOAD_LOCAL_PHOTOS

#include "SaveLoad/savegame_photo_unique_id.h"


class CTextureDictionariesForGalleryPhotos
{
public:
	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	s32 FindFreeTextureDictionary(const CSavegamePhotoUniqueId &uniquePhotoId, bool &bAlreadyLoaded);

	void SetTextureDownloadSucceeded(const CSavegamePhotoUniqueId &uniquePhotoId);
	void SetTextureDownloadFailed(const CSavegamePhotoUniqueId &uniquePhotoId);

	MemoryCardError GetStatusOfTextureDictionaryCreation(const CSavegamePhotoUniqueId &uniquePhotoId);

	MemoryCardError UnloadPhoto(const CSavegamePhotoUniqueId &uniquePhotoId);
	MemoryCardError UnloadAllPhotos();


	bool IsThisPhotoLoadedIntoAnyTextureDictionary(const CSavegamePhotoUniqueId &uniquePhotoId);
	bool AreAnyPhotosLoadedIntoTextureDictionaries();

	static const u32 ms_MaxLengthOfTextureName = 32;

	static void GetNameOfPhotoTextureDictionary(const CSavegamePhotoUniqueId &uniquePhotoId, char *pTextureName, u32 LengthOfTextureName);

private :

	// Private functions
	s32 GetTextureDictionaryIndexForThisUniquePhotoId(const CSavegamePhotoUniqueId &uniquePhotoId);

#define MAX_TEXTURE_DICTIONARIES_FOR_GALLERY	(13)	//	12 thumbnails and 1 maximised texture

	class TextureDictionaryForGalleryPhoto
	{
		enum eStateOfTextureDictionary
		{
			STATE_OF_TEXTURE_DICTIONARY_UNLOADED,
			//			STATE_OF_TEXTURE_DICTIONARY_BEGIN_CREATION_OF_IMAGE,
			STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE,
			STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY,
			STATE_OF_TEXTURE_DICTIONARY_DONE,
			//			STATE_OF_TEXTURE_DICTIONARY_BEGIN_DELETION,	//	Maybe some more deletion states after this
			STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR		//	Will I need a function to reset the state to STATE_OF_TEXTURE_DICTIONARY_UNLOADED from this state?
		};

	public:
		TextureDictionaryForGalleryPhoto() 
			: m_StateOfTextureDictionary(STATE_OF_TEXTURE_DICTIONARY_UNLOADED) {}

		void Init(unsigned initMode);
		void Shutdown(unsigned shutdownMode);

		bool Unload();

		bool IsFree();
		bool CanBeUsedByThisUniquePhotoId(const CSavegamePhotoUniqueId &uniquePhotoId, bool &bAlreadyContainsThisIndex);
		bool HasUniquePhotoId(const CSavegamePhotoUniqueId &uniquePhotoId);

		bool BeginCreatingTextureDictionaryUsingTextureDownloadManager(const CSavegamePhotoUniqueId &uniquePhotoId);
		void SetStatusToFailed();
		void SetStatusToComplete();

		MemoryCardError GetStatusOfTextureDictionaryCreation();

	private:
		// Private data
		CSavegamePhotoUniqueId m_UniquePhotoIdToAppendToTxdName;
		eStateOfTextureDictionary m_StateOfTextureDictionary;
	};

	TextureDictionaryForGalleryPhoto m_ArrayOfTextureDictionaries[MAX_TEXTURE_DICTIONARIES_FOR_GALLERY];
};


#else	//	__LOAD_LOCAL_PHOTOS


class CTextureDictionariesForGalleryPhotos
{
public:
	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	s32 FindFreeTextureDictionary(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList, bool &bAlreadyLoaded);

	void SetTextureDownloadSucceeded(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);
	void SetTextureDownloadFailed(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList);

	MemoryCardError GetStatusOfTextureDictionaryCreation(const CIndexWithinMergedPhotoList &indexToAppendToTxdName);

	MemoryCardError UnloadPhoto(const CIndexWithinMergedPhotoList &indexToAppendToTxdName);
	MemoryCardError UnloadAllPhotos();


	bool IsThisPhotoLoadedIntoAnyTextureDictionary(const CIndexWithinMergedPhotoList &indexToAppendToTxdName);
	bool AreAnyPhotosLoadedIntoTextureDictionaries();

	static const u32 ms_MaxLengthOfTextureName = 32;

	static void GetNameOfPhotoTextureDictionary(const CIndexWithinMergedPhotoList &photoIndex, char *pTextureName, u32 LengthOfTextureName);

private :

	// Private functions
	s32 GetTextureDictionaryIndexForThisMergedPhotoIndex(const CIndexWithinMergedPhotoList &indexToAppendToTxdName);

#define MAX_TEXTURE_DICTIONARIES_FOR_GALLERY	(13)	//	12 thumbnails and 1 maximised texture

	class TextureDictionaryForGalleryPhoto
	{
		enum eStateOfTextureDictionary
		{
			STATE_OF_TEXTURE_DICTIONARY_UNLOADED,
			//			STATE_OF_TEXTURE_DICTIONARY_BEGIN_CREATION_OF_IMAGE,
			STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE,
			STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY,
			STATE_OF_TEXTURE_DICTIONARY_DONE,
			//			STATE_OF_TEXTURE_DICTIONARY_BEGIN_DELETION,	//	Maybe some more deletion states after this
			STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR		//	Will I need a function to reset the state to STATE_OF_TEXTURE_DICTIONARY_UNLOADED from this state?
		};

	public:
		TextureDictionaryForGalleryPhoto() 
			: m_IndexToAppendToTxdName(-1, false), m_StateOfTextureDictionary(STATE_OF_TEXTURE_DICTIONARY_UNLOADED) {}

		void Init(unsigned initMode);
		void Shutdown(unsigned shutdownMode);

		bool Unload();

		bool IsFree();
		bool CanBeUsedByThisMergedPhotoListIndex(const CIndexWithinMergedPhotoList &indexToBeAdded, bool &bAlreadyContainsThisIndex);
		bool HasMergedIndex(const CIndexWithinMergedPhotoList &indexToAppendToTxdName);

		bool BeginCreatingTextureDictionaryUsingTextureDownloadManager(const CIndexWithinMergedPhotoList &indexToAppendToTxdName);
		void SetStatusToFailed();
		void SetStatusToComplete();

		MemoryCardError GetStatusOfTextureDictionaryCreation();

	private:
		// Private data
		CIndexWithinMergedPhotoList m_IndexToAppendToTxdName;
		eStateOfTextureDictionary m_StateOfTextureDictionary;
	};

	TextureDictionaryForGalleryPhoto m_ArrayOfTextureDictionaries[MAX_TEXTURE_DICTIONARIES_FOR_GALLERY];
};


#endif	//	__LOAD_LOCAL_PHOTOS

#endif	//	SAVEGAME_PHOTO_TEXTURE_DICTIONARIES_H

