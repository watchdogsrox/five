
#include "SaveLoad/savegame_photo_mugshot_uploader.h"


// Game headers
#include "Network/NetworkInterface.h"
#include "SaveLoad/savegame_channel.h"
#include "system/system.h"

//////////////////////////////////////////////////////////////////////////
//
void CSavegamePhotoMugshotUploader::Init()
{

}

//////////////////////////////////////////////////////////////////////////
//
void CSavegamePhotoMugshotUploader::Shutdown()
{
	Reset();
}


//////////////////////////////////////////////////////////////////////////
//
void CSavegamePhotoMugshotUploader::Reset()
{
	m_state = AVAILABLE;

	m_requestId = -1;
}

//////////////////////////////////////////////////////////////////////////
//
bool CSavegamePhotoMugshotUploader::RequestUpload(const u8 *pJpgBuffer, u32 sizeOfJpgBuffer, const char* pCloudPath)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// We might be busy with another request already
	if (CanProcessRequest() == false)
	{
		savegameAssertf(0, "CSavegamePhotoMugshotUploader::RequestUpload - already processing a request");
		return false;
	}

	// Check path is valid
	if (pCloudPath == NULL)
	{
		savegameAssertf(0, "CSavegamePhotoMugshotUploader::RequestUpload - pCloudPath is NULL");
		return false;
	}

	if (!pJpgBuffer)
	{
		savegameAssertf(0, "CSavegamePhotoMugshotUploader::RequestUpload - pJpgBuffer is NULL");
		return false;
	}

	if (sizeOfJpgBuffer == 0)
	{
		savegameAssertf(0, "CSavegamePhotoMugshotUploader::RequestUpload - expected sizeOfJpgBuffer to be greater than 0");
		return false;
	}

	m_pJpgBuffer = pJpgBuffer;
	m_SizeOfJpgBuffer = sizeOfJpgBuffer;

	// Cache file path
	strcpy(&m_fileName[0], pCloudPath);

	// Lock further requests 
	m_state = REQUEST_UPLOADING;

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void CSavegamePhotoMugshotUploader::Update()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// Nothing to do
	if (m_state == AVAILABLE || m_state == REQUEST_WAITING_ON_UPLOAD)
	{
		return;
	}

	// Process upload request
	if (m_state == REQUEST_UPLOADING)
	{
		m_state = ProcessUploadRequest();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void CSavegamePhotoMugshotUploader::OnCloudEvent(const CloudEvent* pEvent)
{
	// We only care about requests
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
	{
		return;
	}

	// Grab event data
	const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

	// Check if we're interested
	if(pEventData->nRequestID != m_requestId)
	{
		return;
	}

	// Check if it succeeded
	if (pEventData->bDidSucceed == false)
	{
		m_state = REQUEST_FAILED;
		return;
	}

	// Succeeded!
	m_state = REQUEST_SUCCEEDED;
	return;
}

//////////////////////////////////////////////////////////////////////////
//
bool CSavegamePhotoMugshotUploader::CanProcessRequest() const
{
	return ( (m_state == AVAILABLE) || (m_state == REQUEST_FAILED) || (m_state == REQUEST_SUCCEEDED) );
}


//////////////////////////////////////////////////////////////////////////
//
CSavegamePhotoMugshotUploader::State CSavegamePhotoMugshotUploader::ProcessUploadRequest() 
{
	// Check the data is still there
	if (m_pJpgBuffer == NULL || m_SizeOfJpgBuffer == 0)
	{
		return REQUEST_FAILED;
	}

	// Check the cloud is available
	if (NetworkInterface::IsCloudAvailable() == false)
	{
		return REQUEST_FAILED;
	}

	// Submit the post request
	const char* pFilePath = &m_fileName[0];

	m_requestId = CloudManager::GetInstance().RequestPostMemberFile(NetworkInterface::GetLocalGamerIndex(), pFilePath, m_pJpgBuffer, m_SizeOfJpgBuffer, eRequest_CacheAdd);

	// Bail if we got an invalid cloud request id
	if (m_requestId == -1)
	{
		return REQUEST_FAILED;
	}

	return REQUEST_WAITING_ON_UPLOAD;
}

