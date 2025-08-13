/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CloudCardItem.cpp
// PURPOSE : Represents a card in the page_deck, with a shortname, long-name,
//           description, and txd/texture info
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "CloudCardItem.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/landing_page/items/NewsItem.h"
#include "frontend/ui_channel.h"
#include "network/live/NetworkTelemetry.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"

FWUI_DEFINE_TYPE(CCloudCardItem, 0x89348EE8);

char const * CCloudCardItem::GetItemTitleText() const
{
    return m_titleStr.c_str();
}

char const * CCloudCardItem::GetPageTitleText() const
{
    return m_titleBigStr.c_str();
}

char const * CCloudCardItem::GetTooltipText() const
{
    return m_tooltipStr.c_str();
}

char const * CCloudCardItem::GetDescriptionText() const
{
    return m_descriptionStr.c_str();
}

bool CCloudCardItem::RequestFeatureTexture(rage::u32 const index)
{
    CSocialClubFeedTilesMgr::Get().LoadTileContentById(m_guid, true);

    // This one ensures we also start to download the card to the left and right
    CSocialClubFeedTilesMgr::Get().TileFocussed(m_guid);

    return CCardItemBase::RequestFeatureTexture(index);
}

void CCloudCardItem::SetGuid(const RosFeedGuid guid)
{
    m_guid = guid;
}

void CCloudCardItem::SetTrackingData(int trackingBit, const atHashString id)
{
    m_trackingBit = trackingBit;
    GetIdStorage() = id;
}

void CCloudCardItem::OnFocusGainedDerived(CComplexObject& displayObject)
{
    superclass::OnFocusGainedDerived(displayObject);
    CNetworkTelemetry::CardFocusGained(m_trackingBit);
}

#endif // UI_PAGE_DECK_ENABLED
