/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DynamicLayoutContext.cpp
// PURPOSE : Helper class that acts as a RAII support for dynamic layouts
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "DynamicLayoutContext.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/page_deck/layout/PageLayoutDecoratorItem.h"
#include "frontend/ui_channel.h"

CDynamicLayoutContext::CDynamicLayoutContext( CComplexObject& root, CPageItemCategoryBase& category )
    : superclass()
    , m_layout( category.GetLayout() )
{
    if( m_layout )
    {
        m_layout->Reset();
        Initialize( root );
        GenerateItems( category );
    }
}

CDynamicLayoutContext::CDynamicLayoutContext( CComplexObject& root, CPageLayoutBase& layout )
    : superclass()
    , m_layout( &layout )
{
    m_layout->Reset();
    Initialize( root );
}

CDynamicLayoutContext::~CDynamicLayoutContext()
{
    RemoveItems();
}

void CDynamicLayoutContext::RemoveItems()
{
    // We don't own the m_layout pointer so leave it alone
    if( m_layout )
    {
        m_layout->UnregisterAll();
    }

    int const c_count = m_items.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        m_items[index]->Shutdown();
        delete m_items[index];
    }
    m_items.ResetCount();
}

void CDynamicLayoutContext::ResolveLayoutDerived( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx )
{
    CComplexObject& displayObject = GetDisplayObject();
    displayObject.SetPositionInPixels( localPosPx );

    if( m_layout )
    {
        m_layout->RecalculateLayout( screenspacePosPx, sizePx );
        m_layout->NotifyOfLayoutUpdate();
    }
}

void CDynamicLayoutContext::GenerateItems( CPageItemCategoryBase& category )
{
    if( m_layout )
    {
        auto itemFunc = [this]( CPageItemBase& item )
        {
            CPageLayoutDecoratorItem * const layoutItem = rage_new CPageLayoutDecoratorItem( *m_layout, item );
            m_items.PushAndGrow( layoutItem );
            layoutItem->Initialize( GetDisplayObject() );
            m_layout->RegisterItem( *layoutItem, layoutItem->GenerateParams() );
        };
        category.ForEachItem( itemFunc );
    }
}

#if RSG_BANK

void CDynamicLayoutContext::DebugRenderDerived() const 
{
    if( m_layout )
    {
        m_layout->DebugRender();
    }
}

#endif

void CDynamicLayoutContext::SetDefaultFocus()
{
    if( m_layout )
    {
        m_layout->SetDefaultFocus();
    }
}

void CDynamicLayoutContext::InvalidateFocus()
{
    if( m_layout )
    {
        m_layout->InvalidateFocus();
    }
}

void CDynamicLayoutContext::RefreshFocusVisuals()
{
    if( m_layout )
    {
        m_layout->RefreshFocusVisuals();
    }
}

void CDynamicLayoutContext::ClearFocusVisuals()
{
    if( m_layout )
    {
        m_layout->ClearFocusVisuals();
    }
}

fwuiInput::eHandlerResult CDynamicLayoutContext::HandleDigitalNavigation( int const deltaX, int const deltaY )
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    if( m_layout )
    {
        result = m_layout->HandleDigitalNavigation( deltaX, deltaY );
    }

    return result;
}

fwuiInput::eHandlerResult CDynamicLayoutContext::HandleInputAction( eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler ) const
{
    fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

    if( m_layout )
    {
        result = m_layout->HandleInputAction( inputAction, messageHandler );
    }

    return result;
}

void CDynamicLayoutContext::UpdateVisuals()
{
    int const c_count = m_items.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        CPageLayoutDecoratorItem * const layoutItem = m_items[ index ];
        layoutItem->UpdateVisuals();
    }
}

void CDynamicLayoutContext::OnPageEvent(uiPageDeckMessageBase& msg)
{	
	int const c_count = m_items.GetCount();
	for (int index = 0; index < c_count; ++index)
	{
		CPageLayoutDecoratorItem * const layoutItem = m_items[index];
		layoutItem->OnPageEvent(msg);
	}
}

char const * CDynamicLayoutContext::GetFocusTooltip() const
{
    char const * result = nullptr;

    if( m_layout )
    {
        result = m_layout->GetFocusTooltip();
    }

    return result;
}

void CDynamicLayoutContext::UpdateInstructionalButtons() const
{
    if( m_layout )
    {
        m_layout->UpdateInstructionalButtons();
    }
}

void CDynamicLayoutContext::PlayEnterAnimation()
{
    if( m_layout )
    {
        m_layout->PlayEnterAnimation();
    }
}

#endif // UI_PAGE_DECK_ENABLED

