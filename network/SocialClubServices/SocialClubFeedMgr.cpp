#include "SocialClubFeedMgr.h"

#if GEN9_STANDALONE_ENABLED

// Rage
#include "rline/rldiag.h"
#include "rline/rlsystemui.h"
#include "rline/scfriends/rlscfriends.h"
#include "rline/scauth/rlscauth.h"
#include "system/bit.h"
#include "system/param.h"
#include "file/device.h"
#include "parsercore/streamxml.h"
#include "profile/rocky.h"

// Framework
#include "fwlocalisation/languagepack.h"

// Game
#include "Network/Cloud/CloudManager.h"
#include "Network/Crews/NetworkCrewDataMgr.h"
#include "network/Crews/NetworkCrewEmblemMgr.h"

#include "game/localisation.h"
#include "event/EventNetwork.h"
#include "event/EventGroup.h"

NETWORK_OPTIMISATIONS()

#undef __net_channel
#define __net_channel net_feed
RAGE_DEFINE_SUBCHANNEL(net, feed);


PARAM(noSCFeed, "Disable SC Feeds");

#if TEST_FAKE_FEED
NOSTRIP_PARAM(scDebugFeed, "Load the feed from the text file.");
NOSTRIP_PARAM(scDebugFeedLanding, "Load the landing page from the text file.");
NOSTRIP_PARAM(scDebugFeedPriority, "Load the priority feed from the text file.");
NOSTRIP_PARAM(scDebugFeedSeries, "Load the series feed from the text file.");
NOSTRIP_PARAM(scDebugFeedHeist, "Load the heist feed from the text file.");
NOSTRIP_PARAM(scFeedForce4K, "Download high resolution images");
NOSTRIP_PARAM(scFeedShowAllCharacters, "Shows all strings as they are even if the char is missing in the font");
NOSTRIP_PARAM(scFeedNoReorder, "Dont reorder the post to better fit the UI columns");
PARAM(scFeedFromCloud, "Read all feed data from a cloud file");
NOSTRIP_PARAM(scCloudLanding, "Specify the file to use");
#endif

const unsigned ScFeedTextureDownload::IMAGE_DOWNLOAD_RETRY_MS = 2500;
const unsigned ScFeedTextureDownload::DEFAULT_DOWNLOAD_RETRIES = 2;
const unsigned ScFeedRecentDownload::VALID_TIME_SEC = 60 * 5;

ScFeedRecentDownloads ScFeedTextureDownload::s_RecentDownloads;
ScFeedTextureDownloads ScFeedContent::s_TextureDownloads;
atArray<ScFeedContent*> ScFeedContent::s_UpdatingContent;


//It's impossible to have the worst case for everything on screen so I only check for an AVERAGE_TEXTURES_PER_FEED.
//photo_agg posts are very large and posse posts are the only ones with up to 7 images.
//If we hit the limit we'll simply stop loading additional images.
#define AVERAGE_TEXTURES_PER_FEED 3

// Important: width and height have to be a multiple of 4 otherwise the validation check fails (-debugRuntime=2 -validateGfx = 1 -d3dbreakonlist = 104)
const char* ScFeedContentDownloadInfo::RESIZE_LANDING_POST_TILE_HD = "?resize=404:228";
const char* ScFeedContentDownloadInfo::RESIZE_LANDING_POST_BIGIMAGE_HD = "?resize=1400:788";

bool ScFeedContentDownloadInfo::s_Use4KImages = false;

//#####################################################################################

ScFeedRecentDownload::ScFeedRecentDownload()
    :m_Time(0)
    ,m_Result(FeedTextureDownloadState::None)
{

}

ScFeedRecentDownload::ScFeedRecentDownload(const atHashString id, const FeedTextureDownloadState result)
    : m_Time(rlGetPosixTime())
    , m_Id(id)
    , m_Result(result)
{

}

bool ScFeedRecentDownload::IsFresh() const
{
    return (rlGetPosixTime() - m_Time) < ScFeedRecentDownload::VALID_TIME_SEC;
}

//#####################################################################################

ScFeedTextureDownload::ScFeedTextureDownload()
    :m_State(FeedTextureDownloadState::None)
    ,m_TexRequestHandle(CTextureDownloadRequest::INVALID_HANDLE)
    ,m_TextureDictionarySlot(strLocalIndex(-1))
    ,m_Retries(0)
    ,m_NextRetryTime(0)
    ,m_RefCount(0)
{

}

ScFeedTextureDownload::~ScFeedTextureDownload()
{
    gnetAssertf(m_RefCount == 0, "Our manual ref count has failed [%u]", m_RefCount);
}

void ScFeedTextureDownload::AddRef()
{
    gnetAssertf(sysThreadType::IsUpdateThread(), "This ref counter isn't thread safe");

    ++m_RefCount;
}

void ScFeedTextureDownload::RemoveRef()
{
    gnetAssertf(sysThreadType::IsUpdateThread(), "This ref counter isn't thread safe");

    if (gnetVerifyf(m_RefCount > 0, "You called RemoveRef more often than addref"))
    {
        --m_RefCount;
    }

    if (m_RefCount == 0)
    {
        Clear();
    }
}

void ScFeedTextureDownload::Clear()
{
    gnetDebug3("ScFeedTextureDownload - Clear url[%s] cache[%s] handle[%d]", m_Url.c_str(), m_CacheHash.TryGetCStr(), m_TexRequestHandle);

    ClearDownload();
    ClearTexture();

    m_Retries = 0;
    m_NextRetryTime = 0;

    m_Url.Clear();
    m_CacheHash.Clear();

    m_TexRequestDesc = CTextureDownloadRequestDesc();

    m_State = FeedTextureDownloadState::None;
}

void ScFeedTextureDownload::Update()
{
    if (m_State != FeedTextureDownloadState::Downloading)
    {
        return;
    }

    if (m_NextRetryTime > 0)
    {
        const unsigned timeNow = fwTimer::GetSystemTimeInMilliseconds();
        if (timeNow >= m_NextRetryTime)
        {
            gnetDebug1("ScFeedTextureDownload - Retrying url[%s] cache[%s]", m_Url.c_str(), m_CacheHash.TryGetCStr());

            DoDownload();
            m_NextRetryTime = 0;
        }

        return;
    }

    if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_TexRequestHandle))
    {
        const int failureResultCode = DOWNLOADABLETEXTUREMGR.GetTextureDownloadRequestResultCode(m_TexRequestHandle);

        gnetDebug1("ScFeedTextureDownload - Image download url[%s] cache[%s] handle[%d] result[%d]", m_Url.c_str(), m_CacheHash.TryGetCStr(), m_TexRequestHandle, failureResultCode);

        // Call cleardownload to free up the resource on any error.
        ClearDownload();

        // We retry in case of 200 (ok) or 304 (not modified) because it indicates there was 
        // an issue after the download. Sometimes the decoding can fail due to temporary memory issues.
        const bool retriableError = (failureResultCode >= 500 && failureResultCode < 600) || failureResultCode == 200 || failureResultCode == 304;

        const unsigned maxDownloadRetries = static_cast<unsigned>(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SC_FEED_IMAGE_DOWNLOAD_RETRIES", 0x470F6504), static_cast<int>(DEFAULT_DOWNLOAD_RETRIES)));
        const unsigned retryTimeMs = static_cast<unsigned>(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SC_FEED_IMAGE_DOWNLOAD_RETRY_MS", 0x7700DC3A), static_cast<int>(IMAGE_DOWNLOAD_RETRY_MS)));

        if (retriableError && m_Retries < maxDownloadRetries)
        {
            const unsigned timeNow = fwTimer::GetSystemTimeInMilliseconds();
            m_NextRetryTime = timeNow + (retryTimeMs * (1 + m_Retries));
            ++m_Retries;

            gnetDebug1("ScFeedTextureDownload - waiting to retry [%d] [%s]", m_Retries, m_Url.c_str());
        }
        else
        {
            AddRecentDownload(m_CacheHash.GetHash(), FeedTextureDownloadState::Failed);
            m_State = FeedTextureDownloadState::Failed;
        }

        return;
    }

    if (!DOWNLOADABLETEXTUREMGR.IsRequestReady(m_TexRequestHandle))
    {
        return;
    }

    if (m_TexRequestDesc.m_OnlyCacheAndVerify)
    {
        m_State = FeedTextureDownloadState::Done;
    }
    else
    {
        if (gnetVerifyf(ApplyTxdSlot(), "ScFeedTextureDownload - Failed to get valid txd for feed img [%s]", m_Url.c_str()))
        {
            gnetDebug1("ScFeedTextureDownload - Image [%s] has been applied in TXD slot %d", m_Url.c_str(), m_TextureDictionarySlot.Get());
            m_State = FeedTextureDownloadState::Done;
        }
        else
        {
            m_State = FeedTextureDownloadState::Failed;
        }
    }

    // We can now free up the slot in the DownloadableTextureManager as the texture is allocated and increased the ref in ApplyTxdSlot
    ClearDownload();

    // If we failed or we don't rely on the local cache then we add it to the recent downloads
    // If we prefer the local cache and it didn't fail the checks in the DownloadableTextureManager are enough.
    if (m_State == FeedTextureDownloadState::Failed || (m_TexRequestDesc.m_CloudRequestFlags & eRequestFlags::eRequest_CacheForceCache) == 0)
    {
        AddRecentDownload(m_CacheHash.GetHash(), m_State);
    }
}

bool ScFeedTextureDownload::RequestDownload(const char* url, const char* cacheName, const rlCloudMemberId& memberId, const CTextureDownloadRequestDesc::Type fileType, 
    const bool preferCache, const bool preloadOnly)
{
    if (!gnetVerify(url != nullptr && cacheName != nullptr))
    {
        m_State = FeedTextureDownloadState::Failed;
        return false;
    }

    gnetAssertf(m_RefCount > 0, "Eithere there's a ref counting bug or please don't directly use this class outside of ScFeedContent");

    if (ScFeedLoc::IsLocalImage(url))
    {
        gnetDebug2("ScFeedTextureDownload - RequestDownload url[%s] - it's a local file so there's nothing to do", url);

        m_State = FeedTextureDownloadState::Done;
        return true;
    }

    //Feed posts can contain relative paths starting with "global/" but the cloud code attaches it too.
    //This isn't going to change any time soon so we'll just live with it.
    const char* global = "global/";
    // This is a mini hack as the relative URLs aren't quite returned as rlcloud expects them
    if (strncmp(url, global, strlen(global)) == 0)
    {
        url += strlen(global);
    }

    gnetDebug2("ScFeedTextureDownload - RequestDownload url[%s] cache[%s]", m_Url.c_str(), m_CacheHash.TryGetCStr());

    if (!gnetVerifyf(m_State != FeedTextureDownloadState::Downloading, "We're already downloading this feed content texture [%s]", m_Url.c_str()))
    {
        return false;
    }

    m_Url = url;
    m_CacheHash = cacheName;

    // Fill in the descriptor for our request
    m_TexRequestDesc.m_Type = fileType;
    m_TexRequestDesc.m_CloudFilePath = m_Url.c_str();
    m_TexRequestDesc.m_TxtAndTextureHash = cacheName;
    m_TexRequestDesc.m_GamerIndex = NetworkInterface::GetLocalGamerIndex();
    m_TexRequestDesc.m_CloudMemberId = memberId;
    m_TexRequestDesc.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_SC;
    m_TexRequestDesc.m_CloudRequestFlags = eRequestFlags::eRequest_MaintainQueryOnRedirect | (preferCache ? eRequestFlags::eRequest_CacheForceCache : 0);
    m_TexRequestDesc.m_OnlyCacheAndVerify = preloadOnly;

    const ScFeedRecentDownload* recent = ScFeedTextureDownload::GetRecentDownload(GetCacheHash().GetHash());

    if (recent != nullptr && recent->IsFresh())
    {
        // We recently failed this download. We won't retry till the time expires
        if (recent->m_Result == FeedTextureDownloadState::Failed)
        {
            m_State = FeedTextureDownloadState::Failed;
            return false;
        }
        else
        {
            m_TexRequestDesc.m_CloudRequestFlags |= eRequestFlags::eRequest_CacheForceCache;
        }
    }

    return DoDownload();
}

void ScFeedTextureDownload::ConvertToFullDownload()
{
    if (!m_TexRequestDesc.m_OnlyCacheAndVerify)
    {
        return;
    }

    if (m_State == FeedTextureDownloadState::Downloading)
    {
        ClearDownload();
        ClearTexture();

        DoDownload();
        return;
    }

    if (m_State == FeedTextureDownloadState::Failed || m_State == FeedTextureDownloadState::None)
    {
        m_TexRequestDesc.m_OnlyCacheAndVerify = false;
        return;
    }

    if (!gnetVerifyf(m_State == FeedTextureDownloadState::Done, "Unexpected state [%u]. Have you added a new state without handling it here?", static_cast<unsigned>(m_State)))
    {
        m_TexRequestDesc.m_OnlyCacheAndVerify = false;
        m_State = FeedTextureDownloadState::Failed;
        return;
    }

    // We have preloaded the thing. We need to call download again. Won't actually download it.
    m_State = FeedTextureDownloadState::None;
    m_TexRequestDesc.m_CloudRequestFlags |= eRequestFlags::eRequest_CacheForceCache;
    m_TexRequestDesc.m_OnlyCacheAndVerify = false;

    DoDownload();
}

bool ScFeedTextureDownload::IsWorking() const
{
    return m_State == FeedTextureDownloadState::Downloading;
}

bool ScFeedTextureDownload::IsDone() const
{
    return m_State == FeedTextureDownloadState::Done;
}

bool ScFeedTextureDownload::IsFailed() const
{
    return m_State == FeedTextureDownloadState::Failed;
}

bool ScFeedTextureDownload::IsValid() const
{
    return m_RefCount > 0;
}

bool ScFeedTextureDownload::IsPreloadOnly() const
{
    return m_TexRequestDesc.m_OnlyCacheAndVerify;
}

void ScFeedTextureDownload::ClearRecentDownloads()
{
    s_RecentDownloads.clear();
}

void ScFeedTextureDownload::AddRecentDownload(const atHashString id, const FeedTextureDownloadState result)
{
    int oldestPos = -1;
    u64 oldestTime = 0;

    if (!CLiveManager::IsSignedIn() || !CLiveManager::IsOnline())
    {
        gnetDebug2("AddRecentDownload skipped for signed out or offline player - %s [%u]", id.TryGetCStr(), static_cast<unsigned>(result));
        return;
    }

    gnetDebug2("AddRecentDownload - %s [%u]", id.TryGetCStr(), static_cast<unsigned>(result));

    for (int i=0; i < s_RecentDownloads.GetCount(); ++i)
    {
        if (s_RecentDownloads[i].m_Id == id)
        {
            s_RecentDownloads[i].m_Time = rlGetPosixTime();
            s_RecentDownloads[i].m_Result = result;

            return;
        }

        if (oldestPos == -1 || (s_RecentDownloads[i].m_Time < oldestTime))
        {
            oldestTime = s_RecentDownloads[i].m_Time;
            oldestPos = i;
        }
    }

    if (!s_RecentDownloads.IsFull())
    {
        s_RecentDownloads.Push(ScFeedRecentDownload(id, result));

        return;
    }

    if (oldestPos != -1)
    {
        s_RecentDownloads[oldestPos] = ScFeedRecentDownload(id, result);
    }
}

const ScFeedRecentDownload* ScFeedTextureDownload::GetRecentDownload(const atHashString id)
{
    for (int i = 0; i < s_RecentDownloads.GetCount(); ++i)
    {
        if (s_RecentDownloads[i].m_Id == id)
        {
            return &(s_RecentDownloads[i]);
        }
    }

    return nullptr;
}

bool ScFeedTextureDownload::DoDownload()
{
    bool success = false;

    CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
    retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(m_TexRequestHandle, m_TexRequestDesc);

    gnetDebug2("ScFeedTextureDownload - DoDownload url[%s] cache[%s] handle[%d] ret[%d]", m_Url.c_str(), m_CacheHash.TryGetCStr(), m_TexRequestHandle, retVal);

    if (retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER)
    {
        gnetAssert(m_TexRequestHandle != CTextureDownloadRequest::INVALID_HANDLE);
        if (retVal == CTextureDownloadRequest::READY_FOR_USER)
        {
            gnetDebug2("ScFeedTextureDownload - DTM already has TXD ready for url[%s] cache[%s] handle[%d]", m_Url.c_str(), m_CacheHash.TryGetCStr(), m_TexRequestHandle);
        }
        success = true;
    }
    else
    {
        gnetError("ScFeedTextureDownload - RequestTextureDownload failed url[%s] cache[%s] handle[%d] ret[%d]", m_Url.c_str(), m_CacheHash.TryGetCStr(), m_TexRequestHandle, retVal);
    }

    if (!success)
    {
        AddRecentDownload(m_CacheHash.GetHash(), FeedTextureDownloadState::Failed);
    }

    m_State = success ? FeedTextureDownloadState::Downloading : FeedTextureDownloadState::Failed;

    return success;
}

bool ScFeedTextureDownload::ApplyTxdSlot()
{
    strLocalIndex txdSlot = strLocalIndex(DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_TexRequestHandle));

    gnetDebug2("ScFeedTextureDownload - ApplyTxdSlot [%s] [%d]", m_Url.c_str(), txdSlot.Get());

    if (!g_TxdStore.IsValidSlot(txdSlot))
    {
        gnetError("ScFeedTextureDownload - ApplyTxdSlot failed [%s]", m_Url.c_str());
        return false;
    }
    g_TxdStore.AddRef(txdSlot, REF_OTHER);
    m_TextureDictionarySlot = txdSlot;

    gnetDebug2("AddRef txd[%u] num[%d]", txdSlot.Get(), g_TxdStore.GetNumRefs(txdSlot));

    return true;
}

void ScFeedTextureDownload::ClearDownload()
{
    if (m_TexRequestHandle == CTextureDownloadRequest::INVALID_HANDLE)
    {
        return;
    }

    DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_TexRequestHandle);
    m_TexRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
}

void ScFeedTextureDownload::ClearTexture()
{
    if (m_TextureDictionarySlot.Get() < 0)
    {
        return;
    }

    if (g_TxdStore.IsValidSlot(m_TextureDictionarySlot))
    {
        gnetDebug2("ScFeedTextureDownload - RemoveRef - ClearTexture [%s] [%d]", m_Url.c_str(), m_TextureDictionarySlot.Get());

        // Add a renderer streaming reference. If we are in the landing page, UIImage does not do refcounting on TXDs (because the UI updates on the RT), and Scaleform will not add this ref in time.
        //gDrawListMgr->AddTxdReference( m_TextureDictionarySlot ); //TODO_ANDI: verify

        g_TxdStore.RemoveRef(m_TextureDictionarySlot, REF_OTHER);

        gnetDebug2("RemoveRef txd[%u] num[%d]", m_TextureDictionarySlot.Get(), g_TxdStore.GetNumRefs(m_TextureDictionarySlot));
    }

    m_TextureDictionarySlot = strLocalIndex(-1);
}

//#####################################################################################

ScFeedContentDownloadInfo::ScFeedContentDownloadInfo()
    : m_PreferCache(false)
    , m_CachePartition(0)
{
    m_CacheName[0] = 0;
    m_Url[0] = 0;
}

ScFeedContentDownloadInfo::ScFeedContentDownloadInfo(const char* url, const char* cacheName, const atHashString imageId, const bool preferCache)
    : m_PreferCache(preferCache)
    , m_ImageId(imageId)
	, m_CachePartition(0)
{
    safecpy(m_Url, url);
    safecpy(m_CacheName, cacheName);
    FixCacheName();

    m_BaseUrlHash = atHashString(url);
}

ScFeedContentDownloadInfo::ScFeedContentDownloadInfo(const EmblemDescriptor& emblem, const bool preferCache)
    : m_EmblemData(emblem)
    , m_PreferCache(preferCache)
	, m_CachePartition(0)
{
    //rlClan::GetCrewEmblemCloudPath(static_cast<rlClanEmblemSize>(m_EmblemData.m_size), m_Url, sizeof(m_Url), ScFeedContentDownloadInfo::USE_DDS_FOR_CREW_EMBLEM);
    //m_EmblemData.CreateTextureCacheName(m_CacheName, sizeof(m_CacheName), ScFeedContentDownloadInfo::USE_DDS_FOR_CREW_EMBLEM);

    m_BaseUrlHash = atHashString(m_Url);
}

void ScFeedContentDownloadInfo::AppendResize(const char* resizeData)
{
    if (!gnetVerifyf(!m_EmblemData.IsValid(), "Only non-crew emblems can be resized by appending a string"))
    {
        return;
    }

    safecat(m_Url, resizeData);
    safecat(m_CacheName, resizeData);
    FixCacheName();
}

void ScFeedContentDownloadInfo::FixCacheName()
{
    // In GTA Scaleform combines txd and texture hash with a / so we have to remove all slashes
    StringReplaceChars(m_CacheName, "/\\", '_');
}

const char*
ScFeedContentDownloadInfo::ExtractLocalImage(const char* path, const char* defaultTxd, char* txdBuffer, const size_t txdBufferLen)
{
    const char* img = nullptr;
    rtry
    {
        rverify(path != nullptr && path[0] != 0, catchall, gnetError("path is null"));
        rverify(defaultTxd != nullptr && defaultTxd[0] != 0, catchall, gnetError("defaultTxd is null"));

        rverify(ScFeedLoc::IsLocalImage(path), catchall, gnetError("This is not a local image [%s]", path));
        
        img = path + strlen(ScFeedLoc::LOCAL_IMAGE_PROTOCOL);
        rverify(img[0] != 0, catchall, gnetError("Empty path [%s]", path));

        const char* atPos = strchr(img, '@');

        if (atPos == nullptr) //It's in the format "tx://texture"
        {
            safecpy(txdBuffer, defaultTxd, txdBufferLen);
            return img;
        }

        //It's in the format "tx://texture_dictionary@texture"
        const size_t diff = static_cast<size_t>(atPos - img);
        rverify(diff < txdBufferLen, catchall, gnetError("Buffer too small [%s]", path));
        strncpy(txdBuffer, img, diff);
        txdBuffer[diff] = 0;

        img = atPos + 1;
    }
    rcatchall
    {
        return nullptr;
    }

    return img;
}

void ScFeedContentDownloadInfo::Apply4KImageOption()
{
#if TEST_FAKE_FEED
    if (PARAM_scFeedForce4K.Get())
    {
        const char* param = nullptr;
        PARAM_scFeedForce4K.GetParameter(param);

        SetUse4KImages(param == nullptr || stricmp(param, "false") != 0);
        return;
    }
#endif //TEST_FAKE_FEED

    const bool use4K = RunsIn4K();

    SetUse4KImages(use4K);
}

bool ScFeedContentDownloadInfo::RunsIn4K()
{
    //const unsigned MIN_WIDTH_FOR_4K = 3000;
    //const unsigned width = sga::Factory::GetDisplayWidth();

    //return width >= MIN_WIDTH_FOR_4K && /*rage::fwuiDisplayHelper::GetSupports4K()*/ false;
    return false;
}

void ScFeedContentDownloadInfo::SetUse4KImages(const bool val)
{
    s_Use4KImages = val;
}

bool ScFeedContentDownloadInfo::IsImageInTxd(const char* cacheName)
{
    if (ScFeedLoc::IsLocalImage(cacheName))
    {
        return true;
    }

    const strLocalIndex c_txdSlot = g_TxdStore.FindSlotFromHashKey(atHashString(cacheName).GetHash());

    if (!c_txdSlot.IsValid())
    {
        return false;
    }

    // Each slot only has one texture - check if we can get it...
    fwTxd const* c_txd = g_TxdStore.Get(c_txdSlot);
    return c_txd && c_txd->GetEntry(0);
}

//#####################################################################################

ScFeedContent::ScFeedContent()
    : m_State(FeedContentState::None)
    , m_RequestStartTime(0)
    , m_UsedTime(0)
{

}

ScFeedContent::~ScFeedContent()
{
    Clear();
}

void ScFeedContent::Clear()
{
    m_FeedId = RosFeedGuid();
    m_State = FeedContentState::None;
    m_RequestStartTime = 0;

    for (int i = 0; i < m_Textures.size(); ++i)
    {
        if (m_Textures[i] != nullptr)
        {
            m_Textures[i]->RemoveRef();
            m_Textures[i] = nullptr;
        }
    }

    CleanArray();

    StopContentUpdate();
}

bool ScFeedContent::Update()
{
    if (m_State != FeedContentState::Downloading)
    {
        return false;
    }

    for (int i = 0; i < m_Textures.size(); ++i)
    {
        if (m_Textures[i] != nullptr)
        {
            m_Textures[i]->Update();
        }
    }

    CheckDone();

    return m_State == FeedContentState::Finished;
}

void ScFeedContent::Touch()
{
    if (m_State == FeedContentState::None)
    {
        return;
    }

    m_UsedTime = rlGetPosixTime();
}

bool ScFeedContent::RetrieveContent(const ScFeedPost* meta, const AmendDownloadInfoFunc& func, const bool bigFeed, const bool preloadOnly)
{
    if (meta == nullptr)
    {
        return false;
    }

    if (m_State == FeedContentState::Downloading)
    {
        if (!gnetVerifyf(m_FeedId == meta->GetId(), "Retrieve content called with different metadata [%s] vs [%s]", meta->GetId().ToFixedString().c_str(), m_FeedId.ToFixedString().c_str()))
        {
            return false;
        }

        if (!preloadOnly)
        {
            ConvertToFullDownload();
        }

        m_UsedTime = rlGetPosixTime();
        return true;
    }

    if (m_State == FeedContentState::Finished)
    {
        if (!preloadOnly)
        {
            ConvertToFullDownload();
        }

        m_UsedTime = rlGetPosixTime();
        return true;
    }

    gnetDebug1("ScFeedContent - Retrieving feed content for [%s]", meta->GetId().ToFixedString().c_str());
    
    m_RequestStartTime = rlGetPosixTime();
    m_UsedTime = m_RequestStartTime;
    m_FeedId = meta->GetId();

    CleanArray();

    const unsigned imageFlags = ScFeedPost::ImageFlagsFromType(meta->GetFeedType());

    if (imageFlags == SC_F_I_NONE)
    {
        SetState(FeedContentState::Finished);
        return true;
    }

    if (bigFeed)
    {
        DownloadBigFeedContent(*meta, func, preloadOnly);
    }
    else
    {
        DownloadContent(*meta, func, preloadOnly);
    }

    SetState(FeedContentState::Downloading);

    return true;
}

void ScFeedContent::ConvertToFullDownload()
{
    bool reactivated = false;

    for (int i = 0; i < m_Textures.size(); ++i)
    {
        if (m_Textures[i] != nullptr)
        {
            m_Textures[i]->ConvertToFullDownload();
            reactivated |= m_Textures[i]->IsWorking();
        }
    }

    if (reactivated)
    {
        SetState(FeedContentState::Downloading);
    }
}

bool ScFeedContent::RetrieveAvatarImage(const RosFeedGuid& id, const char* url)
{
    if (m_State == FeedContentState::Downloading)
    {
        if (!gnetVerifyf(m_FeedId == id, "Retrieve content called with different metadata [%s] vs [%s]", id.ToFixedString().c_str(), m_FeedId.ToFixedString().c_str()))
        {
            return false;
        }
        return true;
    }

    if (m_State == FeedContentState::Finished)
    {
        return true;
    }

    gnetDebug1("ScFeedContent - RetrieveAvatarImage [%s] [%s]", id.ToFixedString().c_str(), url);

    m_RequestStartTime = rlGetPosixTime();
    m_UsedTime = m_RequestStartTime;
    m_FeedId = id;

    CleanArray();

    m_Textures.Append() = RequestDownload(url, url, rlCloudMemberId(), CTextureDownloadRequestDesc::WWW_FILE, false, false);

    SetState(FeedContentState::Downloading);

    return false;
}

void ScFeedContent::GetDownloadInfo(const ScFeedPost& activity, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfoArray& items)
{
    const unsigned imageFlags = ScFeedPost::ImageFlagsFromType(activity.GetFeedType());

    const bool useRegarding = (imageFlags & SC_F_I_FROM_REGARDING) != 0;
    const bool useGroup = (imageFlags & SC_F_I_FROM_GROUP) != 0;
    const bool useActor = (imageFlags & SC_F_I_FROM_ACTOR) != 0;
    const bool useImages = (imageFlags & SC_F_I_FROM_IMAGES) != 0;

    const atArray<ScFeedRegarding>& regardings = activity.GetRegardings();
    const atArray<ScFeedPost>& group = activity.GetGroup();
    const atArray<ScFeedPost>& regardingPosts = activity.GetRegardingPosts();

    if (useActor)
    {
        const char* mainUri = activity.GetActor().GetImage();

        if (mainUri != nullptr && mainUri[0] != 0)
        {
            ScFeedContentDownloadInfo info(mainUri, mainUri, atHashString(), true);
            PushDownloadInfo(activity, func, info, items);
        }

        for (const ScFeedActor& actor : activity.m_Actors)
        {
            const char* uri = actor.GetImage();

            if (uri != nullptr && uri[0] != 0)
            {
                ScFeedContentDownloadInfo info(uri, uri, atHashString(), true);
                PushDownloadInfo(activity, func, info, items);
            }
        }
    }

    if (useImages)
    {
        for (const atString& img : activity.m_Images)
        {
            if (!img.empty())
            {
                ScFeedContentDownloadInfo info(img.c_str(), img.c_str(), atHashString(), true);
                PushDownloadInfo(activity, func, info, items);
            }
        }
    }

    // The feed system may return images that we don't actually display so we check which part we care about
    if (useRegarding)
    {
        for (int i = 0; i < regardings.size() && !items.IsFull(); ++i)
        {
            const ScFeedRegarding& regarding = regardings[i];
            const atString& img = regarding.GetSubject().m_Image;

            if (img.length() > 0 && regarding.GetRegardingType() != ScFeedRegardingType::Crew && // Crew images are handled separately
                !items.IsFull())
            {
                ScFeedContentDownloadInfo info(img.c_str(), img.c_str(), atHashString(), true); // These images haven't got any special ID yet
                PushDownloadInfo(activity, func, info, items);
            }
        }

        for (int i = 0; i < regardingPosts.size() && !items.IsFull(); ++i)
        {
            GetDownloadInfo(regardingPosts[i], func, items);
        }

        const atString* locImg = activity.GetLoc(ScFeedLoc::IMAGE_ID);
        if (locImg && locImg->length() > 0 && !items.IsFull())
        {
            ScFeedContentDownloadInfo info(locImg->c_str(), locImg->c_str(), ScFeedLoc::IMAGE_ID, true);
            PushDownloadInfo(activity, func, info, items);
        }

        const atString* locLogo = activity.GetLoc(ScFeedLoc::LOGO_ID);
        if (locLogo && locLogo->length() > 0 && !items.IsFull())
        {
            ScFeedContentDownloadInfo info(locLogo->c_str(), locLogo->c_str(), ScFeedLoc::LOGO_ID, true);
            PushDownloadInfo(activity, func, info, items);
        }
    }

    // The feed system may return images that we don't actually display so we check which part we care about
    if (useGroup)
    {
        for (int i = 0; i < group.size() && !items.IsFull(); ++i)
        {
            GetDownloadInfo(group[i], func, items);
        }
    }
}

void ScFeedContent::GetBigFeedDownloadInfo(const ScFeedPost& activity, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfoArray& items)
{
    const atString* smallImg = activity.GetLoc(ScFeedLoc::IMAGE_ID);
    const atString* bigImg = activity.GetLoc(ScFeedLoc::BIGIMAGE_ID);
    const atString* logoImg = activity.GetLoc(ScFeedLoc::LOGO_ID);

    if (smallImg != nullptr && smallImg->length() > 0 && gnetVerifyf(!items.IsFull(), "There should always be space for the small image"))
    {
        PushDownloadInfo(activity, func, ScFeedContentDownloadInfo(smallImg->c_str(), smallImg->c_str(), ScFeedLoc::IMAGE_ID, true), items);
    }

    if (bigImg != nullptr && bigImg->length() > 0 && gnetVerifyf(!items.IsFull(), "There should always be space for the big image"))
    {
        PushDownloadInfo(activity, func, ScFeedContentDownloadInfo(bigImg->c_str(), bigImg->c_str(), ScFeedLoc::BIGIMAGE_ID, true), items);
    }

    if (logoImg != nullptr && logoImg->length() > 0 && gnetVerifyf(!items.IsFull(), "There should always be space for the logo"))
    {
        PushDownloadInfo(activity, func, ScFeedContentDownloadInfo(logoImg->c_str(), logoImg->c_str(), ScFeedLoc::LOGO_ID, true), items);
    }
}

bool ScFeedContent::GetDownloadInfo(const ScFeedPost& feed, const atHashString& id, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfo& info)
{
    const atString* img = feed.GetLoc(id);

    if (img)
    {
        info = ScFeedContentDownloadInfo(img->c_str(), img->c_str(), id, true);

        if (func)
        {
            func(info, feed, 0);
        }

        return true;
    }

    info = ScFeedContentDownloadInfo();
    return false;
}

void ScFeedContent::UpdateContentDownload(ContentDoneFunc func)
{
    gnetAssertf(sysThreadType::IsUpdateThread(), "UpdateContentDownload isn't thread safe");

    int idx = s_UpdatingContent.GetCount() - 1;
    while (idx >= 0)
    {
        ScFeedContent* c = s_UpdatingContent[idx];

        if (c != nullptr && c->Update())
        {
            if (func != nullptr)
            {
                func(*c);
            }
        }

        if (c == nullptr || c->m_State != FeedContentState::Downloading)
        {
            s_UpdatingContent.Delete(idx);
        }

        --idx;
    }
}

void ScFeedContent::PushDownloadInfo(const ScFeedPost& feed, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfo info, ScFeedContentDownloadInfoArray& items)
{
    if (items.IsFull())
    {
        return;
    }

    if (func)
    {
        func(info, feed, static_cast<unsigned>(items.size()));
    }

    for (const ScFeedContentDownloadInfo& other : items)
    {
        // If the imageId is null we avoid dupes. If m_ImageId is set we allow dupes because
        // the UI will request the image by ID (like "bigimg", "img2" ...) so if the id is missing we won't show an image.
        if (info.m_BaseUrlHash.IsNotNull() && other.m_BaseUrlHash == info.m_BaseUrlHash && info.m_ImageId.IsNull())
        {
            // It's the same image so we don't need to download it multiple times (in different resolutions)
            return;
        }
    }

    items.Push(info);
}

void ScFeedContent::CheckDone()
{
    if (m_State != FeedContentState::Downloading)
    {
        return;
    }

    for (int i = 0; i < m_Textures.size(); ++i)
    {
        const ScFeedTextureDownload* textureDownload = m_Textures[i];
        if (textureDownload && textureDownload->IsWorking())
        {
            return;
        }
    }

    gnetDebug1("All feed content retrieved for [%s]", m_FeedId.ToFixedString().c_str());
    SetState(FeedContentState::Finished);
}

void ScFeedContent::DownloadContent(const ScFeedPost& activity, const AmendDownloadInfoFunc& func, const bool preloadOnly)
{
    ScFeedContentDownloadInfoArray items;
    GetDownloadInfo(activity, func, items);

    for (int i = 0; i < items.size() && !m_Textures.IsFull(); ++i)
    {
        const ScFeedContentDownloadInfo& info = items[i];
        if (info.m_EmblemData.IsValid())
        {
        }
        else
        {
            m_Textures.Append() = RequestDownload(info.m_Url, info.m_CacheName, rlCloudMemberId(), CTextureDownloadRequestDesc::WWW_FILE, info.m_PreferCache, preloadOnly);
        }
    }
}

void ScFeedContent::DownloadBigFeedContent(const ScFeedPost& activity, const AmendDownloadInfoFunc& func, const bool preloadOnly)
{
    ScFeedContentDownloadInfoArray items;
    GetBigFeedDownloadInfo(activity, func, items);

    for (int i = 0; i < items.size() && !m_Textures.IsFull(); ++i)
    {
        const ScFeedContentDownloadInfo& info = items[i];
        m_Textures.Append() = RequestDownload(info.m_Url, info.m_CacheName, rlCloudMemberId(), CTextureDownloadRequestDesc::WWW_FILE, info.m_PreferCache, preloadOnly);
    }
}

void ScFeedContent::SetState(FeedContentState state)
{
    gnetDebug3("ScFeedContent - SetState [%u] for [%s]", static_cast<unsigned>(state), m_FeedId.ToFixedString().c_str());

    gnetAssertf(sysThreadType::IsUpdateThread(), "SetState isn't thread safe");

    if (m_State != FeedContentState::Downloading && state == FeedContentState::Downloading)
    {
        s_UpdatingContent.PushAndGrow(this);
    }

    m_State = state;
}

void ScFeedContent::StopContentUpdate()
{
    gnetAssertf(sysThreadType::IsUpdateThread(), "StopContentUpdate isn't thread safe");

    atArray<ScFeedContent*>::iterator iter = s_UpdatingContent.begin();
    while (iter != s_UpdatingContent.end())
    {
        if (*iter == this)
        {
            iter = s_UpdatingContent.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

void ScFeedContent::CleanArray()
{
    //SetZero works on the existin entries so first we set the size to max.
    m_Textures.Resize(m_Textures.GetMaxCount());

    for (ScFeedTextureDownload*& txt : m_Textures)
    {
        txt = nullptr;
    }

    // And now set the entries count back to 0
    m_Textures.Reset();
}

ScFeedTextureDownload*
ScFeedContent::RequestDownload(const char* url, const char* cacheName, const rlCloudMemberId& memberId, const CTextureDownloadRequestDesc::Type fileType, 
	const bool preferCache, const bool preloadOnly)
{
    int freeIdx = -1;

    ScFeedTextureDownload* found = nullptr;

    atHashString cn(cacheName);

    for (int i = 0; i < s_TextureDownloads.GetCount(); ++i)
    {
        ScFeedTextureDownload& item = s_TextureDownloads[i];
        if (item.GetCacheHash() == cn)
        {
            if (!preloadOnly)
            {
                item.ConvertToFullDownload();
            }

            item.AddRef();
            return &item;
        }

        if (freeIdx == -1 && !item.IsValid())
        {
            freeIdx = i;
        }
    }

    if (freeIdx != -1)
    {
        found = &(s_TextureDownloads[freeIdx]);
    }

    if (found == nullptr && !s_TextureDownloads.IsFull())
    {
        found = &s_TextureDownloads.Append();
    }

    if (found == nullptr)
    {
        return nullptr;
    }

    found->AddRef();
    found->RequestDownload(url, cacheName, memberId, fileType, preferCache, preloadOnly);

    return found;
}

//#####################################################################################

CSocialClubFeedMgr::CSocialClubFeedMgr()
{
    // We don't want to move the netStatus while being worked on
    m_ActiveRequests.SetCount(m_ActiveRequests.GetMaxCount());

    m_PresenceDlgt.Bind(this, &CSocialClubFeedMgr::OnPresenceEvent);
    m_RosDlgt.Bind(this, &CSocialClubFeedMgr::OnRosEvent);
}

CSocialClubFeedMgr::~CSocialClubFeedMgr()
{
    Shutdown();
}

CSocialClubFeedMgr& CSocialClubFeedMgr::Get()
{
    typedef atSingleton<CSocialClubFeedMgr> CSocialClubFeedMgr;
    if (!CSocialClubFeedMgr::IsInstantiated())
    {
        CSocialClubFeedMgr::Instantiate();
    }

    return CSocialClubFeedMgr::GetInstance();
}

void CSocialClubFeedMgr::Init()
{
    gnetDebug2("CSocialClubFeedMgr - Init");

    ClearActiveRequests();

    rlPresence::AddDelegate(&m_PresenceDlgt);
    rlRos::AddDelegate(&m_RosDlgt);
}

void CSocialClubFeedMgr::Shutdown()
{
    gnetDebug2("CSocialClubFeedMgr - Shutdown");

    ClearAll();

    rlPresence::RemoveDelegate(&m_PresenceDlgt);
    rlRos::RemoveDelegate(&m_RosDlgt);
}

void CSocialClubFeedMgr::ClearFeeds()
{
    gnetDebug2("CSocialClubFeedMgr - ClearFeeds");
    ClearActiveRequests();
}

void CSocialClubFeedMgr::ClearAll()
{
    gnetDebug2("CSocialClubFeedMgr - ClearAll");

    ClearActiveRequests();
    ClearFeeds();

    // After sign-out & co. we re-allow all download attempts
    ScFeedTextureDownload::ClearRecentDownloads();
}

void CSocialClubFeedMgr::Update()
{
    PROFILE

    ScFeedContent::UpdateContentDownload(CSocialClubFeedMgr::ContentDone);
    UpdateRequests();
}

void CSocialClubFeedMgr::UpdateRequests()
{
    for (unsigned i=0; i < m_ActiveRequests.size(); ++i)
    {
        ScFeedActiveRequest& activeRequest = m_ActiveRequests[i];

        if (!activeRequest.IsValid())
        {
            if (!m_Requests.IsEmpty())
            {
                ScFeedRequest request = m_Requests[0];
                DownloadFeeds(activeRequest, request);

                // Has to be done after DownloadFeeds so IsWorking stays correct
                m_Requests.Delete(0);
            }

            continue;
        }

        //See if it was pending and is not done
        if (!activeRequest.m_RefreshFeedsStatus.Pending())
        {
            ScFeedRequest& request = activeRequest.m_Request;
            const netStatus& status = activeRequest.m_RefreshFeedsStatus;

            if (request.m_RequestCallback.IsValid()) // We call the callback with pending in case the requesting code needs to some clean up before starting
            {
                netStatus status;
                status.SetPending();
                request.m_RequestCallback(status, request);
                status.SetSucceeded(); // to avoid the assert
            }

            //If we succeed;
            if (status.Succeeded())
            {
                gnetDebug1("CSocialClubFeedMgr - Refresh succeeded");
                ProcessReceivedFeeds(activeRequest);
            }
            else if (status.Failed())
            {
                gnetDebug1("CSocialClubFeedMgr - Refresh failed");
            }
            else
            {
                gnetDebug1("CSocialClubFeedMgr - Refresh canceled");
            }

            if (request.m_RequestCallback.IsValid())
            {
                request.m_RequestCallback(status, request);
            }

            activeRequest.Clear();
        }
    }
}

unsigned CSocialClubFeedMgr::RenewFeeds(const ScFeedFilterId& filter, const unsigned numItems, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb)
{
    gnetDebug2("CSocialClubFeedMgr - RenewFeeds");
    return RequestFeeds(filter, 0, numItems, requestCb, postCb);
}

unsigned CSocialClubFeedMgr::RequestFeeds(const ScFeedFilterId& filter, const unsigned startIndex, const unsigned number, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb)
{
    gnetDebug2("CSocialClubFeedMgr - RequestFeeds [%u, +%u]", startIndex, number);

    ScFeedRequest request(filter, startIndex, number, requestCb, postCb);
    return AddRequest(request);
}

bool CSocialClubFeedMgr::ViewUrlOnSocialClub(const ScFeedPost& /*post*/, const char* /*googleAnalyticsCategory*/, bool /*useExternalBrowser*/ /*= false*/)
{
#if 0
    rtry
    {
        const atString* url = post.GetLoc(ScFeedLoc::URL_ID);

        rverify(ScFeedPost::HasWebUrl(post), catchall, gnetError("post with id[%s] has no URL", post.GetId().ToFixedString().c_str()));

        rverify(googleAnalyticsCategory != nullptr && googleAnalyticsCategory[0] != 0, catchall, gnetError("googleAnalyticsCategory is null"));

        bool hasAuthLink = false;
        char uribuffer[sysServiceArgs::MAX_ARG_LENGTH];

        const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
        if (dlink && CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
        {
            hasAuthLink = sysServiceArgs::GetParamValue(dlink->m_Value.c_str(), dlink->m_Value.length(), ScFeedParam::DLINK_SCAUTHLINK, uribuffer);
        }

        if (hasAuthLink)
        {
            rverify(CLiveManager::GetSocialClubMgr().GetAuthTokenAndShowBrowser(NetworkInterface::GetLocalGamerIndex(), uribuffer),
                catchall, gnetError("Failed to launch browser [%s]", uribuffer));
        }
        else
        {
            rcheck(url, catchall, gnetWarning("Couldn't use authlink but there's no url"));

            rlSystemBrowserConfig config;
            config.m_Url = url->c_str(); // Looks wrong but is fine. It's used on the fly
			if (useExternalBrowser)
			{
				config.SetExternalBrowser();
			}
			else
			{
				config.SetFullScreen();
				config.SetUseSafeRegion(true);
				config.SetHeaderEnabled(false);
				config.SetFooterEnabled(false);
			}

            if (!g_SystemUi.CanShowBrowser())
            {
                CLiveManager::GetSocialClubMgr().ShowMissingBrowser("");
                rcheck(false, catchall, gnetDebug1("Skipping browser url %s", url->c_str()));
            }

            rverify(g_SystemUi.ShowWebBrowser(NetworkInterface::GetLocalGamerIndex(), config), catchall, gnetError("ShowWebBrowser failed"));
        }

        ReportGoogleAnalyticEvent(post.GetId(), googleAnalyticsCategory, "view_url");

        return true;
    }
    rcatchall
    {
    }
#endif

    return false;
}

bool CSocialClubFeedMgr::IsWorking() const
{
    if (!m_Requests.IsEmpty())
    {
        return true;
    }

    for (int i = 0; i < m_ActiveRequests.GetCount(); ++i)
    {
        if (m_ActiveRequests[i].IsValid())
        {
            return true;
        }
    }

    return false;
}

bool CSocialClubFeedMgr::HasRequestPending(unsigned requestId) const
{
    for (int i = 0; i < m_Requests.GetCount(); ++i)
    {
        if (m_Requests[i].GetRequestId() == requestId)
        {
            return true;
        }
    }

    for (int i = 0; i < m_ActiveRequests.GetCount(); ++i)
    {
        if (m_ActiveRequests[i].GetRequestId() == requestId)
        {
            return true;
        }
    }

    return false;
}

void CSocialClubFeedMgr::FireScriptEvent(const atHashString /*eventId*/)
{
}

bool CSocialClubFeedMgr::IsUgcRestrictedContent(const ScFeedType feedType)
{
    // If a user has UGC privileges, then the content will not be restricted.
    if (CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL, true))
        return false;

    // Allow all Rockstar non-UGC content
    if (ScFeedPost::IsRockstarFeedType(feedType) && !ScFeedPost::IsUgcFeedType(feedType))
    {
        return false;
    }

    // Selectively allow some other content, but reject most by default.
    switch (feedType)
    {
    case ScFeedType::AccountRegistered:
        return false;
    default:
        return true;
    }
}

#if TEST_FAKE_FEED
bool CSocialClubFeedMgr::LoadFeedsFromFile(ScFeedActiveRequest& activeRequest, const ScFeedRequest& request, const char* filePath)
{
    fiStream* xmlStream = ASSET.Open(filePath, NULL);

    if (!gnetVerifyf(xmlStream != NULL, "Failed to load %s", filePath))
    {
        return false;
    }

    bool success = false;

    INIT_PARSER;

    parTree* tree = PARSER.LoadTree(xmlStream);

    const parTreeNode* node = tree != nullptr ? tree->GetRoot() : nullptr;
    const parTreeNode* messagesNode = node ? node->FindChildWithName("Result") : nullptr;
    const parElement* el = messagesNode ? &messagesNode->GetElement() : nullptr;

    int msgCount = 0;
    bool postsAvailable = false;
    if (el != nullptr)
    {
        //Number of messages
        const parAttribute* countAttr = el->FindAttribute("c");
        msgCount = countAttr ? countAttr->FindIntValue() : 0;

        //Total number of messages
        const parAttribute* paAttr = el->FindAttribute("pa");
        postsAvailable = paAttr ? paAttr->FindBoolValue() : false;

        success = countAttr != nullptr;
    }

    xmlStream->Close();

    activeRequest.m_FeedIter.SetParserTree(tree, msgCount, msgCount, postsAvailable);

    SHUTDOWN_PARSER;

    activeRequest.m_Request = request;

    if (!activeRequest.m_RefreshFeedsStatus.Pending())
    {
        activeRequest.m_RefreshFeedsStatus.SetPending();
    }

    if (!gnetVerifyf(success, "Failed to parse result"))
    {
        activeRequest.m_RefreshFeedsStatus.SetFailed();
        return false;
    }

    activeRequest.m_RefreshFeedsStatus.SetSucceeded();

    return true;
}
#endif //TEST_FAKE_FEED

bool CSocialClubFeedMgr::DownloadFeeds(ScFeedActiveRequest& activeRequest, const ScFeedRequest& request)
{
    if (PARAM_noSCFeed.Get())
    {
        ClearActiveRequests();
        return false;
    }

    gnetDebug1("CSocialClubFeedMgr - DownloadFeeds");

    //Make sure we're not already busy
    if (activeRequest.m_RefreshFeedsStatus.Pending())
    {
        gnetError("CSocialClubFeedMgr - DoRefreshFeeds - Skipped. Refresh is in progress");
        return false;
    }

#if TEST_FAKE_FEED
    const char* fileName = "";

    if (PARAM_scDebugFeedLanding.Get(fileName) && request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Landing)
    {
        return LoadFeedsFromFile(activeRequest, request, fileName);
    }
    else if (PARAM_scDebugFeedPriority.Get(fileName) && request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Priority)
    {
        return LoadFeedsFromFile(activeRequest, request, fileName);
    }
    else if (PARAM_scDebugFeedSeries.Get(fileName) && request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Series)
    {
        return LoadFeedsFromFile(activeRequest, request, fileName);
    }
    else if (PARAM_scDebugFeedHeist.Get(fileName) && request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Heist)
    {
        return LoadFeedsFromFile(activeRequest, request, fileName);
    }
    else if (PARAM_scDebugFeed.Get(fileName))
    {
        return LoadFeedsFromFile(activeRequest, request, fileName != nullptr && fileName[0] != 0 ? fileName : "common:/non_final/www/feed/debug_feed.txt");
    }
#endif //TEST_FAKE_FEED

    //check to see if the recipient is the current gamer.
    int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
    {
        gnetDebug1("CSocialClubFeedMgr - DoRefreshFeeds - Skipped. Invalid gamer index");

        return false;
    }

    const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
    if (!cred.IsValid())
    {
        gnetDebug1("CSocialClubFeedMgr - DoRefreshFeeds - Skipped. Invalid credentials");

        return false;
    }

    const sysLanguage sysLanguage = rlGetLanguage();
    const rlScLanguage scLanguage = fwLanguagePack::GetScLanguageFromLanguage(sysLanguage);
    const char* actorTypeFilterStr = ScFeedFilterId::ActorTypeFilterToString(request.GetFilter().GetActorTypeFilter());
    
    const char* tag[2] = { 0 };
    const ScFeedPostTypeFilter postTypeFilter = request.GetFilter().GetTypeFilter();
    tag[0] = ScFeedFilterId::PostTypeFilterToString(postTypeFilter);

    unsigned numTags = (tag[0] != nullptr && tag[0][0] != 0) ? 1 : 0;
    
    // This is slightly messy but as we don't support any && on filters these checks are needed
    // If the user has not linked his account he can only see Rockstar stuff and there are similar restrictions if he has no UGC privileges
    // If the chosen filter is more "restrictive" though than "Marketing" we use that as we still want to keep the Offers page working and such things.
    if (!ScFeedFilterId::IsSafeFilterForAlUsers(postTypeFilter))
    {
        const bool hasUgcPrivileges = CLiveManager::CheckUserContentPrivileges(GAMER_INDEX_LOCAL, true);
        
        if (!hasUgcPrivileges)
        {
            tag[0] = ScFeedFilterId::PostTypeFilterToString(ScFeedPostTypeFilter::Marketing);
        }

        numTags = 1;
    }

    bool success = false;

    const atArray<RosFeedGuid>& requestedPosts = request.m_RequestedPosts;

    if (requestedPosts.size() > 0)
    {
        char strings[ScFeedRequest::MAX_INDIVIDUAL_IDS][32];
        const char* ids[ScFeedRequest::MAX_INDIVIDUAL_IDS];

        const unsigned num = rage::Min(ScFeedRequest::MAX_INDIVIDUAL_IDS, static_cast<unsigned>(requestedPosts.GetCount()));
        for (unsigned i=0; i< num; ++i)
        {
            safecpy(strings[i], requestedPosts[i].ToFixedString().c_str());
            ids[i] = strings[i];
        }

        success = rlFeed::GetFeedPosts(localGamerIndex,
            ids,
            num,
            scLanguage,
            &activeRequest.m_FeedIter,
            &activeRequest.m_RefreshFeedsStatus);
    }
#if TEST_FAKE_FEED
    else if (PARAM_scFeedFromCloud.Get() || (request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Landing && PARAM_scCloudLanding.Get()))
    {
        const char* suffix = "";

        if (request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Landing)
        {
            PARAM_scCloudLanding.Get(suffix);
        }

        char cloudPath[RAGE_MAX_PATH] = { 0 };

        if (cloudPath[0] == 0)
        {
            safecpy(cloudPath, request.GetFilter().FormatCloudFilePath(suffix ? suffix : "").c_str());
        }

        CloudRequestID req = CloudManager::GetInstance().RequestGetTitleFile(cloudPath, 0, eRequest_CacheNone | eRequest_SecurityNone);
        success = req != INVALID_CLOUD_REQUEST_ID;

        if (success)
        {
            activeRequest.m_RefreshFeedsStatus.SetPending();
            activeRequest.m_CloudRequestId = req;
        }

    }
#endif
    else if (request.GetFilter().GetType() == ScFeedRequestType::Feeds)
    {
        success = rlFeed::GetFeed(localGamerIndex,
            numTags == 0 ? nullptr : tag,
            numTags,
            request.m_Start,
            request.m_Amount,
            scLanguage,
            actorTypeFilterStr,
            &activeRequest.m_FeedIter,
            &activeRequest.m_RefreshFeedsStatus);
    }
    else
    {
        char ownerIdStr[64] = { 0 };
        formatf(ownerIdStr, "%" I64FMT "d", request.GetFilter().GetOwnerId());

        success = rlFeed::GetWall(localGamerIndex,
            static_cast<unsigned>(request.GetFilter().GetWallType()),
            ownerIdStr,
            nullptr,                        // no tags for now
            0,                              // no tags for now
            request.m_Start,
            request.m_Amount,
            scLanguage,
            &activeRequest.m_FeedIter,
            &activeRequest.m_RefreshFeedsStatus);
    }

    if (success)
    {
        activeRequest.m_Request = request;
    }
    else
    {
        gnetError("CSocialClubFeedMgr - rlFeed::GetFeed failed");

        activeRequest.Clear();
    }

    return success;
}

void CSocialClubFeedMgr::ProcessReceivedFeeds(ScFeedActiveRequest& activeRequest)
{
    gnetDebug1("CSocialClubFeedMgr - ProcessReceivedFeeds - received %d items", activeRequest.m_FeedIter.GetNumMessagesRetrieved());

    if (!gnetVerifyf(activeRequest.IsValid(), "The request is not valid. Will be skipped"))
    {
        return;
    }

    ScFeedRequest& request = activeRequest.m_Request;

    unsigned numMessage = activeRequest.m_FeedIter.GetNumMessagesRetrieved();
    numMessage = rage::Min(numMessage, request.m_Amount);

#if TEST_FAKE_FEED
    const bool isLanding = request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Landing;
    const bool isPriority = request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Priority;
    const bool isSeries = request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Series;
    const bool isHeist = request.GetFilter().GetTypeFilter() == ScFeedPostTypeFilter::Heist;
    const bool isNegativeFilter = isLanding || isPriority || isSeries || isHeist;
    const bool isFakeFeed = (isLanding && PARAM_scDebugFeedLanding.Get())
                         || (isPriority && PARAM_scDebugFeedPriority.Get())
                         || (isSeries && PARAM_scDebugFeedSeries.Get())
                         || (isHeist && PARAM_scDebugFeedHeist.Get())
                         || (!isNegativeFilter && PARAM_scDebugFeed.Get());

    if (!isNegativeFilter && PARAM_scDebugFeed.Get())
    {
        numMessage = activeRequest.m_FeedIter.GetNumMessagesRetrieved();
    }
#endif //TEST_FAKE_FEED

    unsigned numUnread = 0;
    const char* feedBody = nullptr;

    for (unsigned i = 0; i < numMessage; ++i)
    {
        ScFeedPost* feedMsg = nullptr;
        rtry
        {
        #if TEST_FAKE_FEED
            const unsigned worldPos = request.m_Start + static_cast<unsigned>(i);

            // We're reading from a file so GetNumMessagesRetrieved is always the entire list
            if (isFakeFeed /*&& worldPos >= m_FeedIter.GetNumMessagesRetrieved()*/)
            {
                unsigned fakepos = worldPos % activeRequest.m_FeedIter.GetNumMessagesRetrieved();
                rverify(activeRequest.m_FeedIter.GetFeedAtIndex(fakepos, &feedBody), catchall, gnetWarning("Failed to parse feed"));
            }
            else
        #endif //TEST_FAKE_FEED
            {
                rverify(activeRequest.m_FeedIter.NextFeed(&feedBody), catchall, gnetWarning("Failed to iterate feeds"));
            }

            if (request.m_PostCallback.IsValid())
            {
                request.m_PostCallback(feedBody, request);
            }

            numUnread += feedMsg && feedMsg->GetReadPosixTime() == 0 ? 1 : 0;
        }
        rcatchall
        {
            if (feedMsg)
            {
                delete feedMsg;
            }
        }
    }

    gnetDebug1("CSocialClubFeedMgr - ProcessReceivedFeeds - range[%u, +%u] result[num=%u, terminalorder=%u, hasmore=%s]", request.m_Start, request.m_Amount,
        activeRequest.m_FeedIter.GetNumMessagesRetrieved(), activeRequest.m_FeedIter.GetTerminalOrder(), activeRequest.m_FeedIter.HasMorePostsAvailabel() ? "yes" : "no");
}

unsigned CSocialClubFeedMgr::AddRequest(const ScFeedRequest& request)
{
    gnetDebug2("CSocialClubFeedMgr -  AddRequest [%u, +%u]", request.m_Start, request.m_Amount);

    for (int i = 0; i < m_Requests.GetCount(); ++i)
    {
        if (m_Requests[i].Contains(request))
        {
            // We're already reading this data
            return m_Requests[i].GetRequestId();
        }
    }

    for (int i = 0; i < m_ActiveRequests.GetCount(); ++i)
    {
        if (m_ActiveRequests[i].m_Request.Contains(request))
        {
            // We're already reading this data
            return m_ActiveRequests[i].GetRequestId();
        }
    }

    if (m_Requests.IsFull())
    {
        gnetAssertf(false, "We're adding a request but we're full. If you hit this there's either a bug or I need to add checks to ensure no one spams requests");
        m_Requests.Delete(0);
    }

    m_Requests.Push(request);

    return request.GetRequestId();
}

void CSocialClubFeedMgr::ClearRequests(const ScFeedFilterId& filter)
{
    gnetDebug2("CSocialClubFeedMgr -  ClearRequests");

    // Never remove the current active request. There's a check on the receiving side
    for (int i = m_Requests.GetCount() - 1; i >= 1; --i)
    {
        if (m_Requests[i].GetFilter() == filter)
        {
            m_Requests.Delete(i);
        }
    }

    for (int i = m_Requests.GetCount() - 1; i >= 1; --i)
    {
        if (m_ActiveRequests[i].GetRequest().GetFilter() == filter)
        {
            m_ActiveRequests[i].Clear();
        }
    }
}

void CSocialClubFeedMgr::ClearActiveRequests()
{
    gnetDebug2("CSocialClubFeedMgr -  ClearActiveRequests");

    for (int i = m_ActiveRequests.GetCount() - 1; i >= 1; --i)
    {
        m_ActiveRequests[i].Clear();
    }
}

int CSocialClubFeedMgr::GetUsableContentIdx(const ScFeedContent* contents, const int numContents)
{
    int oldestPos = -1;
    u64 oldestTime = 0;
    int pos = -1;
    for (int i = 0; i < numContents; ++i)
    {
        if (contents[i].IsSet())
        {
            // If the download isn't finished we don't free up the resource.
            // Just to ensure we don't load too much and keep on overwriting things
            if (contents[i].IsDone() && (oldestPos == -1 || (contents[i].GetLastUsedTime() < oldestTime)))
            {
                oldestTime = contents[i].GetLastUsedTime();
                oldestPos = i;
            }
            continue;
        }
        pos = i;
        break;
    }

    if (!gnetVerifyf(pos != -1 || oldestPos != -1, "No slot found for new feed content"))
    {
        return -1;
    }

    int realPos = pos != -1 ? pos : oldestPos;
    return realPos;
}

void CSocialClubFeedMgr::RefreshUI(const RosFeedGuid /*guid*/, unsigned /*actionType*/)
{
#if 0
    CSocialClubFeedRegistrationMgr& registrationManager = uiBootstrapStatic::GetAccessHelper().GetSocialClubFeedRegistrationManager();
    registrationManager.SetFeedEntryAction(guid, actionType);
#endif
}

void CSocialClubFeedMgr::OnPresenceEvent(const rlPresenceEvent* pEvent)
{
    if (pEvent->GetId() != PRESENCE_EVENT_SIGNIN_STATUS_CHANGED)
    {
        return;
    }

    const rlPresenceEventSigninStatusChanged* pThisEvent = pEvent->m_SigninStatusChanged;

    const int nControllerIndex = rlPresence::GetActingUserIndex();
    bool bIsActiveGamer = nControllerIndex == pThisEvent->m_GamerInfo.GetLocalIndex();

    if (!bIsActiveGamer)
    {
        return;
    }

    if (pThisEvent->SignedOut())
    {
        ClearAll();
    }
}


void CSocialClubFeedMgr::OnRosEvent(const rlRosEvent& rEvent)
{
    gnetDebug2("OnRosEvent :: %s received", rEvent.GetAutoIdNameFromId(rEvent.GetId()));

    if (rEvent.GetId() == RLROS_EVENT_GET_CREDENTIALS_RESULT )
    {
    }
}

void CSocialClubFeedMgr::OnCloudEvent(const CloudEvent* TEST_FAKE_FEED_ONLY(pEvent))
{
#if TEST_FAKE_FEED
    rtry
    {
        rcheck(pEvent, catchall, );
        rcheck(pEvent->GetType() == CloudEvent::EVENT_REQUEST_FINISHED, catchall, );
        const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

        for (unsigned i = 0; i < m_ActiveRequests.size(); ++i)
        {
            ScFeedActiveRequest& activeRequest = m_ActiveRequests[i];
            
            if (activeRequest.m_CloudRequestId == pEventData->nRequestID)
            {
                if (pEventData->bDidSucceed)
                {
                    char memFileName[64];
                    fiDevice::MakeMemoryFileName(memFileName, sizeof(memFileName), pEventData->pData, pEventData->nDataSize, false, "FeedCloudFile");

                    ScFeedRequest rq = activeRequest.GetRequest();
                    LoadFeedsFromFile(activeRequest, rq, memFileName);
                }
                else
                {
                    activeRequest.m_RefreshFeedsStatus.SetFailed();
                }

                return;
            }
        }
    }
    rcatchall
    {
    }
#endif
}

bool CSocialClubFeedMgr::GetRockstarIdFeedTopic(char buf[128])
{
    rtry
    {
        int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
        rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetError("Invalid gamer index"));

        rlRosCredentials credentials = rlRos::GetCredentials(localGamerIndex);
        rverify(credentials.IsValid(), catchall, gnetError("Invalid credentials"));

        sprintf(buf, "feed/user/%" I64FMT "d", credentials.GetRockstarId());
        return true;
    }
    rcatchall
    {

    }

    return false;
}

void CSocialClubFeedMgr::ContentDone(const ScFeedContent& content)
{
    CSocialClubFeedMgr::RefreshUI(content.GetId(), 0);
}

#if __BANK

#if TEST_FAKE_FEED

void Bank_FeedShowAllCharacters()
{
    PARAM_scFeedShowAllCharacters.Set(PARAM_scFeedShowAllCharacters.Get() ? nullptr : "true");
}

void Bank_ToggleForece4K()
{
    ScFeedContentDownloadInfo::SetUse4KImages(!ScFeedContentDownloadInfo::Use4KImages());
}

#endif //TEST_FAKE_FEED

void CSocialClubFeedMgr::InitWidgets(bkBank& bank)
{
    bank.AddButton("Show all characters (glyphs)", CFA(Bank_FeedShowAllCharacters));
    bank.AddButton("Toggle Force 4K", CFA(Bank_ToggleForece4K));
}

#endif //___BANK

#endif //GEN9_STANDALONE_ENABLED