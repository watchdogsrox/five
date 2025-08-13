#ifndef INC_SOCIALCLUBFEEDTILESMGR_H_
#define INC_SOCIALCLUBFEEDTILESMGR_H_

#include "fwutil/Gen9Settings.h"
#if GEN9_STANDALONE_ENABLED

// Rage
#include "atl/array.h"
#include "atl/singleton.h"
#include "atl/string.h"
#include "rline/ugc/rlugccommon.h"
#include "rline/rlfriendevents.h"

// Game
#include "socialclubfeedmgr.h"
#include "frontend/profilesettings.h"

class CSocialClubFeedTilesMgr;

enum class TilesMetaDownloadState
{
    None,
    DownloadingMeta,
    MetaReady
};

enum class TilesImagesDownloadState
{
    None,
    DownloadingBasics, // It's still downloading the images which show up immediately
    DownloadingMore, // You could display the main page without any missing image but not everything has downloaded yet
    Ready, // finished to download or we failed to download
};

enum class TilesPendingUpdate
{
    None,
    GoneForceOffline,
    RealDataPending,
};

//PURPOSE
//  Keep track of what we're preloading
class ScTilePreloadParams
{
public:
    ScTilePreloadParams();

public:
    u64 m_StartTime;

    bool m_Enabled; // If false we're not preloading
    bool m_IncludePriorityFeed; // If true we also preload the priority feed
};

//PURPOSE
//  Keep track of the currently focused tile
class ScFocussedTile
{
public:
    ScFocussedTile();

    void Clear();

public:
    static const unsigned WAIT_TIME_MS = 250;

    RosFeedGuid m_Tile;
    netTimeout m_Timeout;

    // If true we already requested the download of the neighbours
    bool m_NeighboursRequested;
};

class ScFeedTileDownloadInfo
{
public:
    ScFeedTileDownloadInfo();

    TilesMetaDownloadState m_MetaDownloadState;

    // The last time we tried to download metadata
    u64 m_LastDownloadAttempt;

    // The last the metadata download succeeded
    u64 m_LastDownloadSuccess;

    // The id of the current metadata download request
    unsigned m_FeedRequestId;

    int m_ErrorCode;
};

class ScFeedTile
{
public:
    ScFeedTile();

    //PURPOSE
    //  Clear feed and images
    void Clear();

    //PURPOSE
    //  Loads the images
    //PARAMS
    //  func           - A function that can be passed in to edit the download infos before they're added. Can be null
    void LoadContent(const AmendDownloadInfoFunc& func);

    //PURPOSE
    //  Unloads the images
    void ClearContent();

    bool GetCoords(unsigned& col, unsigned& row) const;

    static bool GetCoords(const ScFeedPost& post, unsigned& col, unsigned& row);

public:
    static const unsigned MAX_LANDING_ITEMS;
    static const unsigned MAX_CAROUSEL_ITEMS;
    static const unsigned MAX_PRIORITY_ITEMS;
    static const unsigned MIN_PRIORITY_POSTS_PRELOAD;

    ScFeedPost m_Feed;
    ScFeedContent m_FeedContent;

    // The order in the row (lower values are on the left)
    int m_Order;

    //  Set to true when the tile has been preloaded once
    bool m_TilePreloaded;
};

typedef atArray<ScFeedTile*> ScFeedTileRow;

//PURPOSE
//  Used to identify a tile that's being cached
struct ScPendingTileCache
{
    RosFeedGuid m_Guid;
    bool m_Working;
};

//PURPOSE
//  Download and cache the images of the landing page tiles
class ScTilesCaching
{
public:
    ScTilesCaching();

    void Clear();

    void SetMaxDownloads(const unsigned maxDownloads);

    void CacheTile(const RosFeedGuid& guid, const bool important);

    void RemoveTile(const RosFeedGuid& guid);

    const bool HasImportant() const { return m_Important.size() != 0; }
    const bool HasAny() const { return HasImportant() || m_Common.size() != 0; }

public:
    static const unsigned MAX_CONCURRENT_DOWNLOADS = 8;

    atFixedArray<ScFeedContent, MAX_CONCURRENT_DOWNLOADS> m_Contents;

    atArray<ScPendingTileCache> m_Important;
    atArray<ScPendingTileCache> m_Common;

    // How many we download at once
    unsigned m_MaxConcurrentDownloads;

    // If true a cache is finished
    bool m_Done;
};

enum class MetaDownloadAllowance
{
    None,   // Download disabled
    Finish, // A started download is allowed to finish but no new downloads are allowed
    All     // Downloads are fully enabled
};

//PURPOSE
//  This is the base class to handle loading feed, priority feed & co.
class CSocialClubFeedTilesRow
{
public:
    friend class CSocialClubFeedTilesMgr;

    CSocialClubFeedTilesRow();
    virtual ~CSocialClubFeedTilesRow() {}

    //PURPOSE
    //  Set up the manager
    virtual void Init(const ScFeedPostTypeFilter filter, const unsigned maxRows, const atHashString rowArea, CSocialClubFeedTilesMgr* owner);

    //PURPOSE
    //  Clear the manager and all feeds. Can be called in case of a sign out or such things
    virtual void Shutdown();

    //PURPOSE
    //  Turn the meta/text download on or off
    void EnableMetaDownload(const MetaDownloadAllowance val);

    //PURPOSE
    //  Clears all feeds and their content
    void ClearFeeds();

    //PURPOSE
    //  Clears the loaded data (textures) of the feeds
    void ClearContent();

    //PURPOSE
    //  Requests to download the tiles again if they're out of date
    void RenewTiles();

    //PURPOSE
    //  Requests to download the tiles again.
    //  This will ignore the last download time but if there's one in progress it won't download
    void ForceRenewTiles();

    //PURPOSE
    //  Add a post to the grid
    void AddPost(const ScFeedPost& post);

    //PURPOSE
    //  Parse and add a post to the grid
    void AddPost(const char* feedPost, const unsigned requestId);

    //PURPOSE
    //  Returns the index in the array or -1 if not found
    int GetTileIdx(const RosFeedGuid& feedId) const;

    //PURPOSE
    //  Returns tile or nullptr if not found
    const ScFeedTile* GetTile(const RosFeedGuid& feedId) const;
    const ScFeedTile* GetTile(const unsigned column, const unsigned row) const;
    const ScFeedTile* GetTile(const unsigned column, const unsigned row, const unsigned layoutPublishId) const;

    //PURPOSE
    //  The metadata download state
    TilesMetaDownloadState GetMetaDownloadState() const { return m_Info.m_MetaDownloadState; }

    const ScFeedTileRow& GetTiles() const { return m_Row; }

    ScFeedTileRow& GetTiles() { return m_Row; }

    //PURPOSE
    //  The error code from the feed request. When 0 there was no error.
    int GetErrorCode() const { return m_Info.m_ErrorCode; }

    void PreloadUI(const unsigned numBig, const unsigned numSmall = 0xff);

    //PURPOSE
    //  Checks and updates the preload state
    void CheckUIPreload();

    TilesImagesDownloadState GetUIPreloadState() const { return m_UIPreloadState; }

    //PURPOSE
    //  Clears the row (deletes the feed and clears the images)
    //PARAMS
    //  row            - The row to work on
    static void ClearRow(ScFeedTileRow& row);

    //PURPOSE
    //  Loads the images for a row
    //PARAMS
    //  row            - The row to work on
    //  func           - A function that can be passed in to edit the download infos before they're added. Can be null
    static void LoadRowContent(ScFeedTileRow& row, const AmendDownloadInfoFunc& func);

    //PURPOSE
    //  Unloads the images for a feeds
    //PARAMS
    //  row            - The row to work on
    static void ClearRowContent(ScFeedTileRow& row);

    //PURPOSE
    //  Adds a feed post to the row
    //PARAMS
    //  feed           - The feed post to insert
    //  row            - The row to work on
    //  orderIndex     - The order used to sort the list. Successive items with the same order will be dropped
    static bool InsertIntoRow(const ScFeedPost* feed, ScFeedTileRow& row, const unsigned orderIndex);

    //PURPOSE
    //  Adds a feed post to the row
    //PARAMS
    //  feed           - The feed post to insert
    //  row            - The row to work on
    //  priority       - The priority with the slightly unconventional rule:
    //                   0 ... lowest priority
    //                   prio(N) > prio(N+1) when N != 0
    //                   if entries have the same priority the one added first stays in front
    static bool InsertIntoRowByPriority(const ScFeedPost* feed, ScFeedTileRow& row, int priority);

protected:
    void InnerClearFeeds();

    //PURPOSE
    //  Called when we have received a response.
    void ResponseStarting(const unsigned requestId);

    //PURPOSE
    //  Finalize the request result
    //  ResponseStarting and ResponseDone happen in the same frame one after the other
    virtual void ResponseDone(const netStatus* result, const unsigned requestId);

protected:
    ScFeedTileDownloadInfo m_Info;
    ScFeedTileRow m_Row;
    ScFeedPostTypeFilter m_Filter;
    ScFeedRequestCallback m_RequestCb;
    ScFeedPostCallback m_PostCb;
    atHashString m_RowArea;
    CSocialClubFeedTilesMgr* m_Owner;
    TilesImagesDownloadState m_UIPreloadState;
    MetaDownloadAllowance m_MetaDownloadAllowance;
    int m_ErrorCode;
};

class CSocialClubFeedLanding : public CSocialClubFeedTilesRow
{
public:
    CSocialClubFeedLanding();
    ~CSocialClubFeedLanding();

    void Init(const ScFeedPostTypeFilter filter, const unsigned maxRows, const atHashString rowArea, CSocialClubFeedTilesMgr* owner) override;
    void Shutdown() override;

    void Clear();
    void Update();

protected:
    void ResponseDone(const netStatus* result, const unsigned requestId) override;
    void StartRetry();

public:
    static const unsigned COLUMNS;
    static const unsigned ROWS;
    static const int MIN_RETRY_INTERVAL_MS = 5 * 1000;
    static const int INITIAL_MAX_RETRY_INTERVAL_MS = 8 * 1000;
    static const int MAX_RETRY_INTERVAL_MS = 10 * 60 * 1000;
    static const int EXPONENTIAL_BACKOFF = 2 * 100;

protected:
    netRetryTimer m_TilesRetryTimer;
    bool m_RetryPending;
};

//PURPOSE
//  Ensure to download the tunable while on the landing page.
class PreLaunchData
{
public:
    PreLaunchData();
    ~PreLaunchData();

    void Update();

    void SetEnabled(const bool isEnabled);

    bool IsEnabled() const { return m_Enabled; }

public:
    static const int MIN_RETRY_INTERVAL_MS = 5 * 1000;
    static const int INITIAL_MAX_RETRY_INTERVAL_MS = 8 * 1000;
    static const int MAX_RETRY_INTERVAL_MS = 20 * 60 * 1000;
    static const int EXPONENTIAL_BACKOFF = 2 * 100;

protected:
    netRetryTimer m_TunableRetryTimer;
    bool m_Enabled;
};

// There are some posts which only show in a specific area so we need distinct args.
// Polling isn't possible due to the ui being on another thread.
class PriorityFeedAreaSettings
{
public:
	PriorityFeedAreaSettings() : m_SkipButtonDealySec(0), m_HasAnyVisiblePriorityFeed(0) {};

	static PriorityFeedArea GetAreaFromHash(const atHashString areaHash);
	static PriorityFeedArea GetAreaFromHash(const ScFeedPost& post);
	static unsigned GetSkipDelay(const ScFeedPost& post);

public:
	unsigned m_SkipButtonDealySec; // The time before the buttons shows up.
	bool m_HasAnyVisiblePriorityFeed; // True when any feed is shown in that area.
};

//PURPOSE
//  Manages the buttons/tiles coming from the feed system and used by the game to fill in the menus
//  In our case this is mainly the landing page. A page is composed of multiple rows and each rows can have multiple tiles
class CSocialClubFeedTilesMgr : public CloudListener
{
public:
    friend class CSocialClubFeedTilesRow;

    CSocialClubFeedTilesMgr();
    virtual ~CSocialClubFeedTilesMgr();

    static CSocialClubFeedTilesMgr& Get();

    //PURPOSE
    //  Set up the manager
    void Init();

    //PURPOSE
    //  Clear the manager and all feeds. Can be called in case of a sign out or such things
    void Shutdown();

    //PURPOSE
    //  Needs to be called so the download operations can be executed and processed
    void Update();

    //PURPOSE
    //  Optionally set the function to change at which resolution we download the images
    void SetAmendDownloadFunc(const AmendDownloadInfoFunc& func);

    //PURPOSE
    //  Clears all feeds and their content
    void ClearFeeds();

    //PURPOSE
    //  Clears the landing page tiles and also priority & co.
    void ClearAll();

    //PURPOSE
    //  Clears the loaded data (textures) of the feeds
    void ClearFeedsContent(const bool onlyBigFeed);

    //PURPOSE
    //  Download and cache all tiles
    //PARAMS
    //  includePriorityFeed    - If true we also cache the data for the priority feed
    //  maxConcurrentDownloads - how many we try to download at once. A value of 0 means we stop caching
    void CacheTiles(const bool includePriorityFeed, const unsigned maxConcurrentDownloads);

    //PURPOSE
    //  Load the textures of a specific tile
    //PARAMS
    //  feedId         - The id of the feed/tile
    //  includeBigFeed - If true we also load the big feed data
    void LoadTileContentById(const RosFeedGuid& feedId, const bool includeBigFeed);

    //PURPOSE
    //  Load the textures of a specific tile
    void LoadTileContent(ScFeedTile& tile, const bool includeBigFeed);

    //PURPOSE
    //  Clears the textures of a specific tile
    //PARAMS
    //  feedId         - The id of the feed/tile
    //  onlyBigFeed    - If true we only clear the big feed data
    void ClearTileContentById(const RosFeedGuid& feedId, const bool onlyBigFeed);

    //PURPOSE
    //  Clears the textures of a specific tile
    void ClearTileContent(ScFeedTile& tile, const bool onlyBigFeed);

    //PURPOSE
    void TileFocussed(const RosFeedGuid& feedId);

    //PURPOSE
    // Swap the data around. Can be called on the UI thread.
    bool SwapPendingData();

    //PURPOSE
    //  The images caching state
    TilesImagesDownloadState GetCacheState() const;

    //PURPOSE
    //  Access a tile by the feedId. Also includes priority, loading & co.
    const ScFeedTile* GetTileIncludeAll(const RosFeedGuid& feedId);

    //PURPOSE
    //  Returns true if we fell back to the locally stored tiles.
    bool IsUsingOfflineGrid() const { return m_ForcedOffline == true; }
    void ForceOffline();
    void SetCacheImagesHint(const bool canDownlaodImages);

    //PURPOSE
    //  The manager for the landing feed. m_ActiveLanding is never null
    CSocialClubFeedLanding& Landing() { return m_Landing; }
    CSocialClubFeedLanding& OfflineLanding() { return m_OfflineLanding; }

    //PURPOSE
    //  The manager for the priority feed
    CSocialClubFeedTilesRow& Priority() { return m_Priority; }

    //PURPOSE
    //  The manager for the heist feed
    CSocialClubFeedTilesRow& Heist() { return m_Heist; }

    //PURPOSE
    //  The manager for the series feed
    CSocialClubFeedTilesRow& Series() { return m_Series; }

    //PURPOSE
    //  The row based on the UI area
    CSocialClubFeedTilesRow* Row(const atHashString uiArea);

    //PURPOSE
    //  The landing page prelaunch data
    PreLaunchData& PrelaunchData() { return m_PrelaunchData; }

	//PURPOSE
	//  Data of the 'big feed' (== large background images)
	ScFeedContent* GetBigFeedContentById( const RosFeedGuid& feedId );

    //PURPOSE
    //  Save a priority feed as seen by the user. It's saved in the player profile.
    void PriorityFeedSeen(const RosFeedGuid guid);

    //PURPOSE
    //  Returns how often a post has been seen.
    //  This value is only kept for posts which have an impression limit
    int GetPriorityFeedImpressionCount(const RosFeedGuid guid, int& slotIdx) const;

    //PURPOSE
    // Returns true if the player has seen the post enough time to reach the impression limit
    bool HasSeenPriorityFeed(const RosFeedGuid guid) const;

    //PURPOSE
    //  True if any of the priority feeds hasn't been seen yet
    //PARAMS
    //  tabArea - sp or mp
    //  veryFirstLaunch - Tue when this is the first time the priority feed opens regardless of the area.
    bool HasUnseenPriorityFeed(const PriorityFeedArea tabArea, const bool veryFirstLaunch) const;

    //PURPOSE
    //  True if any of the current priority feeds can be displayed to the user in that area.
    bool HasAnyVisiblePriorityFeed(const PriorityFeedArea tabArea) const;

	//PURPOSE
	//  The button delay of the first visible post in the specified area
	unsigned GetSkipButtonDelaySec(const PriorityFeedArea tabArea) const;

    //PURPOSE
    //  The min time in seconds we wait to redownload landing page and priority feed tiles.
    //  The timeout is ignored in case of a sign out and such things where we want to refresh the data.
    static u64 GetRedownloadIntervalSec();

    BANK_ONLY(void InitWidgets(bkBank& bank));

protected:
    void InnerClearFeeds();

    //PURPOSE
    //  Update the state of the currently highlighted tile. Used to download neighboring images
    void UpdateFocussedTile();

    //PURPOSE
    //  Update the images which are being cached
    void UpdateCaching();

    //PURPOSE
    //  Updates the visibility check. Has to be done on the main thread.
    void UpdateHasAnyVisiblePriorityFeed();

    //PURPOSE
    //  Goes through all tiles and runs the function on them
    void IterateTiles(const bool includePriorityFeed, LambdaRef<void(bool, unsigned, ScFeedTile&)> func);
    void IteratePriorityFeedTiles(LambdaRef<void(bool, unsigned, ScFeedTile&)> func);

    //PURPOSE
    //  Loads the big feed data
    void LoadBigFeedContent(const ScFeedTile& tile);

    //PURPOSE
    //  Loads the fallback data for when the feed system is down.
    bool LoadOfflineGrid();

    //PURPOSE
    //  Is called when a cloud status or file changes
    void OnCloudEvent(const CloudEvent* pEvent);

    //PURPOSE
    //  Called when the FeedMgr finishes a read operation
    static void ResponseCallback(const netStatus& result, const ScFeedRequest& request);

    //PURPOSE
    //  Returns the on which row the feed will end up
    static int GetRowIndexFromFeed(const ScFeedPost* feed);

    //PURPOSE
    //  Clear the data of the 'big feed' (== large background images)
    void ClearBigFeedContentById(const RosFeedGuid& feedId);

    //PURPOSE
    //  Used for presence events (sign-in/out) to clear data when needed
    void OnPresenceEvent(const rlPresenceEvent* pEvent);

protected:
    // How long we keep valid downloaded data before we check for new one.
    // There's no timer which forces a read when it expires. This is only used when someone calls RenewFeeds()
    static const u64 REDOWNLOAD_WAIT_TIME_SEC;

    // The number of background images we keep in memory
    static const unsigned MAX_BIG_FEEDS_CONTENTS = 10;

    // The string used to identify dev-only tiles
    static const char* DEV_TILE_DLINK_DATA;

    CSocialClubFeedLanding m_Landing;
    CSocialClubFeedLanding m_OfflineLanding;
    CSocialClubFeedTilesRow m_Priority;
    CSocialClubFeedTilesRow m_Heist;
    CSocialClubFeedTilesRow m_Series;

    PreLaunchData m_PrelaunchData;
    ScTilesCaching m_Cacher;
    // the tile currently highlighted by the UI
    ScFocussedTile m_FocussedTile;

    atFixedArray<ScFeedContent, MAX_BIG_FEEDS_CONTENTS> m_FeedsContent;

    TilesImagesDownloadState m_CachingState;

    rlPresence::Delegate m_PresenceDlgt;

    AmendDownloadInfoFunc m_AmendDownloadFunc;

    // True if the game is in a state where preloading won't impact gameplay
    bool m_CanCacheImagesHint;

    // True if we need to do a full clear (after a profile change)
    bool m_ClearAllPending;

    // True when we entered the MP page but we haven't (fully) received the data
    bool m_ForcedOffline;

    // Settings for the individual priority feed areas
    PriorityFeedAreaSettings m_AreaSettings[static_cast<unsigned>(PriorityFeedArea::Count)];
};

#endif //GEN9_STANDALONE_ENABLED

#endif //INC_SOCIALCLUBFEEDTILESMGR_H_