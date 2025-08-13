/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MultiCardItem.h
// PURPOSE : Extension of CardItem to provide multiple feature elements
//
// AUTHOR  : james.strain
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef MULTI_CARD_ITEM_H
#define MULTI_CARD_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CardItem.h"

class CMultiCardItem : public CCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CMultiCardItem, CCardItem);

public: // Declarations and variables

public:
    CMultiCardItem() : superclass() {}
    virtual ~CMultiCardItem() {}

private: // declarations and variables
    rage::atArray< rage::ConstString > m_featuredTxdCollection;
    rage::atArray< rage::ConstString > m_featuredTextureCollection;

    PAR_PARSABLE;
    NON_COPYABLE( CMultiCardItem );

    void ForEachFeatureTextureDerived(PerTextureLambda action) final;
    u32 const GetFeatureItemCount() const;
    char const * GetFeatureTxd(rage::u32 const index ) const final;
    char const * GetFeatureTexture(rage::u32 const index ) const final;
};

#endif // MULTI_CARD_ITEM_H

#endif // CARD_ITEM_H
