
#ifndef SAVEGAME_PHOTO_LOCAL_METADATA_H
#define SAVEGAME_PHOTO_LOCAL_METADATA_H

// Rage headers
#include "atl/map.h"

//Game headers
#include "SaveLoad/savegame_photo_metadata.h"
#include "SaveLoad/savegame_photo_unique_id.h"


//	Contains the metadata for the current page of local photos

//	#define MAX_TEXTURE_DICTIONARIES_FOR_GALLERY	(12)	//	The gallery displays 12 photos

class CMetadataForOnePhoto
{
public:
	CMetadataForOnePhoto(CSavegamePhotoMetadata &metadata, atString title, atString description) 
		: m_Metadata(metadata), m_Title(title), m_Description(description), m_bHasValidSignature(false) {}

	const CSavegamePhotoMetadata *GetMetadata() const { return &m_Metadata; }

	const char *GetTitleOfPhoto() const { return m_Title.c_str(); }
	void SetTitleOfPhoto(const char *pTitle) { m_Title = pTitle; }
	const char *GetDescriptionOfPhoto() const { return m_Description.c_str(); }

	void SetHasValidSignature(bool bValid) { m_bHasValidSignature = bValid; }
	bool GetHasValidSignature() const { return m_bHasValidSignature; }

private:
	CSavegamePhotoMetadata m_Metadata;
	atString m_Title;
	atString m_Description;
	bool m_bHasValidSignature;
};

class CSavegamePhotoLocalMetadata
{
public:
	CSavegamePhotoLocalMetadata() { Init(); }

	// Init - could call this when changing page on the gallery screen
	static void Init();

	// Add - call this after a photo is loaded for displaying in the gallery
	static void Add(const CSavegamePhotoUniqueId &uniqueId, CSavegamePhotoMetadata &metadata, atString title, atString description);

	// Remove - call this when deleting a local photo
	static void Remove(const CSavegamePhotoUniqueId &uniqueId);

	static CMetadataForOnePhoto *GetMetadataForUniquePhotoId(const CSavegamePhotoUniqueId &uniqueId);

private:
	//	Or CMetadataForOnePhoto*
	typedef atMap<int, CMetadataForOnePhoto> MetadataMap;

	static MetadataMap ms_MetadataMap;
};

#endif	//	SAVEGAME_PHOTO_LOCAL_METADATA_H

