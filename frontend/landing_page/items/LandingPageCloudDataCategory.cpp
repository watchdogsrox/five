/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageCloudDataCategory.cpp
// PURPOSE : A category dedicated to the landing page
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingPageCloudDataCategory.h"

#if UI_PAGE_DECK_ENABLED
#include "LandingPageCloudDataCategory_parser.h"

// game
#include "frontend/landing_page/layout/LandingOnlineLayout.h"
#include "frontend/page_deck/items/CloudCardItem.h"
#include "frontend/page_deck/items/CloudCardNewsItem.h"
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/ui_channel.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"

PARAM( disableOverridingFixedItems, "Disallow cloud data overriding the fixed column items");
PARAM( landingFullFallback, "Display a full fallback layout including the fixed columns");

CLandingPageCloudDataCategory::CLandingPageCloudDataCategory()
    : superclass()
    , m_backupLayoutType(CLandingOnlineLayout::LAYOUT_ONE_PRIMARY)
    , m_currentDataSource(eLandingPageOnlineDataSource::None)
{
    
}

CLandingPageCloudDataCategory::~CLandingPageCloudDataCategory()
{
}

eLandingPageOnlineDataSource CLandingPageCloudDataCategory::GetCurrentDataSource() const
{
    return m_currentDataSource;
}

bool CLandingPageCloudDataCategory::CreateCard(const ScFeedPost& post, int x, int y)
{
    CCloudCardItem* ci = nullptr;
        
    if (ScFeedPost::IsAboutNews(post))
    {
        ci = rage_new CCloudCardNewsItem();
    }
    else
    {
        ci = rage_new CCloudCardItem();
    }

    if (ci != nullptr)
    {
        uiCloudSupport::FillLandingPageCard(*ci, post);
        ci->SetTrackingData(CNetworkTelemetry::LandingPageMapPos(x, y), ScFeedPost::GetDlinkTrackingId(post));
        AddCloudItem(ci);
    }

    return ci != nullptr;
}

bool CLandingPageCloudDataCategory::FillData(CSocialClubFeedTilesRow& row)
{
    const eLandingPageOnlineDataSource dataSource = CLandingPageArbiter::GetBestDataSource();

    if( dataSource == eLandingPageOnlineDataSource::FromCloud )
    {
        rtry
        {
            rcheck(row.GetTiles().GetCount() > 0, catchall, gnetWarning("CLandingPageCloudDataCategory - No data in the feed"));

            CLandingOnlineLayout::eLayoutType layoutType = CLandingOnlineLayout::LAYOUT_ONE_PRIMARY;
            unsigned layoutPublishId = 0;
            rverifyall(uiCloudSupport::FillLandingPageLayout(layoutType, layoutPublishId));
            rverifyall(uiCloudSupport::VerifyLandingPageLayout(layoutType, layoutPublishId, row));
            const uiCloudSupport::LayoutMapping* mapping = uiCloudSupport::GetLayoutMapping(layoutType);
            rverifyall(mapping != nullptr);

            RefreshLayoutType( layoutType );

            atHashString persistentDest[PERSISTENT_CARDS_COUNT];
            const ScFeedPost* persistentTile[PERSISTENT_CARDS_COUNT] = {0};

            // The four default tiles
            for (unsigned i=0; i< PERSISTENT_CARDS_COUNT; ++i)
            {
                bool const c_allowOverrides = !PARAM_disableOverridingFixedItems.Get();
                const ScFeedTile* tile = c_allowOverrides ? row.GetTile(PERSISTENT_CARDS_COLUMN, i, layoutPublishId) : nullptr;

                if (tile != nullptr)
                {
                    persistentTile[i] = &tile->m_Feed;

                    const atHashString uiTarget = ScFeedPost::GetDlinkParamHash(tile->m_Feed, ScFeedParam::DLINK_UI_TARGET_ID);

                    // This is the name in landing_page_deck.meta. I don't have a better way to know what's what
                    if (uiTarget == ATSTRINGHASH("series_list", 0xDF0B1F46))
                    {
                        persistentDest[i] = ScFeedUIArare::SERIES_FEED;
                    }
                    else if (uiTarget == ATSTRINGHASH("heists_list", 0xA2DB0740))
                    {
                        persistentDest[i] = ScFeedUIArare::HEIST_FEED;
                    }

                    CreateCard(tile->m_Feed, PERSISTENT_CARDS_COLUMN, i);
                }
                else
                {
                    // The first four are the base items. There will be a fifth in m_items which is the big one
                    CCardItem* backup = GetBackupItem(PERSISTENT_CARDS_COLUMN, i, persistentDest[i]);

                    if (uiVerifyf(backup, "We should have a backup for the four cards to the right of the screen"))
                    {
                        AddCloudItem(backup);
                    }
                }
            }

            for (int i = 0; i < mapping->m_numCards; ++i)
            {
                const uiCloudSupport::LayoutPos& pos = mapping->m_pos[i];
                const ScFeedTile* tile = row.GetTile(pos.m_x, pos.m_y, layoutPublishId);

                if (tile == nullptr || !CreateCard(tile->m_Feed, pos.m_x, pos.m_y))
                {
                    uiAssertf(false, "The card(%d,%d) is missing. The uiCloudSupport checks should have caught that", pos.m_x, pos.m_y);
                }
            }

            CNetworkTelemetry::SetLandingPagePersistentTileData(persistentDest, PERSISTENT_CARDS_COUNT);
            CNetworkTelemetry::SetLandingPageOnlineData(layoutPublishId, mapping, &row, persistentTile);

            ApplySource(eLandingPageOnlineDataSource::FromCloud);
            return true;
        }
        rcatchall
        {
        }
    }

    return false;
}

bool CLandingPageCloudDataCategory::LoadRichData(CSocialClubFeedTilesRow& row)
{
    // Not entirely correct in case we have dupes on the same slot but good enough
    row.PreloadUI(0, ScFeedTile::MAX_LANDING_ITEMS);
    return true;
}

void CLandingPageCloudDataCategory::OnFailed()
{
    const eLandingPageOnlineDataSource dataSource = CLandingPageArbiter::GetBestDataSource();

    switch (dataSource)
    {
    case eLandingPageOnlineDataSource::NoMpAccess:
        {
            FillLandingPageFallbackData_Offline();
        } break;
    case eLandingPageOnlineDataSource::NoMpCharacter:
        {
            FillLandingPageFallbackData_NoMPCharacter();
        } break;
    default:
        {
            FillLandingPageFallbackData();
        } break;
    }
}

void CLandingPageCloudDataCategory::FillLandingPageFallbackData()
{
    if (m_currentDataSource == eLandingPageOnlineDataSource::FromDisk && HasCloudItems())
    {
        return;
    }

    ApplySource(eLandingPageOnlineDataSource::FromDisk);

#if !__NO_OUTPUT
    if (PARAM_landingFullFallback.Get())
    {
        ClearCloudData();

        RefreshLayoutType( m_backupLayoutType );

        AddCloudItem(&m_freeRoamCard);
        AddCloudItem(&m_heistCard);
        AddCloudItem(&m_seriesCard);
        AddCloudItem(&m_newsCard);
        AddCloudItem(&m_mainCard);
    }
    else
#endif
    {
        FillLandingPageAs1x1(m_mainCard);
    }
}

CCardItem* CLandingPageCloudDataCategory::GetBackupItem(const unsigned column, const unsigned row, atHashString& area)
{
    area = atHashString();

    // Please update MetricLandingPageOnline::Set if the order changes
    if (column == PERSISTENT_CARDS_COLUMN)
    {
        switch (row)
        {
        case 0: { return &m_freeRoamCard; }
        case 1:
        {
            area = ScFeedUIArare::HEIST_FEED;
            return &m_heistCard;
        }
        case 2:
        {
            area = ScFeedUIArare::SERIES_FEED;
            return &m_seriesCard;
        }
        case 3: { return &m_newsCard; }
        default: { return nullptr; }
        }
    }

    if (column == 0 && row == 0)
    {
        // We may end up with different main cards depending on the error reason but for now one is enough
        return &m_mainCard;
    }

    return nullptr;
}

void CLandingPageCloudDataCategory::FillLandingPageFallbackData_Offline()
{
    // Not sure if a error reason change requires a FillLandingPageAs1x1 call.
    // If not we should still call UpdateErrorType() but then return if m_currentDataSource hasn't changed.
    bool errorChanged = m_networkErrorCard.UpdateErrorType();

    if (m_currentDataSource == eLandingPageOnlineDataSource::NoMpAccess && HasCloudItems() && !errorChanged)
    {
        return;
    }

    ApplySource(eLandingPageOnlineDataSource::NoMpAccess);
    FillLandingPageAs1x1( m_networkErrorCard );
}

void CLandingPageCloudDataCategory::FillLandingPageFallbackData_NoMPCharacter()
{
    if (m_currentDataSource == eLandingPageOnlineDataSource::NoMpCharacter && HasCloudItems())
    {
        return;
    }

    ApplySource(eLandingPageOnlineDataSource::NoMpCharacter);
    FillLandingPageAs1x1( m_noCharacterCard );
}

void CLandingPageCloudDataCategory::FillLandingPageAs1x1(CCardItemBase& item)
{
    ClearCloudData();
    RefreshLayoutType( CLandingOnlineLayout::LAYOUT_FULLSCREEN );
    AddCloudItem(&item);
}

void CLandingPageCloudDataCategory::ApplySource( eLandingPageOnlineDataSource source)
{
    CNetworkTelemetry::AppendLandingPageMisc();
    CNetworkTelemetry::AppendLandingPagePersistent();
    CNetworkTelemetry::AppendLandingPageOnline(source);

    m_currentDataSource = source;
}

void CLandingPageCloudDataCategory::RefreshLayoutType( CLandingOnlineLayout::eLayoutType const layoutType )
{
    CLandingOnlineLayout * const layout = GetLayout<CLandingOnlineLayout>();

    // We lazy-init this as if we set it in the constructor then parGen will stomp our m_layout pointer with
    // whatever is in the meta file
    if( layout == nullptr )
    {
        CPageLayoutBase* layout = rage_new CLandingOnlineLayout( layoutType );
        SetLayout(layout);
    }
    else
    {
        layout->SetLayoutType( layoutType );
        layout->UnregisterAll();
        layout->Reset();
    }
}

bool CLandingPageCloudDataCategory::RefreshContentDerived() 
{
    eLandingPageOnlineDataSource const c_currentSource = GetCurrentDataSource();
    eLandingPageOnlineDataSource expectedSource = eLandingPageOnlineDataSource::None;

    bool const c_result = CLandingPageArbiter::IsDataSourceOk( c_currentSource, expectedSource );

    if( !c_result )
    {
        ClearCloudData();
        UpdateRequest();
        ApplySource( expectedSource );
    }

    return !c_result;
}

#endif // UI_PAGE_DECK_ENABLED
