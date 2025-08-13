/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Carousel.h
// PURPOSE : Quick wrapper for the common carousel interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_CAROUSEL_H
#define UI_CAROUSEL_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage 
#include "atl/array.h"

// game
#include "frontend/input/uiInputEnums.h"
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CPageItemBase;
class CPageItemCategoryBase;

enum eCarouselItemType {
    eCarouselItemType_Price,
    eCarouselItemType_Text,
    eCarouselItemType_Bonus
};

class CCarousel final : public CPageLayoutItem
{
public:
	typedef LambdaRef< void(CPageItemBase const&)>    PerItemConstLambda;

    CCarousel()
        : m_focusedIndex( INDEX_NONE )
        , m_wrapping( false )
    {}
    CCarousel( bool const wrapping )
        : m_focusedIndex( INDEX_NONE )
        , m_wrapping( wrapping )
    {}
    ~CCarousel() 
    {}

    void AddCategory( CPageItemCategoryBase& category, const eCarouselItemType type = eCarouselItemType_Price);

    void SetDefaultFocus();
    void InvalidateFocus();
    void RefreshFocusVisuals();
    void ClearFocusVisuals();
    char const * GetFocusTooltip() const;

	void ForEachItem(PerItemConstLambda action) const;

	void SetSticker( int const index, IItemStickerInterface::StickerType const stickerType, char const * const text );

    void UpdateVisuals();
    void UpdateInstructionalButtons( atHashString const acceptLabelOverride = atHashString() ) const;

    CPageItemBase const* GetFocusedItem() const;

    fwuiInput::eHandlerResult HandleDigitalNavigation( int const deltaX );
    fwuiInput::eHandlerResult HandleInputAction( eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler );

    void PlayRevealAnimation( int const sequenceIndex ) final;

private: // declarations and variables
    atArray<CPageItemBase*>     m_items;
    atArray<bool>               m_requiresTextureRefresh;
    int                         m_focusedIndex;
    bool                        m_wrapping;
    NON_COPYABLE( CCarousel );

private: // methods
    char const * GetSymbol() const final { return "carousel"; }

    CPageItemBase* GetFocusedItemInternal();
    CPageItemBase* GetItemByIndex( int const index );
    CPageItemBase const* GetItemByIndex( int const index ) const;
    int GetItemCount() const { return m_items.GetCount(); }
    bool IsItemEnabled( int const index ) const;

    void AddItem( CPageItemBase& item, const eCarouselItemType type);
    void RefreshItem( int const index );
    void RefreshEnabledState( int const index ); 
    void RefreshEnabledState( int const index, CComplexObject& displayObject );
    bool UpdateFocus( int const delta );

	void SetStickerInternal( int const index, IItemStickerInterface::StickerType const stickerType, char const * const text );

    void ShutdownDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_CAROUSEL_H
