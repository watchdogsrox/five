//===========================================================================
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.             
//===========================================================================

#ifndef CLOUDMANAGER_H
#define CLOUDMANAGER_H

//rage headers
#include "atl/atfixedstring.h"
#include "atl/dlist.h"
#include "atl/functor.h"
#include "atl/singleton.h"
#include "data/callback.h"
#include "data/growbuffer.h"
#include "file/device.h"
#include "net/status.h"
#include "rline/cloud/rlcloud.h"
#include "network/Cloud/CloudCache.h"

#if __BANK
#include "bank/combo.h"
#endif

#define CLOUD_REQUEST_IN_CACHE			(-2)
#define CLOUD_REQUEST_FAILED			(-1)

namespace rage
{
	class netCloudRequestHelper;
	class netCloudRequestGetTitleFile;
	class netCloudRequestMemPolicy;
	class bkBank;
}

enum eNamespace
{
	kNamespace_Invalid = -1,
	kNamespace_Member,
	kNamespace_Crew,
	kNamespace_UGC,
	kNamespace_Title,
	kNamespace_Global,
	kNamespace_WWW,
	kNamespace_Num,
};

enum eCacheType
{
	kCache_Invalid = -1,
	kCache_Version = 0,
	kCache_TimeModified,
	kCache_DoNotCache,
	kCache_Num,
};

enum eCacheRequestType
{
	eCacheRequest_None,
	eCacheRequest_Header,
	eCacheRequest_Data,
};

typedef int CloudRequestID;
static const int INVALID_CLOUD_REQUEST_ID = -1;

u8* CloudAllocate(unsigned nSize, unsigned nAlignment = 16);

template<class T> T* CloudAllocate()
{
	return (T*)CloudAllocate(sizeof(T));
}

template<class T> T* CloudAllocateAndConstruct()
{
	T* tBuffer = CloudAllocate<T>();

	return new(tBuffer) T();
}

// Parameter pack not available on all compilers, so we have to use macros to get what we want.
// Usage: Type* t = _CloudAllocateAndConstruct(Type, constructorArg0, constructorArg0);
#define _CloudAllocateAndConstruct(Type, ...) new(CloudAllocate<Type>()) Type(__VA_ARGS__);

void CloudFree(void* pAddress);
#define CloudSafeFree(a) if(a) { CloudFree(a); a = nullptr; }

// placement delete
template<class T> void CloudSafeDelete(T* p)
{
	if(p) 
	{
		p->~T();	
		CloudFree(p);
	}
}

class CloudEvent
{
public:

	enum CloudEventType
	{
		EVENT_REQUEST_FINISHED,
		EVENT_AVAILABILITY_CHANGED,
	};

	CloudEventType GetType() const { return m_EventType; }

	struct sRequestFinishedEvent
	{
		CloudRequestID nRequestID;	//< the cloud request ID
		bool bDidSucceed;			//< did the request succeed
		int nResultCode;			//< result code for more detail (see http.h)
		void* pData;				//< data associated with the request (if relevant)
		unsigned nDataSize;			//< size of data (if relevant)
		const char* szFileName;		//< name of file requested (if relevant)
        u64 nModifiedTime;          //< time the file was modified
		bool m_bFromCache;			//< is this data from cache
	};

	void OnRequestFinished(CloudRequestID nRequestID, bool bSuccess, int nResultCode, void* pData, unsigned uSizeOfData, const char* szFileName, u64 nModifiedTime, bool bFromCache)
	{
		m_EventType = CloudEvent::EVENT_REQUEST_FINISHED;
		m_EventData.RequestFinishedEvent.nRequestID = nRequestID;
		m_EventData.RequestFinishedEvent.bDidSucceed = bSuccess;
		m_EventData.RequestFinishedEvent.nResultCode = nResultCode;
		m_EventData.RequestFinishedEvent.pData = pData;
		m_EventData.RequestFinishedEvent.nDataSize = uSizeOfData;
		m_EventData.RequestFinishedEvent.szFileName = szFileName;
        m_EventData.RequestFinishedEvent.nModifiedTime = nModifiedTime;
		m_EventData.RequestFinishedEvent.m_bFromCache = bFromCache;
    }

	const sRequestFinishedEvent* GetRequestFinishedData() const { return &m_EventData.RequestFinishedEvent; }

	struct sAvailabilityChangedEvent
	{
		bool bIsAvailable;			//< is the cloud available
	};

	void OnAvailabilityChanged(bool bIsAvailable)
	{
		// build event
		m_EventType = CloudEvent::EVENT_AVAILABILITY_CHANGED;
		m_EventData.AvailabilityChangedEvent.bIsAvailable = bIsAvailable;
	}

	const sAvailabilityChangedEvent* GetAvailabilityChangedData() const { return &m_EventData.AvailabilityChangedEvent; }

private:

	union EventData
	{
		sRequestFinishedEvent		RequestFinishedEvent;
		sAvailabilityChangedEvent	AvailabilityChangedEvent;
	};

	CloudEventType m_EventType;
	EventData m_EventData; 
};

class CloudListener
{
protected:

	CloudListener();
	virtual ~CloudListener();

public:

	virtual void OnCloudEvent(const CloudEvent* pEvent) = 0;
};

enum eRequestFlags
{
	eRequest_CacheNone				= 0,
    eRequest_CacheAdd				= BIT0,
    eRequest_CacheEncrypt			= BIT1,
    eRequest_CacheOnPost			= BIT2,
    eRequest_CacheForceCache		= BIT3,		//< force cache data if it exists (skip cloud)
	eRequest_CacheForceCloud		= BIT4,		//< force cloud request (skip cache)
	eRequest_FromScript				= BIT5,
    eRequest_RequireSig				= BIT6,		//< turns on RLROS_SECURITY_REQUIRE_CONTENT_SIG
	eRequest_AlwaysQueue			= BIT7,
	eRequest_CheckMemberSpace		= BIT8,
	eRequest_CheckMemberSpaceNoSig	= BIT9,
	eRequest_CheckingMemberSpace	= BIT10,	//< added internally, don't specify as a parameter
	eRequest_Critical				= BIT11,
	eRequest_UseBuildVersion		= BIT12,
	eRequest_AllowCacheOnError		= BIT13,
	eRequest_CloudEncrypted			= BIT14,
	eRequest_MaintainQueryOnRedirect= BIT15,	//< keep the http query on a http-redirect
	eRequest_UsePersistentCache		= BIT16,	//< persistent cache is the folder which isn't automatically cleared by the system. The user can still delete it and such things but it's much less volatile.
	eRequest_SecurityNone			= BIT17,
	eRequest_CacheAddAndEncrypt		= eRequest_CacheAdd | eRequest_CacheEncrypt,
};

class CCloudManager : datBase, public CloudCacheListener
{
public:

	 CCloudManager();
	~CCloudManager();

	void Init();
	void Update();
	void Shutdown();

	void ClearCompletedRequests();

	void AddListener(CloudListener* pListener);
	bool RemoveListener(CloudListener* pListener);

	bool CanAddRequest(eNamespace kNamespace, unsigned nRequestFlags);
	bool CanRequestGlobalFile(unsigned nRequestFlags);
	bool CanRequestTitleFile(unsigned nRequestFlags);
	CloudRequestID GenerateRequestID();
	CloudRequestID RequestGetMemberFile(int iGamerIndex, rlCloudMemberId cloudMemberId, const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, rlCloudOnlineService nService = RL_CLOUD_ONLINE_SERVICE_NATIVE);
	CloudRequestID RequestPostMemberFile(int iGamerIndex, const char* szCloudFilePath, const void* pData, const unsigned uSizeOfData, unsigned nRequestFlags, rlCloudOnlineService nService = RL_CLOUD_ONLINE_SERVICE_NATIVE);
	CloudRequestID RequestDeleteMemberFile(int iGamerIndex, const char* szCloudFilePath, rlCloudOnlineService nService = RL_CLOUD_ONLINE_SERVICE_NATIVE);
	CloudRequestID RequestGetCrewFile(int iGamerIndex, rlCloudMemberId targetCrewId, const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, rlCloudOnlineService nService = RL_CLOUD_ONLINE_SERVICE_NATIVE);
	CloudRequestID RequestGetTitleFile(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, CloudRequestID nRequestID = INVALID_CLOUD_REQUEST_ID, const char* szMemberPath = NULL);
	CloudRequestID RequestGetWWWFile(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, CloudRequestID nRequestID = INVALID_CLOUD_REQUEST_ID);
	CloudRequestID RequestGetGlobalFile(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, CloudRequestID nRequestID = INVALID_CLOUD_REQUEST_ID);
    CloudRequestID RequestGetUgcFile(int iGamerIndex,
                                     const rlUgcContentType kType,
                                     const char* szContentId,
                                     const int nFileId, 
                                     const int nFileVersion,
                                     const rlScLanguage nLanguage,
                                     u32 uPresize, 
                                     unsigned nRequestFlags);
    CloudRequestID RequestGetUgcCdnFile(int gamerIndex, const char* contentName, const rlUgcTokenValues& tokenValues, const rlUgcUrlTemplate* urlTemplate, u32 uPresize, unsigned nRequestFlags);

	rage::netCloudRequestHelper* GetRequestByID(CloudRequestID nRequestID);
	bool IsRequestActive(CloudRequestID nRequestID); 
	bool HasRequestCompleted(CloudRequestID nRequestID); 

	// these calls will be successful for one frame after the request completes 
	// after which the request will no longer exist
	bool DidRequestSucceed(CloudRequestID nRequestID); 
	int GetRequestResult(CloudRequestID nRequestID); 

    // drop any in-flight cloud requests (just the cloud part)
    void OnCredentialsResult(bool bSuccess);
	void OnSignedOnline();
	void OnSignedOffline();
    void OnSignOut();
	void CheckPendingRequests(); 
    void CancelPendingRequests();

#if __BANK
	void InitWidgets(rage::bkBank* pBank);
	bool GetOverriddenCloudAvailability(bool& bIsOverridden);
#endif

	void StartRequests() { m_bRunRequests = true; }

    // for checking whether cloud is accessible
    void CheckCloudAvailability();
    bool IsCheckingCloudAvailability() const { return m_AvailablityCheckRequestId != INVALID_CLOUD_REQUEST_ID; }
    u64 GetAvailabilityCheckPosix() const { return m_AvailablityCheckPosix; }
	bool GetAvailabilityCheckResult() const;

	rage::netCloudRequestMemPolicy& GetCloudRequestMemPolicy();
	rage::netCloudRequestMemPolicy& GetCloudPostMemPolicy();

	static void SetCacheEncryptedEnabled(const bool bCacheEncryptedEnabled);

private:

	struct PendingCloudRequest
	{
		PendingCloudRequest()
			: m_nRequestID(INVALID_CLOUD_REQUEST_ID)
			, m_uPresize(0)
			, m_nRequestFlags(0)
			, m_kNamespace(kNamespace_Invalid)
		{
			m_CloudFilePath[0] = '\0';
		}

		CloudRequestID m_nRequestID;
		u32 m_uPresize;
		char m_CloudFilePath[RAGE_MAX_PATH];
		char m_MemberFilePath[RAGE_MAX_PATH];
		unsigned m_nRequestFlags;
		eNamespace m_kNamespace;
	};

	struct CloudRequestNode
	{
		CloudRequestNode() 
			: m_nRequestID(INVALID_CLOUD_REQUEST_ID)
			, m_nSemantic(0)
			, m_pRequest(NULL)
			, m_nRequestFlags(0)
			, m_bFlaggedToClear(false)
			, m_nCacheRequestID(INVALID_CACHE_REQUEST_ID)
			, m_nCacheRequestType(eCacheRequest_None)
			, m_bHasProcessedRequest(false)
			, m_bSucceeded(false)
#if !__NO_OUTPUT
			, m_nTimeStarted(0)
			, m_nTimestamp(0)
#endif
		{
			m_szCloudPath[0] = '\0';
		}

        ~CloudRequestNode();

		CloudRequestID m_nRequestID;
		u64 m_nSemantic;
		u64 m_nCacheSemantic;
		rage::netCloudRequestHelper* m_pRequest;
		unsigned m_nRequestFlags;
		bool m_bFlaggedToClear;
		CacheRequestID m_nCacheRequestID; 
		eCacheRequestType m_nCacheRequestType;	
		bool m_bHasProcessedRequest;
		bool m_bSucceeded;

		// this is only needed for member space re-directs
		static const unsigned MAX_CLOUD_PATH = 64;
		char m_szCloudPath[MAX_CLOUD_PATH];

#if !__NO_OUTPUT
		u32 m_nTimeStarted;
		u32 m_nTimestamp;
#endif
	};

	u64 GenerateMemberHash(const char* szFileName);
	u64 GenerateHash(const char* szFileName, const char* szNamespace, const char* szID);
	const char* GetFullCloudPath(const char* szCloudFilePath, unsigned nRequestFlags);

	CloudRequestNode* FindNode(u64 nSemantic);
	CloudRequestNode* FindNode(CloudRequestID nRequestID);

	CloudRequestID AddPendingRequest(const char* szCloudFilePath, u32 uPresize, unsigned nRequestFlags, eNamespace kNamespace, const char* szMemberPath = NULL);
	CloudRequestID AddRequest(eNamespace kNamespace, rage::netCloudRequestHelper* pRequest, u64 nSemantic, unsigned nRequestFlags, CloudRequestID nRequestID = INVALID_CLOUD_REQUEST_ID, u64 nCacheSemantic = 0);
	bool BeginRequest(CloudRequestNode* pNode);
	void CachePostRequest(u64 nSemantic, unsigned nRequestFlags, const void* pData, unsigned uSizeOfData); 
	void OnRequestFinished(CloudRequestID nRequestID, bool bSuccess, int nResultCode, void* pData, unsigned uSizeOfData, const char* szFileName, u64 nModifiedTime, bool bFromCache);
	
    virtual void OnCacheFileLoaded(const CacheRequestID nRequestID, bool bCacheLoaded);
    
    void GeneralFileCallback(CallbackData pRequest);

#if __BANK
    class BankCloudListener : public CloudListener
    {
    public:
        virtual ~BankCloudListener() {}
        virtual void OnCloudEvent(const CloudEvent* pEvent);
    };

    void Bank_Encrypt(bool compress);
    void Bank_EncryptDataWithCloudKey();
    void Bank_EncryptAndCompressDataWithCloudKey();
    void Bank_Decrypt(bool decompress);
    void Bank_DecryptDataWithCloudKey();
    void Bank_DecryptAndDecompressDataWithCloudKey();
	void Bank_DumpOpenStreams();
    void Bank_InvalidateRosCredentials();
    void Bank_RenewRosCredentials();
    void Bank_ApplyNoCloudSometimes();
	void Bank_ApplyNoHTTPSometimes(); 
	void Bank_ApplyFullTimedHTTPFailure();
	void Bank_ApplyFullTimedCloudFailure();
	void Bank_UpdateTimedSimulatedFailure();
	void Bank_ListDownloadedFiles();
	void Bank_DeleteSelectedFile();

    void Bank_CheckDownload(const CloudEvent::sRequestFinishedEvent& request);
    void Bank_StartDownload();
    void Bank_DownloadFiles(const char* format, const int count);
    void Bank_DownloadGlobalFile(const char* name);
    void Bank_Download20Kb();
    void Bank_Download50Kb();
    void Bank_Download100Kb();
    void Bank_Download200Kb();
    void Bank_Download500Kb();
    void Bank_Download1Mb();
    void Bank_Download2Mb();
    void Bank_Download5Mb();
    void Bank_Download10Mb();
    void Bank_Download20Mb();
    void Bank_Download10x20Mb();
    void Bank_Download100Mb();
    void Bank_Download5x100Mb();
    void Bank_Download100Files();
    void Bank_DownloadAllFiles();

    static void Bank_ListFileFunc(const char* path, fiHandle handle, const fiFindData& data);
#endif

	CloudRequestID m_nRequestIDCount;

    CloudRequestID m_AvailablityCheckRequestId;
    u64 m_AvailablityCheckPosix;
    bool m_bAvailablityCheckResult;

	atArray<PendingCloudRequest> m_CloudPendingList;
	atArray<CloudRequestNode*> m_CloudRequestList;
    atArray<CloudListener*> m_CloudListeners;

	bool m_bIsCloudAvailable; 
	bool m_bRunRequests;
    bool m_bHasCredentialsResult;

	static bool sm_bCacheEncryptedEnabled;
    
#if __BANK
    static const unsigned kMAX_NAME = 64;

    struct BankPendingDownload
    {
        CloudRequestID m_Id;
        atFixedString<kMAX_NAME> m_Name;
    };

    struct BankFileInfo
    {
        atString m_Desc;
        CachePartition m_Partition;
        u64 m_Semantic;
        u64 m_Size;
    };

	bool m_bBankApplyOverrides;
	bool m_bBankIsCloudAvailable;
	char m_szNoCloudSometimesRate[5];
	char m_szNoHTTPSometimesRate[5];
	int  m_BankFullFailureTimeSecs;
	u32	 m_BankFullHTTPFailureCounter;
	u32  m_BankFullCloudFailureCounter;

    char m_szDataFileName[kMAX_NAME];

    atArray<BankFileInfo> m_BankListedFiles;
    atArray<const char*> m_BankListedFilesComboStrings;
    bkCombo* m_BankFileListCombo;
    int m_BankFileListIdx;
    BankCloudListener* m_BankCloudListener;
    atArray<BankPendingDownload> m_BankPendingDownloads;
    u64 m_BankDownloadStart;
    int m_BankDownloadFailed;
    int m_BankDownloadSucceeded;
    int m_BankDownloadDuration;
    long m_BankDownloadBps;
    long m_BankDownloadBytes;
    bool m_BankForceCloudDownload;
    bool m_BankForceCache;
    bool m_BankContinuousDownload;
#endif
};

typedef atSingleton<CCloudManager> CloudManager;

#endif // CLOUDMANAGER_H
