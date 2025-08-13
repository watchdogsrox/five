/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutDecoratorItem.h
// PURPOSE : This class acts as a wrapper to decorate a CPageItemBase to fit with
//           the needs of a CPageLayoutItem. 
//
//           In MVC terms, this is a transport mechanism to get model data into
//           the view.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_LAYOUT_DECORATOR_ITEM_H
#define PAGE_LAYOUT_DECORATOR_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "system/noncopyable.h"

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/page_deck/layout/PageLayoutItemParams.h"

class CPageItemBase;
class CPageLayoutBase;

class CPageLayoutDecoratorItem final : public CPageLayoutItem
{
    typedef CPageLayoutItem superclass;
public:
    CPageLayoutDecoratorItem( CPageLayoutBase& layout, CPageItemBase& pageItem ) 
        : superclass()
        , m_layout( layout )
        , m_pageItem( pageItem )
    {}
    virtual ~CPageLayoutDecoratorItem() {}

    bool IsInteractive() const { return true; }
    bool IsEnabled() const { return m_pageItem.IsEnabled(); }
    void OnHoverGained() final;
    void OnHoverLost() final;
    void OnFocusGained() final;
    void OnFocusLost() final;

    void PlayRevealAnimation( int const sequenceIndex ) final;

    fwuiInput::eHandlerResult OnSelected( IPageMessageHandler& messageHandler ) final;
    char const * GetTooltop() const final;

    char const * GetSymbol() const;
    CPageLayoutItemParams GenerateParams() const;

    void UpdateVisuals();
	void OnPageEvent(uiPageDeckMessageBase& msg);

private: // declarations and variables
    CPageLayoutBase& m_layout;
    CPageItemBase& m_pageItem;
    NON_COPYABLE( CPageLayoutDecoratorItem );

private: // methods
    void InitializeDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_LAYOUT_DECORATOR_ITEM_H
