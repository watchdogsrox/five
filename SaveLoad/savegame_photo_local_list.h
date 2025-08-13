
#ifndef SAVEGAME_PHOTO_LOCAL_LIST_H
#define SAVEGAME_PHOTO_LOCAL_LIST_H

// Game headers
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_photo_unique_id.h"


class CSavegamePhotoLocalList
{
public:
	static void Init();

	//	Add - call this when enumerating the contents of the local savegame folder
	static void Add(const CSavegamePhotoUniqueId &idToAdd);

	//	InsertAtTopOfList - call this when saving a new photo (including a save made by the meme editor)
	static void InsertAtTopOfList(const CSavegamePhotoUniqueId &idToAdd);

	//	Delete - call this when deleting a local photo
	static void Delete(const CSavegamePhotoUniqueId &idToDelete);

	static bool DoesIdExistInList(const CSavegamePhotoUniqueId *pIdToCheck);

	static void SetListHasBeenCreated(bool bCreated) { ms_bListHasBeenCreated = bCreated; }
	static bool GetListHasBeenCreated() { return ms_bListHasBeenCreated; }

	static u32 GetNumberOfPhotosInList() { return ms_NumberOfPhotos; }

	static bool GetUniquePhotoIdAtIndex(u32 arrayIndex, CSavegamePhotoUniqueId &returnUniqueId);

#if !__NO_OUTPUT
	static void DisplayContentsOfList();
#endif	//	!__NO_OUTPUT

private:
	static CSavegamePhotoUniqueId ms_ArrayOfLocalPhotoIds[NUMBER_OF_LOCAL_PHOTOS];

	static u32 ms_NumberOfPhotos;
	static bool ms_bListHasBeenCreated;
};

#endif	//	SAVEGAME_PHOTO_LOCAL_LIST_H



