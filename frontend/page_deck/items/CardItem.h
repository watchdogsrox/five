/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CardItem.h
// PURPOSE : Represents a card in the page_deck, with a shortname, long-name,
//           description, and txd/texture info
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CARD_ITEM_H
#define CARD_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CardItemBase.h"

class CCardItem : public CCardItemBase
{
    FWUI_DECLARE_DERIVED_TYPE(CCardItem, CCardItemBase);

public: // Declarations and variables

    atHashString GetDescription() const { return m_description; }
    char const * GetDescriptionText() const override;

public:
    CCardItem() : superclass(), m_trackingBit(-1) {}
    virtual ~CCardItem() {}

protected:
    virtual void OnFocusGainedDerived(CComplexObject& displayObject);

private: // declarations and variables
    atHashString m_description;
    int m_trackingBit;
    PAR_PARSABLE;
    NON_COPYABLE( CCardItem );
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CARD_ITEM_H
