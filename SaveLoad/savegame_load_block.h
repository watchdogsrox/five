

#ifndef SAVEGAME_LOAD_BLOCK_H
#define SAVEGAME_LOAD_BLOCK_H

//Rage headers
#include "system/param.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "net/status.h"
#include "data/growbuffer.h"
#include "rline/ros/rlroscommon.h"
#include "rline/ugc/rlugccommon.h"

// Game headers
#include "Renderer/ScreenshotManager.h"
#include "SaveLoad/savegame_defines.h"
#include "Network/Cloud/CloudManager.h"
#include "network/Network.h"


// Forward declarations
class CPhotoBuffer;

class CloudFileLoadListener : public CloudListener
{
public:
	CloudFileLoadListener();

	virtual ~CloudFileLoadListener() {}

	virtual void OnCloudEvent(const CloudEvent* pEvent);

	bool BeginLoadingJpegFromCloud(const sRequestData &requestData, CPhotoBuffer *pBufferToContainJpeg, u32 maxSizeOfBuffer, rlCloudNamespace cloudNamespace, bool bDisplayLoadError);
	MemoryCardError UpdateLoadingJpegFromCloud(bool bDisplayLoadError);

	bool GetIsRequestPending() { return m_bIsRequestPending; }
	bool GetLastLoadSucceeded() { return m_bLastLoadSucceeded; }

private:
	void Initialise();
	bool RequestLoadOfCloudJpeg(const sRequestData &requestData, CPhotoBuffer *pBufferToContainJpeg, u32 maxSizeOfBuffer, rlCloudNamespace cloudNamespace);

	// Private data
	CPhotoBuffer *m_pBufferToContainJpeg;
	u32 m_MaxSizeOfBuffer;

	CloudRequestID m_CloudRequestId;
	bool m_bIsRequestPending;
	bool m_bLastLoadSucceeded;
};



enum eLoadJpegFromCloudState
{
	LOAD_JPEG_FROM_CLOUD_DO_NOTHING = 0,
	LOAD_JPEG_FROM_CLOUD_INITIAL_CHECKS,
	LOAD_JPEG_FROM_CLOUD_BEGIN_LOAD,
	LOAD_JPEG_FROM_CLOUD_DO_LOAD,
	LOAD_JPEG_FROM_CLOUD_HANDLE_ERRORS
};


enum eBlockLoadStateCloud
{
	BLOCK_LOAD_STATE_CLOUD_INIT,
	BLOCK_LOAD_STATE_CLOUD_UPDATE
};


class CSavegameLoadJpegFromCloud
{
private:
//	Private data
	static CloudFileLoadListener *ms_pCloudFileLoadListener;

	static eLoadJpegFromCloudState	ms_LoadJpegFromCloudStatus;
	static eBlockLoadStateCloud ms_CloudLoadSubstate;

	static CPhotoBuffer *ms_pBufferToContainJpeg;
	static u32 ms_MaxSizeOfBuffer;
    static rlCloudNamespace ms_CloudNamespace;
    static sRequestData ms_RequestData;
	static bool ms_bDisplayLoadError;

//	Private functions
//	static MemoryCardError DoLoadBlock(u8 *pBuffer);
	static MemoryCardError DoLoadBlockCloud();

//	static void				EndLoadBlock();
	static void				EndLoadBlockCloud();

public:
//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

//	static MemoryCardError BeginLoadBlock(int SlotNumber, const bool isCloudOperation, eFileTypeForBlockLoadOrSave fileType);
	static MemoryCardError BeginLoadJpegFromCloud(const sRequestData &requestData, CPhotoBuffer *pBufferToContainJpeg, u32 maxSizeOfBuffer, rlCloudNamespace cloudNamespace, bool bDisplayLoadError);

//	static void SetJsonBuffer(char *pJsonBuffer);

//	static MemoryCardError CalculateSizeOfRequiredBuffer();
	
	static MemoryCardError LoadJpegFromCloud();
//	static MemoryCardError DeSerialize();

//	static u32 GetBlockBufferSize() { return ms_SizeOfBlockBuffer; }
};

#endif	//	SAVEGAME_LOAD_BLOCK_H


