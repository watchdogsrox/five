
#ifndef SAVEGAME_PHOTO_MUGSHOT_UPLOADER_H
#define SAVEGAME_PHOTO_MUGSHOT_UPLOADER_H

// Game headers
#include "Network/Cloud/CloudManager.h"

//	Copied from PedHeadshotTextureExporter

// Helper class to export mugshot photo jpegs to the cloud.
// It can only deal with one request at a time
class CSavegamePhotoMugshotUploader : public CloudListener
{
public:
	enum State 
	{
		AVAILABLE = 0,
		REQUEST_UPLOADING,
		REQUEST_WAITING_ON_UPLOAD,
		REQUEST_FAILED,
		REQUEST_SUCCEEDED
	};

	CSavegamePhotoMugshotUploader() { Reset(); }

	void	Init();
	void	Shutdown();

	bool	RequestUpload(const u8 *pJpgBuffer, u32 sizeOfJpgBuffer, const char* pCloudPath);
	void	Update();

	bool	HasRequestFailed() const { return m_state == REQUEST_FAILED; };
	bool	HasRequestSucceeded() const { return m_state == REQUEST_SUCCEEDED; };

	// CloudListener callback
	void	OnCloudEvent(const CloudEvent* pEvent);

private:
	State	ProcessUploadRequest();

	bool	CanProcessRequest() const;
	void	Reset();

	char				m_fileName[RAGE_MAX_PATH];
	State				m_state;
	CloudRequestID		m_requestId;

	const u8*			m_pJpgBuffer;
	u32					m_SizeOfJpgBuffer;
};

#endif	//	SAVEGAME_PHOTO_MUGSHOT_UPLOADER_H

