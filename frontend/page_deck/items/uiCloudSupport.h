#ifndef CLOUD_WRAPPER_H
#define CLOUD_WRAPPER_H

// rage
#include "atl/hashstring.h"

// game
#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/landing_page/layout/LandingOnlineLayout.h"
#include "frontend/page_deck/items/IItemStickerInterface.h"
#include "network/SocialClubServices/SocialClubFeed.h"

#if GEN9_LANDING_PAGE_ENABLED

class CCloudCardItem;
class CSocialClubFeedTilesRow;
class uiPageDeckMessageBase;

namespace uiCloudSupport
{
    //Entering
    // Works for the landing page and also Heist and Series.
    // The example uses the landing page. CSocialClubFeedTilesMgr::Get() will be omitted
    // 1) Call Landing().RenewTiles() to download the data
    // 2) Check Landing().GetMetaDownloadState() to see if the data has finished. Use a timeout and abort if it takes too long.
    //    Finished does not mean succeeded. The data may be empty in which case you should fall back to the offline version.
    // 3) If the metadata has been loaded you can call Landing().PreloadUI(1, <num items visible on screen>) and GetUIPreloadState() to also (down)load the images.
    //    Wait for it to finish or if it takes too long simply enter the page. The images will continue to download
    // 2) and 3) combined should be wrapped in a timeout of 5-15 seconds.
    // 4) Call the appropriate fill functions

    //While on the page, specific to Heist and Series
    // *) Call FillCarouselBig whenever the focus changes. Also call CSocialClubFeedTilesMgr::Get().LoadTileContent(tile, true) for the focused item.
    // *) Call CSocialClubFeedTilesMgr::Get().LoadTileContent(tile, false) for each visible card on screen. This is only needed if we have a large number of cards.
    //    If not we can probably just call it once for each card when filling in the data.

    //Exiting:
    // When exiting the whole ares please call CSocialClubFeedTilesMgr::Get().ClearFeedsContent(false);
    // You can also call Landing().ClearContent() & co. when switching between landing, heist and series for example

    //Sign out:
    // Each user can have different data so on a sign out and in we have to re-enter the page.

    //PSN/XBL sign out or going offline:
    // If the PSN connection is lost we can keep on showing the current page as long as entering multiplayer is gated off correctly once the user clicks on a tile

    //Online connection established while on the offline page:
    // We can download the data if we reconnect. It's more a design decision what to do then.

    struct LayoutPos
    {
        LayoutPos() : m_x(0), m_y(0) {}
        LayoutPos(int x, int y) : m_x(x), m_y(y) {}

        int m_x;
        int m_y;
    };

    struct LayoutMapping
    {
        // Does not include the optional cards to the right of the screen
        static const int MAX_CUSTOM_CARDS = 4;

        int m_numCards;
        LayoutPos m_pos[MAX_CUSTOM_CARDS];
    };

    bool FillLandingPageLayout(CLandingOnlineLayout::eLayoutType& layout, unsigned& layoutPublishId);
    bool VerifyLandingPageLayout(CLandingOnlineLayout::eLayoutType layout, const unsigned layoutPublishId, const CSocialClubFeedTilesRow& row);
    void FillEmptyCard(CCloudCardItem& dest);
    const LayoutMapping* GetLayoutMapping(CLandingOnlineLayout::eLayoutType layout);
    bool FillLandingPageCard(CCloudCardItem& dest, const int column, const int row, const unsigned layoutPublishId);
    void FillLandingPageCard(CCloudCardItem& dest, const ScFeedPost& post);

    // Carousel
    // Use CSocialClubFeedTilesMgr::Get().Heist() or Series() to access the data
    void FillCarouselCard(CCloudCardItem& dest, const ScFeedPost& post);

    // Common
    CSocialClubFeedTilesRow* GetSource(const atHashString id);

    // These two do an allocation with rage_new so you have to ensure they're deleted as well
    uiPageDeckMessageBase* CreateLink(const ScFeedPost& post, atHashString* linkMetricType = nullptr);
    uiPageDeckMessageBase* CreateLink(const char* text, atHashString* linkMetricType = nullptr);

    atHashString GetTrackingType(const uiPageDeckMessageBase& msg);

    void FillText(ConstString& dest, const atHashString id, const ScFeedPost& post, const atHashString fallback = atHashString());
    void FillImage(ConstString& tx, ConstString& txd, const atHashString id, const ScFeedPost& post, const char* defaultTxd = nullptr);
    void FillSticker(IItemStickerInterface::StickerType& stickerType, ConstString& stickerText, const ScFeedPost& post);

    //PURPOSE
    //  Returns true if the texture of a downloaded image has been loaded. Although not entirely accurate,
    //  this should be fine to be called on non-downloaded textures.
    bool IsTextureLoaded(const char* tx, const char* txd);

    // Gambling and other checks
    bool CanBeShown(const ScFeedPost& post); // To check if the post can be displayed at all
    bool CanShowGamblingCards(); // To check if we're allowed to display any gambling cards
    bool CanEnableNewsCard(); // To check if the news (what's new) card is enabled/clickable

} // namespace uiCloudSupport

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // CLOUD_WRAPPER_H
