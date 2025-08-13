/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DynamicLayoutContext.h
// PURPOSE : Helper class that acts as a RAII support for dynamic layouts.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef DYNAMIC_LAYOUT_CONTEXT_H
#define DYNAMIC_LAYOUT_CONTEXT_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// framework
#include "fwui/Input/fwuiInputEnums.h"

// game
#include "frontend/input/uiInputEnums.h"
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CPageItemCategoryBase;
class CPageLayoutBase;
class CPageLayoutDecoratorItem;
class IPageMessageHandler;

// Inherits from layout item so we can easily cascade layout resolves
class CDynamicLayoutContext final : public CPageLayoutItem
{
    typedef CPageLayoutItem superclass;
public:
    CDynamicLayoutContext( CComplexObject& root, CPageItemCategoryBase& category );
    CDynamicLayoutContext( CComplexObject& root, CPageLayoutBase& layout );
    virtual ~CDynamicLayoutContext();

    void AddCategory( CPageItemCategoryBase& category ) { GenerateItems( category ); }
    void RemoveItems();
    
    void SetDefaultFocus();
    void InvalidateFocus();
    void RefreshFocusVisuals();
    void ClearFocusVisuals();
    char const * GetFocusTooltip() const;
    void UpdateInstructionalButtons() const;

    void PlayEnterAnimation();

    fwuiInput::eHandlerResult HandleDigitalNavigation( int const deltaX, int const deltaY );
    fwuiInput::eHandlerResult HandleInputAction( eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler ) const;
    void UpdateVisuals();

	void OnPageEvent(uiPageDeckMessageBase& msg);

private: // declarations and variables
    fwRegdRef<CPageLayoutBase>          m_layout;
    atArray<CPageLayoutDecoratorItem*>  m_items;
    NON_COPYABLE(CDynamicLayoutContext);

private: // methods
    void GenerateItems( CPageItemCategoryBase& category );

    void ResolveLayoutDerived( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx ) final;
#if RSG_BANK
    void DebugRenderDerived() const final;
#endif 
};

#endif // UI_PAGE_DECK_ENABLED

#endif // DYNAMIC_LAYOUT_CONTEXT_H
