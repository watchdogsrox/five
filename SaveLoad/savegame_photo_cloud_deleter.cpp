
#include "SaveLoad/savegame_photo_cloud_deleter.h"


#if __LOAD_LOCAL_PHOTOS

#include "Network/Cloud/UserContentManager.h"
#include "SaveLoad/savegame_channel.h"


atQueue<atString, MAX_PHOTOS_TO_LOAD_FROM_CLOUD> CSavegamePhotoCloudDeleter::sm_QueueOfCloudPhotosToDelete;
CSavegamePhotoCloudDeleter::eCloudDeleterState CSavegamePhotoCloudDeleter::sm_CloudDeleterState = CSavegamePhotoCloudDeleter::CLOUD_DELETER_BEGIN_DELETE;

void CSavegamePhotoCloudDeleter::Init(unsigned UNUSED_PARAM(initMode))
{

}


void CSavegamePhotoCloudDeleter::Process()
{
	if (!sm_QueueOfCloudPhotosToDelete.IsEmpty())
	{
		switch (sm_CloudDeleterState)
		{
			case CLOUD_DELETER_BEGIN_DELETE :
				{
					const char *pContentId = sm_QueueOfCloudPhotosToDelete.Top().c_str();
					if (photoVerifyf(pContentId && (strlen(pContentId) > 0), "CSavegamePhotoCloudDeleter::Process - Content ID string isn't valid so we can't attempt to delete it"))
					{
						photoDisplayf("CSavegamePhotoCloudDeleter::Process - about to call SetDeleted for Content Id %s", pContentId);
						if (photoVerifyf(UserContentManager::GetInstance().SetDeleted(pContentId, true, RLUGC_CONTENT_TYPE_GTA5PHOTO), "CSavegamePhotoCloudDeleter::Process - SetDeleted for UGC photo has failed to start for Content Id %s", pContentId))
						{
							photoDisplayf("CSavegamePhotoCloudDeleter::Process - SetDeleted for UGC photo has been started");
							sm_CloudDeleterState = CLOUD_DELETER_WAIT_FOR_DELETE;
						}
						else
						{
							RemoveHeadOfQueue();
						}
					}
					else
					{
						RemoveHeadOfQueue();
					}
				}
				break;

			case CLOUD_DELETER_WAIT_FOR_DELETE :
				{
					if (UserContentManager::GetInstance().HasModifyFinished())
					{
						if (UserContentManager::GetInstance().DidModifySucceed())
						{
							photoDisplayf("CSavegamePhotoCloudDeleter::Process - cloud delete succeeded");
						}
						else
						{
							photoDisplayf("CSavegamePhotoCloudDeleter::Process - cloud delete failed");
						}

						RemoveHeadOfQueue();
						sm_CloudDeleterState = CLOUD_DELETER_BEGIN_DELETE;
					}
				}
				break;
		}
	}
}


bool CSavegamePhotoCloudDeleter::AddContentIdToQueue(const char *pContentId)
{
	if (photoVerifyf(!sm_QueueOfCloudPhotosToDelete.IsFull(), "CSavegamePhotoCloudDeleter::AddContentIdToQueue - queue is full"))
	{
		atString newString(pContentId);

		photoDisplayf("CSavegamePhotoCloudDeleter::AddContentIdToQueue - adding Content Id %s to queue", pContentId);
		return sm_QueueOfCloudPhotosToDelete.Push(newString);
	}

	return false;
}


void CSavegamePhotoCloudDeleter::RemoveHeadOfQueue()
{
	if (photoVerifyf(!sm_QueueOfCloudPhotosToDelete.IsEmpty(), "CSavegamePhotoCloudDeleter::RemoveHeadOfQueue - queue is empty"))
	{
		sm_QueueOfCloudPhotosToDelete.Pop().Clear();
	}
}

#endif	//	__LOAD_LOCAL_PHOTOS
