/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutDecoratorItem.cpp
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
#include "PageLayoutDecoratorItem.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/page_deck/uiPageEnums.h"
#include "frontend/page_deck/PageItemBase.h"

char const * CPageLayoutDecoratorItem::GetSymbol() const
{
    return m_layout.GetSymbol( m_pageItem );
}

CPageLayoutItemParams CPageLayoutDecoratorItem::GenerateParams() const
{
    return m_layout.GenerateParams( m_pageItem );
}

void CPageLayoutDecoratorItem::OnHoverGained()
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.OnHoverGained( displayObject );
}

void CPageLayoutDecoratorItem::OnHoverLost()
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.OnHoverGained( displayObject );
}

void CPageLayoutDecoratorItem::OnFocusGained()
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.OnFocusGained( displayObject );
}

void CPageLayoutDecoratorItem::OnFocusLost()
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.OnFocusLost( displayObject );
}

void CPageLayoutDecoratorItem::PlayRevealAnimation(int const sequenceIndex)
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.OnReveal( displayObject, sequenceIndex );
}

fwuiInput::eHandlerResult CPageLayoutDecoratorItem::OnSelected( IPageMessageHandler& messageHandler )
{
    CComplexObject& displayObject = GetDisplayObject();
    bool const c_handled = m_pageItem.OnSelected( displayObject, messageHandler );
    return c_handled ? fwuiInput::ACTION_HANDLED : fwuiInput::ACTION_NOT_HANDLED;
}

char const * CPageLayoutDecoratorItem::GetTooltop() const 
{
    char const * const c_result = m_pageItem.GetTooltipText();
    return c_result;
}

void CPageLayoutDecoratorItem::UpdateVisuals()
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.UpdateVisuals( displayObject );
}

void CPageLayoutDecoratorItem::OnPageEvent(uiPageDeckMessageBase& msg)
{	
	m_pageItem.OnPageEvent(msg);
}

void CPageLayoutDecoratorItem::InitializeDerived()
{
    CComplexObject& displayObject = GetDisplayObject();
    m_pageItem.SetDisplayObjectContent( displayObject );
}

#endif // UI_PAGE_DECK_ENABLED
