/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NewsItem.cpp
// PURPOSE : Card specialization for the what's new tile
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "NewsItem.h"
#if UI_PAGE_DECK_ENABLED
#include "NewsItem_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE( CNewsItem, 0x6EDC36A7);

bool CNewsItem::IsEnabled() const
{
    return uiCloudSupport::CanEnableNewsCard();
}

#endif // UI_PAGE_DECK_ENABLED
