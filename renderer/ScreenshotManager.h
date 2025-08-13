//
// renderer/ScreenshotManager.h
//
// Copyright (C) 2011 Rockstar Games.  All Rights Reserved.
//
// Helper class to take in-game screen shots
// only one screen shot request can be dealt with 
// at a time
// It also provides an interface to load JPEG files

#ifndef _SCREENSHOT_MANAGER_H_
#define _SCREENSHOT_MANAGER_H_

#include "renderer/rendertargets.h"

#define __OLD_SCREENSHOT_DEBUG_CODE	(0)

// Forward declarations
class CPhotoBuffer;

class CHighQualityScreenshot
{
public:

	CHighQualityScreenshot();
//	~CHighQualityScreenshot();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	// this function is asynchronous and assumes pFilename and buffer/size
	// is available until the screenshot has been stored
	static bool TakeScreenShot(const char* pFilename, u32 width, u32 height);
	static bool TakeScreenShot( u8* buffer, size_t bufferSize, u32 width, u32 height);

	// can be used to poll the status on the last 
	// screenshot request
	static bool HasScreenShotFinished()	{ return ms_bLastScreenShotStored; };

	static bool HasScreenShotFailed() { return (SSHOT_HAD_ERROR == ms_currentState); }

	static bool IsProcessingScreenshot() { return ( (ms_currentState >= SSHOT_SETTING_UP_DATA) && (ms_currentState <= SSHOT_STORED) ); }

	static bool AreRenderPhasesDisabled() { return ms_bRenderPhasesDisabled; }		//	static member function cannot have 'const' qualifier

	// call only in main/writer thread in a safe synchronisation point
	static void Update();

	// call only in render/reader thread
	static void Execute();
#if RSG_ORBIS || RSG_DURANGO
	static void InsertFence();
#endif // RSG_ORBIS || RSG_DURANGO

	static bool ReadyToTakeScreenShot();
	// call only in render thread
	static bool DoTakeScreenShot();

	static grcRenderTarget* GetRenderTarget()				{ Assert(ms_pRenderTarget); return ms_pRenderTarget; };

	static bool BeginTakeHighQualityPhoto(CPhotoBuffer *pPhotoBuffer, u32 screenshotScale WIN32PC_ONLY(, bool bUseConsoleResolution));
	static void ResetStateForHighQualityPhoto(CPhotoBuffer *pPhotoBuffer);

	static u32  GetDefaultQuality();
	static void GetDefaultScreenshotSize( u32& out_width, u32& out_height );

	static void ResetState();

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif	//	__BANK

	static VideoResManager::eDownscaleFactor GetDownscaleForNoneMaximizedView() { return VideoResManager::DOWNSCALE_QUARTER; }

private:

	static void SetRenderTarget(grcRenderTarget* pTarget)	{ Assert(pTarget); ms_pRenderTarget = pTarget; };
	static bool CreateRenderTarget(u32 width, u32 height);
	static void DeleteRenderTarget();

	static void DisablePasses();
	static void EnablePasses();

	static bool StoreScreenShot(const char* fileName, grcRenderTarget* pRenderTarget, bool bSaveToMemoryBuffer, u8* pMemoryBuffer, u32 SizeOfMemoryBuffer, u32* pFinalSize, bool bJpeg);
	static bool StoreRawScreenShot( grcRenderTarget* pRenderTarget, u8* pMemoryBuffer, size_t const SizeOfMemoryBuffer );

	static bool StoreScreenShot(const char* fileName, grcRenderTarget* pRenderTarget, bool bSaveToMemoryBuffer, u8* pMemoryBuffer, u32 SizeOfMemoryBuffer, u32* pFinalSize, bool bJpeg, u32 quality, CPhotoBuffer* pSrcPhotoBuffer = NULL);
	static bool StoreScreenShotInBuffer(u8 *pMemoryBuffer, u32* FinalSize, u32 SizeOfMemoryBuffer, void *pSrc, int width, int height, int pitch, u32 quality, CPhotoBuffer* pSrcPhotoBuffer);
	static bool StoreRawScreenShotInBuffer(u8 *pMemoryBuffer, size_t const SizeOfMemoryBuffer, 
		void const * const pSrc, int const width, int const height, int const pitch );

	static bool CheckScreenshotDimensionsFitWithinDeviceDimensions(u32 width, u32 height);

	enum ScreenShotState 
	{
		SSHOT_IDLE = 0,
		SSHOT_SETTING_UP_DATA,
		SSHOT_PENDING,
		SSHOT_TAKING,
		SSHOT_TAKEN,
		SSHOT_STORING,
		SSHOT_STORED,
		SSHOT_WAITING_FOR_MEMORY_BUFFER_TO_BE_FREED,
		SSHOT_HAD_ERROR
	};

	static ScreenShotState		ms_currentState;
	static grcRenderTarget*		ms_pRenderTarget;
	static const char*			ms_pCurrentFileName;
	static u8*					ms_pCurrentRGBBuffer;
	static size_t				ms_rgbBufferSize;
	

#if RSG_ORBIS || RSG_DURANGO
	static grcFenceHandle ms_fence;
#endif // RSG_ORBIS || RSG_DURANGO

	static CPhotoBuffer *ms_pPhotoBuffer;

	static bool				ms_bSaveToMemoryBuffer;
	static bool				ms_bLastScreenShotStored;
	static bool				ms_bRenderPhasesDisabled;


#if __BANK
	static bool ms_bLastScreenShotFailed;
	static bool ms_bPhotoMemoryIsAllocated;
#endif

	friend class PhonePhotoEditor;
};



class CLowQualityScreenshot
{
public:
	enum ePhotoRotation
	{
		PHOTO_NO_ROTATION,
		PHOTO_CLOCKWISE,
		PHOTO_ANTI_CLOCKWISE,
		PHOTO_MAX_ROTATIONS
	};

	CLowQualityScreenshot();

	bool BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto(CPhotoBuffer *pPhotoBuffer, u32 desiredWidth, u32 desiredHeight);
	void RequestFreeMemoryForLowQualityPhoto();

	bool HasLowQualityCopySucceeded() { return (m_LowQualityCopyState == LOW_QUALITY_COPY_SUCCEEDED); }
	bool HasLowQualityCopyFailed() { return (m_LowQualityCopyState == LOW_QUALITY_COPY_FAILED); }

	bool IsRendering() { return (m_pTextureForLowQualityPhoto != NULL); }
	grcTexture *GetGrcTexture() { return m_pTextureForLowQualityPhoto; }

	void ClearGrcTexturePointerWithoutDeleting();

	void SetRenderToPhone(bool bRender, ePhotoRotation photoRotation) { m_bRenderToPhone = bRender; m_PhotoRotation = photoRotation; }
	bool GetRenderToPhone()	{ return m_bRenderToPhone; }

	void Execute();

	void Render(bool bRenderToPhone);

	void Shutdown(unsigned shutdownMode);

#if __BANK
	void AddWidgets(bkBank& bank);

	void SetRenderDebug(bool bRender)	{ m_bRenderDebug = bRender; }
	bool GetRenderDebug() { return m_bRenderDebug; }

	void SetForceMemoryAllocationToFail(bool bForceFail) { m_bForceMemoryAllocationToFail = bForceFail; }

#if __OLD_SCREENSHOT_DEBUG_CODE
	void DebugRequestLoadJPEGFromFile();
#endif	//	__OLD_SCREENSHOT_DEBUG_CODE

#endif	//	__BANK

private:

	// query memory requirements for LoadJPEGIntoRGBA8Texture
	bool GetJPEGInfoForRGBA8Texture(const char* pFilename, u32& width, u32& height, u32& pitch);
	bool GetJPEGInfoForRGBA8Texture(fiStream* pStream, u32& width, u32& height, u32& pitch);

	// call only from render thread
	grcTexture* LoadJPEGIntoRGBA8Texture(const char* pFilename, void* pDstMem, u32 desiredWidth, u32 desiredHeight, bool bIsSystemMemory);
	grcTexture* LoadJPEGIntoRGBA8Texture(fiStream* pStream, void* pDstMem, u32 desiredWidth, u32 desiredHeight, bool bIsSystemMemory);

	bool BeginCreateLowQualityCopyOfPhoto(const u8 *pSourceBuffer, u32 SizeOfSourceBuffer, u32 desiredWidth, u32 desiredHeight);

	void FreeMemoryForLowQualityPhoto();
	void CreateLowQualityCopyOfPhoto();

	static void StaticRender(grcTexture **ppTexture, ePhotoRotation photoRotation, bool bRenderToPhone);
	static void DrawPhotoQuad(float x1, float y1, float x2, float y2, float zVal, ePhotoRotation photoRotation, const Color32 &color32);

#if __BANK

#if __OLD_SCREENSHOT_DEBUG_CODE
	void DebugLoadJPEGFromFile();
#endif	//	__OLD_SCREENSHOT_DEBUG_CODE

#endif	//	__BANK

	enum eCopyLowQualityPhotoState
	{
		LOW_QUALITY_COPY_SUCCEEDED,
		LOW_QUALITY_COPY_IN_PROGRESS,
		LOW_QUALITY_COPY_FAILED
	};

	eCopyLowQualityPhotoState m_LowQualityCopyState;
	bool m_bFreeMemoryForLowQualityPhoto;
	bool m_bCreateLowQualityCopyOfPhoto;

	const u8 *m_pMemoryBufferToCopyHighQualityPhotoFrom;
	u32 m_SizeOfMemoryBufferToCopyHighQualityPhotoFrom;

	u32 m_desiredWidthOfLowQualityCopy;
	u32 m_desiredHeightOfLowQualityCopy;

	grcTexture*		m_pTextureForLowQualityPhoto;
	void*			m_pBufferForLowQualityPhoto;

	ePhotoRotation	m_PhotoRotation;
	bool		m_bRenderToPhone;

#if __BANK
	bool			m_bRenderDebug;
	bool			m_bCopyHasFinished;
	bool			m_bCopyHasFailed;
	bool			m_bMemoryHasBeenAllocatedForCopy;

	bool			m_bForceMemoryAllocationToFail;

#if __OLD_SCREENSHOT_DEBUG_CODE
	char			m_textureName[128];
	bool			m_bLoadJpegFromFile;
#endif	//	__OLD_SCREENSHOT_DEBUG_CODE

#endif	//	__BANK
};


#endif //_SCREENSHOT_MANAGER_H_
