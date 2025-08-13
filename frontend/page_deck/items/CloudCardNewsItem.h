/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CloudCardNewsItem.h
// PURPOSE : Represents a news card in the page_deck filled with data downloaded
//           from our servers
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CLOUD_CARD_NEWS_ITEM_H
#define CLOUD_CARD_NEWS_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CloudCardItem.h"

class CCloudCardNewsItem : public CCloudCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CCloudCardNewsItem, CCloudCardItem);

public:
    CCloudCardNewsItem() : superclass() {}
    virtual ~CCloudCardNewsItem() {}

    bool IsEnabled() const final;

private:
    NON_COPYABLE(CCloudCardNewsItem);
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CLOUD_CARD_NEWS_ITEM_H
