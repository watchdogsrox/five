/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageLayoutItem.cpp
// PURPOSE : Base class for an item that can be laid out by our small layout engine
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageLayoutItem.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/ui_channel.h"

void CPageLayoutItem::Initialize( CComplexObject& parentObject )
{
    if( !IsInitialized() )
    {
        char const * const c_symbol = GetSymbol();
        if( c_symbol == nullptr || c_symbol[0] == rage::TEXT_CHAR_TERMINATOR )
        {
            m_displayObject = parentObject.CreateEmptyMovieClip( nullptr, -1 );
        }
        else
        {
             m_displayObject = parentObject.AttachMovieClipInstance( c_symbol, -1 );
        }
       
        m_displayObject.SetVisible( true );
        InitializeDerived();
    }
}

void CPageLayoutItem::Shutdown()
{
    if( IsInitialized() )
    {
        ShutdownDerived();
        m_displayObject.Release();
    }
}

void CPageLayoutItem::ResolveLayoutDerived( Vec2f_In UNUSED_PARAM(screenspacePosPx), Vec2f_In localPosPx, Vec2f_In UNUSED_PARAM(sizePx) )
{
    m_displayObject.SetPositionInPixels( localPosPx );
}

void CPageLayoutItem::PlayRevealAnimation(int const sequenceIndex)
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ANIMATE_IN" );
        CScaleformMgr::AddParamInt( sequenceIndex );
        CScaleformMgr::EndMethod();
    }
}

#endif // UI_PAGE_DECK_ENABLED
