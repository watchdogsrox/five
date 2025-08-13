/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageItemBase.cpp
// PURPOSE : Base for items that can be represented on a page_deck menu
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageItemBase.h"
#if UI_PAGE_DECK_ENABLED
#include "PageItemBase_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "text/TextFile.h"

FWUI_DEFINE_TYPE( CPageItemBase, 0xD7414470 );

CPageItemBase::~CPageItemBase()
{
    DestroyOnSelectMessage();
}

char const * CPageItemBase::GetItemTitleText() const
{
    atHashString const c_title = GetItemTitle();
    return TheText.Get( c_title );
}

char const * CPageItemBase::GetTooltipText() const
{
    atHashString const c_tooltip = GetTooltip();
    return c_tooltip.IsNotNull() ? TheText.Get( c_tooltip ) : nullptr;
}

void CPageItemBase::OnReveal( CComplexObject& displayObject, int const sequenceIndex )
{
    OnRevealDerived( displayObject, sequenceIndex );
}

void CPageItemBase::OnHoverGained( CComplexObject& displayObject )
{
#if RSG_PC
    OnHoverGainedDerived(displayObject);
#else // RSG_PC
    UNUSED_VAR(displayObject);
#endif // RSG_PC
}

void CPageItemBase::OnHoverLost( CComplexObject& displayObject )
{
#if RSG_PC
    OnHoverLostDerived(displayObject);
#else // RSG_PC
    UNUSED_VAR(displayObject);
#endif // RSG_PC
}

void CPageItemBase::OnFocusGained( CComplexObject& displayObject )
{
    OnFocusGainedDerived( displayObject );
}

void CPageItemBase::OnFocusLost( CComplexObject& displayObject )
{
    OnFocusLostDerived(displayObject);
}

bool CPageItemBase::OnSelected( CComplexObject& displayObject, IPageMessageHandler& messageHandler )
{
    bool handled = false;

    OnSelectedDerived( displayObject );
    if( m_onSelectMessage )
    {
        // TODO_UI - So far our designs aren't as complicated as other projects, but
        // we may want to check the result here or have a handled param on the message?
        messageHandler.HandleMessage( *m_onSelectMessage );
        handled = true;
    }

    return handled;
}

void CPageItemBase::SetDisplayObjectContent( CComplexObject& displayObject )
{
    SetDisplayObjectContentDerived( displayObject );
}

void CPageItemBase::UpdateVisuals(CComplexObject& displayObject)
{
    UpdateVisualsDerived( displayObject );
}

void CPageItemBase::SetOnSelectMessage(uiPageDeckMessageBase*& message)
{
    DestroyOnSelectMessage();
    m_onSelectMessage = message;
    message = nullptr;
}

void CPageItemBase::DestroyOnSelectMessage()
{
    if (m_onSelectMessage != nullptr)
    {
        delete m_onSelectMessage;
        m_onSelectMessage = nullptr;
    }
}

void CPageItemBase::OnPageEvent(uiPageDeckMessageBase& msg)
{
	OnPageEventDerived(msg);
}

#endif // UI_PAGE_DECK_ENABLED
