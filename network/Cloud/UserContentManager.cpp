//===========================================================================
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.             
//===========================================================================
#include "Network/Cloud/UserContentManager.h"

// rage includes
#include "bank/bank.h"
#include "data/compress.h"
#include "file/asset.h"
#include "file/device.h"
#include "file/stream.h"
#include "game/config.h"
#include "fwlocalisation/languagePack.h"
#include "fwnet/netchannel.h"
#include "fwnet/netcloudfiledownloadhelper.h"
#include "system/memops.h"
#include "system/nelem.h"
#include "system/param.h"
#include "system/simpleallocator.h"
#include "system/timer.h"
#include "rline/rl.h"
#include "rline/rltask.h"
#include "rline/youtube/rlyoutube.h"

// game includes
#include "Network/NetworkInterface.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "SaveLoad/SaveGame_Filenames.h"
#include "SaveLoad/savegame_scriptdatafile.h"
#include "text/text.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, ugc, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_ugc

PARAM(ugcDisableDescriptionCache, "UGC - Allow description cache to be disabled");
PARAM(ugcUseOfflineImages, "UGC - Offline UGC includes image data");
PARAM(ugcUseCloudAllocator, "UGC - Use cloud allocator");

static const int OFFLINE_CONTENT_VERSION = 1;

// simple verification that script error code definitions match (won't pickup re-ordering)
static const unsigned NUM_SCRIPT_UGC_ERROR_CODES = 18;
CompileTimeAssert(NUM_SCRIPT_UGC_ERROR_CODES == RLUGC_ERROR_NUM);

bool CUserContentManager::sm_bUseCdnTemplate = CUserContentManager::DEFAULT_USE_CDN_TEMPLATE;

enum HashBits
{
    QUERY_BITS          = 4,
    QUERY_SHIFT         = 0,
    QUERY_MASK          = (1<<QUERY_BITS)-1,
    QUERY_MAX_VALUE     = (1<<QUERY_BITS)-1,

    BLOCK_BITS          = 3,
    BLOCK_SHIFT         = QUERY_SHIFT + QUERY_BITS,
    BLOCK_MASK          = (1<<BLOCK_BITS)-1,
    BLOCK_MAX_VALUE     = (1<<BLOCK_BITS)-1,

    LENGTH_BITS         = 9,
    LENGTH_SHIFT        = BLOCK_SHIFT + BLOCK_BITS,
    LENGTH_MASK			= (1<<LENGTH_BITS)-1,
    LENGTH_MAX_VALUE    = (1<<LENGTH_BITS)-1,

    OFFSET_BITS         = 16,
    OFFSET_SHIFT        = LENGTH_SHIFT + LENGTH_BITS,
    OFFSET_MASK			= (1<<OFFSET_BITS)-1,
    OFFSET_MAX_VALUE    = (1<<OFFSET_BITS)-1,
};

static bool UseQueryHash(CUserContentManager::eQueryType kQueryType, rlUgcContentType nContentType)
{
    // only support missions
    if(nContentType != RLUGC_CONTENT_TYPE_GTA5MISSION)
        return false;

    // check the query type
    switch(kQueryType)
    {
    case CUserContentManager::eQuery_CatRockstarCreated:
    case CUserContentManager::eQuery_CatRockstarVerified:
        return true;
    default:
        return false;
    }
}

static sysMemAllocator* GetUserContentCreateAllocator()
{
	if(PARAM_ugcUseCloudAllocator.Get())
		return CloudManager::GetInstance().GetCloudPostMemPolicy().m_pHttpAllocator;

	return NULL;
}

static sysMemAllocator* GetUserContentRequestAllocator()
{
	if(PARAM_ugcUseCloudAllocator.Get())
		return CloudManager::GetInstance().GetCloudRequestMemPolicy().m_pHttpAllocator;

	return NULL;
}

// strings for sorting
static const char* gs_szSortString[eSort_Max] = 
{
    "createddate",   //eSort_ByCreatedDate
};

CUserContentManager::CUserContentManager()
: m_bQueryDataOffline(false)
, m_bIsUsingOfflineContent(false)
, m_pDescriptionBlock(NULL)
, m_UgcRequestCount(0)
{
#if __BANK
	m_sbSimulateNoUgcWritePrivilege = false;
#endif

    ResetContentHash();
}

CUserContentManager::~CUserContentManager()
{
    // clear out query results
    ResetContentQuery();
    ResetCreatorQuery();
    ClearCreateResult();
    ClearModifyResult();

#if __BANK
    // drop save stream
    if(m_pSaveStream)
    {
        m_pSaveStream->Close();
        m_pSaveStream = NULL;
    }
#endif
}

bool CUserContentManager::Init()
{
	// bind query delegate
	m_QueryDelegate.Bind(this, &CUserContentManager::OnContentResult);
	m_CreatorDelegate.Bind(this, &CUserContentManager::OnCreatorResult);

	// reset the status members
	m_CreateStatus.Reset();
	m_ModifyStatus.Reset();
	m_QueryStatus.Reset();
	m_QueryCreatorsStatus.Reset();

	// clear out query results
	ResetContentQuery();
	ResetCreatorQuery();

#if __BANK
	// local save for UGC
	m_pSaveStream = NULL;
	m_QueryToSaveStatus.Reset();
	m_QueryToSaveDelegate.Bind(this, &CUserContentManager::OnContentResultToSave);
#endif

#if !__NO_OUTPUT
	m_nCreateTimestamp = 0;
	m_nQueryTimestamp = 0;
	m_nModifyTimestamp = 0;
	m_nQueryCreatorsTimestamp = 0;
	m_nLastQueryResultTimestamp = 0;
#endif

	return true;
}

void CUserContentManager::Shutdown()
{
    // cancel in-flight queries
    if(m_CreateStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_CreateStatus);
    if(m_QueryStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_QueryStatus);
    if(m_ModifyStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_ModifyStatus);
    if(m_QueryCreatorsStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_QueryCreatorsStatus);

	m_CreateStatus.Reset();
	m_ModifyStatus.Reset();
	m_QueryStatus.Reset();
	m_QueryCreatorsStatus.Reset();

    // release any in flight description requests
    ReleaseAllCachedDescriptions();

	// clear out query results
	ResetContentQuery();
	ResetCreatorQuery();
    ClearCreateResult();
    ClearModifyResult();

#if __BANK
    if(m_QueryToSaveStatus.Pending())
        rlGetTaskManager()->CancelTask(&m_QueryToSaveStatus);

    m_QueryToSaveStatus.Reset();

    // drop save stream
    if(m_pSaveStream)
    {
        m_pSaveStream->Close();
        m_pSaveStream = NULL;
    }
#endif
}

void CUserContentManager::Update()
{
    if(m_CreateOp == eOp_Pending && !m_CreateStatus.Pending())
	{
		// create completed
		gnetDebug1("UserContent :: Create Completed. Successful: %s, Time: %d", DidCreateSucceed() ? "True" : "False", sysTimer::GetSystemMsTime() - m_nCreateTimestamp);
		m_CreateOp = eOp_Finished;
	}

	if(m_QueryOp == eOp_Pending)
	{
		if(!m_QueryStatus.Pending())
		{
			// query completed
			gnetDebug1("UserContent :: Get Completed. Successful: %s, Num: %d, Total: %d, Time: %d, Rline: %" I64FMT "u.", DidQuerySucceed() ? "True" : "False", m_nContentNum, m_nContentTotal, sysTimer::GetSystemMsTime() - m_nQueryTimestamp, CLiveManager::GetRlineAllocator()->GetMemoryAvailable());
			m_QueryOp = eOp_Finished;

			// check description block
            if(m_pDescriptionBlock)
            {
                // description block - only write what we have if the request succeeded
				if(DidQuerySucceed())
					WriteDescriptionCache();

                // and delete
                delete[] m_pDescriptionBlock;
                m_pDescriptionBlock = NULL;
            }

            // only store the hash if we were successful
            if(UseQueryHash(m_kQueryType, m_kQueryContentType) && DidQuerySucceed())
                m_QueryHash[m_kQueryType] = m_QueryHashForTask;

			// verify total
#if !__NO_OUTPUT
			if(DidQuerySucceed() && !gnetVerify(m_nContentNum <= m_nContentTotal))
				gnetError("UserContent :: Total (%d) less than returned content (%d)", m_nContentTotal, m_nContentNum);
#endif
		}
        else if(m_bFlaggedToCancel)
        {
            gnetDebug1("UserContent :: Flagged to cancel - Cancelling query");

            CancelQuery();
            m_bFlaggedToCancel = false;
            m_bWasQueryForceCancelled = true; 
        }

#if !__NO_OUTPUT
		static const unsigned TIME_TO_PANIC = 3000;
		if((m_QueryOp == eOp_Pending) && (sysTimer::GetSystemMsTime() - m_nLastQueryResultTimestamp) > TIME_TO_PANIC)
		{
			m_nLastQueryResultTimestamp = sysTimer::GetSystemMsTime();
			gnetError("UserContent :: Get In Progress. No results in %dms. Rline: %" I64FMT "u.", TIME_TO_PANIC, CLiveManager::GetRlineAllocator()->GetMemoryAvailable());
		}
#endif
	}

	if(m_ModifyOp == eOp_Pending && !m_ModifyStatus.Pending())
	{
		// modify completed
		gnetDebug1("UserContent :: Modify Completed. Successful: %s, Time: %d", DidModifySucceed() ? "True" : "False", sysTimer::GetSystemMsTime() - m_nModifyTimestamp);
		m_ModifyOp = eOp_Finished;
	}

	if(m_QueryCreatorsOp == eOp_Pending && !m_QueryCreatorsStatus.Pending())
	{
		// modify completed
		gnetDebug1("UserContent :: Query Creators Completed. Successful: %s, Num: %d, Time: %d", DidQueryCreatorsSucceed() ? "True" : "False", m_nCreatorNum, sysTimer::GetSystemMsTime() - m_nQueryCreatorsTimestamp);

#if __ASSERT
		if(DidQueryCreatorsSucceed())
		{
			gnetAssertf(GetCreatorTotal() >= m_nCreatorNum, "UserContent :: Get Creators Completed. Total %d < Num %d", GetCreatorTotal(), m_nCreatorNum);
		}
#endif

		m_QueryCreatorsOp = eOp_Finished;
	}

	// update requests
	int nRequests = m_RequestList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(!m_RequestList[i]->m_MyStatus.Pending() && !m_RequestList[i]->m_bFlaggedToClear)
		{
			gnetDebug1("[%d] Request Completed. Successful: %s, Code: %d", m_RequestList[i]->m_nRequestID, m_RequestList[i]->m_MyStatus.Succeeded() ? "True" : "False", m_RequestList[i]->m_MyStatus.GetResultCode());
			
			// manage the provided status
			if(m_RequestList[i]->m_Status)
			{
				if(m_RequestList[i]->m_MyStatus.Succeeded())
					m_RequestList[i]->m_Status->SetSucceeded();
				else
					m_RequestList[i]->m_Status->SetFailed(m_RequestList[i]->m_MyStatus.GetResultCode());

				// detach status - we no longer need this and don't want to write to it again
				m_RequestList[i]->m_Status = NULL;
			}
			
			// flag request to clear
			m_RequestList[i]->m_bFlaggedToClear = true;
		}
	}

#if __BANK
	if(m_QueryToSaveOp == eOp_Pending && !m_QueryToSaveStatus.Pending())
	{
		// save completed
		gnetDebug1("UserContent :: Local Save Completed. Successful: %s", m_QueryToSaveStatus.Succeeded() ? "True" : "False");
		m_QueryToSaveOp = eOp_Finished;

		if(m_pSaveStream)
		{
			m_pSaveStream->Close();
			m_pSaveStream = NULL;
		}
	}

	// we can only push so much through the cloud at once - throttle to MAX_SIMULATANEOUS_REQUESTS
	static const int MAX_SIMULATANEOUS_REQUESTS = 5;

	// added sequentially so this is fairly simple
	int nDataRequests = m_DataRequests.GetCount();
	for(int i = 0; i < nDataRequests; i++)
	{
		// already getting the request?
		if(m_DataRequests[i].nRequestID >= 0)
			continue;

		// if we're not getting the request and have a slot, make the request
		else if(i <= MAX_SIMULATANEOUS_REQUESTS)
			m_DataRequests[i].nRequestID = RequestContentData(m_DataRequests[i].m_nContentType, 
                                                              m_DataRequests[i].m_szContentID, 
                                                              m_DataRequests[i].m_nFileID, 
                                                              m_DataRequests[i].m_nFileVersion, 
                                                              m_DataRequests[i].m_nLanguage, 
                                                              false);
	}
#endif
}

void CUserContentManager::OnGetCredentials(const int localGamerIndex)
{
    gnetDebug1("OnGetCredentials");
    if (localGamerIndex == NetworkInterface::GetLocalGamerIndex() && Tunables::IsInstantiated() && Tunables::GetInstance().HasCloudRequestFinished())
    {
        RequestUrlTemplates(localGamerIndex);
    }
}

void CUserContentManager::InitTunables()
{
    gnetDebug1("InitTunables");
    RequestUrlTemplates(NetworkInterface::GetLocalGamerIndex());
}

void CUserContentManager::ResetContentHash()
{
    // clear hash values
    for(int i = 0; i < eQuery_Num; i++)
        m_QueryHash[i] = 0;
}

bool CUserContentManager::Create(const char* szName, 
                                 const char* szDesc, 
                                 const rlUgc::SourceFileInfo* pFileInfo, 
                                 const unsigned nFiles,
                                 const char* szTagCSV, 
                                 const char* szDataJSON, 
                                 rlUgcContentType kType, 
                                 bool bPublish)
{
	// check that cloud is available
	if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "Create :: Cloud not available!"))
		return false;

#if __BANK
    if(m_sbSimulateNoUgcWritePrivilege)
        return false;
#endif

	const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
	if(!gnetVerifyf(credentials.HasPrivilege(RLROS_PRIVILEGEID_UGCWRITE), "Create :: Player does not have privilege RLROS_PRIVILEGEID_UGCWRITE"))
		return false;

	// check that we're not already creating UGC
	if(!gnetVerifyf(!m_CreateStatus.Pending(), "Create :: Operation in progress!"))
		return false;

	// logging
	gnetDebug1("Create :: Creating %s with %d files", szName, nFiles);
	
	// reset the status
	m_CreateStatus.Reset();
	m_CreateOp = eOp_Idle; 

	// reset previous content
	m_CreateResult.Clear();

	// call up to UGC
    bool bSuccess = rlUgc::CreateContent(NetworkInterface::GetLocalGamerIndex(), 
                                         kType, 
                                         szName, 
                                         szDataJSON, 
                                         szDesc,
                                         pFileInfo,
                                         nFiles,
                                         CText::GetScLanguageFromCurrentLanguageSetting(),
                                         szTagCSV, 
                                         bPublish,
                                         &m_CreateResult,
										 GetUserContentCreateAllocator(),
                                         &m_CreateStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "Create :: rlUgc::CreateContent failed!"))
		return false;

#if !__NO_OUTPUT
	m_nCreateTimestamp = sysTimer::GetSystemMsTime();
#endif

	// update op status and indicate success
	m_CreateOp = eOp_Pending;
	return true;
}

bool CUserContentManager::Create(const char* szName, 
                                 const char* szDesc, 
                                 const rlUgc::SourceFile* pFiles, 
                                 const unsigned nFiles,
                                 const char* szTagCSV, 
                                 const char* szDataJSON, 
                                 rlUgcContentType kType, 
                                 bool bPublish)
{
    // check that cloud is available
    if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "Create :: Cloud not available!"))
        return false;

#if __BANK
    if(m_sbSimulateNoUgcWritePrivilege)
        return false;
#endif

    const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
    if(!gnetVerifyf(credentials.HasPrivilege(RLROS_PRIVILEGEID_UGCWRITE), "Create :: Player does not have privilege RLROS_PRIVILEGEID_UGCWRITE"))
        return false;

    // check that we're not already creating UGC
    if(!gnetVerifyf(!m_CreateStatus.Pending(), "Create :: Operation in progress!"))
        return false;

    // logging
#if !__NO_OUTPUT
    gnetDebug1("Create :: Creating %s with %d files", szName, nFiles);
    for(int i = 0; i < nFiles; i++)
        gnetDebug1("\tCreate :: File %d is %dB", pFiles[i].m_FileId, pFiles[i].m_FileLength);
#endif

    // reset the status
    m_CreateStatus.Reset();
    m_CreateOp = eOp_Idle; 

    // reset previous content
    m_CreateResult.Clear();

    // call up to UGC
    bool bSuccess = rlUgc::CreateContent(NetworkInterface::GetLocalGamerIndex(), 
                                         kType, 
                                         szName, 
                                         szDataJSON, 
                                         szDesc,
                                         pFiles,
                                         nFiles,
                                         CText::GetScLanguageFromCurrentLanguageSetting(),
                                         szTagCSV, 
                                         bPublish,
                                         &m_CreateResult, 
										 GetUserContentCreateAllocator(),
										 &m_CreateStatus);

    // check result
    if(!gnetVerifyf(bSuccess, "Create :: rlUgc::CreateContent failed!"))
        return false;

#if !__NO_OUTPUT
    m_nCreateTimestamp = sysTimer::GetSystemMsTime();
#endif

    // update op status and indicate success
    m_CreateOp = eOp_Pending;
    return true;
}

bool CUserContentManager::Copy(const char* szContentID, rlUgcContentType kType)
{
    // check that cloud is available
    if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "Copy :: Cloud not available!"))
        return false;

#if __BANK
	if (m_sbSimulateNoUgcWritePrivilege)
	{
		return false;
	}
#endif

	const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
	if(!gnetVerifyf(credentials.HasPrivilege(RLROS_PRIVILEGEID_UGCWRITE), "Copy :: Player does not have privilege RLROS_PRIVILEGEID_UGCWRITE"))
		return false;

    // check that we're not already creating UGC
    if(!gnetVerifyf(!m_CreateStatus.Pending(), "Copy :: Operation in progress!"))
        return false;

    // logging
    gnetDebug1("Copy :: Copying using %s", szContentID);

    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("Copy :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }

    // reset the status
    m_CreateStatus.Reset();
    m_CreateOp = eOp_Idle; 

    // reset previous content
    m_CreateResult.Clear();

    // call up to UGC
	bool bSuccess = rlUgc::CopyContent(NetworkInterface::GetLocalGamerIndex(), 
									   kType, 
									   szContentID, 
									   &m_CreateResult, 
									   GetUserContentCreateAllocator(), 
									   &m_CreateStatus);

    // check result
    if(!gnetVerifyf(bSuccess, "Copy :: rlUgc::CopyContent failed!"))
        return false;

#if !__NO_OUTPUT
    m_nCreateTimestamp = sysTimer::GetSystemMsTime();
#endif

    // update op status and indicate success
    m_CreateOp = eOp_Pending;
    return true;
}

bool CUserContentManager::CheckContentQuery()
{
	// check that cloud is available
	if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "CheckContentQuery :: Cloud not available!"))
		return false;

	// check that we're not already retrieving UGC
	if(!gnetVerifyf(m_QueryOp != eOp_Pending, "CheckContentQuery :: Operation in progress!"))
		return false;

	// reset the status
	ResetContentQuery();
	
	// reset op tracking to idle
	m_QueryStatus.Reset();
	m_QueryOp = eOp_Idle;

	// all good - can call Get function
	return true;
}

void CUserContentManager::OnContentResult(const int UNUSED_PARAM(nIndex),
										  const char* UNUSED_PARAM(szContentID),
										  const rlUgcMetadata* pMetadata,
										  const rlUgcRatings* pRatings,
										  const char* szStatsJSON,
										  const unsigned UNUSED_PARAM(nPlayers),
										  const unsigned UNUSED_PARAM(nPlayerIndex),
										  const rlUgcPlayer* pPlayer)
{
    if(m_ContentData.GetCount() >= MAX_CONTENT_TO_HOLD)
    {
        gnetError("OnContentResult :: Query holding %d results. Max: %d. Servicing script inactive?", m_ContentData.GetCount(), MAX_CONTENT_TO_HOLD);
        m_bFlaggedToCancel = true;
    }
    else
    {
        // check for double hit
        int nContent = m_ContentData.GetCount();
        for(int i = 0; i < nContent; i++)
        {
            // if we already have this content
            if(m_ContentData[i]->GetMetadata().GetContentId() == pMetadata->GetContentId())
            {
                gnetError("OnContentResult :: Duplicate content. Name: %s, ID: %s", pMetadata->GetContentName(), pMetadata->GetContentId());
                return;
            }
        }

#if !__NO_OUTPUT
        size_t nRlineBefore = CLiveManager::GetRlineAllocator()->GetMemoryAvailable();
#endif
        // stash the content total 
        m_nContentNum++;

        // copy out the content data
        rlUgcContentInfo* pNewContent = rage_new rlUgcContentInfo(pMetadata, pRatings, szStatsJSON);
        m_ContentData.PushAndGrow(pNewContent);

        // copy out the player data
        rlUgcPlayer* pNewPlayer = rage_new rlUgcPlayer;
        pNewPlayer->Clear();
        if(pPlayer)
            *pNewPlayer = *pPlayer;
        m_PlayerData.PushAndGrow(pNewPlayer);

#if !__NO_OUTPUT
        m_nLastQueryResultTimestamp = sysTimer::GetSystemMsTime();
#endif

        // get description hash
		unsigned nDescriptionHash = 0;
        if(!PARAM_ugcDisableDescriptionCache.Get() && pMetadata->GetDescriptionLength() > 0 && m_pDescriptionBlock)
        {
			unsigned nDescriptionLength = Min(MAX_DESCRIPTION, pMetadata->GetDescriptionLength());

			char szDescription[MAX_DESCRIPTION];
            safecpy(szDescription, pMetadata->GetDescription(), nDescriptionLength);

            // write description to block
            char* pCurrentLoc = m_pDescriptionBlock + m_nDescriptionBlockOffset;
            safecpy(pCurrentLoc, szDescription, nDescriptionLength);

#if !__NO_OUTPUT
            if(m_kQueryType > (eQueryType)QUERY_MAX_VALUE)
                gnetError("OnContentResult :: Description Hash QueryType (%d) exceeds QUERY_MAX_VALUE (%d)", m_kQueryType, QUERY_MAX_VALUE);
            if(m_nCurrentDescriptionBlock > BLOCK_MAX_VALUE)
                gnetError("OnContentResult :: Description Hash QueryType (%d) exceeds BLOCK_MAX_VALUE (%d)", m_nCurrentDescriptionBlock, BLOCK_MAX_VALUE);
            if((pMetadata->GetDescriptionLength() - 1) > LENGTH_MAX_VALUE)
                gnetError("OnContentResult :: Description Hash QueryType (%d) exceeds LENGTH_MAX_VALUE (%d)", (pMetadata->GetDescriptionLength() - 1), LENGTH_MAX_VALUE);
            if(m_nDescriptionBlockOffset > OFFSET_MAX_VALUE)
                gnetError("OnContentResult :: Description Hash QueryType (%d) exceeds OFFSET_MAX_VALUE (%d)", m_nDescriptionBlockOffset, OFFSET_MAX_VALUE);
#endif
 
            // the description hash contains all information required to retrieve the description
            nDescriptionHash = (m_kQueryType << QUERY_SHIFT) |
                               (m_nCurrentDescriptionBlock << BLOCK_SHIFT) |
                               ((pMetadata->GetDescriptionLength() - 1) << LENGTH_SHIFT )|
                               (m_nDescriptionBlockOffset << OFFSET_SHIFT);

            // update offset
            m_nDescriptionBlockOffset += pMetadata->GetDescriptionLength();

            // if we have filled this block
            if(m_nDescriptionBlockOffset >= (DESCRIPTION_BLOCK_SIZE - MAX_DESCRIPTION))
            {
                WriteDescriptionCache();
                m_nCurrentDescriptionBlock++;
            }
        }
  
		// add to tracked descriptions (even if not set, to maintain order and indexing)
		m_DescriptionHash.PushAndGrow(nDescriptionHash);

		// logging
		gnetDebug1("OnContentResult :: Added content. Name: %s, ID: %s, DescHash: 0x%08x, Total: %d, Rline Before: %" I64FMT "u, Rline After: %" I64FMT "u", pMetadata->GetContentName(), pMetadata->GetContentId(), nDescriptionHash, m_nContentTotal, nRlineBefore, CLiveManager::GetRlineAllocator()->GetMemoryAvailable());
    }
}

void CUserContentManager::WriteDescriptionCache()
{
    if(m_nDescriptionBlockOffset == 0)
        return;

    if(m_pDescriptionBlock == NULL)
        return;

    // build cache name
    static const unsigned kMAX_PATH = 256;
    char szBlockName[kMAX_PATH];
    formatf(szBlockName, "desc_block_%d_%d", m_kQueryType, m_nCurrentDescriptionBlock);
    unsigned nSemantic = atDataHash(szBlockName, strlen(szBlockName), CLOUD_CACHE_VERSION);

    // logging
    gnetDebug1("WriteDescriptionCache :: Block: %s, Size: %d", szBlockName, m_nDescriptionBlockOffset);

    // write out block to cache and reset 
    CloudCache::AddCacheFile(nSemantic, 0, false, CachePartition::Default, m_pDescriptionBlock, m_nDescriptionBlockOffset);
    m_nDescriptionBlockOffset = 0;
}

bool CUserContentManager::QueryContent(rlUgcContentType kType, const char* szQueryName, const char* pQueryParams, int nOffset, int nMax, int* pTotal, int* pHash, rlUgc::QueryContentDelegate fnDelegate, netStatus* pStatus)
{
	// check that cloud is available
	if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "QueryContent :: Cloud not available!"))
		return false;

	// check that we're not already using this status
	if(!gnetVerifyf(!pStatus->Pending(), "QueryContent :: Operation in progress!"))
		return false;

	// logging
	gnetDebug1("QueryContent (External) :: Query: %s. Params: %s, Max: %d, Offset: %d", szQueryName, pQueryParams ? pQueryParams : "None", nMax, nOffset);

	// call up to UGC
	bool bSuccess = rlUgc::QueryContent(NetworkInterface::GetLocalGamerIndex(),
										kType,
										szQueryName,
                                        pQueryParams,
										nOffset,
										nMax,
                                        pTotal,
                                        pHash,
										fnDelegate,
										GetUserContentRequestAllocator(),
										pStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "QueryContent :: Query '%s' failed to start", szQueryName)) 
		return false;

	// update op status and indicate success
	return true;
}

bool CUserContentManager::QueryContent(rlUgcContentType kType, const char* szQueryName, const char* pQueryParams, int nOffset, int nMaxCount, eQueryType kQueryType)
{
	// check and reset for content access
	if(!CheckContentQuery())
		return false;

    // use default MAX_QUERY_COUNT if 0 was passed
    if(nMaxCount == 0)
        nMaxCount = MAX_QUERY_COUNT;

	// logging
	gnetDebug1("QueryContent :: Query: %s. Params: %s, Max: %d, Offset: %d, Rline: %" I64FMT "u.", szQueryName, pQueryParams ? pQueryParams : "None", nMaxCount, nOffset, CLiveManager::GetRlineAllocator()->GetMemoryAvailable());

	// call up to UGC
	bool bSuccess = rlUgc::QueryContent(NetworkInterface::GetLocalGamerIndex(),
										kType,
										szQueryName,
                                        pQueryParams,
										nOffset,
										nMaxCount,
                                        &m_nContentTotal,
                                        &m_QueryHashForTask,
										m_QueryDelegate,
										GetUserContentRequestAllocator(),
										&m_QueryStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "QueryContent :: Query '%s' failed to start", szQueryName)) 
		return false;

	// update op status and indicate success
	m_QueryOp = eOp_Pending;
    m_bFlaggedToCancel = false;
    m_bWasQueryForceCancelled = false;
    m_kQueryType = kQueryType;
    m_kQueryContentType = kType;

    // description block
    if(m_pDescriptionBlock)
    {
        gnetError("QueryContent :: Description Block not cleared!");
        delete[] m_pDescriptionBlock;
        m_pDescriptionBlock = NULL;
    }

    // if query type that we care about descriptions for
    if(m_kQueryType != eQuery_Invalid && kType == RLUGC_CONTENT_TYPE_GTA5MISSION)
    {
        m_pDescriptionBlock = rage_new char[DESCRIPTION_BLOCK_SIZE];
        m_nCurrentDescriptionBlock = 0;
        m_nDescriptionBlockOffset = 0;
    }
    
#if !__NO_OUTPUT
	m_nQueryTimestamp = sysTimer::GetSystemMsTime();
	m_nLastQueryResultTimestamp = m_nQueryTimestamp;
#endif

	return true;
}

void CUserContentManager::CancelQuery()
{
	// logging
	gnetDebug1("CancelQuery");

	// we shouldn't have any pending queries in this state
	if(m_QueryOp != eOp_Pending)
		return; 

	// check status and cancel
	if(m_QueryStatus.Pending())
		rlGetTaskManager()->CancelTask(&m_QueryStatus);

    // bin all results
    ResetContentQuery();

	// kill query tracking
	m_QueryOp = eOp_Idle;	
}

bool CUserContentManager::GetBookmarked(int nOffset, int nMaxCount, rlUgcContentType kType)
{
    char szQueryParams[256];
    formatf(szQueryParams, sizeof(szQueryParams), "{lang:[%s]}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());

    return QueryContent(kType, "GetMyBookmarkedContent", szQueryParams, nOffset, nMaxCount, eQuery_BookMarked);
}

bool CUserContentManager::GetMyContent(int nOffset, int nMaxCount, rlUgcContentType kType, bool* pbPublished, int nType, bool* pbOpen)
{
	char szTemp[256];
    atString szQueryParams;

    bool bNeedStart = true;
    bool bNeedComma = false;

    if(pbPublished)
	{
        if(bNeedStart)
        {
            szQueryParams += "{";
            bNeedStart = false;
        }
        bNeedComma = true; 

        formatf(szTemp, sizeof(szTemp), "published:%s", *pbPublished ? "true" : "false");
        szQueryParams += szTemp;
	}

    if(nType >= 0)
    {
        if(bNeedStart)
        {
            szQueryParams += "{";
            bNeedStart = false;
        }
        else if(bNeedComma)
            szQueryParams += ",";

		bNeedComma = true; 

        formatf(szTemp, sizeof(szTemp), "type:%d", nType);
        szQueryParams += szTemp;
    }

    // open / closed only applies to challenges
    if(kType == RLUGC_CONTENT_TYPE_GTA5CHALLENGE && pbOpen)
    {
        if(bNeedStart)
        {
            szQueryParams += "{";
            bNeedStart = false;
        }
        else if(bNeedComma)
            szQueryParams += ",";

		bNeedComma = true; 

        formatf(szTemp, sizeof(szTemp), "open:%s", *pbOpen ? "true" : "false");
        szQueryParams += szTemp;
    }

    // implies we also need to close this out
    if(bNeedComma)
        szQueryParams += "}";

    return QueryContent(kType, "GetMyContent", szQueryParams, nOffset, nMaxCount, eQuery_MyContent);
}

bool CUserContentManager::GetFriendContent(int nOffset, int nMaxCount, rlUgcContentType kType)
{
    char szQueryParams[256];
    formatf(szQueryParams, sizeof(szQueryParams), "{lang:[%s]}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());

	return QueryContent(kType, "GetFriendContent", szQueryParams, nOffset, nMaxCount, eQuery_Invalid);
}

bool CUserContentManager::GetClanContent(rlClanId nClanID, int nOffset, int nMaxCount, rlUgcContentType kType)
{
    char szQueryParams[256];
    formatf(szQueryParams, sizeof(szQueryParams), "{lang:[%s], clanid:%" I64FMT "d}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting(), nClanID);

    return QueryContent(kType, "GetClanContent", szQueryParams, nOffset, nMaxCount, eQuery_Invalid);
}

bool CUserContentManager::GetByCategory(rlUgcCategory kCategory, int nOffset, int nMaxCount, rlUgcContentType kType, eUgcSortCriteria nSortCriteria, bool bDescending)
{
    eQueryType kQueryType = static_cast<eQueryType>(eQuery_CatRockstarNone + kCategory);
    
    char szTemp[256];
    formatf(szTemp, sizeof(szTemp), "{lang:[%s], category:\"%s\"", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting(), rlUgcCategoryToString(kCategory));

    atString szQueryParams(szTemp);
    if(UseQueryHash(kQueryType, kType))
    {
        // build content ID (add two for quotes)
        formatf(szTemp, sizeof(szTemp), ", hash:%d", m_QueryHash[kQueryType]);
        szQueryParams += szTemp;
    }

    // sort parameter
    if(nSortCriteria != eSort_NotSpecified)
    {
        formatf(szTemp, sizeof(szTemp), ", sortby:\"%s%s\"", gs_szSortString[nSortCriteria], bDescending ? " desc" : "");
        szQueryParams += szTemp;
    }

    // complete with closing bracket
    szQueryParams += "}";

    // kick off query
    return QueryContent(kType, "GetContentByCategory", szQueryParams, nOffset, nMaxCount, kQueryType);
}

bool CUserContentManager::GetByContentID(char szContentIDs[][RLUGC_MAX_CONTENTID_CHARS], unsigned nContentIDs, bool bLatest, rlUgcContentType kType)
{
    if(!gnetVerifyf(nContentIDs != 0, "GetByContentId :: No content IDs!")) 
		return false;

	if(!gnetVerifyf(nContentIDs <= CUserContentManager::MAX_REQUESTED_CONTENT_IDS, "GetByContentId :: Too many Content IDs: %u, Max: %u", nContentIDs, CUserContentManager::MAX_REQUESTED_CONTENT_IDS))
		nContentIDs = CUserContentManager::MAX_REQUESTED_CONTENT_IDS;

	// track how many were added
	unsigned nContentIDsAdded = 0;

	// start JSON query
	atString szQueryParams("{contentids:[");

	// add each content ID
	static const unsigned MAX_CONTENT_ID_LENGTH = RLUGC_MAX_CONTENTID_CHARS + 5;
	char szContentID[MAX_CONTENT_ID_LENGTH];
	for(int i = 0; i < nContentIDs; i++)
	{
		// do not add empty strings
		if(szContentIDs[i][0] == '\0')
			continue;

        // add comma after the first item
        if(i > 0)
            szQueryParams += ",";

		// build content ID (add two for quotes)
		formatf(szContentID, MAX_CONTENT_ID_LENGTH, "\"%s\"", szContentIDs[i]);
		szQueryParams += szContentID;

		// increment added count
		nContentIDsAdded++;
	}

    char szLanguageParam[256];
    formatf(szLanguageParam, sizeof(szLanguageParam), "],lang:[%s]}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());

	// end content IDs and add language parameter
    szQueryParams += szLanguageParam;

	// check that we added some IDs
	if(!gnetVerifyf(nContentIDsAdded != 0, "GetByContentId :: No content IDs added!")) 
		return false;

    // kick off query
    return QueryContent(kType, bLatest ? "GetLatestVersionByContentId" : "GetContentByContentId", szQueryParams.c_str(), 0, 0, eQuery_ByContentID);
}

bool CUserContentManager::GetMostRecentlyCreated(int nOffset, int nMaxCount, rlUgcContentType kType, bool* pbOpen)
{
    char szTemp[256];
    atString szQueryParams;

    // add languages
    formatf(szTemp, sizeof(szTemp), "{lang:[%s]", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());
    szQueryParams += szTemp;

    // open / closed only applies to challenges
    if(kType == RLUGC_CONTENT_TYPE_GTA5CHALLENGE && pbOpen)
    {
        formatf(szTemp, sizeof(szTemp), ",open:%s", *pbOpen ? "true" : "false");
        szQueryParams += szTemp;
    }

    // close the query
    szQueryParams += "}";
    
    // kick off query
    return QueryContent(kType, "GetRecentlyCreatedContent", szQueryParams, nOffset, nMaxCount, eQuery_RecentlyCreated);
}

bool CUserContentManager::GetMostRecentlyPlayed(int nOffset, int nMaxCount, rlUgcContentType kType)
{
    char szQueryParams[256];
    formatf(szQueryParams, sizeof(szQueryParams), "{lang:[%s]}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());

	return QueryContent(kType, "GetMyRecentlyPlayedContent", szQueryParams, nOffset, nMaxCount, eQuery_RecentlyPlayed);
}

bool CUserContentManager::GetTopRated(int nOffset, int nMaxCount, rlUgcContentType kType)
{
    char szQueryParams[256];
    formatf(szQueryParams, sizeof(szQueryParams), "{lang:[%s]}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());

	return QueryContent(kType, "GetTopRatedContent", szQueryParams, nOffset, nMaxCount, eQuery_TopRated);
}

bool CUserContentManager::CheckContentModify(bool requireWritePermission)
{
	// check that cloud is available
	if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "CheckContentModify :: Cloud not available!"))
		return false;

	if (requireWritePermission)
	{
#if __BANK
		if (m_sbSimulateNoUgcWritePrivilege)
		{
			return false;
		}
#endif

		const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
		if(!gnetVerifyf(credentials.HasPrivilege(RLROS_PRIVILEGEID_UGCWRITE), "CheckContentModify :: Player does not have privilege RLROS_PRIVILEGEID_UGCWRITE"))
			return false;
	}

	// check that we're not already retrieving UGC
	if(!gnetVerifyf(!m_ModifyStatus.Pending(), "CheckContentModify :: Operation in progress!"))
		return false;

	// reset the status
	m_ModifyStatus.Reset();
	m_ModifyOp = eOp_Idle;

	// all good - can call Modify function
	return true;
}

bool CUserContentManager::UpdateContent(const char* szContentID, 
                                        const char* szName, 
                                        const char* szDesc, 
                                        const char* szDataJSON, 
                                        const rlUgc::SourceFileInfo* pFileInfo, 
                                        const unsigned nFiles,
                                        const char* szTags, 
                                        rlUgcContentType kType)
{
	// check and reset for content access
	if(!CheckContentModify(true))
		return false;

	// logging
	gnetDebug1("UpdateContent :: ContentID: %s", szContentID);

    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("UpdateContent :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }

	// reset previous content
	m_ModifyResult.Clear();

	// call up to UGC
    bool bSuccess = rlUgc::UpdateContent(NetworkInterface::GetLocalGamerIndex(), 
                                         kType, 
                                         szContentID, 
                                         szName,
                                         szDataJSON,
                                         szDesc,
                                         pFileInfo,
                                         nFiles,
                                         szTags, 
                                         &m_ModifyResult, 
										 GetUserContentCreateAllocator(),
										 &m_ModifyStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "UpdateContent :: rlUgc::UpdateContent failed!"))
		return false;

	// update op status and indicate success
	m_ModifyOp = eOp_Pending;

#if !__NO_OUTPUT
	m_nModifyTimestamp = sysTimer::GetSystemMsTime();
#endif

	return true;
}

bool CUserContentManager::UpdateContent(const char* szContentID, 
                                        const char* szName, 
                                        const char* szDesc, 
                                        const char* szDataJSON, 
                                        const rlUgc::SourceFile* pFiles, 
                                        const unsigned nFiles,
                                        const char* szTags, 
                                        rlUgcContentType kType)
{
    // check and reset for content access
    if(!CheckContentModify(true))
        return false;

    // logging
    gnetDebug1("UpdateContent :: ContentID: %s", szContentID);

    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("UpdateContent :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }

	// logging
#if !__NO_OUTPUT
	gnetDebug1("UpdateContent :: Updating %s with %u files", szName, nFiles);
	for (int i = 0; i < nFiles; i++)
		gnetDebug1("\tUpdateContent :: File %d is %dB", pFiles[i].m_FileId, pFiles[i].m_FileLength);
#endif

    // reset previous content
    m_ModifyResult.Clear();

    // call up to UGC
    bool bSuccess = rlUgc::UpdateContent(NetworkInterface::GetLocalGamerIndex(), 
                                         kType, 
                                         szContentID, 
                                         szName,
                                         szDataJSON,
                                         szDesc,
                                         pFiles,
                                         nFiles,
                                         szTags, 
										 &m_ModifyResult, 
										 GetUserContentCreateAllocator(),
										 &m_ModifyStatus);

    // check result
    if(!gnetVerifyf(bSuccess, "UpdateContent :: rlUgc::UpdateContent failed!"))
        return false;

    // update op status and indicate success
    m_ModifyOp = eOp_Pending;

#if !__NO_OUTPUT
    m_nModifyTimestamp = sysTimer::GetSystemMsTime();
#endif

    return true;
}

bool CUserContentManager::Publish(const char* szContentID, 
	                                      const char* szBaseContentID, 
	                                      rlUgcContentType kType, 
	                                      const char* httpRequestHeaders)
{
    // check and reset for content access
    if(!CheckContentModify(true))
        return false;

    // logging
    gnetDebug1("Publish :: ContentID: %s. Base Content ID: %s", szContentID, szBaseContentID);
    
    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("Publish :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }

    // call up to UGC
	bool bSuccess = rlUgc::Publish(NetworkInterface::GetLocalGamerIndex(), 
								   kType,
								   szContentID, 
								   szBaseContentID,
								   httpRequestHeaders,
								   GetUserContentCreateAllocator(),
								   &m_ModifyStatus);

    // check result
    if(!gnetVerifyf(bSuccess, "Publish :: rlUgc::Publish failed!"))
        return false;

    // update op status and indicate success
    m_ModifyOp = eOp_Pending;

#if !__NO_OUTPUT
    m_nModifyTimestamp = sysTimer::GetSystemMsTime();
#endif

    return true;
}

bool CUserContentManager::SetBookmarked(const char* szContentID, const bool bIsBookmarked, rlUgcContentType kType)
{
	// check and reset for content access
	if(!CheckContentModify(false))
		return false;

	// logging
	gnetDebug1("SetBookmarked :: ContentID: %s. Bookmarked: %s", szContentID, bIsBookmarked ? "True" : "False");
	
    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("SetBookmarked :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }

	// call up to UGC
	bool bSuccess = rlUgc::SetBookmarked(NetworkInterface::GetLocalGamerIndex(), 
										 kType, 
										 szContentID, 
										 bIsBookmarked, 
										 GetUserContentCreateAllocator(), 
										 &m_ModifyStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "SetBookmarked :: rlUgc::SetBookmarked failed!"))
		return false;

	// update op status and indicate success
	m_ModifyOp = eOp_Pending;

#if !__NO_OUTPUT
	m_nModifyTimestamp = sysTimer::GetSystemMsTime();
#endif

	return true;
}

bool CUserContentManager::SetDeleted(const char* szContentID, const bool bIsDeleted, rlUgcContentType kType)
{
	// check and reset for content access
	if(!CheckContentModify(true))
		return false;

	// logging
	gnetDebug1("SetDeleted :: ContentID: %s. Deleted: %s", szContentID, bIsDeleted ? "True" : "False");
	
    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("SetDeleted :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }

	// call up to UGC
	bool bSuccess = rlUgc::SetDeleted(NetworkInterface::GetLocalGamerIndex(), 
									  kType, 
									  szContentID, 
									  bIsDeleted, 
									  GetUserContentCreateAllocator(),
									  &m_ModifyStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "SetDeleted :: rlUgc::SetDeleted failed!"))
		return false;

	// update op status and indicate success
	m_ModifyOp = eOp_Pending;

#if !__NO_OUTPUT
	m_nModifyTimestamp = sysTimer::GetSystemMsTime();
#endif

	return true;
}

bool CUserContentManager::SetPlayerData(const char* szContentID, const char* szDataJSON, const float* rating, rlUgcContentType kType)
{
	// check and reset for content access
	if(!CheckContentModify(false))
		return false;

	// logging
	gnetDebug1("SetPlayerData :: ContentID: %s", szContentID);

    // validate content ID
    if(!gnetVerify(szContentID != NULL && szContentID[0] != '\0'))
    {
        gnetError("SetPlayerData :: ContentID (%s) cannot be NULL or empty", szContentID);
        return false;
    }
	
	// call up to UGC
	bool bSuccess = rlUgc::SetPlayerData(NetworkInterface::GetLocalGamerIndex(), 
										 kType, 
										 szContentID, 
										 szDataJSON, 
										 rating, 
										 GetUserContentCreateAllocator(),
										 &m_ModifyStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "SetPlayerData :: rlUgc::SetPlayerData failed!"))
		return false;

	// update op status and indicate success
	m_ModifyOp = eOp_Pending;

#if !__NO_OUTPUT
	m_nModifyTimestamp = sysTimer::GetSystemMsTime();
#endif

	return true;
}

// 20KB for now
static const unsigned kMAX_UGC_SIZE = 20 * 1024;

CloudRequestID CUserContentManager::RequestContentData(const int nContentIndex, const int nFileIndex, bool bFromScript)
{
    // logging
    gnetDebug1("RequestContentData :: Index: %d", nContentIndex);

    // check that we can query data
    if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "RequestContentData")))
        return -1;

    // make request to cloud manager
    return CloudManager::GetInstance().RequestGetUgcFile(NetworkInterface::GetLocalGamerIndex(),
                                                         m_ContentData[nContentIndex]->GetMetadata().GetContentType(),
                                                         m_ContentData[nContentIndex]->GetMetadata().GetContentId(),
                                                         m_ContentData[nContentIndex]->GetMetadata().GetFileId(nFileIndex),
                                                         m_ContentData[nContentIndex]->GetMetadata().GetFileVersion(nFileIndex),
                                                         m_ContentData[nContentIndex]->GetMetadata().GetLanguage(),
                                                         kMAX_UGC_SIZE,
                                                         eRequest_CacheAddAndEncrypt | (bFromScript ? eRequest_FromScript : 0));
}

CloudRequestID CUserContentManager::RequestContentData(const rlUgcContentType kType,
                                                       const char* szContentID,
                                                       const int nFileID, 
                                                       const int nFileVersion,
                                                       const rlScLanguage nLanguage, 
                                                       bool bFromScript)
{
    // logging
    gnetDebug1("RequestContentData");

    // make request to cloud manager
    return CloudManager::GetInstance().RequestGetUgcFile(NetworkInterface::GetLocalGamerIndex(),
                                                         kType,
                                                         szContentID,
                                                         nFileID,
                                                         nFileVersion,
                                                         nLanguage,
                                                         kMAX_UGC_SIZE,
                                                         eRequest_CacheAddAndEncrypt | (bFromScript ? eRequest_FromScript : 0));
}


CloudRequestID CUserContentManager::RequestCdnContentData(const char* contentName, unsigned nCloudRequestFlags)
{
    gnetDebug1("RequestCdnContentData :: FromParams");

    rtry
    {
        rcheck(sm_bUseCdnTemplate, catchall, gnetDebug1("RequestCdnContentData - disabled"));

        int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
        rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetDebug1("RequestCdnContentData - invalid gamer index"));

        const sysLanguage sysLanguage = rlGetLanguage();
        const rlScLanguage scLanguage = fwLanguagePack::GetScLanguageFromLanguage(sysLanguage);

        // setup template token values
        rlUgcTokenValues tokenValues;
        tokenValues.SetFileExtension("xml");
        tokenValues.SetLanguage(scLanguage);
        tokenValues.SetPlatform(rlRosTitleId::GetPlatformName());

        // find a matching template
        const rlUgcUrlTemplate* pUrlTemplate = nullptr;
        pUrlTemplate = rlUgcGetTemplateFromParams(m_UrlTemplates.GetElements(), m_UrlTemplates.GetCount(), contentName);
        rcheck(pUrlTemplate != nullptr, catchall, gnetError("Template not found"));

        // make request to cloud manager
        return CloudManager::GetInstance().RequestGetUgcCdnFile(localGamerIndex,
                                                                contentName,
                                                                tokenValues,
                                                                pUrlTemplate,
                                                                0, // use 0 to just allocate what we need
                                                                eRequest_CacheAddAndEncrypt | nCloudRequestFlags);
    }
    rcatchall
    {
    }

    return INVALID_CLOUD_REQUEST_ID;
}

CacheRequestID CUserContentManager::RequestCachedDescription(unsigned nHash)
{
    // unpack hash - the description hash contains all information required to retrieve the description
    unsigned kQueryType = (nHash >> QUERY_SHIFT) & QUERY_MASK;
    unsigned nBlock = (nHash >> BLOCK_SHIFT) & BLOCK_MASK;
    
    // build cache name
    static const unsigned kMAX_PATH = 256;
    char szBlockName[kMAX_PATH];
    formatf(szBlockName, "desc_block_%d_%d", kQueryType, nBlock);
    unsigned nSemantic = atDataHash(szBlockName, strlen(szBlockName), CLOUD_CACHE_VERSION);

    // request cache file
    CacheRequestID nCacheRequestID = CloudCache::OpenCacheFile(nSemantic, CachePartition::Default);

    // stash
    if(nCacheRequestID != INVALID_CACHE_REQUEST_ID)
    {
        gnetDebug2("RequestCachedDescription :: Requesting 0x%08x, ID: %d, Semantic: 0x%08x, Block: %s, Offset: %d, Length: %d", nHash, nCacheRequestID, nSemantic, szBlockName, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);

        // create description struct
        sDescription* pDescription = rage_new sDescription();
        pDescription->m_nHash = nHash;
        pDescription->m_szDescription = NULL;
        pDescription->m_bRequestFinished = false;
        pDescription->m_bRequestSucceeded = false;
        pDescription->m_bFlaggedForRelease = false; 
        pDescription->m_nCacheRequestID = nCacheRequestID;

        // add to list
        m_ContentDescriptionRequests.PushAndGrow(pDescription);
    }
	else
	{
		gnetWarning("RequestCachedDescription :: Failed to request 0x%08x, ID: %d, Semantic: 0x%08x, Block: %s, Offset: %d, Length: %d", nHash, nCacheRequestID, nSemantic, szBlockName, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);
	}

    return nCacheRequestID;
}

bool CUserContentManager::IsDescriptionRequestInProgress(unsigned nHash)
{
    int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_nHash == nHash)
            return !m_ContentDescriptionRequests[i]->m_bRequestFinished;
	}
	gnetDebug2("IsDescriptionRequestInProgress :: Hash 0x%08x not found, Query: %d, Block: %d, Offset: %d, Length: %d", nHash, (nHash >> QUERY_SHIFT) & QUERY_MASK, (nHash >> BLOCK_SHIFT) & BLOCK_MASK, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);
	return false;
}

bool CUserContentManager::HasDescriptionRequestFinished(unsigned nHash)
{
    int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_nHash == nHash)
            return m_ContentDescriptionRequests[i]->m_bRequestFinished;
	}
	gnetDebug2("HasDescriptionRequestFinished :: Hash 0x%08x not found, Query: %d, Block: %d, Offset: %d, Length: %d", nHash, (nHash >> QUERY_SHIFT) & QUERY_MASK, (nHash >> BLOCK_SHIFT) & BLOCK_MASK, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);
	return true;
}

bool CUserContentManager::DidDescriptionRequestSucceed(unsigned nHash)
{
    int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_nHash == nHash)
            return m_ContentDescriptionRequests[i]->m_bRequestSucceeded;
    }
	gnetDebug2("DidDescriptionRequestSucceed :: Hash 0x%08x not found, Query: %d, Block: %d, Offset: %d, Length: %d", nHash, (nHash >> QUERY_SHIFT) & QUERY_MASK, (nHash >> BLOCK_SHIFT) & BLOCK_MASK, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);
	return false;
}

const char* CUserContentManager::GetCachedDescription(unsigned nHash)
{
    int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_nHash == nHash)
        {
            if(!m_ContentDescriptionRequests[i]->m_bRequestFinished)
                return NULL;

            return m_ContentDescriptionRequests[i]->m_szDescription;
        }
	}
	gnetDebug2("GetCachedDescription :: Hash 0x%08x not found, Query: %d, Block: %d, Offset: %d, Length: %d", nHash, (nHash >> QUERY_SHIFT) & QUERY_MASK, (nHash >> BLOCK_SHIFT) & BLOCK_MASK, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);
	return NULL;
}

bool CUserContentManager::ReleaseCachedDescription(unsigned nHash)
{
	// don't bother with a 0 value hash
	if(nHash == 0)
		return false;

	int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_nHash == nHash)
        {
            if(m_ContentDescriptionRequests[i]->m_bRequestFinished)
            {
				gnetDebug2("ReleaseCachedDescription :: Releasing 0x%08x", nHash);

				sDescription* pDescription = m_ContentDescriptionRequests[i];
                m_ContentDescriptionRequests.Delete(i);

                if(pDescription->m_szDescription)
                    delete[] pDescription->m_szDescription;
                delete pDescription;
            }
            else
            {
                gnetDebug2("ReleaseCachedDescription :: 0x%08x Pending. Flagged for Release.", nHash);
                m_ContentDescriptionRequests[i]->m_bFlaggedForRelease = true; 
            }
            
            return true;
        }
    }
	gnetDebug2("ReleaseCachedDescription :: Hash 0x%08x not found, Query: %d, Block: %d, Offset: %d, Length: %d", nHash, (nHash >> QUERY_SHIFT) & QUERY_MASK, (nHash >> BLOCK_SHIFT) & BLOCK_MASK, (nHash >> OFFSET_SHIFT) & OFFSET_MASK, (nHash >> LENGTH_SHIFT) & LENGTH_MASK);
    return false;
}

void CUserContentManager::ReleaseAllCachedDescriptions()
{
    gnetDebug1("ReleaseAllCachedDescriptions");

    int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_bRequestFinished)
        {
            gnetDebug2("ReleaseAllCachedDescriptions :: Releasing 0x%08x", m_ContentDescriptionRequests[i]->m_nHash);

            sDescription* pDescription = m_ContentDescriptionRequests[i];
            m_ContentDescriptionRequests.Delete(i);
            --i;
            --nDescriptions;

            if(pDescription->m_szDescription)
                delete[] pDescription->m_szDescription;
            delete pDescription;
        }
        else
        {
            gnetDebug2("ReleaseAllCachedDescriptions :: 0x%08x Pending. Flagged for Release.", m_ContentDescriptionRequests[i]->m_nHash);
            m_ContentDescriptionRequests[i]->m_bFlaggedForRelease = true; 
        }
    }
}

const char* CUserContentManager::GetStringFromScriptDescription(sScriptDescriptionStruct& scriptDescription, char* szDescription, unsigned nMaxLength)
{
    if(!gnetVerify(szDescription))
    {
        gnetError("GetStringFromScriptDescription :: No description buffer!");
        return NULL;
    }

    // empty - track how much we've added
    szDescription[0] = '\0';
    u32 nLength = 0;

    for(u32 i = 0; i < NUM_SCRIPT_TEXTLABELS_PER_UGC_DESCRIPTION; i++)
    {
        nLength += ustrlen(scriptDescription.textLabel[i]);
        if(nLength > nMaxLength)
            break;

        strcat(szDescription, scriptDescription.textLabel[i]);
    }

    return szDescription;
}

void CUserContentManager::ClearCreateResult()
{
	m_CreateResult.Clear();
}

bool CUserContentManager::IsCreatePending()
{
	return m_CreateOp == eOp_Pending;
}

bool CUserContentManager::HasCreateFinished()
{
	return m_CreateOp == eOp_Finished;
}

bool CUserContentManager::DidCreateSucceed()
{
	return m_CreateStatus.Succeeded();
}

int CUserContentManager::GetCreateResultCode()
{
	return m_CreateStatus.GetResultCode();
}

const char* CUserContentManager::GetCreateContentID()
{
	// check that cloud is available
	if(!gnetVerifyf(DidCreateSucceed(), "GetCreateContentID :: Create did not succeed!"))
		return NULL;

	// return from our result
	return m_CreateResult.GetContentId();
}

s64 CUserContentManager::GetCreateCreatedDate()
{
	if(!gnetVerifyf(DidCreateSucceed(), "GetCreateCreatedDate :: Create did not succeed!"))
		return 0;

	// return from our result
	return m_CreateResult.GetCreatedDate();
}

const rlUgcMetadata* CUserContentManager::GetCreateResult()
{
	if(!gnetVerifyf(DidCreateSucceed(), "GetCreateResult :: Create did not succeed!"))
		return NULL;

	// return from our result
	return &m_CreateResult;
}

void CUserContentManager::ClearQueryResults()
{
#if !__NO_OUTPUT
    size_t nRlineBefore = CLiveManager::GetRlineAllocator()->GetMemoryAvailable();
#endif

	int nContentData = m_ContentData.GetCount();
	for(unsigned i = 0; i < nContentData; i++)
	{
		rlUgcContentInfo* pContent = m_ContentData[i];
		pContent->Clear();
		delete pContent;
	}

	int nPlayerData = m_PlayerData.GetCount();
	for(unsigned i = 0; i < nPlayerData; i++)
	{
		rlUgcPlayer* pPlayer = m_PlayerData[i];
		pPlayer->Clear();
		delete pPlayer;
	}

	m_ContentData.Reset();
	m_PlayerData.Reset();
    m_DescriptionHash.Reset();

    // logging
    gnetDebug1("ClearQueryResults :: Rline before: %" I64FMT "u, Rline After: %" I64FMT "u", nRlineBefore, CLiveManager::GetRlineAllocator()->GetMemoryAvailable());
}

void CUserContentManager::ResetContentQuery()
{
	gnetDebug1("ResetContentQuery");

	m_nContentTotal = 0;
	m_nContentNum = 0;
    m_bFlaggedToCancel = false;
    m_bWasQueryForceCancelled = false;

    // description block
    if(m_pDescriptionBlock)
    {
        delete[] m_pDescriptionBlock;
        m_pDescriptionBlock = NULL;
    }

	ClearQueryResults();
}

bool CUserContentManager::IsQueryPending()
{
	return m_QueryOp == eOp_Pending;
}

bool CUserContentManager::HasQueryFinished()
{
	if(m_bQueryDataOffline)
		return true;

	return m_QueryOp == eOp_Finished;
}

bool CUserContentManager::DidQuerySucceed()
{
	if(m_bQueryDataOffline)
		return m_ContentDataOffline.GetCount() > 0;

	return m_QueryStatus.Succeeded();
}

bool CUserContentManager::WasQueryForceCancelled()
{
    return m_bWasQueryForceCancelled;
}

int CUserContentManager::GetQueryResultCode()
{
	return m_QueryStatus.GetResultCode();
}

bool CUserContentManager::CanQueryData(bool bAllowedOffline OUTPUT_ONLY(, const char* szFunction))
{
	// check that we're offline (no async) or that the get call is pending or finished and succeeded
	if(!bAllowedOffline && !gnetVerifyf(!m_bQueryDataOffline, "%s :: Cannot be called for offline data!", szFunction))
		return false;

	// check that we're offline (no async) or that the get call is pending or finished and succeeded
	if(!gnetVerify(m_bQueryDataOffline || IsQueryPending() || DidQuerySucceed()))
	{
		gnetError("%s :: Query not offline, pending or succeeded!", szFunction);
		return false;
	}

	return true;
}

bool CUserContentManager::CanQueryData(int nContentIndex, bool bAllowedOffline OUTPUT_ONLY(, const char* szFunction))
{
	// check general function
	if(!CanQueryData(bAllowedOffline OUTPUT_ONLY(, szFunction)))
		return false;

	// check that the content index is valid
	if(!gnetVerifyf(nContentIndex >= 0 && nContentIndex < static_cast<int>(GetContentNum()), "%s :: Invalid index of %d", szFunction, nContentIndex))
		return false;

	return true;
}

unsigned CUserContentManager::GetContentNum()
{
	if(m_bQueryDataOffline)
		return static_cast<unsigned>(m_ContentDataOffline.GetCount());
	else
		return static_cast<unsigned>(m_ContentData.GetCount());
}

int CUserContentManager::GetContentTotal()
{
	// total results is the same across all queries
	return m_bQueryDataOffline ? m_nContentOfflineTotal : m_nContentTotal;
}

int CUserContentManager::GetContentHash()
{
    if(UseQueryHash(m_kQueryType, m_kQueryContentType))
        return m_QueryHash[m_kQueryType];

    return 0;
}

int CUserContentManager::GetContentAccountID(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentAccountID")))
		return 0;

	if(m_bQueryDataOffline)
		return m_ContentDataOffline[nContentIndex]->m_AccountId;
	else
		return m_ContentData[nContentIndex]->GetMetadata().GetAccountId();
}

rlUgcContentType CUserContentManager::GetContentType(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentType")))
        return RLUGC_CONTENT_TYPE_UNKNOWN;

   return m_ContentData[nContentIndex]->GetMetadata().GetContentType();
}

const char* CUserContentManager::GetContentID(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentID")))
        return NULL;

    if(m_bQueryDataOffline)
        return m_ContentDataOffline[nContentIndex]->m_ContentId;
    else
        return m_ContentData[nContentIndex]->GetMetadata().GetContentId();
}

const char* CUserContentManager::GetRootContentID(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRootContentID")))
        return NULL;

    return m_ContentData[nContentIndex]->GetMetadata().GetRootContentId();
}

const rlUgcCategory CUserContentManager::GetContentCategory(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentCategory")))
        return RLUGC_CATEGORY_UNKNOWN;

    return m_ContentData[nContentIndex]->GetMetadata().GetCategory();
}

const char* CUserContentManager::GetContentName(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentName")))
        return NULL;

    if(m_bQueryDataOffline)
        return m_ContentDataOffline[nContentIndex]->m_ContentName;
    else
        return m_ContentData[nContentIndex]->GetMetadata().GetContentName();
}

u64 CUserContentManager::GetContentCreatedDate(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentCreatedDate")))
        return 0;

    return static_cast<u64>(m_ContentData[nContentIndex]->GetMetadata().GetCreatedDate());
}

const char* CUserContentManager::GetContentData(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentData")))
        return NULL;

    if(m_bQueryDataOffline)
        return static_cast<const char*>(m_ContentDataOffline[nContentIndex]->m_Data.GetBuffer());
    else
        return m_ContentData[nContentIndex]->GetMetadata().GetData();
}

unsigned CUserContentManager::GetContentDataSize(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentDataSize")))
        return 0;

    if(m_bQueryDataOffline)
        return m_ContentDataOffline[nContentIndex]->m_Data.Length();
    else
        return m_ContentData[nContentIndex]->GetMetadata().GetDataSize();
}

const char* CUserContentManager::GetContentDescription(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentDescription")))
        return NULL;

    if(m_bQueryDataOffline)
        return static_cast<const char*>(m_ContentDataOffline[nContentIndex]->m_Description.GetBuffer());
    else
        return m_ContentData[nContentIndex]->GetMetadata().GetDescription();
}

unsigned CUserContentManager::GetContentDescriptionHash(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentDescriptionHash")))
        return 0;

    // no description
    if(m_ContentData[nContentIndex]->GetMetadata().GetDescriptionLength() == 0)
        return 0;

    // get description hash
    return m_DescriptionHash[nContentIndex];
}

int CUserContentManager::GetContentNumFiles(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentNumFiles")))
        return 0;

    return m_ContentData[nContentIndex]->GetMetadata().GetNumFiles();
}

int CUserContentManager::GetContentFileID(int nContentIndex, int nFileIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentFileID")))
        return 0;

    return m_ContentData[nContentIndex]->GetMetadata().GetFileId(nFileIndex);
}

int CUserContentManager::GetContentFileVersion(int nContentIndex, int nFileIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentFileVersion")))
        return 0;

    return m_ContentData[nContentIndex]->GetMetadata().GetFileVersion(nFileIndex);
}

bool CUserContentManager::GetContentHasLoResPhoto(int nContentIndex)
{
	return GetContentFileVersion(nContentIndex, eFile_LoResPhoto) >= 0;
}

bool CUserContentManager::GetContentHasHiResPhoto(int nContentIndex)
{
	return GetContentFileVersion(nContentIndex, eFile_HiResPhoto) >= 0;
}

rlScLanguage CUserContentManager::GetContentLanguage(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentLanguage")))
        return RLSC_LANGUAGE_UNKNOWN;

    return m_ContentData[nContentIndex]->GetMetadata().GetLanguage();
}

rage::RockstarId CUserContentManager::GetContentRockstarID(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentRockstarID")))
		return InvalidRockstarId;

	if(m_bQueryDataOffline)
		return m_ContentDataOffline[nContentIndex]->m_RockstarId;
	else
		return m_ContentData[nContentIndex]->GetMetadata().GetRockstarId();
}

u64 CUserContentManager::GetContentUpdatedDate(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentUpdatedDate")))
        return 0;

    return static_cast<u64>(m_ContentData[nContentIndex]->GetMetadata().GetUpdatedDate());
}

const char* CUserContentManager::GetContentUserID(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentUserID")))
		return NULL;

	if(m_bQueryDataOffline)
		return m_ContentDataOffline[nContentIndex]->m_UserId;
	else
		return m_ContentData[nContentIndex]->GetMetadata().GetUserId();
}

bool CUserContentManager::GetContentCreatorGamerHandle(int nContentIndex, rlGamerHandle* pHandle)
{
	// validate handle
	if(!pHandle)
		return false;

	// clear handle and import using content user ID
	pHandle->Clear();

	// validate user ID - not set for R* content
	const char* szUserId = GetContentUserID(nContentIndex);
	if(szUserId[0] == '\0')
		return false;

	// convert
	if(pHandle->FromUserId(szUserId))
		return true;

	// failed
	return false;
}

bool CUserContentManager::GetContentCreatedByLocalPlayer(int nContentIndex)
{
	// get creator and handle and compare to local
	rlGamerHandle hGamer;
	if(GetContentCreatorGamerHandle(nContentIndex, &hGamer))
		return hGamer == NetworkInterface::GetLocalGamerHandle();

	return false;
}

const char* CUserContentManager::GetContentUserName(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentUserName")))
		return NULL;

	if(m_bQueryDataOffline)
		return m_ContentDataOffline[nContentIndex]->m_UserName;
	else
		return m_ContentData[nContentIndex]->GetMetadata().GetUsername();
}

bool CUserContentManager::GetContentIsUsingScNickname(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentIsUsingScNickname")))
        return false;

    // the username is an SC nickname and not a gamertag if AccountId is 0 and the UserId is empty, 
    // but the RockstarId is > 0 and the UserName is non-empty.
    if(m_ContentData[nContentIndex]->GetMetadata().GetAccountId() == 0 && m_ContentData[nContentIndex]->GetMetadata().GetUserId()[0] == '\0' && 
       m_ContentData[nContentIndex]->GetMetadata().GetRockstarId() > 0 && m_ContentData[nContentIndex]->GetMetadata().GetUsername()[0] != '\0')
    {
        return true;
    }

    // not an SC nickname
    return false;
}

int CUserContentManager::GetContentVersion(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentVersion")))
        return 0;

    return m_ContentData[nContentIndex]->GetMetadata().GetVersion();
}

bool CUserContentManager::IsContentPublished(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "IsContentPublished")))
        return false;

    return m_ContentData[nContentIndex]->GetMetadata().IsPublished();
}

bool CUserContentManager::IsContentVerified(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "IsContentVerified")))
        return false;

    return m_ContentData[nContentIndex]->GetMetadata().IsVerified();
}

u64 CUserContentManager::GetContentPublishedDate(int nContentIndex)
{
    // check that we can query data
    if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentPublishedDate")))
        return 0;

    return static_cast<u64>(m_ContentData[nContentIndex]->GetMetadata().GetPublishedDate());
}

const char* CUserContentManager::GetContentPath(int nContentIndex, int nFileIndex, char* pBuffer, const unsigned nDataSize)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, true OUTPUT_ONLY(, "GetContentPath")))
		return NULL;

	if(m_bQueryDataOffline)
    {
		return m_ContentDataOffline[nContentIndex]->m_CloudAbsPath[nFileIndex];
    }
	else
    {
        // verify buffer size
        if(!gnetVerify(nDataSize >= RLUGC_MAX_CLOUD_ABS_PATH_CHARS))
            gnetError("GetContentPath :: Buffer provided is less than RLUGC_MAX_CLOUD_ABS_PATH_CHARS!");

        // get ID from index
        int nFileID = m_ContentData[nContentIndex]->GetMetadata().GetFileId(nFileIndex);
        
        // reset string and build path
        pBuffer[0] = '\0';
        bool bSuccess = m_ContentData[nContentIndex]->GetMetadata().ComposeCloudAbsPath(nFileID, pBuffer, nDataSize);

        // verify that we built a valid path
        if(!gnetVerify(bSuccess))
            gnetError("GetContentPath :: Failed to build content path!");

        return pBuffer;
    }
}

const char* CUserContentManager::GetContentStats(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentStats")))
		return NULL;
	
	return m_ContentData[nContentIndex]->GetStats();
}

unsigned CUserContentManager::GetContentStatsSize(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetContentStatsSize")))
		return 0;

    return m_ContentData[nContentIndex]->GetStatsSize();
}

float CUserContentManager::GetRating(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRating")))
		return 0.0f;

	return m_ContentData[nContentIndex]->GetRatings().GetAverage();
}

float CUserContentManager::GetRatingPositivePct(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRatingPositivePct")))
		return 0;

	return m_ContentData[nContentIndex]->GetRatings().GetPositivePercent();
}

float CUserContentManager::GetRatingNegativePct(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRatingNegativePct")))
		return 0.0f;

    return m_ContentData[nContentIndex]->GetRatings().GetNegativePercent();
}

s64 CUserContentManager::GetRatingCount(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRatingCount")))
		return 0;

    return m_ContentData[nContentIndex]->GetRatings().GetUnique();
}

s64 CUserContentManager::GetRatingPositiveCount(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRatingPositiveCount")))
		return 0;

	return m_ContentData[nContentIndex]->GetRatings().GetPositiveUnique();
}

s64 CUserContentManager::GetRatingNegativeCount(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetRatingNegativeCount")))
		return 0;

    return m_ContentData[nContentIndex]->GetRatings().GetNegativeUnique();
}

int CUserContentManager::GetContentIndexFromID(const char* szContentID)
{
	int nContent = GetContentNum();
	for(int i = 0; i < nContent; i++)
	{
		if(strncmp(GetContentID(i), szContentID, RLUGC_MAX_CONTENTID_CHARS) == 0)
			return i;
	}

	// not found
	return -1;
}

const char* CUserContentManager::GetPlayerData(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetPlayerData")))
		return NULL;

	// check that we have a player record
	if(!gnetVerifyf(HasPlayerRecord(nContentIndex), "GetPlayerData :: No player record! Call HasPlayerRecord"))
		return NULL;

	const rlUgcPlayer* pPlayer = m_PlayerData[nContentIndex];
	return pPlayer->GetData();
}

unsigned CUserContentManager::GetPlayerDataSize(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetPlayerDataSize")))
		return 0;

	// check that we have a player record
	if(!gnetVerifyf(HasPlayerRecord(nContentIndex), "GetPlayerDataSize :: No player record! Call HasPlayerRecord"))
		return 0;

	const rlUgcPlayer* pPlayer = m_PlayerData[nContentIndex];
	return pPlayer->GetDataSize();
}

bool CUserContentManager::HasPlayerRecord(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "HasPlayerRecord")))
		return 0;

	return m_PlayerData[nContentIndex]->IsValid();
}

bool CUserContentManager::HasPlayerRated(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "HasPlayerRated")))
		return 0;

	// check that we have a player record
	if(!gnetVerifyf(HasPlayerRecord(nContentIndex), "HasPlayerRated :: No player record! Call HasPlayerRecord"))
		return false;

	const rlUgcPlayer* pPlayer = m_PlayerData[nContentIndex];
	return pPlayer->HasRating();
}

float CUserContentManager::GetPlayerRating(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "GetPlayerRating")))
		return 0;

	// check that the player has rated this content
	if(!gnetVerifyf(HasPlayerRated(nContentIndex), "GetPlayerRating :: Content not rated! Call HasPlayerRated."))
		return 0.0f;

	// check that we have a player record
	if(!gnetVerifyf(HasPlayerRecord(nContentIndex), "GetPlayerRating :: No player record! Call HasPlayerRecord"))
		return false;

	const rlUgcPlayer* pPlayer = m_PlayerData[nContentIndex];
	return pPlayer->GetRating();
}

bool CUserContentManager::HasPlayerBookmarked(int nContentIndex)
{
	// check that we can query data
	if(!CanQueryData(nContentIndex, false OUTPUT_ONLY(, "HasPlayerBookmarked")))
		return false;

	// check that we have a player record
	if(!gnetVerifyf(HasPlayerRecord(nContentIndex), "HasPlayerBookmarked :: No player record! Call HasPlayerRecord"))
		return false;

	const rlUgcPlayer* pPlayer = m_PlayerData[nContentIndex];
	return pPlayer->IsBookmarked();
}

void CUserContentManager::ClearModifyResult()
{
	m_ModifyResult.Clear();
}

bool CUserContentManager::IsModifyPending()
{
	return m_ModifyOp == eOp_Pending;
}

bool CUserContentManager::HasModifyFinished()
{
	return m_ModifyOp == eOp_Finished;
}

bool CUserContentManager::DidModifySucceed()
{
	return m_ModifyStatus.Succeeded();
}

int CUserContentManager::GetModifyResultCode()
{
	return m_ModifyStatus.GetResultCode();
}

const char* CUserContentManager::GetModifyContentID()
{
	// check that cloud is available
	if(!gnetVerifyf(DidModifySucceed(), "GetModifyContentID :: Modify did not succeed!"))
		return NULL;

	// return from our modify result
	return m_ModifyResult.GetContentId();
}

void CUserContentManager::OnCreatorResult(const unsigned nTotal, const rlUgcContentCreatorInfo* pCreator)
{
	// stash the content total 
	m_nCreatorTotal = nTotal;
	m_nCreatorNum++;

	// copy out the content data
	rlUgcContentCreatorInfo* pNewCreator = rage_new rlUgcContentCreatorInfo;
	*pNewCreator = *pCreator;
	m_CreatorData.PushAndGrow(pNewCreator);

	gnetDebug1("OnCreatorResult :: Added creator. Name: %s. Total: %d", pCreator->m_UserName, nTotal);
	gnetAssertf(m_nCreatorTotal > 0, "OnCreatorResult :: Given creator but total is 0!");
	gnetAssertf(m_nCreatorTotal >= m_nCreatorNum, "OnCreatorResult :: Total is less than current count!");
}

bool CUserContentManager::QueryContentCreators(rlUgcContentType kType, const char* szQueryName, const char* pQueryParams, int nOffset, int nMax, rlUgc::QueryContentCreatorsDelegate fnDelegate, netStatus* pStatus)
{
	// check that cloud is available
	if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "QueryContentCreators :: Cloud not available!"))
		return false;

	// check that we're not already retrieving UGC creators
	if(!gnetVerifyf(!pStatus->Pending(), "QueryContentCreators :: Operation in progress!"))
		return false;

	// logging
	gnetDebug1("QueryContentCreators (External) :: Query: %s. Params: %s, Max: %d, Offset: %d", szQueryName, pQueryParams ? pQueryParams : "None", nMax, nOffset);

	// reset the status
	pStatus->Reset();

	bool bSuccess = rlUgc::QueryContentCreators(NetworkInterface::GetLocalGamerIndex(), 
												kType, 
												szQueryName,
                                                pQueryParams,
												nOffset,
												nMax,
												fnDelegate,
												GetUserContentRequestAllocator(),
												pStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "QueryContentCreators :: Query '%s' failed to start", szQueryName)) 
		return false;

	// update op status and indicate success
	return true;
}

bool CUserContentManager::QueryContentCreators(rlUgcContentType kType, const char* szQueryName, const char* pQueryParams, int nOffset)
{
	// check that cloud is available
	if(!gnetVerifyf(NetworkInterface::IsCloudAvailable(), "QueryContentCreators :: Cloud not available!"))
		return false;

	// check that we're not already retrieving UGC creators
	if(!gnetVerifyf(!m_QueryCreatorsStatus.Pending(), "QueryContentCreators :: Operation in progress!"))
		return false;

	// logging
	gnetDebug1("QueryContentCreators :: Query: %s. Params: %s, Max: %d, Offset: %d", szQueryName, pQueryParams ? pQueryParams : "None", MAX_CREATOR_RESULTS, nOffset);

	// reset previous data
	ResetCreatorQuery();

	// reset the status
	m_QueryCreatorsStatus.Reset();
	m_QueryCreatorsOp = eOp_Idle;

	bool bSuccess = rlUgc::QueryContentCreators(NetworkInterface::GetLocalGamerIndex(), 
												kType, 
												szQueryName,
                                                pQueryParams,	// string, query sensitive (can be NULL)
												nOffset,
												MAX_CREATOR_RESULTS,
												m_CreatorDelegate,
												GetUserContentRequestAllocator(),
												&m_QueryCreatorsStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "QueryContentCreators :: Query '%s' failed to start", szQueryName)) 
		return false;

	// update op status and indicate success
	m_QueryCreatorsOp = eOp_Pending;

#if !__NO_OUTPUT
	m_nQueryCreatorsTimestamp = sysTimer::GetSystemMsTime();
#endif

	return true;
}

bool CUserContentManager::GetCreatorsTopRated(int nOffset, rlUgcContentType kType)
{
	return QueryContentCreators(kType, "GetTopRated", NULL, nOffset);
}

bool CUserContentManager::GetCreatorsByUserID(char szUserIDs[][RLROS_MAX_USERID_SIZE], unsigned nUserIDs, const rlUgcContentType kType)
{
	if(!gnetVerifyf(nUserIDs != 0, "GetCreatorsByUserID :: No user IDs!")) 
		return false;

	// build user IDs parameter
	atString szQueryParams;
	if(BuildCreatorUserIDParam("userids", szUserIDs, nUserIDs, szQueryParams))
		return QueryContentCreators(kType, "GetByUserId", szQueryParams.c_str(), 0);

	return false;
}

bool CUserContentManager::BuildCreatorUserIDParam(const char* pQueryType, char szData[][RLROS_MAX_USERID_SIZE], unsigned nStrings, atString& out_QueryParams)
{
	// no strings and no query
	if(nStrings == 0 || pQueryType == NULL) 
		return false;

	// start JSON query
	out_QueryParams = "{";
	out_QueryParams += pQueryType;
	out_QueryParams += ":[";

	// add each user ID
	char szUserID[RLROS_MAX_USERID_SIZE];

	for(int i = 0; i < nStrings; i++)
	{
		if(i > 0)
			out_QueryParams += ",";

		// build user ID (add two for quotes)
		formatf(szUserID, RLROS_MAX_USERID_SIZE, "\"%s\"", szData[i]);
		out_QueryParams += szUserID;
	}

	// end JSON query
	out_QueryParams += "]}";

	return true;
}

void CUserContentManager::ClearCreatorResults()
{
	gnetDebug1("ClearCreatorResults");

	int nCreatorData = m_CreatorData.GetCount();
	for(unsigned i = 0; i < nCreatorData; i++)
	{
		rlUgcContentCreatorInfo* pCreator = m_CreatorData[i];
		pCreator->Clear();
		delete pCreator;
	}

	m_CreatorData.Reset();
}

void CUserContentManager::ResetCreatorQuery()
{
	gnetDebug1("ResetCreatorQuery");

	m_nCreatorTotal = 0;
	m_nCreatorNum = 0;
	ClearCreatorResults();
}

bool CUserContentManager::IsQueryCreatorsPending()
{
	return m_QueryCreatorsOp == eOp_Pending;
}

bool CUserContentManager::HasQueryCreatorsFinished()
{
	return m_QueryCreatorsOp == eOp_Finished;
}

bool CUserContentManager::DidQueryCreatorsSucceed()
{
	return m_QueryCreatorsStatus.Succeeded();
}

int CUserContentManager::GetQueryCreatorsResultCode()
{
	return m_QueryCreatorsStatus.GetResultCode();
}

unsigned CUserContentManager::GetCreatorNum()
{
	return static_cast<unsigned>(m_CreatorData.GetCount());
}

unsigned CUserContentManager::GetCreatorTotal()
{
	return m_nCreatorTotal;
}

const char* CUserContentManager::GetCreatorUserID(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorUserID :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorUserID :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_UserId;
}

const char* CUserContentManager::GetCreatorUserName(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorUserName :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorUserName :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_UserName;
}

int CUserContentManager::GetCreatorNumCreated(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorNumCreated :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorNumCreated :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_NumCreated;
}

int CUserContentManager::GetCreatorNumPublished(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorNumPublished :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorNumPublished :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_NumPublished;
}

float CUserContentManager::GetCreatorRating(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorRating :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorRating :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_Ratings.GetAverage();
}

float CUserContentManager::GetCreatorRatingPositivePct(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorRatingPositivePct :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorRatingPositivePct :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_Ratings.GetPositivePercent();
}

float CUserContentManager::GetCreatorRatingNegativePct(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorRatingNegativePct :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorRatingNegativePct :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_Ratings.GetNegativePercent();
}

s64 CUserContentManager::GetCreatorRatingCount(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorRatingCount :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorRatingCount :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_Ratings.GetUnique();
}

s64 CUserContentManager::GetCreatorRatingPositiveCount(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorRatingPositiveCount :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorRatingPositiveCount :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_Ratings.GetPositiveUnique();
}

s64 CUserContentManager::GetCreatorRatingNegativeCount(int nCreatorIndex)
{
	// check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorRatingNegativeCount :: Query not pending or succeeded!"))
		return 0;

	// check that the content index is valid
	if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorRatingNegativeCount :: Invalid index of %d", nCreatorIndex))
		return 0;

	// access creator row
	return m_CreatorData[nCreatorIndex]->m_Ratings.GetNegativeUnique();
}

const char* CUserContentManager::GetCreatorStats(int nCreatorIndex)
{
    // check that the Get call succeeded
	if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorStats :: Query not pending or succeeded!"))
		return NULL;

    // check that the content index is valid
    if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorStats :: Invalid index of %d", nCreatorIndex))
        return NULL;

    // access creator row
    return reinterpret_cast<const char*>(m_CreatorData[nCreatorIndex]->m_Stats.GetBuffer());
}

unsigned CUserContentManager::GetCreatorStatsSize(int nCreatorIndex)
{
    // check that the Get call succeeded
    if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorStatsSize :: Query not pending or succeeded!"))
		return 0;

    // check that the content index is valid
    if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorStatsSize :: Invalid index of %d", nCreatorIndex))
        return 0;

    // access creator row
    return m_CreatorData[nCreatorIndex]->m_Stats.Length();
}

bool CUserContentManager::GetCreatorIsUsingScNickname(int nCreatorIndex)
{
    // check that the Get call succeeded
    if(!gnetVerifyf(DidQueryCreatorsSucceed() || IsQueryCreatorsPending(), "GetCreatorIsUsingScNickname :: Query not pending or succeeded!"))
        return false;

    // check that the content index is valid
    if(!gnetVerifyf(nCreatorIndex < static_cast<int>(GetCreatorNum()), "GetCreatorIsUsingScNickname :: Invalid index of %d", nCreatorIndex))
        return false;

    // the username is an SC nickname and not a gamertag if AccountId is 0 and the UserId is empty, 
    // but the RockstarId is > 0 and the UserName is non-empty.
    if(m_CreatorData[nCreatorIndex]->m_AccountId == 0 && m_CreatorData[nCreatorIndex]->m_UserId[0] == '\0' && 
       m_CreatorData[nCreatorIndex]->m_RockstarId > 0 && m_CreatorData[nCreatorIndex]->m_UserName[0] != '\0')
    {
        return true;
    }

    // not an SC nickname
    return false;
}

const char* CUserContentManager::GetAbsPathFromRelPath(const char* szRelPath, char* szAbsPath, unsigned nMaxPath)
{
	// no relative path, no absolute path
	if(!szRelPath)
		return NULL;

	// empty relative path, no absolute path
	if(szRelPath[0] == '\0')
		return NULL;

	// work out service stub
#if RSG_DURANGO
	static const char* szService = "xbl";
#elif RSG_ORBIS
	static const char* szService = "np";
#else
	static const char* szService = "sc";
#endif

	// create member ID
	static const unsigned kMAX_MEMBER_ID = 32;
	char szMemberID[kMAX_MEMBER_ID];
	NetworkInterface::GetActiveGamerInfo()->GetGamerHandle().ToUserId(szMemberID, kMAX_MEMBER_ID);

	// build the absolute path
	snprintf(szAbsPath, nMaxPath, "/members/%s/%s/%s", szService, szMemberID, szRelPath);

	// return the passed string
	return szAbsPath;
}

bool CUserContentManager::LoadOfflineQueryData(rlUgcCategory nCategory)
{
	static const unsigned kMAX_PATH = 256;
	char szFileName[kMAX_PATH];
	snprintf(szFileName, kMAX_PATH, "common:/data/ugc/UGC_%s.ugc", rlUgcCategoryToString(nCategory));

    // logging
    gnetDebug1("LoadOfflineQueryData :: Loading %s", szFileName);

	// open the stream
	fiStream* pStream = ASSET.Open(szFileName, "", false);
	if(pStream == NULL)
	{
		gnetAssertf(0, "LoadOfflineQueryData :: Could not open query: %s!", szFileName);
		return false;
	}

    // read the total content number
    int nVersion;
    pStream->ReadInt(&nVersion, 1);

    // verify version
    if(nVersion != OFFLINE_CONTENT_VERSION)
    {
        gnetAssertf(0, "LoadOfflineQueryData :: Invalid version: Found: %d, Local: %d. Wait for next build!", nVersion, OFFLINE_CONTENT_VERSION);
        pStream->Close();
        return false;
    }

	// read the total content number
    int nContentOfflineTotal;
	pStream->ReadInt(&nContentOfflineTotal, 1);
    gnetDebug1("LoadOfflineQueryData :: Total content: %d", nContentOfflineTotal);

	// verify UGC constants
	if(!VerifyOfflineQueryData(pStream))
	{
		gnetDebug1("LoadOfflineQueryData :: Could not verify query: %s!", szFileName);
		pStream->Close();
		return false;
	}

	// read content
	for(int i = 0; i < nContentOfflineTotal; i++)
	{
		sOfflineContentInfo* pInfo = rage_new sOfflineContentInfo;

		unsigned nDescriptionLength = 0;
		unsigned nDataSize = 0;

		// write content info
		pStream->ReadByte(pInfo->m_ContentId, RLUGC_MAX_CONTENTID_CHARS);
		pStream->ReadInt(&pInfo->m_AccountId, 1);
		pStream->ReadByte(pInfo->m_ContentName, RLUGC_MAX_CONTENT_NAME_CHARS);

		pStream->ReadInt(&nDescriptionLength, 1);
		char* pDescription = rage_new char[nDescriptionLength];
		pStream->Read(pDescription, nDescriptionLength);
		pInfo->m_Description.Append(pDescription, nDescriptionLength);
		delete[] pDescription;

		pStream->ReadInt(&nDataSize, 1);
		char* pData = rage_new char[nDataSize];
		pStream->Read(pData, nDataSize);
		pInfo->m_Data.Append(pData, nDataSize);
		delete[] pData;

		pStream->ReadLong(&pInfo->m_RockstarId, 1);
		pStream->ReadByte(pInfo->m_UserId, RLROS_MAX_USERID_SIZE);
		pStream->ReadByte(pInfo->m_UserName, RLROS_MAX_USER_NAME_SIZE);
       
        pStream->ReadInt(&pInfo->m_nNumFiles, 1);
        for(int i = 0; i < pInfo->m_nNumFiles; i++)
            pStream->ReadByte(pInfo->m_CloudAbsPath[i], RLUGC_MAX_CLOUD_ABS_PATH_CHARS);

        m_ContentDataOffline.PushAndGrow(pInfo);

        m_nContentOfflineTotal++;
        gnetDebug1("LoadOfflineQueryData :: Added content. Name: %s, ID: %s, Total: %d", pInfo->m_ContentName, pInfo->m_ContentId, m_nContentOfflineTotal);
    }

	// close stream
	pStream->Close();

    // success
    return true; 
}

bool CUserContentManager::VerifyOfflineQueryData(fiStream* pStream)
{
	// read string constants - verify content before reading
	int nConstant;

	pStream->ReadInt(&nConstant, 1);
	if(!gnetVerify(nConstant == RLUGC_MAX_CONTENTID_CHARS))
	{
		gnetError("VerifyOfflineQueryData :: RLUGC_MAX_CONTENTID_CHARS is different! Need new offline data!");
		return false; 
	}

	pStream->ReadInt(&nConstant, 1);
	if(!gnetVerify(nConstant == RLUGC_MAX_CLOUD_ABS_PATH_CHARS))
	{
		gnetError("VerifyOfflineQueryData :: RLUGC_MAX_CLOUD_ABS_PATH_CHARS is different! Need new offline data!");
		return false; 
	}

	pStream->ReadInt(&nConstant, 1);
	if(!gnetVerify(nConstant == RLUGC_MAX_CONTENT_NAME_CHARS))
	{
		gnetError("VerifyOfflineQueryData :: RLUGC_MAX_CONTENT_NAME_CHARS is different! Need new offline data!");
		return false; 
	}

	pStream->ReadInt(&nConstant, 1);
	if(!gnetVerify(nConstant == RLROS_MAX_USERID_SIZE))
	{
		gnetError("VerifyOfflineQueryData :: RLROS_MAX_USERID_SIZE is different! Need new offline data!");
		return false; 
	}

	pStream->ReadInt(&nConstant, 1);
	if(!gnetVerify(nConstant == RLROS_MAX_USER_NAME_SIZE))
	{
		gnetError("VerifyOfflineQueryData :: RLROS_MAX_USER_NAME_SIZE is different! Need new offline data!");
		return false; 
	}

	// success
	return true;
}

void CUserContentManager::ClearOfflineQueryData()
{
    // logging
    gnetDebug1("ClearOfflineQueryData");

	// clear data
	int nData = m_ContentDataOffline.GetCount();
	for(int i = 0; i < nData; i++)
		delete m_ContentDataOffline[i];

	// wipe entries
	m_ContentDataOffline.Reset();

	// reset total
	m_nContentOfflineTotal = 0;
}

void CUserContentManager::SetContentFromOffline(bool bFromOffline)
{
    if(m_bQueryDataOffline != bFromOffline)
    {
        gnetDebug1("SetContentFromOffline :: Setting to %s", bFromOffline ? "True" : "False");
        m_bQueryDataOffline = bFromOffline;
    }
}

const char* CUserContentManager::GetOfflineContentPath(const char* szContentID, int nFileID, bool useUpdate, char* szPath, unsigned nMaxPath)
{
	if(useUpdate)
	{
		// titleupdate/build path
		snprintf(szPath, nMaxPath, "update2:/common/data/ugc/%s_%02d.ugc", szContentID, nFileID);
	}
	else
	{
		snprintf(szPath, nMaxPath, "common:/data/ugc/%s_%02d.ugc", szContentID, nFileID);
	}

	return szPath;
}

void CUserContentManager::OnCacheFileLoaded(const CacheRequestID nRequestID, bool bLoaded)
{
    int nDescriptions = m_ContentDescriptionRequests.GetCount();
    for(int i = 0; i < nDescriptions; i++)
    {
        if(m_ContentDescriptionRequests[i]->m_nCacheRequestID == nRequestID)
        {
            m_ContentDescriptionRequests[i]->m_bRequestFinished = true; 
            m_ContentDescriptionRequests[i]->m_bRequestSucceeded = bLoaded; 
            
            if(m_ContentDescriptionRequests[i]->m_bRequestSucceeded)
            {
                if(!m_ContentDescriptionRequests[i]->m_bFlaggedForRelease)
                {
                    // make description
                    unsigned nLength = ((m_ContentDescriptionRequests[i]->m_nHash >> LENGTH_SHIFT) & LENGTH_MASK) + 1;
                    
                    // grab the stream
                    fiStream* pStream = CloudCache::GetCacheDataStream(nRequestID);

                    // offset stream to description location and read in length
                    unsigned nOffset = (m_ContentDescriptionRequests[i]->m_nHash >> OFFSET_SHIFT) & OFFSET_MASK;
                    if((pStream->Tell() + nOffset + nLength) <= pStream->Size())
                    {
                        // create description buffer
                        m_ContentDescriptionRequests[i]->m_szDescription = rage_new char[nLength + 1];

                        // seek to description and read
                        pStream->Seek(pStream->Tell() + nOffset);
                        pStream->ReadByte(m_ContentDescriptionRequests[i]->m_szDescription, nLength);

                        // ensure null terminated
                        m_ContentDescriptionRequests[i]->m_szDescription[nLength] = '\0';
                        
                        // logging
                        gnetDebug1("OnCacheFileLoaded :: 0x%08x, Description: %s", m_ContentDescriptionRequests[i]->m_nHash, m_ContentDescriptionRequests[i]->m_szDescription);
                    }
                    else
                    {
                        // logging
                        gnetDebug1("OnCacheFileLoaded :: 0x%08x, Offset (%d) + Length (%d) > Size (%d)", m_ContentDescriptionRequests[i]->m_nHash, nOffset, nLength, pStream->Size() - pStream->Tell());
                        m_ContentDescriptionRequests[i]->m_bRequestSucceeded = false; 
                    }
                }
            }
            else
            {
                gnetDebug1("OnCacheFileLoaded :: 0x%08x Failed", m_ContentDescriptionRequests[i]->m_nHash);
            }

            // release the description if flagged
            if(m_ContentDescriptionRequests[i]->m_bFlaggedForRelease)
            {                
                gnetDebug1("OnCacheFileLoaded :: Releasing 0x%08x", m_ContentDescriptionRequests[i]->m_nHash);

                sDescription* pDescription = m_ContentDescriptionRequests[i];
                m_ContentDescriptionRequests.Delete(i);

                if(pDescription->m_szDescription)
                    delete[] pDescription->m_szDescription;
                delete pDescription;
            }

            // finished 
            break; 
        }
    }
}

void CUserContentManager::SetUsingOfflineContent(bool bIsUsingOfflineContent)
{
    if(m_bIsUsingOfflineContent != bIsUsingOfflineContent)
    {
        // wipe the content hash if we are switching between online / offline
        // the globals will be full of offline content, so we need to make sure we get a full grab
        ResetContentHash();

        gnetDebug1("SetUsingOfflineContent :: Setting to %s", bIsUsingOfflineContent ? "True" : "False");
        m_bIsUsingOfflineContent = bIsUsingOfflineContent;
    }
}

bool CUserContentManager::IsUsingOfflineContent()
{
    return m_bIsUsingOfflineContent;
}

UgcRequestID CUserContentManager::CreateYoutubeContent(const rlYoutubeUpload* pYoutubeUpload, const bool bPublish, netStatus* pStatus)
{
	// validate 
	if(!pYoutubeUpload)
	{
		gnetError("CreateYoutubeContent :: Invalid rlYoutubeUpload!");
		return false;
	}

	// our response needs to be valid
	if(!pYoutubeUpload->m_Response.GetBuffer())
	{
		gnetError("CreateYoutubeContent :: No JSON response!");
		return INVALID_UGC_REQUEST_ID;
	}

	// build metadata
	rlUgcYoutubeMetadata tMetadata;
	if(!PopulateYoutubeMetadata(static_cast<const char*>(pYoutubeUpload->m_Response.GetBuffer()), pYoutubeUpload->m_Response.Length(), &tMetadata))
	{
		gnetError("CreateYoutubeContent :: Failed to read metadata!");
		return INVALID_UGC_REQUEST_ID;
	}

	// assign size and score directly from rlYoutubeUpload
	tMetadata.nSize = pYoutubeUpload->UploadSize;
	tMetadata.nQualityScore = pYoutubeUpload->VideoInfo.QualityScore;
	tMetadata.nTrackId = pYoutubeUpload->VideoInfo.TrackId;
	tMetadata.nFilenameHash = pYoutubeUpload->VideoInfo.FilenameHash;
	tMetadata.nDuration_ms = pYoutubeUpload->VideoInfo.DurationMs;
	tMetadata.nModdedContent = pYoutubeUpload->VideoInfo.ModdedContent;

	// we need to crop title strings to 64 bytes, not 100 bytes as youtube allows
	char title[RLUGC_MAX_CONTENT_NAME_CHARS];
	safecpy(title, pYoutubeUpload->VideoInfo.Title);
	Utf8RemoveInvalidSurrogates(title);

		// call with full parameter set
	return CreateYoutubeContent(title,
								pYoutubeUpload->VideoInfo.Description,
								pYoutubeUpload->VideoInfo.Tags,
								&tMetadata,
								bPublish,
								pStatus);
}

UgcRequestID CUserContentManager::CreateYoutubeContent(const char* szTitle, 
								  const char* szDescription, 
								  const char* szTags, 
								  const rlUgcYoutubeMetadata* pMetadata,
								  const bool bPublish,
								  netStatus* pStatus)
{
	// check that cloud is available
	if(!NetworkInterface::IsCloudAvailable())
	{
		gnetError("CreateYoutubeContent :: Cloud not available!");
		return INVALID_UGC_REQUEST_ID;
	}

#if __BANK
	if(m_sbSimulateNoUgcWritePrivilege)
		return INVALID_UGC_REQUEST_ID;
#endif

	// check UGC write privilege
	if(!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_UGCWRITE))
	{
		gnetWarning("CreateYoutubeContent :: No RLROS_PRIVILEGEID_UGCWRITE privilege!");
		return INVALID_UGC_REQUEST_ID;
	}

	// check given status is NULL or not pending
	if(pStatus && pStatus->Pending())
	{
		gnetError("CreateYoutubeContent :: Status already pending!");
		return INVALID_UGC_REQUEST_ID;
	}

	// create metadata - work out size of buffer
	RsonWriter rsonWriter;
	rsonWriter.InitNull(RSON_FORMAT_JSON);
	if(!WriteYoutubeMetadata(rsonWriter, pMetadata))
	{
		gnetError("CreateYoutubeContent :: Failed to write metadata!");
		return INVALID_UGC_REQUEST_ID;
	}

	unsigned nDataJsonLength = rsonWriter.Length() + 1;
	char* pDataJson = Alloca(char, nDataJsonLength);
	if(!pMetadata)
	{
		gnetError("CreateYoutubeContent :: Failed to allocate metadata buffer!");
		return INVALID_UGC_REQUEST_ID;
	}

	rsonWriter.Init(pDataJson, nDataJsonLength, RSON_FORMAT_JSON);
	if(!WriteYoutubeMetadata(rsonWriter, pMetadata))
	{
		gnetError("CreateYoutubeContent :: Failed to write metadata!");
		return INVALID_UGC_REQUEST_ID;
	}
	
	// create a new request
	UgcCreateRequestNode* pCreateRequest = rage_new UgcCreateRequestNode;
	
	// dummy to resolve calling ambiguity
	const rlUgc::SourceFileInfo* pFileInfo = NULL;

	// call up to UGC
	bool bSuccess = rlUgc::CreateContent(NetworkInterface::GetLocalGamerIndex(), 
										 RLUGC_CONTENT_TYPE_GTA5YOUTUBE, 
										 szTitle, 
										 pDataJson, 
										 szDescription,
										 pFileInfo,
										 0,
										 CText::GetScLanguageFromCurrentLanguageSetting(),
										 szTags,
										 bPublish,
										 &pCreateRequest->m_Metadata,
										 GetUserContentCreateAllocator(),
										 &pCreateRequest->m_MyStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "CreateYoutubeContent :: rlUgc::CreateContent failed!"))
	{
		delete pCreateRequest;
		return INVALID_UGC_REQUEST_ID;
	}

	// fill in create result
	pCreateRequest->m_Status = pStatus;
	pCreateRequest->m_nRequestID = m_UgcRequestCount++;
	pCreateRequest->m_nTimeStarted = sysTimer::GetSystemMsTime();

	// mark result as pending
	if(pCreateRequest->m_Status)
		pCreateRequest->m_Status->SetPending();

	// logging
	gnetDebug1("[%d] CreateYoutubeContent :: Creating %s, VideoID: %s, Publish: %s", pCreateRequest->m_nRequestID, szTitle, pMetadata->szVideoID, bPublish ? "True" : "False");

	// stick onto request list and return ID
	m_RequestList.PushAndGrow(pCreateRequest);
	return pCreateRequest->m_nRequestID;
}

UgcRequestID CUserContentManager::QueryYoutubeContent(const int nOffset, int nMaxCount, const bool bPublished, int* pTotal, rlUgc::QueryContentDelegate fnDelegate, netStatus* pStatus)
{
	// check that cloud is available
	if(!NetworkInterface::IsCloudAvailable())
	{
		gnetError("CreateYoutubeContent :: Cloud not available!");
		return INVALID_UGC_REQUEST_ID;
	}

	// check given status is NULL or not pending
	if(pStatus && pStatus->Pending())
	{
		gnetError("QueryYoutubeContent :: Status already pending!");
		return INVALID_UGC_REQUEST_ID;
	}

	// use default MAX_QUERY_COUNT if 0 was passed
	if(nMaxCount == 0)
		nMaxCount = MAX_QUERY_COUNT;

	// parameters - for this, just whether the content is published
	char szParams[64]; szParams[0] = '\0';
	if(bPublished)
		formatf(szParams, "{published:true}");

	// create a new request
	UgcRequestNode* pRequest = rage_new UgcRequestNode;

	// call up to UGC
	bool bSuccess = rlUgc::QueryContent(NetworkInterface::GetLocalGamerIndex(),
										RLUGC_CONTENT_TYPE_GTA5YOUTUBE,
										"GetMyContent",
										szParams,
										nOffset,
										nMaxCount,
										pTotal,
										NULL,
										fnDelegate,
										GetUserContentRequestAllocator(),
										&pRequest->m_MyStatus);

	// check result
	if(!gnetVerifyf(bSuccess, "QueryYoutubeContent :: rlUgc::QueryContent failed!"))
	{
		delete pRequest;
		return INVALID_UGC_REQUEST_ID;
	}

	// fill in create result
	pRequest->m_Status = pStatus;
	pRequest->m_nRequestID = m_UgcRequestCount++;
	pRequest->m_nTimeStarted = sysTimer::GetSystemMsTime();

	// mark result as pending
	if(pRequest->m_Status)
		pRequest->m_Status->SetPending();

	// logging
	gnetDebug1("[%d] QueryYoutubeContent :: Offset: %d, Max: %d", pRequest->m_nRequestID, nOffset, nMaxCount);

	// stick onto request list and return ID
	m_RequestList.PushAndGrow(pRequest);
	return pRequest->m_nRequestID;
}

bool CUserContentManager::PopulateYoutubeMetadata(const char* pData, unsigned nSizeOfData, rlUgcYoutubeMetadata* pMetadata) const
{
	rtry
	{
		rcheck(nSizeOfData > 0, catchall, gnetError("PopulateYoutubeMetadata :: Invalid data size!"));

		// skip backwards from the end until we reach the closing brace
		unsigned nSizeOfJson = nSizeOfData;
		while(nSizeOfJson > 0 && pData[nSizeOfJson - 1] != '}')
			--nSizeOfJson;

		rcheck(nSizeOfJson > 0, catchall, gnetError("PopulateYoutubeMetadata :: Invalid json data size!"));

		rcheck(RsonReader::ValidateJson(static_cast<const char*>(pData), nSizeOfJson), catchall, gnetError("PopulateYoutubeMetadata :: Invalid JSON"));

		// initialise reader
		RsonReader tReader, tValueReader;
		rcheck(tReader.Init(pData, 0, nSizeOfData), catchall, gnetError("PopulateYoutubeMetadata :: Failed to initialise reader"));

		// get video id
		rcheck(tReader.GetMember("id", &tValueReader) && tValueReader.AsString(pMetadata->szVideoID), catchall, gnetError("PopulateYoutubeMetadata :: Failed to read video ID"));

		// get thumbnail urls
		RsonReader tSnippet, tThumbnails, tThumbnail;
		rcheck(tReader.GetMember("snippet", &tSnippet) && tSnippet.GetMember("thumbnails", &tThumbnails), catchall, gnetError("PopulateYoutubeMetadata :: Failed to find \"thumbnails\" member"));
		rcheck(tThumbnails.GetMember("default", &tThumbnail) && tThumbnail.GetMember("url", &tValueReader) && tValueReader.AsString(pMetadata->szThumbnailDefaultUrl), catchall, gnetError("PopulateYoutubeMetadata :: Failed to read \"default\" thumbnail url"));
		rcheck(tThumbnails.GetMember("medium", &tThumbnail) && tThumbnail.GetMember("url", &tValueReader) && tValueReader.AsString(pMetadata->szThumbnailMediumUrl), catchall, gnetError("PopulateYoutubeMetadata :: Failed to read \"medium\" thumbnail url"));
		rcheck(tThumbnails.GetMember("high", &tThumbnail) && tThumbnail.GetMember("url", &tValueReader) && tValueReader.AsString(pMetadata->szThumbnailLargeUrl), catchall, gnetError("PopulateYoutubeMetadata :: Failed to read \"high\" thumbnail url"));

		// few youtube responses seem to have a valid time for "duration" 
		// this may need looking into

		// and the duration
		RsonReader tDetails;
		rcheck(tReader.GetMember("contentDetails", &tDetails) && tDetails.GetMember("duration", &tValueReader) && tValueReader.AsString(pMetadata->szDuration), catchall, gnetError("PopulateYoutubeMetadata :: Failed to read \"duration\" member"));

		// duration is ISO 8601 format
		int nMinutes = 0, nSeconds = 0, nDays = 0;
		if(sscanf(pMetadata->szDuration, "PT%dM%dS", &nMinutes, &nSeconds) == 2)
			pMetadata->nDuration_s = (nMinutes * 60) + nSeconds;
		else if(sscanf(pMetadata->szDuration, "PT%dS", &nSeconds) == 1)
			pMetadata->nDuration_s = nSeconds;
		else if(sscanf(pMetadata->szDuration, "P%dD", &nDays) == 1) // Will probably always be P0D in this case
			pMetadata->nDuration_s = nDays * 24 * 60 * 60;
		else
		{
			// This value isn't reliable so we don't fail if the parsing fails.
			gnetError("PopulateYoutubeMetadata :: Failed to parse \"duration\" member [%s]", pMetadata->szDuration);
			pMetadata->nDuration_s = 0;
		}

		return true;
	}
	rcatchall
	{
#if !__NO_OUTPUT
		// log to file
		fiStream* pStream(ASSET.Create("Youtube_ValidationFail.txt", ""));
		if(pStream)
		{
			pStream->Write(pData, nSizeOfData);
			pStream->Close();
		}
#endif
		return false;
	}
}

bool CUserContentManager::ReadYoutubeMetadata(const char* pData, unsigned nSizeOfData, rlUgcYoutubeMetadata* pMetadata) const
{
	rtry
	{
		rcheck(nSizeOfData > 0, catchall, gnetError("ReadYoutubeMetadata :: Invalid data size!"));

		// skip backwards from the end until we reach the closing brace
		unsigned nSizeOfJson = nSizeOfData;
		while(nSizeOfJson > 0 && pData[nSizeOfJson - 1] != '}')
			--nSizeOfJson;

		rcheck(nSizeOfJson > 0, catchall, gnetError("ReadYoutubeMetadata :: Invalid json data size!"));

		rcheck(RsonReader::ValidateJson(static_cast<const char*>(pData), nSizeOfJson), catchall, gnetError("ReadYoutubeMetadata :: Invalid JSON"));

		// initialise reader
		RsonReader tReader, tValueReader;
		rcheck(tReader.Init(pData, 0, nSizeOfData), catchall, gnetError("ReadYoutubeMetadata :: Failed to initialise reader"));

		// read data
		if (tReader.GetMember("vid", &tValueReader))
			tValueReader.AsString(pMetadata->szVideoID);
		if (tReader.GetMember("dur", &tValueReader))
			tValueReader.AsUns(pMetadata->nDuration_ms);
		if (tReader.GetMember("size", &tValueReader))
			tValueReader.AsUns64(pMetadata->nSize);
		if (tReader.GetMember("qs", &tValueReader))
			tValueReader.AsUns(pMetadata->nQualityScore);
		if (tReader.GetMember("trackId", &tValueReader))
			tValueReader.AsUns(pMetadata->nTrackId);
		if (tReader.GetMember("fnhash", &tValueReader))
			tValueReader.AsUns(pMetadata->nFilenameHash);
		if (tReader.GetMember("modified", &tValueReader))
			tValueReader.AsBool(pMetadata->nModdedContent);
		if (tReader.GetMember("thmd", &tValueReader))
			tValueReader.AsString(pMetadata->szThumbnailDefaultUrl);
		if (tReader.GetMember("thmm", &tValueReader))
			tValueReader.AsString(pMetadata->szThumbnailMediumUrl);
		if (tReader.GetMember("thml", &tValueReader))
			tValueReader.AsString(pMetadata->szThumbnailLargeUrl);

		return true;
	}
	rcatchall
	{
#if !__NO_OUTPUT
		// log to file
		fiStream* pStream(ASSET.Create("Youtube_ValidationFail.txt", ""));
		if(pStream)
		{
			pStream->Write(pData, nSizeOfData);
			pStream->Close();
		}
#endif
		return false;
	}
}

bool CUserContentManager::WriteYoutubeMetadata(RsonWriter& rw, const rlUgcYoutubeMetadata* pMetadata) const
{
	rtry
	{
		rcheck(rw.Begin(NULL, NULL), catchall, );

		if (pMetadata->szVideoID) rcheck(rw.WriteString("vid", pMetadata->szVideoID), catchall, );
		rcheck(rw.WriteUns("dur", pMetadata->nDuration_ms), catchall, );
		rcheck(rw.WriteUns64("size", pMetadata->nSize), catchall, );
		rcheck(rw.WriteUns("qs", pMetadata->nQualityScore), catchall, );
		rcheck(rw.WriteUns("trackId", pMetadata->nTrackId), catchall, );
		rcheck(rw.WriteUns("fnhash", pMetadata->nFilenameHash), catchall, );
		rcheck(rw.WriteBool("modified", pMetadata->nModdedContent), catchall, );

		if (pMetadata->szThumbnailDefaultUrl) rcheck(rw.WriteString("thmd", pMetadata->szThumbnailDefaultUrl), catchall, );
		if (pMetadata->szThumbnailMediumUrl) rcheck(rw.WriteString("thmm", pMetadata->szThumbnailMediumUrl), catchall, );
		if (pMetadata->szThumbnailLargeUrl) rcheck(rw.WriteString("thml", pMetadata->szThumbnailLargeUrl), catchall, );

		rcheck(rw.End(), catchall, );

		return true;
	}
	rcatchall
	{
		return false;
	}
}

void CUserContentManager::ClearCompletedRequests()
{
	// clear completed requests
	int nRequests = m_RequestList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		UgcRequestNode* pNode = m_RequestList[i];

		// clear out this node if indicated
		if(pNode->m_bFlaggedToClear)
		{
			gnetDebug1("[%d] ClearCompletedRequests :: Time: %d", pNode->m_nRequestID, sysTimer::GetSystemMsTime() - pNode->m_nTimeStarted);

			// delete the element
			m_RequestList.Delete(i);
			--i;
			--nRequests;

			// delete the node data
			delete pNode;
		}
	}
}

bool CUserContentManager::HasRequestFinished(UgcRequestID nRequestID)
{
	int nRequests = m_RequestList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(m_RequestList[i]->m_nRequestID == nRequestID)
			return !m_RequestList[i]->m_MyStatus.Pending();
	}

	// not here, assume finished
	return true;
}

bool CUserContentManager::IsRequestPending(UgcRequestID nRequestID)
{
	int nRequests = m_RequestList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(m_RequestList[i]->m_nRequestID == nRequestID)
			return m_RequestList[i]->m_MyStatus.Pending();
	}

	// not here, assume finished
	return false;
}

bool CUserContentManager::DidRequestSucceed(UgcRequestID nRequestID)
{
	int nRequests = m_RequestList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(m_RequestList[i]->m_nRequestID == nRequestID)
			return m_RequestList[i]->m_MyStatus.Succeeded();
	}

	// not here, assume it didn't
	return false;
}

void* CUserContentManager::GetRequestData(UgcRequestID nRequestID)
{
	int nRequests = m_RequestList.GetCount();
	for(int i = 0; i < nRequests; i++)
	{
		if(m_RequestList[i]->m_nRequestID == nRequestID)
			return m_RequestList[i]->GetRequestData();
	}

	// not here, no data
	return NULL;
}

bool CUserContentManager::RequestUrlTemplates(const int localGamerIndex)
{
	gnetDebug1("RequestUrlTemplates");

	rtry
	{
		rcheck(sm_bUseCdnTemplate, catchall, gnetDebug1("RequestUrlTemplates skipped - disabled"));
		rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetDebug1("RequestUrlTemplates skipped - invalid user"));
		rcheck(m_UrlTemplatesRequestStatus.None(), catchall, gnetDebug1("RequestUrlTemplates skipped - in progress or done"));

		const unsigned flags = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LOCATOR_FLAGS", 0x77A262E2), (unsigned)rlUgcCdnContentFlags::CF_Decrypt);

		char urlTemplatesFileUrl[RL_MAX_URL_BUF_LENGTH];
		formatf(
			urlTemplatesFileUrl,
			"https://%s-locator-cloud.rockstargames.com/titles/%s/%s/template.json",
			g_rlTitleId->m_RosTitleId.GetEnvironmentName(),
			g_rlTitleId->m_RosTitleId.GetTitleName(),
			g_rlTitleId->m_RosTitleId.GetPlatformName());

		m_UrlTemplates.clear();

		return rlUgc::GetUrlTemplates(
			localGamerIndex,
			urlTemplatesFileUrl,
			(rlUgcCdnContentFlags)flags,
			GetUserContentRequestAllocator(),
			m_UrlTemplates,
			m_UrlTemplatesRequestStatus);
	}
	rcatchall
	{
	}

	return false;
}


void CUserContentManager::SetUseCdnTemplate(bool useTemplate)
{
	if (sm_bUseCdnTemplate != useTemplate)
	{
		gnetDebug1("UseCdnTemplate %s", LogBool(useTemplate));
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

		if (useTemplate && RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlRos::GetCredentials(localGamerIndex).IsValid()
			&& Tunables::IsInstantiated() && Tunables::GetInstance().HasCloudRequestFinished())
		{
			UserContentManager::GetInstance().RequestUrlTemplates(localGamerIndex);
		}
	}

	sm_bUseCdnTemplate = useTemplate;
}

#if __BANK

int g_Bank_nQueryTotal = 0;
rlUgc::QueryContentDelegate g_Bank_QueryDelegate;

void Bank_QueryContentDelegate(const int nIndex,
						  const char* szContentID,
						  const rlUgcMetadata* pMetadata,
						  const rlUgcRatings* UNUSED_PARAM(pRatings),
						  const char* UNUSED_PARAM(szStatsJSON),
						  const unsigned UNUSED_PARAM(nPlayers),
						  const unsigned UNUSED_PARAM(nPlayerIndex),
						  const rlUgcPlayer* UNUSED_PARAM(pPlayer))
{
	gnetDebug1("Bank_QueryContentDelegate :: [%d] ID: %s, Title: %s, Metadata: %s", nIndex, szContentID, pMetadata->GetContentName(), pMetadata->GetData());
}

void CUserContentManager::InitWidgets(rage::bkBank* pBank)
{
	snprintf(m_szContentFileName, kMAX_NAME, "");
	snprintf(m_szContentDisplayName, kMAX_NAME, "My Awesome Mission!");
	snprintf(m_szDataFileName, kMAX_NAME, ".json");
	snprintf(m_szContentID, kMAX_NAME, "");
    safecpy(m_szUrlTemplatesFile, "common:/ugcUrlTemplates.json");

	g_Bank_QueryDelegate.Bind(&Bank_QueryContentDelegate);

	pBank->PushGroup("UGC");
		pBank->AddText("Content Filename", m_szContentFileName, kMAX_NAME, NullCB);
		pBank->AddText("Content Display Name", m_szContentDisplayName, kMAX_NAME, NullCB);
		pBank->AddText("Data File Name", m_szDataFileName, kMAX_NAME, NullCB);
		pBank->AddButton("Create Test Content", datCallback(MFA(CUserContentManager::Bank_Create), this));
        pBank->AddButton("Validate JSON", datCallback(MFA(CUserContentManager::Bank_ValidateJSON), this));
        pBank->AddSeparator();
		pBank->AddText("Content ID", m_szContentID, kMAX_NAME, NullCB);
		pBank->AddButton("Request By Content ID", datCallback(MFA(CUserContentManager::Bank_GetContentByID), this));
		pBank->AddButton("Request By Content IDs", datCallback(MFA(CUserContentManager::Bank_GetContentByIDs), this));
		pBank->AddButton("Request My Content", datCallback(MFA(CUserContentManager::Bank_GetMyContent), this));
		pBank->AddButton("Request Friend Content", datCallback(MFA(CUserContentManager::Bank_GetFriendContent), this));
		pBank->AddButton("Request Crew Content", datCallback(MFA(CUserContentManager::Bank_GetClanContent), this));
		pBank->AddButton("Request Rockstar Created", datCallback(MFA(CUserContentManager::Bank_GetByCategory), this));
		pBank->AddButton("Request Most Recently Created", datCallback(MFA(CUserContentManager::Bank_GetMostRecentlyCreated), this));
		pBank->AddButton("Request Top Rated", datCallback(MFA(CUserContentManager::Bank_GetTopRated), this));
		pBank->AddButton("Clear Results", datCallback(MFA(CUserContentManager::Bank_ClearQueryResults), this));
		pBank->AddSeparator();
		pBank->AddButton("Save Rockstar Created", datCallback(MFA(CUserContentManager::Bank_SaveRockstarCreated), this));
		pBank->AddButton("Save Rockstar Verified", datCallback(MFA(CUserContentManager::Bank_SaveRockstarVerified), this));
		pBank->AddSeparator();
		pBank->AddButton("Load Rockstar Created", datCallback(MFA(CUserContentManager::Bank_LoadRockstarCreated), this));
		pBank->AddButton("Load Rockstar Verified", datCallback(MFA(CUserContentManager::Bank_LoadRockstarVerified), this));
		pBank->AddButton("Clear Offline", datCallback(MFA(CUserContentManager::Bank_ClearOfflineQueryData), this));
		pBank->AddSeparator();
		pBank->AddToggle("Simulate no UGCWRITE privilege", &m_sbSimulateNoUgcWritePrivilege);
		pBank->AddSeparator();
		pBank->AddButton("Test Youtube Metadata", datCallback(MFA(CUserContentManager::Bank_TestYoutubeMetadata), this));
		pBank->AddButton("Create Youtube Content", datCallback(MFA(CUserContentManager::Bank_CreateYoutubeContent), this));
		pBank->AddButton("Query Youtube Content", datCallback(MFA(CUserContentManager::Bank_QueryYoutubeContent), this));
		pBank->AddSeparator();
		pBank->AddText("Url Templates File", m_szUrlTemplatesFile, RAGE_MAX_PATH, NullCB);
		pBank->AddButton("Load Url Templates From File", datCallback(MFA(CUserContentManager::Bank_LoadUrlTemplateFile), this));
		pBank->AddButton("Request Latest Url Templates", datCallback(MFA(CUserContentManager::Bank_RequestUrlTemplates), this));
        pBank->AddButton("Download Credits", datCallback(MFA(CUserContentManager::Bank_DownloadCredits), this));
	pBank->PopGroup();
}

void CUserContentManager::Bank_Create()
{
	if(!NetworkInterface::IsCloudAvailable())
		return;

	// build relative path
	static const unsigned kMAX_PATH = 256;
	char szRelPath[kMAX_PATH];
	snprintf(szRelPath, kMAX_PATH, "share/gta5/ugc/missions5/%s", m_szContentFileName);

	// build local file path
	char szDataPath[kMAX_PATH];
	snprintf(szDataPath, kMAX_PATH, "common:/data/ugc/%s", m_szDataFileName);

	// open the data file
	fiStream* pStream = ASSET.Open(szDataPath, "");
	if(!pStream)
		return;

	// allocate data to read the file
	unsigned nDataSize = pStream->Size();
	char* pDataJSON = rage_new char[nDataSize + 1];

	// read out the contents
	pStream->Read(pDataJSON, nDataSize);

    pDataJSON[nDataSize] = '\0';

    // build source file info
	char szCloudPath[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
    CUserContentManager::GetAbsPathFromRelPath(CSavegameFilenames::GetFilenameOfCloudFile(), szRelPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS);

    rlUgc::SourceFileInfo srcFile;
    srcFile.m_FileId = 0;
    formatf(srcFile.m_CloudAbsPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS, szCloudPath);

	const char* szTagCSV = NULL;

	// call into Create
	Create(m_szContentDisplayName, "Description", &srcFile, 1, szTagCSV, pDataJSON, RLUGC_CONTENT_TYPE_GTA5MISSION, true);

	// punt data
	delete[] pDataJSON; 
	pStream->Close();
}

void CUserContentManager::Bank_ValidateJSON()
{
    if(!NetworkInterface::IsCloudAvailable())
        return;

    // build local file path
    static const unsigned kMAX_PATH = 256;
    char szDataPath[kMAX_PATH];
    snprintf(szDataPath, kMAX_PATH, "common:/data/ugc/%s", m_szDataFileName);

    // open the data file
    fiStream* pStream = ASSET.Open(szDataPath, "");
    if(!pStream)
        return;

    // allocate data to read the file
    unsigned nDataSize = pStream->Size();
    char* pDataJSON = rage_new char[nDataSize];

    // read out the contents
    pStream->Read(pDataJSON, nDataSize);

    // validate the JSON file
    RsonReader::ValidateJson(static_cast<const char*>(pDataJSON), nDataSize);

    // punt data
    delete[] pDataJSON; 
    pStream->Close();
}

void CUserContentManager::Bank_GetContentByID()
{
	char szContentIDs[1][RLUGC_MAX_CONTENTID_CHARS];
	formatf(szContentIDs[0], RLUGC_MAX_CONTENTID_CHARS, m_szContentID);

	// retrieve content
	GetByContentID(szContentIDs, 1, false, RLUGC_CONTENT_TYPE_GTA5MISSION);
}

void CUserContentManager::Bank_GetContentByIDs()
{
	char szContentIds[CUserContentManager::MAX_REQUESTED_CONTENT_IDS][RLUGC_MAX_CONTENTID_CHARS];
	{
		unsigned idx = 0;
		// 0
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		//10
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		//20
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		//30
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		//40
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		//50
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		// 60
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
		safecpy(szContentIds[idx++], "qwSIyw_qTkGKuCYxiCjyww", RLUGC_MAX_CONTENTID_CHARS);
	};

	// retrieve content
	GetByContentID(szContentIds, CUserContentManager::MAX_REQUESTED_CONTENT_IDS, true, RLUGC_CONTENT_TYPE_GTA5MISSION);
}

void CUserContentManager::Bank_GetMyContent()
{
	// retrieve content
	bool bPublished = true; 
	bool bOpen = true; 
	GetMyContent(0, 0, RLUGC_CONTENT_TYPE_GTA5MISSION, &bPublished, -1, &bOpen);
}

void CUserContentManager::Bank_GetFriendContent()
{
	// retrieve content
	GetFriendContent(0, 0, RLUGC_CONTENT_TYPE_GTA5MISSION);
}

void CUserContentManager::Bank_GetClanContent()
{
	// retrieve content
	const rlClanDesc* pClan = CLiveManager::GetNetworkClan().GetPrimaryClan();
	if(!pClan)
	{
		gnetError("Bank_GetClanContent :: Local player has no primary clan!");
		return;
	}

	GetClanContent(pClan->m_Id, 0, 0, RLUGC_CONTENT_TYPE_GTA5MISSION);
}

void CUserContentManager::Bank_GetByCategory()
{
	// retrieve content
	GetByCategory(RLUGC_CATEGORY_ROCKSTAR_CREATED, 0, 0, RLUGC_CONTENT_TYPE_GTA5MISSION, eSort_NotSpecified, false);
}

void CUserContentManager::Bank_GetMostRecentlyCreated()
{
	// retrieve content
	GetMostRecentlyCreated(0, 0, RLUGC_CONTENT_TYPE_GTA5MISSION, NULL);
}

void CUserContentManager::Bank_GetTopRated()
{
	// retrieve content
	GetTopRated(0, 0, RLUGC_CONTENT_TYPE_GTA5MISSION);
}

void CUserContentManager::Bank_ClearQueryResults()
{
	ResetContentQuery();
}

void CUserContentManager::OnContentResultToSave(const int UNUSED_PARAM(nIndex),
												const char* szContentID,
												const rlUgcMetadata* pMetadata,
												const rlUgcRatings* UNUSED_PARAM(pRatings),
												const char* UNUSED_PARAM(szStatsJSON),
												const unsigned UNUSED_PARAM(nPlayers),
												const unsigned UNUSED_PARAM(nPlayerIndex),
												const rlUgcPlayer* UNUSED_PARAM(pPlayer))
{
	if(!m_pSaveStream)
	{
		static const unsigned kMAX_PATH = 256;
		char szFileName[kMAX_PATH];
		snprintf(szFileName, kMAX_PATH, "common:/data/ugc/UGC_%s.ugc", rlUgcCategoryToString(m_nCurrentCategory));

		// open UGC content file
		m_pSaveStream = ASSET.Create(szFileName, "");

		// check the stream was created
		if(!gnetVerifyf(m_pSaveStream, "OnContentResultToSave :: Cannot create stream! Check common:/data/ugc/ is valid!"))
			return;

        // write the offline content version 
        int nVersion = OFFLINE_CONTENT_VERSION;
        m_pSaveStream->WriteInt(&nVersion, 1);

		// write the total content number
		m_pSaveStream->WriteInt(&m_nContentOfflineTotal, 1);

		// write string constants - we write these so that we can avoid changes taking out the offline content
		int nConstant;
		nConstant = RLUGC_MAX_CONTENTID_CHARS;		m_pSaveStream->WriteInt(&nConstant, 1);
		nConstant = RLUGC_MAX_CLOUD_ABS_PATH_CHARS;	m_pSaveStream->WriteInt(&nConstant, 1);
		nConstant = RLUGC_MAX_CONTENT_NAME_CHARS;	m_pSaveStream->WriteInt(&nConstant, 1);
		nConstant = RLROS_MAX_USERID_SIZE;			m_pSaveStream->WriteInt(&nConstant, 1);
		nConstant = RLROS_MAX_USER_NAME_SIZE;		m_pSaveStream->WriteInt(&nConstant, 1);

		// add logging for content total
		gnetDebug1("OnContentResultToSave :: Content Total is %d", m_nContentOfflineTotal);
	}

	// grab data values
	PlayerAccountId nAccountId = pMetadata->GetAccountId();
	unsigned nDescriptionLength = pMetadata->GetDescriptionLength();
	unsigned nDataSize = pMetadata->GetDataSize();
	RockstarId nRockstarId = pMetadata->GetRockstarId();

	// write content info
	m_pSaveStream->WriteByte(szContentID, RLUGC_MAX_CONTENTID_CHARS);
	m_pSaveStream->WriteInt(&nAccountId, 1);
	m_pSaveStream->WriteByte(pMetadata->GetContentName(), RLUGC_MAX_CONTENT_NAME_CHARS);
	m_pSaveStream->WriteInt(&nDescriptionLength, 1);
	m_pSaveStream->WriteByte(pMetadata->GetDescription(), pMetadata->GetDescriptionLength());
	m_pSaveStream->WriteInt(&nDataSize, 1);
	m_pSaveStream->WriteByte(pMetadata->GetData(), pMetadata->GetDataSize());
	m_pSaveStream->WriteLong(&nRockstarId, 1);
	m_pSaveStream->WriteByte(pMetadata->GetUserId(), RLROS_MAX_USERID_SIZE);
	m_pSaveStream->WriteByte(pMetadata->GetUsername(), RLROS_MAX_USER_NAME_SIZE);

    int nFiles = pMetadata->GetNumFiles();
    m_pSaveStream->WriteInt(&nFiles, 1);

    static const int UGC_MISSION_DATA = 0;
    static const int UGC_IMAGE_DATA = 1;

    // write cloud path using composed path from metadata
    for(int i = 0; i < nFiles; i++)
    {
        // compose the cloud paths and write these into the data
        char szPath[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
        pMetadata->ComposeCloudAbsPath(pMetadata->GetFileId(i), szPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS);
        m_pSaveStream->WriteByte(szPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS);

        // add a request for the mission content (smash server time)
        if((i == UGC_MISSION_DATA) || (i == UGC_IMAGE_DATA && PARAM_ugcUseOfflineImages.Get()))
        {
            sDataRequest tRequest;
            formatf(tRequest.m_szLocalPath, sDataRequest::MAX_LOCAL_PATH, "%s_%02d", pMetadata->GetContentId(), i);
            tRequest.m_nContentType = pMetadata->GetContentType();
            safecpy(tRequest.m_szContentID, pMetadata->GetContentId());
            tRequest.m_nFileID = pMetadata->GetFileId(i);
            tRequest.m_nFileVersion = pMetadata->GetFileVersion(i);
            tRequest.m_nLanguage = pMetadata->GetLanguage();

            // add
            m_DataRequests.PushAndGrow(tRequest);
        }
    }
}

void CUserContentManager::OnCloudEvent(const CloudEvent* pEvent)
{
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
		return;

	// grab event data
	const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

	// check if we're interested
	int nIndex = m_DataRequests.Find(pEventData->nRequestID);
	if(nIndex < 0)
		return;

    if(pEventData->nDataSize == 0)
    {
        gnetDebug1("OnCloudEvent :: Skipping %s. Not found!", m_DataRequests[nIndex].m_szLocalPath);
    }
    else
    {
        // file name is content name
        static const unsigned kMAX_PATH = 256;
        char szFileName[kMAX_PATH];
        snprintf(szFileName, kMAX_PATH, "common:/data/ugc/%s.ugc", m_DataRequests[nIndex].m_szLocalPath);

        // save file
        fiStream* pSaveStream = ASSET.Create(szFileName, "");
        pSaveStream->Write(pEventData->pData, pEventData->nDataSize);
        pSaveStream->Close(); 
    }

	// remove the data request
	m_DataRequests.Delete(nIndex);
}

void CUserContentManager::Bank_SaveCategory(rlUgcCategory nCategory)
{
	// check we're clear
	if(!gnetVerifyf(!m_QueryToSaveStatus.Pending(), "SaveCategory :: (%s) Query still ongoing!", rlUgcCategoryToString(nCategory)))
		return;
	if(!gnetVerifyf(m_pSaveStream == NULL, "SaveCategory :: (%s) Stream still open!", rlUgcCategoryToString(nCategory)))
		return;

	// build query
	char szQueryParams[128];
	formatf(szQueryParams, sizeof(szQueryParams), "{category:\"%s\"}", rlUgcCategoryToString(nCategory));

	bool bSuccess = QueryContent(RLUGC_CONTENT_TYPE_GTA5MISSION,
								 "GetContentByCategory",
                                 szQueryParams,
								 0, 
								 MAX_QUERY_COUNT, 
								 &m_nContentOfflineTotal,
                                 NULL,
								 m_QueryToSaveDelegate,
								 &m_QueryToSaveStatus);

	if(!gnetVerifyf(bSuccess, "SaveCategory :: (%s) Query failed!", rlUgcCategoryToString(nCategory)))
		return;

	// stash category
	m_nCurrentCategory = nCategory;
	m_QueryToSaveOp = eOp_Pending;
}

void CUserContentManager::Bank_SaveRockstarCreated()
{
	Bank_SaveCategory(RLUGC_CATEGORY_ROCKSTAR_CREATED);
}

void CUserContentManager::Bank_SaveRockstarVerified()
{
	Bank_SaveCategory(RLUGC_CATEGORY_ROCKSTAR_VERIFIED);
}

void CUserContentManager::Bank_LoadRockstarCreated()
{
	LoadOfflineQueryData(RLUGC_CATEGORY_ROCKSTAR_CREATED);
}

void CUserContentManager::Bank_LoadRockstarVerified()
{
	LoadOfflineQueryData(RLUGC_CATEGORY_ROCKSTAR_VERIFIED);
}

void CUserContentManager::Bank_ClearOfflineQueryData()
{
	ClearOfflineQueryData();
}

void CUserContentManager::Bank_TestYoutubeMetadata()
{
	if(!NetworkInterface::IsCloudAvailable())
		return;

	// build local file path
	static const unsigned kMAX_PATH = 256;
	char szDataPath[kMAX_PATH];
	snprintf(szDataPath, kMAX_PATH, "common:/data/ugc/%s", m_szDataFileName);

	// open the data file
	fiStream* pStream = ASSET.Open(szDataPath, "");
	if(!pStream)
		return;

	// allocate data to read the file
	unsigned nDataSize = pStream->Size();
	char* pDataJSON = rage_new char[nDataSize];

	// read out the contents
	pStream->Read(pDataJSON, nDataSize);

	// read data
	rlUgcYoutubeMetadata tMetaData;
	if(!PopulateYoutubeMetadata(static_cast<const char*>(pDataJSON), nDataSize, &tMetaData))
	{
		gnetError("Bank_TestYoutubeMetadata :: Failed to read Youtube response!");
	}
	else
	{
		gnetDebug1("Bank_TestYoutubeMetadata :: VideoID: %s", tMetaData.szVideoID);
		gnetDebug1("Bank_TestYoutubeMetadata :: Duration (seconds): %ds", tMetaData.nDuration_s);
		gnetDebug1("Bank_TestYoutubeMetadata :: Size: %" I64FMT "d", tMetaData.nSize);
		gnetDebug1("Bank_TestYoutubeMetadata :: Thumbnail Default Url: %s", tMetaData.szThumbnailDefaultUrl);
		gnetDebug1("Bank_TestYoutubeMetadata :: Thumbnail Medium Url: %s", tMetaData.szThumbnailMediumUrl);
		gnetDebug1("Bank_TestYoutubeMetadata :: Thumbnail High Url:  %s", tMetaData.szThumbnailLargeUrl);
	}

	delete[] pDataJSON; 
	pStream->Close();

	// test write / read
	{
		// create metadata - work out size of buffer
		RsonWriter rsonWriter;
		rsonWriter.InitNull(RSON_FORMAT_JSON);
		if(!WriteYoutubeMetadata(rsonWriter, &tMetaData))
		{
			gnetError("Bank_TestYoutubeMetadata :: Failed to write metadata!");
			return;
		}

		unsigned nDataJsonLength = rsonWriter.Length() + 1;
		char* pRlDataJson = Alloca(char, nDataJsonLength);
		if(!pRlDataJson)
		{
			gnetError("Bank_TestYoutubeMetadata :: Failed to allocate metadata buffer!");
			return;
		}

		rsonWriter.Init(pRlDataJson, nDataJsonLength, RSON_FORMAT_JSON);
		if(!WriteYoutubeMetadata(rsonWriter, &tMetaData))
		{
			gnetError("Bank_TestYoutubeMetadata :: Failed to write metadata!");
			return;
		}

		rlUgcYoutubeMetadata tReadMetaData;
		if(!ReadYoutubeMetadata(pRlDataJson, nDataJsonLength, &tReadMetaData))
		{
			gnetError("Bank_TestYoutubeMetadata :: Failed to read data!");
		}
		else
		{
			gnetDebug1("Bank_TestYoutubeMetadata :: VideoID: %s", tMetaData.szVideoID);
			gnetDebug1("Bank_TestYoutubeMetadata :: Duration (seconds): %ds", tMetaData.nDuration_s);
			gnetDebug1("Bank_TestYoutubeMetadata :: Size: %" I64FMT "d", tMetaData.nSize);
			gnetDebug1("Bank_TestYoutubeMetadata :: Thumbnail Default Url: %s", tMetaData.szThumbnailDefaultUrl);
			gnetDebug1("Bank_TestYoutubeMetadata :: Thumbnail Medium Url: %s", tMetaData.szThumbnailMediumUrl);
			gnetDebug1("Bank_TestYoutubeMetadata :: Thumbnail High Url:  %s", tMetaData.szThumbnailLargeUrl);
		}
	}
}

void CUserContentManager::Bank_CreateYoutubeContent()
{
	if(!NetworkInterface::IsCloudAvailable())
		return;

	// build local file path
	static const unsigned kMAX_PATH = 256;
	char szDataPath[kMAX_PATH];
	snprintf(szDataPath, kMAX_PATH, "common:/data/ugc/%s", m_szDataFileName);

	// open the data file
	fiStream* pStream = ASSET.Open(szDataPath, "");
	if(!pStream)
		return;

	// allocate data to read the file
	unsigned nDataSize = pStream->Size();
	char* pDataJSON = rage_new char[nDataSize];

	// read out the contents
	pStream->Read(pDataJSON, nDataSize);

	// read data
	rlUgcYoutubeMetadata tMetaData;
	if(!PopulateYoutubeMetadata(static_cast<const char*>(pDataJSON), nDataSize, &tMetaData))
	{
		gnetError("Bank_CreateYoutubeContent :: Failed to read data!");
	}
	else
	{
		gnetDebug1("Bank_CreateYoutubeContent :: VideoID: %s", tMetaData.szVideoID);
		gnetDebug1("Bank_CreateYoutubeContent :: Duration (seconds): %ds", tMetaData.nDuration_s);
		gnetDebug1("Bank_CreateYoutubeContent :: Thumbnail Default Url: %s", tMetaData.szThumbnailDefaultUrl);
		gnetDebug1("Bank_CreateYoutubeContent :: Thumbnail Medium Url: %s", tMetaData.szThumbnailMediumUrl);
		gnetDebug1("Bank_CreateYoutubeContent :: Thumbnail High Url:  %s", tMetaData.szThumbnailLargeUrl);

		// test creation
		if(CreateYoutubeContent("Test", "Title", "Tag1,Tag2", &tMetaData, true, NULL) == INVALID_UGC_REQUEST_ID)
		{
			gnetError("Bank_CreateYoutubeContent :: Failed to create youtube content!");
		}
	}

	// punt data
	delete[] pDataJSON; 
	pStream->Close();
}

void CUserContentManager::Bank_QueryYoutubeContent()
{
	// retrieve content
	gnetDebug1("Bank_QueryYoutubeContent");
	QueryYoutubeContent(0, MAX_QUERY_COUNT, true, &g_Bank_nQueryTotal, g_Bank_QueryDelegate, NULL);
}

void CUserContentManager::Bank_LoadUrlTemplateFile()
{
    fiStream* fileStream = fiStream::Open(m_szUrlTemplatesFile, true);
    if (!gnetVerify(fileStream)) return;

    const int fileSize = fileStream->Size();
    if (!gnetVerifyf(fileSize >= 0, "Error reading the size of file '%s'", m_szUrlTemplatesFile)) return;

    char* buffer = (char*)alloca(fileSize + 2);
    sysMemSet(buffer, 0, fileSize + 2);

    fileStream->ReadByte(buffer, fileSize);

    fileStream->Close();

    RsonReader templates;
    if (!gnetVerify(templates.Init(buffer, fileSize))) return;
    if (!gnetVerify(templates.GetMember(rlUgcUrlTemplate::TEMPLATE_ROOT, &templates))) return;
    gnetVerify(rlUgcCreateUrlTemplatesFromJson(templates, m_UrlTemplates));
}

void CUserContentManager::Bank_RequestUrlTemplates()
{
    gnetDebug1("Bank_RequestUrlTemplates");
    sm_bUseCdnTemplate = true;
    if (m_UrlTemplatesRequestStatus.Finished())
    {
        m_UrlTemplatesRequestStatus.Reset();
    }
    RequestUrlTemplates(NetworkInterface::GetLocalGamerIndex());
}

void CUserContentManager::Bank_DownloadCredits()
{
    gnetDebug1("Bank_DownloadCredits");
    RequestCdnContentData("Credits");
}

#endif
