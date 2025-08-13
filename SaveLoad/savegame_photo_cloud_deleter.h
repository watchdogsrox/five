
#ifndef SAVEGAME_PHOTO_CLOUD_DELETER_H
#define SAVEGAME_PHOTO_CLOUD_DELETER_H

// Rage headers
#include "atl/queue.h"

// Game headers
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/savegame_photo_cloud_list.h"

#if __LOAD_LOCAL_PHOTOS

class CSavegamePhotoCloudDeleter
{
public:
	static void Init(unsigned initMode);

	static void Process();

	static bool AddContentIdToQueue(const char *pContentId);

	static bool IsQueueEmpty() { return sm_QueueOfCloudPhotosToDelete.IsEmpty(); }

private:
	static void RemoveHeadOfQueue();

private:
	enum eCloudDeleterState
	{
		CLOUD_DELETER_BEGIN_DELETE,
		CLOUD_DELETER_WAIT_FOR_DELETE
	};

	static atQueue<atString, MAX_PHOTOS_TO_LOAD_FROM_CLOUD> sm_QueueOfCloudPhotosToDelete;
	static eCloudDeleterState sm_CloudDeleterState;
};

#endif	//	__LOAD_LOCAL_PHOTOS

#endif	//	SAVEGAME_PHOTO_CLOUD_LIST_H

