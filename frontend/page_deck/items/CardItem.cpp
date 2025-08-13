/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CardItem.cpp
// PURPOSE : Represents a card in the page_deck, with a shortname, long-name,
//           description, and txd/texture info
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "CardItem.h"
#if UI_PAGE_DECK_ENABLED
#include "CardItem_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "network/live/NetworkTelemetry.h"

FWUI_DEFINE_TYPE( CCardItem, 0x647FEF52 );

char const * CCardItem::GetDescriptionText() const
{
    atHashString const c_description = GetDescription();
    return c_description.IsNotNull() ? TheText.Get( c_description ) : nullptr;
}

void CCardItem::OnFocusGainedDerived(CComplexObject& displayObject)
{
    superclass::OnFocusGainedDerived(displayObject);
    CNetworkTelemetry::CardFocusGained(m_trackingBit);
}

#endif // UI_PAGE_DECK_ENABLED
