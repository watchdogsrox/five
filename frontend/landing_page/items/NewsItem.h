/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NewsItem.h
// PURPOSE : Card specialization for the what's new tile
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef NEWS_ITEM_H
#define NEWS_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CardItem.h"

class CNewsItem final : public CCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CNewsItem, CCardItem);

public: // Declarations and variables


public:
    CNewsItem() : superclass() {}
    virtual ~CNewsItem() {}

    bool IsEnabled() const final;

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE(CNewsItem);
};

#endif // UI_PAGE_DECK_ENABLED

#endif // NEWS_ITEM_H
