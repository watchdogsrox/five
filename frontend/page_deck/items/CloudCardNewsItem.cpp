/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CloudCardNewsItem.cpp
// PURPOSE : Represents a news card in the page_deck filled with data downloaded
//           from our servers
//
/////////////////////////////////////////////////////////////////////////////////
#include "CloudCardNewsItem.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/items/uiCloudSupport.h"

FWUI_DEFINE_TYPE(CCloudCardNewsItem, 0xA08ADE39);

bool CCloudCardNewsItem::IsEnabled() const
{
    return uiCloudSupport::CanEnableNewsCard();
}

#endif // UI_PAGE_DECK_ENABLED
