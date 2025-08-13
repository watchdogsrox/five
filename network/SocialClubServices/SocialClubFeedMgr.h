#ifndef INC_SOCIALCLUBFEEDMGR_H_
#define INC_SOCIALCLUBFEEDMGR_H_

#include "fwutil/Gen9Settings.h"
#if GEN9_STANDALONE_ENABLED

// Rage
#include "atl/array.h"
#include "atl/singleton.h"
#include "atl/string.h"
#include "data/rson.h"
#include "net/status.h"
#include "rline/feed/rlfeed.h"

// Net
#include "net/time.h"

// Framework
#include "streaming/streamingdefs.h"

// Game
#include "socialclubfeed.h"
#include "scene/downloadabletexturemanager.h"
#include "network/cloud/CloudManager.h"
#include "network/crews/NetworkCrewEmblemMgr.h"

struct EmblemDescriptor;
class CSocialClubFeedMgr;
class ScFeedContent;

enum class FeedContentState
{
    None,
    Downloading,
    Finished // This is also set if any of the internal downloads fails 
};

enum class FeedTextureDownloadState
{
    None,
    Downloading,
    Done,
    Failed
};

//PURPOSE
//  For most images we can just rely on the DownloadableTextureManager and m_UseCacheFile to avoid repeating downloads
//  For crew emblems and some other images we can't just use m_UseCacheFile though as the file can change on the server
//  In addition in case of errors (404 ...) we would keep on trying to download the file.
//  For those cases we keep a little list of recently downloaded or failed posts
class ScFeedRecentDownload
{
public:
    static const unsigned MAX_CACHED_DOWNLOAD_RESULTS = 32;
    static const unsigned VALID_TIME_SEC;

    ScFeedRecentDownload();
    ScFeedRecentDownload(const atHashString id, const FeedTextureDownloadState result);

    //PURPOSE
    //  Returns true if the info is not out of date
    bool IsFresh() const;

public:
    u64 m_Time;
    atHashString m_Id;
    FeedTextureDownloadState m_Result;
};

typedef atFixedArray<ScFeedRecentDownload, ScFeedRecentDownload::MAX_CACHED_DOWNLOAD_RESULTS> ScFeedRecentDownloads;

//PURPOSE
// Small helper class to manage the download state of a texture
class ScFeedTextureDownload
{
public:
    // This needs to be big enough to hold all images shown on screen in the feed view
    // but also big enough to keep all images shown on the landing page
    //TODO_ANDI: I had to increase this for the _agg list which is an overlay. Reduce me again in case we can change the ui for that.
    static const unsigned MAX_DOWNLOADED_TEXTURES = 100;

    ScFeedTextureDownload();
    ~ScFeedTextureDownload();

    //PURPOSE
    //  Increase the ref count
    void AddRef();

    //PURPOSE
    //  Decrease the ref count
    void RemoveRef();

    //PURPOSE
    // Updates the download flow
    void Update();

    //PURPOSE
    // Starts the download
    //PARAMS
    //  url           - The url of the image. Depending on the type this my be the full url
    //  cacheName     - The name of the file in the cache. Ensure this is always the same for the same file.
    //  memberId      - The id of the user or crew
    //  fileType      - The type of download (Crew, WWW ...)
    //  preferCache   - Tries to use the cached file if available without checking the server
    //  preloadOnly   - When true we download the images and cache them to the HDD but we don't keep the texture in memory
    //  partition     - If we store it in a longer lasting cache or in the default one.
    //                  Use Persistent with caution. The memory has to be managed manually.
    bool RequestDownload(const char* url,
        const char* cacheName,
        const rlCloudMemberId& memberId,
        const CTextureDownloadRequestDesc::Type fileType,
        const bool preferCache,
        const bool preloadOnly);

    //PURPOSE
    // Can be called when we're in preload-only mode to switch to a full download
    void ConvertToFullDownload();

    //PURPOSE
    // Returns true if we're downloading
    bool IsWorking() const;

    //PURPOSE
    // True if the download and texture generation has been completed successfully
    bool IsDone() const;

    //PURPOSE
    // True if something failed
    bool IsFailed() const;

    //PURPOSE
    // True if this instance is valid and used
    bool IsValid() const;

    //PURPOSE
    // True if we only cache the file but don't keep the texture in memory
    bool IsPreloadOnly() const;

    //PURPOSE
    // The url of the texture.
    const atString& GetUrl() const { return m_Url; }

    //PURPOSE
    // The local 'file path' of the texture.
    const atHashString GetCacheHash() const { return m_CacheHash; }

    //PURPOSE
    // This is the id of the texture. Can be used to delete/deref the texture and such things
    const strLocalIndex& GetTxtSlot() const { return m_TextureDictionarySlot; }

    //PURPOSE
    // Remove all recent download info
    static void ClearRecentDownloads();

    //PURPOSE
    //  Add a recent download
    //PARAMS
    //  id     - Should be the local cache name of the file
    //  result - The current status/result of the download
    static void AddRecentDownload(const atHashString id, const FeedTextureDownloadState result);

    //PURPOSE
    //  Returns the data of a recent download or null if it isn't found
    static const ScFeedRecentDownload* GetRecentDownload(const atHashString id);

protected:
    //PURPOSE
    // Try to release the image memory and/or stop the download
    void Clear();

    //PURPOSE
    //  Releases the slot in the DownloadableTextureManager and stops the download if in progress
    void ClearDownload();

    //PURPOSE
    //  Reduces the texture ref count
    void ClearTexture();

    //PURPOSE
    //  Execute the download
    bool DoDownload();

    //PURPOSE
    //  Get the texture slot and increment the ref count
    bool ApplyTxdSlot();

protected:
    static const unsigned IMAGE_DOWNLOAD_RETRY_MS;
    static const unsigned DEFAULT_DOWNLOAD_RETRIES;

    static ScFeedRecentDownloads s_RecentDownloads;

    FeedTextureDownloadState m_State;
    CTextureDownloadRequestDesc m_TexRequestDesc;
    TextureDownloadRequestHandle m_TexRequestHandle;
    strLocalIndex m_TextureDictionarySlot;
    atString m_Url;
    atHashString m_CacheHash;
    unsigned m_Retries;
    unsigned m_NextRetryTime;

    // Not great I know but we need some sort of counting for when different posts use the same image.
    // The DownloadableTextureManager doesn't handle that in a way that's compatible with how the feed pages work
    unsigned m_RefCount;
};

typedef atFixedArray<ScFeedTextureDownload, ScFeedTextureDownload::MAX_DOWNLOADED_TEXTURES> ScFeedTextureDownloads;

//TODO_ANDI: it's time to put the texture download classes in a separate cpp/h but do that as a separate CL
//PURPOSE
//  Little helper class to enlist the images we need to download for a specific post
class ScFeedContentDownloadInfo
{
public:
    static const unsigned MAX_TEXTURES_PER_FEED = 7;

    static const char* RESIZE_LANDING_POST_TILE_HD;
    static const char* RESIZE_LANDING_POST_BIGIMAGE_HD;

    ScFeedContentDownloadInfo();

    //PURPOSE
    //  Build the info from url and cache name
    //PARAMS
    //  url          - The URL of the image
    //  cacheName    - The id we use locally to store and reference the image
    //  imageId      - The id we use to idnetify the image type (BIGIMG ...)
    //  preferCache  - If true we use the locally cached image if available
    ScFeedContentDownloadInfo(const char* url, const char* cacheName, const atHashString imageId, const bool preferCache);

    //PURPOSE
    //  Build the infor from an emblem. The Url and cache name will be extracted
    ScFeedContentDownloadInfo(const EmblemDescriptor& emblem, const bool preferCache);

    //PURPOSE
    //  The name used by the game to reference the image
    const char* GetCacheName() const { return m_CacheName; }

    //PURPOSE
    //  Can be used to append a resize querystring to the url (and cache name)
    void AppendResize(const char* resizeData);

    void FixCacheName();

    //PURPOSE
    //  Parses a local image in the format tx://texture_dictionary@texture or tx://texture
    //PARAMS
    //  path         - The image path to parse
    //  defaultTxd   - When the image path does not contain a texture dictionary this will be returned as dictionary
    //  txdBuffer    - The output texture dictionary buffer
    //  txdBufferLen - The size of the output texture dictionary buffer
    static const char* ExtractLocalImage(const char* path, const char* defaultTxd, char* txdBuffer, const size_t txdBufferLen);

    //PURPOSE
    //  Returns true if we want to download images in '4K' (== the large version of the images; not neccessarily 3840x2160)
    static bool Use4KImages() { return s_Use4KImages; }

    //PURPOSE
    //  Applies the default logic to turn on/off 4k image
    static void Apply4KImageOption();

    //PURPOSE
    //  Returns true if the game runs in a resolution which is close to 4K.
    static bool RunsIn4K();

    //PURPOSE
    //  Set 4K images on or off
    static void SetUse4KImages(const bool val);

    //PURPOSE
    //  Returnes true if the image texture has been created and loaded
    static bool IsImageInTxd(const char* cacheName);

public:
    // The url of the image to download
    char m_Url[RL_MAX_URL_BUF_LENGTH];

    // The local cache name of the image
    char m_CacheName[RL_MAX_URL_BUF_LENGTH];

    // In case it's an emblem this is the emblem data
    EmblemDescriptor m_EmblemData;

    // The id we use to identify the image type
    atHashString m_ImageId;

    // The hash of the non-amended url to download. If set it's used to avoid duplicates
    atHashString m_BaseUrlHash;

    // If true the game will use the cached texture if available without checking if the server images has changed.
    // Saves quite a few server calls and also a bit of time.
    bool m_PreferCache;

    // The cache location
    unsigned m_CachePartition;

private:
    static bool s_Use4KImages;
};

typedef atFixedArray<ScFeedContentDownloadInfo, ScFeedContentDownloadInfo::MAX_TEXTURES_PER_FEED> ScFeedContentDownloadInfoArray;
typedef atFixedArray<ScFeedTextureDownload*, ScFeedContentDownloadInfo::MAX_TEXTURES_PER_FEED> ScFeedTextureDownloadArray;

//PURPOSE
//  Used to apply UI specific changes to the downloaded image (== set the desired size where needed)
typedef void(*AmendDownloadInfoFunc)(ScFeedContentDownloadInfo& data, const ScFeedPost& feed, const unsigned index);

typedef void(ContentDoneFunc)(const ScFeedContent& content);

//PURPOSE
// Used to retrieve content like localized strings and foremost the needed textures.
class ScFeedContent
{
public:
    ScFeedContent();
    ~ScFeedContent();

    //PURPOSE
    // Clear up all downloaded data
    void Clear();

    //PURPOSE
    // Flag the content as used
    void Touch();

    //PURPOSE
    //  Starts to download the content (images) of the feed. Returns true if the operation started successfully.
    //PARAMS
    //  meta        - The feed that needs images
    //  func        - A function that can be passed in to edit the download infos before they're added. Can be null
    //  bigFeed     - if true we download the BigFeed data
    //  preloadOnly - When true we download the images and cache them to the HDD but we don't keep the texture in memory
    bool RetrieveContent(const ScFeedPost* meta, const AmendDownloadInfoFunc& func, const bool bigFeed, const bool preloadOnly = false);

    // Can be called when we're in preload-only mode to switch to a full download
    void ConvertToFullDownload();

    //PURPOSE
    //  Downloads an avatar image which isn't part of the feed.
    //PARAMS
    //  id     - The id of the download
    //  url    - The url of the image
    bool RetrieveAvatarImage(const RosFeedGuid& id, const char* url);

    //PURPOSE
    // Returns the associated feed id
    const RosFeedGuid& GetId() const { return m_FeedId; }

    //PURPOSE
    // True if it's linked to a valid feed
    bool IsSet() const { return m_FeedId.IsValid() != 0; }

    //PURPOSE
    // True if the content is ready
    bool IsDone() const { return m_State == FeedContentState::Finished; }

    //PURPOSE
    // UTC time when the download request started
    u64 GetRequestStartTime() const { return m_RequestStartTime; }

    //PURPOSE
    // UTC time when the data has been used
    u64 GetLastUsedTime() const { return m_UsedTime; }

    //PURPOSE
    // Returns the list of images/textures contained in the feed
    const ScFeedTextureDownloadArray& GetTextures() const { return m_Textures; }

    //PURPOSE
    //  Used to enlist the images to download
    //PARAMS
    //  feed   - The feed post containing the image paths
    //  func   - A function that can be passed in to edit the download infos before they're added. Can be null
    //  items  - the list of results containing url and cache name
    static void GetDownloadInfo(const ScFeedPost& feed, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfoArray& items);

    //PURPOSE
    //  Used to enlist the images to download for the big feed
    //PARAMS
    //  feed   - The feed post containing the image paths
    //  func   - A function that can be passed in to edit the download infos before they're added. Can be null
    //  items  - the list of results containing url and cache name
    static void GetBigFeedDownloadInfo(const ScFeedPost& feed, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfoArray& items);

    static bool GetDownloadInfo(const ScFeedPost& feed, const atHashString& id, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfo& info);

    //PURPOSE
    //  Check the download state of all currently downloading posts
    static void UpdateContentDownload(ContentDoneFunc func);

protected:
    ScFeedContent(const ScFeedContent& other);
    ScFeedContent& operator=(const ScFeedContent& other);
    //PURPOSE
    //  Append the info to the items and call the func on it.
    static void PushDownloadInfo(const ScFeedPost& feed, const AmendDownloadInfoFunc& func, ScFeedContentDownloadInfo info, ScFeedContentDownloadInfoArray& items);

    //PURPOSE
    // Update the download. Returns true when we switch from downloading to finished
    bool Update();

    //PURPOSE
    // Verifies if the retrieval is still in progress or if it failed or succeeded
    void CheckDone();

    //PURPOSE
    //  Fills up the list of textures
    //PARAMS
    //  feed        - The feed that needs images
    //  func        - A function that can be passed in to edit the download infos before they're added. Can be null
    //  preloadOnly - When true we download the images and cache them to the HDD but we don't keep the texture in memory
    void DownloadContent(const ScFeedPost& feed, const AmendDownloadInfoFunc& func, const bool preloadOnly);

    //PURPOSE
    //  Fills up the list of textures for the big feed
    //PARAMS
    //  feed        - The feed that needs images
    //  func        - A function that can be passed in to edit the download infos before they're added. Can be null
    //  preloadOnly - When true we download the images and cache them to the HDD but we don't keep the texture in memory
    void DownloadBigFeedContent(const ScFeedPost& feed, const AmendDownloadInfoFunc& func, const bool preloadOnly);

    //PURPOSE
    // Change the state of the content (Downloading, Failed ...)
    void SetState(FeedContentState state);

    //PURPOSe
    //  Called to remove the content from the update list
    void StopContentUpdate();

    //PURPOSE
    //  0-s the whole m_Textures array to fix threading issues
    //  The ui thread needs to access the textures but Push(...) isn't thread safe
    //  because it increments the counter first and only then it applies the data.
    void CleanArray();

    //PURPOSE
    // Tries to get a free ScFeedTextureDownload and starts the download when needed
    // If a download of the same image already exists it returns that and increases the ref count
    static ScFeedTextureDownload* RequestDownload(const char* url, const char* cacheName, const rlCloudMemberId& memberId, 
        const CTextureDownloadRequestDesc::Type fileType, const bool preferCache, const bool preloadOnly);

protected:
    static ScFeedTextureDownloads s_TextureDownloads;
    static atArray<ScFeedContent*> s_UpdatingContent;

    RosFeedGuid m_FeedId;
    FeedContentState m_State;

    // This is a atFixedArray as we won't have many textures per feed and because there are some pointers which need to stay consistent during the download
    ScFeedTextureDownloadArray m_Textures; // List of textures to download

    u64 m_RequestStartTime;
    mutable u64 m_UsedTime;
};

//PURPOSE
// Downloads and manages the feeds of the player
class CSocialClubFeedMgr : public CloudListener
{
public:
    friend class ScFeedImagePreloadHelper;
    friend class ScFeedQueue;

    static const unsigned MAX_PENDING_REQUESTS = 6;
    static const unsigned MAX_ACTIVE_SC_REQUESTS = 4;

    CSocialClubFeedMgr();
    virtual ~CSocialClubFeedMgr();

    static CSocialClubFeedMgr& Get();

    //PURPOSE
    // Set up the manager
    void Init();

    //PURPOSE
    // Clear the manager and all feeds. Can be called in case of a sign out or such things
    void Shutdown();

    //PURPOSE
    // Clears all feeds and their content
    void ClearFeeds();

    //PURPOSE
    // Clear feeds and also any manually loaded data
    void ClearAll();

    //PURPOSE
    // Needs to be called so the download operations can be executed and processed
    void Update();

    //PURPOSE
    // Requests to download a specific wall or 'special' list of feeds
    //PARAMS
    // filter - the wall filter
    // numItems  - The number of items we wish to retrieve
    // requestCb - A callback that will be called after the request finished or failed
    // postCb    - A callback that will be called to process a feed post
    //             Important: When this callback is specified no wall will be generated and therefore the 
    //             user has to parse the post and handle its storage
    //RETURNS
    // Returns the id of the request or ScFeedRequest::INVALID_REQUEST_ID if something went wrong
    unsigned RenewFeeds(const ScFeedFilterId& filter,
        const unsigned int numItems,
        const ScFeedRequestCallback& requestCb = ScFeedRequestCallback(),
        const ScFeedPostCallback& postCb = ScFeedPostCallback());

    //PURPOSE
    //  Version which takes a feed post for when the feedMgr isn't the owner of the post
    bool ViewUrlOnSocialClub(const ScFeedPost& post, const char* googleAnalyticsCategory, bool useExternalBrowser=false);

    //PURPOSE
    // Returns true while we're reading feeds
    bool IsWorking() const;

    //PURPOSE
    // Returns true if the specified request is still in work
    bool HasRequestPending(unsigned requestId) const;

    //PURPOSE
    //  Fire a script event about a feed event that happened
    void FireScriptEvent(const atHashString eventId);

    //PURPOSE
    //  Check if a user's user-generated content privileges allow a feed message.
    static bool IsUgcRestrictedContent(const ScFeedType feedType);

    //PURPOSE
    //  Iterates through contents and returns a free one or the oldest one
    static int GetUsableContentIdx(const ScFeedContent* contents, const int numContents);

#if TEST_FAKE_FEED
    //PURPOSE
    // The file needs to have the same format as a response when querying the server.
    bool LoadFeedsFromFile(ScFeedActiveRequest& activeRequest, const ScFeedRequest& request, const char* filePath);
#endif

#if __BANK
    void InitWidgets(bkBank& bank);
    void UpdateWidgets();
#endif //___BANK

protected:
    void UpdateRequests();

    //PURPOSE
    // Read more feeds on a specific wall
    //PARAMS
    // filter     - The wall filter
    // startIndex - The index in the feed list to start from
    // number     - The amount of feeds to download
    // requestCb  - A callback that will be called after the request finished or failed
    // postCb     - A callback that will be called to process a feed post
    unsigned RequestFeeds(const ScFeedFilterId& filter, const unsigned startIndex, const unsigned number, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb);

    //PURPOSE
    // Executes the pending http request to read the feeds
    bool DownloadFeeds(ScFeedActiveRequest& activeRequest, const ScFeedRequest& request);

    //PURPOSE
    // Parses the read result in case of a success
    void ProcessReceivedFeeds(ScFeedActiveRequest& request);

    //PURPOSE
    // Removes the feed with id feedId from all walls
    void InnerDeleteFeed(const RosFeedGuid& feedId);

    //PURPOSE
    // Add a new request for the specific wall
    unsigned AddRequest(const ScFeedRequest& request);

    //PURPOSE
    // Remove all requests for a specific wall
    void ClearRequests(const ScFeedFilterId& filter);

    //PURPOSE
    // Stop all feed download requests
    void ClearActiveRequests();

    //PURPOSE
    //  Used for presence events (sign-in/out) to clear data when needed
    void OnPresenceEvent(const rlPresenceEvent* pEvent);

    //PURPOSE
    //  Listen for online service events
    void OnRosEvent(const rlRosEvent& rEvent);

    //PURPOSE
    //  Listen for cloud events
    void OnCloudEvent(const CloudEvent* pEvent) override;

    //PURPOSE
    //  Gets the topic to subscribe for
    static bool GetRockstarIdFeedTopic(char buf[128]);

    //PURPOSE
    // Placeholder method for when the UI needs a refresh
    static void RefreshUI(const RosFeedGuid guid, unsigned actionType);

    //PURPOSE
    //  Called when  content download for a post ends (fails or succeeds)
    static void ContentDone(const ScFeedContent& content);

protected:
    atFixedArray<ScFeedRequest, MAX_PENDING_REQUESTS> m_Requests;
    atFixedArray<ScFeedActiveRequest, MAX_ACTIVE_SC_REQUESTS> m_ActiveRequests;

    rlPresence::Delegate m_PresenceDlgt;
    rlRos::Delegate m_RosDlgt;
};

#endif //GEN9_STANDALONE_ENABLED

#endif //INC_SOCIALCLUBFEEDMANAGER_H_
