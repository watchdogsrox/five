#include "Network/Cloud/VideoUploadManager.h"

#if defined(GTA_REPLAY) && GTA_REPLAY 
#if USE_ORBIS_UPLOAD

#include "frontend/VideoEditor/ui/Menu.h"

void CVideoUploadManager_Orbis::Init()
{
	CVideoUploadManager::Init();
}

void CVideoUploadManager_Orbis::Shutdown()
{
	CVideoUploadManager::Shutdown();
}

void CVideoUploadManager_Orbis::Update()
{
	CVideoUploadManager::Update();

	UpdateUsers();
}

bool CVideoUploadManager_Orbis::RequestPostVideo(const char* sourcePathAndFileName, u32 duration, const char* createdDate, s64 contentId)
{
	(void)contentId; // TODO: once we have the API, we send up the content Id to start upload process on PS4

	if (!IsUploadEnabled())
		return false;

	// on orbis, we save off file information while we wait for it to be posted to activity feed
	if (!GatherHeaderData(sourcePathAndFileName, duration, createdDate))
		return false;

	CVideoEditorMenu::SetUserConfirmationScreen( "YT_UPLOAD_ON_SYSTEM" );

	return true;
}

CVideoUploadManager::UploadProgressState::Enum CVideoUploadManager_Orbis::GetProgressState(s64 UNUSED_PARAM(contentId), u32 UNUSED_PARAM(duration), const char* UNUSED_PARAM(createdDate)) const
{
	return UploadProgressState::NotUploaded;
}

#endif // USE_ORBIS_UPLOAD
#endif //  defined(GTA_REPLAY) && GTA_REPLAY 
