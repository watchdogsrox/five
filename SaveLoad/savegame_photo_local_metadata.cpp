
#include "SaveLoad/savegame_photo_local_metadata.h"

// Game headers
#include "SaveLoad/savegame_channel.h"


// Static variables
CSavegamePhotoLocalMetadata::MetadataMap CSavegamePhotoLocalMetadata::ms_MetadataMap;


void CSavegamePhotoLocalMetadata::Init()
{
	ms_MetadataMap.Reset();
}

void CSavegamePhotoLocalMetadata::Add(const CSavegamePhotoUniqueId &uniqueId, CSavegamePhotoMetadata &metadata, atString title, atString description)
{
	const int nUniqueId = uniqueId.GetValue();

	if (uniqueId.GetIsMaximised())
	{
		photoDisplayf("CSavegamePhotoLocalMetadata::Add - this photo %d is maximised so returning without adding to local metadata", nUniqueId);
		return;
	}

	if (ms_MetadataMap.Access(nUniqueId))
	{
		photoAssertf(0, "CSavegamePhotoLocalMetadata::Add - entry with id %d already exists in map. I'll replace it with the new data", nUniqueId);
		ms_MetadataMap.Delete(nUniqueId);
	}

	CMetadataForOnePhoto newData(metadata, title, description);

	photoDisplayf("CSavegamePhotoLocalMetadata::Add - adding metadata for unique photo id %d", nUniqueId);

	ms_MetadataMap.Insert(nUniqueId, newData);
}

void CSavegamePhotoLocalMetadata::Remove(const CSavegamePhotoUniqueId &uniqueId)
{
	const int nUniqueId = uniqueId.GetValue();

	if (uniqueId.GetIsMaximised())
	{
		photoDisplayf("CSavegamePhotoLocalMetadata::Remove - this photo %d is maximised so returning without removing from local metadata", nUniqueId);
		return;
	}

	if (photoVerifyf(ms_MetadataMap.Access(nUniqueId), "CSavegamePhotoLocalMetadata::Remove - entry with id %d doesn't exist in map", nUniqueId))
	{
		ms_MetadataMap.Delete(nUniqueId);
	}
}


CMetadataForOnePhoto *CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(const CSavegamePhotoUniqueId &uniqueId)
{
	const int nUniqueId = uniqueId.GetValue();

	CMetadataForOnePhoto *pMetadata = ms_MetadataMap.Access(nUniqueId);

	photoAssertf(pMetadata, "CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId - entry with unique photo id %d doesn't exist in map", nUniqueId);

	return pMetadata;
}

