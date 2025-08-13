#include "socialclubfeedtilesmgr.h"

#if GEN9_STANDALONE_ENABLED

// Rage
#include "rline/rldiag.h"
#include "rline/rlsystemui.h"
#include "rline/scfriends/rlscfriends.h"
#include "rline/scauth/rlscauth.h"
#include "system/param.h"
#include "system/stack.h"
#include "text/TextFile.h"
#include "file/device.h"
#include "parsercore/streamxml.h"

// framework
#include "fwnet/netchannel.h"
#include "fwnet/netcloudfiledownloadhelper.h"
#include "fwsys/fileExts.h"

// Game
#include "network/Crews/NetworkCrewDataMgr.h"
#include "network/general/networkutil.h"
#include "core/gamesessionstatemachine.h"
#include "event/eventnetwork.h"
#include "event/eventgroup.h"

NETWORK_OPTIMISATIONS()

#undef __net_channel
#define __net_channel net_feed
RAGE_DECLARE_SUBCHANNEL(net, feed);

#if __FINAL
#define ALLOW_DEV_TILES 0
#else
#define ALLOW_DEV_TILES 1
#endif

TEST_FAKE_FEED_ONLY(NOSTRIP_XPARAM(scDebugFeedLanding));
TEST_FAKE_FEED_ONLY(NOSTRIP_XPARAM(scCloudLanding));
PARAM(frontendMpForceIntro, "Force the simple screen");
PARAM(SCFeedForceOfflineTiles, "Set this to always use the offline tile data");
PARAM(SCFeedHideDevTiles, "Set this to hide dev tiles which would be shown on dev builds");

const u64 CSocialClubFeedTilesMgr::REDOWNLOAD_WAIT_TIME_SEC = 60 * 30;
const char* CSocialClubFeedTilesMgr::DEV_TILE_DLINK_DATA = "devonly";

const unsigned CSocialClubFeedLanding::COLUMNS = 3;
const unsigned CSocialClubFeedLanding::ROWS = 4;

const unsigned ScFeedTile::MAX_LANDING_ITEMS = (2 * 8) + 2; // The largest one has 8 items. We keep space for two to pick the 'best' one.
const unsigned ScFeedTile::MAX_CAROUSEL_ITEMS = 16;
const unsigned ScFeedTile::MAX_PRIORITY_ITEMS = 8;
const unsigned ScFeedTile::MIN_PRIORITY_POSTS_PRELOAD = 2;

bool IsImportant(const unsigned x, const bool isLandingTile, bool& bigFeedImportant);

#if 0
CompileTimeAssert(ScFeedTile::MAX_PRIORITY_ITEMS <= (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST + 1));
#endif

//#####################################################################################

ScTilePreloadParams::ScTilePreloadParams()
 : m_StartTime(0)
 , m_Enabled(false)
 , m_IncludePriorityFeed(false)
{

}

//#####################################################################################

ScFocussedTile::ScFocussedTile()
    : m_NeighboursRequested(false)
{

}

void
ScFocussedTile::Clear()
{
    m_Tile = RosFeedGuid();
    m_Timeout.Clear();
    m_NeighboursRequested = false;
}

//#####################################################################################

ScFeedTileDownloadInfo::ScFeedTileDownloadInfo()
    : m_FeedRequestId(ScFeedRequest::INVALID_REQUEST_ID)
    , m_MetaDownloadState(TilesMetaDownloadState::None)
    , m_LastDownloadAttempt(0)
    , m_LastDownloadSuccess(0)
	, m_ErrorCode(0)
{

}

//#####################################################################################

ScFeedTile::ScFeedTile()
    : m_Order(0)
    , m_TilePreloaded(false)
{

}

void
ScFeedTile::Clear()
{
    m_Feed = ScFeedPost();
    m_FeedContent.Clear();
    m_TilePreloaded = false;
}

void
ScFeedTile::LoadContent(const AmendDownloadInfoFunc& func)
{
    m_FeedContent.RetrieveContent(&m_Feed, func, false);
}

void
ScFeedTile::ClearContent()
{
    m_FeedContent.Clear();
    // Don't reset m_TilePreloaded
}

bool
ScFeedTile::GetCoords(unsigned& col, unsigned& row) const
{
    return GetCoords(m_Feed, col, row);
}

bool
ScFeedTile::GetCoords(const ScFeedPost& post, unsigned& col, unsigned& row)
{
    const ScFeedParam* c = post.GetParam(ScFeedParam::COL_ID);
    const ScFeedParam* r = post.GetParam(ScFeedParam::ROW_ID);

    col = c ? static_cast<unsigned>(c->ValueAsInt64()) : 0u;
    row = r ? static_cast<unsigned>(r->ValueAsInt64()) : 0u;

    return (c != nullptr || r != nullptr);
}

//#####################################################################################

ScTilesCaching::ScTilesCaching()
    : m_MaxConcurrentDownloads(1) // one at a time by default
    , m_Done(false)
{
    m_Contents.SetCount(m_Contents.GetMaxCount());
}

void
ScTilesCaching::Clear()
{
    m_Important.clear();
    m_Common.clear();
    m_Done = false;

    for (unsigned i = 0; i < static_cast<unsigned>(m_Contents.size()); ++i)
    {
        m_Contents[i].Clear();
    }
}

void
ScTilesCaching::SetMaxDownloads(const unsigned maxDownloads)
{
    m_MaxConcurrentDownloads = rage::Max(rage::Min(maxDownloads, MAX_CONCURRENT_DOWNLOADS), 1u);
}

void
ScTilesCaching::CacheTile(const RosFeedGuid& guid, const bool important)
{
    ScPendingTileCache val = {guid, false};

    if (important)
    {
        m_Important.PushAndGrow(val);
    }
    else
    {
        m_Common.PushAndGrow(val);
    }
}

void
ScTilesCaching::RemoveTile(const RosFeedGuid& guid)
{
    for (int i = 0; i < m_Important.GetCount(); ++i)
    {
        if (m_Important[i].m_Guid == guid)
        {
            m_Important.Delete(i);
            return;
        }
    }

    for (int i = 0; i < m_Common.GetCount(); ++i)
    {
        if (m_Common[i].m_Guid == guid)
        {
            m_Common.Delete(i);
            return;
        }
    }

    m_Done = !HasAny();
}

//#####################################################################################

CSocialClubFeedTilesRow::CSocialClubFeedTilesRow()
 : m_Filter(ScFeedPostTypeFilter::Invalid)
 , m_Owner(nullptr)
 , m_UIPreloadState(TilesImagesDownloadState::None)
 , m_MetaDownloadAllowance(MetaDownloadAllowance::All)
 , m_ErrorCode(0)
{

}

void
CSocialClubFeedTilesRow::Init(const ScFeedPostTypeFilter filter, const unsigned maxRows, const atHashString rowArea, CSocialClubFeedTilesMgr* owner)
{
    m_Filter = filter;
    m_Row.Reserve(maxRows);
    m_RowArea = rowArea;
    m_Owner = owner;
    m_MetaDownloadAllowance = MetaDownloadAllowance::All;

    if (m_RequestCb.IsValid() && m_PostCb.IsValid())
    {
        return;
    }

    m_RequestCb = [this](const netStatus& result, const ScFeedRequest& request)
    {
        if (result.Pending())
        {
            ResponseStarting(request.GetRequestId());
            return;
        }

        ResponseDone(&result, request.GetRequestId());
    };

    m_PostCb = [this](const char* feedPost, const ScFeedRequest& request)
    {
        AddPost(feedPost, request.GetRequestId());
    };
}

void
CSocialClubFeedTilesRow::Shutdown()
{
    ClearFeeds();
    m_Owner = nullptr;
}

void
CSocialClubFeedTilesRow::EnableMetaDownload(const MetaDownloadAllowance val)
{
    gnetDebug1("[%s] EnableMetaDownload val[%d]", m_RowArea.TryGetCStr(), static_cast<int>(val));

    m_MetaDownloadAllowance = val;

    if (m_MetaDownloadAllowance == MetaDownloadAllowance::None)
    {
        m_Info.m_FeedRequestId = ScFeedRequest::INVALID_REQUEST_ID;
    }
}

void
CSocialClubFeedTilesRow::ClearFeeds()
{
    gnetDebug1("[%s] ClearFeeds", m_RowArea.TryGetCStr());

    InnerClearFeeds();
    m_Info = ScFeedTileDownloadInfo();
}

void
CSocialClubFeedTilesRow::InnerClearFeeds()
{
    ClearContent();
    CSocialClubFeedTilesRow::ClearRow(m_Row);
}

void
CSocialClubFeedTilesRow::ClearContent()
{
    gnetDebug1("[%s] ClearFeedsContent. UIPreloadState is None", m_RowArea.TryGetCStr());

    m_UIPreloadState = TilesImagesDownloadState::None;

    if (m_Owner)
    {
        for (unsigned i = 0; i < m_Row.GetCount(); ++i)
        {
            if (m_Row[i] == nullptr)
            {
                continue;
            }

            m_Owner->ClearBigFeedContentById(m_Row[i]->m_Feed.GetId());
        }
    }

    CSocialClubFeedTilesRow::ClearRowContent(m_Row);
}

void
CSocialClubFeedTilesRow::RenewTiles()
{
    const u64 curreTime = rlGetPosixTime();
    const u64 successDiff = curreTime - m_Info.m_LastDownloadSuccess;

    const u64 waitTimeSec = CSocialClubFeedTilesMgr::GetRedownloadIntervalSec();

    // If not enough time has passed and we have valid data we don't renew
    if (successDiff < waitTimeSec || waitTimeSec == 0 || m_MetaDownloadAllowance != MetaDownloadAllowance::All)
    {
        gnetDebug2("CSocialClubFeedTilesRow - RenewFeeds - skipping. %u seconds passed since the last successful read", static_cast<unsigned>(successDiff));
        return;
    }

    ForceRenewTiles();
}

void
CSocialClubFeedTilesRow::ForceRenewTiles()
{
    gnetDebug1("[%s] ForceRenewTiles", m_RowArea.TryGetCStr());

    if (m_Info.m_FeedRequestId != ScFeedRequest::INVALID_REQUEST_ID)
    {
        gnetWarning("CSocialClubFeedTilesRow - RenewFeeds skipped as it's already in progress with. RequestId[%u]", m_Info.m_FeedRequestId);
        return;
    }

    int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
    {
        gnetDebug1("CSocialClubFeedTilesRow - ForceRenewFeeds - Skipped. Invalid gamer index");
        return;
    }

    const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
    if (!cred.IsValid())
    {
        gnetDebug1("CSocialClubFeedTilesRow - ForceRenewFeeds - Skipped. Invalid credentials");
        return;
    }

    gnetDebug1("[%s] UIPreloadState is None", m_RowArea.TryGetCStr());

    m_Info.m_ErrorCode = 0;
    m_Info.m_MetaDownloadState = TilesMetaDownloadState::DownloadingMeta;
    m_UIPreloadState = TilesImagesDownloadState::None;

    ScFeedFilterId filter;
    filter.SetFeedTypeFiler(m_Filter);

    m_Info.m_LastDownloadAttempt = rlGetPosixTime();

    const unsigned num = m_Row.GetCapacity();
    m_Info.m_FeedRequestId = CSocialClubFeedMgr::Get().RenewFeeds(filter, num, m_RequestCb, m_PostCb);

    gnetDebug2("CSocialClubFeedTilesRow - RenewFeeds. RequestId[%u]", m_Info.m_FeedRequestId);

    // If for any reason the request fails we see if we have the data locally (very unlikely) or we just fail when the wall is null
    if (m_Info.m_FeedRequestId == ScFeedRequest::INVALID_REQUEST_ID)
    {
        ResponseDone(nullptr, m_Info.m_FeedRequestId);
    }
}

void
CSocialClubFeedTilesRow::PreloadUI(const unsigned numBig, const unsigned numSmall)
{
    gnetDebug1("[%s] PreloadUI numBig[%u] numSmall[%u] UIPreloadState is DownloadingBasics", m_RowArea.TryGetCStr(), numBig, numSmall);

    gnetAssertf(numSmall >= numBig, "The total (numSmall [%u]) should be >= numBig [%u]", numSmall, numBig);

    const int num = rage::Min(m_Row.GetCount(), static_cast<int>(numSmall));
    const int maxBig = rage::Min(num, static_cast<int>(numBig));

    for (int i = 0; i < num; ++i)
    {
        CSocialClubFeedTilesMgr::Get().LoadTileContent(*(m_Row[i]), i < maxBig);
    }

    m_UIPreloadState = TilesImagesDownloadState::DownloadingBasics;
}

void
CSocialClubFeedTilesRow::CheckUIPreload()
{
    if (m_UIPreloadState != TilesImagesDownloadState::DownloadingBasics)
    {
        return;
    }

    bool done = true;

    for (ScFeedTile* tile : m_Row)
    {
        if (tile && tile->m_FeedContent.IsSet() && !tile->m_FeedContent.IsDone())
        {
            done = false;
            break;
        }

        ScFeedContent* bfc = m_Owner ? m_Owner->GetBigFeedContentById(tile->m_Feed.GetId()) : nullptr;
        if (bfc && bfc->IsSet() && !bfc->IsDone())
        {
            done = false;
            break;
        }
    }

    if (done)
    {
        gnetDebug1("[%s] UIPreloadState is Ready", m_RowArea.TryGetCStr());
        m_UIPreloadState = TilesImagesDownloadState::Ready;
    }
}

void
CSocialClubFeedTilesRow::AddPost(const ScFeedPost& post)
{
    const ScFeedParam* prio = post.GetParam(ScFeedParam::PRIORITY_PRIORITY_ID);
    const int priority = prio ? static_cast<int>(prio->ValueAsInt64()) : 0;

    CSocialClubFeedTilesRow::InsertIntoRowByPriority(&post, m_Row, priority);
}

void
CSocialClubFeedTilesRow::AddPost(const char* feedPost, const unsigned requestId)
{
    rtry
    {
        ScFeedPost post;

        rcheck(feedPost, catchall, gnetError("peedPost is null"));
        rcheck(requestId == m_Info.m_FeedRequestId, catchall, gnetError("Mismatching request. Incoming[%u] Local[%u]", requestId, m_Info.m_FeedRequestId));
        rcheck(post.ParseContent(feedPost), catchall, gnetError("Parsing feed failed"));
        rcheck(!CSocialClubFeedMgr::IsUgcRestrictedContent(post.GetFeedType()), catchall, gnetDebug1("Skipping feed msg [%d], UGC is restricted", post.GetFeedType()));

        AddPost(post);
    }
    rcatchall
    {

    }
}

int
CSocialClubFeedTilesRow::GetTileIdx(const RosFeedGuid& feedId) const
{
    for (unsigned i = 0; i < m_Row.GetCount(); ++i)
    {
        if (m_Row[i] != nullptr && m_Row[i]->m_Feed.GetId() == feedId)
        {
            return i;
        }
    }

    return -1;
}

const ScFeedTile*
CSocialClubFeedTilesRow::GetTile(const RosFeedGuid& feedId) const
{
    const int idx = GetTileIdx(feedId);
    return (idx == -1 || idx >= m_Row.GetCount()) ? nullptr : m_Row[idx];
}

const ScFeedTile*
CSocialClubFeedTilesRow::GetTile(const unsigned column, const unsigned row) const
{
	return GetTile(column, row, 0);
}

const ScFeedTile*
CSocialClubFeedTilesRow::GetTile(const unsigned column, const unsigned row, const unsigned layoutPublishId) const
{
    for (const ScFeedTile* tile : m_Row)
    {
        if (!tile)
        {
            continue;
        }

        unsigned c = 0, r = 0;

        // If we have multiple posts for the same slot but the highest
        // priority one is hidden we keep on looking for a visible one.
        if (tile->GetCoords(c, r) && row == r && column == c && !ScFeedPost::IsHiddenByDlinkFlag(tile->m_Feed))
        {
            const ScFeedParam* lpidParam = tile->m_Feed.GetParam(ScFeedParam::LAYOUT_PUBLISH_ID);
            const unsigned lpid = lpidParam ? (unsigned)lpidParam->ValueAsInt64() : 0;

            // If layoutPublishId is zero we accept any post at the right coords
            if (layoutPublishId == 0 || layoutPublishId == lpid)
            {
                return tile;
            }
        }
    }

    return nullptr;
}

void
CSocialClubFeedTilesRow::ResponseStarting(const unsigned requestId)
{
    if (requestId != m_Info.m_FeedRequestId)
    {
        gnetWarning("RequestDone- Mismatching request. Incoming[%u] Local[%u]", requestId, m_Info.m_FeedRequestId);
        return;
    }

    InnerClearFeeds();
}

void
CSocialClubFeedTilesRow::ResponseDone(const netStatus* result, const unsigned requestId)
{
    gnetDebug1("[%s] ResponseDone", m_RowArea.TryGetCStr());

    if (requestId != m_Info.m_FeedRequestId)
    {
        gnetWarning("RequestDone- Mismatching request. Incoming[%u] Local[%u]", requestId, m_Info.m_FeedRequestId);
        return;
    }

    const bool success = result && result->Succeeded();
    const int resultCode = result ? result->GetResultCode() : static_cast<int>(rlFeedError::GENERIC_ERROR.GetHash());

    m_Info.m_ErrorCode = 0;

    if (success && m_Row.GetCount() > 0)
    {
        m_Info.m_LastDownloadSuccess = rlGetPosixTime();
    }

    if (!success)
    {
        InnerClearFeeds();
        m_Info.m_ErrorCode = resultCode;
    }

    CSocialClubFeedMgr::Get().FireScriptEvent(m_RowArea);

    gnetDebug1("[%s] m_MetaDownloadState is MetaReady", m_RowArea.TryGetCStr());

    m_Info.m_MetaDownloadState = TilesMetaDownloadState::MetaReady;
    m_Info.m_FeedRequestId = ScFeedRequest::INVALID_REQUEST_ID;
}


void
CSocialClubFeedTilesRow::ClearRow(ScFeedTileRow& row)
{
    for (unsigned x = 0; x < row.GetCount(); ++x)
    {
        ScFeedTile* tile = row[x];

        if (tile)
        {
            tile->Clear();
        }

        delete tile;
        row[x] = nullptr;
    }

    row.clear();
}

void
CSocialClubFeedTilesRow::LoadRowContent(ScFeedTileRow& row, const AmendDownloadInfoFunc& func)
{
    for (unsigned x = 0; x < row.GetCount(); ++x)
    {
        ScFeedTile* tile = row[x];

        if (tile)
        {
            tile->LoadContent(func);
        }
    }
}

void
CSocialClubFeedTilesRow::ClearRowContent(ScFeedTileRow& row)
{
    for (unsigned x = 0; x < row.GetCount(); ++x)
    {
        ScFeedTile* tile = row[x];

        if (tile)
        {
            tile->ClearContent();
        }
    }
}

bool
CSocialClubFeedTilesRow::InsertIntoRow(const ScFeedPost* feed, ScFeedTileRow& row, const unsigned newOrder)
{
    rtry
    {
        rcheck(feed, catchall, gnetError("The feed is null"));
        rcheck(row.GetCount() < row.GetCapacity(), catchall, gnetError("The row is full. Feed[%s]", feed->GetId().ToFixedString().c_str()));

        for (int i = 0; i < row.GetCount(); ++i)
        {
            rcheck(row[i] != nullptr && row[i]->m_Order != static_cast<int>(newOrder), catchall, gnetDebug2("Duplicate item at index [%d]. Feed[%s]", i, feed->GetId().ToFixedString().c_str()));

            if (row[i]->m_Order > static_cast<int>(newOrder))
            {
                ScFeedTile* tile = rage_new ScFeedTile();
                rcheck(tile, catchall, gnetError("Failed to allocate the tile. Feed[%s]", feed->GetId().ToFixedString().c_str()));

                tile->m_Feed = *feed;
                tile->m_Order = static_cast<int>(newOrder);

                row.Insert(i) = tile;
                return true;
            }
        }

        ScFeedTile* tile = rage_new ScFeedTile();
        rcheck(tile, catchall, gnetError("Failed to allocate the tile. Feed[%s]", feed->GetId().ToFixedString().c_str()));

        tile->m_Feed = *feed;
        tile->m_Order = static_cast<int>(newOrder);

        row.Push(tile);

        return true;
    }
        rcatchall
    {

    }

    return false;
}

bool
CSocialClubFeedTilesRow::InsertIntoRowByPriority(const ScFeedPost* feed, ScFeedTileRow& row, int priority)
{
    rtry
    {
        rcheck(feed, catchall, gnetError("The feed is null"));
        rcheck(row.GetCount() < row.GetCapacity(), catchall, gnetError("The row is full. Feed[%s]", feed->GetId().ToFixedString().c_str()));

        if (priority == 0)
        {
            priority = 0x7fffffff;
        }

        for (int i = 0; i < row.GetCount(); ++i)
        {
            if (row[i]->m_Order > priority)
            {
                ScFeedTile* tile = rage_new ScFeedTile();
                rcheck(tile, catchall, gnetError("Failed to allocate the tile. Feed[%s]", feed->GetId().ToFixedString().c_str()));

                tile->m_Feed = *feed;
                tile->m_Order = priority;

                row.Insert(i) = tile;
                return true;
            }
        }

        ScFeedTile* tile = rage_new ScFeedTile();
        rcheck(tile, catchall, gnetError("Failed to allocate the tile. Feed[%s]", feed->GetId().ToFixedString().c_str()));

        tile->m_Feed = *feed;
        tile->m_Order = priority;

        row.Push(tile);

        return true;
    }
    rcatchall
    {

    }

    return false;
}

//#####################################################################################

CSocialClubFeedLanding::CSocialClubFeedLanding()
    : m_RetryPending(false)
{

}

CSocialClubFeedLanding::~CSocialClubFeedLanding()
{

}

void
CSocialClubFeedLanding::Init(const ScFeedPostTypeFilter filter, const unsigned maxRows, const atHashString rowArea, CSocialClubFeedTilesMgr* owner)
{
    CSocialClubFeedTilesRow::Init(filter, maxRows, rowArea, owner);
    m_TilesRetryTimer.InitMilliseconds(MIN_RETRY_INTERVAL_MS, INITIAL_MAX_RETRY_INTERVAL_MS);
    m_TilesRetryTimer.Stop();
}

void
CSocialClubFeedLanding::Shutdown()
{
    CSocialClubFeedTilesRow::Shutdown();
    m_TilesRetryTimer.Shutdown();
    m_RetryPending = false;
}

void
CSocialClubFeedLanding::Clear()
{
    m_RetryPending = false;
    ClearFeeds();
}

void
CSocialClubFeedLanding::Update()
{
    CheckUIPreload();

    m_TilesRetryTimer.Update();

    if (m_RetryPending && m_TilesRetryTimer.IsTimeToRetry())
    {
        m_RetryPending = false;
        RenewTiles();
    }

    if (m_Info.m_FeedRequestId != ScFeedRequest::INVALID_REQUEST_ID && !CSocialClubFeedMgr::Get().HasRequestPending(m_Info.m_FeedRequestId))
    {
        gnetError("CSocialClubFeedLanding - RequestId[%u] is missing", m_Info.m_FeedRequestId);
        ResponseDone(nullptr, m_Info.m_FeedRequestId);
    }
}

void
CSocialClubFeedLanding::ResponseDone(const netStatus* result, const unsigned requestId)
{
    const bool current = requestId == m_Info.m_FeedRequestId;
    const bool success = result && result->Succeeded();

    if (current && success)
    {
        m_RetryPending = false;
    }

    CSocialClubFeedTilesRow::ResponseDone(result, requestId);

    if (current && !success && (m_MetaDownloadAllowance == MetaDownloadAllowance::All))
    {
        StartRetry();
    }
    
    //TODO_ANDI
    //if (current && success && m_CanCacheImagesHint)
    //{
    //    CacheTiles(true, m_Cacher.m_MaxConcurrentDownloads);
    //}
}

void
CSocialClubFeedLanding::StartRetry()
{
    if (m_TilesRetryTimer.IsStopped())
    {
        m_TilesRetryTimer.Start();
    }
    else
    {
        m_TilesRetryTimer.ScaleRange(EXPONENTIAL_BACKOFF, MAX_RETRY_INTERVAL_MS);
        m_TilesRetryTimer.Restart();
    }

    m_RetryPending = true;
}


//#####################################################################################

PreLaunchData::PreLaunchData()
    : m_Enabled(false)
{

}

PreLaunchData::~PreLaunchData()
{
}

void
PreLaunchData::SetEnabled(const bool isEnabled)
{
    if (!m_Enabled && isEnabled)
    {
        m_TunableRetryTimer.InitMilliseconds(MIN_RETRY_INTERVAL_MS, INITIAL_MAX_RETRY_INTERVAL_MS);
        m_TunableRetryTimer.Stop();
    }

    m_Enabled = isEnabled;

    if (!m_Enabled)
    {
        m_TunableRetryTimer.Shutdown();
    }
}

void
PreLaunchData::Update()
{
	// We keep the tunable download retry logic even though the Beta data download is over.
	// Some pages such ash the store and and the PS+ upsell rely on the tunable.
	if (m_Enabled && Tunables::IsInstantiated())
	{
		m_TunableRetryTimer.Update();

		if (CLiveManager::IsOnlineRos(GAMER_INDEX_LOCAL))
		{
			if (Tunables::GetInstance().HasCloudRequestFinished() && !Tunables::GetInstance().HasCloudTunables()
				&& !Tunables::GetInstance().IsCloudRequestPending())
			{
				if (m_TunableRetryTimer.IsStopped())
				{
					gnetDebug1("Starting tunables download wait");
					m_TunableRetryTimer.Start();
				}
				else if (m_TunableRetryTimer.IsTimeToRetry())
				{
					gnetDebug1("Starting tunables download");
					const bool ok = Tunables::GetInstance().StartCloudRequest();

					m_TunableRetryTimer.ScaleRange(EXPONENTIAL_BACKOFF, MAX_RETRY_INTERVAL_MS);
					m_TunableRetryTimer.Restart();

					if (ok)
					{
						m_TunableRetryTimer.Stop();
					}
				}
			}
			else if (Tunables::GetInstance().HasCloudRequestFinished() && Tunables::GetInstance().HasCloudTunables())
			{
				m_TunableRetryTimer.InitMilliseconds(MIN_RETRY_INTERVAL_MS, INITIAL_MAX_RETRY_INTERVAL_MS);
				m_TunableRetryTimer.Stop();
			}
		}
	}
}

//#####################################################################################

PriorityFeedArea
PriorityFeedAreaSettings::GetAreaFromHash(const atHashString areaHash)
{
	if (areaHash == ScFeedParam::PRIORITY_TAB_SP)
	{
		return PriorityFeedArea::Sp;
	}

	if (areaHash == ScFeedParam::PRIORITY_TAB_MP)
	{
		return PriorityFeedArea::Mp;
	}

	return PriorityFeedArea::Any;
}

PriorityFeedArea
PriorityFeedAreaSettings::GetAreaFromHash(const ScFeedPost& post)
{
	const ScFeedParam* param = post.GetParam(ScFeedParam::PRIORITY_TAB_ID);

	return GetAreaFromHash(param ? param->ValueAsHash() : atHashString());
}

unsigned
PriorityFeedAreaSettings::GetSkipDelay(const ScFeedPost& post)
{
	const ScFeedParam* param = post.GetParam(ScFeedParam::PRIORITY_SKIP_DELAY_ID);
	const s64 val = param ? param->ValueAsInt64() : 0;

	return val > 0 ? static_cast<unsigned>(val) : 0;
}

//#####################################################################################

CSocialClubFeedTilesMgr::CSocialClubFeedTilesMgr()
    : m_AmendDownloadFunc(nullptr)
    , m_CanCacheImagesHint(true)
    , m_ClearAllPending(false)
    , m_ForcedOffline(false)
{
    m_FeedsContent.SetCount(m_FeedsContent.GetMaxCount());
    m_PresenceDlgt.Bind(this, &CSocialClubFeedTilesMgr::OnPresenceEvent);
}

CSocialClubFeedTilesMgr::~CSocialClubFeedTilesMgr()
{

}

CSocialClubFeedTilesMgr&
CSocialClubFeedTilesMgr::Get()
{
    typedef atSingleton<CSocialClubFeedTilesMgr> CSocialClubFeedTilesMgr;
    if (!CSocialClubFeedTilesMgr::IsInstantiated())
    {
        CSocialClubFeedTilesMgr::Instantiate();
    }

    return CSocialClubFeedTilesMgr::GetInstance();
}

void
CSocialClubFeedTilesMgr::Init()
{
    gnetDebug2("CSocialClubFeedTilesMgr - Init");

    rlPresence::AddDelegate(&m_PresenceDlgt);
    m_Landing.Init(ScFeedPostTypeFilter::Landing, ScFeedTile::MAX_LANDING_ITEMS, ScFeedUIArare::LANDING_FEED, this);
    m_Priority.Init(ScFeedPostTypeFilter::Priority, ScFeedTile::MAX_PRIORITY_ITEMS, ScFeedUIArare::PRIORITY_FEED, this);
    m_Series.Init(ScFeedPostTypeFilter::Series, ScFeedTile::MAX_CAROUSEL_ITEMS, ScFeedUIArare::SERIES_FEED, this);
    m_Heist.Init(ScFeedPostTypeFilter::Heist, ScFeedTile::MAX_CAROUSEL_ITEMS, ScFeedUIArare::HEIST_FEED, this);

    if (m_OfflineLanding.GetTiles().GetCount() == 0) // If it's already loaded skip it
    {
        LoadOfflineGrid();
    }
}

void
CSocialClubFeedTilesMgr::Shutdown()
{
    gnetDebug2("CSocialClubFeedTilesMgr - Shutdown");

    rlPresence::RemoveDelegate(&m_PresenceDlgt);

    ClearAll();

    m_Landing.Shutdown();
    m_Priority.Shutdown();
    m_Series.Shutdown();
    m_Heist.Shutdown();

    m_ForcedOffline = false;
}

void
CSocialClubFeedTilesMgr::SetAmendDownloadFunc(const AmendDownloadInfoFunc& func)
{
    m_AmendDownloadFunc = func;
}

void
CSocialClubFeedTilesMgr::ClearFeeds()
{
    gnetDebug2("CSocialClubFeedTilesMgr - ClearFeeds");
    InnerClearFeeds();
}

void
CSocialClubFeedTilesMgr::InnerClearFeeds()
{
    gnetDebug2("CSocialClubFeedTilesMgr - InnerClearFeeds");

    ClearFeedsContent(false);

    m_FocussedTile.Clear();
    m_Landing.Clear();
}

void
CSocialClubFeedTilesMgr::ClearAll()
{
    gnetDebug2("CSocialClubFeedTilesMgr - ClearAll");

    m_ClearAllPending = false;

    ClearFeeds();

    m_Landing.ClearFeeds();
    m_Priority.ClearFeeds();
    m_Series.ClearFeeds();
    m_Heist.ClearFeeds();

	for (int i = 0; i < static_cast<int>(PriorityFeedArea::Count); ++i)
	{
		m_AreaSettings[i] = PriorityFeedAreaSettings();
	}
}

void
CSocialClubFeedTilesMgr::ClearFeedsContent(const bool onlyBigFeed)
{
    gnetDebug2("CSocialClubFeedTilesMgr - ClearFeedsContent [%s]", onlyBigFeed ? "onlyBigFeed" : "all");

    m_FocussedTile.Clear();

    if (!onlyBigFeed)
    {
        m_Landing.ClearContent();
        m_Priority.ClearContent();
        m_Heist.ClearContent();
        m_Series.ClearContent();
    }

    m_Cacher.Clear();

    for (unsigned i = 0; i < static_cast<unsigned>(m_FeedsContent.size()); ++i)
    {
        m_FeedsContent[i].Clear();
    }
}

void 
CSocialClubFeedTilesMgr::CacheTiles(const bool includePriorityFeed, const unsigned maxConcurrentDownloads)
{
    if (maxConcurrentDownloads == 0)
    {
        m_Cacher.Clear();
        return;
    }

    m_Cacher.SetMaxDownloads(maxConcurrentDownloads);

    if (m_Cacher.HasAny()) // Already caching
    {
        return;
    }

    if (m_Landing.m_Info.m_MetaDownloadState != TilesMetaDownloadState::MetaReady)
    {
        return;
    }

    auto doCount = [this](bool landingTile, unsigned x, ScFeedTile& tile)
    {
        if (tile.m_TilePreloaded)
        {
            return;
        }

        bool bigFeedImportant = false;
        const bool important = IsImportant(x, landingTile, bigFeedImportant);

        m_Cacher.CacheTile(tile.m_Feed.GetId(), important || bigFeedImportant);
    };

    IterateTiles(includePriorityFeed, doCount);
}

void
CSocialClubFeedTilesMgr::LoadTileContentById(const RosFeedGuid& feedId, const bool includeBigFeed)
{
    ScFeedTile* tile = const_cast<ScFeedTile*>(GetTileIncludeAll(feedId));

    if (tile)
    {
        LoadTileContent(*tile, includeBigFeed);
   }
}

void
CSocialClubFeedTilesMgr::LoadTileContent(ScFeedTile& tile, const bool includeBigFeed)
{
    tile.LoadContent(m_AmendDownloadFunc);

    if (!includeBigFeed)
    {
        return;
    }

    LoadBigFeedContent(tile);
}

void
CSocialClubFeedTilesMgr::ClearTileContentById(const RosFeedGuid& feedId, const bool onlyBigFeed)
{
    ScFeedTile* tile = const_cast<ScFeedTile*>(GetTileIncludeAll(feedId));

    if (tile)
    {
        ClearTileContent(*tile, onlyBigFeed);
    }
}

void
CSocialClubFeedTilesMgr::ClearTileContent(ScFeedTile& tile, const bool onlyBigFeed)
{
    if (!onlyBigFeed)
    {
        tile.ClearContent();
    }

    ClearBigFeedContentById(tile.m_Feed.GetId());
}

ScFeedContent*
CSocialClubFeedTilesMgr::GetBigFeedContentById(const RosFeedGuid& feedId)
{
    for (int i = 0; i < m_FeedsContent.GetCount(); ++i)
    {
        if (m_FeedsContent[i].GetId() == feedId)
        {
            m_FeedsContent[i].Touch();
            return &m_FeedsContent[i];
        }
    }

    return nullptr;
}

u64
CSocialClubFeedTilesMgr::GetRedownloadIntervalSec()
{
	return Tunables::IsInstantiated()
		? Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FEED_TILES_REDOWNLOAD_SEC", 0x33239D51), static_cast<int>(REDOWNLOAD_WAIT_TIME_SEC))
		: REDOWNLOAD_WAIT_TIME_SEC;
}

u64 GuidToProfileVal(const RosFeedGuid guid)
{
    return (guid.m_data64[0] ^ guid.m_data64[1]) & 0xfffffffffffffff0;
}

void
CSocialClubFeedTilesMgr::PriorityFeedSeen(const RosFeedGuid /*guid*/)
{
#if 0
    CProfileSettings& settings = CProfileSettings::GetInstance();

    if (!settings.AreSettingsValid())
    {
        gnetWarning("PriorityFeedSeen - skipped, no settings. [%s]", guid.ToFixedString().c_str());
        return;
    }

    const ScFeedTile* tile = m_Priority.GetTile(guid);
    if (!gnetVerifyf(tile != nullptr, "Tile not found [%s]", guid.ToFixedString().c_str()))
    {
        return;
    }

    const ScFeedParam* param = tile->m_Feed.GetParam(ScFeedParam::PRIORITY_IMPRESSION_COUNT_ID);
    const s64 requiredImpressions = param == nullptr ? 0 : param->ValueAsInt64();

    if (requiredImpressions == 0)
    {
        gnetDebug3("PriorityFeedSeen - It's a 'show always' post[%s]", guid.ToFixedString().c_str());
        return;
    }

    int slotIdx = -1;
    int impressionCount = GetPriorityFeedImpressionCount(guid, slotIdx);
    if (impressionCount >= requiredImpressions)
    {
        gnetDebug3("PriorityFeedSeen - already seen [%d] times. [%s]", impressionCount, guid.ToFixedString().c_str());
        return;
    }

    const u64 seenPostGuid = GuidToProfileVal(guid);
    const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;

    unsigned idx = 0;

    if (slotIdx < 0) // find any empty or no longer needed slot
    {
        idx = static_cast<unsigned>(settings.GetInt(CProfileSettings::SC_PRIO_SEEN_NEXT_SLOT)) % num;

        // There's at most 8 so this double loop is fine. Used to avoid overwriting old posts.
        for (unsigned i = 0; i < num; ++i)
        {
            bool slotUsed = false;
            u64 impressionGuid = 0;
            int count = 0;

            GetImpression(i, impressionGuid, count);

            if (impressionGuid == 0)
            {
                break;
            }

            for (unsigned j = 0; j < m_Priority.GetTiles().GetCount(); ++j)
            {
                const u64 tileGuid = m_Priority.GetTiles()[j] ? GuidToProfileVal(m_Priority.GetTiles()[j]->m_Feed.GetId()) : 0;

                if (impressionGuid == tileGuid)
                {
                    slotUsed = true; // The slot is occupied by an existing post
                    break;
                }
            }

            if (!slotUsed)
            {
                break;
            }

            idx = (idx + 1) % num;
        }
    }
    else
    {
        idx = slotIdx;
    }

    gnetDebug1("PriorityFeedSeen id[%s] count[%u] idx[%u]", guid.ToFixedString().c_str(), impressionCount + 1, idx);

    if (StoreImpression(idx, seenPostGuid, impressionCount + 1))
    {
        settings.Set(CProfileSettings::SC_PRIO_SEEN_NEXT_SLOT, static_cast<int>((idx + 1) % num));
    }
#endif
}

int
CSocialClubFeedTilesMgr::GetPriorityFeedImpressionCount(const RosFeedGuid /*guid*/, int& /*slotIdx*/) const
{
#if 0
    slotIdx = -1;

    CProfileSettings& settings = CProfileSettings::GetInstance();

    if (!settings.AreSettingsValid())
    {
        return false;
    }

    const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;
    const u64 compactGuid = GuidToProfileVal(guid);

    for (unsigned i = 0; i < num; ++i)
    {
        u64 impressionGuid = 0;
        int count = 0;

        if (GetImpression(i, impressionGuid, count) && compactGuid == impressionGuid)
        {
            slotIdx = i;
            return count;
        }
    }
#endif

    return 0;
}

bool
CSocialClubFeedTilesMgr::HasSeenPriorityFeed(const RosFeedGuid guid) const
{
    const ScFeedTile* tile = m_Priority.GetTile(guid);

    if (tile == nullptr)
    {
        return false;
    }

    const ScFeedParam* param = tile->m_Feed.GetParam(ScFeedParam::PRIORITY_IMPRESSION_COUNT_ID);
    const s64 requiredImpressions = param == nullptr ? 0 : param->ValueAsInt64();

    if (requiredImpressions == 0)
    {
        return false;
    }

    int slotIdx = -1;
    const int impressionCount = GetPriorityFeedImpressionCount(tile->m_Feed.GetId(), slotIdx);

    return impressionCount >= requiredImpressions;
}

bool
CSocialClubFeedTilesMgr::HasUnseenPriorityFeed(const PriorityFeedArea tabArea, const bool veryFirstLaunch) const
{
    for (unsigned i = 0; i < m_Priority.GetTiles().GetCount(); ++i)
    {
        const ScFeedTile* tile = m_Priority.GetTiles()[i];

        if (tile == nullptr || ScFeedPost::IsHiddenByDlinkFlag(tile->m_Feed))
        {
            continue;
        }

        const PriorityFeedArea area = PriorityFeedAreaSettings::GetAreaFromHash(tile->m_Feed);

        // Only on the very first launch we auto-show the non area specific posts.
        // Otherwise we'll always open sp and mp if there's at least one global post.
        if (!veryFirstLaunch && area != tabArea && tabArea != PriorityFeedArea::Any)
        {
            continue;
        }

        // If the area doesn't match and it's not a global post it doesn't belong to this view
        if (area != tabArea && area != PriorityFeedArea::Any && tabArea != PriorityFeedArea::Any)
        {
            continue;
        }

        const ScFeedParam* param = tile->m_Feed.GetParam(ScFeedParam::PRIORITY_IMPRESSION_COUNT_ID);
        const s64 requiredImpressions = param == nullptr ? 0 : param->ValueAsInt64();

        if (requiredImpressions == 0)
        {
            return true;
        }

        int slotIdx = -1;
        const int impressionCount = GetPriorityFeedImpressionCount(tile->m_Feed.GetId(), slotIdx);
        if (impressionCount < requiredImpressions)
        {
            return true;
        }
    }

    return false;
}

bool
CSocialClubFeedTilesMgr::HasAnyVisiblePriorityFeed(const PriorityFeedArea tabArea) const
{
    return tabArea < PriorityFeedArea::Count ? m_AreaSettings[static_cast<int>(tabArea)].m_HasAnyVisiblePriorityFeed : false;
}

unsigned
CSocialClubFeedTilesMgr::GetSkipButtonDelaySec(const PriorityFeedArea tabArea) const
{
	return tabArea < PriorityFeedArea::Count ? m_AreaSettings[static_cast<int>(tabArea)].m_SkipButtonDealySec : 0;
}

void
CSocialClubFeedTilesMgr::UpdateHasAnyVisiblePriorityFeed()
{
#define UPDATE_MAX_SKIP_DELAY(settings, area, delay) settings[static_cast<int>(area)].m_SkipButtonDealySec = rage::Max(delay, settings[static_cast<int>(area)].m_SkipButtonDealySec);

    PriorityFeedAreaSettings areaSettings[static_cast<unsigned>(PriorityFeedArea::Count)];

    for (unsigned i = 0; i < m_Priority.GetTiles().GetCount(); ++i)
    {
        const ScFeedTile* tile = m_Priority.GetTiles()[i];

        if (tile == nullptr || ScFeedPost::IsHiddenByDlinkFlag(tile->m_Feed) || HasSeenPriorityFeed(tile->m_Feed.GetId()))
        {
            continue;
        }

        const PriorityFeedArea area = PriorityFeedAreaSettings::GetAreaFromHash(tile->m_Feed);

        const unsigned skipDelay = PriorityFeedAreaSettings::GetSkipDelay(tile->m_Feed);

        if (area == PriorityFeedArea::Any)
        {
            UPDATE_MAX_SKIP_DELAY(areaSettings, PriorityFeedArea::Sp, skipDelay);
            UPDATE_MAX_SKIP_DELAY(areaSettings, PriorityFeedArea::Mp, skipDelay);
        }
        else
        {
            UPDATE_MAX_SKIP_DELAY(areaSettings, area, skipDelay);
        }

        UPDATE_MAX_SKIP_DELAY(areaSettings, PriorityFeedArea::Any, skipDelay);

        areaSettings[static_cast<int>(area)].m_HasAnyVisiblePriorityFeed = true;
        areaSettings[static_cast<int>(PriorityFeedArea::Any)].m_HasAnyVisiblePriorityFeed = true;
    }

    for (int i = 0; i < static_cast<unsigned>(PriorityFeedArea::Count); ++i)
    {
        m_AreaSettings[i] = areaSettings[i];
    }
}

void
CSocialClubFeedTilesMgr::ClearBigFeedContentById(const RosFeedGuid& feedId)
{
    ScFeedContent* content = GetBigFeedContentById(feedId);

    if (content == nullptr)
    {
        return;
    }

    content->Clear();
}

void
CSocialClubFeedTilesMgr::TileFocussed(const RosFeedGuid& feedId)
{
    if (m_FocussedTile.m_Tile == feedId)
    {
        return;
    }

    m_FocussedTile.Clear();
    m_FocussedTile.m_Tile = feedId;
    m_FocussedTile.m_Timeout.SetLongFrameThreshold(netTimeout::DEFAULT_LONG_FRAME_THRESHOLD_MS);
    m_FocussedTile.m_Timeout.InitMilliseconds(ScFocussedTile::WAIT_TIME_MS);
}

void
CSocialClubFeedTilesMgr::ForceOffline()
{
    gnetDebug1("CSocialClubFeedTilesMgr - ForceOffline");
    m_ForcedOffline = true;
}

bool
CSocialClubFeedTilesMgr::SwapPendingData()
{
    gnetDebug1("CSocialClubFeedTilesMgr - SwapPendingData");

    if (m_Landing.GetTiles().GetCount() == 0)
    {
        return false;
    }

    m_OfflineLanding.ClearContent();
    return true;
}

void
CSocialClubFeedTilesMgr::Update()
{
    PROFILE

    if (m_ClearAllPending)
    {
        ClearAll();
    }

    m_Priority.CheckUIPreload();
    m_Series.CheckUIPreload();
    m_Heist.CheckUIPreload();
    m_Landing.Update();

    UpdateCaching();

    UpdateFocussedTile();

    m_PrelaunchData.Update();

    // The prelanuchdata check avoids the update while not on the landing page.
    if (m_PrelaunchData.IsEnabled())
    {
        UpdateHasAnyVisiblePriorityFeed();
    }
}

TilesImagesDownloadState
CSocialClubFeedTilesMgr::GetCacheState() const
{
    if (m_Cacher.m_Done)
    {
        return TilesImagesDownloadState::Ready;
    }

    if (!m_Cacher.HasAny())
    {
        return TilesImagesDownloadState::None;
    }

    return m_Cacher.HasImportant() ? TilesImagesDownloadState::DownloadingBasics : TilesImagesDownloadState::DownloadingMore;
}

const ScFeedTile*
CSocialClubFeedTilesMgr::GetTileIncludeAll(const RosFeedGuid& feedId)
{
    const ScFeedTile* tile = m_Landing.GetTile(feedId);
    if (tile) return tile;

    tile = m_OfflineLanding.GetTile(feedId);
    if (tile) return tile;

    tile = m_Priority.GetTile(feedId);
    if (tile) return tile;

    tile = m_Series.GetTile(feedId);
    if (tile) return tile;

    tile = m_Heist.GetTile(feedId);
    if (tile) return tile;

    return nullptr;
}

void
CSocialClubFeedTilesMgr::SetCacheImagesHint(const bool canDownlaodImages)
{
    m_CanCacheImagesHint = canDownlaodImages;

    if (m_CanCacheImagesHint)
    {
        CacheTiles(true, m_Cacher.m_MaxConcurrentDownloads);
    }
}

CSocialClubFeedTilesRow*
CSocialClubFeedTilesMgr::Row(const atHashString uiArea)
{
    if (uiArea == m_Landing.m_RowArea) return &m_Landing;
    if (uiArea == m_Heist.m_RowArea) return &m_Heist;
    if (uiArea == m_Series.m_RowArea) return &m_Series;
    if (uiArea == m_Priority.m_RowArea) return &m_Priority;

    return nullptr;
}

void
CSocialClubFeedTilesMgr::UpdateFocussedTile()
{
    m_FocussedTile.m_Timeout.Update();

    if (m_FocussedTile.m_Timeout.IsTimedOut())
    {
        m_FocussedTile.m_Timeout.Clear();
        LoadTileContentById(m_FocussedTile.m_Tile, true);
    }

    if (!m_FocussedTile.m_NeighboursRequested && m_FocussedTile.m_Tile.IsValid())
    {
        unsigned c, r = 0;

        CSocialClubFeedTilesRow* row = &m_Series;
        const ScFeedTile* tile = row->GetTile(m_FocussedTile.m_Tile);

        if (tile == nullptr)
        {
            row = &m_Heist;
            tile = row->GetTile(m_FocussedTile.m_Tile);
        }

        ScFeedContent* content = tile ? GetBigFeedContentById(m_FocussedTile.m_Tile) : nullptr;

        if (content && content->IsDone() && tile && tile->GetCoords(c, r))
        {
            m_FocussedTile.m_NeighboursRequested = true;

            if (c + 1 < (unsigned)row->GetTiles().GetCount())
            {
                const ScFeedTile* right = row->GetTile(c + 1, r);

                if (right)
                {
                    LoadTileContentById(right->m_Feed.GetId(), true);
                }
            }

            if (c > 0)
            {
                const ScFeedTile* left = row->GetTile(c - 1, r);

                if (left)
                {
                    LoadTileContentById(left->m_Feed.GetId(), true);
                }
            }
        }
    }
}

void
CSocialClubFeedTilesMgr::UpdateCaching()
{
    if (!m_Cacher.HasAny())
    {
        return;
    }

    unsigned num = 0;

    for (int i = 0; i < m_Cacher.m_Contents.GetCount(); ++i)
    {
        ScFeedContent& content = m_Cacher.m_Contents[i];

        if (!content.IsSet())
        {
            continue;
        }

        if (content.IsDone())
        {
            ScFeedTile* tile = const_cast<ScFeedTile*>(GetTileIncludeAll(content.GetId()));
            if (tile)
            {
                tile->m_TilePreloaded = true;
            }

            m_Cacher.RemoveTile(content.GetId());
            content.Clear();
        }
        else
        {
            ++num;
        }
    }

    if (!m_CanCacheImagesHint) // We're blocked from downloading new images
    {
        return;
    }

    // We're very conservative with the caching. As soon as something is using the feed or cloud we wait
    if (CSocialClubFeedMgr::Get().IsWorking() /*|| CloudManager::GetInstance().HasAnyPendingRequest()*/) //TODO_ANDI
    {
        return;
    }

    atArray<ScPendingTileCache>& current = m_Cacher.HasImportant() ? m_Cacher.m_Important : m_Cacher.m_Common;

    int contentIdx = 0;
    int pendingIdx = 0;

    while (contentIdx < m_Cacher.m_Contents.GetCount() && num < m_Cacher.m_MaxConcurrentDownloads && pendingIdx < current.GetCount())
    {
        if (m_Cacher.m_Contents[contentIdx].IsSet())
        {
            ++contentIdx;
            continue;
        }

        if (current[pendingIdx].m_Working)
        {
            ++pendingIdx;
            continue;
        }

        bool ok = false;
        const ScFeedTile* tile = GetTileIncludeAll(current[pendingIdx].m_Guid);

        if (tile != nullptr)
        {
            current[pendingIdx].m_Working = true;
            ok = m_Cacher.m_Contents[contentIdx].RetrieveContent(&(tile->m_Feed), m_AmendDownloadFunc, true);
        }

        if (ok)
        {
            ++num;
        }
        else
        {
            m_Cacher.RemoveTile(current[pendingIdx].m_Guid);
        }
    }
}

void
CSocialClubFeedTilesMgr::IterateTiles(const bool includePriorityFeed, LambdaRef<void(bool, unsigned, ScFeedTile&)> func)
{
    if (includePriorityFeed)
    {
        IteratePriorityFeedTiles(func);
    }

    //TODO_ANDI: m_OfflineLanding
    ScFeedTileRow& row = m_Landing.GetTiles();

    for (unsigned x = 0; x < row.GetCount(); ++x)
    {
        ScFeedTile& tile = *(row[x]);
        func(true, x, tile);
    }
}

void
CSocialClubFeedTilesMgr::IteratePriorityFeedTiles(LambdaRef<void(bool, unsigned, ScFeedTile&)> func)
{
    for (unsigned x = 0; x < m_Priority.m_Row.GetCount(); ++x)
    {
        ScFeedTile& tile = *(m_Priority.m_Row[x]);
        func(false, x, tile);
    }
}

void
CSocialClubFeedTilesMgr::LoadBigFeedContent(const ScFeedTile& tile)
{
    ScFeedContent* content = GetBigFeedContentById(tile.m_Feed.GetId());

    if (content != nullptr)
    {
        gnetDebug2("CSocialClubFeedTilesMgr - LoadFeedContentById[%s] - content in list trying to retrieve", tile.m_Feed.GetId().ToFixedString().c_str());
        content->RetrieveContent(&(tile.m_Feed), m_AmendDownloadFunc, true);

        return;
    }

    gnetDebug2("CSocialClubFeedTilesMgr - LoadFeedContentById[%s] - content not in list - adding", tile.m_Feed.GetId().ToFixedString().c_str());
    int realPos = CSocialClubFeedMgr::GetUsableContentIdx(&m_FeedsContent[0], m_FeedsContent.GetCount());

    if (realPos != -1)
    {
        m_FeedsContent[realPos].Clear();
        m_FeedsContent[realPos].RetrieveContent(&(tile.m_Feed), m_AmendDownloadFunc, true);
    }
}

bool 
CSocialClubFeedTilesMgr::LoadOfflineGrid()
{
    const char* path = "platform:/data/metadata/landingofflinetiles";

    fiStream* xmlStream = ASSET.Open(path, META_FILE_EXT);

    if (xmlStream == nullptr)
    {
        // For now this is just a warning. We may want to assert/fail later on
        gnetWarning("Failed to load %s", path);
        return false;
    }

    m_OfflineLanding.Clear();

    INIT_PARSER;

    parTree* tree = nullptr;

    bool success = true;

    RosFeedGuid guid;
    guid.m_data32[0] = 100;
    guid.m_data32[1] = 2;
    guid.m_data32[2] = 3;
    guid.m_data32[3] = 0;

    rtry
    {
        tree = PARSER.LoadTree(xmlStream);
        rcheck(tree, catchall, gnetError("tree is null"));

        const parTreeNode* rootNode = tree->GetRoot();
        rcheck(rootNode, catchall, gnetError("rootNode is null"));

        parTreeNode::ChildNodeIterator it = rootNode->BeginChildren();
        for (; it != rootNode->EndChildren(); ++it)
        {
            const parElement& el = (*it)->GetElement();

            if (stricmp(el.GetName(), "M") != 0)
            {
                continue;
            }

            char* jsonData = (*it)->GetData();
            
            ScFeedPost post;
            if (post.ParseContent(jsonData))
            {
                // We create fake guids and the column is also an incremental value.
                // This way people can't accidentally set the same number or id.
                ++guid.m_data32[0];
                post.SetId(guid);
                m_OfflineLanding.AddPost(post);
            }
        }

    }
    rcatchall
    {
        success = false;
    }

    if (tree)
    {
        delete tree;
        tree = nullptr;
    }

    SHUTDOWN_PARSER;

    if (xmlStream)
    {
        xmlStream->Close();
    }

    return success;
}

void
CSocialClubFeedTilesMgr::OnCloudEvent(const CloudEvent* pEvent)
{
    switch (pEvent->GetType())
    {
    case CloudEvent::EVENT_REQUEST_FINISHED:
        {
        } break;
    case CloudEvent::EVENT_AVAILABILITY_CHANGED:
        {
            const CloudEvent::sAvailabilityChangedEvent* pEventData = pEvent->GetAvailabilityChangedData();

            if (!pEventData->bIsAvailable)
            {
                return;
            }

            //if (m_Priority.GetMetaDownloadState() == TilesMetaDownloadState::None)
            //{
            //    m_Priority.RenewTiles();
            //}

            if (m_Landing.GetMetaDownloadState() == TilesMetaDownloadState::None)
            {
                m_Landing.RenewTiles();
            }

            if (m_Series.GetMetaDownloadState() == TilesMetaDownloadState::None)
            {
                m_Series.RenewTiles();
            }

            if (m_Heist.GetMetaDownloadState() == TilesMetaDownloadState::None)
            {
                m_Heist.RenewTiles();
            }

        } break;
    }
}

int
CSocialClubFeedTilesMgr::GetRowIndexFromFeed(const ScFeedPost* feed)
{
    const ScFeedParam* param = feed ? feed->GetParam(ScFeedParam::ROW_ID) : nullptr;
    return param ? static_cast<int>(param->ValueAsInt64()) : -1;
}

void
CSocialClubFeedTilesMgr::OnPresenceEvent(const rlPresenceEvent* pEvent)
{
    if (pEvent->GetId() != PRESENCE_EVENT_SIGNIN_STATUS_CHANGED)
    {
        return;
    }

    const rlPresenceEventSigninStatusChanged* pThisEvent = pEvent->m_SigninStatusChanged;

    bool bIsActiveGamer = (pThisEvent->m_GamerInfo.GetLocalIndex() == rlPresence::GetActingUserIndex());

    if (!bIsActiveGamer)
    {
        return;
    }

    // The user may be on the page which uses these tiles so we delay this slightly
    // The mgr code is fine with that but it causes a ton of UI asserts.
    if (pThisEvent->SignedOut())
    {
        m_ClearAllPending = true;
    }
}

//PURPOSE
//  Returns true if the specified tile is shown 'early on' when entering the screen.
bool
IsImportant(const unsigned x, const bool isLandingTile, bool& bigFeedImportant)
{
    if (isLandingTile)
    {
        bigFeedImportant = false;
        return true;
    }

    // For the non-landing tile (== priority feed) the big feed data has all we need
    bigFeedImportant = (x == 0);
    return false;
}

#if __BANK
void
Bank_GetLanding()
{
#if TEST_FAKE_FEED
    PARAM_scDebugFeedLanding.Set(nullptr);
#endif
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
}

#if TEST_FAKE_FEED
void
Bank_GetDebugLanding()
{
    static const char* fileName = "common:/non_final/www/feed/debug_tile_feed.txt";
    PARAM_scDebugFeedLanding.Set(fileName);

    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
}
#endif //TEST_FAKE_FEED

void
Bank_ClearLanding()
{
    CSocialClubFeedTilesMgr::Get().ClearFeeds();
}

void
Bank_CacheTileFeed()
{
    CSocialClubFeedTilesMgr::Get().CacheTiles(true, ScTilesCaching::MAX_CONCURRENT_DOWNLOADS);
}

void
Bank_PreloadTileFeed()
{
    CSocialClubFeedTilesMgr::Get().Landing().PreloadUI(true);
}

void
Bank_ForceOfflineLanding()
{
    CSocialClubFeedTilesMgr::Get().ForceOffline();
}

void
Bank_Landing1prim()
{
#if TEST_FAKE_FEED
    PARAM_scCloudLanding.Set("_1prim");
    CSocialClubFeedTilesMgr::Get().Landing().Clear();
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
#endif
}

void
Bank_Landing2prim()
{
#if TEST_FAKE_FEED
    PARAM_scCloudLanding.Set("_2prim");
    CSocialClubFeedTilesMgr::Get().Landing().Clear();
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
#endif
}

void
Bank_Landing1prim2sub()
{
#if TEST_FAKE_FEED
    PARAM_scCloudLanding.Set("_1prim2sub");
    CSocialClubFeedTilesMgr::Get().Landing().Clear();
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
#endif
}

void
Bank_Landing1wide2sub()
{
#if TEST_FAKE_FEED
    PARAM_scCloudLanding.Set("_1wide2sub");
    CSocialClubFeedTilesMgr::Get().Landing().Clear();
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
#endif
}

void
Bank_Landing1wide1sub2mini()
{
#if TEST_FAKE_FEED
    PARAM_scCloudLanding.Set("_1wide1sub2mini");
    CSocialClubFeedTilesMgr::Get().Landing().Clear();
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
#endif
}

void
Bank_Landing4sub()
{
#if TEST_FAKE_FEED
    PARAM_scCloudLanding.Set("_4sub");
    CSocialClubFeedTilesMgr::Get().Landing().Clear();
    CSocialClubFeedTilesMgr::Get().Landing().RenewTiles();
#endif
}

void
Bank_GetPriority()
{
    CSocialClubFeedTilesMgr::Get().Priority().RenewTiles();
}

void
Bank_ClearPriority()
{
    CSocialClubFeedTilesMgr::Get().Priority().ClearFeeds();
}

void
Bank_GetSeries()
{
    CSocialClubFeedTilesMgr::Get().Series().RenewTiles();
}

void
Bank_ClearSeries()
{
    CSocialClubFeedTilesMgr::Get().Series().ClearFeeds();
}

void
Bank_GetHeist()
{
    CSocialClubFeedTilesMgr::Get().Heist().RenewTiles();
}

void
Bank_ClearHeist()
{
    CSocialClubFeedTilesMgr::Get().Heist().ClearFeeds();
}

static int s_SeenIndex = 0;

void
Bank_ToggleForceIntro()
{
    const char* param = nullptr;
    const bool isForceOff = PARAM_frontendMpForceIntro.GetParameter(param) && stricmp(param, "false") == 0;
    PARAM_frontendMpForceIntro.Set(isForceOff ? "true" : "false");
}

void Bank_ClearForceIntro()
{
    PARAM_frontendMpForceIntro.Set(nullptr);
}

void Bank_PriorityFeedSeen()
{
    CSocialClubFeedTilesRow& prio = CSocialClubFeedTilesMgr::Get().Priority();

    if (s_SeenIndex < prio.GetTiles().size())
    {
        CSocialClubFeedTilesMgr::Get().PriorityFeedSeen(prio.GetTiles()[s_SeenIndex]->m_Feed.GetId());
    }
}

void Bank_ClearPriorityFeedSeen()
{
#if 0
    const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;

    for (int i = 0; i < num; ++i)
    {
        StoreImpression(i, 0, 0);
    }
#endif
}

void
CSocialClubFeedTilesMgr::InitWidgets(bkBank& bank)
{
    bank.PushGroup("Landing Page Tiles");
    {
        bank.AddButton("Get Landing", CFA(Bank_GetLanding));
        TEST_FAKE_FEED_ONLY(bank.AddButton("Get Debug Landing", CFA(Bank_GetDebugLanding)));
        bank.AddButton("Clear Landing", CFA(Bank_ClearLanding));
        bank.AddButton("Cache Images", CFA(Bank_CacheTileFeed));
        bank.AddButton("Preload Images", CFA(Bank_PreloadTileFeed));
        bank.AddButton("Force Offline", CFA(Bank_ForceOfflineLanding));
        bank.AddButton("Landing 1prim", CFA(Bank_Landing1prim));
        bank.AddButton("Landing 2prim", CFA(Bank_Landing2prim));
        bank.AddButton("Landing 1prim2sub", CFA(Bank_Landing1prim2sub));
        bank.AddButton("Landing 1wide2sub", CFA(Bank_Landing1wide2sub));
        bank.AddButton("Landing 1wide1sub2mini", CFA(Bank_Landing1wide1sub2mini));
        bank.AddButton("Landing 4sub", CFA(Bank_Landing4sub));
        bank.AddSeparator();
        bank.AddButton("Get Priority", CFA(Bank_GetPriority));
        bank.AddButton("Clear Priority", CFA(Bank_ClearPriority));
        bank.AddButton("Get Series", CFA(Bank_GetSeries));
        bank.AddButton("Clear Series", CFA(Bank_ClearSeries));
        bank.AddButton("Get Heist", CFA(Bank_GetHeist));
        bank.AddButton("Clear Heist", CFA(Bank_ClearHeist));
        bank.AddSeparator();
        bank.AddButton("Priority Feed Seen", CFA(Bank_PriorityFeedSeen));
        bank.AddSlider("Priority Feed Seen Index", &s_SeenIndex, 0, 5, 1);
        bank.AddButton("Clear Priority Feed Seen", CFA(Bank_ClearPriorityFeedSeen));

        bank.AddSeparator();
        bank.AddButton("Toggle Force Intro MP Landing Page", CFA(Bank_ToggleForceIntro));
        bank.AddButton("Clear Force Intro MP Landing Page", CFA(Bank_ClearForceIntro));
    }
    bank.PopGroup();
}

#endif //__BANK

#endif //GEN9_STANDALONE_ENABLED