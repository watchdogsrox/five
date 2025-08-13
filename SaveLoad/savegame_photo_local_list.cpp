
#include "SaveLoad/savegame_photo_local_list.h"

// Game headers
#include "SaveLoad/savegame_channel.h"


CSavegamePhotoUniqueId CSavegamePhotoLocalList::ms_ArrayOfLocalPhotoIds[NUMBER_OF_LOCAL_PHOTOS];

u32 CSavegamePhotoLocalList::ms_NumberOfPhotos = 0;
bool CSavegamePhotoLocalList::ms_bListHasBeenCreated = false;


void CSavegamePhotoLocalList::Init()
{
	for (u32 loop = 0; loop < NUMBER_OF_LOCAL_PHOTOS; loop++)
	{
		ms_ArrayOfLocalPhotoIds[loop].Reset();
	}

	ms_NumberOfPhotos = 0;
	ms_bListHasBeenCreated = false;
}

void CSavegamePhotoLocalList::Add(const CSavegamePhotoUniqueId &idToAdd)
{
	u32 loop = 0;
	bool bFoundAFreeSlot = false;

	while (!bFoundAFreeSlot && (loop < NUMBER_OF_LOCAL_PHOTOS) )
	{
		if (ms_ArrayOfLocalPhotoIds[loop].GetIsFree())
		{
			photoAssertf(loop == ms_NumberOfPhotos, "CSavegamePhotoLocalList::Add - loop = %u, ms_NumberOfPhotos = %u", loop, ms_NumberOfPhotos);
			ms_ArrayOfLocalPhotoIds[loop] = idToAdd;
			ms_NumberOfPhotos++;
			bFoundAFreeSlot = true;
		}
		else if (idToAdd == ms_ArrayOfLocalPhotoIds[loop])
		{
			photoAssertf(0, "CSavegamePhotoLocalList::Add - %d already exists in the list", idToAdd.GetValue() );
			bFoundAFreeSlot = true;
		}
		else
		{
			loop++;
		}
	}

	photoAssertf(bFoundAFreeSlot, "CSavegamePhotoLocalList::Add - failed to find a free slot");
}


void CSavegamePhotoLocalList::InsertAtTopOfList(const CSavegamePhotoUniqueId &idToAdd)
{
	//	Check that the number isn't already in the list. It shouldn't be
	u32 loop = 0;
	bool bFound = false;

	while (!bFound && (loop < ms_NumberOfPhotos) )
	{
		if (idToAdd == ms_ArrayOfLocalPhotoIds[loop])
		{
			bFound = true;
		}
		else
		{
			loop++;
		}
	}

	if (bFound)
	{
		photoAssertf(0, "CSavegamePhotoLocalList::InsertAtTopOfList - unique photo Id %d already exists at array index %u so I'll swap it with array index 0", idToAdd.GetValue(), loop);

		CSavegamePhotoUniqueId tempCopyOfId;
		tempCopyOfId = ms_ArrayOfLocalPhotoIds[loop];
		ms_ArrayOfLocalPhotoIds[loop] = ms_ArrayOfLocalPhotoIds[0];
		ms_ArrayOfLocalPhotoIds[0] = tempCopyOfId;
	}
	else
	{
		if (photoVerifyf(ms_NumberOfPhotos < NUMBER_OF_LOCAL_PHOTOS, "CSavegamePhotoLocalList::InsertAtTopOfList - list is full ms_NumberOfPhotos = %u NUMBER_OF_LOCAL_PHOTOS = %u", ms_NumberOfPhotos, NUMBER_OF_LOCAL_PHOTOS))
		{
			if (ms_NumberOfPhotos > 0)
			{
				for (u32 copy_loop = ms_NumberOfPhotos; copy_loop > 0; copy_loop--)
				{
					ms_ArrayOfLocalPhotoIds[copy_loop] = ms_ArrayOfLocalPhotoIds[copy_loop-1];				
				}
			}

			ms_ArrayOfLocalPhotoIds[0] = idToAdd;
			ms_NumberOfPhotos++;
		}
	}
}


void CSavegamePhotoLocalList::Delete(const CSavegamePhotoUniqueId &idToDelete)
{
	u32 loop = 0;
	bool bFoundTheId = false;

	while (!bFoundTheId && (loop < NUMBER_OF_LOCAL_PHOTOS) )
	{
		if (ms_ArrayOfLocalPhotoIds[loop] == idToDelete)
		{
			ms_ArrayOfLocalPhotoIds[loop].Reset();
			bFoundTheId = true;
			ms_NumberOfPhotos--;
		}
		else
		{
			loop++;
		}
	}

	if (photoVerifyf(bFoundTheId, "CSavegamePhotoLocalList::Delete - failed to find %d in the list", idToDelete.GetValue() ))
	{	//	Close gap
		while (loop < (NUMBER_OF_LOCAL_PHOTOS-1) )
		{
			ms_ArrayOfLocalPhotoIds[loop] = ms_ArrayOfLocalPhotoIds[loop+1];
			loop++;
		}

		ms_ArrayOfLocalPhotoIds[NUMBER_OF_LOCAL_PHOTOS-1].Reset();
	}
}

bool CSavegamePhotoLocalList::DoesIdExistInList(const CSavegamePhotoUniqueId *pIdToCheck)
{
	if (pIdToCheck)
	{
		for (u32 loop = 0; loop < ms_NumberOfPhotos; loop++)
		{
			if (ms_ArrayOfLocalPhotoIds[loop] == *pIdToCheck)
			{
				return true;
			}
		}
	}

	return false;
}

bool CSavegamePhotoLocalList::GetUniquePhotoIdAtIndex(u32 arrayIndex, CSavegamePhotoUniqueId &returnUniqueId)
{
	if (photoVerifyf(arrayIndex < ms_NumberOfPhotos, "CSavegamePhotoLocalList::GetUniquePhotoIdAtIndex - arrayIndex %u is greater than the total number of photos in the list %u", arrayIndex, ms_NumberOfPhotos))
	{
		returnUniqueId = ms_ArrayOfLocalPhotoIds[arrayIndex];
		return true;
	}

	return false;
}

#if !__NO_OUTPUT
void CSavegamePhotoLocalList::DisplayContentsOfList()
{
	photoDisplayf("CSavegamePhotoLocalList::DisplayContentsOfList - number of photos in list = %u", ms_NumberOfPhotos);
	for (u32 loop = 0; loop < NUMBER_OF_LOCAL_PHOTOS; loop++)
	{
		if (!ms_ArrayOfLocalPhotoIds[loop].GetIsFree())
		{
			photoDisplayf("%u = %d", loop, ms_ArrayOfLocalPhotoIds[loop].GetValue());
		}
	}
}
#endif	//	!__NO_OUTPUT


