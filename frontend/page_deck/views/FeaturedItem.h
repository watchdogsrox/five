/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FeaturedItem.h
// PURPOSE : Quick wrapper for the common featured item interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_FEATURED_ITEM_H
#define UI_FEATURED_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CPageItemBase;

class CFeaturedItem : public CPageLayoutItem
{
    typedef CPageLayoutItem superclass;
public:
	CFeaturedItem() :
		m_useRichText(false),
		m_showCostText(false)
    {}
    ~CFeaturedItem() {}

	void SetUseRichText( bool useRichText );
	void SetShowCostText(bool showCostText);
    void SetDetails( CPageItemBase const * const pageItem );
	void SetSticker(CPageItemBase const * const pageItem);
	void SetDetails(atHashString const title, atHashString const description, char const * const tagTxd, char const * const tagTexture, char const * const cost);	
  

    void SetStats( atHashString const stat0Label, int const stat0, atHashString const stat1Label, int const stat1, 
        atHashString const stat2Label, int const stat2, atHashString const stat3Label, int const stat3, atHashString const stat4Label = atHashString::Null(), int const stat4 = -1 );
    void SetStatsLiteral( char const * const stat0Label, int const stat0, char const * const stat1Label, int const stat1, 
        char const * const stat2Label, int const stat2, char const * const stat3Label, int const stat3, char const * const stat4Label = "", int const stat4 = -1 );
    void ClearStats();

private: // declarations and variables
    NON_COPYABLE( CFeaturedItem );

	bool m_showCostText;
	bool m_useRichText;

private: // methods
    char const * GetSymbol() const override { return "mainFeature"; }
    void SetStickerInternal( IItemStickerInterface::StickerType const stickerType, char const * const text );
	void SetDetailsLiteral(char const * const title, char const * const description, char const * const tagTxd, char const * const tagTexture, char const * const cost);
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_FEATURED_ITEM_H
