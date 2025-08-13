/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageCloudDataCategory.h
// PURPOSE : A category dedicated to the landing page
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_CLOUD_DATA_CATEGORY_H
#define LANDING_PAGE_CLOUD_DATA_CATEGORY_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CardItem.h"
#include "frontend/page_deck/items/CloudDataCategory.h"
#include "frontend/landing_page/items/NewsItem.h"
#include "frontend/landing_page/layout/LandingOnlineLayout.h"
#include "frontend/landing_page/items/NetworkErrorItem.h"

class CLandingPageCloudDataCategory : public CCloudDataCategory
{
    typedef CCloudDataCategory superclass;

public:
    CLandingPageCloudDataCategory();
    virtual ~CLandingPageCloudDataCategory();

    eLandingPageOnlineDataSource GetCurrentDataSource() const;

private:
    bool CreateCard(const ScFeedPost& post, int x, int y);
    bool FillData(CSocialClubFeedTilesRow& row) final;
    bool LoadRichData(CSocialClubFeedTilesRow& row) final;
    void OnFailed() final;

    void FillLandingPageFallbackData();
    CCardItem* GetBackupItem(const unsigned column, const unsigned row, atHashString& area);

    void FillLandingPageFallbackData_Offline();
    void FillLandingPageFallbackData_NoMPCharacter();
    void FillLandingPageAs1x1( CCardItemBase& item );
    void ApplySource( eLandingPageOnlineDataSource source );
    void RefreshLayoutType( CLandingOnlineLayout::eLayoutType const layoutType );

    bool RefreshContentDerived() final;

private:
    static const unsigned PERSISTENT_CARDS_COUNT = 4;
    static const unsigned PERSISTENT_CARDS_COLUMN = 2;

    CLandingOnlineLayout::eLayoutType m_backupLayoutType;
    eLandingPageOnlineDataSource m_currentDataSource;

    PAR_PARSABLE;

    CCardItem m_freeRoamCard;
    CCardItem m_heistCard;
    CCardItem m_seriesCard;
    CNewsItem m_newsCard;
    CCardItem m_mainCard;

    CNetworkErrorItem m_networkErrorCard;
    CParallaxCardItem m_noCharacterCard;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // LANDING_PAGE_CLOUD_DATA_CATEGORY_H
