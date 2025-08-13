//===========================================================================
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.             
//===========================================================================
#include "Network/Cloud/CloudManager.h"
#include "Network/Cloud/CloudCache.h"

// rage headers
#include "bank/bank.h"
#include "data/aes.h"
#include "file/device.h"
#include "net/http.h"
#include "net/nethardware.h"
#include "parser/manager.h"
#include "rline/rltitleid.h"
#include "rline/ros/rlros.h"
#include "rline/cloud/rlcloud.h"
#include "system/param.h"

// framework headers
#include "fwnet/netchannel.h"
#include "fwnet/netcloudfiledownloadhelper.h"
#include "streaming/streamingengine.h"

// game headers
#include "Event/EventGroup.h"
#include "Event/EventNetwork.h"
#include "Network/NetworkInterface.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, cloudmgr, DIAG_SEVERITY_DEBUG3);
#undef __net_channel
#define __net_channel net_cloudmgr

PARAM(cloudCacheAll, "If set, all cloud sourced files will be added to system cache");
PARAM(cloudCacheNone, "If set, no cloud sourced files will be added to system cache");
PARAM(cloudForceGetCloud, "If set, we will always get the cloud content - regardless of what's in the cache. This will still be cached");
PARAM(cloudForceUseCache, "If set, we will always return what's in the cache without hitting the cloud (if cached file available - otherwise, cloud as normal)");
PARAM(cloudForceAvailable, "If set, NetworkInterface::IsCloudAvailable is always true. Every day is cloud day.");
PARAM(cloudForceNotAvailable, "If set, NetworkInterface::IsCloudAvailable is always false.");
PARAM(cloudNoAllocator, "Don't use an allocator to make cloud manager cloud requests");
PARAM(cloudForceEnglishUGC, "Set UGC download requests to use English language");
PARAM(cloudUseStreamingForPost, "Use streaming heap for cloud post requests");
PARAM(cloudAllowCheckMemberSpace, "Allow member space checks by default");
PARAM(cloudOverrideAvailabilityResult, "Override the result of our cloud available check");
PARAM(cloudEnableAvailableCheck, "Enables / Disables the cloud available check");

namespace rage {
	XPARAM(rlrosdomainenv);
	extern sysMemAllocator* g_rlAllocator;
}

const u32 ATTR_FILE			= ATSTRINGHASH("file", 0xb2744904);
const u32 ATTR_FILENAME		= ATSTRINGHASH("filename", 0x40ca983a);
const u32 ATTR_NAMESPACE	= ATSTRINGHASH("namespace", 0xb25e00b2);
const u32 ATTR_ID			= ATSTRINGHASH("id", 0x1b60404d);
const u32 ATTR_CACHE_TYPE	= ATSTRINGHASH("cacheType", 0xbe1f38c8);
const u32 ATTR_VERSION		= ATSTRINGHASH("version", 0x68c27e33);
const u32 ATTR_DEVVERSION	= ATSTRINGHASH("devversion", 0xe011a83c);

static const char* g_szNamespace[kNamespace_Num] =
{
	"Member",	// kNamespace_Member
	"Crew",		// kNamespace_Crew
	"UGC",		// kNamespace_UGC
	"Title",	// kNamespace_Title
	"Global",	// kNamespace_Global
	"WWW",		// kNamespace_WWW
};

//#if !__NO_OUTPUT
//
//static const char* g_szCacheType[kCache_Num] =
//{
//	"Version",				// kCache_Version
//	"TimeModified",			// kCache_TimeModified
//	"DoNotCache",			// kCache_DoNotCache
//};
//
//#endif

u8* CloudAllocate(unsigned nSize, unsigned nAlignment)
{
    // use game heap
    sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
    sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
    u8* pData = reinterpret_cast<u8*>(sysMemAllocator::GetCurrent().TryAllocate(nSize, nAlignment));
    sysMemAllocator::SetCurrent(oldAllocator);

    // return data
    return pData; 
}

void CloudFree(void* pAddress)
{
    // validate that we were given something
    if(!gnetVerify(pAddress))
        return;

    // use game heap
    sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
    sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
    sysMemAllocator::GetCurrent().Free(pAddress);
    sysMemAllocator::SetCurrent(oldAllocator);
}

netHttpOptions BuildHttpOptionsFromCloudRequestFlags(const unsigned cloudRequestFlags)
{
    unsigned httpOptions = NET_HTTP_OPTIONS_NONE;

    if ((cloudRequestFlags & eRequestFlags::eRequest_MaintainQueryOnRedirect) != 0)
        httpOptions |= NET_HTTP_MAINTAIN_QUERY_ON_REDIRECT;

    return static_cast<netHttpOptions>(httpOptions);
}

CloudListener::CloudListener()
{
	CloudManager::GetInstance().AddListener(this);
}

CloudListener::~CloudListener()
{
	CloudManager::GetInstance().RemoveListener(this);
}

CCloudManager::CloudRequestNode::~CloudRequestNode()
{
    CloudSafeDelete(m_pRequest);
}

bool CCloudManager::sm_bCacheEncryptedEnabled = true; 

CCloudManager::CCloudManager()
    : m_bRunRequests(false)
	, m_nRequestIDCount(0)
    , m_AvailablityCheckRequestId(INVALID_CLOUD_REQUEST_ID)
    , m_AvailablityCheckPosix(0) //< not checked
    , m_bAvailablityCheckResult(false)
    , m_bHasCredentialsResult(false)
	, m_bIsCloudAvailable(false)
{
#if __BANK
	m_BankFullCloudFailureCounter = 0;
	m_BankFullHTTPFailureCounter = 0;
	m_bBankApplyOverrides = false;
    m_BankFileListCombo = nullptr;
    m_BankFileListIdx = 0;
    m_BankFullFailureTimeSecs = 30;
    m_BankFullCloudFailureCounter = 0;
    m_BankFullHTTPFailureCounter = 0;
    m_BankDownloadStart = 0;
    m_BankDownloadFailed = 0;
    m_BankDownloadSucceeded = 0;
    m_BankDownloadDuration = 0;
    m_BankDownloadBytes = 0;
    m_BankDownloadBps = 0;
    m_BankForceCloudDownload = false;
    m_BankForceCache = false;
    m_BankContinuousDownload = false;
#endif
}

CCloudManager::~CCloudManager()
{
	Shutdown();
}

void CCloudManager::Init()
{
	// initialise cache
	CloudCache::Init();

	// flatten state
	m_bRunRequests = true; 
	m_nRequestIDCount = 0;
	m_bIsCloudAvailable = false;

#if __BANK
    m_BankCloudListener = rage_new BankCloudListener();
#endif
}

void CCloudManager::Shutdown()
{
#if __BANK
    if (m_BankCloudListener)
    {
        delete m_BankCloudListener;
        m_BankCloudListener = nullptr;
    }
#endif

    // punt any pending nodes
    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
        CloudSafeDelete(m_CloudRequestList[i]);

    m_CloudRequestList.Reset();
	m_CloudPendingList.Reset();

	// drop cache
	CloudCache::Shutdown();
}

void CCloudManager::AddListener(CloudListener* pListener)
{
	m_CloudListeners.PushAndGrow(pListener);
}

bool CCloudManager::RemoveListener(CloudListener* pListener)
{
	int nListeners = m_CloudListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
	{
		if(m_CloudListeners[i] == pListener)
		{
			m_CloudListeners.Delete(i);
			return true;
		}
	}
	return false;
}

void CCloudManager::Update()
{
	// track cloud availability
	bool bIsCloudAvailable = NetworkInterface::IsCloudAvailable();
	if(bIsCloudAvailable != m_bIsCloudAvailable)
	{
		// logging for tracking
		gnetDebug1("AvailabilityChanged :: %s", bIsCloudAvailable ? "Available" : "Not Available");

		// build event
		CloudEvent tCloudEvent; 
		tCloudEvent.OnAvailabilityChanged(bIsCloudAvailable);

		// manage pending requests
		CheckPendingRequests();

		// if we gain cloud access whilst a request is in flight, flag a retry
		if(bIsCloudAvailable)
		{
			int nNodes = m_CloudRequestList.GetCount();
			for(int i = 0; i < nNodes; i++)
			{
				CloudRequestNode* pNode = m_CloudRequestList[i];
				if(pNode->m_pRequest->IsProcessing())
					pNode->m_pRequest->FlagRetry();
			}
		}

		// callback
		int nListeners = m_CloudListeners.GetCount(); 
		for(int i = 0; i < nListeners; i++)
			m_CloudListeners[i]->OnCloudEvent(&tCloudEvent);

		// update tracking variable
		m_bIsCloudAvailable = bIsCloudAvailable;
	}

	if(m_bRunRequests)
	{
		// may have deleted some nodes
        int nNodes = m_CloudRequestList.GetCount();
        for(int i = 0; i < nNodes; i++)
        {
            CloudRequestNode* pNode = m_CloudRequestList[i];

			// don't process nodes that are pending delete
			if(pNode->m_bFlaggedToClear)
				continue;

			// retrieve and update the request
			rage::netCloudRequestHelper* pRequest = pNode->m_pRequest;
			pRequest->Update();

			// cache rlCloudFileInfo (will be NULL for POST requests)
			rlCloudFileInfo* pFileInfo = pRequest->GetFileInfo();

			// if the request has finished...
			if(pRequest->HasFinished() && !pNode->m_bHasProcessedRequest)
			{
				// flag that we shouldn't do this again
				pNode->m_bHasProcessedRequest = true; 

				// grab the result code
				int nResultCode = pRequest->GetResultCode();
#if !__NO_OUTPUT
				u32 nTime = sysTimer::GetSystemMsTime() - pNode->m_nTimestamp;
				gnetDebug1("[%d] RequestFinished :: ResultCode: %d, Time: %d, Modified: %u, Size: %u", pNode->m_nRequestID, nResultCode, nTime, pFileInfo ? static_cast<u32>(pFileInfo->m_LastModifiedPosixTime) : 0, pFileInfo ? pFileInfo->m_ContentLength : 0);
				pNode->m_nTimestamp = sysTimer::GetSystemMsTime();
#endif
				// have we requested a file?
				if(pNode->m_nSemantic != 0)
				{
					// cleanup flag for failed requests
					bool bFailed = false;

					// if this is a pending cache, it means we need to store the updated timestamp in the cached file
					if((pNode->m_nRequestFlags & eRequest_CacheOnPost) != 0)
					{
						if(pFileInfo && pFileInfo->m_LastModifiedPosixTime != 0)
						{
							gnetDebug1("[%d]\t RequestFinished :: Posted at %d", pNode->m_nRequestID, static_cast<u32>(pFileInfo->m_LastModifiedPosixTime));

                            // this makes a blocking read / write (removed for now, while we look
                            // to upgrade UpdateCacheHeader to async)
							//CloudCache::UpdateCacheHeader(pNode->m_nCacheSemantic, static_cast<u32>(pFileInfo->m_LastModifiedPosixTime));
						}

						// callback (no data)
						OnRequestFinished(pNode->m_nRequestID, pRequest->DidSucceed(), nResultCode, NULL, 0, pRequest->GetPath(), 0, false);
					}
					// we have a request
					else
					{
						// new file data
						if(pRequest->DidSucceed())
						{
							switch(nResultCode)
							{
							case NET_HTTPSTATUS_NOT_FOUND:
								gnetDebug1("[%d]\t RequestFinished :: Not Found", pNode->m_nRequestID);
								break;

							case NET_HTTPSTATUS_NOT_MODIFIED:
								{
									const CachePartition partition = (pNode->m_nRequestFlags & eRequest_UsePersistentCache) != 0 ? CachePartition::Persistent : CachePartition::Default;
									CacheRequestID nRequestID = CloudCache::OpenCacheFile(pNode->m_nCacheSemantic, partition);
									if(nRequestID != INVALID_CACHE_REQUEST_ID)
									{
										// flag that we're waiting and capture the request ID
										pNode->m_nCacheRequestID = nRequestID;
										pNode->m_nCacheRequestType = eCacheRequest_Data;
									}
									else
									{
										gnetError("[%d]\t RequestFinished :: Invalid cache request ID!", pNode->m_nRequestID);
									}

									// logging
									gnetDebug1("[%d]\t RequestFinished :: Not Modified. CacheID: %d", pNode->m_nRequestID, nRequestID);
								}
								break; 

							default:
								{				
									// we have new data
									rage::netCloudRequestGetFile* pFileRequest = static_cast<rage::netCloudRequestGetFile*>(pRequest);
									datGrowBuffer& tGB = pFileRequest->GetGrowBuffer();

									gnetDebug1("[%d]\t RequestFinished :: Received Content. Size: %d", pNode->m_nRequestID, tGB.Length());
									
									// grab the file info
									rlCloudFileInfo* pFileInfo = pRequest->GetFileInfo();
									if(!gnetVerify(pFileInfo))
									{
										gnetError("[%d]\t RequestFinished :: Invalid File Info!", pNode->m_nRequestID);
										bFailed = true;
									}

									// if encrypted, decrypt using cloud key
									if(pFileInfo->m_Encrypted || (pNode->m_nRequestFlags & eRequest_CloudEncrypted) != 0)
									{
										// check cloud AES key is valid
										if(!AES::GetCloudAes())
										{
											gnetError("[%d]\t RequestFinished :: Encrypted but AES key invalid!", pNode->m_nRequestID);
											bFailed = true;
										}
										else
										{
											if(!gnetVerify(AES::GetCloudAes()->Decrypt(tGB.GetBuffer(), tGB.Length())))
											{
												gnetError("[%d]\t RequestFinished :: Error decrypting!", pNode->m_nRequestID);
												bFailed = true;
											}
											else
											{
												gnetDebug1("[%d]\t RequestFinished :: Decrypted %u bytes!", pNode->m_nRequestID, tGB.Length());
											}
										}
									}

									if(!bFailed)
									{
										// check the cache flag
										if((pNode->m_nRequestFlags & eRequest_CacheAdd) != 0 && tGB.Length() > 0)
										{
											gnetDebug1("[%d]\t RequestFinished :: Caching. Modified: %d", pNode->m_nRequestID, (u32)pFileInfo->m_LastModifiedPosixTime);
											const CachePartition partition = (pNode->m_nRequestFlags & eRequest_UsePersistentCache) != 0 ? CachePartition::Persistent : CachePartition::Default;
											CloudCache::AddCacheFile(pNode->m_nCacheSemantic, (u32)pFileInfo->m_LastModifiedPosixTime, (pNode->m_nRequestFlags & eRequest_CacheEncrypt) != 0, partition, tGB.GetBuffer(), tGB.Length());
										}

										// callback (with data)
										OnRequestFinished(pNode->m_nRequestID, true, nResultCode, tGB.GetBuffer(), tGB.Length(), pRequest->GetPath(), pFileInfo->m_LastModifiedPosixTime, false);
									}
								}
							}
						}
						// failed
						else
						{
							// 
							if((pNode->m_nRequestFlags & eRequest_CheckingMemberSpace) != 0)
							{
								// remove flag
								pNode->m_nRequestFlags &= ~eRequest_CheckingMemberSpace;

								gnetDebug1("[%d]\t RequestFinished :: Failed via member space. TitlePath: %s", pNode->m_nRequestID, pNode->m_szCloudPath);
								
								// it's possible to schedule a title request without the title path (i.e. fail if we don't have the member file)
								if(pNode->m_szCloudPath[0] != '\0')
								{
									// grab current request
									rage::netCloudRequestGetFile* pMemberRequest = static_cast<rage::netCloudRequestGetFile*>(pNode->m_pRequest);

									// create and initialise request
									rage::netCloudRequestGetTitleFile* pRequest = (rage::netCloudRequestGetTitleFile*)CloudAllocate(sizeof(rage::netCloudRequestGetTitleFile));
									if(!pRequest)
										gnetError("[%d]\t RequestFinished :: Failed via member space. Cannot allocate new request", pNode->m_nRequestID);
									else
									{
										// construct request
										new(pRequest) rage::netCloudRequestGetTitleFile(GetFullCloudPath(pNode->m_szCloudPath, pNode->m_nRequestFlags), 0, 
											BuildHttpOptionsFromCloudRequestFlags(pNode->m_nRequestFlags),
											pMemberRequest->GetPresize(), datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

										// delete the request and re-issue via title space 
										CloudSafeDelete(pNode->m_pRequest);

										// assign request and begin, reset cache ID
										pNode->m_pRequest = pRequest;
										pNode->m_nCacheSemantic = 0;
										BeginRequest(pNode);

										// next node, we're done here
										continue;
									}
								}
							}

							// if we failed with a non-404 and have cache, retrieve that (unless task was cancelled)
							// consider a result code of 0 as client side error - do not use cache in this case
							if((nResultCode != NET_HTTPSTATUS_NOT_FOUND) && (nResultCode != 0) && !pRequest->WasCancelled())
							{
								if((pNode->m_nRequestFlags & eRequest_CacheAdd) != 0 && (pNode->m_nRequestFlags & eRequest_AllowCacheOnError) != 0 && pRequest->GetTimestamp() > 0)
								{
									// request the cache data
									CacheRequestID nRequestID = CloudCache::OpenCacheFile(pNode->m_nCacheSemantic, (pNode->m_nRequestFlags & eRequest_UsePersistentCache) != 0 ? CachePartition::Persistent : CachePartition::Default);
									if(nRequestID != INVALID_CACHE_REQUEST_ID)
									{
										// flag that we're waiting and capture the request ID
										pNode->m_nCacheRequestID = nRequestID;
										pNode->m_nCacheRequestType = eCacheRequest_Data;
										gnetDebug1("[%d]\t RequestFinished :: Server Error. Using cache. ID: %d", pNode->m_nRequestID, pNode->m_nCacheRequestID);
									}
									else
									{
										gnetError("[%d]\t RequestFinished :: Invalid cache request ID!", pNode->m_nRequestID);
									}
								}
							}

							// if we didn't pull from cache
							if(pNode->m_nCacheRequestID == INVALID_CACHE_REQUEST_ID)
							{
								gnetDebug1("[%d]\t RequestFinished :: Failed", pNode->m_nRequestID);
								OnRequestFinished(pNode->m_nRequestID, false, nResultCode, NULL, 0, pRequest->GetPath(), 0, false);
							}
						}
					}

					// if we failed, signal to interested parties
					if(bFailed)
					{
						gnetDebug1("[%d]\t RequestFinished :: Failed", pNode->m_nRequestID);
						OnRequestFinished(pNode->m_nRequestID, false, nResultCode, NULL, 0, pRequest->GetPath(), 0, false);
					}
				}
				else
				{
					// callback (no data)
					OnRequestFinished(pNode->m_nRequestID, pRequest->DidSucceed(), nResultCode, NULL, 0, pRequest->GetPath(), 0, false);
				}

				// flag request to be cleared if we're not waiting on the cache to callback
				if(pNode->m_nCacheRequestID == INVALID_CACHE_REQUEST_ID)
				{
					gnetDebug1("[%d]\t RequestFinished :: Flagged to Clear. Time: %d", pNode->m_nRequestID, sysTimer::GetSystemMsTime() - pNode->m_nTimeStarted);

					pNode->m_bFlaggedToClear = true;
					pNode->m_bSucceeded = pNode->m_pRequest->DidSucceed();

					// generate an event for script - needs to be a frame ahead of actually being removed
                    if((pNode->m_nRequestFlags & eRequest_FromScript) != 0)
					    GetEventScriptNetworkGroup()->Add(CEventNetworkCloudFileResponse(pNode->m_nRequestID, pNode->m_bSucceeded));
				}
			}
		}
	}

	// update async cache requests
	CloudCache::Update();

	BANK_ONLY(Bank_UpdateTimedSimulatedFailure());
}

void CCloudManager::ClearCompletedRequests()
{
	// clear completed requests
	int nNodes = m_CloudRequestList.GetCount();
	for(int i = 0; i < nNodes; i++)
	{
		CloudRequestNode* pNode = m_CloudRequestList[i];

		// clear out this node if indicated
		if(pNode->m_bFlaggedToClear)
		{
			gnetDebug1("[%d] DeletingNode :: Time: %d", pNode->m_nRequestID, sysTimer::GetSystemMsTime() - pNode->m_nTimeStarted);

			// delete the element
			m_CloudRequestList.Delete(i);
			--i;
			--nNodes;

			// delete the node data
			CloudSafeDelete(pNode);
		}
	}
}

void CCloudManager::OnCacheFileLoaded(const CacheRequestID nRequestID, bool bCacheLoaded)
{
	// check my nodes
    CloudRequestNode* pNode = NULL;
        
    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
    {
        CloudRequestNode* pTemp = m_CloudRequestList[i];
		if(pTemp->m_nCacheRequestID == nRequestID)
        {
            pNode = pTemp;
			break;
        }
	}

	// found a node with this request ID
	if(pNode)
	{
#if !__NO_OUTPUT
		u32 nTime = sysTimer::GetSystemMsTime() - pNode->m_nTimestamp;
        gnetDebug1("[%d] OnCacheFileLoaded :: CacheID: %d, Loaded: %s, WaitingOn: %d, Time: %d.", pNode->m_nRequestID, nRequestID, bCacheLoaded ? "True" : "False", pNode->m_nCacheRequestType, nTime);
		pNode->m_nTimestamp = sysTimer::GetSystemMsTime();
#endif

		switch(pNode->m_nCacheRequestType)
		{
		case eCacheRequest_Header:
			{
				if(bCacheLoaded)
                {
                    // open up the cache header
                    sCacheHeader tHeader;
				    bool bSuccess = CloudCache::GetCacheHeader(nRequestID, &tHeader);
    				
				    // update the timestamp
				    if(gnetVerify(bSuccess))
				    {
					    // cache logging
					    gnetDebug1("[%d]\t OnCacheFileLoaded :: Header loaded, Timestamp: %d.", pNode->m_nRequestID, tHeader.nModifiedTime);
					    pNode->m_pRequest->UpdateTimestamp(static_cast<u64>(tHeader.nModifiedTime));
				    }
				    else
				    {
					    gnetError("[%d]\t OnCacheFileLoaded :: Failed to open header file!", pNode->m_nRequestID);
				    }
                }

				// if the request is flagged as finished here, it was cancelled
				if(!pNode->m_pRequest->HasFinished())
				{
                    gnetDebug1("[%d]\t OnCacheFileLoaded :: Starting request.", pNode->m_nRequestID);
                   
                    // start request, but check if this should be cancelled
					pNode->m_pRequest->Start();
					if(CLiveManager::HasRosCredentialsResult() && !CLiveManager::HasValidRosCredentials())
                    {
                        gnetDebug1("[%d]\t OnCacheFileLoaded :: Lost credentials. Cancelling request.", pNode->m_nRequestID);
						pNode->m_pRequest->Cancel(); 
                    }
				}
			}
			break;

		case eCacheRequest_Data:
			{
                // track whether we handled this request
                bool bHandled = false;
				bool bFinished = true;

                // if we loaded successfully
				if(bCacheLoaded)
                {
                    // cache request completed
                    sCacheHeader tHeader;
                    bool bSuccess = CloudCache::GetCacheHeader(nRequestID, &tHeader);
                    if(!gnetVerify(bSuccess))
                    {
                        gnetError("[%d]\t OnCacheFileLoaded :: Failed to open header file!", pNode->m_nRequestID);
                    }
                    else
                    {
                        // try allocate
                        u8* pData = CloudAllocate(tHeader.nSize + 1);
                        if(pData)
                        {
                            // add a null terminator in case this data is going to be sent
                            // to an rson reader which requires null terminated strings
                            pData[tHeader.nSize] = 0;

                            // open the data file
                            bSuccess = CloudCache::GetCacheData(nRequestID, pData, tHeader.nSize);
                            if(!gnetVerify(bSuccess))
                            {
                                gnetError("[%d]\t OnCacheFileLoaded :: Failed to open data file!", pNode->m_nRequestID);
                            }
                            else
                            {
                                // cache logging and check we didn't overrun
                                gnetDebug1("[%d]\t OnCacheFileLoaded :: Data loaded. Size: %d. Removing node.", pNode->m_nRequestID, tHeader.nSize);
                                gnetAssert(pData[tHeader.nSize] == 0);

                                // assume cache data is valid
                                bool bCacheIsValid = true;

                                // if this is a 304 / NET_HTTPSTATUS_NOT_MODIFIED, we want to verify that the contents of the 
                                // cache are valid against our cloud signature
                                rage::netCloudRequestHelper* pRequest = pNode->m_pRequest;
                                if(pNode->m_bHasProcessedRequest && pRequest->GetResultCode() == NET_HTTPSTATUS_NOT_MODIFIED)
                                {
                                    // if we have a have a valid file info and it contains a hash or signature
                                    rlCloudFileInfo* pFileInfo = pRequest->GetFileInfo();
                                    if(pFileInfo && (pFileInfo->m_HaveSignature || pFileInfo->m_HaveHash))
                                    {
                                        // compute SHA1 hash
                                        Sha1 sha1;
                                        u8 aDigest[Sha1::SHA1_DIGEST_LENGTH] = {0};
                                        sha1.Update(pData, tHeader.nSize);
                                        sha1.Final(aDigest);

                                        if(pFileInfo->m_HaveSignature)
                                        {
                                            // need absolute path
                                            char szAbsolutePath[MAX_FILEPATH_LENGTH + 1];

                                            // validate cloud signature
                                            bCacheIsValid = rlCloud::VerifyCloudSignature(pFileInfo->m_Signature, pRequest->GetAbsPath(szAbsolutePath, MAX_FILEPATH_LENGTH + 1), aDigest, Sha1::SHA1_DIGEST_LENGTH);
											gnetDebug1("[%d]\tOnCacheFileLoaded :: Signature Validation %s!", pNode->m_nRequestID, bCacheIsValid ? "Succeeded" : "Failed");
                                        }
                                        else if(pFileInfo->m_HaveHash)
                                        {
                                            // verify that SHA-1 hash values match
                                            for(int i = 0; i < Sha1::SHA1_DIGEST_LENGTH; i++)
                                            {
                                                if(aDigest[i] != pFileInfo->m_Sha1Hash[i])
                                                {
                                                    bCacheIsValid = false;
                                                    break;
                                                }
                                            }
											gnetDebug1("[%d]\tOnCacheFileLoaded :: Hash Validation %s!", pNode->m_nRequestID, bCacheIsValid ? "Succeeded" : "Failed");
										}
                                    }
                                }

                                // handled callback
                                bHandled = true;

								// if cache was invalid, reissue request without hitting cache to get a valid copy
								if(!bCacheIsValid)
								{
									gnetDebug1("[%d]\tOnCacheFileLoaded :: Cache Invalid. Reissue Request", pNode->m_nRequestID);

									pNode->m_nRequestFlags |= eRequest_CacheForceCloud;
									pNode->m_pRequest->Reset();
									pNode->m_pRequest->UpdateTimestamp(0);
									BeginRequest(pNode);

									// request has been re-issued, don't clean up
									bFinished = false;
								}
								else
									OnRequestFinished(pNode->m_nRequestID, bSuccess, NET_HTTPSTATUS_NOT_MODIFIED, pData, tHeader.nSize, pNode->m_pRequest->GetPath(), static_cast<u64>(tHeader.nModifiedTime), true);
                            }
                        }
                        else 
                        {
                            // failed to allocate data
                            gnetError("[%d]\t OnCacheFileLoaded :: Failed to allocate data buffer!", pNode->m_nRequestID);
                        }

                        // punt data
                        CloudSafeFree(pData);
                    }

					// clean up if finished
					if(bFinished)
					{
						// flag this node for deletion
						pNode->m_bFlaggedToClear = true;
						pNode->m_bSucceeded = bSuccess;

						// generate an event for script - needs to be a frame ahead of actually being removed
						if((pNode->m_nRequestFlags & eRequest_FromScript) != 0)
							GetEventScriptNetworkGroup()->Add(CEventNetworkCloudFileResponse(pNode->m_nRequestID, pNode->m_bSucceeded));
					}
                }
                else
                {
                    bool bStraightToCache = (pNode->m_nRequestFlags & eRequest_CacheForceCache) != 0;
#if !__FINAL
                    bStraightToCache |= PARAM_cloudForceUseCache.Get();
#endif
                    // if we wanted to load straight out of cache - we want to start the request here
                    // since we couldn't find (or failed to load) the cache request
                    if(bStraightToCache)
                    {
						// if the request is flagged as finished here, it was cancelled
						if(!pNode->m_pRequest->HasFinished())
						{
							// start request, but check if this should be cancelled
							pNode->m_pRequest->Start();
							if(CLiveManager::HasRosCredentialsResult() && !CLiveManager::HasValidRosCredentials())
								pNode->m_pRequest->Cancel(); 
						}

                        // kicked off request, not finished yet
                        bHandled = true; 
                    }
                    else
                    {
                        // flag this node for deletion
                        pNode->m_bFlaggedToClear = true;
                        pNode->m_bSucceeded = false;

                        // generate an event for script - needs to be a frame ahead of actually being removed
                        if((pNode->m_nRequestFlags & eRequest_FromScript) != 0)
                            GetEventScriptNetworkGroup()->Add(CEventNetworkCloudFileResponse(pNode->m_nRequestID, pNode->m_bSucceeded));
                    }
                }

                // need to let interested parties know that this failed
                if(!bHandled)
                {
                    gnetDebug1("[%d]\t OnCacheFileLoaded :: Not handled.", pNode->m_nRequestID);
                    OnRequestFinished(pNode->m_nRequestID, false, NET_HTTPSTATUS_NOT_MODIFIED, NULL, 0, pNode->m_pRequest->GetPath(), 0, true);
                }
			}
			break;

		default:
			gnetAssertf(0, "[%d] OnCacheFileLoaded :: Node found with invalid request type!", pNode->m_nRequestID);
			break;
		}

		// flush the current request
		pNode->m_nCacheRequestID = INVALID_CACHE_REQUEST_ID;
		pNode->m_nCacheRequestType = eCacheRequest_None;
	}
}

CCloudManager::CloudRequestNode* CCloudManager::FindNode(u64 nSemantic)
{
	// find node with this semantic
    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
    {
        CloudRequestNode* pNode = m_CloudRequestList[i];
        if(pNode->m_nSemantic == nSemantic)
			return pNode;
	}

	return NULL; 
}

CCloudManager::CloudRequestNode* CCloudManager::FindNode(CloudRequestID nRequestID)
{
	// find node with this semantic
	int nNodes = m_CloudRequestList.GetCount();
	for(int i = 0; i < nNodes; i++)
	{
		CloudRequestNode* pNode = m_CloudRequestList[i];
		if(pNode->m_nRequestID == nRequestID)
			return pNode;
	}

	return NULL; 
}

CloudRequestID CCloudManager::AddPendingRequest(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, eNamespace kNamespace, const char* szMemberPath)
{
	// don't add this twice
	int nRequests = m_CloudPendingList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(strcmp(szCloudFilePath, m_CloudPendingList[i].m_CloudFilePath) == 0)
			return m_CloudPendingList[i].m_nRequestID;
	}

	// store given parameters
	PendingCloudRequest tPendingRequest;
	safecpy(tPendingRequest.m_CloudFilePath, szCloudFilePath);
	safecpy(tPendingRequest.m_MemberFilePath, szMemberPath);
	tPendingRequest.m_nRequestFlags = nRequestFlags;
	tPendingRequest.m_uPresize = uPresize;
	tPendingRequest.m_kNamespace = kNamespace;

	// assign a request ID
	tPendingRequest.m_nRequestID = m_nRequestIDCount;
	m_nRequestIDCount++; 

	// request logging
	gnetDebug1("[%d] AddPendingRequest :: Adding Pending Request, Path: %s, Flags: %d, MemberPath: %s", tPendingRequest.m_nRequestID, szCloudFilePath, nRequestFlags, szMemberPath ? szMemberPath : "Invalid");

	// add it to our list and update our ID count
	m_CloudPendingList.PushAndGrow(tPendingRequest);

	// return ID
	return tPendingRequest.m_nRequestID;
}

bool CCloudManager::CanAddRequest(eNamespace kNamespace, unsigned nRequestFlags)
{
	// Title and Global cloud requests require valid ROS credentials, but do not depend on game setting gamer index.
	// Other cloud requests require the local gamer index to be set.
	int gamerIndex = (kNamespace == kNamespace_Title || kNamespace == kNamespace_Global) ? GAMER_INDEX_ANYONE : GAMER_INDEX_LOCAL;
	if(m_bHasCredentialsResult && ((nRequestFlags & eRequest_AlwaysQueue) == 0) && ((nRequestFlags & eRequest_CheckingMemberSpace) == 0) && !NetworkInterface::IsCloudAvailable(gamerIndex))
		return false;

	return true;
}

bool CCloudManager::CanRequestGlobalFile(unsigned UNUSED_PARAM(nRequestFlags))
{
	// some information, such as the local gamer credentials or the cloud environment, isn't available immediately. Hold
	// off on a request by adding it to a pending list while we wait for this
	return NetworkInterface::IsCloudAvailable(GAMER_INDEX_ANYONE) && CLiveManager::HasRosCredentialsResult();
}

bool CCloudManager::CanRequestTitleFile(unsigned nRequestFlags)
{
	// some information, such as the local gamer credentials or the cloud environment, isn't available immediately. Hold
	// off on a request by adding it to a pending list while we wait for this
	// for member space re-directs, we also need to wait for the player to sign in so that the gamer handle is valid
	return NetworkInterface::IsCloudAvailable(GAMER_INDEX_ANYONE) && CLiveManager::HasRosCredentialsResult() && !(((nRequestFlags & eRequest_CheckMemberSpace) != 0) && !CLiveManager::IsOnline());
}

CloudRequestID CCloudManager::GenerateRequestID()
{
	return m_nRequestIDCount++;
}

CloudRequestID CCloudManager::AddRequest(eNamespace kNamespace, rage::netCloudRequestHelper* pRequest, u64 nSemantic, unsigned nRequestFlags, CloudRequestID nRequestID, u64 nCacheSemantic)
{
    // if we have been given a credentials result, that failed, then reject the request this allows queuing ahead of a result when offline, but not after (and having failed)
    if(!CanAddRequest(kNamespace, nRequestFlags))
    {
        // pRequest is attached to pNode (which we are no longer created), clear it out here and return INVALID_CLOUD_REQUEST_ID
        gnetDebug1("AddRequest :: Rejected. Semantic: 0x%016" I64FMT "x, Path: %s, Request: %s, Flags: %d", nSemantic, pRequest->GetPath(), pRequest->GetRequestName(), nRequestFlags);
        CloudSafeDelete(pRequest);
        return INVALID_CLOUD_REQUEST_ID;
    }

    // add a new node and keep request
    CloudRequestNode* pNode = (CloudRequestNode*)CloudAllocate(sizeof(CloudRequestNode));
    if(!pNode)
    {
        gnetError("AddRequest :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s, Request: %s", nSemantic, pRequest->GetPath(), pRequest->GetRequestName());
        CloudSafeDelete(pRequest);
        return INVALID_CLOUD_REQUEST_ID;
    }

    // construct request
    new(pNode) CloudRequestNode();
	pNode->m_pRequest = pRequest;
	pNode->m_nSemantic = nSemantic;
	pNode->m_nRequestFlags = nRequestFlags;
	pNode->m_nCacheSemantic = nCacheSemantic;

#if !__FINAL
	if(PARAM_cloudCacheAll.Get())
		pNode->m_nRequestFlags |= eRequest_CacheAdd;
	else if(PARAM_cloudCacheNone.Get())
		pNode->m_nRequestFlags &= ~eRequest_CacheAdd;
#endif

	// special case for when we already assigned an ID
	if(nRequestID != INVALID_CLOUD_REQUEST_ID)
		pNode->m_nRequestID = nRequestID;
	else
		pNode->m_nRequestID = m_nRequestIDCount++;

	// request logging
	gnetDebug1("[%d] AddRequest :: Semantic: 0x%016" I64FMT "x, CacheSemantic: 0x%016" I64FMT "x, Cache: %s, CacheID: %d, Path: %s, Request: %s, Flags: %d", pNode->m_nRequestID, nSemantic, nCacheSemantic, (nRequestFlags & eRequest_CacheAdd) != 0 ? "True" : "False", pNode->m_nCacheRequestID, pRequest->GetPath(), pRequest->GetRequestName(), nRequestFlags);

	// add it to our list and update our ID count
	m_CloudRequestList.PushAndGrow(pNode);
	
	// begin request
	BeginRequest(pNode);

	// return the ID
	return pNode->m_nRequestID;
}

bool CCloudManager::BeginRequest(CloudRequestNode* pNode)
{
	if(!gnetVerify(pNode))
	{
		gnetError("[x] BeginRequest :: Invalid Node");
		return false;
	}

	if(!gnetVerify(pNode->m_pRequest))
	{
		gnetError("[%d] BeginRequest :: Invalid Request", pNode->m_nRequestID);
		return false;
	}

	// initialise variables
	pNode->m_bFlaggedToClear = false;
	pNode->m_nCacheRequestID = INVALID_CACHE_REQUEST_ID;
	pNode->m_nCacheRequestType = eCacheRequest_None;
	pNode->m_bHasProcessedRequest = false;

	// if we haven't been given a cache semantic, just use the node one
	if(pNode->m_nCacheSemantic == 0)
		pNode->m_nCacheSemantic = pNode->m_nSemantic;

#if !__FINAL
	// if we don't pull a timestamp from the header and leave it at 0, we'll always pull from cloud
	if(!PARAM_cloudForceGetCloud.Get())
#endif
	{
		// work out if we need to wait for the cache header
		if(pNode->m_pRequest->GetOpType() == eOp_GET && ((pNode->m_nRequestFlags & eRequest_CacheAdd) != 0) && ((pNode->m_nRequestFlags & eRequest_CacheForceCloud) == 0) && (((pNode->m_nRequestFlags & eRequest_CacheEncrypt) == 0) || sm_bCacheEncryptedEnabled))
		{
			CacheRequestID nRequestID = CloudCache::OpenCacheFile(pNode->m_nCacheSemantic, (pNode->m_nRequestFlags & eRequest_UsePersistentCache) != 0 ? CachePartition::Persistent : CachePartition::Default);
			if(nRequestID != INVALID_CACHE_REQUEST_ID)
			{
				bool bStraightToCache = (pNode->m_nRequestFlags & eRequest_CacheForceCache) != 0;
#if !__FINAL
				bStraightToCache |= PARAM_cloudForceUseCache.Get();
#endif
				// flag that we're waiting and capture the request ID
				pNode->m_nCacheRequestID = nRequestID;
				pNode->m_nCacheRequestType = bStraightToCache ? eCacheRequest_Data : eCacheRequest_Header;
			}
		}
	}

#if !__NO_OUTPUT
	pNode->m_nTimeStarted = sysTimer::GetSystemMsTime();
	pNode->m_nTimestamp = sysTimer::GetSystemMsTime();
#endif

	// request logging
	gnetDebug1("[%d] BeginRequest :: Cache Semantic: 0x%016" I64FMT "x, Cache: %s, CacheID: %d, Path: %s, Request: %s, Flags: %d", pNode->m_nRequestID, pNode->m_nCacheSemantic, (pNode->m_nRequestFlags & eRequest_CacheAdd) != 0 ? "True" : "False", pNode->m_nCacheRequestID, pNode->m_pRequest->GetPath(), pNode->m_pRequest->GetRequestName(), pNode->m_nRequestFlags);

	// nothing to wait for in cache... just start the request
	if(pNode->m_nCacheRequestType == eCacheRequest_None)
	{
		gnetDebug1("[%d] BeginRequest :: No cache request. Starting", pNode->m_nRequestID);

		// start request, but check if this should be cancelled
		pNode->m_pRequest->Start();
		if(CLiveManager::HasRosCredentialsResult() && !CLiveManager::HasValidRosCredentials())
			pNode->m_pRequest->Cancel(); 
	}

	// check if this request should be given a number of retries
	// do not do this for member space re-directs
	static const unsigned NUM_CRITICAL_RETRIES = 3;
	if(((pNode->m_nRequestFlags & eRequest_Critical) != 0) && ((pNode->m_nRequestFlags & eRequest_CheckingMemberSpace) == 0))
	{
		gnetDebug1("[%d] BeginRequest :: Marking as Critical", pNode->m_nRequestID);
		pNode->m_pRequest->SetNumRetries(NUM_CRITICAL_RETRIES);
	}

	// Set our security flags
	// eRequest_RequireSig maps to the RLROS_SECURITY_REQUIRE_CONTENT_SIG security flag
	// do not do this for member space re-directs
	if(((pNode->m_nRequestFlags & eRequest_RequireSig) != 0) && ((pNode->m_nRequestFlags & eRequest_CheckMemberSpaceNoSig) == 0))
	{
		gnetDebug1("[%d] BeginRequest :: Require Signature", pNode->m_nRequestID);
		pNode->m_pRequest->SetSecurityFlags(static_cast<rlRosSecurityFlags>(RLROS_SECURITY_DEFAULT | RLROS_SECURITY_REQUIRE_CONTENT_SIG));
	}

#if !__FINAL
	if ((pNode->m_nRequestFlags & eRequest_SecurityNone) != 0)
	{
		pNode->m_pRequest->SetSecurityFlags(RLROS_SECURITY_NONE);
	}
#endif

	// success
	return true;
}

CloudRequestID CCloudManager::RequestGetMemberFile(int iGamerIndex, rlCloudMemberId cloudMemberId, const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, rlCloudOnlineService nService)
{
	// generate semantic
	u64 nSemantic = GenerateHash(szCloudFilePath, g_szNamespace[kNamespace_Member], cloudMemberId.ToString());

	// check this doesn't already exist
	CloudRequestNode* pNode = FindNode(nSemantic);
	if(pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
	{
		gnetDebug1("[%d] RequestGetMemberFile :: Found existing in-flight request", pNode->m_nRequestID);
		return pNode->m_nRequestID;
	}

    // create and initialise request
    rage::netCloudRequestGetMemberFile* pRequest = (rage::netCloudRequestGetMemberFile*)CloudAllocate(sizeof(rage::netCloudRequestGetMemberFile));
    if(!pRequest)
    {
        gnetError("RequestGetMemberFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }

    // construct request
    new(pRequest) rage::netCloudRequestGetMemberFile(iGamerIndex, cloudMemberId, szCloudFilePath, 0, 
        BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
        uPresize, nService, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

	// add our request
	return AddRequest(kNamespace_Member, pRequest, nSemantic, nRequestFlags);
}

CloudRequestID CCloudManager::RequestPostMemberFile(int iGamerIndex, const char* szCloudFilePath, const void* pData, const unsigned uSizeOfData, unsigned nRequestFlags, rlCloudOnlineService nService)
{
    // create and initialise request
    rage::netCloudRequestPostMemberFile* pRequest = (rage::netCloudRequestPostMemberFile*)CloudAllocate(sizeof(rage::netCloudRequestPostMemberFile));
    if(!pRequest)
    {
        gnetError("RequestPostMemberFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", (u64)0, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }

    // construct request
    new(pRequest) rage::netCloudRequestPostMemberFile(iGamerIndex, szCloudFilePath, pData, uSizeOfData, nService, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudPostMemPolicy());

	// if we are caching this file, cache now and update the timestamp when it comes back
	u64 nSemantic = 0;
	if((nRequestFlags & eRequest_CacheAdd) != 0)
	{
        // cache the file now
		nSemantic = GenerateMemberHash(szCloudFilePath);
		CachePostRequest(nSemantic, nRequestFlags, pData, uSizeOfData);

        // flag that we need to update the cache when the request completes
        nRequestFlags |= eRequest_CacheOnPost;
	}
	
	// add our request
	return AddRequest(kNamespace_Member, pRequest, nSemantic, nRequestFlags);
}

CloudRequestID CCloudManager::RequestDeleteMemberFile(int iGamerIndex, const char* szCloudFilePath, rlCloudOnlineService nService)
{
    // create and initialise request
    rage::netCloudRequestDeleteMemberFile* pRequest = (rage::netCloudRequestDeleteMemberFile*)CloudAllocate(sizeof(rage::netCloudRequestDeleteMemberFile));
    if(!pRequest)
    {
        gnetError("RequestDeleteMemberFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", (u64)0, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }

    // construct request
    new(pRequest) rage::netCloudRequestDeleteMemberFile(iGamerIndex, szCloudFilePath, nService, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true));

	// add our request
	return AddRequest(kNamespace_Member, pRequest, 0, 0);
}

CloudRequestID CCloudManager::RequestGetCrewFile(int iGamerIndex, rlCloudMemberId targetCrewId, const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, rlCloudOnlineService nService)
{
	// generate semantic
	u64 nSemantic = GenerateHash(szCloudFilePath, g_szNamespace[kNamespace_Crew], targetCrewId.ToString());

	// check this doesn't already exist
	CloudRequestNode* pNode = FindNode(nSemantic);
	if(pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
	{
		gnetDebug1("[%d] RequestGetCrewFile :: Found existing in-flight request", pNode->m_nRequestID);
		return pNode->m_nRequestID;
	}

    // create and initialise request
    rage::netCloudRequestGetCrewFile* pRequest = (rage::netCloudRequestGetCrewFile*)CloudAllocate(sizeof(rage::netCloudRequestGetCrewFile));
    if(!pRequest)
    {
        gnetError("RequestGetCrewFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }   

    // construct request
    new(pRequest) rage::netCloudRequestGetCrewFile(iGamerIndex, targetCrewId, szCloudFilePath, 0, 
        BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
        uPresize, nService, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

	// kick off and add our request
	return AddRequest(kNamespace_Crew, pRequest, nSemantic, nRequestFlags);
}

CloudRequestID CCloudManager::RequestGetUgcFile(int iGamerIndex,
                                                const rlUgcContentType kType,
                                                const char* szContentId,
                                                const int nFileId, 
                                                const int nFileVersion,
                                                const rlScLanguage nLanguage,
                                                u32 uPresize, 
                                                unsigned nRequestFlags)
{
    char szCloudFilePath[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];

    rlVerify(rlUgcMetadata::ComposeCloudAbsPath(kType, szContentId, nFileId, nFileVersion, nLanguage, szCloudFilePath, sizeof(szCloudFilePath)));

    // generate semantic
    u64 nSemantic = GenerateHash(szCloudFilePath, g_szNamespace[kNamespace_UGC], NULL);

    // check this doesn't already exist
    CloudRequestNode* pNode = FindNode(nSemantic);
	if(pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
	{
		gnetDebug1("[%d] RequestGetUgcFile :: Found existing in-flight request", pNode->m_nRequestID);
		return pNode->m_nRequestID;
	}

    // grab language
    rlScLanguage nLanguageRequested = nLanguage;

#if !__FINAL
    // this can be used to force the language requested to always use English
    if(PARAM_cloudForceEnglishUGC.Get())
        nLanguageRequested = RLSC_LANGUAGE_ENGLISH;
#endif

    // create and initialise request
    rage::netCloudRequestGetUgcFile* pRequest = (rage::netCloudRequestGetUgcFile*)CloudAllocate(sizeof(rage::netCloudRequestGetUgcFile));
    if(!pRequest)
    {
        gnetError("RequestGetUgcFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }  

    // construct request
    new(pRequest) rage::netCloudRequestGetUgcFile(iGamerIndex, 
                                                  kType,
                                                  szContentId,
                                                  nFileId,
                                                  nFileVersion,
                                                  nLanguageRequested,
                                                  0, 
                                                  BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
                                                  uPresize, 
                                                  datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true),
                                                  GetCloudRequestMemPolicy());


	// mark requests for mission data as critical (increases retry attempts to get the required memory)
	if(nFileId == 0)
	{
		static const unsigned DEFAULT_RETRIES = 3; 
		pRequest->MarkAsCritical();
		pRequest->SetNumRetries(DEFAULT_RETRIES);
	}

    // kick off and add our request
    return AddRequest(kNamespace_UGC, pRequest, nSemantic, nRequestFlags);
}

CloudRequestID CCloudManager::RequestGetUgcCdnFile(int gamerIndex, const char* contentName, const rlUgcTokenValues& tokenValues, const rlUgcUrlTemplate* urlTemplate, u32 uPresize, unsigned nRequestFlags)
{
    // build a unique path, use the requested content Id (featured might pivot off the root content Id)
    char szCloudFilePath[MAX_URI_PATH_BUF_SIZE] = {0};
    bool success = rlUgcBuildUrlFromTemplate(urlTemplate->templateUrl, urlTemplate->tokens, tokenValues, szCloudFilePath, (unsigned)sizeof(szCloudFilePath));
    if (!success)
    {
        gnetError("RequestGetUgcCdnFile :: Failed to build url for %s", contentName);
        return INVALID_CLOUD_REQUEST_ID;
    }

    // generate semantic
    u64 nSemantic = GenerateHash(szCloudFilePath, g_szNamespace[kNamespace_UGC], nullptr);

    // check this doesn't already exist
    CloudRequestNode* pNode = FindNode(nSemantic);
    if (pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
    {
        gnetDebug1("[%d] RequestGetUgcCdnFile :: Found existing in-flight request", pNode->m_nRequestID);
        return pNode->m_nRequestID;
    }

    rage::netCloudRequestGetUgcCdnFile* pRequest = (rage::netCloudRequestGetUgcCdnFile*)CloudAllocate(sizeof(rage::netCloudRequestGetUgcCdnFile));
    if (!pRequest)
    {
        gnetError("RequestGetUgcCdnFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }

    new(pRequest) rage::netCloudRequestGetUgcCdnFile(
            gamerIndex,
            contentName,
            tokenValues,
            urlTemplate,
            0,
            NET_HTTP_OPTIONS_NONE,
            uPresize,
            datCallback(MFA1(CCloudManager::GeneralFileCallback), this, nullptr, true),
            GetCloudRequestMemPolicy());

    if (!pRequest)
    {
        gnetError("RequestGetUgcCdnFile :: Failed to allocate request. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, contentName);
        return INVALID_CLOUD_REQUEST_ID;
    }

    unsigned numRetries = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CDN_CREDIT_RETRIES", 0xB2CE61C0), 0);

    if (numRetries > 0)
    {
        pRequest->MarkAsCritical();
        pRequest->SetNumRetries(static_cast<unsigned>(numRetries));
    }

    // kick off and add our request
    return AddRequest(kNamespace_UGC, pRequest, nSemantic, nRequestFlags);
}

CloudRequestID CCloudManager::RequestGetTitleFile(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, CloudRequestID nRequestID, const char* szMemberPath)
{
	if(!szCloudFilePath && !szMemberPath)
	{
		gnetDebug1("[%d] RequestGetTitleFile :: Invalid title and member path provided", -1);
		return INVALID_CLOUD_REQUEST_ID;
	}

	if(((nRequestFlags & eRequest_CheckMemberSpace) == 0) && (!szCloudFilePath || szCloudFilePath[0] == 0))
	{
		gnetDebug1("[%d] RequestGetTitleFile :: Requesting title file without title path!", -1);
		return INVALID_CLOUD_REQUEST_ID;
	}

	// check we can make a title space request
	if(!CanRequestTitleFile(nRequestFlags) && nRequestID == INVALID_CLOUD_REQUEST_ID)
		return AddPendingRequest(szCloudFilePath != NULL ? szCloudFilePath : szMemberPath, uPresize, nRequestFlags, kNamespace_Title, szMemberPath);

	// generate semantic
	u64 nSemantic = GenerateHash(szCloudFilePath != NULL ? szCloudFilePath : szMemberPath, g_szNamespace[kNamespace_Title], NULL);
	
	// check this doesn't already exist
	CloudRequestNode* pNode = FindNode(nSemantic);
	if(pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
	{
		gnetDebug1("[%d] RequestGetTitleFile :: Found existing in-flight request", pNode->m_nRequestID);
		return pNode->m_nRequestID;
	}

	bool bCheckMemberSpace = false;
	int nLocalGamerIndex = -1;
	if((nRequestFlags & eRequest_CheckMemberSpace) != 0)
	{
		for(int i = 0; i < RL_MAX_LOCAL_GAMERS; ++i)
		{
			const rlRosCredentials& tCredentials = rlRos::GetCredentials(i);
			if(tCredentials.IsValid() && rlPresence::IsOnline(i))
			{
				if(rlRos::HasPrivilege(i, RLROS_PRIVILEGEID_DEVELOPER) || rlRos::HasPrivilege(i, RLROS_PRIVILEGEID_ALLOW_MEMBER_REDIRECT) NOTFINAL_ONLY(|| PARAM_cloudAllowCheckMemberSpace.Get()))
				{
					bCheckMemberSpace = true;
					nLocalGamerIndex = i;
					break;
				}
			}
		}
	}

	// check if we want to look for this file in member space first
	if(bCheckMemberSpace)
	{
		// create and initialise request
		rage::netCloudRequestGetMemberFile* pRequest = (rage::netCloudRequestGetMemberFile*)CloudAllocate(sizeof(rage::netCloudRequestGetMemberFile));
		if(!pRequest)
		{
			gnetError("RequestGetTitleFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szMemberPath);
			return INVALID_CLOUD_REQUEST_ID;
		}

		rlGamerHandle hGamer;
		rlPresence::GetGamerHandle(nLocalGamerIndex, &hGamer);

		// use secure folder so that we can't upload from the game
		char szFullMemberPath[RAGE_MAX_PATH];
		formatf(szFullMemberPath, "secure/%s/%s", g_rlTitleId->m_RosTitleId.GetTitleName(), szMemberPath != NULL ? szMemberPath : szCloudFilePath);

		// construct request
		rlCloudMemberId hCloudMember(hGamer);
		new(pRequest) rage::netCloudRequestGetMemberFile(nLocalGamerIndex, hCloudMember, szFullMemberPath, 0, 
            BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
            uPresize, RL_CLOUD_ONLINE_SERVICE_NATIVE, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

		// generate cache semantic
		u64 nCacheSemantic = GenerateHash(szMemberPath != NULL ? szMemberPath : szCloudFilePath, g_szNamespace[kNamespace_Member], hCloudMember.ToString());

		// add our request (tag on checking flag)
		CloudRequestID nGivenRequestID = AddRequest(kNamespace_Member, pRequest, nSemantic, nRequestFlags | eRequest_CheckingMemberSpace, nRequestID, nCacheSemantic);
		
		// we need to stash the unaltered name
		if(nGivenRequestID != INVALID_CLOUD_REQUEST_ID)
		{
			CloudRequestNode* pNode = FindNode(nGivenRequestID);
			if(gnetVerifyf(pNode, "[%d] RequestGetTitleFile :: Cannot find member override node!", nGivenRequestID))
			{
				if(szCloudFilePath)
					formatf(pNode->m_szCloudPath, szCloudFilePath);
				
				gnetDebug1("[%d] RequestGetTitleFile :: Checking member space. Node: %d, Title fallback path: %s", nGivenRequestID, pNode->m_nRequestID, szCloudFilePath);
			}
		}

		return nGivenRequestID;
	}
	// make sure we don't have the checking flag applied
	else if((nRequestFlags & eRequest_CheckingMemberSpace) != 0)
	{
		gnetError("RequestGetTitleFile :: eRequest_CheckingMemberSpace applied externally!");
		nRequestFlags &= ~eRequest_CheckingMemberSpace;
	}

	// create and initialise request
	rage::netCloudRequestGetTitleFile* pRequest = (rage::netCloudRequestGetTitleFile*)CloudAllocate(sizeof(rage::netCloudRequestGetTitleFile));
	if(!pRequest)
	{
		gnetError("RequestGetTitleFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
		return INVALID_CLOUD_REQUEST_ID;
	} 

	// construct request
	new(pRequest) rage::netCloudRequestGetTitleFile(GetFullCloudPath(szCloudFilePath, nRequestFlags), 0, 
        BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
        uPresize, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

	// kick off and add our request
	return AddRequest(kNamespace_Title, pRequest, nSemantic, nRequestFlags, nRequestID);
}

CloudRequestID CCloudManager::RequestGetGlobalFile(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, CloudRequestID nRequestID)
{
	// some information, such as the local gamer credentials or the cloud environment, isn't available immediately. Hold
	// off on a request by adding it to a pending list while we wait for this
	if(!CanRequestGlobalFile(nRequestFlags) && nRequestID == INVALID_CLOUD_REQUEST_ID)
		return AddPendingRequest(szCloudFilePath, uPresize, nRequestFlags, kNamespace_Global);

	// generate semantic
	u64 nSemantic = GenerateHash(szCloudFilePath, g_szNamespace[kNamespace_Global], NULL);

	// check this doesn't already exist
	CloudRequestNode* pNode = FindNode(nSemantic);
	if(pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
	{
		gnetDebug1("[%d] RequestGetGlobalFile :: Found existing in-flight request", pNode->m_nRequestID);
		return pNode->m_nRequestID;
	}

	// create and initialise request
	rage::netCloudRequestGetGlobalFile* pRequest = (rage::netCloudRequestGetGlobalFile*)CloudAllocate(sizeof(rage::netCloudRequestGetGlobalFile));
    if(!pRequest)
    {
        gnetError("RequestGetGlobalFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
        return INVALID_CLOUD_REQUEST_ID;
    }  

    // construct request
	new(pRequest) rage::netCloudRequestGetGlobalFile(GetFullCloudPath(szCloudFilePath, nRequestFlags), 0, 
        BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
        uPresize, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

	// kick off and add our request
	return AddRequest(kNamespace_Global, pRequest, nSemantic, nRequestFlags, nRequestID);
}

CloudRequestID CCloudManager::RequestGetWWWFile(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, CloudRequestID nRequestID)
{
	if(!CanRequestGlobalFile(nRequestFlags) && nRequestID == INVALID_CLOUD_REQUEST_ID)
		return AddPendingRequest(szCloudFilePath, uPresize, nRequestFlags, kNamespace_WWW);

	// generate semantic
	u64 nSemantic = GenerateHash(szCloudFilePath, g_szNamespace[kNamespace_WWW], NULL);

	// check this doesn't already exist
	CloudRequestNode* pNode = FindNode(nSemantic);
	if(pNode && !pNode->m_bFlaggedToClear && !pNode->m_bHasProcessedRequest)
	{
		gnetDebug1("[%d] RequestGetWWWFile :: Found existing in-flight request", pNode->m_nRequestID);
		return pNode->m_nRequestID;
	}

	// create and initialise request
	rage::netCloudRequestGetWWWFile* pRequest = (rage::netCloudRequestGetWWWFile*)CloudAllocate(sizeof(rage::netCloudRequestGetWWWFile));
	if (!pRequest)
	{
		gnetError("RequestGetWWWFile :: Failed to allocate buffer. Semantic: 0x%016" I64FMT "x, Path: %s", nSemantic, szCloudFilePath);
		return INVALID_CLOUD_REQUEST_ID;
	}

	// construct request
	new(pRequest) rage::netCloudRequestGetWWWFile(GetFullCloudPath(szCloudFilePath, nRequestFlags), 0,
		BuildHttpOptionsFromCloudRequestFlags(nRequestFlags),
		uPresize, datCallback(MFA1(CCloudManager::GeneralFileCallback), this, NULL, true), GetCloudRequestMemPolicy());

	// kick off and add our request
	return AddRequest(kNamespace_Global, pRequest, nSemantic, nRequestFlags, nRequestID);
}

netCloudRequestHelper* CCloudManager::GetRequestByID(CloudRequestID nRequestID) 
{
    // negative request ID is invalid
    if(nRequestID < 0)
        return NULL;

    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
    {
        CloudRequestNode* pNode = m_CloudRequestList[i];
		if(pNode->m_nRequestID == nRequestID)
			return pNode->m_pRequest;
	}

	return NULL;
}

bool CCloudManager::IsRequestActive(CloudRequestID nRequestID)
{
	return (GetRequestByID(nRequestID) != NULL);
}

bool CCloudManager::HasRequestCompleted(CloudRequestID nRequestID)
{
	// anything on the pending list has not completed
	int nPendingNodes = m_CloudPendingList.GetCount();
	for(int i = 0; i < nPendingNodes; i++)
	{
		if(m_CloudPendingList[i].m_nRequestID == nRequestID)
			return false;
	}

    int nNodes = m_CloudRequestList.GetCount();
	for(int i = 0; i < nNodes; i++)
	{
        CloudRequestNode* pNode = m_CloudRequestList[i];
		if(pNode->m_nRequestID == nRequestID)
			return pNode->m_bFlaggedToClear;
	}
	return nRequestID <= m_nRequestIDCount;
}

bool CCloudManager::DidRequestSucceed(CloudRequestID nRequestID)
{
    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
    {
        CloudRequestNode* pNode = m_CloudRequestList[i];
        if(pNode->m_nRequestID == nRequestID)
			return pNode->m_bSucceeded;
	}
	return false;
}

int CCloudManager::GetRequestResult(CloudRequestID nRequestID)
{
    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
    {
        CloudRequestNode* pNode = m_CloudRequestList[i];
        if(pNode->m_nRequestID == nRequestID)
			return pNode->m_pRequest->HasFinished() ? pNode->m_pRequest->GetResultCode() : -1;
	}
	return -1;
}

void CCloudManager::OnCredentialsResult(bool OUTPUT_ONLY(bSuccess))
{
    gnetDebug1("OnCredentialsResult :: Success: %s", bSuccess ? "True" : "False");

    // track that we have received a credentials response (ever)
    m_bHasCredentialsResult = true;
	CheckPendingRequests();
}

void CCloudManager::OnSignedOnline()
{
	gnetDebug1("OnSignedOnline");
	CheckPendingRequests();
}

void CCloudManager::OnSignedOffline()
{
	gnetDebug1("OnSignedOffline");
	OnSignOut();
}

void CCloudManager::OnSignOut()
{
    gnetDebug1("OnSignOut");

	// kill any pending requests
	CancelPendingRequests();

    // reset credentials response tracking
    m_bHasCredentialsResult = false;
}

void CCloudManager::CheckPendingRequests()
{
	gnetDebug1("CheckPendingRequests :: NumRequests: %d, HasCredentialsResult: %s, LiveCredResult: %s, CloudAvailable: %s, ValidCred: %s, ReadPriv: %s, Online: %s", 
				m_CloudPendingList.GetCount(),
				m_bHasCredentialsResult ? "T" : "F",
				CLiveManager::HasRosCredentialsResult(GAMER_INDEX_ANYONE) ? "T" : "F",
				NetworkInterface::IsCloudAvailable(GAMER_INDEX_ANYONE) ? "T" : "F",
				NetworkInterface::HasValidRosCredentials(GAMER_INDEX_ANYONE) ? "T" : "F",
				NetworkInterface::HasRosPrivilege(RLROS_PRIVILEGEID_CLOUD_STORAGE_READ, GAMER_INDEX_ANYONE) ? "T" : "F",
				NetworkInterface::IsLocalPlayerOnline(false) ? "T" : "F");

	// we need to have had a credentials response and we need to be online
	// to process our pending lists
	if(!m_bHasCredentialsResult)
		return; 

	// flush all pending cloud requests
	int nRequests = m_CloudPendingList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		// we need to handle the case where it fails by generating an event using the cloud ID we were given
		CloudRequestID nRequestID = INVALID_CLOUD_REQUEST_ID;
		bool bRequestAttempted = false;

		switch(m_CloudPendingList[i].m_kNamespace)
		{
		case kNamespace_Title:
			// always request when we have a credentials result
			if(CanRequestTitleFile(m_CloudPendingList[i].m_nRequestFlags) || (m_bHasCredentialsResult && !NetworkInterface::HasValidRosCredentials(GAMER_INDEX_ANYONE)))
			{
				bRequestAttempted = true;
				nRequestID = RequestGetTitleFile(m_CloudPendingList[i].m_CloudFilePath, m_CloudPendingList[i].m_uPresize, m_CloudPendingList[i].m_nRequestFlags, m_CloudPendingList[i].m_nRequestID, m_CloudPendingList[i].m_MemberFilePath);
			}
			break;
		case kNamespace_Global:
			// always request when we have a credentials result
			if(CanRequestGlobalFile(m_CloudPendingList[i].m_nRequestFlags) || (m_bHasCredentialsResult && !NetworkInterface::HasValidRosCredentials(GAMER_INDEX_ANYONE)))
			{
				bRequestAttempted = true;
				nRequestID = RequestGetGlobalFile(m_CloudPendingList[i].m_CloudFilePath, m_CloudPendingList[i].m_uPresize, m_CloudPendingList[i].m_nRequestFlags, m_CloudPendingList[i].m_nRequestID);
			}
			break;
		case kNamespace_WWW:
			if (CanRequestGlobalFile(m_CloudPendingList[i].m_nRequestFlags) || (m_bHasCredentialsResult && !NetworkInterface::HasValidRosCredentials(GAMER_INDEX_ANYONE)))
			{
				bRequestAttempted = true;
				nRequestID = RequestGetWWWFile(m_CloudPendingList[i].m_CloudFilePath, m_CloudPendingList[i].m_uPresize, m_CloudPendingList[i].m_nRequestFlags, m_CloudPendingList[i].m_nRequestID);
			}
			break;
		default:
			gnetError("[%d] CheckPendingRequests :: Unhandled Pending Request Namespace: %d!", m_CloudPendingList[i].m_nRequestID, m_CloudPendingList[i].m_kNamespace);
		}

		if(bRequestAttempted)
		{
			if(nRequestID == INVALID_CLOUD_REQUEST_ID)
				OnRequestFinished(m_CloudPendingList[i].m_nRequestID, false, 0, NULL, 0, m_CloudPendingList[i].m_CloudFilePath, 0, false);

			// remove requests we attempted
			m_CloudPendingList.Delete(i);
			--i;
			--nRequests;
		}
	}

	// immediately cancel tasks if we don't have cloud / credentials
	// it's done this way so that the tasks run through the correct flow 
	// and notify interested parties correctly
	if(!NetworkInterface::HasValidRosCredentials(GAMER_INDEX_ANYONE))
		CancelPendingRequests();
}

void CCloudManager::CancelPendingRequests()
{
	gnetDebug1("CancelPendingRequests");

    int nNodes = m_CloudRequestList.GetCount();
    for(int i = 0; i < nNodes; i++)
    {
        CloudRequestNode* pNode = m_CloudRequestList[i];

        // if not finished, cancel the request
        if(!pNode->m_pRequest->HasFinished())
		{
			gnetDebug1("[%d] CancelPendingRequests", pNode->m_nRequestID);
            pNode->m_pRequest->Cancel();
		}
    }
}

bool IsCloudAvailabilityCheckEnabled()
{
#if !__FINAL
	int value = 0;
	if(PARAM_cloudEnableAvailableCheck.Get(value))
		return value != 0;
#endif

	if(!Tunables::IsInstantiated())
		return false; 
		
	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CLOUD_AVAILABILITY_CHECK_ENABLED", 0x28a98126), false);
}

void CCloudManager::CheckCloudAvailability()
{
	// check if this functionality is enabled or not
	if(!IsCloudAvailabilityCheckEnabled())
	{
		gnetDebug1("CheckCloudAvailability :: Disabled");
		return;
	}

    // do not cancel an in light request
    if(m_AvailablityCheckRequestId != INVALID_CLOUD_REQUEST_ID)
    {
        gnetDebug1("CheckCloudAvailability :: Already checking");
        return;
    }

    // if the cloud is not available - no point in this test
    if(!NetworkInterface::IsCloudAvailable())
    {
        gnetDebug1("CheckCloudAvailability :: Cloud unavailable");
		m_AvailablityCheckRequestId = INVALID_CLOUD_REQUEST_ID;
        m_bAvailablityCheckResult = false;
        return;
    }

    gnetDebug1("CheckCloudAvailability :: Checking");

    // request our check file - no caching (defeats the purpose)
	m_AvailablityCheckRequestId = RequestGetTitleFile("check.json", 32, eRequest_CacheNone);
}

bool CCloudManager::GetAvailabilityCheckResult() const
{
	if(!IsCloudAvailabilityCheckEnabled())
		return true;

	return m_bAvailablityCheckResult;
}

void CCloudManager::CachePostRequest(u64 nSemantic, unsigned nRequestFlags, const void* pData, unsigned uSizeOfData)
{
	// we want to cache this file in addition to posting to cloud
	bool bCache = true; 

	// still want to cache
	if(bCache)
		CloudCache::AddCacheFile(nSemantic, 0, (nRequestFlags & eRequest_CacheEncrypt) != 0, (nRequestFlags & eRequest_UsePersistentCache) != 0 ? CachePartition::Persistent : CachePartition::Default, pData, uSizeOfData);
}

void CCloudManager::GeneralFileCallback(CallbackData UNUSED_PARAM(pRequest))
{
	// nothing to do here - handled in the update
}

u64 CCloudManager::GenerateMemberHash(const char* szFileName)
{
	// get the local gamer handle
	static const int kMaxName = 128;
	char szUserID[kMaxName];
	NetworkInterface::GetActiveGamerInfo()->GetGamerHandle().ToUserId(szUserID, kMaxName);

	// generate member hash for local player
	return GenerateHash(szFileName, g_szNamespace[kNamespace_Member], szUserID);
}

u64 CCloudManager::GenerateHash(const char* szFileName, const char* szNamespace, const char* szID)
{
	static const unsigned kMaxString = 256;
	char szQualifier[kMaxString];

#if !__FINAL
    // get environment - pushed this into non-final only. The environment is not actually set when we first
    // request a bunch of cloud files so we use RL_ENV_UNKNOWN but this will change when we receive our ticket
    // so the actual cached file will be different. We could fix this by delaying all requests until we have
    // an environment but that's a change that would affect final and carries risk that we don't need to take
	rlEnvironment nEnvironment = rlGetEnvironment();
	const char* szParamEnv = NULL;
	if(nEnvironment == RL_ENV_UNKNOWN)
	{
        if(PARAM_rlrosdomainenv.Get(szParamEnv) && szParamEnv != NULL)
        {
		    if(stricmp(szParamEnv, "prod") == 0)
			    nEnvironment = RL_ENV_PROD;
		    else if(stricmp(szParamEnv, "cert") == 0)
			    nEnvironment = RL_ENV_CERT;
			else if(stricmp(szParamEnv, "cert2") == 0)
				nEnvironment = RL_ENV_CERT_2;
		    else
			    nEnvironment = RL_ENV_DEV;
        }
        else // assume dev
            nEnvironment = RL_ENV_DEV;
	}

    // use the version to build the hash to avoid storing it with the cache
    snprintf(szQualifier, kMaxString, "%d.%s.%s", nEnvironment, szFileName, szNamespace);
#else
    // use the version to build the hash to avoid storing it with the cache
    snprintf(szQualifier, kMaxString, "%s.%s", szFileName, szNamespace);
#endif

    unsigned nLo = atDataHash(szQualifier, strlen(szQualifier), CLOUD_CACHE_VERSION);
    
    // further breakdown by unique identifier (or filename again if NULL)
    unsigned nHi = szID ? atDataHash(szID, strlen(szID), CLOUD_CACHE_VERSION) : atDataHash(szFileName, strlen(szFileName), CLOUD_CACHE_VERSION);

    // build full hash
	return (static_cast<u64>(nHi) << 32) + static_cast<u64>(nLo);
}

const char* CCloudManager::GetFullCloudPath(const char* szCloudFilePath, unsigned nRequestFlags)
{
	static char szFullCloudPath[RAGE_MAX_PATH];
	safecpy(szFullCloudPath, szCloudFilePath);

	if((nRequestFlags & eRequest_UseBuildVersion) != 0)
	{
		atString szPath(szCloudFilePath);
		if(szPath.IndexOf('.') != -1)
		{
			atString szLeft;
			atString szRight;
			szPath.Split(szLeft, szRight, '.');

			// grab version number
			unsigned nVersionHi = 0, nVersionLo = 0;
			sscanf(CDebug::GetVersionNumber(), "%d.%d", &nVersionHi, &nVersionLo);

			// build full path
			formatf(szFullCloudPath, "%s_%d_%d.%s", szLeft.c_str(), nVersionHi, nVersionLo, szRight.c_str());
		}
	}

	return szFullCloudPath;
}

void CCloudManager::OnRequestFinished(CloudRequestID nRequestID, bool bSuccess, int nResultCode, void* pData, unsigned uSizeOfData, const char* szFileName, u64 nModifiedTime, bool bFromCache)
{
    // catch if this is the availability check
    if(nRequestID == m_AvailablityCheckRequestId)
    {
#if !__FINAL || __FINAL_LOGGING
		int overrideValue = 0;
		const bool hasOverrideValue = PARAM_cloudOverrideAvailabilityResult.Get(overrideValue);
#endif

        // stash results and exit
        gnetDebug1("OnRequestFinished :: AvailabilityCheck - Result: %s, Override: %s", bSuccess ? "Available" : "Not Available", hasOverrideValue ? (overrideValue > 0 ? "True" : "False") : "None");
		m_AvailablityCheckRequestId = INVALID_CLOUD_REQUEST_ID;
        m_AvailablityCheckPosix = rlGetPosixTime();
        m_bAvailablityCheckResult = bSuccess;

#if !__FINAL || __FINAL_LOGGING
		if(hasOverrideValue)
			m_bAvailablityCheckResult = (overrideValue > 0);
#endif

        return;
    }

	// build event
	CloudEvent tCloudEvent;
	tCloudEvent.OnRequestFinished(nRequestID, bSuccess, nResultCode, pData, uSizeOfData, szFileName, nModifiedTime, bFromCache);

	// callback (with data)
	int nListeners = m_CloudListeners.GetCount(); 
	for(int i = 0; i < nListeners; i++)
		m_CloudListeners[i]->OnCloudEvent(&tCloudEvent);
}

#define __CLOUD_ALLOC_STACK_TRACE 0
#if __CLOUD_ALLOC_STACK_TRACE
RAGE_DEFINE_CHANNEL(cloudAllocator, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#endif

class CloudManagerMemAllocator : public sysMemAllocator
{
public:
	CloudManagerMemAllocator() 
		: sysMemAllocator()
	{

	}

	virtual void* TryAllocate(size_t size, size_t align, int heapIndex = 0)
	{
		void* pAllocated = NULL;
		{
			MEM_USE_USERDATA(MEMUSERDATA_NETWORKING);
			USE_MEMBUCKET(MEMBUCKET_NETWORK);
			pAllocated = strStreamingEngine::GetAllocator().Allocate(size, align, heapIndex);
		}

#if __CLOUD_ALLOC_STACK_TRACE
		//Bank_CaptureStackTrace(pAllocated, size);
		RAGE_DEBUGF3(cloudAllocator, "ALLOCATE, %lu, (%p)", size, pAllocated);
#endif

		return pAllocated;
	}

	virtual void* Allocate(size_t size, size_t align, int heapIndex = 0)
	{
		void* pAllocated = NULL;
		{
			MEM_USE_USERDATA(MEMUSERDATA_NETWORKING);
			USE_MEMBUCKET(MEMBUCKET_NETWORK);
			pAllocated = strStreamingEngine::GetAllocator().Allocate(size, align, heapIndex);
		}
		
#if __CLOUD_ALLOC_STACK_TRACE
		//Bank_CaptureStackTrace(pAllocated, size);
		RAGE_DEBUGF3(cloudAllocator, "ALLOCATE, %lu, (%p)", size, pAllocated);
#endif
		return pAllocated;
	}

	virtual void Free(const void *ptr)
	{
		MEM_USE_USERDATA(MEMUSERDATA_NETWORKING);
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		strStreamingEngine::GetAllocator().Free(ptr);

#if __CLOUD_ALLOC_STACK_TRACE
		RAGE_DEBUGF3(cloudAllocator, "FREE, 0, (%p)", ptr);
#endif
	}

	virtual size_t GetMemoryUsed(int )
	{
		return 0;
	}

	// PURPOSE:	Returns amount of memory remaining in the system
	// RETURNS:	Amount of memory available, in bytes, or zero if unknown
	virtual size_t GetMemoryAvailable() 
	{
		return 0;
	}
	
#if __CLOUD_ALLOC_STACK_TRACE
	void Bank_DumpStacks()
	{
		sysStack::OpenSymbolFile();

		for (int iStacks = 0; iStacks < m_stacks.GetCount(); iStacks++)
		{
			m_stacks[iStacks].DoDump();
		}
		
		sysStack::CloseSymbolFile();
	}
#endif//__CLOUD_ALLOC_STACK_TRACE

private:

#if __CLOUD_ALLOC_STACK_TRACE
	struct Stacktrace  
	{
		size_t m_size;
		static const s32 STACKENTRIES = 32;
		size_t m_trace[STACKENTRIES];

		void DoDump()
		{
			RAGE_DEBUGF3(cloudAllocator, "ALLOCATION SIZE - (%lu)", m_size);
			s32 entries = STACKENTRIES;
			for (const size_t *tp=m_trace; *tp && entries--; tp++)
			{
				char symname[256] = "unknown";
				sysStack::ParseMapFileSymbol(symname, sizeof(symname), *tp);
				RAGE_DEBUGF3(cloudAllocator, "%s", symname);
			}
		}
	};
	void Bank_CaptureStackTrace(void* UNUSED_PARAM(ptr), size_t size)
	{
		Stacktrace &traceObj = m_stacks.Grow();
		sysStack::CaptureStackTrace(traceObj.m_trace, Stacktrace::STACKENTRIES, 1);
		traceObj.m_size = size;
	}
	atArray<Stacktrace> m_stacks;
#endif //__CLOUD_ALLOC_STACK_TRACE
};

CloudManagerMemAllocator s_CloudAllocator;
netCloudRequestMemPolicy& CCloudManager::GetCloudRequestMemPolicy()
{
	if(PARAM_cloudNoAllocator.Get())
		return netCloudRequestMemPolicy::NULL_POLICY;
	
	static netCloudRequestMemPolicy s_RequestMemPolicy;
	s_RequestMemPolicy.m_pGrowBufferAllocator = &s_CloudAllocator;
	s_RequestMemPolicy.m_pGrowBufferBackup = g_rlAllocator;
	s_RequestMemPolicy.m_GrowBufferGrowIncrement = g_rscVirtualLeafSize;
	s_RequestMemPolicy.m_pHttpAllocator = NULL;

	return s_RequestMemPolicy;
}

rage::netCloudRequestMemPolicy& CCloudManager::GetCloudPostMemPolicy()
{
	if(PARAM_cloudNoAllocator.Get())
		return netCloudRequestMemPolicy::NULL_POLICY;

	static netCloudRequestMemPolicy s_PostMemPolicy;
	s_PostMemPolicy.m_pGrowBufferAllocator = nullptr;  //< Not used for Post
	s_PostMemPolicy.m_pGrowBufferBackup = nullptr;  //< Not used for Post
	if(PARAM_cloudUseStreamingForPost.Get())
		s_PostMemPolicy.m_pHttpAllocator = &s_CloudAllocator;

	return s_PostMemPolicy;
}

void CCloudManager::SetCacheEncryptedEnabled(const bool bCacheEncryptedEnabled)
{
	if(sm_bCacheEncryptedEnabled != bCacheEncryptedEnabled)
	{
		gnetDebug1("SetCacheEncryptedEnabled :: Setting to %s", bCacheEncryptedEnabled ? "True" : "False");
		sm_bCacheEncryptedEnabled = bCacheEncryptedEnabled;
	}
}

#if __BANK && __CLOUD_ALLOC_STACK_TRACE
void Bank_AllocDump()
{
	s_CloudAllocator.Bank_DumpStacks();
}
#endif

#if __BANK

void Bank_PrintCacheFiles()
{
    rlDisplay("Cache Partition Files");
    fiCacheUtils::ListFiles(fiCachePartition::GetCachePrefix(), fiCacheUtils::PrintFileFunc);
    rlDisplay("###### END ###### Cache Partition Files ###### END ######");
}

void Bank_PrintPersistentCacheFiles()
{
    rlDisplay("Persistent Cache Partition Files");
    fiCacheUtils::ListFiles(fiPersistentCachePartition::GetPersistentStorageRoot(), fiCacheUtils::PrintFileFunc);
    rlDisplay("###### END ###### Persistent Cache Partition Files ###### END ######");
}

void Bank_ExportCacheFiles()
{
    CloudCache::ExportFiles(CachePartition::Default);
}

void Bank_ExportPersistentCacheFiles()
{
    CloudCache::ExportFiles(CachePartition::Persistent);
}

void CCloudManager::InitWidgets(rage::bkBank* pBank)
{
	m_bBankApplyOverrides = false;
	m_bBankIsCloudAvailable = false;

	m_szNoCloudSometimesRate[0] = '\0';
	m_szNoHTTPSometimesRate[0] = '\0';

	m_BankFullFailureTimeSecs = 30;

    m_szDataFileName[0] = '\0';

	pBank->PushGroup("Manager");
        pBank->AddText("Data File Name", m_szDataFileName, kMAX_NAME, NullCB);
        pBank->AddButton("Encrypt Data With Cloud Key", datCallback(MFA(CCloudManager::Bank_EncryptDataWithCloudKey), this));
        pBank->AddButton("Encrypt and Compress Data With Cloud Key", datCallback(MFA(CCloudManager::Bank_EncryptAndCompressDataWithCloudKey), this));
        pBank->AddButton("Decrypt Data With Cloud Key", datCallback(MFA(CCloudManager::Bank_DecryptDataWithCloudKey), this));
		pBank->AddButton("Decrypt And Decmpress Data With Cloud Key", datCallback(MFA(CCloudManager::Bank_DecryptAndDecompressDataWithCloudKey), this));
		pBank->AddButton("Dump Open Streams", datCallback(MFA(CCloudManager::Bank_DumpOpenStreams), this));
		pBank->AddSeparator();
		pBank->AddToggle("Apply Overrides", &m_bBankApplyOverrides);
		pBank->AddToggle("Is Cloud Available (Override)", &m_bBankIsCloudAvailable);
		pBank->AddSeparator();
        pBank->AddButton("Invalidate Ros Credentials", datCallback(MFA(CCloudManager::Bank_InvalidateRosCredentials), this));
        pBank->AddButton("Renew Ros Credentials", datCallback(MFA(CCloudManager::Bank_RenewRosCredentials), this));
        pBank->AddText("No Cloud Sometimes", m_szNoCloudSometimesRate, 5, NullCB);
		pBank->AddButton("Apply No Cloud Sometimes", datCallback(MFA(CCloudManager::Bank_ApplyNoCloudSometimes), this));
		pBank->AddText("No HTTP Sometimes", m_szNoHTTPSometimesRate, 5, NullCB);
		pBank->AddButton("Apply No HTTP Sometimes", datCallback(MFA(CCloudManager::Bank_ApplyNoHTTPSometimes), this));
		pBank->AddText("Full Failure Time (Sec)", &m_BankFullFailureTimeSecs, false);
		pBank->AddButton("Apply Timed Full HTTP Failure", datCallback(MFA(CCloudManager::Bank_ApplyFullTimedHTTPFailure),this));
		pBank->AddButton("Apply Timed Full Cloud Failure", datCallback(MFA(CCloudManager::Bank_ApplyFullTimedCloudFailure),this));
		pBank->AddSeparator();

#if __CLOUD_ALLOC_STACK_TRACE
		pBank->AddButton("Dump Allocator Stacks", datCallback(CFA(Bank_AllocDump)));
#endif//__CLOUD_ALLOC_STACK_TRACE

        {
            pBank->PushGroup("Cache Partition");

            pBank->AddButton("Print", Bank_PrintCacheFiles);
            pBank->AddButton("Export to x:\\dumps\\", Bank_ExportCacheFiles);
            pBank->AddSeparator();
            pBank->AddButton("Delete All Files (Use at your own risk)", CFA(fiCachePartition::DeleteCacheFiles));

            pBank->PopGroup();
        }

#if CUSTOM_PERSISTENT_CACHE
        {
            pBank->PushGroup("Persistent Cache Partition");

            pBank->AddButton("Print", Bank_PrintPersistentCacheFiles);
            pBank->AddButton("Export to x:\\dumps\\", Bank_ExportPersistentCacheFiles);
            pBank->AddSeparator();
            pBank->AddButton("Delete Old Files", CFA(fiPersistentCachePartition::DeleteOldFiles));
            pBank->AddButton("Delete All Files (Use at your own risk)", CFA(fiPersistentCachePartition::DeleteCacheFiles));

            pBank->PopGroup();
        }
#endif //CUSTOM_PERSISTENT_CACHE

        pBank->AddSeparator();
        pBank->AddButton("Refresh downloaded file list", datCallback(MFA(CCloudManager::Bank_ListDownloadedFiles), this));
        const char** list = m_BankListedFilesComboStrings.GetCount() == 0 ? nullptr : &m_BankListedFilesComboStrings[0];
        m_BankFileListCombo = pBank->AddCombo("Downloaded Files", &m_BankFileListIdx, m_BankListedFilesComboStrings.GetCount(), list);
        pBank->AddButton("Delete Selected File", datCallback(MFA(CCloudManager::Bank_DeleteSelectedFile), this));

        pBank->PushGroup("Download Test Files");
        pBank->AddText("Duration (ms)", &m_BankDownloadDuration);
        pBank->AddText("Success", &m_BankDownloadSucceeded);
        pBank->AddText("Fail", &m_BankDownloadFailed);
        pBank->AddText("Download Bytes (body only)", &m_BankDownloadBytes);
        pBank->AddText("Download bps (body only)", &m_BankDownloadBps);
        pBank->AddButton("Download 20 * 20KB", datCallback(MFA(CCloudManager::Bank_Download20Kb), this));
        pBank->AddButton("Download 20 * 50KB", datCallback(MFA(CCloudManager::Bank_Download50Kb), this));
        pBank->AddButton("Download 20 * 100KB", datCallback(MFA(CCloudManager::Bank_Download100Kb), this));
        pBank->AddButton("Download 20 * 200KB", datCallback(MFA(CCloudManager::Bank_Download200Kb), this));
        pBank->AddButton("Download 20 * 500KB", datCallback(MFA(CCloudManager::Bank_Download500Kb), this));
        pBank->AddButton("Download 10 * 1MB", datCallback(MFA(CCloudManager::Bank_Download1Mb), this));
        pBank->AddButton("Download 10 * 2MB", datCallback(MFA(CCloudManager::Bank_Download2Mb), this));
        pBank->AddButton("Download 10 * 5MB", datCallback(MFA(CCloudManager::Bank_Download5Mb), this));
        pBank->AddButton("Download 10 * 10MB", datCallback(MFA(CCloudManager::Bank_Download10Mb), this));
        pBank->AddButton("Download 1 * 20MB", datCallback(MFA(CCloudManager::Bank_Download20Mb), this));
        pBank->AddButton("Download 10 * 20MB", datCallback(MFA(CCloudManager::Bank_Download10x20Mb), this));
        pBank->AddButton("Download 1 * 100MB", datCallback(MFA(CCloudManager::Bank_Download100Mb), this));
        pBank->AddButton("Download 5 * 100MB", datCallback(MFA(CCloudManager::Bank_Download5x100Mb), this));
        pBank->AddButton("Download 100 Files", datCallback(MFA(CCloudManager::Bank_Download100Files), this));
        pBank->AddButton("Download All Files", datCallback(MFA(CCloudManager::Bank_DownloadAllFiles), this));
        pBank->AddToggle("Download with ForceCloud flag", &m_BankForceCloudDownload);
        pBank->AddToggle("Download with ForceCache flag", &m_BankForceCache);
        pBank->AddToggle("Continuous Download", &m_BankContinuousDownload);
        pBank->PopGroup();

	pBank->PopGroup();
}

bool CCloudManager::GetOverriddenCloudAvailability(bool& bIsOverridden)
{
	if(m_bBankApplyOverrides)
		bIsOverridden = m_bBankIsCloudAvailable;
	return m_bBankApplyOverrides;
}

void CCloudManager::Bank_Encrypt(bool compress)
{
    // open the data file
    fiStream* pStream = ASSET.Open(m_szDataFileName, "");
    if(!pStream)
        return;

    // allocate data to read the file
    unsigned nDataSize = pStream->Size();
    u8* pData = CloudAllocate(nDataSize);
    if(!pData)
        return;

    // read out the contents
    pStream->Read(pData, nDataSize);

    // close the stream
    pStream->Close();

    unsigned compressedBufLen = nDataSize;
    u8* pCompressBuff = nullptr;

    unsigned totalConsumed = 0;
    unsigned totalProduced = 0;

    if (compress)
    {
        pCompressBuff = CloudAllocate(compressedBufLen);

        zlibStream zstrm;
        zstrm.BeginEncode(rlGetAllocator());
        unsigned numConsumed = 0, numProduced = 0;

        while (totalConsumed < nDataSize && !zstrm.HasError())
        {
            numConsumed = numProduced = 0;

            zstrm.Encode(&pData[totalConsumed]
                , nDataSize - totalConsumed
                , &pCompressBuff[totalProduced]
                , compressedBufLen - totalProduced
                , &numConsumed
                , &numProduced);

            if (numProduced)
            {
                gnetAssert(numProduced < compressedBufLen);
            }

            totalConsumed += numConsumed;
            totalProduced += numProduced;
        }

        while (!zstrm.IsComplete() && !zstrm.HasError())
        {
            numConsumed = numProduced = 0;

            zstrm.Encode(NULL,
                0,
                &pCompressBuff[totalProduced],
                compressedBufLen - totalProduced,
                &numConsumed,
                &numProduced);

            totalConsumed += numConsumed;
            totalProduced += numProduced;
        }

        gnetAssertf(!zstrm.HasError(), "Compress failed");
    }

    u8* pSrc = compress ? pCompressBuff : pData;
    unsigned nSrcSize = compress ? totalProduced : nDataSize;

    // assume space available in buffer for signature + minimum block needed for encryption
    //if(!gnetVerifyf((nDataSize % kAES_BLOCK_SIZE) == 0, "Bank_EncryptDataWithCloudKey :: Data block (size %d) needs to be divisible by %d", nBufferSize, kAES_BLOCK_SIZE))
    //    return false;

    // encrypt data
    gnetDebug1("Bank_EncryptDataWithCloudKey :: Encrypting %u bytes", nSrcSize);
    if(!gnetVerify(AES::GetCloudAes()->Encrypt(pSrc, nSrcSize)))
    {
        gnetError("Bank_EncryptDataWithCloudKey :: Error encrypting buffer!");
    }
    else
    {
        // build local file path
        static const unsigned kMAX_PATH = 256;
        char szDataPath[kMAX_PATH];
        snprintf(szDataPath, kMAX_PATH, "%s_AESe.dat", m_szDataFileName);

        fiStream* pOutStream(ASSET.Create(szDataPath, ""));
        if(pOutStream)
        {
            pOutStream->Write(pSrc, nSrcSize);
            pOutStream->Close();
        }
    }

    CloudSafeFree(pData);
    CloudSafeFree(pCompressBuff);
}

void CCloudManager::Bank_EncryptDataWithCloudKey()
{
    Bank_Encrypt(false);
}

void CCloudManager::Bank_EncryptAndCompressDataWithCloudKey()
{
    Bank_Encrypt(true);
}

void CCloudManager::Bank_Decrypt(bool decompress)
{
    // open the data file
    fiStream* pStream = ASSET.Open(m_szDataFileName, "");
    if(!pStream)
        return;

    // allocate data to read the file
    unsigned nDataSize = pStream->Size();
    u8* pData = CloudAllocate(nDataSize);
    if(!pData)
        return;

    // read out the contents
    pStream->Read(pData, nDataSize);

    // close the stream
    pStream->Close();

    bool success = true;

    // encrypt data
    gnetDebug1("Bank_DecryptDataWithCloudKey :: Decrypting %u bytes", nDataSize);
    if(!gnetVerify(AES::GetCloudAes()->Decrypt(pData, nDataSize)))
    {
        gnetError("Bank_DecryptDataWithCloudKey :: Error encrypting buffer!");
        success = false;
    }

    unsigned decompressBufLen = nDataSize * 8;
    u8* pDeompressBuff = nullptr;

    unsigned totalConsumed = 0, totalProduced = 0;
    unsigned numConsumed = 0, numProduced = 0;

    if (success && decompress)
    {
        pDeompressBuff = CloudAllocate(decompressBufLen);

        // create zlib stream and initialise decoding
        zlibStream zstrm;
        zstrm.BeginDecode(rlGetAllocator());

        // consume all bytes from submission into the zlib buffer.
        while (totalConsumed < nDataSize && !zstrm.IsComplete() && !zstrm.HasError())
        {
            zstrm.Decode(&pData[totalConsumed],
                nDataSize - totalConsumed,
                &pDeompressBuff[totalProduced],
                decompressBufLen - totalProduced,
                &numConsumed,
                &numProduced);

            gnetDebug1("Decoding #1: Consumed: %u / %u, Produced: %u / %u", numConsumed, totalProduced, numProduced, totalProduced);

            totalConsumed += numConsumed;
            totalProduced += numProduced;
        }

        // produce any remaining bytes into the zlib buffer.
        while (!zstrm.IsComplete() && !zstrm.HasError())
        {
            zstrm.Decode(nullptr,
                0,
                &pDeompressBuff[totalProduced],
                decompressBufLen - totalProduced,
                &numConsumed,
                &numProduced);

            gnetDebug1("Decoding #2: Consumed: %u / %u, Produced: %u / %u", numConsumed, totalProduced, numProduced, totalProduced);

            totalConsumed += numConsumed;
            totalProduced += numProduced;
        }

        gnetAssertf(!zstrm.HasError(), "Decompress failed");
        success = !zstrm.HasError();
    }

    u8* pSrc = decompress ? pDeompressBuff : pData;
    unsigned nSrcSize = decompress ? totalProduced : nDataSize;

    if (success)
    {
        // build local file path
        static const unsigned kMAX_PATH = 256;
        char szDataPath[kMAX_PATH];
        snprintf(szDataPath, kMAX_PATH, "%s_AESd.dat", m_szDataFileName);

        fiStream* pOutStream(ASSET.Create(szDataPath, ""));
        if(pOutStream)
        {
            pOutStream->Write(pSrc, nSrcSize);
            pOutStream->Close();
        }
    }

    CloudSafeFree(pData);
    CloudSafeFree(pDeompressBuff);
}

void CCloudManager::Bank_DecryptDataWithCloudKey()
{
    Bank_Decrypt(false);
}

void CCloudManager::Bank_DecryptAndDecompressDataWithCloudKey()
{
    Bank_Decrypt(true);
}

void CCloudManager::Bank_DumpOpenStreams()
{
	// dump streams with cloud prefix
	fiStream::DumpOpenFiles("Cloud");
}

void CCloudManager::Bank_InvalidateRosCredentials()
{
    rlRos::Bank_InvalidateCredentials(NetworkInterface::GetLocalGamerIndex());
}

void CCloudManager::Bank_RenewRosCredentials()
{
    rlRos::Bank_RenewCredentials(NetworkInterface::GetLocalGamerIndex());
}

void CCloudManager::Bank_ApplyNoCloudSometimes()
{
	rlCloud::SetSimulatedFailPct(m_szNoCloudSometimesRate);
}

void CCloudManager::Bank_ApplyNoHTTPSometimes()
{
	netHttpRequest::SetSimulatedFailPct(m_szNoHTTPSometimesRate);
}

void CCloudManager::Bank_ApplyFullTimedCloudFailure()
{
	if (m_BankFullCloudFailureCounter != 0)
	{
		m_BankFullCloudFailureCounter = 0;
		return;
	}

	gnetDisplay("Applying Full CLOUD FAILURE FOR %d SECONDS\n\n", m_BankFullFailureTimeSecs);
	m_BankFullCloudFailureCounter = sysTimer::GetSystemMsTime() + m_BankFullFailureTimeSecs * 1000;
}

void CCloudManager::Bank_ApplyFullTimedHTTPFailure()
{
	// click again on the button will end the timed failure
	if (m_BankFullHTTPFailureCounter != 0)
	{
		m_BankFullHTTPFailureCounter = 0;
		gnetDisplay("Early ENDING Simulated Full HTTP FAILURE");
		netHttpRequest::SetSimulatedFailPct("0");
		return;
	}

	gnetDisplay("Applying Full HTTP FAILURE FOR %d SECONDS\n\n", m_BankFullFailureTimeSecs);
	m_BankFullHTTPFailureCounter = sysTimer::GetSystemMsTime() + m_BankFullFailureTimeSecs * 1000;
}

void CCloudManager::Bank_UpdateTimedSimulatedFailure()
{
	if (m_BankFullHTTPFailureCounter == 0 && m_BankFullCloudFailureCounter == 0)
	{
		return;
	}

	u32 curTimeMs = sysTimer::GetSystemMsTime();
	if (m_BankFullHTTPFailureCounter != 0)
	{
		if (curTimeMs >= m_BankFullHTTPFailureCounter)
		{
			m_BankFullHTTPFailureCounter = 0;
			gnetDisplay("ENDING Simulated Full HTTP FAILURE");
			netHttpRequest::SetSimulatedFailPct("0");
		}
		else
		{
			//Slam the failure
			gnetDisplay("Applying Simulated Full HTTP FAILURE");
			netHttpRequest::SetSimulatedFailPct("100");
		}
	}

	if (m_BankFullCloudFailureCounter != 0)
	{
		if (curTimeMs >= m_BankFullCloudFailureCounter)
		{
			m_BankFullCloudFailureCounter = 0;
			gnetDisplay("ENDING Simulated Full CLOUD FAILURE");
			rlCloud::SetSimulatedFailPct("0");
		}
		else
		{
			gnetDisplay("Applying Simulated Full CLOUD FAILURE");
			rlCloud::SetSimulatedFailPct("100");
		}
	}
}

#define TEST_FILES_BASE_PATH "sc/feed_static_content/non_final/test_files/"

void CCloudManager::BankCloudListener::OnCloudEvent(const CloudEvent* pEvent)
{
    if (pEvent && pEvent->GetType() == CloudEvent::EVENT_REQUEST_FINISHED)
    {
        const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();
        CloudManager::GetInstance().Bank_CheckDownload(*pEventData);
    }
}

void CCloudManager::Bank_CheckDownload(const CloudEvent::sRequestFinishedEvent& request)
{
    if (m_BankPendingDownloads.size() == 0)
    {
        return;
    }

    int idx = -1;
    BankPendingDownload pd;

    for (int i = 0; i < m_BankPendingDownloads.GetCount(); ++i)
    {
        if (m_BankPendingDownloads[i].m_Id == request.nRequestID)
        {
            pd = m_BankPendingDownloads[i];
            idx = i;
            break;
        }
    }

    if (idx == -1)
    {
        return;
    }

    u64 timeNow = sysTimer::GetSystemMsTime();
    int duration = static_cast<int>(timeNow - m_BankDownloadStart);
    m_BankPendingDownloads.Delete(idx);
    m_BankDownloadSucceeded += request.bDidSucceed ? 1 : 0;
    m_BankDownloadFailed += request.bDidSucceed ? 0 : 1;
    m_BankDownloadBytes += (int)request.nDataSize;
    m_BankDownloadBps = (m_BankDownloadBytes * 8000) / (duration > 0 ? duration : 1);

    if (m_BankContinuousDownload)
    {
        Bank_DownloadGlobalFile(pd.m_Name.c_str());
    }

    if (m_BankPendingDownloads.size() == 0)
    {
        m_BankDownloadDuration = duration;

        gnetDisplay("Finished downloading files. count[%d] success[%d] fail[%d] duration[%d]ms force_cloud[%s] force_cache[%s]",
            m_BankDownloadSucceeded + m_BankDownloadFailed, m_BankDownloadSucceeded, m_BankDownloadFailed, m_BankDownloadDuration, LogBool(m_BankForceCloudDownload), LogBool(m_BankForceCache));
    }
}

void CCloudManager::Bank_StartDownload()
{
    m_BankDownloadStart = sysTimer::GetSystemMsTime();
    m_BankDownloadFailed = 0;
    m_BankDownloadSucceeded = 0;
    m_BankDownloadDuration = 0;
    m_BankDownloadBytes = 0;
    m_BankDownloadBps = 0;
}

void CCloudManager::Bank_DownloadFiles(const char* format, const int count)
{
    char buffer[256] = { 0 };

    for (int i = 0; i < count; ++i)
    {
        formatf(buffer, format, i + 1); // Files start from index 1
        Bank_DownloadGlobalFile(buffer);
    }
}

void CCloudManager::Bank_DownloadGlobalFile(const char* name)
{
    const int flags = eRequest_CacheAddAndEncrypt | (m_BankForceCloudDownload ? eRequest_CacheForceCloud : 0) | (m_BankForceCache ? eRequest_CacheForceCache : 0);
    CloudRequestID id = RequestGetGlobalFile(name, 1024 * 8, flags);

    if (id == INVALID_CLOUD_REQUEST_ID)
    {
        ++m_BankDownloadFailed;
    }
    else
    {
        const BankPendingDownload pd = { id, name };
        m_BankPendingDownloads.PushAndGrow(pd);
    }
}

void CCloudManager::Bank_Download20Kb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_20KB_%02d.bin", 20);
}

void CCloudManager::Bank_Download50Kb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_50KB_%02d.bin", 20);
}

void CCloudManager::Bank_Download100Kb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_100KB_%02d.bin", 20);
}

void CCloudManager::Bank_Download200Kb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_200KB_%02d.bin", 20);
}

void CCloudManager::Bank_Download500Kb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_500KB_%02d.bin", 20);
}

void CCloudManager::Bank_Download1Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_1MB_%02d.bin", 10);
}

void CCloudManager::Bank_Download2Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_2MB_%02d.bin", 10);
}

void CCloudManager::Bank_Download5Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_5MB_%02d.bin", 10);
}

void CCloudManager::Bank_Download10Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_10MB_%02d.bin", 10);
}

void CCloudManager::Bank_Download20Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_20MB_%02d.bin", 1);
}

void CCloudManager::Bank_Download10x20Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_20MB_%02d.bin", 10);
}

void CCloudManager::Bank_Download100Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_100MB_%02d.bin", 1);
}

void CCloudManager::Bank_Download5x100Mb()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_100MB_%02d.bin", 5);
}

void CCloudManager::Bank_Download100Files()
{
    Bank_StartDownload();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_20KB_%02d.bin", 20);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_50KB_%02d.bin", 20);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_100KB_%02d.bin", 20);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_200KB_%02d.bin", 20);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_500KB_%02d.bin", 20);
}

void CCloudManager::Bank_DownloadAllFiles()
{
    Bank_Download100Files();
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_1MB_%02d.bin", 10);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_2MB_%02d.bin", 10);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_5MB_%02d.bin", 10);
    Bank_DownloadFiles(TEST_FILES_BASE_PATH "binary_10MB_%02d.bin", 10);
}

static CachePartition s_ListingPartition = CachePartition::Default;

void CCloudManager::Bank_ListDownloadedFiles()
{
    m_BankListedFilesComboStrings.clear();
    m_BankListedFiles.clear();
    s_ListingPartition = CachePartition::Default;
    fiCacheUtils::ListFiles(fiCachePartition::GetCachePrefix(), Bank_ListFileFunc);
#if CUSTOM_PERSISTENT_CACHE
    s_ListingPartition = CachePartition::Persistent;
    fiCacheUtils::ListFiles(fiPersistentCachePartition::GetCachePrefix(), Bank_ListFileFunc);
#endif

    for (BankFileInfo& info : m_BankListedFiles)
    {
        m_BankListedFilesComboStrings.PushAndGrow(info.m_Desc.c_str());
    }

    const char** list = m_BankListedFilesComboStrings.GetCount() == 0 ? nullptr : &m_BankListedFilesComboStrings[0];
    m_BankFileListCombo->UpdateCombo("Downloaded Files", &m_BankFileListIdx, m_BankListedFilesComboStrings.GetCount(), list);
}

void CCloudManager::Bank_DeleteSelectedFile()
{
    if (m_BankFileListIdx >= 0 && m_BankFileListIdx < m_BankListedFiles.GetCount())
    {
        BankFileInfo& info = m_BankListedFiles[m_BankFileListIdx];

        if (info.m_Semantic != 0)
        {
            CloudCache::DeleteCacheFileBlocking(info.m_Semantic, info.m_Partition);
        }
    }
}

void CCloudManager::Bank_ListFileFunc(const char* path, fiHandle UNUSED_PARAM(handle), const fiFindData& data)
{
    BankFileInfo info;

    if (CloudCache::SemanticFromFileName(data.m_Name, info.m_Semantic))
    {
        char buffer[RAGE_MAX_PATH + 100] = { 0 };
        u64 sval = data.m_Size < 1000 ? data.m_Size : (data.m_Size + 999) / 1000;
        const char* sstr = data.m_Size < 1000 ? " byte" : " KiB";
        formatf(buffer, "%s [%" I64FMT "u%s]", path, sval, sstr);

        info.m_Desc = buffer;
        info.m_Size = data.m_Size;
        info.m_Partition = s_ListingPartition;

        CloudManager::GetInstance().m_BankListedFiles.PushAndGrow(info);
    }
}

#endif //__BANK
