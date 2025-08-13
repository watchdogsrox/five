//
// renderer/ScreenshotManager.cpp
//
// Copyright (C) 2011 Rockstar Games.  All Rights Reserved.
//
// Helper class to take in-game screen shots
// 
// PS3:
// Synchronisation is achieved with a single-reader (render thread),
// single-writer approach using a simple state machine:
//	- Main thread is the *only* thread updating the state, *always* in
//	  a safe synchronisation point.
//	- Render thread executes any action associated with the current state
//
// 360:
// Synchronisation is simpler; screenshot is resolved in a blocking operation

#include "renderer/ScreenshotManager.h"
#include "camera/viewports/ViewportManager.h"
#include "fwrenderer/RenderPhaseBase.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#if __BANK
#include "debug/Rendering/DebugDeferred.h"
#include "cutscene/CutSceneManagerNew.h"
#endif
#include "grcore/allocscope.h"
#include "grcore/image.h"
#include "grcore/light.h"
#include "grcore/quads.h"
#include "grcore/setup.h"
#include "grcore/texturedefault.h"

#include "grmodel/setup.h"

#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_buffer.h"

RENDER_OPTIMISATIONS();

// Screen shot manager
#if __XENON
	#include "grcore/texturexenon.h"
	#include "system/xtl.h"
	#define DBG 0
	#include <xgraphics.h>
	#undef DBG
#endif 



#if __XENON || __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS
bool CHighQualityScreenshot::StoreScreenShotInBuffer(u8 *pMemoryBuffer, u32* FinalSize, u32 SizeOfMemoryBuffer, void *pSrc, int width, int height, int pitch, u32 quality, CPhotoBuffer* WIN32PC_ONLY(pSrcPhotoBuffer))
{
	fiSafeStream SafeStreamToGetSize(ASSET.Create("count:", ""));

	bool bOk = grcImage::SaveRGBA8SurfaceToJPEG(SafeStreamToGetSize, quality, pSrc, width, height, pitch, false) > 0;
	if (bOk)
	{
		SafeStreamToGetSize->Flush(); // don't forget this!
//	In savegame_data.cpp, Align16 is called on the size. The comment says that's required for encryption so the alignment might not be necessary here
//		SizeOfMemoryBuffer = Align16(SafeStreamToGetSize->Size());

		u32 requiredSize = SafeStreamToGetSize->Size();
		FinalSize[0] = requiredSize;
		if (photoVerifyf(requiredSize > 0, "StoreScreenShotInBuffer - calculated buffer size is 0"))
		{
			photoDisplayf("StoreScreenShotInBuffer - %u bytes required to store this jpeg", requiredSize);

#if !__WIN32PC
			if (requiredSize > SizeOfMemoryBuffer)
			{
				photoAssertf(0, "StoreScreenShotInBuffer - required size for this jpeg is %u, but the maximum buffer size is %u", requiredSize, SizeOfMemoryBuffer);
				bOk = false;
			}
			else
#else
			if (requiredSize > SizeOfMemoryBuffer && pSrcPhotoBuffer)
			{
//				if (GetSaveBuffer() && (pMemoryBuffer == GetSaveBuffer()))
				if (pSrcPhotoBuffer->GetJpegBuffer() && (pMemoryBuffer == pSrcPhotoBuffer->GetJpegBuffer()))
				{
					bOk = pSrcPhotoBuffer->ReAllocateMemoryForIncreasedJpegSize(requiredSize);
				}
			}

			if (bOk)
#endif
			{
				char memFileName[RAGE_MAX_PATH];
				fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, pMemoryBuffer, SizeOfMemoryBuffer, false, "");
				fiSafeStream SafeStreamForSaving(ASSET.Create(memFileName, ""));

				bOk = grcImage::SaveRGBA8SurfaceToJPEG(SafeStreamForSaving, quality, pSrc, width, height, pitch, false) > 0;
			}
		}
		else
		{
			bOk = false;
		}
	}

	return bOk;
}
#endif	//	__XENON || __PS3 || __WIN32PC || RSG_DURANGO || RSG_ORBIS

bool CHighQualityScreenshot::StoreRawScreenShotInBuffer( u8 *pMemoryBuffer, size_t const SizeOfMemoryBuffer, 
														void const * const pSrc, int const width, int const height, int const pitch )
{
	if( !Verifyf( pMemoryBuffer, "CHighQualityScreenshot::StoreRawScreenShotInBuffer - NULL destination buffer" ) ||
		!Verifyf( SizeOfMemoryBuffer > 0, "CHighQualityScreenshot::StoreRawScreenShotInBuffer - Invalid destination size" ) ||
		!Verifyf( pSrc, "CHighQualityScreenshot::StoreRawScreenShotInBuffer - NULL source buffer" ) ||
		!Verifyf( width > 0 && height > 0 && pitch > 0, "CHighQualityScreenshot::StoreRawScreenShotInBuffer - invalid source parameters" ) )
	{ 
		return false;
	}

	size_t const bufferPitch = SizeOfMemoryBuffer / height;
	if( bufferPitch == pitch )
	{
		sysMemCpy( pMemoryBuffer, (u8*)pSrc, pitch * height );
	}
	else
	{
		u8 const * srcLocal = (u8 const*)pSrc;
		for( size_t index = 0; index < height; ++index, srcLocal += pitch )
		{
			sysMemCpy( pMemoryBuffer + ( index * bufferPitch ), srcLocal, bufferPitch ); 
		}
	}

	return true;
}

#if __XENON
// These functions were present in image_jpeg.cpp to support xenon.
// Original comment below:
// TBR: tidy these functions up, temporarily copied from xgraphics.h 
// to avoid bringing in an awful lot of unnecessary code, which was also causing compile errors

//------------------------------------------------------------------------
// Calculate the log2 of a texel pitch which is less than or equal to 16
// and a power of 2.s
inline u32 XenonLog2LE16(u32 TexelPitch)
{
#if defined(_X86_) || defined(_AMD64_)
	return (TexelPitch >> 2) + ((TexelPitch >> 1) >> (TexelPitch >> 2));
#else
	return 31 - _CountLeadingZeros(TexelPitch);
#endif
}

//------------------------------------------------------------------------
// Translate the address of a surface texel/block from 2D array coordinates into
// a tiled memory offset measured in texels/blocks.
inline u32 XenonAddress2DTiledOffset(
									 u32 x,             // x coordinate of the texel/block
									 u32 y,             // y coordinate of the texel/block
									 u32 Width,         // Width of the image in texels/blocks
									 u32 TexelPitch     // Size of an image texel/block in bytes
									 )
{
	u32 AlignedWidth;
	u32 LogBpp;
	u32 Macro;
	u32 Micro;
	u32 Offset;

	AlignedWidth = (Width + 31) & ~31;
	LogBpp       = XenonLog2LE16(TexelPitch);
	Macro        = ((x >> 5) + (y >> 5) * (AlignedWidth >> 5)) << (LogBpp + 7);
	Micro        = (((x & 7) + ((y & 6) << 2)) << LogBpp);
	Offset       = Macro + ((Micro & ~15) << 1) + (Micro & 15) + ((y & 8) << (3 + LogBpp)) + ((y & 1) << 4);

	return (((Offset & ~511) << 3) + ((Offset & 448) << 2) + (Offset & 63) +
		((y & 16) << 7) + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)) >> LogBpp;
}
 
// These are probably no longer necessary, row and stride are passed in directly.
static void* xenon_hack_base; // Cache a copy of the base pointer in xenon, to figure out which scanline we're on
static u32 xenon_hack_pitch;


static void copyscan_nogamma(u8 *dest,void *base,int row,int width,int stride,u8 * /*gammalut*/)
{
	base = (char*)base + stride * row;
	u32 y = (size_t(base) - size_t(xenon_hack_base)) / xenon_hack_pitch;
	for (int x = 0; x < width; x++)
	{
		const u32 addr = XenonAddress2DTiledOffset(x, y, width, sizeof(u32));
		const u32 n = ((const u32*)xenon_hack_base)[addr];
		dest[0] = u8(n >> 16);
		dest[1] = u8(n >> 8);
		dest[2] = u8(n);
		dest += 3;
	}
}


#elif !RSG_ORBIS

static void copyscan_nogamma(u8 *dest,void *base,int row,int width,int stride,u8 * /*gammalut*/)
{
	base = (char*)base + row * stride;
	u32 *src = (u32*)base;
	while (width--)
	{
		u32 n = *src++;
		dest[0] = u8(n);
		dest[1] = u8(n >> 8);
		dest[2] = u8(n >> 16);
		dest += 3;
	}
}

#else

static void copyscan_nogamma(u8 *dest,void *base,int row,int width,int stride,u8 * /*gammalut*/)
{
	base = (char*)base + row * stride;
	u32 *src = (u32*)base;
	while (width--)
	{
		u32 n = *src++;
		dest[0] = u8(n >> 16);
		dest[1] = u8(n >> 8);
		dest[2] = u8(n);
		dest += 3;
	}
}

#endif

bool CHighQualityScreenshot::StoreScreenShot(const char* fileName, grcRenderTarget* pRenderTarget, bool bSaveToMemoryBuffer, u8* pMemoryBuffer, u32 SizeOfMemoryBuffer, u32* pFinalSize, bool bJpeg)
{
//	Ask Luis about this - CL 4873829
	return StoreScreenShot(fileName, pRenderTarget, bSaveToMemoryBuffer, pMemoryBuffer, SizeOfMemoryBuffer, pFinalSize, bJpeg, GetDefaultQuality(), ms_pPhotoBuffer);
}

bool CHighQualityScreenshot::StoreScreenShot(const char* fileName, grcRenderTarget* pRenderTarget, bool bSaveToMemoryBuffer, u8* pMemoryBuffer, u32 SizeOfMemoryBuffer, u32* pFinalSize, bool bJpeg, u32 quality, CPhotoBuffer* pSrcPhotoBuffer)
{
	if (!photoVerifyf(!bSaveToMemoryBuffer || bJpeg, "PNG can't currently be saved to a memory buffer."))
		return false;

#if __XENON
	D3DTexture* pTexture = static_cast<D3DTexture*>(pRenderTarget->GetTexturePtr());

	XGTEXTURE_DESC desc;
	XGGetTextureDesc(pTexture, 0, &desc);


	D3DLOCKED_RECT rect;
	XGGetTextureDesc(pTexture, 0, &desc);
	GRCDEVICE.CpuWaitOnGpuIdle();
	for (int i = 0; i < 16; i++)
	{
		GRCDEVICE.GetCurrent()->SetTexture(i, NULL);
	}
	pTexture->LockRect(0,&rect,NULL,D3DLOCK_READONLY);
		bool bOk = false;
		if (bSaveToMemoryBuffer)
		{
			bOk = StoreScreenShotInBuffer(pMemoryBuffer, pFinalSize, SizeOfMemoryBuffer, rect.pBits, desc.Width, desc.Height, rect.Pitch, quality, pSrcPhotoBuffer);
		}
		else
		{
			if (bJpeg)
			{
				bOk = grcImage::SaveRGBA8SurfaceToJPEG(fileName, quality, rect.pBits, desc.Width, desc.Height, rect.Pitch) > 0;
			}
			else
			{
				xenon_hack_base = rect.pBits;
				xenon_hack_pitch = (u32)rect.Pitch;
				bOk = grcImage::WritePNG(fileName, copyscan_nogamma, desc.Width, desc.Height, rect.pBits, rect.Pitch, 0);
			}
		}
	pTexture->UnlockRect(0);

	return bOk;

#elif __PS3

	const grcRenderTargetGCM* pGCMRenderTarget = static_cast<const grcRenderTargetGCM*>(pRenderTarget);
	const CellGcmTexture* gcmTexture=reinterpret_cast<const CellGcmTexture*>(pGCMRenderTarget->GetTexturePtr());
	photoAssertf(gcmTexture->format==CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN | CELL_GCM_TEXTURE_NR,"we only support 8 bit, linear, normalized textures");
	
	int	width = pGCMRenderTarget->GetWidth();
	int	height = pGCMRenderTarget->GetHeight();
	int pitch = pGCMRenderTarget->GetPitch();

	void* pSrc = (gcmTexture->location == CELL_GCM_LOCATION_LOCAL ? gcm::LocalPtr(pGCMRenderTarget->GetMemoryOffset()) : gcm::MainPtr(pGCMRenderTarget->GetMemoryOffset()));

	bool bOk = false;
	if (bSaveToMemoryBuffer)
	{
		bOk = StoreScreenShotInBuffer(pMemoryBuffer, pFinalSize, SizeOfMemoryBuffer, pSrc, width, height, pitch, quality, pSrcPhotoBuffer);
	}
	else
	{
		if (bJpeg)
		{
			bOk = grcImage::SaveRGBA8SurfaceToJPEG(fileName, quality, pSrc, width, height, pitch) > 0;
		}
		else
			bOk = grcImage::WritePNG(fileName, copyscan_nogamma, width, height, pSrc, pitch, 0);
	}

	return bOk;

#elif __D3D11

	bool bOk = false;

	grcTextureLock lock;
	pRenderTarget->LockRect(0, 0, lock);

	if (bSaveToMemoryBuffer)
	{
		bOk = StoreScreenShotInBuffer(pMemoryBuffer, pFinalSize, SizeOfMemoryBuffer, lock.Base, lock.Width, lock.Height, lock.Pitch, quality, pSrcPhotoBuffer);
	}
	else
	{
		if (bJpeg)
			bOk = grcImage::SaveRGBA8SurfaceToJPEG(fileName, quality, lock.Base, lock.Width, lock.Height, lock.Pitch) > 0;
		else
			bOk = grcImage::WritePNG(fileName, copyscan_nogamma, lock.Width, lock.Height, lock.Base, lock.Pitch, 0);
	}

	pRenderTarget->UnlockRect(lock);

	return bOk;
#elif RSG_ORBIS
	const sce::Gnm::Texture *gnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(pRenderTarget->GetTexturePtr());

	u32 bytesPerElement = gnmTexture->getDataFormat().getBytesPerElement();
	int width = gnmTexture->getWidth();
	int height = gnmTexture->getHeight();
	int pitch =  gnmTexture->getWidth() * bytesPerElement;

	u32* pSrc = NULL;

	u32 *allocatedData = NULL;
	grcTextureLock lock;

	sce::GpuAddress::TilingParameters tp;
	tp.initFromTexture(gnmTexture, 0, 0);
	if( tp.m_tileMode == sce::Gnm::kTileModeDisplay_LinearAligned )
	{
		pRenderTarget->LockRect(0, 0, lock);
		pSrc = (u32*)lock.Base;
	}
	else
	{
		Assertf(false,"Trying to save a non linear buffer");		
		uint64_t offset;
		uint64_t size;

#if SCE_ORBIS_SDK_VERSION < (0x00930020u)
		sce::GpuAddress::computeSurfaceOffsetAndSize(
#else
		sce::GpuAddress::computeTextureSurfaceOffsetAndSize(
#endif
		&offset,&size,gnmTexture,0,0);

		allocatedData = rage_new u32[size];

		sce::GpuAddress::detileSurface(pSrc, ((char*)gnmTexture->getBaseAddress() + offset),&tp);
		pSrc = allocatedData;
	}

	bool bOk = false;
	if (bSaveToMemoryBuffer)
	{
		bOk = StoreScreenShotInBuffer(pMemoryBuffer, pFinalSize, SizeOfMemoryBuffer, pSrc, width, height, pitch, quality, pSrcPhotoBuffer);
	}
	else
	{
		if (bJpeg)
			bOk = grcImage::SaveRGBA8SurfaceToJPEG(fileName, quality, pSrc, width, height, pitch) > 0;
		else
			bOk = grcImage::WritePNG(fileName, copyscan_nogamma, width, height, pSrc, pitch, 0);
	}

	if( allocatedData )
	{
		delete [] allocatedData;
	}
	else
	{
		pRenderTarget->UnlockRect(lock);
	}

	return bOk;

#endif

}

bool CHighQualityScreenshot::StoreRawScreenShot( grcRenderTarget* pRenderTarget, u8* pMemoryBuffer, size_t const SizeOfMemoryBuffer )
{
	if (!photoVerifyf(pRenderTarget, "CHighQualityScreenshot::StoreRawScreenShot - NULL render target") ||
		!photoVerifyf(pMemoryBuffer, "CHighQualityScreenshot::StoreRawScreenShot - NULL buffer") ||
		!photoVerifyf(SizeOfMemoryBuffer > 0, "CHighQualityScreenshot::StoreRawScreenShot - Invalid buffer size") )
		return false;

	bool bOk = false;

#if __D3D11

	grcTextureLock lock;
	if( pRenderTarget->LockRect(0, 0, lock) )
	{
		bOk = StoreRawScreenShotInBuffer( pMemoryBuffer, SizeOfMemoryBuffer, (u8*)lock.Base, lock.Width, lock.Height, lock.Pitch );

		pRenderTarget->UnlockRect(lock);
	}

#elif RSG_ORBIS
	const sce::Gnm::Texture *gnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(pRenderTarget->GetTexturePtr());

	u32 bytesPerElement = gnmTexture->getDataFormat().getBytesPerElement();
	int width = gnmTexture->getWidth();
	int height = gnmTexture->getHeight();
	int pitch =  gnmTexture->getWidth() * bytesPerElement;

	uint64_t offset;
	uint64_t size;

#if SCE_ORBIS_SDK_VERSION < (0x00930020u)
	sce::GpuAddress::computeSurfaceOffsetAndSize(
#else
	sce::GpuAddress::computeTextureSurfaceOffsetAndSize(
#endif
		&offset,&size,gnmTexture,0,0);

	sce::GpuAddress::TilingParameters tp;
	tp.initFromTexture(gnmTexture, 0, 0);

	u32* pSrc = rage_new u32[size];

	sce::GpuAddress::detileSurface(pSrc, ((char*)gnmTexture->getBaseAddress() + offset),&tp);
	
	bOk = StoreRawScreenShotInBuffer( pMemoryBuffer, SizeOfMemoryBuffer, pSrc, width, height, pitch );

	delete [] pSrc;

#endif

	return bOk;
}

CHighQualityScreenshot::ScreenShotState	CHighQualityScreenshot::ms_currentState = SSHOT_IDLE;
grcRenderTarget* CHighQualityScreenshot::ms_pRenderTarget = NULL;
const char* CHighQualityScreenshot::ms_pCurrentFileName = NULL;
u8* CHighQualityScreenshot::ms_pCurrentRGBBuffer = NULL;
size_t CHighQualityScreenshot::ms_rgbBufferSize = 0;

#if RSG_ORBIS || RSG_DURANGO
grcFenceHandle CHighQualityScreenshot::ms_fence = NULL;
#endif // RSG_ORBIS || RSG_DURANGO

CPhotoBuffer *CHighQualityScreenshot::ms_pPhotoBuffer = NULL;

bool CHighQualityScreenshot::ms_bSaveToMemoryBuffer = false;
bool CHighQualityScreenshot::ms_bLastScreenShotStored = false;
bool CHighQualityScreenshot::ms_bRenderPhasesDisabled = false;

#if __BANK
bool CHighQualityScreenshot::ms_bLastScreenShotFailed = false;
bool CHighQualityScreenshot::ms_bPhotoMemoryIsAllocated = false;
#endif	//	__BANK


CHighQualityScreenshot::CHighQualityScreenshot()
{
	Init(INIT_SESSION);
}

void CHighQualityScreenshot::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		ms_currentState = SSHOT_IDLE;
		ms_pRenderTarget = NULL;
		ms_pCurrentFileName = NULL;
		ms_pCurrentRGBBuffer = NULL;
		ms_rgbBufferSize = 0;

#if RSG_ORBIS || RSG_DURANGO
		ms_fence = NULL;
#endif // RSG_ORBIS || RSG_DURANGO

		ms_pPhotoBuffer = NULL;

		ms_bSaveToMemoryBuffer = false;
		ms_bLastScreenShotStored = false;

		ms_bRenderPhasesDisabled = false;

#if __BANK
		ms_bLastScreenShotFailed = false;
		ms_bPhotoMemoryIsAllocated = false;
#endif	//	__BANK
	}
}

// CHighQualityScreenshot::~CHighQualityScreenshot()
// {
// 		FreeJpegBuffer();
// }

void CHighQualityScreenshot::EnablePasses() 
{
#if __ASSERT
	// there are certain cases (e.g.: screenshot taking) where we need to disable one or several render phases (but *not* all of them) 
	// that share the same entity list; in these cases, the asserts below will trigger even though everything's fine.
	RENDERPHASEMGR.SetEntityListSanityCheck(true);
#endif

	g_CascadeShadowsNew->Enable();
	g_SceneToGBufferPhaseNew->Enable();
	g_DefLighting_LightsToScreen->Enable();

	ms_bRenderPhasesDisabled = false;
}

void CHighQualityScreenshot::DisablePasses() 
{
#if __ASSERT
	// there are certain cases (e.g.: screenshot taking) where we need to disable one or several render phases (but *not* all of them) 
	// that share the same entity list; in these cases, the asserts below will trigger even though everything's fine.
	RENDERPHASEMGR.SetEntityListSanityCheck(false);
#endif

	g_SceneToGBufferPhaseNew->Disable();
	g_CascadeShadowsNew->Disable();
	g_DefLighting_LightsToScreen->Disable();
	
	ms_bRenderPhasesDisabled = true;
}

bool CHighQualityScreenshot::ReadyToTakeScreenShot()
{
#if __PS3
	return ms_currentState == SSHOT_TAKING;
#else
	return ms_currentState == SSHOT_PENDING;
#endif
}

bool CHighQualityScreenshot::DoTakeScreenShot()
{
	// this must be called from RenderThread
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

#if __PS3
	if (ms_currentState == SSHOT_TAKING) 
	{	
		DisablePasses();
		return true;
	}
#elif __XENON
	if (ms_currentState == SSHOT_PENDING) 
	{	
		ms_currentState = SSHOT_TAKING;
		return true;
	}
#elif __WIN32PC || RSG_DURANGO || RSG_ORBIS
	if (ms_currentState == SSHOT_PENDING)
	{
		photoDisplayf("CHighQualityScreenshot::DoTakeScreenShot - about to change ms_currentState from SSHOT_PENDING to SSHOT_TAKING");
		ms_currentState = SSHOT_TAKING;
		photoDisplayf("CHighQualityScreenshot::DoTakeScreenShot - ms_currentState has been changed from SSHOT_PENDING to SSHOT_TAKING");
		return true;
	}
#endif

	return false;
}

void CHighQualityScreenshot::Update()
{
	// this must be called from MainThread
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

#if __PS3
	switch (ms_currentState) 
	{
	case (SSHOT_STORING): 
		{
			ms_currentState = SSHOT_STORED;
			break;
		}

	case (SSHOT_TAKEN): 
		{
			ms_currentState = SSHOT_STORING;
			break;
		}

	case (SSHOT_TAKING):
		{
			ms_currentState = SSHOT_TAKEN;
			break;
		}

	case (SSHOT_PENDING):
		{
			ms_currentState = SSHOT_TAKING;
			break;
		}
	}
#elif __WIN32PC || RSG_DURANGO || RSG_ORBIS
	if (ms_currentState == SSHOT_SETTING_UP_DATA)
	{
		photoDisplayf("CHighQualityScreenshot::Update - about to change ms_currentState from SSHOT_SETTING_UP_DATA to SSHOT_PENDING");
		ms_currentState = SSHOT_PENDING;
		photoDisplayf("CHighQualityScreenshot::Update - ms_currentState has been changed from SSHOT_SETTING_UP_DATA to SSHOT_PENDING");
	}
#endif

}

u32 CHighQualityScreenshot::GetDefaultQuality()
{
	return 90;
}

void CHighQualityScreenshot::GetDefaultScreenshotSize( u32& out_width, u32& out_height )
{
	VideoResManager::GetScreenshotDimensions( out_width, out_height, VideoResManager::DOWNSCALE_HALF );
}

void CHighQualityScreenshot::Execute()
{
	// this must be called from RenderThread
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

#if __PS3
	bool bStoreScreenShotFailed = false;
	if (ms_currentState == SSHOT_STORING)
	{
		if (StoreScreenShot(ms_pCurrentFileName, ms_pRenderTarget, ms_bSaveToMemoryBuffer, m_pJpegBuffer, GetSizeOfAllocatedJpegBuffer(), &m_ExactSizeOfJpegData, true) == false)
		{
			bStoreScreenShotFailed = true;
		}
	}

	if ( (ms_currentState == SSHOT_STORED) || bStoreScreenShotFailed )
	{
		EnablePasses();
		DeleteRenderTarget();

		if (ms_bSaveToMemoryBuffer)
		{
			if (bStoreScreenShotFailed)
			{
				ms_currentState = SSHOT_HAD_ERROR;
#if __BANK
				ms_bLastScreenShotFailed = true;
#endif	//	__BANK
			}
			else
			{
				ms_currentState = SSHOT_WAITING_FOR_MEMORY_BUFFER_TO_BE_FREED;
			}
		}
		else
		{
			ms_currentState = SSHOT_IDLE;
		}
		ms_pCurrentFileName = NULL;
		ms_bLastScreenShotStored = true;
	}

#elif __XENON
	bool bStoreScreenShotFailed = false;
	if (ms_currentState == SSHOT_TAKING)
	{		
		if (StoreScreenShot(ms_pCurrentFileName, ms_pRenderTarget, ms_bSaveToMemoryBuffer, m_pJpegBuffer, GetSizeOfAllocatedJpegBuffer(), &m_ExactSizeOfJpegData, true))
		{
			ms_currentState =  SSHOT_STORED;
		}
		else
		{
			bStoreScreenShotFailed = true;
		}
	}

	if ( (ms_currentState == SSHOT_STORED) || bStoreScreenShotFailed )
	{
		DeleteRenderTarget();

		if (ms_bSaveToMemoryBuffer)
		{
			if (bStoreScreenShotFailed)
			{
				ms_currentState = SSHOT_HAD_ERROR;
#if __BANK
				ms_bLastScreenShotFailed = true;
#endif	//	__BANK
			}
			else
			{
				ms_currentState = SSHOT_WAITING_FOR_MEMORY_BUFFER_TO_BE_FREED;
			}
		}
		else
		{
			ms_currentState = SSHOT_IDLE;
		}
		ms_pCurrentFileName = NULL;
		ms_bLastScreenShotStored = true;
	}
#elif __WIN32PC || RSG_DURANGO || RSG_ORBIS
	if (ms_currentState == SSHOT_TAKING)
	{
#if RSG_ORBIS || RSG_DURANGO
	Assert(ms_fence);
	if (GRCDEVICE.IsFencePending(ms_fence))
		return;
#endif // RSG_ORBIS || RSG_DURANGO

		if( ms_pCurrentRGBBuffer && ms_rgbBufferSize > 0 )
		{
			if( StoreRawScreenShot( ms_pRenderTarget, ms_pCurrentRGBBuffer, ms_rgbBufferSize ) )
			{
				ms_currentState = SSHOT_IDLE;
			}
			else
			{
				ms_currentState = SSHOT_HAD_ERROR;
#if __BANK
				ms_bLastScreenShotFailed = true;
#endif	//	__BANK
			}

			ms_pCurrentRGBBuffer = NULL;
			ms_rgbBufferSize = 0;
		}
		else
		{
			if (ms_bSaveToMemoryBuffer)
			{
				if (ms_pPhotoBuffer && StoreScreenShot(ms_pCurrentFileName, ms_pRenderTarget, ms_bSaveToMemoryBuffer, 
														ms_pPhotoBuffer->GetJpegBuffer(), ms_pPhotoBuffer->GetSizeOfAllocatedJpegBuffer(), ms_pPhotoBuffer->GetPointerToVariableForStoringExactSizeOfJpegData(), 
														true))
				{
					ms_currentState = SSHOT_WAITING_FOR_MEMORY_BUFFER_TO_BE_FREED;
				}
				else
				{
					ms_currentState = SSHOT_HAD_ERROR;
#if __BANK
					ms_bLastScreenShotFailed = true;
#endif	//	__BANK
				}
			}
			else
			{
				if (StoreScreenShot(ms_pCurrentFileName, ms_pRenderTarget, ms_bSaveToMemoryBuffer, NULL, 0, NULL, true))
				{
					ms_currentState = SSHOT_IDLE;
				}
				else
				{
					ms_currentState = SSHOT_HAD_ERROR;
#if __BANK
					ms_bLastScreenShotFailed = true;
#endif	//	__BANK
				}
			}
		}

		DeleteRenderTarget();

		ms_pCurrentFileName = NULL;
		ms_bLastScreenShotStored = true;
	}
#endif

}

#if RSG_ORBIS || RSG_DURANGO
void CHighQualityScreenshot::InsertFence()
{
	if (ms_fence)
		GRCDEVICE.GpuFreeFence(ms_fence);
	ms_fence = GRCDEVICE.AllocFenceAndGpuWrite();
}
#endif // RSG_ORBIS || RSG_DURANGO

bool CHighQualityScreenshot::TakeScreenShot(const char* pFilename, u32 width, u32 height)
{
	const bool bBufferIsAllocated = (ms_pPhotoBuffer && ms_pPhotoBuffer->BufferIsAllocated());
	photoAssertf(ms_currentState == SSHOT_IDLE, "CHighQualityScreenshot::TakeScreenShot - expected ms_currentState to be SSHOT_IDLE but it's %d", ms_currentState);
	photoAssertf(pFilename != NULL || bBufferIsAllocated, "CHighQualityScreenshot::TakeScreenShot - expected Filename or JPEG Buffer to be set");

	if ( (pFilename || bBufferIsAllocated ) && (ms_currentState == SSHOT_IDLE) && CreateRenderTarget(width, height)) 
	{
		ms_pCurrentFileName = pFilename;
		photoDisplayf("CHighQualityScreenshot::TakeScreenShot(1) - about to change ms_currentState from SSHOT_IDLE to SSHOT_SETTING_UP_DATA");
		ms_currentState = SSHOT_SETTING_UP_DATA;
		photoDisplayf("CHighQualityScreenshot::TakeScreenShot(1) - ms_currentState has been changed from SSHOT_IDLE to SSHOT_SETTING_UP_DATA");

		if (pFilename)
		{
			photoDisplayf("CHighQualityScreenshot::TakeScreenShot to file - %s - %d %d", pFilename, width, height);
			photoAssertf(bBufferIsAllocated == false, "CHighQualityScreenshot::TakeScreenShot - pFilename is not NULL so didn't expect JPEG Buffer to have been allocated");
			ms_bSaveToMemoryBuffer = false;
		}
		else
		{
			photoDisplayf("CHighQualityScreenshot::TakeScreenShot - filename NULL - %d %d", width, height);
			photoAssertf(bBufferIsAllocated, "CHighQualityScreenshot::TakeScreenShot - pFilename is NULL so expected JPEG Buffer to have been allocated");
			ms_bSaveToMemoryBuffer = true;
		}
		
#if __BANK
		ms_bLastScreenShotFailed = false;
#endif	//	__BANK
		ms_bLastScreenShotStored = false;
		return true;
	}
	else
	{
		photoAssertf(false, "Couldn't create render target.");

		ms_bLastScreenShotStored = true;
		ms_currentState = SSHOT_HAD_ERROR;
	}

	return false;
}

bool CHighQualityScreenshot::TakeScreenShot( u8* buffer, size_t bufferSize, u32 width, u32 height )
{
	photoAssertf(ms_currentState == SSHOT_IDLE, "CHighQualityScreenshot::TakeScreenShot - expected ms_currentState to be SSHOT_IDLE but it's %d", ms_currentState);
	photoAssertf(buffer != NULL && bufferSize > 0, "CHighQualityScreenshot::TakeScreenShot - expected valid buffer");

	if ( buffer && bufferSize > 0 && (ms_currentState == SSHOT_IDLE) && CreateRenderTarget(width, height)) 
	{
		photoDisplayf("CHighQualityScreenshot::TakeScreenShot to buffer - %d %d", width, height);
		ms_pCurrentRGBBuffer = buffer;
		ms_rgbBufferSize = bufferSize;
		photoDisplayf("CHighQualityScreenshot::TakeScreenShot(2) - about to change ms_currentState from SSHOT_IDLE to SSHOT_SETTING_UP_DATA");
		ms_currentState = SSHOT_SETTING_UP_DATA;
		photoDisplayf("CHighQualityScreenshot::TakeScreenShot(2) - ms_currentState has been changed from SSHOT_IDLE to SSHOT_SETTING_UP_DATA");

#if __BANK
		ms_bLastScreenShotFailed = false;
#endif	//	__BANK
		ms_bLastScreenShotStored = false;
		return true;
	}
	else
	{
		photoAssertf(false, "Couldn't create render target.");

		ms_bLastScreenShotStored = true;
		ms_currentState = SSHOT_HAD_ERROR;
	}

	return false;
}

void CHighQualityScreenshot::ResetState()
{
	photoAssertf( (ms_currentState == SSHOT_IDLE) || (ms_currentState == SSHOT_WAITING_FOR_MEMORY_BUFFER_TO_BE_FREED) || (ms_currentState == SSHOT_HAD_ERROR), 
		"CHighQualityScreenshot::ResetState - ms_currentState is %d. Expected it to be SSHOT_IDLE, SSHOT_WAITING_FOR_MEMORY_BUFFER_TO_BE_FREED or SSHOT_HAD_ERROR", ms_currentState);

	ms_currentState = SSHOT_IDLE;
	ms_bLastScreenShotStored = false;
}

bool CHighQualityScreenshot::CheckScreenshotDimensionsFitWithinDeviceDimensions(u32 width, u32 height)
{
	if (width > GRCDEVICE.GetWidth() || height > GRCDEVICE.GetHeight())
	{
		return false;
	}

	return true;
}

bool CHighQualityScreenshot::CreateRenderTarget(u32 width, u32 height)
{
	if (!CheckScreenshotDimensionsFitWithinDeviceDimensions(width, height)) {
		photoAssertf(0, "CHighQualityScreenshot::CreateRenderTarget: screenshot dimensions (w: %u h: %u) cannot be larger than the device's (w: %u h: %u)", width, height, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight());
		return false;
	}

	grcRenderTarget* pScreenShotTarget = NULL;

#if __XENON

	grcTextureFactory::CreateParams qparams;	
	qparams.Format		= grctfA8R8G8B8;
	qparams.HasParent	= true;
	qparams.Parent		= grcTextureFactory::GetInstance().GetBackBufferDepth(false);

	pScreenShotTarget	= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0, "ScreenShot", grcrtPermanent, width, height, 32, &qparams, kScreenShotHeap);
	pScreenShotTarget->AllocateMemoryFromPool();

#elif __PS3

	grcTextureFactory::CreateParams params;
	params.HasParent			= true;
	params.Parent				= NULL;
	params.EnableCompression	= false;
	params.Format				= grctfA8R8G8B8;

	pScreenShotTarget	= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "ScreenShot", grcrtPermanent, width, height, 32, &params, 8);
	pScreenShotTarget->AllocateMemoryFromPool();

#elif __WIN32PC || RSG_DURANGO || RSG_ORBIS

	grcTextureFactory::CreateParams params;
	params.Multisample  = 0;
	params.Format       = grctfA8B8G8R8;
	params.Lockable     = true;
#if RSG_ORBIS
	params.ForceNoTiling = true;
#endif // RSG_ORBIS
	pScreenShotTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Screenshot ", grcrtPermanent, width, height, 32, &params, 0);

#endif

	if (pScreenShotTarget) 
	{

		CHighQualityScreenshot::SetRenderTarget(pScreenShotTarget);
		return true;
	}

	return false;

}
void CHighQualityScreenshot::DeleteRenderTarget() 
{
	if (ms_pRenderTarget)
	{
		ms_pRenderTarget->ReleaseMemoryToPool();
		ms_pRenderTarget->Release();
	}
}


bool CHighQualityScreenshot::BeginTakeHighQualityPhoto(CPhotoBuffer *pPhotoBuffer, u32 screenshotScale WIN32PC_ONLY(, bool bUseConsoleResolution))
{
	VideoResManager::eDownscaleFactor scaling( VideoResManager::DOWNSCALE_HALF );

	switch (screenshotScale)
	{
	case 8:
		scaling = VideoResManager::DOWNSCALE_EIGHTH;
		break;
	case 4:
		scaling = VideoResManager::DOWNSCALE_QUARTER;
		break;
	case 2:
		scaling = VideoResManager::DOWNSCALE_HALF;
		break;
	case 1 :
		scaling = VideoResManager::DOWNSCALE_ONE;
		break;
	default :
		photoAssertf(0, "CHighQualityScreenshot::BeginTakeHighQualityPhoto - expected screenshotScale parameter to be 1, 2, 4 or 8");
		break;
	}

	if (pPhotoBuffer)
	{
		ms_pPhotoBuffer = pPhotoBuffer;
		if (!ms_pPhotoBuffer->AllocateBufferForTakingScreenshot())
		{	//	Allocation failed. It's up to the caller whether to try again.
			photoAssertf(0, "CHighQualityScreenshot::BeginTakeHighQualityPhoto - failed to allocate a buffer for the jpeg");
			return false;
		}
	}
	else
	{
		photoAssertf(0, "CHighQualityScreenshot::BeginTakeHighQualityPhoto - pPhotoBuffer parameter is NULL");
		return false;
	}


	ms_bLastScreenShotStored = false;

	{
#if __BANK
		ms_bPhotoMemoryIsAllocated = true;
#endif	//	__BANK

		u32 width = 0;
		u32 height = 0;

		bool bFindValidDimensions = true;
		bool bValidDimensionsHaveBeenFound = false;

		while (bFindValidDimensions)
		{
#if RSG_PC
			if (bUseConsoleResolution)
			{
				VideoResManager::GetScreenshotDimensionsOfConsoles( width, height, scaling );
			}
			else
#endif
			{
				VideoResManager::GetScreenshotDimensions( width, height, scaling );
			}

			if( scaling != VideoResManager::DOWNSCALE_ONE )
			{
				// Hi-quality photos can be further downscaled when later loaded. If we've used any scaling other than one,
				// then we can't guarantee the later loaded picture can be correctly scaled. As such, constrain the existing scale
				// by the maximum scale we can apply later
				VideoResManager::ConstrainScreenshotDimensions( width, height, GetDownscaleForNoneMaximizedView() );
			}

			if (CheckScreenshotDimensionsFitWithinDeviceDimensions(width, height))
			{
				bValidDimensionsHaveBeenFound = true;
				bFindValidDimensions = false;
			}
			else
			{
				switch (scaling)
				{
					case VideoResManager::DOWNSCALE_ONE :
						scaling = VideoResManager::DOWNSCALE_HALF;
						break;

					case VideoResManager::DOWNSCALE_HALF :
						scaling = VideoResManager::DOWNSCALE_QUARTER;
						break;

					case VideoResManager::DOWNSCALE_QUARTER :
						scaling = VideoResManager::DOWNSCALE_EIGHTH;
						break;

					case VideoResManager::DOWNSCALE_EIGHTH :
					default :
						{	//	There aren't any more scaling values to try so give up
							bValidDimensionsHaveBeenFound = false;
							bFindValidDimensions = false;
						}
						break;
				}
			}
		}

		if (bValidDimensionsHaveBeenFound)
		{
			// set a flag here to say "take a photo"
			return TakeScreenShot( NULL, width, height );
		}
		else
		{
			return false;
		}
	}

	//	Allocation failed. It's up to the caller whether to try again.
//	return false;
}


void CHighQualityScreenshot::ResetStateForHighQualityPhoto(CPhotoBuffer *pPhotoBuffer)
{
	if (ms_pPhotoBuffer == pPhotoBuffer)
	{
		ms_pPhotoBuffer = NULL;

#if __BANK
		ms_bPhotoMemoryIsAllocated = false;
#endif	//	__BANK

		ResetState();
	}
}

void CHighQualityScreenshot::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
//		FreeMemoryForHighQualityPhoto();
		ResetState();
	}
}

#if __BANK
void CHighQualityScreenshot::AddWidgets(bkBank& bank)
{
	bank.PushGroup("High Quality Photo");
	
//	datCallback(MFA1(fn), obj, data, true)   ->       obj->fn(data)
//		bank.AddButton("Take Photo", datCallback(CFA1(CHighQualityScreenshot::BeginTakeHighQualityPhoto), (datBase*)this, NULL, true));
		bank.AddSlider("Take Photo State (read-only)", (s32*) &ms_currentState, 0, (SSHOT_HAD_ERROR+1), 1);
		bank.AddToggle("Take Photo Finished (read-only)", &ms_bLastScreenShotStored);
		bank.AddToggle("Take Photo Failed (read-only)", &ms_bLastScreenShotFailed);

		bank.AddToggle("Photo Memory Is Allocated (read-only)", &ms_bPhotoMemoryIsAllocated);
//		bank.AddButton("Free Photo Memory", datCallback(CFA(CHighQualityScreenshot::FreeMemoryForHighQualityPhoto), (datBase*)this));
	bank.PopGroup();
}
#endif	//	__BANK

// **************************** CLowQualityScreenshot ****************************

// CLowQualityScreenshot::eCopyLowQualityPhotoState CLowQualityScreenshot::m_LowQualityCopyState = LOW_QUALITY_COPY_SUCCEEDED;
// bool CLowQualityScreenshot::m_bFreeMemoryForLowQualityPhoto = false;
// bool CLowQualityScreenshot::m_bCreateLowQualityCopyOfPhoto = false;
// 
// const u8 *CLowQualityScreenshot::m_pMemoryBufferToCopyHighQualityPhotoFrom = NULL;
// u32 CLowQualityScreenshot::m_SizeOfMemoryBufferToCopyHighQualityPhotoFrom = 0;
// 
// grcTexture *CLowQualityScreenshot::m_pTextureForLowQualityPhoto = NULL;
// void *CLowQualityScreenshot::m_pBufferForLowQualityPhoto = NULL;
// 
// CLowQualityScreenshot::ePhotoRotation CLowQualityScreenshot::m_PhotoRotation = PHOTO_NO_ROTATION;
// bool CLowQualityScreenshot::m_bRenderToPhone = false;
// 
// #if __BANK
// bool CLowQualityScreenshot::m_bRenderDebug = false;
// bool CLowQualityScreenshot::m_bCopyHasFinished = false;
// bool CLowQualityScreenshot::m_bCopyHasFailed = false;
// bool CLowQualityScreenshot::m_bMemoryHasBeenAllocatedForCopy = false;
// 
// #if __OLD_SCREENSHOT_DEBUG_CODE
// char CLowQualityScreenshot::m_textureName[128] = {0};
// bool CLowQualityScreenshot::m_bLoadJpegFromFile = false;
// #endif	//	__OLD_SCREENSHOT_DEBUG_CODE
// 
// #endif	//	__BANK


CLowQualityScreenshot::CLowQualityScreenshot()
{
	m_LowQualityCopyState = LOW_QUALITY_COPY_SUCCEEDED;
	m_bFreeMemoryForLowQualityPhoto = false;
	m_bCreateLowQualityCopyOfPhoto = false;

	m_pMemoryBufferToCopyHighQualityPhotoFrom = NULL;
	m_SizeOfMemoryBufferToCopyHighQualityPhotoFrom = 0;

	m_pTextureForLowQualityPhoto = NULL;
	m_pBufferForLowQualityPhoto = NULL;

	m_PhotoRotation = PHOTO_NO_ROTATION;
	m_bRenderToPhone = false;

#if __BANK
	m_bRenderDebug = false;
	m_bCopyHasFinished = false;
	m_bCopyHasFailed = false;
	m_bMemoryHasBeenAllocatedForCopy = false;

	m_bForceMemoryAllocationToFail = false;

#if __OLD_SCREENSHOT_DEBUG_CODE
	m_textureName[128] = {0};
	m_bLoadJpegFromFile = false;
#endif	//	__OLD_SCREENSHOT_DEBUG_CODE

#endif	//	__BANK
}

bool CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto(CPhotoBuffer *pPhotoBuffer, u32 desiredWidth, u32 desiredHeight)
{
	//	Create smaller texture for displaying on the phone
	if (photoVerifyf(pPhotoBuffer, "CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto - pPhotoBuffer is NULL"))
	{
		if (photoVerifyf(pPhotoBuffer->GetJpegBuffer() && CHighQualityScreenshot::HasScreenShotFinished() && !CHighQualityScreenshot::HasScreenShotFailed(), "CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto - there is no high quality photo to copy"))
		{
			return BeginCreateLowQualityCopyOfPhoto(pPhotoBuffer->GetJpegBuffer(), pPhotoBuffer->GetExactSizeOfJpegData(), desiredWidth, desiredHeight);
		}
	}

	return false;
}


bool CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhoto(const u8 *pSourceBuffer, u32 SizeOfSourceBuffer, u32 desiredWidth, u32 desiredHeight)
{
	if (m_pBufferForLowQualityPhoto)
	{
		photoAssertf(0, "CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhoto - expected m_pBufferForLowQualityPhoto to be NULL at this point");
		RequestFreeMemoryForLowQualityPhoto();
		FreeMemoryForLowQualityPhoto();
	}

	m_pMemoryBufferToCopyHighQualityPhotoFrom = pSourceBuffer;
	m_SizeOfMemoryBufferToCopyHighQualityPhotoFrom = SizeOfSourceBuffer;

	m_desiredWidthOfLowQualityCopy = desiredWidth;
	m_desiredHeightOfLowQualityCopy = desiredHeight;
	m_bCreateLowQualityCopyOfPhoto = true;

	m_LowQualityCopyState = LOW_QUALITY_COPY_IN_PROGRESS;

#if __BANK
	m_bCopyHasFinished = false;
	m_bCopyHasFailed = false;
#endif	//	__BANK

	return true;
}

void CLowQualityScreenshot::RequestFreeMemoryForLowQualityPhoto()
{
 	m_bFreeMemoryForLowQualityPhoto = true;
}

void CLowQualityScreenshot::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{	//	Is it safe to do this?
		//	Can I rely on the render thread to call FreeMemoryForLowQualityPhoto() while the session is shutting down?
		//	FreeMemoryForLowQualityPhoto() is actually called at the start of the next session. It seems to work though
		RequestFreeMemoryForLowQualityPhoto();
	}
}

void CLowQualityScreenshot::ClearGrcTexturePointerWithoutDeleting()
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CLowQualityScreenshot::ClearGrcTexturePointerWithoutDeleting can only be called from update thread");

	m_pBufferForLowQualityPhoto = NULL;
	m_pTextureForLowQualityPhoto = NULL;
}

void CLowQualityScreenshot::FreeMemoryForLowQualityPhoto()
{
	if (m_bFreeMemoryForLowQualityPhoto == false)
	{
		return;
	}
	m_bFreeMemoryForLowQualityPhoto = false;

	if (m_pBufferForLowQualityPhoto)
	{
		MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
		strStreamingEngine::GetAllocator().Free(m_pBufferForLowQualityPhoto);
		m_pBufferForLowQualityPhoto = NULL;

		photoDisplayf("CLowQualityScreenshot::FreeMemoryForLowQualityPhoto - buffer has been freed");

#if __BANK
		m_bMemoryHasBeenAllocatedForCopy = false;
#endif
	}

	if (m_pTextureForLowQualityPhoto) 
	{
		m_pTextureForLowQualityPhoto->Release();
	}

	m_pTextureForLowQualityPhoto = NULL;
}

void CLowQualityScreenshot::CreateLowQualityCopyOfPhoto()
{
	if (m_bCreateLowQualityCopyOfPhoto == false)
	{
		return;
	}

	m_bCreateLowQualityCopyOfPhoto = false;

	char memFileName[RAGE_MAX_PATH];
	fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, m_pMemoryBufferToCopyHighQualityPhotoFrom, m_SizeOfMemoryBufferToCopyHighQualityPhotoFrom, false, "");
	fiStream *pStreamToGetInfo = ASSET.Create(memFileName, "");

	u32 w, h, pitch, textureDataSize;
	if (pStreamToGetInfo && GetJPEGInfoForRGBA8Texture(pStreamToGetInfo, w, h, pitch))
	{
		RequestFreeMemoryForLowQualityPhoto();
		FreeMemoryForLowQualityPhoto();

		photoDisplayf("CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - Values returned from GetJPEGInfoForRGBA8Texture: w = %u, h = %u, pitch = %u", w, h, pitch);

		const bool bAllocateFromVirtual = (RSG_DURANGO ? false : true);

		u32 desiredWidth = m_desiredWidthOfLowQualityCopy;
		u32 desiredHeight = m_desiredHeightOfLowQualityCopy;
//		u32 scalingDenom = 
		grcImage::GetSizesOfClosestAvailableScaleDenom(w, h, desiredWidth, desiredHeight, textureDataSize, !bAllocateFromVirtual);

#if RSG_DURANGO
		// temporary fix for 1689747 - avoid crashing (GetTextureDataSize is not implemented)
		textureDataSize = pitch*h + 64*1024;
#endif

//		Displayf("Closest Available: Width = %u, Height = %u, textureDataSize = %u, scalingDenom = %u", desiredWidth, desiredHeight, textureDataSize, scalingDenom);
#if !RSG_ORBIS
		size_t size = pgRscBuilder::ComputeLeafSize(textureDataSize, !bAllocateFromVirtual);
		//		Displayf("size = %d", size);
#endif


#if __BANK
		if (m_bForceMemoryAllocationToFail)
		{
			photoDisplayf("CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - widget is ticked to force memory allocation to fail");
		}
		else
#endif	//	__BANK
#if !RSG_ORBIS
		{
			MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
			m_pBufferForLowQualityPhoto = strStreamingEngine::GetAllocator().Allocate(size, 16, bAllocateFromVirtual?MEMTYPE_RESOURCE_VIRTUAL:MEMTYPE_RESOURCE_PHYSICAL);
		}

		if (m_pBufferForLowQualityPhoto == NULL)
		{
			m_LowQualityCopyState = LOW_QUALITY_COPY_FAILED;
#if __BANK
			m_bCopyHasFinished = true;
			m_bCopyHasFailed = true;
#endif	//	__BANK
			photoAssertf(0, "CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - failed to allocate buffer for texture");
			return;
		}
		else
		{
			photoDisplayf("CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - %" SIZETFMT "d bytes have been allocated for the buffer", size);
		}
#else //!RSG_ORBIS
		m_pBufferForLowQualityPhoto = NULL;
#endif //!RSG_ORBIS
#if __BANK
		m_bMemoryHasBeenAllocatedForCopy = true;
#endif

		fiStream *pStreamToLoad = ASSET.Create(memFileName, "");

		m_pTextureForLowQualityPhoto = LoadJPEGIntoRGBA8Texture(pStreamToLoad, m_pBufferForLowQualityPhoto, desiredWidth, desiredHeight, bAllocateFromVirtual);

		if (photoVerifyf(m_pTextureForLowQualityPhoto, "Failed to create texture"))
		{

#if __D3D11
			((grcTextureD3D11*)m_pTextureForLowQualityPhoto)->UpdateGPUCopy(0, 0, m_pBufferForLowQualityPhoto);
#endif

#if !RSG_ORBIS
//			photoDisplayf("Physical size of texture = %u", m_pTextureForLowQualityPhoto->GetPhysicalSize());
			photoAssertf(m_pTextureForLowQualityPhoto->GetPhysicalSize() <= size, "CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - physical size of texture (%u) is greater than the buffer allocated for it (%" SIZETFMT "u)", m_pTextureForLowQualityPhoto->GetPhysicalSize(), size);
#endif// !RSG_ORBIS

#if __XENON
			//	Luis had a look at this. It seems that GetPhysicalSize() on the 360 returns (w*h*4) rather than (pitch*h) so the returned value is actually less than the number of bytes used
			photoAssertf(m_pTextureForLowQualityPhoto->GetPhysicalSize() <= textureDataSize, "CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - expected the physical size of the texture (%u) to be less than or equal to the size returned by GetSizesOfClosestAvailableScaleDenom (%u)", m_pTextureForLowQualityPhoto->GetPhysicalSize(), textureDataSize);
#elif __PS3
			photoAssertf(m_pTextureForLowQualityPhoto->GetPhysicalSize() == textureDataSize, "CLowQualityScreenshot::CreateLowQualityCopyOfPhoto - expected the physical size of the texture (%u) to match the size returned by GetSizesOfClosestAvailableScaleDenom (%u)", m_pTextureForLowQualityPhoto->GetPhysicalSize(), textureDataSize);
#elif __WIN32PC
			// The texture always has a physical size of 0 on PC?
			// It does, however, seem to work correctly.
#endif

			m_LowQualityCopyState = LOW_QUALITY_COPY_SUCCEEDED;
#if __BANK
			m_bCopyHasFinished = true;
			m_bCopyHasFailed = false;
#endif	//	__BANK
		}
		else
		{
			m_LowQualityCopyState = LOW_QUALITY_COPY_FAILED;
#if __BANK
			m_bCopyHasFinished = true;
			m_bCopyHasFailed = true;
#endif	//	__BANK
		}
	}
	else
	{
		m_LowQualityCopyState = LOW_QUALITY_COPY_FAILED;
#if __BANK
		m_bCopyHasFinished = true;
		m_bCopyHasFailed = true;
#endif	//	__BANK
	}
}


bool CLowQualityScreenshot::GetJPEGInfoForRGBA8Texture(const char* pFilename, u32& width, u32& height, u32& pitch)
{
	return grcImage::GetJPEGInfoForRGBA8Surface(pFilename, width, height, pitch);
}

bool CLowQualityScreenshot::GetJPEGInfoForRGBA8Texture(fiStream* pStream, u32& width, u32& height, u32& pitch)
{
	return grcImage::GetJPEGInfoForRGBA8Surface(pStream, width, height, pitch);
}

grcTexture* CLowQualityScreenshot::LoadJPEGIntoRGBA8Texture(const char* pFilename, void* pDstMem, u32 desiredWidth, u32 desiredHeight, bool bIsSystemMemory)
{
	// this must be called from RenderThread
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CHighQualityScreenshot::LoadJPEGIntoRGBA8Texture can only be called from render thread");

	grcTexture *pReturnTexture = grcImage::LoadJPEGToRGBA8Surface(pFilename, pDstMem, desiredWidth, desiredHeight, bIsSystemMemory);

	return pReturnTexture;
}

grcTexture* CLowQualityScreenshot::LoadJPEGIntoRGBA8Texture(fiStream* pStream, void* pDstMem, u32 desiredWidth, u32 desiredHeight, bool bIsSystemMemory)
{
	// this must be called from RenderThread
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CHighQualityScreenshot::LoadJPEGIntoRGBA8Texture can only be called from render thread");

	grcTexture *pReturnTexture = grcImage::LoadJPEGToRGBA8Surface(pStream, pDstMem, desiredWidth, desiredHeight, bIsSystemMemory);

	return pReturnTexture;
}


void CLowQualityScreenshot::Execute()
{
	// this must be called from RenderThread
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	FreeMemoryForLowQualityPhoto();

#if __BANK && __OLD_SCREENSHOT_DEBUG_CODE
	DebugLoadJPEGFromFile();
#endif	//	__BANK && __OLD_SCREENSHOT_DEBUG_CODE

	CreateLowQualityCopyOfPhoto();
}


//	No rotation
// 0,0
// 0,1
// 1,1
// 1,0

//	Clockwise
// 0,1
// 1,1
// 1,0
// 0,0

//	Anticlockwise
// 1,0
// 0,0
// 0,1
// 1,1

float s_TopLeftX[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 0.0f, 0.0f, 1.0f };
float s_TopLeftY[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 0.0f, 1.0f, 0.0f };

float s_BottomLeftX[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 0.0f, 1.0f, 0.0f };
float s_BottomLeftY[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 1.0f, 1.0f, 0.0f };

float s_BottomRightX[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 1.0f, 1.0f, 0.0f };
float s_BottomRightY[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 1.0f, 0.0f, 1.0f };

float s_TopRightX[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 1.0f, 0.0f, 1.0f };
float s_TopRightY[CLowQualityScreenshot::PHOTO_MAX_ROTATIONS] = { 0.0f, 0.0f, 1.0f };


//	Graeme - I copied grcDrawQuadf and modified it to allow rotation by 90 degrees. This is so that when the phone is in landscape mode, the photo can still be displayed upright
void CLowQualityScreenshot::DrawPhotoQuad(float x1, float y1, float x2, float y2, float zVal, ePhotoRotation photoRotation, const Color32 &color32)
{
	grcBegin(drawTriStrip,4);

	if ( (photoRotation < 0) || (photoRotation >= PHOTO_MAX_ROTATIONS) )
	{
		photoRotation = PHOTO_NO_ROTATION;
	}

	grcVertex(x1, y1, zVal, 0,0,0, color32,s_TopLeftX[photoRotation],s_TopLeftY[photoRotation]);
	grcVertex(x1, y2, zVal, 0,0,0, color32,s_BottomLeftX[photoRotation],s_BottomLeftY[photoRotation]);
	grcVertex(x2, y1, zVal, 0,0,0, color32,s_TopRightX[photoRotation],s_TopRightY[photoRotation]);
	grcVertex(x2, y2, zVal, 0,0,0, color32,s_BottomRightX[photoRotation],s_BottomRightY[photoRotation]);

	grcEnd();
}


void CLowQualityScreenshot::Render(bool bRenderToPhone)
{
	DLC_Add(StaticRender, &m_pTextureForLowQualityPhoto, m_PhotoRotation, bRenderToPhone);
}

float s_phonePhotoOffset = 0.1f;

void CLowQualityScreenshot::StaticRender(grcTexture **ppTexture, ePhotoRotation photoRotation, bool bRenderToPhone)
{
	//	FreeMemoryForLowQualityPhoto();	//	DebugReleaseJPEG();
	//	DebugLoadJPEGFromFile();
	//	CreateLowQualityCopyOfPhoto();	//	DebugLoadJPEGFromMemory();

	if ( (ppTexture == NULL) || (*ppTexture == NULL) )
	{
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PUSH_DEFAULT_SCREEN();
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcBindTexture(*ppTexture);
	CShaderLib::SetGlobalEmissiveScale(1.0f, true);

	if (bRenderToPhone)
	{
		bool prevLit = grcLightState::IsEnabled();
		grcLightState::SetEnabled(false);

		GRCDEVICE.Clear(true, Color32(0,0,0), false, 0.0f, false);
		const float fScreenWidth = (float)VideoResManager::GetNativeWidth();
		const float fScreenHeight = (float)VideoResManager::GetNativeHeight();
		const float fOffset = s_phonePhotoOffset*fScreenWidth;

		DrawPhotoQuad(fOffset, 0.0f, fScreenWidth-fOffset, fScreenHeight,0, photoRotation, Color32(255,255,255,255));
		grcLightState::SetEnabled(prevLit);
	}
	else
	{
		DrawPhotoQuad(600,50,800,250,0, photoRotation, Color32(255,255,255,255));
	}
	POP_DEFAULT_SCREEN();
}

#if __BANK
void CLowQualityScreenshot::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Low Quality Photo");
	bank.AddSlider("Offset", &s_phonePhotoOffset, -1280.0f, 1280.0f, 0.0001f);
	
//	bank.AddButton("Start Creating Low Quality Copy", datCallback(MFA(CLowQualityScreenshot::BeginCreateLowQualityCopyOfPhotoFromHighQualityPhoto), (datBase*)this));
	bank.AddSlider("Copy State (read-only)", (s32*) &m_LowQualityCopyState, LOW_QUALITY_COPY_SUCCEEDED, (LOW_QUALITY_COPY_FAILED+1), 1);
	bank.AddToggle("Copy has finished (read-only)", &m_bCopyHasFinished);
	bank.AddToggle("Copy has failed (read-only)", &m_bCopyHasFailed);

	bank.AddToggle("Render Copy to screen", &m_bRenderDebug);
	bank.AddToggle("Render Copy to Phone", &m_bRenderToPhone);

	bank.AddToggle("Memory for Copy has been Allocated (read-only)", &m_bMemoryHasBeenAllocatedForCopy);
	bank.AddButton("Free Memory for Low Quality Copy", datCallback(MFA(CLowQualityScreenshot::RequestFreeMemoryForLowQualityPhoto), (datBase*)this));

#if __OLD_SCREENSHOT_DEBUG_CODE
	bank.AddText("JPEG Name:", &m_textureName[0], 128, false, NullCB);
	bank.AddButton("Load JPEG",  CFA1(CLowQualityScreenshot::DebugRequestLoadJPEGFromFile));
	bank.AddButton("Release JPEG",  CFA(CLowQualityScreenshot::RequestFreeMemoryForLowQualityPhoto));
#endif	//	__OLD_SCREENSHOT_DEBUG_CODE

	bank.AddSlider("Photo Rotation", (s32*) &m_PhotoRotation, 0, 2, 1);

//	pBank->AddSlider("Profile Display Height", &ms_ProfileDisplayStartRow, 0, 60, 1.0f);

	bank.PopGroup();
}

#if __OLD_SCREENSHOT_DEBUG_CODE
void CLowQualityScreenshot::DebugRequestLoadJPEGFromFile()
{
	m_bLoadJpegFromFile = true;
}

void CLowQualityScreenshot::DebugLoadJPEGFromFile()
{
	if (m_bLoadJpegFromFile == false) 
	{
		return;
	}

	m_bLoadJpegFromFile = false;

	u32 w, h, pitch;
	const char* pName = &m_textureName[0];

	if (pName && GetJPEGInfoForRGBA8Texture(pName, w, h, pitch))
	{
//		DebugRequestReleaseJPEG();
//		DebugReleaseJPEG();
		RequestFreeMemoryForLowQualityPhoto();
		FreeMemoryForLowQualityPhoto();

		size_t size = pgRscBuilder::ComputeLeafSize(h*pitch, true);

		{
			MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
			m_pBufferForLowQualityPhoto = strStreamingEngine::GetAllocator().Allocate(size, 16, MEMTYPE_RESOURCE_VIRTUAL);
		}

		if (m_pBufferForLowQualityPhoto == NULL)
		{
			photoAssertf(0, "CLowQualityScreenshot::DebugLoadJPEGFromFile - failed to allocate buffer for texture");
			return;
		}

#if __BANK
		m_bMemoryHasBeenAllocatedForCopy = true;
#endif

		m_pTextureForLowQualityPhoto = LoadJPEGIntoRGBA8Texture(pName, m_pBufferForLowQualityPhoto, w, h, true);
	}
}

#endif	//	__OLD_SCREENSHOT_DEBUG_CODE

#endif	//	__BANK

