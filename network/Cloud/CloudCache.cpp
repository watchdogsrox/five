//
// streaming/cacheloader.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "network/cloud/CloudCache.h"
#include "network/cloud/CloudManager.h"

#include "file/asset.h"
#include "file/cachepartition.h"
#include "file/device.h"
#include "file/stream.h"
#include "file/tcpip.h"
#if IS_GEN9_PLATFORM
#include "sga/sga_resourcecache.h"
#endif
#include "streaming/packfilemanager.h"
#include "streaming/streamingengine.h"
#include "string/stringutil.h"
#include "system/param.h"
#include "system/membarrier.h"
#include "system/memops.h"
#include "system/criticalsection.h"

#include "fwnet/netchannel.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, cloudcache);
#undef __net_channel
#define __net_channel net_cloudcache

#define CLOUD_CACHE_FILE_PREFIX "cloud_"
#define CLOUD_CACHE_FILE_EXTENSION ".dat"
#define CLOUD_CACHE_FILE_FMT CLOUD_CACHE_FILE_PREFIX "%016" I64FMT "x" CLOUD_CACHE_FILE_EXTENSION

PARAM(cloudCacheDisabled, "[net_cloud] Disables caching of cloud files", "Disabled", "All", "");
PARAM(cloudCacheDisableEncryption, "[net_cloud] Uses encryption for cloud cache headers and data (when requested)", "Disabled", "All", "");

const unsigned kCACHE_HEADER_SIZE = sizeof(sCacheHeader);
const unsigned kAES_BLOCK_SIZE = 16;
const unsigned kCACHE_SEED = 0xCAC4E;

CompileTimeAssert(kCACHE_HEADER_SIZE % 16 == 0);

bool CloudCache::ms_bCacheEnabled = true;
bool CloudCache::ms_bCachePartitionInitialised = false;

CacheRequestID CloudCache::ms_nRequestIDCount = 0;
CacheRequestList CloudCache::ms_CacheRequests;
atArray<CloudCacheListener*> CloudCache::m_CacheListeners;
AES* CloudCache::ms_pCloudCacheAES = nullptr;

volatile unsigned s_TotalPending = 0;

#if !__NO_OUTPUT
static char szLogHandle[32];
char* LogStreamHandle(fiStream* pStream)
{
    if(!pStream)
        formatf(szLogHandle, "INVALID");
    else 
        formatf(szLogHandle, "%" HANDLEFMT "", pStream->GetLocalHandle());

    return szLogHandle;
}
#endif

enum class CloudCacheOperation
{
    None,
    Write,
    Read,
    Delete,
    DeleteOldFiles
};

struct fiAsyncDataRequest 
{
	enum AsyncState 
	{ 
		PENDING, 
		STARTED, 
		FAILED, 
		FINISHED 
	};
	
	fiAsyncDataRequest();

	char m_szName[RAGE_MAX_PATH];
	unsigned m_WriteCount;
	CacheRequestID m_RequestId;
	CloudCacheOperation m_Operation;
	void* m_UserData;
	volatile AsyncState m_State;
	fiAsyncDataRequest* m_pNext;

	static const unsigned MAX_BUFFERS = 2;
	struct sRequestBuffer 
	{
        sRequestBuffer() : m_pData(nullptr), m_DataSize(0) {}
		void* m_pData;
		unsigned m_DataSize;
	} 
	m_DataBuffer[MAX_BUFFERS];
};

fiAsyncDataRequest::fiAsyncDataRequest() 
    : m_WriteCount(0)
    , m_RequestId(INVALID_CACHE_REQUEST_ID)
    , m_Operation(CloudCacheOperation::None)
    , m_UserData(nullptr)
    , m_State(PENDING)
    , m_pNext(nullptr)
{
    m_szName[0] = '\0';
}

sysCriticalSectionToken s_QueueCs;
static fiAsyncDataRequest *s_First, *s_Last;
static sysIpcSema s_Pending = nullptr;

void fiAsyncDataWorker(void*)
{
	// these are arbitrary, just need to be different
	const unsigned BAD_HEADER = 'clou', GOOD_HEADER = 'CLOU';		

	for(;;)
	{
		sysIpcWaitSema(s_Pending);
		
		// grab the head of the list and release the sema so main thread won't block.  
		// remove from list immediately, as soon as the state becomes FINISHED or FAILED
		// the main thread can delete the request
		s_QueueCs.Lock();
		
			fiAsyncDataRequest* pRequest = s_First;
			s_First = pRequest->m_pNext;

			// track in logging
			gnetDebug1("[%d] fiAsyncDataWorker :: Unlinking Request: 0x%p, First: 0x%p", pRequest->m_RequestId, pRequest, s_First);
		
		s_QueueCs.Unlock();

		// mark the request as started
		pRequest->m_State = fiAsyncDataRequest::STARTED;

		// process the request in full
		const fiDevice* pDevice = fiDevice::GetDevice(pRequest->m_szName);

		fiAsyncDataRequest::AsyncState nResult = fiAsyncDataRequest::FAILED; 
		fiHandle hFile = fiHandleInvalid;
		if(pRequest->m_Operation == CloudCacheOperation::Write)
		{
			hFile = pDevice->Create(pRequest->m_szName);
			if(pDevice && fiIsValidHandle(hFile)) 
			{
				bool bSuccess = pDevice->SafeWrite(hFile, &BAD_HEADER, sizeof(BAD_HEADER));
				gnetDebug1("[%d] fiAsyncDataWorker :: Write BadHeader %s", pRequest->m_RequestId, bSuccess ? "Succeeded" : "Failed");

				for(unsigned i = 0; i < pRequest->m_WriteCount && bSuccess; i++)
				{
					bSuccess = pDevice->SafeWrite(hFile, pRequest->m_DataBuffer[i].m_pData, pRequest->m_DataBuffer[i].m_DataSize);
					gnetDebug1("[%d] fiAsyncDataWorker :: Write Data[%d] (Size: %u) %s", pRequest->m_RequestId, i, pRequest->m_DataBuffer[i].m_DataSize, bSuccess ? "Succeeded" : "Failed");
				}

				// only validate the file if all writes succeeded (i.e. detect full cache drive)
				if(bSuccess)
				{
					bSuccess = ((pDevice->Seek(hFile, 0, seekSet) == 0) && pDevice->SafeWrite(hFile, &GOOD_HEADER, sizeof(GOOD_HEADER)));
					gnetDebug1("[%d] fiAsyncDataWorker :: Write GoodHeader %s", pRequest->m_RequestId, bSuccess ? "Succeeded" : "Failed");
				}

				// update state
                nResult = bSuccess ? fiAsyncDataRequest::FINISHED : fiAsyncDataRequest::FAILED;
			}
			else 
			{
                nResult = fiAsyncDataRequest::FAILED;
				gnetDebug1("[%d] fiAsyncDataWorker :: Cannot Create File. Device: 0x%p", pRequest->m_RequestId, pDevice);
			}

			// wipe data - the request will be processed on the main thread but we don't need this
			CloudSafeFree(pRequest->m_DataBuffer[0].m_pData);
			CloudSafeFree(pRequest->m_DataBuffer[1].m_pData);
		}
		else if(pRequest->m_Operation == CloudCacheOperation::Read)
		{
			// reads are processed by the main thread, which reclaims the request memory
			unsigned testHeader;

			hFile = pDevice->Open(pRequest->m_szName, true);
			if(pDevice && fiIsValidHandle(hFile))
			{
				if(pDevice->Read(hFile, &testHeader, sizeof(testHeader)) == sizeof(testHeader) && testHeader == GOOD_HEADER)
				{
					int nFileSize = pDevice->Size(hFile) - sizeof(testHeader);
					pRequest->m_DataBuffer[0].m_DataSize = nFileSize;
					pRequest->m_DataBuffer[0].m_pData = (char*)CloudAllocate(pRequest->m_DataBuffer[0].m_DataSize);

					if(pRequest->m_DataBuffer[0].m_pData && pDevice->SafeRead(hFile, pRequest->m_DataBuffer[0].m_pData, nFileSize))
					{
						nResult = fiAsyncDataRequest::FINISHED;
					}
					else
					{
						gnetError("[%d] fiAsyncDataWorker :: Short Read - Size: %d, Device: 0x%p", pRequest->m_RequestId, nFileSize, pDevice);
						CloudSafeFree(pRequest->m_DataBuffer[0].m_pData);
					}
				}
				else
				{
					gnetError("[%d] fiAsyncDataWorker :: Header Read Failure - Size: %d, Device: 0x%p", pRequest->m_RequestId, pDevice->Size(hFile), pDevice);
				}
			}
			else
			{
				gnetDebug1("[%d] fiAsyncDataWorker :: Cannot Open File - Device: 0x%p", pRequest->m_RequestId, pDevice);
			}
		}
		else if (pRequest->m_Operation == CloudCacheOperation::DeleteOldFiles)
		{
			// DeleteOldFiles is blocking. As long as we don't access any file in the cache folder while this runs we should be fine.
			// That should be true because rlCloudcache should be the only one reading or writing.
			fiPersistentCachePartition::DeleteOldFiles();
			nResult = fiAsyncDataRequest::FINISHED;
		}
		else
		{
			rlDebug1("[%d] Unknown operation. File '%s' on device %p", pRequest->m_RequestId, pRequest->m_szName, pDevice);
			nResult = fiAsyncDataRequest::FAILED;
		}

		if(fiIsValidHandle(hFile))
			pDevice->Close(hFile);

		// update the state to let the main thread know
		sysMemWriteBarrier();
		pRequest->m_State = nResult;

		sysInterlockedDecrement(&s_TotalPending);
	}
}

void fiSubmitAsyncData(fiAsyncDataRequest* pRequest)
{
	SYS_CS_SYNC(s_QueueCs);

	// shouldn't get here - means we have a cache request that hasn't validate that the cache
	// partition was created
	if(!gnetVerify(s_Pending))
		return;

    if(s_First)
        s_Last->m_pNext = pRequest;
    else
        s_First = pRequest;

    s_Last = pRequest;

    gnetDebug2("[%d] fiSubmitAsyncData :: Operation: %s, Name: %s, Request: 0x%p, First: 0x%p, Last: 0x%p", pRequest->m_RequestId, pRequest->m_WriteCount ? "Write" : "Read", pRequest->m_szName, pRequest, s_First, s_Last);
    pRequest->m_pNext = NULL;
    pRequest->m_State = fiAsyncDataRequest::PENDING;

	sysIpcSignalSema(s_Pending);
	sysInterlockedIncrement(&s_TotalPending);
}

bool fiIsAsyncWritePending(void* pUserData)
{
	SYS_CS_SYNC(s_QueueCs);
	for(fiAsyncDataRequest* pRequest = s_First; pRequest != NULL; pRequest = pRequest->m_pNext)
		if((pRequest->m_UserData == pUserData) && (pRequest->m_State != fiAsyncDataRequest::FAILED) && (pRequest->m_State != fiAsyncDataRequest::FINISHED))
			return true;

	return false;
}

CloudCacheListener::CloudCacheListener()
{
	CloudCache::AddListener(this);
}

CloudCacheListener::~CloudCacheListener()
{
	CloudCache::RemoveListener(this);
}

void CloudCache::Drain()
{
	if(s_TotalPending) 
	{
		unsigned nWaitTime = 0;
		while(s_TotalPending) 
		{
			static const unsigned SLEEP_TIME = 10;
			static const unsigned WAIT_WARNING = 1000;
			CompileTimeAssert((WAIT_WARNING % SLEEP_TIME) == 0);

			sysIpcSleep(SLEEP_TIME);
			if((nWaitTime += SLEEP_TIME) == WAIT_WARNING)
				gnetWarning("CloudCache::Drain - waiting for pending operations");
		}
	}
}


void CloudCache::Init()
{
	// reset state
	ms_bCacheEnabled = true;

	// prop request
	ms_pCloudCacheAES = (AES*)CloudAllocate(sizeof(AES));
	if(gnetVerifyf(ms_pCloudCacheAES != nullptr, "Init :: Failed to allocate AES"))
	{
		new(ms_pCloudCacheAES) AES();
	}

#if !__FINAL
	// global enable / disable
	if(PARAM_cloudCacheDisabled.Get())
		ms_bCacheEnabled = false;
#endif

	// initialise the cache partition
	if(ms_bCacheEnabled)
	{
		ms_bCachePartitionInitialised = fiCachePartition::Init();
		if(gnetVerifyf(ms_bCachePartitionInitialised, "Init :: Failed to initialise cache partition"))
		{
			if(!s_Pending)
			{
				s_Pending = sysIpcCreateSema(false);

#if RSG_SCARLETT || RSG_PROSPERO
				sysIpcCreateThreadWithCoreMask(fiAsyncDataWorker, nullptr, sysIpcMinThreadStackSize, PRIO_NORMAL, "fiWriteAsyncDataWorker", sysDependencyConfig::GetWorkerCoreMask());
#else
				sysIpcCreateThread(fiAsyncDataWorker, nullptr, sysIpcMinThreadStackSize, PRIO_NORMAL, "fiWriteAsyncDataWorker");
#endif
			}

			AddDeleteOldFilesRequest();
		}
	}

	// log out state of cloud cache
	gnetDebug1("Init :: Enabled: %s, Initialised: %s", ms_bCacheEnabled ? "True" : "False", ms_bCachePartitionInitialised ? "True" : "False");
}

void CloudCache::Shutdown()
{
	// shutdown cache
	fiCachePartition::Shutdown();
}

void CloudCache::Update()
{
    // check async cache requests
	int nCacheRequests = ms_CacheRequests.GetCount();
    
    // grab this once (irrelevant with no cache requests)
    int nListeners = (nCacheRequests > 0) ? m_CacheListeners.GetCount() : 0; 

	for(int i = 0; i < nCacheRequests; i++)
	{
		sCacheRequest* pRequest = ms_CacheRequests[i];
		fiAsyncDataRequest* pAsyncRequest = pRequest->m_AsyncRequest;

		// Do we need to construct a memory stream now because the request finished?
		fiAsyncDataRequest::AsyncState nRequestState = pAsyncRequest->m_State;
		sysMemReadBarrier();

		// write
		if(pAsyncRequest->m_WriteCount != 0)
		{
			if(nRequestState == fiAsyncDataRequest::FINISHED || nRequestState == fiAsyncDataRequest::FAILED)
			{
				gnetDebug1("[%d] Write %s", pRequest->m_nRequestID, (nRequestState == fiAsyncDataRequest::FINISHED) ? "Succeeded" : "Failed");
				
				// invoke listeners
				for(int j = 0; j < nListeners; j++)
					m_CacheListeners[j]->OnCacheFileAdded(pRequest->m_nRequestID, (nRequestState == fiAsyncDataRequest::FINISHED));

				// mark for delete
				pRequest->m_bFlaggedToClear = true; 
			}
		}
		// read
		else
		{
			if(nRequestState == fiAsyncDataRequest::FINISHED)
			{
				if(!pRequest->m_pStream)
				{
					// create stream
					char szFileName[128];
					fiDevice::MakeMemoryFileName(szFileName, sizeof(szFileName), pAsyncRequest->m_DataBuffer[0].m_pData, pAsyncRequest->m_DataBuffer[0].m_DataSize, true, "cache");
					pRequest->m_pStream = fiStream::Open(szFileName);

					// logging
					gnetDebug1("[%d] Read Succeeded - Name: %s, Handle: %s, Size: %d", pRequest->m_nRequestID, szFileName, LogStreamHandle(pRequest->m_pStream), pRequest->m_pStream ? pRequest->m_pStream->Size() : 0);

					// assume that the request will be freed
					pRequest->m_bFlaggedToClear = true;

					// callback
					for(int j = 0; j < nListeners; j++)
						m_CacheListeners[j]->OnCacheFileLoaded(pRequest->m_nRequestID, pRequest->m_pStream != NULL);

					// failed... free the buffer since stream code won't do it.
					if(!pRequest->m_pStream)
						CloudSafeFree(pAsyncRequest->m_DataBuffer[0].m_pData);
				}
			}
			else if(nRequestState == fiAsyncDataRequest::FAILED)
			{
				// logging
				gnetDebug1("[%d] Read Failed", pRequest->m_nRequestID);
				for(int j = 0; j < nListeners; j++)
					m_CacheListeners[j]->OnCacheFileLoaded(pRequest->m_nRequestID, false);
			}
		}
		
		// clean up failed requests, or ones that are completed.
		if(pRequest->m_bFlaggedToClear || nRequestState == fiAsyncDataRequest::FAILED)
		{
            // logging
            gnetDebug1("[%d] Clear :: Stream: %s, Request: 0x%p", pRequest->m_nRequestID, LogStreamHandle(pRequest->m_pStream), pRequest->m_AsyncRequest);

			// check stream is valid
			if(pRequest->m_pStream)
			{
				pRequest->m_pStream->Close();
				pRequest->m_pStream = NULL;
			}

			// delete the async request
			CloudSafeFree(pRequest->m_AsyncRequest);

			// delete the request
			CloudSafeFree(pRequest);

			// delete and decrement counters
			ms_CacheRequests.Delete(i);
			--i;
			--nCacheRequests;
		}
	}
}

void CloudCache::BuildCacheFileName(char(&fileName)[RAGE_MAX_PATH], const u64 nSemantic)
{
    formatf(fileName, CLOUD_CACHE_FILE_FMT, nSemantic);
}

void CloudCache::BuildCacheFilePath(char(&filePath)[RAGE_MAX_PATH], const char* const pFileName, CachePartition partition)
{
    const char* szRoot = fiCachePartition::GetCachePrefix();

    switch (partition)
    {
    case CachePartition::Persistent: szRoot = fiPersistentCachePartition::GetCachePrefix(); break;
    default: break;
    }

    formatf(filePath, "%s%s", szRoot, pFileName);
}

bool CloudCache::EncryptBuffer(void* pBuffer, unsigned nBufferSize)
{
#if !__FINAL
	if(PARAM_cloudCacheDisableEncryption.Get())
		return true;
#endif

	// check buffer is valid
	if(!gnetVerifyf(pBuffer, "EncryptBuffer :: Invalid Buffer!"))
		return false;

	// assume space available in buffer for signature + minimum block needed for encryption
	if(!gnetVerifyf((nBufferSize % kAES_BLOCK_SIZE) == 0, "EncryptBuffer :: Data block (size %d) needs to be divisible by %d", nBufferSize, kAES_BLOCK_SIZE))
		return false;

	// encrypt data
	if(!gnetVerify(ms_pCloudCacheAES && ms_pCloudCacheAES->Encrypt(pBuffer, nBufferSize)))
    {
        gnetError("EncryptBuffer :: Error encrypting buffer!");
        return false;
    }

	// success
	return true; 
}

bool CloudCache::DecryptBuffer(void* pBuffer, unsigned nBufferSize)
{
#if !__FINAL
	if(PARAM_cloudCacheDisableEncryption.Get())
		return true;
#endif

	// check buffer is valid
	if(!gnetVerifyf(pBuffer, "DecryptBuffer :: Invalid Buffer!"))
		return false;

    // assume space available in buffer for signature + minimum block needed for encryption
    if(!gnetVerifyf((nBufferSize % kAES_BLOCK_SIZE) == 0, "DecryptBuffer :: Data block (size %d) needs to be divisible by %d", nBufferSize, kAES_BLOCK_SIZE))
        return false;

	// decrypt buffer
	if(!gnetVerify(ms_pCloudCacheAES && ms_pCloudCacheAES->Decrypt(pBuffer, nBufferSize)))
    {
        gnetError("DecryptBuffer :: Error decrypting buffer!");
        return false;
    }

	// success
	return true; 
}

CacheRequestID CloudCache::AddCacheFile(u64 nSemantic, u32 nModifiedTime, bool bEncrypted, CachePartition partition, const void* pData, u32 nDataSize)
{
	// build cache name
    char szCacheName[RAGE_MAX_PATH];
    char szCacheFilePath[RAGE_MAX_PATH];
    BuildCacheFileName(szCacheName, nSemantic);
    BuildCacheFilePath(szCacheFilePath, szCacheName, partition);

	// validate cache is enabled
	if(!ms_bCacheEnabled)
	{
		gnetWarning("AddCacheFile :: Failed to add %s. Cache not enabled!", szCacheName);
		return INVALID_CACHE_REQUEST_ID;
	}

	// validate cache partition is initialised
	if(!ms_bCachePartitionInitialised)
	{
		gnetWarning("AddCacheFile :: Failed to add %s. Cache partition not initialised!", szCacheName);
		return INVALID_CACHE_REQUEST_ID;
	}

    // validate data size
    if(!gnetVerify(nDataSize > 0))
    {
        gnetError("AddCacheFile :: Failed to add %s. DataSize is 0!", szCacheName);
        return INVALID_CACHE_REQUEST_ID;
    }

	// prop request
	sCacheRequest* pCacheRequest = (sCacheRequest*)CloudAllocate(sizeof(sCacheRequest));
	if(!pCacheRequest)
	{
		gnetDebug1("AddCacheFile :: Failed to add %s. Failed to allocate cache request", szCacheName);
		return INVALID_CACHE_REQUEST_ID;
	}
	new(pCacheRequest) sCacheRequest();

	// try to create file for writing
	fiAsyncDataRequest* pRequest = (fiAsyncDataRequest*)CloudAllocate(sizeof(fiAsyncDataRequest));
    if(!pRequest)
    {
        gnetError("AddCacheFile :: Failed to add %s. Failed to allocate request!", szCacheName);
        return INVALID_CACHE_REQUEST_ID;
    }
    new(pRequest) fiAsyncDataRequest();

    // fill in request
    safecpy(pRequest->m_szName, szCacheFilePath);
	pRequest->m_Operation = CloudCacheOperation::Write;
	pRequest->m_WriteCount = 2;
	pRequest->m_UserData = (void*)(size_t)nSemantic;

    // if encrypting, we need to make sure we're using a file divisible by kAES_BLOCK_SIZE
    unsigned nDataBufferSize = nDataSize;
    if(bEncrypted)
    {
        unsigned nModulo = nDataBufferSize % kAES_BLOCK_SIZE; 
        if(nModulo != 0)
            nDataBufferSize += (kAES_BLOCK_SIZE - nModulo);
    }

    // allocate a buffer to write encryption signature and data to
    u8* pDataBuffer = CloudAllocate(nDataBufferSize);
    if(!pDataBuffer)
    {
        gnetError("AddCacheFile :: Failed to add %s. Failed to allocate buffer of %u!", szCacheName, nDataBufferSize);
        CloudSafeFree(pRequest);
        return INVALID_CACHE_REQUEST_ID;
    }

    // write data
    sysMemCpy(pDataBuffer, pData, nDataSize);

    // allocate a buffer to write encryption signature and header
    u8* pHeaderBuffer = CloudAllocate(kCACHE_HEADER_SIZE);
    if(!pHeaderBuffer)
    {
        gnetError("AddCacheFile :: Failed to add %s. Failed to allocate header buffer of %u!", szCacheName, kCACHE_HEADER_SIZE);
        CloudSafeFree(pDataBuffer);
        CloudSafeFree(pRequest);
        return INVALID_CACHE_REQUEST_ID;
    }

    // build header
    sCacheHeader tHeader;
    tHeader.nSemantic = nSemantic;
    tHeader.nModifiedTime = nModifiedTime;
    tHeader.nSize = nDataSize;
    tHeader.bEncrypted = bEncrypted ? TRUE : FALSE;

    // generate CRC - include data hash and header validation
    unsigned nDataCRC = atDataHash(reinterpret_cast<const char*>(pDataBuffer), nDataBufferSize, kCACHE_SEED);
    unsigned nValidation = GenerateHeaderCRC(nSemantic, nModifiedTime, nDataSize, tHeader.bEncrypted);
    tHeader.nCRC = nDataCRC + nValidation;

	// copy into header buffer
	sysMemCpy(pHeaderBuffer, &tHeader, sizeof(sCacheHeader));

#if !__FINAL
	if(!PARAM_cloudCacheDisableEncryption.Get())
#endif
	{
		// encrypt data
		if(!gnetVerify(ms_pCloudCacheAES && ms_pCloudCacheAES->Encrypt(pHeaderBuffer, sizeof(sCacheHeader))))
		{
			gnetError("AddCacheFile :: Failed to add %s. Error encrypting header!", szCacheName);
			CloudSafeFree(pHeaderBuffer);
			CloudSafeFree(pDataBuffer);
			CloudSafeFree(pRequest);
			return INVALID_CACHE_REQUEST_ID;
		}
	}

    // write to file and punt the temporary buffer
    pRequest->m_DataBuffer[0].m_pData = pHeaderBuffer;
    pRequest->m_DataBuffer[0].m_DataSize = kCACHE_HEADER_SIZE;

    // do data after so that the header CRC is on the unencrypted data
    if(bEncrypted)
        EncryptBuffer(pDataBuffer, nDataBufferSize);

    // write to file and punt the temporary buffer
    pRequest->m_DataBuffer[1].m_pData = pDataBuffer;
    pRequest->m_DataBuffer[1].m_DataSize = nDataBufferSize;

	// assign to cache request
	pCacheRequest->m_AsyncRequest = pRequest;
	pCacheRequest->m_nRequestID = ms_nRequestIDCount++;

	// submit write
	pRequest->m_RequestId = pCacheRequest->m_nRequestID;
	fiSubmitAsyncData(pRequest);

	// add to current list
	ms_CacheRequests.PushAndGrow(pCacheRequest);

	// logging
	gnetDebug1("[%d] AddCacheFile :: Adding %s, Time: %u, Encrypted: %s, Size: %u", pCacheRequest->m_nRequestID, szCacheName, nModifiedTime, bEncrypted ? "T" : "F", nDataSize);

	// return cache request ID
	return pCacheRequest->m_nRequestID; 
}

void CloudCache::AddListener(CloudCacheListener* pListener)
{
	m_CacheListeners.PushAndGrow(pListener);
}

bool CloudCache::RemoveListener(CloudCacheListener* pListener)
{
	int nListeners = m_CacheListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
	{
		if(m_CacheListeners[i] == pListener)
		{
			m_CacheListeners.Delete(i);
			return true;
		}
	}
	return false;
}

CacheRequestID CloudCache::OpenCacheFile(u64 nSemantic, CachePartition partition)
{
	return AddCacheRequest(nSemantic, partition);
}

bool CloudCache::CacheFileExists(u64 nSemantic, CachePartition partition)
{
	char szCacheName[RAGE_MAX_PATH];
	char szPath[RAGE_MAX_PATH];
	BuildCacheFileName(szCacheName, nSemantic);
	BuildCacheFilePath(szPath, szCacheName, partition);

	// lock queue CS so we don't try to write or read from it while we check
	s_QueueCs.Lock();

	const fiDevice* pDevice = fiDevice::GetDevice(szPath);
	fiHandle hFile = pDevice->Open(szPath, true);

	const bool bIsValid = fiIsValidHandle(hFile);
	if (bIsValid)
	{
		pDevice->Close(hFile);
	}

	s_QueueCs.Unlock();

	return bIsValid;
}

bool CloudCache::ValidateCache(CacheRequestID nRequestID, sCacheHeader* pHeader, u8** pDataBuffer)
{
    // grab request
    sCacheRequest* pRequest = GetCacheRequest(nRequestID);
    if (!gnetVerify(pRequest))
    {
        gnetError("[%d] ValidateCache :: No cache request", nRequestID);
        return false;
    }

    // grab stream
    fiStream* pStream = pRequest->m_pStream;

    return ValidateCache(pStream, nRequestID, pHeader, pDataBuffer);
}

bool CloudCache::ValidateCache(fiStream* pStream, CacheRequestID OUTPUT_ONLY(nRequestID), sCacheHeader* pHeader, u8** pDataBuffer)
{
    if(!gnetVerify(pStream))
    {
        gnetError("[%d] ValidateCache :: Cache request steam invalid", nRequestID);
		return false;
    }

	// should always be the header and some amount of data
	if(!gnetVerify(pStream->Size() > kCACHE_HEADER_SIZE))
    {
        gnetError("[%d] ValidateCache :: Invalid cache file size of %d", nRequestID, pStream->Size());
		return false;
    }

    // local cache header
    sCacheHeader tHeader;

    // read directly into the header (seek to start)
	if(pStream->Seek(0) != 0 ||
       pStream->Read(reinterpret_cast<u8*>(&tHeader), kCACHE_HEADER_SIZE) != kCACHE_HEADER_SIZE)
	{
		gnetError("[%d] ValidateCache :: Failed reading header", nRequestID);
		return false;
	}

    // decrypt header
#if !__FINAL
	if(!PARAM_cloudCacheDisableEncryption.Get())
#endif
	{
		if(!gnetVerify(ms_pCloudCacheAES && ms_pCloudCacheAES->Decrypt(&tHeader, sizeof(sCacheHeader))))
		{
			gnetError("[%d] ValidateCache :: Error decrypting header!", nRequestID);
			return false;
		}
	}

    // validate header 
    unsigned nValidation = GenerateHeaderCRC(tHeader.nSemantic, tHeader.nModifiedTime, tHeader.nSize, tHeader.bEncrypted);
    unsigned nDataCRC = tHeader.nCRC - nValidation;

    // grab the size and make sure we have enough room
    unsigned nDataBlockSize = pStream->Size() - kCACHE_HEADER_SIZE;

    // validate data size
    if(!(((tHeader.bEncrypted == TRUE) && (nDataBlockSize % kAES_BLOCK_SIZE == 0)) || (nDataBlockSize == tHeader.nSize)))
    {
        gnetError("[%d] ValidateCache :: Invalid Datasize - Size: %d, Stored: %d", nRequestID, nDataBlockSize, tHeader.nSize);
        return false;
    }

    // allocate a buffer to write data block into
    u8* pBuffer = CloudAllocate(nDataBlockSize);
    if(!pBuffer)
    {
        gnetError("[%d] ValidateCache :: Failed to allocate buffer of %dB", nRequestID, nDataBlockSize);
        return false;
    }

    // seek to the end of the header (or the start of the data) and read data block
    if(pStream->Seek(kCACHE_HEADER_SIZE) != kCACHE_HEADER_SIZE ||
       pStream->Read(pBuffer, nDataBlockSize) != static_cast<int>(nDataBlockSize))
    {
        gnetError("[%d] ValidateCache :: Failed reading data!", nRequestID);
        CloudSafeFree(pBuffer);
        return false;
    }

    // decrypt data block
    bool bDataValid = !tHeader.bEncrypted || DecryptBuffer(pBuffer, nDataBlockSize);
    if(!bDataValid)
    {
        gnetError("[%d] ValidateCache :: Invalid data!", nRequestID);
        CloudSafeFree(pBuffer);
        return false;
    }

    // generate CRC and validate against header copy
    unsigned nCachedDataCRC = atDataHash(reinterpret_cast<const char*>(pBuffer), nDataBlockSize, kCACHE_SEED);
    if(nCachedDataCRC != nDataCRC)
    {
        gnetError("[%d] ValidateCache :: Cached data CRC (0x%08x) does not match header CRC (0x%08x)", nRequestID, nCachedDataCRC, nDataCRC);
        CloudSafeFree(pBuffer);
        return false;
    }

    // copy into provided header
    if(pHeader)
        sysMemCpy(pHeader, &tHeader, sizeof(sCacheHeader));

    // copy into provided data pointer
    if(pDataBuffer)
        *pDataBuffer = pBuffer;
    else
        CloudSafeFree(pBuffer);

	// success
	return true;
}

bool CloudCache::GetCacheHeader(CacheRequestID nRequestID, sCacheHeader* pHeader)
{
    return ValidateCache(nRequestID, pHeader, NULL);
}

bool CloudCache::GetCacheData(CacheRequestID nRequestID, void* pDataBuffer, u32 nDataSize)
{
    // needed to copy out data from ValidateCache
    sCacheHeader tHeader; 
    u8* pBuffer = NULL; 
    
    // validate and retrieve data
    if(ValidateCache(nRequestID, &tHeader, &pBuffer))
    {
        // check we have enough space
        if(nDataSize < tHeader.nSize)
        {
            gnetError("[%d] GetCacheData :: Provided data block (%dB) not large enough for cached data (%dB)", nRequestID, nDataSize, tHeader.nSize);
            CloudSafeFree(pBuffer);
            return false;
        }

        // copy and then punt buffer
        sysMemCpy(pDataBuffer, pBuffer, tHeader.nSize);
        CloudSafeFree(pBuffer);

        // success
        return true;
    }

    // fail
    return false;
}

fiStream* CloudCache::GetCacheDataStream(CacheRequestID nRequestID)
{
    // grab request
    sCacheRequest* pRequest = GetCacheRequest(nRequestID);
    if(!gnetVerifyf(pRequest, "[%d] GetCacheDataStream :: No request matching ID", nRequestID))
        return NULL;

    // grab stream
    fiStream* pStream = pRequest->m_pStream;
    if(!gnetVerifyf(pStream, "[%d] GetCacheDataStream :: Stream invalid!", nRequestID))
        return NULL;

    // seek to the data
    if(pStream->Seek(kCACHE_HEADER_SIZE) != kCACHE_HEADER_SIZE)
        return NULL;

    // return the stream
    return pStream;
}

bool CloudCache::RetainCacheFile(CacheRequestID nRequestID)
{
	// grab request
	sCacheRequest* pRequest = GetCacheRequest(nRequestID);
	if(!gnetVerifyf(pRequest, "[%d] RetainCacheFile :: No request matching ID", nRequestID))
		return false;

	// logging
	gnetDebug1("[%d] RetainCacheFile :: Retaining", nRequestID);

	// cache requests cleared by default, this indicates otherwise
	pRequest->m_bFlaggedToClear = false;

	// found the file
	return true;
}

bool CloudCache::ReleaseCacheFile(CacheRequestID nRequestID)
{
	// grab request
	sCacheRequest* pRequest = GetCacheRequest(nRequestID);
	if(!gnetVerifyf(pRequest, "[%d] ReleaseCacheFile :: No request matching ID", nRequestID))
		return false;

    // logging
	gnetDebug1("[%d] ReleaseCacheFile :: Releasing", nRequestID);

	// this is a flag, we'll remove this in the next update
	pRequest->m_bFlaggedToClear = true;

	// found the file
	return true;
}

CacheRequestID CloudCache::AddCacheRequest(u64 nSemantic, CachePartition partition)
{
    // build cache name
    char szCacheName[RAGE_MAX_PATH];
    char szCacheFilePath[RAGE_MAX_PATH];
    BuildCacheFileName(szCacheName, nSemantic);
    BuildCacheFilePath(szCacheFilePath, szCacheName, partition);

	// validate cache is enabled
	if(!ms_bCacheEnabled)
	{
		gnetWarning("AddCacheRequest :: Failed to add %s. Cache not enabled!", szCacheName);
		return INVALID_CACHE_REQUEST_ID;
	}

	// validate cache partition is initialised
	if(!ms_bCachePartitionInitialised)
	{
		gnetWarning("AddCacheRequest :: Failed to request %s. Cache partition not initialised!", szCacheName);
		return INVALID_CACHE_REQUEST_ID;
	}

    // If the request is currently being written out, fail the read for now.
	if(fiIsAsyncWritePending((void*)(size_t)nSemantic))
    {
        gnetDebug1("AddCacheRequest :: Pending write for %s", szCacheName);
		return INVALID_CACHE_REQUEST_ID;
    }

	// prop request for opening
	sCacheRequest* pCacheRequest = (sCacheRequest*)CloudAllocate(sizeof(sCacheRequest));
    if(!pCacheRequest)
    {
        gnetDebug1("AddCacheRequest :: Failed to allocate cache request");
        return INVALID_CACHE_REQUEST_ID;
    }
    new(pCacheRequest) sCacheRequest();

    // allocate data request
	fiAsyncDataRequest* pDataRequest = (fiAsyncDataRequest*)CloudAllocate(sizeof(fiAsyncDataRequest));
    if(!pDataRequest)
    {
        gnetDebug1("AddCacheRequest :: Failed to allocate data request");
        CloudSafeFree(pCacheRequest);
        return INVALID_CACHE_REQUEST_ID;
    }
    new(pDataRequest) fiAsyncDataRequest();

    // assign to cache request
    pCacheRequest->m_AsyncRequest = pDataRequest;
	pCacheRequest->m_nRequestID = ms_nRequestIDCount++;

    // setup data request
    safecpy(pDataRequest->m_szName, szCacheFilePath);
    pDataRequest->m_UserData = pCacheRequest;
	pDataRequest->m_Operation = CloudCacheOperation::Read;
	pDataRequest->m_WriteCount = 0;	// this signals a read, not a write (buffers will be allocated by worker thread)
	pDataRequest->m_RequestId = pCacheRequest->m_nRequestID;
	fiSubmitAsyncData(pDataRequest);

	// add to current list
    ms_CacheRequests.PushAndGrow(pCacheRequest);

	// logging
	gnetDebug1("[%d] AddCacheRequest :: Requesting %s", pCacheRequest->m_nRequestID, szCacheName);

    // return cache request ID
	return pCacheRequest->m_nRequestID; 
}

sCacheRequest* CloudCache::GetCacheRequest(CacheRequestID nRequestID)
{
	int nCacheRequests = ms_CacheRequests.GetCount();
	for(int i = 0; i < nCacheRequests; i++)
	{
		sCacheRequest* pRequest = ms_CacheRequests[i];
		if(pRequest->m_nRequestID == nRequestID)
			return pRequest;
	}

	return NULL;
}

CacheRequestID CloudCache::AddDeleteOldFilesRequest()
{
#if CUSTOM_PERSISTENT_CACHE
	// validate cache is enabled
	if(!ms_bCacheEnabled)
	{
		gnetWarning("AddDeleteOldFilesRequest :: Cache not enabled!");
		return INVALID_CACHE_REQUEST_ID;
	}

	// validate cache partition is initialised
	if(!ms_bCachePartitionInitialised)
	{
		gnetWarning("AddDeleteOldFilesRequest :: Cache partition not initialised!");
		return INVALID_CACHE_REQUEST_ID;
	}

	// prop request for opening
	sCacheRequest* pCacheRequest = (sCacheRequest*)CloudAllocate(sizeof(sCacheRequest));
	if (!pCacheRequest)
	{
		gnetDebug1("AddDeleteOldFilesRequest :: Failed to allocate cache request");
		return INVALID_CACHE_REQUEST_ID;
	}
	new(pCacheRequest) sCacheRequest();

	fiAsyncDataRequest* pDataRequest = (fiAsyncDataRequest*)CloudAllocate(sizeof(fiAsyncDataRequest));
	if (!pDataRequest)
	{
		gnetDebug1("AddDeleteOldFilesRequest :: Failed to allocate data request");
		CloudSafeFree(pCacheRequest);
		return INVALID_CACHE_REQUEST_ID;
	}
	new(pDataRequest) fiAsyncDataRequest();

	// assign to cache request
	pCacheRequest->m_AsyncRequest = pDataRequest;
	pCacheRequest->m_nRequestID = ms_nRequestIDCount++;

	// setup data request
	safecpy(pDataRequest->m_szName, "delete old");
	pDataRequest->m_UserData = pCacheRequest;
	pDataRequest->m_Operation = CloudCacheOperation::DeleteOldFiles;
	pDataRequest->m_WriteCount = 0;
	pDataRequest->m_RequestId = pCacheRequest->m_nRequestID;
	fiSubmitAsyncData(pDataRequest);

	// add to current list
	ms_CacheRequests.PushAndGrow(pCacheRequest);

	// logging
	gnetDebug1("[%d] AddDeleteOldFilesRequest", pCacheRequest->m_nRequestID);

	// return cache request ID
	return pCacheRequest->m_nRequestID;
#else
	return INVALID_CACHE_REQUEST_ID;
#endif
}

unsigned CloudCache::GenerateHeaderCRC(u64 nSemantic, u32 nModifiedTime, u32 nDataSize, u32 nEncrypted)
{
    static const unsigned kMaxString = 256;
    char szQualifier[kMaxString];
    formatf(szQualifier, "%016" I64FMT "x.%d.%d.%d", nSemantic, nModifiedTime, nDataSize, nEncrypted);

    // use the version to build the hash to avoid storing it with the cache
    return atDataHash(szQualifier, strlen(szQualifier), CLOUD_CACHE_VERSION);
}

#if __BANK
#define EXPORT_PATH_BASE "X:\\dumps\\"
#define EXPORT_PATH_TEMP "temp\\"
#define EXPORT_PATH_PERSISTENT "persistent\\"
#define EXPORT_PATH_FMT "%s%s_%s"

void MakeExportDir(const char* dest)
{
    const fiDevice* dstDevice = fiDevice::GetDevice(EXPORT_PATH_BASE);

    const char* ipAddr = fiDeviceTcpIp::GetLocalIpAddrName();

    char buffer[RAGE_MAX_PATH] = { 0 };
    if (dstDevice)
    {
        dstDevice->MakeDirectory(EXPORT_PATH_BASE);

        formatf(buffer, EXPORT_PATH_FMT, EXPORT_PATH_BASE, ipAddr, dest);
        dstDevice->MakeDirectory(buffer);
    }
}

void ExportTempFileFunc(const char* path, fiHandle UNUSED_PARAM(handle), const fiFindData& UNUSED_PARAM(data))
{
    char destFolder[RAGE_MAX_PATH] = { 0 };
    const char* ipAddr = fiDeviceTcpIp::GetLocalIpAddrName();
    formatf(destFolder, EXPORT_PATH_FMT, EXPORT_PATH_BASE, ipAddr, EXPORT_PATH_TEMP);

    CloudCache::ExportFile(path, destFolder);
}

void ExportPersistentFileFunc(const char* path, fiHandle UNUSED_PARAM(handle), const fiFindData& UNUSED_PARAM(data))
{
    char destFolder[RAGE_MAX_PATH] = { 0 };
    const char* ipAddr = fiDeviceTcpIp::GetLocalIpAddrName();
    formatf(destFolder, EXPORT_PATH_FMT, EXPORT_PATH_BASE, ipAddr, EXPORT_PATH_PERSISTENT);

    CloudCache::ExportFile(path, destFolder);
}

void CloudCache::ExportFile(const char* path, const char* destinationFolder)
{
    USE_DEBUG_MEMORY();

    u8* buffer = nullptr;
    u8* cloudBuffer = nullptr;
    fiHandle handle = fiHandleInvalid;
    fiHandle inhandle = fiHandleInvalid;
    const fiDevice* srcDevice = nullptr;
    const fiDevice* dstDevice = nullptr;
    fiStream* srcStream = nullptr;
    sCacheHeader tHeader;
    char fullName[RAGE_MAX_PATH];

    rtry
    {
        const char* name = path;

        bool isPersistent = false;
        bool isTemp = false;
        if (StringStartsWithI(path, fiPersistentCachePartition::GetCachePrefix()))
        {
            name = &path[strlen(fiPersistentCachePartition::GetCachePrefix())];
            isPersistent = true;
        }
        else if (StringStartsWithI(path, fiCachePartition::GetCachePrefix()))
        {
            name = &path[strlen(fiCachePartition::GetCachePrefix())];
            isTemp = true;
        }

        char tempName[RAGE_MAX_PATH];
        formatf(tempName, "%s", name);

        StringReplaceChars(tempName, "/\\", '_');

        formatf(fullName, "%s%s", destinationFolder, tempName);

        srcDevice = fiDevice::GetDevice(path);
        dstDevice = fiDevice::GetDevice(fullName);

        rverify(srcDevice && srcDevice, catchall, );

        u64 fileSize = srcDevice->GetFileSize(path);
        rcheck(fileSize > 0, catchfileempty, );

        buffer = rage_new u8[fileSize];

        //char path[RAGE_MAX_PATH];
        //FixName(path, RAGE_MAX_PATH, filename);

        inhandle = srcDevice->Open(path, true);
        rcheck(inhandle != fiHandleInvalid, catchall, );

        int bytesRead = srcDevice->Read(inhandle, buffer, static_cast<int>(fileSize));
        rcheck(bytesRead == fileSize, catchall, );

        u8* data = buffer;
        u64 dataSize = bytesRead;

        if (isTemp || isPersistent)
        {
            rverify(fileSize > sizeof(unsigned), catchall, );

            char szFileName[128];
            fiDevice::MakeMemoryFileName(szFileName, sizeof(szFileName), buffer + sizeof(unsigned), fileSize - sizeof(unsigned), false, "export_cache");

            srcStream = fiStream::Open(szFileName, true);
            rcheck(srcStream, catchall, );

            // validate and retrieve data
            rverify(ValidateCache(srcStream, 0, &tHeader, &cloudBuffer), catchall, );

            data = cloudBuffer;
            dataSize = tHeader.nSize;

            const u8 c_PNGHeaderBytes[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
            const u16 soiId = sysEndian::NtoL(*((u16 *)data));
            const u16 marker = sysEndian::NtoL(*(((u16 *)(data)+1)));

            if (dataSize > 8)
            {
                if (memcmp(data, c_PNGHeaderBytes, sizeof(c_PNGHeaderBytes)) == 0)
                {
                    safecat(fullName, ".png");
                }
                else if (data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ')
                {
                    safecat(fullName, ".dds");
                }
                else if (soiId == 0xd8ff && ((marker & 0xe0ff) == 0xe0ff))
                {
                    safecat(fullName, ".jpg");
                }
                else if (data[0] == '{' && data[1] == '"') // Good chance it's a json file
                {
                    safecat(fullName, ".json");
                }
                else if (data[3] == 'R' && data[2] == 'P' && data[1] == 'F')
                {
                    safecat(fullName, ".rpf");
                }
            }
        }

        handle = dstDevice->Create(fullName);
        rverify(fiIsValidHandle(handle), catchall, );

        int bytesWritten = dstDevice->Write(handle, data, static_cast<int>(dataSize));
        rverify(bytesWritten == dataSize, catchall, );

    }
    rcatch(catchfileempty)
    {
        safecat(fullName, ".empty");

        fiHandle failHandle = dstDevice ? dstDevice->Create(fullName) : fiHandleInvalid;
        if (fiIsValidHandle(failHandle))
        {
            dstDevice->Close(failHandle);
        }
    }
    rcatchall
    {
        safecat(fullName, ".EXPORT_FAILED");

        fiHandle failHandle = dstDevice ? dstDevice->Create(fullName) : fiHandleInvalid;
        if (fiIsValidHandle(failHandle))
        {
            dstDevice->Close(failHandle);
        }
    }

    if (fiIsValidHandle(handle))
    {
        dstDevice->Close(handle);
    }

    if (fiIsValidHandle(inhandle))
    {
        srcDevice->Close(inhandle);
    }

    if (srcStream)
    {
        srcStream->Close();
        srcStream = nullptr;
    }

    if (buffer)
    {
        delete[] buffer;
        buffer = nullptr;
    }

    CloudSafeFree(cloudBuffer);
}

void CloudCache::ExportFiles(CachePartition partition)
{
    if (partition == CachePartition::Default)
    {
        MakeExportDir(EXPORT_PATH_TEMP);
        fiCacheUtils::ListFiles(fiCachePartition::GetCachePrefix(), ExportTempFileFunc);
    }
    else
    {
        // We try to export all files in the persistent storage, not just those in the download folder
        const char* rootPath = fiPersistentCachePartition::GetPersistentStorageRoot();
        rootPath = rootPath == nullptr ? fiPersistentCachePartition::GetCachePrefix() : rootPath;

        MakeExportDir(EXPORT_PATH_PERSISTENT);
        fiCacheUtils::ListFiles(rootPath, ExportPersistentFileFunc);
    }
}

bool CloudCache::SemanticFromFileName(const char* filename, u64& semantic)
{
    rtry
    {
        rcheckall(filename != nullptr);
        rcheckall(StringStartsWith(filename, CLOUD_CACHE_FILE_PREFIX));
        rcheckall(StringEndsWith(filename, CLOUD_CACHE_FILE_EXTENSION));
        rcheckall(sscanf(filename, CLOUD_CACHE_FILE_FMT, &semantic) == 1);
        return true;
    }
    rcatchall
    {
    }

    return false;
}

bool CloudCache::DeleteCacheFileBlocking(u64 nSemantic, CachePartition partition)
{
    char szCacheName[RAGE_MAX_PATH];
    char szPath[RAGE_MAX_PATH];
    BuildCacheFileName(szCacheName, nSemantic);
    BuildCacheFilePath(szPath, szCacheName, partition);

    SYS_CS_SYNC(s_QueueCs);

    const fiDevice* pDevice = fiDevice::GetDevice(szPath);
    return pDevice->Delete(szPath);
}

#endif //__BANK