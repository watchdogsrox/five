

#include "DownloadableTextureManager.h"

#include "data/resourceheader.h"
#include "file/asset.h"
#include "file/cachepartition.h"
#include "file/device.h"
#include "file/stream.h"
#include "fwdrawlist/drawlist.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"
#include "grcore/debugdraw.h"
#include "grcore/im.h"
#include "grcore/image.h"
#include "grcore/image_dxt.h"
#include "grcore/orbisdurangoresourcebase.h"
#include "grcore/quads.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"

#include "math/amath.h"
#include "paging/rscbuilder.h"
#include "parser/psofile.h"
#include "parser/psoparserbuilder.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "scene/dlc_channel.h"
#include "streaming/streamingallocator.h"
#include "streaming/streamingengine.h"
#include "system/endian.h"
#include "system/interlocked.h"
#include "system/lockfreering.h"
#include "system/memory.h"
#include "system/messagequeue.h"
#include "system/param.h"
#include "system/performancetimer.h"
#include "system/system.h"
#include "system/threadpool.h"
#include "system/xtl.h"
#include "script/script.h"

#if __D3D11
#include "grcore/texturefactory_d3d11.h"
#endif //__D3D11

#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"

#if __BANK
#include "grcore/dds.h"
#include "grcore/image_dxt.h"
#endif

#if __BANK
PARAM(downloadabletexturedebug,"Output debug info");
#endif

NETWORK_OPTIMISATIONS();

#define DECODE_JPEG_TO_DXT1 (RSG_DURANGO || RSG_ORBIS || RSG_PC)

using namespace rage;

RAGE_DEFINE_SUBCHANNEL(dlc, dtm);
#undef __dlc_channel
#define __dlc_channel dlc_dtm

static sysThreadPool::Thread s_TexMgrThread;
sysThreadPool s_TexMgrThreadPool;

#if __ASSERT
bool CDownloadableTextureManager::sm_hackMainThread = false;
#endif

// In order to make sure this structure doesn't spiral out of control, memory-wise, we'll set an upper limit to the number of
// textures we can manage.

const int MAX_QUEUED_TO_DECODE = CDownloadableTextureManager::MAX_DOWNLOAD_REQUESTS;

CDownloadableTextureManager *CDownloadableTextureManager::sm_Instance;

#if RSG_DURANGO || RSG_ORBIS
static const int THREAD_CPU_ID = 5;
#else
static const int THREAD_CPU_ID = 0;
#endif

static const int MAX_SCANLINES_TO_DECODE_PER_SLICE = 4;

// --- Local Helper Functions ----------------------------------------------
// There's no error checking: this callback assumes all inputs are correct
static bool CompressRGBA8ChunkToDXT1(void* pDst, const void* pSrc,  u32 dstPitch, u32 srcPitch, u32 numDxtBlocksBatched)
{

	// Block size divided by 4
	const u32 dstStep = (16*(4))/(8*4);

	for (u32 curBlock = 0; curBlock < numDxtBlocksBatched; curBlock++)
	{
		// Cache destination row
		u8* pDstRow = (u8*)pDst + curBlock*dstPitch;

		// Cache source rows
		const u8* srcRow0 = (const u8*)pSrc + (curBlock + 0)*srcPitch;
		const u8* srcRow1 = (const u8*)pSrc + (curBlock + 1)*srcPitch;
		const u8* srcRow2 = (const u8*)pSrc + (curBlock + 2)*srcPitch;
		const u8* srcRow3 = (const u8*)pSrc + (curBlock + 3)*srcPitch;

		const u32 w = srcPitch/4;

		for (u32 x = 0; x < w; x += 4)
		{
			DXT::ARGB8888 temp[16];

			temp[0x00] = ((const DXT::ARGB8888*)srcRow0)[x + 0];
			temp[0x01] = ((const DXT::ARGB8888*)srcRow0)[x + 1];
			temp[0x02] = ((const DXT::ARGB8888*)srcRow0)[x + 2];
			temp[0x03] = ((const DXT::ARGB8888*)srcRow0)[x + 3];
			temp[0x04] = ((const DXT::ARGB8888*)srcRow1)[x + 0];
			temp[0x05] = ((const DXT::ARGB8888*)srcRow1)[x + 1];
			temp[0x06] = ((const DXT::ARGB8888*)srcRow1)[x + 2];
			temp[0x07] = ((const DXT::ARGB8888*)srcRow1)[x + 3];
			temp[0x08] = ((const DXT::ARGB8888*)srcRow2)[x + 0];
			temp[0x09] = ((const DXT::ARGB8888*)srcRow2)[x + 1];
			temp[0x0a] = ((const DXT::ARGB8888*)srcRow2)[x + 2];
			temp[0x0b] = ((const DXT::ARGB8888*)srcRow2)[x + 3];
			temp[0x0c] = ((const DXT::ARGB8888*)srcRow3)[x + 0];
			temp[0x0d] = ((const DXT::ARGB8888*)srcRow3)[x + 1];
			temp[0x0e] = ((const DXT::ARGB8888*)srcRow3)[x + 2];
			temp[0x0f] = ((const DXT::ARGB8888*)srcRow3)[x + 3];

		#if !RSG_PC && !RSG_DURANGO
		#if __XENON
			// argb -> rgba
			for (int i = 0; i < 16; i++)
			{
				const u8 tmp = temp[i].a;
				temp[i].a = temp[i].r;
				temp[i].r = temp[i].g;
				temp[i].g = temp[i].b;
				temp[i].b = tmp;
			}
		#else
			// argb -> bgra
			for (int i = 0; i < 16; i++)
			{
				const u8 tmp1 = temp[i].a;
				temp[i].a = temp[i].b;
				temp[i].b = tmp1;
				const u8 tmp2 = temp[i].r;
				temp[i].r = temp[i].g;
				temp[i].g = tmp2;
			}
		#endif
		#endif

			stb_compress_dxt_block(pDstRow + x*dstStep, (const u8*)temp, 0, STB_DXT_HIGHQUAL);
		}

	}

	return true;
}

// --- Structure/Class Definitions ----------------------------------------------

struct DecodeTextureEntry 
{
	CTextureDownloadRequest*	pRequest;
	DecodeTextureEntry() : pRequest(NULL) {}
};


static sysMessageQueue<DecodeTextureEntry, MAX_QUEUED_TO_DECODE>	s_TexturesToDecode;

// This job generates a grcTexture from a buffer previously downloaded from the cloud.
// The memory for the texture data has been previously allocated.
class CDecodeTextureFromBlobWorkJob : public sysThreadPool::WorkItem
{
public:

	CDecodeTextureFromBlobWorkJob()
	{
	#if __BANK
		m_DecodingTimer = rage_new sysPerformanceTimer("Downloadable Texture Decoding");
	#endif
	}

	~CDecodeTextureFromBlobWorkJob()
	{
	#if __BANK
		delete m_DecodingTimer;
		m_DecodingTimer = NULL;
	#endif
	}

	virtual void DoWork()
	{
		DecodeTextureEntry toDecode;

#if __D3D11 && RSG_PC
		grcTextureD3D11::SetInsertUpdateCommandIntoDrawListFunc(dlCmdGrcDeviceUpdateBuffer::AddTextureUpdate);
		grcTextureD3D11::SetCancelPendingUpdateGPUCopy(dlCmdGrcDeviceUpdateBuffer::CancelBufferUpdate);
#endif //__D3D11 && RSG_PC

		while (s_TexturesToDecode.GetHead(toDecode))
		{
			// Grab the next decoding request
			toDecode = s_TexturesToDecode.Pop();
		
			// Assert the request has been locked (i.e.: the DTM code promises not to modify the entry while m_InProcessFlag is IN_PROCESS_FLAG_ACQUIRED);
			CTextureDownloadRequest* pRequest = toDecode.pRequest;

			// Don't do anything if the request entry hasn't been locked (this shouldn't happen!)
			if (dlcVerifyf(sysInterlockedRead(&(pRequest->m_InProcessFlag)) == IN_PROCESS_FLAG_ACQUIRED, "CDecodeTextureFromBlobWorkJob tried processing a texture that hasn't been locked"))
			{
			#if __BANK
				bool bTimingDebug = PARAM_downloadabletexturedebug.Get();
				if (bTimingDebug)
				{
					m_DecodingTimer->Reset();
					m_DecodingTimer->Start();
				}
			#endif
			
				// Try decoding the texture
				bool bOk = DOWNLOADABLETEXTUREMGR.CreateTextureFromBlob(toDecode.pRequest, toDecode.pRequest->m_pDecodedTexture);
				pRequest->m_Status = (bOk ? CTextureDownloadRequest::DECODE_SUCCESSFUL : CTextureDownloadRequest::DECODE_FAILED);

#if !__NO_OUTPUT
				if (bOk)
				{
					dlcDebugf1("CDecodeTextureFromBlobWorkJob: SUCCESS - %s", pRequest->m_TxdAndTextureHash.TryGetCStr());
				}
				else
				{
					dlcErrorf("CDecodeTextureFromBlobWorkJob: FAILED - %s", pRequest->m_TxdAndTextureHash.TryGetCStr());
				}
#endif //!__NO_OUTPUT

			#if __BANK
				if (bTimingDebug)
				{
					m_DecodingTimer->Stop();
					pRequest->m_timeMsTakenDecoding = (float)m_DecodingTimer->GetTimeMS();
					dlcDisplayf("[DTM] Decoding for request %d (\"%s\") took %5.4f ms", (int)pRequest->m_RequestId, pRequest->m_TxdAndTextureHash.TryGetCStr(), pRequest->m_timeMsTakenDecoding);
				}
			#endif
				// Now let the DTM know we're done with this entry
				sysInterlockedExchange(&(pRequest->m_InProcessFlag), IN_PROCESS_FLAG_FREE);
			}

		}
	}

private:

#if __BANK
	sysPerformanceTimer* m_DecodingTimer;
#endif

} s_DecodeTextureFromBlobWorkJob;

CTextureDownloadRequestDesc::CTextureDownloadRequestDesc()
: m_Type(INVALID)	
, m_GamerIndex(-1)
, m_CloudFilePath(NULL)
, m_BufferPresize(0U)
, m_JPEGScalingFactor(VideoResManager::DOWNSCALE_ONE)
, m_CloudOnlineService(RL_CLOUD_ONLINE_SERVICE_NATIVE)
, m_CloudRequestFlags(eRequestFlags::eRequest_CacheNone)
, m_JPEGEncodeAsDXT(DECODE_JPEG_TO_DXT1)
, m_OnlyCacheAndVerify(false)
{
	m_TxtAndTextureHash.Clear();
}

CTextureDecodeRequestDesc::CTextureDecodeRequestDesc()
: m_Type(UNKNOWN)	
, m_BufferPtr(NULL)	
, m_BufferSize(0U)	
, m_JPEGScalingFactor(VideoResManager::DOWNSCALE_ONE)
, m_JPEGEncodeAsDXT(DECODE_JPEG_TO_DXT1)
{
	m_TxtAndTextureHash.Clear();
}

const int CTextureDownloadRequest::INVALID_HANDLE = -1;

CTextureDownloadRequest::CTextureDownloadRequest()
{
	Reset();
}

void CTextureDownloadRequest::Reset()
{
	m_pDecodedTexture	= NULL;

	m_Handle			= CTextureDownloadRequest::INVALID_HANDLE;

	m_pSrcBuffer		= NULL;
	m_SrcBufferSize		= 0U;
	m_RequestId			= -1;
	m_TxdAndTextureHash.Clear();

	m_TxdSlot			= -1;

	m_pDstBuffer		= NULL;
	m_DstBufferSize		= 0U;

	m_pScratchBuffer	= NULL;
	m_ScratchBufferSize	= 0U;

	m_FileType			= INVALID_FILE;
	m_ImgFormat			= 0U;
	m_Width				= 0U;
	m_Height			= 0U;
	m_NumMips			= 0U;
	m_PixelDataSize		= 0U;
	m_PixelDataOffset	= 0U;


	m_JPEGScalingFactor	= (u32)VideoResManager::DOWNSCALE_ONE;

	m_InProcessFlag		= IN_PROCESS_FLAG_FREE;

	m_bSrcBufferOwnedByUser = false;

	m_bJPEGEncodeAsDXT = DECODE_JPEG_TO_DXT1;

	m_CloudEventResultCode = 0;

	m_bUserReleased		= false;
	m_Status			= FREE_TO_REUSE;
}



CDownloadableTextureManager::CDownloadableTextureManager()
{
	fiCachePartition::Init();

	// Create our thread pool.
	// TODO: We'll want to have a shared thread pool for everybody.
	s_TexMgrThreadPool.Init();
	s_TexMgrThreadPool.AddThread(&s_TexMgrThread, "Downloadable Texture Manager", THREAD_CPU_ID, (32 * 1024) << __64BIT);

	// Initialise the download request pool
	m_DownloadRequests.Reserve(MAX_DOWNLOAD_REQUESTS);
	for (int i = 0; i < MAX_DOWNLOAD_REQUESTS; i++) 
	{
		CTextureDownloadRequest request;
		m_DownloadRequests.Push(request);
	}

	m_TextureMemoryManager.Init();

	m_localFileProvider.Initialize( CPhotoBuffer::GetDefaultSizeOfJpegBuffer() );
	m_localFileProvider.AddListener( this );
}

CDownloadableTextureManager::~CDownloadableTextureManager()
{
	m_localFileProvider.Shutdown();

	s_TexMgrThreadPool.Shutdown();

	m_DownloadRequests.Reset();
}

void CDownloadableTextureManager::InitClass()
{
	dlcAssertf(!sm_Instance, "Texture cache manager initialized twice");
	sm_Instance = rage_new CDownloadableTextureManager();

	CScriptDownloadableTextureManager::InitClass();

#if __BANK
	CDownloadableTextureManagerDebug::InitClass();
#endif

}

void CDownloadableTextureManager::ShutdownClass()
{
	CScriptDownloadableTextureManager::ShutdownClass();

#if __BANK
	CDownloadableTextureManagerDebug::ShutdownClass();
#endif

	delete sm_Instance;
	sm_Instance = NULL;
}

CTextureDownloadRequest::Status CDownloadableTextureManager::RequestTextureDownload(TextureDownloadRequestHandle& outHandle, const CTextureDownloadRequestDesc& requestDesc)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// Initialise output variable to an invalid handle
	outHandle = CTextureDownloadRequest::INVALID_HANDLE;

	// Don't allow passing in an invalid type
	if (requestDesc.m_Type == CTextureDownloadRequestDesc::INVALID)
	{
		dlcErrorf("invalid request type");
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	//  Or invalid paths 
	if (requestDesc.m_CloudFilePath == NULL)
	{
		dlcErrorf("invalid path");
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	// Bail out if hash name is invalid
	if (requestDesc.m_CloudFilePath == nullptr || requestDesc.m_CloudFilePath[0] == 0)
	{
		dlcErrorf("invalid hash name");
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	atFinalHashString txdHash = requestDesc.m_TxtAndTextureHash.IsNull() ? atFinalHashString(requestDesc.m_CloudFilePath) : requestDesc.m_TxtAndTextureHash;

	// If a request for the file already exists return its handle and current state
	CTextureDownloadRequest* pRequest = NULL;
	if (FindDownloadRequestByTxdAndTextureHashName(txdHash, pRequest, false) && pRequest)
	{
		if (pRequest->m_OnlyCacheAndVerify != requestDesc.m_OnlyCacheAndVerify)
		{
			dlcErrorf("Requesting the same file with and without OnlyCacheAndVerify on the same txd is currently not supported");
			return CTextureDownloadRequest::REQUEST_FAILED;
		}

		dlcErrorf("Request exists, returning existing handle %d, status %d", pRequest->m_Handle, pRequest->m_Status);
		outHandle = pRequest->m_Handle;
		return pRequest->m_Status;
	}

	// Or if there are no free request slots at the moment
	TextureDownloadRequestHandle handle = FindFreeDownloadRequest(pRequest);
	if (handle == CTextureDownloadRequest::INVALID_HANDLE) 
	{
		dlcErrorf("no free download requests");
		return CTextureDownloadRequest::TRY_AGAIN_LATER;
	}

	// Cache some data on the request info
	pRequest->m_TxdAndTextureHash = txdHash;
	pRequest->m_SrcBufferSize = requestDesc.m_BufferPresize;
	pRequest->m_Status = CTextureDownloadRequest::IN_PROGRESS;
	pRequest->m_JPEGScalingFactor = (u32)requestDesc.m_JPEGScalingFactor;
	pRequest->m_bSrcBufferOwnedByUser = false;
	pRequest->m_bJPEGEncodeAsDXT = requestDesc.m_JPEGEncodeAsDXT;
	pRequest->m_OnlyCacheAndVerify = requestDesc.m_OnlyCacheAndVerify;

	// In theory the full path might depend on the CloudMemberId but this should nevertheless cover all our needs
	pRequest->m_CloudFileHash = atHashString(requestDesc.m_CloudFilePath);

	// Try finding the requested texture in the txd store and shortcut to a ready state if available
	s32 txdSlot = -1;
	atHashString currentCloudFileHash;
	bool imageContentChanged = false;

	if (FindRequestedTextureInTxdStore(pRequest, txdSlot, currentCloudFileHash, imageContentChanged))
	{
		if (currentCloudFileHash != pRequest->m_CloudFileHash || imageContentChanged)
		{
			RemoveDownloadRequest(pRequest, true);

			dlcWarningf("The texture slot exists but with a different image. Try again later. tdx[%s] txdslot[%d] old[%s] new[%s]",
				txdHash.TryGetCStr(), txdSlot, currentCloudFileHash.TryGetCStr(), pRequest->m_CloudFileHash.TryGetCStr());

			return CTextureDownloadRequest::TRY_AGAIN_LATER;
		}

		dlcWarningf("Texture exists, returing existing handle. tdx[%s] txdslot[%d]", txdHash.TryGetCStr(), txdSlot);

		// Update request
		pRequest->m_TxdSlot = txdSlot;
		pRequest->m_Status = CTextureDownloadRequest::READY_FOR_USER;

		// Add an additional ref to the txd (should at least be 2), which will be removed
		// once the user releases the request; this is done to avoid the texture memory
		// manager getting rid of the txd before the user has had a chance to ref it.
		g_TxdStore.AddRef(pRequest->m_TxdSlot, REF_OTHER);
		dlcDebugf2("TXD AddRef %d has ref count %u (%s)", pRequest->m_TxdSlot.Get(), g_TxdStore.GetNumRefs(pRequest->m_TxdSlot), txdHash.TryGetCStr());
		dlcAssert(g_TxdStore.GetNumRefs(pRequest->m_TxdSlot) > 1);

		outHandle = handle;

		// User can acquire the texture already
		return CTextureDownloadRequest::READY_FOR_USER;
	}

    // cloud flags
    unsigned nCloudRequestFlags = requestDesc.m_Type == CTextureDownloadRequestDesc::DISK_FILE ? eRequestFlags::eRequest_CacheNone : eRequestFlags::eRequest_CacheAdd;
    nCloudRequestFlags |= requestDesc.m_CloudRequestFlags;

	// Use the appropriate api depending on the file type - in all cases, by passing true 
	// as the last parameter value, we ensure the file won't be downloaded again if it's cached.
	if (requestDesc.m_Type == CTextureDownloadRequestDesc::MEMBER_FILE)
	{
		pRequest->m_RequestId = CloudManager::GetInstance().RequestGetMemberFile(requestDesc.m_GamerIndex, requestDesc.m_CloudMemberId, requestDesc.m_CloudFilePath, requestDesc.m_BufferPresize, nCloudRequestFlags);
	}
	else if (requestDesc.m_Type == CTextureDownloadRequestDesc::CREW_FILE)
	{
        pRequest->m_RequestId = CloudManager::GetInstance().RequestGetCrewFile(requestDesc.m_GamerIndex, requestDesc.m_CloudMemberId, requestDesc.m_CloudFilePath, requestDesc.m_BufferPresize, nCloudRequestFlags, requestDesc.m_CloudOnlineService);
	}
	else if (requestDesc.m_Type == CTextureDownloadRequestDesc::UGC_FILE)
	{
        pRequest->m_RequestId = CloudManager::GetInstance().RequestGetUgcFile(requestDesc.m_GamerIndex, 
                                                                              requestDesc.m_nFileID == 0 ? RLUGC_CONTENT_TYPE_GTA5PHOTO : RLUGC_CONTENT_TYPE_GTA5MISSION,
                                                                              requestDesc.m_CloudFilePath,
                                                                              requestDesc.m_nFileID,
                                                                              requestDesc.m_nFileVersion,
                                                                              requestDesc.m_nLanguage,
                                                                              requestDesc.m_BufferPresize, 
                                                                              nCloudRequestFlags);
	}
	else if (requestDesc.m_Type == CTextureDownloadRequestDesc::TITLE_FILE)
	{
		pRequest->m_RequestId = CloudManager::GetInstance().RequestGetTitleFile(requestDesc.m_CloudFilePath, requestDesc.m_BufferPresize, nCloudRequestFlags);
	}
	else if (requestDesc.m_Type == CTextureDownloadRequestDesc::GLOBAL_FILE)
	{
		pRequest->m_RequestId = CloudManager::GetInstance().RequestGetGlobalFile(requestDesc.m_CloudFilePath, requestDesc.m_BufferPresize, nCloudRequestFlags);
	}
	else if (requestDesc.m_Type == CTextureDownloadRequestDesc::WWW_FILE)
	{
		pRequest->m_RequestId = CloudManager::GetInstance().RequestGetWWWFile(requestDesc.m_CloudFilePath, requestDesc.m_BufferPresize, nCloudRequestFlags);
	}
	else if (requestDesc.m_Type == CTextureDownloadRequestDesc::DISK_FILE)
	{
		pRequest->m_RequestId = m_localFileProvider.RequestLocalFile( requestDesc.m_CloudFilePath, requestDesc.m_TxtAndTextureHash.GetCStr());
	}

	// Did the request go through ok?
	if (pRequest->m_RequestId == -1)
	{
		dlcErrorf("Failed to launch request");

		// Passing true to release texture memory
		RemoveDownloadRequest(pRequest, true);
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

#if __BANK
	pRequest->m_timeMsTakenDownloading = fwTimer::GetSystemTimeInMilliseconds();
#endif

	outHandle = handle;
	dlcDebugf1("Request[%d] for %s for TXD %s has begun", handle, requestDesc.m_CloudFilePath, requestDesc.m_TxtAndTextureHash.TryGetCStr());
	return CTextureDownloadRequest::IN_PROGRESS;
}

CTextureDownloadRequest::Status CDownloadableTextureManager::RequestTextureDecode(TextureDownloadRequestHandle& outHandle, const CTextureDecodeRequestDesc& requestDesc)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// Initialise output variable to an invalid handle
	outHandle = CTextureDownloadRequest::INVALID_HANDLE;

	// Don't allow passing in an invalid type
	if (requestDesc.m_Type == CTextureDecodeRequestDesc::UNKNOWN)
	{
		dlcErrorf("invalid request type");
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	// Bail out if any of the hash names is invalid
	if (requestDesc.m_TxtAndTextureHash.IsNull())
	{
		dlcErrorf("m_TxtAndTextureHash is null");
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	// Or if the source buffer is null/buffer size is too small
	if (requestDesc.m_BufferPtr == NULL || requestDesc.m_BufferSize < 16)
	{
		dlcErrorf("Source buffer is null for %s", requestDesc.m_TxtAndTextureHash.TryGetCStr());
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	// If a request for the file already exists return its handle and current state
	CTextureDownloadRequest* pRequest = NULL;
	if (FindDownloadRequestByTxdAndTextureHashName(requestDesc.m_TxtAndTextureHash, pRequest, false))
	{
		dlcDebugf1("Request found for %s", requestDesc.m_TxtAndTextureHash.TryGetCStr());
		outHandle = pRequest->m_Handle;
		return pRequest->m_Status;
	}

	// Or if there are no free request slots at the moment
	TextureDownloadRequestHandle handle = FindFreeDownloadRequest(pRequest);
	if (handle == CTextureDownloadRequest::INVALID_HANDLE) 
	{
		dlcErrorf("no free download requests for %s", requestDesc.m_TxtAndTextureHash.TryGetCStr());
		return CTextureDownloadRequest::TRY_AGAIN_LATER;
	}

	// Cache some data on the request info
	pRequest->m_TxdAndTextureHash = requestDesc.m_TxtAndTextureHash;
	pRequest->m_JPEGScalingFactor = (u32)requestDesc.m_JPEGScalingFactor;
	pRequest->m_bSrcBufferOwnedByUser = true;
	pRequest->m_pSrcBuffer = requestDesc.m_BufferPtr;
	pRequest->m_SrcBufferSize = requestDesc.m_BufferSize;
	pRequest->m_Status = CTextureDownloadRequest::WAITING_ON_DECODING;
	pRequest->m_bJPEGEncodeAsDXT = requestDesc.m_JPEGEncodeAsDXT;

	// Now prepare the request for the decoding stage
	if (PrepareDownloadRequestForDecoding(pRequest) == false)
	{
		dlcErrorf("PrepareDownloadRequestForDecoding failed for %s", requestDesc.m_TxtAndTextureHash.TryGetCStr());

		// Something went wrong, forget about the request
		RemoveDownloadRequest(pRequest, true);
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	// Lock the request if it's in the free state
	if ( dlcVerifyf( sysInterlockedCompareExchange( &(pRequest->m_InProcessFlag), IN_PROCESS_FLAG_ACQUIRED, IN_PROCESS_FLAG_FREE ) == IN_PROCESS_FLAG_FREE, 
		"Trying to push a request to CDecodeTextureFromBlobWorkJob that's already been locked" ) )
	{
		// Enqueue the encoding request
		dlcDebugf1("RequestTextureDecode: Adding decoding job for %s", pRequest->m_TxdAndTextureHash.TryGetCStr());

		DecodeTextureEntry textureToDecode;
		textureToDecode.pRequest = pRequest;
		s_TexturesToDecode.Push(textureToDecode);

		// Start a new job if there isn't one pending already.
		if (!s_DecodeTextureFromBlobWorkJob.Pending())
		{
			s_TexMgrThreadPool.QueueWork(&s_DecodeTextureFromBlobWorkJob);
		}

	}
	else
	{
		// Something went wrong, forget about the request
		RemoveDownloadRequest(pRequest, true);
		return CTextureDownloadRequest::REQUEST_FAILED;
	}

	// Return a valid handle
	outHandle = handle;
	return CTextureDownloadRequest::IN_PROGRESS;

}

void CDownloadableTextureManager::ReleaseTextureDownloadRequest(TextureDownloadRequestHandle handle)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	CTextureDownloadRequest* pRequest;
	if (FindDownloadRequestByHandle(handle, pRequest) && pRequest && pRequest->m_Status != CTextureDownloadRequest::FREE_TO_REUSE )
	{
		dlcDebugf1("ReleaseTextureDownloadRequest for %s", pRequest->m_TxdAndTextureHash.TryGetCStr());

		pRequest->m_bUserReleased = true;
	}
}

int CDownloadableTextureManager::GetTextureDownloadRequestResultCode(TextureDownloadRequestHandle handle) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	const CTextureDownloadRequest* pRequest;
	if (FindDownloadRequestByHandle(handle, pRequest))
	{
		return pRequest->m_CloudEventResultCode;
	}

	return -1;
}

bool CDownloadableTextureManager::IsAnyRequestPendingRelease() const
{
    bool pendingRelease = false;
    dlcAssert(CDownloadableTextureManager::IsThisMainThread());

    int const c_count = m_DownloadRequests.GetCount();
    for( int index = 0; pendingRelease == false && index < c_count; ++index )
    {
        CTextureDownloadRequest const& c_currentRequest = m_DownloadRequests[ index ];
        pendingRelease = c_currentRequest.m_bUserReleased;
    }

    return pendingRelease;
}

bool CDownloadableTextureManager::IsRequestPending( TextureDownloadRequestHandle handle ) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	const CTextureDownloadRequest* pRequest;
	if (FindDownloadRequestByHandle(handle, pRequest))
	{
		return ((pRequest->m_Status == CTextureDownloadRequest::IN_PROGRESS || pRequest->m_Status == CTextureDownloadRequest::WAITING_ON_DECODING || pRequest->m_Status == CTextureDownloadRequest::DECODE_SUCCESSFUL) && pRequest->m_bUserReleased == false);
	}

	return false;
}

bool CDownloadableTextureManager::IsRequestReady(TextureDownloadRequestHandle handle) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	const CTextureDownloadRequest* pRequest;
	if (FindDownloadRequestByHandle(handle, pRequest))
	{
		return (pRequest->m_Status == CTextureDownloadRequest::READY_FOR_USER && pRequest->m_bUserReleased == false);
	}

	return false;
}

bool CDownloadableTextureManager::HasRequestFailed(TextureDownloadRequestHandle handle) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	const CTextureDownloadRequest* pRequest;
	if (FindDownloadRequestByHandle(handle, pRequest))
	{
		return IsFailedState(pRequest->m_Status) || IsPendingUseState(pRequest->m_Status);
	}

	// We cannot find it, so it must have failed
	return true;

}

strLocalIndex CDownloadableTextureManager::GetRequestTxdSlot(TextureDownloadRequestHandle handle) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());
	
	const CTextureDownloadRequest* pRequest;
	
	if (FindDownloadRequestByHandle(handle, pRequest)					&& 
		pRequest->m_Status == CTextureDownloadRequest::READY_FOR_USER	&&
		pRequest->m_bUserReleased == false)
	{
		return strLocalIndex(pRequest->m_TxdSlot);
	}

	return strLocalIndex(-1);
}

bool CDownloadableTextureManager::CreateTextureFromBlob(CTextureDownloadRequest* pRequest, grcTexture*& pOutTexture)
{
	// Error checking
	if (pRequest == NULL)
	{
		dlcWarningf("CreateTextureFromBlob using a null request info instance");
		return false;
	}

	CTextureDownloadRequest::Type fileType = pRequest->m_FileType;

	if (fileType == CTextureDownloadRequest::INVALID_FILE)
	{
		dlcWarningf("CreateTextureFromBlob request has an invalid file type");
		return false;
	}

	if (pRequest->m_bUserReleased)
	{
		dlcWarningf("CreateTextureFromBlob request has been released");
		return false;
	}
	
	// Cache some data from the request 
	grcImage::Format	format				= (grcImage::Format)pRequest->m_ImgFormat;
	u32					width				= pRequest->m_Width;
	u32					height				= pRequest->m_Height;
	u32					numMips				= pRequest->m_NumMips;
	u32					pixelDataOffset		= pRequest->m_PixelDataOffset;
	u32					pixelDataSize		= pRequest->m_PixelDataSize;
	void*				pDstBuffer			= pRequest->m_pDstBuffer;
	void*				pSrcBuffer			= pRequest->m_pSrcBuffer;
	u32					srcBufferSize		= pRequest->m_SrcBufferSize;
	u32					dstBufferSize		= pRequest->m_DstBufferSize;
	u32					jpegScalingFactor	= pRequest->m_JPEGScalingFactor;
	void*				pScratchBuffer		= pRequest->m_pScratchBuffer;
	u32					scratchBufferSize	= pRequest->m_ScratchBufferSize;
	bool				bEncodeToDXT1		= pRequest->m_bJPEGEncodeAsDXT;

	// Initialise output texture
	pOutTexture = NULL;

	// Create texture from DDS file
	if (fileType == CTextureDownloadRequest::DDS_FILE)
	{
		dlcDebugf1("CreateTextureFromBlob: Creating %s using DstBuff %p and SrcBuff %p", pRequest->m_TxdAndTextureHash.GetCStr(), pDstBuffer, pSrcBuffer);
		
		// Offset to pixel data
		char* pSrcData = (char*)(const_cast<void*>(pSrcBuffer));
		pSrcData += pixelDataOffset;
		
#if __D3D11
		// Try creating the texture
		grcTextureFactory::TextureCreateParams params( grcTextureFactory::TextureCreateParams::SYSTEM,
			grcImage::IsFormatDXTBlockCompressed(format) ? grcTextureFactory::TextureCreateParams::TILED : grcTextureFactory::TextureCreateParams::LINEAR,
			grcsRead, NULL,
			grcTextureFactory::TextureCreateParams::NORMAL );

		params.MipLevels = numMips;

		u32 rageFormat = grcTextureFactoryDX11::TranslateToRageFormat(grcTextureFactoryDX11::GetD3DFromatFromGRCImageFormat(format));
		//Check that the texture we're creating is a format that we support.
		Assert(rageFormat!=0);
		BANK_ONLY(grcTexture::SetCustomLoadName("CDownloadableTextureManager");)
		grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height, rageFormat, pSrcData, numMips, &params);
		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)

		(void) pixelDataSize;

		if(!texture)
			return false;

		pOutTexture = texture;

#elif RSG_ORBIS
		// Try creating the texture
		u32 rageFormat = ConvertTogrcFormat(GetXGFormatFromGRCImageFormat(format));
		//Check that the texture we're creating is a format that we support.
		Assert(rageFormat!=0);
		grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height, rageFormat, pSrcData, numMips);

		(void) pixelDataSize;

		if(!texture)
			return false;

		pOutTexture = texture;
#else

		// Try creating the texture
		grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height, XENON_SWITCH(grcTextureXenon::GetInternalFormat((u32)format, false), (u32)format), pDstBuffer, numMips);

		if (texture == NULL)
		{
			return false;
		}

		// Pixel data might need to be byte swapped
		int formatByteSwap = PPU_ONLY(grcImage::IsFormatDXTBlockCompressed(format) ? 1 :) grcImage::GetFormatByteSwapSize(format);
		if (formatByteSwap == 2)
		{
			s16* pSrcDataAsS16 = (s16*)pSrcData;
			for (int i = 0; i < pixelDataSize/sizeof(s16); i++)
			{
				pSrcDataAsS16[i] = sysEndian::NtoL(pSrcDataAsS16[i]);
			}
		}
		else if (formatByteSwap == 4)
		{
			s32* pSrcDataAsS32 = (s32*)pSrcData;
			for (int i = 0; i < pixelDataSize/sizeof(s32); i++)
			{
				pSrcDataAsS32[i] = sysEndian::NtoL(pSrcDataAsS32[i]);
			}
		}

		// Copy texture data
		bool bCopyOk = texture->Copy2D(pSrcData, format, width, height, numMips);

		pOutTexture = texture;

		// Did the copy failed?
		if (!bCopyOk)
		{
			return false;
		}
#endif //__D3D11

		// Everything went ok
		return true;
	}

	// Create texture from JPEG file
	if (fileType == CTextureDownloadRequest::JPEG_FILE)
	{
		// If we're encoding to DXT1 we better have a scratch buffer
		if (!dlcVerify(!bEncodeToDXT1 || pScratchBuffer))
		{
			return false;
		}

		char memFileName[64];
		fiDevice::MakeMemoryFileName(memFileName, sizeof(memFileName), pSrcBuffer, srcBufferSize, false, "DownloadedTexture");

		const bool bFromPhysical = true;

	#ifdef DTM_TEST_CORRUPTED_JPEG_DATA
		u16* pBuff = (u16*)pSrcBuffer;
		for (int i = 2;  i < srcBufferSize/4; i++)
		{
			u16 soiId = sysEndian::NtoL(*((u16 *) pBuff));
			if (soiId & 0xff)
			{
				pBuff[i] = 0x55ff;
			}
		}
	#endif

		// Take into account the scaling factor
		u32 finalWidth = width/jpegScalingFactor;
		u32 finalHeight = height/jpegScalingFactor;

		grcTexture* texture = NULL;
		
		// Let's do DXT1
		if (bEncodeToDXT1)
		{
			texture = grcImage::LoadJPEGToDXT1Surface(	memFileName, &CompressRGBA8ChunkToDXT1,
														pDstBuffer, dstBufferSize, finalWidth, finalHeight, !bFromPhysical,
														pScratchBuffer, scratchBufferSize, MAX_SCANLINES_TO_DECODE_PER_SLICE);
		}
		// Or just an uncompressed texture
		else
		{
#if RSG_ORBIS
			texture = grcImage::LoadJPEGToRGBA8Surface(memFileName, NULL, finalWidth, finalHeight, !bFromPhysical);
#else
			texture = grcImage::LoadJPEGToRGBA8Surface(memFileName, pDstBuffer, finalWidth, finalHeight, !bFromPhysical);
#endif
		}

		if (texture == NULL)
		{
			dlcErrorf("%s failed for %s", bEncodeToDXT1 ? "grcImage::LoadJPEGToDXT1Surface" : "grcImage::LoadJPEGToRGBA8Surface", pRequest->m_TxdAndTextureHash.TryGetCStr());
			return false;
		}

		// Everything went ok
		pOutTexture = texture;
		return true;
	}

	return false;
}

void CDownloadableTextureManager::OnCloudEvent(const CloudEvent* pEvent)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// we only care about requests
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
		return;

	// grab event data
	const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

	// Do we know about this request?
	CTextureDownloadRequest* pRequest;
	if (FindDownloadRequestByRequestId(pEventData->nRequestID, pRequest) == false)
	{
		return;
	}

    // Log request
    dlcDebugf1("OnCloudEvent: Request ID %d finished. Succeeded: %s", pEventData->nRequestID, pEventData->bDidSucceed ? "True" : "False");

	if (pRequest->m_bUserReleased)
	{
		dlcWarningf("OnCloudEvent: Request ID %d has been user released", pEventData->nRequestID);
		pRequest->m_Status = CTextureDownloadRequest::DOWNLOAD_FAILED;

		CTextureDownloadRequest *pAltRequest;

		if (FindDownloadRequestByTxdAndTextureHashName(pRequest->m_TxdAndTextureHash, pAltRequest, false)
			&& pAltRequest && pAltRequest->m_Status == CTextureDownloadRequest::IN_PROGRESS
			&& pAltRequest->m_RequestId == pEventData->nRequestID)
		{
			dlcWarningf("OnCloudEvent: Request ID %d has been released but there's a new pending request with the same hashes [%s] and requestId", 
				pEventData->nRequestID, pAltRequest->m_TxdAndTextureHash.TryGetCStr());

			pRequest = pAltRequest;
		}
		else
		{
			return;
		}
	}

	// Cache result code
	pRequest->m_CloudEventResultCode = pEventData->nResultCode;

	// We do, did it fail?
	if (pEventData->bDidSucceed == false || pEventData->pData == NULL || pEventData->nDataSize <= 4)
	{
		pRequest->m_Status = CTextureDownloadRequest::DOWNLOAD_FAILED;
		return;
	}

    if (pRequest->m_OnlyCacheAndVerify)
    {
        dlcDebugf1("OnCloudEvent: Request ID %d finished - OnlyCacheAndVerify", pEventData->nRequestID);
        pRequest->m_Status = CTextureDownloadRequest::READY_FOR_USER;
        return;
    }

	// Allocate memory for the buffer (the memory being passed in becomes invalid after the callback, so we need to copy it now)
	char* pBufferCopy;	
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);
		MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
		strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
		pBufferCopy = static_cast<char*>(allocator.Allocate(pEventData->nDataSize, MEMTYPE_RESOURCE_VIRTUAL));
	}	

	// Did the allocation fail?
	if (pBufferCopy == NULL)
	{
		pRequest->m_Status = CTextureDownloadRequest::DOWNLOAD_FAILED;
		return;
	}

	// Copy the buffer
	sysMemCpy(pBufferCopy, pEventData->pData, pEventData->nDataSize);

	// Update the request info
	pRequest->m_pSrcBuffer = pBufferCopy;
	pRequest->m_SrcBufferSize = pEventData->nDataSize;
	pRequest->m_Status = CTextureDownloadRequest::WAITING_ON_DECODING;

#if __BANK
	bool bTimingDebug = PARAM_downloadabletexturedebug.Get();
	if (bTimingDebug)
	{
		// Time it took to received the downloaded or cached buffer
		pRequest->m_timeMsTakenDownloading = fwTimer::GetSystemTimeInMilliseconds() - pRequest->m_timeMsTakenDownloading;
		dlcDisplayf("[DTM] Download for request %d (\"%s\") took %u ms", (int)pEventData->nRequestID, pRequest->m_TxdAndTextureHash.TryGetCStr(), pRequest->m_timeMsTakenDownloading);
	}
#endif

	// Save file to disk
#if __BANK
	if (DOWNLOADABLETEXTUREMGRDEBUG.SaveDownloadedFilesToDisk())
	{
		char fileName[RAGE_MAX_PATH];
		sprintf(&fileName[0], "c:/dump/dtm/%s", pRequest->m_TxdAndTextureHash.TryGetCStr());

		// Might be a jpeg too, but we don't know until it goes through PrepareDownloadRequestForDecoding;
		// by that point the buffer might have been byte-swapped so we dump the file here. 
		fiStream *S = ASSET.Create(&fileName[0], nullptr); 
		if (S)
		{
			dlcDisplayf("[DTM] Saving request %d (\"%s\") to disk...", (int)pEventData->nRequestID, pRequest->m_TxdAndTextureHash.TryGetCStr());
			S->WriteByte((char*)(pRequest->m_pSrcBuffer), static_cast<int>(pRequest->m_SrcBufferSize));
			S->Close();
		}
	}
#endif


	// Prepare request for decoding stage
	if (PrepareDownloadRequestForDecoding(pRequest) == false)
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);
		MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
		strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
		allocator.Free(pRequest->m_pSrcBuffer);
		pRequest->m_pSrcBuffer = NULL;

		pRequest->m_Status = CTextureDownloadRequest::DOWNLOAD_FAILED;
		return;
	}

	// Check this request is in a valid state before continuing
	if ( dlcVerifyf( sysInterlockedCompareExchange( &(pRequest->m_InProcessFlag), IN_PROCESS_FLAG_ACQUIRED, IN_PROCESS_FLAG_FREE ) == IN_PROCESS_FLAG_FREE, 
		"Trying to push a request to CDecodeTextureFromBlobWorkJob that's already been locked" ) )
	{
		// Enqueue the encoding request
		dlcDebugf1("OnCloudEvent: Adding decoding job for %s", pRequest->m_TxdAndTextureHash.TryGetCStr());

		DecodeTextureEntry textureToDecode;
		textureToDecode.pRequest = pRequest;
		s_TexturesToDecode.Push(textureToDecode);

		// Start a new job if there isn't one pending already.
		if (!s_DecodeTextureFromBlobWorkJob.Pending())
		{
			s_TexMgrThreadPool.QueueWork(&s_DecodeTextureFromBlobWorkJob);
		}

	}
	else
	{
		pRequest->m_Status = CTextureDownloadRequest::DOWNLOAD_FAILED;
	}
}


bool CDownloadableTextureManager::FindDownloadRequestByTxdAndTextureHashName(atFinalHashString txdName, CTextureDownloadRequest*& pRequest, bool allowUserReleased)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	for (int i = 0; i < m_DownloadRequests.GetCount(); i++)
	{
		if (m_DownloadRequests[i].m_TxdAndTextureHash == txdName && (allowUserReleased || !m_DownloadRequests[i].m_bUserReleased))
		{
			pRequest = &m_DownloadRequests[i];
			return true;
		}
	}

	return false;
}

bool CDownloadableTextureManager::FindDownloadRequestByHandle(TextureDownloadRequestHandle handle, CTextureDownloadRequest*& pRequest)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	if ((int)handle <= CTextureDownloadRequest::INVALID_HANDLE || (int)handle >= m_DownloadRequests.GetCount())
	{
		return false;
	}

	pRequest = &m_DownloadRequests[(int)(handle)];
	return true;
}

bool CDownloadableTextureManager::FindDownloadRequestByHandle(TextureDownloadRequestHandle handle, const CTextureDownloadRequest*& pRequest) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	if ((int)handle <= CTextureDownloadRequest::INVALID_HANDLE || (int)handle >= m_DownloadRequests.GetCount())
	{
		return false;
	}

	pRequest = &m_DownloadRequests[(int)(handle)];
	return true;
}

bool CDownloadableTextureManager::FindDownloadRequestByRequestId(CloudRequestID cloudRequestId, CTextureDownloadRequest*& pRequest)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	for (int i = 0; i < m_DownloadRequests.GetCount(); i++)
	{
		if (m_DownloadRequests[i].m_RequestId == cloudRequestId)
		{
			pRequest = &m_DownloadRequests[i];
			return true;
		}
	}

	return false;
}

TextureDownloadRequestHandle CDownloadableTextureManager::FindFreeDownloadRequest(CTextureDownloadRequest*& pRequest) 
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	for (int i = 0; i < m_DownloadRequests.GetCount(); i++)
	{
		if (m_DownloadRequests[i].m_Status == CTextureDownloadRequest::FREE_TO_REUSE)
		{
			pRequest = &m_DownloadRequests[i];
			pRequest->m_Handle = (TextureDownloadRequestHandle)(i);
			return pRequest->m_Handle;
		}
	}

	return CTextureDownloadRequest::INVALID_HANDLE;
}

bool CDownloadableTextureManager::FindRequestedTextureInTxdStore(const CTextureDownloadRequest* pRequest, s32& txdSlot, atHashString& cloudFileHash, bool& contentChanged) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	txdSlot = g_TxdStore.FindSlotFromHashKey( pRequest->m_TxdAndTextureHash.GetHash() ).Get();

	// It doesn't exist!
	if (txdSlot == -1)
	{
		return false;
	}

	// If it does, we better already know about it
	if (dlcVerifyf(m_TextureMemoryManager.SlotIsRegistered(txdSlot, cloudFileHash, contentChanged), "CDownloadableTextureManager::FindRequestedTextureInTxdStore: requested texture (\"%s\") is already in txd store, but was not processed by the DTM", pRequest->m_TxdAndTextureHash.TryGetCStr()))
	{
		return true;
	}

	return false;
}

static void NullifyCachedTextureDataPointers(grcTexture* pTexture)
{
	(void)pTexture;
}

bool CDownloadableTextureManager::RemoveDownloadRequest(CTextureDownloadRequest* pRequest, bool bReleaseTextureMemory) 
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	if (sysInterlockedRead(&(pRequest->m_InProcessFlag)) == IN_PROCESS_FLAG_FREE)
	{

	#if __BANK
		if (PARAM_downloadabletexturedebug.Get())
		{
			int numRefs = -1;
			if (pRequest->m_TxdSlot != -1)
			{
				numRefs = g_TxdStore.GetNumRefs(pRequest->m_TxdSlot);
			}
			dlcDisplayf("[DTM] Releasing its ref for texture \"%s\" (txdslot: %d refCount: %d), expect >1 or user hasn't followed rules", pRequest->m_TxdAndTextureHash.TryGetCStr(), pRequest->m_TxdSlot.Get(), numRefs);
		}
	#endif

		dlcDebugf1("[DTM] RemoveDownloadRequest txd[%s] txdslot[%d] scratchBuffer[%p]", pRequest->m_TxdAndTextureHash.TryGetCStr(), pRequest->m_TxdSlot.Get(), pRequest->m_pScratchBuffer);

		// We don't want to release any texture memory if the request was successful and
		// the texture was handed over to the TXD store
		if (bReleaseTextureMemory)
		{
			// The texture wasn't created, but the buffer might have been allocated
			if (pRequest->m_pDstBuffer)
			{
				USE_MEMBUCKET(MEMBUCKET_NETWORK);
				MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
				strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
				allocator.Free(pRequest->m_pDstBuffer);
			}

			// The grcTexture was created
			if (pRequest->m_pDecodedTexture != NULL)
			{				
				NullifyCachedTextureDataPointers(pRequest->m_pDecodedTexture);
				pRequest->m_pDecodedTexture->Release();
			}
		}

		// Release temporary scratch buffer
		if (pRequest->m_pScratchBuffer)
		{
			USE_MEMBUCKET(MEMBUCKET_NETWORK);
			MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
			strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
			allocator.Free(pRequest->m_pScratchBuffer);
		}

		// Get rid of the temporary work buffer if we own the memory
		if (pRequest->m_bSrcBufferOwnedByUser == false && pRequest->m_pSrcBuffer != NULL)
		{
			USE_MEMBUCKET(MEMBUCKET_NETWORK);
			MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
			strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
			allocator.Free(pRequest->m_pSrcBuffer);
		}
		// Make request available for reuse 
		pRequest->Reset();

		return true;
	}
	else
	{
		dlcDebugf1("RemoveDownloadRequest for %d is skipped", pRequest ? pRequest->m_TxdSlot.Get() : 0);
		return false;
	}
}

bool CDownloadableTextureManager::PrepareDownloadRequestForDecoding(CTextureDownloadRequest* pRequest)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	if (pRequest->m_SrcBufferSize < 4)
	{
		return false;
	}

#if __BANK
	bool bTimingDebug = PARAM_downloadabletexturedebug.Get();
	if (bTimingDebug)
	{
		pRequest->m_timeMsTakenPreparingForDecoding = fwTimer::GetSystemTimeInMilliseconds();
	}
#endif

	const void* pBlob = pRequest->m_pSrcBuffer;
	const unsigned blobSize = pRequest->m_SrcBufferSize;

	// Is this a DDS file?
	u32 fileMagic = sysEndian::NtoL(*((u32 *) pBlob));
	if (fileMagic == MAKE_MAGIC_NUMBER('D','D','S',' '))
	{
		// It is.
		char memFileName[64];
		fiDevice::MakeMemoryFileName(memFileName, sizeof(memFileName), pBlob, blobSize, false, "DownloadedTexture");

		// Figure out memory requirements
		grcImage::Format format;
		u32 width, height, numMips, pixelDataOffset, pixelDataSize;
		grcImage::GetDDSInfoFromFile(memFileName, format, width, height, numMips, pixelDataOffset, pixelDataSize);

		const bool bFromPhysical = true;
		u32 textureDataSize = grcTextureFactory::GetInstance().GetTextureDataSize(width, height, format, numMips, 0, false, false, bFromPhysical);
		size_t size = pgRscBuilder::ComputeLeafSize(textureDataSize, bFromPhysical);

		// Try allocating the memory
		void* pBuffer = NULL;

#if !__D3D11
		strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
		
		{
			USE_MEMBUCKET(MEMBUCKET_NETWORK);
			MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
			pBuffer = allocator.Allocate(size, 16, (bFromPhysical ? MEMTYPE_RESOURCE_PHYSICAL : MEMTYPE_RESOURCE_VIRTUAL) );
		}

		// Bail out if we couldn't allocate the memory
		if (!pBuffer)
		{
			dlcWarningf("Unable to allocate memory for downloaded texture (DDS)");
			return false;
		}
#endif

		// Cache the texture info gathered from the blob
		pRequest->m_Width			= width;
		pRequest->m_Height			= height;
		pRequest->m_NumMips 		= numMips;
		pRequest->m_ImgFormat		= (u32)format;
		pRequest->m_pDstBuffer		= pBuffer;
		pRequest->m_DstBufferSize	= (unsigned)size;
		pRequest->m_PixelDataSize	= pixelDataSize;
		pRequest->m_PixelDataOffset	= pixelDataOffset;

		pRequest->m_FileType		= CTextureDownloadRequest::DDS_FILE;

	#if __BANK
		if (bTimingDebug)
		{
			// Time it took to prepare texture for decoding
			pRequest->m_timeMsTakenPreparingForDecoding = fwTimer::GetSystemTimeInMilliseconds() - pRequest->m_timeMsTakenPreparingForDecoding;
			dlcDisplayf("[DTM] Preparing request %d (\"%s\") for decoding took %u ms", (int)pRequest->m_RequestId, pRequest->m_TxdAndTextureHash.GetCStr(), pRequest->m_timeMsTakenPreparingForDecoding);
		}
	#endif

		dlcDebugf1("%s prepared for decoding DDS to DstBuffer %p from SrcBuffer %p", pRequest->m_TxdAndTextureHash.GetCStr(), pRequest->m_pDstBuffer, pRequest->m_pSrcBuffer);

		// All good to schedule decoding
		return true;
	}

	// Is this a JPEG file?
	u16 soiId = sysEndian::NtoL(*((u16 *) pBlob));
	u16 marker = sysEndian::NtoL(*(((u16 *)(pBlob)+1)));
	if (soiId == 0xd8ff && ((marker & 0xe0ff) == 0xe0ff))
	{
		// It is.
		char memFileName[64];
		fiDevice::MakeMemoryFileName(memFileName, sizeof(memFileName), pBlob, blobSize, false, "DownloadedTexture");

		// Try getting texture data size (TBR: this is probably not particularly fast, but unless this data is cached somewhere
		// else I'm not sure there's a faster way other than trying to write something faster than what jpeglib does).
		u32 width, height, pitch;
		if (grcImage::GetJPEGInfoForRGBA8Surface(memFileName, width, height, pitch) == false)
		{
			dlcErrorf("GetJPEGInfoForRGBA8Surface failed for %s", pRequest->m_TxdAndTextureHash.TryGetCStr());
			return false;
		}

		// Try allocating memory for texture data taking into account the scaling factor
		u32 finalWidth = width/(pRequest->m_JPEGScalingFactor);
		u32 finalHeight = height/(pRequest->m_JPEGScalingFactor);

		const bool bFromPhysical = true;
		const grcImage::Format format = pRequest->m_bJPEGEncodeAsDXT ? grcImage::DXT1 : grcImage::A8R8G8B8;

		//! B*1851089 - Find the next highest power of 2 for our width. We use that since
		//! that's what the texture allocation may be rounded up to for alignment purposes
		u32 const c_pow2Width = 1 << (Log2Floor(finalWidth) + !!(finalWidth&(finalWidth-1)));

		u32 textureDataSize = 
			grcTextureFactory::GetInstance().GetTextureDataSize( c_pow2Width, finalHeight, 
			format, 1, 0, false, !pRequest->m_bJPEGEncodeAsDXT, bFromPhysical);

		size_t size = textureDataSize;
		strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();

		void* pBuffer;
		{
			USE_MEMBUCKET(MEMBUCKET_NETWORK);
			MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
			pBuffer = allocator.Allocate(size, 16, (bFromPhysical ? MEMTYPE_RESOURCE_PHYSICAL : MEMTYPE_RESOURCE_VIRTUAL) );
		}

		if (!pBuffer)
		{
			dlcWarningf("Unable to allocate memory for downloaded texture (JPEG)");
			return false;
		}

		// Scratch buffer only used when doing DXT
		void* pScratchBuffer = NULL;
		size_t scratchSize = 0;
		if( pRequest->m_bJPEGEncodeAsDXT )
		{
			// Try allocating a scratch buffer for the decoding job
			scratchSize = pgRscBuilder::ComputeLeafSize(finalWidth*4*MAX_SCANLINES_TO_DECODE_PER_SLICE, false);
			{
				USE_MEMBUCKET(MEMBUCKET_NETWORK);
				MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
				pScratchBuffer = allocator.Allocate(scratchSize, 16, MEMTYPE_RESOURCE_VIRTUAL);
			}

			if (!pScratchBuffer)
			{
				// Not happening... release the texture memory
				{
					USE_MEMBUCKET(MEMBUCKET_NETWORK);
					MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
					allocator.Free(pBuffer);
				}
				dlcWarningf("Unable to allocate memory for downloaded texture (JPEG scratch buffer)");
				return false;
			}
		}

		// Cache the texture info gathered from the blob
		pRequest->m_Width				= width;
		pRequest->m_Height				= height;
		pRequest->m_NumMips 			= 0;
		pRequest->m_ImgFormat			= (u32)(format);
		pRequest->m_pDstBuffer			= pBuffer;
		pRequest->m_DstBufferSize		= (unsigned)size;
		pRequest->m_PixelDataSize		= 0U;
		pRequest->m_PixelDataOffset		= 0U;
		pRequest->m_pScratchBuffer		= pScratchBuffer;
		pRequest->m_ScratchBufferSize	= (unsigned)scratchSize;
		pRequest->m_FileType			= CTextureDownloadRequest::JPEG_FILE;

	#if __BANK
		if (bTimingDebug)
		{
			// Time it took to prepare texture for decoding
			pRequest->m_timeMsTakenPreparingForDecoding = fwTimer::GetSystemTimeInMilliseconds() - pRequest->m_timeMsTakenPreparingForDecoding;
			dlcDisplayf("[DTM] Preparing request %d (\"%s\") for decoding took %u ms", (int)pRequest->m_RequestId, pRequest->m_TxdAndTextureHash.GetCStr(), pRequest->m_timeMsTakenPreparingForDecoding);
		}
	#endif

		dlcDebugf1("%s prepared for decoding JPEG to DstBuffer %p", pRequest->m_TxdAndTextureHash.GetCStr(), pRequest->m_pDstBuffer);

		// All good to schedule decoding
		return true;
	}

	// It's a resource.
	if (blobSize < sizeof(datResourceFileHeader))
	{
		return false;
	}

	pRequest->m_FileType = CTextureDownloadRequest::RESOURCE_FILE;
	

	return true;
}

void CDownloadableTextureManager::Update()
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// Give a chance for local files to load
	m_localFileProvider.Update();

	// TXD memory housekeeping
	m_TextureMemoryManager.Update();

#if __BANK
	DOWNLOADABLETEXTUREMGRDEBUG.Update();
#endif

	// This update step only modifies requests in a failed or complete state.
	for (int i = 0; i < m_DownloadRequests.GetCount(); i++)
	{
		CTextureDownloadRequest* pRequest = &m_DownloadRequests[i];

		// This entry does not need any housekeeping
		if (pRequest->m_Status == CTextureDownloadRequest::FREE_TO_REUSE)
		{
			continue;
		}
		// Handle user-cancelled requests first (it can be in any state)
		else if (pRequest->m_bUserReleased)
		{
			ProcessReleasedDownloadRequest(pRequest);
		}
		// Texture is ready to be handed over to the TXD store
		else if (pRequest->m_Status == CTextureDownloadRequest::DECODE_SUCCESSFUL)
		{
			ProcessDecodedDownloadRequest(pRequest);
		}

	}

	// Update the script helper
	SCRIPTDOWNLOADABLETEXTUREMGR.Update();
}

bool CDownloadableTextureManager::ProcessDecodedDownloadRequest(CTextureDownloadRequest* pRequest)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());
	dlcAssert(pRequest && pRequest->m_Status == CTextureDownloadRequest::DECODE_SUCCESSFUL && pRequest->m_bUserReleased == false && pRequest->m_pDecodedTexture != NULL);

	// Fail the request if anything's missing (some asserts would have triggered earlier)
	if (pRequest->m_TxdAndTextureHash.IsNull() || pRequest->m_pDecodedTexture == NULL)
	{
		pRequest->m_Status = CTextureDownloadRequest::DECODE_FAILED;
		return false;
	}

	// Hand the texture over to be retrieved from or added to the TXD store and managed for us by the texture memory manager
	int txdSlot = g_TxdStore.FindSlotFromHashKey(pRequest->m_TxdAndTextureHash).Get(); 
	if(txdSlot == -1)
		txdSlot = m_TextureMemoryManager.AddTxdSlot(pRequest->m_TxdAndTextureHash, pRequest->m_CloudFileHash,
													pRequest->m_pDecodedTexture, pRequest->m_pDstBuffer);

	dlcDebugf1("ProcessDecodedDownloadRequest: %s created from DstBuff %p, pTexture[%p] at TXD slot %d", pRequest->m_TxdAndTextureHash.TryGetCStr(), pRequest->m_pDstBuffer, pRequest->m_pDecodedTexture, txdSlot );

	// If it failed, fail the request too
	if (txdSlot == -1)
	{
		pRequest->m_Status = CTextureDownloadRequest::DECODE_FAILED;
		return false;
	}

	// Update request
	pRequest->m_TxdSlot = txdSlot;
	pRequest->m_Status = CTextureDownloadRequest::READY_FOR_USER;

	// Add an additional ref to the txd (should be 2), which will be removed
	// once the user releases the request; this is done to avoid the texture memory
	// manager getting rid of the txd before the user has had a chance to ref it.
	g_TxdStore.AddRef(pRequest->m_TxdSlot, REF_OTHER);
	dlcDebugf3("g_TxdStore.AddRef id[%d] num[%d] - ProcessDecodedDownloadRequest", pRequest->m_TxdSlot.Get(), g_TxdStore.GetNumRefs(pRequest->m_TxdSlot));

	dlcAssert(g_TxdStore.GetNumRefs(pRequest->m_TxdSlot) == 2);

	return true;
}

bool CDownloadableTextureManager::ProcessReleasedDownloadRequest(CTextureDownloadRequest* pRequest)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());
	dlcAssert(pRequest && pRequest->m_bUserReleased);

	if (IsFailedState(pRequest->m_Status))
	{	
		return ProcessFailedDownloadRequest(pRequest);
	}
	else if (pRequest->m_Status == CTextureDownloadRequest::DECODE_SUCCESSFUL)
	{
		// Passing true to release texture memory
		return RemoveDownloadRequest(pRequest, true);
	}
	else if (pRequest->m_Status == CTextureDownloadRequest::READY_FOR_USER)
	{
		const strLocalIndex idx = pRequest->m_TxdSlot;

		// Release request but don't touch the texture (it's been handed over to the TXD store)
		bool success = RemoveDownloadRequest(pRequest, false);

		if (idx.IsValid() && success)
		{
			dlcDebugf2("CDownloadableTextureManager - ProcessReleasedDownloadRequest - RemoveRef [%d] count[%d]", idx.Get(), g_TxdStore.GetNumRefs(idx));

			// Remove the 2nd ref we added earlier in ProcessDecodedDownloadRequest
			g_TxdStore.RemoveRef(idx, REF_OTHER);

			// Should at least have one reference
			dlcAssert(g_TxdStore.GetNumRefs(idx) >= 1);
		}

		return success;
	}

	// We cannot release the request yet
	return false;
}


bool CDownloadableTextureManager::ProcessFailedDownloadRequest(CTextureDownloadRequest* pRequest)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());
	dlcAssert(pRequest && IsFailedState(pRequest->m_Status));

	// Passing true to release texture memory
	RemoveDownloadRequest(pRequest, true);

	return true;
}

#if __ASSERT
void CDownloadableTextureManager::CheckTXDStoreCapacity()
{
	Assertf((g_TxdStore.GetMaxSize() - g_TxdStore.GetNumUsedSlots()) >= 64,  "[DTM] TxdStore might run out of slots while downloading textures from the cloud (%d/%d)", g_TxdStore.GetNumUsedSlots(), g_TxdStore.GetMaxSize());
}

bool CDownloadableTextureManager::IsThisMainThread()
{
	return sysThreadType::IsUpdateThread() || sm_hackMainThread;
}
#endif

void CDownloadableTextureMemoryManager::Init()
{
	// Initialise the txd slot manager
	m_pTxdSlotManager = rage_new fwStoreSlotManager<fwTxd, fwTxdDef>(g_TxdStore);

	// Initialise the managed txd slots list
	m_ManagedTxdSlots.Init(MAX_MANAGED_TEXTURES);
}

void CDownloadableTextureMemoryManager::Shutdown()
{
	delete m_pTxdSlotManager;
	m_pTxdSlotManager = NULL;

	m_ManagedTxdSlots.Shutdown();
}

int	CDownloadableTextureMemoryManager::AddTxdSlot(atFinalHashString txdAndTextureHash, atHashString cloudFileHash, grcTexture* pTexture, void* pTextureMemory)
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// We're full, cannot manage this texture
	if (m_ManagedTxdSlots.GetNumFree() <= 0)
	{
#if __BANK
		DumpManagedTxdList(false);
#endif
		dlcErrorf("No more TXD slot. Can't add %s", txdAndTextureHash.TryGetCStr());
		return -1;
	}

	const u32 txdHash = txdAndTextureHash.GetHash();
	const char* txdStr = txdAndTextureHash.GetCStr();

	if (m_pTxdSlotManager->SlotExists(txdStr))
	{
		dlcErrorf("TXD slot %s already exists", txdStr);
		return -1;
	}

	// Create a txd for the texture
	fwTxd* pTxd = rage_new fwTxd(1);
	if (pTxd == NULL)
	{
		dlcErrorf("failed to allocate txd for %s", txdStr);
		return -1;
	}

	// Add the texture to the txd
	pTxd->AddEntry(txdHash, pTexture);

	// Try adding to the txd store
	int txdSlot = m_pTxdSlotManager->AddSlot(txdStr, pTxd);

	dlcDebugf1("AddTxdSlot %d %s", txdSlot, txdStr);

	// The texture wasn't added
	if (txdSlot == -1)
	{
#if __BANK
		// Dump the content of m_ManagedTxdSlots when we reach the pool limit
		dlcWarningf("Could not add texture, dumping content of m_ManagedTxdSlots :");
		DumpManagedTxdList();
#endif
		// Don't do anything to the texture - we'll take care of it (need to manually deallocate texture memory)	
		pTxd->SetEntryUnsafe(0, NULL);

		// Get rid of the txd
		delete pTxd;

		return -1;
	}

	// Add txd slot to our list
	ManagedTxdSlotNode* pNode = m_ManagedTxdSlots.Insert();
	pNode->item.Init(txdSlot, pTextureMemory, cloudFileHash OUTPUT_ONLY(, txdAndTextureHash));

	return txdSlot;
}

bool CDownloadableTextureMemoryManager::SlotIsRegistered(s32 txdSlot, atHashString& cloudFileHash, bool& imageContentChanged) const
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// Find matching txd slot
	ManagedTxdSlotNode* pNode = m_ManagedTxdSlots.GetFirst()->GetNext();
	while(pNode != m_ManagedTxdSlots.GetLast())
	{
		ManagedTxdSlotEntry* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		if (pCurEntry->m_TxdSlot == (int)txdSlot)
		{
			cloudFileHash = pCurEntry->m_CloudFileHash;
			imageContentChanged = pCurEntry->m_ImageContentChanged;
			return true;
		}
	}

	return false;
}

bool CDownloadableTextureMemoryManager::ImageContentChanged(atHashString cloudFileHash)
{
	ManagedTxdSlotNode* pNode = m_ManagedTxdSlots.GetFirst()->GetNext();
	while(pNode != m_ManagedTxdSlots.GetLast())
	{
		ManagedTxdSlotEntry* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		if (pCurEntry->m_CloudFileHash == cloudFileHash)
		{
			dlcDebugf1("Image changed for %s. tx[%s]", cloudFileHash.TryGetCStr(), pCurEntry->m_TextureName.TryGetCStr());
			
			pCurEntry->m_ImageContentChanged = true;

			return true;
		}
	}

	return false;
}

void CDownloadableTextureMemoryManager::Update()
{
	dlcAssert(CDownloadableTextureManager::IsThisMainThread());

	// Nothing to do
	if (m_ManagedTxdSlots.GetNumUsed() == 0)
	{
		return;
	}

	// Find TXDs no longer in use
	ManagedTxdSlotNode* pNode = m_ManagedTxdSlots.GetFirst()->GetNext();
	while (pNode != m_ManagedTxdSlots.GetLast())
	{
		ManagedTxdSlotEntry* pCurEntry = &(pNode->item);
		ManagedTxdSlotNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		int numRefs = g_TxdStore.GetNumRefs(pCurEntry->m_TxdSlot);

		// Assert none of the txds we managed have been dereferenced below 1
		dlcAssertf(numRefs >= 1, "CDownloadableTextureMemoryManager::Update: somebody's removed one too many refs from a tracked txd");

		fwTxd* pTxd = g_TxdStore.Get(pCurEntry->m_TxdSlot);

		FatalAssertf(pTxd, "Txd in slot %d is null this is bad!", pCurEntry->m_TxdSlot.Get());

		grcTexture* pTexture = pTxd ? pTxd->GetEntry(0) : nullptr;

		if (dlcVerifyf(pTexture != nullptr, "The texture in slot %d is null", pCurEntry->m_TxdSlot.Get()))
		{
			// If the TXD only has one ref it's time to get rid of it but only if the pTexture is no longer being referenced
			// It should always be 1 but even in case of errors we want to free the allocated memory.
			if (numRefs == 1 && pTexture->GetRefCount() == 1)
			{
				// remove the last ref
				g_TxdStore.RemoveRefWithoutDelete(pCurEntry->m_TxdSlot, REF_OTHER);

				dlcDebugf2("CDownloadableTextureMemoryManager - RemoveSlot %d attempt and RemoveRef [%s]", pCurEntry->m_TxdSlot.Get(), pCurEntry->m_TextureName.TryGetCStr());
#if __BANK
				if (PARAM_downloadabletexturedebug.Get())
				{
					dlcDisplayf("[DTM] Releasing txd %d (gone for good, don't use it scaleform/script!)", pCurEntry->m_TxdSlot.Get());
				}
#endif
				// Deallocate texture memory, only happens on unified memory consoles, where textures are allocated inside the streaming memory
				if (pCurEntry->m_pBuffer)
				{
					strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
					USE_MEMBUCKET(MEMBUCKET_NETWORK);
					MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
					allocator.Free(pCurEntry->m_pBuffer);
				}

				pCurEntry->m_pBuffer = NULL;

				dlcDebugf2("CDownloadableTextureMemoryManager - RemoveSlot %d done", pCurEntry->m_TxdSlot.Get());

				// Get rid of the txd slot
				// Not sure yet if in case of a null pTxd this is always safe to call
				g_TxdStore.RemoveSlot(pCurEntry->m_TxdSlot);

				// Reset our entry
				pCurEntry->Init(-1, NULL, atHashString() OUTPUT_ONLY(, atFinalHashString()));

				// Forget about it
				m_ManagedTxdSlots.Remove(pLastNode);
			}
		}
		else
		{
			dlcDebugf2("CDownloadableTextureMemoryManager - RemoveSlot %d attempt failed texture was null releasing allocated memory", pCurEntry->m_TxdSlot.Get());

			// Deallocate texture memory, only happens on unified memory consoles, where textures are allocated inside the streaming memory
			if (pCurEntry->m_pBuffer)
			{
				strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
				USE_MEMBUCKET(MEMBUCKET_NETWORK);
				MEM_USE_USERDATA(MEMUSERDATA_DL_MANAGER);
				allocator.Free(pCurEntry->m_pBuffer);
			}

			pCurEntry->m_pBuffer = NULL;

			// Reset our entry
			pCurEntry->Init(-1, NULL, atHashString() OUTPUT_ONLY(, atFinalHashString()));

			// Forget about it
			m_ManagedTxdSlots.Remove(pLastNode);
		}
	}
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureMemoryManager::DumpManagedTxdList(bool toScreen) const
{

	if (m_ManagedTxdSlots.GetNumUsed() == 0)
	{
		if(toScreen)
			grcDebugDraw::AddDebugOutput("[DTM] No entries being managed");
		else
			dlcDisplayf("[DTM] No entries being managed");
		return;
	}

	ManagedTxdSlotNode* pNode = m_ManagedTxdSlots.GetFirst()->GetNext();

	if(toScreen)
		grcDebugDraw::AddDebugOutput("[DTM] %d entries being managed", m_ManagedTxdSlots.GetNumUsed());
	else
		dlcDisplayf("[DTM] %d entries being managed", m_ManagedTxdSlots.GetNumUsed());
	int curEntry = 0;

	while(pNode != m_ManagedTxdSlots.GetLast())
	{
		ManagedTxdSlotEntry* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		const strLocalIndex txdSlot = strLocalIndex(pCurEntry->m_TxdSlot);
		int numRefs = g_TxdStore.GetNumRefs(txdSlot);
		const fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(txdSlot);

		if (pTxd != NULL)
		{
			const char* pTxdName = g_TxdStore.GetName(txdSlot);
			if(toScreen)
			{
				grcDebugDraw::AddDebugOutput("[DTM] [%d]\t name: \"%s\"\t txdSlot: %d\t numRefs: %d", curEntry, (pTxdName != NULL ? pTxdName : "NULL") , txdSlot.Get(), numRefs);
			}
			else
			{
				dlcDisplayf("[DTM] [%d]\t name: \"%s\"\t txdSlot: %d\t numRefs: %d", curEntry, (pTxdName != NULL ? pTxdName : "NULL") , txdSlot.Get(), numRefs);
			}
		}

		curEntry++;
	}

#if !__FINAL
	SCRIPTDOWNLOADABLETEXTUREMGR.PrintTextures(toScreen);
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManager::DumpManagedTxdList(bool toScreen) const
{
	m_TextureMemoryManager.DumpManagedTxdList(toScreen);
}

//////////////////////////////////////////////////////////////////////////
//
CDownloadableTextureManagerDebug* CDownloadableTextureManagerDebug::sm_Instance = NULL;

const char*	CDownloadableTextureManagerDebug::sm_pScopeTypeStr[4] = {	"MEMBER",
																		"CREW",
																		"TITLE",
																		"GLOBAL"	};


//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::InitClass()
{
	USE_DEBUG_MEMORY();
	dlcAssertf(!sm_Instance, "CDownloadableTextureManagerDebug initialized twice");
	sm_Instance = rage_new CDownloadableTextureManagerDebug();
	sm_Instance->Init();
}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::ShutdownClass()
{
	USE_DEBUG_MEMORY();
	delete sm_Instance;
	sm_Instance = NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::Init()
{
	m_pScopeTypeCombo = NULL;
	m_scopeTypeComboIdx = 2;
	strcpy(&m_newFileName[0], "test/KillRivalRank.dds");
	m_curRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	m_curRequestTxdSlot = -1;
	m_pCurTexture = NULL;
	m_bDumpManagedTxdList = false;
	m_bPrintOnScreenManagedTxdList = false;
	m_bStoreDownloadsToDisk = false;
	m_bTestDanglingReference = false;
	m_bRemoveDanglingReference = false;
	m_bDanglingReferenceAdded = false;
}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::OnScopeTypeSelected()
{
}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::OnLoadTexture()
{
	CTextureDownloadRequestDesc::Type requestType = CTextureDownloadRequestDesc::INVALID;
	if (m_scopeTypeComboIdx != -1)
	{
		requestType = (CTextureDownloadRequestDesc::Type)(m_scopeTypeComboIdx+1);
	}

	m_curRequestTxdSlot = -1;

	// Fill in the descriptor for our request
	CTextureDownloadRequestDesc requestDesc;
	requestDesc.m_Type = requestType;
	requestDesc.m_GamerIndex		= NetworkInterface::GetLocalGamerIndex();
	requestDesc.m_CloudFilePath		= &m_newFileName[0];
	requestDesc.m_BufferPresize		= 1024*64;
	requestDesc.m_TxtAndTextureHash	= "CDownloadableTextureManagerDebug";

	// Issue the request
	TextureDownloadRequestHandle handle;
	CTextureDownloadRequest::Status retVal;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(handle, requestDesc);

	// Check the return value
	if (retVal != CTextureDownloadRequest::IN_PROGRESS)
	{
		dlcWarningf("CDownloadableTextureManagerDebug::OnLoadTexture: request failed before being issued");
	}

	// Cache the handle for querying the state of the request
	m_curRequestHandle = handle;

}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::OnReleaseTexture()
{
	if (m_curRequestTxdSlot != -1)
	{
		dlcDebugf1("CDownloadableTextureManager - OnReleaseTexture - RemoveRef [%d]", m_curRequestTxdSlot.Get());

		g_TxdStore.RemoveRef(m_curRequestTxdSlot, REF_OTHER);

		if (m_bDanglingReferenceAdded == false)
		{
			m_curRequestTxdSlot = -1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Downloadable Texture Manager");
		bank.AddSeparator();
		bank.AddToggle("Dump Managed TXDs List", &m_bDumpManagedTxdList);
		bank.AddToggle("Print Managed TXDs List On Screen", &m_bPrintOnScreenManagedTxdList);
		bank.AddToggle("Save Downloads To Disk", &m_bStoreDownloadsToDisk);
		bank.AddSeparator();
		bank.AddToggle("Cause Dangling Reference Error", &m_bTestDanglingReference);
		bank.AddToggle("Clear Dangling Reference", &m_bRemoveDanglingReference);
		bank.AddSeparator();
		m_pScopeTypeCombo = bank.AddCombo("Scope", &m_scopeTypeComboIdx, 4, &sm_pScopeTypeStr[0], datCallback(MFA(CDownloadableTextureManagerDebug::OnScopeTypeSelected), (datBase*)this));
		bank.AddText("Texture Cloud Path", &m_newFileName[0], 128);
		bank.AddButton("Load Texture", datCallback(MFA(CDownloadableTextureManagerDebug::OnLoadTexture), (datBase*)this));
		bank.AddButton("Release Texture", datCallback(MFA(CDownloadableTextureManagerDebug::OnReleaseTexture), (datBase*)this));
	bank.PopGroup();

}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::Update()
{

	if (m_bDumpManagedTxdList)
	{
		DOWNLOADABLETEXTUREMGR.DumpManagedTxdList(false);
		m_bDumpManagedTxdList = false;
	}
	if (m_bPrintOnScreenManagedTxdList)
	{
		DOWNLOADABLETEXTUREMGR.DumpManagedTxdList(true);
	}

	// If we have a valid txd slot we might want to do some bad things...
	if (m_curRequestTxdSlot != -1)
	{

		if (m_bTestDanglingReference && m_bDanglingReferenceAdded == false)
		{
			m_pCurTexture->AddRef();
			m_bDanglingReferenceAdded = true;
			m_bTestDanglingReference = false;
		}

		if (m_bRemoveDanglingReference && m_bDanglingReferenceAdded)
		{
			m_curRequestTxdSlot = -1;
			m_pCurTexture->Release();
			m_bRemoveDanglingReference = false;
			m_bDanglingReferenceAdded = false;
		}

	}

	// No requests pending
	if (m_curRequestHandle == CTextureDownloadRequest::INVALID_HANDLE)
	{
		return;
	}

	// Reset our handle if the request has failed
	if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_curRequestHandle))
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_curRequestHandle);

		m_curRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}
	// Check if our request is ready
	else if (DOWNLOADABLETEXTUREMGR.IsRequestReady(m_curRequestHandle))
	{
		m_curRequestTxdSlot = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_curRequestHandle);
		m_pCurTexture = g_TxdStore.Get(m_curRequestTxdSlot)->GetEntry(0);

		// Add a reference for us before releasing the request
		g_TxdStore.AddRef(m_curRequestTxdSlot, REF_OTHER);

		// We have the txd slot, we can now release our request
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_curRequestHandle);

		// We don't want the handle anymore
		m_curRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

}

//////////////////////////////////////////////////////////////////////////
//
void CDownloadableTextureManagerDebug::Render()
{
	if (m_curRequestTxdSlot == -1 || m_pCurTexture == NULL || m_bDanglingReferenceAdded)
	{
		return;
	}

	float x = 100.0f;
	float y = 100.0f;
	float w = (float)m_pCurTexture->GetWidth();
	float h = (float)m_pCurTexture->GetHeight();

	PUSH_DEFAULT_SCREEN();
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcBindTexture(m_pCurTexture);
		grcDrawSingleQuadf(x,y,x+w,y+h,0,0,0,1,1,Color32(255,255,255,255));
	POP_DEFAULT_SCREEN();
}

#endif // __BANK






//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureEntry::Init(ScriptTextureDownloadHandle handle, TextureDownloadRequestHandle requestHandle, const char* pTextureName, s32 txdSlot, s32 refCount, bool bTxdRefAdded)
{
	m_Handle		= handle;
	m_RequestHandle	= requestHandle;

	if (pTextureName == nullptr)
	{
		m_TextureHash = atTxdHashString();
		m_TextureName.Clear();
	}
	else
	{
		m_TextureName = pTextureName;
		m_TextureHash = atTxdHashString(pTextureName);
	}

	m_TxdSlot		= txdSlot;
	m_RefCount		= refCount;
	m_bTxdRefAdded	= bTxdRefAdded;
#if !__FINAL
	m_scriptHash		= CTheScripts::GetCurrentScriptName();
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureEntry::Reset()
{
	Init(CScriptDownloadableTextureManager::INVALID_HANDLE, CTextureDownloadRequest::INVALID_HANDLE, NULL, -1, 0, false);
}

//////////////////////////////////////////////////////////////////////////
//
CScriptDownloadableTextureManager* CScriptDownloadableTextureManager::sm_Instance = NULL;

//////////////////////////////////////////////////////////////////////////
//
CScriptDownloadableTextureManager::CScriptDownloadableTextureManager()
{
	Init();
}

//////////////////////////////////////////////////////////////////////////
//
CScriptDownloadableTextureManager::~CScriptDownloadableTextureManager()
{
	Shutdown();
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::Init()
{
	// Initialise the texture list
	m_TextureList.Init(MAX_SCRIPT_TEXTURES);
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::Shutdown()
{
	// Get rid of all of our entries
	ReleaseAllTextures();

	m_TextureList.Shutdown();
}

//////////////////////////////////////////////////////////////////////////
//
ScriptTextureDownloadHandle	CScriptDownloadableTextureManager::RequestMemberTexture(rlGamerHandle gamerHandle, const char* cloudFilePath, const char* textureName, bool bUseCacheWithoutCloudChecks)
{
    // Fill in the descriptor for our request
    CTextureDownloadRequestDesc requestDesc;
    requestDesc.m_Type              = CTextureDownloadRequestDesc::MEMBER_FILE;
    requestDesc.m_GamerIndex		= NetworkInterface::GetLocalGamerIndex();
    requestDesc.m_CloudFilePath		= cloudFilePath;
    requestDesc.m_BufferPresize		= 1024*64;
    requestDesc.m_TxtAndTextureHash	= textureName;
    requestDesc.m_CloudRequestFlags	= bUseCacheWithoutCloudChecks ? eRequest_CacheForceCache : eRequest_CacheNone;
    requestDesc.m_CloudMemberId     = rlCloudMemberId(gamerHandle);
    requestDesc.m_nFileID           = 0;
    requestDesc.m_nFileVersion      = 0;
    requestDesc.m_nLanguage         = RLSC_LANGUAGE_UNKNOWN;

	return RequestTexture(requestDesc, textureName);
}

//////////////////////////////////////////////////////////////////////////
//
ScriptTextureDownloadHandle	CScriptDownloadableTextureManager::RequestTitleTexture(const char* cloudFilePath, const char* textureName, bool bUseCacheWithoutCloudChecks)
{
    // Fill in the descriptor for our request
    CTextureDownloadRequestDesc requestDesc;
    requestDesc.m_Type              = CTextureDownloadRequestDesc::TITLE_FILE;
    requestDesc.m_GamerIndex		= NetworkInterface::GetLocalGamerIndex();
    requestDesc.m_CloudFilePath		= cloudFilePath;
    requestDesc.m_BufferPresize		= 1024*64;
    requestDesc.m_TxtAndTextureHash	= textureName;
    requestDesc.m_CloudRequestFlags = bUseCacheWithoutCloudChecks ? eRequest_CacheForceCache : eRequest_CacheNone;
    requestDesc.m_nFileID           = 0;
    requestDesc.m_nFileVersion      = 0;
    requestDesc.m_nLanguage         = RLSC_LANGUAGE_UNKNOWN;

    // no member path
    requestDesc.m_CloudMemberId.Clear();

    return RequestTexture(requestDesc, textureName);
}

//////////////////////////////////////////////////////////////////////////
//
ScriptTextureDownloadHandle	CScriptDownloadableTextureManager::RequestUgcTexture(const char* szContentID, int nFileID, int nFileVersion, const rlScLanguage nLanguage, const char* textureName, bool bUseCacheWithoutCloudChecks)
{
    // Fill in the descriptor for our request
    CTextureDownloadRequestDesc requestDesc;
    requestDesc.m_Type              = CTextureDownloadRequestDesc::UGC_FILE;
    requestDesc.m_GamerIndex		= NetworkInterface::GetLocalGamerIndex();
    requestDesc.m_CloudFilePath		= szContentID;  // doubles for content ID
    requestDesc.m_BufferPresize		= 1024*64;
    requestDesc.m_TxtAndTextureHash	= textureName;
    requestDesc.m_CloudRequestFlags = bUseCacheWithoutCloudChecks ? eRequest_CacheForceCache : eRequest_CacheNone;
    requestDesc.m_nFileID           = nFileID;
    requestDesc.m_nFileVersion      = nFileVersion;
    requestDesc.m_nLanguage         = nLanguage;

    // no member path
    requestDesc.m_CloudMemberId.Clear();

    return RequestTexture(requestDesc, textureName);
}

//////////////////////////////////////////////////////////////////////////
//
ScriptTextureDownloadHandle	CScriptDownloadableTextureManager::RequestTexture(const CTextureDownloadRequestDesc& requestDesc, const char* textureName)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// No names?
	if (textureName == NULL || requestDesc.m_CloudFilePath == NULL)
	{
		dlcDebugf1("RequestSimpleHostedTexture :: Invalid Parameters - Name: %s", textureName);
		return CScriptDownloadableTextureManager::INVALID_HANDLE;
	}

	// Do we already have the texture on record?
	CScriptDownloadableTextureEntry* pEntry = NULL;
	if (FindTextureByName(textureName, pEntry))
	{
		// We already know about this texture! but if it's about to be deleted fail the request
		// and let the user try again later
		if (pEntry->m_RefCount == 0)
		{
			dlcDebugf1("RequestSimpleHostedTexture :: Have Texture (Pending Delete)");
			return CScriptDownloadableTextureManager::INVALID_HANDLE;
		}
		else
		{
			return pEntry->m_Handle;
		}
	}

	// Bail out if we're full
	if (m_TextureList.GetNumFree() <= 0)
	{
        dlcErrorf("RequestTexture :: No Free Slots!");
#if !__FINAL
		PrintTextures(false);
#endif
		return CScriptDownloadableTextureManager::INVALID_HANDLE;
	}

	// Now we actually need to issue a request!
	ScriptTextureDownloadHandle scriptHandle = CScriptDownloadableTextureManager::INVALID_HANDLE;

	// Issue the request
	TextureDownloadRequestHandle requestHandle;
	CTextureDownloadRequest::Status retVal;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(requestHandle, requestDesc);

	// Check the return value and bail if it's not in progress or ready for user
	if (retVal != CTextureDownloadRequest::IN_PROGRESS && retVal != CTextureDownloadRequest::READY_FOR_USER)
	{
		return scriptHandle;
	}

	atHashString hashAsHandle(textureName);

	// Compute our script handle
	scriptHandle = (ScriptTextureDownloadHandle)((s32)hashAsHandle.GetHash());

    dlcDebugf1("RequestTexture :: Handle: 0x%08x, Name: %s", scriptHandle, textureName);

	// Store our request for the record
	ScriptTextureNode* pNode = m_TextureList.Insert();
	pNode->item.Init(scriptHandle, requestHandle, textureName, -1 /*txdSlot*/, 1 /*refCount*/, false /*txdRefAdded*/);

	return scriptHandle;
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::ReleaseTexture(ScriptTextureDownloadHandle handle)
{
	dlcAssert(sysThreadType::IsUpdateThread());

	if (IsHandleValid(handle) == false)
	{
        dlcErrorf("ReleaseTexture :: Invalid Handle: 0x%08x", handle);
        return;
	}

	CScriptDownloadableTextureEntry* pEntry = NULL;

	// Do we know about this texture?
	if (FindTextureByHandle(handle, pEntry) == false)
	{
        dlcErrorf("ReleaseTexture :: Cannot find Texture, Handle: 0x%08x", handle);
        return;
	}

    dlcDebugf1("ReleaseTexture :: Releasing - Handle: 0x%08x, Name: %s", handle, pEntry->m_TextureName.GetCStr());

	// Decrease our ref count
	if (dlcVerifyf(pEntry->m_RefCount > 0, "ReleaseTexture: entry \"%s\" is trying to be released too many times", pEntry->m_TextureName.GetCStr()))
	{
		pEntry->m_RefCount--;
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool CScriptDownloadableTextureManager::HasTextureFailed(ScriptTextureDownloadHandle handle) const
{
	dlcAssert(sysThreadType::IsUpdateThread());

	if (IsHandleValid(handle) == false)
	{
		return true;
	}

	const CScriptDownloadableTextureEntry* pEntry = NULL;

	// Do we know about this texture? Failed requests will be automatically disposed of during update
	if (FindTextureByHandle(handle, pEntry) == false)
	{
		dlcWarningf("No texture found for this handle");
		return true;
	}

	// We found the entry, but has it got a valid download handle (it should unless the texture is
	// already good to use)?
	if (pEntry->m_bTxdRefAdded == false && pEntry->m_RequestHandle == CTextureDownloadRequest::INVALID_HANDLE)
	{
		dlcWarningf("No valid download handle for this handle");
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
const char* CScriptDownloadableTextureManager::GetTextureName(ScriptTextureDownloadHandle handle) const
{
	dlcAssert(sysThreadType::IsUpdateThread());

	if (IsHandleValid(handle) == false)
	{
		return NULL;
	}

	const CScriptDownloadableTextureEntry* pEntry = NULL;

	// Do we know about this texture?
	if (FindTextureByHandle(handle, pEntry) == false)
	{
		return NULL;
	}

	// Only return the texture name if it's ready and if it has at least one reference
	// and if we already added a ref to its txd (e.g.: failed requests wouldn't have)
	if (pEntry->m_TxdSlot != -1 && pEntry->m_RefCount > 0 && pEntry->m_bTxdRefAdded)
	{
		return pEntry->m_TextureName.GetCStr();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
bool CScriptDownloadableTextureManager::IsNameOfATexture(const char* pTextureName)
{
	CScriptDownloadableTextureEntry* pEntry = NULL;
	return FindTextureByName(pTextureName, pEntry);
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::Update()
{
	dlcAssert(sysThreadType::IsUpdateThread());

	// Nothing to do
	if (m_TextureList.GetNumUsed() == 0)
	{
		return;
	}

	// Process pending texture requests
	ProcessPendingTextures();

	// Cleanup release textures
	ProcessReleasedTextures();

}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::ProcessPendingTextures()
{
	ScriptTextureNode* pNode = m_TextureList.GetFirst()->GetNext();
	while(pNode != m_TextureList.GetLast())
	{
		CScriptDownloadableTextureEntry* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		// Don't bother if we're about to delete it or if we have already taken care of interacting with the DTM
		if (pCurEntry->m_RefCount == 0 || pCurEntry->m_RequestHandle == CTextureDownloadRequest::INVALID_HANDLE)
		{
			continue;
		}

		// We've got business pending with the DTM! Has the request failed?
		if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(pCurEntry->m_RequestHandle))
		{
			// Forget about the request
			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(pCurEntry->m_RequestHandle);
			pCurEntry->m_RequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
		}
		// Check if our request is ready
		else if (DOWNLOADABLETEXTUREMGR.IsRequestReady(pCurEntry->m_RequestHandle))
		{
			pCurEntry->m_TxdSlot = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(pCurEntry->m_RequestHandle);

			// The DTM promised the request was ready, but double check it's true...
			if (dlcVerifyf(pCurEntry->m_TxdSlot != -1, "ProcessPendingTextures: entry \"%s\" was supposed to be ready", pCurEntry->m_TextureName.GetCStr()))
			{
				// Add a reference for us before releasing the request
				g_TxdStore.AddRef(pCurEntry->m_TxdSlot, REF_OTHER);

				// We have the txd slot, we can now release our request
				DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(pCurEntry->m_RequestHandle);

				// We don't want the handle anymore...
				pCurEntry->m_RequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

				// But we want to remember we added a ref
				pCurEntry->m_bTxdRefAdded = true;
			}
			// Something's gone out of whack in the DTM
			else
			{
				// Forget about the request
				DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(pCurEntry->m_RequestHandle);
				pCurEntry->m_RequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
			}
		}

	}
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::ProcessReleasedTextures()
{
	ScriptTextureNode* pNode = m_TextureList.GetFirst()->GetNext();
	while(pNode != m_TextureList.GetLast())
	{
		CScriptDownloadableTextureEntry* pCurEntry = &(pNode->item);
		ScriptTextureNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		// Are we ready to get rid of this entry?
		if (pCurEntry->m_RefCount == 0)
		{
			// If we added a reference to the TXD - this only happens if the texture was successfully downloaded -, remove it
			if (pCurEntry->m_bTxdRefAdded)
			{
				// Verify the TXD slot is still valid and has at least our ref
				strLocalIndex txdSlot = strLocalIndex(pCurEntry->m_TxdSlot);
				if (Verifyf((g_TxdStore.IsValidSlot(txdSlot) != false && g_TxdStore.GetNumRefs(txdSlot) > 0), "CScriptDownloadableTextureManager::ProcessReleasedTextures: entry \"%s\" (%d) has been released too many times", pCurEntry->m_TextureName.TryGetCStr(), txdSlot.Get()))
				{
					g_TxdStore.RemoveRef(txdSlot, REF_OTHER);
				}
			}

			// Forget about it
			m_TextureList.Remove(pLastNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::ReleaseAllTextures()
{
	ScriptTextureNode* pNode = m_TextureList.GetFirst()->GetNext();
	while(pNode != m_TextureList.GetLast())
	{
		CScriptDownloadableTextureEntry* pCurEntry = &(pNode->item);
		ScriptTextureNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		// If we added a reference to the TXD - this only happens if the texture was successfully downloaded -, remove it
		if (pCurEntry->m_bTxdRefAdded)
		{
			// Verify the TXD slot is still valid and has at least our ref
			strLocalIndex txdSlot = pCurEntry->m_TxdSlot;
			if (Verifyf((g_TxdStore.IsValidSlot(txdSlot) != false && g_TxdStore.GetNumRefs(txdSlot) > 0), "CScriptDownloadableTextureManager::ProcessReleasedTextures: entry \"%s\" (%d) has been released too many times", pCurEntry->m_TextureName.TryGetCStr(), txdSlot.Get()))
			{
				g_TxdStore.RemoveRef(txdSlot, REF_OTHER);
			}
		}
		// It could be we're still tracking its state with the DTM - cancel it
		else if (pCurEntry->m_RequestHandle != CTextureDownloadRequest::INVALID_HANDLE)
		{
			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(pCurEntry->m_RequestHandle);
		}

		// Forget about it
		m_TextureList.Remove(pLastNode);
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool CScriptDownloadableTextureManager::FindTextureByName(const char* pTextureName, CScriptDownloadableTextureEntry*& pOutEntry)
{
	pOutEntry = NULL;

	// Nothing to do
	if (m_TextureList.GetNumUsed() == 0 || pTextureName == NULL)
	{
		return false;
	}

	// Try finding this texture
	ScriptTextureNode* pNode = m_TextureList.GetFirst()->GetNext();
	while(pNode != m_TextureList.GetLast())
	{
		CScriptDownloadableTextureEntry* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		if (pCurEntry->m_TextureName == pTextureName)
		{
			pOutEntry = pCurEntry;
			return true;			
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool CScriptDownloadableTextureManager::FindTextureByHandle(ScriptTextureDownloadHandle handle, CScriptDownloadableTextureEntry*& pOutEntry)
{
	const CScriptDownloadableTextureEntry* pEntry;
	bool bOk = FindTextureByHandle(handle, pEntry);
	pOutEntry = const_cast<CScriptDownloadableTextureEntry*>(pEntry);
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
bool CScriptDownloadableTextureManager::FindTextureByHandle(ScriptTextureDownloadHandle handle, const CScriptDownloadableTextureEntry*& pOutEntry) const
{
	pOutEntry = NULL;

	// Nothing to do
	if (m_TextureList.GetNumUsed() == 0 || handle == CScriptDownloadableTextureManager::INVALID_HANDLE)
	{
		return false;
	}

	// Try finding this texture
	ScriptTextureNode* pNode = m_TextureList.GetFirst()->GetNext();
	while(pNode != m_TextureList.GetLast())
	{
		const CScriptDownloadableTextureEntry* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		if (pCurEntry->m_Handle == handle)
		{
			pOutEntry = pCurEntry;
			return true;			
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::InitClass()
{
	dlcAssertf(!sm_Instance, "CScriptDownloadableTextureManager initialized twice");
	sm_Instance = rage_new CScriptDownloadableTextureManager();
}

//////////////////////////////////////////////////////////////////////////
//
void CScriptDownloadableTextureManager::ShutdownClass()
{
	delete sm_Instance;
	sm_Instance = NULL;
}

#if !__FINAL

void CScriptDownloadableTextureManager::PrintTextures(bool onScreen)
{
	ScriptTextureNode* pTexture = m_TextureList.GetFirst()->GetNext();
	while(pTexture != m_TextureList.GetLast())
	{
		CScriptDownloadableTextureEntry* pEntry = &pTexture->item;
		const char* pTxd = "";
		if(pEntry->m_TxdSlot != -1)
			pTxd = g_TxdStore.GetName(pEntry->m_TxdSlot);
		if(onScreen)
		{
#if __BANK
			grcDebugDraw::AddDebugOutput("[DTM] Script Downloadable texture txd %s, tex %s, script %s", pTxd, pEntry->m_TextureName.GetCStr(), pEntry->m_scriptHash.GetCStr());
#endif
		}
		else
		{
			Displayf("[DTM] Script Downloadable texture txd %s, tex %s, script %s", pTxd, pEntry->m_TextureName.GetCStr(), pEntry->m_scriptHash.GetCStr());
		}

		pTexture = pTexture->GetNext();
	}
}

#endif
