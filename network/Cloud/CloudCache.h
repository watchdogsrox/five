//
// cloud/CloudCache.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CLOUD_CACHE_H
#define CLOUD_CACHE_H

#include "atl/array.h"
#include "data/aes.h"
#include "file/limits.h"

static const unsigned CLOUD_CACHE_VERSION = 10;

namespace rage
{
	class fiStream;
}

struct sCacheHeader
{
	u64 nSemantic;		//! semantic hash - unique identifier derived from filename
	u32 nModifiedTime;  //! when this file was last modified
	u32 nSize;			//! size of this file
    u32 bEncrypted;		//! whether the data is encrypted or not
    u32 nCRC;			//! CRC for this file - verified against cloud copy
	u64 nPadding;		//! to round up the size 
};

typedef int CacheRequestID;
static const int INVALID_CACHE_REQUEST_ID = -1;
struct fiAsyncDataRequest;

struct sCacheRequest
{
    sCacheRequest() : m_nRequestID(-1), m_pStream(NULL), m_bFlaggedToClear(false) {}
	sCacheRequest(CacheRequestID nID) : m_nRequestID(nID), m_pStream(NULL), m_bFlaggedToClear(false) {}

	CacheRequestID m_nRequestID; 
	fiAsyncDataRequest* m_AsyncRequest; 
	fiStream* m_pStream;
	bool m_bFlaggedToClear;
    
	bool operator==(const sCacheRequest& other) const
	{
		return (other.m_nRequestID == m_nRequestID);
	}
};

class CloudCacheListener
{
protected:

	CloudCacheListener();
	virtual ~CloudCacheListener();

public:

    virtual void OnCacheFileLoaded(const CacheRequestID /*nRequestID*/, bool /*bCacheLoaded*/) {}
    virtual void OnCacheFileAdded(const CacheRequestID /*nRequestID*/, bool /*bSucceeded*/) {}
};

typedef atArray<sCacheRequest*> CacheRequestList;

enum class CachePartition
{
	Default,
	Persistent
};

class CloudCache
{
public:

	static void Init();
	static void Shutdown();
	static void Update(); 

	static bool Load();
	static void Save();

	static void Drain();

	// async access
    static CacheRequestID AddCacheFile(u64 nSemantic, u32 nModifiedTime, bool bEncrypted, CachePartition partition, const void* pData, u32 nDataSize);
    static CacheRequestID OpenCacheFile(u64 nSemantic, CachePartition partition);
	static bool CacheFileExists(u64 nSemantic, CachePartition partition);
    static bool ValidateCache(CacheRequestID nRequestID, sCacheHeader* pHeader, u8** pDataBuffer);
    static bool GetCacheHeader(CacheRequestID nRequestID, sCacheHeader* pHeader);
	static bool GetCacheData(CacheRequestID nRequestID, void* pDataBuffer, u32 nDataSize);
    static fiStream* GetCacheDataStream(CacheRequestID nRequestID); 
	static bool RetainCacheFile(CacheRequestID nRequestID);
	static bool ReleaseCacheFile(CacheRequestID nRequestID);

	// listener
	static void AddListener(CloudCacheListener* pListener);
	static bool RemoveListener(CloudCacheListener* pListener);

	//are we ready to receive calls
	static bool IsCacheEnabled() {return ms_bCacheEnabled;}

#if RSG_BANK
	// Export the cloud file to a local directory
	static void ExportFile(const char* sourceFilePath, const char* destinationFolder);
	// Export all files of the specified partition
	static void ExportFiles(CachePartition partition);
	static bool SemanticFromFileName(const char* filename, u64& semantic);
	static bool DeleteCacheFileBlocking(u64 nSemantic, CachePartition partition);
#endif

private:

    static unsigned GenerateHeaderCRC(u64 nSemantic, u32 nModifiedTime, u32 nDataSize, u32 bEncrypted);

	static CacheRequestID AddCacheRequest(u64 nSemantic, CachePartition partition);
	static sCacheRequest* GetCacheRequest(CacheRequestID nRequestID);
	static CacheRequestID AddDeleteOldFilesRequest();

	static bool ValidateCache(fiStream* pStream, CacheRequestID nRequestID, sCacheHeader* pHeader, u8** pDataBuffer);
    static void BuildCacheFileName(char(&fileName)[RAGE_MAX_PATH], const u64 nSemantic);
    static void BuildCacheFilePath(char(&filePath)[RAGE_MAX_PATH], const char* const pFilename, CachePartition partition);
	static bool EncryptBuffer(void* pBuffer, unsigned nBufferSize);
	static bool DecryptBuffer(void* pBuffer, unsigned nBufferSize);
	
	static bool ms_bCacheEnabled;
	static bool ms_bCachePartitionInitialised;

	static CacheRequestList ms_CacheRequests;
	static CacheRequestID	ms_nRequestIDCount;

	static atArray<CloudCacheListener*> m_CacheListeners;
	static AES*				ms_pCloudCacheAES;
};


#endif // CLOUD_CACHE_H

